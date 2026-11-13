package com.lukesteuber.pulsetime.shared.encoding

import org.junit.Assert.assertEquals
import org.junit.Test

class PatternDescriberTest {
    @Test
    fun describedPulseCountMatchesEveryEncoderAtEveryMinute() {
        for (system in TimeSystem.entries) for (precision in listOf(false, true)) for (hour in 0..23) for (minute in 0..59) {
            val pattern = TimeEncoderRegistry.encode(system, hour, minute, EncodingOptions(includePrecision = precision))
            assertEquals(
                "$system $hour:$minute precision=$precision",
                pattern.segments.count { it.onMs > 0 },
                PatternDescriber.pulseCount(system, hour, minute, precision),
            )
        }
    }

    @Test
    fun wordsForKnownTimes() {
        assertEquals(
            "Three hour pulses, then two long quarter pulses, then twelve short minute ticks.",
            PatternDescriber.describe(TimeSystem.TerseQuarter, 3, 42, includePrecision = true),
        )
        assertEquals("Three hour pulses, then no quarter pulses.", PatternDescriber.describe(TimeSystem.TerseQuarter, 15, 0))
        assertEquals(
            "Three long hour pulses, then eight medium five-minute pulses, then two short minute ticks.",
            PatternDescriber.describe(TimeSystem.FiveMinuteLadder, 3, 42),
        )
        assertEquals(
            "Three hour pulses, then two firm pulses for the tens, then one held pulse for zero ones.",
            PatternDescriber.describe(TimeSystem.DecimalPlace, 3, 20),
        )
        assertEquals(
            "Hour bits short short long long, then minute bits long short long short long short.",
            PatternDescriber.describe(TimeSystem.BinaryClock, 3, 42),
        )
        assertEquals(
            "Three hour pulses, then four landmark pulses for :45, then long then short, meaning before, then three minute ticks.",
            PatternDescriber.describe(TimeSystem.NearestQuarter, 3, 42),
        )
        assertEquals(
            "Twelve hour pulses, then one landmark pulse for :00, then long then short, meaning before, then two minute ticks.",
            PatternDescriber.describe(TimeSystem.NearestQuarter, 23, 58),
        )
        assertEquals(
            "Digit 3 as short short short long long, then digit 4 as short short short short long, then digit 2 as short short long long long.",
            PatternDescriber.describe(TimeSystem.MorseDigits, 3, 42),
        )
    }
}
