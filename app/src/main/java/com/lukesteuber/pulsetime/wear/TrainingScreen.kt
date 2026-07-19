// File purpose: Training mode — five lessons plus a short quiz.
// Primary components: TrainingScreen, lesson catalog.
// I/O: Plays sample times; optional spoken labels for TalkBack learners.
package com.lukesteuber.pulsetime.wear

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.wear.compose.material3.Button
import androidx.wear.compose.material3.MaterialTheme
import androidx.wear.compose.material3.Text
import com.lukesteuber.pulsetime.wear.settings.PulseSettings
import com.lukesteuber.pulsetime.wear.encoding.TimeSystem

private data class Lesson(
    val title: String,
    val spoken: String,
    val hourOfDay: Int,
    val minute: Int,
)

private val LESSONS = listOf(
    Lesson("Hours only", "Three o'clock. Three hour pulses, then a pause, no quarters.", 3, 0),
    Lesson("One quarter", "Three fifteen. Three hour pulses, pause, one quarter pulse.", 3, 15),
    Lesson("Two quarters", "Three thirty. Three hour pulses, pause, two quarter pulses.", 3, 30),
    Lesson("Three quarters", "Three forty-five. Three hour pulses, pause, three quarter pulses.", 3, 45),
    Lesson("Noon", "Twelve o'clock. Twelve hour pulses, pause, no quarters.", 12, 0),
)

private data class QuizItem(
    val prompt: String,
    val hourOfDay: Int,
    val minute: Int,
    val correctLabel: String,
    val options: List<String>,
)

private val QUIZ = listOf(
    QuizItem(
        prompt = "Feel the pattern, then choose the time.",
        hourOfDay = 3,
        minute = 20,
        correctLabel = "3:15–3:29",
        options = listOf("3:00–3:14", "3:15–3:29", "3:30–3:44"),
    ),
    QuizItem(
        prompt = "Feel the pattern, then choose the time.",
        hourOfDay = 10,
        minute = 0,
        correctLabel = "10:00",
        options = listOf("10:00", "10:15", "2:00"),
    ),
)

@Composable
fun TrainingScreen(
    settings: PulseSettings,
    controller: TellTimeController,
    onComplete: () -> Unit,
    onBack: () -> Unit,
) {
    if (settings.timeSystem != TimeSystem.TerseQuarter) {
        AlternateSystemTraining(
            settings = settings,
            controller = controller,
            onComplete = onComplete,
            onBack = onBack,
        )
        return
    }
    var lessonIndex by remember { mutableIntStateOf(0) }
    var inQuiz by remember { mutableStateOf(false) }
    var quizIndex by remember { mutableIntStateOf(0) }
    var feedback by remember { mutableStateOf<String?>(null) }
    val scroll = rememberScrollState()
    val trainSettings = settings.copy(precisionEnabled = false)

    Column(
        modifier = Modifier
            .fillMaxSize()
            .verticalScroll(scroll)
            .padding(horizontal = 12.dp, vertical = 24.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        Text(
            text = stringResource(R.string.training),
            style = MaterialTheme.typography.titleMedium,
            textAlign = TextAlign.Center,
        )

        if (!inQuiz) {
            val lesson = LESSONS[lessonIndex]
            Text(
                text = stringResource(R.string.lesson_n, lessonIndex + 1, LESSONS.size),
                style = MaterialTheme.typography.bodyMedium,
                textAlign = TextAlign.Center,
            )
            Text(
                text = lesson.title,
                style = MaterialTheme.typography.titleSmall,
                textAlign = TextAlign.Center,
                modifier = Modifier.semantics { contentDescription = lesson.spoken },
            )
            Text(
                text = lesson.spoken,
                style = MaterialTheme.typography.bodySmall,
                textAlign = TextAlign.Center,
            )
            Button(
                onClick = {
                    controller.playSample(lesson.hourOfDay, lesson.minute, trainSettings)
                },
                modifier = Modifier
                    .fillMaxWidth()
                    .semantics { contentDescription = "Play lesson sample. ${lesson.spoken}" },
            ) {
                Text(stringResource(R.string.play_sample))
            }
            Button(
                onClick = {
                    if (lessonIndex < LESSONS.lastIndex) {
                        lessonIndex++
                    } else {
                        inQuiz = true
                        quizIndex = 0
                        feedback = null
                    }
                },
                modifier = Modifier
                    .fillMaxWidth()
                    .semantics { contentDescription = "Next lesson" },
            ) {
                Text(
                    if (lessonIndex < LESSONS.lastIndex) {
                        stringResource(R.string.next)
                    } else {
                        stringResource(R.string.start_quiz)
                    },
                )
            }
        } else {
            val item = QUIZ[quizIndex]
            Text(
                text = stringResource(R.string.quiz_n, quizIndex + 1, QUIZ.size),
                style = MaterialTheme.typography.bodyMedium,
                textAlign = TextAlign.Center,
            )
            Text(
                text = item.prompt,
                style = MaterialTheme.typography.bodySmall,
                textAlign = TextAlign.Center,
            )
            Button(
                onClick = {
                    feedback = null
                    controller.playSample(item.hourOfDay, item.minute, trainSettings)
                },
                modifier = Modifier
                    .fillMaxWidth()
                    .semantics { contentDescription = "Play quiz pattern" },
            ) {
                Text(stringResource(R.string.play_sample))
            }
            item.options.forEach { option ->
                Button(
                    onClick = {
                        if (option == item.correctLabel) {
                            if (quizIndex < QUIZ.lastIndex) {
                                feedback = "Correct"
                                quizIndex++
                            } else {
                                onComplete()
                            }
                        } else {
                            feedback = "Try again"
                        }
                    },
                    modifier = Modifier
                        .fillMaxWidth()
                        .semantics { contentDescription = "Answer $option" },
                ) {
                    Text(option)
                }
            }
            feedback?.let {
                Text(
                    text = it,
                    style = MaterialTheme.typography.bodyMedium,
                    textAlign = TextAlign.Center,
                    modifier = Modifier.semantics { contentDescription = it },
                )
            }
        }

        Button(
            onClick = onBack,
            modifier = Modifier
                .fillMaxWidth()
                .semantics { contentDescription = "Back to home" },
        ) {
            Text(stringResource(R.string.back))
        }
    }
}

@Composable
private fun AlternateSystemTraining(
    settings: PulseSettings,
    controller: TellTimeController,
    onComplete: () -> Unit,
    onBack: () -> Unit,
) {
    val samples = listOf(
        Triple("Three o'clock", 3, 0),
        Triple("Three forty-two", 3, 42),
        Triple("Eleven fifty-nine", 11, 59),
    )
    Column(
        modifier = Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(horizontal = 12.dp, vertical = 24.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        Text(settings.timeSystem.title, style = MaterialTheme.typography.titleMedium, textAlign = TextAlign.Center)
        Text(settings.timeSystem.description, style = MaterialTheme.typography.bodySmall, textAlign = TextAlign.Center)
        Text(
            when (settings.timeSystem) {
                TimeSystem.FiveMinuteLadder -> "Long pulses are hours, medium pulses are five-minute steps, and short pulses are leftover minutes."
                TimeSystem.DecimalPlace -> "Hours come first. Firm pulses count minute tens; short pulses count minute ones. A held pulse means zero."
                TimeSystem.BinaryClock -> "Four hour bits come first, then six minute bits. A long firm pulse is one; a short soft pulse is zero."
                TimeSystem.NearestQuarter -> "Feel the nearest quarter-hour, then a direction pair and the number of minutes before or after it."
                TimeSystem.MorseDigits -> "Each displayed digit uses standard Morse numerals. Long pulses are dashes and short pulses are dots."
                TimeSystem.TerseQuarter -> ""
            },
            style = MaterialTheme.typography.bodySmall,
            textAlign = TextAlign.Center,
        )
        samples.forEach { (label, hour, minute) ->
            Button(
                onClick = { controller.playSample(hour, minute, settings) },
                modifier = Modifier
                    .fillMaxWidth()
                    .semantics { contentDescription = "Play $label using ${settings.timeSystem.title}" },
            ) {
                Text("Play $label")
            }
        }
        Button(onClick = onComplete, modifier = Modifier.fillMaxWidth()) {
            Text("Finish training")
        }
        Button(onClick = onBack, modifier = Modifier.fillMaxWidth()) {
            Text(stringResource(R.string.back))
        }
    }
}
