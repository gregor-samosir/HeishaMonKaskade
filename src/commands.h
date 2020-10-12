#include <ESP8266WiFi.h>

#define PANASONICQUERYSIZE 110
extern byte panasonicQuery[PANASONICQUERYSIZE];
// #define OPTIONALPCBQUERYSIZE 19
// extern byte optionalPCBQuery[OPTIONALPCBQUERYSIZE];

void send_heatpump_command(char *topic, char *msg, void push_command_buffer(byte *, char *));
