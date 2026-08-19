// File purpose: Headless fixture harness. It encodes every case in Fixtures.mc
// with PulseTimeEncoder, compares the compact wire form against the expected
// string, checks the PulsePlayer chunker invariants on the same patterns,
// prints PASS n/N (or the first mismatches) with System.println, and exits.
// Work is spread over a repeating Timer, a few cases per tick, so the Connect
// IQ watchdog ("Code Executed Too Long") never trips.
// Run with monkeydo against the simulator; output streams to stdout.
import Toybox.Application;
import Toybox.Attention;
import Toybox.Graphics;
import Toybox.Lang;
import Toybox.System;
import Toybox.Timer;
import Toybox.WatchUi;

class HarnessApp extends Application.AppBase {
    function initialize() {
        AppBase.initialize();
    }

    function getInitialView() as [Views] or [Views, InputDelegates] {
        return [new HarnessView()];
    }
}

class HarnessView extends WatchUi.View {
    private var _runner as HarnessRunner;

    function initialize() {
        View.initialize();
        _runner = new HarnessRunner();
    }

    function onShow() as Void {
        _runner.start();
    }

    function onUpdate(dc as Dc) as Void {
        dc.setColor(Graphics.COLOR_WHITE, Graphics.COLOR_BLACK);
        dc.clear();
        dc.drawText(dc.getWidth() / 2, dc.getHeight() / 2, Graphics.FONT_SMALL, _runner.summary(),
                    Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
    }
}

class HarnessRunner {
    const MAX_REPORTED = 10;
    const CASES_PER_TICK = 6;

    private var _timer as Timer.Timer;
    private var _group as Number = 0;
    private var _index as Number = 0;
    private var _cases as Array<Array>;
    private var _pass as Number = 0;
    private var _fail as Number = 0;
    private var _chunkFail as Number = 0;
    private var _reported as Number = 0;
    private var _summary as String = "running";

    function initialize() {
        _timer = new Timer.Timer();
        _cases = Fixtures.group(0);
    }

    function summary() as String {
        return _summary;
    }

    function start() as Void {
        _timer.start(method(:tick), 50, true);
    }

    function tick() as Void {
        var budget = CASES_PER_TICK;
        while (budget > 0) {
            if (_index >= _cases.size()) {
                _group++;
                _index = 0;
                if (_group >= Fixtures.GROUP_COUNT) {
                    finish();
                    return;
                }
                _cases = Fixtures.group(_group);
                continue;
            }
            runCase(_cases[_index]);
            _index++;
            budget--;
        }
    }

    function finish() as Void {
        _timer.stop();
        var total = _pass + _fail;
        if (_fail == 0 && _chunkFail == 0) {
            _summary = "PASS " + _pass + "/" + total + " (chunker ok on all " + total + ")";
        } else {
            _summary = "FAIL encoder " + _fail + "/" + total + ", chunker " + _chunkFail + "/" + total;
        }
        System.println(_summary);
        WatchUi.requestUpdate();
        System.exit();
    }

    function runCase(c as Array) as Void {
        var system = c[0] as Number;
        var speed = c[1] as Number;
        var precision = c[2] as Boolean;
        var hour = c[3] as Number;
        var minute = c[4] as Number;
        var expected = c[5] as String;
        var segments = PulseTimeEncoder.encode(system, hour, minute, speed, precision);
        var actual = PulseTimeEncoder.compact(segments);
        if (actual.equals(expected)) {
            _pass++;
        } else {
            _fail++;
            if (_reported < MAX_REPORTED) {
                _reported++;
                System.println("MISMATCH system=" + system + " speed=" + speed + " precision=" + precision
                    + " time=" + hour + ":" + minute.format("%02d"));
                System.println("  expected " + expected);
                System.println("  actual   " + actual);
            }
        }
        var chunkError = checkChunks(segments);
        if (chunkError != null) {
            _chunkFail++;
            if (_reported < MAX_REPORTED) {
                _reported++;
                System.println("CHUNK system=" + system + " speed=" + speed + " time=" + hour + ":"
                    + minute.format("%02d") + " -> " + chunkError);
            }
        }
    }

    // Chunker invariants for one pattern at Standard strength, layered:
    //  - every chunk holds 1..8 profiles;
    //  - chunks concatenate back to the profile list;
    //  - every seam is inside a silence (chunk ends with, or next begins with,
    //    a duty-0 profile);
    //  - chunk durations sum to the pattern duration minus trailing silence.
    function checkChunks(segments as Array<Array<Number>>) as String? {
        var profiles = PulsePlayer.toProfiles(segments, 70, true);
        var chunks = PulsePlayer.chunk(profiles);
        var flat = 0;
        var durationSum = 0;
        for (var i = 0; i < chunks.size(); i++) {
            var chunk = chunks[i];
            if (chunk.size() < 1 || chunk.size() > PULSE_MAX_PROFILES) {
                return "chunk " + i + " has " + chunk.size() + " profiles";
            }
            for (var j = 0; j < chunk.size(); j++) {
                var p = profiles[flat + j];
                if (p.dutyCycle != chunk[j].dutyCycle || p.length != chunk[j].length) {
                    return "chunk " + i + " profile " + j + " differs from source";
                }
            }
            flat += chunk.size();
            durationSum += PulsePlayer.chunkDuration(chunk);
            if (i + 1 < chunks.size()) {
                var endsInSilence = chunk[chunk.size() - 1].dutyCycle == 0;
                var nextStartsInSilence = chunks[i + 1][0].dutyCycle == 0;
                if (!endsInSilence && !nextStartsInSilence) {
                    return "seam after chunk " + i + " is not inside a gap";
                }
            }
        }
        if (flat != profiles.size()) {
            return "chunks cover " + flat + " of " + profiles.size() + " profiles";
        }
        // Expected duration: everything up to and including the last ON.
        var expectedDuration = 0;
        var lastOn = -1;
        for (var i = 0; i < segments.size(); i++) {
            if (segments[i][0] > 0) { lastOn = i; }
        }
        for (var i = 0; i <= lastOn; i++) {
            expectedDuration += segments[i][0];
            if (i < lastOn) { expectedDuration += segments[i][1]; }
        }
        if (durationSum != expectedDuration) {
            return "duration " + durationSum + " != " + expectedDuration;
        }
        return null;
    }
}
