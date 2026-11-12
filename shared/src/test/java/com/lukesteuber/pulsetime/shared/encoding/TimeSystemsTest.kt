package com.lukesteuber.pulsetime.shared.encoding

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import com.lukesteuber.pulsetime.shared.SystemGuideCatalog

class TimeSystemsTest {
    private val standard = EncodingOptions(amplitudeScale = 1f, layeredHaptics = true)

    @Test
    fun everyEncoderHasExactlyOneSharedGuide() {
        assertEquals(TimeSystem.entries.map { it.storageId }.toSet(), SystemGuideCatalog.all.map { it.id }.toSet())
    }

    @Test
    fun everyGuideQuizOffersItsAnswerOnce() {
        SystemGuideCatalog.all.forEach { guide ->
            assertEquals(guide.id, 1, guide.quiz.choices.count { it == guide.quiz.answer })
            assertEquals(guide.id, "%d:%02d".format(guide.quiz.hour, guide.quiz.minute), guide.quiz.answer)
        }
    }

    @Test
    fun nearestQuarterLandmarkCountsMatchTheGuideLegend() {
        // The guide says: one pulse for :00, two for :15, three for :30, four for :45.
        val landmarkOn = 150L
        listOf(0 to 1, 15 to 2, 30 to 3, 45 to 4).forEach { (minute, count) ->
            val pattern = TimeEncoderRegistry.encode(TimeSystem.NearestQuarter, 3, minute, standard)
            assertEquals("minute $minute", count, pattern.segments.count { it.onMs == landmarkOn })
        }
    }

    @Test
    fun registry_encodesEverySystemAtBoundaries() {
        val times = listOf(0 to 0, 0 to 59, 3 to 42, 11 to 59, 12 to 0, 23 to 59)
        TimeSystem.entries.forEach { system ->
            times.forEach { (hour, minute) ->
                val pattern = TimeEncoderRegistry.encode(system, hour, minute, standard)
                assertTrue("$system $hour:$minute has no pulses", pattern.segments.any { it.onMs > 0 })
                val (timings, amplitudes) = pattern.toWaveform()
                assertEquals(timings.size, amplitudes.size)
                assertEquals(0L, timings.first())
                assertEquals(0, amplitudes.first())
                timings.drop(1).forEach { assertTrue(it > 0) }
                amplitudes.forEach { assertTrue(it in 0..255) }
                assertTrue("$system pattern too long", timings.sum() < 30_000L)
            }
        }
    }

    @Test
    fun storageIds_areStableUniqueAndFallBackSafely() {
        assertEquals(TimeSystem.entries.size, TimeSystem.entries.map { it.storageId }.toSet().size)
        assertEquals(TimeSystem.TerseQuarter, TimeSystem.fromStorage(null))
        assertEquals(TimeSystem.TerseQuarter, TimeSystem.fromStorage("removed_future_system"))
        TimeSystem.entries.forEach { assertEquals(it, TimeSystem.fromStorage(it.storageId)) }
        assertEquals(PlaybackSpeed.Standard, PlaybackSpeed.fromStorage(null))
        assertEquals(PlaybackSpeed.Standard, PlaybackSpeed.fromStorage("warp"))
    }

    @Test
    fun fiveMinuteLadder_usesThreePerceptibleLayersAtThreeFortyTwo() {
        val p = FiveMinuteLadderEncoder.encode(3, 42, standard)
        val on = p.segments.filter { it.onMs > 0 }
        assertEquals(3 + 8 + 2, on.size)
        assertTrue(on.map { it.onMs }.toSet().size >= 3)
        assertTrue(on.map { it.amplitude }.toSet().size >= 3)
    }

    @Test
    fun decimalPlace_encodesHourTensAndOnes() {
        val p = DecimalPlaceEncoder.encode(3, 42, standard)
        val on = p.segments.filter { it.onMs > 0 }
        assertEquals(3 + 4 + 2, on.size)
        assertTrue(on.take(3).all { it.onMs > on.last().onMs })
    }

    @Test
    fun binaryClock_hasTenFixedBitCells() {
        val p = BinaryClockEncoder.encode(3, 42, standard)
        assertEquals(10, p.segments.count { it.onMs > 0 })
        assertEquals(2, p.segments.filter { it.onMs > 0 }.map { it.onMs }.toSet().size)
    }

    @Test
    fun nearestQuarter_threeFortyTwoEncodesBeforeThreeFortyFive() {
        val p = NearestQuarterEncoder.encode(3, 42, standard)
        val on = p.segments.filter { it.onMs > 0 }
        assertEquals(3 + 4 + 2 + 3, on.size)
        val direction = on.drop(7).take(2)
        assertTrue(direction[0].onMs > direction[1].onMs)
    }

    @Test
    fun morseDigits_alwaysUsesFiveMarksPerDigit() {
        val oneDigitHour = MorseDigitsEncoder.encode(3, 42, standard)
        val twoDigitHour = MorseDigitsEncoder.encode(12, 0, standard)
        assertEquals(15, oneDigitHour.segments.count { it.onMs > 0 })
        assertEquals(20, twoDigitHour.segments.count { it.onMs > 0 })
    }

    @Test
    fun layeredToggleKeepsRhythmButFlattensRelativeStrength() {
        val layered = FiveMinuteLadderEncoder.encode(3, 42, standard)
        val flat = FiveMinuteLadderEncoder.encode(3, 42, standard.copy(layeredHaptics = false))
        assertEquals(layered.segments.map { it.onMs to it.offMs }, flat.segments.map { it.onMs to it.offMs })
        assertNotEquals(1, layered.segments.filter { it.onMs > 0 }.map { it.amplitude }.toSet().size)
        assertEquals(1, flat.segments.filter { it.onMs > 0 }.map { it.amplitude }.toSet().size)
    }

    @Test
    fun playbackSpeedChangesDurationWithoutChangingPulseCount() {
        val slow = TimeEncoderRegistry.encode(
            TimeSystem.MorseDigits,
            3,
            42,
            standard.copy(playbackSpeed = PlaybackSpeed.Slow),
        )
        val fast = TimeEncoderRegistry.encode(
            TimeSystem.MorseDigits,
            3,
            42,
            standard.copy(playbackSpeed = PlaybackSpeed.Fast),
        )
        assertEquals(slow.segments.count { it.onMs > 0 }, fast.segments.count { it.onMs > 0 })
        assertTrue(slow.toTimings().sum() > fast.toTimings().sum())
    }
}
