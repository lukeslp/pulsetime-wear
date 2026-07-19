// File purpose: Accessible selection and preview of PulseTime haptic time systems.
package com.lukesteuber.pulsetime.wear

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.wear.compose.material3.Button
import androidx.wear.compose.material3.MaterialTheme
import androidx.wear.compose.material3.SwitchButton
import androidx.wear.compose.material3.Text
import com.lukesteuber.pulsetime.shared.encoding.PlaybackSpeed
import com.lukesteuber.pulsetime.shared.encoding.TimeSystem
import com.lukesteuber.pulsetime.wear.settings.PulseSettings

@Composable
fun TimeSystemScreen(
    settings: PulseSettings,
    controller: TellTimeController,
    onSelectSystem: (TimeSystem) -> Unit,
    onSetLayered: (Boolean) -> Unit,
    onSetSpeed: (PlaybackSpeed) -> Unit,
    onBack: () -> Unit,
) {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(horizontal = 12.dp, vertical = 24.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        Text("Time systems", style = MaterialTheme.typography.titleMedium, textAlign = TextAlign.Center)
        Text(
            "Choose how PulseTime translates the clock into vibration.",
            style = MaterialTheme.typography.bodySmall,
            textAlign = TextAlign.Center,
        )
        TimeSystem.entries.forEach { system ->
            val selected = system == settings.timeSystem
            Button(
                onClick = { onSelectSystem(system) },
                modifier = Modifier
                    .fillMaxWidth()
                    .semantics {
                        contentDescription = "${system.title}, ${system.shortLabel}. ${system.description}. " +
                            if (selected) "Selected" else "Not selected"
                    },
            ) {
                Text((if (selected) "✓ " else "") + system.title, textAlign = TextAlign.Center)
            }
            if (selected) {
                Text(system.description, style = MaterialTheme.typography.bodySmall, textAlign = TextAlign.Center)
                Button(
                    onClick = { controller.playSample(3, 42, settings) },
                    modifier = Modifier
                        .fillMaxWidth()
                        .semantics { contentDescription = "Preview ${system.title} using 3:42" },
                ) {
                    Text("Preview 3:42")
                }
            }
        }
        SwitchButton(
            checked = settings.layeredHaptics,
            onCheckedChange = onSetLayered,
            label = { Text("Layered strength") },
            secondaryLabel = { Text("Use relative strength as well as rhythm") },
            modifier = Modifier.fillMaxWidth(),
        )
        Button(
            onClick = {
                val speeds = PlaybackSpeed.entries
                onSetSpeed(speeds[(speeds.indexOf(settings.playbackSpeed) + 1) % speeds.size])
            },
            modifier = Modifier
                .fillMaxWidth()
                .semantics { contentDescription = "Playback speed, ${settings.playbackSpeed.label}. Change speed" },
        ) {
            Text("Speed: ${settings.playbackSpeed.label}")
        }
        Button(onClick = onBack, modifier = Modifier.fillMaxWidth()) {
            Text("Back")
        }
    }
}
