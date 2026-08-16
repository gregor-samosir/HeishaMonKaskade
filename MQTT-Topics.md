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
TOP29 | Z1_Heat_Curve_Target_High_Temp | Flow target at the upper outside temperature of the heating curve, paired with Outside_High (°C)
TOP30 | Z1_Heat_Curve_Target_Low_Temp | Flow target at the lower outside temperature of the heating curve, paired with Outside_Low (°C)
TOP31 | Z1_Heat_Curve_Outside_High_Temp | Upper outside temperature of the heating curve, paired with Target_High (°C)
TOP32 | Z1_Heat_Curve_Outside_Low_Temp | Lower outside temperature of the heating curve, paired with Target_Low (°C)
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
TOP72 | Z1_Cool_Curve_Target_High_Temp | Flow target at the upper outside temperature of the cooling curve, paired with Outside_High (°C)
TOP73 | Z1_Cool_Curve_Target_Low_Temp | Flow target at the lower outside temperature of the cooling curve, paired with Outside_Low (°C)
TOP74 | Z1_Cool_Curve_Outside_High_Temp | Upper outside temperature of the cooling curve, paired with Target_High (°C)
TOP75 | Z1_Cool_Curve_Outside_Low_Temp | Lower outside temperature of the cooling curve, paired with Target_Low (°C)
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
  confirmed in both directions on both units.
* **TOP99 confirmed in operation** as well: switching the plant on took byte 110
  from 0x59 to 0x99, quiet going `On` alongside TOP18 `Level 3`.

The delay between pressing the button and the new value is *not* part of this
measurement - the moment of switching is only known roughly. What is measured is
the part that belongs to the firmware: once the pump reports the new state, it
is published within one query cycle (6 s at most).

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
im Broker; die zweite Kaskadenstufe zog um 10:37:43 nach. Auch TOP99 ist damit
im Betrieb belegt. Die Zeit zwischen Tastendruck und neuem Wert gehört NICHT zur
Messung - der Schaltzeitpunkt ist nur ungefähr bekannt; gemessen ist der Teil
der Firmware: ab der Meldung der WP vergeht höchstens ein Abfragezyklus (6 s).*

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
SET27 | Z1HeatCurveTargetHighTemp | 75 | Heating curve: flow target at the upper outside temperature | 20 - 55
SET28 | Z1HeatCurveTargetLowTemp | 76 | Heating curve: flow target at the lower outside temperature | 20 - 55
SET29 | Z1HeatCurveOutsideLowTemp | 77 | Heating curve: lower outside temperature | -15 to 15
SET30 | Z1HeatCurveOutsideHighTemp | 78 | Heating curve: upper outside temperature | -15 to 15
SET31 | Z1CoolCurveTargetHighTemp | 86 | Cooling curve: flow target at the upper outside temperature | 5 - 20
SET32 | Z1CoolCurveTargetLowTemp | 87 | Cooling curve: flow target at the lower outside temperature | 5 - 20
SET33 | Z1CoolCurveOutsideLowTemp | 88 | Cooling curve: lower outside temperature | 20 - 30
SET34 | Z1CoolCurveOutsideHighTemp | 89 | Cooling curve: upper outside temperature | 15 - 30

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

Careful with the naming: `High` and `Low` always refer to the **outside**
temperature, for the heating as well as for the cooling curve. `Target_High`
therefore belongs to `Outside_High` and `Target_Low` to `Outside_Low` - there
is no crossing over and no difference between the two curves. What does trip
people up is that `Target_High` is the *lower* flow temperature in both cases:
the warmer it is outside, the less flow temperature a heating curve needs, and
the more a cooling curve cools. A plant running the heating curve 34 &deg;C at
-10 &deg;C outside and 26 &deg;C at +15 &deg;C therefore reports:

```text
Z1_Heat_Curve_Target_High_Temp   26      Z1_Heat_Curve_Outside_High_Temp   15
Z1_Heat_Curve_Target_Low_Temp    34      Z1_Heat_Curve_Outside_Low_Temp   -10
```

That pairing was read back from both plants on 2026-08-11 and is the only one
that makes physical sense: 34 &deg;C flow belongs to -10 &deg;C outside, not to
+15 &deg;C. Note that `Target_High` will not necessarily show the configured
curve value while the plant runs in direct mode - it shares its memory cell
with the flow setpoint (see the `TargetHigh` note below). Zone 2 had the same
set of curve topics (TOP82 - TOP89); they were never verified on these plants
and are gone since 3.4.0.

These commands exist so the heatpump can keep running on its own curve if the
external cascade control is unavailable - the values are kept in sync from
there, and the operator only has to switch the terminal from direct mode to
curve mode.

**`TargetHigh` is the flow setpoint.** `Z1HeatCurveTargetHighTemp` (SET27,
byte 75) and `Z1HeatRequestTemperature` (SET5, byte 38) are the same value
inside the heatpump - the flow setpoint in direct mode, the upper curve point
in curve mode; the same holds for the cooling pair SET31 / SET6. Writing it
therefore changes the setpoint of a running plant, and in direct mode it does
not hold: the next setpoint write drags it along. That is why `TargetHigh` on
a plant in direct mode reads back as the current setpoint instead of the
configured curve point, and why `test/kurven_sync.py` does not mirror it.
Measured on WP1 on 2026-08-10, details in `test/README.md`.

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

**Vorsicht bei der Benennung:** `High` und `Low` beziehen sich immer auf die
**Außentemperatur**, bei der Heizkurve genauso wie bei der Kühlkurve.
`Target_High` gehört also zu `Outside_High` und `Target_Low` zu `Outside_Low` –
es gibt keine Überkreuzung und keinen Unterschied zwischen den beiden Kurven.
Was tatsächlich stolpern lässt: `Target_High` ist in beiden Fällen der
*niedrigere* Vorlaufwert. Je wärmer es draußen ist, desto weniger Vorlauf
braucht eine Heizkurve – und desto mehr kühlt eine Kühlkurve. Eine Anlage, die
die Heizkurve mit 34 °C bei −10 °C außen und 26 °C bei +15 °C fährt, meldet
demnach:

```text
Z1_Heat_Curve_Target_High_Temp   26      Z1_Heat_Curve_Outside_High_Temp   15
Z1_Heat_Curve_Target_Low_Temp    34      Z1_Heat_Curve_Outside_Low_Temp   -10
```

Diese Zuordnung wurde am 2026-08-11 an beiden Anlagen zurückgelesen und ist die
einzige, die physikalisch Sinn ergibt: 34 °C Vorlauf gehören zu −10 °C außen,
nicht zu +15 °C. Zu beachten: `Target_High` zeigt im Direktbetrieb nicht
zwingend den konfigurierten Kurvenwert – es teilt sich die Speicherstelle mit
der Vorlauf-Solltemperatur (siehe unten). Zone 2 hatte denselben Satz
Kurven-Topics (TOP82 – TOP89); sie waren an diesen Anlagen nie überprüfbar und
sind seit 3.4.0 entfallen.

Zweck dieser Kommandos ist der **Notbetrieb**: Die Wärmepumpe soll auf ihrer
eigenen Kurve weiterlaufen können, wenn die externe Kaskadensteuerung ausfällt.
Die Werte werden von dort gespiegelt, und der Betreiber muss am Bedienterminal
nur von Direkt- auf Kurvenbetrieb umschalten.

**`TargetHigh` ist die Vorlauf-Solltemperatur.** `Z1HeatCurveTargetHighTemp`
(SET27, Byte 75) und `Z1HeatRequestTemperature` (SET5, Byte 38) sind in der
Wärmepumpe derselbe Wert – im Direktmodus die Vorlauf-Solltemperatur, im
Kurvenmodus der obere Kurvenpunkt; für das Kühlpaar SET31 / SET6 gilt dasselbe.
Ihn zu schreiben verstellt also den Sollwert einer laufenden Anlage, und im
Direktbetrieb hält er nicht: Der nächste Sollwert-Schreibvorgang zieht ihn mit.
Deshalb liest sich `TargetHigh` an einer Anlage im Direktbetrieb als aktueller
Sollwert statt als konfigurierter Kurvenpunkt, und deshalb überträgt
`test/kurven_sync.py` ihn nicht. Gemessen an WP1 am 2026-08-10, Einzelheiten in
`test/README.md`.

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
