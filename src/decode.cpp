#include "HeishaMon.h"
#include "decode.h"
#include "commands.h"

unsigned long nextalldatatime = 0;

void decode_heatpump_data(char *serial_data, String actual_data[], PubSubClient &mqtt_client)
{
  char log_msg[255];
  std::string mqtt_topic;
  byte input_pos;
  String top_value;

  bool updatealltopics = false;
  if (millis() > nextalldatatime)
  {
    updatealltopics = true;
    nextalldatatime = millis() + UPDATEALLTIME;
  }

  for (unsigned int top_num = 0; top_num < NUMBEROFTOPICS; top_num++)
  {
    //switch on topic numbers with 2 bytes
    switch (top_num)
    {
    case 1: //Pump_Flow
      top_value = getPumpFlow(serial_data);
      break;
    case 11: //Operations_Hours
      top_value = getOperationHour(serial_data);
      break;
    case 12: //Operations_Counter
      top_value = getOperationCount(serial_data);
      break;
    case 90: //Room_Heater_Operations_Hours
      top_value = getRoomHeaterHour(serial_data);
      break;
    case 91: //DHW_Heater_Operations_Hours
      top_value = getDHWHeaterHour(serial_data);
      break;
    case 44: //Error and decription
      top_value = getErrorInfo(serial_data);
      break;
    case 34: // unused Topics
    case 35: // add your unused topics to the list
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
      top_value = "unused";
      break;
    default:
      //call the topic function for 1 byte topics
      input_pos = serial_data[topicBytes[top_num]];
      top_value = topicFunctions[top_num](input_pos);
      break;
    }

    if ((updatealltopics) || (actual_data[top_num] != top_value))
    {
      if (actual_data[top_num] != top_value) //write only changed topics to mqtt log
      {
        sprintf(log_msg, "<REC> TOP%d %s: %s", top_num, topics[top_num], top_value.c_str());
        write_mqtt_log(log_msg);
      }
      actual_data[top_num] = top_value;
      mqtt_topic = Topics::STATE + "/" + topics[top_num];
      mqtt_client.publish(mqtt_topic.c_str(), top_value.c_str(), MQTT_RETAIN_VALUES);
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
String getPumpFlow(char *serial_data)
{ // TOP1 //
  float PumpFlow1 = (float)serial_data[170];
  float PumpFlow2 = (((float)serial_data[169] - 1) / 256);
  float PumpFlow = PumpFlow1 + PumpFlow2;
  return String(PumpFlow, 2);
}

String getOperationHour(char *serial_data)
{
  return String(word(serial_data[183], serial_data[182]) - 1);
}

String getOperationCount(char *serial_data)
{
  return String(word(serial_data[180], serial_data[179]) - 1);
}

String getRoomHeaterHour(char *serial_data)
{
  return String(word(serial_data[186], serial_data[185]) - 1);
}

String getDHWHeaterHour(char *serial_data)
{
  return String(word(serial_data[189], serial_data[188]) - 1);
}

String getErrorInfo(char *serial_data)
{ // TOP44 //
  int Error_type = (int)(serial_data[113]);
  int Error_number = ((int)(serial_data[114])) - 17;
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
