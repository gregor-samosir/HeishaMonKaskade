# Arbeitsplan: `loop()` darf nicht stehen (3.24.0)

> **Ergebnis: offen (angelegt 2026-10-16) — noch nichts umgesetzt.** Paket P3
> der Codedurchsicht vom 2026-10-15
> ([`Massnahmenplan-Codedurchsicht-2026-10-15.md`](Massnahmenplan-Codedurchsicht-2026-10-15.md):
> S2 und S3, K1 vom 2026-09-02). Eine Firmware-Änderung mit Funktionswirkung:
> Der Nachweis läuft am **Prüfling**, nicht an der Anlage.
>
> **Übergabe:** Der Plan führt vom Rettungsanker bis zum Release und ist für den
> Einstieg in einer neuen Session geschrieben. Zuerst „Lies zuerst“, dann
> Schritt 1.

## Worum es geht

Alles in der Firmware läuft in **einem** Task (`loop()`): WebServer,
MQTT-Keepalive, OTA, Telnet, Abfragezyklus zur Wärmepumpe, Notbetrieb. Steht
`loop()`, steht alles — auch der Notbetriebsknopf, also das Werkzeug für den
Fall, dass niemand da ist. Zwei Befunde, ein Thema:

* **S2, ein Telnet-Client kann `loop()` anhalten.** `TelnetStream` schreibt über
  `NetworkClient::write()` des Cores. Ist der Sendepuffer des Clients voll,
  wartet das bis zu 10 × 1 s **je Logzeile** (`WIFI_CLIENT_MAX_WRITE_RETRY`,
  `WIFI_CLIENT_SELECT_TIMEOUT_US`). Die Bibliothek (`ServerTemplate::write`)
  ruft das **für jeden der bis zu fünf verbundenen Clients nacheinander** auf;
  im Höchstfall sind das 50 s je Zeile. Port 23 hat keine Anmeldung. Das
  Debug-Log ist ab Werk an.
* **S3, kein Watchdog für `loop()`.** Der Task-Watchdog überwacht nur den
  Idle-Task von CPU 0; der `loopTask` ist nicht angemeldet. Hängt `loop()`
  irgendwo (S2 ist ein konkreter Weg), heilt das nichts: Jemand muss in den
  Keller und den Stecker ziehen — das Board hängt am WP-Kabel.

## Was die Prüfung der Quellen am 2026-10-16 ergeben hat

Über den Maßnahmenplan hinaus, jeweils im Quelltext nachgesehen (Core 3.3.11,
`TelnetStream` 1.3.0, `NetApiHelpers` 1.0.2):

1. **S2 stimmt, ist aber breiter als beschrieben** (alle Clients nacheinander,
   siehe oben).
2. **Der im Maßnahmenplan vorgesehene Nachweis wäre eine Messfalle.** Er sagt
   „`nc`, Ctrl-Z, 10 min warten“. Der Sendepuffer des ESP ist klein
   (`CONFIG_LWIP_TCP_SND_BUF_DEFAULT` 5744 Byte), aber ein angehaltenes `nc` auf
   dem Mac **bestätigt weiter** und nimmt den Strom in den Empfangspuffer des
   Kernels (grob 128 KB und mehr). Bei rund 120 Byte/s Debug-Log dauert es weit
   über 20 Minuten, bis der ESP blockiert — nach 10 Minuten wäre das Ergebnis
   „läuft weiter“, ohne dass der Fall je eingetreten ist. (Ein zugeklappter
   Laptop, der gar nichts mehr bestätigt, füllt den Puffer dagegen in rund einer
   Minute.) Der Nachweis braucht deshalb einen Testclient mit **winzigem
   Empfangspuffer**, der nie liest, und er muss **zuerst gegen die Vorversion den
   Stillstand zeigen**. Siehe Schritt 3 und die Memory „Messfallen am Prüfling“.
3. **Der Task-Watchdog hat einen einzigen, globalen Timeout.** 120 s für den
   `loopTask` heißt auch 120 s statt 5 s für die Überwachung des Idle-Tasks von
   CPU 0 (`CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0`). Der Interrupt-Watchdog
   (300 ms) bleibt unberührt. Die Lockerung ist der Preis; sie ist vertretbar,
   weil die Hänger, die diese 5 s fingen, in dieser Firmware nie aufgetreten
   sind, und weil `loop()` auf CPU 1 läuft.
4. **Nebengewinn:** Die Konfiguration schreibt bei einem Task-Watchdog-Panic
   einen Coredump in die vorhandene 64-KB-Partition
   (`CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=y`). Ausgelesen wird er nirgends; das
   bleibt außerhalb dieses Plans, ist aber nach dem ersten echten Reset die
   ergiebigste Spur.
5. **OTA füttern geht:** `Update::onProgress()` existiert im Core, wird je
   geschriebenem Block aufgerufen und deckt ArduinoOTA und den HTTP-Updater ab
   (beide schreiben über `Update`). Die `onProgress`-Lambdas von ArduinoOTA sind
   heute leer.
6. **Ein eigener Telnet-Teil macht zwei Bibliotheken überflüssig:**
   `TelnetStream` und die von ihr nachgezogene `NetApiHelpers` (in 3.23.2 als
   Pin eingetragen, E2 dort). Beide Zeilen in `platformio.ini` entfallen.

## Lies zuerst — in dieser Reihenfolge

1. [`CLAUDE.md`](CLAUDE.md) — harte Regeln 4 (Rettungsanker), 5 (kein Zugriff auf
   die produktiven Geräte ohne Auftrag) und 6 (alle Envs, alle Hosttests).
2. [`Massnahmenplan-Codedurchsicht-2026-10-15.md`](Massnahmenplan-Codedurchsicht-2026-10-15.md),
   Befunde S2 und S3, Paket P3.
3. [`Massnahmenplan-Codedurchsicht-2026-09-02.md`](Massnahmenplan-Codedurchsicht-2026-09-02.md),
   K1 und die Tabelle „Blockadezeiten in `loop()`“ — die Zeiten, über denen der
   Watchdog liegen muss.
4. [`test/README.md`](test/README.md), „Prüfstand aufsetzen — ein Backup-Board
   leihen“ (die **Reihenfolge beim Zurückgeben ist sicherheitskritisch**) und
   „Antwortquote messen“.
5. [`src/HeishaMon.cpp`](src/HeishaMon.cpp): `write_telnet_log`,
   `handle_telnetstream`, `setupOTA`, Ende von `setup()`. Dazu
   [`src/sendwindow.h`](src/sendwindow.h) und
   [`test/sendwindow_test.cpp`](test/sendwindow_test.cpp) als Muster für die
   neue Regel (arduino-frei, `uint32_t`, Überlauf im Hosttest).
6. Memory „Messfallen am Prüfling“ (halboffene Telnet-Sitzung, Systemuhr,
   Python-Pufferung) und „Nachweis: Umbau ohne Funktionsänderung“.
7. Die Skills `/pruefen-vor-merge` und `/rollout`.

## Entschieden — nicht neu aufmachen

| Nr. | Frage | Entscheid | Wann |
| --- | --- | --- | --- |
| E1 | Umfang von 3.24.0 | **Nur P3** (S2 und S3). S6 und P4 folgen gemeinsam als 3.25.0 | Owner 2026-10-16 |
| E2 | Mehrere Telnet-Verbindungen | **Ein Client; ein neuer verdrängt den alten** | Owner 2026-10-16 |
| E3 | Form | Eigener Arbeitsplan nach dem Muster von 3.23.1 (diese Datei) | Owner 2026-10-16 (Auftrag) |
| E4 | Timeout des `loop()`-Watchdogs | **120 s** | Owner 2026-10-16 (V1 wie vorgeschlagen) |
| E5 | Wann trennt der Telnet-Teil einen Client? | **15 s ohne gelungenes Schreiben** (Zeitregel), nicht „20 Fehlversuche“ | Owner 2026-10-16 (V2) |
| E6 | Hänger-Route für den Nachweis | **Nur im Prüfling-Build**, hinter dem Notbetriebs-Login | Owner 2026-10-16 (V3) |
| E7 | Abgeschnittene Zeilen | **Hinnehmen**; danach eine `<INF>`-Zeile mit der Zahl der fehlenden Zeilen | Owner 2026-10-16 (V4) |
| E8 | Prüfling | **`h1b`**, Freigabe durch den Owner am 2026-10-16; `mqtt_port` bleibt für Phase A–C bei 1884 (Telnet und HTTP brauchen keinen Broker) | Owner 2026-10-16 |

*V1–V4 aus dem Entwurf hat der Owner am 2026-10-16 wie vorgeschlagen bestätigt; sie stehen als E4–E7 in der Tabelle oben. Die Begründungen aus dem Entwurf:*

* **E4 (120 s):** längste legitime Blockaden Hydraulikschritt bis 30 s, KNX 20,5 s; WiFiManager-Portal (180 s) und NTP (30 s) liegen in `setup()` vor dem Einschalten. Ab etwa 15 s fehlt dem Broker der Keepalive — die andere, bekannte Grenze.
* **E5 (15 s):** Eine Zeitregel hängt nicht davon ab, wie dicht geloggt wird (mit Hexlog sind 20 Zeilen in einer Sekunde da, im Leerlauf dauern sie über eine Minute).
* **E6:** dieselbe Regel wie beim KNX-Prüfzugang (`KNX_PRUEFZUGANG`): nie in den Stufen.
* **E7:** besser eine zerschnittene Logzeile als ein stehender `loop()`.

### Was aus dem Code folgt — kein Entscheid, aber festgelegt

* **Neuer Header `src/telnetlog.h`**, arduino-frei: die Regeln (wann gilt ein
  Client als tot, wie wird gezählt), mit Hosttest. `src/telnetlog.cpp` hält nur
  den dünnen Teil mit `WiFiServer`, Socket und `send(…, MSG_DONTWAIT)`.
* **Objektname `TelnetStream` bleibt** (gleiche Aufrufe `printf`, `println`,
  `available`, `read`, `flush`, `stop`, `begin`), damit `HeishaMon.cpp` und die
  übrigen Quellen unverändert bleiben. Die Bibliothek fällt aus `lib_deps`.
* **Tasten `L D H M W I C R` bleiben** mit denselben Antworttexten.
* **H1 und H2 gleich** — keine Abweichung zwischen den Stufen (STOPP-Regel nicht
  berührt).
* **Version 3.24.0:** Fehlerbehebung mit neuer Komponente (Telnet-Teil), daher
  Minor.
* **Kein neues Topic.** `info/boot_reason` und `info/boot_count` gibt es seit
  3.20.0.

## Arbeitsschritte

Jeder Schritt endet mit seinem Prüfkriterium. Gerätezugriffe, die etwas
verändern, stehen **einzeln** in einem Bash-Aufruf und werden vorher angesagt
(**HALT** = auf das Ok des Owners warten). Die produktiven Geräte werden in
diesem Plan bis zum Rollout nicht angefasst.

### Phase A — Vorbereitung

1. **Rettungsanker und Branch** (`Ablauf-Backup-Boards.md`, „Bei jeder
   Firmware-Änderung“, Schritt 1). `rettungsanker-2026-10-16` ist vergeben:
   ```bash
   git tag -a rettungsanker-vor-loop-stillstand-2026-10-16 -m "Stand vor 3.24.0 (S2/S3)"
   git cat-file -t rettungsanker-vor-loop-stillstand-2026-10-16   # muss "tag" sein
   git checkout -b loop-stillstand
   ```
   *Prüfkriterium:* annotierter Tag, Branch ≠ `main`.

2. **Baseline für den Größenvergleich:** alle Envs bauen, `text/data/bss` jeder
   `.elf` festhalten (Abschnitt „Messwerte“). Diesmal ändert sich der Code
   absichtlich; der Vergleich dient nur dem GROESSE-Eintrag.

3. **Den Fehler zuerst am Prüfling sehen — mit der Vorversion.** Erst wenn
   3.23.2 den Stillstand zeigt, hat der spätere Nachweis Aussagekraft.
   1. **HALT:** Owner leiht ein Backup-Board (`test/README.md`, „Ausleihe“;
      immer nur eines; vorher bedenken, ob die Stufe gerade ihren Notanker
      braucht). Prüfling-Build der **Vorversion** aufspielen.
   2. **Messwerkzeug `test/telnet_haenger.py`** anlegen (Standardbibliothek,
      Muster `telnet_mitschnitt.py`): verbindet sich auf Port 23 mit
      `SO_RCVBUF` von ~1 KB **vor** dem `connect()`, schaltet mit `H` das Hexlog
      ein (hebt die Lograte), liest danach **nie** wieder und fragt parallel alle
      2 s die Startseite des Prüflings ab. Ausgabe: je Abfrage Antwortzeit oder
      Timeout, am Ende die größte Antwortzeit. Mit `--nur-lesen` (liest normal)
      als Gegenprobe der Gegenprobe.
   3. Lauf gegen 3.23.2.
   *Prüfkriterium:* Startseite antwortet länger als 10 s oder gar nicht,
   solange der Testclient hängt. **Bleibt es grün, ist die Messung falsch**
   (Puffer nicht voll geworden), nicht der Befund erledigt — dann Puffergröße
   und Lograte prüfen, bevor irgendetwas gebaut wird.

### Phase B — Code (Kommentare und Serial-Ausgaben ASCII, ohne Umlaute)

4. **`src/telnetlog.h`** (Regeln, Kopfkommentar nach dem Muster `sendwindow.h`:
   Befund S2, warum nicht blockieren, warum Zeitregel statt Zählregel). Inhalt:
   * `TELNET_STILL_MAX_MS 15000u`
   * Zustand je Client: Zeitpunkt des letzten gelungenen Schreibens, Zähler
     verworfener Zeilen.
   * `telnet_schreiben_gelungen(z, jetzt)` und `telnet_schreiben_verworfen(z,
     jetzt)` führen nach; `telnet_trennen(z, jetzt)` antwortet, ob der Client
     tot ist: unsigned Differenz gegen die Dauer, **keine** Frist, die nach
     Ablauf weiter verglichen wird (Lehre aus S1).
   * Nullzeiger werden abgefangen; Zähler sättigen statt zu überlaufen.
   *Prüfkriterium:* übersetzt im Hosttest ohne Warnung (`-Wall -Wextra`).

5. **`src/telnetlog.cpp` und `.h`-Anbindung:** Klasse mit denselben Methoden wie
   `TelnetStreamClass`, ein `WiFiServer` auf 23, **ein** Client.
   * `begin()`, `stop()`, `available()`, `read()`, `peek()`, `flush()`.
   * `write(buf, len)`: kein Client → 0, nichts puffern (wie bisher, siehe
     Memory „Messfallen“). Sonst `send(fd, …, MSG_DONTWAIT)`. `EAGAIN` oder
     Teilschreiben → Rest verwerfen, zählen; Fehler ≠ `EAGAIN` → Client trennen.
     **Kein** `select`, kein Warten.
   * Neue Verbindung **verdrängt** den alten Client (E2), geprüft höchstens alle
     100 ms im Schreib- und im Lesepfad — der Aufruf kommt aus `loop()`
     (`handle_telnetstream`), der Abfrageweg bleibt dort.
   * Nach `telnet_trennen()` den Socket schließen und zählen.
   * Nach einer gelungenen Ausgabe mit offenem Zähler: eine `<INF>`-Zeile
     „N Zeilen verworfen“ (V4).
   *Prüfkriterium:* alle Envs bauen ohne Warnung (`-Werror` aktiv seit 3.23.2).

6. **`platformio.ini`:** `jandrassy/TelnetStream@1.3.0` und `NetApiHelpers@1.0.2`
   samt ihrer Kommentare aus `lib_deps` entfernen; den Kommentar dort um den Satz
   ergänzen, dass Telnet seit 3.24.0 ein eigener Teil ist. `grep -rn
   "TelnetStream.h\|NetApiHelpers" src/ platformio.ini` darf nur den neuen Header
   finden. *Prüfkriterium:* ein frischer Build (`pio pkg list`) zieht keine der
   beiden Bibliotheken mehr.

7. **S3, Watchdog in `src/HeishaMon.cpp`:**
   * Am Ende von `setup()`, **hinter** dem zweiten `karenz_oeffnen()` und vor
     `loop()`: `esp_task_wdt_reconfigure()` auf 120 s (`idle_core_mask` CPU 0,
     `trigger_panic = true`, **Rückgabewert prüfen**), danach `enableLoopWDT()`.
     Scheitert eines von beiden: `Serial`-Zeile und weiterlaufen — ein fehlender
     Watchdog ist der heutige Zustand, kein Grund, das Gerät zu stoppen.
   * `Update.onProgress([](size_t, size_t) { feedLoopWDT(); })` und denselben
     Aufruf in den leeren `ArduinoOTA.onProgress`-Lambdas (`setupOTA`).
   * Konstante und Begründung (Tabelle der Blockaden, 120 > 30 s) in einen
     Kommentarblock **an einer Stelle**; ein `static_assert` gegen die
     vorhandenen Dauerkonstanten (Hydraulik-Lesefrist, KNX), soweit sie als
     Konstanten existieren — die übrigen stehen in der Tabelle.
   * **Zeitliche Falle ausschreiben:** Der Watchdog darf erst nach `setup()`
     laufen; `loop()` startet Sekunden später als `enableLoopWDT()` im selben
     Task, der erste `esp_task_wdt_reset()` kommt aus dem Core vor dem ersten
     `loop()`.
   *Prüfkriterium:* alle Envs bauen ohne Warnung.

8. **Prüfling-Route** `/pruefen/haenger` in `HeishaMon.cpp` unter
   `#ifdef LOOP_PRUEFZUGANG`, Flag in `stage_test_esp32` (V3), hinter dem
   Notbetriebs-Login. `?s=<Sekunden>` blockiert `loop()` so lange mit einer
   Schleife **ohne** Fütterung und kehrt dann zurück (belegt die Grenze nach
   oben: 90 s darf **nicht** auslösen); ohne Parameter hängt sie endlos (belegt
   den Reset). Obergrenze für `s` abfragen und klemmen.
   *Prüfkriterium:* die Stufen-Envs enthalten die Route nicht
   (`strings build_output/firmware/…h1_ota.bin | grep -c pruefen/haenger` = 0).

9. **Blockaden durchsehen:** alle `delay(`, `while`- und Warteschleifen,
   die aus `loop()` erreichbar sind, gegen die 120 s halten. Ergebnis als Zeile
   je Fund in die Blockadetabelle (Schritt 13). *Prüfkriterium:* keine
   Blockade über 60 s ohne Eintrag.

### Phase C — Tests

10. **`test/telnetlog_test.cpp`** (bindet nur `src/telnetlog.h` ein, Aufbau wie
    `sendwindow_test.cpp`):
    1. Anfangszustand: nie tot, auch nicht bei `jetzt = 0`.
    2. Grenze auf die Millisekunde: gelungenes Schreiben bei 1000 →
       nicht tot bei 15999, tot bei 16000.
    3. Ein gelungenes Schreiben setzt die Uhr zurück, auch nach vielen
       verworfenen Zeilen.
    4. Überlauf: letztes Schreiben bei `0xFFFFF000`, Prüfung nach der Naht.
    5. **Die Naht bei 2³¹ ms** (Lehre S1): letztes Schreiben, dann keine
       Nachführung; Prüfung bei `+ 2³¹ − 1`, `+ 2³¹`, `+ 2³² − 1`. Gegenbeispiel
       mit der vorzeichenbehafteten Regel als lokale Funktion.
    6. Zähler sättigen bei `UINT32_MAX`.
    7. Nullzeiger → kein Absturz.
    *Prüfkriterium:* grün mit `c++ -std=c++17 -O2 -Wall`. Gegenprobe von Hand:
    Grenze um 1 verschoben und die vorzeichenbehaftete Regel eingebaut → je ein
    Fall wird rot; Ergebnis im Changelog.

11. **Listen nachziehen:** `test/hosttests.sh` (`cpp_test telnetlog_test` mit
    Begründung; `test/telnet_haenger.py` in `HARDWARE`), `test/README.md`
    (Werkzeugtabelle: beide Dateien; Abschnitt zu S2 mit der Puffer-Falle),
    `./test/hosttests.sh` grün.

### Phase D — Nachweis am Prüfling

12. **Erwartung vorab festhalten**, dann messen:
    1. **S2:** `telnet_haenger.py` gegen 3.24.0 — größte Antwortzeit der
       Startseite unter 1 s, `info/uptime` und die State-Zeitstempel laufen
       weiter, solange der Testclient hängt. Nach spätestens 15 s ist der Client
       getrennt; ein **neuer** Mitschnitt (`telnet_mitschnitt.py`) kommt sofort
       durch und bekommt die `<INF>`-Zeile „N Zeilen verworfen“.
    2. **Tasten:** `L D H M W I C R` einzeln, Antworttexte wie in 3.23.2
       (`R` verweist auf `/reboot`).
    3. **Mitschnittwerkzeuge:** `telnet_mitschnitt.py` und
       `produktiv_mitschnitt.py` je einige Minuten — die `<DBG>`-Zeile im 5-s-Takt
       kommt, die Stille-Erkennung schlägt nicht an.
    4. **Zweite Verbindung verdrängt die erste:** zwei Sitzungen nacheinander,
       die erste meldet das Ende.
    5. **S3, OTA:** ein OTA über ArduinoOTA und eines über `/firmware`, beide
       laufen durch. Dazu — Gegenprobe — den Upload künstlich auf über 120 s
       strecken (langsamer Client, z. B. `curl --limit-rate`); er darf **nicht**
       abbrechen, weil `Update.onProgress` füttert.
    6. **S3, Grenze nach oben:** `/pruefen/haenger?s=90` → Gerät läuft danach
       normal weiter, `info/boot_count` unverändert.
    7. **S3, Reset:** Vorher über `mqtt_pub.py` Notbetriebswerte auf das
       Test-Prefix einspielen (`/notbetrieb/status` zeigt `fehlendMaske` 0).
       Dann `/pruefen/haenger` ohne Parameter: nach rund 120 s Neustart,
       `info/boot_reason` = `TASK_WDT`, `info/boot_count` + 1, die Werte sind
       erhalten (`fehlendMaske` weiter 0, RTC-Spiegel). **Steht dort `PANIC`
       statt `TASK_WDT`,** entscheidet die Messung, ob es der Watchdog war
       (Zeitpunkt nach 120 s, nicht früher); die Erwartung im Plan wird dann
       korrigiert, nicht der Nachweis zurechtgebogen.
    *Prüfkriterium:* alle sieben Punkte erfüllt. Sonst **HALT** — nicht
    ausrollen.

13. **Blockadetabelle nachziehen** in
    [`Massnahmenplan-Codedurchsicht-2026-09-02.md`](Massnahmenplan-Codedurchsicht-2026-09-02.md)
    („Blockadezeiten in `loop()`“): Zeile für Telnet (neu: nie), die Grenze
    120 s und die Ergebnisse aus Schritt 9; K1 mit Nachtrag „umgesetzt in
    3.24.0“.

14. **Prüfling zurückgeben** — Reihenfolge sicherheitskritisch (`test/README.md`):
    **erst `mqtt_port` auf 1884, dann die Stufen-Firmware**, Gegenprobe in
    `/settings`, stromlos ablegen. Die Stufen-Firmware ist hier bereits die neue;
    der Rollout (Phase F) zieht beide Backups ohnehin nach.

### Phase E — Dokumentation und Prüfen vor dem Merge (Markdown mit Umlauten)

15. **`src/version.h`:** `heishamon_version` → `3.24.0`; Changelog oben mit
    **Problem** (S2, S3, mit dem Befund zur Puffer-Falle), **Fix**, **NACHWEIS**
    (Schritt 3: Vorversion zeigt den Stillstand; Schritt 12; Hosttest samt
    Gegenprobe) und **GROESSE** (Format: `/pruefen-vor-merge`, Schritt 6; der
    Wegfall von `TelnetStream` ist hier kein Fehler, sondern erwartet — Delta
    benennen).

16. **`README.md`:** „Aufbau“ (Zeile für `src/telnetlog.h` und `.cpp`, diese
    Arbeitsplan-Datei, `test/telnet_haenger.py`), Abschnitt zum Telnet-Log (ein
    Client, Verdrängen, Verwerfen statt Warten). **`CLAUDE.md`:** `telnetlog.h`
    in die Liste der arduino-freien Header. **`MQTT-Topics.md`:** keine Änderung.

17. **`/pruefen-vor-merge`** — Rettungsanker, Version und Changelog, ASCII,
    Einheitlichkeit H1/H2 (erwartet: kein Treffer), alle Hosttests, alle Envs,
    RAM/Flash-Delta. Danach **Stopp** — Merge, Tag und OTA über `/rollout 3.24.0`,
    vom Owner aufgerufen.

### Phase F — Rollout (`/rollout 3.24.0`)

Der Skill führt Merge, Tag, OTA H1 und H2, Backups, Push und Releases. Dieser
Plan ergänzt ihn:

18. **Zusatzabnahme H1 und H2, lesend:** Telnet-Mitschnitt von ein paar Minuten
    (kommt die `<DBG>`-Zeile im 5-s-Takt?), `info/boot_reason` = `SW` nach dem
    OTA, in den folgenden 24 Stunden **kein** unerwartetes `TASK_WDT` und kein
    Anstieg von `info/boot_count` (Bridge-Wächter, Memory „Wächter-Landschaft“).
    Ein Task-Watchdog-Reset an der Anlage ist ein **Befund**, kein Normalfall.

### Phase G — Abschluss

19. **Nachtrag und Aufräumen:** Kopf dieses Plans auf **erledigt**, im
    Maßnahmenplan S2, S3 und P3 auf „erledigt in 3.24.0“, Memory
    `projektstand-heishamon` überschreiben, die Memory „Messfallen am Prüfling“
    um die Puffer-Falle (Mac-Empfangspuffer) ergänzen,
    `codedurchsicht-2026-09-02` (K1 erledigt), danach `/abschluss`.

## Messwerte

Alle Zahlen: `text / data / bss` der `.elf`.

| Env | Vorher (Anker, 3.23.2) | Nachher (3.24.0) |
| --- | --- | --- |
| `heishamon_esp32_h1_ota` | *offen* | *offen* |
| `heishamon_esp32_h2_ota` | *offen* | *offen* |

## Rückweg

Bis zum Rollout ist nichts ausgerollt: Verwerfen genügt (`git checkout main`,
Branch löschen). Nach dem Rollout gilt der Rückweg des Skills `/rollout`; der
Rettungsanker ist `rettungsanker-vor-loop-stillstand-2026-10-16`. Ein Rückfall
auf 3.23.2 bringt S2 und S3 zurück — ohne Frist, aber mit dem bekannten Risiko
aus dem Maßnahmenplan.
