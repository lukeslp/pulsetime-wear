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
    private fun fixtures(): File {
        var dir: File? = File("").absoluteFile
        while (dir != null) {
            val candidate = File(dir, "appstores/shared/fixtures/patterns.txt")
            if (candidate.exists()) return candidate
            dir = dir.parentFile
        }
        error("appstores/shared/fixtures/patterns.txt not found (run node appstores/shared/pulsetime-patterns.mjs)")
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
        val lines = fixtures().readLines().filter { it.isNotBlank() }
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
}
