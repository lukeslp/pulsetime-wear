"""Drive PulseTime Wake through its button flows in an isolated emulator profile.

usage: <pebble-tool python> emulator-flow.py <pbw> <out dir> <platform> [short|long|unanswered]

Run it with the Python inside the pebble-tool install (on Beast:
~/.local/share/uv/tools/pebble-tool/bin/python) so libpebble2 and pebble_tool
import. Each run uses a throwaway emulator profile under <out dir> and never
touches the default emulator.

  short       alarm set/arm by touch, wakeup ring, Back then resume, snooze,
              snooze ring, stop, intervals (pause, status), Learn
  long        as short, but rings to stage 4 and runs 20 / 10 to Done
  unanswered  arms, rings, and waits for the 10-minute self-stop

Every vibration the app plays is logged by the app itself ("VIBE <label> ...")
because the emulator has no motor. This script presses buttons, moves the
emulated clock to fire the wakeups, saves screenshots and the full app log, and
asserts that each expected log line appears.
"""
import argparse, datetime, json, os, subprocess, sys, tempfile, time
from pathlib import Path
from types import SimpleNamespace
from uuid import UUID
import zipfile

SDK = Path.home() / 'Library/Application Support/Pebble SDK/SDKs/4.33.1'
toolchain = str(SDK.resolve() / 'toolchain/bin')
os.environ.setdefault('PEBBLE_QEMU_PATH', toolchain + '/qemu-pebble')
os.environ['PATH'] = toolchain + os.pathsep + os.environ['PATH']
import png
from pebble_tool.commands.screenshot import ScreenshotCommand
from pebble_tool.commands.install import ToolAppInstaller
from libpebble2.protocol.apps import AppRunState, AppRunStateStart, AppRunStateStop
from libpebble2.protocol.logs import AppLogMessage, AppLogShippingControl
import pebble_tool.sdk.emulator as emulator
from libpebble2.communication.transports.qemu.protocol import QemuButton
from pebble_tool.commands.emucontrol import send_data_to_qemu

pbw = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2]).resolve(); out.mkdir(parents=True, exist_ok=True)
platform = sys.argv[3]
mode = sys.argv[4] if len(sys.argv) > 4 else 'short'
with zipfile.ZipFile(pbw) as z:
    app = UUID(json.loads(z.read('appinfo.json'))['uuid'])

state = Path(tempfile.mkdtemp(prefix=f'emu-{platform}-', dir=out))
def persist(target, version=None):
    p = state / target; p.mkdir(exist_ok=True); return str(p)
emulator.get_sdk_persist_dir = persist
emulator.get_emulator_info_path = lambda: str(state / 'emulators.json')
emulator.get_default_account = lambda: SimpleNamespace(is_logged_in=False)
bridge_log = (state / 'bridge.log').open('w')
emulator.ManagedEmulatorTransport._get_output = lambda self: bridge_log

cmd = ScreenshotCommand(); cmd._set_debugging(0)
def shutdown():
    info = emulator.get_emulator_info(platform, '4.33.1')
    if info:
        for key in ('qemu', 'pypkjs', 'websockify'):
            pid = info.get(key, {}).get('pid')
            if not pid: continue
            process = subprocess.run(['ps', '-p', str(pid), '-o', 'command='], capture_output=True, text=True)
            if process.returncode == 0 and str(state) not in process.stdout:
                raise RuntimeError(f'Refusing to stop PID {pid}: outside this profile')
    cmd._shutdown_platform_emulator(platform, '4.33.1')

watch = cmd._connect_emulator(platform, '4.33.1'); cmd.pebble = watch
logs = []
t0 = time.monotonic()
def on_log(packet):
    logs.append(f'{time.monotonic() - t0:8.2f} {packet.message}')
handle = watch.register_endpoint(AppLogMessage, on_log)
watch.send_packet(AppLogShippingControl(enable=True))
args = argparse.Namespace(no_correction=True, scale=1, no_open=True, v=0)
results = []
BUTTONS = {'back': QemuButton.Button.Back, 'up': QemuButton.Button.Up,
           'select': QemuButton.Button.Select, 'down': QemuButton.Button.Down}

def note(msg):
    line = f'{time.monotonic() - t0:8.2f} STEP {msg}'
    print(line, flush=True); logs.append(line)

def press(name, hold=0.12, after=0.4):
    note(f'press {name}' + (' (hold)' if hold > 0.5 else ''))
    send_data_to_qemu(watch.transport, QemuButton(state=BUTTONS[name])); time.sleep(hold)
    send_data_to_qemu(watch.transport, QemuButton(state=0)); time.sleep(after)

def shot(name):
    pixels = cmd._grab_processed_image(args, show_progress=False)
    path = out / f'{platform}-{name}.png'
    png.from_array(pixels, mode='RGBA;8').save(str(path))
    note(f'screenshot {path.name}')

def wait_for(text, timeout, since=0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if any(text in l and ' STEP ' not in l for l in logs[since:]): return True
        time.sleep(0.05)
    return False

def expect(text, timeout=4, since=0, label=None):
    ok = wait_for(text, timeout, since)
    results.append((label or text, ok))
    note(('PASS ' if ok else 'FAIL ') + (label or text))
    return ok

def set_clock(dt):
    note(f'set clock {dt:%Y-%m-%d %H:%M:%S}')
    cmd._set_time(watch, dt); time.sleep(0.3); cmd._set_time(watch, dt); time.sleep(0.5)

day = datetime.datetime.now().replace(hour=0, minute=0, second=0, microsecond=0)
D = lambda days, h, m, s=0: day + datetime.timedelta(days=days, hours=h, minutes=m, seconds=s)

try:
    time.sleep(4)
    press('back', after=0.5)
    ToolAppInstaller(watch, str(pbw), quiet=True).install(); time.sleep(1)
    watch.send_packet(AppRunState(data=AppRunStateStop(uuid=app))); time.sleep(0.8)

    # ---- bedtime: set the alarm without looking ------------------------------------
    set_clock(D(0, 22, 10))
    mark = len(logs)
    watch.send_packet(AppRunState(data=AppRunStateStart(uuid=app)))
    expect('VIBE position-1 slots=1', since=mark, label='launch: one short pulse = item 1, Wake alarm')
    time.sleep(1.0); shot('01-home')

    mark = len(logs); press('select', after=0.1)
    expect('VIBE status-off 6:30', since=mark, label='Wake opens: falling pair, then 6:30 (alarm off)')
    time.sleep(1.9); shot('02-readout-hours')
    time.sleep(3.4); shot('03-readout-quarters')
    time.sleep(1.5); shot('04-wake-off')

    mark = len(logs); press('up', after=0.1)
    expect('VIBE dial 6:45', timeout=3, since=mark, label='Up: dial 6:45 played after the presses stop')
    time.sleep(6.2)
    mark = len(logs); press('down', after=0.1)
    expect('VIBE dial 6:30', timeout=3, since=mark, label='Down: dial back to 6:30')
    time.sleep(5.6)

    mark = len(logs); press('select', after=0.1)
    expect('WAKE arm dial=6:30 -> 06:30 in 500 min: armed', since=mark, label='Select arms: next 6:30 is 06:30, 500 min away')
    expect('VIBE armed 6:30', since=mark, label='armed: rising pair, then 6:30')
    time.sleep(6.5); shot('05-wake-on')

    mark = len(logs); press('down', hold=1.0, after=0.1)
    expect('VIBE dial 5:30', timeout=3, since=mark, label='Hold Down: one hour earlier, 5:30')
    expect('in 440 min: armed', since=mark, label='armed alarm follows the dial (440 min)')
    time.sleep(5.6)
    mark = len(logs); press('up', hold=1.0, after=0.1)
    expect('VIBE dial 6:30', timeout=3, since=mark, label='Hold Up: back to 6:30')
    time.sleep(5.6)
    mark = len(logs); press('select', hold=1.0, after=0.1)
    expect('VIBE status-on 6:30', since=mark, label='Hold Select: status, rising pair then 6:30')
    time.sleep(5.8)
    press('back', after=0.6); press('back', after=1.2)
    shot('06-watchface-after-arming')

    if mode == 'unanswered':
        mark = len(logs)
        set_clock(D(1, 6, 29, 40))
        expect('WAKE ring start cookie=1 leaves_app=1', timeout=45, since=mark, label='wakeup launches the app at 06:30')
        expect('WAKE unanswered', timeout=640, since=mark, label='unanswered ring stops itself after 10 minutes')
        raise SystemExit(0)

    # ---- morning: the wakeup fires with the app closed ------------------------------
    mark = len(logs)
    set_clock(D(1, 6, 29, 40))
    rang = expect('WAKE ring start cookie=1 leaves_app=1', timeout=45, since=mark, label='wakeup launches the app at 06:30 with the app closed')
    expect('WAKE scheduled cookie=1 at=06:30', since=mark, label='tomorrow\'s 06:30 is booked again on the ring launch')
    expect('VIBE ramp-1 slots=1', since=mark, label='ramp step 0: one 60 ms tick')
    time.sleep(2.0); shot('07-ring')
    if mode == 'long':
        expect('WAKE ramp step=6 stage=2', timeout=70, since=mark, label='stage 2 at 60 s')
        expect('WAKE ramp step=12 stage=3', timeout=55, since=mark, label='stage 3 at 108 s')
        expect('WAKE ramp step=20 stage=4', timeout=55, since=mark, label='stage 4 at 156 s')
        time.sleep(1.0); shot('08-ring-stage4')
    else:
        expect('WAKE ramp step=1 stage=1', timeout=12, since=mark, label='second tick 10 s later')
    expect('WAKE scheduled cookie=3', since=mark, label='ring keeps a resume wakeup booked ahead')

    # Back half asleep is not Stop: the app closes, and the ring comes back.
    mark = len(logs); press('back', after=0.5)
    expect('WAKE ring resumes after', timeout=100, since=mark, label='Back closes the app; the resume wakeup reopens the ring')
    expect('WAKE ring start cookie=3 leaves_app=1', since=mark, label='resumed ring launched by the system')
    expect('WAKE ramp step=', since=mark, label='resumed ring carries on at its elapsed stage')
    time.sleep(1.5)

    mark = len(logs); press('up', after=0.2)
    expect('WAKE snooze booked', since=mark, label='Up: snooze booked')
    expect('WAKE scheduled cookie=2 at=', since=mark, label='snooze wakeup booked 9 minutes ahead')
    expect('VIBE snoozed slots=3', since=mark, label='snoozed: two ticks, then the app leaves')
    snooze_line = next((l for l in logs[mark:] if 'WAKE scheduled cookie=2 at=' in l), '')
    snooze_at = snooze_line.split('at=')[1].split()[0] if snooze_line else '06:41'
    sh, sm = (int(x) for x in snooze_at.split(':'))
    time.sleep(1.5); shot('09-after-snooze')

    mark = len(logs)
    set_clock(D(1, sh, sm, 0) - datetime.timedelta(seconds=15))
    expect('WAKE ring start cookie=2 leaves_app=1', timeout=40, since=mark, label=f'snooze wakeup rings again at {snooze_at}')
    time.sleep(2.5); shot('10-snooze-ring')
    mark = len(logs); press('select', after=0.1)
    expect('WAKE stopped', since=mark, label='Select stops the ring')
    expect(f'VIBE stopped-now {sh}:{sm:02d}', since=mark, label=f'stop answers with the time now ({sh}:{sm:02d}) in Terse Quarter')
    time.sleep(1.2); shot('11-stop-readout')
    time.sleep(6.0); shot('12-after-stop')

    # ---- intervals ----------------------------------------------------------------------
    set_clock(D(1, 7, 0))
    mark = len(logs)
    watch.send_packet(AppRunState(data=AppRunStateStart(uuid=app))); time.sleep(1.5)
    press('down', after=0.8)
    expect('VIBE position-2 slots=3', since=mark, label='Down: two pulses = item 2, Intervals')
    mark = len(logs); press('select', after=0.8)
    expect('VIBE position-1 slots=1', since=mark, label='presets open on item 1, 20 / 10')
    shot('13-presets')
    mark = len(logs); press('select', after=0.1)
    expect('INTERVAL start preset=0', since=mark, label='Select starts 20 / 10')
    expect('VIBE interval-tick', since=mark, label='lead-in tick')
    expect('VIBE interval-work slots=1', timeout=5, since=mark, label='work starts after three ticks')
    time.sleep(6); shot('14-interval-work')
    expect('INTERVAL t=20 phase=1', timeout=15, since=mark, label='countdown ticks before rest')
    expect('VIBE interval-rest slots=', timeout=6, since=mark, label='rest: two pulses, then 7 rounds to go')
    time.sleep(1.5); shot('15-interval-rest')
    if mode == 'long':
        expect('VIBE interval-done', timeout=215, since=mark, label='done after 8 rounds (233 s)')
        time.sleep(3.0); shot('18-interval-done')
    else:
        mark = len(logs); press('up', after=0.3)
        expect('VIBE status-rest', since=mark, label='Up: status = rest + rounds to go')
        time.sleep(6.0)
        mark = len(logs); press('select', after=0.6)
        expect('INTERVAL paused', since=mark, label='Select pauses: one tick')
        shot('16-interval-paused')
        time.sleep(3.0)
        mark = len(logs); press('select', after=0.3)
        expect('INTERVAL resumed', since=mark, label='Select resumes with the phase cue')
        time.sleep(2.0)
    mark = len(logs); press('back', after=0.8)
    if mode != 'long':
        expect('INTERVAL left at', since=mark, label='Back leaves the timer')
    press('back', after=0.8)

    # ---- learn the buzzes ------------------------------------------------------------------
    mark = len(logs)
    press('down', after=0.8); press('down', after=0.8)
    expect('VIBE position-3 slots=5', since=mark, label='item 3, Learn the buzzes')
    mark = len(logs); press('select', after=1.0)
    expect('VIBE learn-1 slots=3', since=mark, label='Learn opens on Alarm on')
    for row in range(2, 13):
        press('down', after=2.6 if row != 10 else 5.4)
        expect(f'VIBE learn-{row} slots=', since=mark, label=f'Learn row {row} plays its signal')
    shot('17-learn')
    press('back', after=0.5); press('back', after=0.5)
finally:
    (out / f'{platform}-{mode}-log.txt').write_text('\n'.join(logs) + '\n')
    passed = sum(1 for _, ok in results if ok)
    summary = f'{platform} {mode}: {passed}/{len(results)} checks passed'
    (out / f'{platform}-{mode}-summary.txt').write_text(summary + '\n' + '\n'.join(('PASS ' if ok else 'FAIL ') + l for l, ok in results) + '\n')
    print(summary, flush=True)
    faults = [l for l in logs if any(term in l.lower() for term in ['crash', 'fault', 'allocation failed', 'assert'])]
    if faults: print('FAULT LINES:', *faults, sep='\n', flush=True)
    watch.unregister_endpoint(handle); cmd._close_pebble_connection(watch); cmd.pebble = None; shutdown(); bridge_log.close()
