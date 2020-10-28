#include <LittleFS.h>
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <DNSServer.h>

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

#define LOGHEXBYTESPERLINE 16
#define MAXCOMMANDSINBUFFER 10
#define MAXDATASIZE 255

// Setup timers
unsigned long currentMillis;

// Command / timer to send commands from buffer to HP
// Default 1000
unsigned long commandStarttime;
const unsigned long commandtime = 1000;

// Query / timer to initiate a query
// Default 14000
unsigned long queryStarttime;
const unsigned long querytime = 14000;

// Serial Buffer Filltime / timer to fill the UART buffer with all 203 bytes from HP board
// Default 500
unsigned long bufferfillStarttime;
const unsigned long bufferfilltime = 500;

// Serial Timout / timer to wait on serial to read all 203 bytes from HP
// Default 750
unsigned long serialreadStarttime;
const unsigned long serialreadtime = 750;
bool serialquerysent = false; // mutex for serial sending

// Mqtt reconnect // timer between mqtt reconnect
// Default 30000
// #define RECONNECTTIME 30000 // time between mqtt reconnect
unsigned long reconnectStarttime ;
const unsigned long reconnecttime = 30000;


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

// instead of passing array pointers between functions we just define this in the global scope
char serial_data[MAXDATASIZE];
byte data_length = 0;
unsigned int querynum = 0;

// store actual value in an String array
String actual_data[NUMBEROFTOPICS];

// log message to sprintf to
char log_msg[255];

// mqtt topic to sprintf and then publish to
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

//doule reset detection
DoubleResetDetect drd(DRD_TIMEOUT, DRD_ADDRESS);

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

/*****************************************************************************/
/* MQTT Client                                                               */
/*****************************************************************************/
WiFiClient mqtt_wifi_client;
PubSubClient mqtt_client(mqtt_wifi_client);

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
    //write_mqtt_log(log_msg);
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
  char *msg = (char *)malloc(sizeof(char) * length + 1);
  strncpy(msg, (char *)payload, length);
  msg[length] = '\0';
  build_heatpump_command(topic, msg, push_command_buffer);
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
    for (int j = 0; ((j < LOGHEXBYTESPERLINE) && ((i + j) < hex_len)); j++)
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
  if (currentMillis - commandStarttime >= commandtime)
  {
    write_mqtt_log((char *)commandBuffer->log_msg);
    byte chk = build_checksum(commandBuffer->value, commandBuffer->length);
    size_t bytesSent = Serial.write(commandBuffer->value, commandBuffer->length);
    bytesSent += Serial.write(chk);
    //sprintf(log_msg, "Send %d bytes with checksum: %d ", bytesSent, int(chk)); write_mqtt_log(log_msg);
    
    if (outputHexDump)
    {
      write_mqtt_hex((char *) commandBuffer->value, commandBuffer->length);
    }

    command_struct *nextCommand = commandBuffer->next;
    free(commandBuffer);
    commandBuffer = nextCommand;
    commandsInBuffer--;

    commandStarttime = currentMillis;
    serialreadStarttime = currentMillis;
    bufferfillStarttime = currentMillis;
    
    serialquerysent = true;
  }
}

/*****************************************************************************/
/* Send query to buffer  (called from loop)                                    */
/*****************************************************************************/
void send_pana_mainquery()
{
  if (currentMillis - queryStarttime >= querytime)
  {
    querynum += 1;
    sprintf(log_msg, "QUERY: %d", querynum);
    push_command_buffer(mainQuery, MAINQUERYSIZE, log_msg);
    queryStarttime = currentMillis;
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
    sprintf(log_msg, "Receive partial datagram %d, please fix SERIALBUFFERFILLTIME", data_length);
    write_mqtt_log(log_msg);
  }
  return false;
}

/*****************************************************************************/
/* Read from pana and decode (call from loop)                           */
/*****************************************************************************/
void read_pana_data()
{
  if (serialquerysent) //only read if we have sent a command so we expect an answer
  {
    if (currentMillis - serialreadStarttime >= serialreadtime)
    {
      write_mqtt_log((char *)"Serial read failed due to timeout!");
      data_length = 0;
      serialquerysent = false; //we are allowed to send a new command
      return;
    }

    if (currentMillis - bufferfillStarttime >= bufferfilltime) // wait to fill the serial buffer
    {
      if (readSerial() == true)
      {
        //write_mqtt_log((char *)"Decode  Start");
        decode_heatpump_data(serial_data, actual_data, mqtt_client, write_mqtt_log);
        serialquerysent = false;
        //write_mqtt_log((char *)"Decode  End");
      }
    }
  }
}

/*****************************************************************************/
/* handle mqtt connection  (call from loop)                                  */
/*****************************************************************************/
void mqtt_loop()
{
  if (!mqtt_client.connected())
  {
    if (currentMillis - reconnectStarttime >= reconnecttime)
    {
      if (mqtt_reconnect())
      {
        reconnectStarttime = currentMillis;
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

  commandStarttime = millis();
  queryStarttime = millis();
  bufferfillStarttime = millis();
  serialreadStarttime = millis();
  reconnectStarttime = millis();
}

void loop()
{
  ArduinoOTA.handle();
  httpServer.handleClient();
  MDNS.update();
  mqtt_loop();

  currentMillis = millis(); //get the current time

  if (commandBuffer)
  {
    send_pana_command(); // Send command from buffer.
  }
  else
  {
    send_pana_mainquery(); // Send mainquery to buffer
  }

  read_pana_data(); // Read serial buffer, decode the received value and publish the changed states to mqtt
}
