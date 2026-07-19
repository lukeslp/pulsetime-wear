package com.lukesteuber.pulsetime.hapticlab

import android.os.SystemClock
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.material3.Button
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.LiveRegionMode
import androidx.compose.ui.semantics.liveRegion
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp

@Composable
internal fun QuickTestScreen(
    strength: Float,
    testContext: String,
    playPattern: (String, HapticPattern, Float) -> Unit,
    recordTrial: (String) -> Unit,
) = Page {
    var targetId by rememberSaveable { mutableStateOf<String?>(null) }
    var answer by rememberSaveable { mutableStateOf<String?>(null) }
    var confidence by rememberSaveable { mutableStateOf<String?>(null) }
    var replays by rememberSaveable { mutableIntStateOf(0) }
    var round by rememberSaveable { mutableIntStateOf(0) }
    var startedAt by remember { mutableStateOf<Long?>(null) }
    var answerElapsedMs by rememberSaveable { mutableStateOf<Long?>(null) }
    val target = targetId?.let(HapticDemoCatalog::byId)
    val choices = remember(targetId) {
        target?.let { selected ->
            (HapticDemoCatalog.all.filter { it.id != selected.id }.map { it.quizAnswer }.shuffled().take(3) + selected.quizAnswer).shuffled()
        }.orEmpty()
    }

    fun startRound() {
        val pool = HapticDemoCatalog.all.filter { it.id != targetId }
        val selected = pool.random()
        targetId = selected.id
        answer = null
        confidence = null
        replays = 1
        round += 1
        startedAt = SystemClock.elapsedRealtime()
        answerElapsedMs = null
        playPattern("quick-test-${selected.id}", selected.pattern, strength)
    }

    PageHeading("Mixed blind test")
    Text(
        "Test neighboring signals without choosing the category first. Each round draws from the whole fictional library. Replay as often as needed. Take all the time you need.",
        style = MaterialTheme.typography.bodyLarge,
    )

    if (target == null) {
        Text("Learn a few signals in the library before starting. Mixed trials work best when several meanings are already familiar.")
        Button(onClick = ::startRound, modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp)) { Text("Start first hidden signal") }
    } else {
        Text("Round $round", fontWeight = FontWeight.Bold)
        Button(
            onClick = {
                replays += 1
                playPattern("quick-test-${target.id}", target.pattern, strength)
            },
            modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp),
        ) { Text("Replay hidden signal") }

        Section("Choose the closest meaning") {
            choices.forEach { choice ->
                FilterChip(
                    selected = answer == choice,
                    onClick = {
                        if (answer == null) {
                            answer = choice
                            answerElapsedMs = startedAt?.let { SystemClock.elapsedRealtime() - it }
                        }
                    },
                    enabled = answer == null,
                    label = { Text(choice) },
                    modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp),
                )
            }
        }

        answer?.let { choice ->
            if (confidence == null) {
                Section("Confidence before the answer") {
                    listOf("Confident", "Unsure", "Guessing").forEach { level ->
                        OutlinedButton(
                            onClick = {
                                confidence = level
                                val result = if (choice == target.quizAnswer) "correct" else "incorrect"
                                recordTrial("Mixed signal | target ${target.title} | $result | chose $choice | answer ${target.quizAnswer} | confidence $level | replays $replays | total round ${answerElapsedMs ?: "unknown"}ms untimed | strength ${(strength * 100).toInt()}% | context $testContext")
                            },
                            modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp),
                        ) { Text(level) }
                    }
                }
            } else {
                val correct = choice == target.quizAnswer
                Text(
                    if (correct) "Correct. The signal was ${target.title}." else "The intended meaning was ${target.quizAnswer}. You chose $choice.",
                    fontWeight = FontWeight.Bold,
                    modifier = Modifier.semantics { liveRegion = LiveRegionMode.Polite },
                )
                Text(target.quizExplanation)
                Button(onClick = ::startRound, modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp)) { Text("Play another hidden signal") }
            }
        }
    }

    Text("The feedback report keeps each intended and chosen meaning. Repeated mix-ups reveal signal pairs that need redesign.")
}
