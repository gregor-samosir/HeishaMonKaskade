// Nachweis fuer die Regeln des Notbetriebs (src/notbetrieb.h), 3.12.0.
//
// Geprueft wird der Code, der auch auf dem Geraet laeuft: die Datei wird hier
// direkt eingebunden, es gibt keine Nachbildung, die auseinanderlaufen kann.
// Dasselbe Muster wie test/sendwindow_test.cpp fuer src/sendwindow.h.
//
// Fuenf Fragen beantwortet dieser Test:
//  1. Bleibt der Knopf gesperrt, solange ein Wert fehlt? (lieber gar nicht
//     schalten als auf die Panasonic-Werkskurve mit 55 C bei -5 C)
//  2. Wird ein Wert ausserhalb der Grenzen VERWORFEN statt geklemmt? Ein
//     stillschweigend zurechtgebogener Kurvenpunkt faellt im Notfall niemandem
//     auf.
//  3. Greift die Karenzzeit-Ausnahme genau fuer den notbetrieb-Zweig - und
//     NICHT fuer set und nicht fuer Topics, die nur so aehnlich heissen?
//     Wird sie vergessen, funktioniert der Knopf im Labor und nach jedem
//     Neustart nicht mehr; das faellt sonst erst im Ernstfall auf.
//  4. Fuehrt der Zustandsautomat die Schritte einzeln, in der richtigen
//     Reihenfolge, und bricht er bei einem ausbleibenden Ruecklesewert ab
//     statt weiterzumachen?
//  5. Haelt das alles dem millis()-Ueberlauf nach 49,7 Tagen stand? An der
//     Anlage waere das nicht abzuwarten.
//
// Bauen und ausfuehren:
//   c++ -std=c++17 -O2 -Wall -o /tmp/notbetrieb_test test/notbetrieb_test.cpp
//   /tmp/notbetrieb_test         (Rueckgabewert != 0 = Test fehlgeschlagen)

#include <cstdio>
#include <cstdint>
#include <cstring>

#include "../src/notbetrieb.h"

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

static void pruefe_text(const char *ist, const char *soll, const char *was)
{
  bool ok = (ist != 0) && (strcmp(ist, soll) == 0);
  printf("  [%s] %-58s (erwartet %s, ist %s)\n", ok ? "ok " : "FEHLER", was, soll,
         ist ? ist : "(null)");
  if (!ok)
    fehler++;
}

/*****************************************************************************/
/* Die Grenzen stehen in setCommands[] (commands.cpp) und werden der Annahme */
/* uebergeben. Hier stehen sie als ERWARTUNG des Tests - das ist keine       */
/* zweite Tabelle im Sinne einer Kopie, sondern die Sollvorgabe, gegen die   */
/* geprueft wird. Laufen sie auseinander, faellt es in commands.cpp auf,     */
/* wo build_heatpump_command() denselben Wert ablehnen wuerde.               */
/*****************************************************************************/
struct Grenze
{
  const char *name;
  int min, max;
};

static const Grenze GRENZEN[] = {
    {"Z1HeatCurveTargetHighTemp", 20, 55},
    {"Z1HeatCurveTargetLowTemp", 20, 55},
    {"Z1HeatCurveOutsideLowTemp", -15, 15},
    {"Z1HeatCurveOutsideHighTemp", -15, 15},
    {"DHWTemp", 40, 75}};

static bool grenze_fuer(const char *name, int *min, int *max)
{
  for (unsigned i = 0; i < sizeof(GRENZEN) / sizeof(GRENZEN[0]); i++)
  {
    if (strcmp(name, GRENZEN[i].name) == 0)
    {
      *min = GRENZEN[i].min;
      *max = GRENZEN[i].max;
      return true;
    }
  }
  return false;
}

// bequemer Aufruf: schlaegt die Grenzen nach und nimmt an
static bool annehmen(NotbetriebSpeicher *sp, NotbetriebRolle rolle,
                     const char *name, int wert)
{
  int min = 0, max = 0;
  if (!grenze_fuer(name, &min, &max))
    return false;
  return notbetrieb_wert_annehmen(sp, rolle, name, wert, min, max);
}

/*****************************************************************************/
/* 1. Vollstaendigkeit                                                       */
/*****************************************************************************/
static void test_vollstaendigkeit()
{
  printf("\n== Vollstaendigkeit der gehaltenen Werte ==\n");

  NotbetriebSpeicher sp;
  notbetrieb_speicher_leeren(&sp);
  pruefe(!notbetrieb_vollstaendig(&sp, NOTBETRIEB_HEIZEN), "leerer Speicher ist unvollstaendig");

  // die echte Hauskurve: 34 C bei -10 C, 26 C bei +15 C
  pruefe(annehmen(&sp, NOTBETRIEB_HEIZEN, "Z1HeatCurveTargetHighTemp", 34), "TargetHigh 34 angenommen");
  pruefe(annehmen(&sp, NOTBETRIEB_HEIZEN, "Z1HeatCurveTargetLowTemp", 26), "TargetLow 26 angenommen");
  pruefe(annehmen(&sp, NOTBETRIEB_HEIZEN, "Z1HeatCurveOutsideLowTemp", -10), "OutsideLow -10 angenommen");
  pruefe(!notbetrieb_vollstaendig(&sp, NOTBETRIEB_HEIZEN),
         "drei von vier Werten: weiter gesperrt");

  pruefe(annehmen(&sp, NOTBETRIEB_HEIZEN, "Z1HeatCurveOutsideHighTemp", 15), "OutsideHigh 15 angenommen");
  pruefe(notbetrieb_vollstaendig(&sp, NOTBETRIEB_HEIZEN), "vier von vier Werten: frei");

  // Warmwasser braucht nur einen Wert
  NotbetriebSpeicher w;
  notbetrieb_speicher_leeren(&w);
  pruefe(!notbetrieb_vollstaendig(&w, NOTBETRIEB_WASSER), "Wasser leer: gesperrt");
  pruefe(annehmen(&w, NOTBETRIEB_WASSER, "DHWTemp", 48), "DHWTemp 48 angenommen");
  pruefe(notbetrieb_vollstaendig(&w, NOTBETRIEB_WASSER), "Wasser vollstaendig: frei");

  // ein Heizen-Speicher ist mit denselben Bits NICHT als Wasser vollstaendig
  // und umgekehrt - die Rolle entscheidet, wie viele Bits noetig sind
  pruefe(notbetrieb_vollstaendig(&w, NOTBETRIEB_WASSER) &&
             !notbetrieb_vollstaendig(&w, NOTBETRIEB_HEIZEN),
         "ein Bit reicht fuer Wasser, nicht fuer Heizen");
}

/*****************************************************************************/
/* 2. Bereichsgrenzen - verwerfen, nicht klemmen                             */
/*****************************************************************************/
static void test_grenzen()
{
  printf("\n== Bereichsgrenzen ==\n");

  NotbetriebSpeicher sp;
  notbetrieb_speicher_leeren(&sp);

  pruefe(annehmen(&sp, NOTBETRIEB_HEIZEN, "Z1HeatCurveTargetHighTemp", 20), "unterer Rand 20 gilt");
  pruefe(annehmen(&sp, NOTBETRIEB_HEIZEN, "Z1HeatCurveTargetHighTemp", 55), "oberer Rand 55 gilt");
  pruefe(!annehmen(&sp, NOTBETRIEB_HEIZEN, "Z1HeatCurveTargetHighTemp", 19), "19 abgelehnt");
  pruefe(!annehmen(&sp, NOTBETRIEB_HEIZEN, "Z1HeatCurveTargetHighTemp", 56), "56 abgelehnt");

  // der abgelehnte Wert darf den zuletzt gueltigen nicht ueberschreiben
  int wert = 0;
  NotbetriebLauf lauf;
  notbetrieb_lauf_leeren(&lauf);
  lauf.schritt = 2; // Index 2 traegt TargetHigh (seit die Betriebsart vorn steht)
  pruefe(notbetrieb_schritt_wert(&lauf, NOTBETRIEB_HEIZEN, &sp, &wert), "Wert liegt vor");
  pruefe_zahl(wert, 55, "nach abgelehnter 56 steht weiterhin 55");

  // negative Aussentemperatur - der Fall, den eine unsigned-Pruefung kaputt macht
  pruefe(annehmen(&sp, NOTBETRIEB_HEIZEN, "Z1HeatCurveOutsideLowTemp", -15), "OutsideLow -15 gilt");
  pruefe(!annehmen(&sp, NOTBETRIEB_HEIZEN, "Z1HeatCurveOutsideLowTemp", -16), "OutsideLow -16 abgelehnt");

  // Namen der jeweils anderen Rolle gehoeren nicht hierher
  pruefe(!annehmen(&sp, NOTBETRIEB_HEIZEN, "DHWTemp", 48), "DHWTemp an Heizen abgelehnt");
  NotbetriebSpeicher w;
  notbetrieb_speicher_leeren(&w);
  pruefe(!annehmen(&w, NOTBETRIEB_WASSER, "Z1HeatCurveTargetHighTemp", 34),
         "Kurvenwert an Wasser abgelehnt");

  // voellig unbekannter Name
  int min = 0, max = 0;
  pruefe(!grenze_fuer("Quatsch", &min, &max), "unbekannter Name hat keine Grenzen");
  pruefe(!notbetrieb_wert_annehmen(&sp, NOTBETRIEB_HEIZEN, "Quatsch", 30, 0, 100),
         "unbekannter Name abgelehnt, auch mit weiten Grenzen");
}

/*****************************************************************************/
/* 3. Die Karenzzeit-Ausnahme                                                */
/*****************************************************************************/
static void test_karenz_ausnahme()
{
  printf("\n== Karenzzeit-Ausnahme fuer den notbetrieb-Zweig ==\n");

  const char *wurzel = "panasonic_heat_pump/notbetrieb";

  pruefe(notbetrieb_ist_notbetrieb_topic("panasonic_heat_pump/notbetrieb/DHWTemp", wurzel),
         "notbetrieb-Topic erkannt");
  pruefe(notbetrieb_ist_notbetrieb_topic(
             "panasonic_heat_pump/notbetrieb/Z1HeatCurveTargetHighTemp", wurzel),
         "langer Name erkannt");

  // das darf NICHT durchrutschen - sonst umgeht ein Set-Kommando die Karenzzeit
  pruefe(!notbetrieb_ist_notbetrieb_topic("panasonic_heat_pump/set/DHWTemp", wurzel),
         "set-Topic NICHT erkannt");
  pruefe(!notbetrieb_ist_notbetrieb_topic("panasonic_heat_pump/state/DHW_Target_Temp", wurzel),
         "state-Topic NICHT erkannt");

  // Praefix-Falle: ohne Pruefung auf das Trennzeichen wuerde das durchgehen
  pruefe(!notbetrieb_ist_notbetrieb_topic("panasonic_heat_pump/notbetriebXY/DHWTemp", wurzel),
         "aehnlich benannter Zweig NICHT erkannt");
  pruefe(!notbetrieb_ist_notbetrieb_topic("panasonic_heat_pump/notbetrieb", wurzel),
         "Wurzel ohne Namen NICHT erkannt");
  pruefe(!notbetrieb_ist_notbetrieb_topic("panasonic_heat_pump/notbetrieb/", wurzel),
         "Wurzel mit leerem Namen NICHT erkannt");

  // die andere Stufe hat einen anderen Prefix - Topics kreuzen sich nicht
  pruefe(!notbetrieb_ist_notbetrieb_topic("panasonic_heat_pump2/notbetrieb/DHWTemp", wurzel),
         "Topic der anderen Stufe NICHT erkannt");

  // Robustheit gegen Nullzeiger - im Callback kommt der Topic vom Broker
  pruefe(!notbetrieb_ist_notbetrieb_topic(0, wurzel), "Nullzeiger als Topic abgefangen");
  pruefe(!notbetrieb_ist_notbetrieb_topic("irgendwas", 0), "Nullzeiger als Wurzel abgefangen");
  pruefe(!notbetrieb_ist_notbetrieb_topic("irgendwas", ""), "leere Wurzel abgefangen");

  // Namensextraktion
  pruefe_text(notbetrieb_name_aus_topic("panasonic_heat_pump/notbetrieb/DHWTemp", wurzel),
              "DHWTemp", "Name hinter der Wurzel");
  pruefe(notbetrieb_name_aus_topic("panasonic_heat_pump/set/DHWTemp", wurzel) == 0,
         "kein Name aus einem set-Topic");
}

/*****************************************************************************/
/* 4. Die Schrittfolge                                                       */
/*****************************************************************************/
static void test_schrittfolge()
{
  printf("\n== Schrittfolge und Reihenfolge ==\n");

  pruefe_zahl((int)notbetrieb_schritt_anzahl(NOTBETRIEB_HEIZEN), 7, "Heizen hat sieben Schritte");
  pruefe_zahl((int)notbetrieb_schritt_anzahl(NOTBETRIEB_WASSER), 3, "Wasser hat drei Schritte");

  // Die Reihenfolge traegt dreifach: erst die Betriebsart (sonst schaltet der
  // Knopf eine Anlage ein, die auf Kuehlen steht), dann der Moduswechsel, dann
  // die Kurve - andersherum schreibt der Werks-Reset des Moduswechsels sie
  // sofort wieder ueber.
  const NotbetriebSchritt *s0 = notbetrieb_schritt(NOTBETRIEB_HEIZEN, 0);
  pruefe_text(s0->set_name, "OperationMode", "Heizen Schritt 1 setzt die Betriebsart");
  pruefe_zahl(s0->fester_wert, 0, "OperationMode auf 0 = Heat only");
  pruefe_zahl(s0->top, 4, "rueckgelesen an TOP4");

  const NotbetriebSchritt *s1 = notbetrieb_schritt(NOTBETRIEB_HEIZEN, 1);
  pruefe_text(s1->set_name, "HeatingMode", "Heizen Schritt 2 schaltet auf Kurve");
  pruefe_zahl(s1->fester_wert, 0, "HeatingMode auf 0 = Kurve");
  pruefe_zahl(s1->top, 76, "rueckgelesen an TOP76");

  const NotbetriebSchritt *s2 = notbetrieb_schritt(NOTBETRIEB_HEIZEN, 2);
  pruefe_text(s2->set_name, "Z1HeatCurveTargetHighTemp", "Schritt 3 ist der Vorlauf bei Kaelte");
  pruefe_zahl(s2->top, 29, "rueckgelesen an TOP29");

  // Die Betriebsart muss VOR der Kurve stehen: Ob ein Moduswechsel die
  // Kurvenpunkte anfasst, ist nicht gemessen - hinter ihr waere es ein Risiko.
  pruefe(notbetrieb_schritt(NOTBETRIEB_HEIZEN, 0)->set_name[0] == 'O' &&
             notbetrieb_schritt(NOTBETRIEB_HEIZEN, 2)->wert_index == 0,
         "Betriebsart steht vor dem ersten gehaltenen Wert");

  const NotbetriebSchritt *s6 = notbetrieb_schritt(NOTBETRIEB_HEIZEN, 6);
  pruefe_text(s6->set_name, "Heatpump", "Heizen Schritt 7 schaltet die Anlage ein");
  pruefe_zahl(s6->fester_wert, 1, "Heatpump auf 1");
  pruefe_zahl(s6->top, 0, "rueckgelesen an TOP0");

  pruefe(notbetrieb_schritt(NOTBETRIEB_HEIZEN, 7) == 0, "hinter dem letzten Schritt ist Schluss");

  // Wasser: OperationMode 3 traegt auch im KNX-Kuehlbetrieb (M3, 2026-08-20)
  const NotbetriebSchritt *w0 = notbetrieb_schritt(NOTBETRIEB_WASSER, 0);
  pruefe_text(w0->set_name, "OperationMode", "Wasser Schritt 1 setzt den Betriebsmodus");
  pruefe_zahl(w0->fester_wert, 3, "OperationMode auf 3 = DHW only");
  pruefe_zahl(w0->top, 4, "rueckgelesen an TOP4");

  const NotbetriebSchritt *w2 = notbetrieb_schritt(NOTBETRIEB_WASSER, 2);
  pruefe_text(w2->set_name, "Heatpump", "Wasser Schritt 3 schaltet die Anlage ein");

  // Jeder Schritt mit gehaltenem Wert muss auf einen gueltigen Index zeigen
  bool indizes_ok = true;
  for (unsigned r = 0; r < 2; r++)
  {
    NotbetriebRolle rolle = (r == 0) ? NOTBETRIEB_HEIZEN : NOTBETRIEB_WASSER;
    for (unsigned i = 0; i < notbetrieb_schritt_anzahl(rolle); i++)
    {
      const NotbetriebSchritt *s = notbetrieb_schritt(rolle, i);
      if (s->wert_index == NOTBETRIEB_FESTER_WERT)
        continue;
      if (s->wert_index < 0 || (unsigned)s->wert_index >= notbetrieb_wert_anzahl(rolle))
        indizes_ok = false;
      // der Schrittname muss zum Namen des gehaltenen Werts passen
      if (strcmp(s->set_name, notbetrieb_wert_name(rolle, (unsigned)s->wert_index)) != 0)
        indizes_ok = false;
    }
  }
  pruefe(indizes_ok, "jeder Wert-Schritt zeigt auf den gleichnamigen gehaltenen Wert");

  // Der Speicher muss fuer die laengere Rolle reichen
  pruefe(notbetrieb_wert_anzahl(NOTBETRIEB_HEIZEN) <= NOTBETRIEB_MAX_WERTE &&
             notbetrieb_wert_anzahl(NOTBETRIEB_WASSER) <= NOTBETRIEB_MAX_WERTE,
         "NOTBETRIEB_MAX_WERTE reicht fuer beide Rollen");
}

/*****************************************************************************/
/* 5. Der Zustandsautomat                                                    */
/*                                                                           */
/* Spielt Ablaeufe durch, statt einzelne Aufrufe zu pruefen - der Fehler,    */
/* den man sucht, steckt in der Abfolge, nicht im einzelnen Schritt.         */
/*****************************************************************************/

// Ein vollstaendiger Lauf, bei dem jeder Schritt nach 'antwortzeit' ms
// zurueckgelesen wird. 'richtung' ist der Rohtext von TOP101, den die Anlage
// waehrend des Laufs meldet - "0" heisst Heizen und laesst den Lauf gewaehren.
// Rueckgabe: Zustand am Ende.
static uint8_t lauf_durchspielen(NotbetriebRolle rolle, uint32_t start,
                                 uint32_t antwortzeit, uint32_t *dauer_out,
                                 const char *richtung = "0")
{
  NotbetriebLauf lauf;
  notbetrieb_lauf_leeren(&lauf);
  (void)notbetrieb_start(&lauf, start);

  uint32_t jetzt = start;
  uint32_t seit_schritt = 0;
  // Obergrenze, damit ein nicht terminierender Automat den Test scheitern
  // laesst statt endlos zu laufen
  for (unsigned runde = 0; runde < 10000; runde++)
  {
    jetzt += 1000; // die Firmware tickt aus loop(), hier 1 s je Runde
    seit_schritt += 1000;
    bool bestaetigt = (seit_schritt >= antwortzeit);
    NotbetriebAktion a = notbetrieb_tick(&lauf, rolle, jetzt, bestaetigt, richtung);
    if (a == NOTBETRIEB_SENDEN)
      seit_schritt = 0;
    if (a == NOTBETRIEB_FERTIG || a == NOTBETRIEB_ABBRUCH || a == NOTBETRIEB_ABBRUCH_KUEHLEN)
      break;
  }
  if (dauer_out)
    *dauer_out = jetzt - start;
  return lauf.zustand;
}

static void test_automat()
{
  printf("\n== Zustandsautomat ==\n");

  NotbetriebLauf lauf;
  notbetrieb_lauf_leeren(&lauf);
  pruefe_zahl(lauf.zustand, NOTBETRIEB_BEREIT, "frischer Lauf ist BEREIT");

  // Start setzt den ersten Schritt ab
  pruefe_zahl(notbetrieb_start(&lauf, 1000), NOTBETRIEB_SENDEN, "Start sendet Schritt 1");
  pruefe_zahl(lauf.zustand, NOTBETRIEB_LAEUFT, "Zustand ist LAEUFT");
  pruefe_zahl(lauf.schritt, 0, "Schrittzaehler steht auf 0");

  // Ein zweiter Klick waehrend des Laufs darf nichts anstossen
  pruefe_zahl(notbetrieb_start(&lauf, 2000), NOTBETRIEB_TU_NICHTS, "zweiter Klick prallt ab");
  pruefe_zahl(lauf.schritt, 0, "Schrittzaehler unveraendert");

  // Warten ohne Rueckmeldung: nichts passiert, solange das Timeout laeuft
  pruefe_zahl(notbetrieb_tick(&lauf, NOTBETRIEB_HEIZEN, 5000, false, "0"), NOTBETRIEB_TU_NICHTS,
              "ohne Rueckmeldung wird gewartet");
  pruefe_zahl(lauf.zustand, NOTBETRIEB_LAEUFT, "immer noch LAEUFT");

  // Eine Rueckmeldung VOR der Mindestwarte zaehlt noch nicht - sie koennte vom
  // Zustand vor dem Kommando stammen
  pruefe_zahl(notbetrieb_tick(&lauf, NOTBETRIEB_HEIZEN, 1000 + 5000, true, "0"), NOTBETRIEB_TU_NICHTS,
              "Rueckmeldung nach 5 s zaehlt noch nicht");
  pruefe_zahl(lauf.schritt, 0, "Schrittzaehler bleibt auf 0");

  // nach der Mindestwarte zaehlt sie
  pruefe_zahl(notbetrieb_tick(&lauf, NOTBETRIEB_HEIZEN,
                              1000 + NOTBETRIEB_SCHRITT_MINDESTWARTE_MS, true, "0"),
              NOTBETRIEB_SENDEN, "Rueckmeldung nach der Mindestwarte stoesst den naechsten Schritt an");
  pruefe_zahl(lauf.schritt, 1, "Schrittzaehler auf 1");

  // Ein realistischer Lauf: die WP antwortet nach 6 s (Abfragezyklus)
  uint32_t dauer = 0;
  pruefe_zahl(lauf_durchspielen(NOTBETRIEB_HEIZEN, 1000, 6000, &dauer), NOTBETRIEB_GRUEN,
              "Heizen mit 6-s-Antworten wird GRUEN");
  pruefe(dauer <= 65000u, "realistischer Heizen-Lauf bleibt unter 65 s");
  printf("       (gemessene Laufdauer: %u ms)\n", dauer);

  pruefe_zahl(lauf_durchspielen(NOTBETRIEB_WASSER, 1000, 6000, &dauer), NOTBETRIEB_GRUEN,
              "Wasser mit 6-s-Antworten wird GRUEN");

  // Die Waermepumpe uebernimmt laut KNX-Messung in 2-8 s; auch der langsame
  // Rand muss durchgehen
  pruefe_zahl(lauf_durchspielen(NOTBETRIEB_HEIZEN, 1000, 19000, &dauer), NOTBETRIEB_GRUEN,
              "auch 19 s je Schritt reichen noch");

  // Antwortet die WP gar nicht, muss ROT kommen - und zwar durch den
  // Schritt-Timeout, nicht durch den Gesamtdeckel
  NotbetriebLauf tot;
  notbetrieb_lauf_leeren(&tot);
  (void)notbetrieb_start(&tot, 1000);
  NotbetriebAktion a = NOTBETRIEB_TU_NICHTS;
  uint32_t t = 1000;
  for (unsigned i = 0; i < 100 && a != NOTBETRIEB_ABBRUCH; i++)
  {
    t += 1000;
    a = notbetrieb_tick(&tot, NOTBETRIEB_HEIZEN, t, false, "0");
  }
  pruefe_zahl(tot.zustand, NOTBETRIEB_ROT, "ohne Rueckmeldung wird es ROT");
  pruefe_zahl((int)(t - 1000), (int)NOTBETRIEB_SCHRITT_TIMEOUT_MS,
              "ROT genau nach dem Schritt-Timeout, nicht spaeter");
  pruefe_zahl(tot.schritt, 0, "abgebrochen im ersten Schritt, nicht weitergemacht");

  // Nach ROT darf neu gestartet werden
  pruefe_zahl(notbetrieb_start(&tot, 100000), NOTBETRIEB_SENDEN, "nach ROT ist ein neuer Lauf erlaubt");

  // Bricht ein Schritt in der Mitte ab, bleibt der Zaehler dort stehen -
  // die Seite zeigt ROT, es wird NICHT weitergemacht
  NotbetriebLauf mitte;
  notbetrieb_lauf_leeren(&mitte);
  (void)notbetrieb_start(&mitte, 1000);
  (void)notbetrieb_tick(&mitte, NOTBETRIEB_HEIZEN, 10000, true, "0"); // Schritt 1 ok
  (void)notbetrieb_tick(&mitte, NOTBETRIEB_HEIZEN, 19000, true, "0"); // Schritt 2 ok
  pruefe_zahl(mitte.schritt, 2, "steht bei Schritt 3");
  a = NOTBETRIEB_TU_NICHTS;
  t = 19000;
  for (unsigned i = 0; i < 100 && a != NOTBETRIEB_ABBRUCH; i++)
  {
    t += 1000;
    a = notbetrieb_tick(&mitte, NOTBETRIEB_HEIZEN, t, false, "0");
  }
  pruefe_zahl(mitte.zustand, NOTBETRIEB_ROT, "Abbruch mitten in der Folge wird ROT");
  pruefe_zahl(mitte.schritt, 2, "der Zaehler bleibt stehen, es wird nicht weitergemacht");

  // Kommt die Rueckmeldung genau im Timeout-Moment, gilt der Schritt als
  // geschafft - ein knapp erreichtes Ziel ist erreicht
  NotbetriebLauf knapp;
  notbetrieb_lauf_leeren(&knapp);
  (void)notbetrieb_start(&knapp, 1000);
  pruefe_zahl(notbetrieb_tick(&knapp, NOTBETRIEB_HEIZEN, 1000 + NOTBETRIEB_SCHRITT_TIMEOUT_MS, true, "0"),
              NOTBETRIEB_SENDEN, "Rueckmeldung im Timeout-Moment zaehlt noch");
  // (liegt hinter der Mindestwarte, sonst waere sie gar nicht gezaehlt worden)
  pruefe_zahl(knapp.zustand, NOTBETRIEB_LAEUFT, "kein ROT im selben Tick");

  // Ein Tick auf einem fertigen Lauf darf nichts mehr tun
  NotbetriebLauf fertig;
  notbetrieb_lauf_leeren(&fertig);
  fertig.zustand = NOTBETRIEB_GRUEN;
  pruefe_zahl(notbetrieb_tick(&fertig, NOTBETRIEB_HEIZEN, 5000, true, "0"), NOTBETRIEB_TU_NICHTS,
              "Tick auf GRUEN tut nichts");

  // Nullzeiger duerfen nicht abstuerzen
  pruefe_zahl(notbetrieb_tick(0, NOTBETRIEB_HEIZEN, 5000, true, "0"), NOTBETRIEB_TU_NICHTS,
              "Tick mit Nullzeiger abgefangen");
  pruefe_zahl(notbetrieb_start(0, 5000), NOTBETRIEB_TU_NICHTS, "Start mit Nullzeiger abgefangen");

  // Der Gesamtdeckel ist abgeleitet, nicht frei gewaehlt
  pruefe_zahl((int)notbetrieb_gesamtdeckel_ms(NOTBETRIEB_HEIZEN),
              (int)(7u * NOTBETRIEB_SCHRITT_TIMEOUT_MS), "Gesamtdeckel Heizen = 7 x Schritt-Timeout");
  pruefe_zahl((int)notbetrieb_gesamtdeckel_ms(NOTBETRIEB_WASSER),
              (int)(3u * NOTBETRIEB_SCHRITT_TIMEOUT_MS), "Gesamtdeckel Wasser = 3 x Schritt-Timeout");
}

/*****************************************************************************/
/* 6. millis()-Ueberlauf nach 49,7 Tagen                                     */
/*                                                                           */
/* Der Lauf beginnt kurz vor dem Ueberlauf und endet dahinter. Ein Vergleich */
/* der Zeitpunkte statt der Differenz wuerde hier sofort ROT liefern, obwohl */
/* jeder Schritt puenktlich zurueckkam.                                      */
/*****************************************************************************/
/*****************************************************************************/
/* 5a. Die Mindestwartezeit                                                   */
/*                                                                            */
/* Der gefaehrliche Fall aus der Praxis: Das Umschalten auf Kurvenbetrieb     */
/* setzt die Kurvenpunkte auf die Werksvorgaben zurueck. Traegt actual_data   */
/* beim naechsten Schritt noch den alten Wert, gilt der Schritt sofort als    */
/* erledigt - und der Werks-Reset ueberschreibt ihn danach.                   */
/*****************************************************************************/
static void test_mindestwarte()
{
  printf("\n== Mindestwartezeit vor der Bestaetigung ==\n");

  // Eine WP, die "sofort" bestaetigt (veralteter Rueckgabewert), darf den Lauf
  // nicht schneller machen als die Mindestwarte je Schritt erlaubt
  uint32_t dauer = 0;
  pruefe_zahl(lauf_durchspielen(NOTBETRIEB_HEIZEN, 1000, 0, &dauer), NOTBETRIEB_GRUEN,
              "Lauf mit Sofortbestaetigung wird GRUEN");
  const uint32_t mindestens = 7u * NOTBETRIEB_SCHRITT_MINDESTWARTE_MS;
  pruefe(dauer >= mindestens, "aber nicht schneller als 7 x Mindestwarte");
  printf("       (Laufdauer %u ms, Untergrenze %u ms)\n", dauer, mindestens);

  // Gegenprobe Wasser: drei Schritte
  (void)lauf_durchspielen(NOTBETRIEB_WASSER, 1000, 0, &dauer);
  pruefe(dauer >= 3u * NOTBETRIEB_SCHRITT_MINDESTWARTE_MS,
         "Wasser ebenso, mit drei Schritten");

  // Die Regel muss in sich stimmig bleiben, sonst endet jeder Lauf in ROT
  pruefe(NOTBETRIEB_SCHRITT_MINDESTWARTE_MS < NOTBETRIEB_SCHRITT_TIMEOUT_MS,
         "Mindestwarte liegt unter dem Schritt-Timeout");
}

static void test_ueberlauf()
{
  printf("\n== millis()-Ueberlauf ==\n");

  const uint32_t kurz_vor_ende = 0xFFFFFF00u; // 256 ms vor dem Ueberlauf
  uint32_t dauer = 0;

  pruefe_zahl(lauf_durchspielen(NOTBETRIEB_HEIZEN, kurz_vor_ende, 6000, &dauer), NOTBETRIEB_GRUEN,
              "Lauf ueber den Ueberlauf hinweg wird GRUEN");

  // und die Gegenprobe: ohne Rueckmeldung muss es auch dort ROT werden,
  // nicht etwa nie
  NotbetriebLauf tot;
  notbetrieb_lauf_leeren(&tot);
  (void)notbetrieb_start(&tot, kurz_vor_ende);
  NotbetriebAktion a = NOTBETRIEB_TU_NICHTS;
  uint32_t t = kurz_vor_ende;
  for (unsigned i = 0; i < 100 && a != NOTBETRIEB_ABBRUCH; i++)
  {
    t += 1000;
    a = notbetrieb_tick(&tot, NOTBETRIEB_HEIZEN, t, false, "0");
  }
  pruefe_zahl(tot.zustand, NOTBETRIEB_ROT, "Timeout greift auch ueber den Ueberlauf");
  pruefe_zahl((int)(uint32_t)(t - kurz_vor_ende), (int)NOTBETRIEB_SCHRITT_TIMEOUT_MS,
              "und zwar zum selben Zeitpunkt wie sonst");
}

/*****************************************************************************/
/* 7. Welchen Wert traegt ein Schritt?                                       */
/*****************************************************************************/
static void test_schritt_wert()
{
  printf("\n== Wert des aktuellen Schritts ==\n");

  NotbetriebSpeicher sp;
  notbetrieb_speicher_leeren(&sp);
  NotbetriebLauf lauf;
  notbetrieb_lauf_leeren(&lauf);
  int wert = -999;

  // fester Wert braucht keinen Speicher
  lauf.schritt = 0;
  pruefe(notbetrieb_schritt_wert(&lauf, NOTBETRIEB_HEIZEN, &sp, &wert), "fester Wert immer verfuegbar");
  pruefe_zahl(wert, 0, "OperationMode 0 = Heat only");

  lauf.schritt = 1;
  pruefe(notbetrieb_schritt_wert(&lauf, NOTBETRIEB_HEIZEN, &sp, &wert), "auch Schritt 2 ist fest");
  pruefe_zahl(wert, 0, "HeatingMode 0 = Kurve");

  // gehaltener Wert fehlt noch
  lauf.schritt = 2;
  pruefe(!notbetrieb_schritt_wert(&lauf, NOTBETRIEB_HEIZEN, &sp, &wert),
         "fehlender gehaltener Wert wird gemeldet");

  // jetzt liegt er vor
  pruefe(annehmen(&sp, NOTBETRIEB_HEIZEN, "Z1HeatCurveTargetHighTemp", 34), "TargetHigh gesetzt");
  pruefe(notbetrieb_schritt_wert(&lauf, NOTBETRIEB_HEIZEN, &sp, &wert), "Wert jetzt verfuegbar");
  pruefe_zahl(wert, 34, "TargetHigh 34 - der Vorlauf bei Kaelte");

  // negative Werte muessen durchkommen
  notbetrieb_speicher_leeren(&sp);
  pruefe(annehmen(&sp, NOTBETRIEB_HEIZEN, "Z1HeatCurveOutsideLowTemp", -10), "OutsideLow gesetzt");
  lauf.schritt = 4; // Index 4 traegt OutsideLow (seit die Betriebsart vorn steht)
  pruefe(notbetrieb_schritt_wert(&lauf, NOTBETRIEB_HEIZEN, &sp, &wert), "negativer Wert verfuegbar");
  pruefe_zahl(wert, -10, "OutsideLow -10 kommt unveraendert durch");

  // hinter dem letzten Schritt gibt es nichts mehr
  lauf.schritt = 99;
  pruefe(!notbetrieb_schritt_wert(&lauf, NOTBETRIEB_HEIZEN, &sp, &wert),
         "kein Wert hinter dem letzten Schritt");
}

/*****************************************************************************/
/* 8. Ruecklesen - die Falle mit dem leeren actual_data                       */
/*****************************************************************************/
static void test_ruecklesen()
{
  printf("\n== Ruecklesen eines Schritts ==\n");

  pruefe(notbetrieb_rueckgelesen("0", 0), "\"0\" bestaetigt 0");
  pruefe(notbetrieb_rueckgelesen("34", 34), "\"34\" bestaetigt 34");
  pruefe(notbetrieb_rueckgelesen("-10", -10), "\"-10\" bestaetigt -10");
  pruefe(!notbetrieb_rueckgelesen("35", 34), "\"35\" bestaetigt NICHT 34");

  // DIE entscheidende Zusicherung: Ein TOP, das noch nie empfangen wurde,
  // steht als leerer String da. atoi("") waere 0 - und der erste Schritt der
  // Heizen-Folge hat den Sollwert 0. Ohne diese Pruefung meldete der Knopf
  // GRUEN, ohne dass die Waermepumpe je geantwortet haette.
  pruefe(!notbetrieb_rueckgelesen("", 0), "LEERER Wert bestaetigt die 0 NICHT");
  pruefe(!notbetrieb_rueckgelesen(0, 0), "Nullzeiger bestaetigt die 0 NICHT");
  pruefe(!notbetrieb_rueckgelesen("   ", 0), "nur Leerzeichen bestaetigen die 0 NICHT");

  // Was keine saubere ganze Zahl ist, zaehlt nicht
  pruefe(!notbetrieb_rueckgelesen("1.5", 1), "\"1.5\" bestaetigt 1 NICHT");
  pruefe(!notbetrieb_rueckgelesen("2 Hz", 2), "\"2 Hz\" bestaetigt 2 NICHT");
  pruefe(!notbetrieb_rueckgelesen("Off", 0), "\"Off\" bestaetigt 0 NICHT");
  pruefe(!notbetrieb_rueckgelesen("--5", -5), "\"--5\" bestaetigt -5 NICHT");

  // Leerzeichen drumherum sind in Ordnung - der Dekodierpfad kann sie liefern
  pruefe(notbetrieb_rueckgelesen(" 26 ", 26), "\" 26 \" bestaetigt 26");
  pruefe(notbetrieb_rueckgelesen("15\n", 15), "abschliessender Zeilenumbruch stoert nicht");
}

/*****************************************************************************/
/* 9. Die Freigabe ueber die Betriebsart (TOP101)                            */
/*                                                                           */
/* Der Befund vom 2026-08-20: Der externe KNX-Schalter gibt die Richtung vor.*/
/* Steht die Anlage auf Kuehlen, verwirft sie jeden Heizmodus stillschweigend*/
/* - der erste Lauf an H1 endete deshalb nach 20 s in einem ROT ohne         */
/* Erklaerung. Seit dem 2026-08-21 ist der Knopf in dieser Lage gesperrt.     */
/*                                                                           */
/* Owner-Entscheidung: ALLES ausser einer sauber gelesenen 0 ist "nicht      */
/* Heizen". Das prueft dieser Abschnitt Fall fuer Fall.                       */
/*****************************************************************************/
static void test_freigabe()
{
  printf("\n== Freigabe ueber die Betriebsart (TOP101) ==\n");

  // Ein vollstaendiger Wertesatz, damit die Werte-Sperre nicht dazwischenfunkt
  NotbetriebSpeicher voll;
  notbetrieb_speicher_leeren(&voll);
  pruefe(annehmen(&voll, NOTBETRIEB_HEIZEN, "Z1HeatCurveTargetHighTemp", 34) &&
             annehmen(&voll, NOTBETRIEB_HEIZEN, "Z1HeatCurveTargetLowTemp", 26) &&
             annehmen(&voll, NOTBETRIEB_HEIZEN, "Z1HeatCurveOutsideLowTemp", -10) &&
             annehmen(&voll, NOTBETRIEB_HEIZEN, "Z1HeatCurveOutsideHighTemp", 15),
         "Wertesatz Heizen vollstaendig");

  // Die eine Freigabe
  pruefe(notbetrieb_heizbetrieb_belegt("0"), "TOP101 = 0 (Heizen) gibt frei");
  pruefe_zahl(notbetrieb_sperrgrund(NOTBETRIEB_HEIZEN, &voll, "0"), NOTBETRIEB_FREI,
              "und damit ist der Knopf frei");

  // Alle vier Sperrfaelle
  pruefe(!notbetrieb_heizbetrieb_belegt("1"), "TOP101 = 1 (Kuehlen) sperrt");
  pruefe(!notbetrieb_heizbetrieb_belegt("2"), "TOP101 = 2 (unknown) sperrt");
  pruefe(!notbetrieb_heizbetrieb_belegt("-1"), "TOP101 = -1 (Feld leer) sperrt");
  pruefe(!notbetrieb_heizbetrieb_belegt(""), "nie empfangenes TOP101 sperrt");
  pruefe(!notbetrieb_heizbetrieb_belegt(0), "Nullzeiger sperrt");
  // Der Klartext aus der Weboberflaeche darf nicht als Freigabe durchgehen -
  // actual_data traegt die Zahl, die Klartexte entstehen erst beim Anzeigen
  pruefe(!notbetrieb_heizbetrieb_belegt("Heat"), "\"Heat\" als Text gibt NICHT frei");

  pruefe_zahl(notbetrieb_sperrgrund(NOTBETRIEB_HEIZEN, &voll, "1"), NOTBETRIEB_SPERRE_HEIZBETRIEB,
              "Kuehlbetrieb sperrt den Knopf");
  pruefe_zahl(notbetrieb_sperrgrund(NOTBETRIEB_HEIZEN, &voll, ""), NOTBETRIEB_SPERRE_HEIZBETRIEB,
              "keine Rueckmeldung sperrt den Knopf");

  // Warmwasser ist NICHT betroffen: OperationMode 3 traegt auch im
  // Kuehlbetrieb - genau der Sommerfall, fuer den Stufe 2 gebaut ist (M3)
  NotbetriebSpeicher wasser;
  notbetrieb_speicher_leeren(&wasser);
  pruefe(annehmen(&wasser, NOTBETRIEB_WASSER, "DHWTemp", 48), "DHWTemp gesetzt");
  pruefe_zahl(notbetrieb_sperrgrund(NOTBETRIEB_WASSER, &wasser, "1"), NOTBETRIEB_FREI,
              "Warmwasser bleibt im Kuehlbetrieb frei");
  pruefe_zahl(notbetrieb_sperrgrund(NOTBETRIEB_WASSER, &wasser, ""), NOTBETRIEB_FREI,
              "Warmwasser braucht TOP101 gar nicht");

  // Fehlende Werte schlagen durch, auch wenn die Betriebsart stimmt
  NotbetriebSpeicher halb;
  notbetrieb_speicher_leeren(&halb);
  pruefe(annehmen(&halb, NOTBETRIEB_HEIZEN, "Z1HeatCurveTargetHighTemp", 34), "nur ein Wert da");
  pruefe_zahl(notbetrieb_sperrgrund(NOTBETRIEB_HEIZEN, &halb, "0"), NOTBETRIEB_SPERRE_WERTE,
              "fehlende Werte sperren trotz Heizbetrieb");
  pruefe_zahl(notbetrieb_sperrgrund(NOTBETRIEB_HEIZEN, &halb, "1"), NOTBETRIEB_SPERRE_WERTE,
              "und sie werden zuerst gemeldet, wenn beides fehlt");
  pruefe_zahl(notbetrieb_sperrgrund(NOTBETRIEB_HEIZEN, 0, "0"), NOTBETRIEB_SPERRE_WERTE,
              "Nullzeiger auf den Speicher sperrt");

  /***************************************************************************/
  /* Der Abbruch mitten im Lauf                                              */
  /*                                                                         */
  /* Hier gilt die andere Regel: NUR eine klare 1 bricht ab. Ein Aussetzer   */
  /* duerfte einen sauber laufenden Vorgang nicht zerreissen und die Anlage  */
  /* halb geschaltet stehen lassen.                                          */
  /***************************************************************************/
  printf("\n== Abbruch bei Kuehlbetrieb waehrend des Laufs ==\n");

  pruefe(notbetrieb_kuehlbetrieb_gemeldet(NOTBETRIEB_HEIZEN, "1"), "klare 1 bricht ab");
  pruefe(!notbetrieb_kuehlbetrieb_gemeldet(NOTBETRIEB_HEIZEN, ""), "ein Aussetzer bricht NICHT ab");
  pruefe(!notbetrieb_kuehlbetrieb_gemeldet(NOTBETRIEB_HEIZEN, "2"), "\"unknown\" bricht NICHT ab");
  pruefe(!notbetrieb_kuehlbetrieb_gemeldet(NOTBETRIEB_WASSER, "1"), "Warmwasser bricht nie ab");

  // Ein Lauf, der von Anfang an gegen eine kuehlende Anlage laeuft
  uint32_t dauer = 0;
  pruefe_zahl(lauf_durchspielen(NOTBETRIEB_HEIZEN, 1000, 6000, &dauer, "1"), NOTBETRIEB_ROT,
              "Lauf gegen Kuehlbetrieb wird ROT");
  pruefe(dauer <= 2000u, "und zwar sofort, nicht erst nach dem Schritt-Timeout");

  // Ein Aussetzer waehrend des Laufs darf nichts anrichten
  pruefe_zahl(lauf_durchspielen(NOTBETRIEB_HEIZEN, 1000, 6000, &dauer, ""), NOTBETRIEB_GRUEN,
              "ohne TOP101-Aussage laeuft der Vorgang durch");

  // Der KNX-Schalter geht mitten im Lauf auf Kuehlen: Der Zaehler bleibt
  // stehen, wo es passiert ist, und es wird NICHT bis zum Einschalten weiter-
  // gemacht - sonst liefe die Anlage am Ende kuehlend.
  NotbetriebLauf um;
  notbetrieb_lauf_leeren(&um);
  (void)notbetrieb_start(&um, 1000);
  uint32_t t = 1000;
  uint32_t seit_schritt = 0;
  NotbetriebAktion a = NOTBETRIEB_TU_NICHTS;
  for (unsigned i = 0; i < 200; i++)
  {
    t += 1000;
    seit_schritt += 1000;
    // ab Schritt 4 meldet die Anlage Kuehlen
    const char *richtung = (um.schritt >= 3) ? "1" : "0";
    a = notbetrieb_tick(&um, NOTBETRIEB_HEIZEN, t, seit_schritt >= 9000, richtung);
    if (a == NOTBETRIEB_SENDEN)
      seit_schritt = 0;
    if (a == NOTBETRIEB_FERTIG || a == NOTBETRIEB_ABBRUCH || a == NOTBETRIEB_ABBRUCH_KUEHLEN)
      break;
  }
  pruefe_zahl(a, NOTBETRIEB_ABBRUCH_KUEHLEN, "Umschalten mitten im Lauf meldet den Kuehl-Abbruch");
  pruefe_zahl(um.zustand, NOTBETRIEB_ROT, "und der Lauf ist ROT");
  pruefe_zahl(um.schritt, 3, "der Zaehler bleibt stehen, wo es passiert ist");
}

/*****************************************************************************/
/* 10. Der Anzeigeverfall                                                    */
/*                                                                           */
/* GRUEN und ROT blieben bis zum 2026-08-21 stehen, bis jemand erneut        */
/* drueckte - wer die Seite am naechsten Tag oeffnete, sah ein Ergebnis von  */
/* gestern und musste raten, ob gerade etwas schiefgeht.                     */
/*****************************************************************************/
static void test_anzeigeverfall()
{
  printf("\n== Anzeigeverfall nach 15 Minuten ==\n");

  const uint32_t start = 100000u;

  // ROT verfaellt - aber nicht zu frueh
  NotbetriebLauf rot;
  notbetrieb_lauf_leeren(&rot);
  notbetrieb_abschluss(&rot, NOTBETRIEB_ROT, start);
  pruefe_zahl((int)rot.ende, (int)start, "der Abschluss haelt den Zeitpunkt fest");
  pruefe(!notbetrieb_verfall_pruefen(&rot, start + NOTBETRIEB_ANZEIGE_VERFALL_MS - 1),
         "kurz vor 15 min steht das ROT noch");
  pruefe_zahl(rot.zustand, NOTBETRIEB_ROT, "und ist unveraendert ROT");
  pruefe(notbetrieb_verfall_pruefen(&rot, start + NOTBETRIEB_ANZEIGE_VERFALL_MS),
         "nach 15 min faellt es weg");
  pruefe_zahl(rot.zustand, NOTBETRIEB_BEREIT, "der Knopf steht wieder da");
  pruefe(!notbetrieb_verfall_pruefen(&rot, start + 2 * NOTBETRIEB_ANZEIGE_VERFALL_MS),
         "und meldet den Verfall nur einmal");

  // GRUEN ebenso
  NotbetriebLauf gruen;
  notbetrieb_lauf_leeren(&gruen);
  notbetrieb_abschluss(&gruen, NOTBETRIEB_GRUEN, start);
  pruefe(notbetrieb_verfall_pruefen(&gruen, start + NOTBETRIEB_ANZEIGE_VERFALL_MS),
         "auch GRUEN verfaellt");

  // Ein LAUFENDER Vorgang wird nie angefasst - dafuer ist der Gesamtdeckel da
  NotbetriebLauf laeuft;
  notbetrieb_lauf_leeren(&laeuft);
  (void)notbetrieb_start(&laeuft, start);
  pruefe(!notbetrieb_verfall_pruefen(&laeuft, start + 2 * NOTBETRIEB_ANZEIGE_VERFALL_MS),
         "ein laufender Vorgang verfaellt nicht");
  pruefe_zahl(laeuft.zustand, NOTBETRIEB_LAEUFT, "er laeuft weiter");

  // Ein neuer Lauf loescht den Zeitstempel des alten Ergebnisses - sonst
  // koennte ein 16 Minuten altes ROT den frischen Lauf sofort abraeumen
  NotbetriebLauf neu;
  notbetrieb_lauf_leeren(&neu);
  notbetrieb_abschluss(&neu, NOTBETRIEB_ROT, start);
  (void)notbetrieb_start(&neu, start + NOTBETRIEB_ANZEIGE_VERFALL_MS - 1000);
  pruefe_zahl((int)neu.ende, 0, "der Start loescht den alten Zeitstempel");

  // Und das alles ueber den millis()-Ueberlauf hinweg
  const uint32_t kurz_vor_ende = 0xFFFFFF00u;
  NotbetriebLauf ueber;
  notbetrieb_lauf_leeren(&ueber);
  notbetrieb_abschluss(&ueber, NOTBETRIEB_ROT, kurz_vor_ende);
  pruefe(!notbetrieb_verfall_pruefen(&ueber, (uint32_t)(kurz_vor_ende + 60000u)),
         "eine Minute nach dem Ueberlauf steht das ROT noch");
  pruefe(notbetrieb_verfall_pruefen(&ueber,
                                    (uint32_t)(kurz_vor_ende + NOTBETRIEB_ANZEIGE_VERFALL_MS)),
         "und faellt auch dort punktgenau weg");
}

int main()
{
  printf("Hosttest Notbetrieb (src/notbetrieb.h)\n");
  test_vollstaendigkeit();
  test_grenzen();
  test_karenz_ausnahme();
  test_schrittfolge();
  test_automat();
  test_mindestwarte();
  test_ueberlauf();
  test_schritt_wert();
  test_ruecklesen();
  test_freigabe();
  test_anzeigeverfall();

  printf("\n%s (%d Fehler)\n", fehler == 0 ? "ALLE PRUEFUNGEN BESTANDEN" : "FEHLGESCHLAGEN", fehler);
  return fehler == 0 ? 0 : 1;
}
