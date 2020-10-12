#include <ESP8266WiFi.h>

#define MAINQUERYSIZE 110
extern byte mainQuery[MAINQUERYSIZE];
#define PCBQUERYSIZE 19
extern byte pcbQuery[PCBQUERYSIZE];

void send_heatpump_command(char *topic, char *msg, void push_command_buffer(byte *, int, char *));
