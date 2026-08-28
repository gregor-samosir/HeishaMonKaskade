#include "HeishaMon.h"
#include "commands.h"
#include "Topics.h"

/*****************************************************************************/
/* Set command table                                                         */
/* one row per set topic: target position in mainCommand, allowed value     */
/* range and the conversion rule from mqtt value to protocol byte           */
/*****************************************************************************/

// conversion rules from mqtt value to protocol byte
// byte-basiert statt int: das enum steckt in jeder Tabellenzeile, und jedes
// gesparte Byte zaehlt dort mal die Zeilenzahl (vgl. Feldreihenfolge in
// decode.h)
enum ConvType : byte
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
  byte number;      // SETn, for logging only
  byte pos;         // target position in mainCommand
  byte mask;        // bits this field owns inside that byte (see below)
  ConvType conv;    // conversion rule
  const char *name; // mqtt topic name below <prefix>/set/
  int min;          // allowed value range (inclusive)
  int max;
  int param;        // offset or factor for the conversion
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
/*   byte 5:  ForceHeater 0x0C  |  HolidayMode 0x30                          */
/*   byte 8:  ForceDefrost 0x02  |  ForceSterilization 0x04                  */
/*   byte 9:  DHWHeaterState 0x0C  |  RoomHeaterState 0x03                   */
/*   byte 28: CoolingMode 0x0C  |  HeatingMode 0x03                          */
/*                                                                           */
/* Exception - byte 7: QuietMode ((n+1)*8) and PowerfulMode (73..76) really  */
/* do overlap in bit 3, PowerfulMode carries an implicit "quiet off". That   */
/* is a protocol property, not an implementation flaw, so both keep mask     */
/* 0xFF and behave exactly as before (last one wins). The conflict warning   */
/* below makes it visible if it ever happens.                               */
/*****************************************************************************/

/*****************************************************************************/
/* Warum der Topic-Name in der Tabelle steht                                 */
/*                                                                           */
/* Bis 3.4.1 fuehrte jeder Set-Topic-Name drei Leben: eine Deklaration in    */
/* Topics.h, die Definition in Topics.cpp und den Verweis darauf hier. Dazu  */
/* kam ein handgeschriebener subscribe-Aufruf in mqtt_reconnect(). Wer den   */
/* vergass, bekam KEINEN Compilerfehler - das Topic war einfach stumm.       */
/* Jetzt steht der Name genau hier, und subscribe_set_topics() laeuft ueber  */
/* dieselbe Tabelle. Ein neues Set-Kommando ist damit eine Zeile.            */
/* Dasselbe Muster wie bei den State-Topics (stateTopics[] in decode.cpp):    */
/* die Tabelle traegt den Namen, die Pfadwurzel kommt aus Topics.cpp.        */
/*****************************************************************************/

/*****************************************************************************/
/* Warum die Nummerierung Luecken hat                                        */
/*                                                                           */
/* SET7 und SET8 waren die Zone-2-Anforderungstemperaturen und sind in 3.4.0 */
/* entfallen - diese Anlage hat keine Zone 2, und in Node-RED wurden sie nie */
/* benutzt. Die uebrigen Nummern sind ABSICHTLICH stehen geblieben: 'number' */
/* ist ein Datenfeld, kein Index, und SET9 heisst in MQTT-Topics.md und in    */
/* allen Notizen weiter SET9. Bitte nicht "aufraeumen" - eine Umnummerierung */
/* macht jede Doku und jeden Mitschnitt von vorher falsch.                   */
/*****************************************************************************/

static const SetCommand setCommands[] = {
    // Nr pos mask conversion    topic-name                     min  max  param
    { 1,  4, 0x03, CONV_ADD,     "Heatpump",                      0,   1,   1}, // off=1 on=2
    { 2,  5, 0x30, CONV_MUL_INC, "HolidayMode",                   0,   1,  16}, // off=16 on=32
    { 3,  7, 0xFF, CONV_MUL_INC, "QuietMode",                     0,   3,   8}, // level -> (n+1)*8
    { 4,  7, 0xFF, CONV_ADD,     "PowerfulMode",                  0,   3,  73}, // 0/30/60/90 min
    { 5, 38, 0xFF, CONV_ADD,     "Z1HeatRequestTemperature",     -5,  65, 128}, // shift or direct temp
    { 6, 39, 0xFF, CONV_ADD,     "Z1CoolRequestTemperature",     -5,  65, 128}, // shift or direct temp
    { 9,  6, 0xFF, CONV_OPMODE,  "OperationMode",                 0,   6,   0}, // via lookup
    {10,  4, 0xC0, CONV_MUL_INC, "ForceDHW",                      0,   1,  64}, // off=64 on=128
    {11, 42, 0xFF, CONV_ADD,     "DHWTemp",                      40,  75, 128}, // DHW target temp
    {12,  8, 0x02, CONV_MUL,     "ForceDefrost",                  0,   1,   2}, // off=0 on=2
    {13,  8, 0x04, CONV_MUL,     "ForceSterilization",            0,   1,   4}, // off=0 on=4
    {14,  4, 0x30, CONV_MUL_INC, "WaterPump",                     0,   2,  16}, // off/on/airpurge
    {15, 45, 0xFF, CONV_ADD,     "WaterPumpSpeed",               65, 254,   1}, // PumpSpeedMax
    {16, 84, 0xFF, CONV_ADD,     "HeatDelta",                     1,  15, 128},
    {17, 94, 0xFF, CONV_ADD,     "CoolDelta",                     1,  15, 128},
    {18, 99, 0xFF, CONV_ADD,     "DHWHeatDelta",                -15,  15, 128},
    {19, 98, 0xFF, CONV_ADD,     "DHWHeatupTime",                 5, 240,   1}, // minutes
    {20, 85, 0xFF, CONV_ADD,     "HeaterOnOutdoorTemp",         -20,  35, 128},
    {21, 83, 0xFF, CONV_ADD,     "HeatingOffOutdoorTemp",         5,  35, 128},
    {22, 72, 0xFF, CONV_ADD,     "SGReadyCapacity1Heat",          0, 254,   1},
    {23, 71, 0xFF, CONV_ADD,     "SGReadyCapacity1DHW",           0, 254,   1},
    {24, 74, 0xFF, CONV_ADD,     "SGReadyCapacity2Heat",          0, 254,   1},
    {25, 73, 0xFF, CONV_ADD,     "SGReadyCapacity2DHW",           0, 254,   1},
    {26, 97, 0xFF, CONV_ADD,     "DHWRoomMaxTime",                0, 254,   1}, // steps of 30 min
    // Zone 1 heating curve: two points, each an outside temperature paired
    // with the flow target at that point. Kept in sync from Node-RED so the
    // heatpump can run on its own curve if the cascade control is unavailable.
    // Read back via TOP29/TOP30/TOP32/TOP31 respectively.
    //
    // High/Low does NOT refer to the same axis in both pairs: Outside_* names
    // outside temperatures, Target_* names flow temperatures, and they cross
    // over - TargetHigh is the flow when it is COLD ("VL kalt"), TargetLow the
    // flow when it is WARM ("VL warm"). Measured 2026-08-20, see MQTT-Topics.md.
    {27, 75, 0xFF, CONV_ADD,     "Z1HeatCurveTargetHighTemp",    20,  55, 128},
    {28, 76, 0xFF, CONV_ADD,     "Z1HeatCurveTargetLowTemp",     20,  55, 128},
    {29, 77, 0xFF, CONV_ADD,     "Z1HeatCurveOutsideLowTemp",   -15,  15, 128},
    // -15..15, NICHT 15..35: an der Anlage ausgemessen. Werte ueber 15
    // werden still verworfen, unter -15 auf -15 geklemmt. Der frueher
    // angenommene Bereich lag komplett auf der falschen Seite - von 21
    // erlaubten Werten war genau einer gueltig.
    {30, 78, 0xFF, CONV_ADD,     "Z1HeatCurveOutsideHighTemp",  -15,  15, 128},
    // Zone 1 cooling curve, same idea. Read back via TOP72/TOP73/TOP75/TOP74.
    {31, 86, 0xFF, CONV_ADD,     "Z1CoolCurveTargetHighTemp",     5,  20, 128},
    {32, 87, 0xFF, CONV_ADD,     "Z1CoolCurveTargetLowTemp",      5,  20, 128},
    // 15..30, Untergrenze berichtigt 2026-08-25: die Klemm-Messung vom
    // 2026-08-10 hatte hier 20 ergeben, das Bedienterminal nennt aber 15 -
    // und genau 15 nimmt die WP am Terminal auch an. Bis 3.14.1 wies die
    // Firmware den Wert selbst ab, bevor das Kommando zur WP ging.
    {33, 88, 0xFF, CONV_ADD,     "Z1CoolCurveOutsideLowTemp",    15,  30, 128},
    // 15..30, ebenfalls ausgemessen: darueber klemmt die WP auf 30,
    // darunter auf 15 - beides ohne jede Rueckmeldung. Genau die Art
    // stiller Verluste, die mit 3.1.0 beseitigt wurde.
    {34, 89, 0xFF, CONV_ADD,     "Z1CoolCurveOutsideHighTemp",   15,  30, 128},
    // Betriebsart je Kreis: 0 = Kompensationskurve, 1 = Direktvorgabe.
    // Byte 28 traegt BEIDE Betriebsarten in zwei Bitfeldern - die Masken sind
    // hier deshalb Pflicht, nicht Kosmetik: ohne sie schaltete ein Kuehl-
    // Kommando die Heizung mit um. Rueckgelesen ueber TOP76 Heating_Mode
    // (getBit7and8) und TOP81 Cooling_Mode (getBit5and6); die Kodierung ist
    // aus genau diesen Dekodierern zurueckgerechnet:
    //   HeatingMode 0 -> (0+1)*1 = 0b01, gelesen (0b01 & 0b11) - 1     = 0
    //   HeatingMode 1 -> (1+1)*1 = 0b10, gelesen (0b10 & 0b11) - 1     = 1
    //   CoolingMode 0 -> (0+1)*4 = 0b0100, gelesen ((4>>2) & 0b11) - 1 = 0
    //   CoolingMode 1 -> (1+1)*4 = 0b1000, gelesen ((8>>2) & 0b11) - 1 = 1
    // ACHTUNG, NICHT FOLGENLOS UMKEHRBAR: Ein Wechsel von Direkt auf Kurve
    // setzt die vier Kurvenpunkte des betroffenen Kreises auf die Panasonic-
    // Werksvorgaben zurueck, und das Zurueckschalten stellt sie NICHT wieder
    // her. Ausserdem uebernimmt der Direktsollwert dabei den unteren
    // Kurvenpunkt. Wer hierher schreibt, muss die Kurve danach aus dem
    // ioBroker nachziehen (test/kurven_sync.py) - siehe test/README.md.
    {35, 28, 0x03, CONV_MUL_INC, "HeatingMode",                   0,   1,   1},
    {36, 28, 0x0C, CONV_MUL_INC, "CoolingMode",                   0,   1,   4},
    // Heizstab (3.17.0). Byte 9 traegt BEIDE Freigaben in zwei Bitfeldern, die
    // Masken sind hier deshalb Pflicht: ohne sie legte ein Raumheiz-Kommando die
    // Warmwasser-Freigabe mit um. Rueckgelesen ueber TOP59 Room_Heater_State
    // (getBit7and8) und TOP58 DHW_Heater_State (getBit5and6), Klartext
    // Blocked/Free; die Kodierung ist aus genau diesen Dekodierern
    // zurueckgerechnet:
    //   RoomHeaterState 0 -> (0+1)*1 = 0b0001, gelesen (0b01 & 0b11) - 1   = 0
    //   RoomHeaterState 1 -> (1+1)*1 = 0b0010, gelesen (0b10 & 0b11) - 1   = 1
    //   DHWHeaterState  0 -> (0+1)*4 = 0b0100, gelesen ((4>>2) & 0b11) - 1 = 0
    //   DHWHeaterState  1 -> (1+1)*4 = 0b1000, gelesen ((8>>2) & 0b11) - 1 = 1
    //
    // FREI HEISST NICHT AN. Die Freigabe ist Bedingung (a) von sechs, die
    // uebrigen fuenf entscheidet die Waermepumpe selbst: 30 min seit Kompressor
    // thermo-on, 9 min seit Pumpenstart, Aussentemperatur unter SET20
    // HeaterOnOutdoorTemp, Vorlauf 4 K unter Soll, 20 min seit dem letzten
    // Heizstab-Aus. Eine Steuerung muss deshalb vorausschauend freigeben;
    // schnelles Ein/Aus bringt nichts. Ausfuehrlich in Vorhaben-HeaterSet.md.
    {37,  9, 0x03, CONV_MUL_INC, "RoomHeaterState",               0,   1,   1}, // blockiert=1 frei=2
    {38,  9, 0x0C, CONV_MUL_INC, "DHWHeaterState",                0,   1,   4}, // blockiert=4 frei=8
    // ForceHeater ist ein ZUSTAND, kein Impuls wie SET12 ForceDefrost - wer ihn
    // setzt, muss ihn auch zuruecknehmen. UND ER STARTET DIE UMWAELZPUMPE: am
    // 2026-08-28 gemessen laeuft sie an, sobald TOP68 aktiv wird, laeuft weiter,
    // nachdem der Heizstab abgeschaltet hat, und stoppt erst mit ForceHeater 0.
    // Ein vergessenes Kommando laesst also die Pumpe dauerhaft laufen. Byte 5
    // teilt er sich mit SET2
    // HolidayMode (0x30), rueckgelesen ueber TOP68 Force_Heater_State
    // (Inactive/Active). Das Servicehandbuch (12.9) beschreibt ihn als
    // Ersatzwaermequelle bei einer Stoerung der Waermepumpe; am 2026-08-28 am
    // Bedienpanel gegengeprueft: bei AUSGESCHALTETER Waermepumpe laesst er sich
    // auch ohne anliegende Stoerung einschalten, bei laufendem Betrieb lehnt das
    // Bedienteil die Anforderung ab.
    {39,  5, 0x0C, CONV_MUL_INC, "ForceHeater",                   0,   1,   4}, // aus=4 an=8
};

static const unsigned int SETCOMMANDCOUNT = sizeof(setCommands) / sizeof(setCommands[0]);

// bits already claimed in the pending command, one entry per protocol byte.
// Only used to detect two fields fighting over the same bits - cleared
// together with mainCommand after each send (see send_pana_command).
byte usedMask[QUERYSIZE];

/*****************************************************************************/
/* Alle Set-Topics der Tabelle abonnieren                                    */
/*                                                                           */
/* Einzige Stelle, an der abonniert wird - die Liste kann damit nicht mehr   */
/* hinter der Tabelle zurueckbleiben. Rueckgabe: false, wenn mindestens ein  */
/* subscribe fehlgeschlagen ist (PubSubClient scheitert z. B., wenn das      */
/* Paket nicht in seinen Puffer passt); der Aufrufer sieht das im Log.       */
/*****************************************************************************/
bool subscribe_set_topics(PubSubClient &mqtt_client)
{
  char log_line[128];
  char full_topic[128];
  bool all_ok = true;

  for (unsigned int i = 0; i < SETCOMMANDCOUNT; i++)
  {
    const SetCommand &cmd = setCommands[i];
    (void)snprintf(full_topic, sizeof(full_topic), "%s/%s", Topics::SET.c_str(), cmd.name);
    if (!mqtt_client.subscribe(full_topic))
    {
      (void)snprintf(log_line, sizeof(log_line), "Error: subscribe failed for SET%u (%s)", cmd.number, cmd.name);
      write_mqtt_log(log_line);
      all_ok = false;
    }
  }
  return all_ok;
}

/*****************************************************************************/
/* Bereichsgrenzen eines Set-Kommandos nachschlagen                          */
/*                                                                           */
/* setCommands[] ist und bleibt die einzige Stelle, an der die erlaubten     */
/* Wertebereiche stehen. Der Notbetrieb haelt Werte unter denselben Namen im */
/* RAM und prueft sie beim Annehmen gegen genau diese Grenzen - eine zweite  */
/* Tabelle waere eine zweite Wahrheit.                                        */
/*****************************************************************************/
bool set_command_range(const char *name, int *min_out, int *max_out)
{
  if (!name || !min_out || !max_out)
    return false;

  for (unsigned int i = 0; i < SETCOMMANDCOUNT; i++)
  {
    if (strcmp(name, setCommands[i].name) == 0)
    {
      *min_out = setCommands[i].min;
      *max_out = setCommands[i].max;
      return true;
    }
  }
  return false;
}

/*****************************************************************************/
/* Build the heatpump command from an mqtt set message                       */
/* returns true if a command was registered, false on any error              */
/* (caller must restart the mainquery timer on false, see mqtt_callback)     */
/*****************************************************************************/
bool build_heatpump_command(char *topic, char *msg)
{
  char log_line[256];

  // parse and validate the payload as integer
  char *endptr;
  long msg_long = strtol(msg, &endptr, 10);
  if (endptr == msg || *endptr != '\0')
  {
    // Laengenbegrenzung im Format ist Pflicht, nicht Kosmetik: msg kommt vom
    // Broker und darf bis ~200 Zeichen lang sein. Mit sprintf und ohne %.32s
    // lief diese Meldung ueber den 256-Byte-Puffer hinaus (bis 303 Bytes) und
    // damit in den Stack - ein langer, nicht numerischer Payload auf einem
    // set-Topic konnte das Geraet abstuerzen lassen. 32 Zeichen genuegen, um
    // zu sehen, was da ankam, und die Meldung passt in ein MQTT-Paket.
    (void)snprintf(log_line, sizeof(log_line), "Error: Invalid integer value '%.32s' received for topic %.64s", msg, topic);
    write_mqtt_log(log_line);
    return false;
  }

  // Pfadwurzel "<prefix>/set/" abschneiden, danach wird nur noch der kurze
  // Name mit der Tabelle verglichen - so, wie er dort steht.
  // strncmp stoppt am Stringende, ein zu kurzes Topic kann also nicht
  // ueberlesen werden; topic[prefixlen] ist nach einem Treffer immer gueltig.
  const size_t prefixlen = Topics::SET.length();
  if ((strncmp(topic, Topics::SET.c_str(), prefixlen) != 0) || (topic[prefixlen] != '/'))
  {
    (void)snprintf(log_line, sizeof(log_line), "Error: Unknown set topic %.64s", topic);
    write_mqtt_log(log_line);
    return false;
  }
  const char *topic_name = topic + prefixlen + 1;

  // find the matching set topic in the table
  for (unsigned int i = 0; i < SETCOMMANDCOUNT; i++)
  {
    const SetCommand &cmd = setCommands[i];
    if (strcmp(cmd.name, topic_name) != 0)
    {
      continue;
    }

    // range check before touching mainCommand
    if (msg_long < cmd.min || msg_long > cmd.max)
    {
      (void)snprintf(log_line, sizeof(log_line), "Error: Value %ld out of range [%d..%d] for topic %s", msg_long, cmd.min, cmd.max, cmd.name);
      write_mqtt_log(log_line);
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
      (void)snprintf(log_line, sizeof(log_line), "Warning: Field conflict in byte %d (%s) - last value wins", cmd.pos, cmd.name);
      write_mqtt_log(log_line);
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

    (void)snprintf(log_line, sizeof(log_line), "<SUB> SET%d %s: %ld", cmd.number, cmd.name, msg_long);
    write_mqtt_log(log_line);
    // trigger buffer
    register_new_command();
    return true;
  }

  // no table entry matched
  (void)snprintf(log_line, sizeof(log_line), "Error: Unknown set topic %.64s", topic);
  write_mqtt_log(log_line);
  return false;
}
