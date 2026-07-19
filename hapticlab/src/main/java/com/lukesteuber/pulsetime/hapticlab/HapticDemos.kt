package com.lukesteuber.pulsetime.hapticlab

data class HapticStep(
    val label: String,
    val onMs: Long,
    val offMs: Long,
    val amplitude: Int,
) {
    init {
        require(label.isNotBlank())
        require(onMs >= 0 && offMs >= 0)
        require(amplitude in 1..255)
    }
}

data class HapticPattern(val steps: List<HapticStep>) {
    init {
        require(steps.isNotEmpty())
        require(durationMs <= 8_000L)
    }

    val durationMs: Long get() = steps.sumOf { it.onMs + it.offMs }

    fun waveform(speed: Float = 1f, strength: Float = 1f): Pair<LongArray, IntArray> {
        require(speed in .5f..2f)
        require(strength in .25f..1.25f)
        val timings = arrayListOf(0L)
        val amplitudes = arrayListOf(0)

        fun append(duration: Long, amplitude: Int) {
            if (duration <= 0) return
            val scaledDuration = (duration * speed).toLong().coerceAtLeast(1)
            val scaledAmplitude = if (amplitude == 0) 0 else (amplitude * strength).toInt().coerceIn(1, 255)
            val sameState = (amplitudes.last() == 0) == (scaledAmplitude == 0)
            if (sameState) {
                timings[timings.lastIndex] += scaledDuration
                if (scaledAmplitude > 0) amplitudes[amplitudes.lastIndex] = maxOf(amplitudes.last(), scaledAmplitude)
            } else {
                timings += scaledDuration
                amplitudes += scaledAmplitude
            }
        }

        steps.forEach { step ->
            append(step.onMs, step.amplitude)
            append(step.offMs, 0)
        }
        return timings.toLongArray() to amplitudes.toIntArray()
    }
}

data class HapticDemo(
    val id: String,
    val category: String,
    val title: String,
    val summary: String,
    val meaning: String,
    val transcript: String,
    val pattern: HapticPattern,
    val quizQuestion: String,
    val quizChoices: List<String>,
    val quizAnswer: String,
    val quizExplanation: String,
    val acknowledgementRequested: Boolean = false,
) {
    val durationLabel: String
        get() = "About ${(pattern.durationMs + 999) / 1_000} seconds"
}

private fun pulse(label: String, on: Long, off: Long = 120, amplitude: Int = 170) =
    HapticStep(label, on, off, amplitude)

private fun gap(label: String = "Field pause", ms: Long = 440) = HapticStep(label, 0, ms, 1)

object HapticDemoCatalog {
    val all = listOf(
        HapticDemo(
            id = "market_now",
            category = "Markets",
            title = "Market right now",
            summary = "Direction and size of a market move.",
            meaning = "The market is sharply down right now.",
            transcript = "Market / down / large move",
            pattern = HapticPattern(listOf(
                pulse("Market: short", 70, 90, 100), pulse("Market: short", 70, 90, 100), pulse("Market: long", 220, 0, 200), gap(),
                pulse("Down: long", 220, 100, 220), pulse("Down: short", 75, 0, 110), gap(),
                pulse("Large: one", 105, 100, 210), pulse("Large: two", 105, 100, 210), pulse("Large: three", 105, 0, 210),
            )),
            quizQuestion = "What did the market do?",
            quizChoices = listOf("Rose slightly", "Fell sharply", "Stayed flat"),
            quizAnswer = "Fell sharply",
            quizExplanation = "Long then short meant down. Three closing beats meant a large move.",
        ),
        HapticDemo(
            id = "trading_day",
            category = "Markets",
            title = "Shape of the day",
            summary = "Four samples form a tactile market story.",
            meaning = "The market opened low, climbed, peaked, then eased before the close.",
            transcript = "Open / low / rising / high / easing / close",
            pattern = HapticPattern(listOf(
                pulse("Market: short", 70, 90, 100), pulse("Market: short", 70, 90, 100), pulse("Market: long", 220, 0, 200), gap(),
                pulse("Opening marker", 260, 0, 210), gap(ms = 340),
                pulse("Open low", 75, 220, 90), pulse("Midday higher", 130, 220, 145),
                pulse("Afternoon high", 220, 220, 220), pulse("Close eased", 145, 0, 160), gap(ms = 300),
                pulse("Closing marker", 260, 0, 210),
            )),
            quizQuestion = "What was the broad shape?",
            quizChoices = listOf("Climbed, then eased", "Fell all day", "Stayed level"),
            quizAnswer = "Climbed, then eased",
            quizExplanation = "The four samples grew from short to long, then shortened slightly at the close.",
        ),
        HapticDemo(
            id = "rain_incoming",
            category = "Weather",
            title = "Rain approaching",
            summary = "A countdown followed by an irregular rain pattern.",
            meaning = "Rain may begin in about 20 minutes.",
            transcript = "Weather / twenty minutes / rain",
            pattern = HapticPattern(listOf(
                pulse("Weather: long", 210, 90, 190), pulse("Weather: short", 70, 90, 100), pulse("Weather: long", 210, 0, 190), gap(),
                pulse("Twenty: one", 110, 130, 160), pulse("Twenty: two", 110, 0, 160), gap(ms = 320),
                pulse("Rain", 45, 80, 105), pulse("Rain", 65, 55, 125), pulse("Rain", 40, 95, 95), pulse("Rain", 75, 0, 135),
            )),
            quizQuestion = "How soon might rain begin?",
            quizChoices = listOf("About 10 minutes", "About 20 minutes", "About 30 minutes"),
            quizAnswer = "About 20 minutes",
            quizExplanation = "Two countdown beats meant twenty minutes. The irregular ending was rain.",
        ),
        HapticDemo(
            id = "thunderstorm",
            category = "Weather",
            title = "Thunderstorm nearby",
            summary = "A memorable condition marker with urgency and certainty.",
            meaning = "A nearby thunderstorm is likely. This is a fictional demonstration, not a safety alert.",
            transcript = "Thunderstorm / nearby / likely",
            pattern = HapticPattern(listOf(
                pulse("Thunder strike", 90, 80, 240), pulse("Thunder strike", 90, 80, 240), pulse("Thunder roll", 330, 0, 200), gap(),
                pulse("Nearby: one", 100, 80, 210), pulse("Nearby: two", 100, 0, 210), gap(ms = 300),
                pulse("Likely: one", 75, 90, 130), pulse("Likely: two", 75, 90, 130), pulse("Likely: three", 75, 0, 130),
            )),
            quizQuestion = "What did the pattern communicate?",
            quizChoices = listOf("Light rain later", "Nearby thunderstorm likely", "Strong wind ending"),
            quizAnswer = "Nearby thunderstorm likely",
            quizExplanation = "Two sharp strikes and a long roll marked thunder. Two beats meant nearby; three soft beats meant likely.",
        ),
        HapticDemo(
            id = "important_person",
            category = "People and messages",
            title = "Important person",
            summary = "A learned sender signature followed by urgency.",
            meaning = "A family member sent a message marked urgent.",
            transcript = "Message / family / urgent",
            pattern = HapticPattern(listOf(
                pulse("Message: long", 220, 90, 200), pulse("Message: short", 70, 90, 105), pulse("Message: short", 70, 0, 105), gap(),
                pulse("Family: short", 70, 80, 105), pulse("Family: short", 70, 80, 105), pulse("Family: long", 220, 0, 200), gap(ms = 320),
                pulse("Urgent: one", 70, 110, 150), pulse("Urgent: two", 90, 75, 185), pulse("Urgent: three", 120, 0, 220),
            )),
            quizQuestion = "Who sent the message, and how was it marked?",
            quizChoices = listOf("Family, urgent", "Work, normal", "Unknown, important"),
            quizAnswer = "Family, urgent",
            quizExplanation = "Short-short-long was the learned family signature. Three accelerating beats meant urgent.",
        ),
        HapticDemo(
            id = "acknowledgement",
            category = "People and messages",
            title = "Acknowledgement requested",
            summary = "An action tail asks for a visible response.",
            meaning = "This message asks you to acknowledge it.",
            transcript = "Message / important / acknowledgement requested",
            pattern = HapticPattern(listOf(
                pulse("Message: long", 220, 90, 200), pulse("Message: short", 70, 90, 105), pulse("Message: short", 70, 0, 105), gap(),
                pulse("Important: one", 120, 120, 180), pulse("Important: two", 120, 0, 180), gap(ms = 320),
                pulse("Question: short", 70, 90, 110), pulse("Question: long", 220, 90, 220), pulse("Question: short", 70, 0, 110),
            )),
            quizQuestion = "What did the final short-long-short cadence request?",
            quizChoices = listOf("Acknowledge the message", "Delete the message", "No action"),
            quizAnswer = "Acknowledge the message",
            quizExplanation = "The short-long-short action tail always means acknowledgement requested.",
            acknowledgementRequested = true,
        ),
    )

    fun byId(id: String): HapticDemo? = all.firstOrNull { it.id == id }
}
