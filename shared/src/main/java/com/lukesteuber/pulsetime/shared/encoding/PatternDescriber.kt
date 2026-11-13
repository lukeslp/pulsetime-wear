// File purpose: Words for a haptic time pattern, so it can be read or spoken as well as felt.
package com.lukesteuber.pulsetime.shared.encoding

/**
 * Describes, in plain words, the pulses an encoder plays for a time. The
 * screens use it as visible text and as the screen-reader description of a
 * pattern, so nobody has to decode a row of dashes and dots.
 *
 * [parts] is the single source: [describe] joins it and [pulseCount] sums it,
 * and a test holds [pulseCount] to what the encoders really emit.
 */
object PatternDescriber {
    data class Part(val text: String, val pulses: Int)

    fun describe(system: TimeSystem, hourOfDay: Int, minute: Int, includePrecision: Boolean = false): String =
        parts(system, hourOfDay, minute, includePrecision).joinToString(", then ") { it.text }
            .replaceFirstChar { it.uppercase() } + "."

    fun pulseCount(system: TimeSystem, hourOfDay: Int, minute: Int, includePrecision: Boolean = false): Int =
        parts(system, hourOfDay, minute, includePrecision).sumOf { it.pulses }

    fun parts(system: TimeSystem, hourOfDay: Int, minute: Int, includePrecision: Boolean = false): List<Part> {
        require(hourOfDay in 0..23)
        require(minute in 0..59)
        val hour = TerseQuarterEncoder.hour12(hourOfDay)
        return when (system) {
            TimeSystem.TerseQuarter -> buildList {
                add(counted(hour, "hour pulse"))
                val quarters = minute / 15
                add(if (quarters == 0) Part("no quarter pulses", 0) else counted(quarters, "long quarter pulse"))
                val rest = minute % 15
                if (includePrecision && rest > 0) add(counted(rest, "short minute tick"))
            }
            TimeSystem.FiveMinuteLadder -> buildList {
                add(counted(hour, "long hour pulse"))
                val fives = minute / 5
                add(if (fives == 0) Part("no five-minute pulses", 0) else counted(fives, "medium five-minute pulse"))
                if (minute % 5 > 0) add(counted(minute % 5, "short minute tick"))
            }
            TimeSystem.DecimalPlace -> listOf(
                counted(hour, "hour pulse"),
                digit(minute / 10, "firm pulse", "tens"),
                digit(minute % 10, "short pulse", "ones"),
            )
            TimeSystem.BinaryClock -> listOf(
                Part("hour bits ${bits(hour, 4)}", 4),
                Part("minute bits ${bits(minute, 6)}", 6),
            )
            TimeSystem.NearestQuarter -> buildList {
                var landmark = ((minute + 7) / 15) * 15
                var landmarkHour = hourOfDay
                if (landmark == 60) {
                    landmark = 0
                    landmarkHour = (hourOfDay + 1) % 24
                }
                val offset = minute - if (landmark == 0 && minute > 30) 60 else landmark
                add(counted(TerseQuarterEncoder.hour12(landmarkHour), "hour pulse"))
                val count = landmark / 15 + 1
                add(Part("${number(count)} landmark ${plural(count, "pulse")} for :${landmark.toString().padStart(2, '0')}", count))
                if (offset != 0) {
                    add(
                        if (offset > 0) Part("short then long, meaning after", 2)
                        else Part("long then short, meaning before", 2),
                    )
                    add(counted(kotlin.math.abs(offset), "minute tick"))
                }
            }
            TimeSystem.MorseDigits -> {
                val digits = hour.toString() + minute.toString().padStart(2, '0')
                digits.map { c -> Part("digit $c as ${morse(c.digitToInt())}", 5) }
            }
        }
    }

    private val words = listOf(
        "zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten",
        "eleven", "twelve", "thirteen", "fourteen",
    )

    private fun number(n: Int) = words.getOrElse(n) { n.toString() }
    private fun plural(n: Int, noun: String) = if (n == 1) noun else noun + "s"
    private fun counted(n: Int, noun: String) = Part("${number(n)} ${plural(n, noun)}", n)

    private fun digit(value: Int, noun: String, place: String) =
        if (value == 0) Part("one held pulse for zero $place", 1)
        else Part("${number(value)} ${plural(value, noun)} for the $place", value)

    private fun bits(value: Int, width: Int) =
        (width - 1 downTo 0).joinToString(" ") { if (value and (1 shl it) != 0) "long" else "short" }

    private val morseCodes = listOf("-----", ".----", "..---", "...--", "....-", ".....", "-....", "--...", "---..", "----.")
    private fun morse(digit: Int) = morseCodes[digit].map { if (it == '-') "long" else "short" }.joinToString(" ")
}
