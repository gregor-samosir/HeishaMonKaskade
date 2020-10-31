#include <Arduino.h>
#include <LittleFS.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <DNSServer.h>
#include "Ticker.h"

#define LOGHEXBYTESPERLINE 16
#define MAXCOMMANDSINBUFFER 10
#define MAXDATASIZE 255

// config your timing
#define COMMANDTIMER 1000 // Command / timer to send commands from buffer to HP
#define QUERYTIMER 10000  // Query / timer to initiate a query
#define BUFFERTIMEOUT 500 // Serial Buffer Filltime / timer to fill the UART buffer with all 203 bytes from HP board
#define SERIALTIMEOUT 750 // Serial Timout / timer to wait on serial to read all 203 bytes from HP

void send_pana_command();
void send_pana_mainquery();
void read_pana_data();
void timeout_serial();
void write_mqtt_log(char *);
void push_command_buffer(byte *, int, char *);
