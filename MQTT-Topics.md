# MQTT Topics for HeishaMonKaskade

Topic names compatible with the original HeishaMon

## Availability Topic:

ID | Topic | Response
--- | --- | ---
|| LWT | Online/Offline (automatically returns to Offline if connection with the HeishaMon lost)

## Log Topic:

ID | Topic | Response
--- | --- | ---
LOG1 | log | response from headpump (level switchable)

## Sensor Topics:

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
TOP29 | Z1_Heat_Curve_Target_High_Temp | Target temperature at lowest point on the heating curve (°C)
TOP30 | Z1_Heat_Curve_Target_Low_Temp | Target temperature at highest point on the heating curve (°C)
TOP31 | Z1_Heat_Curve_Outside_High_Temp | Upper outside temperature of the heating curve, paired with Target_High (°C)
TOP32 | Z1_Heat_Curve_Outside_Low_Temp | Lower outside temperature of the heating curve, paired with Target_Low (°C)
TOP33 | Room_Thermostat_Temp | Remote control thermostat temp (°C)
TOP34 | Z2_Heat_Request_Temp | Zone 2 Heat Requested shift temp (-5 to 5) or direct heat temp (20 to max)
TOP35 | Z2_Cool_Request_Temp | Zone 2 Cool Requested shift temp (-5 to 5) or direct cool temp (5 to 20)
TOP36 | Z1_Water_Temp | Zone 1 Water outlet temperature (°C)
TOP37 | Z2_Water_Temp | Zone 2 Water outlet temperature (°C)
TOP38 | Cool_Energy_Production | Thermal cooling power production (Watt)
TOP39 | Cool_Energy_Consumption | Elektrical cooling power consumption (Watt)
TOP40 | DHW_Energy_Production | Thermal DHW power production (Watt)
TOP41 | DHW_Energy_Consumption | Elektrical DHW power consumption (Watt)
TOP42 | Z1_Water_Target_Temp | Zone 1 water target temperature (°C)
TOP43 | Z2_Water_Target_Temp | Zone 2 water target temperature (°C)
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
TOP57 | Z2_Temp | Zone2: Actual Temperature (°C) 
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
TOP72 | Z1_Cool_Curve_Target_High_Temp | Target temperature at lowest point on the cooling curve (°C)
TOP73 | Z1_Cool_Curve_Target_Low_Temp | Target temperature at highest point on the cooling curve (°C)
TOP74 | Z1_Cool_Curve_Outside_High_Temp | Upper outside temperature of the cooling curve (°C)
TOP75 | Z1_Cool_Curve_Outside_Low_Temp | Lower outside temperature of the cooling curve (°C)
TOP76 | Heating_Mode | Compensation / Direct mode for heat (0 = compensation curve, 1 = direct)
TOP77 | Heating_Off_Outdoor_Temp | Above this outdoor temperature all heating is turned off(5 to 35 °C)
TOP78 | Heater_On_Outdoor_Temp | Below this temperature the backup heater is allowed to be used by heatpump heating logic(-15 to 20 °C)
TOP79 | Heat_To_Cool_Temp | Outdoor temperature to switch from heat to cool mode when in auto setting(°C)
TOP80 | Cool_To_Heat_Temp | Outdoor temperature to switch from cool to heat mode when in auto setting (°C)
TOP81 | Cooling_Mode | Compensation / Direct mode for cool (0 = compensation curve, 1 = direct)
TOP82 | Z2_Heat_Curve_Target_High_Temp | Target temperature at lowest point on the heating curve (°C)
TOP83 | Z2_Heat_Curve_Target_Low_Temp | Target temperature at highest point on the heating curve (°C)
TOP84 | Z2_Heat_Curve_Outside_High_Temp | Lowest outside temperature on the heating curve (°C)
TOP85 | Z2_Heat_Curve_Outside_Low_Temp | Highest outside temperature on the heating curve (°C)
TOP86 | Z2_Cool_Curve_Target_High_Temp | Target temperature at lowest point on the cooling curve (°C)
TOP87 | Z2_Cool_Curve_Target_Low_Temp | Target temperature at highest point on the cooling curve (°C)
TOP88 | Z2_Cool_Curve_Outside_High_Temp | Lowest outside temperature on the cooling curve (°C)
TOP89 | Z2_Cool_Curve_Outside_Low_Temp | Highest outside temperature on the cooling curve (°C)
TOP90 | Room_Heater_Operations_Hours | Electric heater operating time for Room (Hour)
TOP91 | DHW_Heater_Operations_Hours | Electric heater operating time for DHW (Hour)
TOP92 | Pump_Duty | Pump duty
TOP93 | SGReady_Capacity1_Heat | SGReady (%)
TOP94 | SGReady_Capacity1_DHW | SGReady (%)
TOP95 | SGReady_Capacity2_Heat | SGReady (%)
TOP96 | SGReady_Capacity2_DHW | SGReady (%)
TOP97 | DHW_Heatup_Time | DHW Heatup Time (Minutes)


## Command Topics:

Published to `<prefix>/set/<Topic>`, e.g. `panasonic_heat_pump/set/Heatpump`.
The payload is a plain integer; values outside the range are rejected and
logged. Ranges and byte positions below are generated from `commands.cpp` -
that table is the single source of truth.

 ID | Topic | Byte | Description | Value/Range
:--- | :--- | :--- | --- | ---
SET1  | Heatpump | 4 | Set heatpump on or off | 0=off, 1=on
SET2  | HolidayMode | 5 | Set holiday mode on or off | 0=off, 1=on
SET3  | QuietMode | 7 | Set quiet mode level | 0, 1, 2 or 3
SET4  | PowerfulMode | 7 | Set powerful mode run time in minutes | 0=off, 1=30, 2=60 or 3=90
SET5  | Z1HeatRequestTemperature | 38 | Set Z1 heat shift or direct heat temperature | -5 to 65
SET6  | Z1CoolRequestTemperature | 39 | Set Z1 cool shift or direct cool temperature | -5 to 65
SET7  | Z2HeatRequestTemperature | 40 | Set Z2 heat shift or direct heat temperature | -5 to 65
SET8  | Z2CoolRequestTemperature | 41 | Set Z2 cool shift or direct cool temperature | -5 to 65
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
SET30 | Z1HeatCurveOutsideHighTemp | 78 | Heating curve: upper outside temperature | 15 - 35
SET31 | Z1CoolCurveTargetHighTemp | 86 | Cooling curve: flow target at the lower outside temperature | 5 - 20
SET32 | Z1CoolCurveTargetLowTemp | 87 | Cooling curve: flow target at the upper outside temperature | 5 - 20
SET33 | Z1CoolCurveOutsideLowTemp | 88 | Cooling curve: lower outside temperature | 20 - 30
SET34 | Z1CoolCurveOutsideHighTemp | 89 | Cooling curve: upper outside temperature | 20 - 30

*If you operate your Heisha with direct temperature setup: topics ending xxxRequestTemperature will set the absolute target temperature*

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

Careful with the naming: for the **heating** curve the low outside temperature
pairs with `Target_Low` (cold outside, high flow temperature), while for the
**cooling** curve the low outside temperature pairs with `Target_High`. Those
are Panasonic's own field names, not a mistake in this firmware. A plant
running the heating curve 35 &deg;C at -5 &deg;C outside and 20 &deg;C at
+15 &deg;C therefore reports:

```text
Z1_Heat_Curve_Target_High_Temp   20      Z1_Heat_Curve_Outside_High_Temp   15
Z1_Heat_Curve_Target_Low_Temp    35      Z1_Heat_Curve_Outside_Low_Temp    -5
```

These commands exist so the heatpump can keep running on its own curve if the
external cascade control is unavailable - the values are kept in sync from
there, and the operator only has to switch the terminal from direct mode to
curve mode.

**The heatpump caps `Z1CoolCurveOutsideHighTemp` at 30 &deg;C.** Measured on
two units: 31, 32, 35 and 40 all read back as 30, without any error from the
heatpump. The range here is therefore 20-30 rather than the 30-40 some sources
list - a higher value would be forwarded and then quietly dropped. Other curve
parameters may well have similar undocumented caps that only show up when a
value is pushed to its limit; after changing a curve, read the state topics
back and compare.
