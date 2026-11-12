// File purpose: Five-step intensity calibration for OEM vibrator variance.
// Primary components: CalibrationScreen.
// I/O: Sample pulses at rising scales → persists amplitudeScale.
package com.lukesteuber.pulsetime.wear

import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextAlign
import androidx.wear.compose.material3.Button
import androidx.wear.compose.material3.MaterialTheme
import androidx.wear.compose.material3.Text
import com.lukesteuber.pulsetime.wear.settings.PulseSettings

private val CALIBRATION_STEPS = listOf(0.3f, 0.45f, 0.6f, 0.8f, 1.0f)

@Composable
fun CalibrationScreen(
    settings: PulseSettings,
    controller: TellTimeController,
    onSaveScale: (Float) -> Unit,
    onBack: () -> Unit,
) {
    var step by remember { mutableIntStateOf(0) }
    val scale = CALIBRATION_STEPS[step.coerceIn(0, CALIBRATION_STEPS.lastIndex)]

    WatchList {
        item {
            Text(
                text = stringResource(R.string.calibration),
                style = MaterialTheme.typography.titleMedium,
                textAlign = TextAlign.Center,
            )
        }
        item {
            Text(
                text = stringResource(R.string.calibration_step, step + 1, CALIBRATION_STEPS.size),
                style = MaterialTheme.typography.bodyMedium,
                textAlign = TextAlign.Center,
                modifier = Modifier.semantics {
                    contentDescription = "Calibration step ${step + 1} of ${CALIBRATION_STEPS.size}"
                },
            )
        }
        item {
            Text(
                text = stringResource(R.string.calibration_hint),
                style = MaterialTheme.typography.bodySmall,
                textAlign = TextAlign.Center,
            )
        }
        item {
            Button(
                onClick = {
                    val sampleSettings = settings.copy(amplitudeScale = scale, precisionEnabled = false)
                    controller.playSample(hourOfDay = 3, minute = 0, settings = sampleSettings)
                },
                modifier = Modifier
                    .fillMaxWidth()
                    .semantics { contentDescription = "Play sample at this intensity" },
            ) {
                Text(stringResource(R.string.play_sample))
            }
        }
        item {
            Button(
                onClick = {
                    onSaveScale(scale)
                    if (step < CALIBRATION_STEPS.lastIndex) {
                        step++
                    } else {
                        onBack()
                    }
                },
                modifier = Modifier
                    .fillMaxWidth()
                    .semantics {
                        contentDescription = if (step < CALIBRATION_STEPS.lastIndex) {
                            "Use this intensity and go to next step"
                        } else {
                            "Save this intensity and finish calibration"
                        }
                    },
            ) {
                Text(
                    if (step < CALIBRATION_STEPS.lastIndex) {
                        stringResource(R.string.next)
                    } else {
                        stringResource(R.string.finish)
                    },
                )
            }
        }
        item {
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
}
