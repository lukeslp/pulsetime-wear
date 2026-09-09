"""Read-only declaration/linker diagnostic for the installed new-Pebble SDK."""
import json
from pathlib import Path
import subprocess

sdk = Path.home() / 'Library/Application Support/Pebble SDK/SDKs/4.33.1'
symbol = 'vibes_enqueue_custom_pattern_with_amplitudes'
nm = next((sdk / 'toolchain').rglob('arm-none-eabi-nm'))
for target in ('emery', 'flint', 'gabbro'):
    platform = sdk / 'sdk-core/pebble' / target
    header = platform / 'include/pebble.h'
    lib = next(platform.rglob('libpebble.a'))
    result = subprocess.run([str(nm), str(lib)], capture_output=True, text=True, check=True)
    print(json.dumps(dict(target=target, sdk='4.33.1',
        declaration=symbol in header.read_text(), linkerSymbol=symbol in result.stdout)))
