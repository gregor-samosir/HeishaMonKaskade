#pragma once
// Ersatz fuer src/HeishaMon.h - nur die Symbole, die decode.cpp anfasst.
#include "Arduino.h"
#include "PubSubClient.h"
#define UPDATEALLTIME 300000
#define MQTT_RETAIN_VALUES true
void write_telnet_log(char *);
void write_mqtt_log(char *);
