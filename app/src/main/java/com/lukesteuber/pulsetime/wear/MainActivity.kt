// File purpose: PulseTime Wear Compose shell — Tell time, training, calibration.
// Primary components: MainActivity, PulseTimeApp navigation screens.
// I/O: User actions → TellTimeController / PulseSettingsRepository.
package com.lukesteuber.pulsetime.wear

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
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
                PulseTimeApp(repo = repo, controller = controller)
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
    val scroll = rememberScrollState()
    Column(
        modifier = Modifier
            .fillMaxSize()
            .verticalScroll(scroll)
            .padding(horizontal = 12.dp, vertical = 24.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        Text(
            text = stringResource(R.string.app_name),
            style = MaterialTheme.typography.titleMedium,
            textAlign = TextAlign.Center,
        )
        if (!settings.firstRunComplete) {
            Text(
                text = stringResource(R.string.add_tile_hint),
                style = MaterialTheme.typography.bodySmall,
                textAlign = TextAlign.Center,
                modifier = Modifier.semantics {
                    contentDescription = "Hint: add the PulseTime tile for one-tap haptic time"
                },
            )
            Button(
                onClick = onDismissFirstRun,
                modifier = Modifier.fillMaxWidth(),
            ) {
                Text(stringResource(R.string.got_it))
            }
        }
        Button(
            onClick = onTellTime,
            modifier = Modifier
                .fillMaxWidth()
                .semantics { contentDescription = "Tell time. Plays current time as haptics." },
        ) {
            Text(stringResource(R.string.tell_time))
        }
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
        Button(
            onClick = onOpenCalibration,
            modifier = Modifier
                .fillMaxWidth()
                .semantics { contentDescription = "Open intensity calibration" },
        ) {
            Text(stringResource(R.string.calibration))
        }
        Button(
            onClick = onOpenTraining,
            modifier = Modifier
                .fillMaxWidth()
                .semantics { contentDescription = "Open haptic training lessons" },
        ) {
            Text(stringResource(R.string.training))
        }
        if (!settings.timeSystem.inherentlyExact) {
            val precisionEnabled = settings.precisionEnabled
            val precisionAllowed = settings.trainingComplete
            SwitchButton(
                checked = precisionEnabled && precisionAllowed,
                onCheckedChange = { checked -> if (precisionAllowed) onTogglePrecision(checked) },
                enabled = precisionAllowed,
                label = { Text(stringResource(R.string.precision)) },
                secondaryLabel = {
                    Text(if (precisionAllowed) stringResource(R.string.precision_desc) else stringResource(R.string.precision_locked))
                },
                modifier = Modifier
                    .fillMaxWidth()
                    .semantics {
                        contentDescription = if (precisionAllowed) "Exact minutes toggle" else "Exact minutes locked until training is complete"
                    },
            )
        }
        if (settings.trainingComplete) {
            Text(
                text = stringResource(R.string.training_done),
                style = MaterialTheme.typography.bodySmall,
                textAlign = TextAlign.Center,
            )
        }
        Spacer(Modifier.height(16.dp))
    }
}
