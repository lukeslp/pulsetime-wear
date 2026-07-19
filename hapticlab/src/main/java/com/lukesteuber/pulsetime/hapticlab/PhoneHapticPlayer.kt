package com.lukesteuber.pulsetime.hapticlab

import android.content.Context
import android.os.Build
import android.os.VibrationEffect
import android.os.Vibrator
import android.os.VibratorManager

data class HapticCapabilities(
    val hasVibrator: Boolean,
    val hasAmplitudeControl: Boolean,
    val supportsClearEffects: Boolean,
    val supportsPrimitives: Boolean,
    val supportsEnvelopes: Boolean,
)

enum class ClearCue { Gentle, Standard, Strong }

class PhoneHapticPlayer(context: Context) {
    private val vibrator: Vibrator = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
        context.getSystemService(VibratorManager::class.java).defaultVibrator
    } else {
        @Suppress("DEPRECATION")
        context.getSystemService(Vibrator::class.java)
    }

    val isAvailable: Boolean get() = vibrator.hasVibrator()
    val hasAmplitudeControl: Boolean get() = vibrator.hasAmplitudeControl()

    val capabilities: HapticCapabilities
        get() {
            val effects = if (Build.VERSION.SDK_INT >= 30) {
                vibrator.areEffectsSupported(
                    VibrationEffect.EFFECT_TICK,
                    VibrationEffect.EFFECT_CLICK,
                    VibrationEffect.EFFECT_HEAVY_CLICK,
                ).all { it == Vibrator.VIBRATION_EFFECT_SUPPORT_YES }
            } else false
            val primitives = if (Build.VERSION.SDK_INT >= 30) {
                vibrator.arePrimitivesSupported(
                    VibrationEffect.Composition.PRIMITIVE_LOW_TICK,
                    VibrationEffect.Composition.PRIMITIVE_CLICK,
                    VibrationEffect.Composition.PRIMITIVE_THUD,
                ).all { it }
            } else false
            val envelopes = Build.VERSION.SDK_INT >= 36 && vibrator.areEnvelopeEffectsSupported()
            return HapticCapabilities(isAvailable, hasAmplitudeControl, effects, primitives, envelopes)
        }

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

    fun playClearCue(cue: ClearCue): Boolean {
        cancel()
        if (!isAvailable) return false
        val predefined = when (cue) {
            ClearCue.Gentle -> VibrationEffect.EFFECT_TICK
            ClearCue.Standard -> VibrationEffect.EFFECT_CLICK
            ClearCue.Strong -> VibrationEffect.EFFECT_HEAVY_CLICK
        }
        val canUsePredefined = Build.VERSION.SDK_INT >= 30 &&
            vibrator.areEffectsSupported(predefined).first() == Vibrator.VIBRATION_EFFECT_SUPPORT_YES
        val effect = if (Build.VERSION.SDK_INT >= 29 && canUsePredefined) {
            VibrationEffect.createPredefined(predefined)
        } else {
            val amplitude = when (cue) {
                ClearCue.Gentle -> 95
                ClearCue.Standard -> 165
                ClearCue.Strong -> 235
            }
            VibrationEffect.createOneShot(70, if (hasAmplitudeControl) amplitude else VibrationEffect.DEFAULT_AMPLITUDE)
        }
        vibrator.vibrate(effect)
        return true
    }

    fun cancel() = vibrator.cancel()
}
