package com.lukesteuber.pulsetime.hapticlab

import android.os.Build
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
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp

@Composable
internal fun PerceptionLabScreen(
    player: PhoneHapticPlayer,
    strength: Float,
    testContext: String,
    onContextChange: (String) -> Unit,
    playPattern: (String, HapticPattern, Float) -> Unit,
    playClearCue: (ClearCue) -> Unit,
    recordTrial: (String) -> Unit,
) = Page {
    val capabilities = player.capabilities
    var comfortAnswer by rememberSaveable { mutableStateOf("") }
    PageHeading("Perception lab")
    Text(
        "Find comfortable settings, then compare the building blocks used throughout Pulse Lab. There is no perfect setting. Choose the gentlest pulse you can feel comfortably.",
        style = MaterialTheme.typography.bodyLarge,
    )

    Section("This phone") {
        Card(colors = CardDefaults.cardColors(containerColor = Color.White)) {
            Column(Modifier.padding(18.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("${Build.MANUFACTURER} ${Build.MODEL}", fontWeight = FontWeight.Bold)
                CapabilityLine("Vibrator", capabilities.hasVibrator)
                CapabilityLine("Different strength levels", capabilities.hasAmplitudeControl)
                CapabilityLine("Clear built-in effects", capabilities.supportsClearEffects)
                CapabilityLine("Composable haptic primitives", capabilities.supportsPrimitives)
                CapabilityLine("Android 16 envelope effects", capabilities.supportsEnvelopes)
            }
        }
        if (!capabilities.hasAmplitudeControl) {
            Text("This phone turns every nonzero strength into the same motor level. Use rhythm, duration, and pauses when judging patterns.", fontWeight = FontWeight.Bold)
        }
    }

    Section("Where are you testing?") {
        Text("Keep the phone in the same place while comparing two patterns.")
        ChoiceColumn(
            listOf(
                "Holding the phone while still" to "Holding the phone while still",
                "Holding the phone while walking" to "Holding the phone while walking",
                "Phone in a pocket" to "Phone in a pocket",
                "Phone on a surface" to "Phone on a surface",
            ),
            testContext,
            onContextChange,
        )
    }

    Section("Comfort check") {
        Text("Play each sample once or twice. Stop if any level feels unpleasant. Overall app strength is ${(strength * 100).toInt()} percent.")
        listOf(
            Triple("Gentle sample", 90, "gentle"),
            Triple("Medium sample", 160, "medium"),
            Triple("Firm sample", 230, "firm"),
        ).forEach { (label, amplitude, id) ->
            OutlinedButton(
                onClick = { playPattern("comfort-$id", HapticPattern(listOf(pulse(label, 90, 0, amplitude))), strength) },
                modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp),
            ) { Text("Play $label") }
        }
        Text("Which is the gentlest level you can feel clearly?", fontWeight = FontWeight.Bold)
        ChoiceColumn(
            listOf("Gentle" to "Gentle", "Medium" to "Medium", "Firm" to "Firm", "None clearly" to "None clearly"),
            comfortAnswer,
        ) { answer ->
            comfortAnswer = answer
            recordTrial("Calibration | minimum clear level $answer | context $testContext | app strength ${(strength * 100).toInt()}% | amplitude control ${capabilities.hasAmplitudeControl}")
        }
    }

    if (capabilities.supportsClearEffects) {
        Section("Built-in clear cues") {
            Text("Android tunes these short cues for this phone. Compare them with the longer waveform samples above.")
            ClearCue.entries.forEach { cue ->
                OutlinedButton(
                    onClick = { playClearCue(cue) },
                    modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp),
                ) { Text("Play ${cue.name.lowercase()} built-in cue") }
            }
        }
    }

    SignalComparisonCatalog.all.forEach { comparison ->
        ComparisonCard(comparison, strength, testContext, playPattern, recordTrial)
    }

    Section("What these results mean") {
        Text("These comparisons record preference and clarity for this session. They do not prove that one pattern works for everyone. Repeat useful comparisons on another day and on the watch when that build is available.")
    }
}

@Composable
private fun CapabilityLine(label: String, supported: Boolean) {
    Text("$label: ${if (supported) "supported" else "not reported"}")
}

@Composable
private fun ComparisonCard(
    comparison: SignalComparison,
    strength: Float,
    testContext: String,
    playPattern: (String, HapticPattern, Float) -> Unit,
    recordTrial: (String) -> Unit,
) {
    var vote by rememberSaveable(comparison.id) { mutableStateOf<String?>(null) }
    Card(colors = CardDefaults.cardColors(containerColor = Color.White)) {
        Column(Modifier.padding(18.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
            Text(comparison.title, style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.Bold)
            Text(comparison.question)
            Button(
                onClick = { playPattern("${comparison.id}-a", comparison.first, strength) },
                modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp),
            ) { Text("Play A: ${comparison.firstLabel}") }
            Button(
                onClick = { playPattern("${comparison.id}-b", comparison.second, strength) },
                modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp),
            ) { Text("Play B: ${comparison.secondLabel}") }
            Text("Your comparison", fontWeight = FontWeight.Bold)
            listOf("A was clearer", "B was clearer", "They felt equally clear", "I could not distinguish them").forEach { choice ->
                OutlinedButton(
                    onClick = {
                        vote = choice
                        recordTrial("Comparison | ${comparison.title} | $choice | context $testContext | strength ${(strength * 100).toInt()}%")
                    },
                    enabled = vote == null,
                    modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp),
                ) { Text(choice) }
            }
            vote?.let {
                Text("Recorded: $it", fontWeight = FontWeight.Bold)
                OutlinedButton(onClick = { vote = null }, modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp)) {
                    Text("Compare again")
                }
            }
        }
    }
}
