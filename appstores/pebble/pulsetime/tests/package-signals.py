"""Freeze the tested PBW, native evidence, and an independently rebuilt source ZIP."""
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import zipfile

root = Path(__file__).resolve().parents[1]
repo = root.parents[2]
pbw = root / 'build/pulsetime.pbw'
digest = hashlib.sha256(pbw.read_bytes()).hexdigest()
version = json.loads((root / 'package.json').read_text())['version']
out = repo / f'appstores/dist/pebble/signals-{version}' / digest[:12]
out.mkdir(parents=True, exist_ok=True)
evidence_files = []
for target in ('emery', 'flint', 'gabbro'):
    report_file = root / f'build/signals-evidence/{target}-report.json'
    report = json.loads(report_file.read_text())
    assert report['pbwSHA256'] == digest and report['liveWeather'], 'Native evidence does not match this PBW'
    assert report['idleExpiryChecked'] and report['failedSourceAndRestartChecked'], 'Freshness checks are missing'
    assert report['authenticatedChannels'] == [2, 3], 'Authenticated source checks are missing'
    assert report['balancedComparisonChecked'] and report['favoriteMenuChecked'], 'Favorite and comparison checks are missing'
    evidence_files.append(report_file)
    for frame in report['frames']:
        assert Path(frame).name == frame and frame.startswith(target + '-') and frame.endswith('.png')
        evidence_files.append(report_file.parent / frame)
shutil.copyfile(pbw, out / f'pulsetime-{version}.pbw')
shutil.copyfile(root / 'GUIDE.md', out / 'GUIDE.md')
shutil.copyfile(root / 'store/signals-release-notes.md', out / 'RELEASE-NOTES.md')
shutil.copyfile(repo / 'docs/pebble-signal-feeds.md', out / 'CUSTOM-FEEDS.md')
source = out / f'pulsetime-{version}-source.zip'
files = [root / 'package.json', root / 'wscript', root / 'GUIDE.md', root / 'store/signals-release-notes.md']
for directory in ('src', 'worker_src', 'resources'):
    files.extend(p for p in (root / directory).rglob('*') if p.is_file())
with zipfile.ZipFile(source, 'w', zipfile.ZIP_DEFLATED) as archive:
    for file in sorted(files):
        assert not file.is_symlink()
        archive.write(file, 'pulsetime/' + file.relative_to(root).as_posix())
    archive.write(repo / 'LICENSE', 'pulsetime/LICENSE')
    archive.write(repo / 'docs/pebble-signal-feeds.md', 'pulsetime/CUSTOM-FEEDS.md')
    archive.writestr('pulsetime/BUILD.txt', 'Install Pebble SDK 4.33.1 and pebble-tool 5.0.39 or compatible.\nRun pebble build in this directory. No npm install is needed.\nThis candidate supports Time 2, 2 Duo, and Round 2.\nIt is not a published release or a physical tactile validation.\n')
rebuild = Path(tempfile.mkdtemp(prefix='signals-rebuild-', dir=root / 'build'))
with zipfile.ZipFile(source) as archive: archive.extractall(rebuild)
result = subprocess.run(['pebble', 'build'], cwd=rebuild / 'pulsetime', capture_output=True, text=True)
(out / 'source-build.log').write_text(result.stdout + result.stderr)
assert result.returncode == 0, 'Independent source build failed'
def normalized(name, content):
    if not name.endswith('.bin'): return content
    # SDK 4.33.1 inject_metadata.py documents CRC 0x14 and resource timestamp
    # 0x7c. readelf identifies the GNU build-id note at 0x82, aligned to 0x84.
    # Verify its layout before ignoring only its 20-byte digest (0x94..0xa8).
    assert content[0x84:0x94] == struct.pack('<III4s', 4, 20, 3, b'GNU\0'), 'Unexpected binary layout'
    copy = bytearray(content)
    for start, end in ((0x14, 0x18), (0x7c, 0x80), (0x94, 0xa8)): copy[start:end] = bytes(end - start)
    return copy

with zipfile.ZipFile(pbw) as tested, zipfile.ZipFile(rebuild / 'pulsetime/build/pulsetime.pbw') as rebuilt:
    payloads = [name for name in tested.namelist() if name.endswith(('.bin', '.pbpack', '.js'))]
    mismatched = [name for name in payloads if normalized(name, tested.read(name)) != normalized(name, rebuilt.read(name))]
    assert not mismatched, f'Rebuilt payload differs: {mismatched}'
evidence = out / 'evidence'; evidence.mkdir(exist_ok=True)
for file in evidence_files:
    shutil.copyfile(file, evidence / file.name)
manifest = dict(version=version, targets=['emery', 'flint', 'gabbro'], pbwSHA256=digest,
                sourceSHA256=hashlib.sha256(source.read_bytes()).hexdigest(),
                independentlyRebuiltPayloads=payloads,
                ignoredBinaryMetadata='SDK CRC 0x14..0x18, resource timestamp 0x7c..0x80, verified GNU build-id digest 0x94..0xa8',
                limits='No physical tactile validation, amplitude export, or store publication.')
(out / 'candidate.json').write_text(json.dumps(manifest, indent=2) + '\n')
(out / 'SHA256SUMS.txt').write_text(''.join(hashlib.sha256(file.read_bytes()).hexdigest() + '  ' + file.relative_to(out).as_posix() + '\n' for file in sorted(out.rglob('*')) if file.is_file() and file.name != 'SHA256SUMS.txt'))
print(out)
print(f'{len(payloads)} rebuilt payloads match after excluding documented build metadata; resources and companion match byte for byte.')
