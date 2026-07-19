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
        require(onMs + offMs > 0)
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

data class SignalGrammar(
    val topic: String,
    val payload: String,
    val modifier: String? = null,
    val action: String? = null,
) {
    init {
        require(topic.isNotBlank())
        require(payload.isNotBlank())
    }

    val transcript: String
        get() = listOfNotNull(topic, payload, modifier, action).joinToString(" / ")
}

data class HapticDemo(
    val id: String,
    val category: String,
    val title: String,
    val summary: String,
    val meaning: String,
    val grammar: SignalGrammar,
    val pattern: HapticPattern,
    val quizQuestion: String,
    val quizChoices: List<String>,
    val quizAnswer: String,
    val quizExplanation: String,
    val headlinePattern: HapticPattern? = null,
    val safetyNote: String? = null,
    val acknowledgementRequested: Boolean = false,
) {
    val transcript: String get() = grammar.transcript
    val durationLabel: String get() = "About ${(pattern.durationMs + 999) / 1_000} seconds"
}

data class SignalComparison(
    val id: String,
    val title: String,
    val question: String,
    val firstLabel: String,
    val first: HapticPattern,
    val secondLabel: String,
    val second: HapticPattern,
)

internal fun pulse(label: String, on: Long, off: Long = 120, amplitude: Int = 170) =
    HapticStep(label, on, off, amplitude)

internal fun gap(label: String = "Field pause", ms: Long = 440) = HapticStep(label, 0, ms, 1)

object SignalComparisonCatalog {
    val all = listOf(
        SignalComparison(
            id = "direction_order",
            title = "Direction by order",
            question = "Which pair makes up versus down easier to remember?",
            firstLabel = "Short then long",
            first = HapticPattern(listOf(pulse("Short", 75, 110, 150), pulse("Long", 230, 0, 210))),
            secondLabel = "Long then short",
            second = HapticPattern(listOf(pulse("Long", 230, 110, 210), pulse("Short", 75, 0, 150))),
        ),
        SignalComparison(
            id = "urgency_tempo",
            title = "Urgency by tempo",
            question = "Which ending feels more urgent without becoming uncomfortable?",
            firstLabel = "Even beats",
            first = HapticPattern(listOf(
                pulse("One", 85, 150, 180), pulse("Two", 85, 150, 180), pulse("Three", 85, 0, 180),
            )),
            secondLabel = "Accelerating beats",
            second = HapticPattern(listOf(
                pulse("One", 70, 180, 145), pulse("Two", 90, 105, 180), pulse("Three", 120, 0, 220),
            )),
        ),
        SignalComparison(
            id = "field_boundary",
            title = "Field boundary",
            question = "Which separator makes two pieces of information easier to split?",
            firstLabel = "One long pause",
            first = HapticPattern(listOf(pulse("Topic", 100, 620, 180), pulse("Value", 100, 0, 180))),
            secondLabel = "Two punctuation beats",
            second = HapticPattern(listOf(
                pulse("Topic", 100, 220, 180), pulse("Boundary", 45, 70, 110), pulse("Boundary", 45, 220, 110), pulse("Value", 100, 0, 180),
            )),
        ),
    )
}

object PersonalPatternBuilder {
    const val MAX_STEPS = 12

    fun short(amplitude: Int) = pulse("Short pulse", 75, 120, amplitude)
    fun long(amplitude: Int) = pulse("Long pulse", 230, 120, amplitude)
    fun pause() = gap("Pause", 420)

    fun build(steps: List<HapticStep>): HapticPattern? = if (steps.isEmpty()) null else HapticPattern(steps)
}
