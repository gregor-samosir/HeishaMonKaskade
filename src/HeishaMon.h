#include <Arduino.h>
#include <LittleFS.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
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

extern "C" {
#include "user_interface.h"
}

#define TIME_ZONE TZ_Europe_Berlin

#define MAXDATASIZE 256
#define QUERYSIZE 110

#define UPDATEALLTIME 300000 // time to resend all to mqtt
#define MQTT_RETAIN_VALUES 1

// config your timing
#define COMMANDTIMER 500 // Command / timer to send commands from buffer to HP
#define QUERYTIMER 5000   // Query / timer to initiate a query
#define BUFFERTIMEOUT 500 // Serial Buffer Filltime / timer to fill the UART buffer with all 203 bytes from HP board
#define SERIALTIMEOUT 600 // Serial Timout / timer to wait on serial to read all 203 bytes from HP

void send_pana_command(void);
void send_pana_mainquery(void);
void read_pana_data(void);
void timeout_serial(void);
void write_mqtt_log(char *);
void write_telnet_log(char *);
void register_new_command(void);

// Global command buffer
extern byte mainCommand[QUERYSIZE];
extern bool isquery;
