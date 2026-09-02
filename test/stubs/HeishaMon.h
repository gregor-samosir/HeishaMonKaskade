#pragma once
// Ersatz fuer src/HeishaMon.h - nur die Symbole, die decode.cpp anfasst.
#include "Arduino.h"
#include "PubSubClient.h"
#define UPDATEALLTIME 300000
#define MQTT_RETAIN_VALUES true
void write_telnet_log(char *);
void write_mqtt_log(char *);
// seit 3.20.0: die <PUB>-Zeile geht nicht mehr ueber write_mqtt_log(),
// sondern an write_wert_log() vorbei am Logring (M4)
void write_wert_log(char *);
