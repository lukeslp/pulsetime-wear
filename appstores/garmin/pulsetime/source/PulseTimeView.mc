// File purpose: Full app view. Vibrates the current time when shown, draws the
// time, the active system's short label, and the repeat hint, and refreshes the
// published complication with the time it just played.
import Toybox.Application;
import Toybox.Graphics;
import Toybox.Lang;
import Toybox.System;
import Toybox.WatchUi;

class PulseTimeView extends WatchUi.View {
    // Short labels match TimeSystem.shortLabel in the Kotlin source.
    private var _shortLabels as Array<String> = ["Simple", "Exact", "Digits", "Compact", "Clock-style", "Morse"];
    private var _titles as Array<String> = ["Terse Quarter", "Five-Minute Ladder", "Decimal Place Rhythm",
                                             "Binary Clock", "Nearest-Quarter Offset", "Morse Digits"];
    private var _player as PulsePlayer;
    private var _system as Number = PulseTimeEncoder.SYSTEM_TERSE_QUARTER;
    private var _speed as Number = PulseTimeEncoder.SPEED_STANDARD;
    private var _strength as Number = 1;
    private var _layered as Boolean = true;
    private var _exactMinutes as Boolean = false;
    private var _lastHour as Number = 0;
    private var _lastMinute as Number = 0;

    function initialize() {
        View.initialize();
        _player = new PulsePlayer();
        reloadSettings();
    }

    // Read every property with null guards and clamping. Called at start and
    // from AppBase.onSettingsChanged.
    function reloadSettings() as Void {
        _system = readNumber("system", 0, PulseTimeEncoder.SYSTEM_COUNT - 1, PulseTimeEncoder.SYSTEM_TERSE_QUARTER);
        _speed = readNumber("speed", 0, PulseTimeEncoder.SPEED_COUNT - 1, PulseTimeEncoder.SPEED_STANDARD);
        _strength = readNumber("strength", 0, 2, 1);
        _layered = readBoolean("layered", true);
        _exactMinutes = readBoolean("exactMinutes", false);
    }

    private function readNumber(key as String, lo as Number, hi as Number, fallback as Number) as Number {
        var raw = Application.Properties.getValue(key);
        var value = fallback;
        if (raw instanceof Lang.Number) {
            value = raw;
        } else if (raw instanceof Lang.Float || raw instanceof Lang.String || raw instanceof Lang.Long) {
            var converted = raw.toNumber();
            if (converted != null) {
                value = converted;
            }
        }
        if (value < lo) { value = lo; }
        if (value > hi) { value = hi; }
        return value;
    }

    private function readBoolean(key as String, fallback as Boolean) as Boolean {
        var raw = Application.Properties.getValue(key);
        if (raw instanceof Lang.Boolean) {
            return raw;
        }
        return fallback;
    }

    // Soft / Standard / Strong -> base duty cycle 45 / 70 / 100.
    private function baseDuty() as Number {
        if (_strength == 0) { return 45; }
        if (_strength == 2) { return 100; }
        return 70;
    }

    function systemIndex() as Number {
        return _system;
    }

    // Cycle the time system, persist it, and replay.
    function cycleSystem(delta as Number) as Void {
        var next = (_system + delta) % PulseTimeEncoder.SYSTEM_COUNT;
        if (next < 0) { next += PulseTimeEncoder.SYSTEM_COUNT; }
        _system = next;
        Application.Properties.setValue("system", _system);
        playNow();
    }

    // Encode the current local time and start (or restart) playback.
    function playNow() as Void {
        var now = System.getClockTime();
        _lastHour = now.hour;
        _lastMinute = now.min;
        var segments = PulseTimeEncoder.encode(_system, now.hour, now.min, _speed, _exactMinutes);
        _player.play(segments, baseDuty(), _layered);
        // Keep the published complication showing the time that was just played.
        PulseTimeComplication.publish();
        WatchUi.requestUpdate();
    }

    function stopPlayback() as Void {
        _player.stop();
    }

    function onShow() as Void {
        playNow();
    }

    function onHide() as Void {
        _player.stop();
    }

    function onUpdate(dc as Dc) as Void {
        var w = dc.getWidth();
        var h = dc.getHeight();
        var cx = w / 2;
        dc.setColor(Graphics.COLOR_BLACK, Graphics.COLOR_BLACK);
        dc.clear();

        // Time, large.
        dc.setColor(Graphics.COLOR_WHITE, Graphics.COLOR_TRANSPARENT);
        dc.drawText(cx, h * 0.42, Graphics.FONT_NUMBER_HOT, timeText(),
                    Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);

        // System label above the time.
        dc.setColor(0x5CD9FF, Graphics.COLOR_TRANSPARENT);
        dc.drawText(cx, h * 0.20, Graphics.FONT_SMALL, _titles[_system] + " (" + _shortLabels[_system] + ")",
                    Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);

        // Hint below the time.
        dc.setColor(Graphics.COLOR_LT_GRAY, Graphics.COLOR_TRANSPARENT);
        var hint = WatchUi.loadResource(Rez.Strings.TapToRepeat) as String;
        var settings = System.getDeviceSettings();
        if (settings has :vibrateOn && !settings.vibrateOn) {
            hint = WatchUi.loadResource(Rez.Strings.VibrationOff) as String;
        }
        dc.drawText(cx, h * 0.68, Graphics.FONT_TINY, hint,
                    Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
        dc.drawText(cx, h * 0.80, Graphics.FONT_XTINY, WatchUi.loadResource(Rez.Strings.ChangeSystemHint) as String,
                    Graphics.TEXT_JUSTIFY_CENTER | Graphics.TEXT_JUSTIFY_VCENTER);
    }

    // The time that was last played, formatted per the 12/24-hour setting.
    private function timeText() as String {
        var hour = _lastHour;
        var settings = System.getDeviceSettings();
        if (!settings.is24Hour) {
            hour = hour % 12;
            if (hour == 0) { hour = 12; }
        }
        return hour.format("%d") + ":" + _lastMinute.format("%02d");
    }
}
