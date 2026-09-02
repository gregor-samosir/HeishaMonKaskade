# Diagnose- und Nachweiswerkzeuge

Entstanden bei der Fehlersuche zum Bitmasken-Merge (Version 3.1.0, 2026-08-07).
Alle Werkzeuge laufen mit der Python-Standardbibliothek, es muss nichts
installiert werden.

## Der Fehler, um den es geht

Mehrere SET-Topics teilen sich ein Protokollbyte in verschiedenen Bitgruppen.
Bis 3.0.1 wurde das Byte als Ganzes zugewiesen, wodurch die fremden Felder auf
0 = "keine Aenderung" fielen. Trafen zwei SETs im selben 500-ms-Sammelfenster
ein, loeschte das zweite das erste still aus:

| Byte | Felder | Masken |
| --- | --- | --- |
| 4 | Heatpump, WaterPump, ForceDHW | 0x03, 0x30, 0xC0 |
| 5 | HolidayMode | 0x30 |
| 8 | ForceDefrost, ForceSterilization | 0x02, 0x04 |

Byte 7 (QuietMode/PowerfulMode) ueberlappt im Protokoll selbst und bleibt
bewusst unveraendert - dort warnt die Firmware nur.

## Werkzeuge

| Datei | Zweck | Braucht Hardware |
| --- | --- | --- |
| `merge_test.cpp` | Merge-Logik auf dem Host durchspielen, inkl. Suchlauf ueber den realen Wertebereich | nein |
| `telegramm_test.cpp` | Typ-, Laengen- und Pruefsummenpruefung des Antworttelegramms (bindet `src/telegram.h` direkt ein) | nein |
| `sendwindow_test.cpp` | Zeitregeln des Kommando-Sammelfensters inkl. `millis()`-Ueberlauf (bindet `src/sendwindow.h` direkt ein) | nein |
| `byte110_test.cpp` | Die vier Ist-Zustands-Topics aus Byte 110 (TOP99-102) gegen den echten Dekodierpfad pruefen | nein |
| `byte28_test.cpp` | Kodierung von SET35/SET36 gegen die Dekodierer aus `decode.cpp` haltbar machen (Byte 28, zwei Bitfelder) | nein |
| `byte9_test.cpp` | Kodierung der Heizstab-Kommandos SET37-SET39 gegen den echten Dekodierpfad (Byte 9 traegt beide Freigaben, Byte 5 ForceHeater neben HolidayMode) | nein |
| `byte23_25_test.cpp` | Die sieben Installer-Topics TOP105-111 aus Byte 25 und Byte 23 gegen die gemessenen Rohbytes (`h2.log`, `h2_ext.log`) | nein |
| `notbetrieb_test.cpp` | Regeln des Notbetriebs: Vollstaendigkeit der Werte, Bereichsgrenzen, Karenzzeit-Ausnahme, Zustandsautomat, Freigabe ueber TOP101, Anzeigeverfall und die Plausibilitaet der Kurve (bindet `src/notbetrieb.h` direkt ein) | nein |
| `verbindung_test.cpp` | Zeitregeln der Verbindungswacht: Karenz, "seit dem Neustart nie verbunden" und der `millis()`-Ueberlauf (bindet `src/verbindung.h` direkt ein) | nein |
| `rtcspiegel_test.cpp` | Gueltigkeitsregel des RTC-Spiegels: Magic mit Layoutnummer, Rolle, Maskenbreite, Pruefsumme, Bitkipper und die Saettigung des Bootzaehlers (bindet `src/rtcspiegel.h` direkt ein) | nein |
| `decode_hosttest.sh` | Baurahmen fuer `byte110_test.cpp`, `byte9_test.cpp` und `byte23_25_test.cpp` - kopiert `decode.cpp` neben die Ersatzheader aus `stubs/` | nein |
| `hexlog_test.py` | Kerntest: Heatpump + WaterPump muessen in einem Telegramm landen | Pruefstand |
| `verteiler_test.py` | Abnahmetest: alle sechs Kanaele des Node-RED-Verteilers gleichzeitig | Pruefstand |
| `produktiv_mitschnitt.py` | Passiv am laufenden Geraet mithoeren, sendet nichts | Produktivgeraet |
| `kurven_test.py` | Kurven-Kommandos SET27-SET34 nachweisen (schreibt die Ist-Werte zurueck, veraendert nichts) | Produktivgeraet |
| `kurven_sync.py` | Heiz-/Kuehlkurve aus dem ioBroker-Konfigurationsbaum in die WPs spiegeln (`--dry-run`); bricht bei verdrehter Kurve ab (`--kurve-ignorieren`) | Produktivgeraet |
| `kurven_grenzen.py` | Ermitteln, welche Kurvenwerte die WP wirklich annimmt (veraendert Werte, stellt sie zurueck) | Produktivgeraet |
| `decode_vergleich.py` | Dekodierpfad zweier Codestaende gegeneinander laufen lassen, auf dem Mac | nein |
| `frame_diff.py` | Rohtelegramme eines Mitschnitts ueber alle 203 Bytes vergleichen, angereichert aus `ProtocolByteDecrypt.md` | nein |
| `retained_loeschen.py` | Retained Messages entfallener state-Topics vom Broker raeumen (Anzeige, `--loeschen` fuer echt) | Broker |
| `tablesnap.py` | Momentaufnahme der Topic-Tabelle ueber `/tablerefresh`, zeilenweise diffbar - fuer die Abnahme nach dem Flashen | Produktivgeraet (nur lesend) |
| `top_watch.py` | Verlauf statt Momentaufnahme: ausgewaehlte TOPs im Takt abfragen und jede Aenderung mit Zeitstempel melden | Produktivgeraet (nur lesend) |
| `set_top_zuordnung.py` | Erzeugt die Tabellen in `SET-TOP-Zuordnung.md`: welches State-Topic liest ein Set-Kommando zurueck | nein |
| `byte_monitor.py` | Einzelne Bytes des Antworttelegramms beobachten, um eine Byte-Zuordnung zu belegen statt sie abzuleiten | Produktivgeraet (nur lesend) |
| `heisha_probe.py` | gemeinsame Helfer (Telnet, Hexlog-Parser) | - |
| `telnet_mitschnitt.py` | Passiver Telnet-Mitschnitt eines Geraets - sendet NICHTS, roher Socket auf Port 23 (telnetlib ist ab Python 3.13 entfernt). Fuer die Antwortquote und fuer `<DBG>`-Zeilen, die `produktiv_mitschnitt.py` nicht zeigt | Geraet im Netz |
| `mqtt_pub.py` | minimaler MQTT-Publisher ohne Abhaengigkeiten | - |
| `mqtt_sub.py` | minimaler MQTT-Subscriber - zeigt, was der Broker einem NEUEN Abonnenten von sich aus einspielt | Broker |
| `stubs/` | Arduino-Ersatzheader, gemeinsam genutzt von `byte110_test.cpp`, `byte9_test.cpp` und `decode_vergleich.py` | - |

## Pruefstand aufsetzen - ein Backup-Board leihen

Bis 3.15.0 war der Pruefstand ein eigenes Board (`d1_mini_test`, D1 mini,
192.168.2.197), das man einfach anstecken konnte. Mit 3.16.0 ist der
ESP8266-Pfad weg, und ein drittes ESP32-Board gibt es bewusst nicht. Der
Pruefstand ist seitdem eines der beiden **Backup-Boards, leihweise**
([`Ablauf-Backup-Boards.md`](../Ablauf-Backup-Boards.md)).

Eine Waermepumpe braucht der Pruefstand nach wie vor nicht: Der Hexlog gibt das
Telegramm aus, bevor es auf die Leitung geht.

### Warum das ueberhaupt sicher ist

`HEISHA_MQTT_PREFIX` ist ein reines Build-Flag - es wechselt mit der Firmware.
Hostname, MQTT-Port, Broker und Zugangsdaten stehen dagegen in der
`config.json` in LittleFS, und `loadConfigValue()` ueberschreibt damit beim
Start die Build-Vorgaben. LittleFS ueberlebt OTA wie USB-Flash, die Partitionen
sind getrennt (`min_spiffs.csv`).

Groesse | Herkunft | Beim Flashen mit Test-Firmware
:--- | :--- | :---
MQTT-Prefix | Build-Flag | **wechselt** auf `panasonic_heat_pump32`
Hostname | `config.json` schlaegt Build-Flag | bleibt `HeishaMon32_h1b`
MQTT-Port | `config.json` | bleibt **1884**
Broker, Zugangsdaten, WLAN | `config.json` | bleiben

Ein frisch mit Test-Firmware bespieltes Backup-Board ist damit **doppelt
gesperrt**: falsches Prefix *und* toter Port. Es kann in diesem Zustand nichts
anrichten.

### Ausleihe

1. Backup-Board holen und mit Strom versorgen.
2. `pio run -e heishamon_esp32_usb -t upload` - Prefix `panasonic_heat_pump32`.
3. Ueber `http://<ip>/settings` den **MQTT-Port auf 1883** setzen. Erst jetzt
   erreicht der Pruefling den Broker - auf dem Test-Prefix, an das der
   Node-RED-Verteiler nichts sendet.
4. Testen.

Der Hostname bleibt dabei `HeishaMon32_h1b`. Das ist kein Fehler, sondern die
zweite Sicherung: Er ist zugleich die MQTT-Client-ID und kollidiert mit keinem
produktiven Board.

### Rueckgabe - die Reihenfolge ist sicherheitskritisch

> **Erst den Port auf 1884 stellen, dann die Stufen-Firmware flashen.
> Nie umgekehrt.**

Nach Schritt 3 steht in der `config.json` `mqtt_port = 1883`. Wird in diesem
Zustand die Stufen-Firmware aufgespielt, wechselt das Prefix zurueck auf
`panasonic_heat_pump` - und das Backup-Board sitzt mit **produktivem Prefix auf
dem echten Broker**. Es publiziert dann `state`- und `LWT`-Topics und abonniert
die `set`-Topics parallel zum laufenden Board, ohne ueberhaupt an einer
Waermepumpe zu haengen. Der abweichende Hostname verhindert nur den
gegenseitigen Client-ID-Rauswurf, nicht das Mitschreiben.

1. Ueber `/settings` des Prueflings **`mqtt_port` auf 1884** setzen. Das Geraet
   startet neu und erreicht den Broker nicht mehr.
2. Erst danach `pio run -e heishamon_esp32_h1_usb -t upload` (bzw. `_h2_usb`).
3. Gegenprobe: `http://<ip>/settings` oeffnen und Port **1884** sowie Hostname
   `HeishaMon32_h1b` ablesen, bevor das Board zurueck in die Schublade geht.
   Das ist kein Formalismus - es ist die einzige Stelle, an der ein Fehler in
   Schritt 1 noch auffaellt.
4. Stromlos ablegen.

### Was die Ausleihe kostet

Solange ein Backup-Board Pruefstand ist, hat die betreffende Stufe **keinen
Notanker**. Deshalb: immer nur **ein** Board gleichzeitig ausleihen, und vorher
kurz pruefen, ob an der eigenen Stufe gerade etwas ansteht (Rollout,
Heizperiode, Abwesenheit). Ein Pruefstand ist selten dringend.

## Antwortquote messen

Die aussagekraeftigste einzelne Zahl ueber die serielle Strecke: Wie viele der
gesendeten Telegramme die Waermepumpe beantwortet hat. Nur eine Telnet-Sitzung
gleichzeitig - vorher pruefen, dass niemand sonst drauf ist.

```bash
./test/telnet_mitschnitt.py 192.168.2.120 420 > mitschnitt.txt
sed -i '' 's/\x1b\[[0-9;]*m//g' mitschnitt.txt   # Farbcodes raus
for M in "Send query" "Send command" "Valid data" "Telegramm verworfen" \
         "Serial interface read timeout" "Restdaten vor dem Senden" \
         "Mqtt reconnect" "Lesefenster laeuft noch"; do
  printf '%-32s %s\n' "$M" "$(grep -c "$M" mitschnitt.txt)"
done
```

`Send query` + `Send command` muss gleich `Valid data` sein. Referenzwerte aus
7-Minuten-Laeufen an Stufe 1:

Datum | Version | gesendet | Valid data | verworfen
:--- | :--- | ---: | ---: | ---:
2026-08-13 | 3.6.0 | 68 + 4 | 72 | 0
2026-08-27 | 3.16.0 | 68 + 2 | 70 | 0

Der Zyklus liegt konstant bei 6 s. **Ausreisser von 11-12 s sind kein Befund**,
sondern Bauart: Die Leitung ist halbduplex, ein Kommandotelegramm kostet eine
Abfragerunde.

**`Telegramm verworfen (unvollstaendig): Typ 0x00, Laenge 0` heisst: gar keine
Antwort.** Das `0x00` ist der Platzhalter der Logzeile (`HeishaMon.cpp`, `(serial_length > 0) ? serial_data[0] : 0`), kein Byte von der Leitung - es stand
also nichts auf der Leitung, nicht Muell. Am 2026-08-27 trat das zweimal
waehrend eines Notbetriebslaufs auf, beide Male mit einem zweiten Telegramm
dicht hinter dem ersten (der 5-min-Re-Assert fiel in den letzten Schritt).
Verloren ist dabei eine Leserunde, kein Kommando: Der Notbetrieb liest jeden
Schritt zurueck, die Steuerung wiederholt alle 5 min. Im Normalbetrieb liegt
die Quote bei 100 %, siehe Tabelle.

## Ausfuehren

Die C++-Programme pruefen ihre Ergebnisse selbst und geben bei gebrochener
Zusicherung `1` zurueck - die CI bricht dann ab. Vorher (bis 3.5.0) gaben sie
ihre Zahlen nur aus.

`byte110_test.cpp` und `byte9_test.cpp` laufen ueber das Skript, weil dabei
`decode.cpp` neben die Ersatzheader kopiert werden muss (Begruendung im
Skriptkopf). Ohne Argument baut das Skript `byte110_test.cpp`.

```bash
c++ -std=c++17 -O2 -o /tmp/merge_test merge_test.cpp && /tmp/merge_test
c++ -std=c++17 -O2 -Wall -o /tmp/byte28_test byte28_test.cpp && /tmp/byte28_test
c++ -std=c++17 -O2 -o /tmp/telegramm_test telegramm_test.cpp && /tmp/telegramm_test
c++ -std=c++17 -O2 -o /tmp/sendwindow_test sendwindow_test.cpp && /tmp/sendwindow_test
c++ -std=c++17 -O2 -Wall -o /tmp/notbetrieb_test notbetrieb_test.cpp && /tmp/notbetrieb_test
c++ -std=c++17 -O2 -Wall -o /tmp/verbindung_test verbindung_test.cpp && /tmp/verbindung_test
c++ -std=c++17 -O2 -Wall -o /tmp/rtcspiegel_test rtcspiegel_test.cpp && /tmp/rtcspiegel_test
./decode_hosttest.sh          # byte110_test.cpp, aus dem Repo-Wurzelverzeichnis auch ./test/...
./decode_hosttest.sh test/byte9_test.cpp   # Heizstab-Kommandos SET37-SET39 (Pfad immer repo-relativ)

./hexlog_test.py     --esp <ip-des-pruefstands> --broker 192.168.2.147
./verteiler_test.py  --esp <ip-des-pruefstands>
./produktiv_mitschnitt.py --esp 192.168.2.120     # nur mithoeren
```

Die IP des Pruefstands ist die des geliehenen Backup-Boards (DHCP, ueber den
Router oder den mDNS-Namen `HeishaMon32_h1b.local` zu finden). Bis 3.15.0 stand
hier die feste 192.168.2.197 des D1-mini-Pruefstands - sie taucht in den
datierten Nachweisen weiter unten deshalb noch auf.

Die beiden sendenden Tests weigern sich, gegen ein Prefix ohne `test` im Namen
zu laufen.

## Belegtes Ergebnis (2026-08-07, ESP8266-Pruefstand)

```text
3.0.1   F1 6C 01 10 10 ...   Heatpump-Bits = 0, verloren
3.1.0   F1 6C 01 10 12 ...   Heatpump = 2, WaterPump = 1
```

Bemerkenswert: 3.0.1 quittiert das verschluckte Kommando im Log noch mit
`<SUB> SET1 Heatpump: 1` - deshalb fiel der Fehler jahrelang nicht auf.

Der Abnahmetest zeigt alle sechs Verteiler-Kanaele (Byte 4, 6, 38, 39, 45) in
einem einzigen 110-Byte-Telegramm.

Am selben Tag am Produktivgeraet WP1 (192.168.2.120) bestaetigt: `Byte 4 = 0x11`,
Heatpump und WaterPump gemeinsam. Vorgehen fuer so einen Nachweis an einer
laufenden Anlage: genau die Werte senden, die der Verteiler ohnehin gerade
kommandiert (hier Modus 0/AUS: Heatpump 0, WaterPump 0). Dann aendert sich am
Sollzustand nichts, und ein Fehlschlag korrigiert sich spaetestens mit dem
5-min-Re-Assert von selbst.

## Fallstrick beim Deuten eines Mitschnitts

Der Node-RED-Verteiler sendet **idempotent**: nur Kanaele, deren Wert sich
geaendert hat (`lastSent`-Filter in `syncOutputs`). Ein Telegramm mit nur
einem gesetzten Feld ist also normal und kein Fehler. Beim Wechsel von Modus 0
auf Modus 3 (Kuehlen) aendern sich zum Beispiel nur `heatpump` und
`pumpspeed` - `uwpmode`, `opmode` und `cooltarget` bleiben gleich und gehen
gar nicht erst raus.

Alle Kanaele auf einmal kommen erst beim zyklischen Re-Assert (alle 5 min,
dort wird `lastSent` geleert). Nur dort ist im Mitschnitt zu sehen, dass
Heatpump und WaterPump gemeinsam in Byte 4 stehen. `produktiv_mitschnitt.py`
hoert deshalb per Vorgabe 360 s mit statt beim ersten Kommando abzubrechen.

Beleg aus dem Normalbetrieb (2026-08-08, WP1 im Kuehlbetrieb, Re-Assert):

```text
F1 6C 01 10 12 ...   Heatpump an | WaterPump auto | OperationMode 19
                     Z1 Heat 20 C | Z1 Cool 20 C | PumpSpeed 150
```

## Fallstrick beim Hexlog-Parsen

Der Hexlog gibt nicht nur das gesendete Kommando aus, sondern auch die
**empfangenen** 203 Bytes der Waermepumpe (`HeishaMon.cpp`, `readSerial`). Ein
naiver Parser klebt beide zu 313 Bytes zusammen. `heisha_probe.py` schneidet
deshalb ab dem `F1`-Header hart nach 110 Bytes ab. Auf dem Pruefstand ohne WP
faellt das nicht auf, am Produktivgeraet sofort.

## Telegrammpruefung (telegramm_test.cpp, 3.6.0)

Bis 3.5.0 galt ein empfangenes Telegramm als vollstaendig, sobald die Anzahl
gelesener Bytes zum Laengenbyte passte - danach entschied allein die
8-Bit-Pruefsumme, also 1 von 256. Blieben nach einem Serial-Timeout Reste einer
abgebrochenen Antwort im UART-Puffer stehen, las der naechste Zyklus sie mit;
ein so verschobener Bytestrom konnte als Messdaten durchgehen und retained in
die Kaskadenregelung laufen.

`telegramm_test.cpp` bindet `src/telegram.h` direkt ein - es prueft also den
Code, der auf dem Geraet laeuft, nicht eine Nachbildung. Ergebnis:

```text
Fall 1  echtes Antworttelegramm (203 Bytes, 0x71/0xC8)      angenommen
Fall 2  1386 Varianten mit nachgezogener Pruefsumme          0 verworfen
Fall 3  111-Byte-Abfrageecho, Typ 0xF1, 204 Bytes,
        unvollstaendige Antwort, gekipptes Byte              alle abgewiesen
Fall 4  alle 202 Verschiebungen des Antworttelegramms        alle abgewiesen
Fall 5  200000 Zufallspuffer, Laenge passend zum Laengenbyte
        alte Regel 817 Annahmen (0,41 % = 1/256), neue Regel 0
```

Fall 2 ist die wichtigere Haelfte: Die Pruefung darf kein gueltiges Telegramm
wegen seines Inhalts verwerfen, sonst stuende die Anlage still.

## Ist-Zustaende aus Byte 110 (byte110_test.cpp, 3.7.0)

Byte 110 traegt vier 2-Bit-Felder mit den TATSAECHLICHEN Zustaenden der
Waermepumpe (TOP99-TOP102). Bei vier Topics auf EINEM Byte ist der
naheliegende Fehler, dass ein Topic die Bits eines anderen liest - das faellt
im Betrieb nur auf, wenn beide Felder gerade unterschiedlich stehen.

Der Test uebersetzt `src/decode.cpp` mit und ruft `getTopicPayload()` auf,
prueft also den Code, der auf dem Geraet laeuft. Die Erwartung wird unabhaengig
vom Dekodierer aus dem Rohwert gerechnet.

```text
Fall 1  vier Zeilen in stateTopics[], TOP99-102, Quellbyte 110
Fall 2  je 256 Rohwerte gegen die erwartete Bitgruppe      0 Abweichungen
Fall 3  belegte Zustaende: 0x55 Grundzustand (= Byte 110 im
        Antwortbeispiel), 0x95 Quiet an, 0x59 Kuehlen,
        0x69 Powerful an beim Kuehlen
Fall 4  Anzeigeindex bleibt in -1..2, desc[2] = "unknown"
Fall 5  keine doppelten TOP-Nummern oder Topic-Namen in der ganzen Tabelle
```

Fall 4 ist der Grund fuer das dritte Array-Element: Die Web-Tabelle
(`webfunctions.cpp`) faengt nur negative Indizes ab, und ein 2-Bit-Feld kann
`b11` liefern - das ergibt Index 2. Bei Byte 110 ist das kein theoretischer
Fall, weil `External_SW_State` an dieser Anlage dauerhaft unbelegt bleibt - der
External-SW-Eingang ist hier nicht angeschlossen, das Feld ist also nicht
pruefbar (`Powerful_Mode_Active` ist seit dem 2026-08-16 in beiden Zustaenden
belegt). Benutzt wird hier der externe Kompressor-Schalter; fuer ihn war beim
Betaetigen in keinem der 203 Bytes eine Reaktion zu sehen (`frame_diff.py`),
ein Statusbyte dafuer ist bisher nicht gefunden.

Gegenprobe zum Test selbst: mit vertauschter Bitgruppe (`getBit3and4` statt
`getBit1and2` bei TOP99) meldet Fall 2 192 Abweichungen und der Lauf bricht ab.

Der Baurahmen ist ein Skript, weil `decode.cpp` mit `#include "HeishaMon.h"`
beginnt und ein Include in Anfuehrungszeichen immer zuerst im Verzeichnis der
einbindenden Datei sucht: aus `src/` heraus gewinnt der echte Header und zieht
LittleFS, WiFi und den Rest der Arduino-Welt nach. `decode_hosttest.sh` kopiert
die Uebersetzungseinheit deshalb neben die Ersatzheader aus `stubs/`.

## Abnahme nach dem Flashen (tablesnap.py, 3.9.0)

Die Projektkonvention nach jedem OTA: Baseline der Topic-Tabelle vor dem Flash
ziehen, nach dem Flash noch einmal, und beide zeilenweise halten. Bis 3.9.0 ging
das per Augenschein ueber die Weboberflaeche - bei 90 Zeilen je Stufe ist das
muehsam und uebersieht leicht etwas.

`tablesnap.py` holt `/tablerefresh` und gibt je Zeile `TOP<n>|Name|Wert|Klartext`
aus. Damit ist die Abnahme ein `diff`:

```
./tablesnap.py 192.168.2.120 > vorher.txt
pio run -e heishamon_esp32_h1_ota -t upload
./tablesnap.py 192.168.2.120 > nachher.txt
diff vorher.txt nachher.txt
```

Was uebrig bleibt, ist entweder ein laufender Messwert oder ein Befund. Reines
GET, kein Eingriff ins Geraet. Beim Rollout von 3.9.0 am 2026-08-19 blieb Stufe 1
vollstaendig ohne Abweichung, Stufe 2 mit einer einzigen (`Eva_Outlet_Temp`
21 -> 22 Grad).

Zwei Hinweise: Nach dem Neustart braucht das Geraet ein paar Abfragezyklen, bis
die Tabelle wieder gefuellt ist - erst schnappen, wenn keine Zeile mehr leer
oder `unused` ist. Und die Baseline gehoert unmittelbar vor den Flash gezogen,
sonst wandern in der Zwischenzeit Messwerte und der Diff wird unuebersichtlich.

## SET-TOP-Zuordnung (set_top_zuordnung.py, 3.9.0)

Erzeugt die Tabellen in `SET-TOP-Zuordnung.md` - welches State-Topic liest ein
Set-Kommando zurueck, und wo gibt es keins. Liest nur den Quelltext, kein
Geraeteeingriff:

```
./set_top_zuordnung.py --pruefen     # Zusammenfassung
./set_top_zuordnung.py               # alle Tabellen als Markdown
```

Zugeordnet wird ueber **Byte-Position und Bitmaske, nie ueber Namen**. Die
tatsaechlich beschriebene Maske eines Kommandos entsteht aus seinem
Wertebereich: fuer jeden erlaubten Wert das Protokollbyte bilden, alle
gesetzten Bits verodern. Ohne diesen Schritt landet `QuietMode` beim falschen
Topic - seine Tabellenmaske ist `0xFF`, belegt sind aber nur die Bits 3-5, und
Byte 7 traegt drei Topics.

Zwei Dinge, die dabei herauskommen und ohne die Rechnung nicht auffallen:
`PowerfulMode` schreibt Quiet-Stufe und Quiet-Zeitprogramm mit auf Off (seine
Werte `0x49`-`0x4C` setzen deren Bits immer mit), und nach `set/OperationMode 2`
meldet TOP4 nie den geschriebenen Wert zurueck, sondern 2 **oder** 7 - die WP
legt die Richtung selbst fest.

Nach jeder Aenderung an `setCommands[]` oder `stateTopics[]` laufen lassen und
die Ausgabe gegen die Doku halten.

**Beim Nachschlagen in `ProtocolByteDecrypt.md`:** Die Zahl in der ersten
Spalte ist eine Topic-Nummer des *Original*-Projekts, keine Byte-Position.
Wo eine Zuordnung zweifelhaft ist, entscheidet die Messung - `byte_monitor.py`,
siehe naechster Abschnitt.

## Byte-Zuordnung belegen (byte_monitor.py, 3.9.0)

Ein Byte beobachten, den zugehoerigen Wert aendern, die Flanke ansehen. Damit
wird aus einer abgeleiteten Zuordnung eine gemessene. Das Werkzeug schaltet den
Hexlog per Telnet ein, schneidet die 203-Byte-Antworten mit, rechnet die
gewuenschten Bytes in alle im Protokoll ueblichen Formen um und schaltet den
Hexlog danach wieder ab - auch dann, wenn es mit einem Fehler abbricht.

```
./byte_monitor.py 192.168.2.120 4 45 95 --dauer 20
```

**Ablauf fuer einen Aenderungsnachweis** (Beispiel SET15, 2026-08-19):

```
./byte_monitor.py 192.168.2.120 45 95                      # Ausgangslage
./mqtt_pub.py --host 192.168.2.147 panasonic_heat_pump/set/WaterPumpSpeed=110
./byte_monitor.py 192.168.2.120 45 95                      # geaendert
./mqtt_pub.py --host 192.168.2.147 panasonic_heat_pump/set/WaterPumpSpeed=100
./byte_monitor.py 192.168.2.120 45 95                      # zurueck
```

Ergebnis: Byte 45 ging `0x65` -> `0x6F` -> `0x65`, also `X-1` = 100 -> 110 ->
100 im Gleichschritt mit dem Kommando; Byte 95 blieb ueber alle Messungen auf
`0x94` (`X-128` = 20). Die Flanke stand jeweils **innerhalb** eines
Mitschnitts - ein Telegramm mit dem alten, die folgenden mit dem neuen Wert;
die Zeile "Waehrend des Mitschnitts veraendert" weist genau darauf hin.

Die zweite Kontrolle kostet gar nichts: Die beiden Kaskadenstufen haben hier
verschiedene Werte konfiguriert (100 und 125), und Byte 45 zeigte an Stufe 2
`0x7E` = 125. Wo zwei Stufen unterschiedlich eingestellt sind, ist der
Stufenvergleich der billigste Nachweis - ganz ohne Schreibvorgang.

**Byte 4, Bits 3+4 (SET14 WaterPump)** am selben Tag an H1 gemessen, bei
stehender Waermepumpe: `set/WaterPump` 0 -> 1 -> 0 liess Byte 4 von `0x55` auf
`0x65` und zurueck gehen, die Bits 3+4 also von `b01` (Auto) auf `b10` (On).
Der Wert `0x65` deckt sich mit der Referenz. Dass die Pumpe dabei wirklich
anlief, zeigen TOP65 (2300 1/min), TOP1 (11,95 l/min) und TOP92 - das Bitfeld
meldet den wirksamen Zustand, nicht nur den Wunsch. `Air purge` (`b11`) ist
bewusst NICHT gemessen: Das haette an einer intakten Anlage eine
Entlueftungsroutine ausgeloest.

**Die Bedeutung von Byte 45** faellt bei laufender Pumpe gleich mit ab. Mit
eingeschalteter Pumpe die Grenze verstellen:

```
  Byte 45 (X-1)   TOP92 Pump_Duty   TOP65 Pump_Speed   TOP1 Pump_Flow
            100               100         2300 1/min      11,95 l/min
             80                80         1500 1/min       6,93 l/min
```

Die Pumpe laeuft im Handbetrieb genau bis zur konfigurierten Obergrenze. Byte
45 ist damit als **maximaler Duty** belegt, nicht als Drehzahl - der Topic-Name
`WaterPumpSpeed` fuehrt in die Irre, das Rueckleseziel heisst `Pump_Duty_Max`.

Nach jedem solchen Test die Sollwerte zuruecksetzen und nachsehen, dass die
Anlage wieder steht (`Pump_Speed` und `Pump_Duty` auf 0). Node-RED zieht seinen
5-min-Re-Assert zwar ohnehin nach, aber darauf ist kein Nachweis zu bauen.

Aus diesen Messungen sind in 3.10.0 die Topics **TOP103 `Pump_Duty_Max`** und
**TOP104 `Water_Pump_Mode`** geworden - die Rueckmeldung zu SET15 und SET14.
Damit hat jedes Set-Kommando, fuer das es ueberhaupt ein Antwortbyte gibt, eine
Rueckmeldung; Byte 8 (ForceDefrost, ForceSterilization) traegt im
Antworttelegramm nichts.

Zwei Hinweise: Der Hexlog haengt am Telnet-Debugflag (`write_hex_log` schreibt
ueber `write_telnet_log`) - steht `outputTelnetLog` auf false, kommt nichts an.
Und `H` ist ein Umschalter: War der Hexlog schon an, schaltet der erste Druck
ihn aus. Das Skript erkennt das und nimmt es zurueck.

## Verhaeltnis zu `pio test`

Das sind eigenstaendige Diagnosewerkzeuge, keine Unity-Testsuites - `pio test`
nutzt sie nicht. Echte Unit-Tests fuer Encoder, Decoder und Merge (Schritt 5
des Umbauplans) waeren der naechste Ausbau; `merge_test.cpp`,
`telegramm_test.cpp` und `sendwindow_test.cpp` sind die Vorlage dafuer - alle
brechen mit Rueckgabewert 1 ab, wenn eine Zusicherung bricht, und laufen so in
der CI.

## Zeitregeln des Sendepfads (sendwindow_test.cpp, 3.8.0)

Dasselbe Muster wie bei der Telegrammpruefung: Die Regeln stehen in
`src/sendwindow.h`, Firmware und Hosttest binden dieselbe Datei ein. Geprueft
werden der Deckel des Sammelfensters (`COMMAND_WINDOW_MAX`) und die Grenze
fuers Verschieben des Sendens (`COMMAND_DEFER_MAX`).

Der Test deckt drei Dinge ab, die sich sonst nirgends belegen liessen:

* **Terminierung** unter SET-Stroemen mit 1, 100 und 400 ms Abstand. Bis 3.7.0
  stiess jedes SET den 500-ms-Timer neu an - ein Strom dichter als 500 ms hielt
  Senden und Abfrage unbegrenzt an.
* **Die zugesagte Obergrenze** ueber jeden Abstand von 1 bis 3000 ms. Sie
  betraegt 2499 ms und nicht 2500: der letzte Anstoss kann hoechstens bei
  `COMMAND_WINDOW_MAX - 1` liegen, weil auf dem Deckel selbst nicht mehr
  verlaengert wird.
* **Den `millis()`-Ueberlauf nach 49,7 Tagen.** Das Geraet laeuft monatelang
  durch, der Zaehler laeuft im Betrieb also wirklich ueber - abwarten liesse
  sich das nicht. Eine Gegenprobe im Test zeigt, dass die naive Schreibweise
  `now < start + limit` genau an dieser Naht falsch liegt.

Deshalb rechnen die Regeln in `uint32_t` und nicht in `unsigned long`: auf dem
ESP32 ist beides 32 Bit, auf dem Mac waere `unsigned long` 64 Bit und der
Ueberlauftest wuerde gegen nichts pruefen.

Gegenprobe gemacht: mit ungedeckeltem Fenster faellt der Test mit 10
Abweichungen durch. **Was er nicht abdeckt**, sind die Zustandsuebergaenge in
`HeishaMon.cpp` selbst (Ticker, Serial, Flags) - die sind ohne die halbe
Arduino-Welt nicht uebersetzbar und bleiben Sache des Abnahmetests.

## Verbindungswacht (verbindung_test.cpp, 3.13.0)

Dasselbe Muster: Die Regeln stehen in `src/verbindung.h`, Firmware und Hosttest
binden dieselbe Datei ein. Geprueft wird, seit wann die Verbindung zur
Hausteuerung weg ist und wie die Weboberflaeche das formuliert.

Der Anlass steht im Vorhaben: Waehrend eines Broker-Ausfalls am 2026-08-21 heizte
die Waermepumpe mit dem zuletzt gesetzten Sollwert einfach weiter, und die
Oberflaeche zeigte davon nichts. Vier Dinge muessen stimmen, damit die Anzeige
nicht schlimmer ist als keine - und alle vier sind am Geraet schlecht bis gar
nicht nachweisbar:

* **Die Karenz von 5 Minuten**, auf die Sekunde. Geprueft wird die Grenze
  selbst: eine Sekunde davor darf nichts stehen, auf der Grenze muss die
  Meldung da sein. Ein Test, der nur "nach 10 Minuten steht es da" prueft,
  wuerde eine um Faktor zwei falsche Karenz nicht bemerken.
* **Der Sonderfall "seit dem Neustart nie verbunden"** muss sich vom normalen
  Ausfall unterscheiden lassen. Dort ist die wahre Dauer unbekannt: Der Broker
  kann seit Tagen weg sein, das Geraet ist nur gerade neu gestartet.
* **Der `millis()`-Ueberlauf nach 49,7 Tagen.** Der Test trifft dabei bewusst
  einen boesartigen Zeitpunkt - eine Ausfalldauer knapp ueber der Naht, an der
  die naive Differenz UNTER der Karenz liegt. Nur so ist belegt, dass die
  Stoermeldung dort nicht verschwindet, statt dass der Fall bloss zufaellig
  nicht auftrat.
* **Die Textform** ("14 Minuten", "1 Stunde", "2 Tagen", "mehr als 30 Tagen")
  samt der Schwellen: Bis 89 Minuten wird in Minuten gezaehlt, bis 47 Stunden
  in Stunden - "seit 90 Minuten" ist die genauere Auskunft als "seit 1 Stunde".

Dazu die Regeln des **Herzschlags** (der zweite Ausfall: Broker da, aber die
Kaskadenregelung rechnet nicht mehr):

* **Die Stumm-Karenz von 12 Minuten**, ebenfalls auf die Sekunde. Der Re-Assert
  kommt alle 300,0 s (gemessen, siehe Abschnitt darueber) - zwoelf Minuten
  decken zwei verpasste Takte samt Reserve ab.
* **Der Vorrang**: Ist der Broker weg, gilt der Broker-Ausfall. Beides zu melden
  wuerde jemanden zum Server schicken, um dort nach dem falschen Fehler zu
  suchen.
* **Die Stumm-Uhr steht still, solange der Broker weg ist**, und startet mit dem
  Verbindungsaufbau neu. Ohne diese Regel meldete die Seite unmittelbar nach der
  Rueckkehr des Brokers sofort einen zweiten Fehler, den es nie gab.

95 Zusicherungen. Beide Uhren teilen sich denselben Kern (`struct Ausfall`) -
die Ueberlauffestigkeit ist der subtile Teil, und zweimal hingeschrieben waere
zweimal Gelegenheit, sie falsch zu machen.

**Gegenproben gemacht, fuenfmal:**

| Regel gebrochen | Abweichungen |
|:--- |:--- |
| Karenz auf 1 Minute verstellt | 6 |
| Dauer gerechnet statt fortgeschrieben | 6 |
| Stumm-Uhr laeuft ohne Verbindung weiter | 6 |
| Stumm-Karenz auf 4 Minuten (unter einem Takt) | 7 |
| Vorrang in `verbindung_lage()` umgedreht | 1 |

Zwei davon sind es wert, einzeln genannt zu werden:

* Bei **gerechneter statt fortgeschriebener Dauer** lautet der Text nach 49,7
  Tagen Ausfall **"1 Minute"**. Genau diese Falschauskunft verhindert der Deckel
  bei 30 Tagen: Sie waere schlimmer als gar keine Angabe, weil sie ausgerechnet
  nach einem sehr langen Ausfall "alles in Ordnung" behauptet.
* Die Gegenprobe zum **Vorrang bestand zunaechst** - und das war der
  aufschlussreiche Fall. Der Vorrang in `verbindung_lage()` war gar nicht
  geprueft, weil beide Uhren im Betrieb nie gleichzeitig laufen (die
  Stumm-Uhr wird beim Verbindungsverlust zurueckgesetzt). Die Reihenfolge steht
  als zweite Sicherung weiter da, fuer den Fall dass jemand das Zuruecksetzen
  spaeter entfernt; damit sie eine geprueft ist und keine geglaubte, baut der
  Test den unmoeglichen Zustand jetzt von Hand.

Beim ersten Lauf standen 11 Abweichungen - und vier davon lagen an **den
Zusicherungen**, nicht am Header: Der Verlustmoment kostet einen
Nachfuehrschritt (bei 1-s-Schritten also genau eine Sekunde), "1 Tag" kann mit
den Schwellen gar nicht entstehen, und 30 Tage liegen NICHT unter der halben
`millis()`-Breite von 24,85 Tagen. Das muessen sie auch nicht - die
unsigned-Differenz ist bis zur vollen Naht bei 49,7 Tagen eindeutig, und
zwischen Deckel und Naht bleiben 19 Tage, in denen loop() die Ueberschreitung
bemerkt. Das ist der eigentliche Wert des Musters: Es zwingt dazu, die Zusage
auszurechnen statt sie zu schaetzen.

**Was der Test nicht abdeckt:** die Anbindung in `HeishaMon.cpp`
(`mqtt_client.connected()` als Eingang der Wacht, der Aufruf von
`verbindung_set_empfangen()` in `mqtt_callback`) und die Anzeige selbst. Beides
gehoert in den Abnahmetest.

**Merke zur Platzierung des Herzschlags:** Der Aufruf steht in `mqtt_callback()`
**nach** der `SUBSCRIBE_GRACE`-Pruefung. Der Grund ist genau die
Wiedereinspielung, wegen der es die Karenzzeit ueberhaupt gibt: Der
ioBroker-Adapter schickt jedem neuen Abonnenten die gespeicherten Set-Werte -
auch wenn Node-RED laengst tot ist. Dieser Schwall belegt nur, dass der Broker
lebt, und den beobachtet bereits die andere Uhr. Vor der Pruefung gestempelt,
verstummte die Meldung nach jedem Reconnect fuer zwoelf Minuten.

## Der 5-min-Re-Assert kommt bei BEIDEN Stufen an (2026-08-21, H2)

Vorarbeit fuer den Herzschlag (Vorhaben-Notbetrieb-Weboberflaeche.md,
Abschnitt 11): Kann die Firmware am ausbleibenden `set`-Verkehr erkennen, dass
die Kaskadenregelung nicht mehr rechnet? Dafuer muss sie im Normalbetrieb
zuverlaessig Verkehr sehen - und zwar auch die Warmwasserstufe, die keine
Sollwertkurve bekommt.

Zwei Fragen, beide beantwortet:

1. **Schickt der Verteiler ueberhaupt in jedem Takt?** Im Flow-Code nachgesehen
   (`Hauptmodus-Verteiler V6.5`, §6): Er sendet je Kanal nur bei Aenderung
   gegenueber `lastSent` - aber der 5-Minuten-Takt setzt `state.lastSent = {}`
   zurueck. Danach gelten alle dreizehn Kanaele als geaendert, sechs davon
   gehen an H2.
2. **Publiziert der ioBroker-Adapter einen unveraenderten Wert auch?** Das
   entscheidet der Mitschnitt, nicht der Flow-Code. Passiv, 400 s, nichts
   gesendet:

```bash
# roher Telnet-Mitschnitt, zaehlt "Callback from mqtt" (write_telnet_log in
# mqtt_callback, geht IMMER ins Telnet-Log - unabhaengig von der L-Taste)
python3 - <<'EOF'
import socket, time
s = socket.create_connection(("192.168.2.122", 23), timeout=10); s.settimeout(2.0)
start = time.time(); puffer = b""
while time.time() - start < 400:
    try: puffer += s.recv(4096)
    except socket.timeout: continue
    while b"\n" in puffer:
        z, puffer = puffer.split(b"\n", 1)
        txt = z.decode("utf-8", "replace").strip()
        if "Callback from mqtt" in txt: print(f"{time.time()-start:6.1f}s {txt[:80]}")
EOF
```

Ergebnis:

```
13:40:46  6 x "Callback from mqtt" innerhalb von 0,1 s
13:40:57  1 x "Callback from mqtt"
13:45:46  6 x "Callback from mqtt" innerhalb von 0,1 s
13:45:57  1 x "Callback from mqtt"
```

**Taktabstand exakt 300,0 s, je Takt sieben empfangene Kommandos.** An den
Sollwerten hatte sich zwischen den beiden Takten nichts geaendert - der zweite
Takt belegt damit, dass der Adapter auch unveraenderte Werte publiziert. Die
sechs im Schwall sind die WP2-Kanaele des Verteilers, der siebte zehn Sekunden
spaeter kommt aus der Waechter-Logik mit ihrem eigenen Takt (`QuietMode`).

Der Mitschnitt ist **vollstaendig passiv** und braucht kein Testfenster: Er
liest nur den Telnet-Strom und sendet nichts, auch keine Umschalttasten.

## Verbindungsanzeige am Pruefstand (2026-08-21, 3.13.0)

Der Abnahmetest zu 3.13.0 - und der Beleg dafuer, dass er noetig war: Er hat
einen Fehler gefunden, den kein Hosttest finden konnte.

**Der Pruefstand ist fuer DIESE Funktion wieder taugliches Werkzeug.** Fuer den
Notbetriebsknopf war er seit der Sperre ausgeschieden (ohne Waermepumpe kein
TOP101, also bleibt der Knopf gesperrt) - eine Verbindungsanzeige braucht kein
TOP101. Der ganze Nachweis lief ohne Eingriff an H1/H2 und ohne Testfenster.

### Zwei Kniffe, ohne die es nicht gegangen waere

**1. Der Pruefstand wird von allein stumm.** Er laeuft unter dem Prefix
`panasonic_heat_pump_test`, und dorthin sendet der Hauptmodus-Verteiler nichts.
Die Lage 4 ("Steuerung stumm") stellt sich also von selbst ein - der
Node-RED-Container musste nicht angehalten werden.

**2. Fuer den Broker-Ausfall braucht es einen eigenen Broker.** Ueber
`/settings` laesst sich die Serveradresse zwar auf eine tote IP stellen, aber
**jede Aenderung dort startet das Geraet neu** - und danach ist die Lage immer
3 ("seit dem Neustart nie verbunden"), nie 2. Fuer Lage 2 muss eine BESTEHENDE
Verbindung abreissen. Den ioBroker der Anlage dafuer anzufassen waere das
falsche Mittel; stattdessen lief ein minimaler MQTT-Broker auf dem
Arbeitsrechner (rund 200 Zeilen, nur CONNECT/CONNACK, SUBSCRIBE/SUBACK,
PUBLISH, PINGREQ/PINGRESP - mehr braucht PubSubClient nicht), auf den der
Pruefstand fuer die Dauer des Nachweises umgestellt wurde. Als Zugabe zeigt er
die Logzeilen der Firmware direkt an.

### Was gemessen wurde

Zeit | Pruefung | Erwartet | Gemessen
:--- | :--- | :--- | :---
14:23:38 | Stumm-Karenz, 1. Lauf | 14:23:29 | im 20-s-Fenster getroffen
14:39:26 | Stumm-Karenz, 2. Lauf | 14:39:23 | getroffen
14:39:49 | Rueckkehr der Vorgaben | Lage 0 + Logzeile | "hat 745 s keine Vorgaben gesendet"
14:41:05 | Broker gekappt | Lage 1, keine Meldung | ok
14:46:06 | Broker-Karenz 5 min | 14:46:06 | punktgenau
14:55:07 | Vorrang | nach 14 min ohne Broker Lage 2 | `...;2;14 Minuten`
14:55:09 | Rueckkehr des Brokers | Logzeile, Lage 0, keine Stumm-Meldung | "war 850 s nicht erreichbar"
15:00:58 | Lage 3 | Text ohne Minutenzahl, Dauertext leer | ok

### Der Fehler, den der Lauf gefunden hat

Das erste Kommando an den Pruefstand ging verloren:

```
14:24:14  Error: Unknown set topic 0Q
```

Aus `panasonic_heat_pump_test/set/QuietMode` war `0Q` geworden.

**`write_mqtt_log()` ruft `mqtt_client.publish()`, und PubSubClient benutzt fuer
Senden und Empfangen DENSELBEN Puffer.** Genau dorthin zeigen `topic` und
`payload` waehrend des Callbacks. Etappe B setzte die Herzschlag-Meldung mitten
in der Auswertung ab und ueberschrieb damit den Topic-Namen.

**Merke fuer kuenftige Aenderungen an `mqtt_callback()`: dort NICHT loggen.**
Der bestehende Code haelt sich daran - `write_mqtt_log()` steht nur an Stellen,
an denen `topic` und `msg` nicht mehr gebraucht werden. Wer eine Meldung
braucht, merkt sich den Wert und gibt ihn aus `loop()` aus (Muster:
`wifiOutageSeconds`, seit 3.13.0 auch `stilleBeendetSekunden`).

### Nebenbefund: 34 wiedereingespielte Kommandos bei JEDEM Verbinden

```
14:11:34  34 wiedereingespielte Set-Kommandos nach dem Verbinden verworfen
14:27:19  34 wiedereingespielte Set-Kommandos nach dem Verbinden verworfen
```

Der ioBroker-Adapter spielt das dem Pruefstand ein, obwohl unter diesem Prefix
**niemand steuert**. Das ist der gemessene Beleg dafuer, warum der Herzschlag
NACH der `SUBSCRIBE_GRACE`-Pruefung gestempelt wird: Davor haetten diese 34
Nachrichten als "die Steuerung lebt" gezaehlt.

## Kurven-Set-Kommandos (SET27-SET34)

`kurven_test.py` weist die acht Kurvenbefehle am laufenden Geraet nach, ohne
etwas zu verstellen: Es liest die aktuellen Kurvenwerte aus den state-Topics
und schreibt genau diese zurueck. Geprueft wird, ob alle acht korrekt codiert
(Wert+128) im Telegramm landen - und ob sie gemeinsam in einem Telegramm
ankommen.

```bash
./kurven_test.py --esp 192.168.2.120 --prefix panasonic_heat_pump
./kurven_test.py --esp 192.168.2.193 --prefix panasonic_heat_pump2
```

Belegt am 2026-08-10 auf beiden Stufen: acht Werte, ein Telegramm, Bytes
75-78 und 86-89 korrekt, danach alle state-Werte unveraendert. Die
Bereichspruefung greift ebenfalls - ein Wert unter der Grenze wird abgelehnt:

```text
Error: Value 10 out of range [15..35] for topic Z1HeatCurveOutsideHighTemp
```

## Grenzwerte der Waermepumpe (kurven_grenzen.py)

Die in Umlauf befindlichen Wertebereiche der Kurvenparameter stimmen fuer
diese Geraete nicht durchgaengig. `kurven_grenzen.py` prueft je Parameter die
beiden Raender des in `commands.cpp` hinterlegten Bereichs: setzen, warten,
ueber das state-Topic zurueckvergleichen, am Ende Ausgangswerte wiederherstellen.

Nur laufen lassen, wenn die Anlage NICHT im Kurvenbetrieb faehrt.

```bash
./kurven_grenzen.py --esp 192.168.2.120 --prefix panasonic_heat_pump
```

Messung 2026-08-10 an WP1 (Panasonic WH-MDC05H3E5, 5 kW Monoblock):

| Parameter | Etikett | gemessener Bereich | Anmerkung |
| --- | --- | --- | --- |
| Z1HeatCurveTargetHighTemp | VL kalt | 20 .. 55 | = Vorlauf-Sollwert, s. Abschnitt unten |
| Z1HeatCurveTargetLowTemp | VL warm | 20 .. 55 | haltbar |
| Z1HeatCurveOutsideLowTemp | AT kalt | -15 .. 15 | haltbar |
| Z1HeatCurveOutsideHighTemp | AT warm | **-15 .. 15** | frueher 15..35 angenommen - falsche Seite |
| Z1CoolCurveTargetHighTemp | VL kuehl | 5 .. 20 | = Vorlauf-Sollwert, s. Abschnitt unten |
| Z1CoolCurveTargetLowTemp | VL heiss | 5 .. 20 | haltbar |
| Z1CoolCurveOutsideLowTemp | AT kuehl | 15 .. 30 | Untergrenze berichtigt 2026-08-25, s. u. |
| Z1CoolCurveOutsideHighTemp | AT heiss | **15 .. 30** | frueher 20..30 bzw. 30..40 angenommen |

Die beiden fett markierten Bereiche wurden mit einer Firmware ausgemessen,
deren Grenzen zum Test geweitet waren:

```text
Heat Outside_High:  -20 -> -15    -15/-5/5/10/12/14/15 ok    20/25/30/35 -> 15
Cool Outside_High:   10 ->  15     15/20/30 ok               31/32/35/40 -> 30
```

Der Heizwert ist der lehrreiche Fall: Gueltig ist alles BIS 15, nicht AB 15.
Von den 21 Werten des frueheren Bereichs war genau einer gueltig - das fiel nur
auf, weil die Anlagenkonfiguration zufaellig exakt diesen einen nutzt.

Wichtig zur Deutung dieser Messungen: Die WP klemmt beim Schreiben sofort, ein
Rueckvergleich nach ~15 s reicht dafuer. Die Kopplung von TargetHigh an den
Sollwert (naechster Abschnitt) wirkt dagegen VERZOEGERT ueber den 5-min-
Re-Assert - wer nur 15 s misst, haelt einen Wert faelschlich fuer stabil.

### Gegenprobe am Bedienterminal (2026-08-11, WP2)

Die Tabelle oben ist per MQTT ausgemessen - sie zeigt, was die WP annimmt und
was sie klemmt. Das Bedienterminal nennt die Bereiche selbst, und zwar in den
Kurvendialogen (duenn gesetzte Zahlen an den Achsenenden) und in den
Direktwert-Dialogen (Zeile `Bereich:`). Beides stimmt mit der Tabelle ueberein:

| Anzeige am Terminal | genannter Bereich | Tabelle oben |
| --- | --- | --- |
| Heizbetr. Wassertemp, Y-Achse und `Bereich:` | 20 .. 55 | passt |
| Heizbetr. Wassertemp, X-Achse | -15 .. 15 | passt |
| Kuehlbetr. Wassertemp, Y-Achse und `Bereich:` | 5 .. 20 | passt |
| Kuehlbetr. Wassertemp, X-Achse | 15 .. 30 | passt |

Belege in `pictures/`: `IMG_4887` (Heizkurve), `IMG_4889` (Kuehlkurve),
`IMG_4894` / `IMG_4892` (Direktwert-Dialoge heizen/kuehlen). Ein zweites
Foto der Kuehlkurve mit auf 15 C gesetztem unteren Punkt:
`Kuehlen_Kurve.png` (2026-08-25).

Schrittweite laut Terminal ±1 Grad, wie in `commands.cpp` angenommen. Damit
sind besonders die beiden fett markierten Korrekturen aus 3.2.x vom Geraet
selbst bestaetigt: `Z1HeatCurveOutsideHighTemp` reicht BIS 15, und
`Z1CoolCurveOutsideHighTemp` beginnt bei 15.

### Die letzte Unschaerfe aufgeloest (2026-08-25, WP2)

Offen geblieben war ein Widerspruch: Die Kuehl-X-Achse spannt am Terminal
15 .. 30 auf, waehrend fuer `Z1CoolCurveOutsideLowTemp` 20 .. 30 hinterlegt
war. Die Vermutung damals lautete, die Achse zeige nur den weiteren der
beiden Punkte, und die Klemm-Messung mit ihrer Untergrenze 20 habe recht.

**Sie hatte nicht recht.** Am Bedienterminal laesst sich der untere Punkt der
Kuehlkurve auf 15 C stellen, die WP nimmt ihn an und zeigt ihn an
(`pictures/Kuehlen_Kurve.png`). Der Bereich lautet also 15 .. 30 wie die
Achse, nicht 20 .. 30 wie die Messung.

Die Messung vom 2026-08-10 hat den Fehler nicht verursacht, sondern nur nicht
aufgedeckt: Sie prueft von der hinterlegten Grenze aus nach aussen und kann
einen zu eng gesetzten Bereich deshalb gar nicht finden - unterhalb von 20
wurde nie geschrieben. Wer eine Untergrenze bestaetigen will, muss unter ihr
ansetzen, nicht auf ihr.

Bis 3.14.1 wies die Firmware 15 selbst ab, bevor das Kommando zur WP ging:

```text
Error: Value 15 out of range [20..30] for topic Z1CoolCurveOutsideLowTemp
```

Seit 3.14.2 steht 15 als Untergrenze in `commands.cpp`, in `kurven_sync.py`
und in `kurven_grenzen.py`. **Gegenprobe ueber den Set-Pfad an WP1 nach dem
Rollout** (2026-08-25, Firmware 3.14.2, Kuehlkreis auf Direktvorgabe): `SET33 =
15` gesendet, TOP75 meldet 15 zurueck, im Telnet-Mitschnitt keine Fehlerzeile.
Danach auf den Ausgangswert 20 zurueckgestellt. Lehre fuer kuenftige Bereichsmessungen: Die
Angaben des Bedienterminals schlagen die Klemm-Messung, wenn beide
auseinandergehen - das Terminal nennt den erlaubten Bereich, die Messung nur
das Verhalten innerhalb des angenommenen.

## Betriebsart Kurve/Direkt schalten (Byte 28, 2026-08-19, WP1)

Bis 3.10.0 war der Notbetrieb halb automatisiert: Die Kurvenwerte werden
laufend gespiegelt (`kurven_sync.py`), aber das Umschalten von Direkt- auf
Kurvenbetrieb musste ein Mensch am Bedienterminal machen. Offen war die eine
Frage, ob die WP Byte 28 im Kommandotelegramm ueberhaupt annimmt - das
Original-Projekt hat dafuer kein Kommando, es gab also keine Fremderfahrung.

**Sie nimmt es an.** Vier Laeufe an Stufe 1, alle bei stehender Anlage
(`Heatpump_State` 0, `Compressor_Freq` 0), Firmware 3.11.0:

```
./test/byte_monitor.py 192.168.2.120 28 --dauer 60     # im Hintergrund
./test/mqtt_pub.py --host 192.168.2.147 panasonic_heat_pump/set/CoolingMode=0
```

| Lauf | Betriebsmodus | Kommando | Byte 28 | Bits 5+6 | Bits 7+8 |
| ---: | --- | --- | --- | --- | --- |
| 1 | Heizen | `CoolingMode 0` | `0x0A` -> `0x06` | **10 -> 01** | 10 -> 10 |
| 2 | Kuehlen | `CoolingMode 0` | `0x0A` -> `0x06` | **10 -> 01** | 10 -> 10 |
| 3 | Heizen | `HeatingMode 0` | `0x0A` -> `0x09` | 10 -> 10 | **10 -> 01** |
| 4 | Heizen | **beide** `0` | `0x0A` -> `0x05` | **10 -> 01** | **10 -> 01** |

Jedes Mal wanderte das Byte im SELBEN Mitschnitt. In den Laeufen 1-3 blieb das
jeweilige Nachbarfeld stehen - **die Bitmaske greift also in beide Richtungen
bitgenau**, das war neben der Annahmefrage der zweite Punkt, der zu klaeren
war. Das Zurueckschalten stellte in allen vier Laeufen `0x0A` her, in Lauf 4
ebenfalls mit beiden Kommandos zusammen (`0x05` -> `0x0A`).

**Damit sind alle vier Rohwerte aus `ProtocolByteDecrypt.md` am Geraet
erzeugt.**

### Lauf 4: beide Kommandos im selben Sammelfenster

Das ist der Fall, den der Notbetrieb tatsaechlich fahren wuerde, und er war der
letzte ungemessene. Die Kaskadensteuerung sendet beide Kommandos aus derselben
Flow-Ausfuehrung; sie landen damit im selben 500-ms-Fenster (`COMMANDTIMER`,
verlaengerbar bis `COMMAND_WINDOW_MAX` 2000 ms) und werden zu EINEM Telegramm
zusammengefasst, in dem beide Bitfelder gleichzeitig einen Wechsel verlangen.
In den Laeufen 1-3 stand das jeweils andere Feld auf `00` = "keine Aenderung" -
diesen Fall hatte die WP also nie gesehen.

```
./test/mqtt_pub.py --host 192.168.2.147 \
    panasonic_heat_pump/set/HeatingMode=0 \
    panasonic_heat_pump/set/CoolingMode=0
```

`mqtt_pub.py` ist genau dafuer gebaut: mehrere Topics ueber EINE Verbindung
dicht hintereinander (hier 20 ms Abstand).

**Die WP nimmt beide Felder an.** TOP76 und TOP81 gingen zusammen auf 0. Die
Firmware-Seite war ohnehin belegt - `byte28_test.cpp` baut `0x05` aus denselben
zwei Merge-Aufrufen, und die Masken `0x03` und `0x0C` sind disjunkt, so dass
die Konfliktwarnung in `commands.cpp` nicht anschlaegt. Offen war allein, was
die WP mit einem doppelten Wechsel macht.

**Fuer das Notbetriebskonzept heisst das:** Umschalten geht in einem Rutsch,
die Kommandos muessen nicht zeitlich getrennt werden. TOP76/TOP81 sollten
trotzdem zurueckgelesen und bei Bedarf nachgelegt werden - in einer
Ausfallsituation ist "gesendet" nicht dasselbe wie "steht", und das Ruecklesen
kostet nichts.

### Was das Umschalten sonst noch anrichtet

Erwartet war der Kurven-Reset auf die Panasonic-Werksvorgaben (Beobachtung vom
2026-08-11 am Bedienterminal, weiter unten). Ueberraschend war, dass es den
jeweils NICHT geschalteten Kreis mittrifft:

| Wert | vorher | `CoolingMode 0` | `HeatingMode 0` | **beide 0** |
| --- | ---: | ---: | ---: | ---: |
| TOP76 `Heating_Mode` | 1 | 1 | **0** | **0** |
| TOP81 `Cooling_Mode` | 1 | **0** | 1 | **0** |
| TOP27 `Z1_Heat_Request_Temp` | 20 | **35** | **0** | **0** |
| TOP28 `Z1_Cool_Request_Temp` | 20 | **0** | **10** | **0** |
| TOP29 `Z1_Heat_Curve_Target_High_Temp` | 20 | **35** | **55** | **55** |
| TOP30 `Z1_Heat_Curve_Target_Low_Temp` | 34 | 34 | **35** | **35** |
| TOP32 `Z1_Heat_Curve_Outside_Low_Temp` | -10 | -10 | **-5** | **-5** |
| TOP72 `Z1_Cool_Curve_Target_High_Temp` | 20 | **15** | **10** | **15** |
| TOP73 `Z1_Cool_Curve_Target_Low_Temp` | 20 | **10** | 20 | **10** |

Die mittleren beiden Spalten sind spiegelbildlich, die letzte zeigt beide
Werkskurven in einer einzigen Momentaufnahme: **Heizkurve 55 C bei -5 C und
35 C bei +15 C**, **Kuehlkurve 15 C bei 20 C und 10 C bei 30 C**. Lauf 3 belegt
die Heizkurve vollstaendig inklusive des Aussenpunkts (TOP32 auf -5); beim
reinen Kuehl-Lauf konnten TOP74/TOP75 nichts zeigen, weil sie schon auf den
Werksvorgaben standen.

**Der Roundtrip-Verlust ist damit auch ueber den Kommandopfad belegt:** Nach dem
Zurueckschalten in Lauf 4 standen TOP27 auf 35 und TOP28 auf 10 - die Sollwerte
hatten die unteren Kurvenpunkte uebernommen. Genau die Beobachtung vom
2026-08-11 am Bedienterminal, diesmal fuer beide Kreise gleichzeitig.

**Der nicht geschaltete Kreis wird mitverstellt:** bei `CoolingMode 0` sprang
der HEIZ-Sollwert TOP27 auf 35, bei `HeatingMode 0` der KUEHL-Sollwert TOP28
auf 10 - jeweils auf den Werkswert des anderen Kreises, obwohl dessen
Betriebsart unveraendert auf Direkt stand.

**Der Betriebsmodus spielt dabei keine Rolle.** Nach Lauf 1 (Heizbetrieb) war
offen, ob die WP immer beide Kreise anfasst oder nur den gerade aktiven - der
mitgewanderte Sollwert war ja der des aktiven Modus. Lauf 2 im Kuehlbetrieb hat
das entschieden: TOP27 sprang wieder auf 35, obwohl der Heizkreis diesmal nicht
der aktive war. Es sind immer beide Kreise.

Der Kurvenbetrieb ist an TOP27/TOP28 zu erkennen: Der Sollwert des Kreises, der
auf Kurve steht, meldet **0**.

### Aufraeumen danach ist Pflicht, nicht Kosmetik

```
./test/kurven_sync.py --prefix panasonic_heat_pump --dry-run   # erst schauen
./test/kurven_sync.py --prefix panasonic_heat_pump
```

Die Kurve muss von Hand nachgezogen werden - sie ist nicht Teil des
Sollwert-Re-Asserts. Die SOLLWERTE dagegen holt sich die Kaskadensteuerung
selbst zurueck:

**Der 5-min-Re-Assert funktioniert.** In den Laeufen 2 und 3 war er im
Mitschnitt zu sehen: In Lauf 2 sendete Node-RED um 16:53:59 und wieder um
16:58:59, TOP27 ging um 16:59:06 von 35 auf 20 zurueck, ohne Zutun. In den Laeufen
3 und 4 standen die vier offenen Werte binnen zweier Minuten wieder richtig. Beim
ersten Lauf blieb er aus - dort war die Anlage nur unter Strom, Kompressor und
WP waren nicht freigegeben, es gab fuer die Kaskadensteuerung also nichts zu
tun. Das ist eine Eigenschaft dieses Anlagenzustands, kein Mangel des
Re-Asserts. Wer in einem solchen Zustand misst, muss die Sollwerte selbst
zuruecksetzen:

```
./test/mqtt_pub.py --host 192.168.2.147 \
    panasonic_heat_pump/set/Z1HeatRequestTemperature=20 \
    panasonic_heat_pump/set/Z1CoolRequestTemperature=20
```

In Lauf 4 holte der Re-Assert die Sollwerte binnen zweier Minuten zurueck und
damit auch die beiden TargetHigh-Werte, die sich mit ihnen eine Speicherstelle
teilen. Nach allen vier Laeufen stand der Ausgangszustand wieder vollstaendig -
alle 15 Werte, beide Betriebsarten und der Betriebsmodus.

## Heizstab schalten (Byte 9 und Byte 5, 2026-08-28, WP1)

Nachweis fuer SET37 `RoomHeaterState` und SET39 `ForceHeater` aus 3.17.0.
Anlage AUS, Modus Heizen, jeder Eingriff einzeln aufgerufen. Ablauf wie bei
Byte 28: Mitschnitt starten, ein Kommando senden, Flanke im laufenden
Mitschnitt ansehen, zurueckstellen.

```
./test/byte_monitor.py 192.168.2.120 9 --dauer 60     # im Hintergrund
./test/mqtt_pub.py --host 192.168.2.147 panasonic_heat_pump/set/RoomHeaterState=0
./test/tablesnap.py 192.168.2.120 | grep -E "TOP(58|59|68)\|"
```

Schritt | Kommando | Byte | Ruecklesen
:--- | :--- | :--- | :---
M0 | - | Byte 9 = `0x56` | TOP59 Free, TOP58 Blocked
M1 | `RoomHeaterState 0` | `0x56` -> `0x55` | TOP59 -> Blocked
M2 | `RoomHeaterState 1` | `0x55` -> `0x56` | TOP59 -> Free
M3 | - | Byte 5 = `0x55` | TOP68 Inactive
M4 | `ForceHeater 1` | `0x55` -> `0x59` | TOP68 -> Active
M5 | `ForceHeater 0` | `0x59` -> `0x55` | TOP68 -> Inactive

**Die WP nimmt beide Bytes an.** In beiden Faellen wechselte NUR die eigene
Bitgruppe. Byte 9 traegt an dieser Anlage noch zwei weitere belegte Gruppen
(1+2 und 3+4, beide `01`) - ohne Maske waere das Byte auf `0x02` zusammen-
gefallen und haette drei Felder auf einmal umgelegt.

**Zwei Befunde, die man beim naechsten Mal kennen sollte:**

1. **SET39 wird verzoegert uebernommen.** Bei Byte 9 lag die Flanke nach zwei
   Telegrammen (rund 12 s), bei ForceHeater erst beim zehnten von zwoelf -
   grob eine halbe Minute. Ein `/tablerefresh` direkt nach dem Senden zeigte
   noch `Inactive`. Das ist Bauart: Die WP prueft erst ihre Randbedingungen und
   uebernimmt den Wert dann. Nicht sofort zuruecklesen und daraus auf ein
   verworfenes Kommando schliessen.
2. **TOP59 stand vor dem Lauf an beiden Stufen auf Free** (an H2 auch TOP58),
   TOP90 zaehlt an H1 267 Betriebsstunden - der Owner hatte den Heizstab fuer
   eigene Panel-Tests im Installateurmenue aktiviert und freigegeben. Damit ist
   der Lauf zugleich eine Bestaetigung der Zuordnung: Byte 9 zeigte genau das,
   was am Panel eingestellt war.
3. **Dass der Stab waehrend M4 nicht anlief** (TOP60 Inactive, TOP90
   unveraendert), lag am fehlenden Heizbedarf bei der Aussentemperatur des
   Tages. Am Panel wurde er mit kurz auf 40 Grad angehobener Zieltemperatur
   sehr wohl aktiv.

Nebenbei aus demselben Mitschnitt: Bytes 104-106 (Startverzoegerung und Deltas
des internen Heizstabs, laut Referenz "J/K/L series") stehen alle drei auf
`0x00` - bei dieser Serie also nicht belegt. TOP78 `Heater_On_Outdoor_Temp`
steht auf 2 Grad.

## ForceHeater in Betrieb: Pumpe und Regelung (2026-08-28, WP1)

Zweiter Lauf des Tages, mit Mitschrieb. Stufe 1 AUS (`Heatpump_State` 0),
Aussentemperatur 17 Grad, Pumpe stand. Fenster ueber
`~/nodered-flows/testfenster.py --warte 240` geholt - der Re-Assert setzt
`Z1HeatRequestTemperature` alle 5 Minuten zurueck, ohne Fenster misst man
dagegen an.

```
./test/top_watch.py 192.168.2.120 0 1 5 6 7 8 16 27 60 65 68 90 --dauer 900 --takt 5 &
./test/mqtt_pub.py --host 192.168.2.147 panasonic_heat_pump/set/ForceHeater=1
./test/mqtt_pub.py --host 192.168.2.147 panasonic_heat_pump/set/Z1HeatRequestTemperature=30
# ... beobachten ...
./test/mqtt_pub.py --host 192.168.2.147 panasonic_heat_pump/set/Z1HeatRequestTemperature=20
./test/mqtt_pub.py --host 192.168.2.147 panasonic_heat_pump/set/ForceHeater=0
```

Zeiten aus `top_watch.py` (5-s-Takt), Kommandos aus dem Sendelog:

Zeit | Ereignis | Anlage
:--- | :--- | :---
21:43:11 | `ForceHeater 1` | -
21:43:19 | TOP68 Active | **Pumpe laeuft im selben Schritt an**, 0 -> 2300 1/min
21:43:27 | `Z1HeatRequestTemperature 30` | TOP7/TOP27 folgen 21:43:34
21:45:31 | TOP60 Active, TOP16 3000 W | Heizstab laeuft, Vorlauf steigt
21:45:57 | `Z1HeatRequestTemperature 20` | TOP7/TOP27 folgen 21:46:06
21:46:16 | TOP60 Inactive, 0 W | Vorlauf 25,0 - **Pumpe laeuft weiter**
21:46:37 | `ForceHeater 0` | -
21:46:47 | TOP68 Inactive | -
21:46:57 | - | **Pumpe steht**, 2300 -> 0

Vorlauf 22,5 -> 25,5 Grad (Hoechstwert 11 s NACH dem Abschalten), Ruecklauf
21,0 -> 23,0, Durchfluss konstant rund 12 l/min, 3000 W elektrisch fuer 3 kW
thermisch.

**Die Umwaelzpumpe haengt an SET39, nicht am Heizstab** - sie startet mit dem
Kommando und stoppt erst, wenn es zurueckgenommen wird. Ein vergessenes SET39
laesst sie dauerhaft laufen; TOP65 `Pump_Speed` und TOP1 `Pump_Flow` zeigen das.

**Die Anlage regelt im Force-Modus mit** - der Stab ging von selbst aus, als der
Vorlauf ueber die Stoppschwelle stieg, 10 s nachdem der zurueckgenommene
Sollwert im Antworttelegramm stand. Auf die Sekunde nachrechnen laesst sich die
15-s-Stoppbedingung damit nicht (die WP hatte den Sollwert schon vorher), der
Punkt selbst steht: kein ungeregeltes Durchheizen.

**Der Stab lief 2:20 min nach dem Kommando an**, nicht erst nach den neun
Minuten Pumpenlauf aus der Handbuchbedingung. Warum, ist offen.

**Die Uebernahme von SET39 schwankt** - mittags rund eine halbe Minute, abends
8 s (ein) und 10 s (aus). Keine feste Groesse, mit der man rechnen kann.

**Vorsicht bei der Auswertung:** TOP90 `Room_Heater_Operations_Hours` blieb ueber
den ganzen Lauf auf 267 h stehen. Kurze Laeufe erfasst der Zaehler nicht - dafuer
sind TOP60 und TOP16 zustaendig.

## Kurvenbetrieb: was die WP annimmt und was sie verwirft (2026-08-20, WP1)

Der Lauf gehoert zum Vorhaben Notbetrieb (M1/M2, siehe
`Vorhaben-Notbetrieb-Weboberflaeche.md`). Aufbau: Anlage steht
(`Heatpump_State` 0, `Compressor_Freq` 0), Kompressor ueber KNX freigegeben,
Richtung Kuehlen (TOP101 = 1), Aussentemperatur 28 C, Firmware 3.11.0.

```
./test/top_watch.py 192.168.2.120 7 27 29 30 --dauer 240 --takt 5   # im Hintergrund
./test/mqtt_pub.py --host 192.168.2.147 \
    panasonic_heat_pump/set/Z1HeatCurveTargetHighTemp=34 \
    panasonic_heat_pump/set/Z1HeatCurveTargetLowTemp=26 \
    panasonic_heat_pump/set/Z1HeatCurveOutsideLowTemp=-10 \
    panasonic_heat_pump/set/Z1HeatCurveOutsideHighTemp=15
```

**Die vier Kurvenpunkte gehen im Kurvenbetrieb durch** - alle in einem
Sammelfenster gesendet, alle binnen 15 s zurueckgelesen. Auch der obere Punkt
(SET27), den `kurven_sync.py` bewusst auslaesst: Dessen Einschraenkung gilt nur
im Direktbetrieb, wo er sich mit dem Sollwert eine Speicherstelle teilt.

**Der Benutzerwert ist im Kurvenbetrieb die Parallelverschiebung** und eine
EIGENE Speicherstelle - der bisherige Satz "SET5 und SET27 sind derselbe Wert"
gilt nur im Direktbetrieb:

| Gesendet | TOP27 Request | TOP7 Main_Target | TOP29 TargetHigh |
| --- | ---: | ---: | ---: |
| - (Kurve 34/26) | 0 | 26 | 34 |
| `Z1HeatRequestTemperature=2` | 2 | **28** | 34 |
| `Z1HeatRequestTemperature=4` | 4 | **30** | 34 |
| `Z1HeatRequestTemperature=20` | 4 | 30 | 34 |

**Werte ausserhalb -5..+5 verwirft die WP stillschweigend.** Die 20 steht im
Kommandotelegramm (`produktiv_mitschnitt.py` zeigt `Z1 Heat 20 C`), sie geht
also raus - die WP uebernimmt sie nur nicht. Zweimal einzeln reproduziert, dazu
ein im selben Fenster mitgeschnittener echter Re-Assert der Kaskadensteuerung
(`Heatpump aus`, `WaterPump auto`, `OperationMode`, `Z1 Heat 20 C`,
`Z1 Cool 20 C`, `PumpSpeed 100`), der die Verschiebung ebenfalls nicht anfasste.
Die Bereichspruefung sitzt also in der Waermepumpe; die Firmware muss sie nicht
nachbilden.

**Wiederholtes `HeatingMode 1` im Direktbetrieb ist folgenlos** (M2): 31 s
beobachtet, keine Aenderung an TOP7/27/28/29/30/76.

**Der Werks-Reset laeuft in BEIDE Richtungen und trifft die Kuehlseite mit.**
Beim Zurueckschalten auf Direkt, 4 s nach dem Moduswechsel: TOP29 und TOP30 auf
35, TOP32 auf -5, Sollwert TOP27 uebernahm die 35 - und TOP28/TOP42/TOP72 auf
10, obwohl nur der Heizkreis geschaltet wurde. Der Kuehl-Sollwert stand so 90 s
auf 10 C. Bei stehender Anlage folgenlos, im laufenden Kuehlbetrieb waere es
ein Eingriff.

### Der Nebenbefund: TargetHigh gehoert zur NIEDRIGEN Aussentemperatur

Bei 28 C draussen, Kurve 34/26 und Aussenpunkten -10/+15 meldete
`Main_Target_Temp` (TOP7) **26** - also TargetLow. Damit ist die Paarung:

| Wert | gehoert zu | gilt bei | im ioBroker-Baum | Etikett |
| --- | --- | --- | --- | --- |
| `Z1HeatCurveTargetHighTemp` (SET27, TOP29) | `OutsideLow` (SET29, TOP32) | **kaltem** Wetter | `KK_HK_vlLo` | **VL kalt** |
| `Z1HeatCurveTargetLowTemp` (SET28, TOP30) | `OutsideHigh` (SET30, TOP31) | **warmem** Wetter | `KK_HK_vlHi` | **VL warm** |
| `Z1HeatCurveOutsideLowTemp` (SET29, TOP32) | `TargetHigh` (SET27, TOP29) | **kaltem** Wetter | `KK_HK_atLo` | **AT kalt** |
| `Z1HeatCurveOutsideHighTemp` (SET30, TOP31) | `TargetLow` (SET28, TOP30) | **warmem** Wetter | `KK_HK_atHi` | **AT warm** |

Die Werkskurve bestaetigt es: 55 C bei -5 C, 35 C bei +15 C. Eine Heizkurve
faellt mit steigender Aussentemperatur.

**`MQTT-Topics.md` und das `MAPPING` in `kurven_sync.py` haben es umgekehrt.**
Gespiegelt wird dadurch `KK_HK_vlLo` (Vorlauf bei niedriger Aussentemperatur)
in das Feld fuer warmes Wetter. Ohne Wirkung, solange die Anlage im
Direktbetrieb laeuft - aber der Notbetrieb aktiviert genau diese Kurve. Die
Korrektur ist im Vorhaben Notbetrieb, Abschnitt 6a, als Vorbedingung notiert.

*Nachtrag 2026-08-23: beide Stellen sind seit dem 2026-08-20 korrigiert. Damit
die Kreuzung nicht wieder untergeht, fuehren Werkzeugausgaben und Tabellen seit
diesem Tag ein Etikett mit (VL kalt / VL warm / AT kalt / AT warm), das keiner
der beiden Namenskonventionen folgt.*

### Die Kurve wird auf ihre Richtung geprueft (seit 3.14.0)

Ein Etikett macht die Verwechslung sichtbar, verhindert sie aber nicht. Deshalb
liegt seit 3.14.0 eine Regel daneben - `notbetrieb_kurve_pruefen()` in
`src/notbetrieb.h`, vom Hosttest abgedeckt:

| Fall | Beurteilung |
| --- | --- |
| VL kalt >= VL warm | in Ordnung (Gleichheit erlaubt: flache Kurve) |
| VL kalt < VL warm | **Vorlaeufe vertauscht** |
| AT kalt < AT warm | in Ordnung |
| AT kalt >= AT warm | **Aussenpunkte vertauscht oder gleich** |

Kein Bereichstest kann diese Faelle finden: 26 und 34 sind beide gueltig, es
kommt allein auf ihr Verhaeltnis an. Die Regel WARNT und sperrt nicht - der
Notbetriebsknopf bleibt bedienbar, und ein Lauf auf verdrehter Kurve ist immer
noch besser als keiner. Sichtbar wird sie auf der Notbetriebsseite (blassgelbes
Feld), im MQTT-Log beim Wechsel der Beurteilung und im achten Feld von
`/notbetrieb/status`.

`kurven_sync.py` prueft dasselbe VOR dem Senden und bricht dann ab - dort waere
die verdrehte Kurve nicht nur ein Hinweis, sondern der Zustand, den die Anlage
im Notbetrieb faehrt. `--kurve-ignorieren` hebt den Abbruch auf.

## Der Notbetriebszweig wird wiedereingespielt (2026-08-20, Broker)

Der Notbetrieb ruht darauf, dass der ioBroker-MQTT-Adapter einem NEUEN
Abonnenten die gespeicherten Werte einspielt - nur so hat die Firmware ihre
Kurvenwerte nach einem Neustart binnen Sekunden wieder. Fuer den `set`-Zweig ist
das belegt (2026-08-13, und dort ist es die Gefahr, gegen die SUBSCRIBE_GRACE
gebaut wurde). Offen war, ob der Adapter das auch fuer einen Zweig tut, den er
vorher nie gesehen hat.

**Er tut es.** Nachgewiesen ohne Geraet, weil `mqtt_sub.py` genau das macht, was
die Firmware nach einem Neustart tut - verbinden, abonnieren, zuhoeren:

```
./test/mqtt_pub.py --host 192.168.2.147 \
    panasonic_heat_pump_test/notbetrieb/Z1HeatCurveTargetHighTemp=34 \
    panasonic_heat_pump_test/notbetrieb/Z1HeatCurveTargetLowTemp=26 \
    panasonic_heat_pump_test/notbetrieb/Z1HeatCurveOutsideLowTemp=-10 \
    panasonic_heat_pump_test/notbetrieb/Z1HeatCurveOutsideHighTemp=15

./test/mqtt_sub.py --host 192.168.2.147 'panasonic_heat_pump_test/notbetrieb/#'
```

Die zweite Verbindung bekam alle vier Werte, ohne dass jemand publizierte - und
zwar mit **retain=0**, genau wie beim `set`-Zweig. Ueber das Retain-Bit ist
diese Wiedereinspielung also auch hier nicht von einem echten Kommando zu
unterscheiden; die Trennung laeuft allein ueber den Topic-Zweig.

Gelaufen ist das gegen das Test-Prefix `panasonic_heat_pump_test`, auf das kein
Geraet hoert. Der produktive Zweig blieb leer (Gegenprobe ueber die
simple-api: 0 Objekte unter `mqtt.0.panasonic_heat_pump.notbetrieb*`). Die vier
Testobjekte bleiben im ioBroker stehen - sie stoeren nichts und sind der Beleg.

## Der Notbetrieb ueberlebt den Neustart (2026-08-20, Pruefstand)

Am Pruefstand 192.168.2.197 (D1 mini, keine Waermepumpe, Prefix
`panasonic_heat_pump_test`, Rolle Heizen mangels Rollen-Flag). Der Ablauf ist
genau der, auf den sich der Notbetrieb verlaesst: Werte hinterlegen, Geraet neu
starten, und danach sendet **niemand** mehr etwas.

```
./test/mqtt_pub.py --host 192.168.2.147 \
    panasonic_heat_pump_test/notbetrieb/Z1HeatCurveTargetHighTemp=34 \
    panasonic_heat_pump_test/notbetrieb/Z1HeatCurveTargetLowTemp=26 \
    panasonic_heat_pump_test/notbetrieb/Z1HeatCurveOutsideLowTemp=-10 \
    panasonic_heat_pump_test/notbetrieb/Z1HeatCurveOutsideHighTemp=15

./test/mqtt_sub.py --host 192.168.2.147 --dauer 60 \
    panasonic_heat_pump_test/info/log panasonic_heat_pump_test/info/LWT
curl -u admin:heisha http://192.168.2.197/reboot
```

Was nach dem Neustart von allein im Log stand:

```text
Notbetrieb: Rolle Heizen, 4 Werte erwartet
Notbetrieb einsatzbereit: alle 4 Werte liegen vor
34 wiedereingespielte Set-Kommandos nach dem Verbinden verworfen
```

**Die dritte Zeile ist die entscheidende.** Im selben Moment, in dem die
Karenzzeit 34 wiedereingespielte SET-Kommandos wegwirft, kommen die vier
Notbetriebswerte durch - weil sie vor der Karenzpruefung behandelt werden. Genau
diese Ausnahme ist der Unterschied zwischen einem Knopf, der nach jedem Neustart
funktioniert, und einem, der es nicht tut.

### Die Ablehnungspfade, ebenfalls am Geraet

| Gesendet | Im MQTT-Log |
| --- | --- |
| `Z1HeatCurveTargetHighTemp=34` | nichts (der Einzelwert laeuft nur ins Telnet-Log) |
| `Z1HeatCurveTargetHighTemp=99` | `abgelehnt (erlaubt 20..55)` |
| `Z1HeatCurveTargetLowTemp=abc` | `ist keine Zahl (abc) - verworfen` |
| `notbetrieb/Quatsch=5` | nichts - das Topic wird gar nicht abonniert |

Die letzte Zeile ist kein Mangel, sondern der Beleg fuer die
Abonnement-Entscheidung: Die Firmware abonniert nur die Namen ihrer Rolle, kein
`notbetrieb/#`. Ein Tippfehler im Node-RED-Flow erreicht das Geraet also gar
nicht erst - er faellt beim Abgleich der Topics auf, nicht erst beim Druck auf
den Knopf.

### Falls der Pruefstand frisch geflasht ist

Nach dem Flashen kann `mqtt_server` leer sein - dann bleibt das LWT auf
`Offline` und nichts von alldem passiert:

```
curl -u admin:heisha "http://192.168.2.197/settings?mqtt_server=192.168.2.147"
```

### Das Telnet-Log zeigt die Einzelwerte

Jeder angenommene Wert steht dort, auch wenn er nicht ins MQTT-Log geht:

```text
[2026-08-20 15:59:32] <DBG> Notbetrieb gemerkt: Z1HeatCurveTargetLowTemp = 27
```

Dafuer ist **kein** `L` noetig: `L` schaltet nur um, wohin `write_mqtt_log()`
schreibt (MQTT oder Telnet), waehrend `write_telnet_log()` ohnehin auf Telnet
geht. Was man braucht, ist eine stehende Verbindung zum Zeitpunkt des
Ereignisses - ein erster Versuch dieses Laufs lief ins Leere, weil die
Verbindung unmittelbar vor einem Reboot aufgebaut und danach nicht sauber neu
hergestellt wurde. Wer ueber einen Neustart hinweg mitlesen will, muss
wiederverbinden; ein Lebenszeichen holt man mit `R` (antwortet nur mit Text und
aendert nichts).

Fuer Ablaeufe ueber einen Neustart hinweg ist `info/log` mit `mqtt_sub.py`
trotzdem der bequemere Weg, weil der Broker die Zeilen puffert.

## Der erste Lauf an der Anlage (2026-08-20, WP1) - ROT in Schritt 1

Der erste Druck auf den echten Knopf, H1 mit der Firmware dieses Branches.
Gefahren im Ruhefenster des 5-min-Re-Assert (`~/nodered-flows/testfenster.py
--warte 240` meldete 4:35 min Ruhe), damit kein fremdes Kommando dazwischenkommt.

```text
  Ausgangslage 21:31:17 - /notbetrieb/status = 0;1;7;0
      TOP0  Heatpump_State                 0     TOP29 Z1_Heat_Curve_Target_High  20
      TOP4  Operating_Mode_State           1     TOP30 Z1_Heat_Curve_Target_Low   26
      TOP7  Main_Target_Temp              20     TOP31 Z1_Heat_Curve_Outside_High 15
      TOP27 Z1_Heat_Request_Temp          20     TOP32 Z1_Heat_Curve_Outside_Low -10
      TOP76 Heating_Mode                   1

  21:31:34  KNOPF GEDRUECKT - HTTP 200
  21:31:34  + 0.2s  Status 1;1;7;0   laeuft, Schritt 1 von 7
  21:31:55  +20.7s  Status 3;1;7;0   ROT, Schritt 1 von 7
```

Schritt 1 ist `OperationMode` = 0 (Heat only), rueckgelesen an TOP4. Der Wert kam
nicht zurueck, der Automat brach nach dem vollen Schritt-Timeout ab - **ohne
weiterzumachen**: Die vier Kurvenpunkte wurden nie geschrieben, `Heatpump` = 1
nie gesendet, und alle neun beobachteten TOPs standen hinterher exakt wie
vorher. Genau dafuer ist der Abbruch gebaut.

### Die Gegenmessung trennt Firmware und Waermepumpe

Unmittelbar danach ein einzelnes `set/OperationMode 0` ueber MQTT, ohne den
Notbetrieb:

```text
  21:33:16  mqtt_pub.py -> panasonic_heat_pump/set/OperationMode = 0
  21:33:16  Log der Bridge: <SUB> SET9 OperationMode: 0
  21:33:56  TOP4   Operating_Mode_State   1  Cool     <- unveraendert
            TOP101 Heat_Cool_SW_State     1  Cool     <- unveraendert
```

Die Firmware hat das Kommando also abgesetzt (Bereichspruefung, Merge und
Telegramm sind durchlaufen), die **Waermepumpe hat es verworfen** - 40 s und
damit ueber sechs Abfragezyklen lang, und zwar sowohl im kommandierten Wert
(TOP4) als auch im echten Ist-Zustand aus Byte 110 (TOP101). Dasselbe
stillschweigende Verwerfen wie bei `Z1HeatRequestTemperature` im Kurvenbetrieb.

### Offen: zwei Hypothesen, noch nicht getrennt

**(a) Der KNX-Aktor gibt Heizen/Kuehlen vor.** Im Kuehlbetrieb nimmt die Anlage
kein "Heat only" per MQTT an. Pruefung: Heiz/Kuehl-Schalter auf Heizen stellen,
SET9 wiederholen.

**(b) Die Anlage stand aus** (`Heatpump_State` = 0) und nimmt im Aus-Zustand
keine Betriebsartaenderung an. Pruefung: `Heatpump` = 1 senden, dann SET9
wiederholen. Dafuer spricht, dass die Gegenprobe an H2 (`OperationMode` = 3 aus
dem Kuehlbetrieb heraus) an einer **laufenden** Stufe gemessen wurde.

Trifft (b) zu, ist es ein Fehler in der Schrittfolge - `Heatpump` = 1 gehoert
dann nach vorn. Trifft (a) zu, ist der Notbetrieb Heizen im Kuehlbetrieb
grundsaetzlich nicht schaltbar, und der Knopf gehoert gesperrt, solange TOP101
auf Cool steht.

## Der Knopf am Pruefstand (2026-08-20, Bausteine B-D)

Der Pruefstand hat keine Waermepumpe - und genau deshalb laeuft dort der
FEHLERpfad, den man an der echten Anlage nicht provozieren will.

```
curl -u admin:heisha http://192.168.2.197/notbetrieb            # Seite
curl -u admin:heisha -X POST http://192.168.2.197/notbetrieb/start
curl http://192.168.2.197/notbetrieb/status                     # ...;Sperre;Lage;Dauertext (Lage: 0 ok, 2 Broker weg, 4 Steuerung stumm)
```

**Ablauf, wie er sein soll:**

```text
POST -> HTTP 303 (Umleitung; ein Neuladen wiederholt die Anzeige, nicht das Kommando)
NOTBETRIEB ausgeloest ueber die Weboberflaeche
Notbetrieb Schritt 1/6: HeatingMode = 0
<SUB> SET35 HeatingMode: 0            <- regulaerer Pfad ueber build_heatpump_command()
Status 1;1;6;0  (LAEUFT, Schritt 1 von 6)  ... 20 s lang
Notbetrieb ROT: Schritt 1/6 (HeatingMode) kam nicht zurueck
Status 3;1;6;0  (ROT)
```

**Der entscheidende Befund ist, was NICHT passiert ist.** Am Pruefstand
antwortet keine Waermepumpe, `actual_data` fuer TOP76 ist also leer - und der
Sollwert des ersten Schritts ist 0. Mit einem naiven `atoi(actual_data[...])`
waere die Folge sofort durchgerauscht und haette GRUEN gemeldet, ohne dass
irgendetwas geschehen waere. `notbetrieb_rueckgelesen()` faengt genau das ab
(Hosttest: "LEERER Wert bestaetigt die 0 NICHT").

### Der gesperrte Knopf

Einen Wert ungueltig machen und neu starten - dann fehlt er:

```
./test/mqtt_pub.py --host 192.168.2.147 \
    panasonic_heat_pump_test/notbetrieb/Z1HeatCurveOutsideLowTemp=99
curl -u admin:heisha http://192.168.2.197/reboot
```

| Pruefung | Ergebnis |
| --- | --- |
| Status | `0;1;6;4` - Bit 2 der fehlend-Maske, also Wert Nr. 3 |
| Seite | "Nicht bereit", nennt `Z1HeatCurveOutsideLowTemp` beim Namen |
| POST trotzdem | HTTP 303, aber **kein Lauf** - Status bleibt `0` |

Der Knopf ist also nicht nur ausgeblendet, sondern der Ausloeser selbst
verweigert. Ein zusammengebasteltes POST von aussen startet nichts.

### Anmeldung

| Route | Ohne Anmeldung |
| --- | --- |
| `/notbetrieb` | HTTP 401 |
| `/notbetrieb/start` | HTTP 401 |
| `/notbetrieb/status` | HTTP 200 - gibt nur "Schritt 3 von 6" heraus und aendert nichts |

## Umbauten am Dekodierpfad absichern (decode_vergleich.py)

Wer an `decode.cpp` umbaut, will wissen, ob sich die AUSGABE veraendert hat -
und zwar bevor geflasht wird. `decode_vergleich.py` uebersetzt dafuer zwei
Codestaende auf dem Mac (Arduino-Ersatzheader liegen im Skript, kein Geraet
noetig) und fuettert beide mit denselben Telegrammen. Verglichen werden je
Topic TOP-Nummer, Name, dekodierter Wert und Einheit.

```bash
./decode_vergleich.py                  # Arbeitsstand gegen HEAD
./decode_vergleich.py --basis v3.2.2   # gegen ein Tag
```

Die Telegramme sind bewusst nicht nur zufaellig: Phase 1 setzt jeden Bytewert
von 0 bis 255 auf allen Positionen gleichzeitig und trifft damit jeden Zweig
jedes Dekodierers - auch die seltenen wie die Fehlercodes F/H (Bytewerte 177
und 161) und die Nachkommabits. Phase 2 haengt 500 Pseudozufallstelegramme an,
um Kombinationen ueber mehrere Bytes zu erwischen. Zusammen 756 Telegramme,
also 74844 verglichene Zeilen bei 99 Topics.

Fuer den Fall, dass Topics absichtlich entfallen oder dazukommen, gibt es
`--entfallen` und `--neu`:

```bash
./decode_vergleich.py --entfallen Z2_               # Zone-2-Topics duerfen neu fehlen
./decode_vergleich.py --neu Quiet_Mode_Active ...   # diese Topics duerfen neu sein
```

Damit wurde 3.3.0 abgenommen (Umbau auf die eine `stateTopics`-Tabelle):
74844 Zeilen identisch. Und 3.7.0 (Byte-110-Topics): 65016 Zeilen ueber die 86
bestehenden Topics identisch, die vier neuen ausgeblendet - was die neuen
liefern, prueft `byte110_test.cpp`.

Die Arduino-Ersatzheader stehen seit 3.7.0 nicht mehr als Zeichenketten im
Skript, sondern in `stubs/` - `byte110_test.cpp` benutzt dieselben Dateien.

## Entfallene Topics aufraeumen (retained_loeschen.py)

Die Firmware publiziert alle state-Topics mit Retain-Flag. Faellt ein Topic aus
der Tabelle, hoert die Firmware zwar auf zu senden - ein normaler Broker
(mosquitto o. ae.) liefert den zuletzt gesendeten Wert aber weiter an jeden
neuen Abonnenten aus. Das Topic verschwindet dort also nicht, es friert auf
seinem letzten Wert ein. Dagegen schickt `retained_loeschen.py` eine leere
Nutzlast mit Retain-Flag auf die betroffenen Topics. Welche das sind, wird
nicht von Hand gepflegt, sondern aus dem Code ermittelt (Topic-Namen des
Basisstandes gegen die des Arbeitsstandes) - die Liste kann nicht veralten.

```bash
./retained_loeschen.py --basis v3.3.0              # nur anzeigen
./retained_loeschen.py --basis v3.3.0 --loeschen   # wirklich loeschen
```

**Reihenfolge:** erst die neue Firmware auf BEIDE Stufen flashen, dann
loeschen. Andersherum publiziert die noch laufende alte Firmware die Werte
sofort wieder.

### Hier gilt das nur zur Haelfte - der Broker IST der ioBroker

Am 2026-08-11 beim Zone-2-Ausbau nachgemessen: Der Broker auf
192.168.2.147:1883 ist **kein eigenstaendiger Broker, sondern der
ioBroker-MQTT-Adapter im Server-Modus** (`mqtt.0.info.connection` fuehrt die
verbundenen Clients auf, darunter HeishaMon32_h1 und HeishaMon32_h2). Damit
gibt es keinen getrennten Retained-Speicher: Der Adapter bedient neue
Abonnenten aus seiner eigenen Objektdatenbank, und zwar mit **retain=0** -
auch bei Zone-1-Topics, die es noch gibt.

Folge fuer das Aufraeumen:

* `retained_loeschen.py` setzt die betroffenen ioBroker-States auf `null` -
  belegt, die Zeitstempel der Objekte sprangen auf den Zeitpunkt des Laufs.
  Schaden richtet es keinen an, aber die Topics sind damit nicht weg.
* Sie werden weiter angekuendigt, jetzt mit Nutzlast `null`, **solange die
  Objekte unter `mqtt.0.*` existieren**.
* Das eigentliche Aufraeumen passiert deshalb im ioBroker: Objekte loeschen
  (Admin, Objekte, `mqtt.0.panasonic_heat_pump.state.Z2_*` und dasselbe unter
  `panasonic_heat_pump2`). Eine Loeschschnittstelle hat die simple-api auf
  Port 8087 nicht - deren Endpunkte sind get/getBulk/set/setBulk/toggle/
  objects/states/search/query, mehr nicht. Also Admin-Oberflaeche.

Das Skript bleibt trotzdem sinnvoll: Sobald der Broker einmal ein echter
mosquitto ist (z. B. im Container), greift der Retain-Mechanismus wie
beschrieben.

## Fallstrick: MQTT-Client-ID bei schnellen Reconnects

Ein zweiter MQTT-Client mit derselben Client-ID trennt laut Spezifikation den
ersten. Wer je Nachricht neu verbindet und dabei immer dieselbe ID nutzt,
verliert bei schnell aufeinanderfolgenden Reconnects Nachrichten - am
2026-08-10 blieb so ein Kurvenwert auf dem Testwert 55 stehen, obwohl das
Ruecksetz-Kommando abgesetzt wurde. Konsequenz fuer diese Werkzeuge: EINE
Verbindung fuer alle Publishes eines Vorgangs, Client-ID mit Prozess-ID, und
eine Wiederherstellung wird nachgeprueft statt nur abgesetzt.

## Wichtig: TargetHigh ist die Vorlauf-Solltemperatur - im DIREKTbetrieb

`Z1HeatCurveTargetHighTemp` (SET27, Byte 75) und `Z1HeatRequestTemperature`
(SET5, Byte 38) sind in der Waermepumpe **derselbe Wert** - aber nur, solange
der Heizkreis auf Direktvorgabe steht. Fuer das Kuehl-Paar (SET31 / SET6) gilt
dasselbe.

> **Im Kurvenbetrieb nicht.** Dort sind es zwei getrennte Speicherstellen: SET27
> ist der Kurvenpunkt, SET5 die Parallelverschiebung (-5..+5). Am 2026-08-20
> gemessen, siehe "Kurvenbetrieb: was die WP annimmt und was sie verwirft"
> weiter oben. Die Messung vom 2026-08-10, auf der die Gleichsetzung beruht,
> lief ausschliesslich im Direktbetrieb.

Am 2026-08-10 an WP1 in beide Richtungen belegt:

```text
CurveTargetHigh=26 gesetzt, dann RequestTemp=20 gesendet
   -> CurveTargetHigh fiel auf 20

RequestTemp=20 und CurveTargetHigh=26 im selben Telegramm
   -> BEIDE standen danach auf 26
```

Daraus folgen drei Dinge:

1. **Der obere Kurvenpunkt ist im laufenden Direktbetrieb nicht haltbar.** Der
   5-min-Re-Assert des Node-RED-Verteilers schreibt die Solltemperatur und
   zieht den Kurvenpunkt mit. `kurven_sync.py` uebertraegt ihn deshalb nicht.
2. **Ihn zu setzen ist ein Eingriff in den laufenden Betrieb** - er verstellt
   die aktive Vorlauf-Solltemperatur. Genau das ist beim ersten Sync-Lauf
   passiert (Kuehl-Sollwert einige Minuten auf 19 statt 20).
3. **Fuer den Notbetrieb:** Der obere Kurvenpunkt gehoert in die
   Notfall-Checkliste am Bedienterminal. Die urspruengliche Befuerchtung war,
   dass beim Umschalten der zuletzt gefahrene Direktwert als oberer
   Kurvenpunkt stehenbleibt und eine praktisch waagerechte Kurve ergibt. Das
   trifft so nicht zu - siehe naechster Abschnitt, die WP ueberschreibt beim
   Umschalten ohnehin alle vier Punkte.

## Der Moduswechsel ueberschreibt die Kurvenwerte (2026-08-11, WP2)

Gemessen im Wartungsmodus: Anlage aus, Re-Assert des Node-RED-Verteilers
gestoppt, dann am Bedienterminal von Direktvorgabe auf Heizkurve und wieder
zurueck geschaltet. Ohne jeden Schreibzugriff von aussen.

| Topic | Start (Direkt) | nach Umschalten auf Kurve | zurueck auf Direkt |
| --- | --- | --- | --- |
| TOP29 Z1_Heat_Curve_Target_High_Temp | 20 | **55** | 35 |
| TOP30 Z1_Heat_Curve_Target_Low_Temp | 34 | **35** | 35 |
| TOP31 Z1_Heat_Curve_Outside_High_Temp | 15 | 15 | 15 |
| TOP32 Z1_Heat_Curve_Outside_Low_Temp | -10 | **-5** | -5 |
| TOP72 Z1_Cool_Curve_Target_High_Temp | 20 | **15** | 10 |
| TOP73 Z1_Cool_Curve_Target_Low_Temp | 20 | **10** | 10 |
| TOP74 Z1_Cool_Curve_Outside_High_Temp | 30 | 30 | 30 |
| TOP75 Z1_Cool_Curve_Outside_Low_Temp | 25 | **20** | 20 |
| TOP27 Z1_Heat_Request_Temp | 20 | **0** | **35** |
| TOP28 Z1_Cool_Request_Temp | 20 | **0** | **10** |

Belege in `pictures/`: `Start_Test1` / `Start_Test2` (Ausgangswerte),
`Bild ... 21.07` / `21.08` (nach Umschalten auf Kurve), `Bild ... 21.15` /
`21.16` (nach Rueckstellung auf Direkt). Die Kurvendialoge `IMG_4887` und
`IMG_4889` zeigen dieselben Werte am Terminal.

Drei Befunde:

1. **Die konfigurierte Kurve ist nach dem Umschalten weg.** Es stehen
   55 Grad bei -5 und 35 Grad bei +15 (heizen) bzw. 15 Grad bei 20 und
   10 Grad bei 30 (kuehlen) - die Panasonic-Werksvorgaben. Das Zurueckschalten
   stellt die alten Werte NICHT wieder her.
2. **Im Kurvenbetrieb melden TOP27/TOP28 den Wert 0.** Wer den Direktsollwert
   als Regelgroesse oder Lebenszeichen liest, sieht dort nichts Brauchbares.
3. **Der Direktsollwert geht beim Roundtrip verloren.** Vorher 20 Grad,
   danach 35 (heizen) bzw. 10 (kuehlen) - er uebernimmt den unteren
   Kurvenpunkt. Nach einem Rueckschalten faehrt die Anlage also mit einem
   fremden Sollwert weiter, bis der 5-min-Re-Assert greift.

Nicht geklaert und bewusst nicht weiterverfolgt: ob die WP auf feste
Werksdefaults zurueckstellt oder einen getrennt gespeicherten Kurvensatz
hervorholt, der hier noch im Auslieferungszustand war. Beides sagt fuer diesen
Durchlauf dasselbe voraus. Die Antwort wuerde nur entscheiden, ob eine
Automatik die Kurve bei jedem Wechsel oder nur einmal schreiben muesste - und
das ist gegenstandslos, weil die Kaskadensteuerung ueber die Direktvorgabe
regelt und die Kurve nicht braucht.

**Konsequenz fuer den Notbetrieb:** Wer im Notfall am Terminal auf Heizkurve
umschaltet, bekommt die Werksvorgaben und muss anschliessend alle vier Punkte
von Hand einstellen. Die Notfall-Unterlage braucht deshalb die Sollwerte
vollstaendig - als Tabelle oder als Bild der beiden Kurvendialoge -, nicht nur
den oberen Punkt.
