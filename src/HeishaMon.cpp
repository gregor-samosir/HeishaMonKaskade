
#include "HeishaMon.h"
#include "webfunctions.h"
#include "decode.h"
#include "commands.h"

Ticker command_timer(send_pana_command, COMMANDTIMER, 0, MILLIS);  // loop
Ticker query_timer(send_pana_mainquery, QUERYTIMER, 0, MILLIS); // loop
Ticker bufferfill_timeout(read_pana_data, BUFFERTIMEOUT, 1, MILLIS); // one time
Ticker serial_timeout(timeout_serial, SERIALTIMEOUT, 1, MILLIS); // one time

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

//log and debugg
bool outputMqttLog = true;  // toggle to write logmessages to mqtt log

// global scope
char serial_data[MAXDATASIZE];
byte serial_length = 0;
unsigned int querynum = 0;

// store actual value in an String array
String actual_data[NUMBEROFTOPICS];

// log message
char log_msg[255];

// mqtt topic
char mqtt_topic[255];

Buffer *commandBuffer;
unsigned int commandsInBuffer = 0;

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

WiFiClient mqtt_wifi_client;
PubSubClient mqtt_client(mqtt_wifi_client);
unsigned long lastReconnectAttempt = 0;

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
    // sprintf(log_msg, "Receive byte : %d", serial_length); write_mqtt_log(log_msg);
  }
  if (serial_length > 0)
  { // received length part of header now
    if (serial_length > (serial_data[1] + 3))
    {
      write_mqtt_log((char *)"Datagram longer than header suggests");
      serial_length = 0;
      return false;
    }

    if (serial_length == (serial_data[1] + 3))
    {
      if (!validate_checksum())
      {
        write_mqtt_log((char *)"Datagram checksum not valid");
        serial_length = 0;
        return false;
      }
      serial_length = 0;
      return true;
    }
    sprintf(log_msg, "Receive partial datagram %d, please fix bufferfill_timeout", serial_length);
    write_mqtt_log(log_msg);
  }
  return false;
}

/*****************************************************************************/
/* Write to buffer                                                      */
/*****************************************************************************/
void push_command_buffer(byte *command, int length, char *log_msg)
{
  if (commandsInBuffer < MAXCOMMANDSINBUFFER)
  {
    Buffer *newCommand = new Buffer;
    newCommand->length = length;
    for (int i = 0; i < length; i++)
    {
      newCommand->command[i] = command[i];
      newCommand->log_msg[i] = log_msg[i];
    }
    newCommand->next = commandBuffer;
    commandBuffer = newCommand;
    commandsInBuffer++;
    //sprintf(log_msg, "BUFFERSIZE: %d", commandsInBuffer); write_mqtt_log(log_msg);
  }
  else
  {
    write_mqtt_log(log_msg);
    write_mqtt_log((char *)"Buffer full. Ignoring this command");
  }
}

/*****************************************************************************/
/* Send commands from buffer to pana  (called from loop)                     */
/*****************************************************************************/
void send_pana_command()
{
  if (commandsInBuffer > 0)
  {
    write_mqtt_log((char *)commandBuffer->log_msg);
    // checksum
    byte chk = 0;
    for (int i = 0; i < commandBuffer->length; i++)
    {
      chk += commandBuffer->command[i];
    }
    chk = (chk ^ 0xFF) + 01;

    unsigned int bytesSent = Serial.write(commandBuffer->command, commandBuffer->length);
    bytesSent += Serial.write(chk);
    //sprintf(log_msg, "Send %d bytes with checksum: %d ", bytesSent, int(chk)); write_mqtt_log(log_msg);
    
    Buffer *nextCommand = commandBuffer->next;
    free(commandBuffer);
    commandBuffer = nextCommand;
    commandsInBuffer--;

    serialquerysent = true;
    serial_timeout.start();
    bufferfill_timeout.start();
  }
}

/*****************************************************************************/
/* Send query to buffer  (called from loop)                                    */
/*****************************************************************************/
void send_pana_mainquery()
{
  if (commandsInBuffer == 0)
  {
    querynum += 1;
    sprintf(log_msg, "REQUEST: #%d", querynum);
    push_command_buffer(mainQuery, MAINQUERYSIZE, log_msg);
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
      serialquerysent = false;
      //write_mqtt_log((char *)"Decode  Start");
      decode_heatpump_data(serial_data, actual_data, mqtt_client);    
      //write_mqtt_log((char *)"Decode  End");
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
    serial_length = 0;
    serialquerysent = false; //we are allowed to send a new command
    write_mqtt_log((char *)"Serial read failed due to timeout!");
  }
}

/*****************************************************************************/
/* main                                                                      */
/*****************************************************************************/
void setup()
{
  setupSerial();
  setupWifi(wifi_hostname, ota_password, mqtt_server, mqtt_port, mqtt_username, mqtt_password);
  MDNS.begin(wifi_hostname);
  setupOTA();
  setupMqtt();
  setupHttp();
  switchSerial();
  delay(100);

  command_timer.start();
  query_timer.start();
  lastReconnectAttempt = 0;

  send_pana_mainquery(); // fill buffer for first query
}

void loop()
{
  ArduinoOTA.handle();
  httpServer.handleClient();
  MDNS.update();

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

  command_timer.update(); // trigger send_pana_command()   - send command or query from buffer
  query_timer.update();   // trigger send_pana_mainquery() - send query to buffer if no command in buffer

  bufferfill_timeout.update(); // trigger read_pana_data() - read from serial, decode bytes and publish to mqtt
  serial_timeout.update();     // trigger timeout_serial() - stop read from serial after timeout
}
