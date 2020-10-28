#include <LittleFS.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DoubleResetDetect.h>
#include <ArduinoJson.h>

void setupWifi(DoubleResetDetect &drd, char *wifi_hostname, char *ota_password, char *mqtt_server, char *mqtt_port, char *mqtt_username, char *mqtt_password);
void handleRoot(ESP8266WebServer *httpServer);
void handleTableRefresh(ESP8266WebServer *httpServer, String actual_data[]);
void handleJsonOutput(ESP8266WebServer *httpServer, String actual_data[]);
void handleFactoryReset(ESP8266WebServer *httpServer);
void handleReboot(ESP8266WebServer *httpServer);
void handleSettings(ESP8266WebServer *httpServer, char *wifi_hostname, char *ota_password, char *mqtt_server, char *mqtt_port, char *mqtt_username, char *mqtt_password);
