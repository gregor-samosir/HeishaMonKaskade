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
  byte mask;                // bits this field owns inside that byte (see below)
  int min;                  // allowed value range (inclusive)
  int max;
  ConvType conv;            // conversion rule
  int param;                // offset or factor for the conversion
};

/*****************************************************************************/
/* Why the mask column exists                                                */
/*                                                                           */
/* Several set topics share one protocol byte, each owning a different group */
/* of bits (0 in a group means "no change" for that field). Writing the byte */
/* as a whole therefore silently wipes every other field in it - two set     */
/* commands arriving inside the same 500 ms window would cancel each other.  */
/* Applying the value through its mask keeps neighbouring fields intact:     */
/*                                                                           */
/*   byte 4:  Heatpump 0x03  |  WaterPump 0x30  |  ForceDHW 0xC0             */
/*   byte 5:  HolidayMode 0x30                                               */
/*   byte 8:  ForceDefrost 0x02  |  ForceSterilization 0x04                  */
/*                                                                           */
/* Exception - byte 7: QuietMode ((n+1)*8) and PowerfulMode (73..76) really  */
/* do overlap in bit 3, PowerfulMode carries an implicit "quiet off". That   */
/* is a protocol property, not an implementation flaw, so both keep mask     */
/* 0xFF and behave exactly as before (last one wins). The conflict warning   */
/* below makes it visible if it ever happens.                                */
/*****************************************************************************/

static const SetCommand setCommands[] = {
    // Nr topic            pos  mask  min  max  conversion    param
    {1, &Topics::SET1, 4, 0x03, 0, 1, CONV_ADD, 1},        // Heatpump off=1 on=2
    {2, &Topics::SET2, 5, 0x30, 0, 1, CONV_MUL_INC, 16},   // HolidayMode off=16 on=32
    {3, &Topics::SET3, 7, 0xFF, 0, 3, CONV_MUL_INC, 8},    // QuietMode level -> (n+1)*8
    {4, &Topics::SET4, 7, 0xFF, 0, 3, CONV_ADD, 73},       // PowerfulMode 0/30/60/90 min
    {5, &Topics::SET5, 38, 0xFF, -5, 65, CONV_ADD, 128},   // Z1 heat: shift or direct temp
    {6, &Topics::SET6, 39, 0xFF, -5, 65, CONV_ADD, 128},   // Z1 cool: shift or direct temp
    {7, &Topics::SET7, 40, 0xFF, -5, 65, CONV_ADD, 128},   // Z2 heat: shift or direct temp
    {8, &Topics::SET8, 41, 0xFF, -5, 65, CONV_ADD, 128},   // Z2 cool: shift or direct temp
    {9, &Topics::SET9, 6, 0xFF, 0, 6, CONV_OPMODE, 0},     // OperationMode via lookup
    {10, &Topics::SET10, 4, 0xC0, 0, 1, CONV_MUL_INC, 64}, // ForceDHW off=64 on=128
    {11, &Topics::SET11, 42, 0xFF, 40, 75, CONV_ADD, 128}, // DHW target temp
    {12, &Topics::SET12, 8, 0x02, 0, 1, CONV_MUL, 2},      // ForceDefrost off=0 on=2
    {13, &Topics::SET13, 8, 0x04, 0, 1, CONV_MUL, 4},      // ForceSterilization off=0 on=4
    {14, &Topics::SET14, 4, 0x30, 0, 2, CONV_MUL_INC, 16}, // WaterPump off/on/airpurge
    {15, &Topics::SET15, 45, 0xFF, 65, 254, CONV_ADD, 1},  // PumpSpeedMax
    {16, &Topics::SET16, 84, 0xFF, 1, 15, CONV_ADD, 128},  // HeatDelta
    {17, &Topics::SET17, 94, 0xFF, 1, 15, CONV_ADD, 128},  // CoolDelta
    {18, &Topics::SET18, 99, 0xFF, -15, 15, CONV_ADD, 128},// DHWHeatDelta
    {19, &Topics::SET19, 98, 0xFF, 5, 240, CONV_ADD, 1},   // DHWHeatupTime minutes
    {20, &Topics::SET20, 85, 0xFF, -20, 35, CONV_ADD, 128},// HeaterOnOutdoorTemp
    {21, &Topics::SET21, 83, 0xFF, 5, 35, CONV_ADD, 128},  // HeatingOffOutdoorTemp
    {22, &Topics::SET22, 72, 0xFF, 0, 254, CONV_ADD, 1},   // SGReadyCapacity1Heat
    {23, &Topics::SET23, 71, 0xFF, 0, 254, CONV_ADD, 1},   // SGReadyCapacity1DHW
    {24, &Topics::SET24, 74, 0xFF, 0, 254, CONV_ADD, 1},   // SGReadyCapacity2Heat
    {25, &Topics::SET25, 73, 0xFF, 0, 254, CONV_ADD, 1},   // SGReadyCapacity2DHW
    {26, &Topics::SET26, 97, 0xFF, 0, 254, CONV_ADD, 1},   // DHWRoomMaxTime (steps of 30 min)
    // Zone 1 heating curve: two points, each an outside temperature paired
    // with the flow target at that point. Kept in sync from Node-RED so the
    // heatpump can run on its own curve if the cascade control is unavailable.
    // Read back via TOP29/TOP30/TOP32/TOP31 respectively.
    {27, &Topics::SET27, 75, 0xFF, 20, 55, CONV_ADD, 128},  // Z1HeatCurveTargetHighTemp
    {28, &Topics::SET28, 76, 0xFF, 20, 55, CONV_ADD, 128},  // Z1HeatCurveTargetLowTemp
    {29, &Topics::SET29, 77, 0xFF, -15, 15, CONV_ADD, 128}, // Z1HeatCurveOutsideLowTemp
    {30, &Topics::SET30, 78, 0xFF, 15, 35, CONV_ADD, 128},  // Z1HeatCurveOutsideHighTemp
    // Zone 1 cooling curve, same idea. Read back via TOP72/TOP73/TOP75/TOP74.
    {31, &Topics::SET31, 86, 0xFF, 5, 20, CONV_ADD, 128},   // Z1CoolCurveTargetHighTemp
    {32, &Topics::SET32, 87, 0xFF, 5, 20, CONV_ADD, 128},   // Z1CoolCurveTargetLowTemp
    {33, &Topics::SET33, 88, 0xFF, 20, 30, CONV_ADD, 128},  // Z1CoolCurveOutsideLowTemp
    // upper bound is 30, not 40: the heatpump silently clamps anything above
    // (measured on both units - 31, 32, 35 and 40 all came back as 30).
    // Without this the firmware would happily forward a value that never
    // arrives, which is exactly the kind of silent loss we removed elsewhere.
    {34, &Topics::SET34, 89, 0xFF, 20, 30, CONV_ADD, 128},  // Z1CoolCurveOutsideHighTemp
};

// bits already claimed in the pending command, one entry per protocol byte.
// Only used to detect two fields fighting over the same bits - cleared
// together with mainCommand after each send (see send_pana_command).
byte usedMask[QUERYSIZE];

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

    // short topic name without path prefix, log format matches the <PUB> lines
    const char *topic_name = strrchr(topic, '/');
    topic_name = (topic_name != NULL) ? topic_name + 1 : topic;

    // range check before touching mainCommand
    if (msg_long < cmd.min || msg_long > cmd.max)
    {
      (void)sprintf(log_msg, "Error: Value %ld out of range [%d..%d] for topic %s", msg_long, cmd.min, cmd.max, topic_name);
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

    // conflict diagnosis: two fields claiming the same bits before the buffer
    // was sent. Must never happen silently again - log it, then continue with
    // the previous behaviour (last value wins).
    if (usedMask[cmd.pos] & cmd.mask)
    {
      (void)sprintf(log_msg, "Warning: Field conflict in byte %d (%s) - last value wins", cmd.pos, topic_name);
      write_mqtt_log(log_msg);
    }
    usedMask[cmd.pos] |= cmd.mask;

    // insert the field bit-exact: other fields sharing this byte survive.
    // Clearing the mask first is what makes a repeated set overwrite its own
    // previous value instead of OR-ing on top of it.
    mainCommand[cmd.pos] = (mainCommand[cmd.pos] & ~cmd.mask) | (set_byte & cmd.mask);

    // explicit flag instead of deriving "buffer is filled" from a byte sum:
    // a checksum of 0 is a valid command (e.g. ForceDefrost off) and wraps
    // at 256, which used to drop whole command telegrams silently
    setDataPending = true;

    (void)sprintf(log_msg, "<SUB> SET%d %s: %ld", cmd.number, topic_name, msg_long);
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
