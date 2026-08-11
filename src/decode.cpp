#include "HeishaMon.h"
#include "decode.h"
// #include "commands.h"

/*****************************************************************************/
/* Einheiten und Klartexte fuer die Weboberflaeche                           */
/* Index 0 == "value" heisst: der Wert ist eine Zahl und Index 1 ist ihre    */
/* Einheit. Sonst ist der Wert ein Index in die Liste der Klartexte.         */
/*****************************************************************************/
static const char *DisabledEnabled[] = {"Disabled", "Enabled"};
static const char *BlockedFree[] = {"Blocked", "Free"};
static const char *OffOn[] = {"Off", "On"};
static const char *InactiveActive[] = {"Inactive", "Active"};
static const char *HolidayState[] = {"Off", "Scheduled", "Active"};
static const char *OpModeDesc[] = {"Heat", "Cool", "Auto(Heat)", "DHW", "Heat+DHW", "Cool+DHW", "Auto(Heat)+DHW", "Auto(Cool)", "Auto(Cool)+DHW"};
static const char *Powerfulmode[] = {"Off", "30min", "60min", "90min"};
static const char *Quietmode[] = {"Off", "Level 1", "Level 2", "Level 3"};
static const char *Valve[] = {"Room", "DHW"};
static const char *LitersPerMin[] = {"value", "l/min"};
static const char *RotationsPerMin[] = {"value", "1/min"};
static const char *Pressure[] = {"value", "Kgf/cm2"};
static const char *Celsius[] = {"value", "&deg;C"};
static const char *Kelvin[] = {"value", "K"};
static const char *Hertz[] = {"value", "Hz"};
static const char *Counter[] = {"value", "Count"};
static const char *Hours[] = {"value", "Hours"};
static const char *Watt[] = {"value", "Watt"};
static const char *ErrorState[] = {"value", "Error"};
static const char *Ampere[] = {"value", "Ampere"};
static const char *Minutes[] = {"value", "Minutes"};
static const char *Duty[] = {"value", "Duty"};
static const char *HeatCoolModeDesc[] = {"Comp. Curve", "Direct"};
static const char *Percent[] = {"value", "&#37"};


/*****************************************************************************/
/* Die State-Topic-Tabelle - eine Zeile pro Topic                            */
/*                                                                           */
/*   TOP-Nr | Byte im Telegramm | Name | 1-Byte-Dekodierer |                 */
/*   Mehrbyte-Dekodierer | Einheit/Klartexte    (siehe struct StateTopic)    */
/* 'number' ist die TOP-Nummer als Datenfeld, nicht der Index: Zeilen        */
/* koennen entfallen, ohne dass sich die Nummern der uebrigen verschieben.   */
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
    { 29,  75, "Z1_Heat_Curve_Target_High_Temp",   getIntMinus128,       nullptr,                   Celsius},
    { 30,  76, "Z1_Heat_Curve_Target_Low_Temp",    getIntMinus128,       nullptr,                   Celsius},
    { 31,  78, "Z1_Heat_Curve_Outside_High_Temp",  getIntMinus128,       nullptr,                   Celsius},
    { 32,  77, "Z1_Heat_Curve_Outside_Low_Temp",   getIntMinus128,       nullptr,                   Celsius},
    { 33, 156, "Room_Thermostat_Temp",             getIntMinus128,       nullptr,                   Celsius},
    { 34,  40, "Z2_Heat_Request_Temp",             getIntMinus128,       nullptr,                   Celsius},
    { 35,  41, "Z2_Cool_Request_Temp",             getIntMinus128,       nullptr,                   Celsius},
    { 36, 145, "Z1_Water_Temp",                    getIntMinus128,       nullptr,                   Celsius},
    { 37, 146, "Z2_Water_Temp",                    getIntMinus128,       nullptr,                   Celsius},
    { 38, 196, "Cool_Energy_Production",           getIntMinus1Times200, nullptr,                   Watt},
    { 39, 195, "Cool_Energy_Consumption",          getIntMinus1Times200, nullptr,                   Watt},
    { 40, 198, "DHW_Energy_Production",            getIntMinus1Times200, nullptr,                   Watt},
    { 41, 197, "DHW_Energy_Consumption",           getIntMinus1Times200, nullptr,                   Watt},
    { 42, 147, "Z1_Water_Target_Temp",             getIntMinus128,       nullptr,                   Celsius},
    { 43, 148, "Z2_Water_Target_Temp",             getIntMinus128,       nullptr,                   Celsius},
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
    { 57, 140, "Z2_Temp",                          getIntMinus128,       nullptr,                   Celsius},
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
    { 82,  79, "Z2_Heat_Curve_Target_High_Temp",   getIntMinus128,       nullptr,                   Celsius},
    { 83,  80, "Z2_Heat_Curve_Target_Low_Temp",    getIntMinus128,       nullptr,                   Celsius},
    { 84,  82, "Z2_Heat_Curve_Outside_High_Temp",  getIntMinus128,       nullptr,                   Celsius},
    { 85,  81, "Z2_Heat_Curve_Outside_Low_Temp",   getIntMinus128,       nullptr,                   Celsius},
    { 86,  90, "Z2_Cool_Curve_Target_High_Temp",   getIntMinus128,       nullptr,                   Celsius},
    { 87,  91, "Z2_Cool_Curve_Target_Low_Temp",    getIntMinus128,       nullptr,                   Celsius},
    { 88,  93, "Z2_Cool_Curve_Outside_High_Temp",  getIntMinus128,       nullptr,                   Celsius},
    { 89,  92, "Z2_Cool_Curve_Outside_Low_Temp",   getIntMinus128,       nullptr,                   Celsius},
    { 90,   0, "Room_Heater_Operations_Hours",     nullptr,              getRoomHeaterHour,         Hours},
    { 91,   0, "DHW_Heater_Operations_Hours",      nullptr,              getDHWHeaterHour,          Hours},
    { 92, 172, "Pump_Duty",                        getIntMinus1,         nullptr,                   Duty},
    { 93,  72, "SGReady_Capacity1_Heat",           getIntMinus1,         nullptr,                   Percent},
    { 94,  71, "SGReady_Capacity1_DHW",            getIntMinus1,         nullptr,                   Percent},
    { 95,  74, "SGReady_Capacity2_Heat",           getIntMinus1,         nullptr,                   Percent},
    { 96,  73, "SGReady_Capacity2_DHW",            getIntMinus1,         nullptr,                   Percent},
    { 97,  98, "DHW_Heatup_Time",                  getIntMinus1,         nullptr,                   Minutes},
    { 98,  97, "DHW_Room_Max_Time",                getIntMinus1Times30,  nullptr,                   Minutes},
};

// Haelt NUMBEROFTOPICS (Array-Groesse von actual_data) und die Tabelle zusammen
static_assert(sizeof(stateTopics) / sizeof(stateTopics[0]) == NUMBEROFTOPICS,
              "NUMBEROFTOPICS passt nicht zur Zeilenzahl von stateTopics[]");


unsigned long nextalldatatime = 0;

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

  char top_value[MAXVALUELEN]; // stack buffer, decoders are String-free
  // index laeuft ueber die Tabellenzeilen, die TOP-Nummer kommt aus der Zeile
  for (unsigned int index = 0; index < NUMBEROFTOPICS; index++)
  {
    const StateTopic &topic = stateTopics[index];
    getTopicPayload(index, serial_data, top_value);
    bool changed = (strcmp(actual_data[index], top_value) != 0);

    if (updatealltopics || changed)
    {
      if (changed) // write only changed topics to mqtt log
      {
        (void)snprintf(pub_msg, sizeof(pub_msg), "<PUB> TOP%u %s: %s", topic.number, topic.name, top_value);
        write_mqtt_log(pub_msg);
      }
      strlcpy(actual_data[index], top_value, MAXVALUELEN);
      (void)snprintf(mqtt_topic, sizeof(mqtt_topic), "%s/%s", Topics::STATE.c_str(), topic.name);
      (void)mqtt_client.publish(mqtt_topic, top_value, MQTT_RETAIN_VALUES);
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

void getLeft5bits(byte input, char *out)
{
  (void)snprintf(out, MAXVALUELEN, "%d", (input >> 3) - 1);
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
