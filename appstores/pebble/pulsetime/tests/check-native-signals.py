"""Isolated new-Pebble smoke test, using the packaged companion and live weather.

Run with pebble-tool's Python. Uses an explicit test location (New York City),
not the wearer's location. Captures are native; tactile perception is untested.
"""
import argparse
import datetime
import dbm.dumb
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time
from types import SimpleNamespace
from uuid import UUID

ROOT = Path(__file__).resolve().parents[1]
SDK = Path.home() / 'Library/Application Support/Pebble SDK/SDKs/4.33.1'
os.environ['PEBBLE_QEMU_PATH'] = str(SDK / 'toolchain/bin/qemu-pebble')
os.environ['PATH'] = str(SDK / 'toolchain/bin') + os.pathsep + os.environ['PATH']
import png
from pebble_tool.commands.screenshot import ScreenshotCommand
from pebble_tool.commands.install import ToolAppInstaller
from pebble_tool.commands.emucontrol import send_data_to_qemu
import pebble_tool.sdk.emulator as emulator
from libpebble2.protocol.apps import AppRunState, AppRunStateStart
from libpebble2.protocol.logs import AppLogMessage, AppLogShippingControl
from libpebble2.communication.transports.qemu.protocol import QemuButton


def run(platform):
    pbw = ROOT / 'build/pulsetime.pbw'
    digest = hashlib.sha256(pbw.read_bytes()).hexdigest()
    app = UUID(json.loads((ROOT / 'package.json').read_text())['pebble']['uuid'])
    state = Path(tempfile.mkdtemp(prefix='signals-emulator-', dir=ROOT / 'build'))
    out = ROOT / 'build/signals-evidence'; out.mkdir(exist_ok=True)
    profile = state / platform; (profile / 'localstorage').mkdir(parents=True)
    with dbm.dumb.open(str(profile / 'localstorage' / str(app)), 'c') as storage:
        storage['clay-settings'] = json.dumps({'WEATHER_ENABLED': True, 'WEATHER_LAT': '40.7128', 'WEATHER_LON': '-74.0060'})
    emulator.get_sdk_persist_dir = lambda *args: str(profile)
    emulator.get_emulator_info_path = lambda: str(state / 'emulators.json')
    emulator.get_default_account = lambda: SimpleNamespace(is_logged_in=False)
    bridge_log = (out / f'{platform}-bridge.log').open('w')
    emulator.ManagedEmulatorTransport._get_output = lambda self: bridge_log
    cmd = ScreenshotCommand(); cmd._set_debugging(0)
    watch = None; logs = []; frames = []

    def shutdown():
        info = emulator.get_emulator_info(platform, '4.33.1')
        if info:
            for key in ('qemu', 'pypkjs', 'websockify'):
                pid = info.get(key, {}).get('pid')
                if not pid: continue
                process = subprocess.run(['ps', '-p', str(pid), '-o', 'command='], capture_output=True, text=True)
                if process.returncode == 0 and str(profile) not in process.stdout:
                    raise RuntimeError('Process no longer belongs to this isolated profile')
        cmd._shutdown_platform_emulator(platform, '4.33.1')

    def click(name, duration=.1):
        send_data_to_qemu(watch.transport, QemuButton(state=getattr(QemuButton.Button, name)))
        time.sleep(duration)
        send_data_to_qemu(watch.transport, QemuButton(state=0)); time.sleep(.6)

    def capture(name):
        data = cmd._grab_processed_image(argparse.Namespace(no_correction=True, scale=1, no_open=True, v=0), show_progress=False)
        file = out / f'{platform}-{name}.png'
        png.from_array(data, mode='RGBA;8').save(str(file)); frames.append(file.name)

    try:
        watch = cmd._connect_emulator(platform, '4.33.1'); cmd.pebble = watch
        watch.register_endpoint(AppLogMessage, lambda packet: logs.append(str(packet.message)))
        watch.send_packet(AppLogShippingControl(enable=True))
        time.sleep(4); click('Back')
        cmd._set_time(watch, datetime.datetime.now())
        ToolAppInstaller(watch, str(pbw), quiet=True).install()
        watch.send_packet(AppRunState(data=AppRunStateStart(uuid=app)))
        time.sleep(3); capture('welcome')
        for _ in range(10): click('Down')  # Complete the first-run guide.
        capture('clock')
        click('Select', 1); time.sleep(.5)
        for _ in range(7): click('Down')
        capture('time-menu-signals'); click('Select'); capture('signals-menu')
        start = len(logs); click('Select')
        deadline = time.monotonic() + 21
        while time.monotonic() < deadline and not any('signal 1 state' in line for line in logs[start:]): time.sleep(.1)
        capture('live-weather')
        live_weather = any('signal 1 state 1' in line for line in logs[start:])
        click('Down', 1); capture('weather-details'); click('Back')
        click('Back')  # signals menu
        for _ in range(3): click('Down')
        click('Select'); capture('lesson-dry'); click('Down'); capture('lesson-rain-early')
        click('Up', 1); capture('lesson-summary'); click('Back')
        click('Down'); click('Select'); capture('references')
        click('Back'); click('Down'); click('Select'); capture('playback')
        click('Back'); click('Down'); click('Select'); capture('trial')
        click('Select', 1); capture('trial-answer')
        click('Back'); click('Down'); click('Select')
        for page in range(5): capture(f'help-{page + 1}'); click('Select')
        assert hashlib.sha256(pbw.read_bytes()).hexdigest() == digest
        assert any('signal 1 state' in line for line in logs), 'Never reached signal playback'
        assert not any('crash' in line.lower() or 'fault' in line.lower() for line in logs)
        report = dict(platform=platform, pbwSHA256=digest, liveWeather=live_weather,
                      testLocation='New York City', frames=frames, logs=logs,
                      checkedAt=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                      limits='Native emulator and packaged companion; no physical perception evidence.')
        (out / f'{platform}-report.json').write_text(json.dumps(report, indent=2) + '\n')
        print(json.dumps({key: value for key, value in report.items() if key != 'logs'}), flush=True)
        assert live_weather, 'Live weather did not produce a current reading; inspect bridge log'
    finally:
        if watch: cmd._close_pebble_connection(watch); cmd.pebble = None
        shutdown(); bridge_log.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('platform', choices=['emery', 'flint', 'gabbro'])
    run(parser.parse_args().platform)
