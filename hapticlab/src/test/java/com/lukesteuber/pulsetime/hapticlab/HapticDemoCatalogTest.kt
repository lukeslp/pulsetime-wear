package com.lukesteuber.pulsetime.hapticlab

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class HapticDemoCatalogTest {
    @Test fun catalogHasSixBoundedDistinctDemos() {
        val demos = HapticDemoCatalog.all
        assertEquals(6, demos.size)
        assertEquals(demos.size, demos.map { it.id }.toSet().size)
        demos.forEach { demo ->
            assertTrue(demo.pattern.durationMs in 1..8_000)
            assertTrue(demo.quizChoices.contains(demo.quizAnswer))
            assertTrue(demo.pattern.steps.all { it.onMs >= 0 && it.offMs >= 0 && it.amplitude in 1..255 })
        }
    }

    @Test fun waveformAlwaysHasMatchingTimingAndAmplitudeSlots() {
        HapticDemoCatalog.all.forEach { demo ->
            val (timings, amplitudes) = demo.pattern.waveform()
            assertEquals(timings.size, amplitudes.size)
            assertEquals(0L, timings.first())
            assertEquals(0, amplitudes.first())
            amplitudes.asList().zipWithNext().forEach { (first, second) ->
                assertTrue((first == 0) != (second == 0))
            }
        }
    }

    @Test fun slowPlaybackPreservesPatternAndExtendsDuration() {
        val pattern = HapticDemoCatalog.all.first().pattern
        val normal = pattern.waveform().first.sum()
        val slow = pattern.waveform(1.5f).first.sum()
        assertTrue(slow > normal)
    }
}
