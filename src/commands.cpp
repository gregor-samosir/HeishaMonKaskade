#include "HeishaMon.h"
#include "commands.h"
#include "Topics.h"
#include <climits>
// #include <string>

/*****************************************************************************/
/* Allowed value range per set topic                                         */
/* commands outside these limits are rejected before touching mainCommand    */
/*****************************************************************************/
struct SetLimit
{
  const std::string *topic;
  int min;
  int max;
};

static const SetLimit setLimits[] = {
    {&Topics::SET1, 0, 1},    // Heatpump on/off
    {&Topics::SET2, 0, 1},    // HolidayMode on/off
    {&Topics::SET3, 0, 3},    // QuietMode level
    {&Topics::SET4, 0, 3},    // PowerfulMode 0/30/60/90 min
    {&Topics::SET5, -5, 65},  // Z1 heat: shift -5..5 or direct temp
    {&Topics::SET6, -5, 65},  // Z1 cool: shift -5..5 or direct temp
    {&Topics::SET7, -5, 65},  // Z2 heat: shift -5..5 or direct temp
    {&Topics::SET8, -5, 65},  // Z2 cool: shift -5..5 or direct temp
    {&Topics::SET9, 0, 6},    // OperationMode
    {&Topics::SET10, 0, 1},   // ForceDHW on/off
    {&Topics::SET11, 40, 75}, // DHW target temp
    {&Topics::SET12, 0, 1},   // ForceDefrost on/off
    {&Topics::SET13, 0, 1},   // ForceSterilization on/off
    {&Topics::SET14, 0, 2},   // WaterPump off/on/airpurge
    {&Topics::SET15, 65, 254},// PumpSpeedMax (254: +1 conversion must fit a byte)
    {&Topics::SET16, 1, 15},  // HeatDelta
    {&Topics::SET17, 1, 15},  // CoolDelta
    {&Topics::SET18, -15, 15},// DHWHeatDelta
    {&Topics::SET19, 5, 240}, // DHWHeatupTime minutes
    {&Topics::SET20, -20, 35},// HeaterOnOutdoorTemp
    {&Topics::SET21, 5, 35},  // HeatingOffOutdoorTemp
    {&Topics::SET22, 0, 254}, // SGReadyCapacity1Heat
    {&Topics::SET23, 0, 254}, // SGReadyCapacity1DHW
    {&Topics::SET24, 0, 254}, // SGReadyCapacity2Heat
    {&Topics::SET25, 0, 254}, // SGReadyCapacity2DHW
    {&Topics::SET26, 0, 254}, // DHWRoomMaxTime (steps of 30 min)
};

// returns true if a command was registered, false on any error
// (caller must restart the mainquery timer on false, see mqtt_callback)
bool build_heatpump_command(char *topic, char *msg)
{
  char log_msg[256];
  byte set_byte = 0;
  byte set_pos = COMMANDPOS_UNSET; // position in mainCommand, sentinel = no topic matched

  char *endptr;
  long msg_long = strtol(msg, &endptr, 10); // 10 is for base-10

  // Check for conversion errors
  if (endptr == msg || *endptr != '\0')
  {
    (void)sprintf(log_msg, "Error: Invalid integer value '%s' received for topic %s", msg, topic);
    write_mqtt_log(log_msg);
    return false;
  }

  // Check the value against the allowed range of the matching set topic
  // negative values are valid for shift temperatures (SET5-8, SET18, SET20)
  for (unsigned int i = 0; i < (sizeof(setLimits) / sizeof(setLimits[0])); i++)
  {
    if (setLimits[i].topic->compare(topic) == 0)
    {
      if (msg_long < setLimits[i].min || msg_long > setLimits[i].max)
      {
        (void)sprintf(log_msg, "Error: Value %ld out of range [%d..%d] for topic %s", msg_long, setLimits[i].min, setLimits[i].max, topic);
        write_mqtt_log(log_msg);
        return false;
      }
      break;
    }
  }
  int msg_int = (int)msg_long;

  // set heatpump state to on by sending 1
  if (Topics::SET1.compare(topic) == 0)
  {
    set_pos = 4;
    set_byte = 1;
    if (msg_int == 1)
    {
      set_byte = 2;
    }
    (void)sprintf(log_msg, "<SUB> SET1 %s: %d", topic, set_byte);
  }

  // set 0 for Off mode, set 1 for Quiet mode 1, set 2 for Quiet mode 2, set 3 for Quiet mode 3
  else if (Topics::SET3.compare(topic) == 0)
  {
    set_pos = 7;
    set_byte = (msg_int + 1) * 8;
    (void)sprintf(log_msg, "<SUB> SET3 %s: %d", topic, set_byte / 8 - 1);
  }

  // z1 heat request temp -  set from -5 to 5 to get same temperature shift point or set direct temp
  else if (Topics::SET5.compare(topic) == 0)
  {
    set_pos = 38;
    set_byte = msg_int + 128;
    (void)sprintf(log_msg, "<SUB> SET5 %s: %d", topic, set_byte - 128);
  }

  // z1 cool request temp -  set from -5 to 5 to get same temperature shift point or set direct temp
  else if (Topics::SET6.compare(topic) == 0)
  {
    set_pos = 39;
    set_byte = msg_int + 128;
    (void)sprintf(log_msg, "<SUB> SET6 %s: %d", topic, set_byte - 128);
  }

  // z2 heat request temp -  set from -5 to 5 to get same temperature shift point or set direct temp
  else if (Topics::SET7.compare(topic) == 0)
  {
    set_pos = 40;
    set_byte = msg_int + 128;
    (void)sprintf(log_msg, "<SUB> SET7 %s: %d", topic, set_byte - 128);
  }

  // z2 cool request temp -  set from -5 to 5 to get same temperature shift point or set direct temp
  else if (Topics::SET8.compare(topic) == 0)
  {
    set_pos = 41;
    set_byte = msg_int + 128;
    (void)sprintf(log_msg, "<SUB> SET8 %s: %d", topic, set_byte - 128);
  }

  // set mode to force DHW by sending 1
  else if (Topics::SET10.compare(topic) == 0)
  {
    set_pos = 4;
    set_byte = 64; // hex 0x40
    if (msg_int == 1)
    {
      set_byte = 128; // hex 0x80
    }
    (void)sprintf(log_msg, "<SUB> SET10 %s: %d", topic, set_byte);
  }

  // set mode to force defrost  by sending 1
  else if (Topics::SET12.compare(topic) == 0)
  {
    set_pos = 8;
    set_byte = 0;
    if (msg_int == 1)
    {
      set_byte = 2; // hex 0x02
    }
    (void)sprintf(log_msg, "<SUB> SET12 %s: %d", topic, set_byte);
  }

  // set mode to force sterilization by sending 1
  else if (Topics::SET13.compare(topic) == 0)
  {
    set_pos = 8;
    set_byte = 0;
    if (msg_int == 1)
    {
      set_byte = 4; // hex 0x04
    }
    (void)sprintf(log_msg, "<SUB> SET13 %s: %d", topic, set_byte);
  }

  // set Holiday mode by sending 1, off will be 0
  else if (Topics::SET2.compare(topic) == 0)
  {
    set_pos = 5;
    set_byte = 16; // hex 0x10
    if (msg_int == 1)
    {
      set_byte = 32; // hex 0x20
    }
    (void)sprintf(log_msg, "<SUB> SET2 %s: %d", topic, set_byte);
  }

  // set Powerful mode by sending 0 = off, 1 for 30min, 2 for 60min, 3 for 90 min
  else if (Topics::SET4.compare(topic) == 0)
  {
    set_pos = 7;
    set_byte = (msg_int) + 73;
    (void)sprintf(log_msg, "<SUB> SET4 %s: %d", topic, (set_byte - 73));
  }

  // set Heat pump operation mode 0 = heat only, 1 = cool only, 2 = Auto(Heat), 3 = DHW only, 4 = Heat+DHW, 5 = Cool+DHW, 6 = Auto(Heat) + DHW
  else if (Topics::SET9.compare(topic) == 0)
  {
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
    (void)sprintf(log_msg, "<SUB> SET9 %s: %d", topic, set_byte);
  }

  // set DHW temperature by sending desired temperature between 40C-75C
  else if (Topics::SET11.compare(topic) == 0)
  {
    set_pos = 42;
    set_byte = msg_int + 128;
    (void)sprintf(log_msg, "<SUB> SET11 %s: %d", topic, set_byte - 128);
  }

  // set water pump state to on=1 off=0 airpurge = 2
  else if (Topics::SET14.compare(topic) == 0)
  {
    set_pos = 4;
    set_byte = 16; // hex 0x10
    if (msg_int == 1)
    {
      set_byte = 32; // hex 0x20
    }
    if (msg_int == 2)
    {
      set_byte = 48; // hex 0x30
    }
    (void)sprintf(log_msg, "<SUB> SET14 %s: %d", topic, set_byte);
  }

  // set PumpSpeedMax 65 - 255
  else if (Topics::SET15.compare(topic) == 0)
  {
    set_pos = 45;
    set_byte = msg_int + 1;
    (void)sprintf(log_msg, "<SUB> SET15 %s: %d", topic, set_byte - 1);
  }
  // set heat delta 1-15
  else if (Topics::SET16.compare(topic) == 0)
  {
    set_pos = 84;
    set_byte = msg_int + 128;
    (void)sprintf(log_msg, "<SUB> SET16 %s: %d", topic, set_byte - 128);
  }
  // set cool delta 1-15
  else if (Topics::SET17.compare(topic) == 0)
  {
    set_pos = 94;
    set_byte = msg_int + 128;
    (void)sprintf(log_msg, "<SUB> SET17 %s: %d", topic, set_byte - 128);
  }
  // set DHW reheat delta -5 -15
  else if (Topics::SET18.compare(topic) == 0)
  {
    set_pos = 99;
    set_byte = msg_int + 128;
    (void)sprintf(log_msg, "<SUB> SET18 %s: %d", topic, set_byte - 128);
  }
  // set DHW heatup time (max) 5 -240
  else if (Topics::SET19.compare(topic) == 0)
  {
    set_pos = 98;
    set_byte = msg_int + 1;
    (void)sprintf(log_msg, "<SUB> SET19 %s: %d", topic, set_byte - 1);
  }
  // set Heater_On_Outdoor_Temp
  else if (Topics::SET20.compare(topic) == 0)
  {
    set_pos = 85;
    set_byte = msg_int + 128;
    (void)sprintf(log_msg, "<SUB> SET20 %s: %d", topic, set_byte - 128);
  }
  // set Heating_Off_Outdoor_Temp
  else if (Topics::SET21.compare(topic) == 0)
  {
    set_pos = 83;
    set_byte = msg_int + 128;
    (void)sprintf(log_msg, "<SUB> SET21 %s: %d", topic, set_byte - 128);
  }
  // set SG-Ready SGReady_Capacity1_Heat
  else if (Topics::SET22.compare(topic) == 0)
  {
    set_pos = 72;
    set_byte = msg_int + 1;
    (void)sprintf(log_msg, "<SUB> SET22 %s: %d", topic, set_byte - 1);
  }
  // set SG-Ready SGReady_Capacity1_DHW
  else if (Topics::SET23.compare(topic) == 0)
  {
    set_pos = 71;
    set_byte = msg_int + 1;
    (void)sprintf(log_msg, "<SUB> SET23 %s: %d", topic, set_byte - 1);
  }
  // set SG-Ready SGReady_Capacity2_Heat
  else if (Topics::SET24.compare(topic) == 0)
  {
    set_pos = 74;
    set_byte = msg_int + 1;
    (void)sprintf(log_msg, "<SUB> SET24 %s: %d", topic, set_byte - 1);
  }
  // set SG-Ready SGReady_Capacity2_DHW
  else if (Topics::SET25.compare(topic) == 0)
  {
    set_pos = 73;
    set_byte = msg_int + 1;
    (void)sprintf(log_msg, "<SUB> SET25 %s: %d", topic, set_byte - 1);
  }
  // set DHW room time (max) in steps of 30 minutes
  else if (Topics::SET26.compare(topic) == 0)
  {
    set_pos = 97;
    set_byte = msg_int + 1;
    (void)sprintf(log_msg, "<SUB> SET26 %s: %d", topic, set_byte - 1);
  }

  // no else-if branch matched: never write with the uninitialized sentinel position
  if (set_pos == COMMANDPOS_UNSET)
  {
    (void)sprintf(log_msg, "Error: Unknown set topic %s", topic);
    write_mqtt_log(log_msg);
    return false;
  }

  mainCommand[set_pos] = set_byte;

  write_mqtt_log(log_msg);
  // trigger buffer
  register_new_command();
  return true;
}
