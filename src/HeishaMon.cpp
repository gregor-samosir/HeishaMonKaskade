#include "HeishaMon.h"
#include "webfunctions.h"
#include "decode.h"
#include "commands.h"

Ticker Send_Pana_Command_Timer(send_pana_command, COMMANDTIMER, 1);   // one time
Ticker Send_Pana_Mainquery_Timer(send_pana_mainquery, QUERYTIMER, 1); // one time
Ticker Read_Pana_Data_Timer(read_pana_data, BUFFERTIMEOUT, 1);        // one time
Ticker Timeout_Serial_Timer(timeout_serial, SERIALTIMEOUT, 1);        // one time

// Eine Antwort steht aus - die Leitung gehoert bis zum Lesen oder bis zum
// Serial-Timeout dem laufenden Zyklus. Seit 3.8.0 wird das Flag vor dem
// Senden auch tatsaechlich geprueft (send_pana_command).
bool serialquerysent = false;

// Default settings if config does not exists
// stage-specific hostname comes as build flag from platformio.ini, fallback = stage 1
// (config.json on the device overrides this default at boot)
#ifndef HEISHA_HOSTNAME
#define HEISHA_HOSTNAME "HeishaMon"
#endif
const char *update_path = "/firmware";
const char *update_username = "admin";

/*****************************************************************************/
/* Der Zugang zum Notbetriebsknopf - ein EIGENES Passwort                    */
/*                                                                           */
/* Bis 3.11.0 haetten /notbetrieb und /notbetrieb/start dasselbe Passwort     */
/* verlangt wie /firmware und /settings. Das ist genau falsch herum: Den      */
/* Knopf soll im Ernstfall JEDER aus der Familie druecken koennen, er steht   */
/* mit Passwort in der ausgedruckten Anleitung. Dasselbe Blatt haette damit   */
/* auch den Zugang zum Firmware-Upload und zu den MQTT-Zugangsdaten geoeffnet.*/
/*                                                                           */
/* Deshalb: eigener Benutzer, eigenes Passwort, einkompiliert aus             */
/* platformio_user_env.ini (Owner-Entscheidung 2026-08-21). Der Knopf kann    */
/* nichts weiter als die Schrittfolge ausloesen - wer ihn kennt, kann keine   */
/* Firmware aufspielen und keine Einstellungen aendern.                       */
/*                                                                           */
/* Der Fallback unten ist nur der Notnagel fuer einen Build ohne diese Datei; */
/* er steht im oeffentlichen Repo und schuetzt entsprechend wenig - dasselbe  */
/* Muster wie beim Setup-AP in webfunctions.cpp.                              */
/*****************************************************************************/
#ifndef HEISHA_NOTBETRIEB_PASSWORD
#define HEISHA_NOTBETRIEB_PASSWORD "notbetrieb"
#warning "HEISHA_NOTBETRIEB_PASSWORD nicht gesetzt - der Notbetriebsknopf laeuft mit dem oeffentlich bekannten Fallback (siehe platformio_user_env_sample.ini)"
#endif
// Vier Zeichen sind die Untergrenze, unter der ein Passwort keines mehr ist.
// Der Build bricht hier lieber, als dass es erst an der Waermepumpe auffaellt.
static_assert(sizeof(HEISHA_NOTBETRIEB_PASSWORD) >= 5, "HEISHA_NOTBETRIEB_PASSWORD braucht mindestens 4 Zeichen");
const char *notbetrieb_username = "notbetrieb";
const char *notbetrieb_password = HEISHA_NOTBETRIEB_PASSWORD;
// Groessen aus webfunctions.h - dort werden die Felder aus der config.json
// gefuellt, und beide Seiten muessen dieselbe Puffergroesse annehmen
char wifi_hostname[CONFIG_FIELD_LEN] = HEISHA_HOSTNAME;
char ota_password[CONFIG_FIELD_LEN] = "heisha";
char mqtt_server[CONFIG_FIELD_LEN];
char mqtt_port[CONFIG_PORT_LEN] = "1883";
char mqtt_username[CONFIG_FIELD_LEN];
char mqtt_password[CONFIG_FIELD_LEN];

/*****************************************************************************/
/* Die Adresse des Hydraulik-Switch (seit 3.15.0)                            */
/*                                                                           */
/* Der Tasmota-Schalter, der die Hydraulik zwischen 1- und 2-stufig umlegt.  */
/* Er gehoert zu den Einstellungen und NICHT fest in den Code: Er steht in   */
/* derselben Groessenordnung wie der MQTT-Broker und wird sich irgendwann    */
/* aendern.                                                                  */
/*                                                                           */
/* Leer heisst "nicht eingerichtet". Der Notbetrieb bricht dann im ersten    */
/* Schritt ab, statt loszulaufen und die Hydraulik ungeprueft zu lassen -    */
/* siehe notbetrieb.cpp.                                                     */
/*****************************************************************************/
char hydraulik_switch[CONFIG_FIELD_LEN] = "";

// log and debug
bool outputMqttLog = true;   // toggle to write logmessages to mqtt (true) or telnetstream (false)
bool outputTelnetLog = true; // enable/disable telnet DEBUG
bool outputHexLog = false;

// global scope
uint8_t serial_data[MAXDATASIZE]; // uint8_t statt char: Bytes > 127, kein Vorzeichen-Risiko
unsigned int serial_length = 0; // int instead of byte: must be able to reach MAXDATASIZE for the overflow check

// store actual values in a fixed char array: permanent String objects
// fragment the heap on long running ESP8266 devices
char actual_data[NUMBEROFTOPICS][MAXVALUELEN];

// log message
char log_msg[MAXDATASIZE];

bool newcommand = false;    // a send is due (command or plain query)
bool setDataPending = false; // the buffer holds at least one real SET field

// Sammelfenster: Zeitpunkt, an dem es geoeffnet wurde, und Zahl der bisher
// eingesammelten Verlaengerungen. Regeln und Begruendung in sendwindow.h.
// uint32_t passend zur Regel dort - auf beiden Chips dasselbe wie millis().
uint32_t commandWindowStart = 0;
unsigned int commandWindowExtensions = 0;

// Wie oft das Senden schon verschoben wurde, weil noch ein Lesefenster lief.
// Wird bei jedem tatsaechlichen Senden zurueckgesetzt.
unsigned int commandSendDeferrals = 0;

WebServerClass httpServer(80);
HTTPUpdateServerClass httpUpdater;

WiFiClient mqtt_wifi_client;
PubSubClient mqtt_client(mqtt_wifi_client);
unsigned long lastReconnectAttempt = 0;
unsigned long mqttReconnectDelay = MQTT_RECONNECT_MIN; // waechst bei Misserfolg

// Karenzfenster nach dem Abonnieren (siehe SUBSCRIBE_GRACE) und Zaehler der
// dabei verworfenen Kommandos - er wird einmal gemeldet, wenn das Fenster zu ist
unsigned long setCommandsIgnoredUntil = 0;
unsigned int ignoredSetCommands = 0;

// Verbindung zur Hausteuerung (siehe verbindung.h). Sie haengt am MQTT-Client
// und nicht am WLAN: Der Ausfall, um den es geht, ist der des ioBroker - und
// der MQTT-Broker IST der ioBroker-Adapter. Das WLAN laeuft dabei weiter,
// sonst waere auch die Weboberflaeche weg, die diese Auskunft anzeigen soll.
VerbindungsWacht hausteuerung;

// Dauer der zuletzt beendeten Stille, wartet aufs Loggen aus loop(). Der Wert
// wird im MQTT-Callback gesetzt und NICHT dort geloggt - warum, steht an der
// Setzstelle in mqtt_callback(). Gleiches Muster wie wifiOutageSeconds.
uint32_t stilleBeendetSekunden = 0;

// WLAN-Watchdog (siehe check_wifi)
bool wifiIsDown = false;
unsigned long wifiDownSince = 0;
unsigned long lastWifiRetry = 0;
unsigned long wifiOutageSeconds = 0; // letzte Ausfalldauer, wartet auf MQTT

byte mainQuery[] = {0x71, 0x6c, 0x01, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
byte mainCommand[] = {0xF1, 0x6c, 0x01, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
// BEWUSST AUFGEHOBEN, obwohl nirgends benutzt (Owner-Entscheid 2026-08-13):
// Das Original-HeishaMon schickt dieses Telegramm einmalig beim Start. Bis
// heute ist ungeklaert, wozu die Waermepumpe es braucht - an beiden Stufen
// hier laeuft die Abfrage seit Jahren ohne. Der Verdacht der Community ist,
// dass eine fabrikneue oder frisch zurueckgesetzte Waermepumpe diese
// Initialisierung doch erwartet. Deshalb bleibt das Muster stehen: Es kostet
// sieben Bytes und waere im Ernstfall nicht mehr zu rekonstruieren.
byte initialQuery[] = {0x31, 0x05, 0x10, 0x01, 0x00, 0x00, 0x00};
byte cleanCommand[QUERYSIZE];

/*****************************************************************************/
/* OTA                                                                       */
/*****************************************************************************/
void setupOTA()
{
#if !defined(ESP32)
  ArduinoOTA.setPort(8266); // ESP32 keeps default 3232 (matches espota upload)
#endif
  ArduinoOTA.setHostname(wifi_hostname); // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setPassword(ota_password);  // Set authentication
  ArduinoOTA.onStart([]() {});
  ArduinoOTA.onEnd([]() {});
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {});
  ArduinoOTA.onError([](ota_error_t error) {});
  ArduinoOTA.begin();
}

/*****************************************************************************/
/* Write to mqtt log                                                         */
/*****************************************************************************/
void write_mqtt_log(char *string)
{
  if (outputMqttLog)
  {
    (void)mqtt_client.publish(Topics::LOG.c_str(), string);
  }
  else
  {
    (void)TelnetStream.printf("[%02d-%02d-%02d %02d:%02d:%02d] %s\n", year(), month(), day(), hour(), minute(), second(), string);
  }
}

/*****************************************************************************/
/* DEBUG to Telnet log                                                       */
/*****************************************************************************/
void write_telnet_log(char *string)
{
  if (outputTelnetLog)
  {
    TelnetStream.printf("[%02d-%02d-%02d %02d:%02d:%02d] \e[31m<DBG>\e[39m %s\n", year(), month(), day(), hour(), minute(), second(), string);
  }
}

/*****************************************************************************/
/* DEBUG hex log  (Igor Ybema)                                               */
/*****************************************************************************/
void write_hex_log(uint8_t *hex, unsigned int hex_len)
{
  int bytesperline = 32; // please be aware of max mqtt message size - 32 bytes per line does not work
  for (unsigned int i = 0; i < hex_len; i += bytesperline)
  {
    char buffer[(bytesperline * 3) + 1];
    buffer[bytesperline * 3] = '\0';
    for (unsigned int j = 0; ((j < (unsigned int)bytesperline) && ((i + j) < hex_len)); j++)
    {
      // 4 = zwei Hexziffern, Leerzeichen, Terminator
      (void)snprintf(&buffer[3 * j], 4, "%02X ", (unsigned char)hex[i + j]);
    }
    (void)snprintf(log_msg, sizeof(log_msg), "data: %s", buffer);
    write_telnet_log(log_msg);
  }
}

/*****************************************************************************/
/* FreeMemory (Igor Ybema)                                                   */
/*****************************************************************************/
int getFreeMemory()
{
  // store total memory at boot time
  static uint32_t total_memory = 0;
  if (0 == total_memory)
    total_memory = ESP.getFreeHeap();

  uint32_t free_memory = ESP.getFreeHeap();
  return (100 * free_memory / total_memory); // as a %
}

/*****************************************************************************/
/* WiFi Quality (Igor Ybema)                                                  */
/*****************************************************************************/
int getWifiQuality()
{
  if (WiFi.status() != WL_CONNECTED)
    return -1;
  int dBm = WiFi.RSSI();
  if (dBm <= -100)
    return 0;
  if (dBm >= -50)
    return 100;
  return 2 * (dBm + 100);
}

/*****************************************************************************/
/* HTTP                                                                      */
/*****************************************************************************/
void setupHttp()
{
  httpUpdater.setup(&httpServer, update_path, update_username, ota_password);
  httpServer.on("/", []()
                { handleRoot(&httpServer); });
  httpServer.on("/tablerefresh", []()
                { handleTableRefresh(&httpServer, actual_data); });
  // state-changing endpoints require login (same credentials as /firmware)
  httpServer.on("/reboot", []()
                {
    if (!httpServer.authenticate(update_username, ota_password))
    {
      return httpServer.requestAuthentication();
    }
    handleReboot(&httpServer); });
  httpServer.on("/settings", []()
                {
    if (!httpServer.authenticate(update_username, ota_password))
    {
      return httpServer.requestAuthentication();
    }
    handleSettings(&httpServer, wifi_hostname, ota_password, mqtt_server, mqtt_port, mqtt_username, mqtt_password, hydraulik_switch); });
  // Notbetrieb: Seite und Ausloeser verlangen einen EIGENEN Zugang, nicht den
  // des Firmware-Uploads - Begruendung oben bei notbetrieb_password.
  // Die Statusroute verlangt gar keinen: Sie gibt nur "Schritt 3 von 7" heraus
  // und wird alle zwei Sekunden abgefragt.
  httpServer.on("/notbetrieb", []()
                {
    if (!httpServer.authenticate(notbetrieb_username, notbetrieb_password))
    {
      return httpServer.requestAuthentication();
    }
    handleNotbetrieb(&httpServer); });
  httpServer.on("/notbetrieb/start", HTTP_POST, []()
                {
    if (!httpServer.authenticate(notbetrieb_username, notbetrieb_password))
    {
      return httpServer.requestAuthentication();
    }
    handleNotbetriebStart(&httpServer); });
  httpServer.on("/notbetrieb/status", []()
                { handleNotbetriebStatus(&httpServer); });
  httpServer.on("/togglelog", []()
                {
    if (!httpServer.authenticate(update_username, ota_password))
    {
      return httpServer.requestAuthentication();
    }
    write_mqtt_log((char *)"Toggled mqtt log flag");
    outputMqttLog ^= true;
    handleRoot(&httpServer); });
  httpServer.on("/toggledebug", []()
                {
    if (!httpServer.authenticate(update_username, ota_password))
    {
      return httpServer.requestAuthentication();
    }
    write_mqtt_log((char *)"Toggled debug flag");
    outputTelnetLog ^= true;
    handleRoot(&httpServer); });
  httpServer.begin();
}

/*****************************************************************************/
/* Serial setup and switch                                                   */
/*****************************************************************************/
void setupSerial()
{
  Serial.begin(115200); // ESP8266: boot debug before swap / ESP32: USB CDC console
#if defined(ESP32)
  delay(100); // let USB CDC come up
#endif
  Serial.flush();
}

void switchSerial()
{
#if defined(ESP32)
  // heatpump on its own UART - USB console stays available in parallel
  Serial.println("Starting heatpump serial on Serial1 (RX18/TX17). USB debug stays alive.");
  heatpumpSerial.setRxBufferSize(MAXDATASIZE);
  heatpumpSerial.begin(9600, SERIAL_8E1, HEATPUMPRX, HEATPUMPTX);
  heatpumpSerial.flush();
  pinMode(ENABLEOTPIN, OUTPUT);
  digitalWrite(ENABLEOTPIN, LOW); // keep unused OpenTherm 24V booster off
  pinMode(ENABLEPIN, OUTPUT);
  digitalWrite(ENABLEPIN, HIGH); // enable mosfet for TX line to heatpump
#else
  Serial.println("Switch serial to heatpump. Look for debug on mqtt log topic.");
  // serial to cn-cnt
  heatpumpSerial.flush();
  heatpumpSerial.end();
  heatpumpSerial.begin(9600, SERIAL_8E1);
  heatpumpSerial.flush();
  heatpumpSerial.swap();       // swap to gpio13 (D7) and gpio15 (D8)
  pinMode(ENABLEPIN, OUTPUT);  // enable gpio15 after boot using gpio5 (D1)
  digitalWrite(ENABLEPIN, HIGH);
#endif
}

/*****************************************************************************/
/* MQTT Client reconnect                                                     */
/*****************************************************************************/
bool mqtt_reconnect()
{
  write_telnet_log((char *)"Mqtt reconnect");
  if (mqtt_client.connect(wifi_hostname, mqtt_username, mqtt_password, Topics::WILL.c_str(), 1, true, "Offline"))
  {
    // Retain-Flag: das Will "Offline" liegt retained beim Broker (Argument 6
    // im connect() darueber). Ohne Retain hier bliebe nach jedem Reconnect
    // "Offline" der letzte gespeicherte Wert - ein Abonnent, der sich spaeter
    // verbindet (Node-RED-Neustart, Kaskaden-Waechter), haelt die Stufe dann
    // fuer tot, obwohl sie laeuft. Beide Zustaende desselben Topics muessen
    // retained sein, sonst ist der zuletzt gespeicherte immer der falsche.
    (void)mqtt_client.publish(Topics::WILL.c_str(), "Online", true);

    // Set-Topics kommen aus setCommands[] in commands.cpp. Hier stand bis
    // 3.4.1 je Topic ein eigener subscribe-Aufruf - eine zweite Liste, die
    // stillschweigend hinter der Tabelle zurueckbleiben konnte.
    (void)subscribe_set_topics(mqtt_client);

    // Der Notbetriebszweig wird ebenfalls abonniert. Auf DIESEN Schwall ist
    // die Firmware angewiesen: Der Adapter spielt jedem neuen Abonnenten die
    // gespeicherten Werte ein, und nur so sind die Kurvenwerte nach einem
    // Neustart binnen Sekunden wieder da, ohne dass Node-RED etwas tun muss.
    (void)notbetrieb_subscribe(mqtt_client);

    // Ab jetzt kommt der Wiedereinspiel-Schwall des Brokers - siehe
    // SUBSCRIBE_GRACE und die Pruefung in mqtt_callback()
    setCommandsIgnoredUntil = millis() + SUBSCRIBE_GRACE;

    // ein waehrend des Ausfalls gemerkter WLAN-Abriss wird jetzt meldbar
    if (wifiOutageSeconds > 0)
    {
      (void)snprintf(log_msg, sizeof(log_msg), "WLAN war %lu s weg", wifiOutageSeconds);
      write_mqtt_log(log_msg);
      wifiOutageSeconds = 0;
    }
  }
  return mqtt_client.connected();
}

/*****************************************************************************/
/* MQTT Callback                                                             */
/*****************************************************************************/
void mqtt_callback(char *topic, byte *payload, unsigned int length)
{
  // Payload zuerst kopieren - beide Zweige darunter brauchen ihn als
  // nullterminierten String, und der Notbetriebszweig kommt VOR der
  // Karenzpruefung dran.
  char msg[length + 1];
  for (unsigned int i = 0; i < length; i++)
  {
    msg[i] = (char)payload[i];
  }
  msg[length] = '\0';

  // Der Notbetriebszweig wird VOR der Karenzpruefung behandelt, und das sind
  // die entscheidenden Zeilen dieses Vorhabens: Die Wiedereinspielung des
  // Brokers ist hier kein Problem, sondern der Mechanismus. Nur weil diese
  // Werte den Schwall passieren duerfen, sind sie nach jedem Neustart und
  // jedem Reconnect binnen Sekunden wieder da. Die Gefahr, die zur Karenzzeit
  // gefuehrt hat, kann hier nicht auftreten - sie gehen nie ungefragt an die
  // Waermepumpe. Wird diese Ausnahme vergessen, funktioniert der Knopf im
  // Labor und nach jedem Neustart nicht mehr (Hosttest: notbetrieb_test.cpp).
  if (notbetrieb_mqtt_annehmen(topic, msg))
  {
    return; // abschliessend behandelt, kein Set-Kommando, kein Timer angefasst
  }

  // Karenzzeit nach dem Abonnieren: Der ioBroker-Adapter
  // schickt einem neuen Abonnenten den gespeicherten Wert jedes Set-Topics.
  // Am 2026-08-13 gemessen, was das an der Anlage anrichtet: ein Kurvenwert
  // vom 10.08. setzte nach jedem Neustart den Vorlauf-Sollwert auf 55 Grad,
  // und QuietMode 3 zusammen mit PowerfulMode 0 im selben Sammelfenster
  // schaltete den Fluestermodus ab (beide schreiben Byte 7, der letzte
  // gewinnt). Ueber das Retain-Bit ist das NICHT zu filtern: der Adapter
  // sendet die Wiedereinspielung mit retain=0, und PubSubClient reicht das
  // Flag ohnehin nicht an den Callback durch. Ein in diesem Fenster wirklich
  // gemeintes Kommando geht verloren - der Re-Assert der Steuerung holt es
  // binnen 5 Minuten nach.
  if ((long)(millis() - setCommandsIgnoredUntil) < 0)
  {
    ignoredSetCommands++;
    (void)snprintf(log_msg, sizeof(log_msg), "Verworfen (Wiedereinspielung nach Connect): %.64s", topic);
    write_telnet_log(log_msg);
    return; // kein Timer angefasst, der Abfragezyklus laeuft unveraendert weiter
  }

  // HERZSCHLAG DER STEUERUNG (3.13.0). Der Aufruf steht bewusst HIER - nach
  // der Karenzpruefung, vor dem Bauen des Kommandos:
  //
  // * Nach der Karenzpruefung, weil der Wiedereinspiel-Schwall des
  //   ioBroker-Adapters KEIN Lebenszeichen der Kaskadenregelung ist. Der
  //   Adapter schickt jedem neuen Abonnenten die gespeicherten Werte, auch
  //   wenn Node-RED laengst tot ist. Zaehlte er mit, verstummte die Meldung
  //   nach jedem Reconnect fuer zwoelf Minuten, ohne dass sich etwas
  //   geaendert haette. (Am 2026-08-21 am Pruefstand gemessen: 34
  //   wiedereingespielte Kommandos direkt nach dem Verbinden.)
  // * Vor build_heatpump_command(), weil ein Kommando, das die Firmware
  //   danach verwirft (unbekanntes Topic, Bereichsfehler), trotzdem beweist,
  //   dass die Steuerung sendet - und genau darum geht es hier.
  //
  // HIER WIRD NUR GEMERKT, NICHT GELOGGT - und das ist keine Stilfrage:
  // write_mqtt_log() ruft mqtt_client.publish(), und PubSubClient benutzt fuer
  // Senden und Empfangen DENSELBEN Puffer. In diesen Puffer zeigen `topic` und
  // `payload` waehrend des Callbacks. Ein Publish an dieser Stelle
  // ueberschreibt sie also mitten in der Auswertung. Am 2026-08-21 am
  // Pruefstand genau so passiert: Aus "panasonic_heat_pump_test/set/QuietMode"
  // wurde "0Q", die Firmware meldete "Unknown set topic 0Q" und das Kommando
  // ging verloren. Die Meldung geht deshalb aus loop() raus, wenn der Puffer
  // wieder frei ist - dasselbe Muster wie bei wifiOutageSeconds.
  {
    uint32_t stille_s = verbindung_set_empfangen(&hausteuerung, (uint32_t)millis());
    if (stille_s > 0)
    {
      stilleBeendetSekunden = stille_s;
    }
  }

  Send_Pana_Mainquery_Timer.stop();
  write_telnet_log((char *)"Callback from mqtt");

  // on error no command was registered: restart the query cycle,
  // otherwise no timer is running anymore and polling stops forever
  if (!build_heatpump_command(topic, msg))
  {
    Send_Pana_Mainquery_Timer.start();
  }
}

/*****************************************************************************/
/* MQTT Setup                                                                */
/*****************************************************************************/
void setupMqtt()
{
  char *endptr;
  long port = strtol(mqtt_port, &endptr, 10);
  if (endptr == mqtt_port || *endptr != '\0' || port < 1 || port > 65535) {
    (void)snprintf(log_msg, sizeof(log_msg), "Warning: Invalid MQTT port '%s'. Using default 1883.", mqtt_port);
    write_mqtt_log(log_msg);
    port = 1883;
  }
  mqtt_client.setServer(mqtt_server, (uint16_t)port);
  mqtt_client.setCallback(mqtt_callback);
  // ohne das blockiert jeder Verbindungsversuch bis zu 15 s in loop()
  mqtt_client.setSocketTimeout(MQTT_SOCKET_TIMEOUT_S);
  mqtt_reconnect();
}

/*****************************************************************************/
/* WLAN-Watchdog  (aus loop aufgerufen, blockiert nie)                       */
/*                                                                           */
/* Bis 3.5.0 gab es keine Pruefung: faellt das WLAN dauerhaft aus, laeuft    */
/* das Geraet blind weiter - es fragt die Waermepumpe ab, kann aber weder    */
/* Messwerte melden noch Sollwerte empfangen, und niemand merkt es. Der      */
/* Fallback des WiFiManagers greift nur beim Booten.                         */
/*****************************************************************************/
void check_wifi()
{
  unsigned long now = millis();

  if (WiFi.status() == WL_CONNECTED)
  {
    if (wifiIsDown) // gerade zurueckgekehrt
    {
      // Dauer nur merken, nicht sofort loggen: MQTT ist in diesem Moment noch
      // getrennt, die Meldung ginge verloren. Sie geht nach dem naechsten
      // erfolgreichen mqtt_reconnect() raus.
      wifiOutageSeconds = (now - wifiDownSince) / 1000UL;
      wifiIsDown = false;
      write_telnet_log((char *)"WLAN wieder verbunden");
    }
    return;
  }

  if (!wifiIsDown) // erster Durchlauf ohne Verbindung: Zeitpunkt merken
  {
    wifiIsDown = true;
    wifiDownSince = now;
    lastWifiRetry = now;
    write_telnet_log((char *)"WLAN-Verbindung verloren");
    return;
  }

  // Stufe 1: alle 30 s einen neuen Verbindungsversuch anstossen (nicht blockierend)
  if ((now - lastWifiRetry) >= WIFI_RETRY_TIMEOUT)
  {
    lastWifiRetry = now;
    write_telnet_log((char *)"WLAN weg, erneuter Verbindungsversuch");
    (void)WiFi.reconnect();
  }

  // Stufe 2: nach 5 min neu starten. Fuer die Waermepumpe ist das
  // ungefaehrlich - sie behaelt ihre Sollwerte, und die Node-RED-Steuerung
  // schreibt sie ohnehin alle 5 min neu.
  if ((now - wifiDownSince) >= WIFI_REBOOT_TIMEOUT)
  {
    write_telnet_log((char *)"WLAN seit 5 min weg, Neustart");
    delay(100);
    ESP.restart();
  }
}

/*****************************************************************************/
/* Validate checksum                                                         */
/*****************************************************************************/
bool validate_checksum()
{
  // Regel liegt in telegram.h, damit Firmware und Hosttest dieselbe benutzen
  return telegram_checksum_ok(serial_data, serial_length);
}

/*****************************************************************************/
/* Calculate checksum                                                        */
/*****************************************************************************/
byte calculate_checksum(byte *command)
{
  byte chk = 0;
  for (int i = 0; i < QUERYSIZE; i++)
  {
    chk += command[i];
  }
  chk = (chk ^ 0xFF) + 01;
  return chk;
}

/*****************************************************************************/
/* Read raw from serial                                                      */
/*****************************************************************************/
bool readSerial()
{
  while (true)
  {
    if (heatpumpSerial.available() > 0)
    {
      // bounds check: discard everything on overflow instead of wrapping the buffer
      if (serial_length >= MAXDATASIZE)
      {
        write_telnet_log((char *)"Serial buffer overflow, discarding data");
        while (heatpumpSerial.available() > 0)
        {
          (void)heatpumpSerial.read();
        }
        serial_length = 0;
        return false;
      }
      serial_data[serial_length] = heatpumpSerial.read();
      serial_length += 1;
      // only enable next line to DEBUG
      // sprintf(log_msg, "DEBUG Receive byte : %d", serial_length); write_telnet_log(log_msg);
    }
    else
      break;
  }

  // Erst hier entscheidet sich, ob das Gelesene ueberhaupt ein Antwort-
  // telegramm ist. Vor 3.6.0 genuegte "Laenge passt zum Laengenbyte" plus
  // Pruefsumme - ein um n Bytes verschobener Strom (Rest einer abgebrochenen
  // Antwort) konnte damit mit Wahrscheinlichkeit 1/256 als gueltige Messdaten
  // durchgehen und retained in die Kaskadenregelung laufen. Regeln: telegram.h
  TelegramCheck result = check_telegram(serial_data, serial_length);

  if (result == TELEGRAM_OK)
  {
    write_telnet_log((char *)"Valid data");
    if (outputHexLog)
      write_hex_log(serial_data, serial_length);
    serial_length = 0;
    return true;
  }

  // Fehlerfall: Typbyte und Laenge mitloggen, sonst ist im Betrieb nicht zu
  // unterscheiden, ob die Leitung stumm ist oder Muell liefert
  (void)snprintf(log_msg, sizeof(log_msg), "Telegramm verworfen (%s): Typ 0x%02X, Laenge %u (erwartet 0x%02X, %u)",
                 telegram_check_text(result), (serial_length > 0) ? serial_data[0] : 0, serial_length,
                 TELEGRAM_TYPE_DATA, TELEGRAM_DATA_LEN);
  write_telnet_log(log_msg);
  if (outputHexLog && (serial_length > 0))
    write_hex_log(serial_data, serial_length);
  serial_length = 0;
  return false;
}

/*****************************************************************************/
/* Empfangspuffer leeren, bevor gesendet wird                                */
/*                                                                           */
/* Ohne das startet jede Abfrage auf den Resten der vorigen: nach einem      */
/* Serial-Timeout (WP antwortet langsam oder unvollstaendig) bleiben Bytes   */
/* im UART-Puffer stehen und schieben sich vor die naechste Antwort. Der     */
/* Telegrammcheck in readSerial wirft das inzwischen weg - besser ist, es    */
/* gar nicht erst entstehen zu lassen.                                       */
/*****************************************************************************/
void flush_serial_input()
{
  unsigned int dropped = 0;
  // Obergrenze gegen eine dauerhaft sendende Leitung: die Schleife darf die
  // loop() nicht festhalten
  while ((heatpumpSerial.available() > 0) && (dropped < (2 * MAXDATASIZE)))
  {
    (void)heatpumpSerial.read();
    dropped++;
  }
  serial_length = 0;

  if (dropped > 0)
  {
    (void)snprintf(log_msg, sizeof(log_msg), "Restdaten vor dem Senden verworfen: %u Bytes", dropped);
    write_telnet_log(log_msg);
  }
}

/*****************************************************************************/
/* Register new command
/* Wait COMMANDTIMER for multible commands from mqtt
/*
/* Das Sammelfenster hat seit 3.8.0 einen Deckel: Bis dahin stiess JEDES
/* eintreffende SET den 500-ms-Timer neu an, damit mehrere Felder in ein
/* Telegramm wandern. Kommen die SETs dichter als 500 ms, wurde das Fenster
/* damit unbegrenzt verlaengert - Senden und Abfrage standen still, ohne dass
/* im Log etwas aufgefallen waere. Die Regel dafuer steht in sendwindow.h.
/*
/* Das Feld selbst ist zu diesem Zeitpunkt bereits in mainCommand eingetragen
/* (build_heatpump_command macht das VOR dem Aufruf hier). Am Deckel geht
/* also nichts verloren - der Wert faehrt mit dem gleich abgehenden Telegramm.
/*****************************************************************************/
void register_new_command()
{
  uint32_t now = (uint32_t)millis();

  // Fall 1: kein Fenster offen - eines oeffnen und den Sammeltimer anstossen
  if (!newcommand)
  {
    newcommand = true;
    commandWindowStart = now;
    commandWindowExtensions = 0;
    Send_Pana_Command_Timer.start();
    write_telnet_log((char *)"Register command/query");
    return;
  }

  // Fall 2: Fenster laeuft und darf noch verlaengert werden
  if (send_window_may_extend(now, commandWindowStart, COMMAND_WINDOW_MAX))
  {
    commandWindowExtensions++;
    Send_Pana_Command_Timer.start(); // weiter sammeln
    write_telnet_log((char *)"Register command/query");
    return;
  }

  // Fall 3: Deckel erreicht - Timer NICHT neu anstossen. Er laeuft aus dem
  // letzten Anstoss noch und feuert von selbst, spaetestens
  // COMMAND_WINDOW_MAX + COMMANDTIMER nach dem ersten SET.
  (void)snprintf(log_msg, sizeof(log_msg),
                 "Sammelfenster am Deckel (%u Verlaengerungen), wird jetzt gesendet",
                 commandWindowExtensions);
  write_telnet_log(log_msg);
}

/*****************************************************************************/
/* Send commands from buffer to pana  (called from loop)
/* send the set command global mainCommand[]
/* chk calculation must be on each time we send
/*****************************************************************************/
void send_pana_command()
{
  if (newcommand == false)
  {
    return;
  }

  // Laeuft noch ein Lesefenster, wartet die Waermepumpe gerade mit einer
  // Antwort auf. Bis 3.7.0 wurde trotzdem gesendet - das flush_serial_input()
  // unten warf die schon eingetroffene Antwort weg, und die anschliessende
  // Leserunde lief ins Leere ("Telegramm verworfen", eine Messrunde weg).
  // Auftreten konnte das, wenn ein SET 0 bis 500 ms nach dem Absenden einer
  // Abfrage eintraf: der Sammeltimer feuerte dann kurz vor dem Lesetimer.
  //
  // Jetzt wird stattdessen nachgefasst. timeout_serial gibt die Leitung nach
  // SERIALTIMEOUT (600 ms) in jedem Fall wieder frei, das dauert also
  // hoechstens ein bis zwei Runden. COMMAND_DEFER_MAX ist der Notausgang,
  // falls das Flag doch einmal haengt - sonst bliebe das Kommando fuer immer
  // liegen (Regel und Begruendung in sendwindow.h).
  //
  // Der Preis: Read_Pana_Data_Timer wird in loop() NACH diesem Timer
  // aktualisiert. Wird im selben Durchlauf gelesen, ist die Leitung sofort
  // frei, das Kommando wartet aber trotzdem die vollen COMMANDTIMER ab.
  // 500 ms Verzoegerung gegen eine verlorene Messrunde von ueber 5 s - der
  // Tausch lohnt, und bei einem 6-s-Takt faellt er nicht auf.
  if (serialquerysent == true)
  {
    if (send_may_defer(commandSendDeferrals, COMMAND_DEFER_MAX))
    {
      commandSendDeferrals++;
      Send_Pana_Command_Timer.start(); // in COMMANDTIMER erneut versuchen
      write_telnet_log((char *)"Lesefenster laeuft noch, Senden verschoben");
      return;
    }

    // Notausgang: Verhalten wie bis 3.7.0 - senden und die alte Antwort
    // verwerfen. Auffaellig genug loggen, das darf im Betrieb nicht vorkommen.
    (void)snprintf(log_msg, sizeof(log_msg),
                   "Warnung: Lesefenster nach %u Versuchen noch offen, Senden erzwungen",
                   commandSendDeferrals);
    write_mqtt_log(log_msg);
    serialquerysent = false;
  }
  commandSendDeferrals = 0;

  // Saubere Ausgangslage: alles, was noch im Empfangspuffer liegt, gehoert
  // zu einem abgeschlossenen Zyklus und darf die neue Antwort nicht stoeren.
  flush_serial_input();

  if (!setDataPending)
  {
    heatpumpSerial.write(mainQuery, QUERYSIZE);
    heatpumpSerial.write(calculate_checksum(mainQuery));
    serialquerysent = true;
    write_telnet_log((char *)"Send query");
  }
  else
  {
    heatpumpSerial.write(mainCommand, QUERYSIZE);
    heatpumpSerial.write(calculate_checksum(mainCommand));
    serialquerysent = true;
    write_telnet_log((char *)"Send command");
    if (outputHexLog)
      write_hex_log(mainCommand, QUERYSIZE);
    // buffer is on the wire: drop both the payload and the claimed bits
    memcpy(mainCommand, cleanCommand, QUERYSIZE);
    memset(usedMask, 0, QUERYSIZE);
    setDataPending = false;
  }
  newcommand = false;
  Read_Pana_Data_Timer.start();
  Timeout_Serial_Timer.start();
}

/*****************************************************************************/
/* Send query to buffer  (called from loop)
/* only to trigger the next query if we have no new set command on buffer
/*****************************************************************************/
void send_pana_mainquery()
{
  if (newcommand == false)
  {
    register_new_command();
  }
}

/*****************************************************************************/
/* Read from pana and decode (call from loop)                           */
/*****************************************************************************/
void read_pana_data()
{
  if (serialquerysent == true) // only read if we have sent a command so we expect an answer
  {
    if (readSerial() == true)
    {
      serialquerysent = false;
      write_telnet_log((char *)"Decode topics ---------- Start ------------------");
      publish_heatpump_data(serial_data, actual_data, mqtt_client);
      write_telnet_log((char *)"Decode topics ---------- End --------------------\n");
      Send_Pana_Mainquery_Timer.start();
    }
  }
}

/*****************************************************************************/
/* handle serial timeout                                                     */
/*****************************************************************************/
void timeout_serial()
{
  if (serialquerysent == true)
  {
    serialquerysent = false; // we are allowed to send a new command
    write_telnet_log((char *)"Serial interface read timeout");
    Send_Pana_Mainquery_Timer.start();
  }
}

/*****************************************************************************/
/* handle telnet stream                                                      */
/*****************************************************************************/
void handle_telnetstream()
{
  if (TelnetStream.available() > 0)
  {
    switch (TelnetStream.read())
    {
    // K1 (3.8.1): Kein Reboot mehr ueber Telnet. Port 23 ist unauthentifiziert,
    // /reboot in der Weboberflaeche verlangt dagegen Login - ein Tastendruck
    // durfte damit die Waermepumpe von der Steuerung trennen, ohne dass
    // irgendjemand sich anmelden musste. Die Taste bleibt belegt und antwortet,
    // damit der Weg im Ernstfall nicht erraten werden muss.
    case 'R':
      TelnetStream.println("Reboot geht nur noch ueber die Weboberflaeche: http://<ip>/reboot (Login erforderlich)");
      break;
    case 'C':
      TelnetStream.println("bye bye");
      TelnetStream.flush();
      TelnetStream.stop();
      break;
    case 'L':
      TelnetStream.println("Toggled mqtt log flag");
      outputMqttLog ^= true;
      break;
    case 'D':
      TelnetStream.println("Toggled debug flag");
      outputTelnetLog ^= true;
      break;
    case 'H':
      TelnetStream.println("Toggled hexlog flag");
      outputHexLog ^= true;
      break;
    case 'M':
      TelnetStream.printf("[%02d-%02d-%02d %02d:%02d:%02d] <INF> Memory: %d\n", year(), month(), day(), hour(), minute(), second(), getFreeMemory());
      break;
    case 'W':
      TelnetStream.printf("[%02d-%02d-%02d %02d:%02d:%02d] <INF> WiFi: %d\n", year(), month(), day(), hour(), minute(), second(), getWifiQuality());
      break;
    case 'I':
      TelnetStream.printf("[%02d-%02d-%02d %02d:%02d:%02d] <INF> localIP: %s\n", year(), month(), day(), hour(), minute(), second(), WiFi.localIP().toString().c_str());
      break;
    }
  }
}

/*****************************************************************************/
/* Setup Time                                                                */
/*****************************************************************************/
void setupTime()
{
#if defined(ESP32)
  configTzTime(TIME_ZONE, "0.de.pool.ntp.org");
#else
  configTime(TIME_ZONE, "0.de.pool.ntp.org");
#endif
  // wait max. 30 s for NTP, otherwise continue without valid time
  // (timestamps then start at 1970, but heatpump polling must not be blocked)
  unsigned long start = millis();
  time_t now = time(nullptr);
  while ((now < SECS_YR_2000) && ((millis() - start) < NTPTIMEOUT))
  {
    delay(100);
    now = time(nullptr);
  }
  if (now < SECS_YR_2000)
  {
    write_mqtt_log((char *)"Warning: NTP timeout, continuing without valid time");
    return;
  }
  // take local time incl. DST from the TZ database instead of fixed +3600
  struct tm local;
  localtime_r(&now, &local);
  setTime(local.tm_hour, local.tm_min, local.tm_sec, local.tm_mday, local.tm_mon + 1, local.tm_year + 1900);
}

/*****************************************************************************/
/* main                                                                      */
/*****************************************************************************/
void setup()
{
  getFreeMemory();
  setupSerial();

  // vor setupMqtt(): danach koennen schon Notbetriebswerte hereinkommen
  notbetrieb_init();

  // Anfangszustand der Verbindungswacht: getrennt und noch nie verbunden.
  // Muss VOR setupMqtt() stehen - dort faellt die erste Verbindung, und ein
  // spaeteres init() wuerde sie wieder auf "nie verbunden" zuruecksetzen.
  verbindung_init(&hausteuerung, millis());

  setupWifi(wifi_hostname, ota_password, mqtt_server, mqtt_port, mqtt_username, mqtt_password, hydraulik_switch);

  // mDNS is comfort only: log and continue instead of blocking the device forever
  if (MDNS.begin(wifi_hostname))
  {
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("telnet", "tcp", 23);
  }
  else
  {
    Serial.println("Warning: mDNS setup failed, continuing without mDNS");
  }

  setupOTA();
  setupMqtt();
  setupHttp();
  switchSerial();

  setupTime();
  TelnetStream.begin();

  memcpy(cleanCommand, mainCommand, QUERYSIZE); // copy the empty command
  Send_Pana_Mainquery_Timer.start();            // start only the query timer

  lastReconnectAttempt = 0;
}

void loop()
{
  ArduinoOTA.handle();
  httpServer.handleClient();
#if !defined(ESP32)
  MDNS.update(); // not needed/available on ESP32
#endif

  handle_telnetstream();
  check_wifi();

  // Notbetrieb: senden -> zuruecklesen -> naechster Schritt. Laeuft kein
  // Schaltvorgang, kehrt der Aufruf sofort zurueck. Er steht VOR der
  // MQTT-Wiederverbindung, weil der Notbetrieb genau dann gebraucht wird,
  // wenn der Broker weg ist - er darf nicht hinter einem Verbindungsversuch
  // haengen, der ohnehin nur scheitern kann.
  notbetrieb_loop(actual_data);

  // Verbindungswacht nachfuehren. Sie kostet nichts, wenn sich nichts aendert,
  // und steht bewusst VOR dem Wiederverbindungsversuch: So meldet sie die
  // Rueckkehr erst im naechsten Durchlauf, wenn die Verbindung wirklich steht
  // und die Log-Zeile darunter auch beim Broker ankommt.
  {
    // Die im Callback gemerkte Stille-Dauer jetzt melden - hier ist der
    // PubSubClient-Puffer frei (siehe die Begruendung in mqtt_callback).
    if (stilleBeendetSekunden > 0)
    {
      (void)snprintf(log_msg, sizeof(log_msg),
                     "Hausteuerung hat %lu s keine Vorgaben gesendet",
                     (unsigned long)stilleBeendetSekunden);
      write_mqtt_log(log_msg);
      stilleBeendetSekunden = 0;
    }

    // uint32_t und nicht unsigned long: Auf ESP8266 und ESP32 ist beides
    // 32 Bit, aber die Wacht rechnet durchgaengig in uint32_t (siehe
    // verbindung.h). Ein Zeigercast zwischen beiden Typen waere genau die
    // Falle, die dieser Header vermeidet.
    uint32_t ausfall_s = 0;
    if (verbindung_nachfuehren(&hausteuerung, mqtt_client.connected(),
                               (uint32_t)millis(), &ausfall_s))
    {
      // Gleiches Muster wie die WLAN-Meldung: waehrend des Ausfalls waere sie
      // ins Leere gegangen, nach der Rueckkehr kommt sie an. Fuer den
      // Menschen vor der Weboberflaeche ist sie zu spaet - dafuer steht die
      // Anzeige auf der Seite. Sie ist die Spur fuer die Nachschau danach.
      (void)snprintf(log_msg, sizeof(log_msg), "Hausteuerung war %lu s nicht erreichbar",
                     (unsigned long)ausfall_s);
      write_mqtt_log(log_msg);
    }
  }

  if (!mqtt_client.connected())
  {
    unsigned long now = millis();
    // Ohne WLAN ist ein Verbindungsversuch reine Blockadezeit: er kann nur
    // scheitern und wuerde den Abfragetakt anhalten. Backoff nicht hochzaehlen.
    if (WiFi.status() != WL_CONNECTED)
    {
      lastReconnectAttempt = now;
    }
    else if ((now - lastReconnectAttempt) >= mqttReconnectDelay)
    {
      lastReconnectAttempt = now;
      if (mqtt_reconnect())
      {
        mqttReconnectDelay = MQTT_RECONNECT_MIN; // Verbindung steht wieder
      }
      else
      {
        // Broker weg (z. B. ioBroker-Neustart): Abstand verdoppeln statt alle
        // 5 s erneut in den Socket-Timeout zu laufen
        mqttReconnectDelay *= 2;
        if (mqttReconnectDelay > MQTT_RECONNECT_MAX)
        {
          mqttReconnectDelay = MQTT_RECONNECT_MAX;
        }
      }
    }
  }
  else
  {
    (void)mqtt_client.loop(); // Trigger the mqtt_callback and send the set command to the buffer
  }

  // Bilanz des Karenzfensters einmal melden, sobald es zu ist. Einzeln waeren
  // das 32 MQTT-Nachrichten in einem Schwung - die Einzelheiten stehen im
  // Telnet-Log, hier reicht die Zahl.
  if ((ignoredSetCommands > 0) && ((long)(millis() - setCommandsIgnoredUntil) >= 0))
  {
    (void)snprintf(log_msg, sizeof(log_msg),
                   "%u wiedereingespielte Set-Kommandos nach dem Verbinden verworfen", ignoredSetCommands);
    write_mqtt_log(log_msg);
    ignoredSetCommands = 0;
  }

  Send_Pana_Command_Timer.update();   // trigger send_pana_command()   - send command or query from buffer
  Send_Pana_Mainquery_Timer.update(); // trigger send_pana_mainquery() - send query to buffer if no command in buffer

  Read_Pana_Data_Timer.update(); // trigger read_pana_data() - read from serial, decode bytes and publish to mqtt
  Timeout_Serial_Timer.update(); // trigger timeout_serial() - stop read from serial after timeout
}
