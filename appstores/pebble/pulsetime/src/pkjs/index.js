// PulseTime — phone-side companion (PebbleKit JS).
// The watch does all the work; the phone only hosts the Clay configuration
// page (time system, playback speed, exact minutes, flick launch) and mirrors whatever the
// watch reports back so the page never shows a stale choice after the user
// cycles systems with Up/Down on the watch itself.

'use strict';

var Clay = require('./vendor/pebble-clay');
var clayConfig = require('./config.json');
var clay = new Clay(clayConfig); // auto-handles showConfiguration + webviewclosed

var SYSTEMS = ['terse_quarter', 'five_minute_ladder', 'decimal_place', 'binary_clock', 'nearest_quarter', 'morse_digits'];
var SPEEDS = ['slow', 'standard', 'fast'];

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
