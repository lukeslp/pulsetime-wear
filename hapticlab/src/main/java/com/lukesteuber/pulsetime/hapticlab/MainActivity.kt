package com.lukesteuber.pulsetime.hapticlab

import android.content.Intent
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.SystemBarStyle
import androidx.activity.compose.BackHandler
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.background
import androidx.compose.foundation.focusable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.statusBars
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.semantics.LiveRegionMode
import androidx.compose.ui.semantics.clearAndSetSemantics
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.heading
import androidx.compose.ui.semantics.liveRegion
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.compose.LocalLifecycleOwner
import com.lukesteuber.pulsetime.shared.SystemGuideCatalog
import com.lukesteuber.pulsetime.shared.encoding.EncodingOptions
import com.lukesteuber.pulsetime.shared.encoding.PlaybackSpeed
import com.lukesteuber.pulsetime.shared.encoding.TimeEncoderRegistry
import com.lukesteuber.pulsetime.shared.encoding.TimeSystem
import java.text.SimpleDateFormat
import java.util.Calendar
import java.util.Date
import java.util.Locale
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

private val Ink = Color(0xFF102A33)
private val Graphite = Color(0xFF07121A)
private val Paper = Color(0xFFF8FCFC)
private val Mist = Color(0xFFE9F5F7)
private val Signal = Color(0xFF087F8C)
private val SignalBright = Color(0xFF42D4E8)
private val Urgent = Color(0xFFB3261E)

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge(statusBarStyle = SystemBarStyle.dark(android.graphics.Color.rgb(7, 18, 26)))
        setContent { PulseLabTheme { PulseLabApp() } }
    }
}

@Composable
private fun PulseLabTheme(content: @Composable () -> Unit) {
    MaterialTheme(
        colorScheme = lightColorScheme(
            primary = Signal,
            onPrimary = Color.White,
            error = Urgent,
            background = Paper,
            surface = Color.White,
            onSurface = Ink,
        ),
        content = content,
    )
}

@Composable
private fun PulseLabApp() {
    val context = LocalContext.current
    val lifecycleOwner = LocalLifecycleOwner.current
    val player = remember { PhoneHapticPlayer(context) }
    val scope = rememberCoroutineScope()
    val trials = remember { mutableStateListOf<String>() }
    var screen by rememberSaveable { mutableStateOf("home") }
    var playingId by remember { mutableStateOf<String?>(null) }
    var status by remember { mutableStateOf(if (player.isAvailable) "Ready to vibrate this phone" else "This phone has no vibrator") }
    var strength by rememberSaveable { mutableFloatStateOf(.8f) }
    var completionJob by remember { mutableStateOf<Job?>(null) }

    fun stop(message: String = "Vibration stopped") {
        completionJob?.cancel()
        player.cancel()
        playingId = null
        status = message
    }

    fun play(id: String, timings: LongArray, amplitudes: IntArray, durationMs: Long) {
        stop()
        if (!player.play(timings, amplitudes)) {
            status = "This phone cannot vibrate"
            return
        }
        playingId = id
        status = "Playing vibration"
        completionJob = scope.launch {
            delay(durationMs + 120)
            playingId = null
            status = "Vibration finished"
        }
    }

    fun navigate(destination: String) {
        stop("Ready to vibrate this phone")
        screen = destination
    }

    BackHandler(screen != "home") { navigate("home") }
    DisposableEffect(lifecycleOwner) {
        val observer = LifecycleEventObserver { _, event ->
            if (event == Lifecycle.Event.ON_STOP) stop("Vibration stopped when app left foreground")
        }
        lifecycleOwner.lifecycle.addObserver(observer)
        onDispose {
            lifecycleOwner.lifecycle.removeObserver(observer)
            player.cancel()
        }
    }

    Scaffold(
        topBar = {
            Column(
                Modifier.fillMaxWidth().background(Graphite).windowInsetsPadding(WindowInsets.statusBars).padding(20.dp),
                verticalArrangement = Arrangement.spacedBy(10.dp),
            ) {
                if (screen != "home") {
                    OutlinedButton(onClick = { navigate("home") }, modifier = Modifier.heightIn(min = 48.dp)) {
                        Text("Back to Pulse Lab", color = SignalBright)
                    }
                }
                Text(
                    "PULSE / LAB",
                    color = SignalBright,
                    fontFamily = FontFamily.Monospace,
                    fontWeight = FontWeight.Bold,
                    letterSpacing = 2.sp,
                    modifier = Modifier.semantics { heading() },
                )
                Text(status, color = Color.White, modifier = Modifier.fillMaxWidth().semantics { liveRegion = LiveRegionMode.Polite })
                if (playingId != null) {
                    OutlinedButton(onClick = { stop() }, modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp)) {
                        Text("Stop vibration", color = Color.White)
                    }
                }
            }
        },
    ) { padding ->
        Box(Modifier.fillMaxSize().background(Paper).padding(padding)) {
            when {
                screen == "home" -> HomeScreen(player, strength, { strength = it }, ::navigate)
                screen == "time" -> TimeLabScreen(strength, playingId, ::play) { trials += it }
                screen == "signals" -> SignalsScreen { navigate("demo:$it") }
                screen == "feedback" -> FeedbackScreen(trials)
                screen.startsWith("demo:") -> HapticDemoCatalog.byId(screen.removePrefix("demo:"))?.let { demo ->
                    DemoScreen(demo, strength, playingId == demo.id, ::play) { trials += it }
                }
            }
        }
    }
}

@Composable
private fun HomeScreen(
    player: PhoneHapticPlayer,
    strength: Float,
    onStrength: (Float) -> Unit,
    navigate: (String) -> Unit,
) = Page {
    PageHeading("Feel, learn, improve")
    Text("Test every PulseTime clock and explore other information carried through phone vibration. No watch or account required.", style = MaterialTheme.typography.bodyLarge)
    Card(colors = CardDefaults.cardColors(containerColor = Mist)) {
        Column(Modifier.padding(18.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
            SectionHeading("Before you begin")
            Text("Hold the phone or place it on a surface. Stop if vibration feels uncomfortable.")
            if (!player.hasAmplitudeControl) Text("This phone supports rhythm but not distinct vibration strengths.", fontWeight = FontWeight.Bold)
            Text("Overall vibration strength", fontWeight = FontWeight.Bold)
            ChoiceColumn(
                choices = listOf("Gentle" to .55f, "Standard" to .8f, "Strong" to 1f),
                selected = strength,
                onSelect = onStrength,
            )
        }
    }
    DestinationCard("Tell time on this phone", "All six time systems, exact-minute options, speed, layered haptics, examples, and blind tests.") { navigate("time") }
    DestinationCard("Explore other signals", "Blind trials for markets, weather, people, urgency, and acknowledgement.") { navigate("signals") }
    DestinationCard("Send testing feedback", "Share what was clear, what was confused, and accessibility or navigation notes.") { navigate("feedback") }
}

@Composable
private fun DestinationCard(title: String, body: String, open: () -> Unit) {
    Card(onClick = open, modifier = Modifier.fillMaxWidth(), colors = CardDefaults.cardColors(containerColor = Color.White)) {
        Column(Modifier.padding(20.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text(title, style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.Bold)
            Text(body)
            Text("Open", color = Signal, fontWeight = FontWeight.Bold)
        }
    }
}

@Composable
private fun TimeLabScreen(
    strength: Float,
    playingId: String?,
    play: (String, LongArray, IntArray, Long) -> Unit,
    recordTrial: (String) -> Unit,
) = Page {
    var systemId by rememberSaveable { mutableStateOf(TimeSystem.TerseQuarter.storageId) }
    var speedId by rememberSaveable { mutableStateOf(PlaybackSpeed.Standard.storageId) }
    var layered by rememberSaveable { mutableStateOf(true) }
    var precision by rememberSaveable { mutableStateOf(false) }
    var hour by rememberSaveable { mutableIntStateOf(3) }
    var minute by rememberSaveable { mutableIntStateOf(42) }
    var quizAnswer by remember(systemId) { mutableStateOf<String?>(null) }
    val system = TimeSystem.fromStorage(systemId)
    val speed = PlaybackSpeed.fromStorage(speedId)
    val guide = SystemGuideCatalog.byId(systemId)!!

    fun playTime(id: String, h: Int, m: Int) {
        val encoded = TimeEncoderRegistry.encode(system, h, m, EncodingOptions(strength, layered, speed, precision))
        val waveform = encoded.toWaveform()
        play(id, waveform.first, waveform.second, waveform.first.sum())
    }

    PageHeading("Tell time on this phone")
    Text("The same encoders used by PulseTime on the watch. Choose a language, learn it, then test it without seeing the answer.")
    Section("Time system") {
        TimeSystem.entries.forEach { item ->
            FilterChip(
                selected = system == item,
                onClick = { systemId = item.storageId },
                label = { Text("${item.title} — ${item.shortLabel}") },
                modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp),
            )
        }
    }
    Card(colors = CardDefaults.cardColors(containerColor = Mist)) {
        Column(Modifier.padding(18.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
            SectionHeading(guide.title)
            Text(guide.summary)
            Text(guide.mentalModel, fontWeight = FontWeight.Bold)
            guide.legends.forEach { Text("${it.name}: ${it.meaning} (${it.mark})") }
        }
    }
    Section("Customize and compare") {
        Text("Speed changes spacing and pulse length. Try each speed with the same example.")
        ChoiceColumn(PlaybackSpeed.entries.map { it.label to it.storageId }, speedId) { speedId = it }
        SettingToggle("Layer vibration strength", "Uses stronger and softer pulses as a second channel. Rhythm still carries the meaning when off.", layered) { layered = it }
        SettingToggle("Include exact-minute ticks", "Adds leftover minute ticks to Terse Quarter. The other five systems are already exact.", precision) { precision = it }
        Text("Overall strength is set on the Pulse Lab home screen: ${(strength * 100).toInt()} percent.")
        OutlinedButton(onClick = { playTime("settings-test", 3, 42) }, modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp)) {
            Text(if (playingId == "settings-test") "Playing 3:42" else "Test these settings at 3:42")
        }
    }
    Section("Choose a practice time") {
        Text("Hour: $hour. Minute: ${minute.toString().padStart(2, '0')}.", fontWeight = FontWeight.Bold)
        Stepper("Hour", hour, { hour = if (hour == 1) 12 else hour - 1 }, { hour = if (hour == 12) 1 else hour + 1 })
        Stepper("Minute", minute, { minute = (minute + 59) % 60 }, { minute = (minute + 1) % 60 })
        Button(onClick = { playTime("practice", hour, minute) }, modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp)) { Text("Play $hour:${minute.toString().padStart(2, '0')}") }
        val now = Calendar.getInstance()
        OutlinedButton(onClick = { playTime("now", now.get(Calendar.HOUR), now.get(Calendar.MINUTE)) }, modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp)) { Text("Play the current time") }
    }
    Section("Blind recognition test") {
        Text("The answer stays hidden until you choose. Nothing is timed.")
        Button(onClick = { quizAnswer = null; playTime("time-quiz", guide.quiz.hour, guide.quiz.minute) }, modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp)) { Text("Play hidden time") }
        guide.quiz.choices.forEach { choice ->
            FilterChip(
                selected = quizAnswer == choice,
                onClick = {
                    quizAnswer = choice
                    val result = if (choice == guide.quiz.answer) "correct" else "incorrect"
                    recordTrial("Time | ${guide.title} | $result | chose $choice | answer ${guide.quiz.answer} | speed ${speed.label} | strength ${(strength * 100).toInt()}% | layered $layered | precision $precision")
                },
                label = { Text(choice) },
                modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp),
            )
        }
        quizAnswer?.let { choice ->
            Text((if (choice == guide.quiz.answer) "Correct. " else "The answer was ${guide.quiz.answer}. ") + guide.quiz.explanation, fontWeight = FontWeight.Bold, modifier = Modifier.semantics { liveRegion = LiveRegionMode.Polite })
        }
    }
}

@Composable
private fun Stepper(label: String, value: Int, decrease: () -> Unit, increase: () -> Unit) {
    Column(Modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Text("$label: $value", fontWeight = FontWeight.Bold)
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            OutlinedButton(decrease, Modifier.weight(1f).heightIn(min = 52.dp)) { Text("Decrease $label") }
            OutlinedButton(increase, Modifier.weight(1f).heightIn(min = 52.dp)) { Text("Increase $label") }
        }
    }
}

@Composable
private fun SettingToggle(title: String, explanation: String, checked: Boolean, change: (Boolean) -> Unit) {
    Card(colors = CardDefaults.cardColors(containerColor = Color.White)) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text(title, fontWeight = FontWeight.Bold)
            Text(explanation)
            Row(verticalAlignment = Alignment.CenterVertically) {
                Switch(checked, change)
                Spacer(Modifier.width(10.dp))
                Text(if (checked) "On" else "Off")
            }
        }
    }
}

@Composable
private fun SignalsScreen(openDemo: (String) -> Unit) = Page {
    PageHeading("Explore other signals")
    Text("Each fictional pattern has an untimed blind trial. Results can be included in your feedback.")
    HapticDemoCatalog.all.groupBy { it.category }.forEach { (category, demos) ->
        SectionHeading(category)
        demos.forEach { demo -> DestinationCard(demo.title, demo.summary) { openDemo(demo.id) } }
    }
}

@Composable
private fun DemoScreen(
    demo: HapticDemo,
    strength: Float,
    isPlaying: Boolean,
    play: (String, LongArray, IntArray, Long) -> Unit,
    recordTrial: (String) -> Unit,
) = Page {
    var blind by remember(demo.id) { mutableStateOf(false) }
    var answer by remember(demo.id) { mutableStateOf<String?>(null) }

    fun playDemo() {
        val waveform = demo.pattern.waveform(strength = strength)
        play(demo.id, waveform.first, waveform.second, waveform.first.sum())
    }

    PageHeading(demo.title)
    Text(demo.summary, style = MaterialTheme.typography.titleMedium)
    if (!blind || answer != null) {
        Section("What it means") { Text(demo.meaning, style = MaterialTheme.typography.bodyLarge) }
        Section("Pattern explanation") {
            PulseRail(demo)
            Text(demo.transcript, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold, color = Signal)
            Text("Width shows duration and height reinforces strength. Rhythm still carries meaning without amplitude control.")
        }
    }
    if (!blind) {
        Button(onClick = ::playDemo, modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp)) { Text(if (isPlaying) "Replay explained pattern" else "Play explained pattern") }
        OutlinedButton(onClick = { blind = true; answer = null; playDemo() }, modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp)) { Text("Start blind trial") }
    } else {
        Section("Blind trial") {
            Text("The meaning is hidden. Replay as often as you need, then choose what you felt.")
            Button(onClick = ::playDemo, modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp)) { Text("Replay hidden pattern") }
            demo.quizChoices.forEach { choice ->
                FilterChip(
                    selected = answer == choice,
                    onClick = {
                        answer = choice
                        val result = if (choice == demo.quizAnswer) "correct" else "incorrect"
                        recordTrial("Signal | ${demo.title} | $result | chose $choice | answer ${demo.quizAnswer} | strength ${(strength * 100).toInt()}%")
                    },
                    label = { Text(choice) },
                    modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp),
                )
            }
            answer?.let { choice ->
                Text((if (choice == demo.quizAnswer) "Correct. " else "The intended answer was ${demo.quizAnswer}. ") + demo.quizExplanation, fontWeight = FontWeight.Bold, modifier = Modifier.semantics { liveRegion = LiveRegionMode.Polite })
                OutlinedButton(onClick = { blind = false }, modifier = Modifier.fillMaxWidth()) { Text("Review full explanation") }
            }
        }
    }
}

@Composable
private fun FeedbackScreen(trials: List<String>) = Page {
    val context = LocalContext.current
    var felt by rememberSaveable { mutableStateOf("") }
    var meaning by rememberSaveable { mutableStateOf("") }
    var clarity by rememberSaveable { mutableStateOf("Unsure") }
    var accessNotes by rememberSaveable { mutableStateOf("") }

    PageHeading("Testing feedback")
    Text("For blind and sighted testers: describe perception in your own words. Recorded blind-trial results are included automatically.")
    OutlinedTextField(felt, { felt = it }, label = { Text("What did the vibration feel like?") }, modifier = Modifier.fillMaxWidth(), minLines = 3)
    OutlinedTextField(meaning, { meaning = it }, label = { Text("What did you think it meant?") }, modifier = Modifier.fillMaxWidth(), minLines = 3)
    Text("How distinguishable was it?", fontWeight = FontWeight.Bold)
    ChoiceColumn(listOf("Clear" to "Clear", "Unsure" to "Unsure", "Could not distinguish" to "Could not distinguish"), clarity) { clarity = it }
    OutlinedTextField(accessNotes, { accessNotes = it }, label = { Text("TalkBack, navigation, comfort, or other notes") }, modifier = Modifier.fillMaxWidth(), minLines = 4)
    Text("Blind trials recorded this session: ${trials.size}.")
    Button(
        onClick = {
            val report = buildString {
                appendLine("PulseTime signal test feedback")
                appendLine("Date: ${SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.US).format(Date())}")
                appendLine("Device: ${Build.MANUFACTURER} ${Build.MODEL}; Android ${Build.VERSION.RELEASE} (API ${Build.VERSION.SDK_INT})")
                appendLine("Clarity: $clarity")
                appendLine("What I felt: $felt")
                appendLine("What I thought it meant: $meaning")
                appendLine("Accessibility and comfort notes: $accessNotes")
                appendLine("Blind trial results:")
                if (trials.isEmpty()) appendLine("None recorded") else trials.forEach { appendLine("- $it") }
            }
            context.startActivity(Intent.createChooser(Intent(Intent.ACTION_SEND).apply {
                type = "text/plain"
                putExtra(Intent.EXTRA_SUBJECT, "PulseTime signal test feedback")
                putExtra(Intent.EXTRA_TEXT, report)
            }, "Share PulseTime feedback"))
        },
        modifier = Modifier.fillMaxWidth().heightIn(min = 52.dp),
    ) { Text("Share feedback") }
    Text("Sharing opens Android’s standard share sheet. Pulse Lab does not send anything by itself.")
}

@Composable
private fun <T> ChoiceColumn(choices: List<Pair<String, T>>, selected: T, onSelect: (T) -> Unit) {
    Column(Modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        choices.forEach { (label, value) ->
            FilterChip(selected == value, { onSelect(value) }, { Text(label) }, Modifier.fillMaxWidth().heightIn(min = 48.dp))
        }
    }
}

@Composable
private fun PulseRail(demo: HapticDemo) {
    Row(
        Modifier.fillMaxWidth().height(96.dp).background(Graphite, RoundedCornerShape(12.dp)).padding(12.dp)
            .semantics { contentDescription = "Signal strip: ${demo.transcript}. ${demo.durationLabel}." },
        horizontalArrangement = Arrangement.spacedBy(4.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        demo.pattern.steps.forEach { step ->
            if (step.onMs == 0L) Spacer(Modifier.width(14.dp)) else Box(
                Modifier.weight(step.onMs.toFloat().coerceAtLeast(50f)).height((22 + step.amplitude / 5).dp.coerceAtMost(68.dp))
                    .background(if (step.amplitude >= 200) SignalBright else Color(0xFF89DCE5), RoundedCornerShape(4.dp)).clearAndSetSemantics { },
            )
        }
    }
}

@Composable
private fun Page(content: @Composable ColumnScope.() -> Unit) {
    Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(20.dp), verticalArrangement = Arrangement.spacedBy(16.dp), content = content)
}

@Composable
private fun PageHeading(text: String) {
    val requester = remember { FocusRequester() }
    Text(text, style = MaterialTheme.typography.headlineLarge, fontWeight = FontWeight.Bold, modifier = Modifier.focusRequester(requester).focusable().semantics { heading() })
    LaunchedEffect(text) { requester.requestFocus() }
}

@Composable
private fun SectionHeading(text: String) = Text(text, style = MaterialTheme.typography.headlineSmall, fontWeight = FontWeight.Bold, modifier = Modifier.semantics { heading() })

@Composable
private fun Section(title: String, content: @Composable ColumnScope.() -> Unit) {
    Column(Modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(10.dp)) {
        Text(title, style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.Bold, modifier = Modifier.semantics { heading() })
        content()
    }
}
