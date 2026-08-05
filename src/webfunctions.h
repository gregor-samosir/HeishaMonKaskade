#pragma once
#include <LittleFS.h>
#include "HeishaMon.h" // WebServerClass platform alias, WiFi includes
#include <ArduinoJson.h>
#include "decode.h"

void setupWifi(char *wifi_hostname, char *ota_password, char *mqtt_server, char *mqtt_port, char *mqtt_username, char *mqtt_password);
void handleRoot(WebServerClass *httpServer);
void handleTableRefresh(WebServerClass *httpServer, char actual_data[][MAXVALUELEN]);
void handleReboot(WebServerClass *httpServer);
void handleSettings(WebServerClass *httpServer, char *wifi_hostname, char *ota_password, char *mqtt_server, char *mqtt_port, char *mqtt_username, char *mqtt_password);
