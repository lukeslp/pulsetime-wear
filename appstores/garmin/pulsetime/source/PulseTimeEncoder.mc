// File purpose: Pure Monkey C port of the PulseTime haptic time encoders.
// Source of truth: shared/src/main/java/com/lukesteuber/pulsetime/shared/encoding/
// (TimeSystems.kt + TerseQuarterEncoder.kt). Fixture-locked by
// appstores/shared/fixtures/patterns.json (see ../tests/).
//
// encode() returns an Array of [onMs, offMs, layerPercent] Number triples.
// layerPercent is the relative strength (100 / 72 / 50 / 45); 0 marks a
// silence-only gap (onMs == 0). Millisecond values are already scaled by the
// playback-speed multiplier exactly as the Kotlin computes them: a Number times
// a single-precision Float, truncated toward zero, then floored at 1 ms.
//
// No UI, Attention, or System dependencies on purpose: the test harness under
// ../tests/harness compiles this file on its own.
import Toybox.Lang;

module PulseTimeEncoder {
    const SYSTEM_TERSE_QUARTER = 0;
    const SYSTEM_FIVE_MINUTE_LADDER = 1;
    const SYSTEM_DECIMAL_PLACE = 2;
    const SYSTEM_BINARY_CLOCK = 3;
    const SYSTEM_NEAREST_QUARTER = 4;
    const SYSTEM_MORSE_DIGITS = 5;
    const SYSTEM_COUNT = 6;

    const SPEED_SLOW = 0;
    const SPEED_STANDARD = 1;
    const SPEED_FAST = 2;
    const SPEED_COUNT = 3;

    const LAYER_FULL = 100;
    const LAYER_MEDIUM = 72;
    const LAYER_HALF = 50;
    const LAYER_LOW = 45;

    const FIELD_GAP = 700;
    const GROUP_GAP = 420;
    const SHORT_GAP = 150;

    // Terse Quarter timings (TerseQuarterTimings in Kotlin).
    const TQ_HOUR_ON = 200;
    const TQ_HOUR_OFF = 280;
    const TQ_GAP = 700;
    const TQ_QUARTER_ON = 320;
    const TQ_QUARTER_OFF = 280;
    const TQ_MINUTE_ON = 80;
    const TQ_MINUTE_OFF = 120;

    // Playback multipliers: Slow 1.25, Standard 1.0, Fast 0.78. Monkey C Float
    // is single precision, matching Kotlin's Float.
    function speedMultiplier(speedIndex as Number) as Float {
        if (speedIndex == SPEED_SLOW) { return 1.25; }
        if (speedIndex == SPEED_FAST) { return 0.78; }
        return 1.0;
    }

    function hour12(hourOfDay as Number) as Number {
        var h = hourOfDay % 12;
        return h == 0 ? 12 : h;
    }

    // Kotlin: (ms * multiplier).toLong() -- single-precision multiply, truncate.
    function scaled(ms as Number, mult as Float) as Number {
        var v = (ms * mult).toNumber();
        return v < 1 ? 1 : v;
    }

    function pulse(out as Array<Array<Number>>, on as Number, off as Number, mult as Float, layer as Number) as Void {
        out.add([scaled(on, mult), off > 0 ? scaled(off, mult) : 0, layer]);
    }

    function gap(out as Array<Array<Number>>, ms as Number, mult as Float) as Void {
        out.add([0, scaled(ms, mult), 0]);
    }

    // groupAt == 0 means no grouping gap.
    function counted(out as Array<Array<Number>>, count as Number, on as Number, off as Number,
                     mult as Float, layer as Number, groupAt as Number) as Void {
        for (var i = 0; i < count; i++) {
            pulse(out, on, i == count - 1 ? 0 : off, mult, layer);
            if (groupAt > 0 && i + 1 < count && (i + 1) % groupAt == 0) {
                gap(out, GROUP_GAP, mult);
            }
        }
    }

    // Terse Quarter: hours, long gap, quarters, optional remainder ticks.
    // The Kotlin builds unscaled segments then applies withSpeed() to every
    // non-zero on/off with the same truncate-and-floor rule, so scaling each
    // field as it is emitted is equivalent.
    function terseQuarter(out as Array<Array<Number>>, hourOfDay as Number, minute as Number,
                          mult as Float, includePrecision as Boolean) as Void {
        var h = hour12(hourOfDay);
        var q = minute / 15;
        var rem = minute % 15;
        for (var i = 0; i < h; i++) {
            pulse(out, TQ_HOUR_ON, i < h - 1 ? TQ_HOUR_OFF : 0, mult, LAYER_FULL);
        }
        gap(out, TQ_GAP, mult);
        for (var i = 0; i < q; i++) {
            pulse(out, TQ_QUARTER_ON, i < q - 1 ? TQ_QUARTER_OFF : 0, mult, LAYER_FULL);
        }
        if (includePrecision && rem > 0) {
            gap(out, TQ_GAP / 2, mult);
            for (var i = 0; i < rem; i++) {
                pulse(out, TQ_MINUTE_ON, i < rem - 1 ? TQ_MINUTE_OFF : 0, mult, LAYER_FULL);
            }
        }
    }

    function fiveMinuteLadder(out as Array<Array<Number>>, hourOfDay as Number, minute as Number, mult as Float) as Void {
        counted(out, hour12(hourOfDay), 240, 190, mult, LAYER_FULL, 5);
        gap(out, FIELD_GAP, mult);
        counted(out, minute / 5, 145, 145, mult, LAYER_MEDIUM, 5);
        if (minute % 5 > 0) {
            gap(out, GROUP_GAP, mult);
            counted(out, minute % 5, 75, 110, mult, LAYER_LOW, 0);
        }
    }

    function addDigit(out as Array<Array<Number>>, digit as Number, mult as Float, on as Number, layer as Number) as Void {
        if (digit == 0) {
            pulse(out, on * 2, 0, mult, layer);
        } else {
            counted(out, digit, on, SHORT_GAP, mult, layer, 5);
        }
    }

    function decimalPlace(out as Array<Array<Number>>, hourOfDay as Number, minute as Number, mult as Float) as Void {
        counted(out, hour12(hourOfDay), 230, 180, mult, LAYER_FULL, 5);
        gap(out, FIELD_GAP, mult);
        addDigit(out, minute / 10, mult, 155, LAYER_MEDIUM);
        gap(out, GROUP_GAP, mult);
        addDigit(out, minute % 10, mult, 80, LAYER_LOW);
    }

    function addBits(out as Array<Array<Number>>, value as Number, width as Number, mult as Float) as Void {
        for (var bit = width - 1; bit >= 0; bit--) {
            var one = (value & (1 << bit)) != 0;
            pulse(out, one ? 210 : 75, SHORT_GAP, mult, one ? LAYER_FULL : LAYER_LOW);
        }
    }

    function binaryClock(out as Array<Array<Number>>, hourOfDay as Number, minute as Number, mult as Float) as Void {
        addBits(out, hour12(hourOfDay), 4, mult);
        gap(out, FIELD_GAP, mult);
        addBits(out, minute, 6, mult);
    }

    function nearestQuarter(out as Array<Array<Number>>, hourOfDay as Number, minute as Number, mult as Float) as Void {
        var landmark = ((minute + 7) / 15) * 15;
        var landmarkHour = hourOfDay;
        if (landmark == 60) {
            landmark = 0;
            landmarkHour = (hourOfDay + 1) % 24;
        }
        var absoluteNow = hourOfDay * 60 + minute;
        var absoluteLandmark = landmarkHour * 60 + landmark;
        if (hourOfDay == 23 && landmarkHour == 0) {
            absoluteLandmark = 24 * 60;
        }
        var offset = absoluteNow - absoluteLandmark;
        counted(out, hour12(landmarkHour), 230, 180, mult, LAYER_FULL, 5);
        gap(out, FIELD_GAP, mult);
        var landmarkCount = landmark == 0 ? 1 : landmark / 15 + 1;
        counted(out, landmarkCount, 150, 140, mult, LAYER_MEDIUM, 0);
        if (offset != 0) {
            gap(out, GROUP_GAP, mult);
            if (offset > 0) {
                pulse(out, 75, 100, mult, LAYER_LOW);
                pulse(out, 160, 0, mult, LAYER_FULL);
            } else {
                pulse(out, 160, 100, mult, LAYER_FULL);
                pulse(out, 75, 0, mult, LAYER_LOW);
            }
            gap(out, GROUP_GAP, mult);
            counted(out, offset < 0 ? -offset : offset, 70, 105, mult, LAYER_LOW, 5);
        }
    }

    // Morse numerals 0-9 as dash bitmasks read from the first mark: bit 4 is
    // the first mark, bit 0 the last. 1 = dash, 0 = dot.
    //   0 ----- 0x1F   1 .---- 0x0F   2 ..--- 0x07   3 ...-- 0x03   4 ....- 0x01
    //   5 ..... 0x00   6 -.... 0x10   7 --... 0x18   8 ---.. 0x1C   9 ----. 0x1E
    function morseMask(digit as Number) as Number {
        if (digit == 0) { return 0x1F; }
        if (digit == 1) { return 0x0F; }
        if (digit == 2) { return 0x07; }
        if (digit == 3) { return 0x03; }
        if (digit == 4) { return 0x01; }
        if (digit == 5) { return 0x00; }
        if (digit == 6) { return 0x10; }
        if (digit == 7) { return 0x18; }
        if (digit == 8) { return 0x1C; }
        return 0x1E;
    }

    function addCode(out as Array<Array<Number>>, digit as Number, mult as Float) as Void {
        var mask = morseMask(digit);
        for (var bit = 4; bit >= 0; bit--) {
            var dash = (mask & (1 << bit)) != 0;
            pulse(out, dash ? 230 : 75, SHORT_GAP, mult, dash ? LAYER_FULL : LAYER_HALF);
        }
    }

    function morseDigits(out as Array<Array<Number>>, hourOfDay as Number, minute as Number, mult as Float) as Void {
        var h = hour12(hourOfDay);
        if (h >= 10) {
            addCode(out, 1, mult);
            gap(out, GROUP_GAP, mult);
            addCode(out, h - 10, mult);
        } else {
            addCode(out, h, mult);
        }
        gap(out, FIELD_GAP, mult);
        addCode(out, minute / 10, mult);
        gap(out, GROUP_GAP, mult);
        addCode(out, minute % 10, mult);
    }

    // Main entry. hourOfDay 0-23, minute 0-59, speedIndex 0-2. includePrecision
    // only affects Terse Quarter (the other five systems are inherently exact).
    function encode(system as Number, hourOfDay as Number, minute as Number,
                    speedIndex as Number, includePrecision as Boolean) as Array<Array<Number>> {
        var out = [] as Array<Array<Number>>;
        var mult = speedMultiplier(speedIndex);
        if (system == SYSTEM_FIVE_MINUTE_LADDER) {
            fiveMinuteLadder(out, hourOfDay, minute, mult);
        } else if (system == SYSTEM_DECIMAL_PLACE) {
            decimalPlace(out, hourOfDay, minute, mult);
        } else if (system == SYSTEM_BINARY_CLOCK) {
            binaryClock(out, hourOfDay, minute, mult);
        } else if (system == SYSTEM_NEAREST_QUARTER) {
            nearestQuarter(out, hourOfDay, minute, mult);
        } else if (system == SYSTEM_MORSE_DIGITS) {
            morseDigits(out, hourOfDay, minute, mult);
        } else {
            terseQuarter(out, hourOfDay, minute, mult, includePrecision);
        }
        return out;
    }

    // Wire form used by the fixtures: "on/off/layer" joined by single spaces.
    function compact(segments as Array<Array<Number>>) as String {
        var s = "";
        for (var i = 0; i < segments.size(); i++) {
            var seg = segments[i];
            if (i > 0) { s += " "; }
            s += seg[0].toString() + "/" + seg[1].toString() + "/" + seg[2].toString();
        }
        return s;
    }

    // Total wall-clock duration of a pattern in ms.
    function durationMs(segments as Array<Array<Number>>) as Number {
        var total = 0;
        for (var i = 0; i < segments.size(); i++) {
            total += segments[i][0] + segments[i][1];
        }
        return total;
    }
}
