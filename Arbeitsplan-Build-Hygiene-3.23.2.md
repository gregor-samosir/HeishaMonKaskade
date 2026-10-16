# Arbeitsplan: Build-Hygiene (3.23.2)

> **Ergebnis: in Arbeit (angelegt 2026-10-16).** Paket P2 der Codedurchsicht
> vom 2026-10-15
> ([`Massnahmenplan-Codedurchsicht-2026-10-15.md`](Massnahmenplan-Codedurchsicht-2026-10-15.md):
> S5, C2, C5a, C5f und die Kommentarbeispiele aus C3). **Ohne
> Funktionsänderung** — der Nachweis ist ein Größenvergleich der `.elf`, kein
> Gerätetest. Der Rollout (`/rollout 3.23.2`) ist Sache des Owners.

## Worum es geht

* **S5, Build nicht reproduzierbar:** `sstaub/Ticker` steht ohne Version in
  `platformio.ini` und trägt die gesamte Zeitkette; die übrigen Bibliotheken
  stehen mit `^`, die CI installiert PlatformIO mit `--upgrade`. Ein frischer
  Rechner baut so unter Umständen eine andere Firmware als die geflashte.
* **C2, Warnungen abgeschaltet:** `build_unflags = -Wall`. Mit `-Wall -Wextra`
  ergeben sich 21 Warnungen im eigenen Code, alle harmlos — aber nur sichtbar,
  solange jemand sie anschaltet. Dazu 3 × `%u` mit `uint32_t`: auf dem ESP32 ist
  `uint32_t` `unsigned long`, auf dem Mac `unsigned int` — der Hosttest kann
  diese Warnung nie zeigen.
* **C5a, C5f, C3:** ein toter Zweig, ein widersprüchlicher Kommentar und
  veraltete Zahlen in Kommentaren.

## Lies zuerst

1. [`CLAUDE.md`](CLAUDE.md), harte Regeln 4 (Rettungsanker) und 6 (alle Envs, alle
   Hosttests).
2. Der Maßnahmenplan, Befunde S5, C2, C3, C5.
3. Die Memory-Regel „Nachweis: Umbau ohne Funktionsänderung“: `text/data/bss`
   der `.elf`, nie die Prüfsumme der `.bin`.
4. Die Skills `/pruefen-vor-merge` und `/rollout` (`.claude/skills/`).

## Entschieden — nicht neu aufmachen

| Nr. | Frage | Entscheid | Wann |
| --- | --- | --- | --- |
| E1 | Umfang von 3.23.2 | S5, C2, C5a, C5f und die C3-Kommentarbeispiele — wie im Maßnahmenplan | Owner-Bestätigung 2026-10-16 |
| E2 | Transitive Bibliothek `NetApiHelpers` (kommt über `TelnetStream`) | **Mitpinnen**, als eigene Zeile in `lib_deps` | Owner-Bestätigung 2026-10-16 |
| E3 | `-Werror` für `src/` | **Ja**, fest in `build_src_flags`, nachdem der Build ohne Warnung läuft | Owner-Bestätigung 2026-10-16 |
| E5 | Form | Eigener Arbeitsplan nach dem Muster von 3.23.1 (diese Datei) | Owner-Bestätigung 2026-10-16 |

*Technische Wahl, kein Owner-Entscheid:* `PRIu32` (`<inttypes.h>`) statt eines
Casts für die drei `%u` — passt auf ESP32 (`lu`) und Mac (`u`) ohne Annahme über
die Typbreite.

## Schritte

Jeder Schritt endet mit seinem Prüfkriterium. Kein Gerätezugriff in diesem Plan.

1. **Rettungsanker und Branch** (`Ablauf-Backup-Boards.md`, „Bei jeder
   Firmware-Änderung“, Schritt 1): annotierter Tag `rettungsanker-2026-10-16`,
   Branch `build-hygiene`. *Prüfkriterium:* `git cat-file -t` meldet `tag`,
   Branch ≠ `main`.
2. **Baseline vor jeder Änderung:** alle sechs Envs bauen, `text/data/bss` jeder
   `.elf` und `nm --size-sort` von `heishamon_esp32_h1_ota` festhalten; einmal mit
   `-Wall -Wextra` bauen, um die 21 Stellen zu bestätigen (Orte können sich seit
   3.23.1 verschoben haben). *Prüfkriterium:* Zahlen im Abschnitt „Messwerte“.
3. **S5, Pins:** `platformio.ini` `[env] lib_deps` auf die heute aufgelösten
   Stände (`PubSubClient@2.8.0`, `ArduinoJson@7.4.3`, `TelnetStream@1.3.0`,
   `NetApiHelpers@1.0.2`, `WiFiManager@2.0.17`, `Ticker@4.4.0`); in
   `.github/workflows/main.yml` `pip install platformio==6.1.19`.
   *Prüfkriterium:* `pio pkg list` zeigt dieselben Stände wie vorher.
4. **C2, Warnungen:** `build_unflags = -Wall` entfernen,
   `build_src_flags = -Wall -Wextra -Wno-unused-parameter`; die Stellen beheben
   (Kommentarköpfe schließen, `PRIu32`, Zeitstempelpuffer), ohne das Verhalten zu
   ändern. Erst danach `-Werror` anhängen. *Prüfkriterium:* alle Envs bauen ohne
   Warnung, auch mit `-Werror`.
5. **Kleinpunkte:** C5a (`"unused"`-Zweig in `webfunctions.cpp`), C5f (Kommentar
   in `write_hex_log`), C3 (die Beispiele aus dem Maßnahmenplan: 1,5 s → geltende
   Zahl, 48 s → 88 s, „bis 111 bei 99 Zeilen“ → 112). Kommentar nennt die
   geltende Regel, Entstehungsgeschichte steht in `version.h`.
6. **Nachweis:** `text/data/bss` gegen Schritt 2. Erwartet: gleich, außer den
   beiden Stufen-Envs (C5a, wenige Byte weniger) und der Versionszeichenkette;
   `nm --size-sort` zeigt, *welche* Funktion sich geändert hat;
   `test/decode_vergleich.py` identisch. Die Versionsnummer wird erst danach
   gezogen.
7. **`src/version.h`:** 3.23.2 mit Problem, Nachweis und GROESSE-Zeile.
8. **`/pruefen-vor-merge`.** Danach Stopp — Merge, Tag und OTA über
   `/rollout 3.23.2`, vom Owner aufgerufen.
9. **Nachtrag:** Kopf dieses Plans auf **erledigt**, Maßnahmenplan (S5, C2, C5a,
   C5f, C3-Beispiele), Memory `projektstand-heishamon`.

## Messwerte

*(wird in Schritt 2 und 6 gefüllt)*

## Rückweg

Reine Build-Änderung, nichts ist ausgerollt, bevor der Owner `/rollout` aufruft:
Verwerfen genügt (`git checkout main`, Branch löschen). Nach dem Rollout gilt der
Rückweg des Skills `/rollout`; der Rettungsanker ist `rettungsanker-2026-10-16`.
