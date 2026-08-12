#pragma once
#include <LittleFS.h>
#include "HeishaMon.h" // WebServerClass platform alias, WiFi includes
#include <ArduinoJson.h>
#include "decode.h"

// Puffergroessen der Konfigurationsfelder. Stehen hier einmal fuer beide
// Seiten: die Puffer selbst liegen in HeishaMon.cpp, gefuellt werden sie hier.
// Vorher waren 39/40 und 5/6 an einem Dutzend Stellen einzeln hingeschrieben.
#define CONFIG_FIELD_LEN 40 // Hostname, Passwoerter, Server, Benutzername
#define CONFIG_PORT_LEN 6   // Portnummer als Text ("65535" + Terminator)

void setupWifi(char *wifi_hostname, char *ota_password, char *mqtt_server, char *mqtt_port, char *mqtt_username, char *mqtt_password);
void handleRoot(WebServerClass *httpServer);
void handleTableRefresh(WebServerClass *httpServer, char actual_data[][MAXVALUELEN]);
void handleReboot(WebServerClass *httpServer);
void handleSettings(WebServerClass *httpServer, char *wifi_hostname, char *ota_password, char *mqtt_server, char *mqtt_port, char *mqtt_username, char *mqtt_password);
