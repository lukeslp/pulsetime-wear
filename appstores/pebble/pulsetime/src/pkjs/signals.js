'use strict';

// Luke Steuber. Remote services return readings, never vibration instructions.
var STATES = {unknown: 0, healthy: 1, ok: 1, busy: 2, degraded: 3, down: 4};
function integer(n, min, max) { return typeof n === 'number' && isFinite(n) && Math.floor(n) === n && n >= min && n <= max; }
function clean(s, length) { return String(s || '').replace(/[^\x20-\x7e]|\|/g, ' ').slice(0, length); }
function https(url) {
  return typeof url === 'string' && url.length <= 1024 && /^https:\/\/[a-z0-9.-]+(?::443)?(?:\/[^\s#]*)?$/i.test(url) && url.indexOf('@') < 0;
}
function unknown(label, detail, now) {
  return {observed: now, expires: now + 60, value: [0, 0, 0], state: 0, timeline: 0, label: label, detail: detail};
}
function normalize(raw, now) {
  if (!raw || raw.version !== 1 || !Object.prototype.hasOwnProperty.call(STATES, raw.state) ||
      !integer(raw.observed_at, 1, now) || !integer(raw.expires_at, now + 1, raw.observed_at + 86400) ||
      typeof raw.label !== 'string' || !clean(raw.label, 24).trim() ||
      (raw.detail !== undefined && typeof raw.detail !== 'string')) throw new Error('Invalid or old reading');
  var values = raw.values;
  if (values !== undefined && (!Array.isArray(values) || values.length !== 3 || !values.every(function (v) { return integer(v, 0, 3); }))) throw new Error('Invalid samples');
  return {observed: raw.observed_at, expires: raw.expires_at, value: values || [0, 0, 0],
    state: STATES[raw.state], timeline: values ? 1 : 0, label: clean(raw.label, 24), detail: clean(raw.detail, 96)};
}
function weather(raw, now) {
  var h = raw && raw.hourly;
  if (!h || !Array.isArray(h.time) || !Array.isArray(h.rain) || !Array.isArray(h.showers) ||
      !raw.hourly_units || raw.hourly_units.rain !== 'mm' || raw.hourly_units.showers !== 'mm') throw new Error('Invalid rain forecast');
  var values = [], names = ['dry', 'light', 'rain', 'heavy'];
  for (var bucket = 0; bucket < 3; ++bucket) {
    var from = now + bucket * 7200, to = from + 7200, covered = 0, rain = 0;
    for (var i = 0; i < h.time.length; ++i) {
      if (!integer(h.time[i], 1, 4294967295) || (i && h.time[i] !== h.time[i - 1] + 3600)) throw new Error('Invalid forecast timeline');
      // Open-Meteo rain/showers are sums for the PRECEDING hour.
      var overlap = Math.max(0, Math.min(to, h.time[i]) - Math.max(from, h.time[i] - 3600));
      if (!overlap) continue;
      if (typeof h.rain[i] !== 'number' || typeof h.showers[i] !== 'number' || !isFinite(h.rain[i]) || !isFinite(h.showers[i]) || h.rain[i] < 0 || h.showers[i] < 0) throw new Error('Missing rain samples');
      covered += overlap;
      rain += (h.rain[i] + h.showers[i]) * overlap / 3600;
    }
    if (covered !== 7200) throw new Error('Forecast does not cover next six hours');
    var rate = rain / 2;
    values.push(rate < 0.1 ? 0 : rate < 0.5 ? 1 : rate < 2.5 ? 2 : 3);
  }
  return {observed: now, expires: now + 1800, value: values, state: 1, timeline: 1,
    label: 'Rain forecast', detail: 'Next 6h: ' + values.map(function(v) { return names[v]; }).join(' / ') + '. Model forecast.'};
}
function wire(reading, id, channel) {
  return [1, id, channel, reading.observed, reading.expires].concat(reading.value,
    [reading.state, reading.timeline, clean(reading.label, 24), clean(reading.detail, 96)]).join('|');
}

function create(deps) {
  var generation = 0, active = null, cancelLocation = null, sending = false, pending = [];
  function pump() {
    if (sending || !pending.length) return;
    var item = pending.shift();
    if (item.generation !== generation) { pump(); return; }
    sending = true;
    function done() { sending = false; pump(); }
    deps.send({SIG_DATA: item.data}, done, done);
  }
  function cancel() {
    generation++; pending = [];
    if (active) active.abort(); active = null;
    if (cancelLocation) cancelLocation(); cancelLocation = null;
  }
  function request(payload) {
    if (typeof payload !== 'string' || !/^\d{1,10}:[123]$/.test(payload)) return;
    var parts = payload.split(':'), id = Number(parts[0]), channel = Number(parts[1]);
    if (!integer(id, 1, 4294967295)) return;
    cancel();
    var ticket = generation, config = deps.settings(), label = ['','Weather','Drummer','Custom signal'][channel];
    var started = Math.floor(deps.now() / 1000), url, token = '';
    function respond(reading) {
      if (ticket !== generation) return;
      pending.push({generation: ticket, data: wire(reading, id, channel)}); pump();
    }
    function fail(message) { respond(unknown(label, message, Math.floor(deps.now() / 1000))); }
    if (channel === 1) {
      if (config.WEATHER_ENABLED !== true && config.WEATHER_ENABLED !== 1) { fail('Enable weather in phone settings.'); return; }
      // Existing manual-coordinate installs keep their old behavior until
      // the wearer saves the new location walkthrough.
      if (config.WEATHER_LOCATION === 'phone') {
        if (!deps.locate) { fail('Location unavailable. Check Pebble permission on phone.'); return; }
        cancelLocation = deps.locate(function (position) {
          if (ticket !== generation) return;
          fetchWeather(position.latitude, position.longitude);
        }, function (message) { if (ticket === generation) fail(message); });
        return;
      }
      var lat = Number(config.WEATHER_LAT), lon = Number(config.WEATHER_LON);
      if (String(config.WEATHER_LAT || '').trim() === '' || String(config.WEATHER_LON || '').trim() === '' ||
          !isFinite(lat) || !isFinite(lon) || Math.abs(lat) > 90 || Math.abs(lon) > 180) { fail('Set latitude and longitude on phone.'); return; }
      fetchWeather(lat, lon); return;
    } else {
      url = config[channel === 2 ? 'DRUMMER_URL' : 'SIGNAL_URL'];
      token = config[channel === 2 ? 'DRUMMER_TOKEN' : 'SIGNAL_TOKEN'] || '';
      if (!url) { fail('Set the HTTPS feed in phone settings.'); return; }
      if (!https(url) || typeof token !== 'string' || /[\r\n]/.test(token) || token.length > 512) { fail('Invalid HTTPS feed settings.'); return; }
    }
    fetchSource();
    function fetchWeather(lat, lon) {
      url = 'https://api.open-meteo.com/v1/forecast?latitude=' + lat + '&longitude=' + lon +
        '&hourly=rain,showers&forecast_days=2&timezone=UTC&timeformat=unixtime&precipitation_unit=mm';
      fetchSource();
    }
    function fetchSource() {
    if (ticket !== generation) return;
    var xhr = deps.xhr(), settled = false;
    active = xhr;
    function finish() { if (settled || ticket !== generation) return false; settled = true; active = null; return true; }
    xhr.onload = function () {
      if (!finish()) return;
      var now = Math.floor(deps.now() / 1000);
      if (xhr.status !== 200) { fail('Source unavailable. Try again.'); return; }
      if (xhr.responseText.length > 65536 || now - started > 20) { fail('Response too large or too late.'); return; }
      try {
        var raw = JSON.parse(xhr.responseText);
        respond(channel === 1 ? weather(raw, now) : normalize(raw, now));
      } catch (e) { fail('Missing, invalid or old source data.'); }
    };
    xhr.onerror = xhr.ontimeout = function () { if (finish()) fail('No connection to source.'); };
    try {
      xhr.open('GET', url, true); xhr.timeout = channel === 1 ? 8000 : 12000;
      if (token) xhr.setRequestHeader('Authorization', 'Bearer ' + token);
      xhr.send();
    } catch (e) { if (finish()) fail('Could not request this source.'); }
    }
  }
  return {request: request, cancel: cancel};
}
module.exports = {create: create, normalize: normalize, weather: weather, wire: wire, https: https};
