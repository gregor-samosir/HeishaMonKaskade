#include "commands.h"
#include "Topics.h"
#include <string>

byte mainQuery[]   = {0x71, 0x6c, 0x01, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
// byte pcbSET[] = {0xF1, 0x11, 0x01, 0x50, 0x00, 0x00, 0x40, 0xFF, 0xFF, 0xE5, 0xFF, 0xFF, 0x00, 0xFF, 0xEB, 0xFF, 0xFF, 0x00, 0x00};

void build_heatpump_command(char *topic, char *msg, void push_command_buffer(byte *, int, char *))
{
  byte mainCommand[] = {0xF1, 0x6c, 0x01, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  char log_msg[255];
  byte set_byte;
  byte set_pos; // position in mainCommand

  String state_string(msg);

  // set heatpump state to on by sending 1
  if (Topics::SET1.compare(topic) == 0)
  {
    //pos 4
    set_pos = 4;
    set_byte = 1;
    if (state_string.toInt() == 1)
    {
      set_byte = 2;
    }
    sprintf(log_msg, "SET1 Heatpump: %d", set_byte);
    mainCommand[set_pos] = set_byte;
  }

  // set 0 for Off mode, set 1 for Quiet mode 1, set 2 for Quiet mode 2, set 3 for Quiet mode 3
  else if (Topics::SET3.compare(topic) == 0)
  {
    //pos 7
    set_pos = 7;
    set_byte = (state_string.toInt() + 1) * 8;
    sprintf(log_msg, "SET3 QuietMode: %d", set_byte / 8 - 1);
    mainCommand[set_pos] = set_byte;
  }

  // z1 heat request temp -  set from -5 to 5 to get same temperature shift point or set direct temp
  else if (Topics::SET5.compare(topic) == 0)
  {
    //pos = 38
    set_pos = 38;
    set_byte = state_string.toInt() + 128;
    sprintf(log_msg, "SET5 Z1HeatRequestTemperature: %d", set_byte - 128);
    mainCommand[set_pos] = set_byte;
  }

  // z1 cool request temp -  set from -5 to 5 to get same temperature shift point or set direct temp
  else if (Topics::SET6.compare(topic) == 0)
  {
    //pos 39
    set_pos = 39;
    set_byte = state_string.toInt() + 128;
    sprintf(log_msg, "SET6 Z1CoolRequestTemperature: %d", set_byte - 128);
    mainCommand[set_pos] = set_byte;
  }

  // z2 heat request temp -  set from -5 to 5 to get same temperature shift point or set direct temp
  else if (Topics::SET7.compare(topic) == 0)
  {
    //pos 40
    set_pos = 40;
    set_byte = state_string.toInt() + 128;
    sprintf(log_msg, "SET7 Z2HeatRequestTemperature: %d", set_byte - 128);
    mainCommand[set_pos] = set_byte;
  }

  // z2 cool request temp -  set from -5 to 5 to get same temperature shift point or set direct temp
  else if (Topics::SET8.compare(topic) == 0)
  {
    //pos 41
    set_pos = 41;
    set_byte = state_string.toInt() + 128;
    sprintf(log_msg, "SET8 Z2CoolRequestTemperature: %d", set_byte - 128);
    mainCommand[set_pos] = set_byte;
  }

  // set mode to force DHW by sending 1
  else if (Topics::SET10.compare(topic) == 0)
  {
    //pos 4
    set_pos = 4;
    set_byte = 64; //hex 0x40
    if (state_string.toInt() == 1)
    {
      set_byte = 128; //hex 0x80
    }
    sprintf(log_msg, "SET10 ForceDHW: %d", set_byte);
    mainCommand[set_pos] = set_byte;
  }

  // set mode to force defrost  by sending 1
  else if (Topics::SET12.compare(topic) == 0)
  {
    //pos 8
    set_pos = 8;
    set_byte = 0;
    if (state_string.toInt() == 1)
    {
      set_byte = 2; //hex 0x02
    }
    sprintf(log_msg, "SET12 ForceDefrost: %d", set_byte);
    mainCommand[set_pos] = set_byte;
  }

  // set mode to force sterilization by sending 1
  else if (Topics::SET13.compare(topic) == 0)
  {
    //pos 8
    set_pos = 8;
    set_byte = 0;
    if (state_string.toInt() == 1)
    {
      set_byte = 4; //hex 0x04
    }
    sprintf(log_msg, "SET13 ForceSterilization: %d", set_byte);
    mainCommand[set_pos] = set_byte;
  }

  // set Holiday mode by sending 1, off will be 0
  else if (Topics::SET2.compare(topic) == 0)
  {
    //pos 5
    set_pos = 5;
    set_byte = 16; //hex 0x10
    if (state_string.toInt() == 1)
    {
      set_byte = 32; //hex 0x20
    }
    sprintf(log_msg, "SET2 HolidayMode: %d", set_byte);
    mainCommand[set_pos] = set_byte;
  }

  // set Powerful mode by sending 0 = off, 1 for 30min, 2 for 60min, 3 for 90 min
  else if (Topics::SET4.compare(topic) == 0)
  {
    //pos 7
    set_pos = 7;
    set_byte = (state_string.toInt()) + 73;
    sprintf(log_msg, "SET4 PowerfulMode: %d", (set_byte - 73));
    mainCommand[set_pos] = set_byte;
  }

  // set Heat pump operation mode 0 = heat only, 1 = cool only, 2 = Auto(Heat), 3 = DHW only, 4 = Heat+DHW, 5 = Cool+DHW, 6 = Auto(Heat) + DHW, 7 = Auto(Cool), 8 = Auto(Cool) + DHW
  else if (Topics::SET9.compare(topic) == 0)
  {
    //pos 6
    set_pos = 6;
    switch (state_string.toInt())
    {
    case 0: // Heat
      set_byte = 82;
      break;
    case 1: // Cool
      set_byte = 83;
      break;
    case 2: // Auto(Heat)
      set_byte = 89;
      break;
    case 3: // DHW
      set_byte = 33;
      break;
    case 4: // Heat + DHW
      set_byte = 98;
      break;
    case 5: // Cool + DHW
      set_byte = 99;
      break;
    case 6: // Auto(Heat) + DHW
      set_byte = 104;
      break;
    case 7: // Auto(Cool) 
      set_byte = 90;
      break;
    case 8: // Auto(Cool) + DHW
      set_byte = 106;
      break;
    default:
      set_byte = 0;
      break;
    }
    sprintf(log_msg, "SET9 OperationMode: %d", set_byte);
    mainCommand[set_pos] = set_byte;
  }

  // set DHW temperature by sending desired temperature between 40C-75C
  else if (Topics::SET11.compare(topic) == 0)
  {
    //pos 42
    set_pos = 42;
    set_byte = state_string.toInt() + 128;
    sprintf(log_msg, "SET11 DHWTemp: %d", set_byte - 128);
    mainCommand[set_pos] = set_byte;
  }

  // set water pump state to on=1  or off=0 
  else if (Topics::SET14.compare(topic) == 0)
  {
    //pos 4
    set_pos = 4;
    set_byte = 16; //hex 0x10
    if (state_string.toInt() == 1)
    {
      set_byte = 32; //hex 0x20
    }
    sprintf(log_msg, "SET14 WaterPump: %d", set_byte);
    mainCommand[set_pos] = set_byte;
  }

  // set PumpSpeedMax 65 - 255
  else if (Topics::SET15.compare(topic) == 0)
  {
    //pos 45
    set_pos = 45;
    set_byte = state_string.toInt() + 1;
    sprintf(log_msg, "SET15 WaterPumpSpeed: %d", set_byte);
    mainCommand[set_pos] = set_byte;
  }
  // set heat delta 1-15
  else if (Topics::SET16.compare(topic) == 0)
  {
    //pos 84
    set_pos = 84;
    set_byte = state_string.toInt() + 128;
    sprintf(log_msg, "SET16 HeatDelta: %d", set_byte - 128);
    mainCommand[set_pos] = set_byte;
  }
  // set cool delta 1-15
  else if (Topics::SET17.compare(topic) == 0)
  {
    //pos 94
    set_pos = 94;
    set_byte = state_string.toInt() + 128;
    sprintf(log_msg, "SET17 CoolDelta: %d", set_byte - 128);
    mainCommand[set_pos] = set_byte;
  }
  // set cool delta 1-15
  else if (Topics::SET18.compare(topic) == 0)
  {
    //pos 99
    set_pos = 99;
    set_byte = state_string.toInt() + 128;
    sprintf(log_msg, "SET18 DHWHeatDelta: %d", set_byte - 128);
    mainCommand[set_pos] = set_byte;
  }
  // set DHW heatup time (max) 5 -240
  else if (Topics::SET19.compare(topic) == 0)
  {
    //pos 98
    set_pos = 98;
    set_byte = state_string.toInt() + 1;
    sprintf(log_msg, "SET19 DHWHeatupTime: %d", set_byte - 1);
    mainCommand[set_pos] = set_byte;
  }
  push_command_buffer(mainCommand, sizeof(mainCommand), log_msg);
}
