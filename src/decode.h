#pragma once

#include <Arduino.h> // byte, String etc. - not only indirectly via PubSubClient.h
#include <PubSubClient.h>
#include "Topics.h"

#define NUMBEROFTOPICS 99 // last topic number + 1
#define MAXVALUELEN 16    // longest payload value incl. terminator (e.g. "No error", "-123.75")

void publish_heatpump_data(uint8_t *, char (*)[MAXVALUELEN], PubSubClient &);

// all decoders write into a caller buffer of MAXVALUELEN bytes:
// no String allocations in the decode path (runs every 5 seconds)
void getTopicPayload(unsigned int, uint8_t *, char *);

void unknown(byte, char *);
void getBit1and2(byte, char *);
void getBit3and4(byte, char *);
void getBit5and6(byte, char *);
void getBit7and8(byte, char *);
void getBit3and4and5(byte, char *);
void getLeft5bits(byte, char *);
void getRight3bits(byte, char *);
void getIntMinus1(byte, char *);
void getIntMinus128(byte, char *);
void getIntMinus1Div5(byte, char *);
void getIntMinus1Times10(byte, char *);
void getIntMinus1Times30(byte, char *);
void getIntMinus1Times50(byte, char *);
void getIntMinus1Times200(byte, char *);
void getOpMode(byte, char *);
void getPumpFlow(uint8_t *, char *);
void getOperationHour(uint8_t *, char *);
void getOperationCount(uint8_t *, char *);
void getRoomHeaterHour(uint8_t *, char *);
void getDHWHeaterHour(uint8_t *, char *);
void getErrorInfo(uint8_t *, char *);
void getInletTempWithFraction(uint8_t *, char *);
void getOutletTempWithFraction(uint8_t *, char *);

typedef void (*topicFP)(byte, char *);

// lookup tables, defined once in decode.cpp (previously static copies per translation unit)
extern const char *topicNames[NUMBEROFTOPICS];
extern const byte topicBytes[NUMBEROFTOPICS];
extern const topicFP topicFunctions[NUMBEROFTOPICS];
extern const char **topicDescription[NUMBEROFTOPICS];
