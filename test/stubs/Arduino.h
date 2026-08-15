#pragma once
// Arduino-Ersatz fuer die Hosttests - nur was der Dekodierpfad braucht.
#include <cstdint>
#include <cstdio>
#include <cstring>
typedef uint8_t byte;
static inline unsigned int word(byte h, byte l) { return ((unsigned int)h << 8) | l; }
static inline char *dtostrf(double val, signed char width, unsigned char prec, char *out)
{
  snprintf(out, 32, "%*.*f", (int)width, (int)prec, val);
  return out;
}
static inline unsigned long millis() { return 0; }
