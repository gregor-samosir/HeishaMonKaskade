# Vorhaben: Heizstab freigeben (Byte 9) und ForceHeater (Byte 5)

Ziel ist, den internen Heizstab über MQTT ansprechbar zu machen — bis 3.16.0
ging das nur am Bedienterminal.

**Stand dieser Datei:** 2026-08-28, Firmware 3.17.0.

> **In 3.17.0 gebaut, am Gerät noch nicht gemessen.** Alle **drei** Kommandos
> stehen in `setCommands[]`: SET37 `RoomHeaterState`, SET38 `DHWHeaterState`,
> SET39 `ForceHeater`. Owner-Entscheidung vom 2026-08-28, alle drei zusammen zu
> bauen statt sie zu staffeln — die Begründung dafür steht am Ende von
> Abschnitt 1. Die Kodierung ist ohne Gerät belegt
> ([`test/byte9_test.cpp`](test/byte9_test.cpp), in der CI); offen bleibt der
> Messplan aus Abschnitt 8 und damit die Frage, ob die WH-MDC05H3E5 Byte 9
> überhaupt annimmt.
>
> Die Topic-Namen tragen **kein** `Set`-Präfix: Bei uns heißt der Pfad
> `<prefix>/set/<Name>`, also `set/RoomHeaterState`. Der Upstream schreibt
> `commands/SetRoomHeaterState` — dort steckt das `Set` im Namen, weil sein
> Pfad es nicht trägt.

**Herkunft:** Fund beim Abgleich von `TobiasHanss/ioBroker.heishamon` und
`Egyras/HeishaMon` gegen unseren Stand am 2026-08-28. Das ioBroker-Repo brachte
nichts Neues (1:1-Portierung des Upstreams), der aktuelle Upstream dagegen drei
Set-Kommandos, die es bei uns nicht gibt.

---

## 1. Das Ziel — und warum `ForceHeater` es im laufenden Betrieb nicht erreicht

Motiv ist ein **Komfortgewinn im Heizbetrieb**, keine Notwendigkeit. Der
Heizstab ist an beiden Anlagen heute **deaktiviert**, und das aus gutem Grund:
Selbst bei −15 °C Außentemperatur hat sich gezeigt, dass die große Masse der
Fußbodenheizung ihn nicht braucht. Er hat 3 kW fix je Stufe — er kann
mithelfen, mehr nicht.

Interessant ist deshalb nicht die Dauerfreigabe, sondern das **gezielte
Zuschalten**: dann, wenn die Wassertemperaturen es laut Kaskadensteuerung
erlauben. Ob das den Komfort spürbar hebt, müssen Experimente im Winter zeigen —
das Vorhaben schafft nur die Voraussetzung dafür.

Das naheliegende Kommando dafür wäre `ForceHeater` gewesen. **Für dieses Ziel
ist es das falsche.** Das Panasonic-Servicehandbuch (Kapitel 12.9, liegt in
`doku-intern/`, nicht öffentlich) beschreibt Force Heater als Ersatzwärmequelle
**bei einer Störung der Wärmepumpe** — die Betriebsart setzt einen anliegenden
Fehler voraus und wird von der Fernbedienung im Fehlerfall auch selbst
aktiviert, wenn im Servicemenü „Force Heater: Auto" eingestellt ist. Bei
laufender Anlage wird die Anforderung abgelehnt, das Bedienteil meldet dann
sinngemäß „wegen laufendem Betrieb nicht möglich". In einer Frostphase läuft die
Anlage — genau dann greift `ForceHeater` also nicht.

**Der richtige Hebel steht in Kapitel 12.6.1.** Für den normalen Heizbetrieb
schaltet die Wärmepumpe den Heizstab selbst zu, sobald sechs Bedingungen
gleichzeitig erfüllt sind:

Bedingung | bei uns
:--- | :---
Heizstab-Schalter ist an | **fehlt — das ist dieses Vorhaben**, Byte 9 / TOP59
30 min seit Kompressor-Thermo-ON | Anlage
9 min seit Start der Umwälzpumpe | Anlage
Außentemperatur unter der Heizstab-Schwelle | TOP78 / SET20 `HeaterOnOutdoorTemp` — **haben wir**
Vorlauf mehr als 4 K unter Soll | Anlage
20 min seit dem letzten Heizstab-Aus | Anlage

Abgeschaltet wird bei Außentemperatur über Schwelle + 2 K oder Vorlauf über
Soll − 2 K, jeweils 15 s durchgehend, sowie bei Heizstab-Schalter aus oder
Kompressor thermo-off.

Damit ist die Lage klar: **Von allem, was für das Ziel nötig ist, fehlt genau
ein Schalter** — die Freigabe in Byte 9. Die Schwelle, ab der die Anlage den
Heizstab überhaupt in Betracht zieht, können wir mit SET20 längst setzen.

### Nachtrag 2026-08-28: `ForceHeater` ist trotzdem ein eigener Hebel

Am Bedienpanel gegengeprüft, und der Befund kippt die Staffelung dieses
Dokuments: **Steht die Wärmepumpe auf aus, lässt sich der Heizstab über Force
Heater einschalten, ohne dass eine Störung anliegt.** Die Handbuchaussage
bleibt richtig, sie ist nur unvollständig — sie beschreibt den Fall der
laufenden Anlage, und dort wird die Anforderung tatsächlich abgelehnt.

Für das Ziel dieses Vorhabens — Komfort im laufenden Heizbetrieb — ändert das
nichts: Dort bleibt SET37 der Hebel. Für **stehende** Anlage ist `ForceHeater`
dagegen der einzige Weg, den Heizstab überhaupt anzufordern, und das ist der
Fall, der im Notbetrieb und bei einer Störung interessiert. Deshalb ist SET39
in 3.17.0 mitgebaut worden statt zurückgestellt.

## 2. Abgrenzung — was ausdrücklich NICHT dazugehört

**Keine Anbindung an den Notbetrieb.** Entscheidung vom 2026-08-28: Das würde
den Notbetrieb nur unnötig komplex machen, und Komforteinbußen im Notbetrieb
sind akzeptiert. `NOTBETRIEB_WERTE_HEIZEN[]` in `src/notbetrieb.h` bleibt
unverändert.

**`SetReset` (Byte 8, Bit 0) wird nicht übernommen.** Das Kommando quittiert
verriegelte Fehlercodes aus der Ferne (Äquivalent der Reset-Taste am
Bedienteil). Läuft die Anlage in einen Fehler, der einen Reset braucht, ist ein
Mensch am Bedienpanel die richtige Antwort — nicht ein MQTT-Topic. Byte 8 Bit 0
bleibt frei; wir belegen dort nur `0x02` (SET12 `ForceDefrost`) und `0x04`
(SET13 `ForceSterilization`).

**Der Warmwasser-Heizstab ist nicht das Ziel.** Für DHW hängt an dieser Anlage
bereits ein externer Heizstab. SET38 `DHWHeaterState` ist trotzdem gebaut, weil
Byte 9 beide Felder trägt und der Hosttest dadurch den Nachbarschutz belegen
kann — gebraucht wird es an dieser Anlage nicht.

**Kompatibilität der SET-Nummern mit HeishaMon ist kein Ziel.** Unsere
Nummerierung ist ab SET16 gegenüber dem Upstream verschoben und bleibt es. Was
zählt, ist Byte, Funktion und Name. Vergeben wurden **SET37 – SET39**; nächste
freie Nummer ist damit SET40.

## 3. Was belegt ist

Quelle ist `commands.cpp` des aktuellen Upstreams (`Egyras/HeishaMon`), im
Original nachgelesen — nicht über die TypeScript-Portierung.

### Byte 9 — Heizstab freigeben (das Vorhaben)

Kommando | Werte | Bits | Maske | Rücklesen
:--- | :--- | :--- | :--- | :---
SET37 `RoomHeaterState` | `1` = blockiert, `2` = frei | `getBit7and8` | `0x03` | TOP59 `Room_Heater_State`
SET38 `DHWHeaterState` | `4` = blockiert, `8` = frei | `getBit5and6` | `0x0C` | TOP58 `DHW_Heater_State`

Byte 9 war in `setCommands[]` bis 3.16.0 gar nicht belegt. Beide Rücklese-Topics
existierten schon und zeigen `Blocked` / `Free`.

### Byte 5 — ForceHeater (SET39)

Das Kommando schreibt Byte 5 auf `4` (aus) oder `8` (an) — die Bits, die
`getBit5and6` liest, also **Maske `0x0C`**. Rücklesen über TOP68
`Force_Heater_State`. Byte 5 war schon belegt: SET2 `HolidayMode` mit
Maske `0x30`, kein Überlapp. `ProtocolByteDecrypt.md` Zeile 10 führt für Byte 5
zusätzlich „Dry Concrete" auf Bits 7+8; auch das bleibt unberührt.

### Die Umsetzung ist je eine Tabellenzeile

`setCommands[]` in [`src/commands.cpp`](src/commands.cpp) trägt Byte, Maske,
Umrechnung, Name und Grenzen in einer Zeile; `subscribe_set_topics()` und
`set_command_range()` laufen über dieselbe Tabelle:

```c
    {37,  9, 0x03, CONV_MUL_INC, "RoomHeaterState",     0,   1,   1}, // blockiert=1 frei=2
    {38,  9, 0x0C, CONV_MUL_INC, "DHWHeaterState",      0,   1,   4}, // blockiert=4 frei=8
    {39,  5, 0x0C, CONV_MUL_INC, "ForceHeater",         0,   1,   4}, // aus=4 an=8
```

So stehen sie seit 3.17.0 im Code — mit den erklärenden Kommentarblöcken
davor, die hier in Abschnitt 1 und 4 begründet sind.

`CONV_MUL_INC` ist `(Wert + 1) * param` und trifft alle drei Wertepaare exakt —
dasselbe Muster wie SET2 `HolidayMode` (`16`/`32`). Nachgerechnet:
`RoomHeaterState 0` → `1*1 = 1`, `RoomHeaterState 1` → `2*1 = 2`.

**Mit geändert:** der Kommentarblock „Why the mask column exists" in
`commands.cpp` listet die geteilten Bytes auf. Byte 9 ist dort eine neue Zeile,
Byte 5 hat `ForceHeater 0x0C` dazubekommen. Wer die Liste nicht pflegt, nimmt
der nächsten Session die einzige Übersicht darüber, welche Kommandos sich ein
Byte teilen.

## 4. Nebenwirkungen der Freigabe

**Die Freigabe ändert das Abtauverhalten.** Laut Servicehandbuch 12.6.2 läuft
der Raumheizstab während der Abtauung mit — allerdings nur, wenn der Backup-
Heizer im Custom Setup überhaupt freigegeben ist. Zweck ist der Schutz des
Plattenwärmetauschers vor Eisbildung; ausgelöst wird es bei niedrigem Vorlauf,
tiefer Außentemperatur oder niedrigem Rücklauf während der Abtauung, und es
hängt nicht am Heizstab-Knopf der Fernbedienung.

Für dieses Vorhaben ist das kein Gegenargument — die Freigabe **soll** die
Regelgröße sein —, aber es hat eine Folge für die Auswertung: In einer
Freigabephase läuft der Heizstab nicht nur dann, wenn die Steuerung ihn haben
wollte, sondern auch bei jeder Abtauung, die in diese Phase fällt. Wer den
Nutzen der gezielten Zuschaltung beziffern will, muss den Abtau-Anteil davon
trennen. Da der Heizstab heute gesperrt ist, läuft er auch beim Abtauen nicht
mit; der Vergleich „vorher/nachher" misst also beides zusammen.

### Die Anlage entscheidet weiter mit

Die Freigabe ist Bedingung (a) von sechs. Die übrigen fünf bleiben in Kraft,
und daraus folgt für eine Steuerung, die gezielt freigibt:

* **Die Freigabe wirkt nicht sofort.** Nach ihr müssen erst 30 min
  Kompressorlauf, 9 min Pumpenlauf und 4 K Vorlaufabweichung zusammenkommen.
  Die Steuerung muss also vorausschauend freigeben, nicht reaktiv im Moment des
  Bedarfs.
* **Schnelles Ein/Aus bringt nichts.** Nach jedem Abschalten des Heizstabs
  sperrt die Anlage ihn 20 Minuten.
* **SET20 muss passen.** Die Außentemperaturschwelle
  (`HeaterOnOutdoorTemp`, TOP78) ist Bedingung (d). Steht sie so, dass die
  Anlage den Heizstab nie in Betracht zieht, bleibt die Freigabe wirkungslos.
  Bei einem seit Jahren deaktivierten Heizstab ist gut möglich, dass der Wert
  nie bewusst gesetzt wurde — **vor dem ersten Versuch prüfen**.
* **Die Bedingung „4 K unter Soll" ist vorhersagbar.** Sie lässt sich aus
  `Main_Outlet_Temp` (TOP6) und `Main_Target_Temp` (TOP7) mitrechnen. Die
  Steuerung kann damit erkennen, ob die Anlage bei freigegebenem Stab
  überhaupt zuschalten würde — und nur dann freigeben.

**Sperren unabhängig vom Schalter.** Der Heizstab läuft laut Handbuch generell
nicht, wenn Vorlauf- oder Rücklaufsensor gestört sind, der Strömungswächter
gestört ist oder die Umwälzpumpe steht. Das ist die Temperatur- und
Durchflussüberwachung, die die Anlage selbst mitführt — ein freigegebener
Heizstab ist damit kein unbeaufsichtigter 3-kW-Tauchsieder.

## 5. Risiken

**Elektrische Leistung — geklärt.** 3 kW fix je Stufe, also bis zu 6 kW
zusätzlich, wenn beide Stufen gleichzeitig zuschalten. Das ist überschaubar,
setzt die Zuschaltung aber unter dieselbe Beobachtung wie alles andere in der
Kaskade: Ob beide Stufen den Heizstab gleichzeitig freigegeben bekommen sollen,
ist eine Entscheidung, keine Selbstverständlichkeit.

**Der 5-min-Re-Assert der Kaskadensteuerung.** Läufe nur im Ruhefenster, sonst
läuft die Messung gegen die Steuerung. Jeden Eingriff einzeln aufrufen — ein
Abbruch mitten in einer Befehlskette greift an dieser Anlage nicht zuverlässig.

**Byte 9 nicht ohne Maske schreiben.** Ein Kommando, das Byte 9 als Ganzes
setzt, löscht das jeweils andere Feld — der Raumheizstab würde die DHW-Freigabe
mit umlegen. Die Maskenspalte deckt das ab, solange sie richtig gesetzt ist.
Zwei Kommandos im selben 500-ms-Sammelfenster sind damit unkritisch; Lauf 4 des
Byte-28-Vorhabens hat gezeigt, dass die Anlage zwei gleichzeitig wechselnde
Bitfelder annimmt.

**`ForceHeater` ist ein Zustand, kein Impuls** wie SET12 `ForceDefrost`.
Praktisch entschärft die Anlage das selbst — die Betriebsart endet mit dem
Fehler, mit „Betrieb aus" oder mit einem Netz-Reset —, aber ein gesetztes
Kommando, das niemand zurücknimmt, bleibt ein loses Ende. Wer SET39 benutzt,
plant das Zurücknehmen mit ein.

## 6. Was offen ist

1. **Nimmt die H-Serie Byte 9 an?** In `ProtocolByteDecrypt.md` Zeile 14 ist das
   Byte ohne Serieneinschränkung dokumentiert, belegt ist es für unsere
   WH-MDC05H3E5 aber nicht. Das ist die einzige echte Unbekannte — wie seinerzeit
   bei Byte 28, und dort hat die Anlage angenommen.

2. **Was heißt „frei" wirklich?** Unser Code zeigt `Blocked`/`Free`,
   `ProtocolByteDecrypt.md` Zeile 14 schreibt für dieselben Bits „heater
   off/on". Nach Kapitel 12.6.1 ist „Freigabe" richtig: Bedingung (a) ist der
   Schalter, die übrigen fünf Bedingungen entscheidet die Anlage. M2 im
   Messplan prüft, dass das Bit ankommt — ob „frei" auch heißt, dass der Stab
   je läuft, zeigt erst das Winterexperiment über TOP60 und TOP90.

3. ~~**Was steht heute in Byte 9?**~~ **Beantwortet:** Der Heizstab ist an
   beiden Anlagen deaktiviert, Byte 9 steht also auf blockiert. Damit ist auch
   klar, dass M1 im Messplan den Ist-Zustand trifft und M2 die eigentliche
   Änderung ist.

4. **Die Feineinstellung fehlt uns vermutlich dauerhaft.** Startverzögerung
   (Byte 104), Start-Delta (105) und Stopp-Delta (106) sind in
   `ProtocolByteDecrypt.md` Zeilen 109–111 als „J/K/L series" markiert. Bei
   H-Serie stehen die Bytes erfahrungsgemäß auf `00`. Ebenfalls aus dem
   Mitschnitt zu klären. Wenn sie leer sind, bleibt als einziger Stellhebel die
   Außentemperaturschwelle SET20 — die reicht für das Ziel.

## 7. Reihenfolge der Umsetzung

Die ursprüngliche Staffelung („erst SET37, den Rest nur bei Bedarf") ist mit
der Owner-Entscheidung vom 2026-08-28 hinfällig — gebaut sind alle drei.

Schritt | Stand
:--- | :---
**1.** Alle drei Kommandos in `setCommands[]`, Masken im Kommentarblock nachgezogen | **erledigt, 3.17.0**
**2.** Hosttest `test/byte9_test.cpp`, in der CI | **erledigt, 3.17.0**
**3.** `MQTT-Topics.md`, `SET-TOP-Zuordnung.md`, `README.md`, `test/README.md`, Changelog | **erledigt, 3.17.0**
**4.** Mitschnitt auswerten: SET20 / TOP78 und Byte 104–106 im Ist-Zustand (kein Eingriff) | offen
**5.** OTA auf Stufe 1, Abnahme über `/tablerefresh`, M0–M2 messen (Abschnitt 8) | offen
**6.** Winterexperiment vorbereiten: Freigabekriterium in der Steuerung festlegen, Mitschrieb einrichten | offen

Byte 9 ist ohne Messung bekannt: blockiert, an beiden Anlagen.

## 8. Messplan

Muster wie beim Byte-28-Vorhaben: Ausgangszustand sichern, ein Bit ändern,
zurücklesen, zurückstellen. Alles an **Stufe 1**, im Ruhefenster.

Schritt | Kommando | Erwartung Byte | Erwartung Rücklesen
:--- | :--- | :--- | :---
M0 | — | Byte 9 notieren | TOP58/59 notieren
M1 | `RoomHeaterState 0` | Byte 9 Bits 0+1 → `01` | TOP59 → `Blocked`
M2 | `RoomHeaterState 1` | Byte 9 Bits 0+1 → `10` | TOP59 → `Free`, **TOP58 unverändert**

M2 ist der eigentliche Nachweis: dass das Nachbarfeld im selben Byte stehen
bleibt, ist der Punkt, an dem eine falsche Maske auffällt. Die Bitrechnung
dahinter ist seit 3.17.0 ohne Gerät belegt
([`test/byte9_test.cpp`](test/byte9_test.cpp)) — M0–M2 beantworten die eine
Frage, die ein Hosttest nicht beantworten kann: ob die Wärmepumpe Byte 9
annimmt.

**SET39 `ForceHeater` misst sich nicht im selben Fenster.** Nach dem Befund vom
2026-08-28 nimmt die Anlage ihn nur bei **ausgeschalteter** Wärmepumpe an — das
ist kein Ruhefenster-Lauf nebenbei, sondern ein eigener Termin an einer
stehenden Stufe:

Schritt | Kommando | Erwartung Byte | Erwartung Rücklesen
:--- | :--- | :--- | :---
M3 | — (WP aus) | Byte 5 notieren | TOP68, TOP19, TOP13 notieren
M4 | `ForceHeater 1` | Byte 5 Bits 5+6 → `10` | TOP68 → `Active`, **TOP19 und TOP13 unverändert**
M5 | `ForceHeater 0` | Byte 5 Bits 5+6 → `01` | TOP68 → `Inactive`

Auch hier gilt: zurückstellen, was M3 notiert hat.

Ausgangszustand aus M0 nach dem Lauf wiederherstellen — der Heizstab gehört
danach wieder auf blockiert, bis das Winterexperiment vorbereitet ist.

### Das Winterexperiment

Ob der Heizstab tatsächlich zuschaltet, lässt sich im Messfenster **nicht**
prüfen: dafür müssten 30 min Kompressorlauf, die Außentemperaturschwelle und
4 K Vorlaufabweichung zusammenkommen. Das ist eine Beobachtung über eine
Frostphase, keine Messung.

Mitzuschreiben:

Topic | wofür
:--- | :---
`Room_Heater_Operations_Hours` (TOP90) | der belastbarste Zeuge — zählt nur, wenn der Stab wirklich lief
`Room_Heater_State` (TOP59) | wann die Steuerung freigegeben hat
`Heat_Power_Consumption` (TOP16) | Leistungsaufnahme, zeigt den 3-kW-Sprung
`Defrosting_State` (TOP26) | trennt den Abtau-Anteil vom geregelten Anteil (Abschnitt 4)
`Main_Outlet_Temp` / `Main_Target_Temp` (TOP6/7) | die 4-K-Bedingung, gegen die freigegeben wurde
`Outside_Temp` (TOP14) | Bezug zur Außentemperaturschwelle

Die Frage, die das Experiment beantworten soll, ist eine Komfortfrage, keine
Verbrauchsfrage: Kommt die Raumtemperatur in der Frostphase spürbar früher
nach? Der Mehrverbrauch steht ohnehin fest — 3 kW mal Laufzeit aus TOP90.

**Auswertung nicht vergessen:** Ohne TOP26 daneben ist der Abtau-Anteil in TOP90
nicht vom geregelten Anteil zu trennen, und dann misst das Experiment etwas
anderes als das, was gesteuert wurde.

**Werkzeuge:** `test/frame_diff.py` für die Bytes (Ausgabe ist **hexadezimal**),
`test/top_watch.py` für die Rückmeldungen, `test/mqtt_pub.py` zum Senden.
