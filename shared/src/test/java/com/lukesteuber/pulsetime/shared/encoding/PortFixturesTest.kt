// File purpose: Pin the wearable ports (Garmin, Pebble) to the Kotlin encoders.
// appstores/shared/pulsetime-patterns.mjs is a reference port that generates
// appstores/shared/fixtures/patterns.txt; this test proves that reference
// matches the real encoders, so anything that matches the fixtures matches us.
package com.lukesteuber.pulsetime.shared.encoding

import java.io.File
import kotlin.math.roundToInt
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class PortFixturesTest {
    private fun fixtures(name: String): File {
        var dir: File? = File("").absoluteFile
        while (dir != null) {
            val candidate = File(dir, "appstores/shared/fixtures/$name")
            if (candidate.exists()) return candidate
            dir = dir.parentFile
        }
        error("appstores/shared/fixtures/$name not found (run node appstores/shared/pulsetime-patterns.mjs)")
    }

    private fun layerPercent(seg: PulseSegment): Int {
        if (seg.onMs == 0L) return 0
        // Fixtures carry the relative layer, not the 0–255 amplitude.
        return (seg.amplitude * 100f / PulseSegment.BASE_AMPLITUDE).roundToInt()
    }

    private fun compact(pattern: EncodedPattern): String =
        pattern.segments.joinToString(" ") { "${it.onMs}/${it.offMs}/${layerPercent(it)}" }

    @Test
    fun referencePortMatchesEncoders() {
        val lines = fixtures("patterns.txt").readLines().filter { it.isNotBlank() }
        assertTrue("fixtures present", lines.size >= 400)
        var checked = 0
        for (line in lines) {
            val parts = line.split(" ", limit = 6)
            val system = TimeSystem.fromStorage(parts[0])
            val speed = PlaybackSpeed.fromStorage(parts[1])
            val precision = parts[2] == "1"
            val hour = parts[3].toInt()
            val minute = parts[4].toInt()
            val expected = parts[5]
            val encoded = TimeEncoderRegistry.encode(
                system, hour, minute,
                EncodingOptions(amplitudeScale = 1f, layeredHaptics = true, playbackSpeed = speed, includePrecision = precision),
            )
            assertEquals("$system $speed precision=$precision $hour:$minute", expected, compact(encoded))
            checked++
        }
        assertEquals(lines.size, checked)
    }

    /**
     * The hours-only / minutes-only projections used by Pebble's Quick Launch
     * buttons. Their own fixture file, because patterns.txt is parsed by three
     * harnesses and the Garmin port implements no scopes.
     */
    @Test
    fun referencePortMatchesScopedEncoders() {
        val lines = fixtures("patterns-scoped.txt").readLines().filter { it.isNotBlank() }
        assertTrue("scoped fixtures present", lines.size >= 800)
        var checked = 0
        for (line in lines) {
            val parts = line.split(" ", limit = 7)
            val system = TimeSystem.fromStorage(parts[0])
            val speed = PlaybackSpeed.fromStorage(parts[1])
            val precision = parts[2] == "1"
            val scope = TimeScope.fromStorage(parts[3])
            val hour = parts[4].toInt()
            val minute = parts[5].toInt()
            val expected = parts[6]
            assertTrue("scope column parsed: ${parts[3]}", scope != TimeScope.Full)
            val encoded = TimeEncoderRegistry.encode(
                system, hour, minute,
                EncodingOptions(amplitudeScale = 1f, layeredHaptics = true, playbackSpeed = speed, includePrecision = precision),
                scope,
            )
            assertEquals("$system $speed precision=$precision $scope $hour:$minute", expected, compact(encoded))
            checked++
        }
        assertEquals(lines.size, checked)
    }

    /** A scoped pattern must always say something. Silence reads as a fault. */
    @Test
    fun everyScopedPatternHasAPulse() {
        for (system in TimeSystem.entries) {
            for (speed in PlaybackSpeed.entries) {
                for (scope in listOf(TimeScope.HoursOnly, TimeScope.MinutesOnly)) {
                    for (hour in 0..23) {
                        for (minute in 0..59) {
                            val encoded = TimeEncoderRegistry.encode(
                                system, hour, minute,
                                EncodingOptions(playbackSpeed = speed, includePrecision = false),
                                scope,
                            )
                            assertTrue(
                                "$system $speed $scope $hour:$minute is silent",
                                encoded.segments.any { it.onMs > 0L },
                            )
                        }
                    }
                }
            }
        }
    }
}
