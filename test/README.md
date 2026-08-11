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
| `hexlog_test.py` | Kerntest: Heatpump + WaterPump muessen in einem Telegramm landen | Pruefstand |
| `verteiler_test.py` | Abnahmetest: alle sechs Kanaele des Node-RED-Verteilers gleichzeitig | Pruefstand |
| `produktiv_mitschnitt.py` | Passiv am laufenden Geraet mithoeren, sendet nichts | Produktivgeraet |
| `kurven_test.py` | Kurven-Kommandos SET27-SET34 nachweisen (schreibt die Ist-Werte zurueck, veraendert nichts) | Produktivgeraet |
| `kurven_sync.py` | Heiz-/Kuehlkurve aus dem ioBroker-Konfigurationsbaum in die WPs spiegeln (`--dry-run`) | Produktivgeraet |
| `kurven_grenzen.py` | Ermitteln, welche Kurvenwerte die WP wirklich annimmt (veraendert Werte, stellt sie zurueck) | Produktivgeraet |
| `decode_vergleich.py` | Dekodierpfad zweier Codestaende gegeneinander laufen lassen, auf dem Mac | nein |
| `retained_loeschen.py` | Retained Messages entfallener state-Topics vom Broker raeumen (Anzeige, `--loeschen` fuer echt) | Broker |
| `heisha_probe.py` | gemeinsame Helfer (Telnet, Hexlog-Parser) | - |
| `mqtt_pub.py` | minimaler MQTT-Publisher ohne Abhaengigkeiten | - |

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

```bash
c++ -std=c++17 -O2 -o /tmp/merge_test merge_test.cpp && /tmp/merge_test

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

## Verhaeltnis zu `pio test`

Das sind eigenstaendige Diagnosewerkzeuge, keine Unity-Testsuites - `pio test`
nutzt sie nicht. Echte Unit-Tests fuer Encoder, Decoder und Merge (Schritt 5
des Umbauplans) waeren der naechste Ausbau; `merge_test.cpp` ist die Vorlage
dafuer.

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

Messung 2026-08-10 an WP1:

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

Fuer den Fall, dass Topics absichtlich entfallen, gibt es `--entfallen`:

```bash
./decode_vergleich.py --entfallen Z2_   # Zone-2-Topics duerfen neu fehlen
```

Damit wurde 3.3.0 abgenommen (Umbau auf die eine `stateTopics`-Tabelle):
74844 Zeilen identisch.

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
verbundenen Clients auf: OpenDTU, venus, HeishaMonH2, HeishaMon32_h1). Damit
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

## Wichtig: TargetHigh ist die Vorlauf-Solltemperatur

`Z1HeatCurveTargetHighTemp` (SET27, Byte 75) und `Z1HeatRequestTemperature`
(SET5, Byte 38) sind in der Waermepumpe **derselbe Wert** - im Direktmodus die
Vorlauf-Solltemperatur, im Kurvenmodus der obere Kurvenpunkt. Fuer das
Kuehl-Paar (SET31 / SET6) gilt dasselbe.

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
3. **Fuer den Notbetrieb:** Beim Umschalten auf Kurvenbetrieb steht am oberen
   Kurvenpunkt der zuletzt gefahrene Direktwert. Faellt die Steuerung im
   Winter bei -10 Grad und 34 Grad Vorlauf aus, ergibt das eine praktisch
   waagerechte Kurve (-10/34 nach 15/34) - die WP wuerde auch bei +15 Grad
   Aussentemperatur noch 34 Grad Vorlauf fahren. Der obere Punkt gehoert
   deshalb in die Notfall-Checkliste am Bedienterminal.
