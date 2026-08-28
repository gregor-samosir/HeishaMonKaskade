// Nachweis der Heizstab-Kommandos SET37-SET39 OHNE Geraet (3.17.0).
//
// Byte 9 traegt ZWEI Freigaben (Raumheizung, Warmwasser), Byte 5 traegt
// ForceHeater neben HolidayMode und dem Zeitprogramm. Beide Bytes sind damit
// genau der Fall, fuer den die Maskenspalte in commands.cpp existiert - ohne
// sie legt ein Kommando das Nachbarfeld mit um. Das ist an der Anlage der
// Schritt M2 des Messplans; hier faellt es schon vor dem Flashen auf.
//
// Geprueft wird gegen den ECHTEN Dekodierpfad: src/decode.cpp wird
// mituebersetzt und ueber getTopicPayload() aufgerufen, die Klartexte kommen
// aus desc_text() im Header - es gibt keine Nachbildung der Bitzuordnung, die
// auseinanderlaufen koennte. Nachgebildet ist nur die Merge-Zeile aus
// commands.cpp (dessen Uebersetzungseinheit zieht die halbe Arduino-Welt
// nach); sie steht als merge() an genau einer Stelle.
//
// Sechs Faelle:
//  1. Die fuenf betroffenen Zeilen stehen mit den erwarteten Bytes in der
//     Tabelle (TOP58/59 auf Byte 9, TOP13/19/68 auf Byte 5).
//  2. Jeder gesendete Wert kommt beim Rueckgelesenen wieder heraus - alle vier
//     Kombinationen von Byte 9, samt Klartext Blocked/Free.
//  3. Nachbarschutz Byte 9: ein Kommando auf die Raumheizung laesst die
//     Warmwasser-Freigabe stehen und umgekehrt.
//  4. Byte 5: ForceHeater laesst HolidayMode UND das Zeitprogramm stehen.
//  5. Gegenprobe: ohne Maske faellt genau das um.
//  6. Keine Kodierung tritt aus ihrer Maske heraus (Bereich 0..1 je Kommando).
//
// Bauen und ausfuehren (ueber das Skript, weil decode.cpp neben die
// Ersatzheader aus test/stubs/ kopiert werden muss):
//   ./test/decode_hosttest.sh test/byte9_test.cpp
//   Rueckgabewert != 0 = Zusicherung gebrochen.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

#include "decode.h"

// decode.cpp loggt beim Publizieren - hier nicht gebraucht
void write_telnet_log(char *) {}
void write_mqtt_log(char *) {}

static int fehler = 0;

static void pruefe(bool bedingung, const char *was)
{
  printf("  [%s] %s\n", bedingung ? "ok " : "FEHLER", was);
  if (!bedingung)
    fehler++;
}

// aus commands.cpp: (value + 1) * param, danach bitgenau eingesetzt
static byte merge(byte b, byte mask, int value, int param)
{
  byte set_byte = (byte)((value + 1) * param);
  return (byte)((b & ~mask) | (set_byte & mask));
}

// Tabellenzeile ueber den Topic-Namen finden. Der Index IST nicht die
// TOP-Nummer, deshalb gesucht statt gerechnet.
static int zeile_von(const char *name)
{
  for (unsigned int i = 0; i < NUMBEROFTOPICS; i++)
    if (strcmp(stateTopics[i].name, name) == 0)
      return (int)i;
  return -1;
}

// Ein Telegramm mit einem Rohwert auf dem Quellbyte der Zeile dekodieren.
// Der Puffer ist so gross wie serial_data in HeishaMon.cpp.
static int wert_der_zeile(int zeile, byte roh)
{
  uint8_t telegramm[256];
  memset(telegramm, 0, sizeof telegramm);
  telegramm[stateTopics[zeile].pos] = roh;
  char out[MAXVALUELEN];
  getTopicPayload((unsigned int)zeile, telegramm, out);
  return atoi(out);
}

static const char *text_der_zeile(int zeile, byte roh)
{
  return desc_text(stateTopics[zeile].desc, wert_der_zeile(zeile, roh));
}

// Die fuenf Zeilen, die von SET37-SET39 beruehrt werden. 'byte_nr' ist das
// Quellbyte, das die Tabelle in decode.cpp nennen muss.
struct Feld
{
  byte nummer;
  const char *name;
  byte byte_nr;
};

static const Feld felder[] = {
    { 59, "Room_Heater_State",   9}, // SET37, Maske 0x03
    { 58, "DHW_Heater_State",    9}, // SET38, Maske 0x0C
    { 68, "Force_Heater_State",  5}, // SET39, Maske 0x0C
    { 19, "Holiday_Mode_State",  5}, // SET2,  Maske 0x30 - Nachbar von SET39
    { 13, "Main_Schedule_State", 5}, // kein Set-Kommando - Nachbar von SET39
};
static const int ANZAHL_FELDER = (int)(sizeof(felder) / sizeof(felder[0]));

int main()
{
  char text[160];
  int zeilen[ANZAHL_FELDER];

  printf("== Fall 1: die fuenf Zeilen stehen mit dem erwarteten Quellbyte in der Tabelle ==\n");
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
    snprintf(text, sizeof text, "%s ist TOP%u auf Byte %u (erwartet TOP%u/%u)",
             t.name, t.number, t.pos, felder[f].nummer, felder[f].byte_nr);
    pruefe(t.number == felder[f].nummer && t.pos == felder[f].byte_nr, text);
  }
  const int z_room = zeilen[0];
  const int z_dhw = zeilen[1];
  const int z_force = zeilen[2];
  const int z_holiday = zeilen[3];
  const int z_schedule = zeilen[4];

  printf("\n== Fall 2: Byte 9 - gesendet -> Rohbyte -> zurueckgelesen ==\n");
  for (int room = 0; room <= 1; room++)
  {
    for (int dhw = 0; dhw <= 1; dhw++)
    {
      byte b = 0;
      b = merge(b, 0x03, room, 1); // SET37 RoomHeaterState
      b = merge(b, 0x0C, dhw, 4);  // SET38 DHWHeaterState
      snprintf(text, sizeof text, "Room=%d DHW=%d -> 0x%02X, Room gelesen %d (%s)",
               room, dhw, b, wert_der_zeile(z_room, b), text_der_zeile(z_room, b));
      pruefe(wert_der_zeile(z_room, b) == room, text);
      snprintf(text, sizeof text, "Room=%d DHW=%d -> 0x%02X, DHW gelesen %d (%s)",
               room, dhw, b, wert_der_zeile(z_dhw, b), text_der_zeile(z_dhw, b));
      pruefe(wert_der_zeile(z_dhw, b) == dhw, text);
      // Klartext, so wie ihn Weboberflaeche und MQTT-Nutzlast zeigen
      pruefe(strcmp(text_der_zeile(z_room, b), room ? "Free" : "Blocked") == 0,
             "  Klartext Room_Heater_State ist Blocked/Free");
      pruefe(strcmp(text_der_zeile(z_dhw, b), dhw ? "Free" : "Blocked") == 0,
             "  Klartext DHW_Heater_State ist Blocked/Free");
    }
  }

  printf("\n== Fall 3: Byte 9 - Nachbarschutz ==\n");
  // Ausgangslage wie an der Anlage: beide Heizstaebe blockiert (M0 des Messplans)
  byte b9 = merge(merge(0, 0x03, 0, 1), 0x0C, 0, 4);
  snprintf(text, sizeof text, "Ausgangslage 'beide blockiert' ist 0x%02X (erwartet 0x05)", b9);
  pruefe(b9 == 0x05, text);

  byte nur_room = merge(b9, 0x03, 1, 1); // set/RoomHeaterState 1 - der Schritt M2
  snprintf(text, sizeof text, "0x05 + RoomHeaterState 1 -> 0x%02X (erwartet 0x06)", nur_room);
  pruefe(nur_room == 0x06, text);
  pruefe(wert_der_zeile(z_room, nur_room) == 1, "  Room_Heater_State wird Free");
  pruefe(wert_der_zeile(z_dhw, nur_room) == 0, "  DHW_Heater_State bleibt Blocked");

  byte nur_dhw = merge(b9, 0x0C, 1, 4); // set/DHWHeaterState 1
  snprintf(text, sizeof text, "0x05 + DHWHeaterState 1 -> 0x%02X (erwartet 0x09)", nur_dhw);
  pruefe(nur_dhw == 0x09, text);
  pruefe(wert_der_zeile(z_dhw, nur_dhw) == 1, "  DHW_Heater_State wird Free");
  pruefe(wert_der_zeile(z_room, nur_dhw) == 0, "  Room_Heater_State bleibt Blocked");

  printf("\n== Fall 4: Byte 5 - ForceHeater neben HolidayMode und Zeitprogramm ==\n");
  // Ausgangslage: Urlaubsmodus gesetzt (SET2 HolidayMode 1) und Zeitprogramm
  // aktiv, wie es die Waermepumpe im Antworttelegramm melden kann.
  byte b5 = merge(0, 0x30, 1, 16); // SET2 HolidayMode 1 -> 0x20
  b5 = (byte)((b5 & ~0xC0) | 0x80); // Zeitprogramm 'Enabled', kein Set-Kommando
  const int holiday_vorher = wert_der_zeile(z_holiday, b5);
  const int schedule_vorher = wert_der_zeile(z_schedule, b5);
  snprintf(text, sizeof text, "Ausgangslage 0x%02X: Holiday=%d, Schedule=%d",
           b5, holiday_vorher, schedule_vorher);
  pruefe(b5 == 0xA0 && holiday_vorher == 1 && schedule_vorher == 1, text);

  for (int force = 0; force <= 1; force++)
  {
    byte b = merge(b5, 0x0C, force, 4); // SET39 ForceHeater
    snprintf(text, sizeof text, "ForceHeater %d -> 0x%02X, gelesen %d (%s)",
             force, b, wert_der_zeile(z_force, b), text_der_zeile(z_force, b));
    pruefe(wert_der_zeile(z_force, b) == force, text);
    pruefe(strcmp(text_der_zeile(z_force, b), force ? "Active" : "Inactive") == 0,
           "  Klartext Force_Heater_State ist Inactive/Active");
    pruefe(wert_der_zeile(z_holiday, b) == holiday_vorher, "  Holiday_Mode_State unveraendert");
    pruefe(wert_der_zeile(z_schedule, b) == schedule_vorher, "  Main_Schedule_State unveraendert");
  }
  // Gegenrichtung: das aeltere Kommando darf die neue Freigabe nicht umlegen
  byte mit_force = merge(b5, 0x0C, 1, 4);
  byte dann_holiday = merge(mit_force, 0x30, 0, 16); // set/HolidayMode 0
  pruefe(wert_der_zeile(z_force, dann_holiday) == 1, "HolidayMode 0 laesst Force_Heater_State Active");
  pruefe(wert_der_zeile(z_holiday, dann_holiday) == 0, "  Holiday_Mode_State wird Off");

  printf("\n== Fall 5: Gegenprobe - ohne Maske faellt genau das um ==\n");
  byte b9_roh = merge(b9, 0xFF, 1, 1); // wie SET37, aber Maske 0xFF
  snprintf(text, sizeof text, "0x05 + RoomHeaterState 1 ohne Maske -> 0x%02X (erwartet 0x02)", b9_roh);
  pruefe(b9_roh == 0x02, text);
  pruefe(wert_der_zeile(z_dhw, b9_roh) == -1, "  DHW_Heater_State waere zerstoert (-1)");
  pruefe(strcmp(text_der_zeile(z_dhw, b9_roh), "") == 0, "  und zeigt in der Weboberflaeche leer statt Blocked");

  byte b5_roh = merge(b5, 0xFF, 1, 4); // wie SET39, aber Maske 0xFF
  snprintf(text, sizeof text, "0xA0 + ForceHeater 1 ohne Maske -> 0x%02X (erwartet 0x08)", b5_roh);
  pruefe(b5_roh == 0x08, text);
  pruefe(wert_der_zeile(z_holiday, b5_roh) == -1, "  Holiday_Mode_State waere zerstoert (-1)");
  pruefe(wert_der_zeile(z_schedule, b5_roh) == -1, "  Main_Schedule_State waere zerstoert (-1)");

  printf("\n== Fall 6: keine Kodierung tritt aus ihrer Maske heraus ==\n");
  // Traefe eine Kodierung Bits ausserhalb ihrer Maske, wuerde die Maske sie
  // still abschneiden - das Kommando kaeme verstuemmelt an der Waermepumpe an.
  struct { const char *name; byte mask; int param; } kommandos[] = {
      {"SET37 RoomHeaterState", 0x03, 1},
      {"SET38 DHWHeaterState",  0x0C, 4},
      {"SET39 ForceHeater",     0x0C, 4},
  };
  for (auto &k : kommandos)
  {
    for (int v = 0; v <= 1; v++) // min..max aus setCommands[]
    {
      byte set_byte = (byte)((v + 1) * k.param);
      snprintf(text, sizeof text, "%s Wert %d -> 0x%02X liegt ganz in Maske 0x%02X",
               k.name, v, set_byte, k.mask);
      pruefe((set_byte & (byte)~k.mask) == 0, text);
    }
  }

  printf("\n%s (%d Fehler)\n", fehler ? "FEHLGESCHLAGEN" : "ALLE ZUSICHERUNGEN ERFUELLT", fehler);
  return fehler != 0;
}
