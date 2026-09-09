// PulseTime — phone-side companion (PebbleKit JS).
// The watch encodes time and signals locally. The phone fetches requested data
// and keeps feed credentials and manual weather coordinates off the watch.

'use strict';

var Clay = require('./vendor/pebble-clay');
var clayConfig = require('./config.json');
var clay = new Clay(clayConfig, null, {autoHandleEvents: false});
var locate = require('./location');
var setupGeneration = 0, cancelSetup = null;
function phoneLocation(success, failure) {
  return locate(typeof navigator === 'undefined' ? null : navigator.geolocation, success, failure);
}
function settings() {
  try { return JSON.parse(localStorage.getItem('clay-settings')) || {}; } catch (e) { return {}; }
}
var signals = require('./signals').create({settings: settings, now: Date.now,
  locate: phoneLocation,
  xhr: function () { return new XMLHttpRequest(); },
  send: function (data, success, failure) { Pebble.sendAppMessage(data, success, failure); }});
var watchKeys = ['SYSTEM','SPEED','EXACT_MINUTES','FLICK_LAUNCH','QL_UP','QL_DOWN',
  'SOUND','VOLUME','CHIME','QUIET_FROM','QUIET_TO','THEME','STYLE','FAVORITE_SIGNAL'];
var formBaseline = null;
Pebble.addEventListener('showConfiguration', function () {
  formBaseline = settings();
  clay.config.forEach(function (section) {
    (section.items || []).forEach(function (item) {
      if (item.id === 'weather-status') item.defaultValue = formBaseline.WEATHER_STATUS || 'Location has not been checked yet.';
    });
  });
  Pebble.openURL(clay.generateUrl());
});
Pebble.addEventListener('webviewclosed', function (event) {
  if (!event || !event.response) return;
  try {
    var raw = JSON.parse(event.response.charAt(0) === '{' ? event.response : decodeURIComponent(event.response));
    var current = settings(), changed = {};
    Object.keys(raw).forEach(function (key) {
      var value = raw[key] && typeof raw[key] === 'object' ? raw[key].value : raw[key];
      // Only edited fields overwrite current watch values. An old open phone
      // form must not revert a language subsequently changed on the watch.
      if (!formBaseline || value !== formBaseline[key]) {
        current[key] = value;
        if (watchKeys.indexOf(key) !== -1) changed[key] = typeof value === 'boolean' ? (value ? 1 : 0) : value;
      }
    });
    if (!current.WEATHER_LOCATION) current.WEATHER_LOCATION = 'phone';
    clay.setSettings(current); signals.cancel();
    var revision = ++setupGeneration;
    if (cancelSetup) cancelSetup(); cancelSetup = null;
    if (current.WEATHER_ENABLED === true || current.WEATHER_ENABLED === 1) {
      if (current.WEATHER_LOCATION === 'fixed') clay.setSettings({WEATHER_STATUS: 'Fixed place selected. Phone location will not be requested.'});
      else {
        clay.setSettings({WEATHER_STATUS: 'Checking phone location. Reopen settings to see the result.'});
        cancelSetup = phoneLocation(function () {
          if (revision === setupGeneration) clay.setSettings({WEATHER_STATUS: 'Location is ready. Ask for Weather on your watch.'});
        }, function (message) {
          if (revision === setupGeneration) clay.setSettings({WEATHER_STATUS: message});
        });
      }
    } else clay.setSettings({WEATHER_STATUS: 'Weather is off. Time and lessons remain available.'});
    if (Object.keys(changed).length) Pebble.sendAppMessage(changed, function () {}, function () {});
    formBaseline = null;
  } catch (e) { console.log('PulseTime: settings were not saved; invalid response.'); }
});

var SYSTEMS = ['terse_quarter', 'five_minute_ladder', 'decimal_place', 'binary_clock', 'nearest_quarter', 'morse_digits'];
var SPEEDS = ['slow', 'standard', 'fast'];
var SCOPES = ['full', 'hours', 'minutes', 'favorite'];
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
  if (payload.FAVORITE_SIGNAL !== undefined) {
    var favorite = pick(payload.FAVORITE_SIGNAL, ['weather', 'drummer', 'custom']);
    if (favorite) settings.FAVORITE_SIGNAL = favorite;
  }
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
  if (!e || !e.payload) return;
  if (e.payload.SIG_CANCEL !== undefined) signals.cancel();
  if (e.payload.SIG_REQUEST !== undefined) signals.request(e.payload.SIG_REQUEST);
  mirror(e.payload);
});
