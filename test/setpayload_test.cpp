// Nachweis fuer die Ganzzahlregel und die Buchfuehrung ueber abgelehnte
// Set-Befehle (src/setpayload.h, 3.25.0, Befund S6 der Durchsicht
// 2026-10-15).
//
// Geprueft wird der Code, der auch auf dem Geraet laeuft: die Datei wird hier
// direkt eingebunden, es gibt keine Nachbildung. Es ist der erste Test des
// Set-Pfads, der nicht an einer Nachbildung haengt (C1 der Durchsicht).
//
// Die Regel ist die des bisherigen strtol(msg, &end, 10) - dieser Test haelt
// fest, WAS sie annimmt und ablehnt, damit eine spaetere Aenderung auffaellt.
// Fall 2 nennt die Annahmen, die auf den ersten Blick ueberraschen (" 33",
// "+33"): Sie waren immer so und bleiben es.
//
// Bauen und ausfuehren:
//   c++ -std=c++17 -O2 -Wall -o /tmp/setpayload_test test/setpayload_test.cpp
//   /tmp/setpayload_test   (Rueckgabewert != 0 = Test fehlgeschlagen)

#include <cstdio>
#include <cstdint>
#include <cstring>

#include "../src/setpayload.h"

static int fehler = 0;

// eine Zusicherung mit Klartext, damit der CI-Log ohne Debugger lesbar ist
static void pruefe(bool bedingung, const char *was)
{
  printf("  [%s] %s\n", bedingung ? "ok " : "FEHLER", was);
  if (!bedingung)
    fehler++;
}

// Hilfen: Eingabe -> angenommen und mit welchem Wert?
static bool angenommen(const char *eingabe, int32_t erwartet)
{
  int32_t wert = 12345;
  return setpayload_ganzzahl(eingabe, &wert) == SETPAYLOAD_OK && wert == erwartet;
}

static bool abgelehnt(const char *eingabe)
{
  int32_t wert = 12345;
  const bool abgelehnt_ = setpayload_ganzzahl(eingabe, &wert) == SETPAYLOAD_KEINE_GANZZAHL;
  // ein abgelehnter Wert darf den Ausgabewert nicht anfassen
  return abgelehnt_ && wert == 12345;
}

int main()
{
  printf("Fall 1: gewoehnliche Ganzzahlen\n");
  pruefe(angenommen("33", 33), "\"33\"");
  pruefe(angenommen("0", 0), "\"0\"");
  pruefe(angenommen("-5", -5), "\"-5\"");
  pruefe(angenommen("-0", 0), "\"-0\"");
  pruefe(angenommen("007", 7), "\"007\" (fuehrende Nullen)");

  printf("Fall 2: bisher angenommen und es bleibt so (kein Verhaltenswechsel)\n");
  pruefe(angenommen(" 33", 33), "\" 33\" (fuehrender Leerraum)");
  pruefe(angenommen("\t33", 33), "\"\\t33\" (fuehrender Tabulator)");
  pruefe(angenommen("+33", 33), "\"+33\" (Pluszeichen)");

  printf("Fall 3: abgelehnt - Dezimal, Exponent, Rest nach der Zahl (Anlass N1)\n");
  pruefe(abgelehnt("33.5"), "\"33.5\"");
  pruefe(abgelehnt("33,5"), "\"33,5\"");
  pruefe(abgelehnt("1e1"), "\"1e1\"");
  pruefe(abgelehnt("33 "), "\"33 \" (Leerraum dahinter)");
  pruefe(abgelehnt("33\n"), "\"33\\n\" (Zeilenumbruch dahinter)");
  pruefe(abgelehnt("33abc"), "\"33abc\"");

  printf("Fall 4: abgelehnt - keine Ziffer\n");
  pruefe(abgelehnt(""), "\"\" (leer)");
  pruefe(abgelehnt("-"), "\"-\"");
  pruefe(abgelehnt("+"), "\"+\"");
  pruefe(abgelehnt(" "), "\" \"");
  pruefe(abgelehnt("abc"), "\"abc\"");
  pruefe(abgelehnt("--5"), "\"--5\"");

  printf("Fall 5: abgelehnt - Basis 10, kein Hex\n");
  pruefe(abgelehnt("0x1A"), "\"0x1A\"");
  pruefe(abgelehnt("0X10"), "\"0X10\"");

  printf("Fall 6: Ueberlauf wird geklemmt, nicht abgeschnitten\n");
  pruefe(angenommen("99999999999", INT32_MAX), "99999999999 -> INT32_MAX");
  pruefe(angenommen("-99999999999", INT32_MIN), "-99999999999 -> INT32_MIN");
  pruefe(angenommen("2147483647", INT32_MAX), "2147483647 -> INT32_MAX (genau)");
  pruefe(angenommen("2147483648", INT32_MAX), "2147483648 -> INT32_MAX (eins darueber)");
  pruefe(angenommen("-2147483648", INT32_MIN), "-2147483648 -> INT32_MIN (genau)");
  pruefe(angenommen("99999999999999999999999", INT32_MAX), "weit ueber long long -> INT32_MAX");
  // Gegenprobe: ein stilles Abschneiden auf 32 Bit machte aus 4294967297 eine 1
  // und damit einen GUELTIGEN Wert. Die Klemme verhindert das.
  pruefe(angenommen("4294967297", INT32_MAX), "4294967297 wird NICHT zu 1");

  printf("Fall 7: Nullzeiger\n");
  int32_t wert = 7;
  pruefe(setpayload_ganzzahl(nullptr, &wert) == SETPAYLOAD_KEINE_GANZZAHL, "msg == nullptr");
  pruefe(setpayload_ganzzahl("1", nullptr) == SETPAYLOAD_KEINE_GANZZAHL, "wert == nullptr");

  printf("Fall 8: Buchfuehrung - Zaehler und letzter Befehl\n");
  SetAbgelehnt s;
  setabgelehnt_start(&s);
  pruefe(s.zaehler == 0 && s.letzter[0] == '\0' && !s.neu, "Start: 0, leer, nichts neu");
  setabgelehnt_merken(&s, "panasonic_heat_pump/set/Z1HeatRequestTemperature", "33.5");
  pruefe(s.zaehler == 1, "Zaehler 1");
  pruefe(strcmp(s.letzter, "Z1HeatRequestTemperature=33.5") == 0, "letzter = Name=Wert, ohne Praefix");
  pruefe(s.neu, "neu gesetzt");
  setabgelehnt_merken(&s, "panasonic_heat_pump/set/DHWTemp", "99");
  pruefe(s.zaehler == 2 && strcmp(s.letzter, "DHWTemp=99") == 0, "zweiter Befehl ersetzt den letzten");
  setabgelehnt_merken(&s, "ohneschraeg", "x");
  pruefe(strcmp(s.letzter, "ohneschraeg=x") == 0, "Topic ohne '/' bleibt ganz");
  setabgelehnt_merken(&s, nullptr, nullptr);
  pruefe(strcmp(s.letzter, "?=") == 0, "Nullzeiger: Platzhalter, kein Absturz");
  setabgelehnt_merken(&s, "a/set/Name", "");
  pruefe(strcmp(s.letzter, "Name=") == 0, "leerer Wert bleibt sichtbar");

  printf("Fall 9: Kuerzung auf 64 Zeichen und Klaerung von Fremdtext\n");
  char lang[300];
  memset(lang, 'x', sizeof(lang) - 1);
  lang[sizeof(lang) - 1] = '\0';
  setabgelehnt_merken(&s, "p/set/Name", lang);
  pruefe(strlen(s.letzter) == SETABGELEHNT_LETZTER_MAX, "auf genau 64 Zeichen gekuerzt");
  pruefe(strncmp(s.letzter, "Name=xxx", 8) == 0, "Anfang erhalten");
  pruefe(s.letzter[SETABGELEHNT_LETZTER_MAX] == '\0', "nullterminiert");
  // ein Name allein, der schon 64 Zeichen fuellt: kein '=' mehr, kein Ueberlauf
  char langername[100];
  memset(langername, 'n', sizeof(langername) - 1);
  langername[sizeof(langername) - 1] = '\0';
  setabgelehnt_merken(&s, langername, "w");
  pruefe(strlen(s.letzter) == SETABGELEHNT_LETZTER_MAX, "Name allein fuellt den Puffer: 64 Zeichen, kein Ueberlauf");
  setabgelehnt_merken(&s, "p/set/N", "a\nb\tc\x01\x7f\xc3\xa4z");
  pruefe(strcmp(s.letzter, "N=a?b?c????z") == 0,
         "Zeilenumbruch, Tabulator, Steuerzeichen, DEL und UTF-8 (2 Bytes) werden zu '?'");

  printf("Fall 10: der Zaehler saettigt, springt nicht auf 0\n");
  s.zaehler = UINT32_MAX;
  setabgelehnt_merken(&s, "p/set/N", "x");
  pruefe(s.zaehler == UINT32_MAX, "bleibt bei UINT32_MAX");

  printf("Fall 11: Veroeffentlichen mit Mindestabstand, auch ueber den Ueberlauf\n");
  setabgelehnt_start(&s);
  pruefe(!setabgelehnt_faellig(&s, 100u), "nichts neu -> nicht faellig");
  setabgelehnt_merken(&s, "p/set/N", "x");
  pruefe(setabgelehnt_faellig(&s, 100u), "neu und noch nie veroeffentlicht -> sofort faellig");
  setabgelehnt_veroeffentlicht(&s, 100u);
  pruefe(!s.neu, "nach dem Veroeffentlichen nicht mehr neu");
  setabgelehnt_merken(&s, "p/set/N", "y");
  pruefe(!setabgelehnt_faellig(&s, 100u + 1999u), "1,999 s danach: noch nicht");
  pruefe(setabgelehnt_faellig(&s, 100u + 2000u), "2,000 s danach: faellig");
  // ueber den millis()-Ueberlauf
  setabgelehnt_start(&s);
  setabgelehnt_merken(&s, "p/set/N", "x");
  setabgelehnt_veroeffentlicht(&s, 0xFFFFFFFFu - 500u);
  setabgelehnt_merken(&s, "p/set/N", "y");
  pruefe(!setabgelehnt_faellig(&s, 0xFFFFFFFFu - 500u + 1999u), "ueber den Ueberlauf: 1,999 s -> nicht");
  pruefe(setabgelehnt_faellig(&s, 0xFFFFFFFFu - 500u + 2000u), "ueber den Ueberlauf: 2,000 s -> faellig");
  pruefe(!setabgelehnt_faellig(nullptr, 0u), "Nullzeiger -> nicht faellig");

  printf("\n%s (%d Fehler)\n", fehler == 0 ? "ALLE FAELLE OK" : "FEHLGESCHLAGEN", fehler);
  return fehler == 0 ? 0 : 1;
}
