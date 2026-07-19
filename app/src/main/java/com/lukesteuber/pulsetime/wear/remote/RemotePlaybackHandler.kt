package com.lukesteuber.pulsetime.wear.remote

import com.lukesteuber.pulsetime.shared.PlaybackCommand
import com.lukesteuber.pulsetime.shared.PlaybackProtocol
import com.lukesteuber.pulsetime.shared.PlaybackResult
import com.lukesteuber.pulsetime.shared.PlaybackResultCode
import com.lukesteuber.pulsetime.shared.PulseTimePaths

sealed interface RemotePlaybackAction {
    data class Play(val command: PlaybackCommand) : RemotePlaybackAction
    data class Stop(val requestId: String) : RemotePlaybackAction
    data class Reject(val result: PlaybackResult) : RemotePlaybackAction
}

object RemotePlaybackHandler {
    fun parse(path: String, data: ByteArray): RemotePlaybackAction = when (path) {
        PulseTimePaths.PLAY -> PlaybackProtocol.decode(data)?.let(RemotePlaybackAction::Play)
            ?: RemotePlaybackAction.Reject(
                PlaybackResult("unknown", PlaybackResultCode.InvalidRequest, "The playback request was not valid."),
            )
        PulseTimePaths.STOP -> RemotePlaybackAction.Stop(data.decodeToString().take(64))
        else -> RemotePlaybackAction.Reject(
            PlaybackResult("unknown", PlaybackResultCode.InvalidRequest, "Unknown playback action."),
        )
    }
}
