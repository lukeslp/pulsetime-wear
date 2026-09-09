const assert = require('node:assert/strict');
const {normalize, weather, wire, https, create} = require('../src/pkjs/signals');
const now = 1788933600;
const good = {version:1, observed_at:now-10, expires_at:now+120, state:'healthy', label:'Drummer', detail:'Checks passed'};
assert.equal(normalize(good, now).state, 1);
for (const patch of [{state:'typo'}, {observed_at:now+1}, {expires_at:now}, {expires_at:now+999999}, {values:[1,2]}, {values:[1,2,4]}, {label:''}, {version:2}]) {
  assert.throws(() => normalize({...good,...patch}, now));
}
assert.equal(normalize({...good,state:'unknown'},now).state,0);
assert(https('https://example.com/feed?format=1'));
for (const bad of ['http://example.com','https://user:pass@host/','https://host/#x','https://host/\n','file:///tmp/a']) assert(!https(bad));
const forecast = {hourly_units:{rain:'mm',showers:'mm'},hourly:{time:[],rain:[],showers:[]}};
for(let i=0;i<9;i++){forecast.hourly.time.push(now+(i+1)*3600);forecast.hourly.rain.push(i<2?0:i<4?.3:3);forecast.hourly.showers.push(0);}
assert.deepEqual(weather(forecast,now).value,[0,1,3]);
assert.equal(weather(forecast,now+1800).value[0],0); // overlaps are weighted, not snapped to the hour
assert.throws(()=>weather({...forecast,hourly_units:{rain:'inch',showers:'inch'}},now));
assert.throws(()=>weather({...forecast,hourly:{...forecast.hourly,rain:[null]}},now));
assert.throws(()=>weather(forecast,now-3600));
// A heavy sample ending NOW is past rain and must not leak into the forecast.
const past=JSON.parse(JSON.stringify(forecast));
past.hourly.time.unshift(now);past.hourly.rain.unshift(100);past.hourly.showers.unshift(0);
assert.deepEqual(weather(past,now).value,[0,1,3]);
const duplicate=JSON.parse(JSON.stringify(forecast));duplicate.hourly.time[1]=duplicate.hourly.time[0];assert.throws(()=>weather(duplicate,now));
assert(wire(normalize(good,now),1,2).length<=240);

let config={DRUMMER_URL:'https://example.com/feed',DRUMMER_TOKEN:'private-value'}, sent=[], requests=[];
class XHR {
  constructor(){requests.push(this);}
  open(method,url){this.method=method;this.url=url;}
  setRequestHeader(k,v){this.header=[k,v];}
  send(){}
  abort(){this.aborted=true;}
  resolve(body,status=200){this.status=status;this.responseText=JSON.stringify(body);this.onload();}
}
const client=create({settings:()=>config,now:()=>now*1000,xhr:()=>new XHR(),send:(data,ok)=>{sent.push(data);ok();}});
client.request('1:2'); assert.equal(requests.length,1); assert.deepEqual(requests[0].header,['Authorization','Bearer private-value']);
client.request('2:2'); assert(requests[0].aborted);
requests[0].resolve(good); assert.equal(sent.length,0,'late old reply discarded');
requests[1].resolve(good); assert(sent[0].SIG_DATA.startsWith('1|2|2|'));assert(!sent[0].SIG_DATA.includes('private-value'));
client.request('3:2');client.cancel();requests[2].resolve(good);assert.equal(sent.length,1,'closed page reply discarded');
client.request('4:2');requests[3].resolve(good,503);assert.equal(sent.at(-1).SIG_DATA.split('|')[8],'0','transport failure is unknown, not down');
config={};client.request('5:1');assert.equal(requests.length,4,'disabled weather makes no network request');
config={WEATHER_ENABLED:true,WEATHER_LAT:'0',WEATHER_LON:'0'};client.request('6:1');assert(requests[4].url.includes('latitude=0&longitude=0'));
requests[4].resolve(forecast);assert(sent.at(-1).SIG_DATA.includes('|0|1|3|1|1|'));
console.log('Signal source validation, forecast windows, consent, auth isolation, failures and stale-reply cancellation passed.');
