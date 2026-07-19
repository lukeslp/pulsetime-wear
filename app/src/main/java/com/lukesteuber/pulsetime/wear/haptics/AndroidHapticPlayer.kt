// File purpose: Plays encoded haptic patterns on Wear via VibratorManager.
// Primary components: HapticPlayer interface, AndroidHapticPlayer.
// I/O: EncodedPattern → VibrationEffect waveform; cancel stops playback.
package com.lukesteuber.pulsetime.wear.haptics

import android.content.Context
import android.os.Build
import android.os.VibrationEffect
import android.os.Vibrator
import android.os.VibratorManager
import com.lukesteuber.pulsetime.shared.encoding.EncodedPattern

interface HapticPlayer {
    fun play(pattern: EncodedPattern)
    fun cancel()
    fun isPlaying(): Boolean
}

class AndroidHapticPlayer(context: Context) : HapticPlayer {
    private val vibrator: Vibrator = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
        val mgr = context.getSystemService(VibratorManager::class.java)
        mgr.defaultVibrator
    } else {
        @Suppress("DEPRECATION")
        context.getSystemService(Vibrator::class.java)
    }

    @Volatile
    private var playUntilElapsedRealtime = 0L

    override fun play(pattern: EncodedPattern) {
        cancel()
        val timings = pattern.toTimings()
        val amplitudes = pattern.toAmplitudes()
        if (timings.size < 2) return

        val effect = if (vibrator.hasAmplitudeControl()) {
            VibrationEffect.createWaveform(timings, amplitudes, -1)
        } else {
            VibrationEffect.createWaveform(timings, -1)
        }
        val duration = timings.sum()
        playUntilElapsedRealtime = android.os.SystemClock.elapsedRealtime() + duration
        vibrator.vibrate(effect)
    }

    override fun cancel() {
        vibrator.cancel()
        playUntilElapsedRealtime = 0L
    }

    override fun isPlaying(): Boolean =
        playUntilElapsedRealtime > android.os.SystemClock.elapsedRealtime()
}
