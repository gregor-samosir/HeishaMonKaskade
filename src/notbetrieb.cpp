#include "HeishaMon.h"
#include "commands.h"
#include "Topics.h"
#include "decode.h" // state_topic_index()

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
/* Der zwischengespeicherte Sperrgrund                                       */
/*                                                                           */
/* Er haengt an TOP101 und damit an actual_data[] - das haben die Webhandler  */
/* nicht zur Hand, sie bekommen nur den httpServer. Statt actual_data durch   */
/* drei Handler und zwei Routen zu reichen, rechnet ihn notbetrieb_loop()     */
/* einmal je Durchlauf aus. loop() laeuft im Millisekundentakt; der Wert ist  */
/* also praktisch immer aktuell.                                              */
/*                                                                           */
/* Startwert ist BEWUSST "gesperrt": Zwischen dem Booten und dem ersten       */
/* Durchlauf darf der Knopf nicht offenstehen. Zu diesem Zeitpunkt hat weder  */
/* der Broker Werte geliefert noch die Waermepumpe geantwortet.               */
/*****************************************************************************/
static NotbetriebSperre notbetriebSperre = NOTBETRIEB_SPERRE_WERTE;

NotbetriebSperre notbetrieb_sperre(void)
{
  return notbetriebSperre;
}

/*****************************************************************************/
/* Sieht die gehaltene Kurve plausibel aus?                                  */
/*                                                                           */
/* Anders als die Sperre wird das NICHT zwischengespeichert: Die Pruefung ist */
/* ein Vergleich von vier ints, die Statusroute fragt sie alle zwei Sekunden  */
/* ab, und ein zweiter Zustand waere ein zweiter Ort, der veralten kann.      */
/*****************************************************************************/
NotbetriebKurvenWarnung notbetrieb_kurvenwarnung(void)
{
  return notbetrieb_kurve_pruefen(&notbetriebWerte, notbetriebRolle);
}

/*****************************************************************************/
/* Der Rohtext von TOP101 (Heat_Cool_SW_State) aus actual_data[]             */
/*                                                                           */
/* Leerer Text heisst "nie empfangen" und gilt als NICHT Heizen - siehe die   */
/* Freigaberegel in notbetrieb.h. Der Umweg ueber state_topic_index() ist     */
/* Pflicht: actual_data[] wird ueber den ZEILENINDEX adressiert, nicht ueber  */
/* die TOP-Nummer.                                                            */
/*****************************************************************************/
static int heizKuehlIndex = -1; // einmal in notbetrieb_init() nachgeschlagen

static const char *heiz_kuehl_text(char actual[][MAXVALUELEN])
{
  // Der Zeilenindex steht fest, sobald die Firmware laeuft. Ihn hier jedes Mal
  // neu zu suchen hiesse, bei JEDEM Durchlauf von loop() linear ueber 92 Zeilen
  // zu gehen - der Sperrgrund wird ja nicht mehr nur waehrend eines Laufs
  // gebraucht, sondern staendig.
  return (heizKuehlIndex >= 0) ? actual[heizKuehlIndex] : "";
}

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

  // Einmal pruefen, ob jeder Schritt sein Ruecklese-TOP ueberhaupt findet. Ein
  // Zahlendreher in der Schrittfolge faellt sonst erst im Ernstfall auf: Der
  // Schritt wuerde nie bestaetigt, der Knopf meldete nach 20 s ROT, und der
  // Grund waere nirgends zu sehen.
  for (unsigned i = 0; i < notbetrieb_schritt_anzahl(notbetriebRolle); i++)
  {
    const NotbetriebSchritt *s = notbetrieb_schritt(notbetriebRolle, i);
    if (s && state_topic_index((unsigned)s->top) < 0)
    {
      char log_line[128];
      (void)snprintf(log_line, sizeof(log_line),
                     "FEHLER Notbetrieb: TOP%d (%.32s) fehlt in stateTopics[]",
                     s->top, s->set_name);
      Serial.println(log_line); // MQTT laeuft in setup() noch nicht
    }
  }

  // Dasselbe fuer die Freigabebedingung: Faende TOP101 seine Zeile nicht, waere
  // der Knopf der Rolle Heizen dauerhaft gesperrt, ohne dass irgendwo stuende,
  // warum. Fuer Warmwasser spielt TOP101 keine Rolle (M3).
  heizKuehlIndex = state_topic_index(NOTBETRIEB_TOP_HEIZ_KUEHL);
  if (notbetriebRolle != NOTBETRIEB_WASSER && heizKuehlIndex < 0)
  {
    Serial.println("FEHLER Notbetrieb: TOP101 (Heat_Cool_SW_State) fehlt in stateTopics[]");
  }
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

  // Die Kurve wird nach JEDER Aenderung neu beurteilt, nicht nur beim
  // Vollstaendigwerden: Ein spaeter nachgereichter Wert kann eine plausible
  // Kurve verdrehen. Gemeldet wird nur der WECHSEL - nach jedem Reconnect
  // spielt der Broker alle Werte erneut ein, und eine Zeile je Wert wuerde das
  // MQTT-Log fluten. Die Zahlen stehen mit in der Meldung: Wer sie liest, soll
  // nicht erst die vier Topics nachschlagen muessen.
  static NotbetriebKurvenWarnung letzte_warnung = NOTBETRIEB_KURVE_OK;
  const NotbetriebKurvenWarnung warnung =
      notbetrieb_kurve_pruefen(&notbetriebWerte, notbetriebRolle);
  if (warnung != letzte_warnung)
  {
  if (warnung == NOTBETRIEB_KURVE_VORLAUF_VERDREHT)
  {
    (void)snprintf(log_line, sizeof(log_line),
                     "Notbetrieb: Kurve prueft nicht - VL kalt %d liegt unter VL warm %d",
                     notbetriebWerte.werte[0], notbetriebWerte.werte[1]);
  }
  else if (warnung == NOTBETRIEB_KURVE_AUSSEN_VERDREHT)
  {
    (void)snprintf(log_line, sizeof(log_line),
                     "Notbetrieb: Kurve prueft nicht - AT kalt %d liegt nicht unter AT warm %d",
                     notbetriebWerte.werte[2], notbetriebWerte.werte[3]);
  }
  else
  {
    (void)snprintf(log_line, sizeof(log_line), "Notbetrieb: Kurve wieder plausibel");
  }
  write_mqtt_log(log_line);
  letzte_warnung = warnung;
  }
  return true;
}

/*****************************************************************************/
/* Den aktuellen Schritt absetzen                                            */
/*                                                                           */
/* Die Kommandos gehen durch build_heatpump_command() - UNVERAENDERT. Der    */
/* Handler baut nur den Topic-String. Damit bleiben Bereichspruefung,        */
/* Maskenmerge, Konfliktdiagnose, Log und register_new_command() genau ein   */
/* Mal im Code. Die Alternative - ein vorgefertigtes 110-Byte-Kommando       */
/* unterschieben - ist verworfen: Sie umgeht alle vier Pruefungen und        */
/* ueberschreibt zusaetzlich jedes andere Feld, das in einem gerade offenen  */
/* Sammelfenster steht.                                                      */
/*****************************************************************************/
static bool notbetrieb_schritt_absetzen(void)
{
  const NotbetriebSchritt *s = notbetrieb_schritt(notbetriebRolle, notbetriebLauf.schritt);
  if (!s)
    return false;

  int wert = 0;
  if (!notbetrieb_schritt_wert(&notbetriebLauf, notbetriebRolle, &notbetriebWerte, &wert))
    return false; // fehlender gehaltener Wert - darf nicht gesendet werden

  // build_heatpump_command() nimmt char*, nicht const char* - eigene Puffer
  char topic[128];
  char nutzlast[16];
  (void)snprintf(topic, sizeof(topic), "%s/%s", Topics::SET.c_str(), s->set_name);
  (void)snprintf(nutzlast, sizeof(nutzlast), "%d", wert);

  char log_line[160];
  (void)snprintf(log_line, sizeof(log_line), "Notbetrieb Schritt %u/%u: %.32s = %d",
                   (unsigned)(notbetriebLauf.schritt + 1),
                   notbetrieb_schritt_anzahl(notbetriebRolle), s->set_name, wert);
  write_mqtt_log(log_line);

  return build_heatpump_command(topic, nutzlast);
}

/*****************************************************************************/
/* Der Tick aus loop()                                                       */
/*                                                                           */
/* Die Schritte laufen EINZELN, nicht in einem Sammelfenster: Ein Handler,   */
/* der sechs Kommandos hintereinander absetzt, packt sie alle in EIN         */
/* Telegramm (Deckel 2 s, sendwindow.h) - dann konkurriert das Kurven-       */
/* schreiben mit dem Werks-Reset des Moduswechsels, und welcher gewinnt, ist */
/* unbekannt.                                                                */
/*****************************************************************************/
void notbetrieb_loop(char actual[][MAXVALUELEN])
{
  // Der Rohtext von TOP101 traegt beides: die Freigabe vor dem Start und den
  // Abbruch mitten im Lauf.
  const char *richtung = heiz_kuehl_text(actual);

  // Der Sperrgrund wird JEDEN Durchlauf neu bestimmt, auch waehrend ein Lauf
  // unterwegs ist - die Seite fragt ihn alle zwei Sekunden ab und gibt den
  // Knopf von selbst frei, sobald der KNX-Schalter auf Heizen steht. Ohne das
  // muesste jemand die Seite neu laden und wuesste nicht, wann.
  notbetriebSperre = notbetrieb_sperrgrund(notbetriebRolle, &notbetriebWerte, richtung);

  // Ein Ergebnis von gestern ist keine Auskunft mehr: GRUEN und ROT fallen
  // nach 15 Minuten auf BEREIT zurueck, der Knopf steht dann wieder da.
  if (notbetrieb_verfall_pruefen(&notbetriebLauf, millis()))
    write_telnet_log((char *)"Notbetrieb: Anzeige zurueckgesetzt (15 min)");

  if (notbetriebLauf.zustand != NOTBETRIEB_LAEUFT)
    return;

  const NotbetriebSchritt *s = notbetrieb_schritt(notbetriebRolle, notbetriebLauf.schritt);
  if (!s)
  {
    // kann nur ein Tabellenfehler sein
    notbetrieb_abschluss(&notbetriebLauf, NOTBETRIEB_ROT, millis());
    return;
  }

  // Sollwert des laufenden Schritts. Fehlt er, ist der Lauf nicht zu retten -
  // die Seite laesst ihn dann gar nicht erst starten, dies ist der Notnagel.
  int soll = 0;
  if (!notbetrieb_schritt_wert(&notbetriebLauf, notbetriebRolle, &notbetriebWerte, &soll))
  {
    notbetrieb_abschluss(&notbetriebLauf, NOTBETRIEB_ROT, millis());
    write_mqtt_log((char *)"Notbetrieb abgebrochen: ein Wert fehlt");
    return;
  }

  // GRUEN heisst zurueckgelesen, nicht abgesendet: Der TOP des Schritts muss
  // den Sollwert tragen. notbetrieb_rueckgelesen() faengt dabei den leeren
  // Rueckgabewert ab - sonst bestaetigte ein nie empfangenes TOP jeden
  // Schritt mit dem Sollwert 0.
  // ACHTUNG: s->top ist die TOP-NUMMER, actual_data[] wird ueber den
  // ZEILENINDEX adressiert. Beides ist nicht dasselbe - die Nummerierung hat
  // Luecken (Zone 2 entfiel in 3.4.0) und reicht bis 104 bei 92 Zeilen. Wer
  // hier direkt mit der Nummer indiziert, liest die falsche Zeile.
  const int index = (s->top >= 0) ? state_topic_index((unsigned)s->top) : -1;
  const bool bestaetigt = (index >= 0) ? notbetrieb_rueckgelesen(actual[index], soll) : false;

  char log_line[160];
  switch (notbetrieb_tick(&notbetriebLauf, notbetriebRolle, millis(), bestaetigt, richtung))
  {
  case NOTBETRIEB_SENDEN:
    if (!notbetrieb_schritt_absetzen())
    {
      notbetrieb_abschluss(&notbetriebLauf, NOTBETRIEB_ROT, millis());
      write_mqtt_log((char *)"Notbetrieb abgebrochen: Kommando abgelehnt");
    }
    break;

  case NOTBETRIEB_FERTIG:
    write_mqtt_log((char *)"Notbetrieb GRUEN: alle Schritte zurueckgelesen");
    break;

  // Der KNX-Schalter ist waehrend des Laufs auf Kuehlen gegangen (oder stand
  // nie auf Heizen). Eigene Meldung, weil der Grund ein voellig anderer ist
  // als ein ausbleibender Ruecklesewert - und weil nur hier der Weg zurueck
  // ueber den Schalter fuehrt, nicht ueber die Firmware.
  case NOTBETRIEB_ABBRUCH_KUEHLEN:
    (void)snprintf(log_line, sizeof(log_line),
                       "Notbetrieb ROT: Anlage meldet Kuehlbetrieb (TOP101), Abbruch in Schritt %u/%u",
                       (unsigned)(notbetriebLauf.schritt + 1),
                       notbetrieb_schritt_anzahl(notbetriebRolle));
    write_mqtt_log(log_line);
    break;

  case NOTBETRIEB_ABBRUCH:
    (void)snprintf(log_line, sizeof(log_line),
                       "Notbetrieb ROT: Schritt %u/%u (%.32s) kam nicht zurueck",
                       (unsigned)(notbetriebLauf.schritt + 1),
                       notbetrieb_schritt_anzahl(notbetriebRolle), s->set_name);
    write_mqtt_log(log_line);
    break;

  default:
    break; // laeuft noch
  }
}

/*****************************************************************************/
/* Lauf anstossen - vom Webhandler aufgerufen                                */
/*                                                                           */
/* Der HTTP-Handler stoesst nur an und antwortet sofort; das erste Kommando  */
/* geht hier raus, alles Weitere macht der Tick aus loop().                  */
/*                                                                           */
/* Die Sperre wird HIER geprueft und nicht nur beim Aufbau der Seite: Ein    */
/* POST auf /notbetrieb/start laesst sich auch ohne die Seite absetzen, und  */
/* zwischen dem Aufbau der Seite und dem Klick koennen Minuten liegen. Eine  */
/* Oberflaeche, die nur den Knopf versteckt, ist keine Sperre.                */
/*****************************************************************************/
bool notbetrieb_starten(void)
{
  const NotbetriebSperre sperre = notbetrieb_sperre();
  if (sperre != NOTBETRIEB_FREI)
  {
    write_mqtt_log(sperre == NOTBETRIEB_SPERRE_HEIZBETRIEB
                       ? (char *)"Notbetrieb abgelehnt: die Anlage steht nicht auf Heizen (TOP101)"
                       : (char *)"Notbetrieb abgelehnt: es fehlen Werte");
    return false;
  }
  if (notbetriebLauf.zustand == NOTBETRIEB_LAEUFT)
    return false;

  if (notbetrieb_start(&notbetriebLauf, millis()) != NOTBETRIEB_SENDEN)
    return false;

  write_mqtt_log((char *)"NOTBETRIEB ausgeloest ueber die Weboberflaeche");
  if (!notbetrieb_schritt_absetzen())
  {
    notbetrieb_abschluss(&notbetriebLauf, NOTBETRIEB_ROT, millis());
    write_mqtt_log((char *)"Notbetrieb abgebrochen: erstes Kommando abgelehnt");
    return false;
  }
  return true;
}

/*****************************************************************************/
/* Kurzstatus fuer die Statusroute: Zustand;Schritt;Schritte;fehlend;Sperre  */
/*                                                                           */
/* Bewusst maschinenlesbar und kurz - die Seite fragt ihn alle zwei Sekunden */
/* ab und macht daraus Klartext. Je kuerzer die Antwort, desto weniger       */
/* Arbeit fuer einen ESP8266, der nebenher die Waermepumpe abfragt.          */
/*                                                                           */
/* Das fuenfte Feld ist der Sperrgrund. Mit ihm gibt die Seite den Knopf von */
/* selbst frei, sobald der KNX-Schalter auf Heizen steht - ohne dass jemand  */
/* neu laden muss. Das ist keine Bequemlichkeit: TOP101 folgt dem Schalter   */
/* erst nach bis zu 7,7 s (gemessen 2026-08-16). Wer sofort neu laedt, saehe */
/* noch einmal "nur im Modus Heizen" und hielte den Schalter fuer kaputt.    */
/*****************************************************************************/
void notbetrieb_status(char *out, size_t len)
{
  if (!out || len == 0)
    return;

  // fehlende Werte als Bitmaske - die Seite nennt sie beim Namen
  unsigned fehlend = 0;
  const unsigned n = notbetrieb_wert_anzahl(notbetriebRolle);
  for (unsigned i = 0; i < n; i++)
  {
    if ((notbetriebWerte.gesetzt & (1u << i)) == 0)
      fehlend |= (1u << i);
  }

  (void)snprintf(out, len, "%u;%u;%u;%u;%u",
                   (unsigned)notbetriebLauf.zustand,
                   (unsigned)(notbetriebLauf.schritt + 1),
                   notbetrieb_schritt_anzahl(notbetriebRolle),
                   fehlend,
                   (unsigned)notbetriebSperre);
}
