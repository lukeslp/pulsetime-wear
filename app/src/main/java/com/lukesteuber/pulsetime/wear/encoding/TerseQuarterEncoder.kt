// File purpose: Pure terse-quarter haptic time encoding (no Android deps).
// Primary components: TerseQuarterEncoder, PulseSegment, EncodedPattern.
// I/O: hour/minute (or Calendar) → timed ON/OFF pulse segments.
package com.lukesteuber.pulsetime.wear.encoding

import java.util.Calendar

/**
 * One vibration segment. [onMs] > 0 means vibrate; 0 means silence.
 * [amplitude] is 1–255 when on (ignored for silence).
 */
data class PulseSegment(
    val onMs: Long,
    val offMs: Long = 0,
    val amplitude: Int = BASE_AMPLITUDE,
) {
    init {
        require(onMs >= 0 && offMs >= 0)
        require(amplitude in 1..255)
    }

    companion object {
        const val BASE_AMPLITUDE = 180
    }
}

data class EncodedPattern(
    val segments: List<PulseSegment>,
    val hour12: Int,
    val quarterIndex: Int,
    val minuteRemainder: Int,
) {
    /**
     * Flatten to VibrationEffect.createWaveform(timings, amplitudes) slots.
     * Each index is a duration with matching amplitude (0 = silence).
     * Starts with a 0ms delay. Silence segments become amp-0 slots — never
     * merge into a preceding ON duration (that glued hour+gap+quarter together).
     */
    fun toTimings(): LongArray = toWaveform().first

    fun toAmplitudes(): IntArray = toWaveform().second

    fun toWaveform(): Pair<LongArray, IntArray> {
        val timings = ArrayList<Long>()
        val amps = ArrayList<Int>()
        timings.add(0L)
        amps.add(0)

        fun appendSilence(ms: Long) {
            if (ms <= 0L) return
            if (amps.isNotEmpty() && amps.last() == 0) {
                timings[timings.lastIndex] = timings.last() + ms
            } else {
                timings.add(ms)
                amps.add(0)
            }
        }

        for (seg in segments) {
            if (seg.onMs > 0) {
                timings.add(seg.onMs)
                amps.add(seg.amplitude)
            }
            appendSilence(seg.offMs)
        }

        return timings.toLongArray() to amps.toIntArray()
    }
}

object TerseQuarterTimings {
    // Gaps sized for Pixel Watch motor settle — short offs smear into one buzz.
    const val HOUR_ON_MS = 200L
    const val HOUR_OFF_MS = 280L
    const val GAP_MS = 700L
    const val QUARTER_ON_MS = 320L
    const val QUARTER_OFF_MS = 280L
    const val MINUTE_ON_MS = 80L
    const val MINUTE_OFF_MS = 120L
}

/**
 * Terse-quarter encoding:
 * - Hours 1–12 as pulses (midnight/noon = 12)
 * - Long silence gap
 * - Quarters 0–3 for :00–:14, :15–:29, :30–:44, :45–:59
 * - Optional remainder minutes within the quarter (0–14) as short ticks
 */
object TerseQuarterEncoder {

    fun hour12(hourOfDay: Int): Int {
        require(hourOfDay in 0..23)
        val h = hourOfDay % 12
        return if (h == 0) 12 else h
    }

    fun quarterIndex(minute: Int): Int {
        require(minute in 0..59)
        return minute / 15
    }

    fun minuteRemainder(minute: Int): Int {
        require(minute in 0..59)
        return minute % 15
    }

    fun encode(
        hourOfDay: Int,
        minute: Int,
        includePrecision: Boolean = false,
        amplitudeScale: Float = 1f,
    ): EncodedPattern {
        require(hourOfDay in 0..23)
        require(minute in 0..59)
        require(amplitudeScale in 0.05f..1.5f)

        val h = hour12(hourOfDay)
        val q = quarterIndex(minute)
        val rem = minuteRemainder(minute)
        val amp = scaledAmp(amplitudeScale)

        val segments = ArrayList<PulseSegment>()
        repeat(h) { i ->
            val off = if (i < h - 1) TerseQuarterTimings.HOUR_OFF_MS else 0L
            segments.add(
                PulseSegment(
                    onMs = TerseQuarterTimings.HOUR_ON_MS,
                    offMs = off,
                    amplitude = amp,
                ),
            )
        }
        segments.add(PulseSegment(onMs = 0, offMs = TerseQuarterTimings.GAP_MS, amplitude = amp))

        if (q > 0) {
            repeat(q) { i ->
                val off = if (i < q - 1) TerseQuarterTimings.QUARTER_OFF_MS else 0L
                segments.add(
                    PulseSegment(
                        onMs = TerseQuarterTimings.QUARTER_ON_MS,
                        offMs = off,
                        amplitude = amp,
                    ),
                )
            }
        }

        if (includePrecision && rem > 0) {
            // Gap before remainder ticks
            segments.add(PulseSegment(onMs = 0, offMs = TerseQuarterTimings.GAP_MS / 2, amplitude = amp))
            repeat(rem) { i ->
                val off = if (i < rem - 1) TerseQuarterTimings.MINUTE_OFF_MS else 0L
                segments.add(
                    PulseSegment(
                        onMs = TerseQuarterTimings.MINUTE_ON_MS,
                        offMs = off,
                        amplitude = amp,
                    ),
                )
            }
        }

        return EncodedPattern(segments, h, q, rem)
    }

    fun encodeNow(
        calendar: Calendar = Calendar.getInstance(),
        includePrecision: Boolean = false,
        amplitudeScale: Float = 1f,
    ): EncodedPattern =
        encode(
            hourOfDay = calendar.get(Calendar.HOUR_OF_DAY),
            minute = calendar.get(Calendar.MINUTE),
            includePrecision = includePrecision,
            amplitudeScale = amplitudeScale,
        )

    private fun scaledAmp(scale: Float): Int {
        val v = (PulseSegment.BASE_AMPLITUDE * scale).toInt()
        return v.coerceIn(1, 255)
    }
}
