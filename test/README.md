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
| `notbetrieb_test.cpp` | Regeln des Notbetriebs: Vollstaendigkeit der Werte, Bereichsgrenzen, Karenzzeit-Ausnahme, Zustandsautomat (bindet `src/notbetrieb.h` direkt ein) | nein |
| `decode_hosttest.sh` | Baurahmen fuer `byte110_test.cpp` - kopiert `decode.cpp` neben die Ersatzheader aus `stubs/` | nein |
| `hexlog_test.py` | Kerntest: Heatpump + WaterPump muessen in einem Telegramm landen | Pruefstand |
| `verteiler_test.py` | Abnahmetest: alle sechs Kanaele des Node-RED-Verteilers gleichzeitig | Pruefstand |
| `produktiv_mitschnitt.py` | Passiv am laufenden Geraet mithoeren, sendet nichts | Produktivgeraet |
| `kurven_test.py` | Kurven-Kommandos SET27-SET34 nachweisen (schreibt die Ist-Werte zurueck, veraendert nichts) | Produktivgeraet |
| `kurven_sync.py` | Heiz-/Kuehlkurve aus dem ioBroker-Konfigurationsbaum in die WPs spiegeln (`--dry-run`) | Produktivgeraet |
| `kurven_grenzen.py` | Ermitteln, welche Kurvenwerte die WP wirklich annimmt (veraendert Werte, stellt sie zurueck) | Produktivgeraet |
| `decode_vergleich.py` | Dekodierpfad zweier Codestaende gegeneinander laufen lassen, auf dem Mac | nein |
| `frame_diff.py` | Rohtelegramme eines Mitschnitts ueber alle 203 Bytes vergleichen, angereichert aus `ProtocolByteDecrypt.md` | nein |
| `retained_loeschen.py` | Retained Messages entfallener state-Topics vom Broker raeumen (Anzeige, `--loeschen` fuer echt) | Broker |
| `tablesnap.py` | Momentaufnahme der Topic-Tabelle ueber `/tablerefresh`, zeilenweise diffbar - fuer die Abnahme nach dem Flashen | Produktivgeraet (nur lesend) |
| `top_watch.py` | Verlauf statt Momentaufnahme: ausgewaehlte TOPs im Takt abfragen und jede Aenderung mit Zeitstempel melden | Produktivgeraet (nur lesend) |
| `set_top_zuordnung.py` | Erzeugt die Tabellen in `SET-TOP-Zuordnung.md`: welches State-Topic liest ein Set-Kommando zurueck | nein |
| `byte_monitor.py` | Einzelne Bytes des Antworttelegramms beobachten, um eine Byte-Zuordnung zu belegen statt sie abzuleiten | Produktivgeraet (nur lesend) |
| `heisha_probe.py` | gemeinsame Helfer (Telnet, Hexlog-Parser) | - |
| `mqtt_pub.py` | minimaler MQTT-Publisher ohne Abhaengigkeiten | - |
| `mqtt_sub.py` | minimaler MQTT-Subscriber - zeigt, was der Broker einem NEUEN Abonnenten von sich aus einspielt | Broker |
| `stubs/` | Arduino-Ersatzheader, gemeinsam genutzt von `byte110_test.cpp` und `decode_vergleich.py` | - |

## Pruefstand aufsetzen

Ein beliebiges ESP8266-Board, **ohne Waermepumpe**. Der Nachweis braucht keine
Gegenstelle: Der Hexlog gibt das Telegramm aus, bevor es auf die Leitung geht.

```bash
pio run -e d1_mini_test -t upload      # eigenes MQTT-Prefix, s. platformio.ini
```

Das eigene Prefix ist Pflicht - ohne Stufen-Build-Flags greift der Fallback
`panasonic_heat_pump` aus `Topics.cpp`, und der Pruefling saesse auf dem LWT-
und state-Pfad der produktiven WP1. Falls das Board noch keinen MQTT-Server
kennt, laesst er sich ohne Neuflashen setzen:

```bash
curl -u admin:heisha "http://<ip>/settings?mqtt_server=192.168.2.147"
```

## Ausfuehren

Die C++-Programme pruefen ihre Ergebnisse selbst und geben bei gebrochener
Zusicherung `1` zurueck - die CI bricht dann ab. Vorher (bis 3.5.0) gaben sie
ihre Zahlen nur aus.

`byte110_test.cpp` laeuft ueber das Skript, weil dabei `decode.cpp` neben die
Ersatzheader kopiert werden muss (Begruendung im Skriptkopf).

```bash
c++ -std=c++17 -O2 -o /tmp/merge_test merge_test.cpp && /tmp/merge_test
c++ -std=c++17 -O2 -Wall -o /tmp/byte28_test byte28_test.cpp && /tmp/byte28_test
c++ -std=c++17 -O2 -o /tmp/telegramm_test telegramm_test.cpp && /tmp/telegramm_test
c++ -std=c++17 -O2 -o /tmp/sendwindow_test sendwindow_test.cpp && /tmp/sendwindow_test
c++ -std=c++17 -O2 -Wall -o /tmp/notbetrieb_test notbetrieb_test.cpp && /tmp/notbetrieb_test
./decode_hosttest.sh          # byte110_test.cpp, aus dem Repo-Wurzelverzeichnis auch ./test/...

./hexlog_test.py     --esp 192.168.2.197 --broker 192.168.2.147
./verteiler_test.py  --esp 192.168.2.197
./produktiv_mitschnitt.py --esp 192.168.2.120     # nur mithoeren
```

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

Deshalb rechnen die Regeln in `uint32_t` und nicht in `unsigned long`: auf
ESP8266 und ESP32 ist beides 32 Bit, auf dem Mac waere `unsigned long` 64 Bit
und der Ueberlauftest wuerde gegen nichts pruefen.

Gegenprobe gemacht: mit ungedeckeltem Fenster faellt der Test mit 10
Abweichungen durch. **Was er nicht abdeckt**, sind die Zustandsuebergaenge in
`HeishaMon.cpp` selbst (Ticker, Serial, Flags) - die sind ohne die halbe
Arduino-Welt nicht uebersetzbar und bleiben Sache des Abnahmetests.

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

| Parameter | gemessener Bereich | Anmerkung |
| --- | --- | --- |
| Z1HeatCurveTargetHighTemp | 20 .. 55 | = Vorlauf-Sollwert, s. Abschnitt unten |
| Z1HeatCurveTargetLowTemp | 20 .. 55 | haltbar |
| Z1HeatCurveOutsideLowTemp | -15 .. 15 | haltbar |
| Z1HeatCurveOutsideHighTemp | **-15 .. 15** | frueher 15..35 angenommen - falsche Seite |
| Z1CoolCurveTargetHighTemp | 5 .. 20 | = Vorlauf-Sollwert, s. Abschnitt unten |
| Z1CoolCurveTargetLowTemp | 5 .. 20 | haltbar |
| Z1CoolCurveOutsideLowTemp | 20 .. 30 | haltbar |
| Z1CoolCurveOutsideHighTemp | **15 .. 30** | frueher 20..30 bzw. 30..40 angenommen |

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
`IMG_4894` / `IMG_4892` (Direktwert-Dialoge heizen/kuehlen).

Schrittweite laut Terminal ±1 Grad, wie in `commands.cpp` angenommen. Damit
sind besonders die beiden fett markierten Korrekturen aus 3.2.x vom Geraet
selbst bestaetigt: `Z1HeatCurveOutsideHighTemp` reicht BIS 15, und
`Z1CoolCurveOutsideHighTemp` beginnt bei 15.

Eine Unschaerfe bleibt: Die Kuehl-X-Achse spannt 15 .. 30 auf, waehrend fuer
`Z1CoolCurveOutsideLowTemp` 20 .. 30 hinterlegt ist. Die Achse zeigt
vermutlich nur den weiteren der beiden Punkte - die Klemm-Messung sagte 20.
Kein Widerspruch, aber auch kein Beweis; ein Schreibversuch mit 15 auf SET33
wuerde es klaeren. Bisher nicht gemessen, weil es fuer den Betrieb egal ist.

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

| Wert | gehoert zu | gilt bei |
| --- | --- | --- |
| `Z1HeatCurveTargetHighTemp` (SET27, TOP29) | `OutsideLow` (SET29, TOP32) | **kaltem** Wetter |
| `Z1HeatCurveTargetLowTemp` (SET28, TOP30) | `OutsideHigh` (SET30, TOP31) | **warmem** Wetter |

Die Werkskurve bestaetigt es: 55 C bei -5 C, 35 C bei +15 C. Eine Heizkurve
faellt mit steigender Aussentemperatur.

**`MQTT-Topics.md` und das `MAPPING` in `kurven_sync.py` haben es umgekehrt.**
Gespiegelt wird dadurch `KK_HK_vlLo` (Vorlauf bei niedriger Aussentemperatur)
in das Feld fuer warmes Wetter. Ohne Wirkung, solange die Anlage im
Direktbetrieb laeuft - aber der Notbetrieb aktiviert genau diese Kurve. Die
Korrektur ist im Vorhaben Notbetrieb, Abschnitt 6a, als Vorbedingung notiert.

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
