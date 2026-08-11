#include "Topics.h"

// The mqtt topics will be concatenated with these values.
// Stage-specific prefix comes as build flag from platformio.ini (HEISHA_MQTT_PREFIX),
// fallback = stage 1
#ifndef HEISHA_MQTT_PREFIX
#define HEISHA_MQTT_PREFIX "panasonic_heat_pump"
#endif
const std::string mqttPrefix = HEISHA_MQTT_PREFIX;
const std::string s = "/";
const std::string setTopicPrefix = mqttPrefix + s + "set";
const std::string infTopicPrefix = mqttPrefix + s + "info";
const std::string Topics::STATE = mqttPrefix + s + "state";
const std::string Topics::LOG = infTopicPrefix + s + "log";
const std::string Topics::WILL = infTopicPrefix + s + "LWT";

// set topics
const std::string Topics::SET1 = setTopicPrefix + s + "Heatpump";
const std::string Topics::SET2 = setTopicPrefix + s + "HolidayMode";
const std::string Topics::SET3 = setTopicPrefix + s + "QuietMode";
const std::string Topics::SET4 = setTopicPrefix + s + "PowerfulMode";
const std::string Topics::SET5 = setTopicPrefix + s + "Z1HeatRequestTemperature";
const std::string Topics::SET6 = setTopicPrefix + s + "Z1CoolRequestTemperature";
const std::string Topics::SET7 = setTopicPrefix + s + "Z2HeatRequestTemperature";
const std::string Topics::SET8 = setTopicPrefix + s + "Z2CoolRequestTemperature";
const std::string Topics::SET9 = setTopicPrefix + s + "OperationMode";
const std::string Topics::SET10 = setTopicPrefix + s + "ForceDHW";
const std::string Topics::SET11 = setTopicPrefix + s + "DHWTemp";
const std::string Topics::SET12 = setTopicPrefix + s + "ForceDefrost";
const std::string Topics::SET13 = setTopicPrefix + s + "ForceSterilization";
const std::string Topics::SET14 = setTopicPrefix + s + "WaterPump";
const std::string Topics::SET15 = setTopicPrefix + s + "WaterPumpSpeed";
const std::string Topics::SET16 = setTopicPrefix + s + "HeatDelta";
const std::string Topics::SET17 = setTopicPrefix + s + "CoolDelta";
const std::string Topics::SET18 = setTopicPrefix + s + "DHWHeatDelta";
const std::string Topics::SET19 = setTopicPrefix + s + "DHWHeatupTime";
const std::string Topics::SET20 = setTopicPrefix + s + "HeaterOnOutdoorTemp";
const std::string Topics::SET21 = setTopicPrefix + s + "HeatingOffOutdoorTemp";
const std::string Topics::SET22 = setTopicPrefix + s + "SGReadyCapacity1Heat";
const std::string Topics::SET23 = setTopicPrefix + s + "SGReadyCapacity1DHW";
const std::string Topics::SET24 = setTopicPrefix + s + "SGReadyCapacity2Heat";
const std::string Topics::SET25 = setTopicPrefix + s + "SGReadyCapacity2DHW";
const std::string Topics::SET26 = setTopicPrefix + s + "DHWRoomMaxTime";
// Heizkurve Zone 1 (Byte 75-78). Namen folgen den zugehoerigen state-Topics
// TOP29/TOP30/TOP32/TOP31 - dort werden die gesetzten Werte zurueckgelesen.
const std::string Topics::SET27 = setTopicPrefix + s + "Z1HeatCurveTargetHighTemp";
const std::string Topics::SET28 = setTopicPrefix + s + "Z1HeatCurveTargetLowTemp";
const std::string Topics::SET29 = setTopicPrefix + s + "Z1HeatCurveOutsideLowTemp";
const std::string Topics::SET30 = setTopicPrefix + s + "Z1HeatCurveOutsideHighTemp";
// Kuehlkurve Zone 1 (Byte 86-89), state-Topics TOP72/TOP73/TOP75/TOP74
const std::string Topics::SET31 = setTopicPrefix + s + "Z1CoolCurveTargetHighTemp";
const std::string Topics::SET32 = setTopicPrefix + s + "Z1CoolCurveTargetLowTemp";
const std::string Topics::SET33 = setTopicPrefix + s + "Z1CoolCurveOutsideLowTemp";
const std::string Topics::SET34 = setTopicPrefix + s + "Z1CoolCurveOutsideHighTemp";

