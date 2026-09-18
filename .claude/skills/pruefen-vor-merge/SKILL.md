---
name: pruefen-vor-merge
description: Prüft einen Arbeits-Branch der HeishaMon-Kaskade vor dem Merge — Rettungsanker, Version und Changelog, Einheitlichkeit H1/H2, alle Hosttests, alle Envs, RAM/Flash-Delta. Nur lesen und bauen, kein Gerätezugriff. Verwenden, wenn eine Änderung fertig ist und gemergt oder ausgerollt werden soll.
---

# Prüfen vor dem Merge

Diese Prüfung ist das Tor vor dem Merge (CLAUDE.md, Regel 6). Sie **ändert nichts
und fasst kein Gerät an** — sie baut, testet und meldet. Was fehlt, wird
gemeldet, nicht stillschweigend nachgetragen: Changelog-Texte schreibt nur, wer
den Inhalt der Änderung kennt, und das entscheidet der Owner mit.

Der Ablauf, in den diese Prüfung gehört, steht in `Ablauf-Backup-Boards.md` unter
„Bei jeder Firmware-Änderung" (Schritt 2). Danach folgt `/rollout`.

## 1. Ausgangslage

```bash
git branch --show-current            # darf nicht main sein
git status --short                   # uncommittete Reste nennen
ANKER=$(git tag --merged HEAD --list 'rettungsanker-*' --sort=-creatordate | head -1)
git cat-file -t "$ANKER"             # muss "tag" sein (annotiert), nicht "commit"
git diff --stat "$ANKER"...HEAD
```

- Steht der Branch auf `main` oder fehlt ein Rettungsanker: **abbrechen** und melden.
- Ist der Anker ein einfacher Tag: melden (Regel seit 2026-09-17: immer annotiert),
  aber weiterprüfen.
- Aus dem Diff festhalten: **Ist `src/` betroffen?** Nur dann ist es eine
  Firmware-Änderung mit Version, Changelog und Größenvergleich (Schritte 2, 3, 6).

## 2. Version und Changelog (nur bei Änderung in `src/`)

- `heishamon_version` in `src/version.h` muss sich gegenüber `$ANKER` geändert haben
  (`git show "$ANKER":src/version.h | grep heishamon_version`).
- Der oberste Changelog-Eintrag trägt diese Version und enthält **Problem**,
  **NACHWEIS** und **GROESSE** (RAM/Flash-Delta, Format siehe Schritt 6).
- Quelltext-Kommentare und Serial-Ausgaben ASCII ohne Umlaute:
  `git diff "$ANKER"...HEAD -- src/ | grep '^+' | perl -ne 'print if /[^\x00-\x7F]/'`
  (Treffer in Texten der Weboberfläche sind erlaubt, alles andere melden).
  Nicht `grep -P` (gibt es unter macOS nicht) und nicht `LC_ALL=C grep` mit
  Bytebereich — der findet unter macOS auch in „Grün" nichts und meldet damit
  immer sauber (nachgeprüft 2026-09-17).

## 3. Einheitlichkeit H1/H2 — STOPP-Punkt

Die Firmware unterscheidet die Stufen über die Rolle (`rolle`,
`NOTBETRIEB_ROLLE_*`) und die Stufen-Flags aus `platformio.ini`. Den Diff darauf
durchsehen:

```bash
git diff "$ANKER"...HEAD -- src/ platformio.ini | grep -n -i -E 'rolle|ROLLE_|stage_h|HEISHA_STAGE'
```

**Behandelt die Änderung Heizen (H1) und Warmwasser (H2) neu unterschiedlich,
hier anhalten** und die Abweichung dem Owner vorlegen, bevor weitergeprüft wird.
Ziel ist Einheitlichkeit; eine Abweichung braucht einen ausdrücklichen Entscheid,
keine Annahme aus dem Entwurf (Lehrstück: 3.22.0).

## 4. Hosttests

```bash
./test/hosttests.sh
```

Grün heißt: letzte Zeile `HOSTTESTS GRUEN`, Rückgabewert 0. Bei ROT die
gescheiterten Tests aus der Zusammenfassung nennen. Neue arduino-freie Regeln
gehören mit Test in die Liste in `test/hosttests.sh`.

## 5. Alle Envs bauen

Genau diese Form verwenden — in zsh wird eine Variable ohne Anführungszeichen
**nicht** in Wörter geteilt, `pio run $envs` meldet dann einen einzigen unbekannten
Env-Namen. Den Rückgabewert von `pio` selbst auswerten, **nie durch eine Pipe**
(sonst zählt der Exit-Code von `grep`/`tail`, und ein gescheiterter Build sieht
grün aus — am 2026-09-17 genau so passiert):

```bash
P=~/.platformio/penv/bin/pio
$P run $($P project config --json-output \
  | python3 -c "import json,sys; print(' '.join('-e '+s[0][4:] for s in json.load(sys.stdin) if s[0].startswith('env:')))") \
  > "$TMPDIR/pio_alle.log" 2>&1
echo "pio-Exit: $?"
grep -E 'SUCCESS|FAILED' "$TMPDIR/pio_alle.log" | tail -8
```

Der Build dauert mit warmem Cache unter einer Minute, kalt mehrere — vorher sagen.

## 6. RAM/Flash-Delta (nur bei Änderung in `src/`)

Neuer Stand: die Zeilen `RAM:` und `Flash:` für `heishamon_esp32_h1_ota` und
`heishamon_esp32_h2_ota` aus `$TMPDIR/pio_alle.log`.

Vergleichsstand in einem eigenen Worktree bauen, damit der Arbeitsstand unberührt
bleibt. **Nacheinander**, nicht parallel zu Schritt 5:

```bash
BASIS="$TMPDIR/heisha_basis"
git worktree add "$BASIS" "$ANKER"
cp platformio_user_env.ini "$BASIS/"
~/.platformio/penv/bin/pio run -d "$BASIS" -e heishamon_esp32_h1_ota -e heishamon_esp32_h2_ota \
  > "$TMPDIR/pio_basis.log" 2>&1; echo "pio-Exit: $?"
grep -E '^(RAM|Flash):' "$TMPDIR/pio_basis.log"
git worktree remove --force "$BASIS"
```

Format im Changelog (Beispiel 3.22.0):
`GROESSE gegen 3.21.0 (heishamon_esp32_h1_ota): RAM 61952 -> 61952 Byte (+0), Flash 1222681 -> 1223413 Byte (+732).`

Soll die Änderung das Verhalten **nicht** ändern (Umbau, Aufräumen), ist der
schärfere Nachweis `text`/`data`/`bss` der `.elf` — nie die Prüfsumme der `.bin`,
die trägt das Compile-Datum:

```bash
~/.platformio/packages/toolchain-xtensa-esp-elf/bin/xtensa-esp32s3-elf-size .pioenvs/heishamon_esp32_h1_ota/firmware.elf
```

## 7. Doku mitgezogen?

Nur prüfen und melden:

- Neue Datei → Tabelle in `README.md`, Abschnitt „Aufbau".
- Neues Werkzeug unter `test/` → Tabelle „Werkzeuge" in `test/README.md`.
- Neuer arduino-freier Header → Liste in `CLAUDE.md`, „Konventionen beim Ändern".
- Neues oder geändertes Topic → `MQTT-Topics.md` (englisch), `Byte-Zuordnung.md`
  und bei Set-Kommandos `SET-TOP-Zuordnung.md`. Die Spalten beider Zuordnungen
  prüft `doku_zuordnung_test` in Schritt 4 mit; die Topic-Listen in Abschnitt 3
  von `SET-TOP-Zuordnung.md` nicht — die von Hand ansehen.
- Nichts aus `doku-intern/` zitiert, auch nicht sinngemäß, auch nicht in
  Commit-Messages.

## 8. Ergebnis

Eine kurze Tabelle, je Schritt **grün / rot / entfällt** mit einer Zeile Befund,
dazu die RAM/Flash-Zahlen. Danach:

- Firmware-Änderung, alles grün → nächster Schritt ist `/rollout <version>`
  (dort wird gemergt, vor dem OTA).
- Keine Firmware-Änderung (Doku, Werkzeuge, CI) → Merge `--no-ff` nach `main`
  anbieten; Push nur nach Freigabe.
