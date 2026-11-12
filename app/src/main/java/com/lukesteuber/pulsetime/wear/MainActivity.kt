// File purpose: PulseTime Wear Compose shell — Tell time, training, calibration.
// Primary components: MainActivity, PulseTimeApp navigation screens.
// I/O: User actions → TellTimeController / PulseSettingsRepository.
package com.lukesteuber.pulsetime.wear

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.BackHandler
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextAlign
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.wear.compose.material3.AppScaffold
import androidx.wear.compose.material3.Button
import androidx.wear.compose.material3.MaterialTheme
import androidx.wear.compose.material3.Text
import androidx.wear.compose.material3.SwitchButton
import com.lukesteuber.pulsetime.wear.settings.PulseSettings
import com.lukesteuber.pulsetime.wear.settings.PulseSettingsRepository
import kotlinx.coroutines.launch

private enum class Screen {
    Home,
    Calibration,
    Training,
    TimeSystems,
}

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val repo = PulseSettingsRepository(applicationContext)
        val controller = TellTimeController(applicationContext)
        setContent {
            MaterialTheme {
                // No clock text on top: this app tells the time by touch.
                AppScaffold(timeText = {}) {
                    PulseTimeApp(repo = repo, controller = controller)
                }
            }
        }
    }
}

@Composable
fun PulseTimeApp(
    repo: PulseSettingsRepository,
    controller: TellTimeController,
) {
    val settings by repo.settings.collectAsStateWithLifecycle(initialValue = PulseSettings())
    var screen by remember { mutableStateOf(Screen.Home) }
    val scope = rememberCoroutineScope()

    // Back (the swipe from the left edge, or a hardware back button) returns
    // to Home from a sub-screen instead of closing the app.
    BackHandler(enabled = screen != Screen.Home) { screen = Screen.Home }

    when (screen) {
        Screen.Home -> HomeScreen(
            settings = settings,
            onTellTime = { controller.tellTime(settings) },
            onOpenCalibration = { screen = Screen.Calibration },
            onOpenTraining = { screen = Screen.Training },
            onOpenTimeSystems = { screen = Screen.TimeSystems },
            onTogglePrecision = { enabled ->
                scope.launch { repo.setPrecisionEnabled(enabled) }
            },
            onDismissFirstRun = {
                scope.launch { repo.setFirstRunComplete(true) }
            },
        )
        Screen.Calibration -> CalibrationScreen(
            settings = settings,
            controller = controller,
            onSaveScale = { scale ->
                scope.launch { repo.setAmplitudeScale(scale) }
            },
            onBack = { screen = Screen.Home },
        )
        Screen.Training -> TrainingScreen(
            settings = settings,
            controller = controller,
            onComplete = {
                scope.launch {
                    repo.setTrainingComplete(true)
                    screen = Screen.Home
                }
            },
            onBack = { screen = Screen.Home },
        )
        Screen.TimeSystems -> TimeSystemScreen(
            settings = settings,
            controller = controller,
            onSelectSystem = { system -> scope.launch { repo.setTimeSystem(system) } },
            onSetLayered = { enabled -> scope.launch { repo.setLayeredHaptics(enabled) } },
            onSetSpeed = { speed -> scope.launch { repo.setPlaybackSpeed(speed) } },
            onBack = { screen = Screen.Home },
        )
    }
}

@Composable
private fun HomeScreen(
    settings: PulseSettings,
    onTellTime: () -> Unit,
    onOpenCalibration: () -> Unit,
    onOpenTraining: () -> Unit,
    onOpenTimeSystems: () -> Unit,
    onTogglePrecision: (Boolean) -> Unit,
    onDismissFirstRun: () -> Unit,
) {
    WatchList {
        item {
            Text(
                text = stringResource(R.string.app_name),
                style = MaterialTheme.typography.titleMedium,
                textAlign = TextAlign.Center,
            )
        }
        if (!settings.firstRunComplete) {
            item {
                Text(
                    text = stringResource(R.string.add_tile_hint),
                    style = MaterialTheme.typography.bodySmall,
                    textAlign = TextAlign.Center,
                    modifier = Modifier.semantics {
                        contentDescription = "Hint: add the PulseTime tile for one-tap haptic time"
                    },
                )
            }
            item {
                Button(
                    onClick = onDismissFirstRun,
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    Text(stringResource(R.string.got_it))
                }
            }
        }
        item {
            Button(
                onClick = onTellTime,
                modifier = Modifier
                    .fillMaxWidth()
                    .semantics { contentDescription = "Tell time. Plays current time as haptics." },
            ) {
                Text(stringResource(R.string.tell_time))
            }
        }
        item {
            Button(
                onClick = onOpenTimeSystems,
                modifier = Modifier
                    .fillMaxWidth()
                    .semantics {
                        contentDescription = "Choose time system. Current: ${settings.timeSystem.title}"
                    },
            ) {
                Text("System: ${settings.timeSystem.shortLabel}")
            }
        }
        item {
            Button(
                onClick = onOpenCalibration,
                modifier = Modifier
                    .fillMaxWidth()
                    .semantics { contentDescription = "Open intensity calibration" },
            ) {
                Text(stringResource(R.string.calibration))
            }
        }
        item {
            Button(
                onClick = onOpenTraining,
                modifier = Modifier
                    .fillMaxWidth()
                    .semantics { contentDescription = "Open haptic training lessons" },
            ) {
                Text(stringResource(R.string.training))
            }
        }
        if (!settings.timeSystem.inherentlyExact) {
            item {
                val precisionEnabled = settings.precisionEnabled
                val precisionAllowed = settings.trainingComplete
                SwitchButton(
                    checked = precisionEnabled && precisionAllowed,
                    onCheckedChange = { checked -> if (precisionAllowed) onTogglePrecision(checked) },
                    enabled = precisionAllowed,
                    label = { Text(stringResource(R.string.precision)) },
                    secondaryLabel = {
                        // Three lines: the default two cut both sentences short.
                        Text(
                            if (precisionAllowed) stringResource(R.string.precision_desc) else stringResource(R.string.precision_locked),
                            maxLines = 3,
                        )
                    },
                    modifier = Modifier
                        .fillMaxWidth()
                        .semantics {
                            contentDescription = if (precisionAllowed) "Exact minutes toggle" else "Exact minutes locked until training is complete"
                        },
                )
            }
        }
        if (settings.trainingComplete) {
            item {
                Text(
                    text = stringResource(R.string.training_done),
                    style = MaterialTheme.typography.bodySmall,
                    textAlign = TextAlign.Center,
                )
            }
        }
    }
}
