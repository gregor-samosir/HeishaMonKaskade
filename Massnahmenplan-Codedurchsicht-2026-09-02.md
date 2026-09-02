# Codedurchsicht 2026-09-02: Robustheit und Langzeitstabilität

Zweite Durchsicht der gesamten Firmware (`src/`, Stand 3.19.0) mit der Frage,
was einem Gerät, das an zwei Wärmepumpen monatelang ohne Aufsicht läuft,
gefährlich werden kann. Die erste Durchsicht vom 2026-08-18 hatte Sendepfad,
Telegrammprüfung und Timer-Kette geprüft; die sind seither unverändert und
werden hier nicht erneut aufgerollt. Seit damals sind Notbetrieb (3.12–3.18),
Verbindungswacht (3.13), Hydraulikschritt (3.15) und der reine ESP32-Pfad
(3.16, Core arduino-esp32 3.3.11) dazugekommen. Alle zehn Hosttests liefen
dabei lokal grün.

**Gesamtbild:** Die Firmware ist für ihre Aufgabe ungewöhnlich sauber gebaut.
Alles läuft in einem Task, ohne Interrupts und ohne Nebenläufigkeit; die
Regeln mit Zeitbezug liegen in arduino-freien Headern und sind über den
`millis()`-Überlauf hinweg hosttestbar; die heißen Pfade kommen ohne
Heap-Allokation aus; jede Schleife hat eine Obergrenze; Eingaben vom Broker
und aus der Konfiguration werden geprüft. Es bleiben **ein Befund mit
Handlungsbedarf**, drei Lücken bei der Langzeitbeobachtung und drei Kleinpunkte.

## Vorgehen

Wie beim letzten Mal: Rettungsanker-Tag vor der ersten Änderung, je Maßnahme
ein Branch und eine Versionsnummer, Changelog in `src/version.h` mit Problem,
Nachweis und RAM/Flash-Delta, Abnahme über `test/tablesnap.py`.

| Prio | Maßnahme | Art | Aufwand |
| --- | --- | --- | --- |
| hoch | M1 Karenzfenster wird in `setup()` gestartet, aber erst in `loop()` gebraucht | Fix | eine Zeile |
| mittel | M2 Notbetriebswerte überleben den firmware-eigenen Neustart nicht | Umbau | halber Tag |
| mittel | M3 Keine Langzeit-Telemetrie: Reset-Ursache, Bootzähler, Heap, Uptime | Ausbau | halber Tag |
| mittel | M4 Diagnose geht genau im Störfall verloren | Ausbau | halber Tag |
| niedrig | K1 Kein Watchdog für `loop()` | Absicherung | Stunde, mit Vorbehalt |
| niedrig | K2 Zeitstempel driften und kennen keine Sommerzeit-Umstellung | Fix | Stunde |
| niedrig | K3 `config.json` wird nicht atomar geschrieben, Parse-Fehler löscht WLAN-Zugang | Härtung | Stunde |
| — | Veraltete Zahlen in Kommentaren | Aufräumen | Minuten |

M2 und M3 teilen sich einen Mechanismus (RTC-Speicher, der einen Software-Reset
überlebt) und gehören in eine Version.

---

## M1 — Das Karenzfenster kann abgelaufen sein, bevor der Schwall gelesen wird

**Priorität: hoch. Aufwand: eine Zeile.**

**Problem:** `setupMqtt()` verbindet, abonniert und setzt
`setCommandsIgnoredUntil = millis() + 5000` ([HeishaMon.cpp:353](src/HeishaMon.cpp#L353)).
Der Wiedereinspiel-Schwall des ioBroker-Adapters liegt danach im TCP-Puffer
und wird erst vom ersten `mqtt_client.loop()` gelesen, also erst, wenn
`setup()` fertig ist. Dazwischen steht `setupTime()`
([HeishaMon.cpp:855](src/HeishaMon.cpp#L855)) und wartet bis zu **30 s** auf
NTP. Braucht die Zeitsynchronisation länger als 5 s, ist das Fenster beim
ersten `loop()` bereits zu, und jedes wiedereingespielte Set-Kommando läuft
als frisches Kommando in die Wärmepumpe. Das ist genau der Fall aus 3.6.1
(Vorlauf-Sollwert 55 °C nach jedem Neustart), nur mit einer anderen Ursache.

Das Szenario ist nicht konstruiert: Nach einem Stromausfall im Haus bootet
die Bridge in Sekunden, der Router braucht Minuten für den Internetzugang.
Ist ioBroker schon da, das Internet aber noch nicht, scheitert NTP, der
Schwall wartet 30 s im Puffer und wird dann ausgeführt. Der Keepalive des
Brokers rettet hier nicht: Die Nachrichten sind vor einem etwaigen
Verbindungsabbruch bereits angekommen und werden vor dem Erkennen des
Abbruchs abgearbeitet.

Im Normalfall (NTP in 1–3 s) hält das Fenster, deshalb ist es bisher nicht
aufgefallen.

**Fix, zwei Wege:**

* **A (empfohlen, minimal):** Am Ende von `setup()`, unmittelbar vor
  `Send_Pana_Mainquery_Timer.start()`, das Fenster neu armieren:
  `setCommandsIgnoredUntil = millis() + SUBSCRIBE_GRACE;` mit Kommentar,
  warum es zweimal gesetzt wird. Kein Einfluss auf den Reconnect-Pfad.
* **B (struktureller):** `setupMqtt()` hinter `setupTime()` ziehen, damit die
  Verbindung als Letztes fällt. Dann geht die NTP-Warnung in `setupTime()`
  nicht mehr über MQTT raus und muss auf Serial/Telnet umziehen.

**Nachweis (ohne die produktive Anlage):** Testboard mit Test-Prefix, im
ioBroker unter diesem Prefix ein paar Set-Werte hinterlegen, NTP unerreichbar
machen (Internet am Router trennen oder DNS blocken), Neustart, Telnet
mitschneiden. Vorher: `<SUB> SET…`-Zeilen direkt nach dem Boot. Nachher:
`Verworfen (Wiedereinspielung nach Connect)` und die Bilanzzeile.

---

## M2 — Notbetriebswerte überleben den firmware-eigenen Neustart nicht

**Priorität: mittel. Aufwand: halber Tag, inkl. Hosttest.**

**Problem:** Die Kurvenwerte für den Notbetrieb liegen nur im RAM
(Owner-Entscheid 2026-08-20, [notbetrieb.h:345](src/notbetrieb.h#L345)). Die
Begründung war: Startet der ESP neu, während der Broker weg ist, liegt ein
Stromausfall vor, und der läuft ohne Notbetrieb. Diese Annahme trägt nicht
mehr, weil die Firmware selbst neu startet:

* WLAN-Watchdog nach 5 min ohne WLAN ([HeishaMon.cpp:520](src/HeishaMon.cpp#L520)),
  eingeführt in 3.5.0, also vor dem Notbetrieb,
* `/reboot` und jedes OTA.

Der ungünstige Fall: Der Server im Keller ist tot (Broker weg, das ist der
Notbetriebsfall), und der WLAN-Router startet neu oder bekommt ein Update, das
länger als 5 min dauert. Die Bridge startet neu, die Werte sind weg, der Knopf
zeigt „Nicht bereit – es fehlen Werte" – genau dann, wenn jemand ihn braucht.
Plan B am Bedienfeld bleibt, aber der Knopf war für genau diesen Moment gebaut.

**Fix:** Die gehaltenen Werte zusätzlich in einer `RTC_NOINIT_ATTR`-Struktur
spiegeln (Magic, Rolle, Werte, `gesetzt`-Maske, Prüfsumme). Der RTC-Speicher
überlebt `ESP.restart()` und den Watchdog-Reset, nicht aber das Stromlosmachen.
Das ist genau die Trennlinie des Owner-Entscheids: kein Flash-Schreibzugriff,
keine Datei, nach echtem Stromausfall leer. In `notbetrieb_init()` wird der
Spiegel nur übernommen, wenn Magic und Prüfsumme stimmen, und jeder Wert läuft
dabei erneut durch `set_command_range()`. Die Regel „gültiger Spiegel ja/nein"
gehört als arduino-freie Funktion nach `notbetrieb.h`, damit der Hosttest sie
mit Bitkippern und falscher Rolle prüfen kann.

Bewusst nicht: die Werte bei Broker-Ausfall vom WLAN-Neustart auszunehmen. Der
Neustart ist der einzige Weg zurück ins WLAN, und ohne WLAN ist auch die
Notbetriebsseite unerreichbar.

**Nachweis:** Testboard, Werte einspielen, Broker abschalten, `/reboot` oder
WLAN-Watchdog abwarten, danach muss die Seite den Knopf zeigen und
`Notbetrieb einsatzbereit` darf im Log nicht erneut erscheinen (die Werte
waren ja nie weg). Gegenprobe: stromlos machen, danach „Nicht bereit".

---

## M3 — Keine Langzeit-Telemetrie

**Priorität: mittel. Aufwand: halber Tag, Wächter auf ioBroker-Seite extra.**

**Problem:** Über Monate sind zwei Fehlerbilder ohne Telemetrie unsichtbar:

* **Unbemerkte Neustarts.** Ein Watchdog-Neustart, ein Brownout oder eine
  Panic hinterlassen keine Spur. `WLAN war X s weg` wird nur gemeldet, wenn
  das Gerät die Zeit ohne Neustart überstanden hat; nach einem Neustart weiß
  niemand, dass es einen gab. Ein Gerät, das alle paar Tage neu startet, sähe
  von außen gesund aus (LWT kommt binnen Sekunden zurück).
* **Schleichender Heap-Verlust oder Fragmentierung.** `getFreeMemory()`
  ([HeishaMon.cpp:209](src/HeishaMon.cpp#L209)) ist nur über Telnet `M`
  abrufbar und liefert einen Prozentwert relativ zum Boot. Wer sich nicht
  per Telnet auflegt, sieht nichts. Die Webseiten bauen ihre Antworten aus
  `String`-Objekten; das ist bei seltenen Aufrufen unkritisch, aber ohne Messung
  bleibt es eine Annahme.

**Fix:** Neuer Zweig `<prefix>/info/` mit vier Topics, retained, dokumentiert
in `MQTT-Topics.md`:

| Topic | Inhalt | Wann |
| --- | --- | --- |
| `info/boot` | `esp_reset_reason()` als Text, Bootzähler, Version | einmal nach jedem Connect |
| `info/uptime` | Sekunden seit Boot aus `esp_timer_get_time()` (64 Bit, kein 49-Tage-Überlauf) | mit dem 5-min-Vollupdate |
| `info/heap` | `getFreeHeap`, `getMinFreeHeap`, `getMaxAllocHeap` | mit dem 5-min-Vollupdate |
| `info/stack` | `uxTaskGetStackHighWaterMark(NULL)` des loopTask (8 KB) | mit dem 5-min-Vollupdate |

Der Bootzähler liegt im selben `RTC_NOINIT`-Block wie die Werte aus M2 und
zählt damit Software-Resets; ein Stromausfall setzt ihn auf 1. Zusammen mit
der Reset-Ursache unterscheidet das „Strom weg" von „Firmware hat neu
gestartet". Auf ioBroker-Seite gehört dazu ein Wächter nach dem Muster von
`WP_Befehls_Waechter.js` (Repo `nodered-flows`): Alarm bei Bootzähler > 1 mit
Reset-Ursache ungleich Power-on, und bei fallendem `MinFreeHeap` über Tage.

Der `Send_Pana_Mainquery`-Takt bleibt unberührt, die vier Publishes hängen
am bestehenden Vollupdate in `publish_heatpump_data()`.

---

## M4 — Diagnose geht genau im Störfall verloren

**Priorität: mittel. Aufwand: halber Tag.**

**Problem:** `write_mqtt_log()` ([HeishaMon.cpp:163](src/HeishaMon.cpp#L163))
publiziert ins MQTT-Log und sonst nirgends. Alle Zeilen des Notbetriebs
(`Notbetrieb Schritt 3/10`, `ROT: … kam nicht zurück`, die Hydraulik-Antwort
mit HTTP-Code) laufen über diese Funktion. Im eigentlichen Notbetriebsfall ist
der Broker weg, das Publish scheitert still, und nach dem 15-min-Anzeigeverfall
existiert keine Spur mehr, warum ein Lauf ROT war. Dasselbe gilt für die
Meldungen rund um einen WLAN-Ausfall. K4 der ersten Durchsicht hatte den
Log-Pfad als „nur Diagnose" eingestuft; mit dem Notbetrieb ist er zur einzigen
Nachweisquelle eines sicherheitsrelevanten Vorgangs geworden.

**Fix, zwei Teile:**

1. **Fallback bei Misserfolg:** Schlägt `publish()` fehl oder ist der Client
   getrennt, geht die Zeile stattdessen mit Zeitstempel an `TelnetStream`. Das
   ist die Variante, die K4 bereits als sinnvollsten Weg benannt hatte.
2. **Logring im RAM:** Die letzten 32 Zeilen à 128 Byte (4 KB, statisch) in
   einem Ringpuffer, ausgegeben über eine Route `/log` ohne Anmeldung, analog
   zu `/notbetrieb/status`. Damit sieht die Familie im Ernstfall auf der
   Notbetriebsseite über einen Link, was der letzte Lauf gemeldet hat, und die
   Nachschau am nächsten Tag hat noch etwas zum Lesen. Der Ring überlebt
   keinen Neustart; mit M3 ist der Neustart selbst aber sichtbar.

---

## Kleinpunkte

**K1 — Kein Watchdog für `loop()`.** Der Task-Watchdog des Cores ist aktiv
(5 s, Panic), überwacht aber nur den Idle-Task von CPU 0
(`CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0=y`, CPU 1 nicht). Der loopTask ist
nicht angemeldet. Bleibt `loop()` in einer Bibliothek hängen, sendet die
Firmware keine Keepalives mehr, der Broker setzt nach ~22 s das LWT auf
`Offline`, und der Wächter meldet es. Erkannt wird der Fall also, geheilt
nicht: Jemand muss den Stecker ziehen. Ein eigener Hänger im Code wurde nicht
gefunden, jede Schleife ist begrenzt.

Abhilfe: nach `setup()` `esp_task_wdt_reconfigure()` auf **120 s** und
`enableLoopWDT()` (beides im Core 3.x vorhanden). Die Zeit muss über allen
legitimen Blockaden liegen, siehe Tabelle unten, und ArduinoOTA sowie der
HTTP-Updater müssen den Watchdog in ihrem Fortschritts-Callback füttern, sonst
bricht ein Upload nach zwei Minuten ab. Wegen dieser Nebenwirkungen niedrig
priorisiert; wer es umsetzt, testet OTA auf dem Testboard zuerst.

**K2 — Zeitstempel driften.** `setupTime()` setzt TimeLib einmal beim Boot;
danach läuft `now()` frei auf `millis()`. Über Monate driftet das um Minuten,
und die Sommerzeit-Umstellung im Oktober erreicht die Log-Zeitstempel erst
mit dem nächsten Neustart. Die Systemuhr (`time()`) wird von SNTP im
Hintergrund weiter nachgeführt und kennt die Zeitzone. Abhilfe: in
`write_mqtt_log`, `write_telnet_log` und `handle_telnetstream` `time()` plus
`localtime_r()` statt der TimeLib-Funktionen benutzen; die Abhängigkeit
`paulstoffregen/Time` kann dann entfallen. Betrifft nur die Lesbarkeit von
Mitschnitten, nicht die Regelung.

**K3 — `config.json` nicht atomar, harte Reaktion auf Parse-Fehler.** Die
Datei wird mit `open("w")` überschrieben ([webfunctions.cpp:648](src/webfunctions.cpp#L648)).
Ein Stromausfall in diesen Millisekunden hinterlässt eine leere oder halbe
Datei; beim nächsten Boot greift dann `wifiManager.resetSettings()`
([webfunctions.cpp:174](src/webfunctions.cpp#L174)) und löscht auch die
WLAN-Zugangsdaten. Das Gerät landet im Setup-Hotspot, die Wärmepumpe wird
nicht mehr abgefragt, bis jemand vor Ort ist. Die Wahrscheinlichkeit ist
winzig (geschrieben wird nur beim Speichern der Settings), die Folge aber
ein Handeinsatz an der Anlage. Abhilfe: nach `config.tmp` schreiben und
`LittleFS.rename()`; bei Parse-Fehler die Vorgaben behalten und nur loggen,
statt den WLAN-Zugang zu verwerfen.

**Veraltete Zahlen in Kommentaren** (Tabellenlänge 92 → 99, Gesamtdeckel
180 s → 200 s): [decode.h:97](src/decode.h#L97),
[decode.cpp:547](src/decode.cpp#L547), [notbetrieb.cpp:82](src/notbetrieb.cpp#L82),
[notbetrieb.cpp:695](src/notbetrieb.cpp#L695), [notbetrieb.h:92](src/notbetrieb.h#L92).
Der Changelog in `version.h` bleibt, wie er ist.

---

## Blockadezeiten in `loop()` – gemessen an dem, was der Code zulässt

Die Timer laufen alle per Polling aus `loop()`. Jede Blockade verschiebt den
Abfragezyklus, verliert aber keine Daten: Der UART-Puffer (256 Byte) fasst
das eine Antworttelegramm (203 Byte), das pro Abfrage unterwegs sein kann,
und die Lesereihenfolge (`Read_Pana_Data_Timer` vor `Timeout_Serial_Timer`)
liest es nach der Blockade als Erstes. Zwei Grenzen sind trotzdem zu kennen:
ab ~15 s ohne `mqtt_client.loop()` fehlt dem Broker der Keepalive, es folgt
Reconnect samt Wiedereinspielung; ab 120 s würde der Watchdog aus K1 greifen.

| Quelle | Dauer | Wann |
| --- | --- | --- |
| MQTT-Reconnect: TCP-Connect 3 s (`NetworkClient`-Vorgabe) + CONNACK 2 s | ≤ 5 s | je Versuch, Backoff 5–60 s |
| dito mit DNS, falls `mqtt_server` ein Hostname ist | + DNS-Zeit | IP eintragen, dann entfällt es |
| WebServer wartet auf langsamen Client | ≤ 5 s | je Request |
| Hydraulikschritt: zwei HTTP-Requests, je Connect 5 s + Antwort 5 s + Rumpf 5 s | ≤ 30 s im Fehlerfall, ~0,2 s normal | einmal je Notbetriebslauf |
| OTA-Upload (ArduinoOTA, HTTP-Updater) | 15–60 s | nur beim Flashen |
| `setupTime()` NTP | ≤ 30 s | nur im Boot, siehe M1 |

Der Hydraulikschritt kann mit 30 s über dem Keepalive liegen. Folge ist nur
ein Reconnect, der Notbetrieb selbst ist davon unabhängig, weil seine Werte
im RAM liegen und die Karenzausnahme greift. Kein Handlungsbedarf,
festgehalten damit es beim nächsten Mal niemand neu herleiten muss.

---

## Ausdrücklich geprüft und in Ordnung

* **Ein Task, keine Nebenläufigkeit.** WebServer, PubSubClient, OTA, Telnet
  und alle vier Ticker laufen aus `loop()`; die Ticker pollen `millis()`,
  keine ISR. Es gibt keinen geteilten Zustand zwischen Tasks.
* **`millis()`-Überlauf** an allen Vergleichsstellen, auch den seit 3.13
  neuen (Verbindungswacht mit fortgeschriebener Dauer und 30-Tage-Deckel,
  Notbetrieb-Zeitregeln, Hydraulik-Lesefrist, Bilanz des Karenzfensters).
* **Publish aus dem MQTT-Callback**: In `build_heatpump_command()` und
  `notbetrieb_mqtt_annehmen()` wird jede Meldung erst mit `snprintf`
  formatiert und dann publiziert; `topic` wird nach dem Publish nirgends
  mehr gelesen. Die Puffer-Falle aus 3.13.0 (`0Q`) ist damit auch dort
  ausgeschlossen. `msg` ist ohnehin eine Kopie.
* **Timer-Kette mit Notbetrieb:** Die Schritte gehen durch
  `build_heatpump_command()` und `register_new_command()`, ohne den
  Abfragetimer anzuhalten. Ein zeitgleich feuernder Abfragetimer trifft auf
  `newcommand == true` und tut nichts; ein laufendes Lesefenster verschiebt
  das Senden höchstens vier Runden. Kein Pfad ohne Ausgang.
* **Serielle Strecke:** Antwortquote 100 % in den Referenzläufen
  (`test/README.md`, „Antwortquote messen"); ein einmaliges Lesen nach
  500 ms reicht bei 232 ms Telegrammdauer. Kein Grund, an der Zeitkette zu
  drehen.
* **Puffer und Längen:** `mqtt_callback`-Kopie mit VLA ≤ 256, alle
  `snprintf` mit Größe, `%.32s`/`%.64s` bei Fremdtext, `strlcpy` in der
  Konfiguration, Hydraulik-URL 128 Byte gegen 40 Byte Adresse plus
  längstes Kommando. Stack des loopTask 8 KB, tiefste Verschachtelung ist
  der Hydraulikschritt mit `HTTPClient` auf dem Stack; unkritisch, aber
  genau deshalb der Stack-Wasserstand in M3.
* **UART-Überlauf** endet im Telegrammcheck, nicht im Absturz;
  `flush_serial_input()` ist begrenzt.
* **USB-Konsole ohne Host** blockiert nicht (HWCDC prüft `isConnected()`
  vor dem Schreiben, Core 3.3.11).
* **Konfigurationsfelder** kommen über `loadConfigValue()` mit Vorgabewert
  für fehlende Schlüssel, `mqtt_port` wird beim Boot geprüft.
* **Flash-Verschleiß:** LittleFS wird nur beim Speichern der Settings
  beschrieben, nie im Betrieb. Die RTC-Spiegel aus M2/M3 ändern daran nichts.
