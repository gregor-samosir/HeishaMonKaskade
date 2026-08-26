#include "HeishaMon.h"
#include "commands.h"
#include "Topics.h"
#include "decode.h" // state_topic_index()

// Der Hydraulikschritt spricht den Tasmota-Switch ueber HTTP an. Beide
// Plattformen bringen denselben HTTPClient mit, nur unter verschiedenen
// Kopfdateien - dieselbe Weiche wie bei den WiFi-Kopfdateien in HeishaMon.h.
#if defined(ESP32)
#include <HTTPClient.h>
#else
#include <ESP8266HTTPClient.h>
#endif

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
    // Der Hydraulikschritt liest an keinem TOP zurueck (top = -1) - er haette
    // hier sonst bei JEDEM Start eine FEHLER-Zeile ausgeloest.
    if (s && s->typ == NB_SCHRITT_SET && state_topic_index((unsigned)s->top) < 0)
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

  // Und die Adresse des Hydraulik-Switch. Fehlt sie, laesst sich der Notbetrieb
  // nicht ausloesen - das faellt sonst erst auf, wenn jemand den Knopf drueckt.
  if (hydraulik_switch[0] == '\0')
  {
    Serial.println("WARNUNG Notbetrieb: keine Adresse fuer den Hydraulik-Switch (Settings)");
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
/* Die Hydraulik auf 1-stufig stellen                                        */
/*                                                                           */
/* WARUM. Der Notbetrieb setzt hydraulisch 1-stufigen Betrieb voraus. Steht  */
/* die Hydraulik auf 2-stufig, waehrend eine Stufe im Warmwasser-Notbetrieb  */
/* laeuft, schiebt der Warmwasserbetrieb bis zu 57 C in den Heizkreis - die  */
/* Fussbodenheizung vertraegt das nicht (Owner, 2026-08-26). Im Normalbetrieb*/
/* stellt die Kaskadensteuerung den Switch; im Notbetriebsfall ist genau die */
/* weg. Deshalb macht es die Firmware, als Schritt 1 beider Schrittfolgen.   */
/*                                                                           */
/* DER SCHALTER. Ein Sonoff TH mit Tasmota, ohne Web-Passwort. EIN = 2-stufig,*/
/* AUS = 1-stufig. Er treibt zwei motorische Stellantriebe mit je 90 s        */
/* Laufzeit - das Relais ist der Startschuss der Umschaltung, nicht ihr       */
/* Ergebnis. Eine Wartezeit erzwingt das trotzdem nicht: Die Waermepumpe      */
/* braucht nach dem Einschalten rund drei Minuten bis zum Kompressor, die     */
/* Ventile sind nach 90 s durch (Owner, 2026-08-26). Im Normalbetrieb gehen   */
/* Switch- und WP-Kommandos ohnehin GLEICHZEITIG raus, seit jeher und ohne    */
/* Schaden; der Notbetrieb hat zwischen beiden sogar 16 s (Wasser) bzw. 48 s  */
/* (Heizen) Vorsprung.                                                       */
/*                                                                           */
/* WARUM DER BLOCKIERENDE REQUEST HIER ERTRAEGLICH IST. HTTPClient::GET()     */
/* blockiert loop() bis zur Antwort oder zum Timeout: kein read_pana_data,    */
/* kein timeout_serial, kein Webserver. Der UART-Puffer fasst 256 Byte, ein   */
/* Telegramm hat 203 - ein zweites ginge verloren. Genau deshalb steht der    */
/* Schritt VORN: In diesem Moment ist kein Kommando an die Waermepumpe        */
/* unterwegs und kein Sammelfenster offen. Eine einmalige Blockade trifft nur */
/* den Abfragezyklus, der ohnehin nur liest, und verschiebt ihn.              */
/*                                                                           */
/* Damit sie im Fehlerfall nicht ausufert: Timeout 1,5 s statt der            */
/* voreingestellten 5 s (der Switch haengt im selben Subnetz - antwortet er   */
/* in 1,5 s nicht, antwortet er nicht), hoechstens ein Timeout je Lauf (der   */
/* Lesevorgang bricht ab, bevor der zweite Request kommt) und KEIN            */
/* Wiederholungsversuch: Der Mensch steht vor der Seite, er drueckt erneut,   */
/* und das ist der bessere Wiederholungsversuch.                              */
/*                                                                           */
/* Eine asynchrone Loesung (AsyncTCP, eigener Task) ist verworfen: Sie braechte*/
/* Nebenlaeufigkeit in einen Zustandsautomaten, der vollstaendig aus loop()    */
/* getrieben wird, und kaufte dafuer 1,5 s in einem Fehlerfall zurueck, der   */
/* ohnehin im Abbruch endet.                                                  */
/*****************************************************************************/
#define HYDRAULIK_TIMEOUT_MS 1500

// Wie lange nach dem Antwortkopf noch auf den Rumpf gewartet wird. Er ist rund
// 15 Byte lang und liegt praktisch immer schon im Puffer; die Frist ist der
// Deckel fuer den Fall, dass er es einmal nicht tut. Sie kommt NUR im
// Erfolgsfall ueberhaupt zum Tragen - wer gar nicht antwortet, ist vorher am
// Verbindungstimeout gescheitert.
#define HYDRAULIK_LESEFRIST_MS 300u

enum HydraulikAntwort
{
  HYDRAULIK_AUS = 0,     // der Switch meldet OFF - 1-stufig, so soll es sein
  HYDRAULIK_EIN = 1,     // der Switch meldet ON - 2-stufig, muss geschaltet werden
  HYDRAULIK_UNKLAR = 2   // keine Antwort, oder eine, die niemand deuten kann
};

/*****************************************************************************/
/* Ein Tasmota-Kommando absetzen und die Antwort deuten                      */
/*                                                                           */
/* Beide gebrauchten Kommandos antworten mit demselben JSON-Feld:            */
/*   cmnd=Power       ->  {"POWER":"ON"}  oder  {"POWER":"OFF"}              */
/*   cmnd=Power%20Off ->  {"POWER":"OFF"}                                    */
/* Es genuegt, auf "OFF" bzw. "ON" zu pruefen; ein JSON-Parser wird dafuer    */
/* nicht gebraucht und waere auf einem ESP8266 der teurere Weg.               */
/*                                                                           */
/* Die Antwort wird in einen FESTEN Puffer gelesen statt in einen String:     */
/* Sie ist rund 15 Byte lang, und der Rest der Firmware kommt seit 2.3.0      */
/* ohne Heap-Allokationen in den heissen Pfaden aus.                          */
/*****************************************************************************/
static HydraulikAntwort hydraulik_kommando(const char *kommando)
{
  // Ohne Adresse gibt es nichts zu fragen. Der Aufrufer macht daraus einen
  // Abbruch mit Klartext - lieber gar kein Notbetrieb als einer, der die
  // Fussbodenheizung mit 57 C beschickt.
  if (hydraulik_switch[0] == '\0')
    return HYDRAULIK_UNKLAR;

  // Ein "http://" im Einstellungsfeld ist die naheliegende Fehlbedienung - wer
  // eine Adresse eintraegt, tippt sie oft so, wie sie im Browser steht. Es hier
  // zu ueberspringen ist eine Zeile; ohne sie entstuende "http://http://..."
  // und der Notbetrieb braeche ab, ohne dass jemand den Grund saehe.
  const char *adresse = hydraulik_switch;
  if (strncmp(adresse, "http://", 7) == 0)
    adresse += 7;
  if (adresse[0] == '\0')
    return HYDRAULIK_UNKLAR; // im Feld stand nur das Praefix

  char url[128];
  (void)snprintf(url, sizeof(url), "http://%s/cm?cmnd=%s", adresse, kommando);

  WiFiClient client;
  HTTPClient http;

  http.setReuse(false);
  http.setTimeout(HYDRAULIK_TIMEOUT_MS); // Lesetimeout
#if defined(ESP32)
  // ESP32 trennt Verbindungs- und Lesetimeout; ESP8266 deckt mit setTimeout()
  // beides ab. Ohne diese Zeile stuende die Blockade bei einem stromlosen
  // Switch auf den ESP32-Vorgabewerten statt auf 1,5 s.
  http.setConnectTimeout(HYDRAULIK_TIMEOUT_MS);
#endif

  if (!http.begin(client, url))
    return HYDRAULIK_UNKLAR;

  const int code = http.GET();
  if (code != HTTP_CODE_OK)
  {
    http.end();
    return HYDRAULIK_UNKLAR;
  }

  /***************************************************************************/
  /* Die Antwort lesen - byteweise, mit Puffergrenze und eigener Frist        */
  /*                                                                          */
  /* Drei Fallen liegen hier, und alle drei sind am 2026-08-27 am echten      */
  /* Switch (Tasmota 12.0.2) nachgemessen:                                    */
  /*                                                                          */
  /* 1. TASMOTA ANTWORTET CHUNKED, OHNE Content-Length. getSize() liefert     */
  /*    also -1, und ein Lesen "so viele Bytes wie angekuendigt" gibt es      */
  /*    nicht. Der rohe Strom traegt dafuer die Chunk-Laengen mit - fuer die  */
  /*    Suche nach "OFF"/"ON" ist das gleichgueltig, die Nutzlast steht im    */
  /*    Klartext darin.                                                       */
  /* 2. readBytes(puffer, 63) waere die naheliegende Zeile - und sie wartet,  */
  /*    bis 63 Byte da sind ODER das Timeout ablaeuft. Bei einer 15 Byte      */
  /*    langen Antwort saesse jeder Lauf die vollen 1,5 s ab, auch im         */
  /*    Erfolgsfall, und der Notbetrieb blockierte loop() dreimal so lange    */
  /*    wie noetig.                                                           */
  /* 3. getString() loest die Chunks zwar sauber auf, allokiert aber auf dem  */
  /*    Heap in der Groesse der Antwort. Zeigt die eingetragene Adresse aus   */
  /*    Versehen auf einen richtigen Webserver, waere das eine ganze          */
  /*    HTML-Seite - auf einem ESP8266 mit rund 30 kB freiem Heap genau im    */
  /*    Notfall der falsche Moment.                                           */
  /*                                                                          */
  /* Deshalb: fester Puffer, harte Frist, und Schluss, sobald die Auskunft    */
  /* dasteht. Der uebliche Fall ist nach rund 15 Byte entschieden.            */
  /***************************************************************************/
  char antwort[64] = "";
  size_t gelesen = 0;
  // Stream* statt WiFiClient*: Der konkrete Typ hinter getStreamPtr() heisst
  // auf ESP8266 und ESP32 nicht gleich (und im ESP32-Core 3.x noch einmal
  // anders). read() steht in der gemeinsamen Basisklasse.
  Stream *stream = http.getStreamPtr();
  const uint32_t frist = millis() + HYDRAULIK_LESEFRIST_MS;
  while (stream && gelesen < sizeof(antwort) - 1 && (int32_t)(millis() - frist) < 0)
  {
    const int c = stream->read();
    if (c < 0)
    {
      delay(1); // ruft yield() - der WLAN-Stack braucht die Gelegenheit
      continue;
    }
    antwort[gelesen++] = (char)c;
    antwort[gelesen] = '\0';
    // "OFF" zuerst: "ON" ist in "OFF" nicht enthalten, aber die Reihenfolge
    // waere sonst eine Falle fuer den naechsten Leser.
    if (strstr(antwort, "\"OFF\"") != 0 || strstr(antwort, "\"ON\"") != 0)
      break; // die Auskunft steht, der Rest der Antwort interessiert nicht
  }
  http.end();

  if (strstr(antwort, "\"OFF\"") != 0)
    return HYDRAULIK_AUS;
  if (strstr(antwort, "\"ON\"") != 0)
    return HYDRAULIK_EIN;
  return HYDRAULIK_UNKLAR;
}

/*****************************************************************************/
/* Der Hydraulikschritt: erst lesen, dann nur bei Bedarf schalten            */
/*                                                                           */
/* Der Zustand wird VORHER gelesen und nicht blind geschaltet, obwohl ein     */
/* einzelnes "Power Off" beide Faelle abdecken wuerde (Tasmota antwortet auch */
/* dann mit OFF, wenn der Schalter schon aus war). Der Unterschied ist        */
/* trotzdem wichtig: Nur so steht im Log, ob tatsaechlich umgeschaltet wurde  */
/* - und nur so liesse sich spaeter eine Wartezeit anhaengen, falls die       */
/* Stellantriebe doch einmal knapp werden sollten.                            */
/*                                                                           */
/* Rueckgabe: true, wenn die Hydraulik nachweislich 1-stufig steht.           */
/*****************************************************************************/
static bool hydraulik_auf_einstufig(void)
{
  char log_line[160];

  const HydraulikAntwort ist = hydraulik_kommando("Power");
  if (ist == HYDRAULIK_UNKLAR)
  {
    (void)snprintf(log_line, sizeof(log_line),
                   "Notbetrieb: Hydraulik-Switch (%.40s) antwortet nicht",
                   hydraulik_switch[0] ? hydraulik_switch : "keine Adresse");
    write_mqtt_log(log_line);
    return false; // kein zweiter Request - hoechstens ein Timeout je Lauf
  }

  if (ist == HYDRAULIK_AUS)
  {
    write_mqtt_log((char *)"Notbetrieb: Hydraulik stand bereits auf 1-stufig");
    return true;
  }

  // Sie stand auf 2-stufig - jetzt umlegen und die Antwort nachhalten.
  if (hydraulik_kommando("Power%20Off") != HYDRAULIK_AUS)
  {
    write_mqtt_log((char *)"Notbetrieb: Hydraulik liess sich nicht auf 1-stufig schalten");
    return false;
  }

  write_mqtt_log((char *)"Notbetrieb: Hydraulik auf 1-stufig geschaltet (Stellantriebe 90 s)");
  return true;
}

/*****************************************************************************/
/* Das Ergebnis des Hydraulikschritts                                        */
/*                                                                           */
/* Der Automat bekommt seine Bestaetigung als Wahrheitswert herein - bei den  */
/* Set-Schritten aus actual_data[], hier aus der Antwort des Switch. Der Wert */
/* wird beim Absetzen gesetzt und beim Start jedes Laufs geloescht: Ohne das  */
/* koennte ein "ja" vom vorigen Lauf den naechsten bestaetigen.               */
/*****************************************************************************/
static bool hydraulikBestaetigt = false;

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

  // Der Hydraulikschritt geht nicht an die Waermepumpe, sondern ins Hausnetz.
  // Sein Ergebnis steht sofort fest und wird fuer den Tick gemerkt.
  if (s->typ == NB_SCHRITT_HYDRAULIK)
  {
    char hyd_log[160];
    (void)snprintf(hyd_log, sizeof(hyd_log), "Notbetrieb Schritt %u/%u: %.32s",
                   (unsigned)(notbetriebLauf.schritt + 1),
                   notbetrieb_schritt_anzahl(notbetriebRolle), s->set_name);
    write_mqtt_log(hyd_log);

    hydraulikBestaetigt = hydraulik_auf_einstufig();
    return hydraulikBestaetigt;
  }

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
    notbetrieb_abschluss(&notbetriebLauf, NOTBETRIEB_ROT, millis(), NOTBETRIEB_GRUND_TIMEOUT);
    return;
  }

  bool bestaetigt = false;
  if (s->typ == NB_SCHRITT_HYDRAULIK)
  {
    // Die Bestaetigung kommt aus der Antwort des Switch, nicht aus
    // actual_data[] - gesetzt beim Absetzen, siehe hydraulikBestaetigt.
    bestaetigt = hydraulikBestaetigt;
  }
  else
  {
    // Sollwert des laufenden Schritts. Fehlt er, ist der Lauf nicht zu retten -
    // die Seite laesst ihn dann gar nicht erst starten, dies ist der Notnagel.
    int soll = 0;
    if (!notbetrieb_schritt_wert(&notbetriebLauf, notbetriebRolle, &notbetriebWerte, &soll))
    {
      notbetrieb_abschluss(&notbetriebLauf, NOTBETRIEB_ROT, millis(), NOTBETRIEB_GRUND_TIMEOUT);
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
    bestaetigt = (index >= 0) ? notbetrieb_rueckgelesen(actual[index], soll) : false;
  }

  char log_line[160];
  switch (notbetrieb_tick(&notbetriebLauf, notbetriebRolle, millis(), bestaetigt, richtung))
  {
  case NOTBETRIEB_SENDEN:
    if (!notbetrieb_schritt_absetzen())
    {
      // Der Hydraulikschritt bricht HIER ab und nicht erst nach 20 s: Sein
      // Ergebnis steht mit der Antwort des Switch fest, es gibt nichts
      // nachzulesen. Der Mensch sieht ROT nach rund zwei Sekunden und kann
      // den Schalter im Waschraum von Hand legen.
      const NotbetriebAbbruchgrund grund =
          notbetrieb_grund_fuer_schritt(notbetriebRolle, notbetriebLauf.schritt);
      notbetrieb_abschluss(&notbetriebLauf, NOTBETRIEB_ROT, millis(), grund);
      write_mqtt_log(grund == NOTBETRIEB_GRUND_HYDRAULIK
                         ? (char *)"Notbetrieb ROT: Hydraulik nicht auf 1-stufig, kein Kommando an die WP"
                         : (char *)"Notbetrieb abgebrochen: Kommando abgelehnt");
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
/*                                                                          */
/* SEIT 3.15.0 SETZT DIESER HANDLER KEINEN SCHRITT MEHR AB. Bis 3.14.2 ging  */
/* das erste Kommando von hier aus raus - fuer ein Set-Kommando eine Sache   */
/* von Mikrosekunden. Schritt 1 ist jetzt der Hydraulikschritt und damit ein */
/* HTTP-Request von bis zu 1,5 s; laege er hier, hinge der Browser so lange  */
/* an einer Seite, die noch nichts anzeigen kann. Der erste Tick aus loop()  */
/* holt ihn im naechsten Millisekundentakt nach.                             */
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

  // Ein "ja" vom vorigen Lauf darf den neuen nicht bestaetigen.
  hydraulikBestaetigt = false;

  write_mqtt_log((char *)"NOTBETRIEB ausgeloest ueber die Weboberflaeche");
  return true;
}

/*****************************************************************************/
/* Warum der letzte Lauf abgebrochen wurde                                   */
/*                                                                           */
/* Die Statusroute haengt den Wert hinten an den Kurzstatus, die Seite macht  */
/* daraus Klartext. Bei GRUEN und waehrend eines Laufs steht dort KEINER.     */
/*****************************************************************************/
NotbetriebAbbruchgrund notbetrieb_abbruchgrund(void)
{
  return (NotbetriebAbbruchgrund)notbetriebLauf.grund;
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
