#include <LittleFS.h>
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>

#include "webfunctions.h"
#include "decode.h"
#include "commands.h"

// maximum number of seconds between resets that
// counts as a double reset
#define DRD_TIMEOUT 0.1

// address to the block in the RTC user memory
// change it if it collides with another usage
// of the address block
#define DRD_ADDRESS 0x00

#define WAITTIME 5000      // wait for next data read from heatpump
#define SERIALTIMEOUT 1000 // wait until all 203 bytes are read
#define RECONNECTTIME 30000 // next mqtt reconnect
#define LOGHEXBYTESPERLINE 16 // please be aware of max mqtt message size - 32 bytes per line does not work
#define MAXCOMMANDSINBUFFER 10 //can't have too much in buffer due to memory shortage
#define MAXDATASIZE 256

// Default settings if config does not exists
const char *update_path = "/firmware";
const char *update_username = "admin";
char wifi_hostname[40] = "HeishaMon";
char ota_password[40] = "heisha";
char mqtt_server[40];
char mqtt_port[6] = "1883";
char mqtt_username[40];
char mqtt_password[40];

bool requesthasbeensent = false;          // mutex for serial sending data

unsigned long nextquerytime = 0;          // WAITTIME
unsigned long nextreadtime = 0;           // SERIALTIMEOUT

//log and debugg
bool outputMqttLog = true;  // toggle to write logmessages to mqtt log
bool outputHexDump = false; // toggle to dump raw hex data to mqtt log

// instead of passing array pointers between functions we just define this in the global scope
char data[MAXDATASIZE];
int data_length = 0;
int datagramchanges = 0;
int topicchanges = 0;

// store actual data in an String array
String actData[NUMBER_OF_TOPICS];

// log message to sprintf to
char log_msg[256];

// mqtt topic to sprintf and then publish to
char mqtt_topic[256];

//buffer for commands to send
struct command_struct
{
  byte value[128];
  command_struct *next;
};
command_struct *commandBuffer;
unsigned int commandsInBuffer = 0;

//doule reset detection
DoubleResetDetect drd(DRD_TIMEOUT, DRD_ADDRESS);

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

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
/* HTTP                                                                      */
/*****************************************************************************/
void setupHttp()
{
  httpUpdater.setup(&httpServer, update_path, update_username, ota_password);
  httpServer.on("/", []() {
        handleRoot(&httpServer);
      });
  httpServer.on("/tablerefresh", []() {
    handleTableRefresh(&httpServer, actData);
  });
  httpServer.on("/json", []() {
    handleJsonOutput(&httpServer, actData);
  });
  httpServer.on("/factoryreset", []() {
    handleFactoryReset(&httpServer);
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
/* MQTT Client                                                               */
/*****************************************************************************/
WiFiClient mqtt_wifi_client;
PubSubClient mqtt_client(mqtt_wifi_client);
unsigned long lastReconnectAttempt = 0;

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
  }
  return mqtt_client.connected();
}

/*****************************************************************************/
/* MQTT Callback                                                             */
/*****************************************************************************/
void mqtt_callback(char *topic, byte *payload, unsigned int length)
{
    char *msg = (char *)malloc(sizeof(char) * length + 1);
    strncpy(msg, (char *)payload, length);
    msg[length] = '\0';
    send_heatpump_command(topic, msg, write_mqtt_log, push_command_buffer);
}

/*****************************************************************************/
/* MQTT Setup                                                                */
/*****************************************************************************/
void setupMqtt()
{
  mqtt_client.setServer(mqtt_server, atoi(mqtt_port));
  mqtt_client.setCallback(mqtt_callback);
  mqtt_reconnect();
  lastReconnectAttempt = 0;
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
/* Write raw hex data to mqtt log                                            */
/*****************************************************************************/
void write_mqtt_hex(char *hex, int hex_len) // New version from HeishaMon
{

  for (int i = 0; i < hex_len; i += LOGHEXBYTESPERLINE)
  {
    char buffer[(LOGHEXBYTESPERLINE * 3) + 1];
    buffer[LOGHEXBYTESPERLINE * 3] = '\0';
    for (int j = 0; ((j < LOGHEXBYTESPERLINE) && ((i + j) < hex_len)); j++)
    {
      sprintf(&buffer[3 * j], "%02X ", hex[i + j]);
    }
    sprintf(log_msg, "data: %s", buffer); write_mqtt_log(log_msg);
  }
}

/*****************************************************************************/
/* Build checksum for commands and query                                     */
/*****************************************************************************/
byte build_checksum(byte *command)
{
  byte chk = 0;
  for (int i = 0; i < PANASONICQUERYSIZE; i++)
  {
    chk += command[i];
  }
  chk = (chk ^ 0xFF) + 01;
  return chk;
}

/*****************************************************************************/
/* Validate checksum on serial read data                                     */
/*****************************************************************************/
bool validate_checksum()
{
  byte chk = 0;
  for (int i = 0; i < data_length; i++)
  {
    chk += data[i];
  }
  return (chk == 0); //all received bytes + checksum should result in 0
}

/*****************************************************************************/
/* Write data to serial                                                      */
/*****************************************************************************/
bool send_serial_command(byte *command)
{
  byte chk = build_checksum(command);
  int bytesSent = Serial.write(command, PANASONICQUERYSIZE);
  bytesSent += Serial.write(chk);
  //sprintf(log_msg, "Send %d bytes with checksum: %d ", bytesSent, int(chk)); write_mqtt_log(log_msg);
  if (outputHexDump) write_mqtt_hex((char *)command, PANASONICQUERYSIZE);
  nextreadtime = millis() + SERIALTIMEOUT; //set readtime when to timeout the answer of this command
  return true;
}

/*****************************************************************************/
/* Write query (or buffer) data to pana                                      */
/*****************************************************************************/
void send_panasonic_data()
{
  if (millis() > nextquerytime)
  { 
    nextquerytime = millis() + WAITTIME;
    if (commandBuffer)
    { 
      // sprintf(log_msg, "Pop %d from buffer", commandsInBuffer); write_mqtt_log(log_msg);
      requesthasbeensent = send_serial_command(commandBuffer->value);
      command_struct *nextCommand = commandBuffer->next;
      free(commandBuffer);
      commandBuffer = nextCommand;
      commandsInBuffer--;
      if (commandsInBuffer) { 
        nextquerytime = millis() + SERIALTIMEOUT * 2;
        // write_mqtt_log((char *)"Buffer not empty");
      }
    }
    else
    { //no command in buffer, send query
      //write_mqtt_log((char *)"Request data with query");
      requesthasbeensent = send_serial_command(panasonicQuery);
    }
  }
}

/*****************************************************************************/
/* Read raw data from serial                                                 */
/*****************************************************************************/
bool readSerial()
{
  char rc;
  while (Serial.available())
  {
    rc = Serial.read();
    if ((data[data_length] != rc) && (data_length < 202))
    {
      datagramchanges += 1;
    }
    data[data_length] = rc;
    // only enable this if you really want to see how the data is gathered in multiple tries
    // sprintf(log_msg, "Receive byte : %d : %d", data_length, (int)rc); write_mqtt_log(log_msg);
    data_length += 1;
  }
  if (data_length > 1)
  { //should have received length part of header now

    if (data_length > (data[1] + 3))
    {
      write_mqtt_log((char *)"Datagram longer than header suggests");
      data_length = 0;
      datagramchanges = 0;
      return false;
    }

    if (data_length == (data[1] + 3))
    { 
      requesthasbeensent = false;
      if (outputHexDump) write_mqtt_hex(data, data_length);
      if (!validate_checksum())
      {
        write_mqtt_log((char *)"Datagram checksum not valid");
        data_length = 0;
        datagramchanges = 0;
        return false;
      }
      if (data_length == 203)
      { //for now only return true for this datagram because we can not decode the shorter datagram yet
        data_length = 0;
        return true;
      }
      else
      {
        write_mqtt_log((char *)"Datagram to short to decode in this version");
        data_length = 0;
        datagramchanges = 0;
        return false;
      }
    }
  }
  return false;
}

/*****************************************************************************/
/* Read data from pana and decode                                            */
/*****************************************************************************/
void read_panasonic_data()
{
  if (requesthasbeensent) //only read data if we have sent a command so we expect an answer
  {
    if (millis() > nextreadtime)
    {
      write_mqtt_log((char *)"Serial read failed due to timeout!");
      data_length = 0; //clear any data in array
      datagramchanges = 0;
      requesthasbeensent = false; //we are allowed to send a new command
      return;
    }

    if (readSerial())
    {
      if (datagramchanges > 0)
      {
        //write_mqtt_log((char *)"Decode  Start");
        topicchanges = decode_heatpump_data(data, actData, mqtt_client, write_mqtt_log);
        //sprintf(log_msg, "Bytes  changed: %d", datagramchanges); write_mqtt_log(log_msg);
        //sprintf(log_msg, "Topics changed: %d", topicchanges); write_mqtt_log(log_msg);
        //write_mqtt_log((char *)"Decode  End");
      }
      else
      {
        //write_mqtt_log((char *)"Datagram unchanged");
      }
      datagramchanges = 0;
    }
  }
}

/*****************************************************************************/
/* Write data to buffer                                                      */
/*****************************************************************************/
void push_command_buffer(byte *command)
{
  if (commandsInBuffer < MAXCOMMANDSINBUFFER)
  {
    command_struct *newCommand = new command_struct;
    for (int i = 0; i < PANASONICQUERYSIZE; i++)
    {
      newCommand->value[i] = command[i];
    }
    newCommand->next = commandBuffer;
    commandBuffer = newCommand;
    commandsInBuffer++;
    // sprintf(log_msg, "Push %d to buffer", commandsInBuffer); write_mqtt_log(log_msg);
    nextquerytime = millis() + SERIALTIMEOUT / 2;
  }
  else
  {
    write_mqtt_log((char *)"Buffer full. Ignoring this command");
  }
}

/*****************************************************************************/
/* handle mqtt connection                                                    */
/*****************************************************************************/
void mqtt_loop()
{
  if (!mqtt_client.connected())
  {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > RECONNECTTIME)
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
}

/*****************************************************************************/
/* main                                                                      */
/*****************************************************************************/
void setup()
{
  setupSerial();
  setupWifi(drd, wifi_hostname, ota_password, mqtt_server, mqtt_port, mqtt_username, mqtt_password);
  MDNS.begin(wifi_hostname);
  setupOTA();
  setupMqtt();
  setupHttp();
  switchSerial();
}

void loop()
{
  ArduinoOTA.handle();
  httpServer.handleClient();
  MDNS.update();
  mqtt_loop();
  send_panasonic_data(); // Send query or command from buffer. This trigger heisha to fill the serial buffer
  read_panasonic_data(); // Read serial buffer, decode the received data and publish the changed states to mqtt
}