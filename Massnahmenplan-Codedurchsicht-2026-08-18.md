# Maßnahmenplan zur Codedurchsicht vom 2026-08-18

Ergebnis der Durchsicht der gesamten Firmware (`src/`), der PlatformIO-Konfiguration
und der Testwerkzeuge, mit Fokus auf dem ESP32-Teil, den Eigenheiten der
Panasonic-Schnittstelle und dem Zusammenspiel mit der Node-RED-Steuerung.
Die vier Hosttests (`merge_test`, `telegramm_test`, `sendwindow_test`,
`byte110_test`) liefen dabei lokal grün.

**Gesamtbild:** Sendepfad, Telegrammprüfung, Bitmasken-Merge und die
überlaufsichere Zeitrechnung sind belastbar — die Timer-Kette
(Mainquery → Sammelfenster → Senden → Lesen/Timeout → Mainquery) terminiert in
jedem geprüften Pfad. Es bleiben drei Befunde und vier Kleinpunkte.

## Vorgehen

Nach Projektkonvention ([Rettungsanker](README.md)):

1. Vor der Umsetzung Git-Tag auf den Ausgangsstand setzen (Rückfallebene).
2. Je Maßnahme ein eigener Branch, eigene Versionsnummer, Changelog-Eintrag in
   `src/version.h` mit Problem, Nachweis und RAM/Flash-Delta (alle 10 Envs bauen).
3. Abnahme nach dem Flashen wie üblich: Baseline über `/tablerefresh` ziehen und
   zeilenweise gegen den Stand nach dem OTA halten.

Vorgeschlagene Bündelung:

| Version | Inhalt | Art | Stand |
| --- | --- | --- | --- |
| 3.8.1 | Maßnahme 1 + K1 (Zugangswege, kein Protokoll-/Topic-Einfluss) | Fix | **erledigt 2026-08-18** — umgesetzt, am Gerät abgenommen, auf beiden Stufen ausgerollt |
| 3.8.2 | Maßnahme 2 (retained `Online`) + K3 (Publish-Wiederholung) | Fix | **erledigt 2026-08-19** — gebaut und getestet, Abnahme am Gerät steht aus |
| 3.9.0 | Maßnahme 3 (Weg B + `nullptr`-Abschluss) + Testausbau | Umbau | **erledigt 2026-08-19** — gebaut und getestet, Abnahme am Gerät steht aus |
| — | K2 (Beobachtungsauftrag), K4 (bekannte Grenze) | Entscheid | **entschieden 2026-08-19** — keine Codeänderung |
| — | CI-Trigger nur noch `main` | Aufräumen | **erledigt 2026-08-19** |

Abweichung von der ursprünglich vorgeschlagenen Bündelung: In 3.8.1 sind
Maßnahme 1 und K1 zusammengefasst, weil beide denselben Punkt betreffen — einen
Zugang, der ohne Anmeldung ans Gerät führt. Maßnahme 2 blieb draußen, damit die
Version genau eine Sache belegt und der Nachweis für das Retain-Flag nicht auf
einen Broker warten muss, den es hier noch nicht gibt.

In 3.8.2 sind dann Maßnahme 2 und K3 zusammengefasst — aus demselben Grund wie
bei 3.8.1: Es ist derselbe Fehler in zwei Ausprägungen, ein neu verbundener
Abonnent bekommt vom Broker einen Zustand, den das Gerät längst überholt hat.
Der Nachweis am Broker ist dabei bewusst vom Fix entkoppelt (siehe Maßnahme 2).

**Entscheidungsrunde am 2026-08-19.** Die offenen Punkte wurden einzeln
durchgegangen und entschieden: Maßnahme 2 und Maßnahme 3 umsetzen, K3 entgegen
dem ursprünglichen Vorschlag ebenfalls umsetzen, K2 und K4 ohne Codeänderung
schließen. Was jeweils entschieden wurde, steht beim Punkt.

---

## Maßnahme 1 — Setup-AP mit Passwort schützen — ERLEDIGT (3.8.1)

**Priorität: hoch. Aufwand: eine Zeile.** Umgesetzt am 2026-08-18, siehe
Changelog `src/version.h` 3.8.1.

**Problem:** `wifiManager.autoConnect("HeishaMon-Setup")` in
`src/webfunctions.cpp` öffnet das Konfigurationsportal als **offenen** AP, und
die Parameterfelder sind mit den echten Werten vorbefüllt — darunter
OTA-Passwort und MQTT-Passwort. Das ist kein Erstboot-Thema: Der
WLAN-Watchdog startet nach 5 min Ausfall neu, `autoConnect` scheitert nach
10 s und öffnet dann für 180 s das offene Portal — zyklisch für die Dauer
eines Router-Ausfalls. Jeder in Funkreichweite kann in diesem Fenster die
Zugangsdaten lesen oder dem Gerät einen fremden MQTT-Broker unterschieben.

**Fix:** `autoConnect("HeishaMon-Setup", "<AP-Passwort>")` — WPA2, mindestens
8 Zeichen. Das Passwort gehört **nicht** in git, sondern als Build-Flag in
`platformio_user_env.ini` (gleiches Muster wie Ports und IPs), mit Fallback im
Code wie bei `HEISHA_HOSTNAME`.

**So umgesetzt:** neue Sektion `[ap_defaults]` in `platformio_user_env.ini` mit
`-D HEISHA_AP_PASSWORD`, eingebunden in beide Board-Basen (`[esp8266_base]`,
`[esp32_base]`) — damit gilt sie für alle zehn Envs, ohne dass jedes Env eine
weitere Zeile bekommt. Im Code (`src/webfunctions.cpp`) ein Fallback samt
`#warning`, wenn das Flag fehlt, und zwei `static_assert` auf die WPA2-Längen
8–63: ein zu kurzes Passwort verwirft der WiFiManager still und öffnet den AP
wieder — der Build bricht jetzt stattdessen ab. Nachweise ohne Hardware:
Gegenprobe mit vierstelligem Passwort (Build bricht), Gegenprobe ohne Flag
(`#warning`), `pio project config` (alle zehn Envs führen das Flag), alle zehn
Envs gebaut (ESP32 Flash +112 B, ESP8266 RAM +112 B / Flash +104 B), vier
Hosttests grün.

**Nachweis — erbracht am 2026-08-18** (am D1 mini statt am ESP32-Testgerät, das
hing am USB; Env `d1_mini_test`, MQTT-Server leer gelassen). Statt die SSID
unerreichbar zu machen, wurde der Flash vorher gelöscht — dann geht das Gerät
ohne Umweg ins Portal. Ergebnis: Der AP verlangt ein Passwort und nimmt es an,
die WLAN-Konfiguration funktioniert danach unverändert (Gerät unter
192.168.2.198, `config.json` gespeichert). Der Zyklus aus dem Befund war dabei
im seriellen Log direkt zu sehen: Portal-Timeout nach 180 s → Neustart → AP
wieder auf. K1 im selben Durchgang geprüft, siehe dort.

**Folgeaufgabe — erledigt (2026-08-18):** Das AP-Passwort ist in der
Offline-Notfallliste eingetragen (Owner-Bestätigung). Damit ist der Rettungsweg
bei WLAN-Ausfall wieder vollständig: Hotspot ansprechbar, Passwort dort
nachschlagbar. Im Repository steht es bewusst nirgends — es liegt allein in
`platformio_user_env.ini` (nicht in git) und in der Notfallliste.

---

## Maßnahme 2 — LWT „Online" mit Retain-Flag publizieren — ERLEDIGT (3.8.2)

**Priorität: mittel (wird hoch, sobald ein echter Broker kommt). Aufwand: eine Zeile.**
Umgesetzt am 2026-08-19, siehe Changelog `src/version.h` 3.8.2.

**Problem:** `mqtt_reconnect()` in `src/HeishaMon.cpp` publiziert `Online`
ohne Retain-Flag, während das Will `Offline` retained hinterlegt ist. Auf
einem echten Broker bleibt nach jedem Reconnect `Offline` als retained Wert
stehen: Ein Abonnent, der sich später verbindet (Node-RED-Neustart, Wächter),
hält die Stufe für tot, obwohl sie läuft. Heute maskiert der
ioBroker-MQTT-Adapter das (er bedient neue Abonnenten aus seiner State-DB,
siehe `test/README.md`), aber der Umzug auf einen mosquitto steht als Option
im Raum — und der Kaskaden-Wächter ist genau der Konsument, der darauf
hereinfiele.

**Fix:** `mqtt_client.publish(Topics::WILL.c_str(), "Online", true)`.

**Nachweis:** Gegen einen lokalen mosquitto (Container, docker-compose):
Testgerät verbinden lassen, danach mit einem **neuen** Abonnenten
`<prefix>/info/LWT` abonnieren → es muss retained `Online` kommen; Testgerät
stromlos machen → nach Ablauf des Keepalive retained `Offline`.

**Entscheidung 2026-08-19: Fix jetzt, Broker-Nachweis später.** Der Aufwand
liegt nicht im Fix, sondern im Nachweis — und der braucht einen Broker, der
Retain überhaupt zeigt. Ihn abzuwarten hätte bedeutet, einen bekannten Fehler
stehen zu lassen, bis niemand mehr an diesen Plan denkt. Umgesetzt ist der Fix
deshalb sofort, belegt über Build und Codeinspektion (alle zehn Envs, vier
Hosttests grün); **der oben beschriebene Broker-Test steht aus und gehört in
die Vorbereitung des mosquitto-Umzugs**, wo ohnehin ein mosquitto läuft. Auch
in der README unter „MQTT-Schnittstelle" vermerkt, damit er dort auffällt.

---

## Maßnahme 3 — `desc[]`-Zugriff nach oben begrenzen — ERLEDIGT (3.9.0)

**Priorität: mittel. Aufwand: Tabellenerweiterung + Testausbau (halber Tag).**
Umgesetzt am 2026-08-19, siehe Changelog `src/version.h` 3.9.0.

**Problem:** `handleTableRefresh()` in `src/webfunctions.cpp` fängt beim
Klartext-Nachschlag nur **negative** Indizes ab. Der Kommentar in
`src/decode.cpp` benennt die Lücke selbst, geschlossen wurde sie aber nur
für die beiden Byte-110-Arrays (drittes Element „unknown"). Offen bleiben:

* alle 2-Bit-Felder mit 2er-Arrays — Rohwert `b11` ergibt Index 2
  (`OffOn`, `DisabledEnabled`, `Valve`, `BlockedFree`, `InactiveActive`,
  `HeatCoolModeDesc`; z. B. TOP0, TOP20, TOP58–61),
* die 3-Bit-Dekodierer von TOP17/TOP18 — Index bis 6 bei 4er-Arrays
  (`Powerfulmode`, `Quietmode`).

Gelesen wird dann ein wilder `const char*`, formatiert per `%s` — das kann
das Gerät **an der laufenden Wärmepumpe** abstürzen lassen, sobald ein
Browser die Seite offen hat und die WP einen unerwarteten Rohwert liefert.
Die Erfahrung dieses Projekts (Byte 110, Kurvengrenzen) zeigt, dass genau das
vorkommt: Die WP liefert Werte außerhalb des bisher Beobachteten.

**Fix (im Stil des Projekts, zwei gleichwertige Wege — vor Umsetzung wählen):**

* **A — Zählspalte:** `desc_count` als weiteres `byte`-Feld in `StateTopic`,
  `handleTableRefresh` klemmt auf `value < desc_count`. Vorteil: eine
  zentrale Prüfung; Nachteil: 90 Zeilen anfassen, auf dem ESP8266 je Zeile
  ggf. Padding (Feldreihenfolge beachten, siehe Kommentar in `decode.h`).
* **B — Arrays auffüllen:** jedes Klartext-Array bis zum Maximalindex seines
  Dekodierers mit `"unknown"` auffüllen (wie bei Byte 110 geschehen).
  Vorteil: kein Strukturumbau; Nachteil: die Zusicherung steht nicht im Code,
  sondern muss per Test gehalten werden.

In beiden Fällen: **keine Änderung am Dekodierpfad selbst** — die
MQTT-Werte bleiben identisch.

**Nachweis:**

* `byte110_test.cpp` Fall 4 („Anzeigeindex bleibt im Array") auf die
  **gesamte** Tabelle ausweiten: für jede Zeile alle 256 Rohwerte durch den
  echten Dekodierer schicken, der entstehende Index muss innerhalb des
  Arrays bleiben (bzw. unter `desc_count`). Gegenprobe: mit einem bewusst
  zu kurzen Array muss der Test brechen.
* `test/decode_vergleich.py --basis v3.8.0`: alle Zeilen identisch —
  belegt, dass sich an den publizierten Werten nichts geändert hat.
* RAM/Flash-Delta je Env in den Changelog (ESP8266 im Blick behalten:
  const-Tabellen liegen dort im RAM).

**Entscheidung 2026-08-19: Weg B, plus `nullptr`-Abschluss.** Beim Nachzählen
war der Befund kleiner als hier angenommen: Es gibt nur 13 Klartext-Listen, und
die 2-Bit-Dekodierer liefern nach ihrem `-1` höchstens Index **2**, nicht 3.
Offen waren damit acht Listen (sechs brauchten einen Eintrag mehr,
`Powerfulmode`/`Quietmode` je drei); `HolidayState`, `OpModeDesc` und die beiden
Byte-110-Listen waren schon lang genug. Damit sind Weg A und B nicht mehr
gleichwertig: acht geänderte Zeilen gegen 90 angefasste Tabellenzeilen plus
Padding-Risiko im struct. Weg A wurde verworfen.

**Abweichung von Weg B, wie er hier steht:** Zusätzlich endet jetzt *jede*
Liste mit `nullptr`, und der Nachschlag läuft über die neue Funktion
`desc_text()` in `src/decode.h`, die bis zum gesuchten Index hochzählt statt
direkt zuzugreifen. Grund: Der oben genannte Nachteil von Weg B — „die
Zusicherung steht nicht im Code" — wäre sonst geblieben, und der Test braucht
die Listenlänge, die das struct nicht kennt. Der Abschluss löst beides. Er
macht die Grenze im Code wirksam (ein künftiger Dekodierer mit größerem Index
zeigt eine leere Zelle, statt das Gerät neu zu starten) und erlaubt dem
Hosttest, die Länge jeder Liste selbst zu bestimmen — ohne eine zweite Liste,
die hinter der ersten zurückbleiben kann. `desc_text()` steht als
`inline`-Funktion im Header, damit Firmware und Hosttest dieselbe Regel
benutzen; die Nachbildung der Anzeigelogik in `byte110_test.cpp` entfällt.

**Nachweis — erbracht am 2026-08-19, ohne Hardware:**

* `byte110_test` Fall 6 (neu): alle 90 Zeilen, alle 256 Rohwerte durch den
  echten Dekodierer; 22 Klartext-Zeilen geprüft, höchster Index je Zeile gegen
  die Listenlänge, jeder gedeckte Index liefert einen Text. Alle 90 Listen sind
  mit `nullptr` abgeschlossen.
* Gegenprobe wie gefordert: `OffOn` und `Quietmode` testweise auf die alte
  Länge gekürzt → Fall 6 schlägt an („Rohwert 0x03 ergibt Index 2, Liste hat
  2"), Rückgabewert 1; nach dem Rückbau wieder grün. Zusätzlich prüft der Test
  die Regel an einer bewusst zu kurzen Liste direkt.
* `test/decode_vergleich.py --basis v3.8.2`: 68040 Zeilen, IDENTISCH — 90
  Topics, Nummer, Name, Wert und Einheit gleich.
* Alle zehn Envs gebaut: ESP32 RAM +160 B / Flash +188 B, ESP8266 RAM +152 B /
  Flash +184 B (38 zusätzliche Zeiger à 4 Byte; auf dem ESP8266 56,1 % statt
  55,9 % RAM).

---

## Kleinpunkte — alle vier entschieden (K1 in 3.8.1, K3 in 3.8.2, K2/K4 ohne Codeänderung)

**K1 — Telnet (Port 23) ohne Auth, `R` löst Reboot aus. — ERLEDIGT (3.8.1),
Weg (b).** Der Web-`/reboot` verlangt Login, Telnet nicht — inkonsequent.
Optionen waren: (a) bewusst so lassen (Heimnetz, dokumentieren), (b)
Reboot-Kommando aus dem Telnet-Menü nehmen, (c) Telnet abschaltbar machen.
Umgesetzt ist (b): `case 'R'` in `handle_telnetstream()` startet nicht mehr neu,
sondern antwortet mit dem Verweis auf `http://<ip>/reboot` — still ignorieren
hätte nur Rätselraten erzeugt. Die Umschalter `L`/`D`/`H` und die Abfragen
`M`/`W`/`I` bleiben unverändert, sie werden für die Abnahme gebraucht.

**Nachweis am Gerät (2026-08-18, D1 mini, 192.168.2.198):** `R` bringt die
Hinweiszeile, die Telnet-Verbindung bleibt bestehen, `M` und `I` antworten auf
derselben Verbindung weiter — bei einem Reboot wäre die Verbindung sofort weg.
Gegenprobe auf der seriellen Konsole: keine Bootmeldung im Testfenster.
Ersatzweg geprüft: `/reboot` ohne Login → HTTP 401, mit Login → HTTP 200, Gerät
startet neu und kommt mit gespeicherter Konfiguration wieder hoch.

**K2 — `SUBSCRIBE_GRACE` (5 s) ist lastabhängig. — ENTSCHIEDEN (2026-08-19),
keine Codeänderung.** Braucht der ioBroker-Replay nach dem SUBACK einmal länger
als 5 s (großer Objektbaum, Systemlast), laufen alte Sollwerte wieder durch —
der gemessene 55-Grad-Fall. Kein Codefehler; die Bilanzmeldung im MQTT-Log ist
das Frühwarnsignal.

Begründung des Entscheids: Der Wert darf weder zu groß noch zu klein sein — ein
längeres Fenster schluckt echte Kommandos, ein kürzeres lässt den Schwall
durch — und seine richtige Größe hängt an der Last einer anderen Maschine.
Durch Verschieben wird er nicht besser. Eine echte Lösung müsste das Verwerfen
am Inhalt festmachen (Sequenznummer oder Zeitstempel im Payload) statt an der
Zeit; das ist ein Protokollumbau auf beiden Seiten für ein Problem, das einmal
aufgetreten und über die Bilanzmeldung sichtbar ist.

**Beobachtungsauftrag — steht jetzt in der README** („Der Broker spielt beim
Verbinden alles wieder ein", als Wartungshinweis): Nach jedem ioBroker-Update
einen Reconnect über Telnet mitschneiden und prüfen, ob die Bilanzzeile **vor**
den ersten echten Kommandos kommt und die verworfenen Topics noch alle in
derselben Sekunde wie das SUBACK eintreffen. Er steht dort statt nur hier,
damit er beim Warten der Anlage gefunden wird und nicht in einem
Durchsichtsprotokoll versauert.

**K3 — State-Publish-Rückgabewert unbeachtet** (`publish_heatpump_data`). —
**UMGESETZT (3.8.2), entgegen dem Vorschlag hier.** Schlägt ein Publish fehl,
galt der Wert trotzdem als gesendet und kam erst mit der nächsten Änderung oder
dem 5-min-Vollupdate wieder.

Warum doch angefasst: Die Begründung „das Vollupdate deckelt den Schaden auf
5 min" trägt für einen einzelnen verlorenen Publish, nicht für den praktisch
relevanten Fall. Bei einer MQTT-Unterbrechung läuft die Schleife weiter und
schreibt den Vergleichspuffer fort; nach dem Reconnect gilt jeder
zwischenzeitlich geänderte Wert als gesendet, und beim Broker steht bis zu
5 min lang der alte — retained, also mit dem Anschein von Gültigkeit. Das ist
derselbe Konsument und dasselbe Muster wie bei Maßnahme 2, deshalb dieselbe
Version. Der Fix ist eine Marke, die sich merkt, dass etwas liegenblieb; der
nächste Durchlauf (5 s) schickt die Tabelle erneut. Die `<PUB>`-Logzeile bleibt
an die echte Wertänderung geknüpft, sonst füllt eine laufende Wiederholung das
Log alle 5 s.

**K4 — `write_mqtt_log` gegen den PubSubClient-Puffer. — ENTSCHIEDEN
(2026-08-19), keine Umsetzung.** `log_msg` fasst 256 Bytes, der Paketpuffer aber
auch nur 256 inklusive Topic und Header — Meldungen über ~225 Zeichen würden
still verworfen. Die Grenze ist nicht fest, sondern hängt am MQTT-Prefix.

Begründung des Entscheids: Aktuell erreicht keine Meldung die Grenze, betroffen
ist ausschließlich der Log-Pfad, nicht der Steuer- oder State-Pfad — es ginge um
verlorene Diagnose, nicht um verlorene Regelung. Als bekannte Grenze hier
dokumentiert. Falls der Punkt später doch angefasst wird, ist die sinnvollste
Variante, den Rückgabewert von `publish()` zu prüfen und bei Misserfolg auf
Telnet auszuweichen — das deckt nebenbei den häufigeren Fall ab, dass bei
getrennter MQTT-Verbindung jede Logmeldung ersatzlos verlorengeht.
`setBufferSize(512)` ist ausdrücklich **nicht** die Empfehlung: 256 Byte mehr
Heap dauerhaft, auf dem ESP8266 spürbar.

---

## Zusatzpunkt — CI läuft nur noch für `main` (2026-08-19)

Nicht aus der Codedurchsicht, aber in derselben Runde entschieden und
umgesetzt. `.github/workflows/main.yml` stand auf `on: [push, pull_request]`,
lief also bei jedem Push auf jeden Branch; der `pull_request`-Trigger dagegen
praktisch nie, weil hier lokal gemergt wird.

Jetzt: `push` nur auf `main`, dazu `workflow_dispatch`, damit ein Branch bei
Bedarf trotzdem gezielt durch die CI geschickt werden kann, ohne die Datei zu
ändern.

Bewusst in Kauf genommen: Das Signal kommt nach dem Merge. Vertretbar, weil die
Projektkonvention ohnehin verlangt, vor dem Merge alle zehn Envs lokal zu bauen
und die Hosttests laufen zu lassen — die CI ist damit Rückversicherung für
`main`, nicht Erstprüfung. Kostenargument gibt es keins: Für ein öffentliches
Repository sind Actions-Minuten frei; der Gewinn ist weniger Lauf-Rauschen.

---

## Ausdrücklich geprüft und in Ordnung

Damit die nächste Durchsicht nicht von vorn anfängt — diese Punkte wurden
gezielt untersucht und **nicht** bemängelt:

* Telegramm-Längen und alle Byte-Positionen der Tabellen (max. Position 198
  bei 203 Bytes), `mainQuery`/`mainCommand` exakt 110 Bytes.
* Masken und Konvertierung aller 32 Set-Kommandos gegen ihre Bitgruppen.
* Die Opmode-Asymmetrie (Set 24/40, Read 25/41) — Protokolleigenschaft,
  deckungsgleich mit dem Original.
* `millis()`-Arithmetik an allen Vergleichsstellen (Karenzfenster,
  Sammelfenster, Watchdog, Reconnect-Backoff).
* Timer-Kette auf Abrisspfade: terminiert auch bei fehlgeschlagenem
  `build_heatpump_command`, Deferral am Limit und aktivem Karenzfenster.
* ESP32-Spezifika: `WiFi.setSleep(false)`, RX-Puffer 256 ≥ 203,
  USB-CDC-Konsole parallel zur WP-UART, Partitionswahl fürs 4-MB-Board.
