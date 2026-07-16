// File purpose: Process-wide haptic player so tile retap can cancel in-flight patterns.
// Primary components: HapticPlayerHolder.
// I/O: Context → shared AndroidHapticPlayer instance.
package com.lukesteuber.pulsetime.wear.haptics

import android.content.Context

object HapticPlayerHolder {
    @Volatile
    private var instance: HapticPlayer? = null

    fun get(context: Context): HapticPlayer {
        return instance ?: synchronized(this) {
            instance ?: AndroidHapticPlayer(context.applicationContext).also { instance = it }
        }
    }
}
