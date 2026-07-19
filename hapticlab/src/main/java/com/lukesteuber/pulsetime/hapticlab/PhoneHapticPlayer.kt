package com.lukesteuber.pulsetime.hapticlab

import android.content.Context
import android.os.Build
import android.os.VibrationEffect
import android.os.Vibrator
import android.os.VibratorManager

class PhoneHapticPlayer(context: Context) {
    private val vibrator: Vibrator = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
        context.getSystemService(VibratorManager::class.java).defaultVibrator
    } else {
        @Suppress("DEPRECATION")
        context.getSystemService(Vibrator::class.java)
    }

    val isAvailable: Boolean get() = vibrator.hasVibrator()
    val hasAmplitudeControl: Boolean get() = vibrator.hasAmplitudeControl()

    fun play(pattern: HapticPattern, speed: Float = 1f, strength: Float = .8f): Boolean {
        val (timings, amplitudes) = pattern.waveform(speed, strength)
        return play(timings, amplitudes)
    }

    fun play(timings: LongArray, amplitudes: IntArray): Boolean {
        cancel()
        if (!isAvailable) return false
        val effect = if (hasAmplitudeControl) {
            VibrationEffect.createWaveform(timings, amplitudes, -1)
        } else {
            VibrationEffect.createWaveform(timings, -1)
        }
        vibrator.vibrate(effect)
        return true
    }

    fun cancel() = vibrator.cancel()
}
