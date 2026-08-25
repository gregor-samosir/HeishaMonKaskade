# Vorhaben: Nur noch der ESP32-Pfad

Übergabe für eine eigene Session. Ziel ist, den ESP8266-Pfad (D1 mini) aus
Code, Build-Konfiguration, CI und Dokumentation zu entfernen. Die Firmware auf
den beiden produktiven Boards ändert sich dabei **um kein Byte** — es geht um
Wartbarkeit, nicht um Funktion.

**Stand:** 2026-08-25, Planung. Ausgangsversion 3.14.2. Vorgesehene
Zielversion **3.15.0** (siehe Abschnitt 9).

---

## 1. Ziel und getroffene Entscheidungen

Die Entscheidung selbst ist nicht neu — sie steht seit dem 2026-08-24 in
[`Ablauf-Backup-Boards.md`](Ablauf-Backup-Boards.md) unter „Was damit
entfällt". Dieses Dokument setzt sie um und klärt die drei Punkte, die dort
offen blieben.

Frage | Entscheidung (2026-08-25) | Folge
:--- | :--- | :---
Wie weit geht das Aufräumen? | **Nur der ESP8266-Pfad.** 10 Envs werden 6. | `stage_test_esp32` und `heishamon_esp32_usb`/`_ota` **bleiben** — anders als in `Ablauf-Backup-Boards.md` vorgesehen
Was ersetzt den Prüfstand `d1_mini_test`? | **Ein Backup-Board, leihweise.** Kein drittes Board. | Abschnitt 4, mit einer sicherheitskritischen Reihenfolge
Sind die Backup-Boards schon eingerichtet? | **Noch nicht bzw. nur eines.** | Abschnitt 2 — das ist eine echte Vorbedingung

Der Test-Prefix `panasonic_heat_pump32` bleibt damit erhalten. Das ist die
Abweichung von `Ablauf-Backup-Boards.md`, und sie ist bewusst: Ein toter
MQTT-Port sperrt ein Board gegen den Broker, aber er ersetzt nicht die
Möglichkeit, mit **erreichbarem** Broker zu testen, ohne auf den produktiven
Topics zu sitzen. Genau das brauchte das Prüfstand-Protokoll vom 2026-08-21
([`Vorhaben-Notbetrieb-Weboberflaeche.md`](Vorhaben-Notbetrieb-Weboberflaeche.md),
Abschnitt „Das Protokoll vom 2026-08-21").

---

## 2. Vorbedingung: die Backup-Boards müssen stehen

**Vor Etappe 3 sind beide Backup-Boards nach
[`Ablauf-Backup-Boards.md`](Ablauf-Backup-Boards.md) einzurichten.**

Der Grund ist einfach: Heute ist die Rückfallebene der D1 mini plus das
Binärarchiv. Beides wird durch die Backup-Boards ersetzt. Wer den ESP8266-Pfad
entfernt, bevor die Boards liegen, hat für die Zwischenzeit **gar keinen**
Notanker außer „aus dem Git-Tag neu bauen" — und das setzt einen
funktionierenden Rechner mit PlatformIO voraus, also genau das, was im Ernstfall
nicht garantiert ist.

Nachgeprüft am 2026-08-25: `192.168.2.108` (D1 mini H1), `192.168.2.193`
(D1 mini H2) und `192.168.2.197` (Prüfstand) antworten nicht auf Ping. Die
produktiven `192.168.2.120` und `.122` laufen. Es hängt also derzeit **kein**
ESP8266 im Netz — das Risiko „alte Firmware schreibt auf produktive Topics"
ist nicht akut, aber die Boards sind eben auch kein einsatzbereiter Rückfall,
solange sie stromlos in der Schublade liegen.

Etappen 1 und 2 (Analyse, Doku-Vorbereitung) können vorher laufen.

---

## 3. Bestandsaufnahme — was tatsächlich betroffen ist

### 3.1 Quellcode: acht Weichen

Der Port von 3.0.0 hat sauber getrennt. Board-abhängiger Code steht
ausschließlich in diesen Blöcken:

Datei | Zeilen | Inhalt | Aktion
:--- | ---: | :--- | :---
[`src/HeishaMon.h`](src/HeishaMon.h) | 12–58 | Plattformschicht: Includes, `typedef`s, UART-Pins. `#else`-Zweig = 20 Zeilen | `#else`-Zweig löschen, `#if` auflösen
[`src/HeishaMon.cpp`](src/HeishaMon.cpp) | 137–139 | `ArduinoOTA.setPort(8266)` | ganzer Block weg
[`src/HeishaMon.cpp`](src/HeishaMon.cpp) | 296–298 | `delay(100)` für USB-CDC | `#if` weg, `delay` bleibt
[`src/HeishaMon.cpp`](src/HeishaMon.cpp) | 302–324 | `switchSerial()` — `#else` mit `swap()` auf gpio13/15 | `#else`-Zweig weg
[`src/HeishaMon.cpp`](src/HeishaMon.cpp) | 860–864 | `configTzTime` vs. `configTime` | `#if` weg
[`src/HeishaMon.cpp`](src/HeishaMon.cpp) | 932–934 | `MDNS.update()` | ganzer Block weg
[`src/webfunctions.cpp`](src/webfunctions.cpp) | 130–134 | `LittleFS.begin(true)` | `#if` weg
[`src/webfunctions.cpp`](src/webfunctions.cpp) | 235–242 | `setHostname` + `setSleep(false)` vs. `hostname()` | `#if` weg
[`src/webfunctions.cpp`](src/webfunctions.cpp) | 602–606 | `LittleFS.begin(true)`, zweite Stelle | `#if` weg — **Einrückung ist dort verrutscht**, beim Anfassen mitziehen

### 3.2 Reine Kommentar-Erwähnungen — mit Vorsicht behandeln

Elf Stellen nennen den ESP8266 in einer **Design-Begründung**, ohne
Codewirkung:

Datei | Zeile | Begründet
:--- | ---: | :---
[`src/HeishaMon.cpp`](src/HeishaMon.cpp) | 71 | `actual_data` als festes Array statt `String` (Heap-Fragmentierung)
[`src/HeishaMon.cpp`](src/HeishaMon.cpp) | 962 | `uint32_t` statt `unsigned long` in der Verbindungswacht
[`src/commands.cpp`](src/commands.cpp) | 13 | const-Tabellen liegen im RAM
[`src/decode.h`](src/decode.h) | 78 | dito, plus Zeigerkosten
[`src/notbetrieb.cpp`](src/notbetrieb.cpp) | 454 | kurzer Statusstring, weil die Seite alle 2 s fragt
[`src/sendwindow.h`](src/sendwindow.h) | 48 | 32-Bit-Annahme für `millis()`
[`src/webfunctions.cpp`](src/webfunctions.cpp) | 415 | Startseite im 30-s-Takt statt 2-s
[`src/webfunctions.cpp`](src/webfunctions.cpp) | 444–451 | `sendbuf` static, TCP-Blockbildung
[`src/webfunctions.cpp`](src/webfunctions.cpp) | 713 | Kurzstatus so kurz wie möglich
[`src/webfunctions.cpp`](src/webfunctions.cpp) | 914 | Statusroute bewusst ohne Anmeldung
[`src/webfunctions.cpp`](src/webfunctions.cpp) | 932 | keine zweite Statusroute

**Diese Entscheidungen bleiben alle sachlich richtig.** Sie werden durch den
Wegfall des ESP8266 nicht falsch, nur weniger zwingend. Die Kommentare sind
deshalb **umzuschreiben, nicht zu löschen**: Der Satzteil „…auf dem ESP8266"
verschwindet, die Begründung bleibt stehen. Wer sie ersatzlos streicht, nimmt
einem späteren Leser den Grund und lädt dazu ein, die Entscheidung
versehentlich rückgängig zu machen.

Ein Beispiel für die gewünschte Form:

> **vorher:** „Je kürzer die Antwort, desto weniger Arbeit für einen ESP8266,
> der nebenher die Wärmepumpe abfragt."
> **nachher:** „Je kürzer die Antwort, desto weniger Arbeit für ein Gerät, das
> nebenher die Wärmepumpe abfragt."

Zwei Stellen sind Sonderfälle:

* [`src/webfunctions.cpp:444-451`](src/webfunctions.cpp#L444) beschreibt einen
  **gemessenen Unterschied** zwischen den Cores (ESP32 ~20 ms je
  `sendContent()`, ESP8266 koalesziert). Das ist ein Messergebnis, kein
  Plattformzwang — der Absatz bleibt inhaltlich, er erklärt, warum gepuffert
  wird.
* [`src/sendwindow.h:48`](src/sendwindow.h#L48) nennt beide Plattformen als
  32-Bit — die Aussage gilt für den ESP32 unverändert weiter.

### 3.3 Build-Konfiguration

[`platformio.ini`](platformio.ini):

Zeilen | Inhalt | Aktion
---: | :--- | :---
7, 11, 16 | Kopfkommentar, Aufzählung der Bausteine | anpassen
39–41 | `[env]`: „Nur das, was auf ESP8266 UND ESP32 gleich ist" | Begründung neu fassen
53 | ArduinoJson bewusst 6.x — **Grund war der ESP8266-Footprint** | siehe Abschnitt 9
62–69 | Board-Basis-Kommentar, „beide binden ein" | anpassen
72–86 | `[esp8266_base]` | löschen
108–111 | Hostname-im-Env-Begründung: „ESP8266- und ESP32-Board derselben Stufe können parallel im Netz hängen" | **Begründung fällt weg, Regel bleibt** — jetzt wegen der Backup-Boards (`_h1b`/`_h2b`)
141–144 | `[stage_test_esp8266]` | löschen
173–193 | `[env:d1_mini_h1_ota]`, `[env:d1_mini_h2_ota]` | löschen
223–229 | `[env:d1_mini_usb]` | löschen
258–270 | `[env:d1_mini_test]` | löschen

[`platformio_user_env.ini`](platformio_user_env.ini) und
[`platformio_user_env_sample.ini`](platformio_user_env_sample.ini): je drei
Sektionen entfallen — `[usb_defaults]`, `[ota_defaults_h1]`,
`[ota_defaults_h2]` — plus die Kopfkommentare, die sie auflisten.

**Beide Dateien müssen sektionsgleich bleiben.** Die CI kopiert die
Sample-Datei (`main.yml:68`); fehlt dort eine Sektion, die `platformio.ini`
noch über `${…}` referenziert, bricht der Build sofort und laut. Kein stiller
Fehler, aber ein sicherer Rotlauf.

Die Env-Zahl fällt von **10 auf 6**:

Bleibt | Zweck
:--- | :---
`heishamon_esp32_h1_ota` | Stufe 1 an WP1, `default_envs`
`heishamon_esp32_h2_ota` | Stufe 2 an WP2
`heishamon_esp32_h1_usb` | Erstflash Stufe 1
`heishamon_esp32_h2_usb` | Erstflash Stufe 2
`heishamon_esp32_usb` | Prüfstand über USB, Test-Prefix
`heishamon_esp32_ota` | Prüfstand über mDNS

### 3.4 CI

[`.github/workflows/main.yml`](.github/workflows/main.yml) nennt den Zweck
**dreimal wörtlich**: „Ihr Zweck bleibt das Absichern der
ESP8266-Rückfallebene" (Z. 6), „Für eine CI, deren Zweck das Absichern der
ESP8266-Rückfallebene ist" (Z. 40), „Sonst bleibt die ESP8266-Rückfallebene
ungebaut" (Z. 132). Diese Begründung fällt vollständig weg und braucht eine
neue.

Vorschlag für die neue Begründung: Die CI sichert ab, dass **alle sechs Envs**
übersetzen und die Hosttests halten — insbesondere die Envs, die man beim
lokalen Bauen übergeht, weil man nur die eigene Stufe testet. Sie bleibt
Rückversicherung, nicht Erstprüfung.

Was **nicht** anzufassen ist: Der Schritt „Alle Envs bauen" (Z. 137–142) holt
die Liste dynamisch aus `platformio.ini`. Er wird von allein kürzer, ohne
zweite Liste, die zurückbleiben könnte.

Der Cache-Kommentar (Z. 30–49) nennt „ESP8266 + ESP32, 1163 MB" und rechnet mit
zehn Envs. Die Zahlen sind nach dem Umbau neu zu messen, nicht zu schätzen.

Laufzeit heute: **4:15 min** (Lauf vom 2026-08-25).

### 3.5 Werkzeuge

* [`piotools/obj-dump.py:7`](piotools/obj-dump.py#L7) ruft
  `xtensa-lx106-elf-objdump` — die **ESP8266**-Toolchain. Das Skript ist für
  ESP32 schon heute kaputt; es fällt nur nicht auf, weil einzig
  `name-firmware.py` in `extra_scripts` steht. Entweder auf
  `xtensa-esp32s3-elf-objdump` umstellen oder mit den übrigen Altlasten
  entfernen.
* `espupload.py`, `gzip-firmware.py`, `http-uploader.py`, `sftp-uploader.py`,
  `strip-floats.py` sind ungenutzter Altbestand aus dem Original-HeishaMon.
  Kein ESP8266-Zwang, aber beim Aufräumen die Gelegenheit.
* `-Wl,-Map,firmware.map` steht **nur** in `[esp8266_base]` (Z. 81). Mit dem
  Block verschwindet auch die 3,5-MB-Datei `firmware.map` im
  Wurzelverzeichnis. Sie ist gitignored — kein Verlust, aber es erklärt, warum
  sie nach dem Umbau nicht mehr auftaucht.

### 3.6 Dokumentation

Datei | Stellen | Umgang
:--- | :--- | :---
[`README.md`](README.md) | 5, 101, 479–492, 542, 559, 603, 608, 618–619, 627, 634, 641, 777 | **überarbeiten** — Z. 5 ist der zweite Satz der Einleitung, Z. 479 ein eigenes Kapitel „Eine Codebasis für ESP8266 und ESP32-S3 (3.0.0)", Z. 618–634 die Env-Tabellen
[`test/README.md`](test/README.md) | 54–58, 95, 992 | **überarbeiten** — das Kapitel „Prüfstand aufsetzen" beschreibt ein ESP8266-Board; Abschnitt 4 dieses Dokuments ersetzt es
[`MQTT-Topics.md`](MQTT-Topics.md) | 725, 759 | Design-Begründungen, wie 3.2 behandeln
[`Ablauf-Backup-Boards.md`](Ablauf-Backup-Boards.md) | 113–118 | **richtigstellen** — dort steht, der Test-Prefix entfalle mit; das gilt nach der Entscheidung aus Abschnitt 1 nicht mehr. Stattdessen ist der Ausleih-Ablauf aus Abschnitt 4 zu ergänzen
[`Analyse-Relais-statt-KNX.md`](Analyse-Relais-statt-KNX.md) | 238–245 | Abschnitt 6.3 („Die Rückfallebene D1 mini kann es nicht") ist mit diesem Vorhaben erledigt — als erledigt markieren, nicht löschen

**Nicht anfassen:**
[`Massnahmenplan-Codedurchsicht-2026-08-18.md`](Massnahmenplan-Codedurchsicht-2026-08-18.md),
[`Vorhaben-Notbetrieb-Weboberflaeche.md`](Vorhaben-Notbetrieb-Weboberflaeche.md)
und der Changelog in [`src/version.h`](src/version.h). Das sind **datierte
Nachweise**. Wer dort ESP8266-Erwähnungen „bereinigt", lässt Messungen von
damals unter einer Konfiguration stehen, die es zum Zeitpunkt der Messung nicht
gab. Die RAM/Flash-Deltas im Massnahmenplan (Z. 80, 215) sind erhobene Zahlen,
keine Aussagen über den heutigen Zustand.

### 3.7 Was vollständig unberührt bleibt

* **Alle Hosttests.** Null Treffer auf `ESP8266`/`ESP32` in `test/*.cpp`,
  `test/stubs/` und `decode_hosttest.sh`. Sie waren nie plattformabhängig und
  bleiben es nicht.
* **Die Python-Werkzeuge** in `test/`. Sie sprechen über MQTT und HTTP mit dem
  Gerät, nicht über die Plattform.
* [`.clangd`](.clangd) — entfernt `-mlongcalls`, `-mtext-section-literals`,
  `-free`, `-fipa-pta`. Das sind Xtensa-Flags, die auch die ESP32-Toolchain
  setzt.
* Die gesamte Anwendungslogik: `decode.cpp`, `commands.cpp`, `notbetrieb.*`,
  `telegram.h`, `verbindung.h`, `Topics.*`.

---

## 4. Der Prüfstand — leihweise auf einem Backup-Board

### 4.1 Der entscheidende Befund: die `config.json` schlägt das Build-Flag

`HEISHA_HOSTNAME` ist in [`src/HeishaMon.cpp:54`](src/HeishaMon.cpp#L54) nur
der **Startwert** des Puffers:

```cpp
char wifi_hostname[CONFIG_FIELD_LEN] = HEISHA_HOSTNAME;
```

[`loadConfigValue()`](src/webfunctions.cpp#L84) überschreibt ihn beim Start aus
der `config.json` — dasselbe gilt für `mqtt_port`, `mqtt_server` und die
Zugangsdaten. Für den geliehenen Prüfstand heißt das:

Größe | Herkunft | Beim Flashen mit Test-Firmware
:--- | :--- | :---
**MQTT-Prefix** | reines Build-Flag `HEISHA_MQTT_PREFIX` | **wechselt** auf `panasonic_heat_pump32`
**Hostname** | `config.json` schlägt Build-Flag | bleibt `HeishaMon32_h1b`
**MQTT-Port** | `config.json` | bleibt `1884`
**Broker, Zugangsdaten, WLAN** | `config.json` | bleiben

Die `config.json` liegt in LittleFS und überlebt sowohl OTA als auch den
USB-Flash — die Partitionen sind getrennt (`min_spiffs.csv`).

**Das ist günstig:** Ein frisch mit Test-Firmware bespieltes Backup-Board ist
doppelt gesperrt — falscher Prefix *und* toter Port. Es kann in diesem Zustand
nichts anrichten.

### 4.2 Ausleihe

1. Backup-Board vom Ablageort holen, mit Strom versorgen (ohne Wärmepumpe, wenn
   der Test keine braucht).
2. `pio run -e heishamon_esp32_usb -t upload` — Test-Prefix
   `panasonic_heat_pump32`.
3. Über `http://<IP>/settings` **MQTT-Port auf 1883** setzen. Erst jetzt
   erreicht der Prüfling den Broker — auf dem Test-Prefix, an den der
   Node-RED-Verteiler nichts sendet.
4. Testen.

Der Hostname bleibt dabei `HeishaMon32_h1b`. Das ist kein Fehler, sondern die
zweite Sicherung: Er ist zugleich die MQTT-Client-ID
([`HeishaMon.cpp:333`](src/HeishaMon.cpp#L333)), und er kollidiert mit keinem
produktiven Board.

### 4.3 Rückgabe — die Reihenfolge ist sicherheitskritisch

> **Erst den Port auf 1884 stellen, dann die Stufen-Firmware flashen.
> Nie umgekehrt.**

Der Grund: Nach Schritt 3 oben steht in der `config.json` `mqtt_port = 1883`.
Wird in diesem Zustand die Stufen-Firmware aufgespielt, wechselt der Prefix
zurück auf `panasonic_heat_pump` — und das Backup-Board sitzt mit
**produktivem Prefix auf dem echten Broker**. Es publiziert dann `state`- und
`LWT`-Topics und abonniert die `set`-Topics parallel zum laufenden Board, ohne
überhaupt an einer Wärmepumpe zu hängen. Der abweichende Hostname verhindert
nur den gegenseitigen Client-ID-Rauswurf, nicht das Mitschreiben.

Richtige Reihenfolge:

1. Über `/settings` des Prüflings **`mqtt_port` auf 1884** setzen. Das Gerät
   startet neu und erreicht den Broker nicht mehr.
2. Erst danach `pio run -e heishamon_esp32_h1_usb -t upload` (bzw. `_h2_usb`).
3. Gegenprobe: `http://<IP>/settings` öffnen und Port **1884** sowie Hostname
   `HeishaMon32_h1b` ablesen, bevor das Board zurück in die Schublade geht.
4. Stromlos ablegen.

Schritt 3 ist kein Formalismus. Es ist die einzige Stelle, an der ein Fehler
in Schritt 1 noch auffällt.

### 4.4 Was die Ausleihe kostet

Solange ein Backup-Board Prüfstand ist, hat die betreffende Stufe **keinen
Notanker**. Das ist die bewusst in Kauf genommene Folge der Entscheidung gegen
ein drittes Board. Zwei Konsequenzen für die Praxis:

* Immer nur **ein** Board gleichzeitig ausleihen — die andere Stufe bleibt
  abgesichert.
* Vor der Ausleihe kurz prüfen, ob an der eigenen Stufe gerade etwas ansteht
  (Rollout, Heizperiode, Abwesenheit). Ein Prüfstand ist selten dringend.

---

## 5. Etappen

Etappe | Inhalt | Nachweis
---: | :--- | :---
**0** | Rettungsanker: Tag `rettungsanker-vor-esp32-only-2026-…` auf `main`, Arbeit auf Branch `esp32-only` | `git tag`, `git branch`
**1** | Bestandsaufnahme gegenprüfen: `pio project config --json-output` als Vorstand sichern | Datei im Scratchpad
**2** | **Vorbedingung:** beide Backup-Boards nach `Ablauf-Backup-Boards.md` einrichten | Abschnitt 2
**3** | `platformio.ini`: `[esp8266_base]`, `[stage_test_esp8266]`, vier `d1_mini_*`-Envs raus; Kommentare nachziehen | `pio project config` gegen Vorstand diffen — die sechs ESP32-Envs müssen **unverändert** sein
**4** | `platformio_user_env.ini` **und** Sample: drei Sektionen raus, sektionsgleich halten | `pio run` für alle sechs Envs
**5** | Quellcode: acht Weichen auflösen (3.1), elf Kommentare umschreiben (3.2) | `pio run` für alle sechs Envs; **Größenvergleich** gegen den Stand vor Etappe 5
**6** | CI: neue Begründung, Cache-Kommentar mit gemessenen Zahlen | grüner Lauf, Laufzeit notieren
**7** | `piotools`: `obj-dump.py` umstellen oder entfernen, Altlasten aufräumen | —
**8** | Doku nach 3.6 | —
**9** | Changelog `src/version.h`, Version 3.15.0, Release | —

**Etappe 5 ist die einzige, bei der ein Fehler die Firmware verändern kann.**
Der ESP8266-Code steht in `#else`-Zweigen, die der Präprozessor beim
ESP32-Build ohnehin verwirft. Das Ergebnis **muss** deshalb nach dem Umbau
dasselbe sein wie davor. Ist es das nicht, wurde beim Auflösen der Weichen ein
aktiver Zweig getroffen.

**Ein `md5`-Vergleich der `.bin` taugt dafür nicht.** Der ESP32-App-Descriptor
trägt Compile-Datum und -Uhrzeit im Abbild (am 2026-08-25 nachgeprüft: schon
`heishamon_esp32_h1_ota.bin` und `_h1_usb.bin` mit identischen Build-Flags
haben verschiedene Prüfsummen). Zwei Builds sind hier nie byte-gleich.

Der tragfähige Nachweis ist der **Größenvergleich über die `.elf`**:

```bash
SZ=~/.platformio/packages/toolchain-xtensa-esp-elf/bin/xtensa-esp32s3-elf-size
$SZ .pioenvs/heishamon_esp32_h1_ota/firmware.elf   # vor Etappe 5, wegschreiben
# ... Umbau ...
$SZ .pioenvs/heishamon_esp32_h1_ota/firmware.elf   # muss Zeile für Zeile gleich sein
```

`text`, `data` und `bss` müssen exakt übereinstimmen. Der Maßstab ist
empfindlich genug: Zwischen `h1` und `h2` unterscheidet sich `text` allein
wegen der beiden verschieden langen Prefix-Strings um 332 Byte
(1018419 / 1018087, gemessen 2026-08-25).

Ausgangswerte für Stufe 1, Stand 3.14.2:

```text
   text	   data	    bss	    dec	    hex
1018419	 221386	2192879	3432684	 3460ec
```

Wer es schärfer will, legt zusätzlich `xtensa-esp32s3-elf-nm --size-sort` vor
und nach dem Umbau nebeneinander — das zeigt nicht nur *dass*, sondern *welche*
Funktion sich geändert hat. Der Versionsstring bleibt für diesen Vergleich
unverändert; erst wenn die Zahlen stimmen, wird auf 3.15.0 gezogen.

---

## 6. Risiken

**1. Kein Rückfall in der Zwischenzeit.** Behandelt in Abschnitt 2 — Etappe 2
ist die Antwort darauf.

**2. Der Prüfstand geht verloren, wenn Abschnitt 4 nicht in die Doku kommt.**
`d1_mini_test` war ein Board, das man einfach anstecken konnte. Der geliehene
Prüfstand ist ein Ablauf mit vier Schritten und einer sicherheitskritischen
Reihenfolge. Wird er nicht sauber aufgeschrieben, wird er beim nächsten Mal
nicht benutzt — und dann wird an der laufenden Anlage getestet.

**3. `#include "Ticker.h"` mit Anführungszeichen.** Beim Zusammenlegen der
beiden Include-Blöcke in [`HeishaMon.h`](src/HeishaMon.h) muss die Schreibweise
mit Anführungszeichen erhalten bleiben. Sie zieht `sstaub/Ticker` aus
`lib_deps`. Ein `<Ticker.h>` fände die Core-Ticker des ESP32 mit anderer API
(`attach_ms` statt Konstruktor-Argumenten) — das bräche die Timing-Kette, und
zwar erst zur Laufzeit. Zwei Zeilen werden zu einer; genau diese eine ist die
Falle.

**4. Die Sample-Datei ist Teil der CI.** Siehe 3.3. Beide ini-Dateien in
derselben Etappe anfassen.

**5. Kommentare, die ihre Begründung verlieren.** Siehe 3.2. Der Fehler wäre
nicht das Löschen des Wortes „ESP8266", sondern das Löschen des Satzes.

**6. Rückwirkende Doku-Bereinigung.** Siehe 3.6. Die datierten Nachweise
bleiben, wie sie sind.

---

## 7. Gewinn — und was ausdrücklich keiner ist

### Gewinn

* **Bei jeder Änderung spürbar.** Die Projektkonvention verlangt, vor dem Merge
  alle Envs lokal zu bauen. Sechs statt zehn — das trifft öfter als die CI.
* **CI und Cache.** 10 Envs werden 6; der Cache verliert den kompletten
  espressif8266-Anteil. Ausgangswert für den Vergleich: 4:15 min, 1163 MB.
* **Ein Vorhaben wird freigeschaltet.**
  [`Analyse-Relais-statt-KNX.md`](Analyse-Relais-statt-KNX.md) Abschnitt 6.3:
  Solange der D1 mini Rückfallebene ist, kann die KNX-Ablösung über die
  Board-Relais nicht kommen — ein ESP8266 hat die Relais nicht. Mit diesem
  Vorhaben fällt der Blocker.
* **Der Code wird flach.** Acht `#if`-Blöcke und ein 20-Zeilen-`#else` weniger.
  Beim Lesen entfällt jedes Mal die Frage, welcher Zweig gerade gilt.
* **Ein Kapitel Designzwang entfällt.** const-Tabellen im RAM, PROGMEM-Pflicht,
  `setBufferSize(512)` als zu teuer verworfen, Statusroute ohne Anmeldung,
  30-s- statt 2-s-Refresh. Der Gewinn ist die **Freiheit**, diese
  Entscheidungen zu überdenken — nicht die Pflicht dazu. Sie sind alle
  weiterhin vertretbar.
* **Weniger Doppelpflege.** Drei Sektionen weniger in zwei Dateien, die
  sektionsgleich zu halten sind.
* **Ein stiller Defekt fällt auf.** `piotools/obj-dump.py` ruft seit dem Port
  eine Toolchain, die für dieses Projekt nicht mehr installiert wird.

### Kein Gewinn

* **Die Firmware wird kein Byte kleiner.** Der ESP8266-Code steht in
  `#else`-Zweigen, die beim ESP32-Build ohnehin nicht übersetzt werden. Wer
  eine Größenersparnis erwartet, wird enttäuscht — und genau das ist der Grund
  für den Größenvergleich in Etappe 5.
* **Kein Laufzeitgewinn am Gerät.** Kein Byte weniger heißt auch: keine
  Schleife schneller.
* **Die Hosttests ändern sich nicht.** Sie waren nie plattformabhängig.
* **Kein RAM-Gewinn.** Aktuelle Flash-Belegung: 1,21 MB von 1,9 MB
  OTA-Slot, rund 690 KB frei. Diese Reserve gibt es heute schon.

---

## 8. Abnahme

1. `pio project config --json-output` gegen den in Etappe 1 gesicherten
   Vorstand: die sechs ESP32-Envs unverändert, vier `d1_mini_*` weg.
2. Größenvergleich nach Etappe 5 (siehe dort) — `text`/`data`/`bss` gleich.
   **Nicht** über `md5` der `.bin` — die trägt Compile-Datum und -Uhrzeit.
3. Alle Hosttests lokal, vollständig, **vor** dem Push. Die Lehre aus 3.14.1
   steht im Changelog: Der CSS-Test lief vor dem Rollout von 3.14.0 nicht mit,
   und die Firmware war da schon auf beiden Stufen.
4. CI grün, Laufzeit notieren.
5. Kein OTA an H1/H2 nötig — die Firmware ist unverändert. Wenn 3.15.0
   trotzdem ausgerollt wird, gilt das übliche Verfahren: `test/tablesnap.py`
   vorher und nachher, zeilenweise vergleichen.

---

## 9. Offene Punkte

**Versionsnummer: 3.15.0 oder 4.0.0?** Vorschlag ist **3.15.0**. Begründung:
Die ausgelieferte Firmware verhält sich identisch, es fällt keine Funktion und
kein Topic weg. Für 4.0.0 spräche, dass eine Zielplattform verschwindet — das
ist aber eine Aussage über das Repo, nicht über das Gerät. Entscheidung liegt
beim Owner.

**ArduinoJson 6.x → 7?** Die Festlegung auf 6.x in
[`platformio.ini:53`](platformio.ini#L53) hat als einzige Begründung den
ESP8266-Footprint. Sie wird mit diesem Vorhaben gegenstandslos. **Gehört
trotzdem nicht in dieses Vorhaben:** ArduinoJson 7 ändert die API
(`DynamicJsonDocument` ist entfallen), betrifft `webfunctions.cpp` an mehreren
Stellen und hat ein eigenes Risiko. Vorschlag: Der Kommentar wird in Etappe 3
auf „bewusst 6.x, Umstieg auf 7 nicht geprüft" geändert, mehr nicht.

**`Ablauf-Backup-Boards.md` Abschnitt „Was damit entfällt"** widerspricht nach
der Entscheidung aus Abschnitt 1 der Realität (Test-Prefix bleibt). Etappe 8
stellt das richtig — der Punkt steht hier, damit er nicht untergeht.

**Die drei stromlosen D1 minis.** `192.168.2.108`, `.193` und `.197` liegen
physisch noch vor. Sie sind nach diesem Vorhaben nicht mehr flashbar (die Envs
sind weg) und tragen Firmwarestände, die auf den produktiven Prefix schreiben
würden. Entscheidung offen: entsorgen, oder mit einem Aufkleber „nicht
anschließen" ablegen. Bis dahin gilt: stromlos.
