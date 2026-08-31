// Nachweis fuer die sieben Installer-Topics aus Byte 25 und Byte 23
// (TOP105-TOP111, 3.19.0).
//
// Geprueft wird der Code, der auch auf dem Geraet laeuft: src/decode.cpp wird
// mituebersetzt und ueber getTopicPayload() aufgerufen - es gibt keine
// Nachbildung der Bitzuordnung, die auseinanderlaufen koennte. Die
// Arduino-Header kommen als Ersatz aus test/stubs/.
//
// Warum diese Bytes ueberhaupt Topics sind, steht in decode.cpp ueber den
// Zeilen und ausfuehrlich in MQTT-Topics.md. Kurz: Am 2026-08-31 stand der
// Speicher-Heizstab an WP2 auf EXTERN, obwohl es nur den internen gibt. Die
// Anlage ging beim ersten Warmwasserlauf mit H91 aus.
//
// Vier Fragen beantwortet dieser Test:
//  1. Stehen die sieben Topics mit den erwarteten Nummern auf den erwarteten
//     Bytes? (Ein vertauschtes Quellbyte faellt sonst erst an der Anlage auf.)
//  2. Liest jedes Topic wirklich SEINE zwei Bits - ueber alle 256 Rohwerte?
//     Das ist die eigentliche Absicherung: Byte 25 traegt drei Topics, Byte 23
//     vier, und ein Dekodierer zu weit rechts oder links liefert Werte, die
//     plausibel aussehen und trotzdem falsch sind.
//  3. Ergeben die AM 2026-08-31 GEMESSENEN Rohwerte die dokumentierten
//     Klartexte? 0x95/0x96 auf Byte 25 und 0x99/0x59 auf Byte 23 sind keine
//     Rechenbeispiele, sondern die Bytes aus test/h2.log und test/h2_ext.log.
//     Dazu 0x9E/0x96 von WP1: die Heizstab-Leistung stand dort auf 9 kW an
//     einem Geraet, das es nur mit 3 kW gibt.
//  4. Bleibt die Web-Tabelle im Array, wenn ein Feld b11 oder b00 liefert?
//
// Bauen und ausfuehren:
//   ./test/decode_hosttest.sh test/byte23_25_test.cpp
//   (Rueckgabewert != 0 = Test fehlgeschlagen)

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

// Ein Telegramm mit einem Rohwert auf dem Quellbyte der Zeile dekodieren.
// Der Puffer ist so gross wie serial_data in HeishaMon.cpp.
static int wert_der_zeile(int zeile, uint8_t roh)
{
  uint8_t telegramm[256];
  memset(telegramm, 0, sizeof telegramm);
  telegramm[stateTopics[zeile].pos] = roh;
  char out[MAXVALUELEN];
  getTopicPayload((unsigned int)zeile, telegramm, out);
  return atoi(out);
}

// Klartext wie in der Weboberflaeche: dieselbe desc_text() aus decode.h
static const char *text_der_zeile(int zeile, uint8_t roh)
{
  return desc_text(stateTopics[zeile].desc, wert_der_zeile(zeile, roh));
}

// Die sieben neuen Zeilen mit ihrer Sollzuordnung. 'schiebe' ist die Anzahl
// Bits, um die das Feld nach rechts liegt: 6 = oberstes Bitpaar (getBit1and2),
// 0 = unterstes (getBit7and8).
struct Sollzeile
{
  const char *name;
  int nummer;
  int byte;
  int schiebe;
};

static const Sollzeile soll[] = {
    {"Pad_Heater_Type", 105, 25, 4},
    {"Internal_Heater_Power", 106, 25, 2},
    {"DHW_Heater_Type", 107, 25, 0},
    {"External_Compressor_Config", 108, 23, 6},
    {"External_Error_Signal_Config", 109, 23, 4},
    {"Heat_Cool_SW_Config", 110, 23, 2},
    {"External_Control_Config", 111, 23, 0},
};
static const int SOLLZEILEN = (int)(sizeof soll / sizeof soll[0]);

int main()
{
  printf("Fall 1: die sieben Topics stehen auf den erwarteten Bytes\n");
  for (int i = 0; i < SOLLZEILEN; i++)
  {
    char was[128];
    int z = zeile_von(soll[i].name);
    snprintf(was, sizeof was, "%s ist in der Tabelle", soll[i].name);
    pruefe(z >= 0, was);
    if (z < 0)
      continue;

    snprintf(was, sizeof was, "%s hat Nummer %d", soll[i].name, soll[i].nummer);
    pruefe(stateTopics[z].number == soll[i].nummer, was);

    snprintf(was, sizeof was, "%s liest Byte %d", soll[i].name, soll[i].byte);
    pruefe(stateTopics[z].pos == soll[i].byte, was);
  }

  printf("\nFall 2: jedes Topic liest genau seine zwei Bits (alle 256 Rohwerte)\n");
  for (int i = 0; i < SOLLZEILEN; i++)
  {
    int z = zeile_von(soll[i].name);
    if (z < 0)
      continue;

    bool alle = true;
    for (int roh = 0; roh <= 255 && alle; roh++)
    {
      int erwartet = ((roh >> soll[i].schiebe) & 0b11) - 1;
      if (wert_der_zeile(z, (uint8_t)roh) != erwartet)
        alle = false;
    }
    char was[128];
    snprintf(was, sizeof was, "%s folgt Bits >>%d ueber alle 256 Rohwerte",
             soll[i].name, soll[i].schiebe);
    pruefe(alle, was);
  }

  printf("\nFall 3: die am 2026-08-31 gemessenen Rohwerte ergeben die Klartexte\n");
  // Byte 25 aus test/h2.log: Menuepunkt "Tank heater" Internal <-> External.
  // An WP2 kein Pad-Heater und ein 3-kW-Stab - beides bleibt dabei stehen.
  {
    int pad = zeile_von("Pad_Heater_Type");
    int leistung = zeile_von("Internal_Heater_Power");
    int dhw = zeile_von("DHW_Heater_Type");

    pruefe(strcmp(text_der_zeile(pad, 0x95), "None") == 0,
           "Byte 25 = 0x95 -> Pad_Heater_Type None");
    pruefe(strcmp(text_der_zeile(leistung, 0x95), "3 kW") == 0,
           "Byte 25 = 0x95 -> Internal_Heater_Power 3 kW");
    pruefe(strcmp(text_der_zeile(dhw, 0x95), "Internal") == 0,
           "Byte 25 = 0x95 -> DHW_Heater_Type Internal (Sollstand)");
    pruefe(strcmp(text_der_zeile(dhw, 0x96), "External") == 0,
           "Byte 25 = 0x96 -> DHW_Heater_Type External (der H91-Zustand)");
    // Die beiden anderen Felder duerfen sich beim Umschalten NICHT bewegen -
    // genau das zeigt der Mitschnitt, und genau das muss der Dekodierer halten.
    pruefe(strcmp(text_der_zeile(pad, 0x96), "None") == 0 &&
               strcmp(text_der_zeile(leistung, 0x96), "3 kW") == 0,
           "0x95 -> 0x96 laesst Pad-Heater und Leistung unveraendert");

    // Der dritte Umschaltnachweis, an WP1 statt WP2: Der Menuepunkt
    // "Heater capacity" stand dort auf 9 kW, obwohl es diesen Geraetetyp nur
    // mit 3 kW gibt - ein Wert, den das Bedienteil gar nicht zur Auswahl
    // stellt. Aufrufen und Bestaetigen genuegte, Byte 25 ging 0x9E -> 0x96
    // (2026-08-31). Damit ist auch das mittlere Bitpaar gemessen, das in
    // beiden Mitschnitten stillstand.
    pruefe(strcmp(text_der_zeile(leistung, 0x9E), "9 kW") == 0,
           "Byte 25 = 0x9E -> Internal_Heater_Power 9 kW (Fehlstand WP1)");
    pruefe(strcmp(text_der_zeile(leistung, 0x96), "3 kW") == 0,
           "Byte 25 = 0x96 -> Internal_Heater_Power 3 kW (nach der Korrektur)");
    pruefe(strcmp(text_der_zeile(pad, 0x9E), "None") == 0 &&
               strcmp(text_der_zeile(dhw, 0x9E), "External") == 0,
           "0x9E -> 0x96 bewegt NUR die Heizstab-Leistung");
  }

  // Byte 23 aus test/h2_ext.log: Menuepunkt "External compressor SW" Yes <-> No.
  // 0x99 ist der Sollstand an WP2: Kompressorkontakt und Heat/Cool am KNX
  // angeschlossen, Fehlersignal und externer Steuerkontakt nicht belegt.
  {
    int komp = zeile_von("External_Compressor_Config");
    int fehlersignal = zeile_von("External_Error_Signal_Config");
    int heatcool = zeile_von("Heat_Cool_SW_Config");
    int extern_sw = zeile_von("External_Control_Config");

    pruefe(strcmp(text_der_zeile(komp, 0x99), "Enabled") == 0,
           "Byte 23 = 0x99 -> External_Compressor_Config Enabled");
    pruefe(strcmp(text_der_zeile(fehlersignal, 0x99), "Disabled") == 0,
           "Byte 23 = 0x99 -> External_Error_Signal_Config Disabled");
    pruefe(strcmp(text_der_zeile(heatcool, 0x99), "Enabled") == 0,
           "Byte 23 = 0x99 -> Heat_Cool_SW_Config Enabled");
    pruefe(strcmp(text_der_zeile(extern_sw, 0x99), "Disabled") == 0,
           "Byte 23 = 0x99 -> External_Control_Config Disabled (wie TOP102)");

    // Der Umschaltnachweis: NUR der Kompressorkontakt bewegt sich.
    pruefe(strcmp(text_der_zeile(komp, 0x59), "Disabled") == 0,
           "Byte 23 = 0x59 -> External_Compressor_Config Disabled");
    pruefe(strcmp(text_der_zeile(fehlersignal, 0x59), "Disabled") == 0 &&
               strcmp(text_der_zeile(heatcool, 0x59), "Enabled") == 0 &&
               strcmp(text_der_zeile(extern_sw, 0x59), "Disabled") == 0,
           "0x99 -> 0x59 bewegt NUR den Kompressorkontakt");
  }

  printf("\nFall 4: b00 und b11 laufen nicht aus der desc-Liste\n");
  for (int i = 0; i < SOLLZEILEN; i++)
  {
    int z = zeile_von(soll[i].name);
    if (z < 0)
      continue;

    char was[128];
    // b00 -> Index -1: desc_text() faengt das mit dem Leerstring ab
    uint8_t roh_b00 = (uint8_t)(0x00);
    snprintf(was, sizeof was, "%s: b00 ergibt den Leerstring", soll[i].name);
    pruefe(strcmp(text_der_zeile(z, roh_b00), "") == 0, was);

    // b11 -> Index 2: muss noch IN der Liste liegen, sonst fehlt der dritte
    // Eintrag und die Weboberflaeche liest hinter das Array
    uint8_t roh_b11 = (uint8_t)(0b11 << soll[i].schiebe);
    snprintf(was, sizeof was, "%s: b11 liefert einen Eintrag aus der Liste",
             soll[i].name);
    pruefe(strcmp(text_der_zeile(z, roh_b11), "") != 0, was);
  }

  printf("\n%s (%d Beanstandung%s)\n", fehler == 0 ? "BESTANDEN" : "FEHLGESCHLAGEN",
         fehler, fehler == 1 ? "" : "en");
  return fehler == 0 ? 0 : 1;
}
