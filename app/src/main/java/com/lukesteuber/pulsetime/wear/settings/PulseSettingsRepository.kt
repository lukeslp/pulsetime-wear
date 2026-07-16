// File purpose: DataStore-backed PulseTime user settings.
// Primary components: PulseSettings, PulseSettingsRepository.
// I/O: Persists amplitude scale, precision, training completion.
package com.lukesteuber.pulsetime.wear.settings

import android.content.Context
import androidx.datastore.core.DataStore
import androidx.datastore.preferences.core.Preferences
import androidx.datastore.preferences.core.booleanPreferencesKey
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.floatPreferencesKey
import androidx.datastore.preferences.preferencesDataStore
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map

private val Context.dataStore: DataStore<Preferences> by preferencesDataStore(name = "pulsetime")

data class PulseSettings(
    val amplitudeScale: Float = 1.0f,
    val precisionEnabled: Boolean = false,
    val trainingComplete: Boolean = false,
    val firstRunComplete: Boolean = false,
)

class PulseSettingsRepository(private val context: Context) {
    private val ampKey = floatPreferencesKey("amplitude_scale")
    private val precisionKey = booleanPreferencesKey("precision_enabled")
    private val trainingKey = booleanPreferencesKey("training_complete")
    private val firstRunKey = booleanPreferencesKey("first_run_complete")

    val settings: Flow<PulseSettings> = context.dataStore.data.map { prefs ->
        PulseSettings(
            amplitudeScale = (prefs[ampKey] ?: 1.0f).coerceIn(0.3f, 1.0f),
            precisionEnabled = prefs[precisionKey] ?: false,
            trainingComplete = prefs[trainingKey] ?: false,
            firstRunComplete = prefs[firstRunKey] ?: false,
        )
    }

    suspend fun setAmplitudeScale(scale: Float) {
        context.dataStore.edit { it[ampKey] = scale.coerceIn(0.3f, 1.0f) }
    }

    suspend fun setPrecisionEnabled(enabled: Boolean) {
        context.dataStore.edit { it[precisionKey] = enabled }
    }

    suspend fun setTrainingComplete(complete: Boolean) {
        context.dataStore.edit { it[trainingKey] = complete }
    }

    suspend fun setFirstRunComplete(complete: Boolean) {
        context.dataStore.edit { it[firstRunKey] = complete }
    }
}
