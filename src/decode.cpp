#include "decode.h"
#include "commands.h"

unsigned long nextalldatatime = 0;

void decode_heatpump_data(char *data, String actData[], PubSubClient &mqtt_client, void write_mqtt_log(char *))
{
  char log_msg[256];
  std::string mqtt_topic;
  bool updatealltopics = false;
  byte Input_Byte;
  String Topic_Value;

  if (millis() > nextalldatatime)
  {
    updatealltopics = true;
    nextalldatatime = millis() + UPDATEALLTIME;
  }

  for (unsigned int Topic_Number = 0; Topic_Number < NUMBER_OF_TOPICS; Topic_Number++)
  {
    //switch on topic numbers with 2 bytes
    switch (Topic_Number)
    { 
    case 1: //Pump_Flow
      Topic_Value = getPumpFlow(data);
      break;
    case 11: //Operations_Hours
      Topic_Value = getOperationHour(data);
      break;
    case 12: //Operations_Counter
      Topic_Value = getOperationCount(data);
      break;
    case 90: //Room_Heater_Operations_Hours
      Topic_Value = getRoomHeaterHour(data);
      break;
    case 91: //DHW_Heater_Operations_Hours
      Topic_Value = getDHWHeaterHour(data);
      break;
    case 44: //Error
      Topic_Value = getErrorInfo(data);
      break;
    case 34: // unused Topics
    case 35:
    case 37:
    case 43:
    case 46:
    case 47:
    case 48:
    case 57:
    case 63:
    case 82:
    case 83:
    case 84:
    case 85:
    case 86:
    case 87:
    case 88:
    case 89:
      Topic_Value = "unused";
      break;
    default:
      //call the topic function for 1 byte topics
      Input_Byte = data[topicBytes[Topic_Number]];
      Topic_Value = topicFunctions[Topic_Number](Input_Byte);
      break;
    }

    if ((updatealltopics) || (actData[Topic_Number] != Topic_Value))
    {
      if (actData[Topic_Number] != Topic_Value) //write only changed topics to mqtt log
      {
        sprintf(log_msg, "Receive TOP%d \t %s: %s", Topic_Number, topics[Topic_Number], Topic_Value.c_str()); write_mqtt_log(log_msg);
      }
      actData[Topic_Number] = Topic_Value;
      mqtt_topic = Topics::STATE + "/" + topics[Topic_Number];
      mqtt_client.publish(mqtt_topic.c_str(), Topic_Value.c_str(), MQTT_RETAIN_VALUES);
    }
  }
}

String getBit1and2(byte input)
{
  return String((input >> 6) - 1);
}

String getBit3and4(byte input)
{
  return String(((input >> 4) & 0b11) - 1);
}

String getBit5and6(byte input)
{
  return String(((input >> 2) & 0b11) - 1);
}

String getBit7and8(byte input)
{
  return String((input & 0b11) - 1);
}

String getBit3and4and5(byte input)
{
  return String(((input >> 3) & 0b111) - 1);
}

String getLeft5bits(byte input)
{
  return String((input >> 3) - 1);
}

String getRight3bits(byte input)
{
  return String((input & 0b111) - 1);
}

String getIntMinus1(byte input)
{
  return String((int)input - 1);
}

String getIntMinus128(byte input)
{
  return String((int)input - 128);
}

String getIntMinus1Div5(byte input)
{
  return String((((float)input - 1) / 5), 1);
}

String getIntMinus1Times10(byte input)
{
  return String(((int)input - 1) * 10);
}

String getIntMinus1Times50(byte input)
{
  return String(((int)input - 1) * 50);
}

String getIntMinus1Times200(byte input)
{
  return String(((int)input - 1) * 200);
}

String unknown(byte input)
{
  return "-1";
}

String getOpMode(byte input)
{
  switch ((int)input)
  {
  case 82:
    return "0";
  case 83:
    return "1";
  case 89:
    return "2";
  case 97:
    return "3";
  case 98:
    return "4";
  case 99:
    return "5";
  case 105:
    return "6";
  case 90:
    return "7";
  case 106:
    return "8";
  default:
    return "-1";
  }
}

/* Two bytes per TOP */
String getPumpFlow(char *data)
{ // TOP1 //
  float PumpFlow1 = (float)data[170];
  float PumpFlow2 = (((float)data[169] - 1) / 256);
  float PumpFlow = PumpFlow1 + PumpFlow2;
  return String(PumpFlow, 2);
}

String getOperationHour(char *data)
{
  return String(word(data[183], data[182]) - 1);
}

String getOperationCount(char *data)
{
  return String(word(data[180], data[179]) - 1);
}

String getRoomHeaterHour(char *data)
{
  return String(word(data[186], data[185]) - 1);
}

String getDHWHeaterHour(char *data)
{
  return String(word(data[189], data[188]) - 1);
}

String getErrorInfo(char *data)
{ // TOP44 //
  int Error_type = (int)(data[113]);
  int Error_number = ((int)(data[114])) - 17;
  char Error_string[10];
  switch (Error_type)
  {
  case 177: //B1=F type error
    sprintf(Error_string, "F%02X", Error_number);
    break;
  case 161: //A1=H type error
    sprintf(Error_string, "H%02X", Error_number);
    break;
  default:
    sprintf(Error_string, "No error");
    break;
  }
  return String(Error_string);
}
