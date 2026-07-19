package com.lukesteuber.pulsetime.wear.remote

import com.lukesteuber.pulsetime.shared.PlaybackCommand
import com.lukesteuber.pulsetime.shared.PlaybackProtocol
import com.lukesteuber.pulsetime.shared.PlaybackResultCode
import com.lukesteuber.pulsetime.shared.PulseTimePaths
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class RemotePlaybackHandlerTest {
    @Test fun validPlayPreservesEveryDemoOption() {
        val command = PlaybackCommand("abc-123", "morse_digits", 11, 59, .6f, false, "slow", true)
        val action = RemotePlaybackHandler.parse(PulseTimePaths.PLAY, PlaybackProtocol.encode(command))
        assertTrue(action is RemotePlaybackAction.Play)
        assertEquals(command, (action as RemotePlaybackAction.Play).command)
    }

    @Test fun malformedPlayNeverBecomesPlayback() {
        val action = RemotePlaybackHandler.parse(PulseTimePaths.PLAY, "bad".encodeToByteArray())
        assertTrue(action is RemotePlaybackAction.Reject)
        assertEquals(PlaybackResultCode.InvalidRequest, (action as RemotePlaybackAction.Reject).result.code)
    }

    @Test fun stopKeepsRequestCorrelation() {
        val action = RemotePlaybackHandler.parse(PulseTimePaths.STOP, "request-9".encodeToByteArray())
        assertEquals("request-9", (action as RemotePlaybackAction.Stop).requestId)
    }
}
