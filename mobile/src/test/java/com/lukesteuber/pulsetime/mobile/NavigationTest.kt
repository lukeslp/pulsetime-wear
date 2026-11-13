package com.lukesteuber.pulsetime.mobile

import org.junit.Assert.assertEquals
import org.junit.Test

class NavigationTest {
    @Test fun backReturnsToThePageThatOpenedThisOne() {
        assertEquals("systems", parentScreen("detail"))
        assertEquals("settings", parentScreen("option"))
        listOf("systems", "practice", "settings").forEach { assertEquals("home", parentScreen(it)) }
    }

    @Test fun backChainAlwaysEndsAtHome() {
        listOf("home", "systems", "detail", "practice", "settings", "option").forEach { start ->
            var screen = start
            repeat(3) { screen = parentScreen(screen) }
            assertEquals("home", screen)
        }
    }
}
