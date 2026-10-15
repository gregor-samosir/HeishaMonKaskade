# Codedurchsicht 2026-10-15: Langzeitstabilität, Code, Dokumentation

Dritte Durchsicht, diesmal ausdrücklich „schonungslos“ beauftragt: die
gesamte Firmware (`src/`, Stand 3.23.0, Commit `2c2bff8`), alle Tests und die
Dokumentation, mit drei Fragen — ist der Code sauber und nachvollziehbar, hält
er monatelang durch, und trägt die Doku jemanden, der die Entstehung nicht
kennt? Vorgehen: alles gelesen; Hosttests lokal (15/15 grün, 6 s); eine Kopie
mit `-Wall -Wextra` gebaut; die Live-Lage zu S1 im ioBroker **nur lesend**
geprüft. Geräte und Repo wurden dabei nicht verändert.

**Gesamtbild:** Die Module der letzten Wochen (Notbetrieb, Verbindungswacht,
RTC-Spiegel, KNX) sind vorbildlich gebaut — arduino-freie Header, Überlauf im
Hosttest, jede Länge geprüft. Diese Strenge reicht aber **nicht in den alten
Kern** (Karenzfenster, Kommandopfad, Zeitkette), und genau dort sitzt der
einzige kritische Befund. Die Dokumentation ist umfangreich bis zur
Unübersichtlichkeit und enthält schon falsche Einstiegsangaben.

**Entscheide:** Der Owner hat am 2026-10-15 erklärt, die Punkte nicht im
Einzelnen beurteilen zu können, und sich auf die Analyse verlassen. Deshalb
steht hier **je Punkt eine Empfehlung, kein Entscheid**. Entschieden wird je
Paket, wenn es begonnen wird; das Ergebnis kommt in die Spalte „Stand“.

**Wie mit diesem Plan gearbeitet wird:** Die Übersicht ist der Index. Wird ein
Paket begonnen, entsteht daraus ein eigener Arbeitsplan nach dem Muster von
[`Arbeitsplan-Karenzfenster-3.23.1.md`](Arbeitsplan-Karenzfenster-3.23.1.md)
(Lies zuerst, Entscheide, Schritte mit Prüfkriterium); hier wird nur der Stand
nachgetragen.

## Übersicht

| ID | Befund | Prio | Paket | Stand |
| --- | --- | --- | --- | --- |
| S1 | Karenzfenster kippt nach 24,86 Tagen — alle Set-Kommandos werden still verworfen | **kritisch** | 3.23.1 | **erledigt in 3.23.1** (ausgerollt 2026-10-15, alle vier Boards, Releases angelegt) — [Arbeitsplan](Arbeitsplan-Karenzfenster-3.23.1.md) |
| S2 | Ein Telnet-Client, der nicht mehr liest, hält `loop()` minutenlang an | hoch | P3 | offen |
| S3 | Kein Watchdog für `loop()` (= K1 vom 2026-09-02) | mittel–hoch | P3 | offen |
| S4 | Serielle Strecke ohne Gesundheitsanzeige; Notbetrieb bestätigt gegen eingefrorene Werte | mittel | P4 | offen |
| S5 | Build nicht reproduzierbar — Bibliotheken und PlatformIO nicht gepinnt | mittel | P2 | offen |
| S6 | Abgelehnte Set-Befehle (Nicht-Ganzzahl, außerhalb der Grenzen) sind nirgends dauerhaft sichtbar | mittel | P3 oder P4 | offen — aufgenommen auf Wunsch des Owners 2026-10-15 |
| C1 | Kommandopfad nicht im Header-Muster; Tests prüfen Nachbildungen statt des echten Codes | hoch | P5 | offen |
| C2 | Compiler-Warnungen abgeschaltet | mittel | P2 | offen |
| C3 | Kommentare erzählen Geschichte und veralten | mittel | laufend, P7 | offen |
| C4 | Formatierung uneinheitlich | niedrig | P7 | offen |
| C5 | Kleinpunkte a–f | niedrig | P2, P5, P6 | offen |
| D1 | README-Einstieg falsch (Zahlen, LWT- und Log-Pfad) | hoch für Dritte | P1 | offen |
| D2 | Veraltete Stände in Kopfzeilen | niedrig | P1 | offen |
| D3 | Widerspruch zum Re-Assert, auf dem eine Sicherheitsbegründung ruht | mittel | P1 | offen |
| D4 | Umfang und Struktur der Doku arbeiten gegen das Ziel Wartbarkeit | mittel | P7 | offen |
| N1 | Beifang aus der Abnahme von 3.23.1: Der Notbetriebswerte-Sender schickte Dezimalwerte (33.5, 25.5), die die Firmware komplett verwirft | mittel | `nodered-flows` V1.1 | **erledigt** 2026-10-15 — [Auftrag](Auftrag-Notbetriebssender-Ganzzahl.md) |

## Reihenfolge der Pakete (Empfehlung)

| Schritt | Paket | Inhalt | Art | Braucht |
| --- | --- | --- | --- | --- |
| 1 | **3.23.1** | S1 | Fix mit Frist | eigenen Arbeitsplan (liegt vor) |
| 2 | **P1** Doku-Korrekturen | D1, D2, D3 | nur Doku, direkt auf `main` | Lesen in `nodered-flows` (D3) |
| 3 | **P2** Build-Hygiene (3.23.2) | S5, C2, C5a, C5f | ohne Funktionsänderung | Nachweis über `.elf`-Vergleich |
| 4 | **P3** `loop()` darf nicht stehen (3.24.0) | S2, S3 | Fix | Prüfling, OTA-Test |
| 5 | **P4** Serielle Strecke (3.25.0) | S4 | Ausbau | Owner-Wortlaut für die Notfallseite, Auftrag an `nodered-flows` |
| 6 | **P5** Kommandopfad als Header | C1, C5b, C5c | ohne Funktionsänderung | Vorher/Nachher-Liste aller Kodierungen |
| 7 | **P6** Weboberfläche | C5d, C5e | Härtung | Owner-Entscheid: Notfallablauf benutzt GET |
| 8 | **P7** Struktur | D4, C3, C4 | Umbau Doku/Format | Owner-Entscheid über Umfang |

Warum diese Reihenfolge: Risiko zuerst. P1 ist billig und beseitigt falsche
Angaben, nach denen jemand handeln könnte. P2 vor P3, damit beim Umbau von
`loop()` die Warnungen schon sichtbar sind. Änderungen ohne Funktionswirkung
(P2, P5) bleiben von Funktionsänderungen getrennt, damit ihr Nachweis ein
einfacher Vergleich bleibt.

**Einheitlichkeit H1/H2:** Kein Paket behandelt die Stufen unterschiedlich.
P4 führt eine neue Sperre für beide Rollen ein — vor dem Bau trotzdem nach der
STOPP-Regel mit dem Owner gegenprüfen.

---

## Befunde im Einzelnen

### S1 — Karenzfenster kippt nach 24,86 Tagen *(kritisch)*

Vollständig in [`Arbeitsplan-Karenzfenster-3.23.1.md`](Arbeitsplan-Karenzfenster-3.23.1.md),
dort auch die Live-Lesung vom 2026-10-15 und die Frist. Kurz:
`(long)(millis() - setCommandsIgnoredUntil) < 0` in `src/HeishaMon.cpp`
(Zeilen 814, 1484) meldet ab 2³¹ ms nach dem letzten Connect wieder „Fenster
offen“; jedes Set-Kommando wird still verworfen. Beide früheren Durchsichten
haben die Stelle als überlaufsicher abgehakt.

### S2 — Ein Telnet-Client kann `loop()` anhalten *(hoch)*

**Problem:** `TelnetStream` schreibt über `NetworkClient::write()` des Cores.
Ist der Sendepuffer voll, wartet das bis zu 10 × 1 s — **je Logzeile**
(`WIFI_CLIENT_MAX_WRITE_RETRY`, `WIFI_CLIENT_SELECT_TIMEOUT_US`,
`NetworkClient.cpp`, Core 3.3.11). Das Debug-Log ist ab Werk an
(`outputTelnetLog = true`, `src/HeishaMon.cpp` Zeile 92), je Abfragezyklus
rund zehn Zeilen.

**Folge:** Ein Client, der nicht mehr liest — Laptop zugeklappt mitten im
Mitschnitt, `nc` mit Ctrl-Z angehalten —, füllt den Puffer in etwa einer
halben Minute. Danach steht `loop()` je Zyklus über eine Minute: keine
WP-Abfrage, MQTT-Keepalive reißt, Notbetriebsseite unerreichbar. Bei einem
verschwundenen Client endet das erst mit dem TCP-Abbruch (Minuten), bei einem
lebenden, aber nicht lesenden gar nicht. Port 23 ist ohne Anmeldung — dieselbe
Überlegung wie bei K1 in 3.8.1 („soll niemand ohne Anmeldung auslösen
können“), nur wirksamer als ein Reboot.

**Stand des Nachweises:** Code-Analyse (Core und Bibliothek gelesen), am Gerät
nicht beobachtet.

**Vorschlag:** Telnet-Ausgabe nicht-blockierend. Ein schlanker eigener
Telnet-Teil (ein `WiFiServer` auf 23, ein Client) schreibt per
`send(fd, …, MSG_DONTWAIT)`; bei `EAGAIN` wird die Zeile verworfen und
gezählt, nach z. B. 20 Fehlversuchen in Folge der Client getrennt. Die Tasten
`L D H M W I C R` bleiben. *Verworfen:* das Debug-Log ab Werk auszuschalten —
die Mitschnittwerkzeuge erkennen eine tote Sitzung an der fehlenden
`<DBG>`-Zeile im 5-s-Takt (Memory „Messfallen am Prüfling“).

**Nachweis:** Prüfling: `nc <ip> 23`, Ctrl-Z, 10 min warten — `info/uptime`
und die State-Zeitstempel laufen weiter, die Startseite antwortet. Danach
`fg`: Ausgabe setzt wieder ein. Gegenprobe mit 3.23.x: `loop()` steht.

### S3 — Kein Watchdog für `loop()` *(mittel–hoch)*

K1 der Durchsicht vom 2026-09-02, dort ausführlich. **Neue Gründe, die
Priorität anzuheben:**

* Das Board wird über das WP-Kabel versorgt
  ([`Ablauf-Backup-Boards.md`](Ablauf-Backup-Boards.md), „Im Ernstfall“). Ein
  Hänger heißt: jemand geht in den Keller und zieht den Stecker.
* Während eines Hängers ist auch der Notbetriebsknopf weg — das Werkzeug für
  den Fall, dass niemand da ist.
* S2 zeigt einen konkreten Weg in einen langen Stillstand.

**Vorschlag:** wie K1 — nach `setup()` `esp_task_wdt_reconfigure()` auf 120 s
und `enableLoopWDT()`. Die längsten legitimen Blockaden (Hydraulikschritt bis
30 s, KNX 20,5 s, NTP 30 s in `setup()`) liegen darunter; das 180-s-Portal des
WiFiManagers liegt in `setup()` vor dem Einschalten. OTA: über
`Update.onProgress()` füttern (ArduinoOTA und HTTP-Updater schreiben beide
über `Update`) — **am Prüfling zu verifizieren**, beide Wege.

**Nachweis:** Prüfling: OTA über beide Wege läuft durch. Ein absichtlicher
Hänger über eine Route, die es nur im Prüfling-Build gibt (Muster
`KNX_PRUEFZUGANG`), endet nach 120 s im Neustart mit `info/boot_reason =
TASK_WDT` und `boot_count` + 1; die Notbetriebswerte überleben (RTC-Spiegel).

### S4 — Serielle Strecke ohne Gesundheitsanzeige *(mittel)*

**Problem:** `actual_data[]` wird nur bei einem gültigen Telegramm
fortgeschrieben. Reißt die Strecke ab (Kabel, Pegelwandler, WP-Platine),
bleiben die letzten Werte stehen — auf der Webseite, retained beim Broker und
als Rücklesequelle des Notbetriebs (`src/notbetrieb.cpp`, Zeilen 869–870). Die
8-s-Mindestwarte (`src/notbetrieb.h`, Zeilen 88–105) *setzt voraus*, dass in
dieser Zeit ein frisches Telegramm kommt; geprüft wird das nicht.

**Folge:** Jeder Schritt, dessen Sollwert dem letzten Stand entspricht, wird
ohne Antwort der WP abgehakt. Im Warmwasserzweig können das im ungünstigen
Fall alle fünf WP-Schritte sein — GRÜN, ohne dass ein Kommando angekommen ist.
Außerdem fehlt in `info/` die aussagekräftigste Zahl einer Bridge: wie gut die
Strecke zur WP läuft. Auf ioBroker-Seite gibt es dafür einen Stale-Guard über
`ts`; die Firmware selbst kennt keinen.

**Vorschlag:**

1. Zähler gültiger Telegramme, Timeouts und Prüfsummenfehler sowie der
   Zeitpunkt des letzten gültigen Telegramms. Neue Topics im 5-min-Takt:
   `info/telegram_ok`, `info/telegram_err`, `info/telegram_age` (Sekunden).
2. Notbetrieb: Ein Schritt gilt erst als bestätigt, wenn **nach** Ablauf der
   Mindestwarte ein gültiges Telegramm eingegangen ist. Regel in
   `notbetrieb.h`, Hosttest mit eingefrorenen Werten.
3. Neuer Sperrgrund „keine Verbindung zur Wärmepumpe“, wenn das letzte gültige
   Telegramm älter als z. B. 30 s ist — **Wortlaut der Notfallseite gibt der
   Owner vor** (wie bei allen Texten dort).
4. Auftrag an `nodered-flows`: Wächter auf `info/telegram_age`.

**Nachweis:** Hosttest; am Prüfling ohne WP: `telegram_age` wächst,
Sperrgrund erscheint. An der Anlage: `telegram_err` bleibt im Betrieb nahe 0.

### S5 — Build nicht reproduzierbar *(mittel)*

**Problem:** `sstaub/Ticker` steht in [`platformio.ini`](platformio.ini)
(Zeile 61) **ohne Version** — und trägt die gesamte Zeitkette. Die übrigen
Bibliotheken stehen mit `^`, die CI installiert PlatformIO mit `--upgrade`.
Ein frischer Rechner oder ein CI-Lauf nach einem neuen Release baut eine
andere Firmware als die geflashte — dasselbe Problem, das am 2026-09-13 für
die Plattform gelöst wurde.

**Vorschlag:** exakt pinnen, auf die heute installierten Stände:
`knolleary/PubSubClient@2.8`, `bblanchon/ArduinoJson@7.4.3`,
`jandrassy/TelnetStream@1.3.0` (zieht `NetApiHelpers` 1.0.2),
`tzapu/WiFiManager@2.0.17`, `sstaub/Ticker@4.4.0`; in
`.github/workflows/main.yml` `pip install platformio==6.1.19`. Ein Update ist
danach eine Änderung an `platformio.ini` und läuft wie eine Codeänderung.

**Nachweis:** `text/data/bss` der `.elf` vor und nach dem Pinnen identisch
(Memory „Nachweis: Umbau ohne Funktionsänderung“).

*Zur Kenntnis, kein Handlungsbedarf:* `PubSubClient` 2.8 ist von 2020 und wird
nicht mehr gepflegt. Ein Wechsel wäre ein eigenes Vorhaben mit eigenem Risiko.

### S6 — Abgelehnte Set-Befehle sind nirgends dauerhaft sichtbar *(mittel)*

*Aufgenommen am 2026-10-15 aus der Abnahme von 3.23.1 — kein Fund der
Durchsicht selbst, sondern Folge von N1 (Dezimalwerte im Notbetriebszweig).*

**Problem:** `build_heatpump_command()` (`src/commands.cpp`, Zeilen 284–325)
lehnt jeden Payload ab, den `strtol` nicht vollständig als Ganzzahl liest
(`33.5`, `1e1`, `""`, `33 `), und jeden Wert außerhalb von `min`/`max` der
Tabelle. Der Befehl geht verloren, die Wärmepumpe behält ihren alten Wert, und
der `set`-Datenpunkt im ioBroker zeigt weiter den eingetippten. Die Ablehnung
steht nur als `Error:`-Zeile im Logring (32 Zeilen, vom 5-min-Re-Assert binnen
10–30 min verdrängt, mit dem Neustart weg), in `<präfix>/log` (der Datenpunkt
hält nur die letzte Zeile) und als Telnet-Rückfall. Das ioBroker-Log enthält sie
nicht.

**Folge / Beleg:** Vom 2026-10-10 bis 2026-10-15 verwarf die Bridge die
Dezimalwerte des Notbetriebszweigs, ohne dass es jemand bemerkte (N1). Der
Befehls-Wächter deckt neun Kanäle je Stufe ab, die Temperaturkanäle mit 0,5 K
Toleranz (`<=`): `33.5` gegen einen Ist-Wert von `33` löst keinen Alarm aus.
Der `Warmwasser-Sender` prüft `Number.isInteger` selbst. Für alle übrigen
SET-Befehle gibt es keine Rücklesung. Später soll die Anlage von jemand
anderem betreut werden, der im ioBroker direkt in einen `set`-Datenpunkt
tippt.

**Stand des Nachweises:** Code gelesen; am 2026-10-15 alle 76 `set.*`-Datenpunkte
beider Stufen im ioBroker gelesen — keine Nicht-Ganzzahl. Ein Hosttest für die
Ablehnung fehlt (das ist C1: `commands.cpp` hängt am Arduino-Code).

**Vorschlag:**
1. Regel „Payload ist eine Ganzzahl“ in einen kleinen arduino-freien Header
   ziehen (Muster `sendwindow.h`), mit Hosttest: `33.5`, `" 33"`, `"33 "`,
   `"+33"`, `"0x1A"`, `"1e1"`, `""`, `"-"` und ein Überlauf wie
   `99999999999`. Damit entsteht der erste Test des Set-Pfads ohne Nachbildung.
2. Abgelehnte Befehle zählen und den letzten sichtbar machen:
   `info/set_abgelehnt` (Zähler seit dem Start) und
   `info/set_abgelehnt_letzter` (Topic und Wert, auf 64 Zeichen gekürzt).
   Eine Stelle für alle 38 SET-Befehle, ohne je Kanal eine Rücklesung zu
   pflegen. `MQTT-Topics.md` nachziehen.
3. Auswertung im bestehenden `WP_Bridge_Waechter` (Anstieg des Zählers wie
   `boot_count`) — Auftrag an `nodered-flows`, erst nach der Firmware.

**Offen, nicht Teil von S6:** ob die Firmware Dezimalwerte selbst rundet
(Auftrag-Notbetriebssender-Ganzzahl.md, „Was dieser Auftrag nicht
entscheidet“). Empfehlung: nein — bei Befehlen an die Wärmepumpe ist
„ablehnen und melden“ sicherer als „still anders ausführen“, gerundet wird an
der Quelle. Das ist **kein Entscheid**, nur die Empfehlung.

**Nachweis:** Hosttest wie unter 1. Am Prüfling: `set/Z1HeatRequestTemperature`
= `33.5` senden → `info/set_abgelehnt` + 1, `info/set_abgelehnt_letzter`
nennt Topic und Wert, der State-Wert der Pumpe bleibt unverändert.

**Zuordnung:** als Beigabe zu P3 (3.24.0) vorgeschlagen, thematisch näher an
P4 (`info/`-Zähler der seriellen Strecke, gleiches `MQTT-Topics.md`-Update).
Wo es landet, entscheidet sich beim Planen von P3/P4; die Regel „eine Version
belegt eine Sache“ aus dem Arbeitsplan 3.23.1 spricht für P4.

### C1 — Kommandopfad nicht im Header-Muster *(hoch)*

**Problem:** CLAUDE.md verlangt, prüfbare Regeln in arduino-freie Header zu
legen. Für `setCommands[]`, Umrechnung, Maske und Konfliktprüfung
(`src/commands.cpp`) gilt das nicht. Folge: `test/merge_test.cpp` ist
ausdrücklich eine „Nachbildung“; `byte9_test`, `byte20_test` und
`byte28_test` bilden die Merge-Zeile nach und tragen Masken und Parameter als
eigene Konstanten. Ein Fehler in einer Tabellenzeile lässt alle C++-Tests grün.

**Beispiel:** Parameter 64 statt 16 bei SET40 (Maske `0x30`) ergibt
`0x40 & 0x30 = 0` — das Kommando sendet still „keine Änderung“.

**Vorschlag:** `src/setcommands.h` mit der Tabelle als `constexpr`, der
Umrechnung und der Bereichsprüfung; `commands.cpp` behält nur Abonnieren,
Logzeilen und `register_new_command()`. Dazu je Zeile eine Prüfung zur
Übersetzungszeit: Jeder Wert von `min` bis `max` landet nach der Umrechnung in
0–255 und vollständig innerhalb der Maske. Die Hosttests binden den Header
ein; ihre Testvektoren bleiben, die Nachbildungen fallen weg.

**Nachweis:** Vorher/Nachher-Liste aller Paare (SET, Wert) → (Byte, Maske,
Rohwert), erzeugt aus der Tabelle, identisch; alle Hosttests; Gegenprobe mit
absichtlich falschem Parameter → Übersetzung bricht.

### C2 — Compiler-Warnungen abgeschaltet *(mittel)*

**Problem:** [`platformio.ini`](platformio.ini), Zeile 46:
`build_unflags = -Wall`. Mit `-Wall -Wextra` gebaut ergeben sich 21
Warnungen im eigenen Code, **alle harmlos** — ein gutes Zeichen für den Code:

* 17 × `-Wcomment`: Kommentarköpfe ohne `*/` in `src/HeishaMon.cpp`
  (Zeilen 1046–1058, 1093–1096, 1172–1174).
* 3 × `%u` mit `uint32_t` in `src/verbindung.h` (Zeilen 413, 420, 425). Auf
  dem ESP32 ist `uint32_t` `unsigned long`, auf dem Mac `unsigned int` — der
  Hosttest kann diese Warnung deshalb nie zeigen.
* 1 × `-Wformat-truncation` im Zeitstempel (`src/HeishaMon.cpp`, Zeile 239).

**Vorschlag:** `build_src_flags = -Wall -Wextra -Wno-unused-parameter` (nur
eigener Code, Bibliotheken bleiben still), die 21 Stellen bereinigen, danach
`-Werror` für `src/`.

**Nachweis:** `.elf`-Vergleich; Build ohne Warnung.

### C3 — Kommentare erzählen Geschichte *(mittel)*

**Problem:** 45 % der Zeilen in `src/` (ohne `version.h`) sind Kommentar, dazu
2063 Zeilen Changelog. Das „Warum“ ist wertvoll. Aber dieselbe Begründung
steht samt Datum und Messung an vielen Stellen und veraltet an jeder einzeln —
die eigene Regel „doppelte Fakten veralten getrennt“ gilt auch für Kommentare.
Gefundene Beispiele:

* `src/notbetrieb.cpp`, Zeilen 407–426, begründet „Timeout 1,5 s“; direkt
  darunter (428–450) steht „5000 ms — der Entwurf sah 1500 vor“. „Bis zu
  1,5 s“ außerdem in `notbetrieb.h` Zeile 796 und `notbetrieb.cpp` Zeile 950;
  tatsächlich bis rund 30 s.
* `src/webfunctions.cpp`, Zeile 1091: „48 s“ — ein Heizen-Lauf dauert 88 s.
* „bis 111 bei 99 Zeilen“ in `decode.h` (97), `decode.cpp` (555),
  `notbetrieb.cpp` (867) — höchste Nummer ist seit 3.23.0 die 112.
* `src/HeishaMon.cpp`, Zeile 1494, „wie überall im Projekt überlaufsicher“ —
  neben S1 (wird in 3.23.1 richtiggestellt).

**Vorschlag (Regel für die Zukunft):** Ein Kommentar nennt die **geltende**
Regel und ihren Grund in wenigen Zeilen plus Verweis (Version oder Entscheid).
Datierte Entstehungsgeschichte gehört in `version.h` bzw. die Vorhaben-Dateien.
Umsetzung: die Beispiele oben im nächsten Firmware-Branch (P2), danach jede
Datei, wenn sie ohnehin angefasst wird. Kein Gesamtumbau.

### C4 — Formatierung uneinheitlich *(niedrig)*

`src/notbetrieb.cpp`, Zeilen 249–259 und 279–385: Blockinhalt nicht
eingerückt — in der sicherheitsrelevantesten Datei. Header mit vier, `.cpp`
mit zwei Leerzeichen; kein `.clang-format`. **Vorschlag:** `.clang-format`
nach dem vorherrschenden Stil, Prüfung im pre-commit-Hook. Einmal alles
formatieren (Nachweis `.elf`-Vergleich) oder nur Neues — Owner-Entscheid in P7.

### C5 — Kleinpunkte *(niedrig)*

| | Befund | Paket |
| --- | --- | --- |
| a | Toter Zweig: `strcmp(actual_data[index], "unused")` (`webfunctions.cpp`, Zeile 509) — `"unused"` wird nirgends gesetzt | P2 |
| b | `static const`-Tabellen in `notbetrieb.h` landen in jeder Übersetzungseinheit — `inline constexpr` (C++17) | P5 |
| c | `cleanCommand` wird zur Laufzeit aus `mainCommand` kopiert (`HeishaMon.cpp`, Zeile 1365) — die leere Vorlage gehört als `static const` an eine Stelle | P5 |
| d | `/settings`, `/reboot`, `/togglelog`, `/toggledebug` ändern Zustand auch per GET. Mit im Browser gemerkter Basic-Auth genügt eine fremde Seite mit eingebettetem Link (CSRF). **Achtung:** Der Notfallablauf in `Ablauf-Backup-Boards.md` setzt den MQTT-Port per `curl … /settings?mqtt_port=…` — bei Umstellung auf POST dort und auf dem Notfallblatt `curl -d` eintragen | P6 |
| e | `/settings` schreibt gespeicherte Werte unmaskiert in `value='…'` (`webfunctions.cpp`, ab Zeile 711) | P6 |
| f | `write_hex_log`: Kommentar „32 bytes per line does not work“ neben `bytesperline = 32` (`HeishaMon.cpp`, Zeile 429) | P2 |

### D1 — README-Einstieg falsch *(hoch für Dritte)*

* [`README.md`](README.md), „MQTT-Schnittstelle“: „92 State-Topics und 37
  Set-Kommandos“ — der Code hat 99 und 38.
* Dort steht `<präfix>/LWT` und `<präfix>/log`. Tatsächlich
  **`<präfix>/info/LWT`** und `<präfix>/info/log` (`src/Topics.cpp`, Zeilen
  20–21). Die Zweige `info/` und `notbetrieb/` fehlen in der Übersicht.
  [`MQTT-Topics.md`](MQTT-Topics.md) nennt nur „LWT“ und „log“ ohne Pfad.
* Wer nach der README einen Verfügbarkeits-Wächter baut, abonniert ein Topic,
  das es nicht gibt.

**Vorschlag:** korrigieren; `repo_konsistenz_test` um die Zahlen in diesem
Abschnitt (gegen Tabellen im Code) und um die Topic-Wurzeln (gegen
`Topics.cpp`) erweitern — die Prüfung deckt bisher nur `MQTT-Topics.md` ab.

### D2 — Veraltete Stände *(niedrig)*

[`Ablauf-Notbetrieb.md`](Ablauf-Notbetrieb.md), Zeilen 7–9: „3.22.0 … noch
nicht ausgerollt“ — 3.22.0 und 3.23.0 laufen seit Wochen. Die Kopfprüfung im
`repo_konsistenz_test` erfasst nur `Vorhaben-`, `Auftrag-` und
`Arbeitsplan-`Dateien. **Vorschlag:** korrigieren; die Stand-Zeile der
`Ablauf-`Dateien so fassen, dass sie nicht bei jedem Rollout veraltet (z. B.
„gilt ab 3.22.0“ statt „noch nicht ausgerollt“).

### D3 — Widerspruch zum Re-Assert *(mittel)*

`src/HeishaMon.cpp`, Zeilen 812–813, begründet das Verwerfen im
Karenzfenster mit „der Re-Assert der Steuerung holt es binnen 5 Minuten
nach“; `src/verbindung.h`, Zeilen 63–66, nennt sieben Kommandos alle 300 s an
H2 (gemessen 2026-08-21). Die Wächter-Doku in `nodered-flows` sagt dagegen,
der Re-Assert erreiche die WPs **nicht**, weil der mqtt-Adapter nur bei
Wertänderung publiziert (Mitschnitt 2026-07-25). Eins davon stimmt nicht
(mehr). Stimmt die Wächter-Doku, geht ein im Fenster verworfenes *echtes*
Kommando nicht für 5 min, sondern bis zur nächsten Wertänderung verloren — der
`WP_Befehls_Waechter` würde es melden.

**Vorschlag:** in `nodered-flows` nachlesen, welche Kanäle der
Hauptmodus-Verteiler zyklisch und unabhängig vom Adapter sendet; danach die
Kommentare und den README-Abschnitt 3.6.1 an den richtigen Stand anpassen.
Messen nur, wenn die Doku dort nicht eindeutig ist (Aufwand/Nutzen).

### D4 — Umfang und Struktur der Doku *(mittel)*

Rund 16 000 Zeilen Markdown in 27 Dateien, 26 davon im Wurzelverzeichnis.
Die README hat 1366 Zeilen; in ihrer Mitte steht eine zweite
Versionsgeschichte als Erzählung (3.0–3.23) — dort stecken die veralteten
Angaben aus D1. Als Archiv für den Entwickler ist das hervorragend. Für
jemanden aus der Familie, der die Anlage am Laufen halten soll, fehlt im Repo
die **eine kurze Betriebsseite**: Was ist normal, woran erkenne ich ein
Problem, welche Handgriffe gibt es (Notbetrieb, Backup-Board tauschen,
Neustart, wen fragen). Das ausgedruckte Notfallblatt ist nicht im Repo und
wurde nicht geprüft.

**Vorschlag (Owner-Entscheid):** README auf rund 150 Zeilen (Zweck, Aufbau,
Bauen, Betrieb, Verweise); die Geschichte nach `doku/geschichte.md`; Vorhaben,
Arbeitspläne, Aufträge und Analysen nach `doku/vorhaben/`; Abläufe nach
`doku/betrieb/`; neue `doku/betrieb/Betrieb.md` als Einseiter. Der Linktest
fängt jede Verschiebung ab. Ausgedruckte Blätter mit Pfadangaben vorher
prüfen.

---

## Pakete — Schritt für Schritt

Jedes Firmware-Paket läuft nach `Ablauf-Backup-Boards.md`, „Bei jeder
Firmware-Änderung“: Rettungsanker, Branch, Version und Changelog,
`/pruefen-vor-merge`, `/rollout`. Das steht nicht in jedem Paket erneut.

### 3.23.1 — Karenzfenster (S1)

- [x] Nach [`Arbeitsplan-Karenzfenster-3.23.1.md`](Arbeitsplan-Karenzfenster-3.23.1.md)
      abarbeiten, **vor 2026-10-31 04:01 CET** — erledigt 2026-10-15, beide
      Stufen und beide Backups auf 3.23.1; beide Releases angelegt.

### P1 — Doku-Korrekturen (direkt auf `main`, keine Firmware)

- [ ] D1: README „MQTT-Schnittstelle“ — Zahlen, `info/LWT`, `info/log`, Zweige
      `info/` und `notbetrieb/`; `MQTT-Topics.md` mit vollem Pfad.
- [ ] D1: `repo_konsistenz_test` um Zahlen und Topic-Wurzeln der README
      erweitern; Gegenprobe (alte Zahl eintragen → rot).
- [ ] D2: Stand-Zeile in `Ablauf-Notbetrieb.md`; übrige `Ablauf-`Dateien
      auf dieselbe Art veralteter Stände durchsehen.
- [ ] D3: in `nodered-flows` klären, welche Kanäle zyklisch gesendet werden;
      Ergebnis hier eintragen. Die Kommentarkorrektur in `src/` wandert in
      den nächsten Firmware-Branch (P2).
- [ ] `./test/hosttests.sh --schnell` grün, Commit auf `main`.

### P2 — Build-Hygiene (3.23.2, ohne Funktionsänderung)

- [ ] Vorher: `text/data/bss` der `.elf` aller Envs festhalten.
- [ ] S5: Bibliotheken exakt pinnen, CI auf `platformio==6.1.19`.
- [ ] C2: `build_unflags = -Wall` entfernen, `build_src_flags = -Wall -Wextra
      -Wno-unused-parameter`; die 21 Warnungen beheben (`%u` per Cast oder
      `PRIu32` so, dass auch der Hosttest auf dem Mac sauber bleibt).
- [ ] C5a, C5f; die veralteten Kommentare aus C3; Kommentarkorrektur aus D3.
- [ ] Build ohne Warnung, dann `-Werror` in `build_src_flags`.
- [ ] Nachweis: `.elf` gegen vorher — Unterschiede nur dort, wo Code
      tatsächlich wegfiel (C5a) oder die Versionszeichenkette sich änderte;
      `decode_vergleich.py` identisch.
- [ ] Changelog mit Nachweis und GROESSE.

### P3 — `loop()` darf nicht stehen (3.24.0, Prüfling)

- [ ] Eigenen Arbeitsplan anlegen; Prüfling-Ablauf nach `test/README.md`
      (sicherheitskritische Reihenfolge beim Ausleihen eines Backup-Boards).
- [ ] S2: nicht-blockierende Telnet-Ausgabe; Tastenbelegung unverändert;
      Mitschnittwerkzeuge (`telnet_mitschnitt.py`, `produktiv_mitschnitt.py`)
      gegen den neuen Teil prüfen.
- [ ] S2-Nachweis am Prüfling: angehaltener Client, `loop()` läuft weiter;
      Gegenprobe mit der Vorversion.
- [ ] S3: Watchdog 120 s, Fütterung bei OTA; Prüfling-Route für einen
      absichtlichen Hänger (nur Test-Build).
- [ ] S3-Nachweis: OTA über ArduinoOTA **und** `/firmware`; Hänger →
      `TASK_WDT`, `boot_count` + 1, Notbetriebswerte erhalten.
- [ ] Blockadetabelle aus der Durchsicht 2026-09-02 um die neue Grenze
      ergänzen.
- [ ] S6 (optional mitnehmen, sonst P4): Ganzzahl-Regel als Header mit
      Hosttest; `info/set_abgelehnt` und `info/set_abgelehnt_letzter`;
      `MQTT-Topics.md` nachziehen. Zuordnung P3 oder P4 beim Planen
      entscheiden (siehe S6).

### P4 — Serielle Strecke (3.25.0)

- [ ] Vorab mit dem Owner: Wortlaut des neuen Sperrhinweises auf der
      Notfallseite; Schwelle (Vorschlag 30 s); STOPP-Regel H1/H2 gegenprüfen.
- [ ] Zähler und `info/telegram_ok`, `info/telegram_err`, `info/telegram_age`;
      `MQTT-Topics.md` nachziehen.
- [ ] Bestätigungsregel „gültiges Telegramm nach der Mindestwarte“ in
      `notbetrieb.h`, Hosttest mit eingefrorenen Werten (Gegenprobe: alte
      Regel bestätigt, neue nicht).
- [ ] Sperrgrund „keine Verbindung zur Wärmepumpe“, Seite und Statusroute
      (neues Feld **hinten** anhängen — die Startseite liest Index 5 und 6).
- [ ] Prüfling ohne WP: Alter wächst, Sperre erscheint.
- [ ] `Auftrag-…-nodered-flows.md`: Wächter auf `info/telegram_age`.

### P5 — Kommandopfad als Header (ohne Funktionsänderung)

- [ ] Vorher/Nachher-Liste aller (SET, Wert) → (Byte, Maske, Rohwert)
      erzeugen (z. B. über den Tabellenparser in `set_top_zuordnung.py`).
- [ ] C1: `src/setcommands.h` mit `constexpr`-Tabelle, Umrechnung,
      Bereichsprüfung und den Prüfungen zur Übersetzungszeit.
- [ ] Tests auf den Header umstellen, Nachbildungen entfernen, Testvektoren
      behalten; Gegenprobe mit falschem Parameter.
- [ ] C5b, C5c.
- [ ] Nachweis: Liste identisch, alle Hosttests, `decode_vergleich.py`.

### P6 — Weboberfläche (Owner-Entscheid nötig)

- [ ] Entscheid: zustandsändernde Routen nur noch per POST? Dafür
      `Ablauf-Backup-Boards.md` „Im Ernstfall“, die Backup-Pflege und das
      ausgedruckte Notfallblatt auf `curl -d` umstellen — **vor** dem Rollout.
- [ ] C5e: Werte in `/settings` HTML-maskiert ausgeben.

### P7 — Struktur (Owner-Entscheid nötig)

- [ ] D4: Umfang entscheiden (nur Betriebsseite, oder auch README kürzen und
      Ordner); ausgedruckte Blätter auf Pfadangaben prüfen.
- [ ] C4: `.clang-format` und Hook; einmal alles oder nur Neues.
- [ ] C3: Kommentarregel in `CLAUDE.md` aufnehmen.

---

## Ausdrücklich geprüft und in Ordnung

Damit die nächste Durchsicht hier nicht von vorn anfängt:

* **Vorzeichenbehaftete `millis()`-Vergleiche:** genau drei in `src/` — die
  zwei aus S1 und einer in `notbetrieb.cpp` (Zeile 591), der eine kurzlebige
  Frist in einer begrenzten Schleife prüft und damit korrekt ist. Keine
  direkten Vergleiche `millis() < x`.
* **Alle übrigen Zeitrechnungen** (Verbindungswacht, Notbetrieb, KNX,
  Sendefenster, WLAN-Watchdog, Reconnect-Backoff, Info-Takt, Vollupdate)
  rechnen mit unsigned-Differenzen gegen Dauern — überlaufsicher. Die
  Bibliothek `sstaub/Ticker` 4.4.0 ebenso.
* **KNX-Rahmen zerlegen** (`knxtunnel.h`): jede Länge gegen das Datagramm,
  cEMI-Zusatzinformation und NPDU-Länge eingeschlossen; Quelle gefiltert.
* **RTC-Spiegel:** Layout im Magic, Prüfsumme feldweise, jeder Wert erneut
  durch die Grenzen aus `setCommands[]`.
* **Puffer:** alle `snprintf` mit Größe und Längenbegrenzung bei Fremdtext,
  `desc_text()` mit `nullptr`-Abschluss, `readSerial()` und
  `flush_serial_input()` begrenzt.
* **Build mit `-Wall -Wextra`:** nur die 21 harmlosen Warnungen aus C2.
* **Hosttests:** 15/15 grün, 6 s; Konsistenzprüfungen grün.

## Nicht erneut aufgemacht

OTA-Passwort als Vorgabewert (Owner-Entscheid, bestätigt 2026-09-17), K3
`config.json` (geschlossen 2026-09-07), Relais statt KNX, Zone 2, keine
Unity-Suite, der zurückgestellte große Umbau, die blockierenden Hydraulik- und
KNX-Requests (begründet in der Blockadetabelle vom 2026-09-02).
