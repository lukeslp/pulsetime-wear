// Flatten a fixtures JSON file to one case per line so the C harness
// (tests/check_fixtures.c) can read it without a JSON parser:
//   patterns.json:        <system> <speed> <precision 0|1> <hour> <minute> <pattern...>
//   patterns-scoped.json: <system> <speed> <precision 0|1> <scope> <hour> <minute> <pattern...>
// The scope column is inserted only when the cases carry one, so patterns.txt
// keeps the exact shape the Garmin and Kotlin harnesses already parse.
// Usage: node tests/flatten-fixtures.mjs <patterns.json> <out.txt>
import fs from 'node:fs';
const [, , input, output] = process.argv;
if (!input || !output) { console.error('usage: flatten-fixtures.mjs <patterns.json> <out.txt>'); process.exit(2); }
const doc = JSON.parse(fs.readFileSync(input, 'utf8'));
const lines = doc.cases.map((c) => {
  const scope = c.scope ? ` ${c.scope}` : '';
  return `${c.system} ${c.speed} ${c.includePrecision ? 1 : 0}${scope} ${c.hour} ${c.minute} ${c.pattern}`;
});
fs.writeFileSync(output, lines.join('\n') + '\n');
console.log(`flattened ${lines.length} cases -> ${output}`);
