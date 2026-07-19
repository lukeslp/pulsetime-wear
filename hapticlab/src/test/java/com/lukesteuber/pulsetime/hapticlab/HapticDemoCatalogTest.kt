package com.lukesteuber.pulsetime.hapticlab

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class HapticDemoCatalogTest {
    @Test fun catalogHasTwelveBoundedDistinctDemos() {
        val demos = HapticDemoCatalog.all
        assertEquals(12, demos.size)
        assertEquals(demos.size, demos.map { it.id }.toSet().size)
        assertEquals(demos.size, demos.map { it.quizAnswer }.toSet().size)
        demos.forEach { demo ->
            assertTrue(demo.pattern.durationMs in 1..8_000)
            assertTrue(demo.quizChoices.contains(demo.quizAnswer))
            assertEquals(demo.quizChoices.size, demo.quizChoices.toSet().size)
            assertTrue(demo.grammar.topic.isNotBlank())
            assertTrue(demo.grammar.payload.isNotBlank())
            assertTrue(demo.pattern.steps.all { it.onMs >= 0 && it.offMs >= 0 && it.amplitude in 1..255 })
            demo.headlinePattern?.let { assertTrue(it.durationMs < demo.pattern.durationMs) }
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

    @Test fun comparisonCatalogOffersDistinctBoundedPairs() {
        assertEquals(3, SignalComparisonCatalog.all.size)
        SignalComparisonCatalog.all.forEach { comparison ->
            assertTrue(comparison.first.durationMs in 1..8_000)
            assertTrue(comparison.second.durationMs in 1..8_000)
            assertTrue(comparison.first != comparison.second)
        }
    }

    @Test fun personalBuilderPreservesAccessibleTokens() {
        val steps = listOf(
            PersonalPatternBuilder.short(105),
            PersonalPatternBuilder.pause(),
            PersonalPatternBuilder.long(230),
        )
        val pattern = PersonalPatternBuilder.build(steps)!!
        assertEquals(steps, pattern.steps)
        assertEquals(null, PersonalPatternBuilder.build(emptyList()))
        assertTrue(pattern.durationMs <= 8_000)
    }
}
