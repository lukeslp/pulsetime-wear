// File purpose: JVM unit tests for terse-quarter encoding.
// Primary components: hour/quarter/remainder fixtures and waveform shape checks.
// I/O: Fixed clock values → expected pulse counts and timing invariants.
package com.lukesteuber.pulsetime.wear.encoding

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.util.Calendar

class TerseQuarterEncoderTest {

    @Test
    fun hour12_mapsMidnightAndNoonToTwelve() {
        assertEquals(12, TerseQuarterEncoder.hour12(0))
        assertEquals(12, TerseQuarterEncoder.hour12(12))
        assertEquals(3, TerseQuarterEncoder.hour12(15))
        assertEquals(1, TerseQuarterEncoder.hour12(13))
    }

    @Test
    fun quarterIndex_bucketsMinutes() {
        assertEquals(0, TerseQuarterEncoder.quarterIndex(0))
        assertEquals(0, TerseQuarterEncoder.quarterIndex(14))
        assertEquals(1, TerseQuarterEncoder.quarterIndex(15))
        assertEquals(1, TerseQuarterEncoder.quarterIndex(29))
        assertEquals(2, TerseQuarterEncoder.quarterIndex(30))
        assertEquals(2, TerseQuarterEncoder.quarterIndex(44))
        assertEquals(3, TerseQuarterEncoder.quarterIndex(45))
        assertEquals(3, TerseQuarterEncoder.quarterIndex(59))
    }

    @Test
    fun minuteRemainder_withinQuarter() {
        assertEquals(0, TerseQuarterEncoder.minuteRemainder(15))
        assertEquals(5, TerseQuarterEncoder.minuteRemainder(20))
        assertEquals(14, TerseQuarterEncoder.minuteRemainder(29))
    }

    @Test
    fun threeTwenty_isThreeHoursAndOneQuarter() {
        val p = TerseQuarterEncoder.encode(3, 20, includePrecision = false)
        assertEquals(3, p.hour12)
        assertEquals(1, p.quarterIndex)
        assertEquals(5, p.minuteRemainder)
        val ons = p.segments.filter { it.onMs > 0 }
        assertEquals(4, ons.size)
        assertEquals(TerseQuarterTimings.HOUR_ON_MS, ons[0].onMs)
        assertEquals(TerseQuarterTimings.QUARTER_ON_MS, ons[3].onMs)
        assertTrue(p.segments.any { it.onMs == 0L && it.offMs == TerseQuarterTimings.GAP_MS })
    }

    @Test
    fun onTheHour_noQuarterPulses() {
        val p = TerseQuarterEncoder.encode(10, 0, includePrecision = false)
        assertEquals(10, p.hour12)
        assertEquals(0, p.quarterIndex)
        assertEquals(10, p.segments.count { it.onMs > 0 })
    }

    @Test
    fun noon_twelveHourPulses() {
        val p = TerseQuarterEncoder.encode(12, 0)
        assertEquals(12, p.hour12)
        assertEquals(12, p.segments.count { it.onMs == TerseQuarterTimings.HOUR_ON_MS })
    }

    @Test
    fun midnight_twelveHourPulses() {
        val p = TerseQuarterEncoder.encode(0, 45)
        assertEquals(12, p.hour12)
        assertEquals(3, p.quarterIndex)
        assertEquals(12 + 3, p.segments.count { it.onMs > 0 })
    }

    @Test
    fun precision_addsRemainderTicks() {
        val p = TerseQuarterEncoder.encode(3, 20, includePrecision = true)
        assertEquals(5, p.segments.count { it.onMs == TerseQuarterTimings.MINUTE_ON_MS })
    }

    @Test
    fun precisionOff_skipsRemainder() {
        val p = TerseQuarterEncoder.encode(3, 20, includePrecision = false)
        assertEquals(0, p.segments.count { it.onMs == TerseQuarterTimings.MINUTE_ON_MS })
    }

    @Test
    fun amplitudeScale_reducesIntensity() {
        val soft = TerseQuarterEncoder.encode(1, 0, amplitudeScale = 0.3f)
        val hard = TerseQuarterEncoder.encode(1, 0, amplitudeScale = 1.0f)
        val softAmp = soft.segments.first { it.onMs > 0 }.amplitude
        val hardAmp = hard.segments.first { it.onMs > 0 }.amplitude
        assertTrue(softAmp < hardAmp)
        assertTrue(softAmp >= 1)
        assertTrue(hardAmp <= 255)
    }

    @Test
    fun waveform_timingsAndAmplitudesMatchLength() {
        val p = TerseQuarterEncoder.encode(2, 35, includePrecision = true)
        val t = p.toTimings()
        val a = p.toAmplitudes()
        assertEquals(t.size, a.size)
        assertTrue(t.size >= 2)
        assertEquals(0, a[0])
    }

    @Test
    fun quarterFortyFive_threeQuarters() {
        val p = TerseQuarterEncoder.encode(7, 50)
        assertEquals(7, p.hour12)
        assertEquals(3, p.quarterIndex)
        assertEquals(5, p.minuteRemainder)
    }

    @Test
    fun oneOhFive_oneHourZeroQuarters() {
        val p = TerseQuarterEncoder.encode(13, 5)
        assertEquals(1, p.hour12)
        assertEquals(0, p.quarterIndex)
    }

    @Test
    fun afternoonThreeThirty_matchesMorningThreeThirtyHours() {
        val am = TerseQuarterEncoder.encode(3, 30)
        val pm = TerseQuarterEncoder.encode(15, 30)
        assertEquals(am.hour12, pm.hour12)
        assertEquals(am.quarterIndex, pm.quarterIndex)
    }

    @Test
    fun gap_alwaysPresentBetweenHourAndQuarterBlocks() {
        for (q in 0..3) {
            val minute = q * 15
            val p = TerseQuarterEncoder.encode(4, minute)
            val gapIdx = p.segments.indexOfFirst { it.onMs == 0L && it.offMs == TerseQuarterTimings.GAP_MS }
            assertTrue("missing gap for :$minute", gapIdx >= 0)
        }
    }

    @Test
    fun timings_arePositiveAfterInitialDelay() {
        val p = TerseQuarterEncoder.encode(9, 40, includePrecision = true)
        val t = p.toTimings()
        assertEquals(0L, t[0])
        for (i in 1 until t.size) {
            assertTrue("timing[$i]=${t[i]}", t[i] > 0)
        }
    }

    @Test
    fun elevenFiftyNine_withPrecision() {
        val p = TerseQuarterEncoder.encode(23, 59, includePrecision = true)
        assertEquals(11, p.hour12)
        assertEquals(3, p.quarterIndex)
        assertEquals(14, p.minuteRemainder)
        assertEquals(14, p.segments.count { it.onMs == TerseQuarterTimings.MINUTE_ON_MS })
    }

    @Test
    fun encodeNow_usesCalendarFields() {
        val cal = Calendar.getInstance().apply {
            set(Calendar.HOUR_OF_DAY, 8)
            set(Calendar.MINUTE, 22)
            set(Calendar.SECOND, 0)
        }
        val p = TerseQuarterEncoder.encodeNow(cal, includePrecision = false)
        assertEquals(8, p.hour12)
        assertEquals(1, p.quarterIndex)
    }

    @Test
    fun softestScale_stillAudibleAmplitude() {
        val p = TerseQuarterEncoder.encode(1, 0, amplitudeScale = 0.3f)
        assertTrue(p.segments.first { it.onMs > 0 }.amplitude >= 54)
    }

    @Test
    fun fifteenMinutes_oneQuarterOnly() {
        val p = TerseQuarterEncoder.encode(6, 15)
        assertEquals(1, p.quarterIndex)
        assertEquals(1, p.segments.count { it.onMs == TerseQuarterTimings.QUARTER_ON_MS })
    }
}
