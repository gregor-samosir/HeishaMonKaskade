#include "HeishaMon.h"
#include "decode.h"
// #include "commands.h"

/*****************************************************************************/
/* Einheiten und Klartexte fuer die Weboberflaeche                           */
/* Index 0 == "value" heisst: der Wert ist eine Zahl und Index 1 ist ihre    */
/* Einheit. Sonst ist der Wert ein Index in die Liste der Klartexte.         */
/*                                                                           */
/* JEDE Liste endet mit nullptr, auch die Einheiten. Daran erkennt           */
/* desc_text() (decode.h) das Ende, ohne die Laenge zu kennen - der Nachschlag*/
/* in der Weboberflaeche kann damit nicht mehr ueber das Array hinauslesen.  */
/*                                                                           */
/* Ausserdem deckt jede Liste den GESAMTEN Indexbereich ihres Dekodierers ab, */
/* aufgefuellt mit "unknown": die 2-Bit-Dekodierer (getBit1and2 usw.) liefern */
/* nach ihrem -1 die Werte -1..2, brauchen also drei Eintraege; die 3-Bit-    */
/* Dekodierer (getRight3bits, getBit3and4and5) liefern -1..6 und brauchen     */
/* sieben. Vorher endeten die meisten Listen bei zwei Eintraegen - ein        */
/* Rohwert b11 von der Waermepumpe zeigte damit in den Speicher dahinter.     */
/* Das haelt byte110_test Fall 4 fuer die ganze Tabelle nach.                 */
/*****************************************************************************/
static const char *DisabledEnabled[] = {"Disabled", "Enabled", "unknown", nullptr};
static const char *BlockedFree[] = {"Blocked", "Free", "unknown", nullptr};
static const char *OffOn[] = {"Off", "On", "unknown", nullptr};
static const char *InactiveActive[] = {"Inactive", "Active", "unknown", nullptr};
static const char *HolidayState[] = {"Off", "Scheduled", "Active", nullptr};
static const char *OpModeDesc[] = {"Heat", "Cool", "Auto(Heat)", "DHW", "Heat+DHW", "Cool+DHW", "Auto(Heat)+DHW", "Auto(Cool)", "Auto(Cool)+DHW", nullptr};
static const char *Powerfulmode[] = {"Off", "30min", "60min", "90min", "unknown", "unknown", "unknown", nullptr};
static const char *Quietmode[] = {"Off", "Level 1", "Level 2", "Level 3", "unknown", "unknown", "unknown", nullptr};
static const char *Valve[] = {"Room", "DHW", "unknown", nullptr};
static const char *LitersPerMin[] = {"value", "l/min", nullptr};
static const char *RotationsPerMin[] = {"value", "1/min", nullptr};
static const char *Pressure[] = {"value", "Kgf/cm2", nullptr};
static const char *Celsius[] = {"value", "&deg;C", nullptr};
static const char *Kelvin[] = {"value", "K", nullptr};
static const char *Hertz[] = {"value", "Hz", nullptr};
static const char *Counter[] = {"value", "Count", nullptr};
static const char *Hours[] = {"value", "Hours", nullptr};
static const char *Watt[] = {"value", "Watt", nullptr};
static const char *ErrorState[] = {"value", "Error", nullptr};
static const char *Ampere[] = {"value", "Ampere", nullptr};
static const char *Minutes[] = {"value", "Minutes", nullptr};
static const char *Duty[] = {"value", "Duty", nullptr};
static const char *HeatCoolModeDesc[] = {"Comp. Curve", "Direct", "unknown", nullptr};
static const char *Percent[] = {"value", "&#37", nullptr};
// Die beiden Arrays fuer Byte 110 haben seit 3.7.0 DREI Elemente, obwohl nur
// zwei Zustaende beobachtet sind: Ein 2-Bit-Feld kann b11 liefern, das ergibt
// nach "Rohwert - 1" den Index 2. Damals war das die Ausnahme, seit 3.9.0 ist
// es die Regel fuer alle Listen (siehe oben). Der Bereich -1..2 ist damit
// vollstaendig gedeckt: b00 -> -1 -> leer, b01/b10/b11 -> 0/1/2 -> im Array.
static const char *OffOnUnknown[] = {"Off", "On", "unknown", nullptr};
static const char *HeatCoolActual[] = {"Heat", "Cool", "unknown", nullptr};
// Betriebsart der Umwaelzpumpe (Byte 4, Bits 3+4). Drei Eintraege decken den
// Indexbereich -1..2 von getBit3and4 vollstaendig ab. "Auto" und "Fix" sind am
// 2026-08-19 an WP1 gemessen (Byte 4 wechselte 0x55 <-> 0x65, die Pumpe lief
// dabei fix auf dem Max-Duty-Wert); "Air purge" stammt aus ProtocolByteDecrypt.md
// und ist hier bewusst nicht nachgestellt worden - das haette an einer intakten
// Anlage eine Entlueftungsroutine ausgeloest.
static const char *WaterPumpMode[] = {"Auto", "Fix", "Air purge", nullptr};


/*****************************************************************************/
/* Die State-Topic-Tabelle - eine Zeile pro Topic                            */
/*                                                                           */
/*   TOP-Nr | Byte im Telegramm | Name | 1-Byte-Dekodierer |                 */
/*   Mehrbyte-Dekodierer | Einheit/Klartexte    (siehe struct StateTopic)    */
/* 'number' ist die TOP-Nummer als Datenfeld, nicht der Index: Zeilen        */
/* koennen entfallen, ohne dass sich die Nummern der uebrigen verschieben.   */
/*                                                                           */
/* Genau das ist hier passiert - die Nummerierung hat Luecken bei TOP34,     */
/* 35, 37, 43, 57 und 82-89. Das waren die Zone-2-Topics, entfallen in       */
/* 3.4.0, weil diese Anlage keine Zone 2 hat. Die Luecken sind Absicht:      */
/* TOP36 heisst in MQTT-Topics.md und in jedem alten Mitschnitt weiter       */
/* TOP36. Bitte nicht durchnummerieren.                                      */
/*****************************************************************************/
const StateTopic stateTopics[NUMBEROFTOPICS] = {
    {  0,   4, "Heatpump_State",                   getBit7and8,          nullptr,                   OffOn},
    {  1,   0, "Pump_Flow",                        nullptr,              getPumpFlow,               LitersPerMin},
    {  2,   4, "Force_DHW_State",                  getBit1and2,          nullptr,                   DisabledEnabled},
    {  3,   7, "Quiet_Mode_Schedule",              getBit1and2,          nullptr,                   DisabledEnabled},
    {  4,   6, "Operating_Mode_State",             getOpMode,            nullptr,                   OpModeDesc},
    {  5, 143, "Main_Inlet_Temp",                  getIntMinus128,       getInletTempWithFraction,  Celsius},
    {  6, 144, "Main_Outlet_Temp",                 getIntMinus128,       getOutletTempWithFraction, Celsius},
    {  7, 153, "Main_Target_Temp",                 getIntMinus128,       nullptr,                   Celsius},
    {  8, 166, "Compressor_Freq",                  getIntMinus1,         nullptr,                   Hertz},
    {  9,  42, "DHW_Target_Temp",                  getIntMinus128,       nullptr,                   Celsius},
    { 10, 141, "DHW_Temp",                         getIntMinus128,       nullptr,                   Celsius},
    { 11,   0, "Operations_Hours",                 nullptr,              getOperationHour,          Hours},
    { 12,   0, "Operations_Counter",               nullptr,              getOperationCount,         Counter},
    { 13,   5, "Main_Schedule_State",              getBit1and2,          nullptr,                   DisabledEnabled},
    { 14, 142, "Outside_Temp",                     getIntMinus128,       nullptr,                   Celsius},
    { 15, 194, "Heat_Energy_Production",           getIntMinus1Times200, nullptr,                   Watt},
    { 16, 193, "Heat_Energy_Consumption",          getIntMinus1Times200, nullptr,                   Watt},
    { 17,   7, "Powerful_Mode_Time",               getRight3bits,        nullptr,                   Powerfulmode},
    { 18,   7, "Quiet_Mode_Level",                 getBit3and4and5,      nullptr,                   Quietmode},
    { 19,   5, "Holiday_Mode_State",               getBit3and4,          nullptr,                   HolidayState},
    { 20, 111, "ThreeWay_Valve_State",             getBit7and8,          nullptr,                   Valve},
    { 21, 158, "Outside_Pipe_Temp",                getIntMinus128,       nullptr,                   Celsius},
    { 22,  99, "DHW_Heat_Delta",                   getIntMinus128,       nullptr,                   Kelvin},
    { 23,  84, "Heat_Delta",                       getIntMinus128,       nullptr,                   Kelvin},
    { 24,  94, "Cool_Delta",                       getIntMinus128,       nullptr,                   Kelvin},
    { 25,  44, "DHW_Holiday_Shift_Temp",           getIntMinus128,       nullptr,                   Kelvin},
    { 26, 111, "Defrosting_State",                 getBit5and6,          nullptr,                   DisabledEnabled},
    { 27,  38, "Z1_Heat_Request_Temp",             getIntMinus128,       nullptr,                   Celsius},
    { 28,  39, "Z1_Cool_Request_Temp",             getIntMinus128,       nullptr,                   Celsius},
    // Target_High is the flow when it is COLD ("VL kalt"), Target_Low the flow
    // when it is WARM ("VL warm") - the two pairs cross over, see MQTT-Topics.md.
    { 29,  75, "Z1_Heat_Curve_Target_High_Temp",   getIntMinus128,       nullptr,                   Celsius},
    { 30,  76, "Z1_Heat_Curve_Target_Low_Temp",    getIntMinus128,       nullptr,                   Celsius},
    { 31,  78, "Z1_Heat_Curve_Outside_High_Temp",  getIntMinus128,       nullptr,                   Celsius},
    { 32,  77, "Z1_Heat_Curve_Outside_Low_Temp",   getIntMinus128,       nullptr,                   Celsius},
    { 33, 156, "Room_Thermostat_Temp",             getIntMinus128,       nullptr,                   Celsius},
    { 36, 145, "Z1_Water_Temp",                    getIntMinus128,       nullptr,                   Celsius},
    { 38, 196, "Cool_Energy_Production",           getIntMinus1Times200, nullptr,                   Watt},
    { 39, 195, "Cool_Energy_Consumption",          getIntMinus1Times200, nullptr,                   Watt},
    { 40, 198, "DHW_Energy_Production",            getIntMinus1Times200, nullptr,                   Watt},
    { 41, 197, "DHW_Energy_Consumption",           getIntMinus1Times200, nullptr,                   Watt},
    { 42, 147, "Z1_Water_Target_Temp",             getIntMinus128,       nullptr,                   Celsius},
    { 44,   0, "Error",                            nullptr,              getErrorInfo,              ErrorState},
    { 45,  43, "Room_Holiday_Shift_Temp",          getIntMinus128,       nullptr,                   Kelvin},
    { 46, 149, "Buffer_Temp",                      getIntMinus128,       nullptr,                   Celsius},
    { 47, 150, "Solar_Temp",                       getIntMinus128,       nullptr,                   Celsius},
    { 48, 151, "Pool_Temp",                        getIntMinus128,       nullptr,                   Celsius},
    { 49, 154, "Main_Hex_Outlet_Temp",             getIntMinus128,       nullptr,                   Celsius},
    { 50, 155, "Discharge_Temp",                   getIntMinus128,       nullptr,                   Celsius},
    { 51, 157, "Inside_Pipe_Temp",                 getIntMinus128,       nullptr,                   Celsius},
    { 52, 159, "Defrost_Temp",                     getIntMinus128,       nullptr,                   Celsius},
    { 53, 160, "Eva_Outlet_Temp",                  getIntMinus128,       nullptr,                   Celsius},
    { 54, 161, "Bypass_Outlet_Temp",               getIntMinus128,       nullptr,                   Celsius},
    { 55, 162, "Ipm_Temp",                         getIntMinus128,       nullptr,                   Celsius},
    { 56, 139, "Z1_Temp",                          getIntMinus128,       nullptr,                   Celsius},
    { 58,   9, "DHW_Heater_State",                 getBit5and6,          nullptr,                   BlockedFree},
    { 59,   9, "Room_Heater_State",                getBit7and8,          nullptr,                   BlockedFree},
    { 60, 112, "Internal_Heater_State",            getBit7and8,          nullptr,                   InactiveActive},
    { 61, 112, "External_Heater_State",            getBit5and6,          nullptr,                   InactiveActive},
    { 62, 173, "Fan1_Motor_Speed",                 getIntMinus1Times10,  nullptr,                   RotationsPerMin},
    { 63, 174, "Fan2_Motor_Speed",                 getIntMinus1Times10,  nullptr,                   RotationsPerMin},
    { 64, 163, "High_Pressure",                    getIntMinus1Div5,     nullptr,                   Pressure},
    { 65, 171, "Pump_Speed",                       getIntMinus1Times50,  nullptr,                   RotationsPerMin},
    { 66, 164, "Low_Pressure",                     getIntMinus1,         nullptr,                   Pressure},
    { 67, 165, "Compressor_Current",               getIntMinus1Div5,     nullptr,                   Ampere},
    { 68,   5, "Force_Heater_State",               getBit5and6,          nullptr,                   InactiveActive},
    { 69, 117, "Sterilization_State",              getBit5and6,          nullptr,                   InactiveActive},
    { 70, 100, "Sterilization_Temp",               getIntMinus128,       nullptr,                   Celsius},
    { 71, 101, "Sterilization_Max_Time",           getIntMinus1,         nullptr,                   Minutes},
    { 72,  86, "Z1_Cool_Curve_Target_High_Temp",   getIntMinus128,       nullptr,                   Celsius},
    { 73,  87, "Z1_Cool_Curve_Target_Low_Temp",    getIntMinus128,       nullptr,                   Celsius},
    { 74,  89, "Z1_Cool_Curve_Outside_High_Temp",  getIntMinus128,       nullptr,                   Celsius},
    { 75,  88, "Z1_Cool_Curve_Outside_Low_Temp",   getIntMinus128,       nullptr,                   Celsius},
    { 76,  28, "Heating_Mode",                     getBit7and8,          nullptr,                   HeatCoolModeDesc},
    { 77,  83, "Heating_Off_Outdoor_Temp",         getIntMinus128,       nullptr,                   Celsius},
    { 78,  85, "Heater_On_Outdoor_Temp",           getIntMinus128,       nullptr,                   Celsius},
    { 79,  95, "Heat_To_Cool_Temp",                getIntMinus128,       nullptr,                   Celsius},
    { 80,  96, "Cool_To_Heat_Temp",                getIntMinus128,       nullptr,                   Celsius},
    { 81,  28, "Cooling_Mode",                     getBit5and6,          nullptr,                   HeatCoolModeDesc},
    { 90,   0, "Room_Heater_Operations_Hours",     nullptr,              getRoomHeaterHour,         Hours},
    { 91,   0, "DHW_Heater_Operations_Hours",      nullptr,              getDHWHeaterHour,          Hours},
    { 92, 172, "Pump_Duty",                        getIntMinus1,         nullptr,                   Duty},
    { 93,  72, "SGReady_Capacity1_Heat",           getIntMinus1,         nullptr,                   Percent},
    { 94,  71, "SGReady_Capacity1_DHW",            getIntMinus1,         nullptr,                   Percent},
    { 95,  74, "SGReady_Capacity2_Heat",           getIntMinus1,         nullptr,                   Percent},
    { 96,  73, "SGReady_Capacity2_DHW",            getIntMinus1,         nullptr,                   Percent},
    { 97,  98, "DHW_Heatup_Time",                  getIntMinus1,         nullptr,                   Minutes},
    { 98,  97, "DHW_Room_Max_Time",                getIntMinus1Times30,  nullptr,                   Minutes},
    // Byte 110 - die IST-Zustaende der Waermepumpe. Im Original-HeishaMon nicht
    // dekodiert; die Bitzuordnung stammt aus ProtocolByteDecrypt.md und ist am
    // 2026-08-15 an WP1 empirisch belegt (Stufentest Quiet 0->1->2->3->0,
    // Moduswechsel Cool->Heat ueber KNX und Heat->Cool ueber SET9), Powerful am
    // 2026-08-16 an der laufenden 3.7.0 (SET4 = 1, TOP100 zog auf 1 nach).
    // Zwei Vorbehalte: Quiet meldet nur AN/AUS, keine Stufe (dafuer TOP18), und
    // TOP102 ist hier gar nicht pruefbar - der External-SW-Eingang ist an
    // dieser Anlage nicht belegt, das Feld bleibt dauerhaft auf b01.
    // Wert ist TOP101: Heat_Cool_SW_State folgt dem tatsaechlichen Zustand
    // unabhaengig davon, wer umgeschaltet hat - anders als Byte 6, das den
    // zuletzt kommandierten Modus zeigt.
    { 99, 110, "Quiet_Mode_Active",                getBit1and2,          nullptr,                   OffOnUnknown},
    {100, 110, "Powerful_Mode_Active",             getBit3and4,          nullptr,                   OffOnUnknown},
    {101, 110, "Heat_Cool_SW_State",               getBit5and6,          nullptr,                   HeatCoolActual},
    {102, 110, "External_SW_State",                getBit7and8,          nullptr,                   OffOnUnknown},
    // Die beiden Ruecklesewerte zu SET15 und SET14 (3.10.0). Beide Bytes sind am
    // 2026-08-19 an WP1 ausgemessen worden, statt sie aus der Referenz zu
    // uebernehmen - Wert verstellt, Rohbyte im Hexlog beobachtet, zurueckgestellt:
    //   Byte 45: set/WaterPumpSpeed 100 -> 110 -> 100 liess das Byte 0x65 -> 0x6F
    //     -> 0x65 wandern; Stufe 2 zeigt mit 125 entsprechend 0x7E. Es ist die
    //     MAX-DUTY-Grenze, keine Drehzahl: bei laufender Pumpe folgte TOP92
    //     Pump_Duty der Grenze exakt (100 -> 100, 80 -> 80, Drehzahl 2300 -> 1500).
    //     Deshalb Pump_Duty_Max und nicht der irrefuehrende Kommandoname.
    //   Byte 4:  set/WaterPump 0 -> 1 -> 0 liess die Bits 3+4 b01 -> b10 -> b01
    //     wandern, die Pumpe lief dabei wirklich an (TOP65 2300 1/min, TOP1
    //     11,95 l/min). Das Feld meldet also den wirksamen Zustand.
    {103,  45, "Pump_Duty_Max",                    getIntMinus1,         nullptr,                   Duty},
    {104,   4, "Water_Pump_Mode",                  getBit3and4,          nullptr,                   WaterPumpMode},
};

// Haelt NUMBEROFTOPICS (Array-Groesse von actual_data) und die Tabelle zusammen
static_assert(sizeof(stateTopics) / sizeof(stateTopics[0]) == NUMBEROFTOPICS,
              "NUMBEROFTOPICS passt nicht zur Zeilenzahl von stateTopics[]");


unsigned long nextalldatatime = 0;

// Ein fehlgeschlagenes Publish blieb bis 3.8.1 unbemerkt: der Wert galt als
// gesendet und kam erst mit der naechsten Aenderung oder dem 5-min-Vollupdate
// wieder. Beim Broker stand solange der alte Wert - retained, also mit dem
// Anschein von Gueltigkeit. Diese Marke merkt sich, dass etwas liegenblieb;
// der naechste Durchlauf (5 s) schickt dann die ganze Tabelle erneut.
// EINE Marke statt einer je Zeile: ein Publish scheitert praktisch nur, wenn
// die Verbindung weg ist - dann ist ohnehin die ganze Tabelle betroffen.
static bool republishAfterFailure = false;

void publish_heatpump_data(uint8_t *serial_data, char actual_data[][MAXVALUELEN], PubSubClient &mqtt_client)
{
  char pub_msg[256];
  char mqtt_topic[128]; // fixed buffer instead of std::string concat per publish (heap churn)
  bool updatealltopics = false;

  unsigned long now = millis();
  if (now - nextalldatatime > UPDATEALLTIME)
  {
    updatealltopics = true;
    nextalldatatime = now;
    write_telnet_log((char *)"Publish all topics");
  }

  // Wiederholung aus dem letzten Durchlauf: die Marke wird hier abgeraeumt und
  // unten neu gesetzt, falls es diesmal wieder nicht klappt
  bool retryPending = republishAfterFailure;
  republishAfterFailure = false;

  char top_value[MAXVALUELEN]; // stack buffer, decoders are String-free
  // index laeuft ueber die Tabellenzeilen, die TOP-Nummer kommt aus der Zeile
  for (unsigned int index = 0; index < NUMBEROFTOPICS; index++)
  {
    const StateTopic &topic = stateTopics[index];
    getTopicPayload(index, serial_data, top_value);
    bool changed = (strcmp(actual_data[index], top_value) != 0);

    if (updatealltopics || retryPending || changed)
    {
      // Nur echte Aenderungen ins Log - eine Wiederholung ist keine Aenderung
      // und wuerde das Log sonst alle 5 s fuellen, solange MQTT weg ist
      if (changed)
      {
        (void)snprintf(pub_msg, sizeof(pub_msg), "<PUB> TOP%u %s: %s", topic.number, topic.name, top_value);
        write_mqtt_log(pub_msg);
      }
      strlcpy(actual_data[index], top_value, MAXVALUELEN);
      (void)snprintf(mqtt_topic, sizeof(mqtt_topic), "%s/%s", Topics::STATE.c_str(), topic.name);
      if (!mqtt_client.publish(mqtt_topic, top_value, MQTT_RETAIN_VALUES))
      {
        republishAfterFailure = true; // im naechsten Durchlauf erneut versuchen
      }
    }
  }
}

/*****************************************************************************/
/* calculate the payload                                                     */
/* out must hold at least MAXVALUELEN bytes                                  */
/*                                                                           */
/* Frueher entschied hier ein switch ueber fest verdrahtete TOP-Nummern,     */
/* welches Topic mehrere Bytes braucht. Jede Verschiebung der Nummerierung   */
/* haette diese case-Marken stillschweigend auf andere Topics zeigen lassen  */
/* - es kompiliert ja weiter. Jetzt bringt die Tabellenzeile ihren           */
/* Dekodierer selbst mit, die Zuordnung kann nicht mehr verrutschen.         */
/*****************************************************************************/
void getTopicPayload(unsigned int index, uint8_t *serial_data, char *out)
{
  // Aufrufer indizieren mit Schleifenzaehlern: Bereich pruefen, statt am Ende
  // der Tabelle vorbeizulesen
  if (index >= NUMBEROFTOPICS)
  {
    (void)strlcpy(out, "-1", MAXVALUELEN);
    return;
  }

  const StateTopic &topic = stateTopics[index];

  if (topic.wide != nullptr) // Topic braucht mehrere Bytes des Telegramms
  {
    topic.wide(&topic, serial_data, out);
    return;
  }

  if (topic.decode == nullptr) // weder 1-Byte- noch Mehrbyte-Dekodierer: Tabellenfehler
  {
    (void)strlcpy(out, "-1", MAXVALUELEN);
    return;
  }

  topic.decode(serial_data[topic.pos], out);
}

/*****************************************************************************/
/* 1-byte decoders: extract value and format it into the caller buffer      */
/*****************************************************************************/
void getBit1and2(byte input, char *out)
{
  (void)snprintf(out, MAXVALUELEN, "%d", (input >> 6) - 1);
}

void getBit3and4(byte input, char *out)
{
  (void)snprintf(out, MAXVALUELEN, "%d", ((input >> 4) & 0b11) - 1);
}

void getBit5and6(byte input, char *out)
{
  (void)snprintf(out, MAXVALUELEN, "%d", ((input >> 2) & 0b11) - 1);
}

void getBit7and8(byte input, char *out)
{
  (void)snprintf(out, MAXVALUELEN, "%d", (input & 0b11) - 1);
}

void getBit3and4and5(byte input, char *out)
{
  (void)snprintf(out, MAXVALUELEN, "%d", ((input >> 3) & 0b111) - 1);
}

void getRight3bits(byte input, char *out)
{
  (void)snprintf(out, MAXVALUELEN, "%d", (input & 0b111) - 1);
}

void getIntMinus1(byte input, char *out)
{
  (void)snprintf(out, MAXVALUELEN, "%d", (int)input - 1);
}

void getIntMinus128(byte input, char *out)
{
  (void)snprintf(out, MAXVALUELEN, "%d", (int)input - 128);
}

void getIntMinus1Div5(byte input, char *out)
{
  // dtostrf instead of snprintf %f: float formatting like String(value, 1)
  (void)dtostrf(((float)input - 1) / 5, 1, 1, out);
}

void getIntMinus1Times10(byte input, char *out)
{
  (void)snprintf(out, MAXVALUELEN, "%d", ((int)input - 1) * 10);
}

void getIntMinus1Times50(byte input, char *out)
{
  (void)snprintf(out, MAXVALUELEN, "%d", ((int)input - 1) * 50);
}

void getIntMinus1Times200(byte input, char *out)
{
  (void)snprintf(out, MAXVALUELEN, "%d", ((int)input - 1) * 200);
}

void getIntMinus1Times30(byte input, char *out)
{
  (void)snprintf(out, MAXVALUELEN, "%d", ((int)input - 1) * 30);
}

void getOpMode(byte input, char *out)
{
  int mode;
  switch ((int)(input & 0b111111))
  {
  case 18:
    mode = 0;
    break;
  case 19:
    mode = 1;
    break;
  case 25:
    mode = 2;
    break;
  case 33:
    mode = 3;
    break;
  case 34:
    mode = 4;
    break;
  case 35:
    mode = 5;
    break;
  case 41:
    mode = 6;
    break;
  case 26:
    mode = 7;
    break;
  case 42:
    mode = 8;
    break;
  default:
    mode = -1;
    break;
  }
  (void)snprintf(out, MAXVALUELEN, "%d", mode);
}

/*****************************************************************************/
/* temperatures with fraction: integer part from the standard 1-byte        */
/* decoder, fraction bits from byte 118 (raw 1..4, otherwise no fraction)   */
/*                                                                          */
/* Diese beiden holten Byte und Dekodierer frueher ueber topicBytes[5] bzw.  */
/* [6] aus den Parallel-Tabellen - also ueber eine fest verdrahtete          */
/* TOP-Nummer. Jetzt kommen beide aus der eigenen Tabellenzeile.            */
/*****************************************************************************/
static const char *fractionText[] = {".00", ".25", ".50", ".75"};

// Ganzzahlanteil ueber den 1-Byte-Dekodierer der Zeile, dann die Nachkommastelle
static void appendFraction(const StateTopic *topic, uint8_t *serial_data, char *out, int fractional)
{
  if (topic->decode == nullptr) // Tabellenfehler: Zeile ohne 1-Byte-Dekodierer
  {
    (void)strlcpy(out, "-1", MAXVALUELEN);
    return;
  }
  topic->decode(serial_data[topic->pos], out);
  if (fractional >= 1 && fractional <= 4)
  {
    (void)strlcat(out, fractionText[fractional - 1], MAXVALUELEN);
  }
}

void getInletTempWithFraction(const StateTopic *topic, uint8_t *serial_data, char *out)
{
  appendFraction(topic, serial_data, out, (int)(serial_data[118] & 0b111));
}

void getOutletTempWithFraction(const StateTopic *topic, uint8_t *serial_data, char *out)
{
  appendFraction(topic, serial_data, out, (int)((serial_data[118] >> 3) & 0b111));
}

/*****************************************************************************/
/* multi-byte decoders                                                       */
/*****************************************************************************/
void getPumpFlow(const StateTopic *, uint8_t *serial_data, char *out)
{ // TOP1 //
  float PumpFlow1 = (float)serial_data[170];
  float PumpFlow2 = (((float)serial_data[169] - 1) / 256);
  // dtostrf instead of snprintf %f: float formatting like String(value, 2)
  (void)dtostrf(PumpFlow1 + PumpFlow2, 1, 2, out);
}

void getOperationHour(const StateTopic *, uint8_t *serial_data, char *out)
{
  (void)snprintf(out, MAXVALUELEN, "%d", (int)word(serial_data[183], serial_data[182]) - 1);
}

void getOperationCount(const StateTopic *, uint8_t *serial_data, char *out)
{
  (void)snprintf(out, MAXVALUELEN, "%d", (int)word(serial_data[180], serial_data[179]) - 1);
}

void getRoomHeaterHour(const StateTopic *, uint8_t *serial_data, char *out)
{
  (void)snprintf(out, MAXVALUELEN, "%d", (int)word(serial_data[186], serial_data[185]) - 1);
}

void getDHWHeaterHour(const StateTopic *, uint8_t *serial_data, char *out)
{
  (void)snprintf(out, MAXVALUELEN, "%d", (int)word(serial_data[189], serial_data[188]) - 1);
}

void getErrorInfo(const StateTopic *, uint8_t *serial_data, char *out)
{ // TOP44 //
  int Error_type = (int)serial_data[113];
  int Error_number = ((int)(serial_data[114])) - 17;
  switch (Error_type)
  {
  case 177: // B1=F type error
    (void)snprintf(out, MAXVALUELEN, "F%02X", Error_number);
    break;
  case 161: // A1=H type error
    (void)snprintf(out, MAXVALUELEN, "H%02X", Error_number);
    break;
  default:
    (void)strlcpy(out, "No error", MAXVALUELEN);
    break;
  }
}

/*****************************************************************************/
/* Tabellenindex zu einer TOP-Nummer                                         */
/*                                                                           */
/* stateTopics[] ist nach Zeilen indiziert, nicht nach TOP-Nummern - die      */
/* Nummerierung hat Luecken (Zone 2 ist in 3.4.0 entfallen) und reicht bis    */
/* 104 bei 92 Zeilen. Wer actual_data[] mit einer TOP-Nummer adressiert, liest*/
/* die falsche Zeile oder faellt aus dem Array. Lineare Suche: Sie laeuft im  */
/* Notbetrieb hoechstens sechs Mal je Tick ueber 92 Zeilen und faellt neben   */
/* dem 5-s-Abfragezyklus nicht ins Gewicht.                                   */
/*****************************************************************************/
int state_topic_index(unsigned int top_number)
{
  for (unsigned int i = 0; i < NUMBEROFTOPICS; i++)
  {
    if (stateTopics[i].number == top_number)
    {
      return (int)i;
    }
  }
  return -1;
}
