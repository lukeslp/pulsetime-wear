package com.lukesteuber.pulsetime.shared

data class PulseLegend(val name: String, val meaning: String, val mark: String)
data class GuideExample(val label: String, val hour: Int, val minute: Int, val breakdown: String, val score: String)
data class GuideQuiz(val hour: Int, val minute: Int, val choices: List<String>, val answer: String, val explanation: String)

data class SystemGuide(
    val id: String,
    val title: String,
    val shortLabel: String,
    val summary: String,
    val mentalModel: String,
    val legends: List<PulseLegend>,
    val examples: List<GuideExample>,
    val quiz: GuideQuiz,
)

object SystemGuideCatalog {
    val all = listOf(
        SystemGuide(
            "terse_quarter", "Terse Quarter", "Simple",
            "A quick estimate, with optional ticks for exact minutes.",
            "Count the hour pulses, pause, then count how many quarter-hours have passed.",
            listOf(PulseLegend("Hour pulse", "One hour", "—"), PulseLegend("Quarter pulse", "Fifteen minutes", "━"), PulseLegend("Minute tick", "One leftover minute", "·")),
            listOf(
                GuideExample("3:00", 3, 0, "Three hour pulses; no quarter pulses.", "— — —  /"),
                GuideExample("3:42", 3, 42, "Three hours, two quarters, twelve optional minute ticks.", "— — —  /  ━ ━  /  ·×12"),
            ),
            GuideQuiz(3, 30, listOf("3:15", "3:30", "3:45"), "3:30", "Three hour pulses followed by two quarter pulses means 3:30."),
        ),
        SystemGuide(
            "five_minute_ladder", "Five-Minute Ladder", "Exact",
            "Exact time using three easy-to-distinguish pulse sizes.",
            "Count hours, then five-minute steps, then the remaining one-minute ticks.",
            listOf(PulseLegend("Long pulse", "One hour", "—"), PulseLegend("Medium pulse", "Five minutes", "–"), PulseLegend("Short tick", "One minute", "·")),
            listOf(
                GuideExample("3:20", 3, 20, "Three hours and four five-minute steps.", "— — —  /  – – – –"),
                GuideExample("3:42", 3, 42, "Three hours, eight five-minute steps, and two minutes.", "— — —  /  –×8  /  · ·"),
            ),
            GuideQuiz(4, 27, listOf("4:22", "4:27", "5:27"), "4:27", "Four long pulses, five medium pulses, and two short ticks means 4:27."),
        ),
        SystemGuide(
            "decimal_place", "Decimal Place Rhythm", "Digits",
            "Exact minutes separated into tens and ones.",
            "Count hours, then firm pulses for the minute tens digit and short pulses for the ones digit.",
            listOf(PulseLegend("Hour pulse", "One hour", "—"), PulseLegend("Firm pulse", "One ten-minute group", "━"), PulseLegend("Short pulse", "One minute in the ones place", "·")),
            listOf(
                GuideExample("3:20", 3, 20, "Three hours, two tens, then a held zero marker.", "— — —  /  ━ ━  /  ▬"),
                GuideExample("3:42", 3, 42, "Three hours, four tens, then two ones.", "— — —  /  ━×4  /  · ·"),
            ),
            GuideQuiz(6, 34, listOf("6:24", "6:34", "6:43"), "6:34", "Six hour pulses, three firm tens, and four short ones means 6:34."),
        ),
        SystemGuide(
            "binary_clock", "Binary Clock", "Compact",
            "A fixed ten-pulse exact pattern for people who enjoy binary.",
            "Four bits describe the hour; six more describe the minute. Long means one and short means zero.",
            listOf(PulseLegend("Long firm pulse", "Binary one", "—"), PulseLegend("Short soft pulse", "Binary zero", "·"), PulseLegend("Long pause", "Hour/minute boundary", "/")),
            listOf(
                GuideExample("3:42", 3, 42, "Hour 0011; minute 101010.", "· · — —  /  — · — · — ·"),
                GuideExample("8:15", 8, 15, "Hour 1000; minute 001111.", "— · · ·  /  · · — — — —"),
            ),
            GuideQuiz(5, 10, listOf("5:10", "5:20", "10:05"), "5:10", "0101 is five; 001010 is ten."),
        ),
        SystemGuide(
            "nearest_quarter", "Nearest-Quarter Offset", "Clock-style",
            "A short pattern based on the nearest quarter-hour.",
            "Feel a familiar quarter-hour landmark, then whether the real time is before or after it and by how much.",
            listOf(PulseLegend("Landmark pulses", "One for :00, two for :15, three for :30, four for :45", "━"), PulseLegend("Long then short", "Before the landmark", "—·"), PulseLegend("Short then long", "After the landmark", "·—")),
            listOf(
                GuideExample("3:42", 3, 42, "Three minutes before 3:45.", "— — —  /  ━×4  /  —·  /  · · ·"),
                GuideExample("4:17", 4, 17, "Two minutes after 4:15.", "—×4  /  ━ ━  /  ·—  /  · ·"),
            ),
            GuideQuiz(8, 43, listOf("8:42", "8:43", "8:47"), "8:43", "The before marker and two ticks means two minutes before 8:45."),
        ),
        SystemGuide(
            "morse_digits", "Morse Digits", "Morse",
            "The displayed digits expressed with standard Morse numerals.",
            "Each digit is five marks. Short pulses are dots; long pulses are dashes.",
            listOf(PulseLegend("Short pulse", "Morse dot", "·"), PulseLegend("Long pulse", "Morse dash", "—"), PulseLegend("Pause", "Next digit", "/")),
            listOf(
                GuideExample("3:42", 3, 42, "3 is ...--; 4 is ....-; 2 is ..---.", "···——  /  ····—  /  ··———"),
                GuideExample("10:05", 10, 5, "1, 0, 0, then 5 as Morse numerals.", "·———— / —————  /  ————— / ·····"),
            ),
            GuideQuiz(2, 15, listOf("2:15", "5:12", "2:51"), "2:15", "The three digit groups are Morse numerals 2, 1, and 5."),
        ),
    )

    fun byId(id: String): SystemGuide? = all.firstOrNull { it.id == id }
}
