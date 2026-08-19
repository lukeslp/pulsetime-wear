// Reference port of shared/src/main/java/com/lukesteuber/pulsetime/shared/encoding/
// (TimeSystems.kt + TerseQuarterEncoder.kt) used to generate fixtures that the
// Garmin (Monkey C) and Pebble (C) ports must reproduce exactly. Amplitude is
// expressed as a relative `layer` (1, .72, .5, .45) rather than a 0–255 value so
// each platform maps strength its own way; `on`/`off` are milliseconds after the
// playback-speed multiplier, exactly as the Kotlin computes them.
//
//   node appstores/shared/pulsetime-patterns.mjs            # write fixtures/patterns.json
//   node appstores/shared/pulsetime-patterns.mjs --check    # fail on drift
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

export const SYSTEMS = ['terse_quarter', 'five_minute_ladder', 'decimal_place', 'binary_clock', 'nearest_quarter', 'morse_digits'];
export const SPEEDS = { slow: 1.25, standard: 1, fast: 0.78 };

const FIELD_GAP = 700, GROUP_GAP = 420, SHORT_GAP = 150;
const T = { HOUR_ON: 200, HOUR_OFF: 280, GAP: 700, QUARTER_ON: 320, QUARTER_OFF: 280, MINUTE_ON: 80, MINUTE_OFF: 120 };

// Kotlin: `Long * Float` is a single-precision multiply, then .toLong() truncates
// toward zero. Ports must use float32 (Monkey C Float, C float) and truncate,
// never round; the fixtures are the arbiter (e.g. 200 * 0.78f rounds to exactly
// 156.0f in float32, so 156).
const scale = (ms, mult) => Math.trunc(Math.fround(ms * Math.fround(mult)));
export const hour12 = (h) => (h % 12 === 0 ? 12 : h % 12);

function pulse(on, off, mult, layer = 1) {
  return { on: Math.max(1, scale(on, mult)), off: off > 0 ? Math.max(1, scale(off, mult)) : 0, layer };
}
function gap(ms, mult) {
  return { on: 0, off: Math.max(1, scale(ms, mult)), layer: 0 };
}
function counted(out, count, on, off, mult, layer, groupAt = null) {
  for (let i = 0; i < count; i++) {
    out.push(pulse(on, i === count - 1 ? 0 : off, mult, layer));
    if (groupAt != null && i + 1 < count && (i + 1) % groupAt === 0) out.push(gap(GROUP_GAP, mult));
  }
}

function terseQuarter(h24, m, mult, includePrecision) {
  const h = hour12(h24), q = Math.floor(m / 15), rem = m % 15;
  const out = [];
  for (let i = 0; i < h; i++) out.push({ on: T.HOUR_ON, off: i < h - 1 ? T.HOUR_OFF : 0, layer: 1 });
  out.push({ on: 0, off: T.GAP, layer: 0 });
  for (let i = 0; i < q; i++) out.push({ on: T.QUARTER_ON, off: i < q - 1 ? T.QUARTER_OFF : 0, layer: 1 });
  if (includePrecision && rem > 0) {
    out.push({ on: 0, off: T.GAP / 2, layer: 0 });
    for (let i = 0; i < rem; i++) out.push({ on: T.MINUTE_ON, off: i < rem - 1 ? T.MINUTE_OFF : 0, layer: 1 });
  }
  // withSpeed(): applied afterwards to every non-zero on/off.
  if (mult !== 1) for (const s of out) { if (s.on) s.on = Math.max(1, scale(s.on, mult)); if (s.off) s.off = Math.max(1, scale(s.off, mult)); }
  return out;
}
function fiveMinuteLadder(h24, m, mult) {
  const out = [];
  counted(out, hour12(h24), 240, 190, mult, 1, 5);
  out.push(gap(FIELD_GAP, mult));
  counted(out, Math.floor(m / 5), 145, 145, mult, 0.72, 5);
  if (m % 5 > 0) { out.push(gap(GROUP_GAP, mult)); counted(out, m % 5, 75, 110, mult, 0.45); }
  return out;
}
function addDigit(out, digit, mult, on, layer) {
  if (digit === 0) out.push(pulse(on * 2, 0, mult, layer));
  else counted(out, digit, on, SHORT_GAP, mult, layer, 5);
}
function decimalPlace(h24, m, mult) {
  const out = [];
  counted(out, hour12(h24), 230, 180, mult, 1, 5);
  out.push(gap(FIELD_GAP, mult));
  addDigit(out, Math.floor(m / 10), mult, 155, 0.72);
  out.push(gap(GROUP_GAP, mult));
  addDigit(out, m % 10, mult, 80, 0.45);
  return out;
}
function addBits(out, value, width, mult) {
  for (let bit = width - 1; bit >= 0; bit--) {
    const one = (value & (1 << bit)) !== 0;
    out.push(pulse(one ? 210 : 75, SHORT_GAP, mult, one ? 1 : 0.45));
  }
}
function binaryClock(h24, m, mult) {
  const out = [];
  addBits(out, hour12(h24), 4, mult);
  out.push(gap(FIELD_GAP, mult));
  addBits(out, m, 6, mult);
  return out;
}
function nearestQuarter(h24, m, mult) {
  let landmark = Math.floor((m + 7) / 15) * 15;
  let landmarkHour = h24;
  if (landmark === 60) { landmark = 0; landmarkHour = (h24 + 1) % 24; }
  const absoluteNow = h24 * 60 + m;
  let absoluteLandmark = landmarkHour * 60 + landmark;
  if (h24 === 23 && landmarkHour === 0) absoluteLandmark = 24 * 60;
  const offset = absoluteNow - absoluteLandmark;
  const out = [];
  counted(out, hour12(landmarkHour), 230, 180, mult, 1, 5);
  out.push(gap(FIELD_GAP, mult));
  const landmarkCount = landmark === 0 ? 1 : landmark / 15 + 1;
  counted(out, landmarkCount, 150, 140, mult, 0.72);
  if (offset !== 0) {
    out.push(gap(GROUP_GAP, mult));
    if (offset > 0) { out.push(pulse(75, 100, mult, 0.45)); out.push(pulse(160, 0, mult, 1)); }
    else { out.push(pulse(160, 100, mult, 1)); out.push(pulse(75, 0, mult, 0.45)); }
    out.push(gap(GROUP_GAP, mult));
    counted(out, Math.abs(offset), 70, 105, mult, 0.45, 5);
  }
  return out;
}
const MORSE = ['-----', '.----', '..---', '...--', '....-', '.....', '-....', '--...', '---..', '----.'];
function addCode(out, digit, mult) {
  for (const mark of MORSE[digit]) {
    const dash = mark === '-';
    out.push(pulse(dash ? 230 : 75, SHORT_GAP, mult, dash ? 1 : 0.5));
  }
}
function morseDigits(h24, m, mult) {
  const hour = String(hour12(h24)); const minutes = String(m).padStart(2, '0');
  const out = [];
  [...hour].forEach((c, i) => { addCode(out, Number(c), mult); if (i < hour.length - 1) out.push(gap(GROUP_GAP, mult)); });
  out.push(gap(FIELD_GAP, mult));
  [...minutes].forEach((c, i) => { addCode(out, Number(c), mult); if (i < minutes.length - 1) out.push(gap(GROUP_GAP, mult)); });
  return out;
}

export function encode(system, h24, m, { speed = 'standard', includePrecision = false } = {}) {
  const mult = SPEEDS[speed];
  switch (system) {
    case 'terse_quarter': return terseQuarter(h24, m, mult, includePrecision);
    case 'five_minute_ladder': return fiveMinuteLadder(h24, m, mult);
    case 'decimal_place': return decimalPlace(h24, m, mult);
    case 'binary_clock': return binaryClock(h24, m, mult);
    case 'nearest_quarter': return nearestQuarter(h24, m, mult);
    case 'morse_digits': return morseDigits(h24, m, mult);
    default: throw new Error(`unknown system ${system}`);
  }
}

/** Compact wire form: "on/off/layer" segments joined by spaces, layer as 100/72/50/45/0. */
export const compact = (segs) => segs.map((s) => `${s.on}/${s.off}/${Math.round(s.layer * 100)}`).join(' ');

export const SAMPLE_TIMES = [
  [0, 0], [0, 1], [0, 7], [0, 8], [1, 15], [3, 20], [6, 30], [9, 44], [11, 45], [11, 59],
  [12, 0], [12, 33], [13, 5], [16, 52], [18, 10], [21, 38], [23, 53], [23, 58], [7, 22], [10, 49],
];

const here = path.dirname(fileURLToPath(import.meta.url));
const fixturePath = path.join(here, 'fixtures', 'patterns.json');
if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const cases = [];
  for (const system of SYSTEMS) for (const speed of Object.keys(SPEEDS)) for (const [h, m] of SAMPLE_TIMES) {
    const variants = system === 'terse_quarter' ? [false, true] : [false];
    for (const includePrecision of variants) {
      cases.push({ system, speed, includePrecision, hour: h, minute: m, pattern: compact(encode(system, h, m, { speed, includePrecision })) });
    }
  }
  const json = JSON.stringify({ version: 1, source: 'shared/src/main/java/com/lukesteuber/pulsetime/shared/encoding', note: 'segments as on/off/layer (ms, ms, relative strength %); layer 0 = silence', cases }, null, 1) + '\n';
  // Line-oriented twin for the Kotlin/C/Monkey C harnesses (no JSON parser needed):
  // system speed precision hour minute pattern
  const txt = cases.map((c) => `${c.system} ${c.speed} ${c.includePrecision ? 1 : 0} ${c.hour} ${c.minute} ${c.pattern}`).join('\n') + '\n';
  const txtPath = fixturePath.replace(/\.json$/, '.txt');
  if (process.argv.includes('--check')) {
    const cur = fs.existsSync(fixturePath) ? fs.readFileSync(fixturePath, 'utf8') : '';
    const curTxt = fs.existsSync(txtPath) ? fs.readFileSync(txtPath, 'utf8') : '';
    if (cur !== json || curTxt !== txt) { console.error('stale fixtures'); process.exit(1); }
    console.log(`ok: ${cases.length} cases current`);
  } else {
    fs.mkdirSync(path.dirname(fixturePath), { recursive: true });
    fs.writeFileSync(fixturePath, json);
    fs.writeFileSync(txtPath, txt);
    console.log(`wrote ${cases.length} cases to appstores/shared/fixtures/patterns.{json,txt}`);
  }
}
