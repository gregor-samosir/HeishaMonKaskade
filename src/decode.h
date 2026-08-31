#pragma once

#include <Arduino.h> // byte, String etc. - not only indirectly via PubSubClient.h
#include <PubSubClient.h>
#include "Topics.h"

// Anzahl der Zeilen in stateTopics[]. Wird als Array-Groesse gebraucht (siehe
// actual_data in HeishaMon.cpp) und muss deshalb eine Konstante sein - dass sie
// zur Tabelle passt, sichert ein static_assert in decode.cpp ab.
constexpr unsigned int NUMBEROFTOPICS = 99;

#define MAXVALUELEN 16 // longest payload value incl. terminator (e.g. "No error", "-123.75")

// Obergrenze fuer die Laenge einer desc-Liste. Nur der Hosttest braucht sie:
// er zaehlt bis zum nullptr und muss abbrechen koennen, falls jemand eine neue
// Liste ohne Abschluss anlegt - sonst liefe er ueber das Array hinaus.
constexpr int DESC_MAX_ENTRIES = 32;

/*****************************************************************************/
/* Klartext einer Tabellenzeile fuer die Weboberflaeche nachschlagen         */
/*                                                                           */
/* Der dekodierte Wert IST der Index in desc[] - und er kann groesser sein    */
/* als die Liste: die 2-Bit-Felder liefern bis 2, die 3-Bit-Felder bis 6.     */
/* Geprueft wurde bis 3.8.2 nur nach unten (-1 fuer unbekannt); ein zu        */
/* grosser Index las einen wilden Zeiger, den %s dann formatierte - Absturz   */
/* an der laufenden Anlage, sobald ein Browser die Seite offen hat. Dass die  */
/* Waermepumpe Rohwerte ausserhalb des bisher Beobachteten liefert, hat       */
/* dieses Projekt schon mehrfach erlebt (Byte 110, Kurvengrenzen).            */
/*                                                                           */
/* Die Obergrenze steht deshalb in der Liste selbst: jede desc-Liste endet    */
/* mit nullptr, und hier wird bis zum gesuchten Index hochgezaehlt statt      */
/* direkt zuzugreifen. Das kann per Konstruktion nicht hinauslesen, auch      */
/* wenn spaeter ein Dekodierer einen groesseren Index liefert - dann bleibt   */
/* die Zelle leer, statt dass das Geraet neu startet. Die Listen sind hoechs- */
/* tens neun Eintraege lang, die Schleife faellt neben dem Seitenaufbau nicht */
/* ins Gewicht.                                                              */
/*                                                                           */
/* Steht als inline-Funktion im Header, damit Firmware und Hosttest dieselbe  */
/* Regel benutzen und nicht auseinanderlaufen koennen (test/byte110_test.cpp).*/
/*****************************************************************************/
inline const char *desc_text(const char *const *desc, int value)
{
  if (desc == nullptr || value < 0)
  {
    return "";
  }
  for (int i = 0; i < value; i++)
  {
    if (desc[i] == nullptr) // Index liegt hinter dem Ende der Liste
    {
      return "";
    }
  }
  return (desc[value] == nullptr) ? "" : desc[value];
}

struct StateTopic;

// Dekodierer-Signaturen:
//   topicFP  liest ein einzelnes Byte (der Normalfall)
//   wideFP   braucht mehrere Bytes des Telegramms und bekommt deshalb die
//            ganze Zeile mit, um pos/decode daraus verwenden zu koennen
typedef void (*topicFP)(byte, char *);
typedef void (*wideFP)(const StateTopic *, uint8_t *, char *);

/*****************************************************************************/
/* State topic table                                                         */
/* Eine Zeile pro Topic - Name, Quellbyte, Dekodierer und Einheit stehen      */
/* beieinander statt in vier positionsgleichen Parallel-Tabellen.            */
/*                                                                           */
/* Wichtig: 'number' ist ein Datenfeld, NICHT der Array-Index. Ein Topic      */
/* behaelt damit seine TOP-Nummer, auch wenn Zeilen davor entfernt werden -   */
/* die Nummern stehen so in MQTT-Topics.md und sind die gemeinsame Sprache    */
/* mit dem Original-Projekt. Indiziert wird ueber die Position (actual_data,  */
/* Web-Tabelle), ausgegeben wird 'number'.                                    */
/*****************************************************************************/
/* Feldreihenfolge: die beiden Bytes stehen bewusst nebeneinander. Mit einem   */
/* Zeiger dazwischen kaeme je Zeile 3 Byte Padding dazu - ueber 99 Zeilen      */
/* fast 400 Byte. Bis 3.15.0 waren das 400 Byte RAM (auf dem ESP8266 lagen     */
/* const-Tabellen im RAM), auf dem ESP32 sind es 400 Byte Flash. Die           */
/* Ersparnis bleibt, sie kostet nur eine andere Ressource.                     */
struct StateTopic
{
  byte number;       // TOPn - nur fuer Anzeige und Log, NICHT der Index
  byte pos;          // Byte im Telegramm (0, wenn nur 'wide' zustaendig ist)
  const char *name;  // MQTT-Topic-Name unterhalb von <prefix>/state/
  topicFP decode;    // 1-Byte-Dekodierer, nullptr bei reinen Mehrbyte-Topics
  wideFP wide;       // Mehrbyte-Dekodierer, sonst nullptr
  const char **desc; // Einheit bzw. Klartexte fuer die Weboberflaeche
};

extern const StateTopic stateTopics[NUMBEROFTOPICS];

// Tabellenindex zu einer TOP-Nummer finden. Gebraucht vom Notbetrieb: Seine
// Schritte nennen das TOP, an dem zurueckgelesen wird - actual_data[] wird
// aber ueber den ZEILENINDEX adressiert, und der ist nicht die TOP-Nummer
// (Luecken durch entfallene Topics, hoechste Nummer 104 bei 92 Zeilen).
// -1, wenn es die Nummer nicht gibt.
int state_topic_index(unsigned int top_number);

void publish_heatpump_data(uint8_t *, char (*)[MAXVALUELEN], PubSubClient &);

// all decoders write into a caller buffer of MAXVALUELEN bytes:
// no String allocations in the decode path (runs every 5 seconds)
void getTopicPayload(unsigned int, uint8_t *, char *);

void getBit1and2(byte, char *);
void getBit3and4(byte, char *);
void getBit5and6(byte, char *);
void getBit7and8(byte, char *);
void getBit3and4and5(byte, char *);
void getRight3bits(byte, char *);
void getIntMinus1(byte, char *);
void getIntMinus128(byte, char *);
void getIntMinus1Div5(byte, char *);
void getIntMinus1Times10(byte, char *);
void getIntMinus1Times30(byte, char *);
void getIntMinus1Times50(byte, char *);
void getIntMinus1Times200(byte, char *);
void getOpMode(byte, char *);

void getPumpFlow(const StateTopic *, uint8_t *, char *);
void getOperationHour(const StateTopic *, uint8_t *, char *);
void getOperationCount(const StateTopic *, uint8_t *, char *);
void getRoomHeaterHour(const StateTopic *, uint8_t *, char *);
void getDHWHeaterHour(const StateTopic *, uint8_t *, char *);
void getErrorInfo(const StateTopic *, uint8_t *, char *);
void getInletTempWithFraction(const StateTopic *, uint8_t *, char *);
void getOutletTempWithFraction(const StateTopic *, uint8_t *, char *);
