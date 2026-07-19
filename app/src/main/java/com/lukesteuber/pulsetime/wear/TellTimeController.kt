// File purpose: Shared tell-time action used by UI and Tile.
// Primary components: TellTimeController.
// I/O: Reads settings + clock → plays or cancels haptic pattern.
package com.lukesteuber.pulsetime.wear

import android.content.Context
import com.lukesteuber.pulsetime.wear.encoding.EncodingOptions
import com.lukesteuber.pulsetime.wear.encoding.TimeEncoderRegistry
import com.lukesteuber.pulsetime.wear.haptics.HapticPlayer
import com.lukesteuber.pulsetime.wear.haptics.HapticPlayerHolder
import com.lukesteuber.pulsetime.wear.settings.PulseSettings
import java.util.Calendar

class TellTimeController(
    context: Context,
    private val player: HapticPlayer = HapticPlayerHolder.get(context),
) {
    fun tellTime(settings: PulseSettings, calendar: Calendar = Calendar.getInstance()) {
        if (player.isPlaying()) {
            player.cancel()
            return
        }
        val pattern = TimeEncoderRegistry.encodeNow(
            system = settings.timeSystem,
            calendar = calendar,
            options = settings.encodingOptions(),
        )
        player.play(pattern)
    }

    fun playSample(hourOfDay: Int, minute: Int, settings: PulseSettings) {
        player.cancel()
        val pattern = TimeEncoderRegistry.encode(
            system = settings.timeSystem,
            hourOfDay = hourOfDay,
            minute = minute,
            options = settings.encodingOptions(),
        )
        player.play(pattern)
    }

    fun cancel() = player.cancel()
}

private fun PulseSettings.encodingOptions() = EncodingOptions(
    amplitudeScale = amplitudeScale,
    layeredHaptics = layeredHaptics,
    playbackSpeed = playbackSpeed,
    includePrecision = precisionEnabled,
)
