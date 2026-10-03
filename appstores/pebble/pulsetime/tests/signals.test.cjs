const assert = require('node:assert/strict');
const {normalize, weather, wire, https, create, preset} = require('../src/pkjs/signals');
const now = 1788933600;
const good = {version:1, observed_at:now-10, expires_at:now+120, state:'healthy', label:'Drummer', detail:'Checks passed'};
assert.equal(normalize(good, now).state, 1);
for (const patch of [{state:'typo'}, {observed_at:now+1}, {expires_at:now}, {expires_at:now+999999}, {values:[1,2]}, {values:[1,2,4]}, {label:''}, {version:2}]) {
  assert.throws(() => normalize({...good,...patch}, now));
}
assert.equal(normalize({...good,state:'unknown'},now).state,0);
const departures=normalize(preset({...good,measurements:[2,10,25]},'departures'),now);
assert.deepEqual(departures.value,[3,2,1]);
assert.deepEqual(departures.sampleText,['2 min','10 min','25 min']);
assert(wire(departures,1,3).startsWith('2|1|3|'));
assert(wire(departures,4294967295,3).length<=240);
assert.deepEqual(normalize(preset({...good,measurements:[0,70,95]},'resources'),now).value,[0,2,3]);
assert.equal(departures.observed,good.observed_at,'Preset preserves source freshness');
for(const measurements of [[3,2,1],[1,2],[1,NaN,3],[-1,2,3]]) assert.throws(()=>preset({...good,measurements},'departures'));
assert.throws(()=>preset({...good,measurements:[0,90,101]},'resources'));
assert.throws(()=>normalize({...good,values:[1,2,3],sample_labels:['A','B','C']},now));
assert.throws(()=>normalize({...good,values:[1,2,3],sample_labels:['A','B','C'],sample_text:['bad|value','2','3']},now));
const rich=normalize({...good,values:[1,2,3],sample_labels:['North','East','West'],sample_text:['10 kph','20 kph','30 kph']},now);
assert.equal(rich.sampleLabels[2],'West');
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
config={WEATHER_ENABLED:true,WEATHER_LOCATION:'fixed',WEATHER_LAT:'0',WEATHER_LON:'0'};client.request('6:1');assert(requests[4].url.includes('latitude=0&longitude=0'));
requests[4].resolve(forecast);assert(sent.at(-1).SIG_DATA.includes('|0|1|3|1|1|'));
console.log('Signal source validation, forecast windows, consent, auth isolation, failures and stale-reply cancellation passed.');

function deliveryHarness(onSend) {
  let clock = now * 1000, nextTimer = 0;
  const timers = new Map(), attempts = [], fetches = [];
  class Source {
    constructor() { fetches.push(this); }
    open() {} setRequestHeader() {} send() {} abort() {}
    resolve(reading = good) { this.status = 200; this.responseText = JSON.stringify(reading); this.onload(); }
  }
  const bridge = create({settings: () => ({DRUMMER_URL: 'https://example.com/feed'}),
    now: () => clock, xhr: () => new Source(),
    setTimeout: (callback, delay) => { const id = ++nextTimer; timers.set(id, {callback, at: clock + delay}); return id; },
    clearTimeout: id => timers.delete(id),
    send: (data, ok, fail) => { const attempt = {data, ok, fail}; attempts.push(attempt); if (onSend) onSend(attempt); }});
  function advance(ms) {
    const target = clock + ms;
    for (;;) {
      const next = [...timers].filter(([, timer]) => timer.at <= target).sort((a, b) => a[1].at - b[1].at)[0];
      if (!next) break;
      clock = next[1].at; timers.delete(next[0]); next[1].callback();
    }
    clock = target;
  }
  return {bridge, attempts, fetches, timers, advance};
}

{
  const h = deliveryHarness(); h.bridge.request('101:2'); h.fetches[0].resolve();
  h.attempts[0].fail({error: {message: 'APP_MSG_BUSY'}});
  h.advance(249); assert.equal(h.attempts.length, 1);
  h.advance(1); assert.equal(h.attempts.length, 2, 'Transient delivery failure must retry');
  assert.deepEqual(h.attempts[1].data, h.attempts[0].data, 'Retry preserves the exact request and source timestamps');
  h.attempts[1].ok(); h.advance(20000);
  assert.equal(h.attempts.length, 2); assert.equal(h.fetches.length, 1, 'Delivery retries never fetch the source again');
}

for (const synchronous of [false, true]) {
  const h = deliveryHarness(attempt => { if (synchronous) throw new Error('Busy transport'); attempt.fail(); });
  h.bridge.request('102:2'); h.fetches[0].resolve(); h.advance(20000);
  assert.equal(h.attempts.length, 4, 'Permanent delivery failure stops after four attempts');
  assert.equal(h.fetches.length, 1); assert.equal(h.timers.size, 0);
}

{
  let busy = true;
  const h = deliveryHarness(attempt => { if (busy) { busy = false; throw new Error('Busy once'); } attempt.ok(); });
  h.bridge.request('113:2'); h.fetches[0].resolve(); h.advance(250);
  assert.equal(h.attempts.length, 2, 'A synchronous error can recover on the next delivery attempt');
  assert.deepEqual(h.attempts[0].data, h.attempts[1].data);
  assert.equal(h.fetches.length, 1); assert.equal(h.timers.size, 0);
}

{
  const h = deliveryHarness(); h.bridge.request('103:2'); h.fetches[0].resolve();
  h.attempts[0].fail(); h.bridge.cancel(); h.advance(20000);
  assert.equal(h.attempts.length, 1, 'Cancellation discards an unsent retry');
  assert.equal(h.timers.size, 0);
}

{
  const h = deliveryHarness(); h.bridge.request('104:2'); h.fetches[0].resolve();
  const old = h.attempts[0]; old.fail();
  h.bridge.request('105:2'); h.fetches[1].resolve(); h.advance(250);
  assert.equal(h.attempts.length, 2, 'Superseding request drops old queued bytes');
  assert(h.attempts[1].data.SIG_DATA.startsWith('1|105|2|'));
  h.bridge.request('106:2'); h.fetches[2].resolve();
  old.ok(); old.fail();
  assert.equal(h.attempts.length, 2, 'Duplicate old callbacks cannot release a newer send');
  h.attempts[1].ok();
  assert.equal(h.attempts.length, 3); assert(h.attempts[2].data.SIG_DATA.startsWith('1|106|2|'));
  h.attempts[2].ok(); h.advance(20000); assert.equal(h.attempts.length, 3);
}

{
  const h = deliveryHarness(); h.bridge.request('107:2'); h.fetches[0].resolve();
  h.bridge.cancel(); h.bridge.request('108:2'); h.fetches[1].resolve();
  h.attempts[0].fail();
  assert.equal(h.attempts.length, 2, 'An in-flight old failure unblocks but cannot replay after cancellation');
  assert(h.attempts[1].data.SIG_DATA.startsWith('1|108|2|'));
  h.attempts[1].ok(); h.advance(20000); assert.equal(h.attempts.length, 2);
}

{
  const h = deliveryHarness(); h.bridge.request('114:2'); h.fetches[0].resolve();
  const lost = h.attempts[0];
  h.advance(1000); h.bridge.cancel(); h.bridge.request('115:2'); h.fetches[1].resolve();
  h.advance(16999); assert.equal(h.attempts.length, 1, 'An uncertain in-flight send is not replayed');
  h.advance(1);
  assert.equal(h.attempts.length, 2, 'Missing callback unblocks a newer request at the original deadline');
  assert(h.attempts[1].data.SIG_DATA.startsWith('1|115|2|'));
  h.bridge.request('116:2'); h.fetches[2].resolve();
  lost.ok(); lost.fail();
  assert.equal(h.attempts.length, 2, 'A late callback after timeout cannot release a newer send');
  h.attempts[1].ok(); assert.equal(h.attempts.length, 3);
  assert(h.attempts[2].data.SIG_DATA.startsWith('1|116|2|'));
  h.attempts[2].ok(); h.advance(20000);
  assert.equal(h.attempts.length, 3); assert.equal(h.fetches.length, 3); assert.equal(h.timers.size, 0);
}

{
  const h = deliveryHarness(); h.bridge.request('109:2'); h.advance(17800); h.fetches[0].resolve();
  h.attempts[0].fail(); h.advance(20000);
  assert.equal(h.attempts.length, 1, 'A slow source does not get a fresh delivery deadline');
  assert.equal(h.timers.size, 0);
}

{
  const h = deliveryHarness(); h.advance(900); h.bridge.request('110:2');
  h.fetches[0].resolve({...good, expires_at: now + 1}); h.attempts[0].fail(); h.advance(20000);
  assert.equal(h.attempts.length, 1, 'Reading expiry stops delivery retries');
}

{
  const h = deliveryHarness(); h.bridge.request('111:2'); h.advance(18000); h.fetches[0].resolve();
  assert.equal(h.attempts.length, 0, 'Already late responses are not placed on the transport');
}

{
  const h = deliveryHarness(attempt => { attempt.ok(); throw new Error('Failure after callback'); });
  h.bridge.request('112:2'); h.fetches[0].resolve(); h.advance(20000);
  assert.equal(h.attempts.length, 1, 'A settled callback is never retried');
}

console.log('Response delivery: retry identity, four-attempt cap, synchronous failure, cancellation, supersession, callback isolation and fixed expiry/deadline passed.');
