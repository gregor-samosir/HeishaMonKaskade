# Hinweise für Claude Code

Firmware für zwei kaskadierte Panasonic-Wärmepumpen, die im Dauerbetrieb an
einer echten Anlage hängen. Dieses Repo ist **ausführlich dokumentiert** — die
Fakten stehen in `README.md`, `src/version.h` und den Fachdateien, nicht hier.
Diese Datei ist nur **Landkarte und Arbeitsregel**: wo etwas steht, was bereits
entschieden ist und was nicht angefasst wird. Sie führt bewusst keine Zahlen,
Topics, Versionsnummern oder Adressen — doppelte Fakten veralten getrennt.

## Bevor du einen Befund meldest

**Vieles, was hier wie ein Fehler aussieht, ist geprüft und begründet
entschieden.** Ein Befund, der einen dieser Punkte erneut aufmacht, ist selbst
der Fehler. Zuerst nachsehen in:

- [`Massnahmenplan-Codedurchsicht-2026-08-18.md`](Massnahmenplan-Codedurchsicht-2026-08-18.md)
  und [`Massnahmenplan-Codedurchsicht-2026-09-02.md`](Massnahmenplan-Codedurchsicht-2026-09-02.md)
  — beide Durchsichten mit Entscheid je Punkt, auch für die verworfenen.
- `README.md`, Abschnitt **„Was bewusst nicht drin ist"** — Zone 2, die Relais
  der Platine, keine Unity-Testsuite, der zurückgestellte große Umbau.
- [`src/version.h`](src/version.h) — Changelog mit Problem und Nachweis je
  Version. Wer wissen will, warum etwas so ist, findet es meistens dort.

**Und prüfe die Implementierung, statt ein bekanntes Muster zu unterstellen.**
K3 der Durchsicht 2026-09-02 ist das Lehrstück: „`open("w")` trunkiert, ein
Stromausfall hinterlässt eine halbe Datei" stimmt für SPIFFS und FAT — unter
diesem Projekt liegt littlefs, das erst im `close()` committet. Der Punkt stand
zweimal mit derselben falschen Begründung im Plan, bevor jemand `lfs.c`
aufgeschlagen hat.

## Wo was steht

`README.md` ist lang; steuere es über seine `##`-Überschriften an, besonders
**„Aufbau"** — dort steht die vollständige Tabelle Datei → Inhalt.

| Frage | Datei |
| --- | --- |
| Was ist das, und warum so? | `README.md` |
| Welche Topics gibt es? | `MQTT-Topics.md` (englisch, Upstream-kompatibel) |
| Liest ein Set-Kommando zurück, und über welches Topic? | `SET-TOP-Zuordnung.md` |
| Was bedeutet Byte *n*? | `ProtocolByteDecrypt.md`, ergänzend `doku-intern/` |
| Was wurde wann warum geändert? | `src/version.h` |
| Wie lief ein abgeschlossenes Vorhaben aus? | `Vorhaben-*.md`, `Auftrag-*.md` — Ergebnis steht jeweils im Kopf |
| Was passiert im Notbetrieb, Schritt für Schritt? | `Ablauf-Notbetrieb.md` |
| Wie werden die Ersatzplatinen gepflegt? | `Ablauf-Backup-Boards.md` |
| Welches Werkzeug gibt es für welchen Nachweis? | `test/README.md` |

## Befehle

Gebaut wird mit PlatformIO. `platformio_user_env.ini` fehlt absichtlich in git;
für einen reinen Build genügt `cp platformio_user_env_sample.ini platformio_user_env.ini`.
In einer nicht-interaktiven Shell liegt `pio` nicht im PATH — dann
`~/.platformio/penv/bin/pio` benutzen.

```bash
pio run -e heishamon_esp32_h1_ota          # Standard-Env (Stufe 1, OTA)
pio run $(pio project config --json-output \
  | python3 -c "import json,sys; print(' '.join('-e '+s[0][4:] for s in json.load(sys.stdin) if s[0].startswith('env:')))")
```

Die zweite Zeile baut **alle** Envs, ohne eine zweite Liste zu pflegen. Die
maßgebliche Liste der Hosttests samt Begründung, warum jeder einzelne existiert,
steht im Schritt „Hosttests" in
[`.github/workflows/main.yml`](.github/workflows/main.yml) — von dort
übernehmen, nicht neu erfinden. Nach dem Flashen wird mit `test/tablesnap.py`
gegen den Stand davor abgenommen.

## Harte Regeln

1. **`doku-intern/` wird nicht veröffentlicht.** Panasonic-Servicedokumente,
   urheberrechtlich geschützt. Darin nachschlagen ja, daraus zitieren nein —
   auch nicht auszugsweise, auch nicht sinngemäß in einer Commit-Message.
2. **`platformio_user_env.ini` gehört nicht in git.** Dort stehen Ports, IPs
   sowie OTA- und AP-Passwort.
3. **Keine gebauten Binaries ins Repo**, auch nicht an ein Release: AP- und
   Notbetriebspasswort stehen im Abbild im Klartext.
4. **Vor Codeänderungen den Stand einfrieren:** Tag `rettungsanker-JJJJ-MM-TT`
   auf den aktuellen Commit, dann auf einem Branch arbeiten. Doku-Ergänzungen
   laufen direkt auf `main`.
5. **Die Firmware hängt an einer laufenden Heizung.** Kein Zugriff auf die
   produktiven Geräte ohne ausdrücklichen Auftrag; Testläufe gehören auf den
   Prüfling, und Eingriffe an der Anlage werden einzeln aufgerufen, nie
   gebündelt.
6. **Vor dem Merge alle Envs bauen und alle Hosttests laufen lassen.** Die CI
   läuft nur auf `main` und ist Rückversicherung, nicht Erstprüfung.

## Konventionen beim Ändern

- **Versionsnummer und Changelog in `src/version.h` mitpflegen** — Problem,
  Nachweis und RAM/Flash-Delta, nicht nur das Was. Der Changelog ist hier die
  eigentliche Entwicklungsgeschichte.
- **Prüfbare Regeln kommen in arduino-freie Header** (`notbetrieb.h`,
  `verbindung.h`, `sendwindow.h`, `rtcspiegel.h`, `telegram.h`), die Firmware
  und Hosttest gemeinsam einbinden. Alles mit Zeitbezug muss den
  `millis()`-Überlauf nach 49,7 Tagen aushalten — an der Anlage wäre er nicht
  abzuwarten.
- **Inline-Kommentare je Abschnitt**, und zwar zum Warum. Race Conditions und
  Timing-Fragen werden ausgeschrieben, nicht stillschweigend gelöst.
- **Bounds-Checking bei jeder Rechnung**; Werte, die nicht negativ werden
  können, auf 0 klemmen.
- **Sprache:** Quelltext-Kommentare und Serial-Ausgaben ohne Umlaute (ASCII);
  Texte der Weboberfläche, alle Markdown-Dateien und Commit-Messages mit
  Umlauten. Dokumentation deutsch, `MQTT-Topics.md` englisch.
