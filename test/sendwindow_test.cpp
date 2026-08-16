// Nachweis fuer die Zeitregeln des Kommando-Sammelfensters aus 3.8.0
// (src/sendwindow.h).
//
// Geprueft wird der Code, der auch auf dem Geraet laeuft: die Datei wird hier
// direkt eingebunden, es gibt keine Nachbildung, die auseinanderlaufen kann.
// Dasselbe Muster wie test/telegramm_test.cpp fuer src/telegram.h.
//
// Drei Fragen beantwortet dieser Test:
//  1. Terminiert das Sammelfenster unter einem dichten SET-Strom? (der Fehler,
//     den 3.8.0 behebt - vorher standen Senden UND Abfrage still)
//  2. Bleibt das normale Sammelverhalten unveraendert? (mehrere SETs eines
//     Re-Asserts muessen weiterhin in EIN Telegramm wandern - keine falsch
//     positiven, sonst kostet jeder Kanal eine eigene Runde)
//  3. Haelt die Regel dem millis()-Ueberlauf nach 49,7 Tagen stand? An der
//     Anlage waere das nicht abzuwarten.
//
// Bauen und ausfuehren:
//   c++ -std=c++17 -O2 -Wall -o /tmp/sendwindow_test test/sendwindow_test.cpp
//   /tmp/sendwindow_test         (Rueckgabewert != 0 = Test fehlgeschlagen)

#include <cstdio>
#include <cstdint>

#include "../src/sendwindow.h"

// wie in HeishaMon.h - der Sammeltimer, der nach jeder Verlaengerung neu laeuft
#define COMMANDTIMER 500u

static int fehler = 0;

// eine Zusicherung mit Klartext, damit der CI-Log ohne Debugger lesbar ist
static void pruefe(bool bedingung, const char *was)
{
  printf("  [%s] %s\n", bedingung ? "ok " : "FEHLER", was);
  if (!bedingung)
    fehler++;
}

static void pruefe_zahl(uint32_t ist, uint32_t soll, const char *was)
{
  bool ok = (ist == soll);
  printf("  [%s] %-58s (erwartet %u, ist %u)\n", ok ? "ok " : "FEHLER", was, soll, ist);
  if (!ok)
    fehler++;
}

/*****************************************************************************/
/* Ablaufprobe: SET-Strom mit festem Abstand                                 */
/*                                                                           */
/* Bildet NICHT die Firmware nach, sondern wertet genau die Regel aus, die   */
/* register_new_command() an jedem eintreffenden SET stellt. Rueckgabe: die  */
/* Zahl der Verlaengerungen, bis der Deckel greift. Der Sendezeitpunkt ist   */
/* dann der letzte Anstoss + COMMANDTIMER.                                   */
/*                                                                           */
/* obergrenze bricht die Schleife ab, falls die Regel NICHT terminiert -     */
/* ohne sie wuerde der Test bei einem Rueckschritt endlos laufen statt zu    */
/* scheitern.                                                                */
/*****************************************************************************/
static uint32_t verlaengerungen_bis_deckel(uint32_t start, uint32_t abstand,
                                           uint32_t *letzter_anstoss,
                                           uint32_t obergrenze)
{
  uint32_t zaehler = 0;
  uint32_t jetzt = start;
  *letzter_anstoss = start;

  while (zaehler < obergrenze)
  {
    jetzt += abstand; // naechstes SET trifft ein
    if (!send_window_may_extend(jetzt, start, COMMAND_WINDOW_MAX))
    {
      break; // Deckel erreicht, der laufende Timer feuert von selbst
    }
    zaehler++;
    *letzter_anstoss = jetzt;
  }
  return zaehler;
}

int main(void)
{
  printf("Zeitregeln des Sammelfensters (src/sendwindow.h)\n");
  printf("COMMAND_WINDOW_MAX = %u ms, COMMAND_DEFER_MAX = %u, COMMANDTIMER = %u ms\n\n",
         COMMAND_WINDOW_MAX, COMMAND_DEFER_MAX, COMMANDTIMER);

  /***************************************************************************/
  /* 1. Deckel des Sammelfensters - die Raender                              */
  /***************************************************************************/
  printf("Deckel, Randfaelle:\n");
  pruefe(send_window_may_extend(0, 0, COMMAND_WINDOW_MAX),
         "im selben Millisekundentakt geoeffnet -> darf verlaengern");
  pruefe(send_window_may_extend(COMMAND_WINDOW_MAX - 1, 0, COMMAND_WINDOW_MAX),
         "1 ms vor dem Deckel -> darf verlaengern");
  pruefe(!send_window_may_extend(COMMAND_WINDOW_MAX, 0, COMMAND_WINDOW_MAX),
         "genau auf dem Deckel -> darf NICHT mehr verlaengern");
  pruefe(!send_window_may_extend(COMMAND_WINDOW_MAX + 1, 0, COMMAND_WINDOW_MAX),
         "1 ms nach dem Deckel -> darf NICHT mehr verlaengern");

  /***************************************************************************/
  /* 2. Normalbetrieb darf sich nicht geaendert haben                        */
  /*                                                                         */
  /* Der 5-min-Re-Assert der Node-RED-Steuerung schickt sechs Kanaele in     */
  /* einem Schwung; gemessen am 2026-08-13 kamen sie als EIN Telegramm an    */
  /* der Waermepumpe an. Bei Abstaenden im Millisekundenbereich muss die     */
  /* Regel also durchweg verlaengern.                                        */
  /***************************************************************************/
  printf("\nNormalbetrieb (Re-Assert, sechs Kanaele dicht hintereinander):\n");
  {
    bool alle_verlaengert = true;
    for (uint32_t i = 1; i <= 6; i++)
    {
      if (!send_window_may_extend(i * 5, 0, COMMAND_WINDOW_MAX)) // 5 ms Abstand
        alle_verlaengert = false;
    }
    pruefe(alle_verlaengert, "sechs SETs im 5-ms-Abstand landen alle im selben Fenster");
  }

  /***************************************************************************/
  /* 3. Der Fehlerfall aus 3.7.0: dichter SET-Strom                          */
  /***************************************************************************/
  printf("\nDichter SET-Strom (der Fall, den 3.8.0 behebt):\n");
  {
    const uint32_t OBERGRENZE = 100000; // bricht ab, falls die Regel nicht greift
    uint32_t letzter = 0;

    // 400 ms Abstand: kuerzer als COMMANDTIMER, der Timer wurde bis 3.7.0 also
    // immer wieder zurueckgesetzt und feuerte nie
    uint32_t n = verlaengerungen_bis_deckel(0, 400, &letzter, OBERGRENZE);
    pruefe(n < OBERGRENZE, "400-ms-Strom: Fenster terminiert (lief vor 3.8.0 unbegrenzt)");
    pruefe_zahl(n, 4, "400-ms-Strom: Zahl der Verlaengerungen");
    pruefe_zahl(letzter + COMMANDTIMER, 2100, "400-ms-Strom: Sendezeitpunkt in ms");

    // 100 ms Abstand - der ungemuetlichere Fall
    n = verlaengerungen_bis_deckel(0, 100, &letzter, OBERGRENZE);
    pruefe(n < OBERGRENZE, "100-ms-Strom: Fenster terminiert");
    pruefe(letzter + COMMANDTIMER <= COMMAND_WINDOW_MAX + COMMANDTIMER,
           "100-ms-Strom: Sendezeitpunkt haelt die zugesagte Obergrenze ein");

    // 1 ms Abstand - dichter geht es nicht
    n = verlaengerungen_bis_deckel(0, 1, &letzter, OBERGRENZE);
    pruefe(n < OBERGRENZE, "1-ms-Strom: Fenster terminiert");
    pruefe(letzter + COMMANDTIMER <= COMMAND_WINDOW_MAX + COMMANDTIMER,
           "1-ms-Strom: Sendezeitpunkt haelt die zugesagte Obergrenze ein");
  }

  /***************************************************************************/
  /* 4. Zugesagte Obergrenze ueber alle Abstaende                            */
  /*                                                                         */
  /* Der Kommentar in register_new_command sagt zu: spaetestens              */
  /* COMMAND_WINDOW_MAX + COMMANDTIMER nach dem ersten SET geht das          */
  /* Telegramm raus. Hier gegen jeden Abstand von 1 bis 3000 ms geprueft.    */
  /*                                                                         */
  /* Der Wert wird knapp NICHT erreicht, und zwar aus einem Grund, der beim  */
  /* Aufschreiben der Zusicherung erst auffiel: der letzte Anstoss kann      */
  /* hoechstens bei COMMAND_WINDOW_MAX - 1 liegen (auf dem Deckel selbst     */
  /* wird ja nicht mehr verlaengert), also 1999 + 500 = 2499 ms. Die Zusage  */
  /* "hoechstens 2,5 s" stimmt damit, ist aber eine strikte Obergrenze.      */
  /***************************************************************************/
  printf("\nZugesagte Obergrenze (Abstaende 1..3000 ms):\n");
  {
    uint32_t schlimmster = 0;
    for (uint32_t abstand = 1; abstand <= 3000; abstand++)
    {
      uint32_t letzter = 0;
      (void)verlaengerungen_bis_deckel(0, abstand, &letzter, 100000);
      uint32_t sendezeit = letzter + COMMANDTIMER;
      if (sendezeit > schlimmster)
        schlimmster = sendezeit;
    }
    pruefe_zahl(schlimmster, COMMAND_WINDOW_MAX + COMMANDTIMER - 1,
                "spaetester Sendezeitpunkt ueber alle Abstaende");
    pruefe(schlimmster < COMMAND_WINDOW_MAX + COMMANDTIMER,
           "Zusage aus register_new_command eingehalten (< 2,5 s)");
  }

  /***************************************************************************/
  /* 5. millis()-Ueberlauf nach 49,7 Tagen                                   */
  /*                                                                         */
  /* Das Geraet laeuft monatelang durch, der Zaehler laeuft also im Betrieb  */
  /* wirklich ueber. Faellt das Fenster genau auf die Naht, muss die Regel   */
  /* weiter stimmen. Gegenprobe unten zeigt, was die naive Schreibweise      */
  /* (now < start + limit) hier anrichten wuerde.                            */
  /***************************************************************************/
  printf("\nmillis()-Ueberlauf (Fenster liegt auf der Naht):\n");
  {
    const uint32_t kurz_vor_ende = 0xFFFFFF00u; // 256 ms vor dem Ueberlauf
    pruefe(send_window_may_extend(kurz_vor_ende + 100, kurz_vor_ende, COMMAND_WINDOW_MAX),
           "100 ms spaeter, noch vor der Naht -> darf verlaengern");
    pruefe(send_window_may_extend(100, kurz_vor_ende, COMMAND_WINDOW_MAX),
           "356 ms spaeter, hinter der Naht -> darf verlaengern");
    pruefe(send_window_may_extend(COMMAND_WINDOW_MAX - 257, kurz_vor_ende, COMMAND_WINDOW_MAX),
           "1 ms vor dem Deckel, hinter der Naht -> darf verlaengern");
    pruefe(!send_window_may_extend(COMMAND_WINDOW_MAX - 256, kurz_vor_ende, COMMAND_WINDOW_MAX),
           "genau auf dem Deckel, hinter der Naht -> darf NICHT verlaengern");

    // Ablaufprobe ueber die Naht: muss dieselbe Zahl liefern wie ohne Ueberlauf
    uint32_t letzter_normal = 0, letzter_naht = 0;
    uint32_t n_normal = verlaengerungen_bis_deckel(0, 400, &letzter_normal, 100000);
    uint32_t n_naht = verlaengerungen_bis_deckel(kurz_vor_ende, 400, &letzter_naht, 100000);
    pruefe_zahl(n_naht, n_normal, "400-ms-Strom ueber die Naht: gleiche Zahl Verlaengerungen");
    pruefe_zahl((uint32_t)(letzter_naht - kurz_vor_ende), letzter_normal,
                "400-ms-Strom ueber die Naht: gleicher Sendezeitpunkt");

    // Gegenprobe: die naive Formulierung faellt hier um. Sie steht bewusst
    // NICHT im Firmwarecode - der Test belegt nur, dass der Unterschied real
    // ist und die gewaehlte Schreibweise nicht Geschmackssache.
    bool naiv = (kurz_vor_ende + 100) < (kurz_vor_ende + COMMAND_WINDOW_MAX); // laeuft ueber
    pruefe(naiv == false,
           "Gegenprobe: naives (now < start + limit) liegt an der Naht falsch");
    pruefe(send_window_may_extend(kurz_vor_ende + 100, kurz_vor_ende, COMMAND_WINDOW_MAX) != naiv,
           "Gegenprobe: die benutzte Regel liefert an derselben Stelle das Richtige");
  }

  /***************************************************************************/
  /* 6. Grenze fuers Verschieben des Sendens                                 */
  /***************************************************************************/
  printf("\nVerschieben des Sendens (Notausgang):\n");
  pruefe(send_may_defer(0, COMMAND_DEFER_MAX), "erste Runde darf verschieben");
  pruefe(send_may_defer(COMMAND_DEFER_MAX - 1, COMMAND_DEFER_MAX),
         "letzte erlaubte Runde darf verschieben");
  pruefe(!send_may_defer(COMMAND_DEFER_MAX, COMMAND_DEFER_MAX),
         "auf der Grenze wird erzwungen gesendet");
  pruefe(!send_may_defer(COMMAND_DEFER_MAX + 1, COMMAND_DEFER_MAX),
         "ueber der Grenze wird erzwungen gesendet");

  // Der Notausgang darf nie vor dem Serial-Timeout greifen: timeout_serial
  // gibt die Leitung nach SERIALTIMEOUT (600 ms) frei, das Verschieben deckt
  // COMMAND_DEFER_MAX * COMMANDTIMER ab. Sonst wuerde die Warnung im
  // Normalbetrieb auftauchen statt nur im Fehlerfall.
  pruefe((COMMAND_DEFER_MAX * COMMANDTIMER) > 600u,
         "Verschiebefenster ist laenger als SERIALTIMEOUT (600 ms)");

  printf("\n%s (%d Abweichungen)\n", (fehler == 0) ? "BESTANDEN" : "FEHLGESCHLAGEN", fehler);
  return (fehler == 0) ? 0 : 1;
}
