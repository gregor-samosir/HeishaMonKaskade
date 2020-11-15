#include "HeishaMon.h"
#include "webfunctions.h"
#include "decode.h"
#include "commands.h"

Ticker Command_Timer(send_pana_command, COMMANDTIMER, 0, MICROS);  // loop
Ticker Query_Timer(send_pana_mainquery, QUERYTIMER, 0, MICROS); // loop
Ticker Bufferfill_Timeout(read_pana_data, BUFFERTIMEOUT, 1, MICROS); // one time
Ticker Serial_Timeout(timeout_serial, SERIALTIMEOUT, 1, MICROS); // one time

bool serialquerysent = false; // mutex for serial sending

// Default settings if config does not exists
const char *update_path = "/firmware";
const char *update_username = "admin";
char wifi_hostname[40] = "HeishaMon";
char ota_password[40] = "heisha";
char mqtt_server[40];
char mqtt_port[6] = "1883";
char mqtt_username[40];
char mqtt_password[40];

//log and debug
bool outputMqttLog = true;  // toggle to write logmessages to mqtt or telnetstream
bool outputTelnetLog = true;  // enable/disable telnet DEBUG

// global scope
char serial_data[MAXDATASIZE];
byte serial_length = 0;
unsigned int querynum = 0;

// store actual value in an String array
String actual_data[NUMBEROFTOPICS];

// log message
char log_msg[MAXDATASIZE];

// mqtt topic
char mqtt_topic[256];

unsigned int commandsInBuffer = 0;

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

WiFiClient mqtt_wifi_client;
PubSubClient mqtt_client(mqtt_wifi_client);
unsigned long lastReconnectAttempt = 0;

byte mainQuery[]   = {0x71, 0x6c, 0x01, 0x10};
byte mainCommand[] = {0xF1, 0x6c, 0x01, 0x10};

/*****************************************************************************/
/* OTA                                                                       */
/*****************************************************************************/
void setupOTA()
{
  ArduinoOTA.setPort(8266);              // Port defaults to 8266
  ArduinoOTA.setHostname(wifi_hostname); // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setPassword(ota_password);  // Set authentication
  ArduinoOTA.onStart([]() {
  });
  ArduinoOTA.onEnd([]() {
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
  });
  ArduinoOTA.onError([](ota_error_t error) {
  });
  ArduinoOTA.begin();
}

/*****************************************************************************/
/* Write to mqtt log                                                         */
/*****************************************************************************/
void write_mqtt_log(char *string)
{
  if (outputMqttLog)
  {
    mqtt_client.publish(Topics::LOG.c_str(), string);
  }
  else
  {
    TelnetStream.printf("[%02d-%02d-%02d %02d:%02d:%02d] %s\n", year(), month(), day(), hour(), minute(), second(), string);
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
/* FreeMemory (Igor Ybema)                                                   */
/*****************************************************************************/
int getFreeMemory() {
  //store total memory at boot time
  static uint32_t total_memory = 0;
  if ( 0 == total_memory ) total_memory = ESP.getFreeHeap();

  uint32_t free_memory   = ESP.getFreeHeap();
  return (100 * free_memory / total_memory ) ; // as a %
}

/*****************************************************************************/
/* WiFi Quality (Igor Ybema)                                                  */
/*****************************************************************************/
int getWifiQuality() {
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
  httpServer.on("/", []() {
    handleRoot(&httpServer);
  });
  httpServer.on("/tablerefresh", []() {
    handleTableRefresh(&httpServer, actual_data);
  });
  httpServer.on("/reboot", []() {
    handleReboot(&httpServer);
  });
  httpServer.on("/settings", []() {
    handleSettings(&httpServer, wifi_hostname, ota_password, mqtt_server, mqtt_port, mqtt_username, mqtt_password);
  });
  httpServer.on("/togglelog", []() {
    write_mqtt_log((char *)"Toggled mqtt log flag");
    outputMqttLog ^= true;
    handleRoot(&httpServer);
  });
  httpServer.on("/toggledebug", []() {
    write_mqtt_log((char *)"Toggled debug flag");
    outputTelnetLog ^= true;
    handleRoot(&httpServer);
  });
  httpServer.begin();
}

/*****************************************************************************/
/* Serial setup and switch                                                   */
/*****************************************************************************/
void setupSerial()
{
  Serial.begin(115200); //boot issue's first on normal serial
  Serial.flush();
}

void switchSerial()
{
  Serial.println("Switch serial to to heatpump. Look for debug on mqtt log topic.");
  //serial to cn-cnt
  Serial.flush();
  Serial.end();
  Serial.begin(9600, SERIAL_8E1);
  Serial.flush();
  Serial.swap();      //swap to gpio13 (D7) and gpio15 (D8)
  pinMode(5, OUTPUT); //enable gpio15 after boot using gpio5 (D1)
  digitalWrite(5, HIGH);
}

/*****************************************************************************/
/* MQTT Client reconnect                                                     */
/*****************************************************************************/
boolean mqtt_reconnect()
{
  if (mqtt_client.connect(wifi_hostname, mqtt_username, mqtt_password, Topics::WILL.c_str(), 1, true, "Offline"))
  {
    mqtt_client.publish(Topics::WILL.c_str(), "Online");

    mqtt_client.subscribe(Topics::SET1.c_str());
    mqtt_client.subscribe(Topics::SET2.c_str());
    mqtt_client.subscribe(Topics::SET3.c_str());
    mqtt_client.subscribe(Topics::SET4.c_str());
    mqtt_client.subscribe(Topics::SET5.c_str());
    mqtt_client.subscribe(Topics::SET6.c_str());
    mqtt_client.subscribe(Topics::SET7.c_str());
    mqtt_client.subscribe(Topics::SET8.c_str());
    mqtt_client.subscribe(Topics::SET9.c_str());
    mqtt_client.subscribe(Topics::SET10.c_str());
    mqtt_client.subscribe(Topics::SET11.c_str());
    mqtt_client.subscribe(Topics::SET12.c_str());
    mqtt_client.subscribe(Topics::SET13.c_str());
    mqtt_client.subscribe(Topics::SET14.c_str());
    mqtt_client.subscribe(Topics::SET15.c_str());
    mqtt_client.subscribe(Topics::SET16.c_str());
    mqtt_client.subscribe(Topics::SET17.c_str());
    mqtt_client.subscribe(Topics::SET18.c_str());
    mqtt_client.subscribe(Topics::SET19.c_str());
  }
  return mqtt_client.connected();
}

/*****************************************************************************/
/* MQTT Callback                                                             */
/*****************************************************************************/
void mqtt_callback(char *topic, byte *payload, unsigned int length)
{
  char msg[length + 1];
  for (unsigned int i = 0; i < length; i++)
  {
    msg[i] = (char)payload[i];
  }
  msg[length] = '\0';
  build_heatpump_command(topic, msg);
}

/*****************************************************************************/
/* MQTT Setup                                                                */
/*****************************************************************************/
void setupMqtt()
{
  mqtt_client.setServer(mqtt_server, atoi(mqtt_port));
  mqtt_client.setCallback(mqtt_callback);
  mqtt_reconnect();
}

/*****************************************************************************/
/* Validate checksum                                                         */
/*****************************************************************************/
bool validate_checksum()
{
  byte chk = 0;
  for (int i = 0; i < serial_length; i++)
  {
    chk += serial_data[i];
  }
  return (chk == 0); //all received bytes + checksum should result in 0
}

/*****************************************************************************/
/* Read raw from serial                                                 */
/*****************************************************************************/
bool readSerial()
{
  while (Serial.available() > 0)
  {
    serial_data[serial_length] = Serial.read();
    serial_length += 1;
    // only enable next line to DEBUG
    // sprintf(log_msg, "DEBUG Receive byte : %d", serial_length); write_telnet_log(log_msg);
  }
  if (serial_length > 0)
  { // received length part of header now
    if (serial_length > (serial_data[1] + 3))
    {
      write_telnet_log((char *)"Received data longer than header suggests");
      serial_length = 0;
      return false;
    }

    if (serial_length == (serial_data[1] + 3))
    {
      if (!validate_checksum())
      {
        write_telnet_log((char *)"Checksum not valid");
        serial_length = 0;
        return false;
      }
      write_telnet_log((char *)"Receive valid data from serial");
      serial_length = 0;
      return true;
    }
    sprintf(log_msg, "Receive partial datagram %d, please fix Bufferfill_Timeout", serial_length); write_telnet_log(log_msg);
  }
  return false;
}

/*****************************************************************************/
/* Write to buffer
/* hold only the command topic name
/* commands stored in mainCommand[]
/*****************************************************************************/
void push_command_buffer()
{
  if (commandsInBuffer < MAXCOMMANDSINBUFFER)
  {
    commandsInBuffer++;
    sprintf(log_msg, "Command %d registered", commandsInBuffer); write_telnet_log(log_msg);
  }
  else
  {
    write_telnet_log((char *)"Buffer full. Ignoring last command");
  }
}

/*****************************************************************************/
/* Send commands from buffer to pana  (called from loop)                     */
/* send the set command global mainCommand[]
/* chk calculation must be on each time we send
/*****************************************************************************/
void send_pana_command()
{
  if (commandsInBuffer > 0)
  {
    int status = Command_Timer.state();
    if (status == RUNNING) {
      //write_telnet_log((char *)"Command Timmer pause");
      Command_Timer.pause(); // resume after serial read and decode 
    }   
    // checksum
    byte chk = 0;
    for (int i = 0; i < MAINQUERYSIZE; i++)
    {
      chk += mainCommand[i];
    }
    chk = (chk ^ 0xFF) + 01;
    
    unsigned int bytesSent = Serial.write(mainCommand, MAINQUERYSIZE);
    bytesSent +=  Serial.write(chk);
    
    sprintf(log_msg, "Command %d send with %d bytes", commandsInBuffer, bytesSent); write_telnet_log(log_msg);
    
    commandsInBuffer--;

    serialquerysent = true;
    Bufferfill_Timeout.start();
    Serial_Timeout.start();
  }
}

/*****************************************************************************/
/* Send query to buffer  (called from loop)                                    
/* only to trigger the next query if we have no new set command on buffer 
/*****************************************************************************/
void send_pana_mainquery()
{
  if (commandsInBuffer == 0 && serialquerysent == false)
  {
    querynum += 1;
    sprintf(log_msg, "Inject Query %d", querynum); write_telnet_log(log_msg);
    push_command_buffer();
  }
}


/*****************************************************************************/
/* Read from pana and decode (call from loop)                           */
/*****************************************************************************/
void read_pana_data()
{
  if (serialquerysent == true) //only read if we have sent a command so we expect an answer
  {
    if (readSerial() == true)
    {
      write_telnet_log((char *)"Decode topics ---------- Start ------------------");
      publish_heatpump_data(serial_data, actual_data, mqtt_client);    
      write_telnet_log((char *)"Decode topics ---------- End --------------------\n");
      serialquerysent = false;
      //write_telnet_log((char *)"Command Timmer resume\n");
      Command_Timer.resume();
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
    serialquerysent = false; //we are allowed to send a new command
    Command_Timer.resume();
    write_telnet_log((char *)"Serial read timeout");
  }
}

/*****************************************************************************/
/* handle telnet stream                                                      */
/*****************************************************************************/
void handle_telnetstream()
{
  if (TelnetStream.available() > 0)
  {
    switch (TelnetStream.read()) {
    case 'R':
      TelnetStream.stop();
      delay(100);
      ESP.reset();
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
void setupTime() {
  configTime(TIME_ZONE, "pool.ntp.org");
  delay(250);
  time_t now = time(nullptr);
  while (now < SECS_YR_2000) {
    delay(100);
    now = time(nullptr);
  }
  setTime(now+3600); // FIX CEST dont work
}

/*****************************************************************************/
/* main                                                                      */
/*****************************************************************************/
void setup()
{
  getFreeMemory();
  setupSerial();
  setupWifi(wifi_hostname, ota_password, mqtt_server, mqtt_port, mqtt_username, mqtt_password);
  MDNS.begin(wifi_hostname);
  setupOTA();
  setupMqtt();
  setupHttp();
  switchSerial();
  
  setupTime();
  TelnetStream.begin();

  Query_Timer.start();
  Command_Timer.start();
  
  lastReconnectAttempt = 0;
}

void loop()
{
  ArduinoOTA.handle();
  httpServer.handleClient();
  MDNS.update();

  handle_telnetstream();  

  if (!mqtt_client.connected())
  {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > 5000)
    {
      lastReconnectAttempt = now;
      if (mqtt_reconnect())
      {
        lastReconnectAttempt = 0;
      }
    }
  }
  else
  {
    mqtt_client.loop(); // Trigger the mqtt_callback and send the set command to the buffer
  }

  Command_Timer.update(); // trigger send_pana_command()   - send command or query from buffer
  Query_Timer.update();   // trigger send_pana_mainquery() - send query to buffer if no command in buffer
  
  Bufferfill_Timeout.update(); // trigger read_pana_data() - read from serial, decode bytes and publish to mqtt
  Serial_Timeout.update();     // trigger timeout_serial() - stop read from serial after timeout
}
