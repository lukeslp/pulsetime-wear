package com.lukesteuber.pulsetime.wear.remote

import com.google.android.gms.wearable.MessageEvent
import com.google.android.gms.wearable.Wearable
import com.google.android.gms.wearable.WearableListenerService
import com.lukesteuber.pulsetime.shared.PlaybackProtocol
import com.lukesteuber.pulsetime.shared.PlaybackResult
import com.lukesteuber.pulsetime.shared.PlaybackResultCode
import com.lukesteuber.pulsetime.shared.PulseTimePaths
import com.lukesteuber.pulsetime.wear.TellTimeController
import com.lukesteuber.pulsetime.wear.encoding.PlaybackSpeed
import com.lukesteuber.pulsetime.wear.encoding.TimeSystem
import com.lukesteuber.pulsetime.wear.settings.PulseSettings

class RemotePlaybackService : WearableListenerService() {
    override fun onMessageReceived(event: MessageEvent) {
        val result = when (val action = RemotePlaybackHandler.parse(event.path, event.data)) {
            is RemotePlaybackAction.Play -> play(action.command)
            is RemotePlaybackAction.Stop -> {
                TellTimeController(applicationContext).cancel()
                PlaybackResult(action.requestId, PlaybackResultCode.Stopped)
            }
            is RemotePlaybackAction.Reject -> action.result
        }
        Wearable.getMessageClient(this).sendMessage(
            event.sourceNodeId,
            PulseTimePaths.RESULT,
            PlaybackProtocol.encode(result),
        )
    }

    private fun play(command: com.lukesteuber.pulsetime.shared.PlaybackCommand): PlaybackResult {
        return runCatching {
            val settings = PulseSettings(
                amplitudeScale = command.amplitudeScale,
                precisionEnabled = command.precision,
                timeSystem = TimeSystem.fromStorage(command.systemId),
                layeredHaptics = command.layeredHaptics,
                playbackSpeed = PlaybackSpeed.fromStorage(command.speedId),
            )
            TellTimeController(applicationContext).playSample(command.hour, command.minute, settings)
            PlaybackResult(command.requestId, PlaybackResultCode.Started)
        }.getOrElse {
            PlaybackResult(command.requestId, PlaybackResultCode.Failed, "The watch could not play this pattern.")
        }
    }
}
