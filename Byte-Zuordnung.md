# Byte-Zuordnung

Was steht in Byte *n*, und schreibt oder liest diese Firmware es? Diese Datei
geht das Antworttelegramm Byte für Byte durch, von 1 bis 202 ohne Lücke —
auch dort, wo weder ein Set-Kommando noch ein State-Topic liegt. Bei Bytes mit
Bitfeldern steht jede Bitgruppe in einer eigenen Zeile, die unbelegten
eingeschlossen.

Sie ist das Gegenstück zu [`SET-TOP-Zuordnung.md`](SET-TOP-Zuordnung.md): Dort
geht man vom Kommando oder vom Topic aus, hier von der Byte-Position. Belege und
Messungen stehen dort; hier steht je Zeile nur, was man zum Einordnen braucht.

*In English: a byte-by-byte map of the 203-byte response telegram, bytes 1 to
202 without gaps, with every bit group of a bit-field byte on its own row —
including the unused ones. For each row it names the set command that writes it
and the state topic that reads it, using this firmware's numbering, which
differs from the original project's. Notes are German.*

**Stand:** 2026-09-18, Firmware 3.22.0 — alle 37 Set-Kommandos und alle
99 State-Topics sind untergebracht.

## Lesehilfe

* **Zwei Telegramme, eine Adressierung.** Die Antwort ist 203 Bytes lang
  (0–202). Das Kommando ist 111 Bytes lang: die Nutzbytes 0–109, dazu als
  Byte 110 die Prüfsumme. Bis Byte 109 benutzen beide dieselben Positionen —
  Byte 38 schreibt die Heizanforderung, und Byte 38 liest sie zurück. **Ab
  Byte 110 gibt es nur noch die Antwort und damit kein Set-Kommando.** Byte 0
  ist die Telegrammkennung (`0x71` Antwort, `0xF1` Kommando) und fehlt hier.
* **Bits** zählen wie im ganzen Projekt: Bit 1 ist das höchstwertige, `ganz`
  heißt das volle Byte. Wo ein Feld nicht ins Zweierraster passt (Bytes 6, 7, 8,
  20, 22, 117 und 118), steht die tatsächliche Aufteilung.
* **Bitpaare** kodieren `b01` = 0, `b10` = 1 und `b11` = 2; das State-Topic
  meldet also das Bitpaar − 1. Im Kommando heißt `00` „keine Änderung“ —
  deshalb kann ein Kommando ein Bitpaar schreiben, ohne die Nachbarn
  anzufassen.
* **SET und TOP** sind die Nummern *dieser* Firmware, aus `setCommands[]` in
  [`src/commands.cpp`](src/commands.cpp) und `stateTopics[]` in
  [`src/decode.cpp`](src/decode.cpp). SET9 belegt zwei Bitgruppen und steht in
  beiden Zeilen; Topics über zwei Bytes (TOP1, TOP11, TOP12, TOP44, TOP90,
  TOP91) stehen auf beiden Bytes.
* **Status** ist der Name des State-Topics, so wie *Kommando* der des
  Set-Kommandos ist.
* **Die Bemerkung** nennt bei ganzen Bytes mit Topic in Klammern Einheit und
  Kodierung. Weitere Kennzeichen:
  * `Referenz:` — Bedeutung laut
    [`ProtocolByteDecrypt.md`](ProtocolByteDecrypt.md), von dieser Firmware
    nicht genutzt. Die TOP-Nummern dort gehören zum Original-Projekt und
    stimmen mit unseren **nicht** überein; übernommen sind nur Position und
    Bedeutung.
  * `Original:` — Kommando- und Topic-Namen aus dem Code des Original-Projekts
    (Stand 2026-09-15) für Felder, die diese Firmware nicht nutzt. Die Nummern
    sind bewusst weggelassen. Ein Kommando im Original heißt nur, dass dort
    jemand hinschreibt — nicht, dass diese Anlage es annimmt.
  * `unbelegt` — weder die Referenz noch das Original kennen eine Bedeutung.
  * ¹ – ⁷ verweisen auf die gleichnamigen Fußnoten in
    [`SET-TOP-Zuordnung.md`](SET-TOP-Zuordnung.md), *(3a)* auf deren
    Abschnitt 3a: Einstellwerte, die gelesen, aber nicht geschrieben werden.
* **Bytes 20–30 sind Installateur-Einstellungen.** Diese Firmware liest davon
  nur Byte 23 und Byte 25, nach der Regel in [`MQTT-Topics.md`](MQTT-Topics.md),
  Abschnitt zu TOP105–TOP111.

## Byte 1 – 202

| SET | Kommando | Byte | Bits | TOP | Status | Bemerkung |
| :--- | :--- | ---: | :--- | :--- | :--- | :--- |
|  |  | 1 | ganz |  |  | Kopf: Datenlänge. Antwort `0xC8` (200 + 3 = 203 Bytes), Kommando `0x6C` (108 + 3 = 111 Bytes) |
|  |  | 2 | ganz |  |  | Kopf, stets `0x01` |
|  |  | 3 | ganz |  |  | Kopf, stets `0x10` |
| SET10 | `ForceDHW` | 4 | 1+2 | TOP2 | `Force_DHW_State` | Warmwasser erzwingen, 0 = aus, 1 = ein |
| SET14 | `WaterPump` | 4 | 3+4 | TOP104 | `Water_Pump_Mode` | Umwälzpumpe 0 = Auto, 1 = Fix, 2 = Entlüften; meldet den wirksamen Zustand. Gemessen 2026-08-19, Entlüften nicht ⁵ |
|  |  | 4 | 5+6 |  |  | unbelegt. Die Referenz nennt für die Servicefunktion „Pump Down“ nur einen Bytewert mit Fragezeichen (`0xF0?`), kein Bitfeld |
| SET1 | `Heatpump` | 4 | 7+8 | TOP0 | `Heatpump_State` | Wärmepumpe 0 = aus, 1 = ein |
|  |  | 5 | 1+2 | TOP13 | `Main_Schedule_State` | Wochenprogramm 0 = aus, 1 = ein; das Programm selbst steht nicht im Telegramm (3a) |
| SET2 | `HolidayMode` | 5 | 3+4 | TOP19 | `Holiday_Mode_State` | Urlaubsbetrieb 0 = aus, 1 = geplant, 2 = aktiv |
| SET39 | `ForceHeater` | 5 | 5+6 | TOP68 | `Force_Heater_State` | Heizstab als Ersatzwärmequelle, 0 = inaktiv, 1 = aktiv. Nur bei ausgeschalteter Einheit, wird verzögert übernommen, startet die Umwälzpumpe ⁷ |
|  |  | 5 | 7+8 |  |  | Referenz: Estrichtrocknung aus/ein. Einmaliger Bauvorgang, nicht genutzt |
|  |  | 6 | 1+2 |  |  | Referenz: aktive Zonen, Bit 1 = Zone 2, Bit 2 = Zone 1. Original: `SetZones`, `Zones_State`. Zone 2 in 3.4.0 entfernt |
| SET9 | `OperationMode` | 6 | 3+4 | TOP4 | `Operating_Mode_State` | Warmwasser-Anteil der Betriebsart (`b01` ohne, `b10` mit Warmwasser). SET9 schreibt 3+4 und 5–8 in einem Wert, Bits 1+2 bleiben `00` ³ |
| SET9 | `OperationMode` | 6 | 5–8 | TOP4 | `Operating_Mode_State` | Heizen/Kühlen/Auto. Im Auto-Betrieb setzt die Wärmepumpe selbst ein Bit: TOP4 meldet dann 7 bzw. 8 statt 2 bzw. 6 ³. TOP4 zeigt den zuletzt kommandierten Modus, den tatsächlichen zeigt TOP101 |
|  |  | 7 | 1+2 | TOP3 | `Quiet_Mode_Schedule` | Flüster-Zeitprogramm 0 = aus, 1 = ein. Jedes `set/PowerfulMode` setzt es mit auf 0 ¹ |
| SET3 | `QuietMode` | 7 | 3–5 | TOP18 | `Quiet_Mode_Level` | Flüsterstufe 0–3. SET3 schreibt mit Maske `0xFF`, seine Bits 1+2 sind aber `00` = keine Änderung ¹. Ob er tatsächlich läuft: TOP99 |
| SET4 | `PowerfulMode` | 7 | 6–8 | TOP17 | `Powerful_Mode_Time` | Powerful 0/30/60/90 min. SET4 schreibt das ganze Byte und setzt dabei Flüsterstufe und -programm auf aus ¹. Ob er tatsächlich läuft: TOP100 |
|  |  | 8 | 1+2 |  |  | unbelegt |
|  |  | 8 | 3+4 |  |  | unbelegt |
|  |  | 8 | 5 |  |  | unbelegt |
| SET13 | `ForceSterilization` | 8 | 6 |  |  | Desinfektion anstoßen (Impuls). Byte 8 ist in der Antwort stets `0x00`, kein Rücklesen; Wirkung sichtbar an TOP69 |
| SET12 | `ForceDefrost` | 8 | 7 |  |  | Abtauen anstoßen (Impuls). Kein Rücklesen; Wirkung sichtbar an TOP26 |
|  |  | 8 | 8 |  |  | Original: `SetReset` (verriegelte Fehler quittieren). Bewusst nicht übernommen, quittiert wird am Bedienteil ([`Vorhaben-HeaterSet.md`](Vorhaben-HeaterSet.md)) |
|  |  | 9 | 1+2 |  |  | unbelegt; steht an dieser Anlage auf `01` ⁷ |
|  |  | 9 | 3+4 |  |  | Referenz: Warmwasserleistung Standard/Variabel (nur J-Serie); an dieser Anlage `01` ⁷ |
| SET38 | `DHWHeaterState` | 9 | 5+6 | TOP58 | `DHW_Heater_State` | Freigabe Warmwasser-Heizstab, 0 = gesperrt, 1 = frei. Als einziges der drei Heizstab-Kommandos nicht am Gerät gemessen ⁷ |
| SET37 | `RoomHeaterState` | 9 | 7+8 | TOP59 | `Room_Heater_State` | Freigabe Raumheizstab, 0 = gesperrt, 1 = frei. Frei heißt nicht an: ob er läuft, zeigt TOP60 ⁷ |
|  |  | 10 | ganz |  |  | Referenz: stets `0x00` |
|  |  | 11 | 1+2 |  |  | unbelegt |
|  |  | 11 | 3+4 |  |  | Referenz: Vorrang im Flüsterbetrieb Schall/Leistung (K/L-Serie). Original: `SetQuietModePriority`, `Quiet_Mode_Priority` |
|  |  | 11 | 5+6 |  |  | unbelegt |
|  |  | 11 | 7+8 |  |  | Referenz: Warmwasserfühler oben/Mitte (K/L-Serie, All-in-One). Original: `SetDHWSensorSelection`, `DHW_Sensor_Selection` |
|  |  | 12 | ganz |  |  | Referenz: stets `0x00` |
|  |  | 13 | ganz |  |  | Referenz: stets `0x00` |
|  |  | 14 | ganz |  |  | Referenz: stets `0x00` |
|  |  | 15 | ganz |  |  | Referenz: stets `0x00` |
|  |  | 16 | ganz |  |  | Referenz: stets `0x00` |
|  |  | 17 | ganz |  |  | Referenz: stets `0x00` |
|  |  | 18 | ganz |  |  | Referenz: stets `0x00` |
|  |  | 19 | ganz |  |  | Referenz: stets `0x00` |
|  |  | 20 | 1 |  |  | Referenz: Umlaufmedium Wasser/Glykol. Original: `Liquid_Type` |
|  |  | 20 | 2 |  |  | unbelegt |
|  |  | 20 | 3+4 |  |  | Referenz: alternativer Außenfühler aus/ein. Original: `SetAltExternalSensor`, `Alt_External_Sensor` |
|  |  | 20 | 5+6 |  |  | Referenz: Frostschutz aus/ein. Original: `Anti_Freeze_Mode` |
|  |  | 20 | 7+8 |  |  | Referenz: Optionsplatine aus/ein. Original: `Optional_PCB` |
|  |  | 21 | ganz |  |  | Referenz: Zonenzahl und -ziel als Kennwert — `0x15` eine Zone, Raum; `0x19` eine Zone, Pool; `0x16` zwei Zonen, Z2 Raum; `0x26` zwei Zonen, Z2 Pool |
|  |  | 22 | 1–4 |  |  | Referenz: Fühler Zone 2 — 1 Wassertemperatur, 2 externer Thermostat, 3 interner Thermostat, 4 Thermistor. Original: `Z2_Sensor_Settings` |
|  |  | 22 | 5–8 |  |  | Referenz: Fühler Zone 1, Kodierung wie Zone 2. Original: `Z1_Sensor_Settings` |
|  |  | 23 | 1+2 | TOP108 | `External_Compressor_Config` | Eingang externer Kompressorschalter eingerichtet, 0 = nein, 1 = ja. Menüeinstellung; den Schaltzustand meldet kein Byte. Gemessen 2026-08-31 an Stufe 2 |
|  |  | 23 | 3+4 | TOP109 | `External_Error_Signal_Config` | Eingang externes Fehlersignal eingerichtet. Nicht gemessen, gegen die Verdrahtung gegengeprüft |
|  |  | 23 | 5+6 | TOP110 | `Heat_Cool_SW_Config` | Eingang Heizen/Kühlen-Umschalter eingerichtet. Nicht gemessen, gegengeprüft |
|  |  | 23 | 7+8 | TOP111 | `External_Control_Config` | Eingang externer Ein/Aus-Schalter eingerichtet. Nicht gemessen, gegengeprüft |
|  |  | 24 | 1+2 |  |  | Referenz: Smart DHW Variabel/Standard (L-Serie). Original: `SetSmartDHW`, `Smart_DHW` |
|  |  | 24 | 3+4 |  |  | Referenz: Solar keine/Puffer/Warmwasser. Original: `Solar_Mode` |
|  |  | 24 | 5+6 |  |  | Referenz: Pufferspeicher vorhanden. Original: `SetBuffer`, `Buffer_Installed` |
|  |  | 24 | 7+8 |  |  | Referenz: Warmwasserspeicher vorhanden. Original: `DHW_Installed` |
|  |  | 25 | 1+2 |  |  | unbelegt; an Stufe 2 konstant `b10`, die Referenz nennt nur diesen Wert. Bewusst ohne Topic |
|  |  | 25 | 3+4 | TOP105 | `Pad_Heater_Type` | Bodenwannenheizung 0 = keine, 1 = Typ A, 2 = Typ B. Einziges Feld von Byte 25, das nicht gemessen ist |
|  |  | 25 | 5+6 | TOP106 | `Internal_Heater_Power` | Leistung interner Heizstab 0 = 3 kW, 1 = 6 kW, 2 = 9 kW. Stufe 1 stand auf 9 kW bei eingebauten 3 kW, berichtigt 2026-08-31 |
|  |  | 25 | 7+8 | TOP107 | `DHW_Heater_Type` | Speicher-Heizstab 0 = intern, 1 = extern. Fälschlich extern legte Stufe 2 am 2026-08-31 mit H91 still; an Stufe 1 bleibt extern bewusst stehen ([`MQTT-Topics.md`](MQTT-Topics.md)) |
|  |  | 26 | 1+2 |  |  | Bivalenz. Original: `Bivalent_Advanced_DHW`. Die Bitaufteilung von Byte 26 stammt aus dem Code des Original-Projekts, die Referenz nennt keine |
|  |  | 26 | 3+4 |  |  | Bivalenz. Original: `Bivalent_Advanced_Heat` |
|  |  | 26 | 5+6 |  |  | Bivalenz-Modus. Original: `SetBivalentMode`, `Bivalent_Mode` |
|  |  | 26 | 7+8 |  |  | Bivalenz aus/ein. Original: `SetBivalentControl`, `Bivalent_Control`. Kein bivalenter Erzeuger an dieser Anlage |
|  |  | 27 | 1+2 |  |  | unbelegt |
|  |  | 27 | 3+4 |  |  | unbelegt |
|  |  | 27 | 5+6 |  |  | Referenz: SG Ready aus/ein (Menü). Nur die Freigabe; den aktiven SG-Modus meldet kein Byte des Telegramms |
|  |  | 27 | 7+8 |  |  | Referenz: Demand Control aus/ein (Menü) |
|  |  | 28 | 1+2 |  |  | unbelegt |
|  |  | 28 | 3+4 |  |  | unbelegt |
| SET36 | `CoolingMode` | 28 | 5+6 | TOP81 | `Cooling_Mode` | Kühlen 0 = Kurve, 1 = Direkt. Das Umschalten setzt die Kurven beider Kreise auf Werkswerte zurück ⁶ |
| SET35 | `HeatingMode` | 28 | 7+8 | TOP76 | `Heating_Mode` | Heizen 0 = Kurve, 1 = Direkt. Nebenwirkungen wie Bits 5+6 ⁶ |
|  |  | 29 | 1+2 |  |  | unbelegt |
|  |  | 29 | 3+4 |  |  | Referenz: Pumpenregelung ΔT / Max. Duty (J/K/L-Serie). Original: `SetPumpFlowrateMode`, `Pump_Flowrate_Mode` |
|  |  | 29 | 5+6 |  |  | unbelegt |
|  |  | 29 | 7+8 |  |  | unbelegt |
|  |  | 30 | 1+2 |  |  | unbelegt |
|  |  | 30 | 3+4 |  |  | unbelegt |
|  |  | 30 | 5+6 |  |  | Referenz: Heizregelung Komfort/Effizienz (K/L-Serie). Original: `SetHeatingControl`, `Heating_Control` |
|  |  | 30 | 7+8 |  |  | Referenz: Abtauen über den Warmwasserspeicher nein/ja (K/L-Serie) |
|  |  | 31 | ganz |  |  | Referenz: stets `0x00` |
|  |  | 32 | ganz |  |  | Referenz: stets `0x00` |
|  |  | 33 | ganz |  |  | Referenz: stets `0x00` |
|  |  | 34 | ganz |  |  | Referenz: stets `0x00` |
|  |  | 35 | ganz |  |  | Referenz: stets `0x00` |
|  |  | 36 | ganz |  |  | Referenz: stets `0x00` |
|  |  | 37 | ganz |  |  | Referenz: stets `0x00` |
| SET5 | `Z1HeatRequestTemperature` | 38 | ganz | TOP27 | `Z1_Heat_Request_Temp` | Heiz-Sollwert Zone 1 (°C, Rohwert − 128): Direktvorgabe, im Kurvenbetrieb Parallelverschiebung −5…+5 ² ⁶ |
| SET6 | `Z1CoolRequestTemperature` | 39 | ganz | TOP28 | `Z1_Cool_Request_Temp` | Kühl-Sollwert Zone 1 (°C, Rohwert − 128); sonst wie Byte 38 ² ⁶ |
|  |  | 40 | ganz |  |  | Referenz: Heiz-Sollwert Zone 2. Original: `SetZ2HeatRequestTemperature`, `Z2_Heat_Request_Temp`. Zone 2 in 3.4.0 entfernt |
|  |  | 41 | ganz |  |  | Referenz: Kühl-Sollwert Zone 2. Original: `SetZ2CoolRequestTemperature`, `Z2_Cool_Request_Temp`. Zone 2 entfernt |
| SET11 | `DHWTemp` | 42 | ganz | TOP9 | `DHW_Target_Temp` | Warmwasser-Solltemperatur (°C, Rohwert − 128) |
|  |  | 43 | ganz | TOP45 | `Room_Holiday_Shift_Temp` | Absenkung Raumheizung im Urlaubsbetrieb (K, Rohwert − 128); ob beschreibbar, ist nicht geprüft (3a) |
|  |  | 44 | ganz | TOP25 | `DHW_Holiday_Shift_Temp` | Absenkung Warmwasser im Urlaubsbetrieb (K, Rohwert − 128); ob beschreibbar, ist nicht geprüft (3a) |
| SET15 | `WaterPumpSpeed` | 45 | ganz | TOP103 | `Pump_Duty_Max` | Obergrenze der Pumpenmodulation, keine Drehzahl (Duty, Rohwert − 1); die Ist-Werte sind TOP65 und TOP92. Gemessen 2026-08-19 ⁵ |
|  |  | 46 | ganz |  |  | Referenz: Estrichtrocknung, Zieltemperatur der laufenden Stufe (Rohwert − 128). Einmaliger Bauvorgang |
|  |  | 47 | ganz |  |  | Referenz: stets `0x00` |
|  |  | 48 | ganz |  |  | Referenz: stets `0x00` |
|  |  | 49 | ganz |  |  | Referenz: stets `0x00` |
|  |  | 50 | ganz |  |  | Referenz: stets `0x00` |
|  |  | 51 | ganz |  |  | Referenz: stets `0x00` |
|  |  | 52 | ganz |  |  | Referenz: stets `0x00` |
|  |  | 53 | ganz |  |  | Referenz: stets `0x00` |
|  |  | 54 | ganz |  |  | Referenz: stets `0x00` |
|  |  | 55 | ganz |  |  | Referenz: stets `0x00` |
|  |  | 56 | ganz |  |  | Referenz: stets `0x00` |
|  |  | 57 | ganz |  |  | Referenz: stets `0x00` |
|  |  | 58 | ganz |  |  | Referenz: ΔT Pool (Rohwert − 128). Kein Pool an dieser Anlage |
|  |  | 59 | ganz |  |  | Referenz: ΔT Pufferspeicher (Rohwert − 128). Original: `SetBufferDelta`, `Buffer_Tank_Delta`. Kein Puffer |
|  |  | 60 | ganz |  |  | Referenz: Laufzeit externer Heizer, 20 min – 3 h in 5-min-Schritten (Rohwert − 1) |
|  |  | 61 | ganz |  |  | Referenz: Solar, ΔT Speicher ein (Rohwert − 128). Original: `Solar_On_Delta`. Keine Solaranlage |
|  |  | 62 | ganz |  |  | Referenz: Solar, ΔT Speicher aus (Rohwert − 128). Original: `Solar_Off_Delta` |
|  |  | 63 | ganz |  |  | Referenz: Solar, Frostschutz (Rohwert − 128). Original: `Solar_Frost_Protection` |
|  |  | 64 | ganz |  |  | Referenz: Solar, Obergrenze (Rohwert − 128). Original: `Solar_High_Limit` |
|  |  | 65 | ganz |  |  | Referenz: Bivalenz, Außentemperatur für die Zuschaltung, −15…35 °C (Rohwert − 128). Original: `SetBivalentStartTemp`, `Bivalent_Start_Temp`. Kein bivalenter Erzeuger |
|  |  | 66 | ganz |  |  | Referenz: Bivalenz, Starttemperatur (Rohwert − 128). Original: `SetBivalentAPStartTemp`, `Bivalent_Advanced_Start_Temp` |
|  |  | 67 | ganz |  |  | Referenz: Bivalenz, Startverzögerung (Rohwert − 1). Original: `Bivalent_Advanced_Start_Delay` |
|  |  | 68 | ganz |  |  | Referenz: Bivalenz, Stopptemperatur (Rohwert − 128). Original: `SetBivalentAPStopTemp`, `Bivalent_Advanced_Stop_Temp` |
|  |  | 69 | ganz |  |  | Referenz: Bivalenz, Stoppverzögerung (Rohwert − 1). Original: `Bivalent_Advanced_Stop_Delay` |
|  |  | 70 | ganz |  |  | Referenz: Bivalenz, Verzögerung Warmwasser (Rohwert − 1). Original: `Bivalent_Advanced_DHW_Delay` |
| SET23 | `SGReadyCapacity1DHW` | 71 | ganz | TOP94 | `SGReady_Capacity1_DHW` | SG Ready Stufe 1, Leistung Warmwasser (%, Rohwert − 1). Die Referenz beschriftet Byte 71 als Heizen; maßgeblich ist die Firmware, an dieser Anlage ausgetestet |
| SET22 | `SGReadyCapacity1Heat` | 72 | ganz | TOP93 | `SGReady_Capacity1_Heat` | SG Ready Stufe 1, Leistung Heizen (%, Rohwert − 1); Referenz umgekehrt, siehe Byte 71 |
| SET25 | `SGReadyCapacity2DHW` | 73 | ganz | TOP96 | `SGReady_Capacity2_DHW` | SG Ready Stufe 2, Leistung Warmwasser (%, Rohwert − 1); Referenz umgekehrt, siehe Byte 71 |
| SET24 | `SGReadyCapacity2Heat` | 74 | ganz | TOP95 | `SGReady_Capacity2_Heat` | SG Ready Stufe 2, Leistung Heizen (%, Rohwert − 1); Referenz umgekehrt, siehe Byte 71 |
| SET27 | `Z1HeatCurveTargetHighTemp` | 75 | ganz | TOP29 | `Z1_Heat_Curve_Target_High_Temp` | Heizkurve (°C, Rohwert − 128): Vorlauf bei kaltem Wetter („VL kalt“), gehört zu Byte 77. Im Direktbetrieb dieselbe Speicherstelle wie Byte 38 ² |
| SET28 | `Z1HeatCurveTargetLowTemp` | 76 | ganz | TOP30 | `Z1_Heat_Curve_Target_Low_Temp` | Heizkurve (°C, Rohwert − 128): Vorlauf bei warmem Wetter („VL warm“), gehört zu Byte 78 |
| SET29 | `Z1HeatCurveOutsideLowTemp` | 77 | ganz | TOP32 | `Z1_Heat_Curve_Outside_Low_Temp` | Heizkurve (°C, Rohwert − 128): kalter Außentemperaturpunkt („AT kalt“), −15…15 |
| SET30 | `Z1HeatCurveOutsideHighTemp` | 78 | ganz | TOP31 | `Z1_Heat_Curve_Outside_High_Temp` | Heizkurve (°C, Rohwert − 128): warmer Außentemperaturpunkt („AT warm“), −15…15; ausgemessen, der früher angenommene Bereich 15…35 lag daneben |
|  |  | 79 | ganz |  |  | Referenz: Heizkurve Zone 2, Vorlauf höchster Wert. Original: `SetCurves`, `Z2_Heat_Curve_Target_High_Temp`. Zone 2 entfernt |
|  |  | 80 | ganz |  |  | Referenz: Heizkurve Zone 2, Vorlauf niedrigster Wert. Original: `SetCurves`, `Z2_Heat_Curve_Target_Low_Temp` |
|  |  | 81 | ganz |  |  | Referenz: Heizkurve Zone 2, Außentemperatur niedrigster Wert. Original: `SetCurves`, `Z2_Heat_Curve_Outside_Low_Temp` |
|  |  | 82 | ganz |  |  | Referenz: Heizkurve Zone 2, Außentemperatur höchster Wert. Original: `SetCurves`, `Z2_Heat_Curve_Outside_High_Temp` |
| SET21 | `HeatingOffOutdoorTemp` | 83 | ganz | TOP77 | `Heating_Off_Outdoor_Temp` | Heizgrenze (°C, Rohwert − 128): oberhalb dieser Außentemperatur wird nicht geheizt |
| SET16 | `HeatDelta` | 84 | ganz | TOP23 | `Heat_Delta` | Spreizung Heizen (K, Rohwert − 128) |
| SET20 | `HeaterOnOutdoorTemp` | 85 | ganz | TOP78 | `Heater_On_Outdoor_Temp` | Heizstab erst unterhalb dieser Außentemperatur zugelassen (°C, Rohwert − 128) |
| SET31 | `Z1CoolCurveTargetHighTemp` | 86 | ganz | TOP72 | `Z1_Cool_Curve_Target_High_Temp` | Kühlkurve (°C, Rohwert − 128): Vorlauf bei kühlem Wetter, gehört zu Byte 88. Im Direktbetrieb dieselbe Speicherstelle wie Byte 39 ² |
| SET32 | `Z1CoolCurveTargetLowTemp` | 87 | ganz | TOP73 | `Z1_Cool_Curve_Target_Low_Temp` | Kühlkurve (°C, Rohwert − 128): Vorlauf bei heißem Wetter, gehört zu Byte 89 |
| SET33 | `Z1CoolCurveOutsideLowTemp` | 88 | ganz | TOP75 | `Z1_Cool_Curve_Outside_Low_Temp` | Kühlkurve (°C, Rohwert − 128): kühler Außentemperaturpunkt, 15…30 |
| SET34 | `Z1CoolCurveOutsideHighTemp` | 89 | ganz | TOP74 | `Z1_Cool_Curve_Outside_High_Temp` | Kühlkurve (°C, Rohwert − 128): heißer Außentemperaturpunkt, 15…30 |
|  |  | 90 | ganz |  |  | Referenz: Kühlkurve Zone 2, Vorlauf höchster Wert. Original: `SetCurves`, `Z2_Cool_Curve_Target_High_Temp`. Zone 2 entfernt |
|  |  | 91 | ganz |  |  | Referenz: Kühlkurve Zone 2, Vorlauf niedrigster Wert. Original: `SetCurves`, `Z2_Cool_Curve_Target_Low_Temp` |
|  |  | 92 | ganz |  |  | Referenz: Kühlkurve Zone 2, Außentemperatur niedrigster Wert. Original: `SetCurves`, `Z2_Cool_Curve_Outside_Low_Temp` |
|  |  | 93 | ganz |  |  | Referenz: Kühlkurve Zone 2, Außentemperatur höchster Wert. Original: `SetCurves`, `Z2_Cool_Curve_Outside_High_Temp` |
| SET17 | `CoolDelta` | 94 | ganz | TOP24 | `Cool_Delta` | Spreizung Kühlen (K, Rohwert − 128) |
|  |  | 95 | ganz | TOP79 | `Heat_To_Cool_Temp` | Außentemperatur für den Wechsel Heizen → Kühlen im Auto-Betrieb (°C, Rohwert − 128); ob beschreibbar, ist nicht geprüft (3a) |
|  |  | 96 | ganz | TOP80 | `Cool_To_Heat_Temp` | Außentemperatur für den Wechsel Kühlen → Heizen im Auto-Betrieb (°C, Rohwert − 128); ob beschreibbar, ist nicht geprüft (3a) |
| SET26 | `DHWRoomMaxTime` | 97 | ganz | TOP98 | `DHW_Room_Max_Time` | Höchste Laufzeit Raumbetrieb, Warmwasser-Einstellung (min, (Rohwert − 1) × 30). SET26 schreibt 30-min-Schritte, TOP98 meldet Minuten ⁴ |
| SET19 | `DHWHeatupTime` | 98 | ganz | TOP97 | `DHW_Heatup_Time` | Höchste Aufheizzeit Warmwasser (min, Rohwert − 1) |
| SET18 | `DHWHeatDelta` | 99 | ganz | TOP22 | `DHW_Heat_Delta` | Nachheiz-Hysterese Warmwasser (K, Rohwert − 128) |
|  |  | 100 | ganz | TOP70 | `Sterilization_Temp` | Desinfektionstemperatur (°C, Rohwert − 128); ob beschreibbar, ist nicht geprüft (3a) |
|  |  | 101 | ganz | TOP71 | `Sterilization_Max_Time` | Höchste Dauer der Desinfektion (min, Rohwert − 1); ob beschreibbar, ist nicht geprüft (3a) |
|  |  | 102 | ganz |  |  | Referenz: stets `0x00` |
|  |  | 103 | ganz |  |  | Referenz: stets `0x00` |
|  |  | 104 | ganz |  |  | Referenz: Verzögerung interner Heizstab (J/K/L-Serie, Rohwert − 1). Original: `SetHeaterDelayTime`, `Heater_Delay_Time`. Serie passt nicht |
|  |  | 105 | ganz |  |  | Referenz: Einschalt-Delta interner Heizstab, Raumheizung (J/K/L-Serie, Rohwert − 128). Original: `SetHeaterStartDelta`, `Heater_Start_Delta` |
|  |  | 106 | ganz |  |  | Referenz: Ausschalt-Delta interner Heizstab, Raumheizung (J/K/L-Serie, Rohwert − 128). Original: `SetHeaterStopDelta`, `Heater_Stop_Delta` |
|  |  | 107 | ganz |  |  | Referenz: stets `0x00` |
|  |  | 108 | ganz |  |  | Referenz: stets `0x00` |
|  |  | 109 | ganz |  |  | Referenz: stets `0x00`. Letztes Nutzbyte des Kommandos; dort folgt als Byte 110 die Prüfsumme |
|  |  | 110 | 1+2 | TOP99 | `Quiet_Mode_Active` | Flüsterbetrieb läuft tatsächlich, 0 = nein, 1 = ja; nur an/aus, die Stufe steht in TOP18. Gemessen 2026-08-15 |
|  |  | 110 | 3+4 | TOP100 | `Powerful_Mode_Active` | Powerful läuft tatsächlich. Gemessen 2026-08-16 |
|  |  | 110 | 5+6 | TOP101 | `Heat_Cool_SW_State` | Tatsächliche Richtung 0 = Heizen, 1 = Kühlen; die Regelgröße statt TOP4. Der Notbetriebsknopf „Heizen“ ist nur bei sauber gelesener 0 frei |
|  |  | 110 | 7+8 | TOP102 | `External_SW_State` | Externer Schalter. Eingang an dieser Anlage nicht belegt, steht dauerhaft auf `b01`; nicht prüfbar |
|  |  | 111 | 1+2 |  |  | unbelegt |
|  |  | 111 | 3+4 |  |  | unbelegt |
|  |  | 111 | 5+6 | TOP26 | `Defrosting_State` | Abtauen läuft; Wirkungsnachweis für SET12 |
|  |  | 111 | 7+8 | TOP20 | `ThreeWay_Valve_State` | Dreiwegeventil 0 = Raum, 1 = Warmwasser |
|  |  | 112 | 1+2 |  |  | Referenz: Kesselkontakt (Bivalenz) aus/ein. Kein Kessel an dieser Anlage |
|  |  | 112 | 3+4 |  |  | unbelegt |
|  |  | 112 | 5+6 | TOP61 | `External_Heater_State` | Externer Heizstab läuft |
|  |  | 112 | 7+8 | TOP60 | `Internal_Heater_State` | Interner Heizstab läuft, Raum oder Warmwasser |
|  |  | 113 | ganz | TOP44 | `Error` | Fehlertyp: `0xB1` = F-Fehler, `0xA1` = H-Fehler, sonst „No error“; Nummer in Byte 114 |
|  |  | 114 | ganz | TOP44 | `Error` | Fehlernummer: Rohwert − 17, hexadezimal ausgegeben (`0x56` → 45) |
|  |  | 115 | ganz |  |  | Referenz: Bedeutung unbekannt |
|  |  | 116 | 1+2 |  |  | Referenz: Pumpe Zone 2 aus/ein. Original: `Z2_Pump_State` |
|  |  | 116 | 3+4 |  |  | Referenz: Pumpe Zone 1 aus/ein. Original: `Z1_Pump_State` |
|  |  | 116 | 5+6 |  |  | Referenz: Zweiwegeventil Kühlen/Heizen. Original: `TwoWay_Valve_State` |
|  |  | 116 | 7+8 |  |  | Referenz: Dreiwegeventil Raum/Speicher. Original: `ThreeWay_Valve_State2`; vgl. TOP20 auf Byte 111 |
|  |  | 117 | 1+2 |  |  | unbelegt |
|  |  | 117 | 3+4 |  |  | unbelegt |
|  |  | 117 | 5+6 | TOP69 | `Sterilization_State` | Desinfektion läuft; Wirkungsnachweis für SET13 |
|  |  | 117 | 7 |  |  | Referenz: Zone 2 aktiv |
|  |  | 117 | 8 |  |  | Referenz: Zone 1 aktiv |
|  |  | 118 | 1+2 |  |  | unbelegt |
|  |  | 118 | 3–5 | TOP6 | `Main_Outlet_Temp` | Nachkommastelle zum Vorlauf, Byte 144: 1–4 → .00/.25/.50/.75, sonst keine |
|  |  | 118 | 6–8 | TOP5 | `Main_Inlet_Temp` | Nachkommastelle zum Rücklauf, Byte 143; Kodierung wie Bits 3–5 |
|  |  | 119 | ganz |  |  | Referenz: Bedeutung unbekannt |
|  |  | 120 | 1+2 |  |  | unbelegt |
|  |  | 120 | 3+4 |  |  | Referenz: Custom-Menü, Heizstab inaktiv/aktiv |
|  |  | 120 | 5+6 |  |  | Referenz: Custom-Menü, Kühlen inaktiv/aktiv |
|  |  | 120 | 7+8 |  |  | unbelegt |
|  |  | 121 | ganz |  |  | Referenz: stets `0x00` |
|  |  | 122 | ganz |  |  | Referenz: stets `0x00` |
|  |  | 123 | ganz |  |  | Referenz: stets `0x00` |
|  |  | 124 | ganz |  |  | Referenz: stets `0x00` |
|  |  | 125 | ganz |  |  | Referenz: Wasserdruck in bar, (Rohwert − 1) / 50 (K/L-Serie). Original: `Water_Pressure` |
|  |  | 126 | ganz |  |  | Referenz: Wassereintritt 2 (L-Serie, Rohwert − 128). Original: `Second_Inlet_Temp` |
|  |  | 127 | ganz |  |  | Referenz: Economizer-Austritt (K/L-Serie, Rohwert − 128). Original: `Economizer_Outlet_Temp` |
|  |  | 128 | ganz |  |  | Referenz: stets `0x00`. Das Original liest hier `Second_Room_Thermostat_Temp`, die Referenz führt das zweite Raumthermostat auf Byte 200; ungeklärt |
|  |  | 129 | ganz |  |  | Referenz: Modellkennung, 10 Bytes bis Byte 138 (Tabelle `HeatPumpType.md` im Original-Projekt). Original: `Heat_Pump_Model` |
|  |  | 130 | ganz |  |  | Referenz: Modellkennung, Fortsetzung von Byte 129 |
|  |  | 131 | ganz |  |  | Referenz: Modellkennung, Fortsetzung von Byte 129 |
|  |  | 132 | ganz |  |  | Referenz: Modellkennung, Fortsetzung von Byte 129 |
|  |  | 133 | ganz |  |  | Referenz: Modellkennung, Fortsetzung von Byte 129 |
|  |  | 134 | ganz |  |  | Referenz: Modellkennung, Fortsetzung von Byte 129 |
|  |  | 135 | ganz |  |  | Referenz: Modellkennung, Fortsetzung von Byte 129 |
|  |  | 136 | ganz |  |  | Referenz: Modellkennung, Fortsetzung von Byte 129 |
|  |  | 137 | ganz |  |  | Referenz: Modellkennung, Fortsetzung von Byte 129 |
|  |  | 138 | ganz |  |  | Referenz: Modellkennung, Fortsetzung von Byte 129 |
|  |  | 139 | ganz | TOP56 | `Z1_Temp` | Ist-Temperatur Zone 1 (°C, Rohwert − 128) |
|  |  | 140 | ganz |  |  | Referenz: Ist-Temperatur Zone 2 (Rohwert − 128). Original: `Z2_Temp`. Zone 2 entfernt |
|  |  | 141 | ganz | TOP10 | `DHW_Temp` | Warmwasser-Ist (°C, Rohwert − 128); Stufe 1 hat keinen Speicher und meldet −128 |
|  |  | 142 | ganz | TOP14 | `Outside_Temp` | Außentemperatur (°C, Rohwert − 128) |
|  |  | 143 | ganz | TOP5 | `Main_Inlet_Temp` | Rücklauf, Wassereintritt (°C, Rohwert − 128); Nachkommastelle aus Byte 118 |
|  |  | 144 | ganz | TOP6 | `Main_Outlet_Temp` | Vorlauf, Wasseraustritt (°C, Rohwert − 128); Nachkommastelle aus Byte 118 |
|  |  | 145 | ganz | TOP36 | `Z1_Water_Temp` | Wassertemperatur Zone 1 (°C, Rohwert − 128) |
|  |  | 146 | ganz |  |  | Referenz: Wassertemperatur Zone 2 (Rohwert − 128). Original: `Z2_Water_Temp` |
|  |  | 147 | ganz | TOP42 | `Z1_Water_Target_Temp` | Wasser-Solltemperatur Zone 1 (°C, Rohwert − 128) |
|  |  | 148 | ganz |  |  | Referenz: Wasser-Solltemperatur Zone 2 (Rohwert − 128). Original: `Z2_Water_Target_Temp` |
|  |  | 149 | ganz | TOP46 | `Buffer_Temp` | Pufferspeicher (°C, Rohwert − 128); an dieser Anlage nicht vorhanden |
|  |  | 150 | ganz | TOP47 | `Solar_Temp` | Solar (°C, Rohwert − 128); an dieser Anlage nicht vorhanden |
|  |  | 151 | ganz | TOP48 | `Pool_Temp` | Pool (°C, Rohwert − 128); an dieser Anlage nicht vorhanden |
|  |  | 152 | ganz |  |  | Referenz: Heiz-Sollwert (Verschiebung oder Direktwert) einschließlich ΔT Puffer (Rohwert − 128); auch im Original ohne Topic |
|  |  | 153 | ganz | TOP7 | `Main_Target_Temp` | Vorlauf-Solltemperatur (°C, Rohwert − 128) |
|  |  | 154 | ganz | TOP49 | `Main_Hex_Outlet_Temp` | Austritt 2 hinter dem Wärmetauscher (°C, Rohwert − 128) |
|  |  | 155 | ganz | TOP50 | `Discharge_Temp` | Heißgas am Verdichteraustritt (°C, Rohwert − 128) |
|  |  | 156 | ganz | TOP33 | `Room_Thermostat_Temp` | Raumthermostat RC-1, interner Fühler (°C, Rohwert − 128) |
|  |  | 157 | ganz | TOP51 | `Inside_Pipe_Temp` | Kältemittelleitung innen (°C, Rohwert − 128) |
|  |  | 158 | ganz | TOP21 | `Outside_Pipe_Temp` | Kältemittelleitung außen (°C, Rohwert − 128) |
|  |  | 159 | ganz | TOP52 | `Defrost_Temp` | Abtaufühler (°C, Rohwert − 128) |
|  |  | 160 | ganz | TOP53 | `Eva_Outlet_Temp` | Verdampferaustritt (°C, Rohwert − 128) |
|  |  | 161 | ganz | TOP54 | `Bypass_Outlet_Temp` | Bypass-Austritt (°C, Rohwert − 128) |
|  |  | 162 | ganz | TOP55 | `Ipm_Temp` | Leistungsmodul IPM (°C, Rohwert − 128) |
|  |  | 163 | ganz | TOP64 | `High_Pressure` | Hochdruck (kgf/cm², (Rohwert − 1) / 5) |
|  |  | 164 | ganz | TOP66 | `Low_Pressure` | Niederdruck (kgf/cm², Rohwert − 1). An beiden Stufen konstant `0x01`, also 0 — dieses Modell meldet keinen Niederdruck. Wird beim nächsten Firmware-Release entfernt, die Nummer bleibt frei |
|  |  | 165 | ganz | TOP67 | `Compressor_Current` | Stromaufnahme Außeneinheit (A, (Rohwert − 1) / 5) |
|  |  | 166 | ganz | TOP8 | `Compressor_Freq` | Verdichterfrequenz (Hz, Rohwert − 1) |
|  |  | 167 | ganz |  |  | Referenz: stets `0x00` |
|  |  | 168 | ganz |  |  | Referenz: stets `0x00` |
|  |  | 169 | ganz | TOP1 | `Pump_Flow` | Durchfluss, Nachkommaanteil: (Rohwert − 1) / 256 |
|  |  | 170 | ganz | TOP1 | `Pump_Flow` | Durchfluss, ganze l/min; TOP1 = Byte 170 + (Byte 169 − 1) / 256 |
|  |  | 171 | ganz | TOP65 | `Pump_Speed` | Ist-Drehzahl der Pumpe (1/min, (Rohwert − 1) × 50) |
|  |  | 172 | ganz | TOP92 | `Pump_Duty` | Ist-Modulation der Pumpe (Duty, Rohwert − 1); folgt im Handbetrieb genau der Grenze aus Byte 45 ⁵ |
|  |  | 173 | ganz | TOP62 | `Fan1_Motor_Speed` | Lüfter 1 (1/min, (Rohwert − 1) × 10) |
|  |  | 174 | ganz | TOP63 | `Fan2_Motor_Speed` | Lüfter 2 (1/min, (Rohwert − 1) × 10) |
|  |  | 175 | ganz |  |  | Referenz: Expansionsventil in Schritten (Rohwert − 1). Original: `Expansion_Valve` |
|  |  | 176 | ganz |  |  | Referenz: vermutlich Bypassventil (Rohwert − 1), dort mit Fragezeichen |
|  |  | 177 | ganz |  |  | Referenz: Mischventil-PID Zone 2 (Rohwert − 1). Original: `Z1_Valve_PID`; Referenz und Original widersprechen sich bei der Zone |
|  |  | 178 | ganz |  |  | Referenz: Mischventil-PID Zone 1 (Rohwert − 1). Original: `Z2_Valve_PID`; siehe Byte 177 |
|  |  | 179 | ganz | TOP12 | `Operations_Counter` | Verdichterstarts, niederwertiges Byte; TOP12 = Byte 180 × 256 + Byte 179 − 1 |
|  |  | 180 | ganz | TOP12 | `Operations_Counter` | Verdichterstarts, höherwertiges Byte |
|  |  | 181 | ganz |  |  | Referenz: stets `0x00` |
|  |  | 182 | ganz | TOP11 | `Operations_Hours` | Betriebsstunden, niederwertiges Byte; TOP11 = Byte 183 × 256 + Byte 182 − 1 |
|  |  | 183 | ganz | TOP11 | `Operations_Hours` | Betriebsstunden, höherwertiges Byte |
|  |  | 184 | ganz |  |  | Referenz: stets `0x00` |
|  |  | 185 | ganz | TOP90 | `Room_Heater_Operations_Hours` | Betriebsstunden Raumheizstab, niederwertiges Byte; Bildung wie TOP11 |
|  |  | 186 | ganz | TOP90 | `Room_Heater_Operations_Hours` | Betriebsstunden Raumheizstab, höherwertiges Byte |
|  |  | 187 | ganz |  |  | Referenz: stets `0x00` |
|  |  | 188 | ganz | TOP91 | `DHW_Heater_Operations_Hours` | Betriebsstunden Warmwasser-Heizstab, niederwertiges Byte; Bildung wie TOP11 |
|  |  | 189 | ganz | TOP91 | `DHW_Heater_Operations_Hours` | Betriebsstunden Warmwasser-Heizstab, höherwertiges Byte |
|  |  | 190 | ganz |  |  | Referenz: stets `0x00` |
|  |  | 191 | ganz |  |  | Referenz: Nennleistung der Wärmepumpe in kW (Rohwert − 1) |
|  |  | 192 | ganz |  |  | Referenz: vermutlich Bauart, 1 = Standard, 2 = T-CAP (dort mit Fragezeichen) |
|  |  | 193 | ganz | TOP16 | `Heat_Energy_Consumption` | Leistungsaufnahme Heizen (W, (Rohwert − 1) × 200) |
|  |  | 194 | ganz | TOP15 | `Heat_Energy_Production` | Wärmeleistung Heizen (W, (Rohwert − 1) × 200) |
|  |  | 195 | ganz | TOP39 | `Cool_Energy_Consumption` | Leistungsaufnahme Kühlen (W, (Rohwert − 1) × 200) |
|  |  | 196 | ganz | TOP38 | `Cool_Energy_Production` | Kälteleistung Kühlen (W, (Rohwert − 1) × 200) |
|  |  | 197 | ganz | TOP41 | `DHW_Energy_Consumption` | Leistungsaufnahme Warmwasser (W, (Rohwert − 1) × 200) |
|  |  | 198 | ganz | TOP40 | `DHW_Energy_Production` | Wärmeleistung Warmwasser (W, (Rohwert − 1) × 200) |
|  |  | 199 | ganz |  |  | Referenz: ab `0x03` fragt der CZ-TAW1 ein Zusatztelegramm mit feiner aufgelösten Energiewerten ab (K/L-Serie). Bewusst nicht übernommen (2026-08-28): diese Anlage ist H-Serie |
|  |  | 200 | ganz |  |  | Referenz: Raumthermostat RC-2, interner Fühler (K/L-Serie, Rohwert − 128) |
|  |  | 201 | ganz |  |  | Referenz: stets `0x00` |
|  |  | 202 | ganz |  |  | Prüfsumme: die Summe aller 203 Bytes ergibt 0 (8 Bit); geprüft in [`src/telegram.h`](src/telegram.h) |

## Was beim Aufstellen aufgefallen ist

* **TOP66 `Low_Pressure` trägt an dieser Anlage nichts.** Byte 164 steht an
  beiden Stufen dauerhaft auf `0x01`: Der ioBroker-Verlauf hat seit Beginn der
  Aufzeichnung am 2026-07-12 je rund 98 000 Werte, alle 0, auch bei laufendem
  Verdichter. Damit ist auch die Frage nach dem Faktor erledigt — diese
  Firmware rechnet Rohwert − 1, die Referenz (Rohwert − 1) / 5, das Original
  seit Dezember 2023 (Rohwert − 1) × 50, und jeder davon ergibt hier 0.
  **Owner-Entscheid 2026-09-18:** TOP66 wird bei der nächsten
  Firmware-Änderung entfernt, die ohnehin den vollen Ablauf bis zum Rollout
  durchläuft — kein eigener Release dafür. Vorbild ist Zone 2 in 3.4.0: Zeile
  raus, die übrigen Topics behalten ihre Nummern.
* **Zwei Widersprüche zwischen Referenz und Original-Code**, beide an Feldern,
  die diese Anlage nicht hat: das zweite Raumthermostat (Byte 128 oder
  Byte 200) und die Zuordnung der Mischventil-PID zu den Zonen (Byte 177 und
  178). Nur festgehalten, nicht verfolgt.
* **Byte 71–74** beschriftet die Referenz vertauscht (Heizen und Warmwasser).
  Das ist erledigt: maßgeblich ist die Firmware, SG Ready ist an dieser Anlage
  ausgetestet — siehe die Vorbehalte in
  [`SET-TOP-Zuordnung.md`](SET-TOP-Zuordnung.md).

## Wie die Tabelle entstanden ist

Die Spalten SET, Kommando, TOP und Status sind aus den beiden Code-Tabellen
erzeugt, über dieselben Parser wie in
[`test/set_top_zuordnung.py`](test/set_top_zuordnung.py), und dabei maschinell
geprüft: Jedes Set-Kommando und jedes State-Topic liegt auf
seinem Byte und deckt dort genau die Bits ab, die sein Dekodierer liest; die
Bitgruppen jedes Bytes überlappen sich nicht und ergeben zusammen alle acht
Bits. Die Mehrbyte-Topics sind gegen die `serial_data[]`-Zugriffe in
`decode.cpp` gehalten.

Aufteilung und Bemerkungen sind von Hand. **Seit dem 2026-09-18 prüft
[`test/doku_zuordnung_test.py`](test/doku_zuordnung_test.py) die Tabelle bei
jedem Lauf der Hosttests** — lokal vor dem Merge und in der CI. Geprüft wird
alles, was sich aus dem Code ableiten lässt: Byte 1–202 lückenlos, die
Bitgruppen jedes Bytes, jedes SET und TOP auf seinem Byte und seinen Bits, die
Namen in *Kommando* und *Status*, Einheit und Kodierung in der Klammer und die
Zahlen in der Stand-Zeile. Wer ein Set-Kommando oder ein State-Topic hinzufügt,
streicht oder umkodiert, bekommt dort ROT, bis diese Datei nachgezogen ist. Die
Bedeutungstexte prüft der Test nicht.
