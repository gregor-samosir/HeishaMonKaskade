// Nachweis fuer die vier Ist-Zustands-Topics aus Byte 110 (TOP99-TOP102, 3.7.0).
//
// Geprueft wird der Code, der auch auf dem Geraet laeuft: src/decode.cpp wird
// mituebersetzt und ueber getTopicPayload() aufgerufen - es gibt keine
// Nachbildung der Bitzuordnung, die auseinanderlaufen koennte. Die
// Arduino-Header kommen als Ersatz aus test/stubs/.
//
// Vier Fragen beantwortet dieser Test:
//  1. Stehen die vier Topics mit den erwarteten Nummern auf Byte 110?
//  2. Liest jedes Topic wirklich SEINE zwei Bits - ueber alle 256 Rohwerte?
//  3. Stimmen die Klartexte fuer die an WP1 belegten Zustaende?
//  4. Bleibt die Web-Tabelle im Array, auch wenn ein Feld b11 liefert?
//     (Das ist der Grund fuer das dritte Element "unknown" in den beiden
//     desc-Arrays.)
//  5. Dasselbe fuer die GANZE Tabelle (Fall 6, seit 3.9.0): jede Zeile, alle
//     256 Rohwerte, der Index muss in seiner Liste liegen. Dazu die
//     Gegenprobe, dass eine zu kurze Liste auffaellt.
//
// Bauen und ausfuehren:
//   c++ -std=c++17 -O2 -Wall -I test/stubs -I src -o /tmp/byte110_test \
//       test/byte110_test.cpp src/decode.cpp src/Topics.cpp
//   /tmp/byte110_test            (Rueckgabewert != 0 = Test fehlgeschlagen)

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

#include "decode.h"

// decode.cpp loggt beim Publizieren - hier nicht gebraucht
void write_telnet_log(char *) {}
void write_mqtt_log(char *) {}

static int fehler = 0;

// eine Zusicherung mit Klartext, damit der CI-Log ohne Debugger lesbar ist
static void pruefe(bool bedingung, const char *was)
{
  printf("  [%s] %s\n", bedingung ? "ok " : "FEHLER", was);
  if (!bedingung)
    fehler++;
}

// Tabellenzeile ueber den Topic-Namen finden. Der Index IST nicht die
// TOP-Nummer (siehe Kommentar ueber stateTopics[]), deshalb gesucht statt
// gerechnet.
static int zeile_von(const char *name)
{
  for (unsigned int i = 0; i < NUMBEROFTOPICS; i++)
    if (strcmp(stateTopics[i].name, name) == 0)
      return (int)i;
  return -1;
}

// Ein Telegramm mit einem bestimmten Wert auf Byte 110 dekodieren.
// Der Puffer ist so gross wie serial_data in HeishaMon.cpp.
static int wert_bei(int zeile, uint8_t byte110)
{
  uint8_t telegramm[256];
  memset(telegramm, 0, sizeof telegramm);
  telegramm[110] = byte110;
  char out[MAXVALUELEN];
  getTopicPayload((unsigned int)zeile, telegramm, out);
  return atoi(out);
}

// Die Anzeigelogik ist KEINE Nachbildung mehr: desc_text() aus decode.h ist
// dieselbe Funktion, die webfunctions.cpp benutzt. Bis 3.8.2 stand hier ein
// Nachbau, der nur nach unten prueft - genau wie die Firmware damals.
static const char *anzeigetext(const StateTopic &topic, int wert)
{
  return desc_text(topic.desc, wert);
}

// Laenge einer desc-Liste ueber ihren nullptr-Abschluss bestimmen. Ohne
// Abschluss liefe das Zaehlen ins Leere, deshalb der Deckel: -1 heisst
// "kein Abschluss innerhalb von DESC_MAX_ENTRIES gefunden" und ist ein Fehler.
static int laenge_von(const char *const *desc)
{
  if (desc == nullptr)
    return -1;
  for (int i = 0; i < DESC_MAX_ENTRIES; i++)
    if (desc[i] == nullptr)
      return i;
  return -1;
}

// Ein Telegramm mit einem Rohwert auf dem Quellbyte EINER BELIEBIGEN Zeile
// dekodieren (wert_bei() weiter oben kann nur Byte 110).
static int wert_der_zeile(unsigned int zeile, uint8_t roh)
{
  uint8_t telegramm[256];
  memset(telegramm, 0, sizeof telegramm);
  telegramm[stateTopics[zeile].pos] = roh;
  char out[MAXVALUELEN];
  getTopicPayload(zeile, telegramm, out);
  return atoi(out);
}

// Die vier Topics samt der Bitgruppe, die sie laut ProtocolByteDecrypt.md
// lesen sollen. 'schiebung' ist die Verschiebung nach rechts, mit der die
// erwarteten Bits UNABHAENGIG vom Dekodierer aus dem Rohwert geholt werden.
struct Feld
{
  byte nummer;
  const char *name;
  int schiebung; // Bits 1&2 = 6, 3&4 = 4, 5&6 = 2, 7&8 = 0 (MSB zuerst)
  const char *aus_text;
  const char *an_text;
};

static const Feld felder[] = {
    { 99, "Quiet_Mode_Active",    6, "Off",  "On"},
    {100, "Powerful_Mode_Active", 4, "Off",  "On"},
    {101, "Heat_Cool_SW_State",   2, "Heat", "Cool"},
    {102, "External_SW_State",    0, "Off",  "On"},
};
static const int ANZAHL_FELDER = (int)(sizeof(felder) / sizeof(felder[0]));

int main()
{
  char text[128];

  printf("== Fall 1: die vier Zeilen stehen in der Tabelle, Quellbyte 110 ==\n");
  int zeilen[ANZAHL_FELDER];
  for (int f = 0; f < ANZAHL_FELDER; f++)
  {
    zeilen[f] = zeile_von(felder[f].name);
    snprintf(text, sizeof text, "%s vorhanden", felder[f].name);
    pruefe(zeilen[f] >= 0, text);
    if (zeilen[f] < 0)
    {
      printf("\nFEHLGESCHLAGEN - ohne die Tabellenzeilen sind die weiteren Faelle sinnlos\n");
      return 1;
    }
    const StateTopic &t = stateTopics[zeilen[f]];
    snprintf(text, sizeof text, "%s ist TOP%u auf Byte %u (erwartet TOP%u/110)",
             t.name, t.number, t.pos, felder[f].nummer);
    pruefe(t.number == felder[f].nummer && t.pos == 110, text);
    snprintf(text, sizeof text, "%s hat einen 1-Byte-Dekodierer und keinen Mehrbyte-Dekodierer", t.name);
    pruefe(t.decode != nullptr && t.wide == nullptr, text);
  }

  printf("\n== Fall 2: jedes Topic liest genau seine zwei Bits (alle 256 Rohwerte) ==\n");
  // Die Erwartung wird hier unabhaengig vom Dekodierer gerechnet: Rohwert der
  // Bitgruppe minus 1, wie ueberall im Protokoll (b01 = 0, b10 = 1).
  // Deckt damit auch die Verwechslung zweier Bitgruppen auf - genau der
  // Fehler, der bei vier Topics auf EINEM Byte naheliegt.
  for (int f = 0; f < ANZAHL_FELDER; f++)
  {
    int abweichungen = 0;
    for (int roh = 0; roh < 256; roh++)
    {
      int erwartet = ((roh >> felder[f].schiebung) & 0b11) - 1;
      if (wert_bei(zeilen[f], (uint8_t)roh) != erwartet)
        abweichungen++;
    }
    snprintf(text, sizeof text, "%-21s 256 Rohwerte, %d Abweichungen",
             felder[f].name, abweichungen);
    pruefe(abweichungen == 0, text);
  }

  printf("\n== Fall 3: die an WP1 belegten Zustaende (2026-08-15/16) ==\n");
  {
    // 0x55 = b01 01 01 01: alles aus, Heizbetrieb. Dieser Wert steht auch im
    // Antwortbeispiel aus ProtocolByteDecrypt.md an Byte 110.
    const char *soll_55[] = {"Off", "Off", "Heat", "Off"};
    // 0x95 = b10 01 01 01: Quiet an (an allen drei Stufen gleich - das Feld
    // meldet nur AN/AUS, die Stufe steht in TOP18)
    const char *soll_95[] = {"On", "Off", "Heat", "Off"};
    // 0x59 = b01 01 10 01: Kuehlbetrieb
    const char *soll_59[] = {"Off", "Off", "Cool", "Off"};
    // 0x69 = b01 10 10 01: Powerful aktiv im Kuehlbetrieb. Am 2026-08-16 an
    // der laufenden 3.7.0 belegt ist der Feldwert - nach set/PowerfulMode 1
    // meldeten TOP17 (30 min) und TOP100 (On) gemeinsam; das Rohbyte steht im
    // Log nicht, es ist hier aus den vier Feldwerten zusammengesetzt.
    const char *soll_69[] = {"Off", "On", "Cool", "Off"};
    struct { uint8_t roh; const char **soll; const char *was; } faelle[] = {
        {0x55, soll_55, "0x55 Grundzustand: alles aus, Heizen"},
        {0x95, soll_95, "0x95 Quiet aktiv"},
        {0x59, soll_59, "0x59 Kuehlbetrieb"},
        {0x69, soll_69, "0x69 Powerful aktiv beim Kuehlen"},
    };
    for (auto &fall : faelle)
    {
      for (int f = 0; f < ANZAHL_FELDER; f++)
      {
        const StateTopic &t = stateTopics[zeilen[f]];
        const char *ist = anzeigetext(t, wert_bei(zeilen[f], fall.roh));
        snprintf(text, sizeof text, "%-32s %-21s = %-6s (erwartet %s)",
                 fall.was, t.name, ist, fall.soll[f]);
        pruefe(strcmp(ist, fall.soll[f]) == 0, text);
      }
    }
  }

  printf("\n== Fall 4: die Web-Tabelle bleibt im Array, auch bei b11 ==\n");
  // b00 gibt es im Protokoll nicht (der Dekodierer liefert dafuer -1, die
  // Anzeige bleibt leer), b11 ist unbeobachtet, aber moeglich - und ergibt
  // Index 2. Ein zweielementiges desc-Array wuerde hier hinter sein Ende
  // lesen; die Anzeige stuende dann auf einem beliebigen Zeiger.
  for (int f = 0; f < ANZAHL_FELDER; f++)
  {
    const StateTopic &t = stateTopics[zeilen[f]];
    int hoechster = -1;
    for (int roh = 0; roh < 256; roh++)
    {
      int wert = wert_bei(zeilen[f], (uint8_t)roh);
      if (wert > hoechster)
        hoechster = wert;
      if (wert < -1 || wert > 2)
      {
        snprintf(text, sizeof text, "%s: Rohwert %d ergibt Index %d - ausserhalb -1..2",
                 t.name, roh, wert);
        pruefe(false, text);
        break;
      }
    }
    snprintf(text, sizeof text, "%-21s hoechster Index %d, alle Werte in -1..2",
             t.name, hoechster);
    pruefe(hoechster == 2, text);

    // Der Index 2 muss im Array wirklich belegt sein. Nachweisbar ist das nur
    // ueber den Inhalt - die Laenge fuehrt das struct nicht mit.
    snprintf(text, sizeof text, "%-21s desc[2] = \"%s\" (erwartet \"unknown\")",
             t.name, t.desc[2]);
    pruefe(t.desc[2] != nullptr && strcmp(t.desc[2], "unknown") == 0, text);

    // b00 -> -1 -> leere Anzeige statt Zugriff mit negativem Index
    snprintf(text, sizeof text, "%-21s b00 ergibt eine leere Anzeige", t.name);
    pruefe(strcmp(anzeigetext(t, wert_bei(zeilen[f], 0x00)), "") == 0, text);

    // die beiden belegten Zustaende tragen die erwarteten Texte
    snprintf(text, sizeof text, "%-21s desc[0]/desc[1] = \"%s\"/\"%s\" (erwartet \"%s\"/\"%s\")",
             t.name, t.desc[0], t.desc[1], felder[f].aus_text, felder[f].an_text);
    pruefe(strcmp(t.desc[0], felder[f].aus_text) == 0 && strcmp(t.desc[1], felder[f].an_text) == 0, text);
  }

  printf("\n== Fall 5: die Tabelle als Ganzes bleibt widerspruchsfrei ==\n");
  // Kein Byte-110-Thema, aber vier neue Zeilen sind der Anlass: doppelte
  // TOP-Nummern oder doppelte Namen faellt kein Compiler auf - der ioBroker
  // bekaeme still zwei Werte auf demselben Topic.
  {
    int doppelte_nummern = 0, doppelte_namen = 0, ohne_dekodierer = 0, ohne_desc = 0;
    for (unsigned int i = 0; i < NUMBEROFTOPICS; i++)
    {
      const StateTopic &a = stateTopics[i];
      for (unsigned int j = i + 1; j < NUMBEROFTOPICS; j++)
      {
        if (a.number == stateTopics[j].number)
          doppelte_nummern++;
        if (strcmp(a.name, stateTopics[j].name) == 0)
          doppelte_namen++;
      }
      // Mindestens einer muss da sein - eine Zeile ohne Dekodierer liefert
      // stumm "-1" (getTopicPayload). Beide zusammen sind erlaubt und bei
      // TOP5/TOP6 Absicht: der Mehrbyte-Dekodierer haengt dort nur die
      // Nachkommastelle an den 1-Byte-Wert an (appendFraction).
      if (a.decode == nullptr && a.wide == nullptr)
        ohne_dekodierer++;
      if (a.desc == nullptr || a.name == nullptr)
        ohne_desc++;
    }
    snprintf(text, sizeof text, "%u Zeilen, keine doppelte TOP-Nummer (%d gefunden)",
             NUMBEROFTOPICS, doppelte_nummern);
    pruefe(doppelte_nummern == 0, text);
    snprintf(text, sizeof text, "keine doppelten Topic-Namen (%d gefunden)", doppelte_namen);
    pruefe(doppelte_namen == 0, text);
    snprintf(text, sizeof text, "jede Zeile hat mindestens einen Dekodierer (%d Ausreisser)", ohne_dekodierer);
    pruefe(ohne_dekodierer == 0, text);
    snprintf(text, sizeof text, "jede Zeile hat Name und Einheit (%d Ausreisser)", ohne_desc);
    pruefe(ohne_desc == 0, text);
  }

  printf("\n== Fall 6: kein Anzeigeindex verlaesst seine Liste (ganze Tabelle) ==\n");
  // Fall 4 prueft das fuer die vier Byte-110-Zeilen. Hier gilt dieselbe Frage
  // fuer JEDE Zeile der Tabelle: alle 256 Rohwerte durch den ECHTEN Dekodierer
  // schicken und verlangen, dass der entstehende Index in seiner Liste liegt.
  // Bis 3.8.2 war das fuer die meisten Zeilen NICHT erfuellt - ein Rohwert b11
  // ergab Index 2 in einer zweielementigen Liste.
  {
    int ohne_abschluss = 0, ausserhalb = 0, ohne_text = 0, klartextzeilen = 0;
    for (unsigned int i = 0; i < NUMBEROFTOPICS; i++)
    {
      const StateTopic &t = stateTopics[i];
      int laenge = laenge_von(t.desc);
      if (laenge < 1)
      {
        snprintf(text, sizeof text, "%-24s desc-Liste ohne nullptr-Abschluss", t.name);
        pruefe(false, text);
        ohne_abschluss++;
        continue;
      }
      // Einheiten-Zeilen ({"value", "&deg;C"}) werden nicht ueber den Wert
      // indiziert - die Weboberflaeche nimmt dort immer desc[1]
      if (strcmp(t.desc[0], "value") == 0)
        continue;
      klartextzeilen++;

      int hoechster = -1;
      for (int roh = 0; roh < 256; roh++)
      {
        int wert = wert_der_zeile(i, (uint8_t)roh);
        if (wert > hoechster)
          hoechster = wert;
        if (wert >= laenge)
        {
          snprintf(text, sizeof text, "%-24s Rohwert 0x%02X ergibt Index %d, Liste hat %d",
                   t.name, roh, wert, laenge);
          pruefe(false, text);
          ausserhalb++;
          break;
        }
        // Ein gedeckter Index muss auch wirklich einen Text liefern -
        // "unknown" zaehlt, eine leere Zelle waere eine Luecke
        if (wert >= 0 && anzeigetext(t, wert)[0] == '\0')
        {
          snprintf(text, sizeof text, "%-24s Index %d liefert keinen Text", t.name, wert);
          pruefe(false, text);
          ohne_text++;
          break;
        }
      }
      snprintf(text, sizeof text, "%-24s hoechster Index %d, Liste %d Eintraege",
               t.name, hoechster, laenge);
      pruefe(hoechster < laenge, text);
    }
    snprintf(text, sizeof text, "alle %u Zeilen mit nullptr abgeschlossen (%d ohne)",
             NUMBEROFTOPICS, ohne_abschluss);
    pruefe(ohne_abschluss == 0, text);
    snprintf(text, sizeof text, "%d Klartext-Zeilen geprueft, keine ausserhalb ihrer Liste (%d)",
             klartextzeilen, ausserhalb);
    pruefe(ausserhalb == 0, text);
    snprintf(text, sizeof text, "kein gedeckter Index ohne Text (%d)", ohne_text);
    pruefe(ohne_text == 0, text);
  }

  printf("\n== Gegenprobe: eine zu kurze Liste faellt auf ==\n");
  // Der Plan verlangt sie ausdruecklich: mit einem bewusst zu kurzen Array
  // muss der Test brechen. Statt eine echte Liste zu kuerzen (das wuerde die
  // Firmware veraendern) wird hier eine zu kurze Liste gebaut und gezeigt,
  // dass beide Haelften der Zusicherung greifen - der Nachschlag liest nicht
  // hinaus, und die Pruefung aus Fall 6 wuerde sie verwerfen.
  {
    static const char *zuKurz[] = {"Off", "On", nullptr}; // ein 2-Bit-Feld braucht drei
    pruefe(laenge_von(zuKurz) == 2, "Laenge der zu kurzen Liste wird als 2 erkannt");
    pruefe(strcmp(desc_text(zuKurz, 1), "On") == 0, "gueltiger Index liefert weiter seinen Text");
    pruefe(strcmp(desc_text(zuKurz, 2), "") == 0, "Index 2 (b11) liefert leer statt Fremdspeicher");
    pruefe(strcmp(desc_text(zuKurz, 99), "") == 0, "Index weit hinter dem Ende liefert leer");
    pruefe(strcmp(desc_text(zuKurz, -1), "") == 0, "negativer Index liefert weiter leer");
    // Genau diese Bedingung prueft Fall 6 fuer jede Zeile: hoechster < laenge.
    // Mit einem 2-Bit-Dekodierer (hoechster Index 2) und dieser Liste ist sie
    // verletzt - der Test wuerde also anschlagen.
    pruefe(!(2 < laenge_von(zuKurz)), "Fall 6 wuerde diese Liste an einem 2-Bit-Feld verwerfen");
    static const char *ohneAbschluss = nullptr;
    pruefe(laenge_von(&ohneAbschluss) == 0, "eine leere Liste hat Laenge 0");
    pruefe(strcmp(desc_text(nullptr, 0), "") == 0, "desc == nullptr liefert leer, kein Absturz");
  }

  printf("\n%s (%d Fehler)\n", fehler == 0 ? "ALLE ZUSICHERUNGEN ERFUELLT" : "FEHLGESCHLAGEN", fehler);
  return (fehler == 0) ? 0 : 1;
}
