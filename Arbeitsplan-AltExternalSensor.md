# Arbeitsplan: Alternativer Außenfühler als SET40/TOP112 (3.23.0)

> **Ergebnis: offen — Plan steht, alle Entscheide vom 2026-09-19 liegen vor;
> Umsetzung nicht begonnen.** Die Firmware steht auf 3.22.0 (alle vier Boards).
>
> **Übergabe für die Umsetzungs-Session.** Dieser Plan führt vom ersten Befehl
> bis zum Release in beiden Repos. Er ist vollständig entschieden; offen ist nur
> die Arbeit. Wer ihn aufnimmt, liest zuerst „Entschieden — nicht neu
> aufmachen“ und „Lies zuerst“, dann die Schritte der Reihe nach.

## Worum es geht

Beide Wärmepumpen haben zwei Außentemperaturfühler: den eingebauten am Gehäuse
und je einen alternativen, externen Fühler, im Schatten auf dem Flachdach
montiert. Welcher gilt, legt eine Einstellung im Installateurmenü fest. Steht
sie auf **On**, ist der Gehäusefühler abgeschaltet, und der externe Fühler gilt
für alle Regelvorgänge, die von der Außentemperatur abhängen.

* **Sommer, besonders beim Kühlen:** Der externe Fühler liefert die
  realistischeren Werte, weil der Gehäusefühler Sonne abbekommt.
* **Winter und Frost:** Der Gehäusefühler ist für die Wärmepumpe selbst
  aussagekräftiger — unter anderem hängt das Abtauen von der Außentemperatur ab.

Bisher geht das Umschalten nur am Bedienteil, im Installateurmenü. Dieser Plan
macht es über MQTT schaltbar und rücklesbar:

| Nummer | Name | Byte | Bits | Werte |
| --- | --- | ---: | --- | --- |
| SET40 | `AltExternalSensor` | 20 | 3+4 | 0 = Off, 1 = On |
| TOP112 | `Alt_External_Sensor` | 20 | 3+4 | Off / On |

Kodierung und Dekodierung wie im Original-Projekt (`SetAltExternalSensor`,
`Alt_External_Sensor`). Mitgenommen wird die seit dem 2026-09-18 vorgemerkte
Entfernung von **TOP66 `Low_Pressure`**. Neue Version: **3.23.0**.

Nach Berichten aus der Anwenderschaft des Originals lässt sich die Einstellung
im laufenden Betrieb umschalten, obwohl sie im Installateurmenü liegt. Beim
Umschalten sollte TOP14 `Outside_Temp` um 1–2 K springen; das ist der
sichtbare Wirkungsnachweis.

## Lies zuerst — in dieser Reihenfolge

1. [`CLAUDE.md`](CLAUDE.md) — Harte Regeln, besonders 1 (`doku-intern/`),
   4 (Rettungsanker) und 5 (Eingriffe einzeln).
2. [`SET-TOP-Zuordnung.md`](SET-TOP-Zuordnung.md), Einleitung — warum zu jedem
   Set-Kommando ein Rücklese-Topic gehört.
3. [`MQTT-Topics.md`](MQTT-Topics.md), Abschnitt „Installer settings as topics -
   and where the line is“ — die Drei-Punkte-Regel für Installer-Bytes. TOP112
   braucht dort einen erklärenden Satz (Schritt 8).
4. [`Byte-Zuordnung.md`](Byte-Zuordnung.md), Zeilen zu Byte 20 und Byte 164.
5. [`test/README.md`](test/README.md), „Heizstab schalten (Byte 9 und Byte 5,
   2026-08-28, WP1)“ — der jüngste gleichartige Anlagentest, nach dessen Muster
   der Funktionstest hier läuft.
6. [`README.md`](README.md), „Der Broker spielt beim Verbinden alles wieder ein
   (3.6.1)“ — daraus folgt die Bedienregel E2.
7. Die Skills `/pruefen-vor-merge` und `/rollout` (`.claude/skills/`) — dieser
   Plan ergänzt sie nur um den Funktionstest und das Aufräumen von TOP66.

## Entschieden — nicht neu aufmachen

Nur ausdrückliche Owner-Entscheide. Was aus dem Code folgt, steht getrennt
darunter.

| Nr. | Frage | Entscheid | Wann |
| --- | --- | --- | --- |
| A | Nummern, Namen, Byte, Klartext | SET40 `AltExternalSensor`, TOP112 `Alt_External_Sensor`, Byte 20 Bits 3+4, Off/On, Dekodierung wie im Original | Auftrag 2026-09-19 |
| E1 | Welche WP hat einen externen Fühler? | **Beide**, je ein eigener Fühler; beide stehen heute auf On. SET40/TOP112 gelten an beiden Stufen gleich — keine Abweichung H1/H2 | 2026-09-19 |
| E2 | Wer schaltet um? | **Von Hand über den ioBroker-Datenpunkt**, nicht mehr am Bedienteil. Keine Automatik in diesem Plan. Folgeauftrag in `nodered-flows`: SET40/TOP112 in den `WP_Befehls_Waechter` | 2026-09-19 |
| E3 | Fasst der Notbetrieb SET40 an? | **Nein.** Die Fühlerwahl bleibt im Notbetrieb, wie sie ist | 2026-09-19 |
| E4 | TOP66 `Low_Pressure` mitnehmen? | **Ja**, wie am 2026-09-18 entschieden: Zeile raus, keine Umnummerierung | 2026-09-18, bestätigt 2026-09-19 |
| E5 | Wann läuft der Funktionstest? | **In der Abnahme, H1 vor H2:** nach dem OTA auf H1 sofort der SET40-Test; H2 erst, wenn er grün ist. Merge und Tag sind dann lokal, nichts gepusht | 2026-09-19 |
| E6 | In welchem Anlagenzustand? | **Wie es sich ergibt.** Kein Zeitfenster abwarten; Verdichterzustand (TOP0/TOP8) wird mitgeschrieben und im Ergebnis vermerkt | 2026-09-19 |
| E7 | Wasser oder Glykol? | **Wasser.** Bit 1 von Byte 20 ist damit ohne Belang (siehe unten) | 2026-09-19 |
| E8 | Aliase `alias.0.data.mess.kaskade.wp1\|wp2.Low_Pressure` samt InfluxDB-Historie? | **Löschen** — die Historie ist durchgehend 0 und trägt keine Information. Löscht der Owner im ioBroker-Admin | 2026-09-19 |

### Was aus dem Code folgt — kein Entscheid, aber festgelegt

* **Zeile in `setCommands[]`:** Position 20, Maske `0x30`, `CONV_MUL_INC`,
  Parameter 16, Bereich 0..1. Off → `(0+1)*16 = 0x10` (Bitpaar b01), On →
  `(1+1)*16 = 0x20` (b10). Dieselbe Kodierung wie SET2 `HolidayMode` auf Byte 5.
* **Zeile in `stateTopics[]`:** Byte 20, `getBit3and4`, Klartext `OffOn`.
  `((b >> 4) & 3) - 1` liefert für `0x10` die 0 und für `0x20` die 1.
* **Die Nachbarfelder in Byte 20** (Frostschutz 5+6, Optionsplatine 7+8) bleiben
  unberührt, weil `00` im Kommando „keine Änderung“ heißt. **Bit 1
  (Wasser/Glykol) ist ein Einzelbit**, und jedes SET40-Kommando schickt dort 0.
  Das Original-Projekt macht es genauso; mit E7 ist das folgenlos. Die
  Vorab-Lesung (Schritt 1) bestätigt Bit 1 = 0 trotzdem, bevor gebaut wird.
* **Die Topic-Zahl bleibt bei 99** (−TOP66, +TOP112), `NUMBEROFTOPICS` ändert
  sich nicht. Set-Kommandos: 37 → 38.
* **Versionssprung 3.22.0 → 3.23.0:** neue Funktion.
* **Warum TOP112 trotz der Drei-Punkte-Regel:** Die Regel in `MQTT-Topics.md`
  entscheidet über *reine Lese-Topics* von Installer-Bytes und lässt
  Fühlerzuordnungen ausdrücklich draußen. TOP112 ist kein reines Lese-Topic,
  sondern die Rücklesung von SET40 — und ohne sie würde der ioBroker blind
  schreiben. Der Grundsatz aus `SET-TOP-Zuordnung.md` geht vor. Die dritte
  Bedingung der Regel („gemessen, nicht übernommen“) erfüllt der
  Funktionstest trotzdem.
* **Der Anlagennachweis steht nicht im Changelog der getaggten Version.**
  Nach E5 liegt der Tag `v3.23.0` schon vor dem Funktionstest. Der Changelog
  verweist deshalb auf den Abschnitt in `test/README.md`, der das Ergebnis
  trägt. Sonst müsste nach dem Tag noch einmal `src/` geändert werden.

## Was an der Anlage zu beachten ist

* **On setzt den angeschlossenen Fühler voraus.** Nach E1 hat jede WP einen.
  Was eine WP bei On ohne Fühler tut, wird **nicht** ausprobiert — es wäre
  dieselbe Klasse Fehlkonfiguration wie H91 (eingestellte Hardware, die nicht
  da ist; siehe `MQTT-Topics.md`, Installer-Abschnitt). Kommt ein Fühler einmal
  weg oder geht kaputt: vorher auf Off.
* **Der ioBroker spielt den gespeicherten Wert jedes Set-Topics beim Verbinden
  wieder ein.** Die Firmware verwirft ihn in den ersten 5 s nach dem SUBACK
  (`SUBSCRIBE_GRACE`, 3.6.1). Kommt die Wiedereinspielung einmal später, läuft
  der gespeicherte Wert durch. Daraus folgt die Bedienregel zu E2: **Der
  ioBroker-Datenpunkt `set.AltExternalSensor` muss immer den gewollten Zustand
  tragen.** Wer doch am Bedienteil umschaltet, stellt den Datenpunkt danach auf
  denselben Wert — sonst liegen zwei Wahrheiten vor, und die im ioBroker kann
  die am Gerät still zurückdrehen.
* **Der Datenpunkt entsteht erst mit dem ersten Publish** (Funktionstest,
  Schritt 20). Nach dem Test steht er an beiden Stufen auf 1 = On, also auf dem
  vorgefundenen Zustand der Wärmepumpen.
* **SET40 steht in keinem Re-Assert.** Ein Ruhefenster
  (`nodered-flows/testfenster.py`) braucht der Test deshalb nicht; was die
  Anlage währenddessen tut, schreibt `top_watch.py` mit.
* **Übernahme kann dauern, und manche Kommandos nimmt die WP nur bei
  ausgeschalteter Einheit an** (SET39: Übernahme bis rund 30 s,
  `test/README.md`; nur bei ausgeschalteter Einheit, `README.md`). Deshalb bis zu 120 s beobachten, bevor ein Schritt als
  gescheitert gilt. Nimmt die WP SET40 im vorgefundenen Zustand nicht an:
  **HALT** — ein zweiter Versuch bei ausgeschalteter Einheit ist ein eigener
  Eingriff und braucht das ausdrückliche Ok des Owners.
* **TOP14 ist Wirkungsbeleg, nicht Prüfkriterium.** Lesen beide Fühler gerade
  fast dasselbe (bedeckt, Nacht), bleibt der Sprung aus, ohne dass etwas
  falsch ist. Grün oder rot entscheiden Byte 20 und TOP112.

## Arbeitsschritte

Jeder Schritt endet mit seinem Prüfkriterium. Gerätezugriffe, die etwas
verändern, stehen **einzeln** in einem Bash-Aufruf und werden vorher angesagt
(**HALT** = auf das Ok des Owners warten). Lesende Zugriffe auf produktive
Geräte ebenfalls nur mit Auftrag (CLAUDE.md, Regel 5).

### Phase A — Vorbereitung

0. **Dieser Plan auf `main`** (Doku-Ergänzung, CLAUDE.md Regel 4), mit
   Eintrag in `README.md` → „Aufbau“. *Erledigt am 2026-09-19 mit dem
   Anlegen dieser Datei.*

1. **Vorab-Lesung Byte 20, beide Stufen, rein lesend** — nur mit Auftrag des
   Owners. `byte_monitor.py` schaltet nur den Hexlog des HeishaMon ein und
   wieder aus und schreibt nichts an die Wärmepumpe:
   ```bash
   ./test/byte_monitor.py 192.168.2.120 20 --dauer 30
   ./test/byte_monitor.py 192.168.2.122 20 --dauer 30
   ```
   *Prüfkriterium:* An beiden Stufen Bits 3+4 = `10` (On, wie E1 sagt) und
   Bit 1 = 0 (Wasser, E7). Rohwert je Stufe notieren — er wird der
   On-Testvektor in `byte20_test.cpp`. **Weicht eines ab: anhalten**, nicht
   bauen. Dann stimmt entweder die Zuordnung nicht oder die Ausgangslage.
   *Zweck nach Aufwand/Nutzen:* Stimmt die Zuordnung nicht, fällt das hier
   ohne einen einzigen Eingriff auf und nicht erst nach einem Rollout.

2. **Rettungsanker und Branch** (`Ablauf-Backup-Boards.md`, „Bei jeder
   Firmware-Änderung“, Schritt 1):
   ```bash
   git tag -a rettungsanker-JJJJ-MM-TT -m "Stand vor SET40/TOP112 und TOP66 (3.23.0)"
   git cat-file -t rettungsanker-JJJJ-MM-TT     # muss "tag" sein
   git checkout -b alt-aussenfuehler
   ```
   Ist der Tagesname schon vergeben: `rettungsanker-vor-alt-aussenfuehler-JJJJ-MM-TT`.
   *Prüfkriterium:* annotierter Tag, Branch ≠ `main`.

### Phase B — Code (Kommentare und Serial-Ausgaben ASCII, ohne Umlaute)

3. **`src/commands.cpp` — SET40.**
   - Im Kommentarblock „Why the mask column exists“ die Zeile
     `byte 20: AltExternalSensor 0x30` ergänzen, mit dem Hinweis, dass Byte 20
     weitere Installer-Felder trägt und `00` dort „keine Änderung“ heißt.
     Bit 1 ist ein Einzelbit, das jedes Kommando mit 0 schickt (Wasser) — das
     gehört ausgeschrieben, nicht stillschweigend.
   - Neue letzte Tabellenzeile, davor ein Kommentar zum Warum: Installer-
     Einstellung, On setzt den externen Fühler voraus, Umschalten nur über den
     ioBroker (Wiedereinspielung), der Notbetrieb fasst ihn nicht an (Owner
     2026-09-19). Die Kodierung wie bei SET35–SET38 aus dem Dekodierer
     zurückgerechnet:
     ```cpp
     //   AltExternalSensor 0 -> (0+1)*16 = 0x10, gelesen ((0x10>>4) & 0b11) - 1 = 0
     //   AltExternalSensor 1 -> (1+1)*16 = 0x20, gelesen ((0x20>>4) & 0b11) - 1 = 1
     {40, 20, 0x30, CONV_MUL_INC, "AltExternalSensor",             0,   1,  16}, // aus=16 an=32
     ```
   *Prüfkriterium:* baut; `usedMask`-Konflikt ausgeschlossen (einziges Feld
   in Byte 20).

4. **`src/decode.cpp` — TOP112 hinzu, TOP66 heraus.**
   - Neue letzte Zeile hinter TOP111, mit Kommentar: Rücklesung von SET40,
     warum trotz der Drei-Punkte-Regel (siehe oben), Anlagennachweis in
     `test/README.md`:
     ```cpp
     {112,  20, "Alt_External_Sensor",              getBit3and4,          nullptr,                   OffOn},
     ```
   - Zeile `{ 66, 164, "Low_Pressure", …}` streichen. Im Kopfkommentar der
     Tabelle die Lückenliste um TOP66 ergänzen, mit Grund (Byte 164 an beiden
     Stufen dauerhaft `0x01`, Owner-Entscheid 2026-09-18). **Nicht
     umnummerieren.** `Pressure[]` bleibt, TOP64 benutzt es weiter.
   - `NUMBEROFTOPICS` in `src/decode.h` bleibt 99; der `static_assert`
     prüft das.
   *Prüfkriterium:* baut; `state_topic_index(66) < 0`,
   `state_topic_index(112) >= 0`.

5. **`test/byte20_test.cpp`** nach dem Muster von `byte9_test.cpp`, gebaut über
   `./test/decode_hosttest.sh test/byte20_test.cpp`:
   1. TOP112 steht auf Byte 20 mit `getBit3and4`; TOP66 gibt es nicht mehr.
   2. Beide Werte von SET40 kommen über den echten Dekodierpfad als 0/1 und
      Off/On zurück.
   3. Die Rohwerte aus Schritt 1 (On) dekodieren als On. Dazu Varianten, in
      denen Frostschutz, Optionsplatine und Bit 1 anders stehen — TOP112
      bleibt davon unberührt.
   4. Keine Kodierung tritt aus der Maske `0x30`, und alle übrigen Bits des
      Kommandobytes bleiben 0.
   5. **Gegenprobe von Hand, nicht eingecheckt:** Parameter 8 statt 16 oder
      Maske `0x0C` einbauen — der Test muss rot werden. Ergebnis im Changelog
      vermerken.
   In `test/hosttests.sh` als `decode_test byte20_test` mit
   Begründungskommentar eintragen. Den Kopf von `decode_hosttest.sh` sowie in
   der Werkzeugtabelle von `test/README.md` die Zeilen `decode_hosttest.sh`
   und `stubs/` um `byte20_test.cpp` ergänzen.
   *Prüfkriterium:* `./test/hosttests.sh` grün; ohne Eintrag in der Liste
   meldet das Skript selbst ROT.

6. **Dekodierpfad gegen den Stand davor:**
   ```bash
   ./test/decode_vergleich.py --basis rettungsanker-JJJJ-MM-TT \
     --entfallen Low_Pressure --neu Alt_External_Sensor
   ```
   *Prüfkriterium:* Alle übrigen 98 Topics identisch über alle Telegramme.

### Phase C — Dokumentation (Markdown mit Umlauten, `MQTT-Topics.md` englisch)

7. **`src/version.h`:** `heishamon_version` → `3.23.0`; Changelog-Eintrag mit
   Problem (Fühlerwahl nur am Installateurmenü, zweimal im Jahr; Rücklesung
   als Pflicht), Inhalt (SET40/TOP112, TOP66 entfällt), den Owner-Entscheiden
   E1–E3 mit Datum, **NACHWEIS** (byte20_test samt Gegenprobe,
   decode_vergleich, alle Hosttests, alle Envs; Anlagennachweis: Verweis auf
   `test/README.md`) und **GROESSE** gegen 3.22.0 (Format siehe
   `/pruefen-vor-merge`, Schritt 6).

8. **`MQTT-Topics.md`:**
   - „Sensor Topics“: Zeile TOP112 (`Alternative outdoor sensor in use (0=off,
     1=on) - installer setting, written by SET40`), Zeile TOP66 streichen, im
     Einleitungsabsatz über die Lücken TOP66 (3.23.0) ergänzen.
   - „Command Topics“: Zeile SET40 (Byte 20, `0=off, 1=on`).
   - Neuer Abschnitt *Alternative outdoor sensor (SET40, TOP112, new in
     3.23.0)*: wozu (Sommer/Winter), beide Units haben einen eigenen Fühler,
     **switch via the ioBroker datapoint only** samt Begründung
     (Wiedereinspielung), der Notbetrieb fasst ihn nicht an. Dazu ein
     deutscher Absatz wie im Installer-Abschnitt.
   - Im Abschnitt „Installer settings as topics“ ein Satz: TOP112 existiert als
     Rücklesung von SET40, nicht nach der Drei-Punkte-Regel; die Regel für reine
     Lese-Topics bleibt unverändert.

9. **`SET-TOP-Zuordnung.md`:** Tabellen mit `./test/set_top_zuordnung.py` neu
   erzeugen und einsetzen; Zahlen im Einleitungssatz und in den Überschriften
   (38 Set-Kommandos, 99 State-Topics, Firmware 3.23.0, „36 von 38“);
   TOP66 aus der Liste in Abschnitt 3 streichen. Neue Fußnote ⁸ zu SET40:
   Installer-Einstellung, Bedienregel aus E2, Nachweis in `test/README.md`.

10. **`Byte-Zuordnung.md`:** Byte 20 Bits 3+4 bekommt SET40/TOP112 mit
    Bemerkung (`0 = Off, 1 = On`, Installer-Einstellung, gemessen: Datum nach
    Schritt 20); Byte 164 wird `Referenz:`-Zeile ohne TOP. Stand-Zeile (38/99,
    3.23.0). In der Lesehilfe: Fußnoten ¹–⁸; „Bytes 20–30 … liest davon nur
    Byte 23 und Byte 25“ um Byte 20 Bits 3+4 (liest und schreibt) ergänzen.
    „Was beim Aufstellen aufgefallen ist“: TOP66-Punkt auf „entfernt in
    3.23.0“ umstellen.

11. **`Ablauf-Notbetrieb.md`**, Abschnitt „Was der Notbetrieb ebenfalls nicht
    zurückholt“: Die Fühlerwahl bleibt, wie sie ist (Owner-Entscheid
    2026-09-19) — beide Fühler liefern brauchbare Werte, ein zusätzlicher
    Schritt hätte beide Folgen verlängert, ohne einen Ausfall abzuwenden.

12. **`README.md`:** kein eigener Abschnitt für SET40 — das Original hat das
    Kommando, es ist kein Unterschied zu ihm. Im Abschnitt „Zone 2 entfernt
    (3.4.0)“ einen Satz zu TOP66 (3.23.0) ergänzen. „Aufbau“: nichts Neues
    außer dieser Datei (Schritt 0).

13. **`test/README.md`:** Zeile `byte20_test.cpp` in „Werkzeuge“; neuer
    Abschnitt „Außenfühler umschalten (Byte 20, SET40/TOP112)“ mit Aufrufen,
    zunächst ohne Ergebnis — das kommt in Schritt 23.

*Prüfkriterium Phase C:* `./test/hosttests.sh --schnell` grün
(`doku_zuordnung_test`, `repo_konsistenz_test` prüfen Tabellen, Zahlen und
Verweise). Bedeutungstexte und Fußnoten prüft kein Test — von Hand lesen.
Nichts aus `doku-intern/`, auch nicht sinngemäß, auch nicht in
Commit-Messages.

### Phase D — Prüfen vor dem Merge

14. **`/pruefen-vor-merge`** — Rettungsanker, Version und Changelog, ASCII,
    Einheitlichkeit H1/H2 (erwartet: kein Treffer, SET40/TOP112 stehen in der
    gemeinsamen Tabelle), alle Hosttests, alle Envs, RAM/Flash-Delta gegen den
    Rettungsanker, Doku. *Prüfkriterium:* alles grün; die GROESSE-Zeile steht
    im Changelog.

### Phase E — Rollout (`/rollout 3.23.0`, vom Owner aufgerufen)

Der Skill führt Merge, Tag, OTA, Abnahme, Backups, Push und Releases. Dieser
Plan **ergänzt** ihn an drei Stellen: Funktionstest (Schritt 20 und 21),
Doku-Nachtrag vor dem Push (Schritt 23) und Aufräumen von TOP66 (Schritt 26).

15. **Merge und Tag, lokal** (Skill, Schritt 1): Kurztitel „Außenfühler
    umschaltbar (SET40/TOP112), TOP66 entfällt“. Noch nicht pushen.

16. **Baseline beider Stufen** (Skill, Schritt 2).

17. **OTA auf H1** (Skill, Schritt 3). Erwartung im `tablesnap`-Diff: TOP66
    fehlt, TOP112 ist neu und zeigt **On**. Das ist schon der erste, passive
    Beleg der Dekodierung, denn am Bedienteil steht On. Sollwerte unverändert.
    Auf der MQTT-Seite erscheint `mqtt.0.panasonic_heat_pump.state.Alt_External_Sensor`
    mit 1.

18. **Erst weiter, wenn die Tabelle voll ist** — ein SET40 in den ersten 5 s
    nach einem Verbinden verwirft die Firmware (Wiedereinspielungsschutz).

19. **Die zwei Mitschnitte**, lesend, je Umschaltung neu gestartet — erst
    **nach** dem Ok des Owners, sonst laufen sie während des Wartens ab:
    ```bash
    python3 -u test/byte_monitor.py 192.168.2.120 20 23 25 142 --dauer 180 > "$TMPDIR/h1_byte20_<n>.log" 2>&1
    python3 -u test/top_watch.py 192.168.2.120 0 8 14 44 112 --dauer 180 --takt 5 > "$TMPDIR/h1_top_<n>.log" 2>&1
    ```
    Beide im Hintergrund, rund 20 s Vorlauf vor dem Senden. Byte 23 und 25
    sind die gemessenen Installer-Bytes: Sie dürfen sich **nicht** bewegen.
    Byte 142 ist der Rohwert von TOP14, TOP44 der Fehlercode, TOP0/TOP8
    Einheit und Verdichter.

20. **Funktionstest H1** (E5, E6), jeder Schritt einzeln:
    1. **HALT:** „SET40 = 0 an H1.“ Nach dem Ok: Mitschnitte starten
       (Schritt 19, `<n>` = 1), 20 s warten, dann allein:
       `./test/mqtt_pub.py --host 192.168.2.147 panasonic_heat_pump/set/AltExternalSensor=0`
    2. Bis zu 120 s beobachten. Soll: Byte 20 Bits 3+4 `10` → `01`, alle
       anderen Bits von Byte 20 unverändert, TOP112 On → Off, TOP44 ohne
       Fehler, Byte 23/25 unverändert. TOP14: Sprung notieren (Wirkung).
       TOP0/TOP8 notieren (E6), ebenso die Übernahmedauer.
    3. **HALT:** „SET40 = 1 an H1 (zurück auf den vorgefundenen Zustand).“
       Nach dem Ok: Mitschnitte neu starten (`<n>` = 2), 20 s warten, dann
       allein:
       `./test/mqtt_pub.py --host 192.168.2.147 panasonic_heat_pump/set/AltExternalSensor=1`
    4. Soll: Byte 20 zurück auf den Rohwert aus Schritt 1, TOP112 → On, TOP14
       zurück.
    5. Endzustand prüfen, lesend: `tablesnap.py` gegen den Stand nach dem OTA
       (Sollwerte unverändert) und
       `curl -s "http://192.168.2.147:8087/get/mqtt.0.panasonic_heat_pump.set.AltExternalSensor"`
       → `val` 1.

    *Grün:* Byte 20 und TOP112 folgen in beide Richtungen, nur Bits 3+4
    bewegen sich, kein Fehler. *Rot:* **HALT, kein OTA auf H2.** Der Owner
    entscheidet: zweiter Versuch bei ausgeschalteter Einheit (eigener Eingriff)
    oder Rückweg nach dem Skill („noch nicht gepusht“).

21. **OTA auf H2 und Funktionstest H2** — Skill, Schritt 4, danach Schritte 18–20
    mit `192.168.2.122` und `panasonic_heat_pump2`. Eigene HALTs.

22. **Backups nachziehen** (Skill, Schritt 5). An `h1b`/`h2b` gibt es keine
    Wärmepumpe, TOP112 bleibt dort leer — kein Befund.

23. **Doku-Nachtrag mit dem Messergebnis, vor dem Push** — reine
    Doku-Änderung auf `main`, `src/` bleibt unberührt, der Tag bleibt auf dem
    Merge-Commit. *Abweichung vom Skill:* Dort steht der Nachtrag in Schritt 8;
    hier ist er vorgezogen, damit Repo und Release-Text die Messung schon
    tragen.
    - `test/README.md`: Ergebnistabelle je Stufe (Rohwerte Byte 20, TOP112,
      TOP14 vorher/nachher, Verdichterzustand, Übernahmedauer).
    - `MQTT-Topics.md`: Messung im neuen Abschnitt (Muster: Tabelle „Menu entry
      | unit | changed | byte | raw value | evidence“).
    - `Byte-Zuordnung.md`: „Gemessen JJJJ-MM-TT an beiden Stufen“ in der Zeile
      Byte 20.
    - Kopf dieses Plans: Ergebnis-Zeile vorläufig („an beiden Stufen
      abgenommen“); endgültig in Schritt 29.

24. **Push und CI** (Skill, Schritt 6): `main` und `v3.23.0` einzeln, nie
    `--follow-tags`.

25. **Releases** (Skill, Schritt 7), erst bei grüner CI.
    - **Öffentlich** `gregor-samosir/HeishaMonKaskade`, ohne Binaries:
      `## Warum` (Fühlerwahl Sommer/Winter), `## Was drin ist` (SET40/TOP112,
      TOP66 entfällt, Bedienregel „nur über den ioBroker“), Nachweise
      (byte20_test, decode_vergleich, Funktionstest an beiden Stufen mit
      TOP14-Sprung). Nichts aus `doku-intern/`.
    - **Privat** `gregor-samosir/HeishaMon-Rollback`: `v3.23.0` mit beiden
      `.bin` und der `platformio_user_env.ini`; `v3.22.0` → „Rückfallstand vor
      3.23.0“; das Release `v3.21.0` löschen (der Tag bleibt). Drei Assets
      nachprüfen.

26. **TOP66 aufräumen**, erst jetzt, da beide Stufen die neue Firmware tragen:
    ```bash
    ./test/retained_loeschen.py --basis v3.22.0              # nur anzeigen
    ```
    **HALT**, dann allein: `./test/retained_loeschen.py --basis v3.22.0 --loeschen`.
    Weil der Broker der ioBroker-Adapter ist, reicht das allein nicht. Der
    Owner löscht im ioBroker-Admin die Objekte
    `mqtt.0.panasonic_heat_pump.state.Low_Pressure` und
    `mqtt.0.panasonic_heat_pump2.state.Low_Pressure` sowie nach E8 die Aliase
    `alias.0.data.mess.kaskade.wp1.Low_Pressure` und `….wp2.Low_Pressure`
    samt Historie. *Prüfkriterium:* simple-api liefert für alle vier keinen
    Datenpunkt mehr.

### Phase F — Memory, Folgeauftrag, Abschluss

27. **Memory** (`~/.claude/projects/-Users-alexander-HeishaMonKaskade/memory/`):
    - `projektstand-heishamon.md` **überschreiben**: alle vier Boards auf
      3.23.0; TOP66-Punkt raus; neues offenes Thema: Folgeauftrag
      Befehls-Wächter (Schritt 28), falls noch nicht erledigt.
    - `top66-entfernen-vorgemerkt.md` löschen, die Zeile in
      `INDEX_Firmware_und_Protokoll.md` und den ⏰-Hinweis in der
      Verzeichniszeile von `MEMORY.md` entfernen. Das Ergebnis steht im Repo
      (Changelog, `Byte-Zuordnung.md`).
    - Die Memory zu SET40 (angelegt 2026-09-19) prüfen und bei Bedarf um den
      Befund aus dem Funktionstest ergänzen — nur, was steuert, wie Claude
      arbeitet. Messwerte gehören ins Repo.

28. **Folgeauftrag `nodered-flows`** (E2): SET40/TOP112 beider Stufen in den
    `WP_Befehls_Waechter` (`iobroker-js/common/kaskade/`, 5-min-Karenz wie
    dort üblich). **Nicht** in den `WP_Installer_Waechter`: Der prüft gegen
    einen festen Sollstand, und TOP112 wechselt zweimal im Jahr gewollt. Den
    Auftrag nach der Konvention dort anlegen (`TODO.md` oder eigene
    Auftragsdatei); umgesetzt wird er in einer Session in `nodered-flows`.

29. **Ergebnis-Zeile im Kopf dieses Plans endgültig setzen** (Skill,
    Schritt 8), auf `main` committen, nach Freigabe pushen. Dann
    **`/abschluss`** — Hosttests, Git-Stand, Memory-Abgleich, Köpfe der
    Vorhaben-Dateien.

## Fallen, die schon einmal zugeschnappt sind

- **Exit-Code von `pio` durch eine Pipe** sieht immer grün aus (2026-09-17).
  Befehl aus `/pruefen-vor-merge` übernehmen.
- **ASCII-Prüfung unter macOS:** kein `grep -P`, kein `LC_ALL=C grep` mit
  Bytebereich — der meldet immer sauber. `perl`-Zeile aus dem Skill nehmen.
- **Safari cached `/tablerefresh`.** Abgenommen wird mit `tablesnap.py`, nie
  im Browser.
- **`python3 -u` für Hintergrund-Mitschnitte**, sonst bleibt die Datei bis zum
  Prozessende leer.
- **Stille MQTT-Abonnenten trennt der Adapter nach rund 30 s.** Wer statt
  `top_watch.py` mit `mqtt_sub.py` mitliest, muss es in einer Schleife neu
  starten.
- **Zustandsverändernde Aufrufe nie bündeln** — ein Abbruch des Owners greift
  sonst ins Leere (2026-08-27).
- **Verzögerte Übernahme** (SET39: bis rund 30 s). Ein `/tablerefresh` direkt
  nach dem Senden beweist nichts.
- **`frame_diff.py` gibt Hex aus.** `byte_monitor.py` zeigt die Bitfelder
  selbst an; wer Rohwerte aus einem Mitschnitt vergleicht, liest hexadezimal.

## Nicht in diesem Plan

- **Automatisches Umschalten** (nach Betriebsart, Datum oder Temperatur) —
  nach E2 bewusst nicht. Bei Bedarf eigener Auftrag in `nodered-flows`.
  Vorsicht bei einer Temperaturschwelle: Die gemessene Außentemperatur hängt
  selbst vom gewählten Fühler ab.
- **Der Notbetrieb** fasst SET40 nicht an (E3).
- **Die übrigen Felder von Byte 20** (Medium, Frostschutz, Optionsplatine)
  bekommen kein Topic. Sie erfüllen die Drei-Punkte-Regel nicht, und Byte 20
  kommt nur wegen der Rücklesung von SET40 hinein.
- **Das Verhalten der WP bei On ohne angeschlossenen Fühler** wird nicht
  ausprobiert (siehe oben).

## Stand der Commits bei der Übergabe

`main` bei `ea595b1` (= `origin/main`), darauf lokal der Commit mit diesem
Plan. Die Firmware auf allen vier Boards ist 3.22.0.
