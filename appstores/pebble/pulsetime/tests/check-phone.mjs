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
const plain = value => JSON.parse(JSON.stringify(value));

function Clay(actualConfig) {
  assert.equal(actualConfig, config);
  this.setSettings = settings => updates.push(plain(settings));
}
vm.runInNewContext(source, {
  require(name) {
    if (name === './vendor/pebble-clay') return Clay;
    if (name === './config.json') return config;
    throw new Error(`Unexpected import: ${name}`);
  },
  Pebble: {
    addEventListener: (event, handler) => { events[event] = handler; },
    sendAppMessage: message => messages.push(plain(message))
  },
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
assert.equal(fields.get('SYSTEM').defaultValue, 'terse_quarter');
assert.equal(fields.get('EXACT_MINUTES').defaultValue, false);
assert.equal(fields.get('SPEED').defaultValue, 'standard');
assert.equal(fields.get('THEME').defaultValue, 'bright');
assert.equal(fields.get('STYLE').defaultValue, 'bold');
assert.deepEqual(metadata.pebble.messageKeys, [
  'SYSTEM', 'SPEED', 'EXACT_MINUTES', 'FLICK_LAUNCH', 'QL_UP', 'QL_DOWN',
  'SOUND', 'VOLUME', 'CHIME', 'QUIET_FROM', 'QUIET_TO', 'REQUEST', 'THEME', 'STYLE'
], 'Existing message ids must stay stable; display keys are appended');
for (const key of fields.keys()) {
  assert(metadata.pebble.messageKeys.includes(key), `${key} is missing from package message keys`);
}

console.log('Phone contract: every numeric/string choice, defaults, exact toggle, display mirroring, and stable message keys passed.');
