#include <ESP8266WiFi.h>

#define MAINQUERYSIZE 110
extern byte mainQuery[MAINQUERYSIZE];

void build_heatpump_command(char *topic, char *msg, void push_command_buffer(byte *, int, char *));
