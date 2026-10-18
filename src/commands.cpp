#include "HeishaMon.h"
#include "commands.h"
#include "setcommands.h" // Tabelle, Umrechnung, Maske (3.25.1)
#include "Topics.h"

/*****************************************************************************/
/* Set-Kommandos: Anbindung an MQTT                                          */
/*                                                                           */
/* Die Tabelle setCommands[], die Umrechnung, die Maske und die              */
/* Konfliktpruefung stehen seit 3.25.1 in setcommands.h (arduino-frei, vom   */
/* Hosttest direkt eingebunden). Hier bleibt, was an MQTT, Logring und       */
/* Firmware haengt: abonnieren, Payload annehmen, Logzeilen.                 */
/*****************************************************************************/

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

  const SetCommand *cmd = setcommand_finden(name);
  if (!cmd)
    return false;
  *min_out = cmd->min;
  *max_out = cmd->max;
  return true;
}

/*****************************************************************************/
/* Build the heatpump command from an mqtt set message                       */
/* returns true if a command was registered, false on any error              */
/* (caller must restart the mainquery timer on false, see mqtt_callback)     */
/*****************************************************************************/
bool build_heatpump_command(char *topic, char *msg)
{
  char log_line[256];

  // parse and validate the payload as integer. Die Regel steht in
  // setpayload.h (testbar, Hosttest); sie ist die des bisherigen strtol.
  int32_t msg_long = 0;
  if (setpayload_ganzzahl(msg, &msg_long) != SETPAYLOAD_OK)
  {
    // Laengenbegrenzung im Format ist Pflicht, nicht Kosmetik: msg kommt vom
    // Broker und darf bis ~200 Zeichen lang sein. Mit sprintf und ohne %.32s
    // lief diese Meldung ueber den 256-Byte-Puffer hinaus (bis 303 Bytes) und
    // damit in den Stack - ein langer, nicht numerischer Payload auf einem
    // set-Topic konnte das Geraet abstuerzen lassen. 32 Zeichen genuegen, um
    // zu sehen, was da ankam, und die Meldung passt in ein MQTT-Paket.
    (void)snprintf(log_line, sizeof(log_line), "Error: Invalid integer value '%.32s' received for topic %.64s", msg, topic);
    // 3.25.0: dauerhaft sichtbar (info/set_abgelehnt). IMMER VOR write_mqtt_log():
    // Das veroeffentlicht mitten im MQTT-Callback und ueberschreibt dabei den
    // Puffer des PubSubClient, auf den topic und msg zeigen. Gebucht wurde in
    // der ersten Fassung danach - der Pruefling zeigte "??=33.5" statt des
    // Namens. Kein Hosttest kann das finden, der Puffer gehoert der Bibliothek.
    setabgelehnt_merken(&setAbgelehnt, topic, msg);
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
    setabgelehnt_merken(&setAbgelehnt, topic, msg);
    write_mqtt_log(log_line);
    return false;
  }
  const char *topic_name = topic + prefixlen + 1;

  // find the matching set topic in the table
  const SetCommand *found = setcommand_finden(topic_name);
  if (found)
  {
    const SetCommand &cmd = *found;

    // range check before touching mainCommand
    if (!setcommand_wert_ok(cmd, msg_long))
    {
      (void)snprintf(log_line, sizeof(log_line), "Error: Value %ld out of range [%d..%d] for topic %s", (long)msg_long, cmd.min, cmd.max, cmd.name);
      setabgelehnt_merken(&setAbgelehnt, topic, msg);
      write_mqtt_log(log_line);
      return false;
    }

    // convert the validated value and insert it bit-exact: other fields
    // sharing the byte survive, a repeated set overwrites its own previous
    // value instead of OR-ing on top of it. Conflict diagnosis: two fields
    // claiming the same bits before the buffer was sent must never happen
    // silently - log it, last value wins. Die Regeln stehen in setcommands.h.
    const SetAnwendung anwendung = setcommand_anwenden(cmd, msg_long, mainCommand, usedMask);
    if (anwendung == SETANWENDEN_ABGELEHNT)
    {
      // nach wert_ok() nicht erreichbar (pos steht in der Tabelle unter static_assert);
      // trotzdem sauber behandeln statt ein leeres Kommando zu melden
      (void)snprintf(log_line, sizeof(log_line), "Error: Command %s could not be applied", cmd.name);
      setabgelehnt_merken(&setAbgelehnt, topic, msg);
      write_mqtt_log(log_line);
      return false;
    }
    if (anwendung == SETANWENDEN_KONFLIKT)
    {
      (void)snprintf(log_line, sizeof(log_line), "Warning: Field conflict in byte %d (%s) - last value wins", cmd.pos, cmd.name);
      write_mqtt_log(log_line);
    }

    // explicit flag instead of deriving "buffer is filled" from a byte sum:
    // a checksum of 0 is a valid command (e.g. ForceDefrost off) and wraps
    // at 256, which used to drop whole command telegrams silently
    setDataPending = true;

    (void)snprintf(log_line, sizeof(log_line), "<SUB> SET%d %s: %ld", cmd.number, cmd.name, (long)msg_long);
    write_mqtt_log(log_line);
    // trigger buffer
    register_new_command();
    return true;
  }

  // no table entry matched
  (void)snprintf(log_line, sizeof(log_line), "Error: Unknown set topic %.64s", topic);
  setabgelehnt_merken(&setAbgelehnt, topic, msg);
  write_mqtt_log(log_line);
  return false;
}
