// Nachweis fuer die Verbindungswacht aus 3.13.0 (src/verbindung.h).
//
// Geprueft wird der Code, der auch auf dem Geraet laeuft: die Datei wird hier
// direkt eingebunden, es gibt keine Nachbildung, die auseinanderlaufen kann.
// Gleiches Muster wie test/sendwindow_test.cpp und test/notbetrieb_test.cpp.
//
// Worum es geht: Faellt der ioBroker aus, heizt die Waermepumpe mit dem
// zuletzt gesetzten Sollwert einfach weiter - kein Alarm, kein Hinweis. Die
// Weboberflaeche soll das sagen. Damit sie es RICHTIG sagt, muessen sechs
// Dinge stimmen, und alle sind am Geraet schlecht bis gar nicht nachweisbar:
//
//  1. Die Karenz von 5 Minuten - kurze Broker-Neustarts duerfen keine
//     Stoermeldung ausloesen, die von selbst wieder verschwindet.
//  2. Der Sonderfall "seit dem Neustart nie verbunden" - dort ist die wahre
//     Ausfalldauer unbekannt, eine Minutenangabe waere gelogen.
//  3. Der millis()-Ueberlauf nach 49,7 Tagen - eine bei jeder Abfrage neu
//     gerechnete Dauer faellt dort unter die Karenz zurueck und die
//     Stoermeldung verschwaende ausgerechnet nach einem langen Ausfall.
//  4. Der Deckel bei 30 Tagen samt Textform.
//  5. Der Herzschlag (3.13.0): Broker erreichbar, aber seit ueber zwoelf
//     Minuten kein Kommando - die Kaskadenregelung rechnet nicht mehr.
//  6. Der Vorrang zwischen beiden und die Regel, dass die Stumm-Uhr ohne
//     Verbindung NICHT laeuft. Ohne sie meldete die Seite nach jeder
//     Rueckkehr des Brokers sofort einen zweiten Fehler, den es nie gab.
//
// Bauen und ausfuehren:
//   c++ -std=c++17 -O2 -Wall -o /tmp/verbindung_test test/verbindung_test.cpp
//   /tmp/verbindung_test         (Rueckgabewert != 0 = Test fehlgeschlagen)

#include <cstdio>
#include <cstdint>
#include <cstring>

#include "../src/verbindung.h"

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

static void pruefe_text(const char *ist, const char *soll, const char *was)
{
  bool ok = (strcmp(ist, soll) == 0);
  printf("  [%s] %-40s (erwartet \"%s\", ist \"%s\")\n", ok ? "ok " : "FEHLER", was, soll, ist);
  if (!ok)
    fehler++;
}

/*****************************************************************************/
/* Die Wacht ueber eine Zeitspanne nachfuehren                               */
/*                                                                           */
/* Bildet loop() nach: die Firmware ruft verbindung_nachfuehren() in jedem   */
/* Durchlauf auf. Die Schrittweite ist frei waehlbar, solange sie unter der  */
/* millis()-Naht bleibt - der Test nutzt das, um 30 Tage in Stundenschritten */
/* zu durchlaufen statt in Millisekunden.                                    */
/*                                                                           */
/* Der ERSTE Aufruf erfolgt zum Startzeitpunkt selbst, noch ohne             */
/* Zeitfortschritt. Das ist kein Schoenheitsfehler, sondern die Nachbildung  */
/* des Verlustmoments: In diesem Durchlauf merkt die Firmware, dass die      */
/* Verbindung weg ist, und die Ausfalldauer ist dort per Definition null.    */
/* Ohne diesen Aufruf ginge der erste Schritt fuer die Erkennung drauf und   */
/* jede gepruefte Dauer laege um eine Schrittweite daneben.                  */
/*                                                                           */
/* Rueckgabe: der Zeitpunkt am Ende, damit der Aufrufer weiterrechnen kann.  */
/*****************************************************************************/
static uint32_t laufen_lassen(VerbindungsWacht *w, bool verbunden,
                              uint32_t von, uint32_t dauer, uint32_t schritt)
{
  uint32_t jetzt = von;
  uint32_t rest = dauer;
  uint32_t verworfen = 0;

  (void)verbindung_nachfuehren(w, verbunden, jetzt, &verworfen); // Verlustmoment
  while (rest > 0)
  {
    uint32_t weiter = (rest < schritt) ? rest : schritt;
    jetzt += weiter;
    rest -= weiter;
    (void)verbindung_nachfuehren(w, verbunden, jetzt, &verworfen);
  }
  return jetzt;
}

int main(void)
{
  VerbindungsWacht w;
  char text[48];

  /***************************************************************************/
  /* 1. Anfangszustand: getrennt, noch nie verbunden                         */
  /***************************************************************************/
  printf("\nAnfangszustand nach dem Einschalten:\n");
  verbindung_init(&w, 0);
  pruefe(w.broker.laeuft, "startet als getrennt (setupMqtt kommt erst danach)");
  pruefe(!w.je_verbunden, "startet mit 'nie verbunden'");
  pruefe(verbindung_lage(&w) == VERBINDUNG_KARENZ,
         "direkt nach dem Einschalten gilt die Karenz, keine Stoermeldung");
  pruefe_zahl(verbindung_ausfall_sekunden(&w), 0, "Ausfalldauer beim Start");

  /***************************************************************************/
  /* 2. Die Karenz von 5 Minuten                                             */
  /*                                                                          */
  /* Der wichtigste Punkt ist die Grenze selbst: eine Sekunde davor darf      */
  /* nichts stehen, auf der Grenze muss die Meldung da sein. Ein Test, der    */
  /* nur "nach 10 Minuten steht es da" prueft, wuerde eine um Faktor zwei     */
  /* falsche Karenz nicht bemerken.                                           */
  /***************************************************************************/
  printf("\nKarenz von 5 Minuten (Broker weg, Geraet war vorher verbunden):\n");
  verbindung_init(&w, 0);
  uint32_t t = 1000;
  (void)verbindung_nachfuehren(&w, true, t, nullptr); // erste Verbindung steht
  pruefe(verbindung_lage(&w) == VERBINDUNG_VERBUNDEN, "verbunden wird als verbunden gemeldet");
  pruefe(w.je_verbunden, "die erste Verbindung ist vermerkt");

  t = laufen_lassen(&w, false, t, VERBINDUNG_KARENZ_MS - 1000u, 1000u);
  pruefe(verbindung_lage(&w) == VERBINDUNG_KARENZ,
         "eine Sekunde vor der Grenze: noch keine Stoermeldung");

  t = laufen_lassen(&w, false, t, 1000u, 1000u);
  pruefe(verbindung_lage(&w) == VERBINDUNG_GESTOERT,
         "auf der Grenze: Stoermeldung steht");
  pruefe_zahl(verbindung_ausfall_sekunden(&w), 300, "Ausfalldauer auf der Grenze (Sekunden)");

  verbindung_dauer_text(text, sizeof(text), verbindung_ausfall_sekunden(&w), w.broker.ueber_deckel);
  pruefe_text(text, "5 Minuten", "Text auf der Grenze");

  /***************************************************************************/
  /* 3. Der Sonderfall: seit dem Einschalten nie verbunden                   */
  /*                                                                          */
  /* Hier ist die wahre Ausfalldauer unbekannt - der Broker kann seit Tagen   */
  /* weg sein, das Geraet ist nur gerade neu gestartet. Die Lage muss sich    */
  /* deshalb unterscheiden lassen, damit die Seite "seit dem Neustart" statt  */
  /* einer falschen Minutenzahl schreiben kann.                               */
  /***************************************************************************/
  printf("\nNie verbunden gewesen (Neustart, waehrend der Server aus war):\n");
  verbindung_init(&w, 0);
  t = laufen_lassen(&w, false, 0, VERBINDUNG_KARENZ_MS + 60000u, 1000u);
  pruefe(verbindung_lage(&w) == VERBINDUNG_GESTOERT_SEIT_NEUSTART,
         "eigene Lage, solange nie eine Verbindung bestand");
  pruefe(verbindung_lage(&w) != VERBINDUNG_GESTOERT,
         "wird NICHT mit dem normalen Ausfall verwechselt");

  // sobald die Verbindung einmal stand, gilt der Sonderfall nicht mehr
  (void)verbindung_nachfuehren(&w, true, t, nullptr);
  t = laufen_lassen(&w, false, t, VERBINDUNG_KARENZ_MS + 1000u, 1000u);
  pruefe(verbindung_lage(&w) == VERBINDUNG_GESTOERT,
         "nach der ersten Verbindung ist es ein normaler Ausfall");

  /***************************************************************************/
  /* 4. Die Rueckkehr und ihre Log-Zeile                                     */
  /*                                                                          */
  /* Gemeldet wird nur, was auch auf der Seite stand. Ein Aussetzer unter der */
  /* Karenz war dort kein Thema und soll das Log nicht fuellen - sonst steht  */
  /* nach jedem Adapter-Neustart eine Stoermeldung im Log, die keine ist.     */
  /***************************************************************************/
  printf("\nRueckkehr der Verbindung:\n");
  verbindung_init(&w, 0);
  t = 1000;
  (void)verbindung_nachfuehren(&w, true, t, nullptr);
  t = laufen_lassen(&w, false, t, 14u * 60u * 1000u, 1000u); // 14 Minuten weg

  uint32_t gemeldet = 0;
  bool meldung = verbindung_nachfuehren(&w, true, t, &gemeldet);
  pruefe(meldung, "Rueckkehr nach 14 Minuten wird gemeldet");
  pruefe_zahl(gemeldet, 14u * 60u, "gemeldete Ausfalldauer in Sekunden");
  pruefe(verbindung_lage(&w) == VERBINDUNG_VERBUNDEN, "danach gilt wieder 'verbunden'");
  pruefe_zahl(verbindung_ausfall_sekunden(&w), 0, "Dauer ist nach der Rueckkehr zurueckgesetzt");

  // zweiter Aufruf mit stehender Verbindung darf NICHT erneut melden
  gemeldet = 0;
  t += 1000u;
  pruefe(!verbindung_nachfuehren(&w, true, t, &gemeldet),
         "die Meldung kommt genau einmal, nicht in jedem loop()-Durchlauf");

  // kurzer Aussetzer unter der Karenz: keine Meldung
  t = laufen_lassen(&w, false, t, 60000u, 1000u); // 1 Minute weg
  gemeldet = 4711;
  pruefe(!verbindung_nachfuehren(&w, true, t, &gemeldet),
         "Aussetzer unter der Karenz wird nicht gemeldet");
  pruefe_zahl(gemeldet, 4711, "und der Ausgabewert bleibt unangetastet");

  /***************************************************************************/
  /* 5. Der millis()-Ueberlauf nach 49,7 Tagen                               */
  /*                                                                          */
  /* Der Fall, wegen dem die Dauer fortgeschrieben und nicht bei jeder        */
  /* Abfrage neu gerechnet wird. Eine gerechnete Dauer faellt an der Naht auf */
  /* Null zurueck - die Stoermeldung verschwaende dann ausgerechnet nach      */
  /* einem langen Ausfall, und die Seite behauptete wieder "alles in          */
  /* Ordnung". An der Anlage waere das nicht abzuwarten.                      */
  /***************************************************************************/
  printf("\nmillis()-Ueberlauf nach 49,7 Tagen:\n");
  {
    // Ausfallbeginn 10 Minuten vor der Naht
    const uint32_t naht = 0xFFFFFFFFu;
    const uint32_t start = naht - (10u * 60u * 1000u);

    verbindung_init(&w, start);
    (void)verbindung_nachfuehren(&w, true, start, nullptr);
    uint32_t jetzt = laufen_lassen(&w, false, start, 8u * 60u * 1000u, 1000u);
    pruefe(verbindung_lage(&w) == VERBINDUNG_GESTOERT, "vor der Naht: Stoermeldung steht");
    pruefe_zahl(verbindung_ausfall_sekunden(&w), 8u * 60u, "vor der Naht: 8 Minuten");

    // ueber die Naht hinweg weitere 10 Minuten
    jetzt = laufen_lassen(&w, false, jetzt, 10u * 60u * 1000u, 1000u);
    pruefe(verbindung_lage(&w) == VERBINDUNG_GESTOERT,
           "nach der Naht: Stoermeldung steht immer noch");
    pruefe_zahl(verbindung_ausfall_sekunden(&w), 18u * 60u,
                "nach der Naht: Dauer laeuft richtig weiter");

    // Gegenprobe: die naive Rechnung liegt an derselben Stelle falsch. Sie
    // steht bewusst NICHT im Firmwarecode - der Test belegt nur, dass der
    // Unterschied real ist und die gewaehlte Bauweise nicht Geschmackssache.
    uint32_t naiv_ms = (uint32_t)(jetzt - w.broker.seit);
    pruefe(naiv_ms == 18u * 60u * 1000u,
           "Gegenprobe: die unsigned-Differenz selbst traegt ueber die Naht");
    bool naiv_vergleich = (jetzt >= w.broker.seit + VERBINDUNG_KARENZ_MS);
    pruefe(naiv_vergleich == false,
           "Gegenprobe: naives (now >= start + karenz) liegt an der Naht falsch");
    pruefe(verbindung_lage(&w) == VERBINDUNG_GESTOERT,
           "Gegenprobe: die benutzte Bauweise liefert an derselben Stelle das Richtige");
  }

  /***************************************************************************/
  /* 6. Der Deckel bei 30 Tagen                                              */
  /*                                                                          */
  /* In Stundenschritten durchlaufen - die Firmware fuehrt im               */
  /* Millisekundentakt nach, fuer die Regel ist nur wichtig, dass ein        */
  /* Schritt kleiner als die halbe millis()-Breite bleibt.                    */
  /***************************************************************************/
  printf("\nDeckel bei 30 Tagen:\n");
  {
    verbindung_init(&w, 0);
    (void)verbindung_nachfuehren(&w, true, 0, nullptr);
    const uint32_t stunde = 3600u * 1000u;

    uint32_t jetzt = laufen_lassen(&w, false, 0, 29u * 24u * stunde, stunde);
    pruefe(!w.broker.ueber_deckel, "nach 29 Tagen ist der Deckel noch nicht erreicht");
    pruefe_zahl(verbindung_ausfall_sekunden(&w), 29u * 24u * 3600u, "29 Tage in Sekunden");
    verbindung_dauer_text(text, sizeof(text), verbindung_ausfall_sekunden(&w), w.broker.ueber_deckel);
    pruefe_text(text, "29 Tagen", "Text nach 29 Tagen");

    jetzt = laufen_lassen(&w, false, jetzt, 2u * 24u * stunde, stunde);
    pruefe(w.broker.ueber_deckel, "nach 31 Tagen greift der Deckel");
    verbindung_dauer_text(text, sizeof(text), verbindung_ausfall_sekunden(&w), w.broker.ueber_deckel);
    pruefe_text(text, "mehr als 30 Tagen", "Text ueber dem Deckel");

    // ueber die Naht hinaus weiterlaufen: der Deckel darf nicht zurueckkippen
    (void)laufen_lassen(&w, false, jetzt, 25u * 24u * stunde, stunde);
    pruefe(w.broker.ueber_deckel, "der Deckel kippt auch ueber die millis()-Naht nicht zurueck");
    pruefe(verbindung_lage(&w) == VERBINDUNG_GESTOERT,
           "und die Stoermeldung steht weiterhin");

    // nach der Rueckkehr ist alles zurueckgesetzt
    (void)verbindung_nachfuehren(&w, true, jetzt + stunde, nullptr);
    pruefe(!w.broker.ueber_deckel, "die Rueckkehr setzt den Deckel zurueck");
    pruefe(!w.broker.karenz_ueber, "die Rueckkehr setzt den Karenzmerker zurueck");
  }

  /***************************************************************************/
  /* 6a. Der boesartige Zeitpunkt: Ausfalldauer knapp UEBER 49,7 Tagen       */
  /*                                                                          */
  /* Hier steht die eigentliche Behauptung dieser Datei auf dem Pruefstand.   */
  /* Waere die Dauer bei jeder Abfrage aus (now - getrennt_seit) gerechnet,   */
  /* liefe sie an der millis()-Naht auf null zurueck und die Seite meldete    */
  /* wieder "alles in Ordnung" - nach fast zwei Monaten Ausfall, also         */
  /* ausgerechnet dann, wenn die Meldung am dringendsten waere.               */
  /*                                                                          */
  /* Der Zeitpunkt wird bewusst so getroffen, dass die naive Differenz UNTER  */
  /* der Karenz liegt: nur dann ist der Unterschied nachgewiesen und nicht    */
  /* bloss zufaellig nicht aufgetreten.                                       */
  /***************************************************************************/
  printf("\nAusfall ueber die 49,7-Tage-Naht hinaus:\n");
  {
    const uint32_t stunde = 3600u * 1000u;
    verbindung_init(&w, 0);
    (void)verbindung_nachfuehren(&w, true, 0, nullptr);

    // in Stundenschritten bis knapp vor die Naht, dann in Minutenschritten
    // ein Stueck darueber hinaus
    uint32_t jetzt = laufen_lassen(&w, false, 0, 1193u * stunde, stunde);
    jetzt = laufen_lassen(&w, false, jetzt, 4u * 60u * 1000u, 60u * 1000u);

    uint32_t naiv_ms = (uint32_t)(jetzt - w.broker.seit);
    pruefe(naiv_ms < VERBINDUNG_KARENZ_MS,
           "der Zeitpunkt ist getroffen: naive Differenz liegt unter der Karenz");
    printf("       (naive Differenz: %u ms, Karenz: %u ms)\n", naiv_ms, VERBINDUNG_KARENZ_MS);

    pruefe(verbindung_lage(&w) == VERBINDUNG_GESTOERT,
           "die Stoermeldung steht trotzdem - sie verschwindet NICHT an der Naht");
    pruefe(w.broker.ueber_deckel, "und die Dauer ist gedeckelt statt zurueckgesprungen");
    verbindung_dauer_text(text, sizeof(text), verbindung_ausfall_sekunden(&w), w.broker.ueber_deckel);
    pruefe_text(text, "mehr als 30 Tagen", "Text nach 49,7 Tagen Ausfall");
  }

  /***************************************************************************/
  /* 7. Die Dauer in Worten                                                  */
  /*                                                                          */
  /* Der Satz auf der Seite lautet "seit ... nicht erreichbar", also Dativ.   */
  /* Die groebere Einheit uebernimmt erst, wenn sie zwei ihrer Einheiten voll */
  /* hat - "seit 90 Minuten" ist genauer als "seit 1 Stunde".                 */
  /***************************************************************************/
  printf("\nDie Dauer in Worten:\n");
  verbindung_dauer_text(text, sizeof(text), 0, false);
  pruefe_text(text, "0 Minuten", "null Sekunden");
  verbindung_dauer_text(text, sizeof(text), 60, false);
  pruefe_text(text, "1 Minute", "eine Minute (Singular)");
  verbindung_dauer_text(text, sizeof(text), 119, false);
  pruefe_text(text, "1 Minute", "abgerundet wird, nicht gerundet");
  verbindung_dauer_text(text, sizeof(text), 14u * 60u, false);
  pruefe_text(text, "14 Minuten", "der Fall aus dem Vorhaben");
  verbindung_dauer_text(text, sizeof(text), 5399, false);
  pruefe_text(text, "89 Minuten", "letzte Sekunde in Minuten");
  verbindung_dauer_text(text, sizeof(text), 5400, false);
  pruefe_text(text, "1 Stunde", "erste Stunde (Singular)");
  verbindung_dauer_text(text, sizeof(text), 7200, false);
  pruefe_text(text, "2 Stunden", "zwei Stunden");
  verbindung_dauer_text(text, sizeof(text), 172799, false);
  pruefe_text(text, "47 Stunden", "letzte Sekunde in Stunden");
  verbindung_dauer_text(text, sizeof(text), 172800, false);
  pruefe_text(text, "2 Tagen", "erster Tageswert");
  verbindung_dauer_text(text, sizeof(text), 86400, false);
  pruefe_text(text, "24 Stunden", "ein voller Tag zaehlt noch in Stunden");
  verbindung_dauer_text(text, sizeof(text), 0, true);
  pruefe_text(text, "mehr als 30 Tagen", "Deckel schlaegt jede Sekundenzahl");

  // Die Singularform "1 Tag" steht im Header, kann mit den heutigen Schwellen
  // aber nicht entstehen - die Tagesangabe beginnt bei zwei. Sie bleibt als
  // Absicherung fuer den Fall drin, dass jemand die Stundenschwelle senkt.
  // Genau das haelt diese Zusicherung fest, damit der Widerspruch zwischen
  // Code und Wirklichkeit nicht als Fehler gelesen wird.
  {
    bool tagesangabe_beginnt_bei_zwei = true;
    for (uint32_t s = 172800u; s < 172800u + 86400u; s += 3600u)
    {
      verbindung_dauer_text(text, sizeof(text), s, false);
      if (strncmp(text, "2 Tagen", 7) != 0)
        tagesangabe_beginnt_bei_zwei = false;
    }
    pruefe(tagesangabe_beginnt_bei_zwei,
           "die kleinste Tagesangabe ist 2 - '1 Tag' entsteht nie");
  }

  /***************************************************************************/
  /* 7a. Der Herzschlag: Broker da, aber die Steuerung rechnet nicht mehr    */
  /*                                                                          */
  /* Der Re-Assert kommt alle 300,0 s (am 2026-08-21 an H2 gemessen). Die     */
  /* Karenz von zwoelf Minuten deckt zwei verpasste Takte samt Reserve ab.    */
  /* Auch hier wird die Grenze selbst geprueft, nicht ein Punkt dahinter.     */
  /***************************************************************************/
  printf("\nHerzschlag der Steuerung (Broker erreichbar):\n");
  {
    verbindung_init(&w, 0);
    uint32_t jetzt = 1000;
    (void)verbindung_nachfuehren(&w, true, jetzt, nullptr); // Verbindung steht
    pruefe(w.stumm.laeuft, "die Stumm-Uhr startet mit dem Verbindungsaufbau");
    pruefe(verbindung_lage(&w) == VERBINDUNG_VERBUNDEN, "und meldet zunaechst nichts");

    jetzt = laufen_lassen(&w, true, jetzt, VERBINDUNG_STUMM_KARENZ_MS - 1000u, 1000u);
    pruefe(verbindung_lage(&w) == VERBINDUNG_VERBUNDEN,
           "eine Sekunde vor der Grenze: noch keine Meldung");

    jetzt = laufen_lassen(&w, true, jetzt, 1000u, 1000u);
    pruefe(verbindung_lage(&w) == VERBINDUNG_STEUERUNG_STUMM,
           "auf der Grenze: die Steuerung gilt als stumm");
    pruefe_zahl(verbindung_ausfall_sekunden(&w), 12u * 60u,
                "gemeldete Dauer ist die der Stille, nicht die des Brokers");
    verbindung_dauer_text(text, sizeof(text), verbindung_ausfall_sekunden(&w),
                          verbindung_ueber_deckel(&w));
    pruefe_text(text, "12 Minuten", "Text der stummen Steuerung");

    // Ein Kommando setzt die Uhr zurueck - der naechste Takt wird binnen 300 s
    // erwartet, die Meldung muss sofort verschwinden.
    uint32_t stille = verbindung_set_empfangen(&w, jetzt);
    pruefe_zahl(stille, 12u * 60u, "die beendete Stille wird zum Loggen gemeldet");
    pruefe(verbindung_lage(&w) == VERBINDUNG_VERBUNDEN,
           "nach einem Kommando ist die Meldung sofort weg");
    pruefe_zahl(verbindung_ausfall_sekunden(&w), 0, "und die Dauer ist zurueckgesetzt");

    // Ein Kommando im Normalbetrieb meldet nichts - sonst stuende nach jedem
    // Re-Assert eine Zeile im Log, die keine Stoerung beschreibt.
    jetzt = laufen_lassen(&w, true, jetzt, 300u * 1000u, 1000u); // ein Takt
    pruefe_zahl(verbindung_set_empfangen(&w, jetzt), 0,
                "ein Kommando im 5-min-Takt meldet nichts");
    pruefe(verbindung_lage(&w) == VERBINDUNG_VERBUNDEN, "und die Lage bleibt ruhig");
  }

  /***************************************************************************/
  /* 7b. Der Vorrang: ohne Broker ist Stille keine Aussage                   */
  /*                                                                          */
  /* Die wichtigste Regel des Zusammenspiels. Ohne Broker KANN kein Kommando  */
  /* kommen - liefe die Stumm-Uhr weiter, meldete die Seite nach der Rueckkehr */
  /* des Brokers sofort einen zweiten Fehler, den es nie gab, und schickte    */
  /* jemanden zum Server, um dort nach dem falschen Fehler zu suchen.         */
  /***************************************************************************/
  printf("\nVorrang zwischen Broker-Ausfall und stummer Steuerung:\n");
  {
    verbindung_init(&w, 0);
    uint32_t jetzt = 1000;
    (void)verbindung_nachfuehren(&w, true, jetzt, nullptr);

    // erst stumm werden lassen, dann faellt auch der Broker aus
    jetzt = laufen_lassen(&w, true, jetzt, VERBINDUNG_STUMM_KARENZ_MS, 1000u);
    pruefe(verbindung_lage(&w) == VERBINDUNG_STEUERUNG_STUMM, "Ausgangslage: stumm");

    jetzt = laufen_lassen(&w, false, jetzt, VERBINDUNG_KARENZ_MS, 1000u);
    pruefe(verbindung_lage(&w) == VERBINDUNG_GESTOERT,
           "faellt der Broker aus, gilt der Broker-Ausfall");
    pruefe(!w.stumm.laeuft, "die Stumm-Uhr steht still, solange der Broker weg ist");
    pruefe_zahl(verbindung_ausfall_sekunden(&w), 5u * 60u,
                "gemeldet wird die Dauer des BROKER-Ausfalls");

    // laenger ohne Broker als die Stumm-Karenz: trotzdem nur eine Meldung
    jetzt = laufen_lassen(&w, false, jetzt, 20u * 60u * 1000u, 1000u);
    pruefe(verbindung_lage(&w) == VERBINDUNG_GESTOERT,
           "auch nach 25 min ohne Broker bleibt es der Broker-Ausfall");

    // Rueckkehr: die Stumm-Uhr faengt bei null an, nicht bei 25 Minuten
    uint32_t gemeldet = 0;
    pruefe(verbindung_nachfuehren(&w, true, jetzt, &gemeldet),
           "die Rueckkehr des Brokers wird gemeldet");
    pruefe(verbindung_lage(&w) == VERBINDUNG_VERBUNDEN,
           "unmittelbar danach ist die Lage ruhig - keine zweite Stoermeldung");
    pruefe_zahl(verbindung_ausfall_sekunden(&w), 0, "die Stumm-Uhr faengt bei null an");

    // und laeuft ab jetzt neu: erst nach der vollen Karenz wieder eine Meldung
    jetzt = laufen_lassen(&w, true, jetzt, VERBINDUNG_STUMM_KARENZ_MS - 1000u, 1000u);
    pruefe(verbindung_lage(&w) == VERBINDUNG_VERBUNDEN,
           "kurz vor der Stumm-Karenz nach der Rueckkehr: immer noch ruhig");
    jetzt = laufen_lassen(&w, true, jetzt, 1000u, 1000u);
    pruefe(verbindung_lage(&w) == VERBINDUNG_STEUERUNG_STUMM,
           "kommt dann wirklich nichts, meldet sie sich doch noch");
  }

  /***************************************************************************/
  /* 7b2. Die Reihenfolge in verbindung_lage() als zweite Sicherung          */
  /*                                                                          */
  /* Die eigentliche Vorrangregel steckt oben im Zuruecksetzen der Stumm-Uhr  */
  /* beim Verbindungsverlust - dieser Zustand entsteht im Betrieb also gar    */
  /* nicht. Genau das hat eine Gegenprobe gezeigt: Dreht man die Reihenfolge  */
  /* in verbindung_lage() um, faellt KEINE Zusicherung um, weil beide Uhren   */
  /* nie gleichzeitig laufen.                                                 */
  /*                                                                          */
  /* Die Reihenfolge bleibt trotzdem stehen - als zweite Sicherung fuer den   */
  /* Fall, dass jemand das Zuruecksetzen spaeter entfernt. Damit sie eine     */
  /* geprueft ist und keine geglaubte, baut dieser Abschnitt den Zustand von  */
  /* Hand: beide Uhren laufen, beide ueber ihrer Karenz.                      */
  /***************************************************************************/
  printf("\nReihenfolge in verbindung_lage (zweite Sicherung):\n");
  {
    verbindung_init(&w, 0);
    w.je_verbunden = true;

    // Broker-Ausfall: laeuft, ueber der Karenz
    ausfall_zuruecksetzen(&w.broker);
    ausfall_beginnen(&w.broker, 0);
    ausfall_fortschreiben(&w.broker, VERBINDUNG_KARENZ_MS, VERBINDUNG_KARENZ_MS);

    // Stumm-Uhr: ebenfalls laufend und ueber IHRER Karenz - im Betrieb
    // unmoeglich, hier absichtlich hergestellt
    ausfall_zuruecksetzen(&w.stumm);
    ausfall_beginnen(&w.stumm, 0);
    ausfall_fortschreiben(&w.stumm, VERBINDUNG_STUMM_KARENZ_MS, VERBINDUNG_STUMM_KARENZ_MS);

    pruefe(w.broker.karenz_ueber && w.stumm.karenz_ueber,
           "Aufbau: beide Uhren laufen und sind ueber ihrer Karenz");
    pruefe(verbindung_lage(&w) == VERBINDUNG_GESTOERT,
           "der Broker-Ausfall gewinnt - er ist die Ursache, nicht die Folge");
    pruefe_zahl(verbindung_ausfall_sekunden(&w), 5u * 60u,
                "und die gemeldete Dauer ist die des Brokers");
  }

  /***************************************************************************/
  /* 7c. Die Stumm-Uhr am millis()-Ueberlauf                                 */
  /*                                                                          */
  /* Dieselbe Falle wie beim Broker-Ausfall - und weil beide Uhren denselben  */
  /* Kern benutzen (struct Ausfall), belegt dieser Abschnitt zugleich, dass   */
  /* der Kern wirklich geteilt ist und nicht zweimal dasteht.                 */
  /***************************************************************************/
  printf("\nStumm-Uhr ueber die 49,7-Tage-Naht:\n");
  {
    const uint32_t stunde = 3600u * 1000u;
    verbindung_init(&w, 0);
    (void)verbindung_nachfuehren(&w, true, 0, nullptr);

    uint32_t jetzt = laufen_lassen(&w, true, 0, 1193u * stunde, stunde);
    jetzt = laufen_lassen(&w, true, jetzt, 4u * 60u * 1000u, 60u * 1000u);

    uint32_t naiv_ms = (uint32_t)(jetzt - w.stumm.seit);
    pruefe(naiv_ms < VERBINDUNG_STUMM_KARENZ_MS,
           "der Zeitpunkt ist getroffen: naive Differenz liegt unter der Karenz");
    pruefe(verbindung_lage(&w) == VERBINDUNG_STEUERUNG_STUMM,
           "die Meldung steht trotzdem - sie verschwindet NICHT an der Naht");
    pruefe(verbindung_ueber_deckel(&w), "und die Dauer ist gedeckelt");
    verbindung_dauer_text(text, sizeof(text), verbindung_ausfall_sekunden(&w),
                          verbindung_ueber_deckel(&w));
    pruefe_text(text, "mehr als 30 Tagen", "Text nach 49,7 Tagen Stille");
  }

  /***************************************************************************/
  /* 8. Robustheit gegen Nullzeiger und Nullpuffer                           */
  /*                                                                          */
  /* Die Weboberflaeche ruft das aus einem HTTP-Handler auf. Ein Absturz dort */
  /* nimmt die Firmware mit, und mit ihr den Notbetriebsknopf - also genau    */
  /* das, was in der Lage noch funktionieren muss.                            */
  /***************************************************************************/
  printf("\nRobustheit:\n");
  verbindung_init(nullptr, 0); // darf nicht abstuerzen
  pruefe(!verbindung_nachfuehren(nullptr, false, 1000, nullptr),
         "Nachfuehren mit Nullzeiger liefert false statt abzustuerzen");
  pruefe(verbindung_lage(nullptr) == VERBINDUNG_VERBUNDEN,
         "Lage mit Nullzeiger meldet 'verbunden' statt einen Fehlalarm");
  pruefe_zahl(verbindung_ausfall_sekunden(nullptr), 0, "Dauer mit Nullzeiger");
  pruefe_zahl(verbindung_set_empfangen(nullptr, 1000), 0, "Herzschlag mit Nullzeiger");
  pruefe(!verbindung_ueber_deckel(nullptr), "Deckelabfrage mit Nullzeiger");
  verbindung_dauer_text(nullptr, 10, 60, false); // darf nicht abstuerzen
  verbindung_dauer_text(text, 0, 60, false);     // darf nicht schreiben
  pruefe(true, "Textfunktion vertraegt Nullpuffer und Laenge 0");

  // Der Ausgabepuffer der Seite ist knapp bemessen: der laengste Text muss
  // hineinpassen, sonst wuerde er stillschweigend abgeschnitten.
  verbindung_dauer_text(text, sizeof(text), 0, true);
  pruefe(strlen(text) < 24, "laengster Text bleibt unter 24 Zeichen");

  /***************************************************************************/
  /* 9. Zusammenspiel der Konstanten                                         */
  /***************************************************************************/
  printf("\nKonstanten:\n");
  pruefe_zahl(VERBINDUNG_KARENZ_MS, 300000u, "Karenz betraegt 5 Minuten");
  pruefe(VERBINDUNG_KARENZ_MS > 60000u,
         "Karenz liegt ueber dem groessten Reconnect-Abstand (MQTT_RECONNECT_MAX 60 s)");
  pruefe_zahl(VERBINDUNG_STUMM_KARENZ_MS, 720000u, "Stumm-Karenz betraegt 12 Minuten");
  // Der Re-Assert-Takt ist gemessene 300,0 s. Weniger als zwei volle Takte
  // Karenz hiesse, dass ein einzelner verpasster Takt schon Alarm ausloest.
  pruefe(VERBINDUNG_STUMM_KARENZ_MS > 2u * 300u * 1000u,
         "Stumm-Karenz deckt mehr als zwei Re-Assert-Takte ab (2 x 300 s)");
  pruefe(VERBINDUNG_STUMM_KARENZ_MS > VERBINDUNG_KARENZ_MS,
         "Stumm-Karenz ist groesser als die Broker-Karenz (unsicherere Aussage)");
  pruefe_zahl(VERBINDUNG_DECKEL_MS, VERBINDUNG_DECKEL_TAGE * 24u * 3600u * 1000u,
              "Deckel in ms passt zur Tagesangabe im Text");

  // Zwischen dem Deckel und der millis()-Naht muss ein Fenster bleiben, in dem
  // die Ueberschreitung ueberhaupt bemerkt werden kann. loop() laeuft im
  // Millisekundentakt, ein Fenster von Tagen ist damit ueppig - aber es muss
  // groesser als null sein, und genau das ist hier festgehalten.
  {
    uint32_t fenster_ms = 0xFFFFFFFFu - VERBINDUNG_DECKEL_MS;
    pruefe(VERBINDUNG_DECKEL_MS < 0xFFFFFFFFu, "Deckel passt in uint32_t");
    pruefe(fenster_ms > 7u * 24u * 3600u * 1000u,
           "zwischen Deckel und millis()-Naht liegen mehr als 7 Tage");
    printf("       (Fenster bis zur Naht: %u Tage)\n", fenster_ms / (24u * 3600u * 1000u));
  }

  printf("\n%s (%d Abweichungen)\n", (fehler == 0) ? "BESTANDEN" : "FEHLGESCHLAGEN", fehler);
  return (fehler == 0) ? 0 : 1;
}
