// File purpose: Turns encoded PulseTime segments into Attention.VibeProfile
// chunks and plays them back to back with a Timer.
//
// Attention.vibrate() accepts 1..8 VibeProfiles per call, so a full pattern
// (up to 32 segments, ~64 profiles) is split into chunks. Chunks are cut only
// where a silence profile ends the chunk (or, failing that, begins the next
// one), so every seam falls inside a gap the pattern already had. Timer jitter
// on the Venu therefore lengthens a pause slightly and never truncates a pulse.
import Toybox.Attention;
import Toybox.Lang;
import Toybox.System;
import Toybox.Timer;

// Attention.vibrate() hard limit and Timer.Timer minimum interval.
const PULSE_MAX_PROFILES = 8;
const PULSE_MIN_TIMER_MS = 50;

class PulsePlayer {

    private var _timer as Timer.Timer;
    private var _chunks as Array<Array<Attention.VibeProfile>>;
    private var _durations as Array<Number>;
    private var _next as Number = 0;
    private var _playing as Boolean = false;

    function initialize() {
        _timer = new Timer.Timer();
        _chunks = [] as Array<Array<Attention.VibeProfile>>;
        _durations = [] as Array<Number>;
    }

    function isPlaying() as Boolean {
        return _playing;
    }

    // Convert [on, off, layer] segments to VibeProfiles.
    //   ON  -> VibeProfile(duty, on)   duty = round(baseDuty * layer / 100) if
    //          layered, else baseDuty; clamped 1..100.
    //   OFF/gap -> VibeProfile(0, ms); consecutive silences merge into one
    //          profile; trailing silence is dropped.
    static function toProfiles(segments as Array<Array<Number>>, baseDuty as Number,
                               layered as Boolean) as Array<Attention.VibeProfile> {
        var profiles = [] as Array<Attention.VibeProfile>;
        var pendingSilence = 0;
        for (var i = 0; i < segments.size(); i++) {
            var seg = segments[i];
            var on = seg[0];
            var off = seg[1];
            if (on > 0) {
                if (pendingSilence > 0) {
                    profiles.add(new Attention.VibeProfile(0, pendingSilence));
                    pendingSilence = 0;
                }
                var duty = baseDuty;
                if (layered) {
                    duty = (baseDuty * seg[2] + 50) / 100;
                }
                if (duty < 1) { duty = 1; }
                if (duty > 100) { duty = 100; }
                profiles.add(new Attention.VibeProfile(duty, on));
            }
            if (off > 0) {
                pendingSilence += off;
            }
        }
        // pendingSilence left over here is trailing silence: dropped.
        return profiles;
    }

    // Split profiles into chunks of at most PULSE_MAX_PROFILES, cutting at a silence
    // boundary whenever one exists inside the window.
    static function chunk(profiles as Array<Attention.VibeProfile>) as Array<Array<Attention.VibeProfile>> {
        var chunks = [] as Array<Array<Attention.VibeProfile>>;
        var n = profiles.size();
        var start = 0;
        while (start < n) {
            var end = start + PULSE_MAX_PROFILES;
            if (end >= n) {
                end = n;
            } else {
                // Prefer the latest cut j (start < j <= end) where profile j-1
                // is silence (chunk ends in a gap); otherwise the latest j where
                // profile j is silence (next chunk begins with the gap).
                var cut = -1;
                for (var j = end; j > start; j--) {
                    if (profiles[j - 1].dutyCycle == 0) { cut = j; break; }
                }
                if (cut < 0) {
                    for (var j = end; j > start; j--) {
                        if (profiles[j].dutyCycle == 0) { cut = j; break; }
                    }
                }
                if (cut > start) {
                    end = cut;
                }
                // Otherwise: no silence inside the window; split at 8 anyway.
            }
            chunks.add(profiles.slice(start, end));
            start = end;
        }
        return chunks;
    }

    static function chunkDuration(profiles as Array<Attention.VibeProfile>) as Number {
        var total = 0;
        for (var i = 0; i < profiles.size(); i++) {
            total += profiles[i].length;
        }
        return total;
    }

    // Build the chunk queue and start playback. Any pattern in progress is
    // cancelled first.
    function play(segments as Array<Array<Number>>, baseDuty as Number, layered as Boolean) as Void {
        stop();
        var profiles = toProfiles(segments, baseDuty, layered);
        if (profiles.size() == 0) {
            return;
        }
        _chunks = chunk(profiles);
        _durations = new [_chunks.size()] as Array<Number>;
        for (var i = 0; i < _chunks.size(); i++) {
            _durations[i] = chunkDuration(_chunks[i]);
        }
        _next = 0;
        _playing = true;
        playNext();
    }

    // Cancel: stops the timer and drops the queue. The chunk already handed to
    // the motor (at most eight profiles) finishes on its own; the platform
    // offers no vibrate-cancel call.
    function stop() as Void {
        _timer.stop();
        _chunks = [] as Array<Array<Attention.VibeProfile>>;
        _durations = [] as Array<Number>;
        _next = 0;
        _playing = false;
    }

    // Timer callback: plays chunk _next and arms the timer for the following
    // one. No allocation here beyond what Attention.vibrate needs.
    function playNext() as Void {
        if (_next >= _chunks.size()) {
            _playing = false;
            return;
        }
        var index = _next;
        _next += 1;
        if (Attention has :vibrate) {
            Attention.vibrate(_chunks[index]);
        }
        var wait = _durations[index];
        if (wait < PULSE_MIN_TIMER_MS) { wait = PULSE_MIN_TIMER_MS; }
        // Arm the timer even after the last chunk so _playing clears when the
        // motor is actually done.
        _timer.start(method(:playNext), wait, false);
    }
}
