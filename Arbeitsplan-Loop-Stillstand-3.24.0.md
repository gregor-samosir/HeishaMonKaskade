# Arbeitsplan: `loop()` darf nicht stehen (3.24.0)

> **Ergebnis: erledigt (2026-10-16) — ausgerollt auf allen vier Boards, beide
> Releases angelegt, CI grün.** Paket P3 der Codedurchsicht vom 2026-10-15
> ([`Massnahmenplan-Codedurchsicht-2026-10-15.md`](Massnahmenplan-Codedurchsicht-2026-10-15.md):
> S2 und S3, K1 vom 2026-09-02). Eine Firmware-Änderung mit Funktionswirkung:
> Der Nachweis lief am **Prüfling** (`h1b`), nicht an der Anlage.
>
> **Rollout (2026-10-16):** Merge `c222602` und Tag `v3.24.0`; OTA H1 17:00:46,
> H2 17:03:36, `h2b` 17:07:07 (`h1b` trug 3.24.0 schon vom Rückgabe-Flash des
> Prüflings, aus dem identischen Quelltext). Abnahme beider Stufen: Version
> 3.24.0 auf der Startseite und im ioBroker (`info/version`, `boot_reason` SW,
> `ts` nach dem OTA; `boot_count` je 9), Tabellen-Diff H1 ein laufender Messwert,
> H2 leer, Sollwerte unverändert, Notbetrieb bereit (`fehlendMaske`, `Sperre`,
> `Lage` je 0), passiver Telnet-Mitschnitt von 45 s an beiden Stufen mit 100 %
> Antwortquote (8 und 7 Abfragen, nichts verworfen). Backups: `h2b` auf
> Stufe 2, `mqtt_port` 1884, Hostname und Pflichtfelder unverändert, Notbetrieb
> gesperrt wie gewollt. `CI` und `CodeQL` grün. Öffentlich `v3.24.0` ohne
> Binaries; privat `HeishaMon-Rollback` `v3.24.0` mit beiden `_ota`-Abbildern und
> der ini, `v3.23.2` zum Rückfallstand umbenannt, Release `v3.23.1` gelöscht (Tag
> bleibt; Owner bestätigt).
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
   2. **Messwerkzeug `test/telnet_haenger.py`** (Standardbibliothek): verbindet
      sich auf Port 23 mit `SO_RCVBUF` von ~1 KB **vor** dem `connect()`, sendet
      zu Beginn einen Schwall `M` (je Taste eine Zeile, derselbe Schreibweg wie
      das Debug-Log, ändert keinen Zustand), liest danach **nie** wieder und
      fragt parallel alle 2 s die Startseite ab. Ausgabe: Antwortzeit oder
      Timeout je Abfrage, am Ende größte Antwortzeit, Urteil und ob das Gerät den
      Client getrennt hat. Mit `--nur-lesen` als Gegenprobe der Gegenprobe;
      `--erwarte stillstand|laeuft` setzt den Exit-Code, und „läuft“ ohne
      Trennung des Clients gilt als **ungültig** (Puffer nie voll).
   3. Lauf gegen 3.23.2.
   *Prüfkriterium:* Startseite antwortet länger als 10 s oder gar nicht,
   solange der Testclient hängt. **Bleibt es grün, ist die Messung falsch**
   (Puffer nicht voll geworden), nicht der Befund erledigt — dann Schwall und
   Puffergröße prüfen, bevor irgendetwas gebaut wird.

   **Ergebnis (2026-10-16, Prüfling `h1b`, 3.23.2 mit Test-Prefix): Stillstand
   gemessen.** Startseite antwortet bis 4 s, danach in 90 s nicht mehr
   (viermal „keine Antwort“ nach 20 s Timeout). Die Messung ist erst im dritten
   Anlauf gültig geworden — jeder Fehlversuch war genau die Messfalle, vor der
   der Plan warnt:

   | Anlauf | Einstellung | Ergebnis |
   | --- | --- | --- |
   | 1 | Hexlog `H`, 240 s | LÄUFT, 9,6 KB im Puffer. **Ungültig:** Der Hexlog loggt nur *gesendete Kommandos*, ohne Wärmepumpe also nichts; die Lograte blieb bei rund 40 Byte/s |
   | 2 | Schwall von 300 `M` (rund 13 KB), 90 s | LÄUFT, 15,6 KB im Puffer. **Ungültig:** Der Puffer war nicht voll |
   | 3 | Schwall von 5000 `M`, 90 s | **STILLSTAND**, 196 KB aus dem Puffer gelesen |

   Zwei Folgerungen: (a) Das `SO_RCVBUF` von 1 KB bleibt wirkungslos klein —
   macOS nimmt hier rund 200 KB in den Empfangspuffer; ohne Schwall hätte der
   Lauf Stunden gebraucht. (b) Der Hexlog taugt am Prüfling **nicht**, die Lograte
   zu heben; der Schalter `H` hängt zudem am Gerät, nicht an der Sitzung (ein
   Umschalter, dessen Zustand sich nicht lesen lässt) — das Werkzeug fasst ihn
   deshalb nicht mehr an. Das Gerät erholte sich nach dem Lauf von selbst.

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

   **Ergebnis (2026-10-16):** Alle `delay()` und Warteschleifen in `src/` gesehen.
   Aus `loop()` erreichbar und länger als 5 s ist nur der Hydraulikschritt
   (≤ 30 s, jetzt per `static_assert` gegen die 120 s gehalten) und der
   KNX-Befehl (≤ 20,5 s, ebenfalls `static_assert`). Die `delay()` in `/reboot`,
   `/settings` und im WLAN-Watchdog (je ≤ 2 s) enden im Neustart. Das
   WiFiManager-Portal (`setupWifi`) wird nur aus `setup()` gerufen und liegt vor
   dem Watchdog; dasselbe gilt für die NTP-Wartezeit. Der OTA-Upload füttert den
   Watchdog selbst. Keine Blockade über 60 s ohne Eintrag.

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
    Begründung; `telnet_haenger.py` trägt kein `_test` im Namen und braucht
    deshalb keinen Eintrag in `HARDWARE`), `test/README.md`
    (Werkzeugtabelle: beide Dateien; Abschnitt zu S2 mit der Puffer-Falle),
    `./test/hosttests.sh` grün.

### Phase D — Nachweis am Prüfling

12. **Erwartung vorab festhalten**, dann messen:
    1. **S2:** `telnet_haenger.py --schwall 5000 --erwarte laeuft` gegen 3.24.0 —
       größte Antwortzeit der Startseite unter 1 s, „Gerät hat den Client
       getrennt: ja“, `info/uptime` und die State-Zeitstempel laufen
       weiter, solange der Testclient hängt. Nach spätestens 15 s ist der Client
       getrennt; ein **neuer** Mitschnitt (`telnet_mitschnitt.py`) kommt sofort
       durch. Die `<INF>`-Zeile „N Zeilen verworfen“ gehört dem Client, der
       zeitweise nicht mitkam, aber nicht getrennt wurde (`--lesen-nach`) — ein
       neuer Client beginnt bei null und sieht sie nicht.
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
    *Beim Warten einplanen:* Der Prüfling braucht nach einem Neustart zwischen
    11 s und rund 4 min bis zur Startseite (WLAN-Anmeldung; schon bei der
    3.23.2-Messung davor 92 s) — das ist keine Eigenschaft dieser Änderung, aber
    es verlängert jeden Versuch. Mit `Monitor` auf die Startseite warten.

    *Prüfkriterium:* alle sieben Punkte erfüllt. Sonst **HALT** — nicht
    ausrollen.

    **Ergebnis (2026-10-16, `h1b`, Prüfling-Build, alle sieben Punkte erfüllt):**

    | Punkt | Ergebnis |
    | --- | --- |
    | 1 S2 | Größte Antwortzeit der Startseite 70 ms (3.23.2: 90 s ohne Antwort), „Gerät hat den Client getrennt: ja“, 45 Abfragen ohne Ausfall. `<INF>`-Zeile mit `--lesen-nach` gesehen |
    | 2 Tasten | `M W I R L D H C` einzeln, Antworttexte wie in 3.23.2; `C` antwortet „bye bye“ und trennt |
    | 3 Mitschnittwerkzeuge | `telnet_mitschnitt.py` 40 s: sechs Zyklen, `<DBG>` im Takt; `produktiv_mitschnitt.py` 30 s ohne Fehler |
    | 4 Verdrängen | die erste Sitzung wird getrennt, die zweite bekommt das Log |
    | 5 OTA | ArduinoOTA und `/firmware` laufen durch; ein auf **153 s** gedehnter HTTP-Upload (`--limit-rate 8k`) bricht nicht ab. **Gegenprobe:** ohne die Fütterung in `Update.onProgress` bricht derselbe Upload nach **126 s** ab (curl Exit 56) |
    | 6 Grenze nach oben | `s=90`: kein Reset, dieselbe Telnet-Sitzung läuft danach weiter |
    | 7 Reset | Endloser Hänger: `task_wdt: Task watchdog got triggered … loopTask (CPU 1)` genau 120 s nach dem Aufruf; danach `info/boot_reason = TASK_WDT`, `info/boot_count` 7 → 8. Notbetriebswerte erhalten — auch **ohne Broker** (zweiter Hänger bei `mqtt_port` 1884: `fehlendMaske` 0, `Lage` 1) |

    Zu Punkt 7: Die Erwartung `TASK_WDT` (nicht `PANIC`) hat sich bestätigt. Die
    Prüfling-Route ist hinter dem Notbetriebs-Login; der HTTP-Updater verlangt
    für den Upload `Origin: http://<ip>` und den Feldnamen `firmware` (sonst
    401 — das hat zwei Fehlversuche gekostet, steht jetzt in `test/README.md`).
    Das eigentliche Ziel von Punkt 5, der Gegenprobe, ist der wichtigere Fund:
    Ohne sie wäre „153 s ohne Abbruch“ ein Ergebnis, das auch ohne Fütterung
    hätte kommen können, wenn der Watchdog gar nicht angemeldet gewesen wäre.

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
| `heishamon_esp32_h1_ota` (= `h1_usb`) | 1046695 / 230062 / 2197487 | 1046975 / 230502 / 2196911 |
| `heishamon_esp32_h2_ota` (= `h2_usb`) | 1046275 / 229822 / 2197487 | 1046551 / 230262 / 2196911 |
| `heishamon_esp32_usb` (= `_ota`, Prüfling) | 1047727 / 230506 / 2197503 | 1048451 / 231162 / 2196927 |

RAM/Flash laut `pio`: H1 RAM 61960 → 61384 (−576), Flash 1223541 → 1224317
(+776); H2 RAM 61960 → 61384 (−576), Flash 1222881 → 1223653 (+772).

## Rückweg

Bis zum Rollout ist nichts ausgerollt: Verwerfen genügt (`git checkout main`,
Branch löschen). Nach dem Rollout gilt der Rückweg des Skills `/rollout`; der
Rettungsanker ist `rettungsanker-vor-loop-stillstand-2026-10-16`. Ein Rückfall
auf 3.23.2 bringt S2 und S3 zurück — ohne Frist, aber mit dem bekannten Risiko
aus dem Maßnahmenplan.
