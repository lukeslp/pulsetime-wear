// PulseTime — phone-side companion (PebbleKit JS).
// The watch does all the work; the phone only hosts the Clay configuration page
// and mirrors whatever the watch reports back, so the page never shows a stale
// choice after the wearer changes the language, speed, or display on the watch.

'use strict';

var Clay = require('./vendor/pebble-clay');
var clayConfig = require('./config.json');
var clay = new Clay(clayConfig); // auto-handles showConfiguration + webviewclosed

var SYSTEMS = ['terse_quarter', 'five_minute_ladder', 'decimal_place', 'binary_clock', 'nearest_quarter', 'morse_digits'];
var SPEEDS = ['slow', 'standard', 'fast'];
var SCOPES = ['full', 'hours', 'minutes'];
var CHIMES = ['off', 'hour', 'half', 'quarter'];
var SOUNDS = ['off', 'with', 'only'];
var THEMES = ['bright', 'dark'];
var STYLES = ['bold', 'classic'];

function pick(value, list) {
  var v = typeof value === 'number' ? list[value] : String(value);
  return list.indexOf(v) !== -1 ? v : null;
}

function mirror(payload) {
  var settings = {};
  if (payload.SYSTEM !== undefined) {
    var sys = typeof payload.SYSTEM === 'number' ? SYSTEMS[payload.SYSTEM] : String(payload.SYSTEM);
    if (SYSTEMS.indexOf(sys) !== -1) settings.SYSTEM = sys;
  }
  if (payload.SPEED !== undefined) {
    var spd = typeof payload.SPEED === 'number' ? SPEEDS[payload.SPEED] : String(payload.SPEED);
    if (SPEEDS.indexOf(spd) !== -1) settings.SPEED = spd;
  }
  if (payload.EXACT_MINUTES !== undefined) settings.EXACT_MINUTES = !!Number(payload.EXACT_MINUTES);
  if (payload.FLICK_LAUNCH !== undefined) settings.FLICK_LAUNCH = !!Number(payload.FLICK_LAUNCH);
  ['QL_UP', 'QL_DOWN'].forEach(function (key) {
    if (payload[key] === undefined) return;
    var scope = pick(payload[key], SCOPES);
    if (scope) settings[key] = scope;
  });
  if (payload.CHIME !== undefined) {
    var chime = pick(payload.CHIME, CHIMES);
    if (chime) settings.CHIME = chime;
  }
  if (payload.SOUND !== undefined) {
    var sound = pick(payload.SOUND, SOUNDS);
    if (sound) settings.SOUND = sound;
  }
  if (payload.THEME !== undefined) {
    var theme = pick(payload.THEME, THEMES);
    if (theme) settings.THEME = theme;
  }
  if (payload.STYLE !== undefined) {
    var style = pick(payload.STYLE, STYLES);
    if (style) settings.STYLE = style;
  }
  if (payload.QUIET_FROM !== undefined) settings.QUIET_FROM = String(Number(payload.QUIET_FROM));
  if (payload.QUIET_TO !== undefined) settings.QUIET_TO = String(Number(payload.QUIET_TO));
  if (payload.VOLUME !== undefined) settings.VOLUME = String(Number(payload.VOLUME));
  if (Object.keys(settings).length) clay.setSettings(settings);
}

Pebble.addEventListener('ready', function () {
  // Ask the watch what it is currently using so the config page starts in step.
  Pebble.sendAppMessage({ REQUEST: 1 }, function () {}, function (e) {
    console.log('PulseTime: request failed ' + JSON.stringify(e));
  });
});

Pebble.addEventListener('appmessage', function (e) {
  if (e && e.payload) mirror(e.payload);
});
