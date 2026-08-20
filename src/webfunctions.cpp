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
static const char webHeader[] PROGMEM = "<!DOCTYPE html><html><title>Heisha monitor</title><head><meta name='viewport' content='width=device-width, initial-scale=1'><style>"
    "*{box-sizing:border-box}body{font-family:Verdana,sans-serif;margin:0}h4{margin:10px 0}"
    ".w3-theme,.w3-blue{background:#2196F3;color:#fff}.w3-green{background:#4CAF50;color:#fff}"
    ".w3-container{padding:0.01em 16px}.w3-card-4{box-shadow:0 4px 10px 0 rgba(0,0,0,.2)}"
    ".w3-card{box-shadow:0 2px 5px 0 rgba(0,0,0,.2)}.w3-center{text-align:center}.w3-left{float:left}"
    ".w3-small{font-size:12px}.w3-medium{font-size:15px}.w3-text-grey{color:#757575}"
    ".w3-button{border:none;padding:8px 16px;cursor:pointer;background:inherit;display:inline-block}"
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

void setupWifi(char *wifi_hostname, char *ota_password, char *mqtt_server, char *mqtt_port, char *mqtt_username, char *mqtt_password)
{
  // Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  wifiManager.setDebugOutput(true); // this is debugging on serial port, because serial swap is done after full startup this is ok

  Serial.println("mounting LittleFS...");

#if defined(ESP32)
  if (LittleFS.begin(true)) // ESP32: format on first mount
#else
  if (LittleFS.begin())
#endif
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
        DynamicJsonDocument jsonDoc(1024);
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

  // Set hostname on wifi rather than ESP_xxxxx
#if defined(ESP32)
  WiFi.setHostname(wifi_hostname);
  // modem sleep breaks inbound connections on ESP32 (ping/http time out
  // while outbound mqtt keeps working) - disable it, device is mains powered
  WiFi.setSleep(false);
#else
  WiFi.hostname(wifi_hostname);
#endif

  // save the custom parameters to FS
  if (shouldSaveConfig)
  {
    Serial.println("Save config");
    DynamicJsonDocument jsonDoc(1024);
    jsonDoc["wifi_hostname"] = wifi_hostname;
    jsonDoc["ota_password"] = ota_password;
    jsonDoc["mqtt_server"] = mqtt_server;
    jsonDoc["mqtt_port"] = mqtt_port;
    jsonDoc["mqtt_username"] = mqtt_username;
    jsonDoc["mqtt_password"] = mqtt_password;

    File configFile = LittleFS.open("/config.json", "w");
    if (!configFile)
    {
      Serial.println("Failed to open config file for writing");
    }

    serializeJson(jsonDoc, Serial);
    serializeJson(jsonDoc, configFile);
    configFile.close();
    // end save
  }

  Serial.println("local ip");
  Serial.println(WiFi.localIP());
}

void handleRoot(WebServerClass *httpServer)
{
  httpServer->setContentLength(CONTENT_LENGTH_UNKNOWN);
  httpServer->send(200, "text/html");
  httpServer->sendContent_P(webHeader);
  httpServer->sendContent_P(webBodyStart);
  httpServer->sendContent_P(menuJS);
  httpServer->sendContent_P(refreshJS);

  String httptext = "<div class='w3-sidebar w3-bar-block w3-card w3-animate-left' style='display:none' id='leftMenu'>";
  httpServer->sendContent(httptext);
  httpServer->sendContent_P(sidebar);
  httptext = "<hr><div class='w3-text-grey w3-small'>Version: ";
  httptext = httptext + heishamon_version + "<br><a href = 'https://github.com/gregor-samosir/HeishaMonKaskade '>Heishamon</a></div><hr></div>";
  httptext = httptext + "<br><div class='w3-container'><table class = 'w3-table-all w3-card-4 w3-small'><thead><tr class = 'w3-blue'><th>Topic</th><th>Name</th><th>Value</th><th>Description</th></tr></thead><tbody id =\"heishavalues\"><tr><td>... Loading...</td><td>.</td><td>.</td><td>.</td></tr></tbody></table></div>";
  httpServer->sendContent(httptext);
  httpServer->sendContent_P(webFooter);
  httpServer->sendContent("");
  httpServer->client().stop();
}

/*****************************************************************************/
/* Table rows, collected into TCP-sized blocks                               */
/*                                                                           */
/* One sendContent() per row used to cost about one network round trip on    */
/* the ESP32 (~20 ms each): its core pushes every write out as its own       */
/* packet and waits for the ack, so 99 rows added up to ~1.9 s of "Loading". */
/* The ESP8266 core coalesces writes and never showed the effect. Filling a  */
/* buffer close to the TCP segment size first turns those 99 sends into      */
/* about six on both platforms.                                              */
/*                                                                           */
/* sendbuf is static on purpose: 1400 bytes would be a large share of the    */
/* ESP8266 stack, and this handler is only ever entered from loop().         */
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

void handleSettings(WebServerClass *httpServer, char *wifi_hostname, char *ota_password, char *mqtt_server, char *mqtt_port, char *mqtt_username, char *mqtt_password)
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
    DynamicJsonDocument jsonDoc(512);
    jsonDoc["wifi_hostname"] = wifi_hostname;
    jsonDoc["ota_password"] = ota_password;
    jsonDoc["mqtt_server"] = mqtt_server;
    jsonDoc["mqtt_port"] = mqtt_port;
    jsonDoc["mqtt_username"] = mqtt_username;
    jsonDoc["mqtt_password"] = mqtt_password;

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

  #if defined(ESP32)
  if (LittleFS.begin(true)) // ESP32: format on first mount
#else
  if (LittleFS.begin())
#endif
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

// Alle zwei Sekunden den Kurzstatus holen und daraus Klartext machen. Die
// Antwort ist "Zustand;Schritt;Schritte;fehlendMaske" - so kurz wie moeglich,
// weil der ESP8266 nebenher die Waermepumpe abfragt.
static const char notbetriebJS[] PROGMEM =
    "<script>"
    "function nbState(){fetch('/notbetrieb/status').then(r=>r.text()).then(t=>{"
    "var p=t.trim().split(';');var z=parseInt(p[0]);var s=parseInt(p[1]);var n=parseInt(p[2]);"
    "var e=document.getElementById('nbstat');var f=document.getElementById('nbform');"
    "if(z==1){e.className='w3-panel w3-yellow';e.innerHTML='<h3>Laeuft...</h3><p>Schritt '+s+' von '+n+'. Bitte warten, das dauert bis zu einer Minute.</p>';if(f)f.style.display='none';}"
    "else if(z==2){e.className='w3-panel w3-green';e.innerHTML='<h3>GRUEN</h3><p>Der Notbetrieb ist eingeschaltet. Die Waermepumpe laeuft jetzt selbst weiter.</p><p>Wird es trotzdem nicht warm, fehlt die KNX-Freigabe fuer den Kompressor - siehe Anleitung.</p>';if(f)f.style.display='none';}"
    "else if(z==3){e.className='w3-panel w3-red';e.innerHTML='<h3>ROT</h3><p>Hat nicht geklappt. Bitte nach der ausgedruckten Anleitung am Bedienfeld der Waermepumpe weitermachen.</p>';if(f)f.style.display='block';}"
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
  httpServer->sendContent_P(notbetriebJS);
  httpServer->sendContent("</head><body>");

  String httptext = "<div class='w3-sidebar w3-bar-block w3-card w3-animate-left' style='display:none' id='leftMenu'>";
  httpServer->sendContent(httptext);
  httpServer->sendContent_P(sidebar);
  httpServer->sendContent("<hr></div>");

  const bool wasser = (notbetriebRolle == NOTBETRIEB_WASSER);
  const bool bereit = notbetrieb_vollstaendig(&notbetriebWerte, notbetriebRolle);

  httptext = "<div class='w3-container'>";
  httptext += wasser ? "<h2>Notbetrieb: Warmwasser</h2>" : "<h2>Notbetrieb: Heizen</h2>";
  httptext += wasser
                  ? "<p>Dieser Knopf stellt die Waermepumpe auf reinen Warmwasserbetrieb "
                    "und schaltet sie ein. Sie arbeitet danach ohne die Hausteuerung weiter.</p>"
                  : "<p>Dieser Knopf stellt die Waermepumpe auf ihre eigene Heizkurve "
                    "und schaltet sie ein. Sie heizt danach ohne die Hausteuerung weiter.</p>";
  httpServer->sendContent(httptext);

  if (bereit)
  {
    // Der Knopf. Ein Klick, keine Rueckfrage - in dieser Lage waere eine
    // Sicherheitsabfrage nur eine weitere Huerde vor einem kalten Haus.
    httptext = "<form id='nbform' method='POST' action='/notbetrieb/start'>";
    httptext += "<button class='w3-button w3-red w3-xlarge w3-padding-large' type='submit'>";
    httptext += wasser ? "Warmwasser einschalten" : "Notbetrieb einschalten";
    httptext += "</button></form>";
  }
  else
  {
    // Unvollstaendig heisst gesperrt, mit Klartext. Lieber gar nicht schalten
    // als auf die Panasonic-Werkskurve - die faehrt bei -5 C aussen 55 C
    // Vorlauf, weit jenseits der Estrichgrenze einer Fussbodenheizung.
    httptext = "<div class='w3-panel w3-orange'><h3>Nicht bereit</h3>";
    httptext += "<p>Der Steuerung fehlen Werte, die sie zum Umschalten braucht. "
                "Das passiert, wenn das Geraet neu gestartet ist, waehrend der "
                "Server im Keller aus war.</p><p>Fehlt:</p><ul>";
    const unsigned n = notbetrieb_wert_anzahl(notbetriebRolle);
    for (unsigned i = 0; i < n; i++)
    {
      if ((notbetriebWerte.gesetzt & (1u << i)) == 0)
      {
        httptext += "<li>";
        httptext += notbetrieb_wert_name(notbetriebRolle, i);
        httptext += "</li>";
      }
    }
    httptext += "</ul><p>Bitte nach der ausgedruckten Anleitung am Bedienfeld "
                "der Waermepumpe weitermachen.</p></div>";
  }
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
/* authentifizieren, was auf einem ESP8266 spuerbar ist - und wer die Zahl    */
/* "3 von 6" lesen kann, kann damit nichts anfangen, was er nicht ohnehin     */
/* auf der Startseite saehe.                                                  */
/*****************************************************************************/
void handleNotbetriebStatus(WebServerClass *httpServer)
{
  char status[48];
  notbetrieb_status(status, sizeof(status));
  httpServer->send(200, "text/plain", status);
}
