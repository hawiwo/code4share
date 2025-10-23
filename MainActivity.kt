package com.example.stempeluhr

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.view.WindowCompat
import com.google.gson.Gson
import com.google.gson.reflect.TypeToken
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import java.io.File
import java.util.*
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.ui.graphics.Color
import java.text.SimpleDateFormat
import com.example.feiertage.holeFeiertageBW

// ----------------------------------------------------------
// Datenklassen
// ----------------------------------------------------------
data class Stempel(val typ: String, val zeit: String, val homeoffice: Boolean = false)

data class Einstellungen(
    var startwertMinuten: Int = 0,
    var standDatum: String = "",
    var homeofficeAktiv: Boolean = false
)

data class Urlaubseintrag(
    val von: String,
    val bis: String,
    val tage: Int
)

// ----------------------------------------------------------
// Hauptaktivität
// ----------------------------------------------------------
class MainActivity : ComponentActivity() {
    /* Logcat debug
        override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        WindowCompat.setDecorFitsSystemWindows(window, true)

        // 👇 Testlauf
        testBerechnung(this)

        setContent { StempeluhrApp() }
    }
    */

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        WindowCompat.setDecorFitsSystemWindows(window, true)
        setContent { StempeluhrApp() }
    }
}

// ----------------------------------------------------------
// Haupt-App-Struktur
// ----------------------------------------------------------
@Composable
fun StempeluhrApp() {
    var zeigeEinstellungen by remember { mutableStateOf(false) }
    if (zeigeEinstellungen) {
        EinstellungenScreen(onClose = { zeigeEinstellungen = false })
    } else {
        HauptScreen(onOpenSettings = { zeigeEinstellungen = true })
    }
}

// ----------------------------------------------------------
// Hauptansicht (Zeiterfassung)
// ----------------------------------------------------------
@Composable
fun AbschnittTitel(text: String) {
    Text(
        text,
        fontSize = 18.sp,
        fontWeight = FontWeight.Bold,
        color = MaterialTheme.colorScheme.primary
    )
}
@Composable
fun HauptScreen(onOpenSettings: () -> Unit) {
    val context = androidx.compose.ui.platform.LocalContext.current
    val gson = remember { Gson() }
    val logFile = remember { File(context.filesDir, "stempel.json") }
    val settingsFile = remember { File(context.filesDir, "settings.json") }
    val urlaubFile = remember { File(context.filesDir, "urlaub.json") }
    val stempelListe = remember { mutableStateListOf<Stempel>() }
    val format = remember { SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.getDefault()) }

    var statusText by remember { mutableStateOf("Noch nicht eingestempelt") }
    var istEingestempelt by remember { mutableStateOf(false) }
    var eingestempeltSeit by remember { mutableStateOf<Date?>(null) }
    var homeofficeAktiv by remember { mutableStateOf(false) }

    var arbeitsdauerHeute by remember { mutableStateOf("") }
    var arbeitsdauerWoche by remember { mutableStateOf("") }
    var arbeitsdauerMonat by remember { mutableStateOf("") }
    var arbeitsdauerJahr by remember { mutableStateOf("") }
    var ueberstundenText by remember { mutableStateOf("") }
    var startwertAnzeige by remember { mutableStateOf("") }
    var standDatumAnzeige by remember { mutableStateOf("") }

    // Urlaub
    var urlaubGesamt by remember { mutableStateOf(30) }
    var urlaubGenommen by remember { mutableStateOf(0) }
    var urlaubVerbleibend by remember { mutableStateOf(urlaubGesamt) }

    // Homeoffice-Status laden
    LaunchedEffect(Unit) {
        if (settingsFile.exists()) {
            try {
                val jsonText = settingsFile.readText()
                val map = Gson().fromJson<MutableMap<String, Any>>(
                    jsonText,
                    object : TypeToken<MutableMap<String, Any>>() {}.type
                )
                homeofficeAktiv = (map["homeofficeAktiv"] as? Boolean) ?: false
            } catch (_: Exception) {
            }
        }
    }

    fun speichereHomeofficeStatus(aktiv: Boolean) {
        try {
            val jsonText = if (settingsFile.exists()) settingsFile.readText() else "{}"
            val map = Gson().fromJson<MutableMap<String, Any>>(
                jsonText,
                object : TypeToken<MutableMap<String, Any>>() {}.type
            ) ?: mutableMapOf()
            map["homeofficeAktiv"] = aktiv
            settingsFile.writeText(Gson().toJson(map))
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    // JSON laden
    LaunchedEffect(Unit) {
        try {
            if (logFile.exists()) {
                val text = logFile.readText()
                val type = object : TypeToken<List<Stempel>>() {}.type
                val geleseneListe: List<Stempel> =
                    if (text.isNotBlank()) try {
                        Gson().fromJson(text, type) ?: emptyList()
                    } catch (_: Exception) {
                        emptyList()
                    } else emptyList()
                stempelListe.clear()
                stempelListe.addAll(geleseneListe)
            }

            if (urlaubFile.exists()) {
                try {
                    val json = urlaubFile.readText()
                    val type = object : TypeToken<List<Urlaubseintrag>>() {}.type
                    val liste: List<Urlaubseintrag> = Gson().fromJson(json, type)
                    urlaubGenommen = liste.sumOf { it.tage }
                    urlaubVerbleibend = urlaubGesamt - urlaubGenommen
                } catch (_: Exception) {
                }
            }

            if (stempelListe.isNotEmpty()) {
                val letzter = stempelListe.last()
                if (letzter.typ == "Start") {
                    istEingestempelt = true
                    eingestempeltSeit = parseDateFlexible(letzter.zeit)
                    statusText = "Eingestempelt seit ${letzter.zeit.substring(11)}"
                } else {
                    istEingestempelt = false
                    statusText = "Ausgestempelt"
                }
            }
        } catch (e: Exception) {
            e.printStackTrace()
            logFile.writeText("[]")
            stempelListe.clear()
        }
    }

    // Live-Aktualisierung
    LaunchedEffect(istEingestempelt, eingestempeltSeit, stempelListe.size) {
        while (isActive) {
            try {
                val zeiten =
                    berechneAlleZeiten(stempelListe, eingestempeltSeit, istEingestempelt, context)
                arbeitsdauerHeute = zeiten.heute
                arbeitsdauerWoche = zeiten.woche
                arbeitsdauerMonat = zeiten.monat
                arbeitsdauerJahr = zeiten.jahr
                startwertAnzeige = zeiten.startwert
                standDatumAnzeige = zeiten.standDatum
                ueberstundenText = zeiten.ueberstunden
            } catch (e: Exception) {
                e.printStackTrace()
            }
            delay(60_000)
        }
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .statusBarsPadding()
            .padding(24.dp),
        verticalArrangement = Arrangement.SpaceBetween
    ) {
        Column {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(bottom = 8.dp),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Column {
                    Text(
                        "Stempeluhr",
                        fontSize = 28.sp,
                        fontWeight = FontWeight.Bold
                    )

                    Row(
                        verticalAlignment = Alignment.CenterVertically,
                        modifier = Modifier.padding(top = 24.dp)
                    ) {
                        Checkbox(
                            checked = homeofficeAktiv,
                            onCheckedChange = {
                                homeofficeAktiv = it
                                speichereHomeofficeStatus(it)
                            },
                            modifier = Modifier
                                .size(24.dp)
                                .offset(y = (-2).dp)
                        )
                        Spacer(Modifier.width(8.dp))
                        Text("Homeoffice", fontSize = 18.sp)
                    }
                }

                IconButton(onClick = onOpenSettings) {
                    Icon(Icons.Default.Settings, contentDescription = "Einstellungen")
                }
            }


            Spacer(Modifier.height(12.dp))
            Text(statusText, fontSize = 20.sp)
            Spacer(Modifier.height(16.dp))

            if (arbeitsdauerHeute.isNotEmpty()) {
                Text("Heute: $arbeitsdauerHeute", fontSize = 18.sp)
                Text("Diese Woche: $arbeitsdauerWoche", fontSize = 18.sp)
                Text("Diesen Monat: $arbeitsdauerMonat", fontSize = 18.sp)
                Text("Dieses Jahr: $arbeitsdauerJahr", fontSize = 18.sp)

// Fortschrittsanzeigen für Tag und Woche
                val regex = Regex("(\\d+)h\\s*(\\d+)?min?")

                fun parseMinutes(text: String): Int {
                    val m = regex.find(text)
                    val h = m?.groupValues?.get(1)?.toIntOrNull() ?: 0
                    val min = m?.groupValues?.get(2)?.toIntOrNull() ?: 0
                    return h * 60 + min
                }

                val minutenHeute = parseMinutes(arbeitsdauerHeute)
                val minutenWoche = parseMinutes(arbeitsdauerWoche)

                val fortschrittHeute = minutenHeute / 480f
                val fortschrittWoche = minutenWoche / 2400f

                val progressHeute = fortschrittHeute % 1f
                val progressWoche = fortschrittWoche % 1f

                val animatedHeute = animateFloatAsState(
                    targetValue = progressHeute,
                    animationSpec = tween(durationMillis = 800)
                ).value
                val animatedWoche = animateFloatAsState(
                    targetValue = progressWoche,
                    animationSpec = tween(durationMillis = 800)
                ).value

                @Composable
                fun farbe(progress: Float): Color {
                    return if (progress <= 1f) {
                        androidx.compose.ui.graphics.lerp(Color.Red, Color(0f, 0.6f, 0f), progress)
                    } else {
                        Color(0f, 0.6f, 0f)
                    }
                }

                Spacer(Modifier.height(16.dp))

                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceEvenly,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    // --- Tagesfortschritt ---
                    Column(horizontalAlignment = Alignment.CenterHorizontally) {
                        Box(contentAlignment = Alignment.Center) {
                            CircularProgressIndicator(
                                progress = animatedHeute,
                                strokeWidth = 10.dp,
                                color = farbe(fortschrittHeute),
                                modifier = Modifier.size(100.dp)
                            )
                            Text(
                                text = String.format("%.0f%%", fortschrittHeute * 100),
                                fontSize = 18.sp,
                                fontWeight = FontWeight.Bold,
                                color = farbe(fortschrittHeute)
                            )
                        }
                        Spacer(Modifier.height(6.dp))
                        Text(
                            "Heute: ${arbeitsdauerHeute.ifBlank { "0h" }} / 8h",
                            fontSize = 14.sp,
                            color = MaterialTheme.colorScheme.secondary
                        )
                    }

                    // --- Wochenfortschritt ---
                    Column(horizontalAlignment = Alignment.CenterHorizontally) {
                        Box(contentAlignment = Alignment.Center) {
                            CircularProgressIndicator(
                                progress = animatedWoche,
                                strokeWidth = 10.dp,
                                color = farbe(fortschrittWoche),
                                modifier = Modifier.size(100.dp)
                            )
                            Text(
                                text = String.format("%.0f%%", fortschrittWoche * 100),
                                fontSize = 18.sp,
                                fontWeight = FontWeight.Bold,
                                color = farbe(fortschrittWoche)
                            )
                        }
                        Spacer(Modifier.height(6.dp))
                        Text(
                            "Woche: ${arbeitsdauerWoche.ifBlank { "0h" }} / 40h",
                            fontSize = 14.sp,
                            color = MaterialTheme.colorScheme.secondary
                        )
                    }
                }

                Spacer(Modifier.height(20.dp))


                if (ueberstundenText.isNotEmpty()) {
                    val color = if (ueberstundenText.startsWith("-"))
                        MaterialTheme.colorScheme.error
                    else
                        MaterialTheme.colorScheme.primary
                    val label =
                        if (ueberstundenText.startsWith("-")) "Minusstunden" else "Überstunden"
                    Text(
                        "$label: $ueberstundenText",
                        fontSize = 18.sp,
                        fontWeight = FontWeight.Bold,
                        color = color
                    )
                }

                if (startwertAnzeige.isNotEmpty()) {
                    val datumText =
                        if (standDatumAnzeige.isNotEmpty()) " ($standDatumAnzeige)" else ""
                    Text(
                        "(inkl. Startwert $startwertAnzeige$datumText)",
                        fontSize = 16.sp,
                        color = MaterialTheme.colorScheme.secondary
                    )
                }

                Spacer(Modifier.height(16.dp))
                AbschnittTitel("Urlaub:")
                Text("Genommen: $urlaubGenommen / $urlaubGesamt Tage", fontSize = 16.sp)
                Text("Verbleibend: $urlaubVerbleibend Tage", fontSize = 16.sp, color = MaterialTheme.colorScheme.secondary)

                Spacer(Modifier.height(20.dp))
                AbschnittTitel("Nächste freie Tage:")

                val aktuelleFeiertage = remember {
                    holeFeiertageBW().sortedBy { it.date }
                }
                val heute = java.time.LocalDate.now()
                val naechsteFeiertage = remember {
                    holeFeiertageBW()
                        .filter { it.date.isAfter(java.time.LocalDate.now()) }
                        .take(3)
                }
                val formatter = java.time.format.DateTimeFormatter.ofPattern("EEE, dd.MM.yyyy", Locale.GERMAN)

                naechsteFeiertage.forEach {
                    val text = "${it.date.format(formatter)} – ${it.description}"
                    Text(
                        text = text,
                        fontSize = 16.sp,
                        color = MaterialTheme.colorScheme.secondary
                    )
                }
            }
        }

        Column {
            Button(
                onClick = {
                    if (!istEingestempelt) {
                        val jetzt = Date()
                        addStempel("Start", stempelListe, gson, logFile, homeofficeAktiv)
                        eingestempeltSeit = jetzt
                        statusText =
                            "Eingestempelt seit ${format.format(jetzt).substring(11)}"
                        istEingestempelt = true
                    }
                },
                modifier = Modifier.fillMaxWidth().height(80.dp),
                enabled = !istEingestempelt
            ) { Text("Start", fontSize = 22.sp) }

            Spacer(Modifier.height(24.dp))

            Button(
                onClick = {
                    if (istEingestempelt) {
                        addStempel("Ende", stempelListe, gson, logFile, homeofficeAktiv)
                        statusText = "Ausgestempelt"
                        istEingestempelt = false
                        eingestempeltSeit = null
                    }
                },
                modifier = Modifier.fillMaxWidth().height(80.dp),
                enabled = istEingestempelt
            ) { Text("Ende", fontSize = 22.sp) }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun EinstellungenScreen(onClose: () -> Unit) {
    val context = androidx.compose.ui.platform.LocalContext.current
    val gson = remember { Gson() }
    val settingsFile = remember { File(context.filesDir, "settings.json") }
    val stempelFile = remember { File(context.filesDir, "stempel.json") }
    val urlaubFile = remember { File(context.filesDir, "urlaub.json") }

    val primaryColor = MaterialTheme.colorScheme.primary
    val errorColor = MaterialTheme.colorScheme.error

    var settingsText by remember {
        mutableStateOf(
            if (settingsFile.exists()) settingsFile.readText()
            else "{\n  \"startwertMinuten\": 0,\n  \"standDatum\": \"\",\n  \"homeofficeAktiv\": false\n}"
        )
    }
    var stempelText by remember { mutableStateOf(if (stempelFile.exists()) stempelFile.readText() else "[]") }
    var urlaubText by remember { mutableStateOf(if (urlaubFile.exists()) urlaubFile.readText() else "[]") }

    var statusText by remember { mutableStateOf("") }
    var statusColor by remember { mutableStateOf(primaryColor) }
    var expandedSection by remember { mutableStateOf<String?>(null) }

    val urlaubsliste = remember {
        mutableStateListOf<Urlaubseintrag>().apply {
            if (urlaubFile.exists()) {
                try {
                    val type = object : TypeToken<List<Urlaubseintrag>>() {}.type
                    val daten: List<Urlaubseintrag> = Gson().fromJson(urlaubFile.readText(), type) ?: emptyList()
                    addAll(daten.sortedBy { it.von })
                } catch (_: Exception) {}
            }
        }
    }

    var vonDatum by remember { mutableStateOf("") }
    var bisDatum by remember { mutableStateOf("") }
    var showVonPicker by remember { mutableStateOf(false) }
    var showBisPicker by remember { mutableStateOf(false) }

    val scrollState = rememberScrollState()

    Column(
        modifier = Modifier
            .fillMaxSize()
            .statusBarsPadding()
            .padding(16.dp)
            .verticalScroll(scrollState)
    ) {
        Text("Einstellungen", fontSize = 20.sp, fontWeight = FontWeight.Bold)
        Spacer(Modifier.height(12.dp))

        // JSON-Dateien Sektion (zusammenklappbar)
        Card(
            modifier = Modifier.fillMaxWidth(),
            onClick = { expandedSection = if (expandedSection == "json") null else "json" }
        ) {
            Column(Modifier.padding(12.dp)) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text("JSON-Dateien", fontSize = 16.sp, fontWeight = FontWeight.Medium, modifier = Modifier.weight(1f))
                    Text(if (expandedSection == "json") "▲" else "▼", fontSize = 16.sp)
                }

                if (expandedSection == "json") {
                    Spacer(Modifier.height(8.dp))
                    Text("settings.json", fontSize = 14.sp, fontWeight = FontWeight.Medium)
                    OutlinedTextField(
                        value = settingsText,
                        onValueChange = { settingsText = it },
                        modifier = Modifier.fillMaxWidth().height(80.dp),
                        textStyle = LocalTextStyle.current.copy(fontSize = 12.sp),
                        singleLine = false
                    )

                    Spacer(Modifier.height(8.dp))
                    Text("stempel.json", fontSize = 14.sp, fontWeight = FontWeight.Medium)
                    OutlinedTextField(
                        value = stempelText,
                        onValueChange = { stempelText = it },
                        modifier = Modifier.fillMaxWidth().height(80.dp),
                        textStyle = LocalTextStyle.current.copy(fontSize = 12.sp),
                        singleLine = false
                    )

                    Spacer(Modifier.height(8.dp))
                    Text("urlaub.json", fontSize = 14.sp, fontWeight = FontWeight.Medium)
                    OutlinedTextField(
                        value = urlaubText,
                        onValueChange = { urlaubText = it },
                        modifier = Modifier.fillMaxWidth().height(80.dp),
                        textStyle = LocalTextStyle.current.copy(fontSize = 12.sp),
                        singleLine = false
                    )
                }
            }
        }

        Spacer(Modifier.height(12.dp))

        // Urlaub hinzufügen
        Card(modifier = Modifier.fillMaxWidth()) {
            Column(Modifier.padding(12.dp)) {
                Text("Urlaub hinzufügen", fontSize = 16.sp, fontWeight = FontWeight.Medium)
                Spacer(Modifier.height(8.dp))

                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedButton(onClick = { showVonPicker = true }, modifier = Modifier.weight(1f)) {
                        Text(if (vonDatum.isEmpty()) "Von" else vonDatum, fontSize = 13.sp)
                    }
                    OutlinedButton(onClick = { showBisPicker = true }, modifier = Modifier.weight(1f)) {
                        Text(if (bisDatum.isEmpty()) "Bis" else bisDatum, fontSize = 13.sp)
                    }
                }

                Spacer(Modifier.height(8.dp))
                Button(
                    onClick = {
                        try {
                            val format = SimpleDateFormat("yyyy-MM-dd", Locale.getDefault())
                            val start = format.parse(vonDatum)
                            val ende = format.parse(bisDatum)
                            if (start != null && ende != null && !ende.before(start)) {
                                var tage = 0
                                val cal = Calendar.getInstance()
                                cal.time = start
                                while (!cal.time.after(ende)) {
                                    val tag = cal.get(Calendar.DAY_OF_WEEK)
                                    if (tag in Calendar.MONDAY..Calendar.FRIDAY) tage++
                                    cal.add(Calendar.DAY_OF_YEAR, 1)
                                }

                                val neuerUrlaub = Urlaubseintrag(vonDatum, bisDatum, tage)
                                urlaubsliste.add(neuerUrlaub)
                                val neuesJson = Gson().toJson(urlaubsliste.sortedBy { it.von })
                                urlaubText = neuesJson
                                urlaubFile.writeText(neuesJson)

                                statusText = "✔ $tage Tage hinzugefügt"
                                statusColor = primaryColor
                                vonDatum = ""
                                bisDatum = ""
                            } else {
                                statusText = "❌ Ungültiges Datum"
                                statusColor = errorColor
                            }
                        } catch (e: Exception) {
                            statusText = "❌ Fehler: ${e.message}"
                            statusColor = errorColor
                        }
                    },
                    modifier = Modifier.fillMaxWidth().height(44.dp)
                ) { Text("Hinzufügen") }
            }
        }

        // Date Pickers
        if (showVonPicker) {
            val state = rememberDatePickerState()
            DatePickerDialog(
                onDismissRequest = { showVonPicker = false },
                confirmButton = {
                    TextButton(onClick = {
                        vonDatum = formatDateYMD(state.selectedDateMillis)
                        showVonPicker = false
                    }) { Text("OK") }
                },
                dismissButton = { TextButton(onClick = { showVonPicker = false }) { Text("Abbrechen") } }
            ) { DatePicker(state = state) }
        }

        if (showBisPicker) {
            val state = rememberDatePickerState()
            DatePickerDialog(
                onDismissRequest = { showBisPicker = false },
                confirmButton = {
                    TextButton(onClick = {
                        bisDatum = formatDateYMD(state.selectedDateMillis)
                        showBisPicker = false
                    }) { Text("OK") }
                },
                dismissButton = { TextButton(onClick = { showBisPicker = false }) { Text("Abbrechen") } }
            ) { DatePicker(state = state) }
        }

        Spacer(Modifier.height(12.dp))

        // Urlaubsliste
        Card(modifier = Modifier.fillMaxWidth()) {
            Column(Modifier.padding(12.dp)) {
                Text("Urlaubsliste (${urlaubsliste.size})", fontSize = 16.sp, fontWeight = FontWeight.Medium)
                Spacer(Modifier.height(8.dp))

                if (urlaubsliste.isEmpty()) {
                    Text("Keine Einträge", color = MaterialTheme.colorScheme.secondary, fontSize = 14.sp)
                } else {
                    urlaubsliste.sortedBy { it.von }.forEach {
                        Text("${it.von} – ${it.bis} · ${it.tage}T", fontSize = 14.sp)
                        Spacer(Modifier.height(4.dp))
                    }
                }
            }
        }

        Spacer(Modifier.height(16.dp))

        // Aktionsbuttons
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp), modifier = Modifier.fillMaxWidth()) {
            Button(
                onClick = {
                    try {
                        gson.fromJson(settingsText, Einstellungen::class.java)
                        val typeStempel = object : TypeToken<List<Stempel>>() {}.type
                        gson.fromJson<List<Stempel>>(stempelText, typeStempel)
                        val typeUrlaub = object : TypeToken<List<Urlaubseintrag>>() {}.type
                        gson.fromJson<List<Urlaubseintrag>>(urlaubText, typeUrlaub)

                        settingsFile.writeText(settingsText)
                        stempelFile.writeText(stempelText)
                        urlaubFile.writeText(urlaubText)

                        statusText = "✔ Gespeichert"
                        statusColor = primaryColor
                    } catch (e: Exception) {
                        statusText = "❌ Ungültiges JSON"
                        statusColor = errorColor
                    }
                },
                modifier = Modifier.weight(1f).height(48.dp)
            ) { Text("Speichern") }

            OutlinedButton(onClick = onClose, modifier = Modifier.weight(1f).height(48.dp)) {
                Text("Schließen")
            }
        }

        Spacer(Modifier.height(8.dp))

        OutlinedButton(
            onClick = {
                val meldung = backupDateien(context.applicationContext)
                statusText = meldung
                statusColor = primaryColor
            },
            modifier = Modifier.fillMaxWidth().height(48.dp)
        ) { Text("Backup erstellen") }

        if (statusText.isNotEmpty()) {
            Spacer(Modifier.height(8.dp))
            Text(statusText, color = statusColor, fontSize = 14.sp)
        }

        Spacer(Modifier.height(16.dp))
    }
}