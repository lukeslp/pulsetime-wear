package com.lukesteuber.pulsetime.shared

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class PlaybackProtocolTest {
    @Test fun commandRoundTrips() {
        val value = PlaybackCommand("request-1", "binary_clock", 3, 42, .8f, true, "fast", false)
        assertEquals(value, PlaybackProtocol.decode(PlaybackProtocol.encode(value)))
    }

    @Test fun invalidCommandsAreRejected() {
        assertNull(PlaybackProtocol.decode("2|request|binary_clock|3|42|1|true|fast|false".encodeToByteArray()))
        assertNull(PlaybackProtocol.decode("1|request|unknown|3|42|1|true|fast|false".encodeToByteArray()))
        assertNull(PlaybackProtocol.decode("1|request|binary_clock|25|42|1|true|fast|false".encodeToByteArray()))
    }

    @Test fun everySystemHasCompleteEducation() {
        assertEquals(6, SystemGuideCatalog.all.size)
        SystemGuideCatalog.all.forEach {
            assertTrue(it.legends.isNotEmpty())
            assertTrue(it.examples.size >= 2)
            assertTrue(it.quiz.choices.contains(it.quiz.answer))
        }
    }
}
