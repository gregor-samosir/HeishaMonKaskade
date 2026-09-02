# Arbeitsplan 3.20.0: Robustheit und Langzeitstabilität

Umsetzung von M1–M4 aus `Massnahmenplan-Codedurchsicht-2026-09-02.md`.
Ausgangsstand: 3.19.0, Commit `4a2da3a` auf `main`, alle vier Boards laufen
3.19.0.

## Stand 2026-09-02: der Code ist fertig, die Nachweise stehen aus

Rettungsanker `rettungsanker-2026-09-02` auf `4a2da3a`, Branch
`robustheit-langzeit`. **Alle sieben Commits sind gebaut**, alle sechs Envs
übersetzen, alle elf Hosttests laufen grün:

| | Commit | Inhalt |
| --- | --- | --- |
| ✔ | `1be74ad` | dieser Arbeitsplan |
| ✔ | `feceeab` | M1 — Karenzfenster am Ende von `setup()` |
| ✔ | `a26c2b9` | M2/M3 — `src/rtcspiegel.h` + Hosttest (42 Zusicherungen) |
| ✔ | `df1b124` | M2/M3 — Anbindung, `info/`-Zweig mit acht Topics |
| ✔ | `bbb49b3` | K2 — Zeitstempel aus der Systemuhr, TimeLib entfällt |
| ✔ | `e989e1d` | M4 — Logring, Telnet-Rückfall, Route `/log` |
| ✔ | `32c048d` | Changelog 3.20.0, `MQTT-Topics.md`, veraltete Zahlen |
| ✔ | `nodered-flows fd67900` | Wächter-Entwurf, Branch `bridge-waechter` |

**Stand nach der Kampagne:** P1–P4 sind gefahren und bestanden (Ergebnisse
ganz oben), dazu kam Commit `b523a6b` mit einer am Gerät gefundenen
Korrektur. **Offen ist nur noch der Rollout** auf die vier Boards und die
beiden Releases. `main` ist bis dahin unverändert.

Größe gegen 3.19.0 (`heishamon_esp32_h1_ota`, Vergleichsbau aus dem
Rettungsanker-Tag): RAM 57528 → 61608 Byte (+4080, davon 4096 der Logring —
der Rest hebt sich mit dem Wegfall von TimeLib auf), Flash 1211105 → 1213049
Byte (+1944).

Drei Dinge sind beim Bauen aufgefallen und haben den Plan berichtigt; sie
stehen im jeweiligen Commit ausführlich und hier nur als Merkposten:

* Der Logring durfte nicht an `write_mqtt_log()` allein hängen (die
  `<PUB>`-Zeile hätte ihn geflutet) — deshalb `write_wert_log()`.
* Die `info/`-Telemetrie durfte nicht am Vollupdate hängen (das läuft nur nach
  einem gültigen Antworttelegramm) — deshalb ein eigener Takt.
* `strftime` kostete 12 KB Flash, weil newlib den Locale-Apparat mitzieht —
  deshalb `snprintf` mit den `tm`-Feldern.

## Ergebnis der Prüfstandskampagne (2026-09-02, Prüfling h1b)

**Alle vier Prüfpunkte bestanden.** Prüfling war das Backup-Board `h1b`
(192.168.2.194) über den Test-Prefix `panasonic_heat_pump32`, ohne
Wärmepumpe, mit `hydraulik_switch` auf einem Tasmota-Ersatz am Mac statt auf
der echten `192.168.2.180`. Rückgabe nach Abschnitt 4.3 des Vorhabens
durchgeführt und gegengeprüft: Stufe 1, 3.19.0, Port 1884, Hostname
`HeishaMon32_h1b`, Hydraulik wieder auf `192.168.2.180`. `h2b` blieb
unangetastet.

### P1 — das Karenzfenster: bestanden, mit einer Berichtigung des Berichts

Der Nachweis im Maßnahmenplan (»NTP unerreichbar machen, Neustart«) **kann
den Befund nicht auslösen**, und das ist die wichtigste Erkenntnis dieser
Kampagne:

* **Die Systemuhr überlebt `ESP.restart()`.** Nach `/reboot` und nach jedem
  OTA findet `setupTime()` sofort eine plausible Zeit und kehrt zurück, ohne
  zu warten. Gemessen: Neustart 10:29:16, erste Logzeile 10:29:26 mit
  korrekter Uhrzeit, bei unroutbarem Zeitserver. M1 ist damit
  **ausschließlich ein Kaltstart-Thema**. Ein Prüfer, der dem Plan folgt,
  bekäme grün und hätte den Fix nie ausgelöst — genau das ist beim ersten
  Versuch passiert.
* **Auch der Kaltstart mit voller 30-s-Frist löst ihn nicht aus** — hier
  aber aus einem zweiten Grund: Der ioBroker-MQTT-Adapter trennt stille
  Verbindungen nach rund 30 s von selbst (unabhängig am eigenen Abonnenten
  nachgemessen). Das Board verliert die Verbindung während der NTP-Wartezeit,
  verbindet in `loop()` neu, und der frische `subscribe` armiert das Fenster
  von selbst. Der Satz »Der Keepalive des Brokers rettet hier nicht« im
  Maßnahmenplan **trifft nicht zu**.
* **Der Befund ist trotzdem real.** Mit `NTPTIMEOUT` auf 10 s — `setup()`
  endet dann unterhalb der Trennschwelle des Brokers, die Verbindung bleibt
  stehen, das Fenster ist echt abgelaufen — lief der Schwall vollständig
  durch: 15 `<SUB> SET…`-Zeilen im Log, darunter `SET5
  Z1HeatRequestTemperature: 40`, `SET27/28` (die Kurvenpunkte) und `SET1
  Heatpump: 0`. Das ist der Fall aus 3.6.1, nachgestellt.
* **Gegenprobe mit 3.20.0 unter identischen Bedingungen: 0 ausgeführte
  Kommandos**, 31 einzeln als `Verworfen (Wiedereinspielung nach Connect)`
  protokolliert, dazu die Bilanzzeile.

**Was daraus für den Fix folgt:** Er bleibt richtig und nötig, aber seine
Begründung verschiebt sich. Er schützt nicht gegen etwas, das hier täglich
passiert, sondern macht die Firmware unabhängig davon, ob ein Broker stille
Verbindungen abräumt. Ein ioBroker-Update, ein anderer Broker oder ein NTP,
das schon nach 12 s aufgibt, nehmen diese Rettung weg — und dann steht der
Vorlauf-Sollwert wieder auf einem Monate alten Wert.

Nebenbefund: **37 ist die Zahl aller abonnierten Set-Topics**, nicht die der
hinterlegten Werte. Der Adapter spielt jedes abonnierte Topic ein, auch ohne
gespeicherten Wert — die vier Notbetriebswerte kommen dabei mit leerem
Payload und werden korrekt als »keine Zahl« verworfen.

### P2 — die Notbetriebswerte über den Neustart: bestanden

* Vier Kurvenwerte eingespielt, `fehlend = 0`. Nach `/reboot` weiterhin
  `fehlend = 0` — und im Logring steht **kein** erneutes »Notbetrieb
  einsatzbereit«, weil der Satz beim ersten wiedereingespielten Wert schon
  vollständig war. Genau das Abnahmekriterium aus dem Maßnahmenplan.
* **Der Ernstfall, gemessen:** MQTT-Server auf eine tote Adresse, Neustart —
  `fehlend = 0`, `Sperre = 0`, **der Knopf ist bereit, ohne Broker**. Vor
  3.20.0 stünde hier »Nicht bereit – es fehlen Werte«.
* **Gegenprobe Stromausfall:** `fehlend = 1`, `Sperre = 1`, Bootzähler zurück
  auf 1, Ring leer. Die Trennlinie des Owner-Entscheids vom 2026-08-20 sitzt
  exakt dort, wo sie sitzen soll.
* **Die Rollenprüfung greift mitgemessen:** Beim Umbau des Prüflings von
  Heizen auf Warmwasser wurde der Spiegel als ganzer verworfen (Bootzähler
  auf 1, `fehlend = 1`) — ein Wertesatz der anderen Stufe kommt nicht durch.

### P3 — die Telemetrie: bestanden

Alle acht Topics stehen am Broker. Erste echte Zahlen, zugleich die
Eichgrundlage für die Schwellwerte des Wächters:

| Topic | Wert (frischer Start, ohne Wärmepumpe) |
| --- | --- |
| `heap` | 216 616 |
| `heap_min` | 214 000 – 216 392 |
| `heap_maxblock` | 172 020 |
| `stack` | 4 184 von 8 192 |

**Die offene Annahme aus dem Commit ist damit bestätigt:**
`uxTaskGetStackHighWaterMark` liefert **Bytes** — 4 184 von 8 192 ist
plausibel, Wörter wären mehr als der Stack selbst.

Der Bootzähler zählt wie entworfen: Kaltstart → 1, OTA (Software-Reset) → 2
mit Grund `SW`, `/reboot` → 3, Rollenwechsel → zurück auf 1.

### P4 — der Logring: bestanden

Ein Notbetriebslauf **mit** Broker und einer **ohne**. In beiden Fällen trägt
`/log` den vollständigen Lauf:

```
[2026-09-02 10:55:49] NOTBETRIEB ausgeloest ueber die Weboberflaeche
[2026-09-02 10:55:49] Notbetrieb Schritt 1/6: Hydraulik 1-stufig
[2026-09-02 10:55:49] Notbetrieb: Hydraulik stand bereits auf 1-stufig
[2026-09-02 10:55:58] Notbetrieb Schritt 2/6: ForceHeater = 0
[2026-09-02 10:55:58] <SUB> SET39 ForceHeater: 0
[2026-09-02 10:56:18] Notbetrieb ROT: Schritt 2/6 (ForceHeater) kam nicht zurueck
```

Beim Lauf ohne Broker kamen dieselben sechs Zeilen **zusätzlich über den
Telnet-Rückfall** — vor 3.20.0 wären sie spurlos verschwunden. Der leere Ring
meldet sauber »(noch keine Meldung seit dem Start)«. Der Abbruch nach Schritt
2 ist korrekt: ohne Wärmepumpe kommt kein Rücklesewert, Abbruchgrund 1
(`TIMEOUT`), nicht Hydraulik.

### Zwei Befunde an der neuen Firmware

1. **UTC-Zeitstempel vor `setupTime()`** — behoben in `b523a6b` und am Gerät
   nachgeprüft. Die erste Logzeile eines Boots entsteht in `setupMqtt()`, also
   bevor `configTzTime()` die Zonenregel setzt; im Ring sah das aus wie ein
   Sprung zwei Stunden zurück. Vor 3.20.0 fiel es nicht auf, weil die
   TimeLib-Uhr dort noch auf 1970 stand und die Zeile erkennbar unbrauchbar
   war.
2. **Ein einmaliger `WDT`-Reset direkt nach einem OTA** (Bootzähler 2 → 3,
   `boot_reason = WDT`). Beim nächsten regulären Neustart trat er nicht wieder
   auf, der Zähler ging normal weiter. Nicht reproduziert, nicht erklärt —
   **beim Rollout gezielt darauf achten**; genau dafür gibt es jetzt
   `info/boot_reason`.

### Was im ioBroker zurückbleibt

24 Objekte unter `mqtt.0.panasonic_heat_pump32.*` (10 × `info`, 5 ×
`notbetrieb`, 8 × `set`, plus `LWT`). Der Testbaum bleibt nach
Owner-Entscheid ohnehin stehen; die Objekte sind über den Admin zu löschen,
die simple-api kann das nicht.

---

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
