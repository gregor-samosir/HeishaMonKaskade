// Nachweis fuer den Kommandopfad OHNE Geraet: Tabelle, Umrechnung, Maske,
// Konflikt und leere Vorlage (src/setcommands.h, 3.25.1, Befund C1 der
// Durchsicht 2026-10-15).
//
// Geprueft wird der Code, der auch auf dem Geraet laeuft: setcommands.h wird
// direkt eingebunden, es gibt keine Nachbildung. Bis 3.25.0 trugen
// merge_test.cpp und die byte*_test.cpp Masken und Parameter als eigene
// Konstanten - ein Fehler in einer Tabellenzeile liess sie alle gruen. Das
// Beispiel aus dem Befund: Parameter 64 statt 16 bei SET40 (Maske 0x30) ergibt
// 0x40 & 0x30 = 0, das Kommando sendete still "keine Aenderung".
//
// Fuenf Faelle:
//  1. Die echte Tabelle erfuellt alle Regeln von setcommands_pruefen(). (Beim
//     Uebersetzen prueft das ein static_assert im Header selbst; hier kommt
//     die Auskunft, WELCHE Regel und WELCHE Zeile, falls es bricht.)
//  2. GEGENPROBEN: Eine Kopie der Tabelle, in der genau eine Zeile kaputt ist,
//     muss an genau dieser Zeile mit genau dieser Regel scheitern. Das ist der
//     Beleg, dass die Pruefung etwas pruefen kann - jede Regel hat ihr Beispiel.
//  3. Stichproben der Kodierung gegen die Werte aus der Doku und den Messungen
//     (von Hand aufgeschrieben, nicht aus der Tabelle gerechnet).
//  4. setcommand_anwenden(): Nachbarfelder bleiben, Wiederholung ueberschreibt,
//     Konflikt wird gemeldet, Unfug aendert nichts. Dazu die leere Vorlage.
//  5. Die Notbetriebs-Tabellen nennen nur Namen, die es in setCommands[] gibt,
//     und ihre Festwerte liegen im erlaubten Bereich. Bisher fing das nur eine
//     Laufzeitpruefung in notbetrieb.cpp.
//
// Mit --liste druckt das Programm alle Paare (SET, Wert) -> (Byte, Maske,
// Rohwert) und beendet sich. Das Format ist das der Vorher/Nachher-Liste im
// Arbeitsplan-Kommandopfad-3.25.1.md.
//
// Bauen und ausfuehren (ueber test/hosttests.sh oder einzeln):
//   c++ -std=c++17 -O2 -Wall -Wextra -o /tmp/setcommands_test test/setcommands_test.cpp
//   /tmp/setcommands_test           (Rueckgabewert != 0 = Zusicherung gebrochen)
//   /tmp/setcommands_test --liste   (Paarliste nach stdout)

#include <cstdio>
#include <cstdint>
#include <cstring>

// Dieser Test uebersetzt auch bei kaputter Tabelle, damit Fall 1 Regel und
// Zeile nennen kann; das static_assert im Header bricht dann nur Firmware und
// die uebrigen Tests (siehe Kommentar am Ende von setcommands.h).
#define SETCOMMANDS_OHNE_STATIC_ASSERT
#include "../src/setcommands.h"
#include "../src/notbetrieb.h"

static int fehler = 0;

// eine Zusicherung mit Klartext, damit der CI-Log ohne Debugger lesbar ist
static void pruefe(bool bedingung, const char *was)
{
  printf("  [%s] %s\n", bedingung ? "ok " : "FEHLER", was);
  if (!bedingung)
    fehler++;
}

static void pruefe_byte(int ist, int soll, const char *was)
{
  const bool ok = (ist == soll);
  printf("  [%s] %-58s (erwartet 0x%02X, ist 0x%02X)\n", ok ? "ok " : "FEHLER", was, soll, ist);
  if (!ok)
    fehler++;
}

static const char *regel_name(SetPruefRegel r)
{
  switch (r)
  {
  case SETPRUEF_OK:
    return "OK";
  case SETPRUEF_NAME:
    return "NAME";
  case SETPRUEF_POSITION:
    return "POSITION";
  case SETPRUEF_MASKE:
    return "MASKE";
  case SETPRUEF_BEREICH:
    return "BEREICH";
  case SETPRUEF_OPMODE:
    return "OPMODE";
  case SETPRUEF_ROHWERT:
    return "ROHWERT";
  case SETPRUEF_AUSSERHALB:
    return "AUSSERHALB";
  case SETPRUEF_UEBERLAPP:
    return "UEBERLAPP";
  }
  return "?";
}

// Wert zu einem Namen: kodiert, ohne Umweg ueber die Pufferlogik
static int roh_von(const char *name, int wert)
{
  const SetCommand *c = setcommand_finden(name);
  if (!c)
    return -1000;
  return setcommand_roh(*c, wert);
}

// Gegenprobe: eine Kopie der Tabelle, in Zeile `zeile` veraendert. Das
// Veraendern erledigt der Aufrufer ueber `aendern`; erwartet wird genau diese
// Regel an genau dieser Zeile.
template <typename F>
static void gegenprobe(const char *name_der_zeile, const char *was, SetPruefRegel erwartet, F aendern)
{
  static SetCommand kopie[SETCOMMANDCOUNT];
  memcpy(kopie, setCommands, sizeof kopie);

  unsigned int zeile = SETCOMMANDCOUNT;
  for (unsigned int i = 0; i < SETCOMMANDCOUNT; i++)
    if (strcmp(kopie[i].name, name_der_zeile) == 0)
      zeile = i;
  if (zeile == SETCOMMANDCOUNT)
  {
    printf("  [FEHLER] Zeile %s nicht in der Tabelle - Gegenprobe \"%s\" prueft nichts\n", name_der_zeile, was);
    fehler++;
    return;
  }

  aendern(kopie[zeile]);
  const SetPruefung p = setcommands_pruefen(kopie, SETCOMMANDCOUNT);
  char text[200];
  snprintf(text, sizeof text, "%s -> Regel %s an Zeile %s (ist: %s, Zeile %u)", was, regel_name(erwartet),
           name_der_zeile, regel_name(p.regel), p.zeile);
  pruefe(p.regel == erwartet && p.zeile == zeile, text);
}

// Liste aller Paare, Format wie die Vorher-Liste im Arbeitsplan
static int liste_drucken()
{
  for (unsigned int i = 0; i < SETCOMMANDCOUNT; i++)
  {
    const SetCommand &c = setCommands[i];
    for (int w = c.min; w <= c.max; w++)
    {
      const int roh = setcommand_roh(c, w);
      printf("SET%u %s wert=%d byte=%u maske=0x%02X roh=%d (roh_unbeschnitten=%d)\n", c.number, c.name, w, c.pos,
             c.mask, roh & 0xFF, roh);
    }
  }
  return 0;
}

int main(int argc, char **argv)
{
  if (argc > 1 && strcmp(argv[1], "--liste") == 0)
    return liste_drucken();

  // ---------------------------------------------------------------------
  printf("== Fall 1: die echte Tabelle erfuellt alle Regeln ==\n");
  {
    const SetPruefung p = setcommands_pruefen(setCommands, SETCOMMANDCOUNT);
    char text[120];
    snprintf(text, sizeof text, "setcommands_pruefen(setCommands) = %s (Zeile %u)", regel_name(p.regel), p.zeile);
    pruefe(p.regel == SETPRUEF_OK, text);
    // Zahl der Zeilen fest: wer ein Kommando hinzufuegt oder streicht, muss das
    // hier nachziehen - und denkt dabei an MQTT-Topics.md und SET-TOP-Zuordnung.md
    pruefe(SETCOMMANDCOUNT == 38, "38 Set-Kommandos (SET1-SET40 mit den Luecken 7 und 8)");
  }

  // ---------------------------------------------------------------------
  printf("\n== Fall 2: Gegenproben - jede Regel bricht an ihrem Beispiel ==\n");
  // das Beispiel aus dem Befund C1
  gegenprobe("AltExternalSensor", "SET40 Parameter 64 statt 16", SETPRUEF_AUSSERHALB,
             [](SetCommand &c) { c.param = 64; });
  gegenprobe("AltExternalSensor", "Maske 0", SETPRUEF_MASKE, [](SetCommand &c) { c.mask = 0; });
  gegenprobe("AltExternalSensor", "Maske 0x0F statt 0x30 (Rohwerte 16/32 liegen ausserhalb)", SETPRUEF_AUSSERHALB,
             [](SetCommand &c) { c.mask = 0x0F; });
  gegenprobe("Heatpump", "Name leer", SETPRUEF_NAME, [](SetCommand &c) { c.name = ""; });
  // gemeldet wird die SPAETERE der beiden Zeilen, hier DHWTemp
  gegenprobe("DHWTemp", "Name doppelt (wie SET1 Heatpump)", SETPRUEF_NAME,
             [](SetCommand &c) { c.name = "Heatpump"; });
  gegenprobe("DHWTemp", "Nummer doppelt (wie SET10)", SETPRUEF_NAME, [](SetCommand &c) { c.number = 10; });
  gegenprobe("Heatpump", "Position 3 (Kopf des Telegramms)", SETPRUEF_POSITION, [](SetCommand &c) { c.pos = 3; });
  gegenprobe("Heatpump", "Position 110 (hinter dem Puffer)", SETPRUEF_POSITION,
             [](SetCommand &c) { c.pos = QUERYSIZE; });
  gegenprobe("DHWTemp", "min groesser als max", SETPRUEF_BEREICH, [](SetCommand &c) { c.min = 76; });
  gegenprobe("DHWTemp", "Bereich ueber 256 Werte", SETPRUEF_BEREICH, [](SetCommand &c) { c.max = c.min + 300; });
  gegenprobe("OperationMode", "Bereich bis 7 (opModeBytes hat 7 Eintraege)", SETPRUEF_OPMODE,
             [](SetCommand &c) { c.max = 7; });
  gegenprobe("OperationMode", "Bereich ab -1", SETPRUEF_OPMODE, [](SetCommand &c) { c.min = -1; });
  gegenprobe("Z1HeatRequestTemperature", "Rohwert ueber 255 (Parameter 200)", SETPRUEF_ROHWERT,
             [](SetCommand &c) { c.param = 200; });
  gegenprobe("Z1HeatRequestTemperature", "Rohwert unter 0 (Parameter 0 bei min -5)", SETPRUEF_ROHWERT,
             [](SetCommand &c) { c.param = 0; });
  // ForceHeater (Byte 5, Maske 0x0C) mit der Maske von HolidayMode (0x30) ueberlappt
  gegenprobe("ForceHeater", "Maske 0x3C ueberlappt HolidayMode auf Byte 5", SETPRUEF_UEBERLAPP,
             [](SetCommand &c) { c.mask = 0x3C; });
  // die Ausnahme auf Byte 7 ist genau eine: beide 0xFF. Mit 0x0F kein Freibrief.
  gegenprobe("PowerfulMode", "Byte 7 mit Maske 0x7F statt 0xFF: Ausnahme gilt nicht", SETPRUEF_UEBERLAPP,
             [](SetCommand &c) { c.mask = 0x7F; });
  {
    const SetPruefung p = setcommands_pruefen(nullptr, 5);
    pruefe(p.regel == SETPRUEF_NAME, "nullptr als Tabelle wird abgelehnt statt dereferenziert");
  }

  // ---------------------------------------------------------------------
  printf("\n== Fall 3: Stichproben der Kodierung (Werte aus Doku und Messungen) ==\n");
  // Die Erwartungen sind von Hand aus MQTT-Topics.md und den Tabellenkommentaren
  // uebernommen. Aus der Tabelle gerechnet waeren sie kein Gegenbeleg.
  pruefe_byte(roh_von("Heatpump", 0), 1, "SET1  Heatpump 0 (aus) -> 1");
  pruefe_byte(roh_von("Heatpump", 1), 2, "SET1  Heatpump 1 (an) -> 2");
  pruefe_byte(roh_von("HolidayMode", 1), 32, "SET2  HolidayMode 1 -> 32");
  pruefe_byte(roh_von("QuietMode", 3), 32, "SET3  QuietMode 3 -> (3+1)*8 = 32");
  pruefe_byte(roh_von("PowerfulMode", 1), 74, "SET4  PowerfulMode 1 (30 min) -> 74");
  pruefe_byte(roh_von("Z1HeatRequestTemperature", 35), 163, "SET5  Z1HeatRequestTemperature 35 -> 163");
  pruefe_byte(roh_von("Z1CoolRequestTemperature", 18), 146, "SET6  Z1CoolRequestTemperature 18 -> 146");
  pruefe_byte(roh_von("OperationMode", 4), 34, "SET9  OperationMode 4 (Heat+DHW) -> 34");
  pruefe_byte(roh_von("OperationMode", 6), 40, "SET9  OperationMode 6 (Auto+DHW) -> 40");
  pruefe_byte(roh_von("ForceDHW", 0), 64, "SET10 ForceDHW 0 -> 64");
  pruefe_byte(roh_von("DHWTemp", 48), 176, "SET11 DHWTemp 48 -> 176");
  pruefe_byte(roh_von("ForceDefrost", 1), 2, "SET12 ForceDefrost 1 -> 2");
  pruefe_byte(roh_von("WaterPump", 2), 48, "SET14 WaterPump 2 (airpurge) -> 48");
  pruefe_byte(roh_von("WaterPumpSpeed", 125), 126, "SET15 WaterPumpSpeed 125 -> 126");
  pruefe_byte(roh_von("HeatingMode", 1), 2, "SET35 HeatingMode 1 (Direktvorgabe) -> 2");
  pruefe_byte(roh_von("CoolingMode", 1), 8, "SET36 CoolingMode 1 -> 8");
  pruefe_byte(roh_von("RoomHeaterState", 1), 2, "SET37 RoomHeaterState 1 (frei) -> 2");
  pruefe_byte(roh_von("DHWHeaterState", 1), 8, "SET38 DHWHeaterState 1 (frei) -> 8");
  pruefe_byte(roh_von("ForceHeater", 1), 8, "SET39 ForceHeater 1 (an) -> 8");
  pruefe_byte(roh_von("AltExternalSensor", 0), 0x10, "SET40 AltExternalSensor 0 -> 0x10");
  pruefe_byte(roh_von("AltExternalSensor", 1), 0x20, "SET40 AltExternalSensor 1 -> 0x20");
  pruefe(setcommand_finden("GibtEsNicht") == nullptr, "unbekannter Name -> nullptr");
  pruefe(setcommand_finden(nullptr) == nullptr, "nullptr als Name -> nullptr");
  pruefe(setcommand_finden("heatpump") == nullptr, "Gross-/Kleinschreibung zaehlt (kein Treffer fuer \"heatpump\")");

  // ---------------------------------------------------------------------
  printf("\n== Fall 4: anwenden() und die leere Vorlage ==\n");
  {
    uint8_t befehl[QUERYSIZE];
    uint8_t belegt[QUERYSIZE];
    kommando_zuruecksetzen(befehl, belegt);

    // Byte 4: drei Felder, jedes mit eigener Maske - der gemeldete Fehler aus 3.1.0
    const SetCommand *hp = setcommand_finden("Heatpump");
    const SetCommand *wp = setcommand_finden("WaterPump");
    const SetCommand *dhw = setcommand_finden("ForceDHW");
    pruefe(setcommand_anwenden(*hp, 1, befehl, belegt) == SETANWENDEN_OK, "Heatpump 1: kein Konflikt");
    pruefe(setcommand_anwenden(*wp, 0, befehl, belegt) == SETANWENDEN_OK, "WaterPump 0: kein Konflikt (andere Bits)");
    pruefe(setcommand_anwenden(*dhw, 0, befehl, belegt) == SETANWENDEN_OK, "ForceDHW 0: kein Konflikt (andere Bits)");
    pruefe_byte(befehl[4], 0x52, "Byte 4 = Heatpump 2 | WaterPump 16 | ForceDHW 64");
    pruefe_byte(belegt[4], 0xF3, "belegt[4] merkt alle drei Masken (0x03 | 0x30 | 0xC0)");

    // dasselbe Feld nochmal: ueberschreibt, ver-ODERt nicht - und meldet es
    pruefe(setcommand_anwenden(*hp, 0, befehl, belegt) == SETANWENDEN_KONFLIKT,
           "Heatpump zum zweiten Mal: Konflikt gemeldet");
    pruefe_byte(befehl[4], 0x51, "Heatpump 0 ueberschreibt (1 statt 2|1 = 3), Nachbarn bleiben");

    // Byte 7: das dokumentierte Paar - der Letzte gewinnt, sichtbar als Konflikt
    uint8_t b7[QUERYSIZE], u7[QUERYSIZE];
    kommando_zuruecksetzen(b7, u7);
    pruefe(setcommand_anwenden(*setcommand_finden("QuietMode"), 2, b7, u7) == SETANWENDEN_OK, "QuietMode 2: kein Konflikt");
    pruefe_byte(b7[7], 24, "Byte 7 = (2+1)*8 = 24");
    pruefe(setcommand_anwenden(*setcommand_finden("PowerfulMode"), 1, b7, u7) == SETANWENDEN_KONFLIKT,
           "PowerfulMode danach: Konflikt (Protokollmerkmal, nicht Fehler)");
    pruefe_byte(b7[7], 74, "Byte 7 = 74: der Letzte gewinnt");

    // Unfug aendert nichts
    uint8_t vorher[QUERYSIZE];
    kommando_zuruecksetzen(befehl, belegt);
    memcpy(vorher, befehl, sizeof vorher);
    pruefe(setcommand_anwenden(*hp, 2, befehl, belegt) == SETANWENDEN_ABGELEHNT, "Wert ueber max: abgelehnt");
    pruefe(setcommand_anwenden(*hp, -1, befehl, belegt) == SETANWENDEN_ABGELEHNT, "Wert unter min: abgelehnt");
    pruefe(setcommand_anwenden(*hp, 1, nullptr, belegt) == SETANWENDEN_ABGELEHNT, "kein Befehlspuffer: abgelehnt");
    pruefe(setcommand_anwenden(*hp, 1, befehl, nullptr) == SETANWENDEN_ABGELEHNT, "keine Belegung: abgelehnt");
    SetCommand ausserhalb = *hp;
    ausserhalb.pos = QUERYSIZE;
    pruefe(setcommand_anwenden(ausserhalb, 1, befehl, belegt) == SETANWENDEN_ABGELEHNT,
           "pos hinter dem Puffer: abgelehnt, nichts geschrieben");
    pruefe(memcmp(vorher, befehl, sizeof vorher) == 0, "Puffer nach allen abgelehnten Aufrufen unveraendert");
    uint8_t leer[QUERYSIZE] = {0};
    pruefe(memcmp(leer, belegt, sizeof leer) == 0, "Belegung nach allen abgelehnten Aufrufen unveraendert");

    // leere Vorlage: Kopf F1 6C 01 10, sonst nichts
    kommando_zuruecksetzen(befehl, belegt);
    pruefe_byte(befehl[0], 0xF1, "Vorlage Byte 0 = 0xF1");
    pruefe_byte(befehl[1], 0x6C, "Vorlage Byte 1 = 0x6C");
    pruefe_byte(befehl[2], 0x01, "Vorlage Byte 2 = 0x01");
    pruefe_byte(befehl[3], 0x10, "Vorlage Byte 3 = 0x10");
    bool rest_leer = true;
    for (int i = 4; i < QUERYSIZE; i++)
      if (befehl[i] != 0 || belegt[i] != 0)
        rest_leer = false;
    pruefe(rest_leer, "Bytes 4..109 und die ganze Belegung sind 0");
    pruefe(sizeof(KOMMANDO_VORLAGE) == 110, "Vorlage ist 110 Bytes lang");
    pruefe(!kommando_zuruecksetzen(nullptr, belegt) && !kommando_zuruecksetzen(befehl, nullptr),
           "kommando_zuruecksetzen(nullptr) -> false, ohne zu schreiben");
  }

  // ---------------------------------------------------------------------
  printf("\n== Fall 5: Notbetriebs-Tabellen gegen setCommands[] ==\n");
  {
    // gehaltene Werte: jeder Name muss ein Set-Kommando sein
    bool namen_ok = true;
    for (unsigned i = 0; i < NOTBETRIEB_ANZAHL_HEIZEN; i++)
      if (!setcommand_finden(NOTBETRIEB_WERTE_HEIZEN[i]))
        namen_ok = false;
    for (unsigned i = 0; i < NOTBETRIEB_ANZAHL_WASSER; i++)
      if (!setcommand_finden(NOTBETRIEB_WERTE_WASSER[i]))
        namen_ok = false;
    pruefe(namen_ok, "jeder gehaltene Wert (Heizen, Wasser) ist ein Set-Kommando");

    // Schritte: SET-Schritte nennen ein Set-Kommando, der Festwert liegt im
    // Bereich; ein Index zeigt auf einen gehaltenen Wert der Rolle
    struct Folge
    {
      const char *name;
      const NotbetriebSchritt *schritte;
      unsigned n;
      unsigned anzahl_werte;
    };
    const Folge folgen[] = {
        {"Heizen", NOTBETRIEB_SCHRITTE_HEIZEN, NOTBETRIEB_SCHRITTE_HEIZEN_N, NOTBETRIEB_ANZAHL_HEIZEN},
        {"Wasser", NOTBETRIEB_SCHRITTE_WASSER, NOTBETRIEB_SCHRITTE_WASSER_N, NOTBETRIEB_ANZAHL_WASSER}};
    for (const Folge &f : folgen)
    {
      bool ok = true;
      unsigned set_schritte = 0;
      for (unsigned i = 0; i < f.n; i++)
      {
        const NotbetriebSchritt &s = f.schritte[i];
        if (s.typ != NB_SCHRITT_SET)
          continue; // Hydraulik und Vorderhaus sind keine Set-Kommandos
        set_schritte++;
        const SetCommand *c = setcommand_finden(s.set_name);
        if (!c)
        {
          printf("    Schritt %u (%s): kein Set-Kommando\n", i, s.set_name);
          ok = false;
          continue;
        }
        if (s.wert_index == NOTBETRIEB_FESTER_WERT)
        {
          if (!setcommand_wert_ok(*c, s.fester_wert))
          {
            printf("    Schritt %u (%s): Festwert %d ausserhalb [%d..%d]\n", i, s.set_name, s.fester_wert, c->min, c->max);
            ok = false;
          }
        }
        else if (s.wert_index < 0 || (unsigned)s.wert_index >= f.anzahl_werte)
        {
          printf("    Schritt %u (%s): wert_index %d ausserhalb der gehaltenen Werte\n", i, s.set_name, s.wert_index);
          ok = false;
        }
      }
      char text[120];
      snprintf(text, sizeof text, "Folge %s: %u Set-Schritte, Namen und Festwerte stimmen mit setCommands[] ueberein", f.name,
               set_schritte);
      pruefe(ok && set_schritte > 0, text);
    }
  }

  printf("\n%s (%d Fehler)\n", fehler == 0 ? "ALLE ZUSICHERUNGEN ERFUELLT" : "FEHLGESCHLAGEN", fehler);
  return (fehler == 0) ? 0 : 1;
}
