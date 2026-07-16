// File purpose: Blank activity that plays haptic time then exits (tile entry).
// Primary components: TellTimeRelayActivity.
// I/O: Intent launch → DataStore settings → haptic pattern → finish.
package com.lukesteuber.pulsetime.wear

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.lifecycle.lifecycleScope
import com.lukesteuber.pulsetime.wear.settings.PulseSettings
import com.lukesteuber.pulsetime.wear.settings.PulseSettingsRepository
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.launch

/**
 * Eyes-free entry from the Tile. No Compose chrome so TalkBack stays quiet
 * during normal silent use; vibrator is the only feedback.
 */
class TellTimeRelayActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val controller = TellTimeController(this)
        val repo = PulseSettingsRepository(this)
        lifecycleScope.launch {
            val settings = runCatching { repo.settings.first() }.getOrDefault(PulseSettings())
            controller.tellTime(settings)
            // Keep process alive briefly so the waveform can start, then leave.
            window.decorView.postDelayed({ finish() }, 150)
        }
    }
}
