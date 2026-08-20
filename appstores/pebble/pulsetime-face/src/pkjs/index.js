// PulseTime Face — phone-side companion (PebbleKit JS).
// The watch does all the work; the phone only hosts the Clay configuration page
// and mirrors whatever the watch reports back, so the page never shows a stale
// choice. A watchface has no buttons, so unlike the watchapp every setting here
// can ONLY be changed from this page.

'use strict';

var Clay = require('./vendor/pebble-clay');
var clayConfig = require('./config.json');
var clay = new Clay(clayConfig); // auto-handles showConfiguration + webviewclosed

var SYSTEMS = ['terse_quarter', 'five_minute_ladder', 'decimal_place', 'binary_clock', 'nearest_quarter', 'morse_digits'];
var SPEEDS = ['slow', 'standard', 'fast'];
var CHIMES = ['off', 'hour', 'half', 'quarter'];
var SOUNDS = ['off', 'with', 'only'];

function pick(value, list) {
  var v = typeof value === 'number' ? list[value] : String(value);
  return list.indexOf(v) !== -1 ? v : null;
}

function mirror(payload) {
  var settings = {};
  if (payload.SYSTEM !== undefined) {
    var sys = pick(payload.SYSTEM, SYSTEMS);
    if (sys) settings.SYSTEM = sys;
  }
  if (payload.SPEED !== undefined) {
    var spd = pick(payload.SPEED, SPEEDS);
    if (spd) settings.SPEED = spd;
  }
  if (payload.CHIME !== undefined) {
    var chime = pick(payload.CHIME, CHIMES);
    if (chime) settings.CHIME = chime;
  }
  if (payload.SOUND !== undefined) {
    var sound = pick(payload.SOUND, SOUNDS);
    if (sound) settings.SOUND = sound;
  }
  if (payload.EXACT_MINUTES !== undefined) settings.EXACT_MINUTES = !!Number(payload.EXACT_MINUTES);
  if (payload.TAP_TELL !== undefined) settings.TAP_TELL = !!Number(payload.TAP_TELL);
  if (payload.QUIET_FROM !== undefined) settings.QUIET_FROM = String(Number(payload.QUIET_FROM));
  if (payload.QUIET_TO !== undefined) settings.QUIET_TO = String(Number(payload.QUIET_TO));
  if (payload.VOLUME !== undefined) settings.VOLUME = String(Number(payload.VOLUME));
  if (Object.keys(settings).length) clay.setSettings(settings);
}

Pebble.addEventListener('ready', function () {
  Pebble.sendAppMessage({ REQUEST: 1 }, function () {}, function (e) {
    console.log('PulseTime Face: request failed ' + JSON.stringify(e));
  });
});

Pebble.addEventListener('appmessage', function (e) {
  if (e && e.payload) mirror(e.payload);
});
