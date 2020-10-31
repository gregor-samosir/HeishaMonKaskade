
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
bool outputHexDump = false; // toggle to dump raw hex to mqtt log

// global scope
char serial_data[MAXDATASIZE];
byte data_length = 0;
unsigned int querynum = 0;

// store actual value in an String array
String actual_data[NUMBEROFTOPICS];

// log message
char log_msg[255];

// mqtt topic
char mqtt_topic[255];

//buffer for commands to send
struct command_struct
{
  byte value[128];
  unsigned int length;
  char log_msg[128];
  command_struct *next;
};
command_struct *commandBuffer;
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
  httpServer.on("/togglehexdump", []() {
    write_mqtt_log((char *)"Toggled hexdump log flag");
    outputHexDump ^= true;
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
/* Write to buffer                                                      */
/*****************************************************************************/
void push_command_buffer(byte *command, int length, char *log_msg)
{
  if (commandsInBuffer < MAXCOMMANDSINBUFFER)
  {
    command_struct *newCommand = new command_struct;
    newCommand->length = length;
    for (int i = 0; i < length; i++)
    {
      newCommand->value[i] = command[i];
      newCommand->log_msg[i] = log_msg[i];
    }
    newCommand->next = commandBuffer;
    commandBuffer = newCommand;
    commandsInBuffer++;
    // sprintf(log_msg, "Push %d to buffer", commandsInBuffer); write_mqtt_log(log_msg);
  }
  else
  {
    write_mqtt_log(log_msg);
    write_mqtt_log((char *)"Buffer full. Ignoring this command");
  }
}

/*****************************************************************************/
/* MQTT Callback                                                             */
/*****************************************************************************/
void mqtt_callback(char *topic, byte *payload, unsigned int length)
{
  //command_timer.stop();
  char *msg = (char *)malloc(sizeof(char) * length + 1);
  strncpy(msg, (char *)payload, length);
  msg[length] = '\0';
  build_heatpump_command(topic, msg);
  //command_timer.start();
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
/* Write raw hex to mqtt log                                            */
/*****************************************************************************/
void write_mqtt_hex(char *hex, byte hex_len) // New version from HeishaMon
{

  for (int i = 0; i < hex_len; i += LOGHEXBYTESPERLINE)
  {
    char buffer[(LOGHEXBYTESPERLINE * 3) + 1];
    buffer[LOGHEXBYTESPERLINE * 3] = '\0';
    for (int j = 0; (j < LOGHEXBYTESPERLINE && (i + j) < hex_len); j++)
    {
      sprintf(&buffer[3 * j], "%02X ", hex[i + j]);
    }
    sprintf(log_msg, "Data: %s", buffer);
    write_mqtt_log(log_msg);
  }
}

/*****************************************************************************/
/* Build checksum for commands and query                                     */
/*****************************************************************************/
byte build_checksum(byte *command, int length)
{
  byte chk = 0;
  for (int i = 0; i < length; i++)
  {
    chk += command[i];
  }
  chk = (chk ^ 0xFF) + 01;
  return chk;
}

/*****************************************************************************/
/* Validate checksum                                                         */
/*****************************************************************************/
bool validate_checksum()
{
  byte chk = 0;
  for (int i = 0; i < data_length; i++)
  {
    chk += serial_data[i];
  }
  return (chk == 0); //all received bytes + checksum should result in 0
}

/*****************************************************************************/
/* Send command buffer to pana  (called from loop)                           */
/*****************************************************************************/
void send_pana_command()
{
  if (commandsInBuffer > 0)
  {
    write_mqtt_log((char *)commandBuffer->log_msg);
    byte chk = build_checksum(commandBuffer->value, commandBuffer->length);
    size_t bytesSent = Serial.write(commandBuffer->value, commandBuffer->length);
    bytesSent += Serial.write(chk);
    //sprintf(log_msg, "Send %d bytes with checksum: %d ", bytesSent, int(chk)); write_mqtt_log(log_msg);

    if (outputHexDump)
    {
      write_mqtt_hex((char *)commandBuffer->value, commandBuffer->length);
    }

    command_struct *nextCommand = commandBuffer->next;
    free(commandBuffer);
    commandBuffer = nextCommand;
    commandsInBuffer--;

    serial_timeout.start();
    bufferfill_timeout.start();
    serialquerysent = true;
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
    sprintf(log_msg, "QUERY: %d", querynum);
    push_command_buffer(mainQuery, MAINQUERYSIZE, log_msg);
  }
}

/*****************************************************************************/
/* Read raw from serial                                                 */
/*****************************************************************************/
bool readSerial()
{
  while (Serial.available() > 0)
  {
    serial_data[data_length] = Serial.read();
    data_length += 1;
    // only enable next line to DEBUG
    // sprintf(log_msg, "Receive byte : %d", data_length); write_mqtt_log(log_msg);
  }
  if (data_length > 0)
  { // received length part of header now
    if (data_length > (serial_data[1] + 3))
    {
      write_mqtt_log((char *)"Datagram longer than header suggests");
      data_length = 0;
      return false;
    }

    if (data_length == (serial_data[1] + 3))
    {
      if (outputHexDump)
      {
        write_mqtt_hex(serial_data, data_length);
      }

      if (!validate_checksum())
      {
        write_mqtt_log((char *)"Datagram checksum not valid");
        data_length = 0;
        return false;
      }
      data_length = 0;
      return true;
    }
    sprintf(log_msg, "Receive partial datagram %d, please fix bufferfill_timeout", data_length);
    write_mqtt_log(log_msg);
  }
  return false;
}

/*****************************************************************************/
/* Read from pana and decode (call from loop)                           */
/*****************************************************************************/
void read_pana_data()
{
  if (serialquerysent == true) //only read if we have sent a command so we expect an answer
  {
    bufferfill_timeout.stop();
    if (readSerial() == true)
    {
      serial_timeout.stop();
      //write_mqtt_log((char *)"Decode  Start");
      decode_heatpump_data(serial_data, actual_data, mqtt_client);
      serialquerysent = false;
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
    serial_timeout.stop();
    write_mqtt_log((char *)"Serial read failed due to timeout!");
    data_length = 0;
    serialquerysent = false; //we are allowed to send a new command
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
  delay(1000);

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