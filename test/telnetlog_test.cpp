// Nachweis fuer die Regeln der nicht blockierenden Telnet-Ausgabe
// (src/telnetlog.h, 3.24.0, Befund S2 der Durchsicht 2026-10-15).
//
// Geprueft wird der Code, der auch auf dem Geraet laeuft: die Datei wird hier
// direkt eingebunden, es gibt keine Nachbildung. Dasselbe Muster wie
// test/karenz_test.cpp und test/sendwindow_test.cpp.
//
// Der Anlass: Ein Telnet-Client, der nicht mehr liest, hielt bis 3.23.2
// loop() minutenlang an (am Pruefling gemessen: Startseite 90 s ohne Antwort).
// Die neue Ausgabe wartet nie; ob ein Client noch lebt, entscheidet allein die
// Regel in telnetlog.h. Das Schreiben auf den Socket selbst ist Sache von
// telnetlog.cpp und im Hosttest nicht uebersetzbar - es gehoert zum Nachweis
// am Pruefling (test/telnet_haenger.py).
//
// Fall 7 baut die vorzeichenbehaftete Frist als Gegenbeispiel mit (die Form des
// Fehlers S1) und belegt damit, dass Fall 6 die Naht wirklich trifft.
//
// Bauen und ausfuehren:
//   c++ -std=c++17 -O2 -Wall -o /tmp/telnetlog_test test/telnetlog_test.cpp
//   /tmp/telnetlog_test          (Rueckgabewert != 0 = Test fehlgeschlagen)

#include <cstdio>
#include <cstdint>

#include "../src/telnetlog.h"

static int fehler = 0;

// eine Zusicherung mit Klartext, damit der CI-Log ohne Debugger lesbar ist
static void pruefe(bool bedingung, const char *was)
{
  printf("  [%s] %s\n", bedingung ? "ok " : "FEHLER", was);
  if (!bedingung)
    fehler++;
}

// 2^31 ms: die Naht der vorzeichenbehafteten Regel
static const uint32_t NAHT = 0x80000000u;

// Gegenbeispiel (Fall 7): dieselbe Frage mit vorzeichenbehaftetem Vergleich
// gegen eine feste Frist - die Form, die in S1 nach 24,86 Tagen kippte.
static bool alte_form_tot(uint32_t beginn, uint32_t jetzt)
{
  const uint32_t frist = beginn + TELNET_STILL_MAX_MS;
  return (int32_t)(jetzt - frist) >= 0;
}

int main(void)
{
  const uint32_t M = TELNET_STILL_MAX_MS;
  printf("Telnet-Ausgabe (src/telnetlog.h)\n");
  printf("TELNET_STILL_MAX_MS = %u ms\n\n", M);

  /***************************************************************************/
  /* 1. Anfangszustand und neuer Client                                      */
  /***************************************************************************/
  printf("1. Anfangszustand:\n");
  {
    TelnetClientZustand z;
    telnet_neuer_client(&z);
    pruefe(!telnet_tot(&z, 0), "neuer Client: nicht tot bei jetzt = 0");
    pruefe(!telnet_tot(&z, 0xFFFFFFFFu), "neuer Client: nicht tot bei jetzt = 2^32 - 1");
    pruefe(telnet_gelungen(&z) == 0, "nichts verworfen -> nichts zu melden");
  }

  /***************************************************************************/
  /* 2. Grenze auf die Millisekunde                                          */
  /***************************************************************************/
  printf("\n2. Grenze auf die Millisekunde:\n");
  {
    TelnetClientZustand z;
    telnet_neuer_client(&z);
    telnet_verworfen(&z, 1000u);
    pruefe(!telnet_tot(&z, 1000u), "direkt nach dem ersten Fehlschlag -> lebt");
    pruefe(!telnet_tot(&z, 1000u + M - 1), "1 ms vor der Grenze -> lebt");
    pruefe(telnet_tot(&z, 1000u + M), "auf der Grenze -> tot");
    pruefe(telnet_tot(&z, 1000u + M + 1), "1 ms danach -> tot");
    // weitere Fehlschlaege verschieben die Uhr NICHT: sie laeuft ab dem ersten
    telnet_verworfen(&z, 1000u + M - 1);
    pruefe(telnet_tot(&z, 1000u + M), "weitere Fehlschlaege stossen die Uhr nicht neu an");
  }

  /***************************************************************************/
  /* 3. Eine gelungene Ausgabe beendet die Serie                             */
  /***************************************************************************/
  printf("\n3. Gelungene Ausgabe beendet die Serie:\n");
  {
    TelnetClientZustand z;
    telnet_neuer_client(&z);
    telnet_verworfen(&z, 0u);
    telnet_verworfen(&z, 100u);
    telnet_verworfen(&z, 200u);
    pruefe(telnet_gelungen(&z) == 3, "meldet die drei fehlenden Zeilen");
    pruefe(!telnet_tot(&z, M + 5000u), "danach auch weit nach der alten Grenze nicht tot");
    pruefe(telnet_gelungen(&z) == 0, "die Meldung kommt nur einmal");
    // neue Serie startet mit eigener Uhr
    telnet_verworfen(&z, 50000u);
    pruefe(!telnet_tot(&z, 50000u + M - 1) && telnet_tot(&z, 50000u + M),
           "neue Serie: eigene Uhr ab 50000 ms");
  }

  /***************************************************************************/
  /* 4. Zaehlerueberlauf (millis) innerhalb einer Serie                      */
  /***************************************************************************/
  printf("\n4. millis()-Ueberlauf innerhalb der Serie:\n");
  {
    TelnetClientZustand z;
    telnet_neuer_client(&z);
    const uint32_t b = 0xFFFFF000u; // 4095 ms vor dem Ueberlauf
    telnet_verworfen(&z, b);
    pruefe(!telnet_tot(&z, b + M - 1), "1 ms vor der Grenze (uebergelaufen) -> lebt");
    pruefe(telnet_tot(&z, b + M), "auf der Grenze (uebergelaufen) -> tot");
  }

  /***************************************************************************/
  /* 5. Ohne Fehlschlag nie tot - auch nach sehr langer Stille               */
  /*                                                                         */
  /* Debug-Log aus, Client haengt trotzdem monatelang an der Leitung: der    */
  /* erste einzelne Fehlschlag danach darf ihn nicht sofort kappen.          */
  /***************************************************************************/
  printf("\n5. Lange Stille ohne Fehlschlag:\n");
  {
    TelnetClientZustand z;
    telnet_neuer_client(&z);
    pruefe(!telnet_tot(&z, NAHT), "kein Fehlschlag, 2^31 ms spaeter -> nicht tot");
    telnet_verworfen(&z, NAHT);
    pruefe(!telnet_tot(&z, NAHT), "erster Fehlschlag nach langer Stille -> lebt noch");
    pruefe(telnet_tot(&z, NAHT + M), "und ist erst nach der Grenze tot");
  }

  /***************************************************************************/
  /* 6. Die Naht der vorzeichenbehafteten Regel (S1)                         */
  /*                                                                         */
  /* Eine Serie, die ohne Aufruf ueber 2^31 ms stehen bleibt, bleibt tot.    */
  /***************************************************************************/
  printf("\n6. Die Naht nach 2^31 ms:\n");
  {
    TelnetClientZustand z;
    telnet_neuer_client(&z);
    const uint32_t b = 60000u;
    telnet_verworfen(&z, b);
    pruefe(telnet_tot(&z, b + NAHT - 1), "2^31 - 1 ms nach dem ersten Fehlschlag -> tot");
    pruefe(telnet_tot(&z, b + NAHT), "2^31 ms -> tot");
    pruefe(telnet_tot(&z, b + NAHT + 86400000u), "2^31 ms + 1 Tag -> tot");
    pruefe(telnet_tot(&z, b + 0xFFFFFFFFu), "2^32 - 1 ms -> tot (Ende des eindeutigen Bereichs)");
  }

  /***************************************************************************/
  /* 7. Gegenbeispiel: die vorzeichenbehaftete Frist                         */
  /*                                                                         */
  /* Belegt, dass Fall 6 die Naht wirklich trifft: dieselben Zeitpunkte      */
  /* liefern in der alten Form "lebt".                                       */
  /***************************************************************************/
  printf("\n7. Gegenbeispiel (vorzeichenbehaftete Frist):\n");
  {
    const uint32_t b = 60000u;
    pruefe(alte_form_tot(b, b + M), "alte Form: auf der Grenze -> tot (so weit richtig)");
    pruefe(!alte_form_tot(b, b + NAHT + M),
           "alte Form: 2^31 ms + 15 s -> wieder LEBT (der Fehler)");
  }

  /***************************************************************************/
  /* 8. Saettigung und Nullzeiger                                            */
  /***************************************************************************/
  printf("\n8. Saettigung und Nullzeiger:\n");
  {
    TelnetClientZustand z;
    telnet_neuer_client(&z);
    z.verworfen = UINT32_MAX - 1;
    telnet_verworfen(&z, 0u);
    telnet_verworfen(&z, 0u);
    telnet_verworfen(&z, 0u);
    pruefe(z.verworfen == UINT32_MAX, "Zaehler saettigt bei UINT32_MAX, laeuft nicht ueber");
    pruefe(telnet_gelungen(&z) == UINT32_MAX, "meldet den gesaettigten Wert");

    telnet_neuer_client(nullptr);
    telnet_verworfen(nullptr, 0u);
    pruefe(telnet_gelungen(nullptr) == 0, "Nullzeiger: gelungen -> 0");
    pruefe(!telnet_tot(nullptr, 0u), "Nullzeiger: tot -> false");
  }

  printf("\n");
  if (fehler)
  {
    printf("FEHLER: %d Zusicherung(en) gebrochen\n", fehler);
    return 1;
  }
  printf("alle Zusicherungen bestanden\n");
  return 0;
}
