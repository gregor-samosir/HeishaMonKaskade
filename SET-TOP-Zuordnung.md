# SET-TOP-Zuordnung

Welches State-Topic liest ein Set-Kommando zurück? Diese Datei beantwortet das
für alle 32 Set-Kommandos und alle 92 State-Topics der Firmware 3.10.0 und
hält fest, wo es kein Gegenstück gibt.

Wozu: Eine Steuerung, die schreibt, muss prüfen können, ob der Wert angekommen
ist. Die Wärmepumpe quittiert nichts — sie klemmt Werte außerhalb ihres
Bereichs kommentarlos auf den nächsten Rand (nachgewiesen an den beiden
`OutsideHigh`-Parametern, siehe [`MQTT-Topics.md`](MQTT-Topics.md)). Der
einzige Nachweis ist das Rücklesen des zugehörigen State-Topics. Wo diese
Spalte leer bleibt, schreibt die Steuerung blind.

*In English: which state topic reads a set command back? This file maps all 32
set commands and all 92 state topics of firmware 3.10.0 against each other and
records where no counterpart exists. The heat pump acknowledges nothing and
silently clamps out-of-range values, so reading the matching state topic back
is the only proof a write arrived — where that column is empty, a controller
writes blind. Tables are language-neutral; the notes are German.*

**Stand:** 2026-08-19, Firmware 3.10.0. Quelle sind ausschließlich die beiden
Tabellen im Code — `setCommands[]` in [`src/commands.cpp`](src/commands.cpp)
und `stateTopics[]` in [`src/decode.cpp`](src/decode.cpp). Die Tabellen unten
sind nicht von Hand gepflegt, sondern von
[`test/set_top_zuordnung.py`](test/set_top_zuordnung.py) erzeugt; nach jeder
Änderung an einer der beiden Code-Tabellen läuft das Skript erneut und die
Ausgabe wird gegen diese Datei gehalten:

```bash
./test/set_top_zuordnung.py --pruefen
```

## Wie die Zuordnung entstanden ist

Zugeordnet wurde über **Byte-Position und Bitmaske**, nicht über Namen — Namen
können passen, wo die Bytes es nicht tun, und umgekehrt. Für jedes Set-Kommando
wurde die tatsächlich beschriebene Bitmaske aus dem Wertebereich berechnet: für
jeden erlaubten Wert das Protokollbyte bilden, alle gesetzten Bits verodern.
Nur so werden die Fälle sichtbar, in denen die Tabellenmaske `0xFF` lautet, das
Kommando aber nur eine Bitgruppe belegt (`QuietMode` und `PowerfulMode` auf
Byte 7).

Zwei Eigenschaften des Protokolls tragen die ganze Auswertung:

* **Kommando- und Antworttelegramm benutzen dieselben Byte-Positionen.** Alle
  30 gefundenen Paare liegen auf identischer Position — Byte 38 schreibt die
  Heizanforderung und Byte 38 liest sie zurück, ohne eine einzige Ausnahme. Die
  Zuordnung ist damit nicht geraten, sondern abgelesen.
* **Das Kommandotelegramm ist 110 Bytes lang** (`QUERYSIZE`, Indizes 0–109),
  die Antwort 203 Bytes. Ein State-Topic ab Byte 110 kann grundsätzlich kein
  Set-Kommando haben — die Adresse existiert im Kommando nicht. Das trennt die
  echten Lücken sauber von den Werten, die es nie geben wird.

Daraus ergibt sich die Zweiteilung des Telegramms: Bytes 4–106 tragen
Einstellungen und spiegeln das Kommandotelegramm, ab Byte 110 kommen
Ist-Zustände und Messwerte. Jedes der 30 Paare liegt unter Byte 110.

## 1. Set-Kommandos mit Rückmeldung (32 von 34)

Die Spalte *Bits* zählt wie das Projekt: **Bit 1 ist das höchstwertige Bit**,
`ganz` heißt, das Kommando belegt das volle Byte. *Art* sagt, ob das Topic
genau die Bits zurückliest, die das Kommando schreibt.

SET | Kommando | Byte | Bits | TOP | State-Topic | Art
:--- | :--- | ---: | :--- | :--- | :--- | :---
SET1 | `Heatpump` | 4 | 7+8 | TOP0 | `Heatpump_State` | voll
SET2 | `HolidayMode` | 5 | 3+4 | TOP19 | `Holiday_Mode_State` | voll
SET3 | `QuietMode` | 7 | ganz | TOP18 | `Quiet_Mode_Level` | voll ¹
SET4 | `PowerfulMode` | 7 | ganz | TOP17 | `Powerful_Mode_Time` | teilweise ¹
SET5 | `Z1HeatRequestTemperature` | 38 | ganz | TOP27 | `Z1_Heat_Request_Temp` | voll ²
SET6 | `Z1CoolRequestTemperature` | 39 | ganz | TOP28 | `Z1_Cool_Request_Temp` | voll ²
SET9 | `OperationMode` | 6 | ganz | TOP4 | `Operating_Mode_State` | teilweise ³
SET10 | `ForceDHW` | 4 | 1+2 | TOP2 | `Force_DHW_State` | voll
SET11 | `DHWTemp` | 42 | ganz | TOP9 | `DHW_Target_Temp` | voll
SET14 | `WaterPump` | 4 | 3+4 | TOP104 | `Water_Pump_Mode` | voll ⁵
SET15 | `WaterPumpSpeed` | 45 | ganz | TOP103 | `Pump_Duty_Max` | voll ⁵
SET16 | `HeatDelta` | 84 | ganz | TOP23 | `Heat_Delta` | voll
SET17 | `CoolDelta` | 94 | ganz | TOP24 | `Cool_Delta` | voll
SET18 | `DHWHeatDelta` | 99 | ganz | TOP22 | `DHW_Heat_Delta` | voll
SET19 | `DHWHeatupTime` | 98 | ganz | TOP97 | `DHW_Heatup_Time` | voll
SET20 | `HeaterOnOutdoorTemp` | 85 | ganz | TOP78 | `Heater_On_Outdoor_Temp` | voll
SET21 | `HeatingOffOutdoorTemp` | 83 | ganz | TOP77 | `Heating_Off_Outdoor_Temp` | voll
SET22 | `SGReadyCapacity1Heat` | 72 | ganz | TOP93 | `SGReady_Capacity1_Heat` | voll
SET23 | `SGReadyCapacity1DHW` | 71 | ganz | TOP94 | `SGReady_Capacity1_DHW` | voll
SET24 | `SGReadyCapacity2Heat` | 74 | ganz | TOP95 | `SGReady_Capacity2_Heat` | voll
SET25 | `SGReadyCapacity2DHW` | 73 | ganz | TOP96 | `SGReady_Capacity2_DHW` | voll
SET26 | `DHWRoomMaxTime` | 97 | ganz | TOP98 | `DHW_Room_Max_Time` | voll ⁴
SET27 | `Z1HeatCurveTargetHighTemp` | 75 | ganz | TOP29 | `Z1_Heat_Curve_Target_High_Temp` | voll ²
SET28 | `Z1HeatCurveTargetLowTemp` | 76 | ganz | TOP30 | `Z1_Heat_Curve_Target_Low_Temp` | voll
SET29 | `Z1HeatCurveOutsideLowTemp` | 77 | ganz | TOP32 | `Z1_Heat_Curve_Outside_Low_Temp` | voll
SET30 | `Z1HeatCurveOutsideHighTemp` | 78 | ganz | TOP31 | `Z1_Heat_Curve_Outside_High_Temp` | voll
SET31 | `Z1CoolCurveTargetHighTemp` | 86 | ganz | TOP72 | `Z1_Cool_Curve_Target_High_Temp` | voll ²
SET32 | `Z1CoolCurveTargetLowTemp` | 87 | ganz | TOP73 | `Z1_Cool_Curve_Target_Low_Temp` | voll
SET33 | `Z1CoolCurveOutsideLowTemp` | 88 | ganz | TOP75 | `Z1_Cool_Curve_Outside_Low_Temp` | voll
SET34 | `Z1CoolCurveOutsideHighTemp` | 89 | ganz | TOP74 | `Z1_Cool_Curve_Outside_High_Temp` | voll
SET35 | `HeatingMode` | 28 | 7+8 | TOP76 | `Heating_Mode` | voll ⁶
SET36 | `CoolingMode` | 28 | 5+6 | TOP81 | `Cooling_Mode` | voll ⁶

Bei den Kurven kreuzen sich `High` und `Low` zwischen SET- und TOP-Nummer
(SET29 `OutsideLow` → TOP32, SET30 `OutsideHigh` → TOP31). Das ist kein Fehler
in dieser Tabelle: Die Nummern stammen aus dem Original-Projekt und stehen dort
in anderer Reihenfolge als die Bytes — die Byte-Spalte ist maßgeblich. Die
Bedeutung von `High`/`Low` ist am 2026-08-20 an WP1 nachgemessen und steht
ausführlich in
[`MQTT-Topics.md`](MQTT-Topics.md#zone-1-heiz--und-kühlkurve-set27--set34--deutsche-fassung):
**`Target_High` ist der Vorlauf bei `Outside_Low`**, also der Wert für kaltes
Wetter. Die frühere Angabe hier — „an beiden Anlagen zurückgelesen (2026-08-11)"
— beschrieb nur, was `kurven_sync.py` selbst hineingeschrieben hatte.

### ¹ Byte 7 — QuietMode und PowerfulMode teilen sich das Byte

`QuietMode` belegt die Bits 3–5, `PowerfulMode` die Bits 6–8, aber beide führen
in `setCommands[]` die Maske `0xFF`. Das ist Absicht und im Code begründet: Die
Wertekodierung von `PowerfulMode` (73–76, also `0x49`–`0x4C`) setzt **immer**
auch Bit 5 und Bit 2. Jedes `set/PowerfulMode` schreibt damit zusätzlich:

* Quiet-Stufe auf `Off` → TOP18 `Quiet_Mode_Level` springt auf 0
* Quiet-Zeitprogramm auf `Disabled` → TOP3 `Quiet_Mode_Schedule` springt auf 0

Das ist eine Eigenschaft des Protokolls, kein Implementierungsfehler — Powerful
schaltet den Flüsterbetrieb ab. Umgekehrt passiert nichts: `QuietMode` schreibt
`0x08`–`0x20`, seine Bits 1–2 bleiben `00`, und `00` heißt im Kommandotelegramm
„keine Änderung". Wer Quiet nach einem Powerful-Kommando wiederherstellen will,
muss es erneut senden.

### ² Gemeinsame Speicherstelle Sollwert / oberer Kurvenpunkt

`Z1HeatRequestTemperature` (SET5, Byte 38) und `Z1HeatCurveTargetHighTemp`
(SET27, Byte 75) liegen zwar auf verschiedenen Bytes, sind in der Wärmepumpe
aber derselbe Wert — **solange der Kreis auf Direktvorgabe steht**; für das
Kühlpaar SET6/SET31 gilt dasselbe. **Das Rücklesen von TOP29 bzw. TOP72 belegt
an einer Anlage im Direktbetrieb deshalb nicht, dass der Kurvenwert steht** — es
zeigt den aktuellen Sollwert. Gemessen an WP1 am 2026-08-10, Einzelheiten in
[`test/README.md`](test/README.md).

**Im Kurvenbetrieb sind es getrennte Speicherstellen** (WP1, 2026-08-20): SET27
ist dort der Kurvenpunkt, SET5 die Parallelverschiebung von −5..+5, und Werte
außerhalb dieses Bereichs verwirft die Wärmepumpe stillschweigend. Der
Kurvenpunkt lässt sich dort also sauber schreiben — deshalb lautet die
Reihenfolge im Notbetrieb: erst umschalten, dann die Kurve setzen.

### ³ OperationMode — im Auto-Betrieb meldet TOP4 nie den geschriebenen Wert

SET9 kennt die Werte 0–6, TOP4 die Werte 0–8. Die beiden zusätzlichen Zustände
`Auto(Cool)` (7) und `Auto(Cool)+DHW` (8) sind **nur lesbar**: Auf `Auto` legt
die Wärmepumpe die Richtung selbst fest und setzt dafür ein Bit, das im
Kommando nicht gesetzt war.

geschrieben | Protokollbyte | zurückgelesen als
:--- | :--- | :---
SET9 = 2 (Auto) | 24 | TOP4 = 2 `Auto(Heat)` **oder** 7 `Auto(Cool)`
SET9 = 6 (Auto+DHW) | 40 | TOP4 = 6 `Auto(Heat)+DHW` **oder** 8 `Auto(Cool)+DHW`

Praktische Folge: **Eine Steuerung darf nach `set/OperationMode 2` nicht auf
`TOP4 == 2` warten** — sie muss 2 und 7 als Erfolg werten. Für die anderen fünf
Modi stimmt der zurückgelesene Wert mit dem geschriebenen überein. Abgeleitet
aus den Tabellen in `commands.cpp` und `decode.cpp` (`opModeBytes[2] = 24`,
aber `getOpMode()` kennt keinen Fall 24, sondern 25 → 2 und 26 → 7); an dieser
Anlage nicht nachgemessen, weil sie nicht im Auto-Betrieb läuft.

Unabhängig davon gilt der in [`MQTT-Topics.md`](MQTT-Topics.md) belegte Befund:
**TOP4 zeigt den zuletzt kommandierten Modus, nicht den Zustand des Geräts.**
Als Regelgröße ist TOP101 `Heat_Cool_SW_State` zu nehmen.

### ⁴ Einheiten-Sprung bei DHWRoomMaxTime

SET26 wird in Schritten zu 30 Minuten geschrieben, TOP98 meldet Minuten zurück:
`TOP98 = SET26 × 30`. Ein Vergleich ohne diese Umrechnung schlägt fehl.

### ⁵ Die zwei Pumpen-Rückmeldungen (neu in 3.10.0)

TOP103 und TOP104 sind die Rückmeldungen zu SET15 und SET14. Beide
Byte-Positionen sind am 2026-08-19 an Stufe 1 im Hexlog ausgemessen worden statt
aus Unterlagen übernommen — Wert verstellt, Rohbyte beobachtet, zurückgestellt
([`test/byte_monitor.py`](test/byte_monitor.py)):

Kommando | geschrieben | Byte | Rohwert
:--- | :--- | ---: | :---
SET15 `WaterPumpSpeed` | 100 → 110 → 100 | 45 | `0x65` → `0x6F` → `0x65`
SET14 `WaterPump` | 0 → 1 → 0 | 4, Bits 3+4 | `b01` → `b10` → `b01`

Zweite Kontrolle ganz ohne Schreibvorgang: Die beiden Stufen sind auf
verschiedene Werte konfiguriert (100 und 125), und Byte 45 zeigte an Stufe 2
entsprechend `0x7E`.

**TOP103 ist eine Duty-Grenze, keine Drehzahl.** Bei laufender Pumpe folgte
TOP92 `Pump_Duty` der Grenze exakt:

Byte 45 als `X−1` | TOP92 `Pump_Duty` | TOP65 `Pump_Speed` | TOP1 `Pump_Flow`
---: | ---: | ---: | ---:
100 | **100** | 2300 1/min | 11,95 l/min
80 | **80** | 1500 1/min | 6,93 l/min

Die Pumpe läuft im Handbetrieb also genau bis zur konfigurierten Obergrenze und
nicht darüber. Der Kommandoname `WaterPumpSpeed` führt in die Irre und bleibt
nur deshalb stehen, weil die Kaskadensteuerung darauf schreibt; die Ist-Werte
sind weiter TOP65 und TOP92, TOP103 ist die Grenze zu TOP92.

**TOP104 meldet den wirksamen Zustand**, nicht den Wunsch: Beim Umschalten auf
`Fix` lief die Pumpe tatsächlich an (TOP65, TOP1 und TOP92 zogen alle nach).
Der dritte Zustand `Air purge` (`b11`) ist **nicht** gemessen — ihn herzustellen
hieße, an einer intakten Anlage eine Entlüftungsroutine auszulösen. Er stützt
sich auf `ProtocolByteDecrypt.md`, so wie der zweite Zustand von TOP102
`External_SW_State`.

### ⁶ Betriebsart Kurve ↔ Direkt (neu in 3.11.0)

SET35 und SET36 teilen sich Byte 28: Bits 7+8 tragen die Heiz-, Bits 5+6 die
Kühl-Betriebsart, je `0` = Kompensationskurve, `1` = Direktvorgabe. Die
Bitmasken `0x03` und `0x0C` sind hier keine Kosmetik — ohne sie schaltete ein
Kühl-Kommando die Heizung mit um.

**Beide Kommandos sind am 2026-08-19 an Stufe 1 bei stehender Anlage gemessen**,
in vier Läufen. Byte 28 wanderte jedes Mal im laufenden Mitschnitt, und die
Maske griff jedes Mal bitgenau:

Lauf | Betriebsmodus | Kommando | Byte 28 | TOP76 `Heating_Mode` | TOP81 `Cooling_Mode`
---: | :--- | :--- | :--- | :--- | :---
1 | Heizen | `CoolingMode 0` | `0x0A` → `0x06` | 1 → **1** | 1 → **0**
2 | Kühlen | `CoolingMode 0` | `0x0A` → `0x06` | 1 → **1** | 1 → **0**
3 | Heizen | `HeatingMode 0` | `0x0A` → `0x09` | 1 → **0** | 1 → **1**
4 | Heizen | **beide** `0` | `0x0A` → `0x05` | 1 → **0** | 1 → **0**

**Damit sind alle vier Rohwerte aus `ProtocolByteDecrypt.md` am Gerät erzeugt**
— `0x0A`, `0x06`, `0x09` und `0x05`. Das Zurückschalten stellte in allen vier
Läufen `0x0A` her, in Lauf 4 ebenfalls mit beiden Kommandos zusammen.

**Lauf 4 beantwortet die für den Notbetrieb entscheidende Frage.** Die
Kaskadensteuerung würde beide Kommandos aus derselben Flow-Ausführung senden;
sie landen damit im selben 500-ms-Sammelfenster der Firmware und werden zu
*einem* Telegramm zusammengefasst, in dem beide Bitfelder gleichzeitig einen
Wechsel verlangen. Diesen Fall hatte die Wärmepumpe bis dahin nie gesehen — in
den Läufen 1–3 stand das jeweils andere Feld auf `00` = „keine Änderung".
**Sie nimmt beide an:** TOP76 und TOP81 gingen zusammen auf 0. Der Notbetrieb
kann in einem Rutsch schalten und muss die Kommandos nicht zeitlich trennen.

Trotzdem gehört ins Notbetriebskonzept, dass TOP76/TOP81 **zurückgelesen und
bei Bedarf nachgelegt** werden. In einer Ausfallsituation ist „gesendet" nicht
dasselbe wie „steht", und das Rücklesen kostet nichts.

⚠️ **Das Umschalten ist nicht folgenlos umkehrbar.** Der Wechsel auf
Kurvenbetrieb setzt die Kurve auf die Panasonic-Werksvorgaben zurück, und das
Zurückschalten stellt sie *nicht* wieder her. Gemessen wurde dabei mehr, als zu
erwarten war:

Wert | vorher | `CoolingMode 0` | `HeatingMode 0` | **beide 0**
:--- | ---: | ---: | ---: | ---:
TOP76 `Heating_Mode` | 1 | 1 | **0** | **0**
TOP81 `Cooling_Mode` | 1 | **0** | 1 | **0**
TOP27 `Z1_Heat_Request_Temp` | 20 | **35** | **0** | **0**
TOP28 `Z1_Cool_Request_Temp` | 20 | **0** | **10** | **0**
TOP29 `Z1_Heat_Curve_Target_High_Temp` | 20 | **35** | **55** | **55**
TOP30 `Z1_Heat_Curve_Target_Low_Temp` | 34 | 34 | **35** | **35**
TOP32 `Z1_Heat_Curve_Outside_Low_Temp` | −10 | −10 | **−5** | **−5**
TOP72 `Z1_Cool_Curve_Target_High_Temp` | 20 | **15** | **10** | **15**
TOP73 `Z1_Cool_Curve_Target_Low_Temp` | 20 | **10** | 20 | **10**

Die mittleren beiden Spalten sind spiegelbildlich, und die letzte zeigt beide
Werkskurven in einer einzigen Momentaufnahme: **Heizkurve 55 °C bei −5 °C und
35 °C bei +15 °C**, **Kühlkurve 15 °C bei 20 °C und 10 °C bei 30 °C**. Lauf 3
belegt die Heizkurve vollständig inklusive des Außenpunkts (TOP32 auf −5) —
beim reinen Kühl-Lauf konnten TOP74/TOP75 nichts zeigen, weil sie schon auf den
Werksvorgaben standen.

**Der Roundtrip-Verlust ist damit auch über den Kommandopfad belegt:** Nach dem
Zurückschalten in Lauf 4 standen TOP27 auf 35 und TOP28 auf 10 — die Sollwerte
hatten die unteren Kurvenpunkte übernommen. Das ist genau die Beobachtung vom
2026-08-11 am Bedienterminal, diesmal für beide Kreise gleichzeitig.

**Der jeweils NICHT geschaltete Kreis wird mitverstellt.** Bei `CoolingMode 0`
sprang der Heiz-Sollwert TOP27 auf 35, bei `HeatingMode 0` der Kühl-Sollwert
TOP28 auf 10 — jeweils auf den Werkswert des *anderen* Kreises, obwohl dessen
Betriebsart unverändert auf Direkt stand. Das Protokollfeld ist sauber
getrennt, die Wirkung im Gerät ist es nicht.

**Der Betriebsmodus spielt dabei keine Rolle.** Nach Lauf 1 (Heizbetrieb) war
offen, ob die Wärmepumpe immer beide Kreise anfasst oder nur den gerade
aktiven — der mitgewanderte Sollwert war ja der des aktiven Modus. Lauf 2 im
Kühlbetrieb hat das entschieden: TOP27 sprang wieder auf 35, obwohl der
Heizkreis diesmal nicht der aktive war. **Es sind immer beide Kreise.**

Der Kurvenbetrieb selbst ist an TOP27/TOP28 zu erkennen: Der Sollwert des
Kreises, der auf Kurve steht, meldet **0** — dort ist die Anforderungstemperatur
ohne Bedeutung.

**Folge für jeden, der SET35 oder SET36 benutzt:** danach die Kurve **beider**
Kreise nachziehen — [`test/kurven_sync.py`](test/kurven_sync.py). Die Kurve ist
nicht Teil des Sollwert-Re-Asserts und kommt von allein nicht zurück.

Die **Sollwerte** dagegen holt die Kaskadensteuerung selbst zurück: In den
Läufen 2 und 3 war der 5-Minuten-Re-Assert im Mitschnitt zu sehen (in Lauf 2
sendete Node-RED 16:53:59 und 16:58:59, TOP27 ging 16:59:06 von 35 auf 20; in
Lauf 3 standen alle vier offenen Werte binnen zweier Minuten wieder richtig).
In Lauf 1 blieb er aus — dort war die Anlage nur unter Strom, Kompressor und
Wärmepumpe waren nicht freigegeben, es gab für die Steuerung also nichts zu
tun. **Wer in einem solchen Zustand misst, muss die Sollwerte über SET5/SET6
selbst zurückstellen.**

## 2. Set-Kommandos ohne Rückmeldung (2)

SET | Kommando | Byte | Bits | Lage
:--- | :--- | ---: | :--- | :---
SET12 | `ForceDefrost` | 8 | 7 | Byte 8 ist im Antworttelegramm unbelegt — kein Rücklesen möglich
SET13 | `ForceSterilization` | 8 | 6 | dito

Byte 8 trägt im Antworttelegramm nichts — für diese beiden ist keine
Rückmeldung möglich, das ist keine Lücke im Code. Die früher hier gelisteten
SET14 und SET15 haben seit 3.10.0 ihr Rücklese-Topic, siehe Fußnote ⁵.

Für die beiden Force-Kommandos gibt es keinen Rückgabewert, wohl aber einen
**Wirkungsnachweis**: Läuft die angestoßene Routine, meldet das ein
Ist-Zustands-Topic.

Kommando | Wirkung sichtbar an | Byte
:--- | :--- | ---:
SET12 `ForceDefrost` | TOP26 `Defrosting_State` | 111
SET13 `ForceSterilization` | TOP69 `Sterilization_State` | 117

Das ist etwas anderes als eine Quittung: Es belegt, dass die Wärmepumpe
angefangen hat, nicht dass das Kommando angekommen ist. Bleibt die Routine aus,
lässt sich daraus nicht ableiten, ob das Kommando verworfen wurde oder die
Wärmepumpe es abgelehnt hat.

## 3. State-Topics ohne Set-Kommando (60)

### 3a. Einstellwerte im Kommandobereich — die eigentlichen Lücken (11)

Diese 11 Topics liegen unter Byte 110, ihre Adresse existiert im
Kommandotelegramm also. **Das heißt nicht, dass die Wärmepumpe dort auch
schreiben lässt** — belegt ist nur die Leseseite. Die Spalte *Kodierung* ist
aus dem vorhandenen Dekodierer zurückgerechnet und damit nicht geraten; offen
ist allein, ob das Feld beschreibbar ist und welchen Bereich es zulässt.

TOP | State-Topic | Byte | Bits | Kodierung eines Set-Kommandos | Nutzen
:--- | :--- | ---: | :--- | :--- | :---
TOP68 | `Force_Heater_State` | 5 | 5+6 | Maske `0x0C`, `(n+1)×4` | mittel
TOP79 | `Heat_To_Cool_Temp` | 95 | ganz | `Wert + 128` | mittel
TOP80 | `Cool_To_Heat_Temp` | 96 | ganz | `Wert + 128` | mittel
TOP58 | `DHW_Heater_State` | 9 | 5+6 | Maske `0x0C`, `(n+1)×4` | mittel
TOP59 | `Room_Heater_State` | 9 | 7+8 | Maske `0x03`, `(n+1)×1` | mittel
TOP25 | `DHW_Holiday_Shift_Temp` | 44 | ganz | `Wert + 128` | gering
TOP45 | `Room_Holiday_Shift_Temp` | 43 | ganz | `Wert + 128` | gering
TOP70 | `Sterilization_Temp` | 100 | ganz | `Wert + 128` | gering
TOP71 | `Sterilization_Max_Time` | 101 | ganz | `Wert + 1` | gering
TOP3 | `Quiet_Mode_Schedule` | 7 | 1+2 | Maske `0xC0`, `(n+1)×64` | gering ⁵
TOP13 | `Main_Schedule_State` | 5 | 1+2 | Maske `0xC0`, `(n+1)×64` | gering ⁵

⁵ Die beiden Schedule-Topics melden, ob ein Zeitprogramm aktiv ist. Das
Zeitprogramm selbst steht nicht in diesen Bits — es einzuschalten, ohne es
setzen zu können, bringt für eine externe Steuerung nichts.

### 3b. Ist-Zustände ab Byte 110 — kein Set-Kommando möglich (9)

TOP | State-Topic | Byte | Bits
:--- | :--- | ---: | :---
TOP99 | `Quiet_Mode_Active` | 110 | 1+2
TOP100 | `Powerful_Mode_Active` | 110 | 3+4
TOP101 | `Heat_Cool_SW_State` | 110 | 5+6
TOP102 | `External_SW_State` | 110 | 7+8
TOP20 | `ThreeWay_Valve_State` | 111 | 7+8
TOP26 | `Defrosting_State` | 111 | 5+6
TOP60 | `Internal_Heater_State` | 112 | 7+8
TOP61 | `External_Heater_State` | 112 | 5+6
TOP69 | `Sterilization_State` | 117 | 5+6

Diese Topics melden, was die Wärmepumpe *tut*. Sie liegen außerhalb der
110 Bytes des Kommandotelegramms und sind damit prinzipiell nicht schreibbar —
was richtig so ist: Man schaltet nicht das Dreiwegeventil, man schaltet den
Betriebsmodus. Drei von ihnen sind das Gegenstück zu einem Kommando und die
ehrlichere Prüfgröße als das Rücklesen des Sollwerts:

Kommando | Sollwert zurückgelesen | tatsächlicher Zustand
:--- | :--- | :---
SET3 `QuietMode` | TOP18 (Stufe 0–3) | TOP99 (nur an/aus)
SET4 `PowerfulMode` | TOP17 (Laufzeit) | TOP100
SET9 `OperationMode` | TOP4 (Modus, siehe ³) | TOP101 (nur heizen/kühlen)

### 3c. Messwerte und Zähler — kein Set-Kommando sinnvoll (40)

Temperaturen, Drücke, Drehzahlen, Energiewerte, Betriebsstunden und
Fehlercode. Alle liegen ab Byte 139 oder werden aus mehreren Bytes gebildet;
für keinen davon wäre ein Set-Kommando sinnvoll.

TOP1, TOP5, TOP6, TOP7, TOP8, TOP10, TOP11, TOP12, TOP14, TOP15, TOP16, TOP21,
TOP33, TOP36, TOP38, TOP39, TOP40, TOP41, TOP42, TOP44, TOP46, TOP47, TOP48,
TOP49, TOP50, TOP51, TOP52, TOP53, TOP54, TOP55, TOP56, TOP62, TOP63, TOP64,
TOP65, TOP66, TOP67, TOP90, TOP91, TOP92 — Namen und Einheiten in
[`MQTT-Topics.md`](MQTT-Topics.md).

Zwei davon grenzen an Abschnitt 2: TOP65 `Pump_Speed` (Byte 171, Drehzahl) und
TOP92 `Pump_Duty` (Byte 172, Modulationsgrad) sind die **Ist**-Werte der Pumpe.
Keiner von beiden ist das Rücklesen von SET15 — das setzt auf Byte 45 die
Obergrenze, bis zu der moduliert werden darf, also die Grenze zu TOP92. Der
Kommandoname `WaterPumpSpeed` führt hier in die Irre, siehe Abschnitt 4.

## 4. Was sich schließen ließe

**Erledigt in 3.10.0:** die Rückmeldungen für SET14 und SET15 (TOP103
`Pump_Duty_Max`, TOP104 `Water_Pump_Mode`) — je eine Zeile in `decode.cpp`,
beide Byte-Positionen vorher am Gerät ausgemessen, Belege in Fußnote ⁵. Damit
hat jedes Set-Kommando, für das es überhaupt ein Antwortbyte gibt, eine
Rückmeldung.

**Erledigt in 3.11.0: `Heating_Mode` / `Cooling_Mode` als Set-Kommando
(Byte 28).** SET35 `HeatingMode` und SET36 `CoolingMode`, je 0 = Kurve,
1 = Direkt. Damit ist der Notbetrieb vollständig fernschaltbar — bis hierher
musste beim Ausfall der Kaskadensteuerung jemand ans Bedienterminal.

Am 2026-08-19 an Stufe 1 bei stehender Anlage in vier Läufen gemessen, Ablauf
und Rohdaten in [`test/README.md`](test/README.md):

* **Die Wärmepumpe nimmt Byte 28 an.** `set/CoolingMode 0` ließ Byte 28 im
  laufenden Mitschnitt von `0x0A` auf `0x06` wandern (`byte_monitor.py`,
  11 Telegramme, Flanke zwischen dem fünften und sechsten).
* **Die Maske greift bitgenau.** TOP81 `Cooling_Mode` ging auf 0, TOP76
  `Heating_Mode` blieb auf 1 stehen. Bits 5+6 `10` → `01`, Bits 7+8
  unverändert `10`.
* **Der Rückweg funktioniert ebenso** — `set/CoolingMode 1` stellte `0x0A` und
  TOP81 = 1 wieder her.
* **SET35 verhält sich spiegelbildlich** — `set/HeatingMode 0` ließ Byte 28 auf
  `0x09` wandern (Bits 7+8 `10` → `01`), TOP76 ging auf 0, TOP81 blieb auf 1.
* **Beide Kommandos zusammen werden beide angenommen** — im selben
  Sammelfenster gesendet ergaben sie `0x05`, TOP76 und TOP81 gingen gemeinsam
  auf 0. Der Notbetrieb kann damit in einem Rutsch umschalten.

⚠️ **Das Umschalten hat Nebenwirkungen, und eine davon war neu.** Erwartet und
bestätigt: die Kühlkurve stand danach auf den Panasonic-Werksvorgaben
(TOP72/TOP73 auf 15/10), und TOP28 `Z1_Cool_Request_Temp` meldete im
Kurvenbetrieb 0. **Nicht erwartet:** dasselbe Kommando zog auch den
**Heiz**-Sollwert mit — TOP27 `Z1_Heat_Request_Temp` sprang von 20 auf 35,
obwohl TOP76 durchgehend auf Direkt stand. 35 ist der Werkswert der
*Heiz*kurve bei +15 °C. Das Protokollfeld ist sauber getrennt, die Wirkung im
Gerät ist es nicht. Ein zweiter Lauf im Kühlbetrieb zeigte dasselbe Bild — die
Wärmepumpe fasst also immer beide Kreise an, unabhängig vom Betriebsmodus. Wer
SET35 oder SET36 benutzt, muss danach die Kurve beider Kreise nachziehen; die
Sollwerte holt sich die Kaskadensteuerung über ihren 5-Minuten-Re-Assert
selbst zurück, sofern sie gerade aktiv regelt.

**Alles gemessen** — SET36 in zwei Betriebsmodi, SET35 einzeln, und beide
zusammen im selben Sammelfenster. Alle vier Rohwerte aus
`ProtocolByteDecrypt.md` sind am Gerät erzeugt worden.

Offen bleibt:

**1. Der Rest aus Abschnitt 3a**, wenn ein konkreter Bedarf auftaucht. Jeder
dieser Werte ist eine Zeile in `setCommands[]`; die Arbeit steckt nicht im
Code, sondern im Ausmessen des zulässigen Bereichs. Wie das geht und warum es
nötig ist, steht in [`MQTT-Topics.md`](MQTT-Topics.md) — von den 21 Werten, die
der veröffentlichte Bereich für `Z1HeatCurveOutsideHighTemp` zuließ, war genau
einer gültig. [`test/kurven_grenzen.py`](test/kurven_grenzen.py) misst solche
Grenzen aus.

## 5. Bytes ohne SET und ohne TOP

Der Vollständigkeit halber: Im Kommandobereich (Bytes 4–109) sind in
`ProtocolByteDecrypt.md` weitere Felder beschrieben, die diese Firmware weder
liest noch schreibt. Sie sind hier gelistet, damit die Suche nicht zweimal
gemacht wird — nicht als Vorschlag.

Bytes | Inhalt laut Referenz | warum nicht drin
:--- | :--- | :---
40, 41, 79–82, 90–93 | Zone 2, Anforderung und beide Kurven | Zone 2 in 3.4.0 entfernt, diese Anlagen haben keine
58–70 | Pool, Puffer, Solar, Bivalent, externe Heizstäbe | an dieser Anlage nicht vorhanden
20–26, 29, 30 | Anlagenkonfiguration: Zonenzahl, Sensorart, externe Steuerung, Pumpenregelung | Installateur-Ebene, gehört nicht in eine Kaskadensteuerung
27 | Freigabe SG Ready und Demand Control | Kapazitäten sind über SET22–SET25 gesetzt, die Freigabe steht am Terminal
46 | Estrichtrocknung, Zieltemperatur der Stufe | einmaliger Bauvorgang
104–106 | Verzögerung und Delta für den internen Heizstab (J/K/L-Serie) | Serie passt nicht
11 | Quiet-Priorität, DHW-Sensorwahl (K/L-Serie) | Serie passt nicht

Byte 23 trägt laut Referenz unter anderem den **externen Kompressor-Schalter**.
An dieser Anlage ist der Eingang belegt, ein Statusbyte dafür wurde aber nicht
gefunden: Der Schalter wurde bei den Messungen am 2026-08-15/16 betätigt, ohne
jede Reaktion in den 203 Bytes der Antwort (byteweiser Vergleich mit
[`test/frame_diff.py`](test/frame_diff.py)). Byte 23 ist die
Menü-*Einstellung*, ob der Eingang benutzt wird — nicht sein Zustand. Diese
Suche gilt als abgeschlossen, siehe [`MQTT-Topics.md`](MQTT-Topics.md).

## Vorbehalte

* **Die Zuordnung ist aus dem Code abgeleitet, das meiste nicht durchgemessen.**
  Am Gerät belegt sind die Kurven-Kommandos SET27–SET34 (Rücklesen an beiden
  Anlagen, 2026-08-10/11), SET3, SET4 und SET9 im laufenden Betrieb
  (2026-08-15/16) sowie SET14 auf Byte 4 und SET15 auf Byte 45, beide mit
  Änderung in beide Richtungen (2026-08-19, Fußnote ⁵). Die übrigen Paare
  stützen sich auf die identische
  Byte-Position, die bei jedem einzelnen der 28 Paare zutrifft. Wo Zweifel an
  einer Zuordnung bestehen, klärt sie [`test/byte_monitor.py`](test/byte_monitor.py)
  in wenigen Minuten — Byte beobachten, Wert ändern, Flanke ansehen.
* **Abschnitt 3a listet Möglichkeiten, keine Befunde.** Dass ein Byte im
  Kommandotelegramm erreichbar ist, heißt nicht, dass die Wärmepumpe es annimmt.
* **`ProtocolByteDecrypt.md` ist Referenz des Original-Projekts.** Die dortigen
  TOP-Nummern gehören zu jenem Projekt und stimmen mit den Nummern dieser
  Firmware **nicht** überein; benutzt wurden von dort ausschließlich die
  Byte-Positionen und Bit-Bedeutungen. Die Zahl in der ersten Spalte ist eine
  Topic-Nummer, keine Byte-Position.
* **SG Ready ist an dieser Anlage ausgetestet und arbeitet wie gewollt.** Die
  Referenz führt Byte 71 als *Heating* und Byte 72 als *DHW*, `decode.cpp` und
  `commands.cpp` halten es umgekehrt — maßgeblich ist die Firmware, die
  Abweichung in der Referenz ist damit für dieses Projekt erledigt. Für die
  Zuordnung war sie ohnehin folgenlos: SET23 und TOP94 zeigen beide auf
  Byte 71, das Paar stimmt unabhängig von der Beschriftung.
