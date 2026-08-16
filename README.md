# HeishaMonKaskade

Eine Variante von [HeishaMon](https://github.com/Egyras/HeishaMon) für den
**Kaskadenbetrieb zweier Panasonic-Wärmepumpen** an einer übergeordneten
Steuerung (Node-RED/ioBroker). Läuft auf ESP8266 (D1 mini) und auf dem
offiziellen HeishaMon-ESP32-S3-Board — aus einer Codebasis.

---

## In English — what this is and whether it is for you

This is a fork of HeishaMon, reworked for a **cascade of two heat pumps driven
by an external controller** that sends several set commands at once, repeatedly,
forever. That use case exposed problems the original firmware does not show in
single-unit, hand-operated setups. The most important one:

> **Set topics that share a protocol byte silently overwrote each other.**
> Upstream assigns the whole byte; every other field in it falls back to
> `0 = no change`. Two set commands inside the same 500 ms collection window
> therefore cancelled each other — while the log still cheerfully confirmed both.
> This fork merges each value through a bit mask (`src/commands.cpp`).
> Measured proof: [`test/README.md`](test/README.md).

Other notable changes: range validation for every set command (with limits
*measured on the machine*, not copied from folklore), one data-driven table per
concern instead of four parallel arrays, no heap allocation in the decode path,
per-unit configuration via build flags, a CDN-free authenticated web UI, and a
set of diagnostic tools under `test/`.

**This is not a drop-in replacement.** It is tailored to one specific
installation (two units, no zone 2, a Node-RED cascade controller). Topic
numbering has deliberate gaps, zone 2 is gone, and the defaults are ours. Take
the ideas and the measurements, not the binary. Everything is documented — the
changelog in [`src/version.h`](src/version.h) explains not just *what* changed
but *why*, and what was measured to confirm it.

Documentation is in German from here on. The MQTT reference
[`MQTT-Topics.md`](MQTT-Topics.md) is in English.

---

## Worum es geht

Zwei Panasonic-Wärmepumpen laufen als Kaskade — WH-MDC05H3E5, 5-kW-Monoblöcke
und damit die Baureihe, mit der HeishaMon seinerzeit angefangen hat. Wer sie führt, wann welche
zuschaltet, mit welcher Vorlauftemperatur und welcher Pumpendrehzahl — das
entscheidet eine Steuerung in Node-RED unter ioBroker. HeishaMonKaskade ist die
**Schnittstelle** dazwischen: je ein Mikrocontroller pro Wärmepumpe, angebunden
an deren CN-CNT-Anschluss, der das Panasonic-Protokoll nach MQTT übersetzt und
Kommandos in die Gegenrichtung.

```
   Node-RED / ioBroker                MQTT-Broker            HeishaMonKaskade        Wärmepumpe
   (Kaskadenlogik, Wächter)  <---->   (hier: ioBroker-  <---->  Stufe 1 (ESP32)  <---->  WP1
                                       mqtt-Adapter)      <---->  Stufe 2 (ESP32)  <---->  WP2
```

Die Wärmepumpen-Firmware selbst ist Panasonic-Code und wird nicht angefasst.
Sicherheitsrelevante Vorgänge (Abtauen, Frostschutz) entscheidet die Wärmepumpe
allein.

Was diesen Einsatz von der üblichen HeishaMon-Nutzung unterscheidet: Die
Steuerung schickt **mehrere Set-Kommandos gleichzeitig**, an **zwei Geräte**,
und wiederholt den kompletten Sollzustand **alle 5 Minuten** (Re-Assert). Genau
dieses Muster hat Fehler sichtbar gemacht, die im Einzelbetrieb mit
Handbedienung niemandem auffallen.

## Für wen das interessant sein könnte

Dieses Repo ist **öffentlich, aber nicht allgemeingültig**. Es ist auf eine
konkrete Anlage zugeschnitten: zwei Geräte, keine Zone 2, feste MQTT-Präfixe,
eine bestimmte Steuerung dahinter. Wer es unverändert flasht, bekommt mit hoher
Wahrscheinlichkeit nicht das, was er braucht.

Nützlich ist es trotzdem — für alle, die eine eigene Umsetzung bauen:

* **Der Bitmasken-Fund** betrifft jede Installation, die mehr als ein
  Set-Kommando gleichzeitig schickt. Er ist hier belegt und behoben.
* **Die ausgemessenen Wertebereiche** der Kurvenparameter. Die verbreiteten
  Angaben stimmen für diese Geräte nicht — das Original-Projekt prüft
  Wertebereiche überhaupt nicht, die Zahlen stammen also nicht von dort.
* **Die Refactorings** (datengetriebene Tabellen, String-freier Dekodierpfad,
  Konfiguration über Build-Flags) sind unabhängig vom Kaskadenthema übertragbar.
* **Die Werkzeuge in [`test/`](test/README.md)** lösen wiederkehrende Probleme:
  Wie weise ich an einer *laufenden* Anlage nach, dass ein Kommando ankommt,
  ohne den Betrieb zu stören? Wie vergleiche ich zwei Dekodierstände, *bevor*
  ich flashe?
* **Der Changelog** in [`src/version.h`](src/version.h) ist bewusst
  ausführlich: zu jeder Änderung steht dort das Problem, der Nachweis und die
  Größenänderung in RAM und Flash.

## Was gegenüber dem Original anders ist

| Thema | Original | Hier |
| --- | --- | --- |
| Gleichzeitige Set-Kommandos | Byte wird als Ganzes zugewiesen — Felder löschen sich gegenseitig | Bitgenauer Merge über Maskenspalte, Konflikt-Warnung im Log |
| Wertebereiche | keine Prüfung (`cmd[75] = wert + 128`) | Min/Max je Kommando, an der Anlage ausgemessen |
| Set-Kommandos | im Code verteilt | eine Tabelle `setCommands` |
| State-Topics | vier positionsgleiche Parallel-Tabellen | eine Tabelle `stateTopics` |
| Zonen | Zone 1 + Zone 2 | nur Zone 1 (Anlage hat keine zweite Zone) |
| Hardware | getrennte Codebasen | eine Codebasis, ESP8266 + ESP32-S3 |
| Gerätespezifisches | Code anpassen | Build-Flags je Stufe |
| Web-UI | jQuery/CSS vom CDN | inline, ohne externe Abhängigkeiten, mit Auth |
| Dekodierpfad | `String`-Objekte | feste Puffer, keine Heap-Allokation |
| Empfangenes Telegramm | Prüfsumme entscheidet allein | zusätzlich Typ (0x71) und Länge (203) |
| WLAN-Ausfall | keine Prüfung im Betrieb | Watchdog: Reconnect nach 30 s, Neustart nach 5 min |
| Byte 110 | nicht dekodiert | vier Topics mit den Ist-Zuständen (heizt/kühlt tatsächlich) |

Im Detail:

### Die Ist-Zustände aus Byte 110 (3.7.0)

Byte 110 des Antworttelegramms trägt vier 2-Bit-Felder, die das Original nicht
auswertet: ob Flüstermodus und Powerful-Modus **tatsächlich laufen**, ob die
Anlage **tatsächlich heizt oder kühlt**, und den Zustand des externen
Schalters. Daraus werden TOP99 – TOP102.

Der praktische Nutzen steckt in `Heat_Cool_SW_State` (TOP101): Byte 6
(`Operating_Mode_State`) zeigt nur den zuletzt *kommandierten* Modus, TOP101
dagegen den echten Zustand — egal ob per KNX-Aktor, per MQTT oder am
Bedienterminal umgeschaltet wurde. Damit kann die Kaskadensteuerung den
KNX-Aktor als Statusquelle ersetzen.

Die Bitzuordnung stammt aus `ProtocolByteDecrypt.md` und wurde am 2026-08-15
an Stufe 1 nachgemessen: Flüstermodus 0 → 1 → 2 → 3 → 0 und beide
Moduswechsel, einer über KNX, einer über MQTT. Zwei Vorbehalte stehen so auch
in der Topic-Referenz: Der Flüstermodus meldet hier nur AN/AUS (die Stufe
bleibt in TOP18), und der externe Schalter ist an dieser Anlage gar nicht
prüfbar — der Eingang ist nicht belegt, das Feld bleibt dauerhaft aus. Was hier
stattdessen benutzt wird, ist der externe Kompressor-Schalter, und für den war
in den 203 Bytes kein Statusbyte zu finden. Powerful ist seit dem 2026-08-16 in
beiden Zuständen belegt — `set/PowerfulMode 1` an Stufe 1, TOP17 und TOP100
zogen gemeinsam nach.

Weil ein 2-Bit-Feld auch `b11` liefern kann und die Web-Tabelle Indizes nur
nach unten prüft, haben die beiden neuen Klartext-Arrays ein drittes Element
(`unknown`) — sonst läse die Anzeige hinter dem Array. Geprüft von
[`test/byte110_test.cpp`](test/byte110_test.cpp), das den echten Dekodierpfad
mitübersetzt.

### Der Broker spielt beim Verbinden alles wieder ein (3.6.1)

Gefunden am 2026-08-13 mit einem gezielten Neustart, weil der Flüstermodus
nach jedem Reboot auf 0 stand. Beim Booten abonniert die Firmware die 32
Set-Topics — und der ioBroker-MQTT-Adapter beantwortet **jedes neue Abonnement
aus seiner Objektdatenbank**: Er schickt den gespeicherten Wert *jedes*
Set-Topics, teils Monate alt. Die Firmware sah 32 frische Kommandos, alle im
selben 500-ms-Sammelfenster, und schickte sie als ein Telegramm an die
Wärmepumpe.

Zwei davon taten weh:

```text
14:07:44  Reboot ausgelöst
14:07:58  Quiet_Mode_Level                3  -> 0
14:07:58  Z1_Heat_Curve_Target_High_Temp  20 -> 55
14:08:04  Z1_Heat_Request_Temp            20 -> 55
14:11:18  von der Node-RED-Steuerung zurückgesetzt auf 20
14:11:57  Quiet_Mode_Level zurück auf 3
```

`set/Z1HeatCurveTargetHighTemp` stand seit drei Tagen auf 55 — der
Werksvorgabe, die die Wärmepumpe beim Moduswechsel selbst gesetzt hatte. Dieser
Kurvenpunkt *ist* in der Wärmepumpe der Vorlauf-Sollwert (siehe oben), also
sprang die Solltemperatur nach jedem Neustart für gut drei Minuten auf 55 °C.
Unbemerkt geblieben war das nur, weil die Anlage im Kühlbetrieb lief. Der
Flüstermodus wiederum fiel, weil `QuietMode 3` und `PowerfulMode 0` beide Byte 7
schreiben — der bekannte Protokoll-Überlappungsfall, und PowerfulMode trägt ein
implizites „quiet aus".

**Über das Retain-Bit ist das nicht zu filtern:** Der Adapter sendet die
Wiedereinspielung mit `retain=0` (er hat gar keinen getrennten Retained-Speicher),
und PubSubClient reicht das Flag ohnehin nicht an den Callback durch. Deshalb
verwirft die Firmware Set-Kommandos jetzt für 5 Sekunden ab dem SUBACK — der
Schwall kommt unmittelbar danach. Ein in diesem Fenster wirklich gemeintes
Kommando geht verloren; der 5-Minuten-Re-Assert der Steuerung holt es nach.
Verworfene Topics stehen einzeln im Telnet-Log, ihre Zahl als eine Zeile im
MQTT-Log.

### Nur echte Antworttelegramme werden ausgewertet (3.6.0)

Der einzige gefundene Weg, auf dem **falsche Messwerte** in die
Kaskadenregelung geraten konnten. Bis 3.5.0 galt ein Telegramm als
vollständig, sobald die Anzahl gelesener Bytes zum Längenbyte passte — danach
entschied allein die 8-Bit-Prüfsumme, also 1 von 256. Blieben nach einem
Serial-Timeout Reste einer abgebrochenen Antwort im UART-Puffer stehen, las der
nächste Zyklus sie mit; ein so verschobener Bytestrom konnte als Messdaten
durchgehen, retained im ioBroker landen und die Regelung füttern.

Die Wärmepumpe antwortet auf die Abfrage mit genau einem Telegramm: Typ `0x71`,
Längenbyte `0xC8`, 203 Bytes. Genau das prüft
[`src/telegram.h`](src/telegram.h) jetzt — vor der Prüfsumme, damit ein
verschobener Strom schon am Typbyte auffällt und nicht erst mit 1/256 an der
Prüfsumme. Verworfene Telegramme stehen mit Typ und Länge im Log. Zusätzlich
wird der UART-Empfangspuffer **vor** jedem Senden geleert, damit solche Reste
gar nicht erst entstehen.

Wichtig war dabei die Gegenrichtung: Die Prüfung darf kein gültiges Telegramm
wegen seines *Inhalts* verwerfen — sonst stünde die Anlage still.
[`test/telegramm_test.cpp`](test/telegramm_test.cpp) bindet `src/telegram.h`
direkt ein (kein nachgebauter Zwilling) und belegt beides: 1386
Telegrammvarianten werden weiterhin angenommen, abgewiesen werden dagegen das
111-Byte-Abfrageecho, Typ `0xF1`, 204 Bytes, unvollständige Antworten und alle
202 möglichen Verschiebungen. Über 200 000 Zufallspuffer, deren Länge zum
Längenbyte passt: alte Regel 817 Annahmen (0,41 % — die erwarteten 1/256), neue
Regel 0.

Aus derselben Durchsicht stammen zwei weitere Härtungen: Der MQTT-Reconnect
blockiert die Hauptschleife nicht mehr (2 s Socket-Timeout statt der 15 s des
PubSubClient, Backoff bis 60 s, ohne WLAN gar kein Versuch), und ein
WLAN-Watchdog beendet den Zustand "läuft, aber redet mit niemandem" — nach 30 s
ohne Verbindung ein Reconnect-Versuch, nach 5 Minuten ein Neustart.

### Set-Kommandos bitgenau mischen (3.1.0)

Der wichtigste Fund des Projekts. Mehrere Set-Topics teilen sich ein
Protokollbyte in verschiedenen Bitgruppen:

| Byte | Felder | Masken |
| --- | --- | --- |
| 4 | Heatpump, WaterPump, ForceDHW | 0x03, 0x30, 0xC0 |
| 5 | HolidayMode | 0x30 |
| 8 | ForceDefrost, ForceSterilization | 0x02, 0x04 |

Wird das Byte als Ganzes geschrieben, fallen die fremden Felder auf
`0 = keine Änderung`. Trafen zwei Kommandos im selben 500-ms-Sammelfenster ein,
löschte das zweite das erste aus — **still**, das Log quittierte beide.
Ausgerechnet der 5-Minuten-Re-Assert der Kaskade (sechs Kommandos pro Gerät auf
einmal) trifft diesen Fall zuverlässig.

Belegt am Prüfstand und an der laufenden Anlage:

```text
3.0.1   F1 6C 01 10 10 ...   Heatpump-Bits = 0, verloren
3.1.0   F1 6C 01 10 12 ...   Heatpump = 2, WaterPump = 1
```

Ausnahme Byte 7: QuietMode und PowerfulMode überlappen im Protokoll selbst —
das ist eine Protokolleigenschaft, kein Implementierungsfehler. Dort bleibt das
Verhalten unverändert, die Firmware warnt nur.

Dazu kam ein zweiter Fehler aus derselben Ecke: Ob überhaupt etwas zu senden
war, entschied eine Bytesummen-Heuristik. Die konnte "leerer Puffer" nicht von
"Summe auf 256 umgeschlagen" unterscheiden und verwarf gelegentlich ganze
Kommandotelegramme. Jetzt gibt es ein explizites Flag.

### Wertebereiche — geprüft und nachgemessen (2.1.0, 3.2.1, 3.2.2)

Jedes Set-Kommando hat Min/Max in der Tabelle; ungültige Werte werden abgelehnt
statt weitergereicht. Das ist nicht nur Kosmetik: Die Wärmepumpe **klemmt
Werte außerhalb ihres Bereichs kommentarlos auf den Rand**, ohne Fehlermeldung.
Ohne Prüfung verschwindet ein falscher Wert also lautlos.

Beim Nachmessen mit [`test/kurven_grenzen.py`](test/kurven_grenzen.py) stellte
sich heraus, dass zwei verbreitete Bereichsangaben für diese Geräte schlicht
falsch sind:

| Parameter | verbreitet | gemessen |
| --- | --- | --- |
| `Z1HeatCurveOutsideHighTemp` | 15 … 35 | **-15 … 15** |
| `Z1CoolCurveOutsideHighTemp` | 20 … 30 bzw. 30 … 40 | **15 … 30** |

Der Heizwert ist der lehrreiche Fall: Gültig ist alles *bis* 15, nicht *ab* 15 —
der angenommene Bereich lag komplett auf der falschen Seite. Von 21 vermeintlich
erlaubten Werten war genau einer gültig, und das fiel nur auf, weil die
Anlagenkonfiguration zufällig exakt diesen einen nutzt.

### Heiz- und Kühlkurve als Set-Kommandos (3.2.0)

SET27 – SET34 schreiben die Zone-1-Kurven (Bytes 75-78 und 86-89). Zweck ist
der **Notbetrieb**: Fällt die Node-RED-Steuerung aus, wird am Bedienterminal von
Direkt- auf Kurvenbetrieb umgeschaltet, und die Anlage läuft mit denselben
Werten weiter. Dafür spiegelt [`test/kurven_sync.py`](test/kurven_sync.py) die
im ioBroker gepflegten Kurven in beide Wärmepumpen.

Eine Falle steckt darin, die man kennen sollte: `Z1HeatCurveTargetHighTemp`
(SET27) und `Z1HeatRequestTemperature` (SET5) sind in der Wärmepumpe
**derselbe Wert** — im Direktmodus die Vorlauf-Solltemperatur, im Kurvenmodus
der obere Kurvenpunkt. Den oberen Kurvenpunkt zu setzen greift also in den
laufenden Betrieb ein, und im Direktbetrieb ist er nicht haltbar. Der komplette
Nachweis steht in [`test/README.md`](test/README.md).

### Eine Codebasis für ESP8266 und ESP32-S3 (3.0.0)

Die Plattformunterschiede sind in [`src/HeishaMon.h`](src/HeishaMon.h) isoliert:
Auf dem D1 mini hängt die Wärmepumpe an der getauschten Haupt-UART, auf dem
ESP32-S3-Board an einer eigenen `Serial1` (RX18/TX17) — die USB-Konsole bleibt
dort parallel nutzbar. Die bewährte Timing-Kette (Ticker, `serialquerysent` als
Mutex) ist hardwareunabhängig und wurde unverändert übernommen.

Zwei Stolpersteine, die Zeit gekostet haben und im Changelog stehen:
Das offizielle Board hat **4 MB Flash**, nicht 8 wie die
`esp32-s3-devkitc-1`-Definition annimmt (sonst Boot-Loop), und der
**WiFi-Modem-Sleep muss aus** (`WiFi.setSleep(false)`), sonst sind eingehende
Verbindungen tot.

### Konfiguration je Stufe über Build-Flags (2.2.0)

MQTT-Präfix, Web-Titel und Hostname kommen als Build-Flags aus
`platformio.ini`. Beim Wechsel zwischen Stufe 1 und Stufe 2 wird kein Code mehr
angefasst — nur das Env gewechselt.

### Datengetriebene Tabellen (2.2.0, 3.3.0, 3.5.0)

Set-Kommandos und State-Topics stehen in je einer Tabelle, eine Zeile pro
Topic. Vorher waren die Angaben zu einem State-Topic über vier positionsgleiche
Arrays verteilt, die nur über den Index und einen Kommentar zusammenhingen.

Seit 3.5.0 gilt das auch für die **Namen** der Set-Topics. Bis dahin führte
jeder Name drei Leben — Deklaration in `Topics.h`, Definition in `Topics.cpp`,
Verweis in der Tabelle — und dazu kam ein handgeschriebener `subscribe`-Aufruf
je Topic in `mqtt_reconnect()`. Wer den vergaß, bekam **keinen Compilerfehler**:
Das Topic war stumm, die Wärmepumpe folgte einem Kommando nicht mehr, und im
Log stand nichts. Jetzt trägt die Tabellenzeile den Namen, `Topics.h` nur noch
die Pfadwurzeln, und `subscribe_set_topics()` läuft über dieselbe Tabelle. Ein
neues Set-Kommando ist damit genau eine Zeile.

Wichtig dabei: Die TOP-Nummer ist ein **Datenfeld, nicht der Array-Index**.
Zeilen können entfallen, ohne dass sich die Nummern der übrigen verschieben —
und genau das ist beim Zone-2-Ausbau passiert. Vorher entschied ein `switch`
über fest verdrahtete Nummern (`case 44:`, `case 90:`), welches Topic mehrere
Bytes braucht; jede Verschiebung hätte diese Marken stillschweigend auf andere
Topics zeigen lassen, **ohne Compilerfehler**.

Abgesichert wurde der Umbau mit
[`test/decode_vergleich.py`](test/decode_vergleich.py): Zwei Codestände werden
auf dem Rechner übersetzt (Arduino-Ersatzheader liegen im Skript, kein Gerät
nötig) und mit denselben 756 Telegrammen gefüttert — jeder Bytewert 0-255 auf
allen Positionen plus 500 Pseudozufallstelegramme. Verglichen werden Nummer,
Name, Wert und Einheit je Topic. Ergebnis: identisch.

### Zone 2 entfernt (3.4.0)

Diese Anlagen haben keine zweite Zone; die Topics trugen nur dekodiertes
Rauschen und legten im ioBroker Objekte an, die niemand deuten kann. Weg sind
TOP34, TOP35, TOP37, TOP43, TOP57 und TOP82 – TOP89 sowie SET7/SET8.

**Die Nummerierung hat dadurch Lücken, und das ist Absicht.** Jedes verbliebene
Topic behält seine bisherige Nummer, damit `MQTT-Topics.md`, ältere Mitschnitte
und die Nummern des Original-Projekts weiter gelten. Bitte nicht
durchnummerieren.

### Speicher und Robustheit (2.0.1 – 2.3.1)

* Dekodierpfad komplett `String`-frei — feste Puffer statt Heap-Allokationen im
  5-Sekunden-Takt (auf dem ESP8266 der Unterschied zwischen "läuft" und
  "fragmentiert nach Tagen").
* Web-UI ohne CDN-Abhängigkeiten, Authentifizierung für alle
  zustandsändernden Endpunkte, MQTT-Passwort nicht mehr im HTML.
* NTP-Timeout statt Endlosschleife beim Start, mDNS-Fehler nicht mehr fatal,
  korrekte Sommer-/Winterzeit.
* Bounds-Check für den seriellen Empfangspuffer; der Abfragezyklus bleibt nach
  einem ungültigen MQTT-Wert nicht mehr stehen.

Der vollständige Changelog mit Begründung und Nachweis je Version steht in
[`src/version.h`](src/version.h).

## Aufbau

| Datei | Inhalt |
| --- | --- |
| [`src/HeishaMon.cpp`](src/HeishaMon.cpp) | Hauptschleife, Timing-Kette, serielle Anbindung, MQTT, OTA |
| [`src/HeishaMon.h`](src/HeishaMon.h) | Plattformschicht ESP8266/ESP32, Timing-Konstanten |
| [`src/telegram.h`](src/telegram.h) | Typ-, Längen- und Prüfsummenregel des Antworttelegramms (auch vom Hosttest genutzt) |
| [`src/commands.cpp`](src/commands.cpp) | Tabelle `setCommands` — Quelle der Wahrheit für alle Set-Kommandos |
| [`src/decode.cpp`](src/decode.cpp) | Tabelle `stateTopics` und die Dekodierer |
| [`src/Topics.cpp`](src/Topics.cpp) | Wurzeln der MQTT-Pfade (`state`, `set`, `info`) — die Topic-Namen stehen in den Tabellen |
| [`src/webfunctions.cpp`](src/webfunctions.cpp) | Weboberfläche und Einstellungen |
| [`src/version.h`](src/version.h) | Versionsnummer und ausführlicher Changelog |
| [`MQTT-Topics.md`](MQTT-Topics.md) | Topic-Referenz (englisch), aus den Tabellen nachgezogen |
| [`test/`](test/README.md) | Diagnose- und Nachweiswerkzeuge |
| [`ProtocolByteDecrypt.md`](ProtocolByteDecrypt.md) | Notizen zum Protokoll auf Byte-Ebene |

Zeitverhalten (Konstanten in `HeishaMon.h`): alle 5 s eine Abfrage an die
Wärmepumpe, 500 ms Sammelfenster für eingehende Set-Kommandos, 600 ms Timeout
für die 203 Bytes Antwort. Gesendet wird immer nur eine Sache zur Zeit —
`serialquerysent` wirkt als Mutex.

## Bauen und Flashen

Gebaut wird mit [PlatformIO](https://platformio.org/). Die gerätespezifischen
Zugangsdaten liegen in `platformio_user_env.ini` (nicht in git); als Vorlage
dient `platformio_user_env_sample.ini`:

```bash
cp platformio_user_env_sample.ini platformio_user_env.ini
```

In `platformio_user_env.ini` stehen ausschließlich Upload-Ziele, getrennt nach
Board: `[usb_defaults]` und `[ota_defaults_h1|h2]` für den D1 mini,
`[usb32_defaults]` und `[ota32_defaults_h1|h2|test]` für das ESP32-Board. Alle
Sektionen müssen vorhanden sein, auch wenn nur ein Board benutzt wird.

`platformio.ini` selbst ist in Bausteine gegliedert: Board-Basis
(`[esp8266_base]`, `[esp32_base]`), Stufen-Identität (`[stage_h1]`,
`[stage_h2]`, `[stage_test_*]`) und darauf aufbauend die Envs — die Werte sind
die dieser Anlage und dienen als Beispiel:

**Produktiv (Update per OTA):**

| Env | Board | Zweck |
| --- | --- | --- |
| `heishamon_esp32_h1_ota` | ESP32-S3 | Stufe 1 an WP1 |
| `heishamon_esp32_h2_ota` | ESP32-S3 | Stufe 2 an WP2 |
| `d1_mini_h1_ota` | D1 mini | Stufe 1 auf ESP8266 (Rückfallebene) |
| `d1_mini_h2_ota` | D1 mini | Stufe 2 auf ESP8266 (Rückfallebene) |

**Erstsetup (Flash über USB):**

| Env | Board | Zweck |
| --- | --- | --- |
| `heishamon_esp32_h1_usb` | ESP32-S3 | Erstflash der Stufe 1 (s. u.) |
| `heishamon_esp32_h2_usb` | ESP32-S3 | Erstflash der Stufe 2 (s. u.) |
| `d1_mini_usb` | D1 mini | Erstflash ohne Stufen-Flags |

**Test:**

| Env | Board | Zweck |
| --- | --- | --- |
| `heishamon_esp32_usb` / `_ota` | ESP32-S3 | Testgerät mit eigenem MQTT-Präfix |
| `d1_mini_test` | D1 mini | Prüfstand **ohne** Wärmepumpe |

```bash
pio run -e heishamon_esp32_h1_ota -t upload
```

Für eine eigene Anlage sind die Build-Flags der Stufen anzupassen — Präfix und
Web-Titel in der Stufen-Sektion, der Hostname im Env (ESP8266- und ESP32-Board
derselben Stufe können parallel im Netz hängen und brauchen eindeutige Namen):

```ini
[stage_h1]
build_flags =
	-D HEISHA_MQTT_PREFIX='"panasonic_heat_pump"'
	-D HEISHA_STAGE_NAME='"Heisha Stufe 1"'

[env:meine_stufe]
extends = esp32_base
build_flags =
	${esp32_base.build_flags}
	${stage_h1.build_flags}
	-D HEISHA_HOSTNAME='"HeishaMon32_h1"'
```

Ohne diese Flags greifen die Stufe-1-Fallbacks aus dem Code. **Für ein
Testgerät ist ein eigenes Präfix Pflicht** — sonst sitzt der Prüfling auf dem
LWT- und State-Pfad der produktiven Wärmepumpe.

Nach dem Flashen richtet sich das Gerät wie das Original über einen
WiFi-Manager-Hotspot ein; MQTT-Server und Zugangsdaten stehen danach unter
`http://<ip>/settings`. Weboberfläche, Telnet-Log (Port 23) und OTA sind
verfügbar wie gewohnt.

### Erstflash eines ESP32-Boards, das noch die Original-Firmware trägt

Der erste Flash muss **über USB** laufen: die Original-Firmware bringt eine
andere Partitionstabelle mit, und die lässt sich per OTA nicht tauschen. Zwei
Punkte, die dabei überraschen (beide am 2026-08-11 an Stufe 2 durchgemessen):

* **Die WLAN-Zugangsdaten der Original-Firmware werden nicht übernommen.** Sie
  liegen dort in deren eigenem Speicher, nicht an der Stelle, an der der
  WiFiManager sucht. Das Board geht nach dem Flash in den Setup-Hotspot
  `HeishaMon-Setup` (`http://192.168.4.1`, Portal-Timeout 180 s, danach Reboot
  und der Hotspot kommt neu). WLAN, Hostname, OTA-Passwort und **MQTT-Server**
  dort eintragen — der MQTT-Server hat keinen Default und bleibt sonst leer.
* **Der Hostname aus der `config.json` gewinnt gegen das Build-Flag.** Das Flag
  `HEISHA_HOSTNAME` ist nur der Default für den Fall, dass keine Konfiguration
  existiert. Im Portal also gleich den endgültigen Namen eintragen — er ist
  zugleich die MQTT-Client-ID, und zwei Geräte dürfen sie nicht teilen.

Reihenfolge beim Ersetzen eines laufenden Geräts, damit die produktiven Topics
sauber bleiben: erst **Testfirmware** (eigenes Präfix) per USB aufspielen und
Netz, Web, MQTT und OTA prüfen — solange das Board noch am Schreibtisch liegt
und ein USB-Kabel in Reichweite ist. Vor dem Einbau MQTT stilllegen, am
einfachsten über einen Port, auf dem der Broker nichts anbietet
(`curl -u admin:<pw> "http://<ip>/settings?mqtt_port=1884"`) — sonst legt die
Testfirmware in den Minuten am Kabel einen kompletten Satz State-Objekte unter
dem Testpräfix im ioBroker an. Dann Altgerät stromlos, Board anschließen, per
OTA die Stufen-Firmware aufspielen und den Port zurücksetzen. So steht zu
keiner Zeit ein Leerwert (−128, −1) auf einem produktiven State-Topic.

## MQTT-Schnittstelle

90 State-Topics und 32 Set-Kommandos, Namen kompatibel zum Original-HeishaMon.
Die vollständige Referenz mit Byte-Spalte und Wertebereichen steht in
[`MQTT-Topics.md`](MQTT-Topics.md).

```
<präfix>/LWT                       Online / Offline
<präfix>/log                       Klartext-Log (umschaltbar)
<präfix>/state/<Topic>             dekodierte Werte, mit Retain-Flag
<präfix>/set/<Kommando>            Kommandos an die Wärmepumpe
```

Zwei Hinweise aus der Praxis:

* Die Firmware publiziert **mit Retain-Flag**. Entfällt ein Topic, hört die
  Firmware auf zu senden — ein Broker liefert den letzten Wert aber weiter an
  jeden neuen Abonnenten aus. Das Topic verschwindet nicht, es friert ein.
  Dafür gibt es [`test/retained_loeschen.py`](test/retained_loeschen.py).
* Läuft als Broker der **ioBroker-mqtt-Adapter im Server-Modus** (wie hier),
  gibt es gar keinen getrennten Retained-Speicher: Der Adapter bedient
  Abonnenten aus seiner eigenen Objektdatenbank. Ein Löschbefehl setzt den
  State dann nur auf `null` — wirklich weg ist das Topic erst, wenn das Objekt
  im ioBroker-Admin gelöscht wird.

## Diagnose- und Nachweiswerkzeuge

Unter [`test/`](test/README.md) liegt eine Sammlung von Werkzeugen, die alle
mit der Python-Standardbibliothek auskommen — vom Host-Testprogramm für die
Merge-Logik über den Prüfstandstest bis zum passiven Mitschnitt am laufenden
Gerät. Zwei Vorgehensweisen daraus sind über dieses Projekt hinaus brauchbar:

* **Nachweis an einer laufenden Anlage, ohne sie zu stören:** genau die Werte
  senden, die die Steuerung ohnehin gerade kommandiert. Dann ändert sich am
  Sollzustand nichts, und ein Fehlschlag korrigiert sich spätestens mit dem
  nächsten Re-Assert von selbst.
* **Umbauten am Dekodierpfad absichern, bevor geflasht wird:** zwei Codestände
  auf dem Rechner übersetzen und mit identischen Telegrammen vergleichen.

`test/README.md` dokumentiert außerdem die Fallstricke, über die wir gestolpert
sind — idempotente Sender richtig deuten, den Hexlog korrekt parsen, MQTT-
Client-IDs bei schnellen Reconnects.

## Was bewusst nicht drin ist

* **Zone 2** — siehe oben.
* **Extras des ESP32-Boards** (1-Wire, S0-Zähler, OpenTherm) bleiben ungenutzt.
* **ArduinoJson bleibt auf 6.x** — v7 ist für den ESP8266 wegen des Footprints
  nicht empfohlen.
* **Keine Unity-Testsuite.** Die Werkzeuge in `test/` sind eigenständige
  Diagnoseprogramme, `pio test` nutzt sie nicht. `merge_test.cpp` wäre die
  Vorlage für echte Unit-Tests.
* Ein größerer Umbauplan (RX-State-Machine, FreeRTOS-Tasks, Verify/Ack mit
  Retry, `set/batch`) wurde erarbeitet und bewusst zurückgestellt: für diesen
  Einsatzzweck zu viel Komplexität.

## Herkunft und Dank

Basis ist das Projekt [HeishaMon](https://github.com/Egyras/HeishaMon) von
Egyras und der HeishaMon-Community — ohne deren Protokollarbeit gäbe es hier
nichts. Die Hardware ist das offizielle HeishaMon-ESP32-Board.

Die Umbauten dieses Forks sind in Zusammenarbeit mit **Claude Code**
entstanden. Das erklärt auch den Dokumentationsstil: Zu jeder Änderung gehört
die Begründung, der Nachweis und der Weg zurück. Das übergeordnete Ziel ist
Wartbarkeit — die Anlage soll auch von jemandem am Laufen gehalten werden
können, der ihre Entstehungsgeschichte nicht kennt.

## Kontakt

Fragen, Korrekturen und eigene Messwerte gerne als
[Issue](https://github.com/gregor-samosir/HeishaMonKaskade/issues) — dann steht
die Antwort auch für den nächsten Leser da. Besonders willkommen sind
Rückmeldungen zu den ausgemessenen Wertebereichen: Ob die Klemmgrenzen bei
anderen Modellreihen genauso liegen, weiß ich nicht.

## Lizenz

[MIT](LICENSE).

Und der übliche, hier ernst gemeinte Hinweis: Diese Firmware schreibt in eine
Heizungsanlage. Was sie an die Wärmepumpe schickt, verantwortet der Betreiber.
Vor dem ersten Einsatz an einer fremden Anlage bitte die Wertebereiche in
`src/commands.cpp` gegen das eigene Gerät prüfen — sie sind an *dieser* Anlage
ausgemessen.
