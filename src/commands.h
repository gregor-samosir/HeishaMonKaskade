#include <ESP8266WiFi.h>

#define PANASONICQUERYSIZE 110
extern byte panasonicQuery[PANASONICQUERYSIZE];
void send_heatpump_command(char *topic, char *msg, void write_mqtt_log(char *), void push_command_buffer(byte *));