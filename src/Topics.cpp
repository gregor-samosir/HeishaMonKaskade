#include "Topics.h"

// The mqtt topics will be concatenated with these values.
// Stage-specific prefix comes as build flag from platformio.ini (HEISHA_MQTT_PREFIX),
// fallback = stage 1
#ifndef HEISHA_MQTT_PREFIX
#define HEISHA_MQTT_PREFIX "panasonic_heat_pump"
#endif

// nur Bausteine fuer die Pfade unten - static, damit die sehr allgemeinen
// Namen (vor allem "s") nicht global sichtbar sind
static const std::string mqttPrefix = HEISHA_MQTT_PREFIX;
static const std::string s = "/";
static const std::string infTopicPrefix = mqttPrefix + s + "info";

// Wurzeln der Pfade. Die Topic-Namen dahinter haengen decode.cpp (state) und
// commands.cpp (set) aus ihren Tabellen an.
const std::string Topics::STATE = mqttPrefix + s + "state";
const std::string Topics::SET = mqttPrefix + s + "set";
const std::string Topics::LOG = infTopicPrefix + s + "log";
const std::string Topics::WILL = infTopicPrefix + s + "LWT";
const std::string Topics::NOTBETRIEB = mqttPrefix + s + "notbetrieb";
