#include "HeishaMon.h"
#include "commands.h"
#include "Topics.h"

/*****************************************************************************/
/* Notbetrieb - die Firmware-Anbindung                                       */
/*                                                                           */
/* Die Regeln stehen in notbetrieb.h und laufen dort arduino-frei, damit der */
/* Hosttest sie mituebersetzt. Hier steht nur, was ohne Arduino nicht geht:   */
/* Abonnement, Payload-Auswertung und der globale Zustand.                    */
/*****************************************************************************/

/*****************************************************************************/
/* Die Rolle dieser Stufe kommt aus einem Build-Flag                         */
/*                                                                           */
/* Eine gemeinsame Firmware fuer beide Stufen gibt es ohnehin nicht, solange  */
/* der MQTT-Prefix ein Build-Flag ist (HEISHA_MQTT_PREFIX in platformio.ini). */
/* Ein eigenes Flag in denselben Abschnitten ist sauberer als ein Stringver-  */
/* gleich auf den Stufennamen und braucht kein Konfigurationsfeld auf dem     */
/* Geraet. Ohne Flag gilt Heizen - derselbe Fallback wie beim Prefix.         */
/*****************************************************************************/
#ifdef NOTBETRIEB_ROLLE_WASSER
const NotbetriebRolle notbetriebRolle = NOTBETRIEB_WASSER;
#else
const NotbetriebRolle notbetriebRolle = NOTBETRIEB_HEIZEN;
#endif

// Die gehaltenen Werte und der laufende Schaltvorgang. Beides im RAM, bewusst
// ohne Datei auf LittleFS - Begruendung im Kopf von notbetrieb.h.
NotbetriebSpeicher notbetriebWerte;
NotbetriebLauf notbetriebLauf;

/*****************************************************************************/
/* Beim Start einmal leeren                                                  */
/*                                                                           */
/* Ohne das stuenden nach dem Boot die Bitmuster im RAM, die der Reset dort   */
/* zufaellig hinterlassen hat - und der Knopf haette sich fuer freigegeben    */
/* gehalten, ohne dass je ein Wert eingetroffen waere.                        */
/*****************************************************************************/
void notbetrieb_init(void)
{
  notbetrieb_speicher_leeren(&notbetriebWerte);
  notbetrieb_lauf_leeren(&notbetriebLauf);
}

/*****************************************************************************/
/* Den Notbetriebszweig abonnieren                                           */
/*                                                                           */
/* Nur die Namen der eigenen Rolle - Stufe 2 abonniert die Kurvenwerte gar    */
/* nicht erst. Ein Wildcard-Abonnement auf <prefix>/notbetrieb/# waere        */
/* kuerzer, wuerde aber jeden Tippfehler im Flow stillschweigend annehmen und */
/* erst beim Druck auf den Knopf auffallen.                                   */
/*****************************************************************************/
bool notbetrieb_subscribe(PubSubClient &mqtt_client)
{
  char full_topic[128];
  char log_line[128];
  bool all_ok = true;

  const unsigned n = notbetrieb_wert_anzahl(notbetriebRolle);

  // Die Rolle einmal je Verbindung ins Log: Sie kommt aus einem Build-Flag und
  // ist am Geraet sonst nirgends abzulesen. Wer eine Stufe mit der falschen
  // Firmware flasht, sieht es hier statt erst beim Druck auf den Knopf.
  (void)snprintf(log_line, sizeof(log_line), "Notbetrieb: Rolle %s, %u Werte erwartet",
                 (notbetriebRolle == NOTBETRIEB_WASSER) ? "Warmwasser" : "Heizen", n);
  write_mqtt_log(log_line);

  for (unsigned i = 0; i < n; i++)
  {
    const char *name = notbetrieb_wert_name(notbetriebRolle, i);
    (void)snprintf(full_topic, sizeof(full_topic), "%s/%s", Topics::NOTBETRIEB.c_str(), name);
    if (!mqtt_client.subscribe(full_topic))
    {
      (void)snprintf(log_line, sizeof(log_line), "Error: subscribe failed for notbetrieb/%s", name);
      write_mqtt_log(log_line);
      all_ok = false;
    }
  }
  return all_ok;
}

/*****************************************************************************/
/* Eine Nachricht aus dem Notbetriebszweig annehmen                          */
/*                                                                           */
/* Rueckgabe: true, wenn das Topic in diesen Zweig gehoerte - dann ist die    */
/* Nachricht hier abschliessend behandelt und darf NICHT weiter in den        */
/* Set-Pfad laufen. Ob der Wert dabei uebernommen oder verworfen wurde,       */
/* aendert daran nichts: ein abgelehnter Notbetriebswert ist kein             */
/* Set-Kommando, das noch jemand ausfuehren muesste.                          */
/*                                                                           */
/* Der Abfragezyklus wird hier bewusst NICHT angehalten (kein                 */
/* Send_Pana_Mainquery_Timer.stop()): Diese Werte gehen nicht an die          */
/* Waermepumpe, es gibt also nichts zu senden und nichts zu verzoegern.       */
/*****************************************************************************/
bool notbetrieb_mqtt_annehmen(const char *topic, const char *msg)
{
  const char *name = notbetrieb_name_aus_topic(topic, Topics::NOTBETRIEB.c_str());
  if (!name)
    return false; // gehoert nicht hierher - normal weiterverarbeiten

  char log_line[160];

  // Payload als Ganzzahl auswerten, so streng wie im Set-Pfad: nachlaufende
  // Zeichen machen die ganze Nachricht ungueltig, statt stillschweigend eine
  // fuehrende Zahl zu uebernehmen.
  char *endptr;
  long wert = strtol(msg, &endptr, 10);
  if (endptr == msg || *endptr != '\0')
  {
    (void)snprintf(log_line, sizeof(log_line),
                   "Notbetrieb: %.32s ist keine Zahl (%.32s) - verworfen", name, msg);
    write_mqtt_log(log_line);
    return true;
  }

  // Grenzen aus setCommands[] - eine Quelle, siehe set_command_range()
  //
  // Am Geraet ist dieser Zweig nicht erreichbar, solange die Tabellen stimmen:
  // Abonniert werden nur Namen aus NOTBETRIEB_WERTE_*, und die stehen alle in
  // setCommands[]. Er greift, wenn dort jemand einen Namen eintraegt, den es
  // als Set-Kommando nicht gibt - ein Fehler bei der Weiterentwicklung, der
  // sonst erst beim Druck auf den Knopf auffiele.
  int min_wert = 0, max_wert = 0;
  if (!set_command_range(name, &min_wert, &max_wert))
  {
    (void)snprintf(log_line, sizeof(log_line),
                   "Notbetrieb: %.32s ist kein bekanntes Set-Kommando - verworfen", name);
    write_mqtt_log(log_line);
    return true;
  }

  // War der Satz vor dieser Nachricht schon vollstaendig? Der Uebergang ist
  // das meldenswerte Ereignis, nicht der Zustand - sonst stuende die Meldung
  // nach jedem Reconnect mehrfach im Log.
  const bool vorher_vollstaendig = notbetrieb_vollstaendig(&notbetriebWerte, notbetriebRolle);

  // Ausserhalb der Grenzen wird VERWORFEN, nicht geklemmt: ein still
  // zurechtgebogener Kurvenpunkt faellt im Notfall niemandem auf.
  if (!notbetrieb_wert_annehmen(&notbetriebWerte, notbetriebRolle, name,
                                (int)wert, min_wert, max_wert))
  {
    (void)snprintf(log_line, sizeof(log_line),
                   "Notbetrieb: %.32s = %ld abgelehnt (erlaubt %d..%d)",
                   name, wert, min_wert, max_wert);
    write_mqtt_log(log_line);
    return true;
  }

  // Der einzelne Wert laeuft nur ins Telnet-Log: Nach jedem Reconnect spielt
  // der Broker alle Werte erneut ein, das MQTT-Log soll davon nicht volllaufen.
  (void)snprintf(log_line, sizeof(log_line), "Notbetrieb gemerkt: %.32s = %ld", name, wert);
  write_telnet_log(log_line);

  // Dass der Satz JETZT vollstaendig ist, gehoert dagegen ins MQTT-Log: Es ist
  // das Ereignis, das den Knopf scharf macht, es tritt selten auf, und wer
  // wissen will, ob der Notbetrieb einsatzbereit waere, findet es dort - ohne
  // sich per Telnet auf das Geraet zu legen.
  if (!vorher_vollstaendig && notbetrieb_vollstaendig(&notbetriebWerte, notbetriebRolle))
  {
    (void)snprintf(log_line, sizeof(log_line),
                   "Notbetrieb einsatzbereit: alle %u Werte liegen vor",
                   notbetrieb_wert_anzahl(notbetriebRolle));
    write_mqtt_log(log_line);
  }
  return true;
}
