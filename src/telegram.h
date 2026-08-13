#pragma once
#include <stdint.h>

/*****************************************************************************/
/* Pruefung des Antworttelegramms der Waermepumpe                            */
/*                                                                           */
/* Bewusst frei von Arduino-Abhaengigkeiten: derselbe Code laeuft in der     */
/* Firmware (readSerial) und im Hosttest (test/telegramm_test.cpp). Wer die  */
/* Regeln aendert, aendert beides zugleich - es gibt keine zweite Wahrheit.  */
/*                                                                           */
/* Die Waermepumpe antwortet auf die Abfrage (0x71 0x6C ...) mit genau einem */
/* Telegrammtyp: 0x71 0xC8 0x01 0x10 ... , also 200 Nutzbytes + 3 = 203      */
/* Bytes einschliesslich Laengenbyte, Typ und Pruefsumme (Beleg:             */
/* ProtocolByteDecrypt.md, "Panasonic answer example"). Der Decoder in       */
/* decode.cpp liest bis Byte 202 - jedes kuerzere Telegramm wuerde ueber das */
/* Pufferende hinaus ausgewertet.                                            */
/*****************************************************************************/

#define TELEGRAM_TYPE_DATA 0x71u // Typbyte des Antworttelegramms
#define TELEGRAM_DATA_LEN 203u   // vollstaendige Laenge inkl. Pruefsumme

// Ergebnis der Pruefung - jeder Fall bekommt in readSerial seine eigene
// Logzeile, damit im Fehlerfall erkennbar ist, WAS auf der Leitung stand
enum TelegramCheck
{
  TELEGRAM_OK = 0,
  TELEGRAM_INCOMPLETE,   // noch nicht alle 203 Bytes da
  TELEGRAM_TOO_LONG,     // mehr Bytes als das Telegramm haben darf
  TELEGRAM_WRONG_TYPE,   // Typ- oder Laengenbyte passt nicht zum Datentelegramm
  TELEGRAM_BAD_CHECKSUM  // Summe aller Bytes ist nicht 0
};

// Pruefsumme: die Summe aller Bytes einschliesslich des letzten
// (Pruefsummen-)Bytes muss 0 ergeben (8 Bit, Ueberlauf gewollt)
inline bool telegram_checksum_ok(const uint8_t *data, unsigned int len)
{
  uint8_t chk = 0;
  for (unsigned int i = 0; i < len; i++)
  {
    chk = (uint8_t)(chk + data[i]);
  }
  return (chk == 0);
}

// Reihenfolge der Pruefungen ist Absicht: erst Typ, dann Laenge, dann
// Pruefsumme. Ein verschobener Bytestrom faellt so schon am Typbyte auf und
// nicht erst mit 1/256 Wahrscheinlichkeit an der Pruefsumme.
inline TelegramCheck check_telegram(const uint8_t *data, unsigned int len)
{
  if (len < 2) // Typ- und Laengenbyte muessen mindestens dastehen
  {
    return TELEGRAM_INCOMPLETE;
  }
  if (data[0] != TELEGRAM_TYPE_DATA)
  {
    return TELEGRAM_WRONG_TYPE;
  }
  if (((unsigned int)data[1] + 3u) != TELEGRAM_DATA_LEN)
  {
    return TELEGRAM_WRONG_TYPE; // Laengenbyte passt nicht zum Datentelegramm
  }
  if (len < TELEGRAM_DATA_LEN)
  {
    return TELEGRAM_INCOMPLETE;
  }
  if (len > TELEGRAM_DATA_LEN)
  {
    return TELEGRAM_TOO_LONG;
  }
  if (!telegram_checksum_ok(data, len))
  {
    return TELEGRAM_BAD_CHECKSUM;
  }
  return TELEGRAM_OK;
}

// Klartext fuer die Logzeile
inline const char *telegram_check_text(TelegramCheck result)
{
  switch (result)
  {
  case TELEGRAM_OK:
    return "ok";
  case TELEGRAM_INCOMPLETE:
    return "unvollstaendig";
  case TELEGRAM_TOO_LONG:
    return "zu lang";
  case TELEGRAM_WRONG_TYPE:
    return "falscher Telegrammtyp";
  case TELEGRAM_BAD_CHECKSUM:
    return "Pruefsummenfehler";
  }
  return "unbekannt";
}
