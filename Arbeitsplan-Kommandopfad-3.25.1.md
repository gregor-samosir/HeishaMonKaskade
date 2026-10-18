# Arbeitsplan: Kommandopfad als Header (3.25.1)

> **Ergebnis: umgesetzt und geprüft (2026-10-18), Rollout steht aus.** Paket P5
> der Codedurchsicht vom 2026-10-15
> ([`Massnahmenplan-Codedurchsicht-2026-10-15.md`](Massnahmenplan-Codedurchsicht-2026-10-15.md):
> C1, C5b, C5c). **Ohne Funktionsänderung** — der Nachweis ist ein Vergleich
> aller Kodierungen und der `.elf`, kein Gerätetest. Branch
> `kommandopfad-header`, Rettungsanker `rettungsanker-2026-10-18`.
>
> **Offen:** Merge nach `main`, Tag `v3.25.1`, `/rollout 3.25.1` auf Aufruf des
> Owners (Abnahme am Gerät: siehe Schritt 8).

## Worum es geht

Die Tabelle `setCommands[]`, die Umrechnung, die Maske und die Konfliktprüfung
standen in `src/commands.cpp`, das an der Arduino-Welt hängt. `merge_test` und
`byte9/20/28_test` bildeten die Merge-Zeile deshalb nach und trugen Masken und
Parameter als eigene Konstanten. Ein Fehler in einer Tabellenzeile ließ alle
C++-Tests grün. Das Beispiel aus dem Befund: Parameter 64 statt 16 bei SET40
(Maske `0x30`) ergibt `0x40 & 0x30 = 0` — das Kommando sendet still „keine
Änderung“.

Dazu zwei Kleinpunkte: **C5b**, die Tabellen in `notbetrieb.h` waren
`static const` und lagen je Übersetzungseinheit, die sie einbindet, einmal im
Abbild; **C5c**, `cleanCommand` wurde zur Laufzeit aus `mainCommand` kopiert und
stand daneben als zweite Kopie derselben 110 Bytes.

## Lies zuerst

1. [`CLAUDE.md`](CLAUDE.md), harte Regeln 4 (Rettungsanker) und 6 (alle Envs,
   alle Hosttests); Abschnitt „Prüfbare Regeln kommen in arduino-freie Header“.
2. [`src/setpayload.h`](src/setpayload.h) — das Muster, dem der neue Header folgt.
3. Der Nachweis für Umbauten ohne Funktionsänderung: `text/data/bss` und
   `nm --size-sort` der `.elf`, nie die Prüfsumme der `.bin`.

## Entscheide

Alle am 2026-10-18 vom Owner, jeweils wie empfohlen:

| | Frage | Entscheid |
| --- | --- | --- |
| E1 | Versionsnummer | **3.25.1**, Patch ohne Funktionsänderung (wie 3.23.2) |
| E2 | C5c: wie weit? | **Eine Vorlage**: `KOMMANDO_VORLAGE` einmal im Flash, `mainCommand` startet leer und wird in `setup()` daraus gefüllt; `cleanCommand` und der 110-Byte-Initialisierer entfallen |
| E3 | Zusatztest | **Ja**: jeder Name und jeder Festwert der Notbetriebs-Tabellen gegen `setCommands[]` (vorher nur eine Laufzeitprüfung in `notbetrieb.cpp`) |

**Einheitlichkeit H1/H2:** geprüft, kein Unterschied — die Tabelle ist
rollenunabhängig, es entsteht keine Abweichung (STOPP-Regel nicht berührt).

## Schritte

- [x] **1. Rettungsanker, Branch.** `rettungsanker-2026-10-18` auf `3e03ed7`,
      Branch `kommandopfad-header`.
- [x] **2. Vorher festhalten.** Liste aller Paare (SET, Wert) → (Byte, Maske,
      Rohwert) aus der alten Tabelle mit dem Parser `stz.set_kommandos()` in
      `set_top_zuordnung.py`: **2265 Zeilen**, SHA-256
      `d430a6e072f033b4045c220fe1fa1191d4b21b249c87b13b7aaaf8da275371d2`. Dazu
      `text/data/bss` der `.elf` aller sechs Envs und `nm --size-sort` von
      `heishamon_esp32_h1_ota`.
- [x] **3. `src/setcommands.h`** (arduino-frei): Tabelle als `inline constexpr`,
      `setcommand_finden()`, `setcommand_kodieren()`, `setcommand_anwenden()`
      (Maskenzeile und Konfliktmeldung), `kommando_zuruecksetzen()`,
      `KOMMANDO_VORLAGE`. `QUERYSIZE` hat nur noch hier eine Definition. Die
      Kommentarblöcke zur Tabelle sind mit umgezogen.
- [x] **4. Tabellenprüfung zur Übersetzungszeit.** `setcommands_pruefen()` mit
      `static_assert` im Header. Regeln: Name und Nummer einmalig; `pos` 4..109;
      Maske ≠ 0; `min ≤ max`, höchstens 256 Werte; bei `CONV_OPMODE` Bereich
      0..6; für **jeden** Wert von `min` bis `max` liegt der Rohwert in 0..255
      und vollständig in der Maske; Überlappung am selben Byte nur für das
      dokumentierte Paar QuietMode/PowerfulMode (Byte 7, beide `0xFF`).
- [x] **5. `commands.cpp` ausdünnen.** Bleibt: Abonnieren, Payload-Annahme,
      Logzeilen. `set_command_range()` ruft `setcommand_finden()`, die
      Schnittstelle in `commands.h` ist unverändert. `setabgelehnt_merken()`
      steht weiter **vor** `write_mqtt_log()` (Pufferfalle aus 3.25.0).
- [x] **6. C5c in `HeishaMon.cpp`.** `cleanCommand` und das `memcpy` am Ende von
      `setup()` entfallen; `kommando_zuruecksetzen()` als erste Zeile von
      `setup()` und nach dem Senden.
- [x] **7. C5b in `notbetrieb.h`.** `static const` → `inline constexpr`.
- [x] **8. Tests.** `merge_test`, `byte9/20/28_test` rufen den echten Code
      (`setcommand_anwenden()` über den Topic-Namen; die Gegenproben „ohne
      Maske“ tauschen die Maske an einer Kopie der Zeile). Neu
      `test/setcommands_test.cpp` (Fall 1 bis 5, siehe unten), eingetragen in
      `test/hosttests.sh`; `decode_hosttest.sh` kopiert `setcommands.h` ins
      Bauverzeichnis. `set_top_zuordnung.py` liest jetzt `setcommands.h` und
      auch `opModeBytes[]` von dort (vorher eine Handkopie).
- [x] **9. Doku.** Verweise `commands.cpp` → `setcommands.h` in `README.md`,
      `Byte-Zuordnung.md`, `SET-TOP-Zuordnung.md`, `MQTT-Topics.md`,
      `test/README.md`, `CLAUDE.md` (Liste der arduino-freien Header) und in den
      Kommentaren unter `src/`. Changelog 3.25.1 in `src/version.h`.
- [ ] **10. Merge, Tag, Rollout.** `/pruefen-vor-merge`, `git merge --no-ff`,
      annotierter Tag `v3.25.1`, dann `/rollout 3.25.1` **auf Aufruf des
      Owners**. Abnahme am Gerät je Stufe: `info/version` 3.25.1,
      `tablesnap`-Diff nur Messwerte, ein Re-Assert als `<SUB>`-Zeilen
      angenommen (`Lage` 0), keine „Field conflict“-Warnung, Notbetrieb bereit
      (`fehlendMaske` 0, `Sperre` 0). Backups nachziehen. Kein Prüfling nötig.

## Nachweis

**1. Vorher/Nachher-Liste — identisch.** Dieselben 2265 Zeilen aus drei
unabhängigen Wegen, SHA-256 gleich:

| Weg | Quelle |
| --- | --- |
| vorher | alte `commands.cpp`, Python-Parser |
| nachher (C++) | `setcommands_test --liste` auf dem neuen Header — zweiter, unabhängiger Kodierer |
| nachher (Python) | auf den Header umgestellter Parser |

**2. Hosttests.** Alle 21 grün (`./test/hosttests.sh`), alle sechs Envs bauen
ohne Warnung unter `-Werror`. `decode_vergleich.py`: identisch (99 Topics,
74844 Zeilen). `setcommands_test`:

1. die echte Tabelle erfüllt alle Regeln (38 Zeilen);
2. **Gegenprobe je Regel** an einer kaputten Kopie der Tabelle — 16 Fälle plus `nullptr` als Tabelle, jeder
   scheitert an genau der erwarteten Regel und genau der erwarteten Zeile
   (u. a. SET40 Parameter 64 → `AUSSERHALB`, Maske 0 → `MASKE`, `pos` 3 und 110
   → `POSITION`, Rohwert 265 und −5 → `ROHWERT`, `ForceHeater` mit der Maske von
   `HolidayMode` → `UEBERLAPP`, Byte 7 mit Maske `0x7F` → die Ausnahme gilt
   nicht);
3. Stichproben der Kodierung, von Hand aus Doku und Messungen aufgeschrieben
   (21 Werte), nicht aus der Tabelle gerechnet;
4. `setcommand_anwenden()`: Nachbarfelder bleiben, Wiederholung überschreibt,
   Konflikt wird gemeldet, Unfug (Wert außerhalb, `nullptr`, `pos` hinter dem
   Puffer) ändert nichts; die leere Vorlage;
5. Notbetriebs-Tabellen: 9 Set-Schritte Heizen und 5 Wasser nennen nur Namen
   aus `setCommands[]`, Festwerte im Bereich, Indizes auf gehaltene Werte.

**3. Gegenprobe am Build.** SET40 Parameter 64 statt 16 eingetragen: der
Firmware-Build bricht am `static_assert`; `setcommands_test` (der ohne
`static_assert` übersetzt, um zu berichten) meldet `AUSSERHALB, Zeile 37` und
zwei Stichproben (3 Zusicherungen rot); `merge_test`, `byte9/20/28_test` und
`doku_zuordnung_test` werden ebenfalls rot — 6 von 21 Hosttests. Zurückgenommen,
wieder grün.

**4. `.elf`-Vergleich** (`heishamon_esp32_h1_ota`, `nm --size-sort`): nur die
erwarteten Stellen.

| Stelle | Änderung |
| --- | --- |
| `build_heatpump_command` | −16 Byte |
| `set_command_range` | −34 Byte |
| `setcommand_finden` (neu) | +66 Byte |
| `setup` | +12 Byte |
| `mainCommand` | von `.data` nach `.bss` |
| `cleanCommand` | weg |
| `KOMMANDO_VORLAGE` (neu) | 110 Byte im Flash |
| Tabellen aus `notbetrieb.h` | je einmal statt doppelt (z. B. `NOTBETRIEB_WERTE_HEIZEN`) |
| neue Fehlermeldung „Command … could not be applied“ | +34 Byte Text, für einen nicht erreichbaren Fall |

**Größe** (identisch für alle Envs derselben Stufe): RAM 61512 → 61400 Byte
(−112), Flash +64 Byte (1226437 → 1226501 an H1, 1225809 → 1225873 an H2). Aus den
Abschnitten: `.dram0.data` −112, `.bss` unverändert groß
(`cleanCommand` −110, `mainCommand` +110), `.flash.text` +32,
`.flash.rodata` +144.

**Was sich gegenüber dem Plan verschoben hat.** Erwartet war `data` −110,
`bss` −110, `text` ≈ +110. Tatsächlich bleibt `bss` gleich, weil `mainCommand`
dorthin zieht, während `cleanCommand` wegfällt. Und die Tabellen aus
`notbetrieb.h` lagen auf dem ESP32-S3 schon vorher im Flash — der Gewinn aus C5b
ist die entfallene Doppelkopie, nicht RAM.

## Was dieser Nachweis nicht abdeckt

Das Zusammenspiel von `build_heatpump_command()` mit dem MQTT-Callback und dem
Senden bleibt Firmware-Klebecode. Das schließt die Abnahme nach dem Rollout
(Schritt 10). Den nicht erreichbaren Zweig „Command … could not be applied“
kann kein Test auslösen, ohne die Tabelle zu verbiegen — er steht da, damit ein
künftiger Fehler dort sichtbar wird statt ein leeres Kommando zu melden.

## Nicht in diesem Paket

C5d, C5e (Weboberfläche, P6) und C3, C4, D4 (Struktur, P7). Die bekannte
Einschränkung der Tabellenprüfung: Sie prüft jede Zeile für sich und gegen ihre
Vorgängerinnen, nicht, ob eine Zeile **fachlich** richtig ist (richtiges Byte,
richtige Bits für das gemeinte Feld). Das belegen weiter die Stichproben und die
Messungen an der Anlage — `SET-TOP-Zuordnung.md` und `Byte-Zuordnung.md` gegen
den Code prüft `doku_zuordnung_test.py`.
