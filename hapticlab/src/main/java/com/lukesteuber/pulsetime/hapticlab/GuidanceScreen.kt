package com.lukesteuber.pulsetime.hapticlab

import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.semantics.heading
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp

@Composable
internal fun GuidanceScreen() = Page {
    PageHeading("How Pulse Lab works")
    Text(
        "Pulse Lab turns small pieces of information into short vibration phrases. Every phrase is a proposal. Your tests help decide which parts are clear and which parts need work.",
        style = MaterialTheme.typography.bodyLarge,
    )

    GuideCard(
        "Start with comfort",
        "Choose the gentlest strength you can feel clearly. Hold the phone, put it in a pocket, or place it on a surface. Use the same position while comparing patterns. Stop whenever a vibration feels unpleasant.",
    )
    GuideCard(
        "Four ways a pulse can change",
        "Rhythm changes the order and spacing. Duration makes a pulse short or long. Strength can add a second layer on supported phones. Pauses divide a phrase into parts. Rhythm and duration always carry the main meaning.",
    )
    GuideCard(
        "A phrase has parts",
        "Most signal demonstrations begin with a topic, such as weather or messages. A payload says what happened. A modifier can add size, confidence, or urgency. Some phrases end with an action such as acknowledge or get ready.",
    )
    GuideCard(
        "Run a fair blind trial",
        "Learn the parts first. Start the blind trial when you are ready, replay as often as needed, and choose the closest meaning. Take all the time you need. A wrong answer is useful because it shows which signals collide.",
    )
    GuideCard(
        "Demonstrations stay fictional",
        "Pulse Lab does not read live messages, markets, weather, calendars, health data, or travel directions. Never rely on an experimental pattern as your only warning or instruction.",
    )
    GuideCard(
        "Your data stays with you",
        "The app works offline and keeps session notes in memory. It sends nothing by itself. Feedback leaves the phone only when you open Android's share sheet and choose where to send it.",
    )

    Text(
        "Good signals survive different phones, different days, and time away from practice. One clear test is a start, not a verdict.",
        fontWeight = FontWeight.Bold,
    )
}

@Composable
private fun GuideCard(title: String, body: String) {
    Card(colors = CardDefaults.cardColors(containerColor = Color.White)) {
        Column(
            Modifier.padding(18.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Text(title, style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.Bold, modifier = Modifier.semantics { heading() })
            Text(body)
        }
    }
}
