#include "HeishaMon.h"
#include "commands.h"
#include "Topics.h"

/*****************************************************************************/
/* Set command table                                                         */
/* one row per set topic: target position in mainCommand, allowed value     */
/* range and the conversion rule from mqtt value to protocol byte           */
/*****************************************************************************/

// conversion rules from mqtt value to protocol byte
enum ConvType
{
  CONV_ADD,    // set_byte = value + param
  CONV_MUL,    // set_byte = value * param
  CONV_MUL_INC,// set_byte = (value + 1) * param
  CONV_OPMODE  // set_byte = opModeBytes[value]
};

// protocol bytes for OperationMode 0-6 (heat, cool, auto, dhw, heat+dhw, cool+dhw, auto+dhw)
static const byte opModeBytes[] = {18, 19, 24, 33, 34, 35, 40};

struct SetCommand
{
  byte number;              // SETn, for logging only
  const std::string *topic; // mqtt set topic
  byte pos;                 // target position in mainCommand
  int min;                  // allowed value range (inclusive)
  int max;
  ConvType conv;            // conversion rule
  int param;                // offset or factor for the conversion
};

static const SetCommand setCommands[] = {
    // Nr topic            pos  min  max  conversion    param
    {1, &Topics::SET1, 4, 0, 1, CONV_ADD, 1},        // Heatpump off=1 on=2
    {2, &Topics::SET2, 5, 0, 1, CONV_MUL_INC, 16},   // HolidayMode off=16 on=32
    {3, &Topics::SET3, 7, 0, 3, CONV_MUL_INC, 8},    // QuietMode level -> (n+1)*8
    {4, &Topics::SET4, 7, 0, 3, CONV_ADD, 73},       // PowerfulMode 0/30/60/90 min
    {5, &Topics::SET5, 38, -5, 65, CONV_ADD, 128},   // Z1 heat: shift or direct temp
    {6, &Topics::SET6, 39, -5, 65, CONV_ADD, 128},   // Z1 cool: shift or direct temp
    {7, &Topics::SET7, 40, -5, 65, CONV_ADD, 128},   // Z2 heat: shift or direct temp
    {8, &Topics::SET8, 41, -5, 65, CONV_ADD, 128},   // Z2 cool: shift or direct temp
    {9, &Topics::SET9, 6, 0, 6, CONV_OPMODE, 0},     // OperationMode via lookup
    {10, &Topics::SET10, 4, 0, 1, CONV_MUL_INC, 64}, // ForceDHW off=64 on=128
    {11, &Topics::SET11, 42, 40, 75, CONV_ADD, 128}, // DHW target temp
    {12, &Topics::SET12, 8, 0, 1, CONV_MUL, 2},      // ForceDefrost off=0 on=2
    {13, &Topics::SET13, 8, 0, 1, CONV_MUL, 4},      // ForceSterilization off=0 on=4
    {14, &Topics::SET14, 4, 0, 2, CONV_MUL_INC, 16}, // WaterPump off/on/airpurge
    {15, &Topics::SET15, 45, 65, 254, CONV_ADD, 1},  // PumpSpeedMax
    {16, &Topics::SET16, 84, 1, 15, CONV_ADD, 128},  // HeatDelta
    {17, &Topics::SET17, 94, 1, 15, CONV_ADD, 128},  // CoolDelta
    {18, &Topics::SET18, 99, -15, 15, CONV_ADD, 128},// DHWHeatDelta
    {19, &Topics::SET19, 98, 5, 240, CONV_ADD, 1},   // DHWHeatupTime minutes
    {20, &Topics::SET20, 85, -20, 35, CONV_ADD, 128},// HeaterOnOutdoorTemp
    {21, &Topics::SET21, 83, 5, 35, CONV_ADD, 128},  // HeatingOffOutdoorTemp
    {22, &Topics::SET22, 72, 0, 254, CONV_ADD, 1},   // SGReadyCapacity1Heat
    {23, &Topics::SET23, 71, 0, 254, CONV_ADD, 1},   // SGReadyCapacity1DHW
    {24, &Topics::SET24, 74, 0, 254, CONV_ADD, 1},   // SGReadyCapacity2Heat
    {25, &Topics::SET25, 73, 0, 254, CONV_ADD, 1},   // SGReadyCapacity2DHW
    {26, &Topics::SET26, 97, 0, 254, CONV_ADD, 1},   // DHWRoomMaxTime (steps of 30 min)
};

/*****************************************************************************/
/* Build the heatpump command from an mqtt set message                       */
/* returns true if a command was registered, false on any error              */
/* (caller must restart the mainquery timer on false, see mqtt_callback)     */
/*****************************************************************************/
bool build_heatpump_command(char *topic, char *msg)
{
  char log_msg[256];

  // parse and validate the payload as integer
  char *endptr;
  long msg_long = strtol(msg, &endptr, 10);
  if (endptr == msg || *endptr != '\0')
  {
    (void)sprintf(log_msg, "Error: Invalid integer value '%s' received for topic %s", msg, topic);
    write_mqtt_log(log_msg);
    return false;
  }

  // find the matching set topic in the table
  for (unsigned int i = 0; i < (sizeof(setCommands) / sizeof(setCommands[0])); i++)
  {
    const SetCommand &cmd = setCommands[i];
    if (cmd.topic->compare(topic) != 0)
    {
      continue;
    }

    // range check before touching mainCommand
    if (msg_long < cmd.min || msg_long > cmd.max)
    {
      (void)sprintf(log_msg, "Error: Value %ld out of range [%d..%d] for topic %s", msg_long, cmd.min, cmd.max, topic);
      write_mqtt_log(log_msg);
      return false;
    }

    // convert the validated value to the protocol byte
    int value = (int)msg_long;
    byte set_byte = 0;
    switch (cmd.conv)
    {
    case CONV_ADD:
      set_byte = value + cmd.param;
      break;
    case CONV_MUL:
      set_byte = value * cmd.param;
      break;
    case CONV_MUL_INC:
      set_byte = (value + 1) * cmd.param;
      break;
    case CONV_OPMODE:
      set_byte = opModeBytes[value]; // value is range-checked 0..6
      break;
    }

    mainCommand[cmd.pos] = set_byte;

    (void)sprintf(log_msg, "<SUB> SET%d %s: %ld", cmd.number, topic, msg_long);
    write_mqtt_log(log_msg);
    // trigger buffer
    register_new_command();
    return true;
  }

  // no table entry matched
  (void)sprintf(log_msg, "Error: Unknown set topic %s", topic);
  write_mqtt_log(log_msg);
  return false;
}
