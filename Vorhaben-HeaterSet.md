# Vorhaben: Heizstab schalten (Byte 5 und Byte 9)

Übergabe für eine eigene Session. Ziel sind zwei bis drei neue Set-Kommandos,
mit denen sich der **interne Heizstab für den Heizbetrieb** freigeben und
erzwingen lässt — heute geht das nur am Bedienterminal.

**Stand dieser Datei:** 2026-08-28, Firmware 3.16.0 auf beiden Stufen.
Planungsstand, noch nichts gebaut und nichts an der Anlage gemessen.

**Herkunft:** Fund beim Abgleich von `TobiasHanss/ioBroker.heishamon` und
`Egyras/HeishaMon` gegen unseren Stand am 2026-08-28. Das ioBroker-Repo selbst
brachte nichts Neues (1:1-Portierung des Upstreams), der aktuelle Upstream
dagegen drei Set-Kommandos, die es bei uns nicht gibt.

---

## 1. Warum das lohnt

In harten Frostphasen reicht der Kompressor allein nicht immer aus. Am
Bedienterminal ist damit früher schon manuell experimentiert worden; der
Handgriff ist aber genau dann fällig, wenn er am wenigsten passt — nachts, bei
Frost, und nur wenn jemand im Haus ist.

Das Rücklesen existiert bereits vollständig, der Nachweis kostet also nichts
extra:

Topic | Nr | Byte | Dekoder | Klartext
:--- | :--- | ---: | :--- | :---
`Force_Heater_State` | TOP68 | 5 | `getBit5and6` | `Inactive` / `Active`
`Room_Heater_State` | TOP59 | 9 | `getBit7and8` | `Blocked` / `Free`
`DHW_Heater_State` | TOP58 | 9 | `getBit5and6` | `Blocked` / `Free`

## 2. Abgrenzung — was ausdrücklich NICHT dazugehört

**`SetReset` (Byte 8, Bit 0) wird nicht übernommen.** Das Kommando quittiert
verriegelte Fehlercodes aus der Ferne (Äquivalent der Reset-Taste am
Bedienteil). Entscheidung vom 2026-08-28: Läuft die Anlage in einen Fehler, der
einen Reset braucht, ist ein Mensch am Bedienpanel die richtige Antwort — nicht
ein MQTT-Topic. Byte 8 Bit 0 bleibt frei; wir belegen dort nur `0x02`
(SET12 `ForceDefrost`) und `0x04` (SET13 `ForceSterilization`).

**Der Warmwasser-Heizstab ist nicht das Ziel.** Für DHW gibt es an dieser
Anlage bereits einen externen Heizstab. `SetDHWHeaterState` ist deshalb
nachrangig — siehe aber Abschnitt 4, Byte 9 trägt beide Felder.

**Kompatibilität der SET-Nummern mit HeishaMon ist kein Ziel.** Unsere
Nummerierung ist ab SET16 gegenüber dem Upstream verschoben und bleibt es. Was
zählt, ist Byte, Funktion und Name. Die nächsten freien Nummern bei uns sind
**SET37 aufwärts**.

## 3. Was belegt ist

Quelle ist `commands.cpp` des aktuellen Upstreams (`Egyras/HeishaMon`), im
Original nachgelesen — nicht über die TypeScript-Portierung.

### Byte 5 — Heizstab erzwingen

`set_force_heater` schreibt Byte 5 auf `4` (aus) oder `8` (an). Das sind die
Bits, die `getBit5and6` liest, also **Maske `0x0C`**. Upstream-Beschreibung:
„same as the heater button on the remote", Rückmeldung in TOP68.

Byte 5 ist bei uns schon belegt: SET2 `HolidayMode` mit Maske `0x30`. Kein
Überlapp — genau dafür gibt es die Maskenspalte in `setCommands[]`.
`ProtocolByteDecrypt.md` Zeile 10 führt für Byte 5 zusätzlich „Dry Concrete"
auf Bits 7+8; auch das bleibt unberührt.

### Byte 9 — Heizstab freigeben

Beide Felder liegen im selben Byte:

Funktion | Werte | Bits | Maske | Rücklesen
:--- | :--- | :--- | :--- | :---
`set_room_heater_state` | `1` = blockiert, `2` = frei | `getBit7and8` | `0x03` | TOP59
`set_dhw_heater_state` | `4` = blockiert, `8` = frei | `getBit5and6` | `0x0C` | TOP58

Byte 9 ist in `setCommands[]` bisher gar nicht belegt.

**Der Zusammenhang mit Abschnitt 3.1:** `ForceHeater` erzwingt den Heizstab,
aber die Freigabe in Byte 9 ist die Voraussetzung dafür. Ohne freigegebenen
Raumheizstab läuft das Force-Kommando vermutlich ins Leere. Das ist die
Arbeitshypothese, nicht gemessen — Schritt M2 in Abschnitt 7 klärt sie.

### Die Umsetzung ist je eine Tabellenzeile

`setCommands[]` in [`src/commands.cpp`](src/commands.cpp) trägt Byte, Maske,
Umrechnung, Name und Grenzen in einer Zeile; `subscribe_set_topics()` und
`set_command_range()` laufen über dieselbe Tabelle, ein neues Kommando ist
damit wirklich nur diese Zeile:

```c
    {37,  5, 0x0C, CONV_MUL_INC, "ForceHeater",      0,   1,   4}, // aus=4 an=8
    {38,  9, 0x03, CONV_MUL_INC, "RoomHeaterState",  0,   1,   1}, // blockiert=1 frei=2
    {39,  9, 0x0C, CONV_MUL_INC, "DHWHeaterState",   0,   1,   4}, // blockiert=4 frei=8
```

`CONV_MUL_INC` ist `(Wert + 1) * param` und trifft alle drei Wertepaare exakt —
dasselbe Muster wie SET2 `HolidayMode` (`16`/`32`). Nachgerechnet:
`ForceHeater 0` → `1*4 = 4`, `ForceHeater 1` → `2*4 = 8`.

**Mit zu ändern:** der Kommentarblock „Why the mask column exists" in
`commands.cpp` listet die geteilten Bytes auf. Byte 5 bekommt dort
`ForceHeater 0x0C` dazu, Byte 9 ist eine neue Zeile. Wer die Liste nicht
pflegt, nimmt der nächsten Session die einzige Übersicht darüber, welche
Kommandos sich ein Byte teilen.

## 4. Was offen ist

1. **Nimmt die H-Serie die Bytes überhaupt an?** Byte 5 und Byte 9 sind in
   `ProtocolByteDecrypt.md` ohne Serieneinschränkung dokumentiert (anders als
   Byte 104–106, siehe Punkt 3). Belegt ist das für unsere WH-MDC05H3E5 aber
   nicht. Das ist die einzige echte Unbekannte — genau wie seinerzeit bei
   Byte 28, und dort hat die Anlage angenommen.

2. **Was heißt „frei" wirklich?** Unser Code zeigt `Blocked`/`Free`,
   `ProtocolByteDecrypt.md` Zeile 14 schreibt für dieselben Bits
   „DHW heater off/on" und „Water heater off/on". Ob die Freigabe den Heizstab
   nur erlaubt oder ihn einschaltet, entscheidet, ob `ForceHeater` überhaupt
   gebraucht wird. M2 beantwortet das.

3. **Die Feineinstellung fehlt uns vermutlich dauerhaft.** Startverzögerung
   (Byte 104), Start-Delta (105) und Stopp-Delta (106) sind in
   `ProtocolByteDecrypt.md` Zeilen 109–111 als „J/K/L series" markiert. Bei
   H-Serie stehen die Bytes erfahrungsgemäß auf `00`. Prüfen lässt sich das
   ohne jeden Eingriff aus einem Mitschnitt. Wenn sie leer sind, ist die
   Heizstab-Logik der Anlage nicht parametrierbar — dann bleibt nur die
   Außentemperaturschwelle TOP78 / SET20 `HeaterOnOutdoorTemp`, die wir schon
   haben.

4. **Wie groß ist der interne Heizstab?** `ProtocolByteDecrypt.md` Zeile 30:
   Byte 25, Bits 5+6 — `b01` = 3 kW, `b10` = 6 kW, `b11` = 9 kW. Ebenfalls aus
   dem Mitschnitt ablesbar, ohne Eingriff, und Voraussetzung für Abschnitt 5.
   Dieselbe Zeile trägt in Bits 7+8 auch, ob der DHW-Heizstab intern oder
   extern konfiguriert ist — Gegenprobe zum vorhandenen externen Heizstab.

5. **Was passiert beim Neustart?** Siehe Abschnitt 5.

## 5. Risiken

**Elektrische Leistung — vor dem ersten Lauf zu klären.** Zwei Stufen, beide
Heizstäbe gleichzeitig frei, macht je nach Bestückung bis zu 2 × 9 kW zusätzlich
zur Kompressorleistung. Ob Hausanschluss und Absicherung das tragen, ist keine
Frage, die man beim Messen herausfindet. Punkt 4 aus Abschnitt 4 liefert die
Zahl; danach ist zu entscheiden, ob überhaupt beide Stufen das Kommando
bekommen oder nur eine.

**`ForceHeater` ist ein Zustand, kein Impuls.** Anders als SET12
`ForceDefrost` bleibt der Heizstab an, bis ihn jemand abschaltet. Fällt die
Node-RED-Steuerung aus, während `ForceHeater` gesetzt ist, heizt der Stab
weiter — das ist der teuerste denkbare Fehlerfall dieses Vorhabens. Zu klären,
bevor das Kommando in einen Automatismus wandert:
* Überlebt der Zustand einen Neustart der Wärmepumpe? (Byte 5 zurücklesen)
* Überlebt er einen Neustart der Firmware? (er sollte — wir halten ihn nicht)
* Braucht es eine Zwangsabschaltung nach n Minuten in der Firmware, analog zum
  Sendefenster? Das wäre neue Logik, kein reines Set-Kommando mehr.

**Der 5-min-Re-Assert der Kaskadensteuerung.** Läufe nur im Ruhefenster, sonst
läuft die Messung gegen die Steuerung. Gilt wie bei allen Eingriffen an dieser
Anlage.

**Byte 9 nicht ohne Maske schreiben.** Ein Kommando, das Byte 9 als Ganzes
setzt, löscht das jeweils andere Feld — der Raumheizstab würde die
DHW-Freigabe mit umlegen. Die Maskenspalte deckt das ab, solange sie richtig
gesetzt ist. Zwei Kommandos im selben 500-ms-Sammelfenster sind damit
unkritisch; Lauf 4 des Byte-28-Vorhabens hat gezeigt, dass die Anlage zwei
gleichzeitig wechselnde Bitfelder annimmt.

## 6. Reihenfolge der Umsetzung

1. Mitschnitt auswerten: Byte 5, 9, 25, 104–106 im Ist-Zustand (kein Eingriff).
2. Abschnitt 5 entscheiden: Leistung geklärt, eine oder beide Stufen?
3. SET38 `RoomHeaterState` einbauen, M1/M2 messen.
4. Erst danach SET37 `ForceHeater`, M3 messen.
5. SET39 `DHWHeaterState` nur, falls sich aus M2 ein Grund dafür ergibt.
6. `MQTT-Topics.md`, `SET-TOP-Zuordnung.md`, Changelog in `src/version.h`
   nachziehen; Hosttest wie bei den übrigen Set-Kommandos.

## 7. Messplan

Muster wie beim Byte-28-Vorhaben: Ausgangszustand sichern, ein Bit ändern,
zurücklesen, zurückstellen. Alles an **Stufe 1**, im Ruhefenster, bei stehender
Anlage. Jeder Eingriff einzeln aufrufen — ein Abbruch mitten in einer
Befehlskette greift an dieser Anlage nicht zuverlässig.

Schritt | Kommando | Erwartung Byte | Erwartung Rücklesen
:--- | :--- | :--- | :---
M0 | — | Byte 5, 9, 25 notieren | TOP58/59/68 notieren
M1 | `RoomHeaterState 0` | Byte 9 Bits 0+1 → `01` | TOP59 → `Blocked`
M2 | `RoomHeaterState 1` | Byte 9 Bits 0+1 → `10` | TOP59 → `Free`, **TOP58 unverändert**
M3 | `ForceHeater 1` | Byte 5 Bits 2+3 → `10` | TOP68 → `Active`
M4 | `ForceHeater 0` | Byte 5 Bits 2+3 → `01` | TOP68 → `Inactive`, **TOP19 unverändert**

M2 und M4 sind die eigentlichen Nachweise: dass die Nachbarfelder im selben
Byte stehen bleiben, ist der Punkt, an dem eine falsche Maske auffällt.

Bei M3 zusätzlich beobachten, ob der Heizstab tatsächlich anläuft —
`Heat_Power_Consumption` (TOP16) müsste sprunghaft steigen. Falls nicht, ist
Punkt 2 aus Abschnitt 4 beantwortet: Freigabe ≠ Einschalten, und die Anlage
entscheidet weiter selbst.

Ausgangszustand aus M0 nach jedem Lauf vollständig wiederherstellen.

**Werkzeuge:** `test/frame_diff.py` für die Bytes (Ausgabe ist **hexadezimal**),
`test/top_watch.py` für die Rückmeldungen, `test/mqtt_pub.py` zum Senden.

## 8. Anbindung an den Notbetrieb — später, nicht jetzt

`NOTBETRIEB_WERTE_HEIZEN[]` in [`src/notbetrieb.h`](src/notbetrieb.h) hält
heute die vier Kurvenpunkte. Die Namen dort sind identisch mit den
Set-Kommandos, damit `set_command_range()` die Grenzen aus `setCommands[]`
nachschlagen kann — eine Heizstab-Freigabe ließe sich also mit einem Eintrag
ergänzen.

Das ist bewusst **kein** Teil dieses Vorhabens. Erst wenn Abschnitt 5 geklärt
ist und M1–M4 durch sind, lässt sich beurteilen, ob ein Heizstab im Notbetrieb
eine gute Idee ist oder ein teurer Dauerläufer.
