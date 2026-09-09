'use strict';

// One position, no tracking or persistence. The host owns the permission UI.
function locate(geo, success, failure) {
  var settled = false, timer;
  function finish(callback, value) {
    if (settled) return;
    settled = true; clearTimeout(timer); callback(value);
  }
  timer = setTimeout(function () { finish(failure, 'Location timed out. Open Pebble on phone and retry.'); }, 7000);
  if (!geo || typeof geo.getCurrentPosition !== 'function') {
    finish(failure, 'Location unavailable. Use Fixed place in Advanced settings.');
  } else try {
    geo.getCurrentPosition(function (position) {
      var c = position && position.coords;
      if (!c || typeof c.latitude !== 'number' || typeof c.longitude !== 'number' ||
          !isFinite(c.latitude) || !isFinite(c.longitude) || Math.abs(c.latitude) > 90 || Math.abs(c.longitude) > 180 ||
          (position.timestamp !== undefined && (!isFinite(position.timestamp) || Date.now() - position.timestamp > 300000))) {
        finish(failure, 'No current location. Open Pebble on phone and retry.'); return;
      }
      // Roughly kilometre-scale coordinates are sufficient for this forecast.
      finish(success, {latitude: Math.round(c.latitude * 100) / 100, longitude: Math.round(c.longitude * 100) / 100});
    }, function (error) {
      finish(failure, error && error.code === 1 ? 'Allow Pebble location in phone settings, then retry.' :
        'Location unavailable. Check phone location settings and retry.');
    }, {enableHighAccuracy: false, maximumAge: 60000, timeout: 6500});
  } catch (error) { finish(failure, 'Location unavailable. Check Pebble permission on phone.'); }
  return function () { settled = true; clearTimeout(timer); };
}
module.exports = locate;
