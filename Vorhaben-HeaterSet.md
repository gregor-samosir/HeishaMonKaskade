# Vorhaben: Heizstab freigeben (Byte 9), ForceHeater nachrangig (Byte 5)

Übergabe für eine eigene Session. Ziel ist **ein** neues Set-Kommando, mit dem
sich der interne Heizstab für den Heizbetrieb freigeben lässt — heute geht das
nur am Bedienterminal.

**Stand dieser Datei:** 2026-08-28, Firmware 3.16.0 auf beiden Stufen.
Planungsstand, nichts gebaut und nichts an der Anlage gemessen.

**Herkunft:** Fund beim Abgleich von `TobiasHanss/ioBroker.heishamon` und
`Egyras/HeishaMon` gegen unseren Stand am 2026-08-28. Das ioBroker-Repo brachte
nichts Neues (1:1-Portierung des Upstreams), der aktuelle Upstream dagegen drei
Set-Kommandos, die es bei uns nicht gibt.

---

## 1. Das Ziel — und warum `ForceHeater` es nicht erreicht

Motiv ist ein **Komfortgewinn im Heizbetrieb**, keine Notwendigkeit. Der
Heizstab ist an beiden Anlagen heute **deaktiviert**, und das aus gutem Grund:
Selbst bei −15 °C Außentemperatur hat sich gezeigt, dass die große Masse der
Fußbodenheizung ihn nicht braucht. Er hat 3 kW fix je Stufe — er kann
mithelfen, mehr nicht.

Interessant ist deshalb nicht die Dauerfreigabe, sondern das **gezielte
Zuschalten**: dann, wenn die Wassertemperaturen es laut Kaskadensteuerung
erlauben. Ob das den Komfort spürbar hebt, müssen Experimente im Winter zeigen —
das Vorhaben schafft nur die Voraussetzung dafür.

Das naheliegende Kommando dafür wäre `ForceHeater` gewesen. **Es ist das
falsche.** Das Panasonic-Servicehandbuch (Kapitel 12.9, liegt in
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
bereits ein externer Heizstab. `SetDHWHeaterState` steht unten nur der
Vollständigkeit halber, weil Byte 9 beide Felder trägt.

**Kompatibilität der SET-Nummern mit HeishaMon ist kein Ziel.** Unsere
Nummerierung ist ab SET16 gegenüber dem Upstream verschoben und bleibt es. Was
zählt, ist Byte, Funktion und Name. Nächste freie Nummer bei uns: **SET37**.

## 3. Was belegt ist

Quelle ist `commands.cpp` des aktuellen Upstreams (`Egyras/HeishaMon`), im
Original nachgelesen — nicht über die TypeScript-Portierung.

### Byte 9 — Heizstab freigeben (das Vorhaben)

Funktion | Werte | Bits | Maske | Rücklesen
:--- | :--- | :--- | :--- | :---
`set_room_heater_state` | `1` = blockiert, `2` = frei | `getBit7and8` | `0x03` | TOP59 `Room_Heater_State`
`set_dhw_heater_state` | `4` = blockiert, `8` = frei | `getBit5and6` | `0x0C` | TOP58 `DHW_Heater_State`

Byte 9 ist in `setCommands[]` bisher gar nicht belegt. Beide Rücklese-Topics
existieren schon und zeigen `Blocked` / `Free`.

### Byte 5 — ForceHeater (nachrangig, siehe Abschnitt 1)

`set_force_heater` schreibt Byte 5 auf `4` (aus) oder `8` (an) — die Bits, die
`getBit5and6` liest, also **Maske `0x0C`**. Rücklesen über TOP68
`Force_Heater_State`. Byte 5 ist bei uns schon belegt: SET2 `HolidayMode` mit
Maske `0x30`, kein Überlapp. `ProtocolByteDecrypt.md` Zeile 10 führt für Byte 5
zusätzlich „Dry Concrete" auf Bits 7+8; auch das bleibt unberührt.

### Die Umsetzung ist je eine Tabellenzeile

`setCommands[]` in [`src/commands.cpp`](src/commands.cpp) trägt Byte, Maske,
Umrechnung, Name und Grenzen in einer Zeile; `subscribe_set_topics()` und
`set_command_range()` laufen über dieselbe Tabelle:

```c
    {37,  9, 0x03, CONV_MUL_INC, "SetRoomHeaterState",  0,   1,   1}, // blockiert=1 frei=2
    {38,  9, 0x0C, CONV_MUL_INC, "SetDHWHeaterState",   0,   1,   4}, // blockiert=4 frei=8
    {39,  5, 0x0C, CONV_MUL_INC, "SetForceHeater",      0,   1,   4}, // aus=4 an=8
```

`CONV_MUL_INC` ist `(Wert + 1) * param` und trifft alle drei Wertepaare exakt —
dasselbe Muster wie SET2 `HolidayMode` (`16`/`32`). Nachgerechnet:
`RoomHeaterState 0` → `1*1 = 1`, `RoomHeaterState 1` → `2*1 = 2`.

**Mit zu ändern:** der Kommentarblock „Why the mask column exists" in
`commands.cpp` listet die geteilten Bytes auf. Byte 9 ist eine neue Zeile, Byte 5
bekommt `ForceHeater 0x0C` dazu, falls es gebaut wird. Wer die Liste nicht
pflegt, nimmt der nächsten Session die einzige Übersicht darüber, welche
Kommandos sich ein Byte teilen.

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

**Falls `ForceHeater` doch gebaut wird:** Es ist ein Zustand, kein Impuls wie
SET12 `ForceDefrost`. Praktisch entschärft die Anlage das selbst — die
Betriebsart endet mit dem Fehler, mit „Betrieb aus" oder mit einem
Netz-Reset —, aber ein gesetztes Kommando, das niemand zurücknimmt, bleibt ein
loses Ende.

## 6. Was offen ist

1. **Nimmt die H-Serie Byte 9 an?** In `ProtocolByteDecrypt.md` Zeile 14 ist das
   Byte ohne Serieneinschränkung dokumentiert, belegt ist es für unsere
   WH-MDC05H3E5 aber nicht. Das ist die einzige echte Unbekannte — wie seinerzeit
   bei Byte 28, und dort hat die Anlage angenommen.

2. **Was heißt „frei" wirklich?** Unser Code zeigt `Blocked`/`Free`,
   `ProtocolByteDecrypt.md` Zeile 14 schreibt für dieselben Bits „heater
   off/on". Nach Kapitel 12.6.1 ist „Freigabe" richtig: Bedingung (a) ist der
   Schalter, die übrigen fünf Bedingungen entscheidet die Anlage. M2 im
   Messplan prüft das.

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

1. Mitschnitt auswerten: SET20 / TOP78 und Byte 104–106 im Ist-Zustand
   (kein Eingriff). Byte 9 ist bekannt: blockiert.
2. SET37 `RoomHeaterState` einbauen, M0–M2 messen.
3. Winterexperiment vorbereiten (Abschnitt 8): Freigabekriterium in der
   Steuerung festlegen, Mitschrieb einrichten.
4. SET38 `ForceHeater` und SET39 `DHWHeaterState` nur, falls sich aus 2. oder 3.
   ein konkreter Grund ergibt. Nach Abschnitt 1 ist der für `ForceHeater` nicht
   in Sicht.
5. `MQTT-Topics.md`, `SET-TOP-Zuordnung.md`, Changelog in `src/version.h`
   nachziehen; Hosttest wie bei den übrigen Set-Kommandos.

## 8. Messplan

Muster wie beim Byte-28-Vorhaben: Ausgangszustand sichern, ein Bit ändern,
zurücklesen, zurückstellen. Alles an **Stufe 1**, im Ruhefenster.

Schritt | Kommando | Erwartung Byte | Erwartung Rücklesen
:--- | :--- | :--- | :---
M0 | — | Byte 9 notieren | TOP58/59 notieren
M1 | `RoomHeaterState 0` | Byte 9 Bits 0+1 → `01` | TOP59 → `Blocked`
M2 | `RoomHeaterState 1` | Byte 9 Bits 0+1 → `10` | TOP59 → `Free`, **TOP58 unverändert**

M2 ist der eigentliche Nachweis: dass das Nachbarfeld im selben Byte stehen
bleibt, ist der Punkt, an dem eine falsche Maske auffällt.

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
