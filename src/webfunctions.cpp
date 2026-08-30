#include <LittleFS.h>
#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson

#include "webfunctions.h"
#include "decode.h"
#include "version.h"

// flag for saving
bool shouldSaveConfig = false;

static const char refreshMeta[] PROGMEM = "<meta http-equiv='refresh' content='5; url=/' />";

// CSS is embedded (styles the existing w3-class names): the UI must work
// without internet access, previously w3.css and jquery came from CDNs
// charset MUSS frueh im <head> stehen (in den ersten 1024 Bytes), sonst raet
// der Browser und macht aus "laeuft" mit Umlaut Kauderwelsch. Bis 3.12.0 war
// jeder Text der Oberflaeche in ae/oe/ue geschrieben und die Zeile deshalb
// entbehrlich; die Notbetriebsseite liest im Ernstfall jemand aus der Familie,
// und dort steht ab 3.13.0 richtiges Deutsch.
static const char webHeader[] PROGMEM = "<!DOCTYPE html><html><title>Heisha monitor</title><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'><style>"
    "*{box-sizing:border-box}body{font-family:Verdana,sans-serif;margin:0}h4{margin:10px 0}"
    ".w3-container{padding:0.01em 16px}.w3-card-4{box-shadow:0 4px 10px 0 rgba(0,0,0,.2)}"
    ".w3-card{box-shadow:0 2px 5px 0 rgba(0,0,0,.2)}.w3-center{text-align:center}.w3-left{float:left}"
    ".w3-small{font-size:12px}.w3-medium{font-size:15px}.w3-text-grey{color:#757575}"
    ".w3-xlarge{font-size:24px}.w3-padding-large{padding:12px 24px}"
    ".w3-panel{padding:0.01em 16px;margin:16px 0}"
    ".w3-button{border:none;padding:8px 16px;cursor:pointer;background:inherit;display:inline-block}"
    // Farben NACH .w3-button: dessen background:inherit hat dieselbe
    // Spezifitaet und wuerde eine vorher stehende Farbe ueberschreiben. Genau
    // daran war der Notbetriebsknopf grau statt rot - und der Save-Knopf der
    // Settings-Seite grau statt gruen.
    ".w3-theme,.w3-blue{background:#2196F3;color:#fff}.w3-green{background:#4CAF50;color:#fff}"
    ".w3-red{background:#f44336;color:#fff}.w3-orange{background:#ff9800;color:#fff}"
    ".w3-yellow{background:#ffeb3b;color:#000}"
    // Blassgelb fuer den Kurvenhinweis (3.14.1): ein Hinweis, keine Sperre -
    // Orange bleibt der Sperre vorbehalten, das kraeftige Gelb dem laufenden
    // Vorgang. Beides daneben zu stellen, waere ein drittes Warnsignal.
    ".w3-pale-yellow{background:#ffffcc;color:#000}"
    ".w3-sidebar{position:fixed;top:0;left:0;height:100%;width:200px;background:#fff;z-index:2;overflow:auto}"
    ".w3-bar-block .w3-bar-item{display:block;width:100%;text-align:left;text-decoration:none;color:#000;padding:8px 16px}"
    ".w3-bar-block .w3-bar-item:hover{background:#ccc}"
    ".w3-table-all{border-collapse:collapse;width:100%;margin:16px 0;border:1px solid #ccc}"
    ".w3-table-all th,.w3-table-all td{padding:6px 8px;text-align:left;border-bottom:1px solid #ddd}"
    ".w3-table-all tr:nth-child(even){background:#f1f1f1}"
    "input{padding:8px;border:1px solid #ccc;margin:4px 0}"
    "</style>";

// stage-specific page title comes as build flag from platformio.ini, fallback = stage 1
#ifndef HEISHA_STAGE_NAME
#define HEISHA_STAGE_NAME "Heisha Stufe 1"
#endif
static const char webBodyStart[] PROGMEM = "<button class='w3-button w3-blue w3-medium w3-left' onclick='openLeftMenu()'>&#9776;</button><header class='w3-container w3-card-4 w3-theme'><h4>" HEISHA_STAGE_NAME "</h4></header>";

static const char webFooter[] PROGMEM = "</body></html>";

static const char menuJS[] PROGMEM = "<script> function openLeftMenu() {var x = document.getElementById('leftMenu');if (x.style.display === 'none') {x.style.display = 'block';} else {x.style.display = 'none';}}</script>";

// table refresh via native fetch instead of jquery (was loaded from CDN)
static const char refreshJS[] PROGMEM = "<script>function refreshTable(){fetch('/tablerefresh').then(function(r){return r.text()}).then(function(t){document.getElementById('heishavalues').innerHTML=t}).catch(function(){}).finally(function(){setTimeout(refreshTable,30000)})}document.addEventListener('DOMContentLoaded',refreshTable);</script></head><body>";

static const char sidebar[] PROGMEM = "<a href='/' class='w3-bar-item w3-button w3-small'>Home</a><a href='/reboot' class='w3-bar-item w3-button w3-small'>Reboot</a><a href='/firmware' class='w3-bar-item w3-button w3-small'>Firmware</a><a href='/settings' class='w3-bar-item w3-button w3-small'>Settings</a><a href='/notbetrieb' class='w3-bar-item w3-button w3-small'>Notbetrieb</a><a href='/togglelog' class='w3-bar-item w3-button w3-small'>Toggle mqtt log</a><a href='/toggledebug' class='w3-bar-item w3-button w3-small'>Toggle debug log</a>";

// callback notifying us of the need to save config
void saveConfigCallback()
{
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

/*****************************************************************************/
/* Ein Feld aus der config.json uebernehmen                                  */
/*                                                                           */
/* Fehlt der Schluessel oder steht dort kein Text, bleibt der einkompilierte  */
/* Standardwert stehen. Vorher stand hier strncpy(dst, jsonDoc[key], 39):     */
/* ArduinoJson liefert fuer einen fehlenden Schluessel einen NULLZEIGER, und  */
/* strncpy stuerzt damit ab - das Geraet waere in einer Boot-Endlosschleife    */
/* gelandet und nur per USB an der Waermepumpe zu retten.                     */
/* Der realistische Ausloeser ist nicht das Bearbeiten der Datei von Hand,    */
/* sondern ein spaeterer Firmware-Stand, der ein NEUES Feld liest: die        */
/* config.json auf dem Geraet kennt es noch nicht, und beide Kaskadenstufen   */
/* wuerden direkt nach dem OTA gleichzeitig ausfallen.                        */
/*****************************************************************************/
static void loadConfigValue(char *dst, size_t dstsize, JsonDocument &jsonDoc, const char *key)
{
  const char *value = jsonDoc[key];
  if (value == nullptr)
  {
    Serial.printf("Config: key '%s' missing, keeping default '%s'\n", key, dst);
    return;
  }
  // strlcpy terminiert immer - die frueheren dst[n] = '\0'-Zeilen entfallen
  (void)strlcpy(dst, value, dstsize);
}

/*****************************************************************************/
/* Passwort des Setup-Hotspots (WPA2, seit 3.8.1)                            */
/*                                                                           */
/* Das Konfigurationsportal ist kein reines Erstboot-Thema: Faellt das WLAN   */
/* aus, startet der Watchdog das Geraet nach 5 min neu, autoConnect scheitert */
/* nach 10 s und oeffnet dann fuer 180 s den AP - zyklisch, solange die       */
/* Stoerung dauert. Die Parameterfelder sind dabei mit den ECHTEN Werten      */
/* vorbefuellt, darunter OTA- und MQTT-Passwort. Ohne WPA2 waere das ein      */
/* Fenster, in dem jeder in Funkreichweite die Zugangsdaten mitlesen oder     */
/* dem Geraet einen fremden MQTT-Broker unterschieben kann.                   */
/*                                                                           */
/* Das Passwort kommt als Build-Flag aus platformio_user_env.ini und steht    */
/* NICHT in git - gleiches Muster wie Ports, IPs und OTA-Passwort. Der        */
/* Fallback unten ist nur der Notnagel fuer einen Build ohne diese Datei; er  */
/* liegt im oeffentlichen Repo und schuetzt entsprechend wenig.               */
/*****************************************************************************/
#ifndef HEISHA_AP_PASSWORD
#define HEISHA_AP_PASSWORD "heishamon"
#warning "HEISHA_AP_PASSWORD nicht gesetzt - der Setup-AP laeuft mit dem oeffentlich bekannten Fallback (siehe platformio_user_env_sample.ini)"
#endif
// WPA2-Grenzen pruefen, solange es nichts kostet: ein zu kurzes Passwort
// verwirft der WiFiManager still und macht den AP wieder offen auf. Der Build
// bricht hier lieber, als dass es erst an der Waermepumpe auffaellt.
static_assert(sizeof(HEISHA_AP_PASSWORD) >= 9, "HEISHA_AP_PASSWORD braucht mindestens 8 Zeichen (WPA2)");
static_assert(sizeof(HEISHA_AP_PASSWORD) <= 64, "HEISHA_AP_PASSWORD darf hoechstens 63 Zeichen haben (WPA2)");

void setupWifi(char *wifi_hostname, char *ota_password, char *mqtt_server, char *mqtt_port, char *mqtt_username, char *mqtt_password, char *hydraulik_switch)
{
  // Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  wifiManager.setDebugOutput(true); // this is debugging on serial port, because serial swap is done after full startup this is ok

  Serial.println("mounting LittleFS...");

  if (LittleFS.begin(true)) // format on first mount
  {
    Serial.println("Mount file system");
    if (LittleFS.exists("/config.json"))
    {
      // file exists, reading and loading
      Serial.println("Read config file");
      File configFile = LittleFS.open("/config.json", "r");
      if (configFile)
      {
        Serial.println("Open config file");
        size_t size = configFile.size();
        // Puffer eins groesser als die Datei und selbst terminieren:
        // deserializeJson erwartet einen nullterminierten String und haette
        // sonst hinter dem Dateiende weitergelesen
        std::unique_ptr<char[]> buf(new char[size + 1]);

        size_t read = configFile.readBytes(buf.get(), size);
        buf[read] = '\0';
        // Seit 3.16.0 (ArduinoJson 7) waechst das Dokument elastisch; die
        // frueher noetige feste Groesse ist entfallen. Beim LESEN war sie
        // ohnehin unkritisch: Passte die Datei nicht, kam ein
        // DeserializationError, den der else-Zweig unten abfaengt.
        JsonDocument jsonDoc;
        DeserializationError error = deserializeJson(jsonDoc, buf.get());
        serializeJson(jsonDoc, Serial);
        if (!error)
        {
          Serial.println("\nparsed json");
          // fehlende Schluessel lassen den Standardwert stehen, siehe oben
          loadConfigValue(wifi_hostname, CONFIG_FIELD_LEN, jsonDoc, "wifi_hostname");
          loadConfigValue(ota_password, CONFIG_FIELD_LEN, jsonDoc, "ota_password");
          loadConfigValue(mqtt_server, CONFIG_FIELD_LEN, jsonDoc, "mqtt_server");
          loadConfigValue(mqtt_port, CONFIG_PORT_LEN, jsonDoc, "mqtt_port");
          loadConfigValue(mqtt_username, CONFIG_FIELD_LEN, jsonDoc, "mqtt_username");
          loadConfigValue(mqtt_password, CONFIG_FIELD_LEN, jsonDoc, "mqtt_password");
          // Fehlt der Schluessel (config.json von vor 3.15.0), bleibt das Feld
          // leer - loadConfigValue laesst den Standardwert stehen. Genau dafuer
          // ist es gebaut, siehe Kommentar dort.
          loadConfigValue(hydraulik_switch, CONFIG_FIELD_LEN, jsonDoc, "hydraulik_switch");
        }
        else
        {
          Serial.println("Failed to load config, forcing config reset.");
          wifiManager.resetSettings();
        }
        configFile.close();
      }
    }
    else
    {
      Serial.println("No config. Forcing reset to default.");
      wifiManager.resetSettings();
    }
  }
  else
  {
    Serial.println("Failed to mount FS");
  }

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_text1("<p>Hostname and OTA password</p>");
  WiFiManagerParameter custom_wifi_hostname("wifi_hostname", "wifi hostname", wifi_hostname, CONFIG_FIELD_LEN - 1);
  WiFiManagerParameter custom_ota_password("ota_password", "ota password", ota_password, CONFIG_FIELD_LEN - 1);
  WiFiManagerParameter custom_text2("<p>MQTT settings</p>");
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, CONFIG_FIELD_LEN - 1);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, CONFIG_PORT_LEN - 1);
  WiFiManagerParameter custom_mqtt_username("username", "mqtt username", mqtt_username, CONFIG_FIELD_LEN - 1);
  WiFiManagerParameter custom_mqtt_password("password", "mqtt password", mqtt_password, CONFIG_FIELD_LEN - 1);
  WiFiManagerParameter custom_text3("<p>Hydraulik switch (Tasmota, IP or hostname)</p>");
  WiFiManagerParameter custom_hydraulik_switch("hydraulik_switch", "hydraulik switch", hydraulik_switch, CONFIG_FIELD_LEN - 1);

  // set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  // add all your parameters here
  wifiManager.addParameter(&custom_text1);
  wifiManager.addParameter(&custom_wifi_hostname);
  wifiManager.addParameter(&custom_ota_password);
  wifiManager.addParameter(&custom_text2);
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_username);
  wifiManager.addParameter(&custom_mqtt_password);
  wifiManager.addParameter(&custom_text3);
  wifiManager.addParameter(&custom_hydraulik_switch);

  wifiManager.setConfigPortalTimeout(180);
  wifiManager.setConnectTimeout(10);
  // AP mit WPA2 statt offen - Begruendung und Herkunft des Passworts siehe oben
  if (!wifiManager.autoConnect("HeishaMon-Setup", HEISHA_AP_PASSWORD))
  {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    // reset and try again, or maybe put it to deep sleep
    ESP.restart();
    delay(5000);
  }

  // if you get here you have connected to the WiFi
  Serial.println("Wifi connected...yeey :)");

  // Werte aus dem Konfigurationsportal uebernehmen (strlcpy terminiert immer)
  (void)strlcpy(wifi_hostname, custom_wifi_hostname.getValue(), CONFIG_FIELD_LEN);
  (void)strlcpy(ota_password, custom_ota_password.getValue(), CONFIG_FIELD_LEN);
  (void)strlcpy(mqtt_server, custom_mqtt_server.getValue(), CONFIG_FIELD_LEN);
  (void)strlcpy(mqtt_port, custom_mqtt_port.getValue(), CONFIG_PORT_LEN);
  (void)strlcpy(mqtt_username, custom_mqtt_username.getValue(), CONFIG_FIELD_LEN);
  (void)strlcpy(mqtt_password, custom_mqtt_password.getValue(), CONFIG_FIELD_LEN);
  (void)strlcpy(hydraulik_switch, custom_hydraulik_switch.getValue(), CONFIG_FIELD_LEN);

  // Set hostname on wifi rather than ESP_xxxxx
  WiFi.setHostname(wifi_hostname);
  // modem sleep breaks inbound connections (ping/http time out while outbound
  // mqtt keeps working) - disable it, device is mains powered
  WiFi.setSleep(false);

  // save the custom parameters to FS
  if (shouldSaveConfig)
  {
    Serial.println("Save config");
    JsonDocument jsonDoc;
    jsonDoc["wifi_hostname"] = wifi_hostname;
    jsonDoc["ota_password"] = ota_password;
    jsonDoc["mqtt_server"] = mqtt_server;
    jsonDoc["mqtt_port"] = mqtt_port;
    jsonDoc["mqtt_username"] = mqtt_username;
    jsonDoc["mqtt_password"] = mqtt_password;
    jsonDoc["hydraulik_switch"] = hydraulik_switch;

    // Ueberlaufpruefung, siehe die ausfuehrliche Begruendung in handleSettings:
    // Eine halb geschriebene config.json kostet nach dem Neustart womoeglich
    // das MQTT-Passwort. Lieber den alten Stand behalten.
    if (jsonDoc.overflowed())
    {
      Serial.println("Config document overflowed, keeping previous config.json");
    }
    else
    {
      File configFile = LittleFS.open("/config.json", "w");
      if (!configFile)
      {
        Serial.println("Failed to open config file for writing");
      }
      else
      {
        serializeJson(jsonDoc, Serial);
        serializeJson(jsonDoc, configFile);
        configFile.close();
      }
    }
    // end save
  }

  Serial.println("local ip");
  Serial.println(WiFi.localIP());
}

/*****************************************************************************/
/* Die Saetze der Verbindungsanzeige                                         */
/*                                                                           */
/* Owner-Beobachtung 2026-08-21: Waehrend der Broker weg war, heizte die     */
/* Waermepumpe mit dem zuletzt gesetzten Sollwert einfach weiter - kein      */
/* Alarm, kein Hinweis, nichts. Wer die Seite oeffnet, soll das sehen.       */
/*                                                                           */
/* Der zweite Satz ist der wichtigere: Er beantwortet die Frage, die der     */
/* erste aufwirft ("ist die Heizung jetzt aus?"). Ohne ihn loest die Meldung */
/* eine Panik aus, die die Lage nicht hergibt.                               */
/*****************************************************************************/
#define VB_TXT_VERBUNDEN "Hausteuerung: verbunden"
#define VB_TXT_WEG_VOR "Hausteuerung seit "
#define VB_TXT_WEG_NACH " nicht erreichbar."
#define VB_TXT_WEG_NEUSTART "Hausteuerung seit dem Neustart dieses Geräts nicht erreichbar."
#define VB_TXT_FOLGE "Die Wärmepumpe läuft mit dem zuletzt gesetzten Sollwert weiter. Wird es zu kalt, hilft der Notbetrieb."
#define VB_TXT_FOLGE_HIER "Die Wärmepumpe läuft mit dem zuletzt gesetzten Sollwert weiter."

// Der zweite Ausfall: Der Server im Keller antwortet, aber es kommt nichts mehr
// von ihm. Der Text muss sich deutlich vom ersten unterscheiden - wer zum
// Server läuft, soll wissen, ob dort überhaupt etwas zu holen ist. Deshalb
// steht hier ausdrücklich "erreichbar", und der Hinweis zeigt auf die
// Steuerungssoftware statt auf den Server.
#define VB_TXT_STUMM_VOR "Hausteuerung erreichbar, sendet aber seit "
#define VB_TXT_STUMM_NACH " keine Vorgaben."
#define VB_TXT_STUMM_FOLGE "Der Server antwortet, aber die Steuerung rechnet nicht mehr. Die Wärmepumpe läuft mit dem zuletzt gesetzten Sollwert weiter."


/*****************************************************************************/
/* Die Verbindungsanzeige - einmal als JavaScript, einmal als C++            */
/*                                                                           */
/* Beide Seiten (Startseite und Notbetriebsseite) benutzen dieselben zwei    */
/* Funktionen. Die Zahlen kommen aus der Statusroute, die Textform der Dauer */
/* ("14 Minuten", "mehr als 30 Tagen") kommt FERTIG von dort - gerechnet     */
/* wird sie in src/verbindung.h, also an genau einer Stelle und vom Hosttest */
/* abgedeckt. Das JavaScript setzt nur den Rahmensatz darum.                 */
/*                                                                           */
/* zeigeOk unterscheidet die beiden Seiten: Auf der Notbetriebsseite steht   */
/* auch im Normalfall eine Zeile ("Hausteuerung: verbunden"), weil dort die  */
/* Entscheidung "Knopf druecken oder nicht" ansteht und ein ruhiges          */
/* "verbunden" die haeufigere Fehlentscheidung verhindert. Auf der           */
/* Startseite steht im Normalfall nichts - sie ist ein Nachschauwerkzeug,    */
/* keine Statusampel.                                                        */
/*****************************************************************************/
static const char verbindungJS[] PROGMEM =
    "<script>"
    "function vbSetzen(lage,dauer,zeigeOk){"
    "var e=document.getElementById('nbverb');if(!e)return;"
    // Lage 0 = verbunden, 1 = getrennt aber noch in der Karenz. Die Karenz
    // sieht auf der Seite bewusst aus wie "verbunden": Ein Adapter-Neustart
    // soll keine Meldung ausloesen, die von selbst wieder verschwindet.
    "if(lage==0||lage==1){"
    "if(zeigeOk){e.className='w3-text-grey w3-small';e.innerHTML='<p>" VB_TXT_VERBUNDEN "</p>';}"
    "else{e.className='';e.innerHTML='';}return;}"
    // Lage 4 = Broker da, aber keine Vorgaben mehr. Eigener Text, weil sonst
    // jemand zum Server laeuft, um dort nach dem falschen Fehler zu suchen.
    "if(lage==4){e.className='w3-panel w3-orange';"
    "e.innerHTML='<h3>" VB_TXT_STUMM_VOR "'+dauer+'" VB_TXT_STUMM_NACH "</h3><p>" VB_TXT_STUMM_FOLGE "</p>';return;}"
    // Lage 3 = seit dem Neustart nie verbunden. Dort ist die wahre Dauer
    // unbekannt, eine Minutenangabe waere gelogen.
    "var s=(lage==3)?'" VB_TXT_WEG_NEUSTART "':('" VB_TXT_WEG_VOR "'+dauer+'" VB_TXT_WEG_NACH "');"
    "e.className='w3-panel w3-orange';"
    "e.innerHTML='<h3>'+s+'</h3><p>'+(zeigeOk?'" VB_TXT_FOLGE_HIER "':'" VB_TXT_FOLGE "')+'</p>';}"
    // Nachfuehren der Startseite: dieselbe Route wie der Notbetriebsknopf,
    // aber im 30-s-Takt der Tabelle statt alle zwei Sekunden. Die
    // Notbetriebsseite ruft vbSetzen aus ihrem eigenen 2-s-Takt heraus auf
    // und braucht diese Funktion nicht.
    "function vbState(){fetch('/notbetrieb/status').then(r=>r.text()).then(t=>{"
    "var p=t.trim().split(';');vbSetzen(parseInt(p[5]),p[6],false);"
    "}).catch(()=>{}).finally(()=>{setTimeout(vbState,30000)})}"
    "</script>";

// Nur die Startseite startet das Nachfuehren von sich aus. Die
// Notbetriebsseite ruft vbSetzen() aus ihrem eigenen 2-s-Takt heraus auf und
// wuerde die Route sonst doppelt abfragen.
static const char verbindungStartJS[] PROGMEM =
    "<script>document.addEventListener('DOMContentLoaded',vbState);</script>";

/*****************************************************************************/
/* Dieselbe Zeile beim Aufbau der Seite                                      */
/*                                                                           */
/* Wer die Seite oeffnet, soll die Lage sofort lesen koennen - auch in der    */
/* Sekunde vor der ersten Statusabfrage. Gleiche Begruendung wie beim         */
/* Sperrhinweis des Notbetriebsknopfs darunter.                               */
/*****************************************************************************/
static void verbindungszeile(String &out, bool zeigeOk)
{
  const VerbindungsLage lage = verbindung_lage(&hausteuerung);

  if (lage == VERBINDUNG_VERBUNDEN || lage == VERBINDUNG_KARENZ)
  {
    if (zeigeOk)
    {
      out += "<div id='nbverb' class='w3-text-grey w3-small'><p>" VB_TXT_VERBUNDEN "</p></div>";
    }
    else
    {
      out += "<div id='nbverb'></div>"; // leer, das JavaScript fuellt es bei Bedarf
    }
    return;
  }

  // Der Puffer ist grosszuegig: der laengste Text ist "mehr als 30 Tagen"
  char dauer[32];
  verbindung_dauer_text(dauer, sizeof(dauer),
                        verbindung_ausfall_sekunden(&hausteuerung),
                        verbindung_ueber_deckel(&hausteuerung));

  out += "<div id='nbverb' class='w3-panel w3-orange'><h3>";
  if (lage == VERBINDUNG_STEUERUNG_STUMM)
  {
    out += VB_TXT_STUMM_VOR;
    out += dauer;
    out += VB_TXT_STUMM_NACH;
    out += "</h3><p>" VB_TXT_STUMM_FOLGE "</p></div>";
    return;
  }

  if (lage == VERBINDUNG_GESTOERT_SEIT_NEUSTART)
  {
    out += VB_TXT_WEG_NEUSTART;
  }
  else
  {
    out += VB_TXT_WEG_VOR;
    out += dauer;
    out += VB_TXT_WEG_NACH;
  }
  out += "</h3><p>";
  out += zeigeOk ? VB_TXT_FOLGE_HIER : VB_TXT_FOLGE;
  out += "</p></div>";
}

void handleRoot(WebServerClass *httpServer)
{
  httpServer->setContentLength(CONTENT_LENGTH_UNKNOWN);
  httpServer->send(200, "text/html");
  httpServer->sendContent_P(webHeader);
  httpServer->sendContent_P(webBodyStart);
  httpServer->sendContent_P(menuJS);
  httpServer->sendContent_P(verbindungJS);
  // Die Startseite fuehrt die Verbindungszeile im 30-s-Takt der Tabelle nach,
  // nicht alle zwei Sekunden wie die Notbetriebsseite: Hier steht keine
  // Entscheidung an, und das Geraet fragt nebenher die Waermepumpe ab.
  httpServer->sendContent_P(verbindungStartJS);
  // refreshJS schliesst den <head> und oeffnet den <body> - alles, was in den
  // Kopf gehoert, muss davor stehen.
  httpServer->sendContent_P(refreshJS);

  String httptext = "<div class='w3-sidebar w3-bar-block w3-card w3-animate-left' style='display:none' id='leftMenu'>";
  httpServer->sendContent(httptext);
  httpServer->sendContent_P(sidebar);
  httptext = "<hr><div class='w3-text-grey w3-small'>Version: ";
  httptext = httptext + heishamon_version + "<br><a href = 'https://github.com/gregor-samosir/HeishaMonKaskade '>Heishamon</a></div><hr></div>";
  // Auf der Startseite steht die Zeile NUR im Stoerfall (zeigeOk = false).
  // Sie ist ein Nachschauwerkzeug, keine Statusampel - ein dauerhaftes
  // "verbunden" ueber der Topic-Tabelle waere Rauschen und wuerde nach
  // kurzer Zeit uebersehen, samt der Stoermeldung an derselben Stelle.
  httptext += "<br><div class='w3-container'>";
  verbindungszeile(httptext, false);

  httptext = httptext + "<table class = 'w3-table-all w3-card-4 w3-small'><thead><tr class = 'w3-blue'><th>Topic</th><th>Name</th><th>Value</th><th>Description</th></tr></thead><tbody id =\"heishavalues\"><tr><td>... Loading...</td><td>.</td><td>.</td><td>.</td></tr></tbody></table></div>";
  httpServer->sendContent(httptext);
  httpServer->sendContent_P(webFooter);
  httpServer->sendContent("");
  httpServer->client().stop();
}

/*****************************************************************************/
/* Table rows, collected into TCP-sized blocks                               */
/*                                                                           */
/* One sendContent() per row used to cost about one network round trip on    */
/* the ESP32 (~20 ms each, measured before 3.4.0): the core pushes every     */
/* write out as its own packet and waits for the ack, so 99 rows added up to */
/* ~1.9 s of "Loading". Filling a buffer close to the TCP segment size first */
/* turns those 99 sends into about six. (The ESP8266 core, supported until   */
/* 3.15.0, coalesced writes and never showed the effect.)                    */
/*                                                                           */
/* sendbuf is static on purpose: 1400 bytes would be a large share of the    */
/* stack, and this handler is only ever entered from loop().                 */
/*****************************************************************************/
#define TABLE_SENDBUF 1400 // just below the usual TCP MSS of 1460

void handleTableRefresh(WebServerClass *httpServer, char actual_data[][MAXVALUELEN])
{
  // rows are built in a fixed buffer instead of concatenating one big
  // String (heap fragmentation on every 30s browser refresh)
  char rowbuf[256];
  static char sendbuf[TABLE_SENDBUF];
  size_t used = 0;

  httpServer->setContentLength(CONTENT_LENGTH_UNKNOWN);
  httpServer->send(200, "text/html");
  // index laeuft ueber die Tabellenzeilen, angezeigt wird die TOP-Nummer der Zeile
  for (unsigned int index = 0; index < NUMBEROFTOPICS; index++)
  {
    const StateTopic &topic = stateTopics[index];
    const char *topicdesc;
    if (strcmp(topic.desc[0], "value") == 0)
    {
      topicdesc = topic.desc[1];
    }
    else
    {
      // Bereichspruefung nach BEIDEN Seiten: -1 fuer unbekannte Rohwerte nach
      // unten, das Ende der Liste nach oben. Die Regel steht in desc_text()
      // (decode.h), damit der Hosttest dieselbe benutzt - siehe dort, warum
      // ein zu grosser Index das Geraet zum Absturz bringen konnte.
      topicdesc = desc_text(topic.desc, atoi(actual_data[index]));
    }
    if (strcmp(actual_data[index], "unused") != 0)
    {
      int written = snprintf(rowbuf, sizeof(rowbuf), "<tr><td>TOP%u</td><td>%s</td><td>%s</td><td>%s</td></tr>\n", topic.number, topic.name, actual_data[index], topicdesc);
      if (written < 0)
      {
        continue; // formatting failed, skip this row
      }
      // snprintf returns what it WOULD have written: clamp to what fits
      size_t rowlen = ((size_t)written >= sizeof(rowbuf)) ? sizeof(rowbuf) - 1 : (size_t)written;

      // flush before the row would overflow the block (+1 for the terminator)
      if (used + rowlen + 1 > sizeof(sendbuf))
      {
        sendbuf[used] = '\0';
        httpServer->sendContent(sendbuf);
        used = 0;
      }
      // a single row can never exceed the block, but stay defensive
      if (rowlen + 1 <= sizeof(sendbuf))
      {
        memcpy(sendbuf + used, rowbuf, rowlen);
        used += rowlen;
      }
    }
  }
  if (used > 0) // send whatever is left over
  {
    sendbuf[used] = '\0';
    httpServer->sendContent(sendbuf);
  }
  httpServer->sendContent("");
  httpServer->client().stop();
}

void handleReboot(WebServerClass *httpServer)
{
  httpServer->setContentLength(CONTENT_LENGTH_UNKNOWN);
  httpServer->send(200, "text/html");
  httpServer->sendContent_P(webHeader);
  httpServer->sendContent_P(refreshMeta);
  httpServer->sendContent_P(webBodyStart);

  String httptext = "<div class='w3-container w3-center'>";
  httptext = httptext + "<p>Rebooting</p>";
  httptext = httptext + "</div>";
  httpServer->sendContent(httptext);
  httpServer->sendContent_P(menuJS);
  httpServer->sendContent_P(webFooter);
  httpServer->sendContent("");
  httpServer->client().stop();
  delay(1000);
  ESP.restart();
}

void handleSettings(WebServerClass *httpServer, char *wifi_hostname, char *ota_password, char *mqtt_server, char *mqtt_port, char *mqtt_username, char *mqtt_password, char *hydraulik_switch)
{
  httpServer->setContentLength(CONTENT_LENGTH_UNKNOWN);
  httpServer->send(200, "text/html");
  httpServer->sendContent_P(webHeader);
  httpServer->sendContent_P(webBodyStart);

  String httptext = "<div class='w3-sidebar w3-bar-block w3-card w3-animate-left' style='display:none' id='leftMenu'>";
  httpServer->sendContent(httptext);
  httpServer->sendContent_P(sidebar);
  httptext = "</div>";
  httpServer->sendContent(httptext);

  // check if POST was made with save settings, if yes then save and reboot
  if (httpServer->args())
  {
    // Bis 3.15.0 stand hier eine feste Dokumentgroesse (zuletzt 1024 Byte, davor
    // 512), und die musste bei jedem neuen Feld nachgerechnet werden. Mit
    // ArduinoJson 7 waechst das Dokument elastisch - die Rechnerei entfaellt,
    // die GEFAHR aber nicht: Sie heisst jetzt nicht mehr "Groesse zu klein
    // geschaetzt", sondern "Allokation fehlgeschlagen". Der Ausgang waere
    // derselbe: serializeJson schriebe die config.json still unvollstaendig,
    // und nach dem Neustart fehlte womoeglich das MQTT-Passwort.
    //
    // Deshalb wird unten overflowed() geprueft und im Zweifel GAR NICHT
    // geschrieben. Der alte Stand ist besser als ein halber neuer: Das Geraet
    // startet nach dem Speichern neu, und ein fehlendes MQTT-Passwort faellt
    // erst auf, wenn die Hausteuerung nichts mehr bekommt.
    JsonDocument jsonDoc;
    jsonDoc["wifi_hostname"] = wifi_hostname;
    jsonDoc["ota_password"] = ota_password;
    jsonDoc["mqtt_server"] = mqtt_server;
    jsonDoc["mqtt_port"] = mqtt_port;
    jsonDoc["mqtt_username"] = mqtt_username;
    jsonDoc["mqtt_password"] = mqtt_password;
    jsonDoc["hydraulik_switch"] = hydraulik_switch;

    if (httpServer->hasArg("wifi_hostname"))
    {
      jsonDoc["wifi_hostname"] = httpServer->arg("wifi_hostname");
    }
    if (httpServer->hasArg("new_ota_password") && (httpServer->arg("new_ota_password") != NULL) && (httpServer->arg("current_ota_password") != NULL))
    {
      if (httpServer->hasArg("current_ota_password") && (strcmp(ota_password, httpServer->arg("current_ota_password").c_str()) == 0))
      {
        jsonDoc["ota_password"] = httpServer->arg("new_ota_password");
      }
      else
      {
        httptext = "<div class='w3-container w3-center'>";
        httptext = httptext + "<h3>------- wrong current password -------</h3>";
        httptext = httptext + "<h3>-- do factory reset if password lost --</h3>";
        httptext = httptext + "</div>";
        httpServer->sendContent(httptext);
        httpServer->sendContent_P(refreshMeta);
        httpServer->sendContent_P(webFooter);
        httpServer->sendContent("");
        httpServer->client().stop();
        return;
      }
    }
    if (httpServer->hasArg("mqtt_server"))
    {
      jsonDoc["mqtt_server"] = httpServer->arg("mqtt_server");
    }
    if (httpServer->hasArg("mqtt_port"))
    {
      jsonDoc["mqtt_port"] = httpServer->arg("mqtt_port");
    }
    if (httpServer->hasArg("mqtt_username"))
    {
      jsonDoc["mqtt_username"] = httpServer->arg("mqtt_username");
    }
    // empty field means: keep current password (it is no longer prefilled in the form)
    if (httpServer->hasArg("mqtt_password") && (httpServer->arg("mqtt_password").length() > 0))
    {
      jsonDoc["mqtt_password"] = httpServer->arg("mqtt_password");
    }
    // Anders als beim Passwort ist ein LEERES Feld hier eine gueltige Eingabe:
    // Sie schaltet die Hydraulikumschaltung ab (der Notbetrieb bricht dann im
    // ersten Schritt ab, statt sie ungeprueft zu lassen). Wer den Switch
    // ausbaut, muss das Feld leeren koennen.
    if (httpServer->hasArg("hydraulik_switch"))
    {
      jsonDoc["hydraulik_switch"] = httpServer->arg("hydraulik_switch");
    }

    // Nicht schreiben, wenn eine Allokation fehlgeschlagen ist (Begruendung
    // oben beim Anlegen des Dokuments). Faellt der Code hier durch, bleibt die
    // bisherige config.json stehen, es wird NICHT neu gestartet, und der
    // Browser bekommt die Settings-Seite mit den alten Werten zurueck - der
    // Nutzer sieht also, dass nichts uebernommen wurde.
    if (!jsonDoc.overflowed() && LittleFS.begin(true)) // format on first mount
    {
      File configFile = LittleFS.open("/config.json", "w");
      if (configFile)
      {
        serializeJson(jsonDoc, configFile);
        configFile.close();
        delay(1000);

        httptext = "<div class='w3-container w3-center'>";
        httptext = httptext + "<h3>--- saved ---</h3>";
        httptext = httptext + "<h3>-- rebooting --</h3>";
        httptext = httptext + "</div>";
        httpServer->sendContent(httptext);
        httpServer->sendContent_P(refreshMeta);
        httpServer->sendContent_P(webFooter);
        httpServer->sendContent("");
        httpServer->client().stop();
        delay(1000);
        ESP.restart();
      }
    }
  }

  httptext = "<div class='w3-container w3-center'>";
  httptext = httptext + "<h2>Settings</h2>";
  httptext = httptext + "<form action='/settings' method='POST'>";
  httptext = httptext + "Hostname:<br>";
  httptext = httptext + "<input type='text' name='wifi_hostname' value='" + wifi_hostname + "'>";
  httptext = httptext + "<br><br>";
  httptext = httptext + "Current update password:<br>";
  httptext = httptext + "<input type='password' name='current_ota_password' value=''>";
  httptext = httptext + "<br><br>";
  httptext = httptext + "New update password:<br>";
  httptext = httptext + "<input type='password' name='new_ota_password' value=''>";
  httptext = httptext + "<br><br>";
  httptext = httptext + "Mqtt server:<br>";
  httptext = httptext + "<input type='text' name='mqtt_server' value='" + mqtt_server + "'>";
  httptext = httptext + "<br><br>";
  httptext = httptext + "Mqtt port:<br>";
  httptext = httptext + "<input type='number' name='mqtt_port' value='" + mqtt_port + "'>";
  httptext = httptext + "<br><br>";
  httptext = httptext + "Mqtt username:<br>";
  httptext = httptext + "<input type='text' name='mqtt_username' value='" + mqtt_username + "'>";
  httptext = httptext + "<br><br>";
  // never render the stored password into the page source; empty = keep current
  httptext = httptext + "Mqtt password (leave empty to keep current):<br>";
  httptext = httptext + "<input type='password' name='mqtt_password' value=''>";
  httptext = httptext + "<br><br>";
  // Der Tasmota-Switch der Hydraulik. Steht er nicht drin, laesst sich der
  // Notbetrieb nicht ausloesen - das ist Absicht, siehe notbetrieb.cpp.
  httptext = httptext + "Hydraulik switch (Tasmota, IP or hostname, empty = off):<br>";
  httptext = httptext + "<input type='text' name='hydraulik_switch' value='" + hydraulik_switch + "'>";
  httptext = httptext + "<br><br>";
  httptext = httptext + "<input class='w3-green w3-button' type='submit' value='Save and reboot'>";
  httptext = httptext + "</form>";
  httptext = httptext + "</div>";
  httpServer->sendContent(httptext);

  httpServer->sendContent_P(menuJS);
  httpServer->sendContent_P(webFooter);
  httpServer->sendContent("");
  httpServer->client().stop();
}

/*****************************************************************************/
/* Notbetrieb - die Seite mit dem einen Knopf                                */
/*                                                                           */
/* Die Bedienung ist fuer Laien: "Notbetrieb einschalten" statt "HeatingMode  */
/* auf 0". Wer hier steht, hat einen Ausfall der Kaskadensteuerung vor sich   */
/* und braucht keine Protokollnamen.                                          */
/*                                                                           */
/* Der Knopf ist ein POST-Formular, kein Link: ein Klick, keine Rueckfrage -  */
/* aber auch kein versehentliches Ausloesen durch Link-Vorschau, Virenscanner */
/* oder einen Browser, der die Seite aus der History vorlaedt.                */
/*****************************************************************************/

/*****************************************************************************/
/* Die Saetze der Seite stehen EINMAL hier                                   */
/*                                                                           */
/* Sie werden an zwei Stellen gebraucht: beim Aufbau der Seite (C++) und beim */
/* Nachfuehren alle zwei Sekunden (JavaScript). Als Makro fuegt der Praepro-  */
/* zessor denselben Text in beide Literale ein - zwei Fassungen desselben     */
/* Satzes koennten sonst auseinanderlaufen, und ausgerechnet die Notfallseite */
/* wuerde je nach Zeitpunkt etwas anderes sagen.                              */
/*                                                                           */
/* Mit Umlauten seit 3.13.0 (webHeader traegt jetzt ein charset). Diese Seite */
/* liest im Ernstfall jemand aus der Familie, nicht der Entwickler - "Der    */
/* Notbetrieb ist nur im Modus Heizen moeglich" ist zwar verstaendlich, aber */
/* eine Notfallseite sollte nicht aussehen, als sei sie halbfertig. Die      */
/* uebrigen Seiten (Home, Settings, Firmware) bleiben unveraendert.          */
/*****************************************************************************/
#define NB_TXT_NUR_HEIZEN "Der Notbetrieb ist nur im Modus Heizen möglich"
#define NB_TXT_HEIZEN_HINWEIS "Bitte den Heiz-/Kühlschalter im Haus auf Heizen stellen. Der Knopf gibt sich danach von selbst frei, das dauert bis zu 10 Sekunden."
#define NB_TXT_WERTE_FEHLEN "Der Steuerung fehlen Werte, die sie zum Umschalten braucht. Das passiert, wenn das Gerät neu gestartet ist, während der Server im Keller aus war."
#define NB_TXT_PLAN_B "Bitte nach der ausgedruckten Anleitung am Bedienfeld der Wärmepumpe weitermachen."

/*****************************************************************************/
/* Die Meldung, wenn die Hydraulik nicht umschaltet (3.15.0)                 */
/*                                                                           */
/* Sie tritt an die Stelle des generischen "Hat nicht geklappt" - und zwar    */
/* nur bei diesem einen Abbruchgrund. Der Wortlaut ist vom Owner vorgegeben   */
/* (2026-08-26): Wer vor der Seite steht, soll wissen, WO der Schalter haengt */
/* und was zu tun ist. Ein Verweis auf die Anleitung am Bedienfeld waere hier */
/* falsch - die Wärmepumpe ist gar nicht angefasst worden.                    */
/*                                                                           */
/* Der zweite Satz ist der wichtigere Teil des Wegs zurueck: Nach ROT steht   */
/* der Knopf sofort wieder da (die Seite blendet ihn nur bei "laeuft" und     */
/* GRUEN aus). Wer den Schalter von Hand legt und wiederkommt, drueckt        */
/* erneut - der Lesevorgang meldet dann OFF, und die Folge laeuft durch.      */
/*****************************************************************************/
#define NB_TXT_HYDRAULIK "Die Umschaltung der Hydraulik ist fehlgeschlagen, bitte den Switch im Waschraum von Hand auf AUS schalten"
#define NB_TXT_HYDRAULIK_DANACH "Danach diesen Knopf noch einmal drücken. An der Wärmepumpe ist nichts verstellt worden."

/*****************************************************************************/
/* Die Kurvenwarnung - ein Hinweis, keine Sperre                             */
/*                                                                           */
/* Die Regel steht in notbetrieb.h: Eine Heizkurve faellt mit steigender     */
/* Aussentemperatur. Sind "VL kalt" und "VL warm" vertauscht, liegen die     */
/* Werte trotzdem alle im erlaubten Bereich - kein Bereichstest schlaegt an. */
/* Deshalb dieser Hinweis, und deshalb NUR ein Hinweis: Der Knopf bleibt     */
/* bedienbar, weil ein Notbetrieb mit verdrehter Kurve immer noch besser ist */
/* als keiner. Die Sperrfarbe (orange) bleibt der echten Sperre vorbehalten. */
/*****************************************************************************/
#define NB_TXT_KURVE_VORLAUF "Die Heizkurve sieht vertauscht aus: Der Vorlauf bei Kälte müsste höher sein als der bei Wärme. Der Knopf funktioniert trotzdem - bitte die vier Werte in der Hausteuerung nachsehen."
#define NB_TXT_KURVE_AUSSEN "Die beiden Außentemperaturen der Heizkurve passen nicht zusammen: Die kalte müsste unter der warmen liegen. Der Knopf funktioniert trotzdem - bitte die vier Werte in der Hausteuerung nachsehen."

// Alle zwei Sekunden den Kurzstatus holen und daraus Klartext machen. Die
// Antwort ist "Zustand;Schritt;Schritte;fehlendMaske;Sperre" plus die vier
// hinten angehaengten Felder (Lage, Dauertext, Kurvenwarnung, Abbruchgrund) -
// so kurz wie moeglich, weil das Geraet nebenher die Waermepumpe abfragt.
//
// Die Seite fuehrt Knopf UND Sperrhinweis nach, nicht nur das Ergebnis: Steht
// die Anlage auf Kuehlen und jemand legt den KNX-Schalter um, gibt sich der
// Knopf von selbst frei. Ein Neuladen von Hand waere hier eine Falle - TOP101
// folgt dem Schalter erst nach bis zu 7,7 s (gemessen 2026-08-16), wer sofort
// neu laedt, saehe die Sperre ein zweites Mal.
static const char notbetriebJS[] PROGMEM =
    "<script>"
    "function nbFehlt(m){var l='';for(var i=0;i<nbNamen.length;i++){if(m&(1<<i))l+='<li>'+nbNamen[i]+'</li>';}return l;}"
    "function nbSperrtext(sp,m){"
    "if(sp==2)return '<h3>" NB_TXT_NUR_HEIZEN "</h3><p>" NB_TXT_HEIZEN_HINWEIS "</p>';"
    "return '<h3>Nicht bereit</h3><p>" NB_TXT_WERTE_FEHLEN "</p><p>Fehlt:</p><ul>'+nbFehlt(m)+'</ul><p>" NB_TXT_PLAN_B "</p>';}"
    "function nbState(){fetch('/notbetrieb/status').then(r=>r.text()).then(t=>{"
    "var p=t.trim().split(';');var z=parseInt(p[0]);var s=parseInt(p[1]);var n=parseInt(p[2]);"
    "var m=parseInt(p[3]);var sp=parseInt(p[4]);"
    // Index 7: Plausibilitaet der Kurve (0 = ok, 1 = Vorlauf, 2 = Aussenpunkte).
    // Index 8: Warum der letzte Lauf abgebrochen wurde (3 = Hydraulik).
    //
    // Beide haengen HINTEN an, und das ist keine Kosmetik: Die Indizes 5 und 6
    // (Lage und Dauer der Verbindung) liest die STARTSEITE ueber dieselbe
    // Route (verbindungJS). Ein Einschub in der Mitte haette sie verschoben.
    "var kw=parseInt(p[7]);"
    "var ag=parseInt(p[8]);"
    // Felder 5 und 6: Lage der Verbindung zur Hausteuerung und die Dauer als
    // fertiger Text. true = auch "verbunden" anzeigen, siehe verbindungJS.
    "vbSetzen(parseInt(p[5]),p[6],true);"
    "var e=document.getElementById('nbstat');var f=document.getElementById('nbform');"
    "var g=document.getElementById('nbsperre');var k=document.getElementById('nbwarn');"
    // "bis zu anderthalb Minuten" deckt beide Rollen ab: Der Heizen-Lauf
    // braucht seit 3.18.0 80 s (zehn Schritte), der Warmwasser-Lauf 48 s. Die
    // Angabe steht bewusst ueber der laengeren der beiden - wer laenger wartet
    // als angekuendigt, glaubt an einen Fehler, wo keiner ist.
    "if(z==1){e.className='w3-panel w3-yellow';e.innerHTML='<h3>Konfiguration Notbetrieb läuft</h3><p>Schritt '+s+' von '+n+'. Bitte warten, das dauert bis zu anderthalb Minuten.</p>';}"
    // Der Wortlaut bei GRUEN ist vom Familienrat vorgegeben (2026-08-29).
    // Der frueher hier stehende KNX-Hinweis ist bewusst raus: Wer im Notbetrieb
    // vor der Seite steht, soll nur zwei Dinge wissen - wo die Temperatur
    // nachzustellen ist und dass die Anlage von selbst zurueckkehrt.
    //
    // Der Hinweis hat auch seinen Anlass verloren: Seit dem 2026-08-29 steht
    // die Kompressorfreigabe dauerhaft an (Owner-Entscheid, KNX-Kanal bleibt
    // und wird nur noch fuer Wartung geoeffnet). "GRUEN, aber 0 Hz mangels
    // Freigabe" ist damit der Wartungsfall und kein Regelfall mehr - er steht
    // in Ablauf-Notbetrieb.md und Analyse-Relais-statt-KNX.md Abschnitt 13.
    "else if(z==2){e.className='w3-panel w3-green';e.innerHTML='<h3>GRÜN</h3><p>Der Notbetrieb ist aktiviert.</p><p>Die Temperatur lässt sich am Display im Waschraum in kleinen Schritten einstellen.</p><p>Sobald die Steuerung wieder aktiv ist, kehrt die Wärmepumpe in den Normalbetrieb zurück.</p>';}"
    // Bei ROT entscheidet der Abbruchgrund, was zu tun ist: Bleibt die
    // Hydraulik auf 2-stufig, fuehrt der Weg ueber den Schalter im Waschraum
    // und NICHT ueber das Bedienfeld der Waermepumpe - dort ist nichts
    // verstellt worden, weil der Schritt ganz vorn steht.
    "else if(z==3){e.className='w3-panel w3-red';e.innerHTML=(ag==3)"
    "?'<h3>ROT</h3><p>" NB_TXT_HYDRAULIK "</p><p>" NB_TXT_HYDRAULIK_DANACH "</p>'"
    ":'<h3>ROT</h3><p>Hat nicht geklappt. " NB_TXT_PLAN_B "</p>';}"
    "else{e.className='';e.innerHTML='';}"
    // Waehrend ein Lauf unterwegs ist und nach GRUEN steht weder Knopf noch
    // Sperrhinweis - dort gibt es nichts zu entscheiden. Nach ROT kommt beides
    // zurueck: War die Betriebsart der Grund, steht der Grund jetzt darunter.
    "var zeigen=(z!=1&&z!=2);"
    "if(!zeigen){f.style.display='none';g.style.display='none';}"
    "else if(sp==0){g.style.display='none';f.style.display='block';}"
    "else{f.style.display='none';g.style.display='block';g.innerHTML=nbSperrtext(sp,m);}"
    // Der Hinweis haengt nicht an der Sperre: Er gilt auch dann, wenn der Knopf
    // frei ist - das ist sogar sein wichtigster Fall.
    "if(zeigen&&kw>0){k.style.display='block';"
    "k.innerHTML='<h3>Kurve prüfen</h3><p>'+(kw==1?'" NB_TXT_KURVE_VORLAUF "':'" NB_TXT_KURVE_AUSSEN "')+'</p>';}"
    "else{k.style.display='none';}"
    "}).catch(()=>{}).finally(()=>{setTimeout(nbState,2000)})}"
    "document.addEventListener('DOMContentLoaded',nbState);"
    "</script>";

void handleNotbetrieb(WebServerClass *httpServer)
{
  httpServer->setContentLength(CONTENT_LENGTH_UNKNOWN);
  httpServer->send(200, "text/html");
  httpServer->sendContent_P(webHeader);
  httpServer->sendContent_P(webBodyStart);
  httpServer->sendContent_P(menuJS);

  // Die Namen der gehaltenen Werte fuer das Nachfuehren: Die Statusroute
  // schickt nur eine Bitmaske, damit sie kurz bleibt. Ohne diese Liste koennte
  // die Seite den fehlenden Wert nicht beim Namen nennen.
  String httptext = "<script>var nbNamen=[";
  const unsigned wertanzahl = notbetrieb_wert_anzahl(notbetriebRolle);
  for (unsigned i = 0; i < wertanzahl; i++)
  {
    if (i > 0)
      httptext += ",";
    httptext += "'";
    httptext += notbetrieb_wert_name(notbetriebRolle, i);
    httptext += "'";
  }
  httptext += "];</script>";
  httpServer->sendContent(httptext);

  // verbindungJS VOR notbetriebJS: nbState() ruft vbSetzen() auf. Die
  // Reihenfolge ist hier egal, weil beides Funktionsdeklarationen sind und
  // erst nach DOMContentLoaded laeuft - sie steht trotzdem so da, damit beim
  // Lesen keine Frage offen bleibt.
  httpServer->sendContent_P(verbindungJS);
  httpServer->sendContent_P(notbetriebJS);
  httpServer->sendContent("</head><body>");

  httptext = "<div class='w3-sidebar w3-bar-block w3-card w3-animate-left' style='display:none' id='leftMenu'>";
  httpServer->sendContent(httptext);
  httpServer->sendContent_P(sidebar);
  httpServer->sendContent("<hr></div>");

  const bool wasser = (notbetriebRolle == NOTBETRIEB_WASSER);
  const NotbetriebSperre sperre = notbetrieb_sperre();

  httptext = "<div class='w3-container'>";
  httptext += wasser ? "<h2>Notbetrieb: Warmwasser</h2>" : "<h2>Notbetrieb: Heizen</h2>";
  httptext += wasser
                  ? "<p>Dieser Knopf stellt die Wärmepumpe auf reinen Warmwasserbetrieb "
                    "und schaltet sie ein. Sie arbeitet danach ohne die Hausteuerung weiter.</p>"
                  : "<p>Dieser Knopf stellt die Wärmepumpe auf ihre eigene Heizkurve "
                    "und schaltet sie ein. Sie heizt danach ohne die Hausteuerung weiter.</p>";

  // Die Verbindungszeile steht GANZ OBEN, vor dem Knopf: Sie ist die Auskunft,
  // aus der sich die Entscheidung ergibt, ob der Knopf ueberhaupt gebraucht
  // wird. Auf dieser Seite steht sie auch im Normalfall da (zeigeOk = true) -
  // ein ruhiges "verbunden" verhindert die haeufigere Fehlentscheidung.
  verbindungszeile(httptext, true);
  httpServer->sendContent(httptext);

  // Knopf und Sperrhinweis stehen BEIDE in der Seite; sichtbar ist immer nur
  // einer. Das Nachfuehren alle zwei Sekunden schaltet zwischen ihnen um -
  // waere der Knopf gar nicht erst da, koennte die Seite ihn nicht freigeben,
  // wenn der KNX-Schalter umgelegt wird, und jemand muesste raten, wann ein
  // Neuladen sich lohnt. Dass er im Quelltext steht, ist keine Luecke: Der
  // POST-Handler prueft die Sperre selbst (notbetrieb_starten()).
  httptext = "<form id='nbform' method='POST' action='/notbetrieb/start'";
  httptext += (sperre == NOTBETRIEB_FREI) ? ">" : " style='display:none'>";
  // BLAU und nicht rot (Owner-Entscheidung 2026-08-21): Auf dieser Seite hiess
  // Rot bis 3.12.0 zweierlei - der Knopf war rot im Sinne von "druck mich",
  // das Ergebnisfeld ROT im Sinne von "hat nicht geklappt". Das hat prompt zu
  // einer Verwechslung gefuehrt. Fuer eine Seite, deren ganze Rueckmeldung aus
  // einer Ampel besteht, darf Rot nur eines bedeuten. Auffaellig bleibt der
  // Knopf ueber Groesse und Polsterung, nicht ueber die Farbe.
  httptext += "<button class='w3-button w3-blue w3-xlarge w3-padding-large' type='submit'>";
  httptext += wasser ? "Warmwasser einschalten" : "Notbetrieb einschalten";
  httptext += "</button></form>";

  // Der Sperrhinweis wird hier schon vollstaendig aufgebaut und nicht dem
  // JavaScript ueberlassen: Wer die Seite oeffnet, soll den Grund sofort lesen
  // koennen - auch in der Sekunde vor der ersten Statusabfrage.
  httptext += "<div id='nbsperre' class='w3-panel w3-orange'";
  httptext += (sperre == NOTBETRIEB_FREI) ? " style='display:none'>" : ">";
  if (sperre == NOTBETRIEB_SPERRE_HEIZBETRIEB)
  {
    // Der externe KNX-Schalter gibt die Richtung vor. Steht er auf Kuehlen,
    // verwirft die Waermepumpe jeden Heizmodus stillschweigend - der Lauf
    // endete am 2026-08-20 nach 20 s in einem ROT ohne Erklaerung.
    httptext += "<h3>" NB_TXT_NUR_HEIZEN "</h3><p>" NB_TXT_HEIZEN_HINWEIS "</p>";
  }
  else if (sperre != NOTBETRIEB_FREI)
  {
    // Unvollstaendig heisst gesperrt, mit Klartext. Lieber gar nicht schalten
    // als auf die Panasonic-Werkskurve - die faehrt bei -5 C aussen 55 C
    // Vorlauf, weit jenseits der Estrichgrenze einer Fussbodenheizung.
    httptext += "<h3>Nicht bereit</h3><p>" NB_TXT_WERTE_FEHLEN "</p><p>Fehlt:</p><ul>";
    for (unsigned i = 0; i < wertanzahl; i++)
    {
      if ((notbetriebWerte.gesetzt & (1u << i)) == 0)
      {
        httptext += "<li>";
        httptext += notbetrieb_wert_name(notbetriebRolle, i);
        httptext += "</li>";
      }
    }
    httptext += "</ul><p>" NB_TXT_PLAN_B "</p>";
  }
  httptext += "</div>";

  // Auch dieser Hinweis wird serverseitig fertig aufgebaut - gleiche
  // Begruendung wie beim Sperrhinweis darueber: Wer die Seite oeffnet, soll
  // ihn sofort sehen und nicht erst nach der ersten Statusabfrage.
  const NotbetriebKurvenWarnung kurvenwarnung = notbetrieb_kurvenwarnung();
  httptext += "<div id='nbwarn' class='w3-panel w3-pale-yellow'";
  httptext += (kurvenwarnung == NOTBETRIEB_KURVE_OK) ? " style='display:none'>" : ">";
  if (kurvenwarnung != NOTBETRIEB_KURVE_OK)
  {
    httptext += "<h3>Kurve prüfen</h3><p>";
    httptext += (kurvenwarnung == NOTBETRIEB_KURVE_VORLAUF_VERDREHT)
                    ? NB_TXT_KURVE_VORLAUF
                    : NB_TXT_KURVE_AUSSEN;
    httptext += "</p>";
  }
  httptext += "</div>";

  httptext += "<div id='nbstat'></div></div>";
  httpServer->sendContent(httptext);

  httpServer->sendContent_P(webFooter);
  httpServer->sendContent("");
  httpServer->client().stop();
}

/*****************************************************************************/
/* Der POST-Handler: anstossen und sofort antworten                          */
/*                                                                           */
/* Hier laeuft nichts Langes ab - die Schrittfolge tickt aus loop(). Ein      */
/* Handler, der 48 s auf die Waermepumpe wartet, wuerde den Abfragezyklus     */
/* blockieren und den Browser in einen Timeout laufen lassen.                */
/*****************************************************************************/
void handleNotbetriebStart(WebServerClass *httpServer)
{
  const bool angestossen = notbetrieb_starten();

  // Nach POST auf die Seite umleiten (303): Ein Neuladen wiederholt damit die
  // Anzeige, nicht das Kommando.
  if (!angestossen)
  {
    httpServer->sendHeader("Location", "/notbetrieb", true);
    httpServer->send(303, "text/plain", "nicht bereit");
    return;
  }
  httpServer->sendHeader("Location", "/notbetrieb", true);
  httpServer->send(303, "text/plain", "gestartet");
}

/*****************************************************************************/
/* Die Statusroute - bewusst ohne Anmeldung                                   */
/*                                                                           */
/* Sie gibt nur Zustand und Schrittzahl heraus und aendert nichts. Mit        */
/* Anmeldung muesste die Seite bei jeder Abfrage alle zwei Sekunden erneut    */
/* authentifizieren, was auf dem Geraet spuerbar ist - und wer die Zahl       */
/* "3 von 6" lesen kann, kann damit nichts anfangen, was er nicht ohnehin     */
/* auf der Startseite saehe.                                                  */
/*****************************************************************************/
void handleNotbetriebStatus(WebServerClass *httpServer)
{
  // Puffer grosszuegig: fuenf Zahlen des Notbetriebs plus Lage, Dauertext
  // ("mehr als 30 Tagen" ist der laengste, 17 Zeichen), Kurvenwarnung und
  // Abbruchgrund. Der laengste Fall liegt bei rund 35 Zeichen.
  char status[96];
  notbetrieb_status(status, sizeof(status));

  // Die Verbindungsfelder haengt DIESE Funktion an, nicht notbetrieb_status():
  // Der Notbetrieb weiss nichts von der Verbindungswacht und soll es auch
  // nicht - er funktioniert gerade dann, wenn sie Alarm schlaegt.
  //
  // Warum die Startseite dieselbe Route abfragt, obwohl "notbetrieb" darin
  // steht: Es ist die einzige Statusroute des Geraets, sie ist bewusst ohne
  // Anmeldung erreichbar, und eine zweite Route fuer zwei Felder waere der
  // teurere Weg. Format nach der Erweiterung:
  //   Zustand;Schritt;Schritte;fehlendMaske;Sperre;Lage;Dauertext;Kurvenwarnung;Abbruchgrund
  const size_t used = strlen(status);
  if (used + 1 < sizeof(status))
  {
    char dauer[32] = "";
    const VerbindungsLage lage = verbindung_lage(&hausteuerung);
    // Nur die beiden Lagen mit einer echten Dauer bekommen einen Text. Lage 3
    // ("nie verbunden") hat keine, und ein Text dort waere genau die
    // Minutenzahl, die nicht stimmt.
    if (lage == VERBINDUNG_GESTOERT || lage == VERBINDUNG_STEUERUNG_STUMM)
    {
      verbindung_dauer_text(dauer, sizeof(dauer),
                            verbindung_ausfall_sekunden(&hausteuerung),
                            verbindung_ueber_deckel(&hausteuerung));
    }
    // Kurvenwarnung und Abbruchgrund haengen HINTEN an und nicht bei den
    // Notbetriebsfeldern: Die Startseite liest Lage und Dauer ueber dieselbe
    // Route an den Indizes 5 und 6, ein Einschub in der Mitte haette beide
    // Seiten verschoben. Jedes weitere Feld gehoert aus demselben Grund ans
    // Ende.
    (void)snprintf(status + used, sizeof(status) - used, ";%u;%s;%u;%u",
                   (unsigned)lage, dauer, (unsigned)notbetrieb_kurvenwarnung(),
                   (unsigned)notbetrieb_abbruchgrund());
  }

  httpServer->send(200, "text/plain", status);
}
