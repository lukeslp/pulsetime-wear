// File purpose: Accessible selection and preview of PulseTime haptic time systems.
package com.lukesteuber.pulsetime.wear

import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextAlign
import androidx.wear.compose.material3.Button
import androidx.wear.compose.material3.MaterialTheme
import androidx.wear.compose.material3.SwitchButton
import androidx.wear.compose.material3.Text
import com.lukesteuber.pulsetime.shared.encoding.PatternDescriber
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
    WatchList(initialCenterItemIndex = 2) {
        item {
            Text("Time systems", style = MaterialTheme.typography.titleMedium, textAlign = TextAlign.Center)
        }
        item {
            Text(
                "Choose how PulseTime translates the clock into vibration.",
                style = MaterialTheme.typography.bodySmall,
                textAlign = TextAlign.Center,
            )
        }
        TimeSystem.entries.forEach { system ->
            val selected = system == settings.timeSystem
            item(key = system.storageId) {
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
            }
            if (selected) {
                item(key = "description") {
                    Text(system.description, style = MaterialTheme.typography.bodySmall, textAlign = TextAlign.Center)
                }
                item(key = "preview") {
                    Button(
                        onClick = { controller.playSample(3, 42, settings) },
                        modifier = Modifier
                            .fillMaxWidth()
                            .semantics {
                                contentDescription = "Preview ${system.title} using 3:42. " +
                                    PatternDescriber.describe(system, 3, 42, settings.precisionEnabled)
                            },
                    ) {
                        Text("Preview 3:42")
                    }
                }
            }
        }
        item {
            SwitchButton(
                checked = settings.layeredHaptics,
                onCheckedChange = onSetLayered,
                label = { Text("Layered strength") },
                secondaryLabel = { Text("Use relative strength as well as rhythm", maxLines = 3) },
                modifier = Modifier.fillMaxWidth(),
            )
        }
        item {
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
        }
        item {
            Button(onClick = onBack, modifier = Modifier.fillMaxWidth()) {
                Text("Back")
            }
        }
    }
}
