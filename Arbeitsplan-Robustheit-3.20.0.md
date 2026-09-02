# Arbeitsplan 3.20.0: Robustheit und Langzeitstabilität

Umsetzung von M1–M4 aus `Massnahmenplan-Codedurchsicht-2026-09-02.md`.
Ausgangsstand: 3.19.0, Commit `4a2da3a` auf `main`, alle vier Boards laufen
3.19.0.

## Entscheidungen des Owners (2026-09-02)

1. **Ein Versionsschnitt, nicht drei.** M1–M4 gehen zusammen als 3.20.0 raus:
   ein Prüfstandslauf, ein Rollout auf vier Boards, eine Abnahme, ein Release.
   Der Maßnahmenplan hatte je Maßnahme eine Version vorgesehen; das wären drei
   vollständige Rollout- und Abnahmedurchläufe für Änderungen, die einander
   nicht berühren. Der Preis ist eine unschärfere Zuordnung, falls die Abnahme
   etwas findet — abgefedert dadurch, dass jede Maßnahme ihren eigenen Commit
   bekommt und **M1 der erste ist**: Stockt die RTC-Arbeit, lässt er sich
   allein herauslösen und als 3.20.0 vorziehen.
2. **M1 nach Weg A** (Fenster am Ende von `setup()` neu armieren). Weg B hätte
   die Startreihenfolge verschoben, die seit 3.16.0 unverändert läuft, und die
   NTP-Warnung von MQTT auf Serial/Telnet gezwungen.
3. **Mitfahrer:** K2 (Zeitstempel) und die veralteten Zahlen in den
   Kommentaren. K2 fährt mit, weil M4 `write_mqtt_log()` und
   `write_telnet_log()` ohnehin umbaut und K2 genau deren Zeitstempel ändert —
   getrennt hieße, dieselben Funktionen ein zweites Mal anzufassen und den
   Nachweis zu wiederholen. Der Logring aus M4 bekommt so von Anfang an
   driftfreie Zeiten samt Sommerzeit.
4. **`/log` hinter dem Notbetriebs-Zugang.** Nicht offen wie
   `/notbetrieb/status`: Diese Route gibt echte Meldungstexte heraus, nicht nur
   „Schritt 3 von 7". Der Zugang steht auf dem ausgedruckten Notfallblatt, die
   Familie kommt im Ernstfall also heran, und vom Link auf der Notbetriebsseite
   aus fragt der Browser nicht erneut nach (gleiche Basic-Auth-Anmeldung).

**Bleibt liegen:** K1 (Task-Watchdog für `loop()`) wegen der Nebenwirkung auf
OTA und den HTTP-Updater — der braucht einen eigenen Durchlauf mit eigenem
OTA-Test. K3 (`config.json` atomar) ist unabhängig von allem hier und wartet
auf einen eigenen Anlass.

---

## Zwei Korrekturen am Maßnahmenplan

Beide sind beim Gegenlesen des Codes aufgefallen und ändern, **wie** M3 und M4
gebaut werden — nicht **ob**.

### M4: Der Logring wäre nach 15 Sekunden nutzlos gewesen

`write_mqtt_log()` hat 39 Aufrufstellen. 38 davon sind Ereignismeldungen, die
selten auftreten — der Notbetrieb, die WLAN- und Verbindungsmeldungen, die
Bilanz des Karenzfensters. Die neununddreißigste ist
[decode.cpp:301](src/decode.cpp#L301) und schreibt die `<PUB> TOP…`-Zeile für
**jeden geänderten Messwert in jedem 5-Sekunden-Zyklus**. Vorlauftemperatur,
Durchfluss und Kompressorfrequenz ändern sich praktisch immer; ein Ring über 32
Zeilen enthielte nach wenigen Sekunden ausschließlich Messwerte und hätte genau
die Notbetriebszeilen verdrängt, für die er gebaut wird.

Der Ring darf also nicht in `write_mqtt_log()` selbst hängen, ohne die eine
Aufrufstelle abzutrennen. Gebaut wird deshalb:

* neue Funktion `write_wert_log()` mit dem heutigen Verhalten von
  `write_mqtt_log()` (Publish oder Telnet, kein Ring, kein Fallback) —
  ausschließlich für die `<PUB>`-Zeile in `decode.cpp`,
* `write_mqtt_log()` behält alle 38 Ereignisstellen und bekommt Ring **und**
  Fallback.

Eine geänderte Aufrufstelle. Die Alternative — im Ring auf das Präfix `<PUB>`
zu prüfen — wäre eine Textprüfung an einer Stelle, an der der Compiler nichts
mehr absichert.

### M3: Telemetrie am Vollupdate verstummt genau im Störfall

Der Maßnahmenplan wollte die vier `info/`-Publishes an das bestehende
5-min-Vollupdate in `publish_heatpump_data()` hängen. Diese Funktion wird aber
nur nach einem **gültigen Antworttelegramm** aufgerufen
([HeishaMon.cpp:785](src/HeishaMon.cpp#L785)). Reißt die serielle Strecke ab —
Kabel, Pegelwandler, Wärmepumpe aus —, läuft sie nie wieder, und Uptime, Heap
und Stack wären tot. Ausgerechnet in dem Zustand, in dem man sie ansehen will.

Die vier Publishes bekommen deshalb einen **eigenen 5-min-Takt in `loop()`**,
überlaufsicher gerechnet wie alle Zeitvergleiche im Projekt, mit derselben
Länge `UPDATEALLTIME`. `decode.cpp` bleibt unangetastet und bleibt das, was es
ist: der Dekodierer der Wärmepumpendaten.

---

## Die Commits auf `robustheit-langzeit`

Vor dem ersten: Rettungsanker `rettungsanker-2026-09-02` auf `main`
(annotiert), dann Branch.

### 1 — M1: Karenzfenster am Ende von `setup()` neu armieren

Eine Zeile unmittelbar vor `Send_Pana_Mainquery_Timer.start()`:
`setCommandsIgnoredUntil = millis() + SUBSCRIBE_GRACE;` — mit Kommentar, warum
das Fenster zweimal gesetzt wird. Das in `setupMqtt()` gesetzte bleibt stehen;
es deckt den Reconnect-Pfad ab, der über `mqtt_reconnect()` läuft und diese
Zeile nie sieht.

Bewusst erster Commit: allein herauslösbar, falls der Rest hängt.

### 2 — `src/rtcspiegel.h`, arduino-frei, plus Hosttest

Nach dem Muster von `sendwindow.h`, `verbindung.h` und `notbetrieb.h`: die
Regel wandert in einen Header ohne Arduino-Abhängigkeit, den Firmware und
Hosttest gemeinsam einbinden.

```
struct RtcSpiegel {
    uint32_t magic;
    uint8_t  rolle;                        // NotbetriebRolle
    uint8_t  gesetzt;                      // Bitmaske wie im NotbetriebSpeicher
    uint16_t bootzaehler;                  // sättigt, läuft nicht über
    int32_t  werte[NOTBETRIEB_MAX_WERTE];
    uint32_t pruefsumme;                   // über alles davor
};
```

Dazu `rtc_pruefsumme()`, `rtc_gueltig(sp, erwartete_rolle)` und
`rtc_bootzaehler_erhoehen()` mit Sättigung bei `UINT16_MAX` statt Überlauf.

Hosttest als Erweiterung von `test/notbetrieb_test.cpp` (bindet `notbetrieb.h`
und damit die Wertetabellen ohnehin ein): gültiger Spiegel, falsches Magic,
Bitkipper in `werte[]`, Bitkipper in `gesetzt`, falsche Rolle, `gesetzt`-Maske
mit Bits jenseits der Rollenlänge, Bootzähler an der Sättigungsgrenze.

### 3 — Anbindung M2 und M3

**M2, Spiegel der Notbetriebswerte.** Eine `RTC_NOINIT_ATTR`-Instanz. In
`setup()` vor `notbetrieb_init()` gelesen und geprüft; `notbetrieb_init()`
übernimmt daraus nur, was `rtc_gueltig()` durchlässt, und **jeder einzelne Wert
läuft dabei erneut durch `set_command_range()`** — ein Spiegel aus einer
Firmware mit anderen Grenzen darf nicht durchrutschen. Danach wird der
Bootzähler erhöht und die Prüfsumme neu gerechnet. Nachgeführt wird der Spiegel
bei jeder angenommenen Wertänderung in `notbetrieb_mqtt_annehmen()`.

Die Trennlinie des Owner-Entscheids vom 2026-08-20 bleibt damit exakt erhalten:
kein Flash-Schreibzugriff, keine Datei, nach echtem Stromausfall leer.

**M3, Telemetrie.** Vier Topics unter dem schon vorhandenen `info/`-Zweig
(`Topics.cpp`, `infTopicPrefix`), retained:

| Topic | Inhalt | Wann |
| --- | --- | --- |
| `info/boot` | `esp_reset_reason()` als Text, Bootzähler, Version | einmal je Connect, aus `mqtt_reconnect()` |
| `info/uptime` | Sekunden aus `esp_timer_get_time()` (64 Bit) | eigener 5-min-Takt |
| `info/heap` | `getFreeHeap`, `getMinFreeHeap`, `getMaxAllocHeap` | eigener 5-min-Takt |
| `info/stack` | `uxTaskGetStackHighWaterMark(NULL)` | eigener 5-min-Takt |

Bootzähler und Reset-Ursache zusammen unterscheiden „Strom war weg" von
„Firmware hat neu gestartet" — das ist der ganze Zweck.

### 4 — M4: Fallback, Logring, `/log`

* `write_wert_log()` abspalten, die eine Aufrufstelle in `decode.cpp`
  umstellen (Begründung oben).
* `write_mqtt_log()`: Zeile immer in den Ring; dann publizieren. Ist der Client
  getrennt oder scheitert `publish()`, geht sie mit Zeitstempel an
  `TelnetStream`.
* Ring: 32 Zeilen à 128 Byte, statisch, 4 KB. Kein Heap, keine Allokation im
  Logpfad.
* Route `/log` hinter `notbetrieb_username`/`notbetrieb_password`. Antwort
  **zeilenweise** über `setContentLength(CONTENT_LENGTH_UNKNOWN)` und
  `sendContent()` — ein `String` über 4 KB im Webhandler wäre genau die
  Heap-Spitze, die M3 messen soll.
* Link von der Notbetriebsseite. Neue CSS-Klassen gehen durch
  `test/css_klassen_test.py`.

### 5 — K2: Zeitstempel aus `time()` statt TimeLib

In `write_mqtt_log()`, `write_telnet_log()` und `handle_telnetstream()` auf
`time()` plus `localtime_r()` umstellen. Die Systemuhr wird von SNTP im
Hintergrund nachgeführt und kennt die Zeitzone; TimeLib läuft seit dem Boot
frei auf `millis()`. Prüfen, ob `paulstoffregen/Time` danach vollständig aus
`lib_deps` und `HeishaMon.h` entfallen kann — `SECS_YR_2000` in `setupTime()`
hängt mit daran und braucht dann einen eigenen Ausdruck.

### 6 — Aufräumen und Doku

* Veraltete Zahlen: Tabellenlänge 92 → 99 und Gesamtdeckel 180 s → 200 s in
  [decode.h:97](src/decode.h#L97), [decode.cpp:547](src/decode.cpp#L547),
  [notbetrieb.cpp:82](src/notbetrieb.cpp#L82),
  [notbetrieb.cpp:695](src/notbetrieb.cpp#L695),
  [notbetrieb.h:92](src/notbetrieb.h#L92).
* `MQTT-Topics.md`: der `info/`-Zweig mit allen vier Topics und ihrer Bedeutung.
* `src/version.h`: Changelog 3.20.0 mit Problem, Nachweis und RAM/Flash-Delta
  gegen 3.19.0, Versionsnummer auf 3.20.0.
* `.github/workflows/main.yml`: Begründung für die erweiterte Hosttestliste.

### 7 — Wächter-Entwurf im Nachbarprojekt

Nach dem Muster von `WP_Befehls_Waechter.js` in `nodered-flows`: Alarm, wenn
`info/boot` einen Bootzähler > 1 mit einer Reset-Ursache ungleich Power-on
meldet, und wenn `MinFreeHeap` über Tage fällt. **Nur Entwurf** — der Deploy
läuft über `push.sh` und macht der Owner selbst.

---

## Nachweis am Prüfling

Ein Backup-Board als Prüfling (`h1b` 192.168.2.194 oder `h2b` 192.168.2.166),
über den Test-Prefix, danach zurück in den Backup-Zustand. Alle vier Nachweise
in **einer** Kampagne — das ist der eigentliche Gewinn des gemeinsamen
Versionsschnitts.

**Vorher:** die vollständige Hosttestliste aus `.github/workflows/main.yml`
lokal fahren (jetzt elf statt zehn Tests) und alle sechs Envs bauen. Am
2026-08-23 ist genau dieser Schritt übersprungen worden und `css_klassen_test.py`
fiel erst in der CI auf, als 3.14.0 schon produktiv lief.

| | Nachweis | Erwartung |
| --- | --- | --- |
| **P1** | M1: Set-Werte unter dem Test-Prefix hinterlegen, NTP unerreichbar machen (DNS blocken oder Router trennen), Neustart, roher Telnet-Mitschnitt | keine `<SUB> SET…`-Zeilen direkt nach dem Boot; stattdessen `Verworfen (Wiedereinspielung nach Connect)` und die Bilanzzeile |
| **P2** | M2: Werte einspielen, Broker abschalten, `/reboot` | Seite zeigt den Knopf, `Notbetrieb einsatzbereit` erscheint **nicht** erneut. Gegenprobe: stromlos machen → „Nicht bereit" |
| **P3** | M3: `info/`-Topics am Broker mitlesen über `/reboot` und über Stromlos | Bootzähler zählt beim Software-Reset hoch, springt nach Stromlos auf 1; Reset-Ursache passt zum jeweiligen Fall |
| **P4** | M4: Broker abschalten, Notbetriebslauf auslösen, danach `/log` aufrufen | die Zeilen des Laufs stehen im Ring, mit korrektem Zeitstempel (K2); parallel im Telnet-Mitschnitt sichtbar |

Der Telnet-Mitschnitt läuft über einen rohen Socket auf Port 23 — `telnetlib`
ist ab Python 3.13 entfernt, und `produktiv_mitschnitt.py` zeigt nur
Kommandotelegramme, keine Logzeilen.

## Rollout und Abschluss

1. `tablesnap.py`-Baseline von H1 und H2 unmittelbar vor dem Flash.
2. OTA H1 → Abnahme (zeilenweiser Vergleich, kritische Sollwerte gezielt),
   dann H2 → Abnahme.
3. MQTT-Seite getrennt prüfen: die vier `info/`-Objekte müssen im ioBroker
   ankommen.
4. Backup-Boards `h1b`/`h2b` nachziehen, Stilllegung über Port 1884 prüfen.
5. Rollback-Binaries beider Stufen sichern, bevor der nächste Build sie
   überschreibt.
6. Merge `--no-ff` nach `main`, Tag `v3.20.0`, **einzeln** pushen —
   `git push --follow-tags` schöbe die Rettungsanker-Tags mit hoch.
7. CI prüfen (`gh run list`).
8. Release öffentlich `v3.20.0` mit den Befunden; privat in `HeishaMon-Rollback`
   mit beiden Abbildern **und** `platformio_user_env_v3.20.0.ini`, Titel des
   Vorgängers auf „Rückfallstand vor 3.20.0" umstellen.
9. `Massnahmenplan-Codedurchsicht-2026-09-02.md` um den Erledigungsvermerk
   ergänzen: M1–M4 umgesetzt, K1 und K3 offen.
