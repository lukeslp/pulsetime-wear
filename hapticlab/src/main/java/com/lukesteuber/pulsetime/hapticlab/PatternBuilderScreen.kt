package com.lukesteuber.pulsetime.hapticlab

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.semantics.LiveRegionMode
import androidx.compose.ui.semantics.liveRegion
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp

@Composable
internal fun PatternBuilderScreen(
    strength: Float,
    hasAmplitudeControl: Boolean,
    playPattern: (String, HapticPattern, Float) -> Unit,
    recordTrial: (String) -> Unit,
) = Page {
    val steps = remember { mutableStateListOf<HapticStep>() }
    var meaning by rememberSaveable { mutableStateOf("") }
    var amplitude by rememberSaveable { mutableIntStateOf(170) }
    var savedMessage by remember { mutableStateOf<String?>(null) }
    val pattern = runCatching { PersonalPatternBuilder.build(steps) }.getOrNull()

    fun add(step: HapticStep) {
        if (steps.size < PersonalPatternBuilder.MAX_STEPS) {
            steps += step
            savedMessage = null
        }
    }

    PageHeading("Build a personal pattern")
    Text(
        "Make a short signal for something you care about. Give each part one job, then test whether it still makes sense with the label hidden.",
        style = MaterialTheme.typography.bodyLarge,
    )

    OutlinedTextField(
        value = meaning,
        onValueChange = { meaning = it; savedMessage = null },
        label = { Text("What should this pattern mean?") },
        supportingText = { Text("Example: My ride has arrived") },
        modifier = Modifier.fillMaxWidth(),
        minLines = 2,
    )

    Section("Pulse strength") {
        if (!hasAmplitudeControl) Text("This phone cannot produce separate strength levels. The builder will preserve your choice for testing on other devices.")
        ChoiceColumn(
            listOf("Gentle" to 105, "Medium" to 170, "Firm" to 230),
            amplitude,
        ) { amplitude = it }
    }

    Section("Add a part") {
        Button(
            onClick = { add(PersonalPatternBuilder.short(amplitude)) },
            enabled = steps.size < PersonalPatternBuilder.MAX_STEPS,
            modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp),
        ) { Text("Add short pulse") }
        Button(
            onClick = { add(PersonalPatternBuilder.long(amplitude)) },
            enabled = steps.size < PersonalPatternBuilder.MAX_STEPS,
            modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp),
        ) { Text("Add long pulse") }
        OutlinedButton(
            onClick = { add(PersonalPatternBuilder.pause()) },
            enabled = steps.size < PersonalPatternBuilder.MAX_STEPS,
            modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp),
        ) { Text("Add field pause") }
        Text("${steps.size} of ${PersonalPatternBuilder.MAX_STEPS} parts used.")
    }

    Section("Pattern order") {
        if (steps.isEmpty()) Text("No parts yet.")
        steps.forEachIndexed { index, step ->
            PatternStepCard(
                number = index + 1,
                step = step,
                canMoveEarlier = index > 0,
                canMoveLater = index < steps.lastIndex,
                moveEarlier = {
                    val moved = steps.removeAt(index)
                    steps.add(index - 1, moved)
                },
                moveLater = {
                    val moved = steps.removeAt(index)
                    steps.add(index + 1, moved)
                },
                remove = { steps.removeAt(index) },
            )
        }
    }

    pattern?.let { built ->
        val transcript = built.steps.joinToString(" / ") { it.label }
        Section("Try it") {
            Text("${built.steps.size} parts. About ${(built.durationMs + 999) / 1_000} seconds.")
            Text(transcript, fontWeight = FontWeight.Bold)
            Button(
                onClick = { playPattern("personal-pattern", built, strength) },
                modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp),
            ) { Text("Play personal pattern") }
            OutlinedButton(
                onClick = { if (steps.isNotEmpty()) steps.removeAt(steps.lastIndex) },
                modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp),
            ) { Text("Undo last part") }
            OutlinedButton(
                onClick = { steps.clear(); savedMessage = null },
                modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp),
            ) { Text("Clear pattern") }
            Button(
                onClick = {
                    val name = meaning.trim().ifEmpty { "Unnamed personal pattern" }
                    recordTrial("Personal pattern | $name | $transcript | duration ${built.durationMs}ms | app strength ${(strength * 100).toInt()}%")
                    savedMessage = "Added to this session's feedback."
                },
                modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp),
            ) { Text("Add pattern to session feedback") }
            savedMessage?.let { Text(it, fontWeight = FontWeight.Bold, modifier = Modifier.semantics { liveRegion = LiveRegionMode.Polite }) }
        }
    }

    Text("Pulse Lab keeps this pattern only for the current session. Sharing feedback is always your choice.")
}

@Composable
private fun PatternStepCard(
    number: Int,
    step: HapticStep,
    canMoveEarlier: Boolean,
    canMoveLater: Boolean,
    moveEarlier: () -> Unit,
    moveLater: () -> Unit,
    remove: () -> Unit,
) {
    Card(colors = CardDefaults.cardColors(containerColor = Color.White)) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("Part $number: ${step.label}", style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Bold)
            if (step.onMs > 0) Text("${step.onMs} milliseconds, strength ${step.amplitude} of 255") else Text("${step.offMs} millisecond pause")
            OutlinedButton(moveEarlier, enabled = canMoveEarlier, modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) { Text("Move part $number earlier") }
            OutlinedButton(moveLater, enabled = canMoveLater, modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) { Text("Move part $number later") }
            OutlinedButton(remove, modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)) { Text("Remove part $number") }
        }
    }
}
