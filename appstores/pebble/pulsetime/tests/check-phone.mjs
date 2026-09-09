// Verify the PebbleKit JS/Clay settings contract without a phone or network.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';

const config = JSON.parse(fs.readFileSync(new URL('../src/pkjs/config.json', import.meta.url), 'utf8'));
const metadata = JSON.parse(fs.readFileSync(new URL('../package.json', import.meta.url), 'utf8'));
const source = fs.readFileSync(new URL('../src/pkjs/index.js', import.meta.url), 'utf8');
const events = {};
const updates = [];
const messages = [];
let stored = {};
let cancellations = 0;
let locationRequests = [], clayInstance;
const plain = value => JSON.parse(JSON.stringify(value));

function Clay(actualConfig) {
  assert.equal(actualConfig, config);
  this.config = plain(actualConfig);
  clayInstance = this;
  this.setSettings = settings => {
    updates.push(plain(settings));
    Object.assign(stored, plain(settings));
  };
  this.generateUrl = () => 'data:configuration';
}
vm.runInNewContext(source, {
  require(name) {
    if (name === './vendor/pebble-clay') return Clay;
    if (name === './config.json') return config;
    if (name === './location') return (_geo, success, failure) => { const r = {success, failure, cancelled: false}; locationRequests.push(r); return () => { r.cancelled = true; }; };
    if (name === './signals') return {create: () => ({cancel: () => cancellations++, request: () => {}})};
    throw new Error(`Unexpected import: ${name}`);
  },
  Pebble: {
    openURL: () => {},
    addEventListener: (event, handler) => { events[event] = handler; },
    sendAppMessage: message => messages.push(plain(message))
  },
  localStorage: {getItem: () => JSON.stringify(stored)},
  console
});

events.ready();
assert.deepEqual(messages, [{ REQUEST: 1 }], 'Startup must request the watch settings');

const choices = {
  SYSTEM: ['terse_quarter', 'five_minute_ladder', 'decimal_place', 'binary_clock', 'nearest_quarter', 'morse_digits'],
  SPEED: ['slow', 'standard', 'fast'],
  QL_UP: ['full', 'hours', 'minutes'],
  QL_DOWN: ['full', 'hours', 'minutes'],
  CHIME: ['off', 'hour', 'half', 'quarter'],
  SOUND: ['off', 'with', 'only'],
  THEME: ['bright', 'dark'],
  STYLE: ['bold', 'classic']
};
const fields = new Map();
function visit(items) {
  for (const item of items) {
    if (item.messageKey) {
      assert(!fields.has(item.messageKey), `Duplicate config field ${item.messageKey}`);
      fields.set(item.messageKey, item);
    }
    if (item.items) visit(item.items);
  }
}
visit(config);

for (const [key, values] of Object.entries(choices)) {
  assert.deepEqual(fields.get(key).options.map(option => option.value), values,
                   `${key} options must keep the watch enum order and storage ids`);
  for (const [index, value] of values.entries()) {
    for (const wireValue of [index, value]) {
      events.appmessage({ payload: { [key]: wireValue } });
      assert.deepEqual(updates.at(-1), { [key]: value }, `${key} failed to mirror ${wireValue}`);
    }
  }
  const previousCount = updates.length;
  for (const invalid of [-1, values.length, 'unknown']) {
    events.appmessage({ payload: { [key]: invalid } });
  }
  assert.equal(updates.length, previousCount, `Invalid ${key} must not change saved Clay choices`);
}

events.appmessage({ payload: {
  SYSTEM: 0, EXACT_MINUTES: 1, FLICK_LAUNCH: 0, QUIET_FROM: 22, QUIET_TO: 7,
  VOLUME: 2, THEME: 'dark', STYLE: 'classic'
} });
assert.deepEqual(updates.at(-1), {
  SYSTEM: 'terse_quarter', EXACT_MINUTES: true, FLICK_LAUNCH: false,
  THEME: 'dark', STYLE: 'classic', QUIET_FROM: '22', QUIET_TO: '7', VOLUME: '2'
});
assert(!fields.has('DRUMMER_TOKEN'), 'No Drummer token field in public settings');
assert.equal(fields.get('WEATHER_LOCATION').defaultValue, 'phone');
assert.equal(config.at(-2).items[0].defaultValue, 'Advanced · Fixed place and data feeds');
assert.equal(fields.get('SYSTEM').defaultValue, 'terse_quarter');
assert.equal(fields.get('EXACT_MINUTES').defaultValue, false);
assert.equal(fields.get('SPEED').defaultValue, 'standard');
assert.equal(fields.get('THEME').defaultValue, 'bright');
assert.equal(fields.get('STYLE').defaultValue, 'bold');
assert.deepEqual(metadata.pebble.messageKeys.slice(0, 14), [
  'SYSTEM', 'SPEED', 'EXACT_MINUTES', 'FLICK_LAUNCH', 'QL_UP', 'QL_DOWN',
  'SOUND', 'VOLUME', 'CHIME', 'QUIET_FROM', 'QUIET_TO', 'REQUEST', 'THEME', 'STYLE'
], 'Existing message ids must stay stable');
for (const key of fields.keys()) {
  assert(metadata.pebble.messageKeys.includes(key), `${key} is missing from package message keys`);
}

// Saving a form opened before a watch change must preserve the newer watch value.
stored = {SYSTEM: 'terse_quarter', SPEED: 'standard'};
events.showConfiguration();
events.appmessage({payload: {SYSTEM: 'binary_clock'}});
const priorMessages = messages.length;
events.webviewclosed({response: encodeURIComponent(JSON.stringify({
  SYSTEM: 'terse_quarter', SPEED: 'fast', WEATHER_ENABLED: true,
  WEATHER_LAT: '45', WEATHER_LON: '-122', DRUMMER_TOKEN: 'private%token',
  SIGNAL_TOKEN: 'other secret', SIGNAL_URL: 'https://example.com/feed'
}))});
assert.equal(stored.SYSTEM, 'binary_clock');
assert.equal(stored.SPEED, 'fast');
assert.equal(stored.DRUMMER_TOKEN, 'private%token');
assert.deepEqual(messages.slice(priorMessages), [{SPEED: 'fast'}], 'Secrets and location stay on phone');
assert.equal(cancellations, 1);
events.showConfiguration();
events.webviewclosed({response: JSON.stringify({SIGNAL_TOKEN: 'literal%token'})});
assert.equal(stored.SIGNAL_TOKEN, 'literal%token');
assert.deepEqual(metadata.pebble.targetPlatforms, ['emery', 'flint', 'gabbro']);
console.log('Phone contract: choices, defaults, stable ids, private settings, and stale-form merge passed.');

assert.equal(stored.WEATHER_LOCATION, 'phone');
const pendingLocation = locationRequests.at(-1);
events.webviewclosed({response: JSON.stringify({WEATHER_ENABLED: false})});
assert(pendingLocation.cancelled);
pendingLocation.success({latitude:45,longitude:-122});
assert.equal(stored.WEATHER_STATUS, 'Weather is off. Time and lessons remain available.');
events.showConfiguration();
assert.equal(clayInstance.config.flatMap(x => x.items || []).find(x => x.id === 'weather-status').defaultValue, stored.WEATHER_STATUS);
const count = locationRequests.length;
events.webviewclosed({response: JSON.stringify({WEATHER_ENABLED:true, WEATHER_LOCATION:'fixed'})});
assert.equal(locationRequests.length,count);
assert(!JSON.stringify(messages).includes('WEATHER_STATUS'));
console.log('Setup: disabling cancels location, stale success cannot restore ready, fixed mode skips permission, and rendered status stays current.');
