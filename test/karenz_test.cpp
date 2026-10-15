// Nachweis fuer das Karenzfenster nach dem Verbinden (src/karenz.h, 3.23.1).
//
// Geprueft wird der Code, der auch auf dem Geraet laeuft: die Datei wird hier
// direkt eingebunden, es gibt keine Nachbildung, die auseinanderlaufen kann.
// Dasselbe Muster wie test/sendwindow_test.cpp fuer src/sendwindow.h.
//
// Der Anlass: Bis 3.23.0 stand die Frist als millis() + SUBSCRIBE_GRACE und
// wurde mit (long)(millis() - frist) < 0 geprueft. Das ist nur fuer Fristen
// sicher, die nach Ablauf nicht weiter verglichen werden - diese wurde es.
// 2^31 ms = 24,86 Tage nach dem letzten Connect meldete die Regel wieder
// "offen", und jedes Set-Kommando wurde still verworfen. An der Anlage nicht
// abzuwarten, im Hosttest in Millisekunden.
//
// Fall 9 baut die alte Regel als Gegenbeispiel mit und belegt damit, dass
// Fall 4 die Naht wirklich trifft - ein Test, der auch gegen die alte Regel
// gruen waere, haette nichts bewiesen.
//
// Bauen und ausfuehren:
//   c++ -std=c++17 -O2 -Wall -o /tmp/karenz_test test/karenz_test.cpp
//   /tmp/karenz_test             (Rueckgabewert != 0 = Test fehlgeschlagen)

#include <cstdio>
#include <cstdint>

#include "../src/karenz.h"

static int fehler = 0;

// eine Zusicherung mit Klartext, damit der CI-Log ohne Debugger lesbar ist
static void pruefe(bool bedingung, const char *was)
{
  printf("  [%s] %s\n", bedingung ? "ok " : "FEHLER", was);
  if (!bedingung)
    fehler++;
}

// 2^31 ms: die Naht der alten Regel
static const uint32_t NAHT = 0x80000000u;

// Die Regel bis 3.23.0, nur als Gegenbeispiel (Fall 9). Frist = beginn + dauer,
// vorzeichenbehafteter Vergleich, kein Merker - genau das war der Fehler.
static bool alte_regel_offen(uint32_t beginn, uint32_t jetzt, uint32_t dauer)
{
  const uint32_t frist = beginn + dauer;
  return (int32_t)(jetzt - frist) < 0;
}

int main(void)
{
  const uint32_t G = SUBSCRIBE_GRACE;
  printf("Karenzfenster (src/karenz.h)\n");
  printf("SUBSCRIBE_GRACE = %u ms\n\n", G);

  /***************************************************************************/
  /* 1. Anfangszustand                                                       */
  /***************************************************************************/
  printf("1. Anfangszustand {0, false}:\n");
  {
    Karenzfenster k = {0, false};
    pruefe(!karenz_offen(&k, 0, G), "nie geoeffnet -> zu, auch bei jetzt = 0");
    pruefe(!karenz_offen(&k, 1000, G), "nie geoeffnet -> zu, bei jetzt = 1000");
    pruefe(!karenz_offen(&k, NAHT, G), "nie geoeffnet -> zu, bei jetzt = 2^31");
  }

  /***************************************************************************/
  /* 2. Grundfall: die Raender des Fensters                                  */
  /***************************************************************************/
  printf("\n2. Grundfall (geoeffnet bei 1000):\n");
  {
    Karenzfenster k = {0, false};
    karenz_oeffnen(&k, 1000);
    pruefe(karenz_offen(&k, 1000, G), "im selben Millisekundentakt -> offen");
    pruefe(karenz_offen(&k, 1000 + G - 1, G), "1 ms vor dem Ende -> offen");
    pruefe(!karenz_offen(&k, 1000 + G, G), "genau am Ende -> zu");
    pruefe(!karenz_offen(&k, 1000 + G + 1, G), "1 ms danach -> bleibt zu");
  }

  /***************************************************************************/
  /* 3. millis()-Ueberlauf INNERHALB des Fensters                            */
  /*                                                                         */
  /* Der Zaehler laeuft mitten im Fenster ueber. Die unsigned Differenz muss */
  /* das ueberstehen.                                                        */
  /***************************************************************************/
  printf("\n3. Zaehlerueberlauf im Fenster (geoeffnet bei 0xFFFFF000):\n");
  {
    Karenzfenster k = {0, false};
    const uint32_t b = 0xFFFFF000u;
    karenz_oeffnen(&k, b);
    pruefe(karenz_offen(&k, b + G - 1, G), "nach dem Ueberlauf, 1 ms vor dem Ende -> offen");
    pruefe((uint32_t)(b + G - 1) < b, "(Kontrolle: der Zaehler ist wirklich uebergelaufen)");
    pruefe(!karenz_offen(&k, b + G, G), "genau am Ende -> zu");
  }

  /***************************************************************************/
  /* 4. DER BEFUND: die Naht nach 2^31 ms                                    */
  /*                                                                         */
  /* Das Fenster wird einmal nachgefuehrt (loop() tut das in jedem           */
  /* Durchlauf), danach darf es nie wieder aufgehen - weder an der Naht      */
  /* noch bei einem Tag danach noch ueber den vollen Zaehlerumlauf.          */
  /***************************************************************************/
  printf("\n4. Die Naht nach 2^31 ms (geoeffnet bei 60000, danach nachgefuehrt):\n");
  {
    Karenzfenster k = {0, false};
    const uint32_t b = 60000u;
    karenz_oeffnen(&k, b);
    pruefe(!karenz_offen(&k, b + G, G), "5 s spaeter nachgefuehrt -> zu");

    const uint32_t TAG = 86400000u;
    pruefe(!karenz_offen(&k, b + NAHT - 1, G), "2^31 - 1 ms danach -> zu");
    pruefe(!karenz_offen(&k, b + NAHT, G), "genau 2^31 ms danach -> zu");
    pruefe(!karenz_offen(&k, b + NAHT + G - 1, G), "2^31 ms + 5 s - 1 ms danach -> zu");
    pruefe(!karenz_offen(&k, b + NAHT + G, G), "2^31 ms + 5 s danach -> zu (die Naht der alten Regel)");
    pruefe(!karenz_offen(&k, b + NAHT + TAG, G), "2^31 ms + 1 Tag danach -> zu");
    pruefe(!karenz_offen(&k, b + 0xFFFFFFFFu, G), "2^32 - 1 ms danach -> zu");
    pruefe(!karenz_offen(&k, b + 1000u, G), "voller Umlauf + 1000 ms -> zu (Zaehler steht wieder auf b + 1000)");
  }

  /***************************************************************************/
  /* 5. Erster Aufruf erst NACH der Naht, ohne Nachfuehren dazwischen        */
  /*                                                                         */
  /* Ein Fenster, das nie nachgefuehrt wurde, ist nach 2^31 ms richtig zu -  */
  /* der erste Aufruf sieht die Differenz >= dauer und schliesst.            */
  /*                                                                         */
  /* GRENZE DER VORBEDINGUNG: Liegen zwischen Oeffnen und erstem Aufruf      */
  /* volle 2^32 ms, steht die Differenz wieder bei 0 und das Fenster ist     */
  /* "offen". Deshalb ruft loop() die Regel in JEDEM Durchlauf auf. Der      */
  /* Test dokumentiert die Grenze, statt sie zu verstecken.                  */
  /***************************************************************************/
  printf("\n5. Erster Aufruf erst nach der Naht:\n");
  {
    Karenzfenster k = {0, false};
    karenz_oeffnen(&k, 60000u);
    pruefe(!karenz_offen(&k, 60000u + NAHT, G), "erster Aufruf genau 2^31 ms spaeter -> zu");

    Karenzfenster u = {0, false};
    karenz_oeffnen(&u, 60000u);
    pruefe(karenz_offen(&u, 60000u, G),
           "GRENZE: erster Aufruf nach vollen 2^32 ms ohne Nachfuehren -> offen (Vorbedingung verletzt)");
  }

  /***************************************************************************/
  /* 6. Reconnect: wieder oeffnen nach dem Schliessen                        */
  /***************************************************************************/
  printf("\n6. Wieder oeffnen nach dem Schliessen (Reconnect):\n");
  {
    Karenzfenster k = {0, false};
    karenz_oeffnen(&k, 1000);
    pruefe(!karenz_offen(&k, 1000 + G, G), "erstes Fenster geschlossen");

    const uint32_t spaeter = 1000u + NAHT + 12345u; // Reconnect weit nach der Naht
    karenz_oeffnen(&k, spaeter);
    pruefe(karenz_offen(&k, spaeter, G), "neues Fenster -> offen");
    pruefe(karenz_offen(&k, spaeter + G - 1, G), "1 ms vor dem Ende -> offen");
    pruefe(!karenz_offen(&k, spaeter + G, G), "5 s spaeter -> zu");
  }

  /***************************************************************************/
  /* 7. Zweite Armierung am Ende von setup() (3.20.0, M1)                    */
  /*                                                                         */
  /* Das Fenster steht aus mqtt_reconnect() schon offen, setupTime() wartet  */
  /* bis zu 30 s auf NTP, dann wird neu geoeffnet. Gemessen wird ab dem      */
  /* neuen Zeitpunkt.                                                        */
  /***************************************************************************/
  printf("\n7. Oeffnen bei offenem Fenster (zweite Armierung):\n");
  {
    Karenzfenster k = {0, false};
    karenz_oeffnen(&k, 2000);                  // setupMqtt(): Set-Topics abonniert
    const uint32_t nach_ntp = 2000u + 30000u;  // setupTime() hat 30 s gewartet
    karenz_oeffnen(&k, nach_ntp);              // Ende von setup(), vor dem ersten loop()
    pruefe(karenz_offen(&k, nach_ntp, G), "beim ersten loop() -> offen (haette sonst nach 5 s zu sein)");
    pruefe(karenz_offen(&k, nach_ntp + G - 1, G), "Schwall wird im Fenster gelesen -> offen");
    pruefe(!karenz_offen(&k, nach_ntp + G, G), "5 s ab der zweiten Armierung -> zu");
  }

  /***************************************************************************/
  /* 8. Nullzeiger                                                           */
  /***************************************************************************/
  printf("\n8. Nullzeiger:\n");
  karenz_oeffnen(nullptr, 1000); // darf nicht abstuerzen
  pruefe(true, "karenz_oeffnen(nullptr) -> kein Absturz");
  pruefe(!karenz_offen(nullptr, 1000, G), "karenz_offen(nullptr) -> zu (false), kein Absturz");

  /***************************************************************************/
  /* 9. Gegenbeispiel: die alte Regel                                        */
  /*                                                                         */
  /* Belegt, dass Fall 4 die Naht wirklich trifft: dieselben Zeitpunkte      */
  /* liefern bei der alten Regel "offen".                                    */
  /***************************************************************************/
  printf("\n9. Gegenbeispiel (Regel bis 3.23.0):\n");
  {
    const uint32_t b = 60000u;
    pruefe(!alte_regel_offen(b, b + G, G), "alte Regel: 5 s danach -> zu (so weit richtig)");
    // Die Frist steht bei beginn + dauer, die Differenz kippt also erst um
    // die Dauer spaeter als bei 2^31 ms - die Naht liegt bei 2^31 + 5 s.
    pruefe(!alte_regel_offen(b, b + NAHT + G - 1, G),
           "alte Regel: 2^31 ms + 5 s - 1 ms danach -> noch zu");
    pruefe(alte_regel_offen(b, b + NAHT + G, G),
           "alte Regel: 2^31 ms + 5 s danach -> wieder OFFEN (der Fehler)");
    pruefe(alte_regel_offen(b, b + NAHT + 86400000u, G),
           "alte Regel: 2^31 ms + 1 Tag danach -> offen (jedes Kommando verworfen)");
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
