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

Motiv ist der **Heizbetrieb in harten Frostphasen**: Der Heizstab hat 3 kW fix
(beide Stufen gleich). Er kann in einer Frostphase mithelfen, mehr nicht — es
geht um Unterstützung, nicht um eine zweite Wärmequelle.

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
    {37,  9, 0x03, CONV_MUL_INC, "RoomHeaterState",  0,   1,   1}, // blockiert=1 frei=2
    {38,  5, 0x0C, CONV_MUL_INC, "ForceHeater",      0,   1,   4}, // aus=4 an=8
    {39,  9, 0x0C, CONV_MUL_INC, "DHWHeaterState",   0,   1,   4}, // blockiert=4 frei=8
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

Das ist ein Argument **für** die Freigabe, aber es heißt auch: Wer die Freigabe
per MQTT dynamisch umschaltet, schaltet den Abtau-Schutz mit um. Eine
Automatik, die den Heizstab „nur bei Frost" freigibt, nimmt ihn genau in den
Abtauzyklen weg, in denen er gedacht ist. Sprich für die Nutzung: eher dauerhaft
freigeben und die Zuschaltung über die Außentemperaturschwelle (SET20) regeln,
als die Freigabe selbst als Regelgröße zu benutzen.

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

3. **Was steht heute in Byte 9?** Ist der Raumheizstab an dieser Anlage
   überhaupt freigegeben? Ohne Eingriff aus einem Mitschnitt ablesbar und der
   sinnvolle erste Schritt — steht er schon auf `Free`, ist das Vorhaben
   allenfalls noch Komfort.

4. **Die Feineinstellung fehlt uns vermutlich dauerhaft.** Startverzögerung
   (Byte 104), Start-Delta (105) und Stopp-Delta (106) sind in
   `ProtocolByteDecrypt.md` Zeilen 109–111 als „J/K/L series" markiert. Bei
   H-Serie stehen die Bytes erfahrungsgemäß auf `00`. Ebenfalls aus dem
   Mitschnitt zu klären. Wenn sie leer sind, bleibt als einziger Stellhebel die
   Außentemperaturschwelle SET20 — die reicht für das Ziel.

## 7. Reihenfolge der Umsetzung

1. Mitschnitt auswerten: Byte 9 und 104–106 im Ist-Zustand (kein Eingriff).
2. SET37 `RoomHeaterState` einbauen, M0–M2 messen.
3. Nutzung festlegen: dauerhaft freigeben und über SET20 steuern (siehe
   Abschnitt 4), oder Freigabe als Regelgröße — dann Abtau-Nebenwirkung
   einplanen.
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

Ob der Heizstab danach tatsächlich zuschaltet, lässt sich **nicht** im
Messfenster prüfen — dafür müssten 30 min Kompressorlauf, die
Außentemperaturschwelle und 4 K Vorlaufabweichung zusammenkommen. Das ist eine
Beobachtung für die nächste Frostphase, keine Messung: `Room_Heater_State`
(TOP59), `Heat_Power_Consumption` (TOP16) und die Betriebsstunden
`Room_Heater_Operations_Hours` (TOP90) mitschreiben und hinterher auswerten.
TOP90 ist dabei der belastbarste Zeuge — er zählt nur, wenn der Stab wirklich
lief.

Ausgangszustand aus M0 nach dem Lauf wiederherstellen.

**Werkzeuge:** `test/frame_diff.py` für die Bytes (Ausgabe ist **hexadezimal**),
`test/top_watch.py` für die Rückmeldungen, `test/mqtt_pub.py` zum Senden.
