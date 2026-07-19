// File purpose: Selectable haptic time languages and their pure encoders.
package com.lukesteuber.pulsetime.shared.encoding

import java.util.Calendar

enum class PlaybackSpeed(val storageId: String, val label: String, val multiplier: Float) {
    Slow("slow", "Slow", 1.25f),
    Standard("standard", "Standard", 1f),
    Fast("fast", "Fast", 0.78f),
    ;

    companion object {
        fun fromStorage(value: String?): PlaybackSpeed = entries.firstOrNull { it.storageId == value } ?: Standard
    }
}

enum class TimeSystem(
    val storageId: String,
    val title: String,
    val shortLabel: String,
    val description: String,
    val inherentlyExact: Boolean,
) {
    TerseQuarter(
        "terse_quarter",
        "Terse Quarter",
        "Simple",
        "Hours, quarter-hours, and optional leftover-minute ticks.",
        false,
    ),
    FiveMinuteLadder(
        "five_minute_ladder",
        "Five-Minute Ladder",
        "Exact",
        "Long hours, medium five-minute steps, then short leftover minutes.",
        true,
    ),
    DecimalPlace(
        "decimal_place",
        "Decimal Place Rhythm",
        "Digits",
        "Hours, minute tens, and minute ones as three distinct pulse fields.",
        true,
    ),
    BinaryClock(
        "binary_clock",
        "Binary Clock",
        "Compact",
        "Four hour bits followed by six minute bits; short means zero, long means one.",
        true,
    ),
    NearestQuarter(
        "nearest_quarter",
        "Nearest-Quarter Offset",
        "Clock-style",
        "The nearest quarter-hour, followed by a before-or-after marker and offset.",
        true,
    ),
    MorseDigits(
        "morse_digits",
        "Morse Digits",
        "Morse",
        "The displayed hour and minute digits encoded with Morse numerals.",
        true,
    ),
    ;

    companion object {
        fun fromStorage(value: String?): TimeSystem = entries.firstOrNull { it.storageId == value } ?: TerseQuarter
    }
}

data class EncodingOptions(
    val amplitudeScale: Float = 1f,
    val layeredHaptics: Boolean = true,
    val playbackSpeed: PlaybackSpeed = PlaybackSpeed.Standard,
    val includePrecision: Boolean = false,
)

interface TimeEncoder {
    fun encode(hourOfDay: Int, minute: Int, options: EncodingOptions): EncodedPattern
}

object TimeEncoderRegistry {
    fun encode(
        system: TimeSystem,
        hourOfDay: Int,
        minute: Int,
        options: EncodingOptions,
    ): EncodedPattern {
        require(hourOfDay in 0..23)
        require(minute in 0..59)
        require(options.amplitudeScale in 0.05f..1.5f)
        val raw = when (system) {
            TimeSystem.TerseQuarter -> TerseQuarterEncoder.encode(
                hourOfDay,
                minute,
                includePrecision = options.includePrecision,
                amplitudeScale = options.amplitudeScale,
            )
            TimeSystem.FiveMinuteLadder -> FiveMinuteLadderEncoder.encode(hourOfDay, minute, options)
            TimeSystem.DecimalPlace -> DecimalPlaceEncoder.encode(hourOfDay, minute, options)
            TimeSystem.BinaryClock -> BinaryClockEncoder.encode(hourOfDay, minute, options)
            TimeSystem.NearestQuarter -> NearestQuarterEncoder.encode(hourOfDay, minute, options)
            TimeSystem.MorseDigits -> MorseDigitsEncoder.encode(hourOfDay, minute, options)
        }
        return if (system == TimeSystem.TerseQuarter) raw.withSpeed(options.playbackSpeed) else raw
    }

    fun encodeNow(
        system: TimeSystem,
        calendar: Calendar = Calendar.getInstance(),
        options: EncodingOptions,
    ): EncodedPattern = encode(system, calendar.get(Calendar.HOUR_OF_DAY), calendar.get(Calendar.MINUTE), options)
}

private const val FIELD_GAP = 700L
private const val GROUP_GAP = 420L
private const val SHORT_GAP = 150L

private fun scaled(base: Int, options: EncodingOptions, layer: Float): Int {
    val relative = if (options.layeredHaptics) layer else 1f
    return (base * options.amplitudeScale * relative).toInt().coerceIn(1, 255)
}

private fun pulse(on: Long, off: Long, options: EncodingOptions, layer: Float = 1f) =
    PulseSegment(
        onMs = (on * options.playbackSpeed.multiplier).toLong().coerceAtLeast(1L),
        offMs = (off * options.playbackSpeed.multiplier).toLong().coerceAtLeast(if (off > 0) 1L else 0L),
        amplitude = scaled(PulseSegment.BASE_AMPLITUDE, options, layer),
    )

private fun gap(ms: Long, options: EncodingOptions) =
    PulseSegment(0, (ms * options.playbackSpeed.multiplier).toLong().coerceAtLeast(1L), 1)

private fun ArrayList<PulseSegment>.counted(
    count: Int,
    on: Long,
    off: Long,
    options: EncodingOptions,
    layer: Float,
    groupAt: Int? = null,
) {
    repeat(count) { index ->
        add(pulse(on, if (index == count - 1) 0 else off, options, layer))
        if (groupAt != null && index + 1 < count && (index + 1) % groupAt == 0) add(gap(GROUP_GAP, options))
    }
}

private fun pattern(hourOfDay: Int, minute: Int, segments: List<PulseSegment>) = EncodedPattern(
    segments = segments,
    hour12 = TerseQuarterEncoder.hour12(hourOfDay),
    quarterIndex = minute / 15,
    minuteRemainder = minute % 15,
)

private fun EncodedPattern.withSpeed(speed: PlaybackSpeed): EncodedPattern {
    if (speed == PlaybackSpeed.Standard) return this
    return copy(segments = segments.map {
        it.copy(
            onMs = if (it.onMs == 0L) 0L else (it.onMs * speed.multiplier).toLong().coerceAtLeast(1L),
            offMs = if (it.offMs == 0L) 0L else (it.offMs * speed.multiplier).toLong().coerceAtLeast(1L),
        )
    })
}

object FiveMinuteLadderEncoder : TimeEncoder {
    override fun encode(hourOfDay: Int, minute: Int, options: EncodingOptions): EncodedPattern {
        val segments = arrayListOf<PulseSegment>()
        segments.counted(TerseQuarterEncoder.hour12(hourOfDay), 240, 190, options, 1f, 5)
        segments.add(gap(FIELD_GAP, options))
        segments.counted(minute / 5, 145, 145, options, .72f, 5)
        if (minute % 5 > 0) {
            segments.add(gap(GROUP_GAP, options))
            segments.counted(minute % 5, 75, 110, options, .45f)
        }
        return pattern(hourOfDay, minute, segments)
    }
}

object DecimalPlaceEncoder : TimeEncoder {
    override fun encode(hourOfDay: Int, minute: Int, options: EncodingOptions): EncodedPattern {
        val segments = arrayListOf<PulseSegment>()
        segments.counted(TerseQuarterEncoder.hour12(hourOfDay), 230, 180, options, 1f, 5)
        segments.add(gap(FIELD_GAP, options))
        addDigit(segments, minute / 10, options, 155, .72f)
        segments.add(gap(GROUP_GAP, options))
        addDigit(segments, minute % 10, options, 80, .45f)
        return pattern(hourOfDay, minute, segments)
    }

    private fun addDigit(out: ArrayList<PulseSegment>, digit: Int, options: EncodingOptions, on: Long, layer: Float) {
        if (digit == 0) out.add(pulse(on * 2, 0, options, layer))
        else out.counted(digit, on, SHORT_GAP, options, layer, 5)
    }
}

object BinaryClockEncoder : TimeEncoder {
    override fun encode(hourOfDay: Int, minute: Int, options: EncodingOptions): EncodedPattern {
        val segments = arrayListOf<PulseSegment>()
        addBits(segments, TerseQuarterEncoder.hour12(hourOfDay), 4, options)
        segments.add(gap(FIELD_GAP, options))
        addBits(segments, minute, 6, options)
        return pattern(hourOfDay, minute, segments)
    }

    private fun addBits(out: ArrayList<PulseSegment>, value: Int, width: Int, options: EncodingOptions) {
        for (bit in width - 1 downTo 0) {
            val one = value and (1 shl bit) != 0
            out.add(pulse(if (one) 210 else 75, SHORT_GAP, options, if (one) 1f else .45f))
        }
    }
}

object NearestQuarterEncoder : TimeEncoder {
    override fun encode(hourOfDay: Int, minute: Int, options: EncodingOptions): EncodedPattern {
        var landmark = ((minute + 7) / 15) * 15
        var landmarkHour = hourOfDay
        if (landmark == 60) {
            landmark = 0
            landmarkHour = (hourOfDay + 1) % 24
        }
        val absoluteNow = hourOfDay * 60 + minute
        var absoluteLandmark = landmarkHour * 60 + landmark
        if (hourOfDay == 23 && landmarkHour == 0) absoluteLandmark = 24 * 60
        val offset = absoluteNow - absoluteLandmark
        val segments = arrayListOf<PulseSegment>()
        segments.counted(TerseQuarterEncoder.hour12(landmarkHour), 230, 180, options, 1f, 5)
        segments.add(gap(FIELD_GAP, options))
        val landmarkCount = if (landmark == 0) 1 else landmark / 15 + 1
        segments.counted(landmarkCount, 150, 140, options, .72f)
        if (offset != 0) {
            segments.add(gap(GROUP_GAP, options))
            if (offset > 0) {
                segments.add(pulse(75, 100, options, .45f))
                segments.add(pulse(160, 0, options, 1f))
            } else {
                segments.add(pulse(160, 100, options, 1f))
                segments.add(pulse(75, 0, options, .45f))
            }
            segments.add(gap(GROUP_GAP, options))
            segments.counted(kotlin.math.abs(offset), 70, 105, options, .45f, 5)
        }
        return pattern(hourOfDay, minute, segments)
    }
}

object MorseDigitsEncoder : TimeEncoder {
    private val codes = listOf("-----", ".----", "..---", "...--", "....-", ".....", "-....", "--...", "---..", "----.")

    override fun encode(hourOfDay: Int, minute: Int, options: EncodingOptions): EncodedPattern {
        val hour = TerseQuarterEncoder.hour12(hourOfDay).toString()
        val minutes = minute.toString().padStart(2, '0')
        val segments = arrayListOf<PulseSegment>()
        hour.forEachIndexed { index, c ->
            addCode(segments, c.digitToInt(), options)
            if (index < hour.lastIndex) segments.add(gap(GROUP_GAP, options))
        }
        segments.add(gap(FIELD_GAP, options))
        minutes.forEachIndexed { index, c ->
            addCode(segments, c.digitToInt(), options)
            if (index < minutes.lastIndex) segments.add(gap(GROUP_GAP, options))
        }
        return pattern(hourOfDay, minute, segments)
    }

    private fun addCode(out: ArrayList<PulseSegment>, digit: Int, options: EncodingOptions) {
        codes[digit].forEach { mark ->
            val dash = mark == '-'
            out.add(pulse(if (dash) 230 else 75, SHORT_GAP, options, if (dash) 1f else .5f))
        }
    }
}
