#pragma once

#include <PubSubClient.h>
#include "Topics.h"

#define NUMBEROFTOPICS 99 // last topic number + 1
#define MAXVALUELEN 16    // longest payload value incl. terminator (e.g. "No error", "-123.75")

void publish_heatpump_data(char *, char (*)[MAXVALUELEN], PubSubClient &);
String getTopicPayload(unsigned int, char *);

String unknown(byte);
String getBit1and2(byte);
String getBit3and4(byte);
String getBit5and6(byte);
String getBit7and8(byte);
String getBit3and4and5(byte);
String getLeft5bits(byte);
String getRight3bits(byte);
String getIntMinus1(byte);
String getIntMinus128(byte);
String getIntMinus1Div5(byte);
String getIntMinus1Times10(byte);
String getIntMinus1Times30(byte);
String getIntMinus1Times50(byte);
String getIntMinus1Times200(byte);
String getOpMode(byte);
String getPumpFlow(char *);
String getOperationHour(char *);
String getOperationCount(char *);
String getRoomHeaterHour(char *);
String getDHWHeaterHour(char *);
String getErrorInfo(char *);
String getInletFraction(byte);
String getOutletFraction(byte);
String getInletTempWithFraction(char *);
String getOutletTempWithFraction(char *);

typedef String (*topicFP)(byte);

// lookup tables, defined once in decode.cpp (previously static copies per translation unit)
extern const char *topicNames[NUMBEROFTOPICS];
extern const byte topicBytes[NUMBEROFTOPICS];
extern const topicFP topicFunctions[NUMBEROFTOPICS];
extern const char **topicDescription[NUMBEROFTOPICS];
