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
#include <TelnetStream.h>
#include <TimeLib.h>
#include <sntp.h>
#include <TZ.h>

#define TIME_ZONE TZ_Europe_Berlin

#define LOGHEXBYTESPERLINE 16
#define MAXCOMMANDSINBUFFER 10
#define MAXDATASIZE 256
#define MAINQUERYSIZE 110

#define UPDATEALLTIME 300000 // time to resend all to mqtt
#define MQTT_RETAIN_VALUES 1

// config your timing
#define COMMANDTIMER 200 // Command / timer to send commands from buffer to HP
#define QUERYTIMER 5000   // Query / timer to initiate a query
#define BUFFERTIMEOUT 500 // Serial Buffer Filltime / timer to fill the UART buffer with all 203 bytes from HP board
#define SERIALTIMEOUT 550 // Serial Timout / timer to wait on serial to read all 203 bytes from HP

void send_pana_command(void);
void send_pana_mainquery(void);
void read_pana_data(void);
void timeout_serial(void);
void write_mqtt_log(char *);
void write_telnet_log(char *);
void push_command_buffer(char *, int);

struct Buffer{
    char command_name[MAXDATASIZE];
    unsigned int command_position;
    Buffer *next;
};

extern byte mainCommand[MAINQUERYSIZE];