package com.lukesteuber.pulsetime.shared

object PulseTimePaths {
    const val CAPABILITY = "pulsetime_haptic_playback_v1"
    const val PLAY = "/pulsetime/v1/play-sample"
    const val STOP = "/pulsetime/v1/stop"
    const val RESULT = "/pulsetime/v1/result"
}

data class PlaybackCommand(
    val requestId: String,
    val systemId: String,
    val hour: Int,
    val minute: Int,
    val amplitudeScale: Float,
    val layeredHaptics: Boolean,
    val speedId: String,
    val precision: Boolean,
) {
    init {
        require(requestId.matches(Regex("[A-Za-z0-9-]{1,64}")))
        require(SystemGuideCatalog.byId(systemId) != null)
        require(hour in 0..23)
        require(minute in 0..59)
        require(amplitudeScale in 0.3f..1f)
        require(speedId in setOf("slow", "standard", "fast"))
    }
}

enum class PlaybackResultCode { Started, Stopped, InvalidRequest, UnsupportedVersion, Failed }

data class PlaybackResult(val requestId: String, val code: PlaybackResultCode, val detail: String = "")

object PlaybackProtocol {
    private const val VERSION = "1"

    fun encode(command: PlaybackCommand): ByteArray = listOf(
        VERSION,
        command.requestId,
        command.systemId,
        command.hour,
        command.minute,
        command.amplitudeScale,
        command.layeredHaptics,
        command.speedId,
        command.precision,
    ).joinToString("|").encodeToByteArray()

    fun decode(bytes: ByteArray): PlaybackCommand? = runCatching {
        val fields = bytes.decodeToString().split("|")
        if (fields.size != 9 || fields[0] != VERSION) return null
        PlaybackCommand(
            requestId = fields[1],
            systemId = fields[2],
            hour = fields[3].toInt(),
            minute = fields[4].toInt(),
            amplitudeScale = fields[5].toFloat(),
            layeredHaptics = fields[6].toBooleanStrict(),
            speedId = fields[7],
            precision = fields[8].toBooleanStrict(),
        )
    }.getOrNull()

    fun encode(result: PlaybackResult): ByteArray =
        listOf(VERSION, result.requestId, result.code.name, result.detail.replace("|", "/"))
            .joinToString("|")
            .encodeToByteArray()

    fun decodeResult(bytes: ByteArray): PlaybackResult? = runCatching {
        val fields = bytes.decodeToString().split("|", limit = 4)
        if (fields.size != 4 || fields[0] != VERSION) return null
        PlaybackResult(fields[1], PlaybackResultCode.valueOf(fields[2]), fields[3])
    }.getOrNull()
}
