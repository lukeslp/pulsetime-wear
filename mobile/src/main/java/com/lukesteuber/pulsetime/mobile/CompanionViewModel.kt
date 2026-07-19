package com.lukesteuber.pulsetime.mobile

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.lukesteuber.pulsetime.shared.PlaybackCommand
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch
import java.util.UUID

class CompanionViewModel(application: Application) : AndroidViewModel(application) {
    private val _watchState = MutableStateFlow<WatchState>(WatchState.Checking)
    val watchState: StateFlow<WatchState> = _watchState
    private val client = WatchPlaybackClient(application) { _watchState.value = it }

    var selectedSystemId = "terse_quarter"
    var speedId = "standard"
    var layered = true
    var precision = false
    var strength = .8f

    fun start() {
        client.start()
        refresh()
    }
    fun stop() = client.stop()
    fun refresh() = viewModelScope.launch { client.refresh() }

    fun play(systemId: String, hour: Int, minute: Int) {
        selectedSystemId = systemId
        val command = PlaybackCommand(
            requestId = UUID.randomUUID().toString(),
            systemId = systemId,
            hour = hour,
            minute = minute,
            amplitudeScale = strength,
            layeredHaptics = layered,
            speedId = speedId,
            precision = precision,
        )
        viewModelScope.launch { client.play(command) }
    }

    fun stopPlayback() {
        val state = _watchState.value
        if (state is WatchState.Playing) viewModelScope.launch { client.stopPlayback(state.requestId) }
    }
}
