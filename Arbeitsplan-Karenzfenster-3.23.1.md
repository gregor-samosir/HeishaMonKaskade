# Arbeitsplan: Karenzfenster überlaufsicher (3.23.1)

> **Ergebnis: offen — Frist 2026-10-31 04:01 CET.** Angelegt am 2026-10-15
> aus der Codedurchsicht desselben Tages (Befund S1 in
> [`Massnahmenplan-Codedurchsicht-2026-10-15.md`](Massnahmenplan-Codedurchsicht-2026-10-15.md)).
> Noch nichts umgesetzt.
>
> **Übergabe:** Der Plan führt vom Rettungsanker bis zum Release und ist für
> den Einstieg in einer neuen Session geschrieben. Zuerst „Lies zuerst“, dann
> Schritt 1 — die Frist verschiebt sich mit jedem Reconnect und muss vor dem
> Bauen neu bestimmt werden.

## Worum es geht

Nach jedem Verbinden mit dem Broker verwirft die Firmware fünf Sekunden lang
alle Set-Kommandos, weil der ioBroker-Adapter dann die gespeicherten Werte
wieder einspielt (`SUBSCRIBE_GRACE`, seit 3.6.1). Geprüft wird das Fenster in
3.23.0 so ([`src/HeishaMon.cpp`](src/HeishaMon.cpp), Zeile 814):

```cpp
if ((long)(millis() - setCommandsIgnoredUntil) < 0)   // "Fenster noch offen"
```

Die Frist wird nur beim Verbinden gesetzt (Zeilen 752 und 1388). `long` ist
auf dem ESP32 32 Bit breit. Liegt die letzte Verbindung **2³¹ ms = 24,86 Tage**
zurück, wird die Differenz negativ, und die Bedingung meldet wieder „offen“.
Ab dann wird **jedes Set-Kommando verworfen**, bis zum nächsten Reconnect
(oder weitere 24,86 Tage lang).

* **Still:** je Kommando nur eine Telnet-Zeile. Die Bilanzmeldung (Zeile 1484)
  hat denselben Vergleich und schweigt ebenfalls.
* **Irreführend:** Nach 12 min meldet die Webseite „Hausteuerung erreichbar,
  sendet aber seit … keine Vorgaben“ — die Steuerung sendet, die Firmware
  wirft weg.
* **Nicht betroffen:** der Notbetrieb (läuft vor der Karenzprüfung bzw. gar
  nicht über den MQTT-Callback).
* **Warum es kein Test gesehen hat:** Die Stelle liegt im Firmware-Klebecode
  und nicht in einem arduino-freien Header. Beide früheren Durchsichten haben
  das Karenzfenster als „überlaufsicher geprüft“ abgehakt, weil das Idiom
  allgemein als sicher gilt — es ist es nur für Fristen, die nach Ablauf
  nicht weiter geprüft werden.

Auf dem Host nachgerechnet (32-Bit-Arithmetik wie auf dem ESP32): Naht bei
24,855 Tagen nach dem Connect.

## Die Frist

Lesung im ioBroker am 2026-10-15 (nur lesend, simple-api):

| | Stufe 1 | Stufe 2 |
| --- | --- | --- |
| Firmware | 3.23.0 | 3.23.0 |
| Laufzeit seit Neustart | 26,5 Tage | 26,5 Tage |
| Letzter Connect (`ts` von `info/boot_reason`) | 2026-10-06 08:29:37 | 2026-10-06 08:29:37 |

Der letzte Connect stammt von einem Neustart des Adapters `mqtt.0` um 08:28.
Ohne ihn wären beide Stufen seit etwa 2026-10-14 betroffen gewesen.

**Schritt 1 am 2026-10-15 (15:48) wiederholt:** letzter Connect unverändert
2026-10-06 08:29:37 (beide Stufen), Naht 2026-10-31 05:01 in naiver Ortszeit
(= 04:01 CET, Sommerzeit endet am 2026-10-25), also 16 Tage entfernt.

**Naht = letzter Connect + 2³¹ ms + 5 s = 2026-10-31 04:01 CET**, sofern es
bis dahin keinen weiteren Reconnect gibt. Jeder Reconnect und jedes OTA
verschiebt sie nach hinten, nie nach vorn. Schritt 1 bestimmt sie neu.

**Rückfallebene:** Ist 3.23.1 bis **2026-10-28** nicht ausgerollt, startet der
Owner den Adapter `mqtt.0` im ioBroker-Admin einmal neu. Das schiebt die Naht
um weitere 24,86 Tage, behebt aber nichts.

## Lies zuerst — in dieser Reihenfolge

1. [`CLAUDE.md`](CLAUDE.md) — Harte Regeln, besonders 4 (Rettungsanker),
   5 (Eingriffe einzeln) und 6 (alle Envs, alle Hosttests).
2. [`Massnahmenplan-Codedurchsicht-2026-10-15.md`](Massnahmenplan-Codedurchsicht-2026-10-15.md),
   Befund S1.
3. [`src/HeishaMon.cpp`](src/HeishaMon.cpp) (Stand 3.23.0): Zeilen 127–130
   (Variable), 750–752 (Reconnect), 803–820 (Prüfung im Callback), 1367–1388
   (zweite Armierung am Ende von `setup()`), 1481–1495 (Bilanz). Dazu
   `SUBSCRIBE_GRACE` in [`src/HeishaMon.h`](src/HeishaMon.h), Zeilen 66–72.
4. [`README.md`](README.md), „Der Broker spielt beim Verbinden alles wieder
   ein (3.6.1)“, und [`Arbeitsplan-Robustheit-3.20.0.md`](Arbeitsplan-Robustheit-3.20.0.md),
   M1 — warum das Fenster am Ende von `setup()` **ein zweites Mal** geöffnet
   wird. Das muss erhalten bleiben.
5. [`src/sendwindow.h`](src/sendwindow.h) und
   [`test/sendwindow_test.cpp`](test/sendwindow_test.cpp) — das Muster, nach dem
   die neue Regel gebaut wird (arduino-frei, `uint32_t`, Überlauf im Hosttest).
6. Die Skills `/pruefen-vor-merge` und `/rollout` (`.claude/skills/`) — dieser
   Plan ergänzt sie nur um die Zusatzabnahme (Schritt 18).

## Entschieden — nicht neu aufmachen

| Nr. | Frage | Entscheid | Wann |
| --- | --- | --- | --- |
| A | Was soll 3.23.1 leisten? | Die kritischen Punkte der Durchsicht vom 2026-10-15 in **einer** Release abarbeiten; Einstieg in einer neuen Session über diesen Plan | Owner-Auftrag 2026-10-15 |
| E1 | Umfang von 3.23.1 | **Nur S1** plus die Regressionsprüfung aus E2; S2 (Telnet) und S3 (Watchdog) folgen in 3.24.0 | Owner-Bestätigung 2026-10-15 |
| E2 | Prüfung auf vorzeichenbehaftete `millis()`-Vergleiche | **Ja**, `test/zeitvergleich_test.py` (Schritt 9) | Owner-Bestätigung 2026-10-15 |
| E3 | Nachweis an der Anlage | **Passiv** (Schritt 18): Bilanzzeile nach dem Connect, danach Befehls-Wächter und Startseite | Owner-Bestätigung 2026-10-15 |

## Zu Beginn vom Owner zu bestätigen

*E1–E3 am 2026-10-15 vom Owner wie empfohlen bestätigt und in die Tabelle
oben übernommen; die Begründungen bleiben hier stehen.*

| Nr. | Frage | Empfehlung | Grund |
| --- | --- | --- | --- |
| E1 | Umfang von 3.23.1 | **Nur S1** plus die Regressionsprüfung aus E2. S2 (Telnet) und S3 (Watchdog) folgen in 3.24.0 | S1 ist der einzige *kritische* Befund und hat eine Frist. S1 lässt sich an der Anlage ohne Prüfling nachweisen; S2 und S3 brauchen den Prüfling samt OTA-Test und würden die Frist gefährden. Eine Version belegt eine Sache. |
| E2 | Eine Prüfung mitnehmen, die vorzeichenbehaftete `millis()`-Vergleiche in `src/` meldet? | **Ja** (`test/zeitvergleich_test.py`, Schritt 9) | Verhindert, dass das Muster zurückkommt; ändert die Firmware nicht |
| E3 | Wie wird an der Anlage nachgewiesen? | **Passiv** (Schritt 18): Bilanzzeile nach dem Connect, danach Befehls-Wächter und Startseite | Der Fehlerfall selbst ist an der Anlage nicht abzuwarten; die Naht belegt der Hosttest. Ein Testkommando bringt keinen zusätzlichen Beleg |

### Was aus dem Code folgt — kein Entscheid, aber festgelegt

* **Neuer Header `src/karenz.h`**, arduino-frei. `SUBSCRIBE_GRACE` zieht samt
  Begründung dorthin (Muster `sendwindow.h`: Regel und Konstante stehen
  beieinander und laufen zusammen durch den Hosttest).
* **Startzeit plus Merker statt Frist:** unsigned Differenz gegen die Dauer,
  der Merker schließt beim ersten Aufruf nach Ablauf. Damit gibt es keine
  Frist mehr, die nach Ablauf weiter verglichen wird.
* **Vorbedingung:** Die Regel muss häufiger als alle 49,7 Tage aufgerufen
  werden. `loop()` ruft sie in **jedem** Durchlauf auf, nicht nur bei
  verworfenen Kommandos (siehe Schritt 5).
* **Verhalten in den ersten 24,86 Tagen bleibt gleich:** 5 s ab Subscribe,
  zweite Armierung am Ende von `setup()`, Notbetriebszweig vor der Prüfung,
  Herzschlag der Verbindungswacht erst danach.
* **H1 und H2 gleich** — keine Abweichung zwischen den Stufen.
* **Version 3.23.1:** Fehlerbehebung ohne neue Funktion.
* **Backup-Boards** sind nicht betroffen (MQTT-Port 1884, nie verbunden),
  werden aber nach der Skill-Regel trotzdem nachgezogen.

## Arbeitsschritte

Jeder Schritt endet mit seinem Prüfkriterium. Gerätezugriffe, die etwas
verändern, stehen **einzeln** in einem Bash-Aufruf und werden vorher angesagt
(**HALT** = auf das Ok des Owners warten). Lesende Zugriffe auf produktive
Geräte ebenfalls nur mit Auftrag (CLAUDE.md, Regel 5).

### Phase A — Vorbereitung

0. **Dieser Plan auf `main`** (Doku-Ergänzung, CLAUDE.md Regel 4), mit
   Eintrag in `README.md` → „Aufbau“. *Erledigt am 2026-10-15 mit dem Anlegen
   dieser Datei.*

1. **Frist neu bestimmen — lesend, mit Ok des Owners.** Der Zeitstempel `ts`
   von `info/boot_reason` ist der letzte Connect (der Wert wird bei jedem
   Connect neu geschrieben):
   ```bash
   for p in panasonic_heat_pump panasonic_heat_pump2; do
     curl -s -m 5 "http://192.168.2.147:8087/get/mqtt.0.$p.info.boot_reason" \
     | python3 -c "import json,sys,datetime as d; j=json.load(sys.stdin); \
   t=d.datetime.fromtimestamp(j['ts']/1000); \
   print('$p letzter Connect', t, '-> Naht', t+d.timedelta(milliseconds=2**31+5000))"
   done
   ```
   (Die Naht ist in Ortszeit ohne Sommerzeitkorrektur gerechnet — über die
   Umstellung hinweg eine Stunde früher.)
   *Prüfkriterium:* Naht für beide Stufen notiert. **Liegt sie weniger als
   drei Tage entfernt: HALT** — erst startet der Owner `mqtt.0` neu, dann
   Schritt 1 wiederholen, dann weiter.

2. **Rettungsanker und Branch** (`Ablauf-Backup-Boards.md`, „Bei jeder
   Firmware-Änderung“, Schritt 1):
   ```bash
   git tag -a rettungsanker-JJJJ-MM-TT -m "Stand vor Karenzfenster-Fix (3.23.1)"
   git cat-file -t rettungsanker-JJJJ-MM-TT     # muss "tag" sein
   git checkout -b karenzfenster
   ```
   Ist der Tagesname vergeben: `rettungsanker-vor-karenzfenster-JJJJ-MM-TT`.
   *Prüfkriterium:* annotierter Tag, Branch ≠ `main`.

### Phase B — Code (Kommentare und Serial-Ausgaben ASCII, ohne Umlaute)

3. **`src/karenz.h` anlegen.** Kopfkommentar nach dem Muster von
   `sendwindow.h`: warum es das Fenster gibt (Wiedereinspielung, 3.6.1), warum
   es zweimal geöffnet wird (M1, 3.20.0), warum Startzeit plus Merker (dieser
   Befund, mit der Zahl 24,86 Tage) und die Vorbedingung. Der Kern:
   ```cpp
   #define SUBSCRIBE_GRACE 5000u // ms - Begruendung aus HeishaMon.h mitnehmen

   struct Karenzfenster
   {
     uint32_t beginn; // millis() beim Oeffnen
     bool offen;
   };

   inline void karenz_oeffnen(Karenzfenster *k, uint32_t jetzt)
   {
     if (!k)
       return;
     k->beginn = jetzt;
     k->offen = true;
   }

   // Fuehrt nach UND antwortet: schliesst beim ersten Aufruf nach Ablauf.
   // Vorbedingung: haeufiger als alle 49,7 Tage aufrufen - loop() tut das.
   inline bool karenz_offen(Karenzfenster *k, uint32_t jetzt, uint32_t dauer)
   {
     if (!k)
       return false;
     if (k->offen && (uint32_t)(jetzt - k->beginn) >= dauer)
       k->offen = false;
     return k->offen;
   }
   ```
   *Prüfkriterium:* übersetzt im Hosttest (Schritt 7) ohne Warnung.

4. **`src/HeishaMon.h`:** `#include "karenz.h"` zu den übrigen
   Regel-Headern; die Definition von `SUBSCRIBE_GRACE` samt Kommentar entfällt
   hier, an ihrer Stelle ein Einzeiler „steht in karenz.h“.

5. **`src/HeishaMon.cpp` — vier Stellen:**
   - Zeile 129: `unsigned long setCommandsIgnoredUntil = 0;` →
     `Karenzfenster karenz = {0, false};` (Kommentar darüber anpassen).
   - Zeile 752 (`mqtt_reconnect`) und Zeile 1388 (Ende von `setup()`):
     `karenz_oeffnen(&karenz, (uint32_t)millis());`. Den langen Kommentar
     über Zeile 1388 behalten, Variablennamen darin nachziehen.
   - Zeile 814 (`mqtt_callback`):
     `if (karenz_offen(&karenz, (uint32_t)millis(), SUBSCRIBE_GRACE))`.
   - Zeile 1484 (Bilanz in `loop()`) — der Aufruf steht **vor** der
     Bedingung, damit er in jedem Durchlauf läuft (Vorbedingung aus
     Schritt 3):
     ```cpp
     const bool karenzOffen = karenz_offen(&karenz, (uint32_t)millis(), SUBSCRIBE_GRACE);
     if ((ignoredSetCommands > 0) && !karenzOffen)
     ```
   - Kommentar Zeilen 1492–1495 („wie überall im Projekt überlaufsicher“)
     richtigstellen.
   *Prüfkriterium:* `grep -n "setCommandsIgnoredUntil" src/*.cpp src/*.h`
   findet nur noch historische Zeilen in `version.h`;
   `grep -nE "\((long|int|int32_t)\) *\( *millis" src/` findet nur noch
   `notbetrieb.cpp`.

6. **`src/notbetrieb.cpp`, Zeile 591** — Code unverändert, aber am Ende der
   Zeile der Marker `// kurzlebige Frist: endet mit der Schleife` mit einem
   Satz darüber, warum der vorzeichenbehaftete Vergleich hier zulässig ist
   (die Frist lebt höchstens `HYDRAULIK_LESEFRIST_MS` und wird danach nie
   wieder geprüft). Den Marker liest Schritt 9.

### Phase C — Tests

7. **`test/karenz_test.cpp`** (eigenständig, bindet nur `src/karenz.h` ein;
   Aufbau wie `sendwindow_test.cpp`, Zusicherungen mit Klartext, Rückgabewert
   ≠ 0 bei Fehler):
   1. Anfangszustand `{0, false}`: nie offen, auch nicht bei `jetzt = 0`.
   2. Grundfall: öffnen bei 1000 → offen bei 1000 und 5999, zu bei 6000,
      bleibt zu bei 6001.
   3. Zählerüberlauf im Fenster: öffnen bei `0xFFFFF000` → offen bei
      `0xFFFFF000 + 4999` (übergelaufen), zu bei `+ 5000`.
   4. **Der Befund:** öffnen bei 60000, nachführen bei 65000 (zu) → zu bei
      `+ 2³¹ − 1`, `+ 2³¹`, `+ 2³¹ + 1 Tag`, `+ 2³² − 1`, `+ 2³² + 1000`.
   5. Erster Aufruf erst nach 2³¹ ms ohne Nachführen dazwischen → zu.
      Dazu als Kommentar die Grenze der Vorbedingung: ohne jeden Aufruf
      über volle 2³² ms wäre das Fenster wieder offen — deshalb der Aufruf
      in jedem `loop()`-Durchlauf.
   6. Wieder öffnen nach dem Schließen (Reconnect) → offen, 5 s später zu.
   7. Öffnen während das Fenster offen ist (zweite Armierung, M1) → misst ab
      dem neuen Zeitpunkt.
   8. Nullzeiger → `false`, kein Absturz.
   9. **Gegenbeispiel im Test:** die alte Regel
      `(int32_t)(jetzt - frist) < 0` als lokale Funktion; Zusicherung, dass
      sie bei `+ 2³¹` „offen“ liefert. Belegt, dass Fall 4 die Naht wirklich
      trifft.
   *Prüfkriterium:* grün mit `c++ -std=c++17 -O2 -Wall`.

8. **Gegenprobe von Hand, nicht eingecheckt:** in `karenz_offen()` die alte
   Regel einbauen → Fall 4 muss rot werden. Ergebnis (welche Zusicherungen
   brechen) im Changelog vermerken, dann zurückbauen.

9. **`test/zeitvergleich_test.py`** (nur Standardbibliothek, Muster
   `css_klassen_test.py`): durchsucht `src/*.cpp` und `src/*.h` nach
   `(long)`, `(int)` oder `(int32_t)` vor `(millis()` sowie nach direkten
   Vergleichen `millis() <`, `millis() >`, `< millis()`, `> millis()`.
   Erlaubt nur mit dem Marker `kurzlebige Frist` in derselben Zeile.
   Gegenprobe: ohne den Marker aus Schritt 6 muss die Prüfung rot werden.
   *Prüfkriterium:* grün; meldet bei Befund Datei und Zeile.

10. **Listen nachziehen:**
    - `test/hosttests.sh`: `cpp_test karenz_test` hinter `sendwindow_test`
      und `py_test zeitvergleich_test` hinter `repo_konsistenz_test`, je mit
      Begründungskommentar (warum es den Test gibt — dieser Befund).
    - `test/README.md`, Abschnitt „Werkzeuge“: je eine Zeile für beide
      Dateien.
    *Prüfkriterium:* `./test/hosttests.sh` grün, 17 Tests; ohne Listeneintrag
    meldet das Skript selbst ROT.

### Phase D — Dokumentation (Markdown mit Umlauten)

11. **`src/version.h`:** `heishamon_version` → `3.23.1`; Changelog-Eintrag
    oben mit
    - **Problem:** Naht nach 24,86 Tagen, still, irreführende Anzeige, warum
      die Durchsichten es übersehen haben;
    - **Live-Stand 2026-10-15:** nicht betroffen, nur durch den
      Adapter-Neustart vom 2026-10-06;
    - **Fix:** `karenz.h`, Startzeit plus Merker, Aufruf in jedem Durchlauf;
    - **NACHWEIS:** `karenz_test` samt Gegenprobe (Schritt 8),
      `zeitvergleich_test` samt Gegenprobe, alle Hosttests, alle Envs,
      Zusatzabnahme an der Anlage (Verweis auf Schritt 18, Ergebnis kommt in
      diesen Plan — nicht in `src/`, weil der Tag dann schon steht);
    - **GROESSE** gegen 3.23.0 (Format: `/pruefen-vor-merge`, Schritt 6).

12. **`README.md`:**
    - „Aufbau“: Zeile für `src/karenz.h` (Muster `sendwindow.h`).
    - Abschnitt „Der Broker spielt beim Verbinden alles wieder ein (3.6.1)“:
      ein Absatz zu 3.23.1 — was nach 24,86 Tagen passierte und wie es jetzt
      gebaut ist.

13. **`CLAUDE.md`**, „Konventionen beim Ändern“: `karenz.h` in die Liste der
    arduino-freien Header aufnehmen.

14. **Nachträge in den früheren Durchsichten**, damit deren „geprüft“ nicht
    weiter gilt:
    - [`Massnahmenplan-Codedurchsicht-2026-08-18.md`](Massnahmenplan-Codedurchsicht-2026-08-18.md),
      „Ausdrücklich geprüft und in Ordnung“, Punkt `millis()`-Arithmetik;
    - [`Massnahmenplan-Codedurchsicht-2026-09-02.md`](Massnahmenplan-Codedurchsicht-2026-09-02.md),
      „Ausdrücklich geprüft und in Ordnung“, Punkt `millis()`-Überlauf.
    Je ein kurzer „Nachtrag 2026-10-…“ mit Verweis auf 3.23.1.

*Prüfkriterium Phase D:* `./test/hosttests.sh --schnell` grün
(`repo_konsistenz_test` prüft Aufbau, Werkzeugtabelle, CLAUDE.md-Pfade und
Links). Texte von Hand gegenlesen.

### Phase E — Prüfen vor dem Merge

15. **`/pruefen-vor-merge`** — Rettungsanker, Version und Changelog, ASCII,
    Einheitlichkeit H1/H2 (erwartet: kein Treffer), alle Hosttests, alle
    Envs, RAM/Flash-Delta gegen den Rettungsanker (erwartet: wenige Byte).
    *Prüfkriterium:* alles grün, GROESSE-Zeile im Changelog.

### Phase F — Rollout (`/rollout 3.23.1`, vom Owner aufgerufen)

Der Skill führt Merge, Tag, OTA, Abnahme, Backups, Push und Releases. Dieser
Plan ergänzt ihn nur um die Zusatzabnahme in Schritt 18. **Der Rollout muss
vor der Naht aus Schritt 1 abgeschlossen sein.**

16. **Merge und Tag, lokal** (Skill, Schritt 1). Kurztitel „Karenzfenster
    überlaufsicher (3.23.1)“.

17. **Baseline und OTA auf H1** (Skill, Schritte 2 und 3). Erwartung im
    `tablesnap`-Diff: nur laufende Messwerte.

18. **Zusatzabnahme H1**, lesend:
    1. **Das Fenster wirkt noch:** Innerhalb von rund 10 s nach dem
       Connect steht im Log die Bilanz „N wiedereingespielte Set-Kommandos
       nach dem Verbinden verworfen“ (N > 0; am 2026-09-02 waren es 37).
       Lesen über `/log`
       (Notbetriebs-Zugang) oder `test/mqtt_sub.py` auf
       `panasonic_heat_pump/info/log`.
    2. **Die neue Uhr läuft:** `ts` von `info/boot_reason` im ioBroker =
       Zeitpunkt des OTA (Befehl aus Schritt 1).
    3. **Kommandos kommen nach dem Fenster durch:** nach mindestens 15 min
       kein oranges „sendet aber seit …“ auf der Startseite, und der
       `WP_Befehls_Waechter` meldet keine Abweichung.
    *Prüfkriterium:* alle drei erfüllt. Sonst **HALT** — Rückweg nach Skill.

19. **H2** wie Schritte 17 und 18 (Skill, Schritt 4), Prefix
    `panasonic_heat_pump2`.

20. **Backup-Boards** (Skill, Schritt 5).

21. **Push, CI, Releases** (Skill, Schritte 6 und 7).

### Phase G — Abschluss

22. **Nachtrag und Aufräumen:**
    - Kopf dieses Plans auf **erledigt**, mit Datum und dem Ergebnis aus
      Schritt 18.
    - Im Maßnahmenplan vom 2026-10-15 S1 auf „erledigt in 3.23.1“.
    - Memory `projektstand-heishamon` überschreiben (Rollout 3.23.1, der
      DRINGEND-Punkt entfällt).
    - `/abschluss`.

## Rückweg

Nach dem Skill `/rollout`, Abschnitt „Rückweg“. **Achtung:** Ein Rückfall auf
3.23.0 bringt den Fehler zurück. Dann gilt wieder die Frist aus Schritt 1
(letzter Connect + 24,86 Tage), und sie muss bis zu einem neuen Versuch
beobachtet werden.
