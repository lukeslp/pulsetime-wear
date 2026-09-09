# Proposed PebbleOS amplitude export

`v4.36.0-export-proposal.patch` is an unapplied upstream proposal, based on
[PebbleOS v4.36.0](https://github.com/coredevices/PebbleOS/tree/v4.36.0).
It adds the already implemented public amplitude type and function to the
Vibes export group, advances export revision 109 to 110, and increments the
SDK minor number as directed by that file's notes.

The affected source is `src/fw/applib/ui/vibes.h` and `vibes.c`; the missing
catalog entries are in `tools/generate_native_sdk/exported_symbols.json`.
The header describes 0–100 amplitude and says custom amplitudes bypass the
watch's strength setting. Motor braking remains an internal OS facility.

This proposal has not been built into firmware or a generated SDK. It needs
upstream review, ABI/export generation checks, compatible firmware, and real
watch validation before an application may depend on it. No watch was patched.
No undocumented trampoline or fixed firmware address is used in PulseTime.

To reproduce the installed SDK gap, run:

```sh
python3 docs/pebble-amplitude-export/check-sdk.py
```

The diagnostic looks for both the declaration and linker symbol for Time 2,
2 Duo, and Round 2. SDK 4.33.1 lacks both on the tested installation. Merely
copying a declaration into the app does not make it link or run. Once a
supported SDK exists, integrate it with a firmware compatibility boundary;
do not infer runtime support from a changelog or the watch model alone.
