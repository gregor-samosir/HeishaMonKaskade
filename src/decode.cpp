#include "HeishaMon.h"
#include "decode.h"
// #include "commands.h"

/* lookup tables, declared extern in decode.h */
const char *topicNames[NUMBEROFTOPICS] = {
    States::TOP0,
    States::TOP1,
    States::TOP2,
    States::TOP3,
    States::TOP4,
    States::TOP5,
    States::TOP6,
    States::TOP7,
    States::TOP8,
    States::TOP9,
    States::TOP10,
    States::TOP11,
    States::TOP12,
    States::TOP13,
    States::TOP14,
    States::TOP15,
    States::TOP16,
    States::TOP17,
    States::TOP18,
    States::TOP19,
    States::TOP20,
    States::TOP21,
    States::TOP22,
    States::TOP23,
    States::TOP24,
    States::TOP25,
    States::TOP26,
    States::TOP27,
    States::TOP28,
    States::TOP29,
    States::TOP30,
    States::TOP31,
    States::TOP32,
    States::TOP33,
    States::TOP34,
    States::TOP35,
    States::TOP36,
    States::TOP37,
    States::TOP38,
    States::TOP39,
    States::TOP40,
    States::TOP41,
    States::TOP42,
    States::TOP43,
    States::TOP44,
    States::TOP45,
    States::TOP46,
    States::TOP47,
    States::TOP48,
    States::TOP49,
    States::TOP50,
    States::TOP51,
    States::TOP52,
    States::TOP53,
    States::TOP54,
    States::TOP55,
    States::TOP56,
    States::TOP57,
    States::TOP58,
    States::TOP59,
    States::TOP60,
    States::TOP61,
    States::TOP62,
    States::TOP63,
    States::TOP64,
    States::TOP65,
    States::TOP66,
    States::TOP67,
    States::TOP68,
    States::TOP69,
    States::TOP70,
    States::TOP71,
    States::TOP72,
    States::TOP73,
    States::TOP74,
    States::TOP75,
    States::TOP76,
    States::TOP77,
    States::TOP78,
    States::TOP79,
    States::TOP80,
    States::TOP81,
    States::TOP82,
    States::TOP83,
    States::TOP84,
    States::TOP85,
    States::TOP86,
    States::TOP87,
    States::TOP88,
    States::TOP89,
    States::TOP90,
    States::TOP91,
    States::TOP92,
    States::TOP93,
    States::TOP94,
    States::TOP95,
    States::TOP96,
    States::TOP97,
    States::TOP98,
};

const byte topicBytes[NUMBEROFTOPICS] = {
    // can store the index as byte (8-bit unsigned humber) as there aren't more then 255 bytes (actually only 203 bytes) to decode
    4,   // TOP0
    0,   // TOP1
    4,   // TOP2
    7,   // TOP3
    6,   // TOP4
    143, // TOP5
    144, // TOP6
    153, // TOP7
    166, // TOP8
    42,  // TOP9
    141, // TOP10
    0,   // TOP11
    0,   // TOP12
    5,   // TOP13
    142, // TOP14
    194, // TOP15
    193, // TOP16
    7,   // TOP17
    7,   // TOP18
    5,   // TOP19
    111, // TOP20
    158, // TOP21
    99,  // TOP22
    84,  // TOP23
    94,  // TOP24
    44,  // TOP25
    111, // TOP26
    38,  // TOP27
    39,  // TOP28
    75,  // TOP29
    76,  // TOP30
    78,  // TOP31
    77,  // TOP32
    156, // TOP33
    40,  // TOP34
    41,  // TOP35
    145, // TOP36
    146, // TOP37
    196, // TOP38
    195, // TOP39
    198, // TOP40
    197, // TOP41
    147, // TOP42
    148, // TOP43
    0,   // TOP44
    43,  // TOP45
    149, // TOP46
    150, // TOP47
    151, // TOP48
    154, // TOP49
    155, // TOP50
    157, // TOP51
    159, // TOP52
    160, // TOP53
    161, // TOP54
    162, // TOP55
    139, // TOP56
    140, // TOP57
    9,   // TOP58
    9,   // TOP59
    112, // TOP60
    112, // TOP61
    173, // TOP62
    174, // TOP63
    163, // TOP64
    171, // TOP65
    164, // TOP66
    165, // TOP67
    5,   // TOP68
    117, // TOP69
    100, // TOP70
    101, // TOP71
    86,  // TOP72
    87,  // TOP73
    89,  // TOP74
    88,  // TOP75
    28,  // TOP76
    83,  // TOP77
    85,  // TOP78
    95,  // TOP79
    96,  // TOP80
    28,  // TOP81
    79,  // TOP82
    80,  // TOP83
    82,  // TOP84
    81,  // TOP85
    90,  // TOP86
    91,  // TOP87
    93,  // TOP88
    92,  // TOP89
    0,   // TOP90
    0,   // TOP91
    172, // TOP92
    72,  // TOP93
    71,  // TOP94
    74,  // TOP95
    73,  // TOP96
    98,  // TOP97
    97,  // TOP98
};


const topicFP topicFunctions[NUMBEROFTOPICS] = {
    getBit7and8,          // TOP0
    unknown,              // TOP1
    getBit1and2,          // TOP2
    getBit1and2,          // TOP3
    getOpMode,            // TOP4
    getIntMinus128,       // TOP5
    getIntMinus128,       // TOP6
    getIntMinus128,       // TOP7
    getIntMinus1,         // TOP8
    getIntMinus128,       // TOP9
    getIntMinus128,       // TOP10
    unknown,              // TOP11
    unknown,              // TOP12
    getBit1and2,          // TOP13
    getIntMinus128,       // TOP14
    getIntMinus1Times200, // TOP15
    getIntMinus1Times200, // TOP16
    getRight3bits,        // TOP17
    getBit3and4and5,      // TOP18
    getBit3and4,          // TOP19
    getBit7and8,          // TOP20
    getIntMinus128,       // TOP21
    getIntMinus128,       // TOP22
    getIntMinus128,       // TOP23
    getIntMinus128,       // TOP24
    getIntMinus128,       // TOP25
    getBit5and6,          // TOP26
    getIntMinus128,       // TOP27
    getIntMinus128,       // TOP28
    getIntMinus128,       // TOP29
    getIntMinus128,       // TOP30
    getIntMinus128,       // TOP31
    getIntMinus128,       // TOP32
    getIntMinus128,       // TOP33
    getIntMinus128,       // TOP34
    getIntMinus128,       // TOP35
    getIntMinus128,       // TOP36
    getIntMinus128,       // TOP37
    getIntMinus1Times200, // TOP38
    getIntMinus1Times200, // TOP39
    getIntMinus1Times200, // TOP40
    getIntMinus1Times200, // TOP41
    getIntMinus128,       // TOP42
    getIntMinus128,       // TOP43
    unknown,              // TOP44
    getIntMinus128,       // TOP45
    getIntMinus128,       // TOP46
    getIntMinus128,       // TOP47
    getIntMinus128,       // TOP48
    getIntMinus128,       // TOP49
    getIntMinus128,       // TOP50
    getIntMinus128,       // TOP51
    getIntMinus128,       // TOP52
    getIntMinus128,       // TOP53
    getIntMinus128,       // TOP54
    getIntMinus128,       // TOP55
    getIntMinus128,       // TOP56
    getIntMinus128,       // TOP57
    getBit5and6,          // TOP58
    getBit7and8,          // TOP59
    getBit7and8,          // TOP60
    getBit5and6,          // TOP61
    getIntMinus1Times10,  // TOP62
    getIntMinus1Times10,  // TOP63
    getIntMinus1Div5,     // TOP64
    getIntMinus1Times50,  // TOP65
    getIntMinus1,         // TOP66
    getIntMinus1Div5,     // TOP67
    getBit5and6,          // TOP68
    getBit5and6,          // TOP69
    getIntMinus128,       // TOP70
    getIntMinus1,         // TOP71
    getIntMinus128,       // TOP72
    getIntMinus128,       // TOP73
    getIntMinus128,       // TOP74
    getIntMinus128,       // TOP75
    getBit7and8,          // TOP76
    getIntMinus128,       // TOP77
    getIntMinus128,       // TOP78
    getIntMinus128,       // TOP79
    getIntMinus128,       // TOP80
    getBit5and6,          // TOP81
    getIntMinus128,       // TOP82
    getIntMinus128,       // TOP83
    getIntMinus128,       // TOP84
    getIntMinus128,       // TOP85
    getIntMinus128,       // TOP86
    getIntMinus128,       // TOP87
    getIntMinus128,       // TOP88
    getIntMinus128,       // TOP89
    unknown,              // TOP90
    unknown,              // TOP91
    getIntMinus1,         // TOP92
    getIntMinus1,         // TOP93
    getIntMinus1,         // TOP94
    getIntMinus1,         // TOP95
    getIntMinus1,         // TOP96
    getIntMinus1,         // TOP97
    getIntMinus1Times30,  // TOP98
};

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

const char **topicDescription[NUMBEROFTOPICS] = {
    OffOn,            // TOP0
    LitersPerMin,     // TOP1
    DisabledEnabled,  // TOP2
    DisabledEnabled,  // TOP3
    OpModeDesc,       // TOP4
    Celsius,          // TOP5
    Celsius,          // TOP6
    Celsius,          // TOP7
    Hertz,            // TOP8
    Celsius,          // TOP9
    Celsius,          // TOP10
    Hours,            // TOP11
    Counter,          // TOP12
    DisabledEnabled,  // TOP13
    Celsius,          // TOP14
    Watt,             // TOP15
    Watt,             // TOP16
    Powerfulmode,     // TOP17
    Quietmode,        // TOP18
    HolidayState,     // TOP19
    Valve,            // TOP20
    Celsius,          // TOP21
    Kelvin,           // TOP22
    Kelvin,           // TOP23
    Kelvin,           // TOP24
    Kelvin,           // TOP25
    DisabledEnabled,  // TOP26
    Celsius,          // TOP27
    Celsius,          // TOP28
    Celsius,          // TOP29
    Celsius,          // TOP30
    Celsius,          // TOP31
    Celsius,          // TOP32
    Celsius,          // TOP33
    Celsius,          // TOP34
    Celsius,          // TOP35
    Celsius,          // TOP36
    Celsius,          // TOP37
    Watt,             // TOP38
    Watt,             // TOP39
    Watt,             // TOP40
    Watt,             // TOP41
    Celsius,          // TOP42
    Celsius,          // TOP43
    ErrorState,       // TOP44
    Kelvin,           // TOP45
    Celsius,          // TOP46
    Celsius,          // TOP47
    Celsius,          // TOP48
    Celsius,          // TOP49
    Celsius,          // TOP50
    Celsius,          // TOP51
    Celsius,          // TOP52
    Celsius,          // TOP53
    Celsius,          // TOP54
    Celsius,          // TOP55
    Celsius,          // TOP56
    Celsius,          // TOP57
    BlockedFree,      // TOP58
    BlockedFree,      // TOP59
    InactiveActive,   // TOP60
    InactiveActive,   // TOP61
    RotationsPerMin,  // TOP62
    RotationsPerMin,  // TOP63
    Pressure,         // TOP64
    RotationsPerMin,  // TOP65
    Pressure,         // TOP66
    Ampere,           // TOP67
    InactiveActive,   // TOP68
    InactiveActive,   // TOP69
    Celsius,          // TOP70
    Minutes,          // TOP71
    Celsius,          // TOP72
    Celsius,          // TOP73
    Celsius,          // TOP74
    Celsius,          // TOP75
    HeatCoolModeDesc, // TOP76
    Celsius,          // TOP77
    Celsius,          // TOP78
    Celsius,          // TOP79
    Celsius,          // TOP80
    HeatCoolModeDesc, // TOP81
    Celsius,          // TOP82
    Celsius,          // TOP83
    Celsius,          // TOP84
    Celsius,          // TOP85
    Celsius,          // TOP86
    Celsius,          // TOP87
    Celsius,          // TOP88
    Celsius,          // TOP89
    Hours,            // TOP90
    Hours,            // TOP91
    Duty,             // TOP92
    Percent,          // TOP93
    Percent,          // TOP94
    Percent,          // TOP95
    Percent,          // TOP96
    Minutes,          // TOP97
    Minutes,          // TOP98
};

unsigned long nextalldatatime = 0;

void publish_heatpump_data(char *serial_data, char actual_data[][MAXVALUELEN], PubSubClient &mqtt_client)
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
  for (unsigned int top_num = 0; top_num < NUMBEROFTOPICS; top_num++)
  {
    getTopicPayload(top_num, serial_data, top_value);
    bool changed = (strcmp(actual_data[top_num], top_value) != 0);

    if (updatealltopics || changed)
    {
      if (changed) // write only changed topics to mqtt log
      {
        sprintf(pub_msg, "<PUB> TOP%d %s: %s", top_num, topicNames[top_num], top_value);
        write_mqtt_log(pub_msg);
      }
      strlcpy(actual_data[top_num], top_value, MAXVALUELEN);
      (void)snprintf(mqtt_topic, sizeof(mqtt_topic), "%s/%s", Topics::STATE.c_str(), topicNames[top_num]);
      (void)mqtt_client.publish(mqtt_topic, top_value, MQTT_RETAIN_VALUES);
    }
  }
}

/*****************************************************************************/
/* calculate the payload                                                     */
/* out must hold at least MAXVALUELEN bytes                                  */
/*****************************************************************************/
void getTopicPayload(unsigned int top_num, char *serial_data, char *out)
{
  switch (top_num)
  {
  case 1: // Pump_Flow
    getPumpFlow(serial_data, out);
    break;
  case 5: // InletTemp with fraction
    getInletTempWithFraction(serial_data, out);
    break;
  case 6: // OutletTemp with fraction
    getOutletTempWithFraction(serial_data, out);
    break;
  case 11: // Operations_Hours
    getOperationHour(serial_data, out);
    break;
  case 12: // Operations_Counter
    getOperationCount(serial_data, out);
    break;
  case 90: // Room_Heater_Operations_Hours
    getRoomHeaterHour(serial_data, out);
    break;
  case 91: // DHW_Heater_Operations_Hours
    getDHWHeaterHour(serial_data, out);
    break;
  case 44: // Error and decription
    getErrorInfo(serial_data, out);
    break;
  default:
  {
    // call the topic function for 1 byte topics
    byte serial_value = serial_data[topicBytes[top_num]];
    topicFunctions[top_num](serial_value, out);
    break;
  }
  }
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

void unknown(byte input, char *out)
{
  (void)strlcpy(out, "-1", MAXVALUELEN);
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
/*****************************************************************************/
static const char *fractionText[] = {".00", ".25", ".50", ".75"};

void getInletTempWithFraction(char *serial_data, char *out)
{
  int fractional = (int)(serial_data[118] & 0b111);
  byte serial_value = serial_data[topicBytes[5]];
  topicFunctions[5](serial_value, out); // TOP5 integer part
  if (fractional >= 1 && fractional <= 4)
  {
    (void)strlcat(out, fractionText[fractional - 1], MAXVALUELEN);
  }
}

void getOutletTempWithFraction(char *serial_data, char *out)
{
  int fractional = (int)((serial_data[118] >> 3) & 0b111);
  byte serial_value = serial_data[topicBytes[6]];
  topicFunctions[6](serial_value, out); // TOP6 integer part
  if (fractional >= 1 && fractional <= 4)
  {
    (void)strlcat(out, fractionText[fractional - 1], MAXVALUELEN);
  }
}

/*****************************************************************************/
/* multi-byte decoders                                                       */
/*****************************************************************************/
void getPumpFlow(char *serial_data, char *out)
{ // TOP1 //
  float PumpFlow1 = (float)serial_data[170];
  float PumpFlow2 = (((float)serial_data[169] - 1) / 256);
  // dtostrf instead of snprintf %f: float formatting like String(value, 2)
  (void)dtostrf(PumpFlow1 + PumpFlow2, 1, 2, out);
}

void getOperationHour(char *serial_data, char *out)
{
  (void)snprintf(out, MAXVALUELEN, "%d", (int)word(serial_data[183], serial_data[182]) - 1);
}

void getOperationCount(char *serial_data, char *out)
{
  (void)snprintf(out, MAXVALUELEN, "%d", (int)word(serial_data[180], serial_data[179]) - 1);
}

void getRoomHeaterHour(char *serial_data, char *out)
{
  (void)snprintf(out, MAXVALUELEN, "%d", (int)word(serial_data[186], serial_data[185]) - 1);
}

void getDHWHeaterHour(char *serial_data, char *out)
{
  (void)snprintf(out, MAXVALUELEN, "%d", (int)word(serial_data[189], serial_data[188]) - 1);
}

void getErrorInfo(char *serial_data, char *out)
{ // TOP44 //
  int Error_type = (int)(unsigned char)(serial_data[113]);
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
