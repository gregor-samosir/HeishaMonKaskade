---
name: rollout
description: Rollt eine geprüfte Firmware-Version der HeishaMon-Kaskade aus — Merge und Tag, OTA und Abnahme an H1 und H2, Backup-Boards nachziehen, Push, CI, öffentliches und privates Release. Greift auf die laufende Anlage zu und hält vor jedem Eingriff an.
argument-hint: <version, z. B. 3.23.0>
disable-model-invocation: true
---

# Rollout einer Firmware-Version

Version: **$ARGUMENTS**

Dieser Skill führt durch `Ablauf-Backup-Boards.md`, „Bei jeder Firmware-Änderung",
Schritte 4–8. **Warum** ein Schritt so ist, steht dort und in `test/README.md` —
hier stehen Reihenfolge, Befehle und Haltepunkte. Weicht dieser Skill von der
Ablaufdatei ab, gilt die Ablaufdatei; die Abweichung melden.

Die Geräte, IPs, Prefixe und Hostnamen stehen in `Ablauf-Backup-Boards.md`,
Tabelle „Die vier Boards". Vor dem Start von dort lesen, nicht aus dem Gedächtnis.

## Regeln für den ganzen Lauf

- **HALT** heißt: ansagen, was als Nächstes passiert, und auf das ausdrückliche
  „ok" des Owners warten. Kein Weiter ohne Antwort.
- **Jeder zustandsverändernde Gerätezugriff steht allein in einem Bash-Aufruf** —
  OTA-Upload, `/settings` mit Parametern, `/reboot`, jeder POST. Vorbereitung,
  Warten und Auswerten in eigene Aufrufe. Sonst greift ein Abbruch des Owners
  nicht mehr (am 2026-08-27 löste genau das einen ungewollten Notbetriebslauf aus).
- **Lesende Abfragen dürfen gebündelt werden.**
- **Wartezeiten ansagen** (Boot rund eine Minute, CI rund 3½ Minuten). In
  Warteschleifen nur billige Prüfungen (ein `curl`), nie ein ganzes Werkzeug.
- **Passwörter nie ausgeben.** Das OTA-Passwort (zugleich Login für
  `/settings`) liest der Aufruf selbst aus der ini:
  `PW=$(sed -n 's/.*--auth=//p' platformio_user_env.ini | head -1 | tr -d "\"' ")`
- **Scheitert eine Abnahme: anhalten**, nicht mit der nächsten Stufe weitermachen.
  Befund vorlegen, der Owner entscheidet über den Rückweg (siehe unten).

## 0. Voraussetzungen

- `/pruefen-vor-merge` ist in dieser Sitzung für den Branch grün durchgelaufen —
  sonst zuerst ausführen.
- `heishamon_version` in `src/version.h` ist gleich **$ARGUMENTS**.
- Die vorige Version (`VORHER`) ist die, die an H1 läuft:
  `curl -s -m 5 http://<IP H1>/ | grep -o -E 'Version: [0-9]+\.[0-9]+\.[0-9]+'`
  (mit „Version:" davor — ohne den Anker trifft das Muster auch `192.168.2` einer IP).
  Dieselbe Abfrage dient unten als Nachweis nach jedem OTA.
- Neue Pflichtfelder in `/settings`? Im Changelog der Version nachsehen und
  vorab nennen (Beispiel 3.22.0: `knx_schnittstelle` an allen vier Boards).

## 1. Merge und Tag — lokal, vor dem OTA

```bash
BRANCH=$(git branch --show-current)
git checkout main && git merge --no-ff "$BRANCH" -m "Merge $BRANCH: $ARGUMENTS – <Kurztitel>"
git tag -a "v$ARGUMENTS" -m "$ARGUMENTS – <Kurztitel>"
git cat-file -t "v$ARGUMENTS"          # muss "tag" sein
```

Dann **von `main` aus** die beiden produktiven Envs bauen, Exit-Code von `pio`
direkt auswerten (nicht durch eine Pipe), und prüfen, dass die Abbilder die
Version tragen:

```bash
~/.platformio/penv/bin/pio run -e heishamon_esp32_h1_ota -e heishamon_esp32_h2_ota > "$TMPDIR/pio_rollout.log" 2>&1; echo "pio-Exit: $?"
for e in h1 h2; do strings build_output/firmware/heishamon_esp32_${e}_ota.bin | grep -c -x "$ARGUMENTS"; done
```

**Noch nicht pushen.** Scheitert eine Abnahme, ist so nichts öffentlich.

## 2. Baseline beider Stufen

```bash
python3 test/tablesnap.py <IP H1> > "$TMPDIR/h1_vorher.txt"
python3 test/tablesnap.py <IP H2> > "$TMPDIR/h2_vorher.txt"
```

Unmittelbar vor dem OTA ziehen. Zeilenzahl beider Dateien nennen.

## 3. Stufe 1 (H1)

1. **HALT:** „OTA $ARGUMENTS auf H1 (<IP>)."
2. OTA, allein im Aufruf:
   `~/.platformio/penv/bin/pio run -e heishamon_esp32_h1_ota -t upload`
3. Warten, bis die Startseite die neue Version zeigt — erst einmal direkt
   nachsehen, dann höchstens alle 5 s, mit sichtbarem Zwischenstand, Obergrenze
   3 Minuten.
4. Die Tabelle braucht einige Abfragezyklen: erst schnappen, wenn keine Zeile
   mehr leer oder `unused` ist.
   `python3 test/tablesnap.py <IP H1> > "$TMPDIR/h1_nachher.txt"`
5. **Abnahme:**
   - `diff "$TMPDIR/h1_vorher.txt" "$TMPDIR/h1_nachher.txt"` — Aufbau (TOP-Nummer
     und Name) muss gleich sein, außer gewollt neuen Topics dieser Version.
   - Abweichende Werte einzeln einordnen: laufende Messwerte sind normal. **Sollwerte
     dürfen sich nicht bewegen** — ausdrücklich ansehen: `Quiet_Mode_Level`,
     `Z1_Heat_Request_Temp`, `Z1_Heat_Curve_Target_High_Temp`,
     `Z1_Heat_Curve_Target_Low_Temp`.
   - Die Web-Tabelle immer per `curl`/`tablesnap.py` vergleichen, nie im Browser
     (Safari cached `/tablerefresh`).
6. **MQTT-Seite:** kommt die Version im ioBroker an?
   `curl -s "http://192.168.2.147:8087/get/mqtt.0.<Prefix H1>.info.version"` —
   `val` gleich $ARGUMENTS, `ts` (Millisekunden) **nach** dem OTA-Zeitpunkt.
   Das Topic wird nur beim Verbinden gesendet; ein alter `ts` heißt, die neue
   Firmware hat den Broker noch nicht erreicht.
7. **Notbetrieb bereit:** `curl -s http://<IP H1>/notbetrieb/status` (ohne Login).
   Format `Zustand;Schritt;Schritte;fehlendMaske;Sperre;Lage;…` — bereit heißt
   Feld 4 `fehlendMaske` = 0, Feld 5 `Sperre` = 0, Feld 6 `Lage` = 0. Kurz nach
   dem Boot können Werte noch fehlen, weil der Broker sie erst nachliefert:
   einmal nach einer Minute wiederholen, bevor es ein Befund ist.
8. **Pflichtfelder:** fehlt ein neues Pflichtfeld in `/settings`, **HALT**, dann
   allein im Aufruf setzen (`curl -s -u "admin:$PW" "http://<IP>/settings?<feld>=<wert>"`
   — startet das Gerät neu) und danach zurücklesen.

Ergebnis von H1 in drei Zeilen zusammenfassen.

## 4. Stufe 2 (H2)

Wie Schritt 3 mit `heishamon_esp32_h2_ota`, IP und Prefix von H2, eigenem
**HALT** vor dem OTA.

## 5. Backup-Boards nachziehen

1. **HALT:** Der Owner steckt `h1b` und `h2b` an. Erst weiter, wenn er es bestätigt.
2. Erreichbarkeit und Stilllegung **vor** dem Flash (lesend, gebündelt):
   Startseite und `/settings` beider Backups — `mqtt_port` muss **1884** sein.
   Steht dort 1883: **anhalten**, das Board sitzt sonst mit Stufen-Firmware auf
   den produktiven Topics.
3. OTA je Board, **jeweils allein im Aufruf**, über die reservierte IP (die Envs
   zielen auf die produktiven Boards):
   `~/.platformio/penv/bin/pio run -e heishamon_esp32_h1_ota -t upload --upload-port <IP h1b>`
   `~/.platformio/penv/bin/pio run -e heishamon_esp32_h2_ota -t upload --upload-port <IP h2b>`
4. Gegenprobe je Board: Version und Stufenname auf der Startseite; in `/settings`
   `mqtt_port = 1884`, Hostname `…_h1b` bzw. `…_h2b`, Pflichtfelder gesetzt.
   Ein Backup ohne Broker meldet auf `/notbetrieb/status` fehlende Werte und
   Sperre — das ist der stillgelegte Zustand, kein Befund.
5. Owner lagert beide Boards stromlos ein.

## 6. Push und CI

1. **HALT:** „Push von `main` und `v$ARGUMENTS`."
2. Einzeln pushen — **nie `git push --follow-tags`** (die Rettungsanker bleiben lokal):
   ```bash
   git push origin main
   git push origin "v$ARGUMENTS"
   ```
3. CI beobachten. `gh` ist in nicht-interaktiven Shells nicht angemeldet; das
   Token je Aufruf aus `~/.zshrc` holen, **nie ausgeben**. `gh run list` kennt
   kein `--commit`; nach `workflowName == "CI"` und `headSha` filtern (je Push
   laufen `CI` und `CodeQL`):
   ```bash
   export GITHUB_TOKEN=$(sed -n 's/^export GITHUB_TOKEN=//p' ~/.zshrc | tr -d "\"'" | head -1)
   SHA=$(git rev-parse HEAD)
   gh run list --repo gregor-samosir/HeishaMonKaskade --branch main --limit 6 \
     --json databaseId,workflowName,headSha,status \
     | python3 -c "import json,sys; [print(r['databaseId'],r['status']) for r in json.load(sys.stdin) if r['workflowName']=='CI' and r['headSha']=='$SHA']"
   ```
   Den Lauf mit `gh run watch <id> --exit-status` **im Hintergrund** verfolgen
   und währenddessen die Releases vorbereiten.

## 7. Releases

Beide erst anlegen, wenn die CI grün ist.

**Öffentlich** (`gregor-samosir/HeishaMonKaskade`) — **ohne Binaries**, beide
Passwörter stehen lesbar im Abbild. Aufbau wie die Vorgänger
(`gh release view v<VORHER> --json body`): kurzer Einstieg, `## Warum`,
`## Was drin ist`, Nachweise. Nichts aus `doku-intern/`.

1. Text nach `$TMPDIR/release_$ARGUMENTS.md` schreiben und dem Owner zeigen.
2. **HALT**, dann:
   `gh release create "v$ARGUMENTS" --repo gregor-samosir/HeishaMonKaskade --title "$ARGUMENTS — <Kurztitel>" --notes-file "$TMPDIR/release_$ARGUMENTS.md"`

**Privat** (`gregor-samosir/HeishaMon-Rollback`) — die Rückfallebene. Ein `cp`
in den lokalen Ordner ist **keine** Sicherung.

```bash
R="$TMPDIR/rollback_$ARGUMENTS"; mkdir -p "$R"
cp build_output/firmware/heishamon_esp32_h1_ota.bin "$R/heishamon_esp32_h1_ota_v$ARGUMENTS.bin"
cp build_output/firmware/heishamon_esp32_h2_ota.bin "$R/heishamon_esp32_h2_ota_v$ARGUMENTS.bin"
cp platformio_user_env.ini "$R/platformio_user_env_v$ARGUMENTS.ini"
gh release list --repo gregor-samosir/HeishaMon-Rollback --limit 5
```

**HALT** mit der Liste: neues Release anlegen, `v<VORHER>` umbenennen, das
Release **davor löschen** (endgültig; der Tag bleibt). Dann einzeln:

```bash
gh release create "v$ARGUMENTS" "$R"/* --repo gregor-samosir/HeishaMon-Rollback --title "v$ARGUMENTS — im Einsatz auf beiden Stufen" --notes "<Merge-Commit, Datum, Stufen>"
gh release edit "v<VORHER>" --repo gregor-samosir/HeishaMon-Rollback --title "v<VORHER> — Rückfallstand vor $ARGUMENTS"
gh release delete "v<DAVOR>" --repo gregor-samosir/HeishaMon-Rollback --yes
gh release view "v$ARGUMENTS" --repo gregor-samosir/HeishaMon-Rollback --json assets --jq '.assets[].name'
```

Die letzte Zeile muss drei Assets zeigen, danach `rm -rf "$R"`.

## 8. Nachtrag

- Erledigungsvermerk im Kopf des zugehörigen `Vorhaben-*.md`/`Arbeitsplan-*.md`,
  falls es eines gibt; auf `main` committen und pushen.
- `projektstand-heishamon.md` in der Memory **überschreiben, nicht
  ergänzen**: nur der aktuelle Zustand (Version auf allen vier Boards,
  Pflicht-Konfiguration, offene Folgethemen). Merge, Tag, Rettungsanker,
  CI-Lauf und Abnahmebefund stehen bereits im Changelog von `src/version.h`
  und brauchen keine zweite Ablage.
- Abschluss an den Owner: je Board eine Zeile (Version, Abnahme, Besonderheit),
  dazu CI und beide Releases.

## Rückweg, falls eine Abnahme scheitert

Nur auf Entscheid des Owners, jeder Schritt einzeln:

- **Noch nicht gepusht** (Schritte 3–5): Das Gerät mit der Vorversion flashen —
  in einem Worktree vom Tag `v<VORHER>` bauen (`platformio_user_env.ini`
  hineinkopieren) und per OTA hochladen. Lokal danach `git tag -d "v$ARGUMENTS"`
  und `git reset --hard "v$ARGUMENTS^1"` (erster Elter des Merges = `main` davor).
- **Schon gepusht:** Nichts an der Historie umschreiben. Vorversion wie oben
  flashen und den Befund als neue Version beheben.
- Das Abbild der Vorversion liegt zusätzlich am privaten Release `v<VORHER>`.
