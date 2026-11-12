// File purpose: The scrolling list every PulseTime watch screen is built on.
// Primary components: WatchList.
// I/O: List items → a round-aware, crown-scrollable column.
package com.lukesteuber.pulsetime.wear

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.unit.dp
import androidx.wear.compose.foundation.lazy.ScalingLazyColumn
import androidx.wear.compose.foundation.lazy.ScalingLazyListScope
import androidx.wear.compose.foundation.lazy.rememberScalingLazyListState
import androidx.wear.compose.material3.ScreenScaffold

/**
 * A plain scrolling column lets the round bezel cut through full-width
 * buttons near the top and bottom of the screen. This list shrinks rows as
 * they approach the edge so their labels stay readable, lets the first and
 * last rows reach the middle of the screen, follows the crown or bezel, and
 * shows a scroll position indicator.
 */
@Composable
fun WatchList(content: ScalingLazyListScope.() -> Unit) {
    val state = rememberScalingLazyListState()
    val configuration = LocalConfiguration.current
    // Wear guidance: 5.2% side margins on round screens; a small fixed margin on square ones.
    val side = if (configuration.isScreenRound) (configuration.screenWidthDp * 0.052f).dp else 8.dp
    ScreenScaffold(scrollState = state) {
        ScalingLazyColumn(
            state = state,
            modifier = Modifier.fillMaxSize(),
            contentPadding = PaddingValues(horizontal = side),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.spacedBy(8.dp),
            content = content,
        )
    }
}
