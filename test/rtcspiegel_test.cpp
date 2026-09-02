// Nachweis fuer die Regeln des RTC-Spiegels (src/rtcspiegel.h), 3.20.0.
//
// Geprueft wird der Code, der auch auf dem Geraet laeuft: die Datei wird hier
// direkt eingebunden, es gibt keine Nachbildung, die auseinanderlaufen kann.
// Dasselbe Muster wie test/sendwindow_test.cpp und test/verbindung_test.cpp.
//
// WARUM DIESER TEST IN DER CI STEHT: Der Spiegel liegt in einem Speicher, den
// die Firmware NIE initialisiert (RTC_NOINIT_ATTR). Was dort nach einem Reset
// steht, entscheidet allein die Pruefung in diesem Header. Faellt sie zu
// grosszuegig aus, uebernimmt der Notbetrieb Bitmuster als Kurvenwerte - und
// zwar genau in dem Lauf, in dem der Knopf gebraucht wird. Faellt sie zu streng
// aus, sind die Werte nach jedem Neustart weg und es faellt nie auf, weil der
// Broker sie binnen Sekunden nachliefert, SOLANGE ER DA IST. Beide Fehler sind
// am Geraet nicht zu sehen; hier sind sie es.
//
// Sieben Fragen beantwortet dieser Test:
//  1. Gilt ein Spiegel aus Zufallsmuell als ungueltig - und steht der
//     Bootzaehler danach auf 1?
//  2. Ueberlebt ein gesiegelter Stand den naechsten Boot, mit Werten und mit
//     hochgezaehltem Bootzaehler?
//  3. Faellt ein Bitkipper in JEDEM Feld auf (Werte, Maske, Bootzaehler)?
//  4. Faellt ein Spiegel der ANDEREN Stufe auf? Die Backup-Boards tragen
//     abwechselnd beide Rollen.
//  5. Faellt eine Maske mit Bits jenseits der Wertezahl auf, auch wenn die
//     Pruefsumme stimmt? Sonst haelt notbetrieb_vollstaendig() einen
//     unvollstaendigen Satz fuer vollstaendig.
//  6. Faellt ein Spiegel der VORVERSION auf (Layoutnummer im Magic)? Nach
//     einem OTA liegt genau das im RTC-Speicher.
//  7. Saettigt der Bootzaehler statt ueberzulaufen - und haengt die
//     Pruefsumme wirklich nicht vom Padding ab?
//
// Bauen und ausfuehren:
//   c++ -std=c++17 -O2 -Wall -o /tmp/rtcspiegel_test test/rtcspiegel_test.cpp
//   /tmp/rtcspiegel_test         (Rueckgabewert != 0 = Test fehlgeschlagen)

#include <cstdio>
#include <cstdint>
#include <cstring>

#include "../src/rtcspiegel.h"

static int fehler = 0;

// eine Zusicherung mit Klartext, damit der CI-Log ohne Debugger lesbar ist
static void pruefe(bool bedingung, const char *was)
{
  printf("  [%s] %s\n", bedingung ? "ok " : "FEHLER", was);
  if (!bedingung)
    fehler++;
}

static void pruefe_zahl(int ist, int soll, const char *was)
{
  bool ok = (ist == soll);
  printf("  [%s] %-58s (erwartet %d, ist %d)\n", ok ? "ok " : "FEHLER", was, soll, ist);
  if (!ok)
    fehler++;
}

/*****************************************************************************/
/* Ein RTC-Speicher, wie ihn ein Kaltstart hinterlaesst                      */
/*                                                                           */
/* Nicht mit Nullen fuellen: Ein Spiegel voller Nullen haette Magic 0 und    */
/* fiele schon an der ersten Bedingung durch - der Test wuerde dann etwas    */
/* anderes pruefen als gemeint. Ein wiederholtes Bitmuster ist naeher an     */
/* dem, was auf dem Geraet wirklich dort steht.                              */
/*****************************************************************************/
static void mit_muell_fuellen(RtcSpiegel *sp)
{
  memset(sp, 0xA5, sizeof(*sp));
}

/*****************************************************************************/
/* Einen vollstaendigen Heizen-Satz in den Spiegel legen                     */
/*                                                                           */
/* Die Zahlen sind der Sollstand von H1 (VL kalt 34, VL warm 26, AT kalt -10,*/
/* AT warm 15) - dieselben Werte wie in test/notbetrieb_test.cpp, damit ein  */
/* Blick in beide Tests dieselbe Kurve zeigt.                                */
/*****************************************************************************/
static void heizen_satz(NotbetriebSpeicher *sp)
{
  notbetrieb_speicher_leeren(sp);
  sp->werte[0] = 34;
  sp->werte[1] = 26;
  sp->werte[2] = -10;
  sp->werte[3] = 15;
  sp->gesetzt = 0x0F;
}

/*****************************************************************************/
/* 1. Kaltstart: Muell im Speicher                                           */
/*****************************************************************************/
static void test_kaltstart()
{
  printf("\nKaltstart - was ein Stromausfall hinterlaesst\n");

  RtcSpiegel sp;
  mit_muell_fuellen(&sp);

  const bool gueltig = rtc_spiegel_boot(&sp, NOTBETRIEB_HEIZEN);
  pruefe(!gueltig, "Zufallsmuell gilt NICHT als gueltiger Spiegel");
  pruefe_zahl((int)sp.bootzaehler, 1, "Bootzaehler steht nach dem Kaltstart auf 1");
  pruefe_zahl((int)sp.gesetzt, 0, "keine Werte gelten als gesetzt");
  pruefe(rtc_gueltig(&sp, NOTBETRIEB_HEIZEN),
         "der geleerte Spiegel ist danach selbst gueltig gesiegelt");

  // Der zweite Boot muss den geleerten Stand wiedererkennen - sonst zaehlte
  // der Bootzaehler nie ueber 1 hinaus und M3 waere wertlos.
  const bool zweiter = rtc_spiegel_boot(&sp, NOTBETRIEB_HEIZEN);
  pruefe(zweiter, "der zweite Boot findet einen gueltigen Spiegel vor");
  pruefe_zahl((int)sp.bootzaehler, 2, "Bootzaehler zaehlt den Software-Reset mit");
}

/*****************************************************************************/
/* 2. Werte ueberleben den Neustart                                          */
/*****************************************************************************/
static void test_werte_ueberleben()
{
  printf("\nDie gehaltenen Werte ueber einen Software-Reset\n");

  RtcSpiegel sp;
  mit_muell_fuellen(&sp);
  (void)rtc_spiegel_boot(&sp, NOTBETRIEB_HEIZEN);

  NotbetriebSpeicher werte;
  heizen_satz(&werte);
  rtc_werte_spiegeln(&sp, &werte, NOTBETRIEB_HEIZEN);

  pruefe_zahl((int)sp.bootzaehler, 1,
              "das Spiegeln der Werte laesst den Bootzaehler in Ruhe");

  // jetzt der Neustart: derselbe Speicherinhalt, neuer Boot
  const bool gueltig = rtc_spiegel_boot(&sp, NOTBETRIEB_HEIZEN);
  pruefe(gueltig, "der Spiegel ueberlebt den Neustart");
  pruefe_zahl((int)sp.gesetzt, 0x0F, "alle vier Werte gelten weiter als gesetzt");
  pruefe_zahl((int)sp.werte[0], 34, "VL kalt steht noch da");
  pruefe_zahl((int)sp.werte[1], 26, "VL warm steht noch da");
  pruefe_zahl((int)sp.werte[2], -10, "AT kalt steht noch da (negativ)");
  pruefe_zahl((int)sp.werte[3], 15, "AT warm steht noch da");
  pruefe_zahl((int)sp.bootzaehler, 2, "und der Bootzaehler ist eins weiter");
}

/*****************************************************************************/
/* 3. Bitkipper in jedem Feld                                                */
/*                                                                           */
/* Der RTC-Speicher ist nicht fehlerkorrigiert. Ein gekipptes Bit in einem   */
/* Kurvenwert waere der schlimmste Fall dieser ganzen Maschinerie: Der       */
/* Notbetrieb faehrt dann eine Kurve, die niemand gesetzt hat.               */
/*****************************************************************************/
static void test_bitkipper()
{
  printf("\nBitkipper im RTC-Speicher\n");

  NotbetriebSpeicher werte;
  heizen_satz(&werte);

  {
    RtcSpiegel sp;
    mit_muell_fuellen(&sp);
    (void)rtc_spiegel_boot(&sp, NOTBETRIEB_HEIZEN);
    rtc_werte_spiegeln(&sp, &werte, NOTBETRIEB_HEIZEN);
    sp.werte[2] ^= 0x00000010; // AT kalt: -10 wird zu -26
    pruefe(!rtc_gueltig(&sp, NOTBETRIEB_HEIZEN),
           "gekipptes Bit in einem Kurvenwert macht den Spiegel ungueltig");
  }

  {
    RtcSpiegel sp;
    mit_muell_fuellen(&sp);
    (void)rtc_spiegel_boot(&sp, NOTBETRIEB_HEIZEN);
    rtc_werte_spiegeln(&sp, &werte, NOTBETRIEB_HEIZEN);
    sp.gesetzt ^= 0x01; // ein Wert gilt ploetzlich als nicht gesetzt
    pruefe(!rtc_gueltig(&sp, NOTBETRIEB_HEIZEN),
           "gekipptes Bit in der Maske macht den Spiegel ungueltig");
  }

  {
    RtcSpiegel sp;
    mit_muell_fuellen(&sp);
    (void)rtc_spiegel_boot(&sp, NOTBETRIEB_HEIZEN);
    sp.bootzaehler ^= 0x0080;
    pruefe(!rtc_gueltig(&sp, NOTBETRIEB_HEIZEN),
           "gekipptes Bit im Bootzaehler macht den Spiegel ungueltig");
  }

  {
    RtcSpiegel sp;
    mit_muell_fuellen(&sp);
    (void)rtc_spiegel_boot(&sp, NOTBETRIEB_HEIZEN);
    sp.pruefsumme ^= 0x00000001;
    pruefe(!rtc_gueltig(&sp, NOTBETRIEB_HEIZEN),
           "gekipptes Bit in der Pruefsumme selbst macht den Spiegel ungueltig");
  }

  // und der Weg zurueck: ein gekippter Spiegel kostet die Werte, nicht mehr
  RtcSpiegel sp;
  mit_muell_fuellen(&sp);
  (void)rtc_spiegel_boot(&sp, NOTBETRIEB_HEIZEN);
  rtc_werte_spiegeln(&sp, &werte, NOTBETRIEB_HEIZEN);
  sp.werte[0] ^= 0x00000001;
  const bool gueltig = rtc_spiegel_boot(&sp, NOTBETRIEB_HEIZEN);
  pruefe(!gueltig, "der Boot erkennt den Kipper");
  pruefe_zahl((int)sp.gesetzt, 0, "und wirft ALLE Werte weg, nicht nur den einen");
  pruefe_zahl((int)sp.bootzaehler, 1, "der Bootzaehler faengt wieder bei 1 an");
}

/*****************************************************************************/
/* 4. Der Spiegel der anderen Stufe                                          */
/*****************************************************************************/
static void test_rolle()
{
  printf("\nEin Spiegel der anderen Stufe\n");

  RtcSpiegel sp;
  mit_muell_fuellen(&sp);
  (void)rtc_spiegel_boot(&sp, NOTBETRIEB_WASSER);

  NotbetriebSpeicher wasser;
  notbetrieb_speicher_leeren(&wasser);
  wasser.werte[0] = 48;
  wasser.gesetzt = 0x01;
  rtc_werte_spiegeln(&sp, &wasser, NOTBETRIEB_WASSER);

  pruefe(rtc_gueltig(&sp, NOTBETRIEB_WASSER), "als Warmwasser gelesen: gueltig");
  pruefe(!rtc_gueltig(&sp, NOTBETRIEB_HEIZEN),
         "dieselben Bytes als Heizen gelesen: ungueltig");

  // Der Boot in der falschen Rolle raeumt auf und stellt die Rolle richtig
  const bool gueltig = rtc_spiegel_boot(&sp, NOTBETRIEB_HEIZEN);
  pruefe(!gueltig, "der Boot in der Heizen-Firmware verwirft den Wasser-Stand");
  pruefe_zahl((int)sp.rolle, (int)NOTBETRIEB_HEIZEN, "und traegt die eigene Rolle ein");
  pruefe_zahl((int)sp.werte[0], 0, "der fremde Wert ist weg");
}

/*****************************************************************************/
/* 5. Maskenbits jenseits der Wertezahl                                      */
/*                                                                           */
/* Der gefaehrliche Fall: Die Pruefsumme stimmt, weil die Maske mitgerechnet */
/* wurde. Ohne die Bereichspruefung haelt notbetrieb_vollstaendig() den Satz */
/* fuer vollstaendig, und der Knopf gibt sich frei, obwohl ein Wert fehlt.   */
/*****************************************************************************/
static void test_maske_zu_breit()
{
  printf("\nMaskenbits, zu denen es keinen Wert gibt\n");

  RtcSpiegel sp;
  mit_muell_fuellen(&sp);
  (void)rtc_spiegel_boot(&sp, NOTBETRIEB_WASSER);
  sp.gesetzt = 0x03; // Warmwasser kennt nur Bit 0
  rtc_siegeln(&sp);  // Pruefsumme passt jetzt zur falschen Maske

  pruefe(sp.pruefsumme == rtc_pruefsumme(&sp), "die Pruefsumme stimmt (Vorbedingung)");
  pruefe(!rtc_gueltig(&sp, NOTBETRIEB_WASSER),
         "trotzdem ungueltig - Bit 1 gehoert zu keinem Wasser-Wert");

  // Heizen hat vier Werte, Bit 4 gehoert auch dort zu keinem
  RtcSpiegel h;
  mit_muell_fuellen(&h);
  (void)rtc_spiegel_boot(&h, NOTBETRIEB_HEIZEN);
  h.gesetzt = 0x1F;
  rtc_siegeln(&h);
  pruefe(!rtc_gueltig(&h, NOTBETRIEB_HEIZEN), "dasselbe fuer Heizen mit Bit 4");

  // Gegenprobe: die vollen Masken beider Rollen sind erlaubt
  h.gesetzt = 0x0F;
  rtc_siegeln(&h);
  pruefe(rtc_gueltig(&h, NOTBETRIEB_HEIZEN), "die volle Heizen-Maske 0x0F ist erlaubt");
}

/*****************************************************************************/
/* 6. Der Spiegel der Vorversion nach einem OTA                              */
/*****************************************************************************/
static void test_layoutnummer()
{
  printf("\nEin Spiegel der Vorversion (Layoutnummer im Magic)\n");

  RtcSpiegel sp;
  mit_muell_fuellen(&sp);
  (void)rtc_spiegel_boot(&sp, NOTBETRIEB_HEIZEN);

  // so saehe der Speicher aus, den eine Firmware mit anderem Layout
  // hinterlassen hat: gleiche Kennung, andere Nummer, stimmige Pruefsumme
  sp.magic = RTC_SPIEGEL_MAGIC - 1u;
  sp.pruefsumme = rtc_pruefsumme(&sp);

  pruefe(sp.pruefsumme == rtc_pruefsumme(&sp), "die Pruefsumme stimmt (Vorbedingung)");
  pruefe(!rtc_gueltig(&sp, NOTBETRIEB_HEIZEN),
         "eine andere Layoutnummer macht den Spiegel ungueltig");
}

/*****************************************************************************/
/* 7. Saettigung und Padding                                                 */
/*****************************************************************************/
static void test_saettigung_und_padding()
{
  printf("\nBootzaehler-Saettigung und Padding-Unabhaengigkeit\n");

  RtcSpiegel sp;
  mit_muell_fuellen(&sp);
  (void)rtc_spiegel_boot(&sp, NOTBETRIEB_HEIZEN);

  sp.bootzaehler = UINT16_MAX - 1u;
  rtc_siegeln(&sp);
  (void)rtc_spiegel_boot(&sp, NOTBETRIEB_HEIZEN);
  pruefe_zahl((int)sp.bootzaehler, (int)UINT16_MAX, "der vorletzte Schritt zaehlt normal");
  (void)rtc_spiegel_boot(&sp, NOTBETRIEB_HEIZEN);
  pruefe_zahl((int)sp.bootzaehler, (int)UINT16_MAX,
              "danach bleibt er stehen statt auf 0 zu springen");
  pruefe(rtc_gueltig(&sp, NOTBETRIEB_HEIZEN), "und der Spiegel bleibt dabei gueltig");

  // Padding: zwei Strukturen mit VERSCHIEDENEM Vorleben, danach mit
  // identischen Feldern belegt, muessen dieselbe Pruefsumme haben. Rechnete
  // rtc_pruefsumme() ueber den rohen Speicher, unterschieden sie sich hier.
  RtcSpiegel a, b;
  memset(&a, 0x00, sizeof(a));
  memset(&b, 0xFF, sizeof(b));

  NotbetriebSpeicher werte;
  heizen_satz(&werte);
  a.bootzaehler = 7;
  b.bootzaehler = 7;
  rtc_werte_spiegeln(&a, &werte, NOTBETRIEB_HEIZEN);
  rtc_werte_spiegeln(&b, &werte, NOTBETRIEB_HEIZEN);

  pruefe(a.pruefsumme == b.pruefsumme,
         "gleiche Felder ergeben gleiche Pruefsumme, egal was vorher dastand");
  pruefe(rtc_gueltig(&b, NOTBETRIEB_HEIZEN),
         "der aus 0xFF aufgebaute Spiegel ist gueltig");
}

/*****************************************************************************/
/* Robustheit gegen den Nullzeiger - die Firmware ruft mit &rtcSpiegel auf,  */
/* aber jede Regel dieses Projekts haelt einen Nullzeiger aus, statt sich    */
/* darauf zu verlassen.                                                      */
/*****************************************************************************/
static void test_nullzeiger()
{
  printf("\nNullzeiger\n");
  pruefe(!rtc_gueltig(0, NOTBETRIEB_HEIZEN), "rtc_gueltig(0) meldet ungueltig");
  pruefe(!rtc_spiegel_boot(0, NOTBETRIEB_HEIZEN), "rtc_spiegel_boot(0) meldet ungueltig");
  pruefe(rtc_pruefsumme(0) == 0, "rtc_pruefsumme(0) ist 0");
  rtc_siegeln(0);
  rtc_leeren(0, NOTBETRIEB_HEIZEN);
  rtc_werte_spiegeln(0, 0, NOTBETRIEB_HEIZEN);
  pruefe(true, "die uebrigen Aufrufe mit 0 stuerzen nicht ab");
}

int main()
{
  printf("Hosttest RTC-Spiegel (src/rtcspiegel.h)\n");
  test_kaltstart();
  test_werte_ueberleben();
  test_bitkipper();
  test_rolle();
  test_maske_zu_breit();
  test_layoutnummer();
  test_saettigung_und_padding();
  test_nullzeiger();

  printf("\n%s (%d Fehler)\n", fehler == 0 ? "ALLE PRUEFUNGEN BESTANDEN" : "FEHLGESCHLAGEN", fehler);
  return fehler == 0 ? 0 : 1;
}
