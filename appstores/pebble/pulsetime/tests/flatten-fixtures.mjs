// Flatten appstores/shared/fixtures/patterns.json to one case per line so the C
// harness (tests/check_fixtures.c) can read it without a JSON parser:
//   <system> <speed> <includePrecision 0|1> <hour> <minute> <pattern...>
// Usage: node tests/flatten-fixtures.mjs <patterns.json> <out.txt>
import fs from 'node:fs';
const [, , input, output] = process.argv;
if (!input || !output) { console.error('usage: flatten-fixtures.mjs <patterns.json> <out.txt>'); process.exit(2); }
const doc = JSON.parse(fs.readFileSync(input, 'utf8'));
const lines = doc.cases.map((c) => `${c.system} ${c.speed} ${c.includePrecision ? 1 : 0} ${c.hour} ${c.minute} ${c.pattern}`);
fs.writeFileSync(output, lines.join('\n') + '\n');
console.log(`flattened ${lines.length} cases -> ${output}`);
