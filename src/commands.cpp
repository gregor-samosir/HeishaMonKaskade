#include "HeishaMon.h"
#include "commands.h"
#include "Topics.h"
#include <string>

byte mainQuery[]   = {0x71, 0x6c, 0x01, 0x10};
byte mainCommand[] = {0xF1, 0x6c, 0x01, 0x10};

void build_heatpump_command(char *topic, char *msg)
{
  char log_msg[256];
  byte set_byte;
  byte set_pos; // position in mainCommand
  unsigned int msg_int = atoi(msg);

  // set heatpump state to on by sending 1
  if (Topics::SET1.compare(topic) == 0)
  {
    //pos 4
    set_pos = 4;
    set_byte = 1;
    if (msg_int == 1)
    {
      set_byte = 2;
    }
    sprintf(log_msg, "<SUB> SET1 %s: %d", topic, set_byte);
    mainCommand[set_pos] = set_byte;
  }

  // set 0 for Off mode, set 1 for Quiet mode 1, set 2 for Quiet mode 2, set 3 for Quiet mode 3
  else if (Topics::SET3.compare(topic) == 0)
  {
    //pos 7
    set_pos = 7;
    set_byte = (msg_int + 1) * 8;
    sprintf(log_msg, "<SUB> SET3 %s: %d", topic, set_byte / 8 - 1);
    mainCommand[set_pos] = set_byte;
  }

  // z1 heat request temp -  set from -5 to 5 to get same temperature shift point or set direct temp
  else if (Topics::SET5.compare(topic) == 0)
  {
    //pos = 38
    set_pos = 38;
    set_byte = msg_int + 128;
    sprintf(log_msg, "<SUB> SET5 %s: %d", topic, set_byte - 128);
    mainCommand[set_pos] = set_byte;
  }

  // z1 cool request temp -  set from -5 to 5 to get same temperature shift point or set direct temp
  else if (Topics::SET6.compare(topic) == 0)
  {
    //pos 39
    set_pos = 39;
    set_byte = msg_int + 128;
    sprintf(log_msg, "<SUB> SET6 %s: %d", topic, set_byte - 128);
    mainCommand[set_pos] = set_byte;
  }

  // z2 heat request temp -  set from -5 to 5 to get same temperature shift point or set direct temp
  else if (Topics::SET7.compare(topic) == 0)
  {
    //pos 40
    set_pos = 40;
    set_byte = msg_int + 128;
    sprintf(log_msg, "<SUB> SET7 %s: %d", topic, set_byte - 128);
    mainCommand[set_pos] = set_byte;
  }

  // z2 cool request temp -  set from -5 to 5 to get same temperature shift point or set direct temp
  else if (Topics::SET8.compare(topic) == 0)
  {
    //pos 41
    set_pos = 41;
    set_byte = msg_int + 128;
    sprintf(log_msg, "<SUB> SET8 %s: %d", topic, set_byte - 128);
    mainCommand[set_pos] = set_byte;
  }

  // set mode to force DHW by sending 1
  else if (Topics::SET10.compare(topic) == 0)
  {
    //pos 4
    set_pos = 4;
    set_byte = 64; //hex 0x40
    if (msg_int == 1)
    {
      set_byte = 128; //hex 0x80
    }
    sprintf(log_msg, "<SUB> SET10 %s: %d", topic, set_byte);
    mainCommand[set_pos] = set_byte;
  }

  // set mode to force defrost  by sending 1
  else if (Topics::SET12.compare(topic) == 0)
  {
    //pos 8
    set_pos = 8;
    set_byte = 0;
    if (msg_int == 1)
    {
      set_byte = 2; //hex 0x02
    }
    sprintf(log_msg, "<SUB> SET12 %s: %d", topic, set_byte);
    mainCommand[set_pos] = set_byte;
  }

  // set mode to force sterilization by sending 1
  else if (Topics::SET13.compare(topic) == 0)
  {
    //pos 8
    set_pos = 8;
    set_byte = 0;
    if (msg_int == 1)
    {
      set_byte = 4; //hex 0x04
    }
    sprintf(log_msg, "<SUB> SET13 %s: %d", topic, set_byte);
    mainCommand[set_pos] = set_byte;
  }

  // set Holiday mode by sending 1, off will be 0
  else if (Topics::SET2.compare(topic) == 0)
  {
    //pos 5
    set_pos = 5;
    set_byte = 16; //hex 0x10
    if (msg_int == 1)
    {
      set_byte = 32; //hex 0x20
    }
    sprintf(log_msg, "<SUB> SET2 %s: %d", topic, set_byte);
    mainCommand[set_pos] = set_byte;
  }

  // set Powerful mode by sending 0 = off, 1 for 30min, 2 for 60min, 3 for 90 min
  else if (Topics::SET4.compare(topic) == 0)
  {
    //pos 7
    set_pos = 7;
    set_byte = (msg_int) + 73;
    sprintf(log_msg, "<SUB> SET4 %s: %d", topic, (set_byte - 73));
    mainCommand[set_pos] = set_byte;
  }

  // set Heat pump operation mode 0 = heat only, 1 = cool only, 2 = Auto(Heat), 3 = DHW only, 4 = Heat+DHW, 5 = Cool+DHW, 6 = Auto(Heat) + DHW, 7 = Auto(Cool), 8 = Auto(Cool) + DHW
  else if (Topics::SET9.compare(topic) == 0)
  {
    //pos 6
    set_pos = 6;
    switch (msg_int)
    {
    case 0: // Heat
      set_byte = 18;
      break;
    case 1: // Cool
      set_byte = 19;
      break;
    case 2: // Auto(Heat)
      set_byte = 24;
      break;
    case 3: // DHW
      set_byte = 33;
      break;
    case 4: // Heat + DHW
      set_byte = 34;
      break;
    case 5: // Cool + DHW
      set_byte = 35;
      break;
    case 6: // Auto(Heat) + DHW
      set_byte = 40;
      break;
    default:
      set_byte = 0;
      break;
    }
    sprintf(log_msg, "<SUB> SET9 %s: %d", topic, set_byte);
    mainCommand[set_pos] = set_byte;
  }

  // set DHW temperature by sending desired temperature between 40C-75C
  else if (Topics::SET11.compare(topic) == 0)
  {
    //pos 42
    set_pos = 42;
    set_byte = msg_int + 128;
    sprintf(log_msg, "<SUB> SET11 %s: %d", topic, set_byte - 128);
    mainCommand[set_pos] = set_byte;
  }

  // set water pump state to on=1  or off=0 
  else if (Topics::SET14.compare(topic) == 0)
  {
    //pos 4
    set_pos = 4;
    set_byte = 16; //hex 0x10
    if (msg_int == 1)
    {
      set_byte = 32; //hex 0x20
    }
    sprintf(log_msg, "<SUB> SET14 %s: %d", topic, set_byte);
    mainCommand[set_pos] = set_byte;
  }

  // set PumpSpeedMax 65 - 255
  else if (Topics::SET15.compare(topic) == 0)
  {
    //pos 45
    set_pos = 45;
    set_byte = msg_int + 1;
    sprintf(log_msg, "<SUB> SET15 %s: %d", topic, set_byte - 1);
    mainCommand[set_pos] = set_byte;
  }
  // set heat delta 1-15
  else if (Topics::SET16.compare(topic) == 0)
  {
    //pos 84
    set_pos = 84;
    set_byte = msg_int + 128;
    sprintf(log_msg, "<SUB> SET16 %s: %d", topic, set_byte - 128);
    mainCommand[set_pos] = set_byte;
  }
  // set cool delta 1-15
  else if (Topics::SET17.compare(topic) == 0)
  {
    //pos 94
    set_pos = 94;
    set_byte = msg_int + 128;
    sprintf(log_msg, "<SUB> SET17 %s: %d", topic, set_byte - 128);
    mainCommand[set_pos] = set_byte;
  }
  // set DHW reheat delta -5 -15
  else if (Topics::SET18.compare(topic) == 0)
  {
    //pos 99
    set_pos = 99;
    set_byte = msg_int + 128;
    sprintf(log_msg, "<SUB> SET18 %s: %d", topic, set_byte - 128);
    mainCommand[set_pos] = set_byte;
  }
  // set DHW heatup time (max) 5 -240
  else if (Topics::SET19.compare(topic) == 0)
  {
    //pos 98
    set_pos = 98;
    set_byte = msg_int + 1;
    sprintf(log_msg, "<SUB> SET19 %s: %d", topic, set_byte - 1);
    mainCommand[set_pos] = set_byte;
  }
  push_command_buffer(mainCommand, sizeof(mainCommand), log_msg);
}
