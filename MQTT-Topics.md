# MQTT Topics for HeishaMonKaskade

Topic names compatible with the original HeishaMon. The tables are in English;
the explanatory sections are given in English and German.

*Deutsch: Topic-Namen kompatibel zum Original-HeishaMon. Die Tabellen sind
englisch, die erläuternden Abschnitte stehen englisch und deutsch da – der
Kurvenabschnitt als eigene [deutsche Fassung](#zone-1-heiz--und-kühlkurve-set27--set34--deutsche-fassung)
am Ende.*

## Availability Topic:

ID | Topic | Response
--- | --- | ---
|| LWT | Online/Offline (automatically returns to Offline if connection with the HeishaMon lost)

## Log Topic:

ID | Topic | Response
--- | --- | ---
LOG1 | log | response from headpump (level switchable)

## Sensor Topics:

**Zone 2 was removed in 3.4.0** and the numbering has gaps as a result: TOP34,
TOP35, TOP37, TOP43, TOP57 and TOP82 - TOP89 are gone, on the command side
SET7 and SET8. These plants have no zone 2, so the topics only ever carried
decoded noise. The gaps are deliberate - every remaining topic keeps the number
it always had, so this file, older captures and the numbers used by the
upstream project all stay valid.

*Deutsch: **Zone 2 ist in 3.4.0 entfallen**, deshalb hat die Nummerierung
Lücken: TOP34, TOP35, TOP37, TOP43, TOP57 und TOP82 – TOP89 gibt es nicht mehr,
auf der Kommandoseite SET7 und SET8. Diese Anlagen haben keine Zone 2, die
Topics trugen also nur dekodiertes Rauschen. Die Lücken sind Absicht: Jedes
verbliebene Topic behält seine bisherige Nummer, damit diese Datei, ältere
Mitschnitte und die Nummern des Original-Projekts gültig bleiben.*

ID | Topic | Response/Description
:--- | --- | ---
TOP0 | Heatpump_State | Heatpump state (0=off, 1=on)
TOP1 | Pump_Flow | Pump flow (l/min)
TOP2 | Force_DHW_State | DHW status (0=off, 1=on)
TOP3 | Quiet_Mode_Schedule | Quiet mode schedule (0=inactive, 1=active)
TOP4 | Operating_Mode_State | Operating mode (0=Heat only, 1=Cool only, 2=Auto(Heat), 3=DHW only, 4=Heat+DHW, 5=Cool+DHW, 6=Auto(Heat)+DHW, 7=Auto(Cool), 8=Auto(Cool)+DHW)
TOP5 | Main_Inlet_Temp | Main inlet water temperature (°C)
TOP6 | Main_Outlet_Temp | Main outlet water temperature (°C)
TOP7 | Main_Target_Temp | Main outlet water target temperature (°C)
TOP8 | Compressor_Freq | Compressor frequency (Hz)
TOP9 | DHW_Target_Temp | DHW target temperature (°C)
TOP10 | DHW_Temp | Actual DHW temperature (°C)
TOP11 | Operations_Hours | Heatpump operating time (Hour)
TOP12 | Operations_Counter | Heatpump starts (counter)
TOP13 | Main_Schedule_State | Main thermostat schedule state (inactive - active)
TOP14 | Outside_Temp | Outside ambient temperature (°C)
TOP15 | Heat_Energy_Production | Thermal heat power production (Watt)
TOP16 | Heat_Energy_Consumption | Elektrical heat power consumption at heat mode (Watt)
TOP17 | Powerful_Mode_Time | Powerful state in minutes (0, 1, 2 or 3 x 30min)
TOP18 | Quiet_Mode_Level | Quiet mode level (0, 1, 2, 3)
TOP19 | Holiday_Mode_State | Holiday mode (0=off, 1=scheduled, 2=active)
TOP20 | ThreeWay_Valve_State | 3-way valve mode (0=Room, 1=DHW)
TOP21 | Outside_Pipe_Temp | Outside pipe temperature (°C)
TOP22 | DHW_Heat_Delta | DHW heating delta (-12 to -2) (K)
TOP23 | Heat_Delta | Heat delta (K)
TOP24 | Cool_Delta | Cool delta (K)
TOP25 | DHW_Holiday_Shift_Temp | DHW Holiday shift temperature  (-15 to +15)
TOP26 | Defrosting_State | Defrost state (0=off, 1=on)
TOP27 | Z1_Heat_Request_Temp | Zone 1 Heat Requested shift temp (-5 to 5) or direct heat temp (20 to max)
TOP28 | Z1_Cool_Request_Temp | Zone 1 Cool Requested shift temp (-5 to 5) or direct cool temp (5 to 20)
TOP29 | Z1_Heat_Curve_Target_High_Temp | Heating curve, flow target **when it is cold** (°C) - paired with Outside_Low, see [curve naming](#zone-1-heating-and-cooling-curve-set27---set34)
TOP30 | Z1_Heat_Curve_Target_Low_Temp | Heating curve, flow target **when it is warm** (°C) - paired with Outside_High, see [curve naming](#zone-1-heating-and-cooling-curve-set27---set34)
TOP31 | Z1_Heat_Curve_Outside_High_Temp | Heating curve, **warm** end of the outside axis (°C) - paired with Target_Low, see [curve naming](#zone-1-heating-and-cooling-curve-set27---set34)
TOP32 | Z1_Heat_Curve_Outside_Low_Temp | Heating curve, **cold** end of the outside axis (°C) - paired with Target_High, see [curve naming](#zone-1-heating-and-cooling-curve-set27---set34)
TOP33 | Room_Thermostat_Temp | Remote control thermostat temp (°C)
TOP36 | Z1_Water_Temp | Zone 1 Water outlet temperature (°C)
TOP38 | Cool_Energy_Production | Thermal cooling power production (Watt)
TOP39 | Cool_Energy_Consumption | Elektrical cooling power consumption (Watt)
TOP40 | DHW_Energy_Production | Thermal DHW power production (Watt)
TOP41 | DHW_Energy_Consumption | Elektrical DHW power consumption (Watt)
TOP42 | Z1_Water_Target_Temp | Zone 1 water target temperature (°C)
TOP44 | Error | Last active Error from Heat Pump
TOP45 | Room_Holiday_Shift_Temp | Room heating Holiday shift temperature (-15 to 15)
TOP46 | Buffer_Temp | Actual Buffer temperature (°C)
TOP47 | Solar_Temp | Actual Solar temperature (°C)
TOP48 | Pool_Temp | Actual Pool temperature (°C)
TOP49 | Main_Hex_Outlet_Temp | Outlet 2, after heat exchanger water temperature (°C)
TOP50 | Discharge_Temp | Discharge Temperature (°C)
TOP51 | Inside_Pipe_Temp | Inside pipe temperature (°C)
TOP52 | Defrost_Temp | Defrost temperature (°C)
TOP53 | Eva_Outlet_Temp | Eva Outlet temperature (°C)
TOP54 | Bypass_Outlet_Temp | Bypass Outlet temperature (°C)
TOP55 | Ipm_Temp | Ipm temperature (°C)
TOP56 | Z1_Temp | Zone1: Actual Temperature (°C) 
TOP58 | DHW_Heater_State | When enabled, backup/booster heater can be used for DHW heating (disabled - enabled)
TOP59 | Room_Heater_State | When enabled, backup heater can be used for room heating (disabled - enabled)
TOP60 | Internal_Heater_State | Internal backup heater state (inactive - active)
TOP61 | External_Heater_State | External backup/booster heater state (inactive - active)
TOP62 | Fan1_Motor_Speed | Fan 1 Motor rotation speed (R/Min)
TOP63 | Fan2_Motor_Speed | Fan 2 Motor rotation speed (R/Min)
TOP64 | High_Pressure | High Pressure (Kgf/Cm2)
TOP65 | Pump_Speed | Pump Rotation Speed (R/Min)
TOP66 | Low_Pressure | Low Pressure (Kgf/Cm2)
TOP67 | Compressor_Current | Compressor/Outdoor unit Current (Ampere)
TOP68 | Force_Heater_State | Force heater status (0=inactive, 1=active)
TOP69 | sdC/Sterilization_State | Sterilisation State (0=inactive, 1=active)
TOP70 | sdC/Sterilization_Temp | Sterilisation Temperature (°C)
TOP71 | sdC/Sterilization_Max_Time | Sterilisation maximum time (minutes)
TOP72 | Z1_Cool_Curve_Target_High_Temp | Cooling curve, flow target **when it is cool** (°C) - paired with Outside_Low, see [curve naming](#zone-1-heating-and-cooling-curve-set27---set34)
TOP73 | Z1_Cool_Curve_Target_Low_Temp | Cooling curve, flow target **when it is hot** (°C) - paired with Outside_High, see [curve naming](#zone-1-heating-and-cooling-curve-set27---set34)
TOP74 | Z1_Cool_Curve_Outside_High_Temp | Cooling curve, **hot** end of the outside axis (°C) - paired with Target_Low, see [curve naming](#zone-1-heating-and-cooling-curve-set27---set34)
TOP75 | Z1_Cool_Curve_Outside_Low_Temp | Cooling curve, **cool** end of the outside axis (°C) - paired with Target_High, see [curve naming](#zone-1-heating-and-cooling-curve-set27---set34)
TOP76 | Heating_Mode | Compensation / Direct mode for heat (0 = compensation curve, 1 = direct)
TOP77 | Heating_Off_Outdoor_Temp | Above this outdoor temperature all heating is turned off(5 to 35 °C)
TOP78 | Heater_On_Outdoor_Temp | Below this temperature the backup heater is allowed to be used by heatpump heating logic(-15 to 20 °C)
TOP79 | Heat_To_Cool_Temp | Outdoor temperature to switch from heat to cool mode when in auto setting(°C)
TOP80 | Cool_To_Heat_Temp | Outdoor temperature to switch from cool to heat mode when in auto setting (°C)
TOP81 | Cooling_Mode | Compensation / Direct mode for cool (0 = compensation curve, 1 = direct)
TOP90 | Room_Heater_Operations_Hours | Electric heater operating time for Room (Hour)
TOP91 | DHW_Heater_Operations_Hours | Electric heater operating time for DHW (Hour)
TOP92 | Pump_Duty | Pump duty
TOP93 | SGReady_Capacity1_Heat | SGReady (%)
TOP94 | SGReady_Capacity1_DHW | SGReady (%)
TOP95 | SGReady_Capacity2_Heat | SGReady (%)
TOP96 | SGReady_Capacity2_DHW | SGReady (%)
TOP97 | DHW_Heatup_Time | DHW Heatup Time (Minutes)
TOP98 | DHW_Room_Max_Time | DHW Max Room Max Time (Minutes)
TOP99 | Quiet_Mode_Active | Quiet mode actually running (0=off, 1=on) - on/off only, the level is TOP18
TOP100 | Powerful_Mode_Active | Powerful mode actually running (0=off, 1=on)
TOP101 | Heat_Cool_SW_State | Actual heat/cool state of the unit (0=heat, 1=cool)
TOP102 | External_SW_State | External switch state (0=off, 1=on)
TOP103 | Pump_Duty_Max | Upper limit the water pump may modulate up to (duty) - the readback of SET15
TOP104 | Water_Pump_Mode | Water pump mode (0=auto, 1=fix, 2=air purge) - the readback of SET14

### Actual states from byte 110 (TOP99 - TOP102, new in 3.7.0)

These four report what the heat pump is *doing*, not what it was last told to
do. The upstream HeishaMon does not decode byte 110; the bit assignment comes
from `ProtocolByteDecrypt.md` and was confirmed on unit 1 on 2026-08-15. Each
field is two bits, encoded as everywhere else in this protocol (`b01` = 0,
`b10` = 1).

The useful one is **TOP101**: it follows the real state of the unit no matter
who switched it - the KNX actor, an MQTT `set/OperationMode`, or the local
control panel. Byte 6 (TOP4 `Operating_Mode_State`) only shows the last
commanded mode.

Switching was recorded on the running firmware on 2026-08-16, with the plant
idle and the raw byte captured from the hex log (heat/cool commanded from the
house control):

```text
10:35:09  byte 110 = 0x59   Cool      starting point, stable over 11 frames
10:36:11  byte 110 = 0x55   Heat      cool -> heat
10:37:44  byte 110 = 0x59   Cool      heat -> cool
10:39:39  byte 110 = 0x99   Cool      + quiet on (cooling switched on)
```

Three results worth keeping:

* **No transitional value.** Not a single frame showed `b00` or `b11` - the
  field flips straight from `b10` to `b01` within one query cycle. That matters
  if you want to use TOP101 as a control input.
* **TOP101 and TOP4 change in the same cycle** and reach the broker in the same
  second; the second cascade stage followed at 10:37:43, so the field is
  confirmed in both directions on both units. This does not always hold - a
  later measurement caught a flank with 7.0 s between the two, see *TOP101 end
  to end* below.
* **TOP99 confirmed in operation** as well: switching the plant on took byte 110
  from 0x59 to 0x99, quiet going `On` alongside TOP18 `Level 3`.

The delay between pressing the button and the new value is *not* part of this
measurement - the moment of switching is only known roughly. What is measured is
the part that belongs to the firmware: once the pump reports the new state, it
is published within one query cycle (6 s at most). The full path, from a
switching command with a known timestamp to the value on the broker, was
measured separately - see *TOP101 end to end* below.

Two caveats, both from the measurement:

* **TOP99 is binary.** Quiet levels 1, 2 and 3 all read as `On`; the level
  stays in TOP18 `Quiet_Mode_Level`.
* **TOP102 cannot be verified on this installation.** The external switch input
  is not wired here, so the field never leaves its off state - the `b10` side
  stays unconfirmed, and no measurement on these units can change that. If your
  installation uses that input, a report either way is welcome.
  TOP100 is confirmed in both states (2026-08-16, `set/PowerfulMode 1` on
  unit 1: TOP17 and TOP100 both followed).

What *is* wired here is the **external compressor switch**, and no status byte
has been found for it. It was operated during the 2026-08-15/16 measurements
with no reaction anywhere in the 203-byte answer telegram (compared byte by
byte with `test/frame_diff.py`). So the obvious approach - diff the frames
while flipping the switch - is exhausted; finding it would need a different
angle.

**Careful with the service manual on that input.** It documents the external
compressor switch as *open = compressor ON, short = compressor OFF*. **On this
installation it is the other way round** - closed releases the compressor,
open blocks it. Verified three ways on 2026-08-23: the KNX actuator channel is
a normally-open contact, it is driven true to release the compressor, and that
has been running correctly for five years; the actuator itself was checked
again for this note. Whether that is an error in the manual, a model
difference or a menu option is unknown. Since there is no status byte, nothing
in the telegram contradicts a wrong assumption here - so verify the contact
against `Compressor_Freq` under load rather than against the manual. The
Heat/Cool switch is a *separate* question and must not be inferred from this
one; on this installation it appears to follow the manual (*open = heat*), but
that is not yet proven.

*Deutsch: Die vier Topics melden, was die Wärmepumpe tatsächlich TUT - nicht,
was ihr zuletzt befohlen wurde. Byte 110 ist im Original-HeishaMon nicht
dekodiert; die Bitzuordnung stammt aus `ProtocolByteDecrypt.md` und ist am
2026-08-15 an WP1 belegt. Jedes Feld sind zwei Bits mit der üblichen Kodierung
(`b01` = 0, `b10` = 1).*

*Das Umschalten ist am 2026-08-16 an der laufenden Firmware mitgeschnitten
worden, bei stehender Anlage und mit dem Rohbyte aus dem Hexlog: 10:36:11
`0x59` → `0x55` (Cool → Heat), 10:37:44 zurück auf `0x59`, 10:39:39 auf `0x99`
(Kühlung eingeschaltet, Quiet dazu). **Kein einziger Frame zeigte einen
Zwischenwert** (`b00` oder `b11`) - das Feld springt innerhalb eines
Abfragezyklus direkt um, was für die Nutzung als Regelgröße der entscheidende
Punkt ist. TOP101 und TOP4 wechseln im selben Zyklus und stehen sekundengleich
im Broker (gilt nicht immer, s. *TOP101 end to end* weiter unten); die zweite
Kaskadenstufe zog um 10:37:43 nach. Auch TOP99 ist damit
im Betrieb belegt. Die Zeit zwischen Tastendruck und neuem Wert gehört NICHT zur
Messung - der Schaltzeitpunkt ist nur ungefähr bekannt; gemessen ist der Teil
der Firmware: ab der Meldung der WP vergeht höchstens ein Abfragezyklus (6 s).
Der ganze Weg — vom Schaltbefehl mit bekanntem Zeitstempel bis zum Wert auf dem
Broker — ist separat gemessen, s. denselben Abschnitt.*

*Das nützliche ist **TOP101**: Es folgt dem echten Zustand unabhängig davon, wer
umgeschaltet hat - KNX-Aktor, MQTT-`set/OperationMode` oder Bedienterminal.
Byte 6 (TOP4) zeigt dagegen nur den zuletzt kommandierten Modus. Zwei
Vorbehalte: TOP99 meldet nur AN/AUS (die Stufe steht weiter in TOP18), und
**TOP102 ist an dieser Anlage überhaupt nicht prüfbar** – der External-SW-
Eingang ist hier nicht belegt, das Feld bleibt deshalb dauerhaft im
Aus-Zustand. Keine Messung an diesen Geräten kann daran etwas ändern;
Rückmeldungen aus Anlagen, die den Eingang nutzen, sind willkommen. TOP100 ist
seit dem 2026-08-16 in beiden Zuständen belegt (`set/PowerfulMode 1` an WP1,
TOP17 und TOP100 zogen gemeinsam nach).*

*Belegt ist hier stattdessen der **externe Kompressor-Schalter** – und für den
wurde bisher kein Statusbyte gefunden. Er wurde bei den Messungen am
2026-08-15/16 betätigt, ohne jede Reaktion in den 203 Bytes des
Antworttelegramms (byteweiser Vergleich mit `test/frame_diff.py`). Der
naheliegende Weg – Frames vergleichen, während der Schalter umgelegt wird – ist
damit ausgereizt; ihn zu finden bräuchte einen anderen Ansatz.*

### TOP101 end to end: from the switch command to the broker (2026-08-16)

The measurement above deliberately leaves out the delay between pressing the
button and the new value, because the moment of switching was only known
roughly. This one closes that gap. Heat/cool is commanded here from the house
control through a KNX actor, and the KNX command datapoint carries a
millisecond timestamp - so `t0` is known exactly. Both directions were recorded
by polling the ioBroker states once per second; the resolution comes from the
datapoint timestamps, not from the poll. The plant was switched off (cascade
mode 0) for the whole recording.

```text
Cool -> Heat                                      after t0
  12:51:37.327  KNX command       true -> false   t0
  12:51:37.396  KNX actor feedback                +0.069 s
  12:51:39.460  TOP101 unit 1     1 -> 0          +2.133 s
  12:51:42.526  TOP101 unit 2     1 -> 0          +5.199 s
  12:51:46.493  TOP4   unit 1     1 -> 0          +9.166 s

Heat -> Cool
  12:53:27.858  KNX command       false -> true   t0
  12:53:27.927  KNX actor feedback                +0.069 s
  12:53:35.564  TOP4   unit 1     0 -> 1          +7.706 s
  12:53:35.565  TOP101 unit 1     0 -> 1          +7.707 s
  12:53:35.605  TOP101 unit 2     0 -> 1          +7.747 s
```

Three results:

* **Budget 2 to 8 seconds, not one query cycle.** End to end the two flanks took
  2.1 s and 7.7 s. The second one is longer than a query cycle, so it is not
  covered by the "6 s at most" above - that figure is the firmware's share,
  counted from the moment the pump reports the new state. How long the unit
  itself takes to adopt the KNX input is not separable from these numbers, and
  why the two directions differ by a factor of 3.6 is not established here.
  Anyone using TOP101 as a control input should budget for the full range.
* **TOP101 and TOP4 do not always change in the same cycle.** They did on the
  second flank (1 ms apart), but on the first one 7.0 s lay between them. There
  TOP4 did not follow the plant at all: it moved 7.0 s *after* the controller
  had sent its own `set/OperationMode`, while TOP101 had already reported the
  switch 2.1 s after `t0`. This sharpens the point made above - as a control
  input TOP4 would be circular, because it can end up reporting the command the
  controller just issued rather than the state of the unit.
* **The second cascade stage follows on its own,** but not with a fixed offset:
  3.1 s behind unit 1 on the first flank, 40 ms on the second.

*Deutsch: Die Messung weiter oben klammert die Zeit zwischen Tastendruck und
neuem Wert bewusst aus, weil der Schaltzeitpunkt nur ungefähr bekannt war -
diese hier schließt die Lücke. Heizen/Kühlen wird an dieser Anlage aus der
Haussteuerung über einen KNX-Aktor geschaltet, und der KNX-Befehlsdatenpunkt
trägt einen Millisekunden-Zeitstempel; `t0` ist damit exakt bekannt. Beide
Richtungen wurden am 2026-08-16 durch sekündliches Abfragen der
ioBroker-States mitgeschnitten, die Auflösung stammt aus den Zeitstempeln der
Datenpunkte. Die Anlage stand während der ganzen Aufzeichnung (Kaskade
Modus 0).*

*Drei Ergebnisse: **(1)** Von Ende zu Ende dauerten die beiden Flanken
**2,1 s und 7,7 s**. Die zweite liegt über einem Abfragezyklus und ist damit
NICHT von den „höchstens 6 s" oben abgedeckt - die gelten für den Anteil der
Firmware, gerechnet ab der Meldung der WP. Wie lange die WP selbst braucht, um
den KNX-Eingang zu übernehmen, lässt sich aus diesen Zahlen nicht abtrennen,
und warum die beiden Richtungen um den Faktor 3,6 auseinanderliegen, ist hier
nicht geklärt. Wer TOP101 als Regelgröße nutzt, sollte die ganze Spanne
einplanen. **(2)** TOP101 und TOP4 wechseln **nicht immer im selben Zyklus**:
auf der zweiten Flanke lagen sie 1 ms auseinander, auf der ersten 7,0 s. Dort
folgte TOP4 überhaupt nicht der Anlage, sondern sprang 7,0 s NACH dem eigenen
`set/OperationMode` der Steuerung um, während TOP101 den Wechsel schon 2,1 s
nach `t0` gemeldet hatte. Das schärft die Aussage von oben: Als Regelgröße wäre
TOP4 zirkulär - er kann den Befehl zurückmelden, den die Steuerung gerade
abgesetzt hat, statt den Zustand des Geräts. **(3)** Die zweite Kaskadenstufe
zieht selbständig nach, aber ohne festen Versatz: 3,1 s hinter Stufe 1 auf der
ersten Flanke, 40 ms auf der zweiten.*


### Pump readback (TOP103, TOP104, new in 3.10.0)

Both are the missing readback for the two pump commands, and both byte
positions were measured on the plant rather than taken from documentation
(2026-08-19, raw bytes from the hex log, `test/byte_monitor.py`): the value was
changed, the byte watched, the value put back.

Command | written | byte | raw value
:--- | ---: | ---: | :---
SET15 `WaterPumpSpeed` | 100 → 110 → 100 | 45 | `0x65` → `0x6F` → `0x65`
SET14 `WaterPump` | 0 → 1 → 0 | 4, bits 3+4 | `b01` → `b10` → `b01`

**TOP103 is a duty limit, not a speed.** With the pump running, TOP92
`Pump_Duty` followed the limit exactly - 100 gave duty 100 at 2300 rpm and
11.95 l/min, 80 gave duty 80 at 1500 rpm and 6.93 l/min. The command name
`WaterPumpSpeed` is misleading and stays as it is only for compatibility. The
actual values remain TOP65 `Pump_Speed` and TOP92 `Pump_Duty`; TOP103 is the
ceiling for the latter.

**TOP104 reports the effective state**, not the request: when the mode went to
`Fix`, the pump really started (TOP65, TOP1 and TOP92 all followed). The third
state `Air purge` (`b11`) is **not** measured - producing it would mean starting
an air purge routine on an intact plant. That entry rests on
`ProtocolByteDecrypt.md`, the same way the second state of TOP102 does.

*Deutsch: Beide sind die fehlende Rückmeldung zu den zwei Pumpenkommandos, und
beide Byte-Positionen sind an der Anlage ausgemessen statt aus Unterlagen
übernommen (2026-08-19, Rohbytes aus dem Hexlog mit `test/byte_monitor.py`):
Wert verstellt, Byte beobachtet, zurückgestellt. **TOP103 ist eine
Duty-Grenze, keine Drehzahl** - bei laufender Pumpe folgte TOP92 der Grenze
exakt (100 → Duty 100 bei 2300 1/min, 80 → Duty 80 bei 1500 1/min). Der
Kommandoname `WaterPumpSpeed` führt in die Irre und bleibt nur aus
Kompatibilitätsgründen stehen. **TOP104 meldet den wirksamen Zustand**, nicht
den Wunsch: Beim Umschalten auf `Fix` lief die Pumpe tatsächlich an. Der dritte
Zustand `Air purge` ist NICHT gemessen - ihn herzustellen hieße, an einer
intakten Anlage eine Entlüftungsroutine auszulösen.*

## Command Topics:

Published to `<prefix>/set/<Topic>`, e.g. `panasonic_heat_pump/set/Heatpump`.
The payload is a plain integer; values outside the range are rejected and
logged. Ranges and byte positions below are generated from `commands.cpp` -
that table is the single source of truth.

*Deutsch: Gesendet wird an `<prefix>/set/<Topic>`, also z. B.
`panasonic_heat_pump/set/Heatpump`. Die Nutzlast ist eine einfache ganze Zahl;
Werte außerhalb des Bereichs werden abgewiesen und protokolliert. Bereiche und
Byte-Positionen unten sind aus `commands.cpp` nachgezogen – diese Tabelle ist
die einzige Quelle der Wahrheit.*

Which state topic reads each command back — and which four commands have no
readback at all — is listed in
[`SET-TOP-Zuordnung.md`](SET-TOP-Zuordnung.md) (German, tables are
language-neutral). *Deutsch: Welches State-Topic ein Set-Kommando zurückliest,
steht in [`SET-TOP-Zuordnung.md`](SET-TOP-Zuordnung.md).*

 ID | Topic | Byte | Description | Value/Range
:--- | :--- | :--- | --- | ---
SET1  | Heatpump | 4 | Set heatpump on or off | 0=off, 1=on
SET2  | HolidayMode | 5 | Set holiday mode on or off | 0=off, 1=on
SET3  | QuietMode | 7 | Set quiet mode level | 0, 1, 2 or 3
SET4  | PowerfulMode | 7 | Set powerful mode run time in minutes | 0=off, 1=30, 2=60 or 3=90
SET5  | Z1HeatRequestTemperature | 38 | Set Z1 heat shift or direct heat temperature | -5 to 65
SET6  | Z1CoolRequestTemperature | 39 | Set Z1 cool shift or direct cool temperature | -5 to 65
SET9  | OperationMode | 6 | Sets operating mode | 0=Heat only, 1=Cool only, 2=Auto, 3=DHW only, 4=Heat+DHW, 5=Cool+DHW, 6=Auto+DHW
SET10 | ForceDHW | 4 | Forces DHW (operating mode must first be one with DHW: 3, 4, 5 or 6 - see SET9) | 0, 1
SET11 | DHWTemp | 42 | Set DHW target temperature | 40 - 75
SET12 | ForceDefrost | 8 | Forces defrost routine | 0, 1
SET13 | ForceSterilization | 8 | Forces DHW sterilization routine | 0, 1
SET14 | WaterPump | 4 | Set water pump mode | 0=auto, 1=on, 2=air purge
SET15 | WaterPumpSpeed | 45 | Set max water pump speed in service menu | 65 - 254
SET16 | HeatDelta | 84 | Set deltaT for heating | 1 - 15
SET17 | CoolDelta | 94 | Set deltaT for cooling | 1 - 15
SET18 | DHWHeatDelta | 99 | Set deltaT for DHW reheat | -15 to 15
SET19 | DHWHeatupTime | 98 | Set max heatup time for DHW (minutes) | 5 - 240
SET20 | HeaterOnOutdoorTemp | 85 | Below this outdoor temperature the backup heater may be used | -20 to 35
SET21 | HeatingOffOutdoorTemp | 83 | Above this outdoor temperature all heating is turned off | 5 - 35
SET22 | SGReadyCapacity1Heat | 72 | SG Ready capacity 1, heating (%) | 0 - 254
SET23 | SGReadyCapacity1DHW | 71 | SG Ready capacity 1, DHW (%) | 0 - 254
SET24 | SGReadyCapacity2Heat | 74 | SG Ready capacity 2, heating (%) | 0 - 254
SET25 | SGReadyCapacity2DHW | 73 | SG Ready capacity 2, DHW (%) | 0 - 254
SET26 | DHWRoomMaxTime | 97 | DHW/room max time (steps of 30 min) | 0 - 254
SET27 | Z1HeatCurveTargetHighTemp | 75 | Heating curve: flow target **when it is cold** (pairs with OutsideLow) | 20 - 55
SET28 | Z1HeatCurveTargetLowTemp | 76 | Heating curve: flow target **when it is warm** (pairs with OutsideHigh) | 20 - 55
SET29 | Z1HeatCurveOutsideLowTemp | 77 | Heating curve: **cold** end of the outside axis (pairs with TargetHigh) | -15 to 15
SET30 | Z1HeatCurveOutsideHighTemp | 78 | Heating curve: **warm** end of the outside axis (pairs with TargetLow) | -15 to 15
SET31 | Z1CoolCurveTargetHighTemp | 86 | Cooling curve: flow target **when it is cool** (pairs with OutsideLow) | 5 - 20
SET32 | Z1CoolCurveTargetLowTemp | 87 | Cooling curve: flow target **when it is hot** (pairs with OutsideHigh) | 5 - 20
SET33 | Z1CoolCurveOutsideLowTemp | 88 | Cooling curve: **cool** end of the outside axis (pairs with TargetHigh) | 20 - 30
SET34 | Z1CoolCurveOutsideHighTemp | 89 | Cooling curve: **hot** end of the outside axis (pairs with TargetLow) | 15 - 30
SET35 | HeatingMode | 28 | Heating operation mode | 0=compensation curve, 1=direct
SET36 | CoolingMode | 28 | Cooling operation mode | 0=compensation curve, 1=direct

> ⚠️ **SET35/SET36 are not harmlessly reversible.** Switching a circuit to the
> compensation curve resets that curve to the Panasonic factory defaults, and
> switching back does *not* restore it. Measured on 2026-08-19 it also dragged
> the setpoint of the *other* circuit along, although that circuit was never
> switched. Measured in four runs (both commands, both operating modes, and
> both commands together in one collection window) with mirror-image results,
> so neither the command nor the operating mode makes a difference. Sending
> both at once works — the unit accepts both bit fields in one telegram. After
> using either command, re-apply the curve of both circuits; the setpoints come
> back on their own if a controller re-asserts them. Details and the measured table: footnote 6 in
> [`SET-TOP-Zuordnung.md`](SET-TOP-Zuordnung.md).
>
> *Deutsch: Ein Wechsel auf Kurvenbetrieb setzt die Kurve auf die
> Panasonic-Werksvorgaben zurück; das Zurückschalten stellt sie nicht wieder
> her, und der Sollwert des anderen Kreises wandert mit. Danach die Kurve
> beider Kreise nachziehen.*

*If you operate your Heisha with direct temperature setup: topics ending xxxRequestTemperature will set the absolute target temperature*

*Deutsch: Im Direktbetrieb setzen die Topics, die auf `xxxRequestTemperature`
enden, die absolute Zieltemperatur (statt einer Verschiebung gegenüber der
Kurve).*

### Zone 1 heating and cooling curve (SET27 - SET34)

Each curve is defined by two points. The set values are read back through the
matching state topics, which is what a controller should verify against:

Set | reads back as
:--- | :---
SET27 Z1HeatCurveTargetHighTemp | TOP29 Z1_Heat_Curve_Target_High_Temp
SET28 Z1HeatCurveTargetLowTemp | TOP30 Z1_Heat_Curve_Target_Low_Temp
SET29 Z1HeatCurveOutsideLowTemp | TOP32 Z1_Heat_Curve_Outside_Low_Temp
SET30 Z1HeatCurveOutsideHighTemp | TOP31 Z1_Heat_Curve_Outside_High_Temp
SET31 Z1CoolCurveTargetHighTemp | TOP72 Z1_Cool_Curve_Target_High_Temp
SET32 Z1CoolCurveTargetLowTemp | TOP73 Z1_Cool_Curve_Target_Low_Temp
SET33 Z1CoolCurveOutsideLowTemp | TOP75 Z1_Cool_Curve_Outside_Low_Temp
SET34 Z1CoolCurveOutsideHighTemp | TOP74 Z1_Cool_Curve_Outside_High_Temp

**Careful with the naming - and this file had it wrong until 2026-08-20.**
`High` and `Low` do **not** both refer to the same axis. The `Outside_*` pair
names outside temperatures, the `Target_*` pair names flow temperatures, and
they cross over: **`Target_High` is the flow temperature at `Outside_Low`.**

Value | pairs with | applies when | in the ioBroker tree | label
:--- | :--- | :--- | :--- | :---
`Z1HeatCurveTargetHighTemp` (SET27, TOP29) | `OutsideLow` (SET29, TOP32) | it is **cold** | `KK_HK_vlLo` | **VL kalt**
`Z1HeatCurveTargetLowTemp` (SET28, TOP30) | `OutsideHigh` (SET30, TOP31) | it is **warm** | `KK_HK_vlHi` | **VL warm**
`Z1HeatCurveOutsideLowTemp` (SET29, TOP32) | `TargetHigh` (SET27, TOP29) | it is **cold** | `KK_HK_atLo` | **AT kalt**
`Z1HeatCurveOutsideHighTemp` (SET30, TOP31) | `TargetLow` (SET28, TOP30) | it is **warm** | `KK_HK_atHi` | **AT warm**

The ioBroker tree uses a convention of its own: there, `Hi`/`Lo` always refers to
the outside temperature, for all four values - so `vlHi` is the flow at the
*high* outside temperature and lands in `TargetLow`. The two conventions cross
over, which is why the four labels in the last column exist. They are kept in
German because that is how they read in the ioBroker tree and in the Node-RED
flow (`VL` = Vorlauf, flow; `AT` = Aussentemperatur, outside). They are the
project-wide vocabulary: every tool output, table and comment that puts a
Panasonic name next to a number or an ioBroker name carries the matching label,
and the words `lower`/`upper` are never used to describe a `Target_*` field.

A plant running the heating curve 34 &deg;C at -10 &deg;C outside and 26 &deg;C
at +15 &deg;C therefore has to be configured as:

```text
Z1_Heat_Curve_Target_High_Temp   34      Z1_Heat_Curve_Outside_Low_Temp   -10
Z1_Heat_Curve_Target_Low_Temp    26      Z1_Heat_Curve_Outside_High_Temp   15
```

**How it was measured (WP1, 2026-08-20, outside 26-28 &deg;C).** The referee is
`Main_Target_Temp` (TOP7): the heatpump computes it from the curve, so it says
which point it actually applies. Far above `Outside_High`, TOP7 followed
`Target_Low` in both directions - including with the two values swapped, which
rules out "TOP7 just shows the smaller of the two":

```text
TargetHigh 34, TargetLow 26   ->  TOP7 = 26
TargetHigh 26, TargetLow 34   ->  TOP7 = 34
```

The factory curve agrees: 55 &deg;C at -5 &deg;C and 35 &deg;C at +15 &deg;C.
A heating curve falls as it gets warmer outside; under the old reading it would
have to rise.

The earlier claim - "read back from both plants on 2026-08-11" - was a circular
one: `kurven_sync.py` writes the cold-weather flow value into `TargetLow`, so
reading it back there confirmed nothing but the tool's own mapping. Note also
that `Target_High` will not show the configured curve value while the plant
runs in direct mode - it shares its memory cell with the flow setpoint (see the
`TargetHigh` note below). Zone 2 had the same set of curve topics (TOP82 -
TOP89); they were never verified on these plants and are gone since 3.4.0.

The cooling curve follows the same crossing (factory: 15 &deg;C at 20 &deg;C
and 10 &deg;C at 30 &deg;C). Only the heating side was measured.

These commands exist so the heatpump can keep running on its own curve if the
external cascade control is unavailable - the values are kept in sync from
there, and the operator only has to switch the terminal from direct mode to
curve mode.

**`TargetHigh` is the flow setpoint - but only in direct mode.**
`Z1HeatCurveTargetHighTemp` (SET27, byte 75) and `Z1HeatRequestTemperature`
(SET5, byte 38) are the same value inside the heatpump **while the circuit is on
direct mode**; the same holds for the cooling pair SET31 / SET6. Writing it
therefore changes the setpoint of a running plant, and in direct mode it does
not hold: the next setpoint write drags it along. That is why `TargetHigh` on
a plant in direct mode reads back as the current setpoint instead of the
configured curve point, and why `test/kurven_sync.py` does not mirror it.
Measured on WP1 on 2026-08-10, details in `test/README.md`.

**In curve mode they are separate storage locations** (WP1, 2026-08-20): SET27
is the curve point, SET5 is a parallel shift of the whole curve, -5..+5, and
values outside that range are discarded silently by the heatpump. Measured with
`Main_Target_Temp` (TOP7) as the referee: at a fixed curve point of 34 it
followed the shift 26 -> 28 -> 30 while TOP29 stayed at 34. So the curve point
*can* be written cleanly there - which is exactly what the emergency button
does after switching to curve mode. Confirmed again on 2026-08-21 during the
button runs: in curve mode `Z1_Heat_Request_Temp` read 0, the neutral shift.

**The ranges of both `OutsideHigh` parameters were measured on the plant, not
taken from documentation** - and both published values turned out wrong:

```text
Z1HeatCurveOutsideHighTemp   -20 -> -15    -15 ok   15 ok   20/25/30/35 -> 15
Z1CoolCurveOutsideHighTemp    10 ->  15     15 ok   30 ok   31/32/35/40 -> 30
```

The heating one is the instructive case: valid is everything *up to* 15, not
*from* 15 as widely listed. Of the 21 values the old range allowed, exactly one
was accepted - and that only surfaced because the plant configuration happens
to use precisely that value. The heatpump clamps out-of-range values to the
nearest bound and reports nothing.

Note that the upstream HeishaMon project performs no range checking at all
(`cmd[75] = value + 128`, unfiltered), so the ranges circulating elsewhere do
not come from there. Other curve parameters may have similar undocumented
limits that only show when a value is pushed to its bound - `test/kurven_grenzen.py`
measures them, and after any curve change the state topics should be read back
and compared.

### Zone 1 Heiz- und Kühlkurve (SET27 – SET34) – deutsche Fassung

Dieser Abschnitt gibt den englischen Text darüber vollständig wieder. Das Thema
ist fehleranfällig genug, dass es die Übersetzung wert ist – die Zuordnung der
Feldnamen war in dieser Datei selbst schon einmal falsch beschrieben.

Jede Kurve wird durch zwei Punkte festgelegt. Die gesetzten Werte lassen sich
über die zugehörigen state-Topics zurücklesen; genau dagegen sollte eine
Steuerung prüfen (Tabelle „Set | reads back as" oben).

**Vorsicht bei der Benennung – und in dieser Datei stand es bis zum 2026-08-20
falsch.** `High` und `Low` beziehen sich **nicht** beide auf dieselbe Achse.
Das `Outside_*`-Paar benennt Außentemperaturen, das `Target_*`-Paar
Vorlauftemperaturen, und sie sind über Kreuz zugeordnet: **`Target_High` ist
der Vorlauf bei `Outside_Low`.**

Wert | gehört zu | gilt bei | im ioBroker-Baum | Etikett
:--- | :--- | :--- | :--- | :---
`Z1HeatCurveTargetHighTemp` (SET27, TOP29) | `OutsideLow` (SET29, TOP32) | **kaltem** Wetter | `KK_HK_vlLo` | **VL kalt**
`Z1HeatCurveTargetLowTemp` (SET28, TOP30) | `OutsideHigh` (SET30, TOP31) | **warmem** Wetter | `KK_HK_vlHi` | **VL warm**
`Z1HeatCurveOutsideLowTemp` (SET29, TOP32) | `TargetHigh` (SET27, TOP29) | **kaltem** Wetter | `KK_HK_atLo` | **AT kalt**
`Z1HeatCurveOutsideHighTemp` (SET30, TOP31) | `TargetLow` (SET28, TOP30) | **warmem** Wetter | `KK_HK_atHi` | **AT warm**

Der ioBroker-Baum benutzt eine eigene Konvention: Dort bezieht sich `Hi`/`Lo`
bei allen vier Werten immer auf die Außentemperatur – `vlHi` ist also der
Vorlauf bei *hoher* Außentemperatur und gehört nach `TargetLow`. Weil die
beiden Konventionen über Kreuz liegen, gibt es die vier Etiketten der letzten
Spalte. Sie sind das projektweite Vokabular: Jede Werkzeugausgabe, jede Tabelle
und jeder Kommentar, der einen Panasonic-Namen neben eine Zahl oder einen
ioBroker-Namen stellt, führt das passende Etikett mit – und die Wörter
`lower`/`upper` bzw. „untere/obere" beschreiben nie ein `Target_*`-Feld.

Eine Anlage, die die Heizkurve mit 34 °C bei −10 °C außen und 26 °C bei +15 °C
fährt, muss demnach so konfiguriert sein:

```text
Z1_Heat_Curve_Target_High_Temp   34      Z1_Heat_Curve_Outside_Low_Temp   -10
Z1_Heat_Curve_Target_Low_Temp    26      Z1_Heat_Curve_Outside_High_Temp   15
```

**Wie das gemessen wurde (WP1, 2026-08-20, 26–28 °C außen).** Der Schiedsrichter
ist `Main_Target_Temp` (TOP7): Den rechnet die Wärmepumpe aus der Kurve, er sagt
also, welchen Punkt sie tatsächlich anwendet. Weit oberhalb von `Outside_High`
folgte TOP7 in beide Richtungen dem `Target_Low` – auch mit vertauschten Werten,
was den Einwand „TOP7 zeigt einfach den kleineren der beiden" ausschließt:

```text
TargetHigh 34, TargetLow 26   ->  TOP7 = 26
TargetHigh 26, TargetLow 34   ->  TOP7 = 34
```

Die Werkskurve passt dazu: 55 °C bei −5 °C und 35 °C bei +15 °C. Eine Heizkurve
fällt mit steigender Außentemperatur; nach der alten Lesart müsste sie steigen.

Die frühere Begründung – „am 2026-08-11 an beiden Anlagen zurückgelesen" – war
ein Zirkelschluss: `kurven_sync.py` schreibt den Kaltwetter-Vorlauf nach
`TargetLow`, das Zurücklesen bestätigte also nur die Zuordnung des Werkzeugs.
Zu beachten außerdem: `Target_High` zeigt im Direktbetrieb nicht den
konfigurierten Kurvenwert – es teilt sich die Speicherstelle mit der
Vorlauf-Solltemperatur (siehe unten). Zone 2 hatte denselben Satz Kurven-Topics
(TOP82 – TOP89); sie waren an diesen Anlagen nie überprüfbar und sind seit 3.4.0
entfallen.

Die Kühlkurve folgt derselben Überkreuzung (Werk: 15 °C bei 20 °C und 10 °C bei
30 °C). Gemessen wurde nur die Heizseite.

Zweck dieser Kommandos ist der **Notbetrieb**: Die Wärmepumpe soll auf ihrer
eigenen Kurve weiterlaufen können, wenn die externe Kaskadensteuerung ausfällt.
Die Werte werden von dort gespiegelt, und der Betreiber muss am Bedienterminal
nur von Direkt- auf Kurvenbetrieb umschalten.

**`TargetHigh` ist die Vorlauf-Solltemperatur – aber nur im Direktbetrieb.**
`Z1HeatCurveTargetHighTemp` (SET27, Byte 75) und `Z1HeatRequestTemperature`
(SET5, Byte 38) sind in der Wärmepumpe derselbe Wert, **solange der Kreis auf
Direktvorgabe steht**; für das Kühlpaar SET31 / SET6 gilt dasselbe.
Ihn zu schreiben verstellt also den Sollwert einer laufenden Anlage, und im
Direktbetrieb hält er nicht: Der nächste Sollwert-Schreibvorgang zieht ihn mit.
Deshalb liest sich `TargetHigh` an einer Anlage im Direktbetrieb als aktueller
Sollwert statt als konfigurierter Kurvenpunkt, und deshalb überträgt
`test/kurven_sync.py` ihn nicht. Gemessen an WP1 am 2026-08-10, Einzelheiten in
`test/README.md`.

**Im Kurvenbetrieb sind es getrennte Speicherstellen** (WP1, 2026-08-20): SET27
ist dort der Kurvenpunkt, SET5 die Parallelverschiebung der ganzen Kurve um
−5..+5, und Werte außerhalb dieses Bereichs verwirft die Wärmepumpe
stillschweigend. Schiedsrichter der Messung war `Main_Target_Temp` (TOP7): Bei
festem Kurvenpunkt 34 folgte er der Verschiebung 26 → 28 → 30, während TOP29 auf
34 stehen blieb. Der Kurvenpunkt lässt sich dort also sauber schreiben — genau
das tut der Notbetriebsknopf nach dem Umschalten. Am 2026-08-21 in den
Knopf-Läufen erneut bestätigt: Im Kurvenbetrieb meldete
`Z1_Heat_Request_Temp` den Wert 0, die neutrale Verschiebung.

**Die Bereiche der beiden `OutsideHigh`-Parameter wurden an der Anlage
ausgemessen** statt aus Dokumentation übernommen – und beide veröffentlichten
Angaben erwiesen sich als falsch (Messwerte im Codeblock oben). Der Heizwert ist
der lehrreiche Fall: Gültig ist alles *bis* 15, nicht *ab* 15, wie verbreitet
angegeben. Von den 21 Werten, die der alte Bereich zuließ, wurde genau einer
angenommen – und das fiel nur auf, weil die Anlagenkonfiguration zufällig genau
diesen einen nutzt. Die Wärmepumpe klemmt außerhalb liegende Werte kommentarlos
auf den nächstgelegenen Rand.

Zu beachten: Das Original-HeishaMon-Projekt prüft überhaupt keine Bereiche
(`cmd[75] = value + 128`, ungefiltert). Die andernorts kursierenden Bereiche
stammen also nicht von dort. Andere Kurvenparameter können ähnliche
undokumentierte Grenzen haben, die sich erst zeigen, wenn ein Wert gegen den
Rand gefahren wird – `test/kurven_grenzen.py` misst sie aus. **Nach jeder
Kurvenänderung die state-Topics zurücklesen und vergleichen.**

## Notbetrieb Topics (`<prefix>/notbetrieb/`, new in 3.12.0)

A branch of its own, next to `set/`. What arrives here is **remembered, not
executed**: the firmware keeps the values in RAM and only sends them to the
heatpump when someone presses the emergency button on its web interface.

Why this exists: the MQTT broker *is* the ioBroker adapter. If ioBroker goes
down, the firmware loses both the sender of a command and the transport itself
— so it has to know the heating curve *beforehand*. Background and the full
reasoning: `Vorhaben-Notbetrieb-Weboberflaeche.md`.

Stage | Topic | Label | Source in ioBroker | Range
:--- | :--- | :--- | :--- | :---
1 (heat) | `notbetrieb/Z1HeatCurveTargetHighTemp` | **VL kalt** | `KK_Heizkurve.KK_HK_vlLo` | 20 - 55
1 (heat) | `notbetrieb/Z1HeatCurveTargetLowTemp` | **VL warm** | `KK_Heizkurve.KK_HK_vlHi` | 20 - 55
1 (heat) | `notbetrieb/Z1HeatCurveOutsideLowTemp` | **AT kalt** | `KK_Heizkurve.KK_HK_atLo` | -15 - 15
1 (heat) | `notbetrieb/Z1HeatCurveOutsideHighTemp` | **AT warm** | `KK_Heizkurve.KK_HK_atHi` | -15 - 15
2 (DHW) | `notbetrieb/DHWTemp` | Warmwasser | `KK_Warmwasser.DHW_Target_Temp` | 40 - 75

The names are identical to the matching set commands on purpose: the ranges are
looked up in `setCommands[]` (`set_command_range()`), so there is exactly one
place where they are defined. **Values outside the range are discarded, not
clamped** — a silently corrected curve point would go unnoticed in an emergency.

Note the crossover in the source column: `vlLo` - the flow **when it is cold** -
feeds `TargetHigh`. The label column is what the two sides have in common; see
the curve section above.

Each stage only subscribes to the topics of its own role, chosen by a build
flag (`NOTBETRIEB_ROLLE_WASSER`, otherwise heating). Stage 2 does not listen for
curve values at all.

**These topics deliberately bypass the subscribe grace period** that
`mqtt_callback()` applies to everything else. For `set/` topics the broker's
replay of stored values after a reconnect is a hazard (measured 2026-08-13: a
day-old curve value pushed the flow setpoint to 55 °C on every restart). For
this branch the very same replay is the mechanism: it is what puts the curve
values back within seconds after a restart, without Node-RED having to do
anything. The hazard cannot apply here, because these values never reach the
heatpump unasked.

### The button has its own login (new in 3.12.0)

`/notbetrieb` and `/notbetrieb/start` ask for user **`notbetrieb`** and a
password compiled in from `platformio_user_env.ini` - deliberately *not* the one
guarding `/firmware`, `/settings` and `/reboot`. The button is meant to be
pressed by anyone in the household and therefore appears with its password in a
printed emergency sheet; the same sheet must not also open the firmware upload
and the MQTT credentials.

`/notbetrieb/status` needs no login at all. It only returns state and step
number, is polled every two seconds, and authenticating that on an ESP8266
would be noticeable.

### `/notbetrieb/status` also carries the connection state (new in 3.13.0)

The route grew two fields. Full format (an eighth followed in 3.14.0):

```
Zustand;Schritt;Schritte;fehlendMaske;Sperre;Lage;Dauertext;Kurvenwarnung
```

`Lage` is the state of the connection to the house control: **0** connected,
**1** disconnected but still inside the five-minute grace period, **2**
disconnected past the grace period, **3** never connected since this device
booted, **4** broker reachable but no set command for over twelve minutes — the
cascade control is no longer computing. `Dauertext` is the outage duration
already formatted for display ("14 Minuten", "mehr als 30 Tagen") and is empty
unless `Lage` is 2 or 4 — the formatting rule lives in `src/verbindung.h` so that
firmware, web page and host test share one truth.

`Kurvenwarnung` (new in 3.14.0) says whether the four held curve values are
plausible: **0** fine or nothing to check, **1** the flow values are swapped
(VL kalt below VL warm), **2** the two outside points are swapped or equal. It
**warns and never locks** - the button stays usable, because an emergency run
on a twisted curve still beats no emergency run. The rule itself is in
`src/notbetrieb.h` (`notbetrieb_kurve_pruefen()`) and is covered by the host
test; the reason it exists is the crossover described under "Zone 1 heating and
cooling curve": all four values can sit inside their ranges and still describe a
curve that rises with the outside temperature. The field is appended at the end
so the connection fields keep indices 5 and 6, which the home page reads.

The **home page** polls this same route, at the 30-second interval of the topic
table. That the path says "notbetrieb" is deliberate: it is the device's only
status route, it is reachable without a login, and a second route for two
fields would be the more expensive option on an ESP8266.

What is measured is the **MQTT connection**, not the WLAN. The outage this is
about is the ioBroker going down — and the MQTT broker *is* the ioBroker
adapter. Without WLAN the web interface itself would be gone.

### The heartbeat: which messages count (new in 3.13.0)

`Lage` 4 means the broker answers but nothing arrives from the control. The
timestamp behind it is set in `mqtt_callback()` **after** the `SUBSCRIBE_GRACE`
check, and that placement is the whole point: the adapter replays every stored
`set/` value to each new subscriber, **including when Node-RED is dead**. That
burst proves the broker is alive — which the other clock already watches — and
says nothing about the control. Counting it would silence the message for twelve
minutes after every reconnect.

Topics under `<prefix>/notbetrieb/` never count either: they are handled before
the grace check and only arrive on change and on reconnect, so there is no
interval to reason about. A `set/` command that the firmware then **rejects**
(unknown topic, out of range) does count — the control sent it, so it is alive.

While the broker is unreachable the heartbeat clock is stopped, not just
ignored: without a broker no command can arrive, so silence says nothing. It
restarts when the connection comes back.

### The heating button is locked unless TOP101 reads 0 (new in 3.12.0)

Having the values is not enough. On stage 1 the button is only released while
`Heat_Cool_SW_State` (TOP101) reads **0 = heat**; anything else — `1` (cool),
`2` (unknown), `-1` (empty field) or a TOP never received — keeps it locked and
the page says so in plain words.

The reason was measured on 2026-08-20 and confirmed by the owner: the external
KNX switch dictates the direction. With the plant on cooling, `set/OperationMode
0` passes range check, mask merge and telegram — and the heatpump discards it
silently. The first real run at H1 ended in a red screen after 20 s with no way
to tell why.

If the plant reports cooling **during** a run, the sequence is aborted at once.
Only an explicit `1` does that: a single gap in the readings must not tear apart
a run that is going fine and leave the plant half switched.

Stage 2 is not affected. `OperationMode` = 3 (DHW only) works in the cooling
branch as well — measured 2026-08-20 at H2 — and that is exactly the summer case
the DHW button is built for.

Note the limit: TOP101 is the last value received. If the serial link to the
heatpump dies, it ages silently and the lock cannot notice. A run started in
that state fails at its first step, which is the same outcome as before.
