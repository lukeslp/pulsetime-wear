package com.lukesteuber.pulsetime.mobile

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.BackHandler
import androidx.activity.compose.setContent
import androidx.compose.foundation.background
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.selection.toggleable
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.semantics.LiveRegionMode
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.clearAndSetSemantics
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.heading
import androidx.compose.ui.semantics.liveRegion
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.compose.LifecycleResumeEffect
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewmodel.compose.viewModel
import com.lukesteuber.pulsetime.shared.GuideExample
import com.lukesteuber.pulsetime.shared.SystemGuide
import com.lukesteuber.pulsetime.shared.SystemGuideCatalog
import com.lukesteuber.pulsetime.shared.encoding.PatternDescriber
import com.lukesteuber.pulsetime.shared.encoding.TimeSystem

private val Ink = Color(0xFF102A33)
private val PulseBlue = Color(0xFF087F8C)
private val SignalAmber = Color(0xFFE69B22)
// Text-weight versions of the two accents: at least 4.5:1 on every card color used here.
private val PulseBlueText = Color(0xFF05616B)
private val SignalAmberText = Color(0xFF8A5300)
private val Mist = Color(0xFFE9F5F7)
private val Paper = Color(0xFFF8FCFC)

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent { PulseTimeTheme { CompanionApp() } }
    }
}

@Composable
private fun PulseTimeTheme(content: @Composable () -> Unit) {
    MaterialTheme(
        colorScheme = lightColorScheme(primary = PulseBlue, onPrimary = Color.White, secondary = SignalAmber, background = Paper, surface = Color.White, onSurface = Ink),
        content = content,
    )
}

@Composable
private fun CompanionApp(vm: CompanionViewModel = viewModel()) {
    var screen by rememberSaveable { mutableStateOf("home") }
    var detailId by rememberSaveable { mutableStateOf(vm.selectedSystemId) }
    val watchState by vm.watchState.collectAsStateWithLifecycle()
    DisposableEffect(Unit) {
        vm.start()
        onDispose { vm.stop() }
    }
    // Look for the watch again each time the app comes back to the front.
    LifecycleResumeEffect(Unit) {
        vm.refresh()
        onPauseOrDispose { }
    }
    // Back returns to the page this one was opened from; on Home it leaves the app.
    BackHandler(enabled = screen != "home") { screen = parentScreen(screen) }
    Scaffold(
        topBar = {
            Column(
                Modifier
                    .fillMaxWidth()
                    .background(Mist)
                    .windowInsetsPadding(WindowInsets.statusBars)
                    .padding(horizontal = 20.dp, vertical = 14.dp),
            ) {
                if (screen != "home") OutlinedButton(onClick = { screen = "home" }) { Text("← Home") }
                Text("PULSE / TIME", fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Bold, letterSpacing = 2.sp, color = Ink, modifier = Modifier.semantics { heading() })
                WatchStatus(watchState, vm::refresh, vm::stopPlayback)
            }
        },
    ) { padding ->
        Box(Modifier.fillMaxSize().padding(padding)) {
            when (screen) {
                "home" -> HomeScreen({ screen = "systems" }, { screen = "practice" }, { screen = "settings" })
                "systems" -> SystemsScreen { id -> detailId = id; screen = "detail" }
                "detail" -> SystemDetailScreen(SystemGuideCatalog.byId(detailId)!!, vm) { screen = "practice" }
                "practice" -> PracticeScreen(vm)
                "settings" -> SettingsScreen(vm) { id -> detailId = id; screen = "option" }
                "option" -> OptionDetailScreen(detailId, vm)
            }
        }
    }
}

internal fun parentScreen(screen: String): String = when (screen) {
    "detail" -> "systems"
    "option" -> "settings"
    else -> "home"
}

@Composable
private fun WatchStatus(state: WatchState, retry: () -> Unit, stop: () -> Unit) {
    val text = when (state) {
        WatchState.Checking -> "Checking for your watch…"
        WatchState.NotConnected -> "Watch not connected"
        is WatchState.Ready -> "${state.watchName} is ready"
        is WatchState.Sending -> "Sending to ${state.watchName}…"
        is WatchState.Playing -> "Playing on ${state.watchName}"
        is WatchState.Failed -> state.message
    }
    Row(Modifier.fillMaxWidth().semantics { liveRegion = LiveRegionMode.Polite }, verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.SpaceBetween) {
        Text(text, style = MaterialTheme.typography.bodyMedium, modifier = Modifier.weight(1f))
        when (state) {
            is WatchState.Playing -> OutlinedButton(onClick = stop) { Text("Stop") }
            WatchState.NotConnected, is WatchState.Failed -> OutlinedButton(onClick = retry) { Text("Try again") }
            else -> Unit
        }
    }
}

@Composable
private fun Page(content: @Composable ColumnScope.() -> Unit) {
    Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(20.dp), verticalArrangement = Arrangement.spacedBy(16.dp), content = content)
}

@Composable
private fun HomeScreen(onLearn: () -> Unit, onPractice: () -> Unit, onSettings: () -> Unit) = Page {
    Heading("Learn the clock by touch")
    Text("Explore a system on your phone, then feel every example and test on your watch.", style = MaterialTheme.typography.bodyLarge)
    HomeAction("Learn a time system", "Understand every pulse, with worked examples.", onLearn)
    HomeAction("Practice on my watch", "Replay freely and test yourself without a time limit.", onPractice)
    HomeAction("Watch settings", "Learn what speed, strength, and precision change.", onSettings)
}

@Composable
private fun HomeAction(title: String, description: String, action: () -> Unit) {
    Card(onClick = action, modifier = Modifier.fillMaxWidth(), colors = CardDefaults.cardColors(containerColor = Color.White)) {
        Column(Modifier.padding(20.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Text(title, style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.Bold)
            Text(description)
        }
    }
}

@Composable
private fun SystemsScreen(open: (String) -> Unit) = Page {
    Heading("Time systems")
    Text("Each system has its own explanation, examples, watch demonstrations, and test.")
    SystemGuideCatalog.all.forEach { guide ->
        Card(onClick = { open(guide.id) }, modifier = Modifier.fillMaxWidth()) {
            Column(Modifier.padding(18.dp)) {
                Text(guide.title, style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.Bold)
                Text(guide.shortLabel.uppercase(), fontFamily = FontFamily.Monospace, color = PulseBlueText)
                Text(guide.summary)
            }
        }
    }
}

@Composable
private fun SystemDetailScreen(guide: SystemGuide, vm: CompanionViewModel, onPractice: () -> Unit) = Page {
    Heading(guide.title)
    Text(guide.summary, style = MaterialTheme.typography.titleMedium)
    Section("How it works") { Text(guide.mentalModel) }
    Section("What each pulse means") {
        guide.legends.forEach { legend ->
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(14.dp)) {
                // The mark is a drawing of the pulse; its name and meaning beside it are the words.
                Text(legend.mark, fontFamily = FontFamily.Monospace, fontSize = 24.sp, color = PulseBlueText, modifier = Modifier.width(64.dp).clearAndSetSemantics { })
                Column { Text(legend.name, fontWeight = FontWeight.Bold); Text(legend.meaning) }
            }
        }
    }
    Section("Worked examples") { guide.examples.forEach { ExampleCard(guide, it, vm) } }
    QuizCard(guide, vm)
    Button(onClick = { vm.selectedSystemId = guide.id }, modifier = Modifier.fillMaxWidth()) {
        Text(if (vm.selectedSystemId == guide.id) "Selected for practice" else "Use this system for practice")
    }
    OutlinedButton(onClick = onPractice, modifier = Modifier.fillMaxWidth()) { Text("Open practice") }
}

@Composable
private fun ExampleCard(guide: SystemGuide, example: GuideExample, vm: CompanionViewModel) {
    Card(Modifier.fillMaxWidth(), colors = CardDefaults.cardColors(containerColor = Mist)) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
            Text(example.label, style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.Bold)
            Text(example.breakdown)
            // Exact-minute ticks are drawn in the Terse Quarter score, so describe them too.
            PulseScore(example.score, PatternDescriber.describe(TimeSystem.fromStorage(guide.id), example.hour, example.minute, includePrecision = true))
            Button(onClick = { vm.play(guide.id, example.hour, example.minute) }, modifier = Modifier.fillMaxWidth()) { Text("Play ${example.label} on watch") }
        }
    }
}

@Composable
private fun PulseScore(score: String, spoken: String) {
    // A screen reader gets the pattern in words instead of a row of dashes and dots.
    Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()).clearAndSetSemantics { contentDescription = "Pattern: $spoken" }, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
        score.split(" ").filter { it.isNotBlank() }.forEach { mark ->
            Text(mark, fontFamily = FontFamily.Monospace, fontSize = 22.sp, color = if (mark == "/") SignalAmberText else PulseBlueText, modifier = Modifier.background(Color.White, RoundedCornerShape(6.dp)).padding(horizontal = 8.dp, vertical = 6.dp))
        }
    }
}

@Composable
private fun QuizCard(guide: SystemGuide, vm: CompanionViewModel) {
    var answer by remember(guide.id) { mutableStateOf<String?>(null) }
    val quiz = guide.quiz
    Section("Test yourself") {
        Text("Feel the pattern, then choose the time. Replay as often as you want.")
        Button(onClick = { vm.play(guide.id, quiz.hour, quiz.minute) }, modifier = Modifier.fillMaxWidth()) { Text("Play test pattern on watch") }
        quiz.choices.forEach { choice -> OutlinedButton(onClick = { answer = choice }, modifier = Modifier.fillMaxWidth()) { Text(choice) } }
        answer?.let {
            Text((if (it == quiz.answer) "Correct. " else "Not yet. ") + quiz.explanation, fontWeight = FontWeight.Bold, modifier = Modifier.semantics { liveRegion = LiveRegionMode.Polite })
        }
    }
}

@Composable
private fun PracticeScreen(vm: CompanionViewModel) = Page {
    val guide = SystemGuideCatalog.byId(vm.selectedSystemId)!!
    var hour by rememberSaveable { mutableIntStateOf(3) }
    var minute by rememberSaveable { mutableIntStateOf(42) }
    Heading("Practice on my watch")
    Text("Using ${guide.title}. Choose a time, feel it, then compare it with the explanation.")
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceEvenly) {
        NumberStepper("Hour", hour, { hour = if (hour == 1) 12 else hour - 1 }, { hour = if (hour == 12) 1 else hour + 1 })
        NumberStepper("Minute", minute, { minute = if (minute == 0) 59 else minute - 1 }, { minute = if (minute == 59) 0 else minute + 1 })
    }
    Button(onClick = { vm.play(guide.id, hour, minute) }, modifier = Modifier.fillMaxWidth()) { Text("Play ${"%d:%02d".format(hour, minute)} on watch") }
    Text("What you will feel: " + PatternDescriber.describe(TimeSystem.fromStorage(guide.id), hour, minute, vm.precision))
    guide.examples.forEach { ExampleCard(guide, it, vm) }
}

@Composable
private fun NumberStepper(label: String, value: Int, down: () -> Unit, up: () -> Unit) {
    Column(horizontalAlignment = Alignment.CenterHorizontally) {
        Text(label, fontWeight = FontWeight.Bold)
        OutlinedButton(onClick = up) { Text("Increase $label") }
        // Announce the new value when a stepper button changes it.
        Text(value.toString().padStart(2, '0'), fontFamily = FontFamily.Monospace, fontSize = 36.sp, modifier = Modifier.semantics { liveRegion = LiveRegionMode.Polite; contentDescription = "$label $value" })
        OutlinedButton(onClick = down) { Text("Decrease $label") }
    }
}

@Composable
private fun SettingsScreen(vm: CompanionViewModel, openOption: (String) -> Unit) = Page {
    Heading("Watch settings")
    Text("Every option has an explanation and a watch demonstration.")
    OptionCard("speed", "Playback speed", "Changes the pace, not the meaning.", openOption)
    OptionCard("layering", "Layered strength", "Adds relative strength while preserving duration and rhythm.", openOption)
    OptionCard("precision", "Exact minutes", "Adds leftover-minute ticks to Terse Quarter.", openOption)
    Text("Current demo settings", style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.Bold)
    Text("Speed: ${vm.speedId}. Layered strength: ${if (vm.layered) "on" else "off"}. Exact minutes: ${if (vm.precision) "on" else "off"}.")
}

@Composable
private fun OptionCard(id: String, title: String, description: String, open: (String) -> Unit) {
    Card(onClick = { open(id) }, modifier = Modifier.fillMaxWidth()) { Column(Modifier.padding(18.dp)) { Text(title, style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.Bold); Text(description) } }
}

@Composable
private fun OptionDetailScreen(id: String, vm: CompanionViewModel) = Page {
    when (id) {
        "speed" -> {
            Heading("Playback speed")
            Text("Speed stretches or compresses pauses and pulses together. The pattern keeps the same meaning.")
            listOf("slow" to "Slow gives each pulse more room.", "standard" to "Standard balances clarity and length.", "fast" to "Fast shortens the whole pattern.").forEach { (value, description) ->
                Button(onClick = { vm.speedId = value; vm.play(vm.selectedSystemId, 3, 42) }, modifier = Modifier.fillMaxWidth()) { Text("Play 3:42 at $value speed") }
                Text(description)
            }
        }
        "layering" -> {
            Heading("Layered strength")
            Text("Layering makes different pulse roles feel softer or firmer. Duration and rhythm still carry the same information when strength differences are hard to feel.")
            // One control for a screen reader: the label and the switch together.
            Row(
                Modifier.fillMaxWidth().heightIn(min = 48.dp).toggleable(value = vm.layered, role = Role.Switch, onValueChange = { vm.layered = it }),
                verticalAlignment = Alignment.CenterVertically,
            ) { Text("Use layered strength", modifier = Modifier.weight(1f)); Switch(checked = vm.layered, onCheckedChange = null) }
            Button(onClick = { vm.layered = true; vm.play("five_minute_ladder", 3, 42) }, modifier = Modifier.fillMaxWidth()) { Text("Play layered example on watch") }
            OutlinedButton(onClick = { vm.layered = false; vm.play("five_minute_ladder", 3, 42) }, modifier = Modifier.fillMaxWidth()) { Text("Play even-strength example") }
        }
        else -> {
            Heading("Exact minutes")
            Text("Terse Quarter normally gives the quarter-hour range. Exact minutes adds short ticks for minutes left after the last quarter.")
            Button(onClick = { vm.precision = false; vm.play("terse_quarter", 3, 42) }, modifier = Modifier.fillMaxWidth()) { Text("Play 3:42 without exact ticks") }
            Button(onClick = { vm.precision = true; vm.play("terse_quarter", 3, 42) }, modifier = Modifier.fillMaxWidth()) { Text("Play 3:42 with exact ticks") }
            Text("The other five systems already encode exact minutes.")
        }
    }
}

@Composable private fun Heading(text: String) = Text(text, style = MaterialTheme.typography.headlineLarge, fontWeight = FontWeight.Bold, modifier = Modifier.semantics { heading() })

@Composable
private fun Section(title: String, content: @Composable ColumnScope.() -> Unit) {
    Column(Modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(10.dp)) {
        Text(title, style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.Bold, modifier = Modifier.semantics { heading() })
        content()
    }
    Spacer(Modifier.height(4.dp))
}
