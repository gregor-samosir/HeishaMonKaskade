#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include "telegram.h" // Typ-, Laengen- und Pruefsummenregel des Antworttelegramms

// platform layer: same firmware for ESP8266 (D1 mini) and ESP32-S3
// (official HeishaMon board), differences are isolated here
#if defined(ESP32)
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <HTTPUpdateServer.h>
#include <DNSServer.h>
#include "Ticker.h"
#include <TelnetStream.h>
#include <TimeLib.h>

// class names differ between the cores
typedef WebServer WebServerClass;
typedef HTTPUpdateServer HTTPUpdateServerClass;

// heatpump on its own UART (pins from the official HeishaMon ESP32-S3 board),
// Serial stays available as USB console
#define heatpumpSerial Serial1
#define HEATPUMPRX 18
#define HEATPUMPTX 17
#define ENABLEPIN 5   // mosfet enable for TX line to heatpump
#define ENABLEOTPIN 4 // OpenTherm 24V booster - unused, must stay LOW

#else
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

typedef ESP8266WebServer WebServerClass;
typedef ESP8266HTTPUpdateServer HTTPUpdateServerClass;

// heatpump on the swapped main UART (gpio13/15), enabled via gpio5
#define heatpumpSerial Serial
#define ENABLEPIN 5

#endif

// posix TZ string (identical to TZ_Europe_Berlin), works on both cores
#define TIME_ZONE "CET-1CEST,M3.5.0,M10.5.0/3"

#define MAXDATASIZE 256
#define QUERYSIZE 110

#define UPDATEALLTIME 300000 // time to resend all to mqtt
#define MQTT_RETAIN_VALUES 1

// config your timing
#define COMMANDTIMER 500  // Command / timer to send commands from buffer to HP
#define QUERYTIMER 5000   // Query / timer to initiate a query
#define BUFFERTIMEOUT 500 // Serial Buffer Filltime / timer to fill the UART buffer with all 203 bytes from HP board
#define SERIALTIMEOUT 600 // Serial Timout / timer to wait on serial to read all 203 bytes from HP
#define NTPTIMEOUT 30000  // max. wait for NTP at boot before continuing without valid time

// MQTT-Wiederverbindung: PubSubClient wartet ohne setSocketTimeout 15 s je
// Versuch, und zwar mitten in loop() - der 5-s-Abfragetakt der Waermepumpe
// steht so lange still. 2 s reichen im LAN, der Rest ist Backoff.
#define MQTT_SOCKET_TIMEOUT_S 2   // Sekunden, ersetzt den Bibliotheksstandard 15
#define MQTT_RECONNECT_MIN 5000   // erster Wiederverbindungsversuch nach 5 s
#define MQTT_RECONNECT_MAX 60000  // Obergrenze, danach im Minutentakt

// WLAN-Watchdog: ohne WLAN ist das Geraet fuer die Kaskadenregelung blind.
// Stufe 1 erneuter Verbindungsversuch, Stufe 2 Neustart.
#define WIFI_RETRY_TIMEOUT 30000   // 30 s ohne WLAN -> WiFi.reconnect()
#define WIFI_REBOOT_TIMEOUT 300000 // 5 min ohne WLAN -> Neustart

void send_pana_command(void);
void send_pana_mainquery(void);
void read_pana_data(void);
void timeout_serial(void);
void flush_serial_input(void);
void check_wifi(void);
void write_mqtt_log(char *);
void write_telnet_log(char *);
void register_new_command(void);

// Global command buffer plus the bits already claimed in it (see commands.cpp)
extern byte mainCommand[QUERYSIZE];
extern byte usedMask[QUERYSIZE];

// true once at least one real SET field sits in mainCommand. Replaces the old
// byte-sum heuristic, which could not tell "empty buffer" from "sum wrapped to 0"
extern bool setDataPending;

// query timer, needs restart from mqtt_callback if a command was rejected
extern Ticker Send_Pana_Mainquery_Timer;
