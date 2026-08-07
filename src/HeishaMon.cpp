#include "HeishaMon.h"
#include "webfunctions.h"
#include "decode.h"
#include "commands.h"

Ticker Send_Pana_Command_Timer(send_pana_command, COMMANDTIMER, 1);   // one time
Ticker Send_Pana_Mainquery_Timer(send_pana_mainquery, QUERYTIMER, 1); // one time
Ticker Read_Pana_Data_Timer(read_pana_data, BUFFERTIMEOUT, 1);        // one time
Ticker Timeout_Serial_Timer(timeout_serial, SERIALTIMEOUT, 1);        // one time

bool serialquerysent = false; // mutex for serial sending

// Default settings if config does not exists
// stage-specific hostname comes as build flag from platformio.ini, fallback = stage 1
// (config.json on the device overrides this default at boot)
#ifndef HEISHA_HOSTNAME
#define HEISHA_HOSTNAME "HeishaMon"
#endif
const char *update_path = "/firmware";
const char *update_username = "admin";
char wifi_hostname[40] = HEISHA_HOSTNAME;
char ota_password[40] = "heisha";
char mqtt_server[40];
char mqtt_port[6] = "1883";
char mqtt_username[40];
char mqtt_password[40];

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

WebServerClass httpServer(80);
HTTPUpdateServerClass httpUpdater;

WiFiClient mqtt_wifi_client;
PubSubClient mqtt_client(mqtt_wifi_client);
unsigned long lastReconnectAttempt = 0;

byte mainQuery[] = {0x71, 0x6c, 0x01, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
byte mainCommand[] = {0xF1, 0x6c, 0x01, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
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
      (void)sprintf(&buffer[3 * j], "%02X ", (unsigned char)hex[i + j]);
    }
    (void)sprintf(log_msg, "data: %s", buffer);
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
    handleSettings(&httpServer, wifi_hostname, ota_password, mqtt_server, mqtt_port, mqtt_username, mqtt_password); });
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
    mqtt_client.subscribe(Topics::SET20.c_str());
    mqtt_client.subscribe(Topics::SET21.c_str());
    mqtt_client.subscribe(Topics::SET22.c_str());
    mqtt_client.subscribe(Topics::SET23.c_str());
    mqtt_client.subscribe(Topics::SET24.c_str());
    mqtt_client.subscribe(Topics::SET25.c_str());
    mqtt_client.subscribe(Topics::SET26.c_str());
  }
  return mqtt_client.connected();
}

/*****************************************************************************/
/* MQTT Callback                                                             */
/*****************************************************************************/
void mqtt_callback(char *topic, byte *payload, unsigned int length)
{
  Send_Pana_Mainquery_Timer.stop();
  write_telnet_log((char *)"Callback from mqtt");
  char msg[length + 1];
  for (unsigned int i = 0; i < length; i++)
  {
    msg[i] = (char)payload[i];
  }
  msg[length] = '\0';

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
    (void)sprintf(log_msg, "Warning: Invalid MQTT port '%s'. Using default 1883.", mqtt_port);
    write_mqtt_log(log_msg);
    port = 1883;
  }
  mqtt_client.setServer(mqtt_server, (uint16_t)port);
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
  return (chk == 0); // all received bytes + checksum should result in 0
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

  if (serial_length == (serial_data[1] + 3))
  {
    if (!validate_checksum())
    {
      write_telnet_log((char *)"Checksum error");
      serial_length = 0;
      return false;
    }
    write_telnet_log((char *)"Valid data");
    if (outputHexLog)
      write_hex_log(serial_data, serial_length);
    serial_length = 0;
    return true;
  }

  if (serial_length > (serial_data[1] + 3))
  {
    write_telnet_log((char *)"Data longer than header suggests");
    serial_length = 0;
    return false;
  }

  sprintf(log_msg, "Partial data length %d, please fix Read_Pana_Data_Timer", serial_length);
  write_telnet_log(log_msg);
  serial_length = 0;
  return false;
}

/*****************************************************************************/
/* Register new command
/* Wait COMMANDTIMER for multible commands from mqtt
/*****************************************************************************/
void register_new_command()
{
  newcommand = true;
  Send_Pana_Command_Timer.start(); // wait countdown for multible SET commands
  write_telnet_log((char *)"Register command/query");
}

/*****************************************************************************/
/* Send commands from buffer to pana  (called from loop)
/* send the set command global mainCommand[]
/* chk calculation must be on each time we send
/*****************************************************************************/
void send_pana_command()
{
  if (newcommand == true)
  {
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
    case 'R':
      TelnetStream.stop();
      delay(100);
      ESP.restart();
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

  setupWifi(wifi_hostname, ota_password, mqtt_server, mqtt_port, mqtt_username, mqtt_password);

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
    (void)mqtt_client.loop(); // Trigger the mqtt_callback and send the set command to the buffer
  }

  Send_Pana_Command_Timer.update();   // trigger send_pana_command()   - send command or query from buffer
  Send_Pana_Mainquery_Timer.update(); // trigger send_pana_mainquery() - send query to buffer if no command in buffer

  Read_Pana_Data_Timer.update(); // trigger read_pana_data() - read from serial, decode bytes and publish to mqtt
  Timeout_Serial_Timer.update(); // trigger timeout_serial() - stop read from serial after timeout
}
