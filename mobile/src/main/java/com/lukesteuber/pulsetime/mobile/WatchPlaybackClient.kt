package com.lukesteuber.pulsetime.mobile

import android.content.Context
import com.google.android.gms.wearable.CapabilityClient
import com.google.android.gms.wearable.CapabilityInfo
import com.google.android.gms.wearable.MessageClient
import com.google.android.gms.wearable.MessageEvent
import com.google.android.gms.wearable.Wearable
import com.lukesteuber.pulsetime.shared.PlaybackCommand
import com.lukesteuber.pulsetime.shared.PlaybackProtocol
import com.lukesteuber.pulsetime.shared.PlaybackResultCode
import com.lukesteuber.pulsetime.shared.PulseTimePaths
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.tasks.await

sealed interface WatchState {
    data object Checking : WatchState
    data object NotConnected : WatchState
    data class Ready(val nodeId: String, val watchName: String) : WatchState
    data class Sending(val requestId: String, val watchName: String) : WatchState
    data class Playing(val requestId: String, val watchName: String) : WatchState
    data class Failed(val message: String) : WatchState
}

class WatchPlaybackClient(context: Context, private val onState: (WatchState) -> Unit) :
    MessageClient.OnMessageReceivedListener, CapabilityClient.OnCapabilityChangedListener {
    private val capabilityClient = Wearable.getCapabilityClient(context)
    private val messageClient = Wearable.getMessageClient(context)
    private var ready: WatchState.Ready? = null

    fun start() {
        messageClient.addListener(this)
        // Hear about a watch that comes into reach, or leaves, while the app is open.
        capabilityClient.addListener(this, PulseTimePaths.CAPABILITY)
    }

    fun stop() {
        messageClient.removeListener(this)
        capabilityClient.removeListener(this, PulseTimePaths.CAPABILITY)
    }

    override fun onCapabilityChanged(info: CapabilityInfo) {
        val before = ready
        ready = pickWatch(info.nodes)
        val now = ready
        when {
            now == null -> onState(WatchState.NotConnected)
            // Leave a demonstration that is being sent or played alone.
            before == null || before.nodeId != now.nodeId -> onState(now)
        }
    }

    private fun pickWatch(nodes: Set<com.google.android.gms.wearable.Node>): WatchState.Ready? =
        nodes.sortedWith(compareByDescending<com.google.android.gms.wearable.Node> { it.isNearby }.thenBy { it.displayName })
            .firstOrNull()?.let { WatchState.Ready(it.id, it.displayName) }

    suspend fun refresh() {
        onState(WatchState.Checking)
        val capability = runCatching {
            capabilityClient.getCapability(PulseTimePaths.CAPABILITY, CapabilityClient.FILTER_REACHABLE).await()
        }.getOrElse {
            onState(WatchState.NotConnected)
            return
        }
        ready = pickWatch(capability.nodes)
        onState(ready ?: WatchState.NotConnected)
    }

    suspend fun play(command: PlaybackCommand) {
        val watch = ready ?: run {
            onState(WatchState.NotConnected)
            return
        }
        onState(WatchState.Sending(command.requestId, watch.watchName))
        runCatching {
            messageClient.sendMessage(watch.nodeId, PulseTimePaths.PLAY, PlaybackProtocol.encode(command)).await()
        }.onFailure {
            onState(WatchState.Failed("Nothing played. Check the watch connection and try again."))
        }
    }

    suspend fun stopPlayback(requestId: String) {
        val watch = ready ?: return
        // A watch that walked out of range makes sendMessage fail; report it
        // instead of letting the exception escape viewModelScope and crash.
        runCatching {
            messageClient.sendMessage(watch.nodeId, PulseTimePaths.STOP, requestId.encodeToByteArray()).await()
        }.onFailure {
            if (it is CancellationException) throw it
            onState(WatchState.NotConnected)
        }
    }

    override fun onMessageReceived(event: MessageEvent) {
        if (event.path != PulseTimePaths.RESULT) return
        val result = PlaybackProtocol.decodeResult(event.data) ?: return
        val watch = ready ?: return
        onState(
            when (result.code) {
                PlaybackResultCode.Started -> WatchState.Playing(result.requestId, watch.watchName)
                PlaybackResultCode.Stopped -> watch
                else -> WatchState.Failed(result.detail.ifBlank { "The watch could not play this pattern." })
            },
        )
    }
}
