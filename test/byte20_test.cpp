// Nachweis der Fuehlerumschaltung SET40/TOP112 OHNE Geraet (3.23.0).
//
// Byte 20 traegt AltExternalSensor (Bits 3+4, Maske 0x30) neben Feldern ohne
// eigenes Set-Kommando: Frostschutz (Bits 5+6) und Optionsplatine (Bits 7+8).
// Bit 1 (Wasser/Glykol) ist ein Einzelbit, das jedes SET40-Kommando mit 0
// schickt - das Bitpaar 1+2 steht deshalb hier stellvertretend fuer diesen
// Nachbarn. TOP66 Low_Pressure entfaellt im selben Zug (Byte 164 stand an
// beiden Stufen dauerhaft auf 0x01, siehe decode.cpp).
//
// Geprueft wird gegen den ECHTEN Dekodierpfad: src/decode.cpp wird
// mituebersetzt und ueber getTopicPayload() aufgerufen, wie in byte9_test.cpp.
// Nachgebildet ist nur die Merge-Zeile aus commands.cpp.
//
// Der On-Testvektor 0x2A ist an der Anlage gemessen (Arbeitsplan-
// AltExternalSensor.md, Schritt 1, 2026-09-19): beide Stufen identisch, Byte
// 20 bewegte sich waehrend 30 s Mitschnitt nicht. Frostschutz und
// Optionsplatine standen dabei auf b10, das Bitpaar 1+2 auf b00 (Bit 1 = 0,
// Wasser, E7).
//
// Vier Faelle:
//  1. TOP112 steht mit getBit3and4 auf Byte 20; TOP66 gibt es nicht mehr.
//  2. Jeder gesendete Wert kommt beim Rueckgelesenen wieder heraus - beide
//     Werte von SET40, mit Klartext Off/On.
//  3. Der gemessene Rohwert (On) dekodiert als On; alle Kombinationen von
//     Frostschutz, Optionsplatine und Bitpaar 1+2 lassen TOP112 unberuehrt.
//  4. Keine Kodierung von SET40 tritt aus ihrer Maske 0x30 heraus.
//
// Bauen und ausfuehren (ueber das Skript, wie bei byte9_test.cpp):
//   ./test/decode_hosttest.sh test/byte20_test.cpp
//   Rueckgabewert != 0 = Zusicherung gebrochen.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

#include "decode.h"

// decode.cpp loggt beim Publizieren - hier nicht gebraucht
void write_telnet_log(char *) {}
void write_mqtt_log(char *) {}
void write_wert_log(char *) {} // seit 3.20.0, siehe stubs/HeishaMon.h

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

int main()
{
  char text[160];

  printf("== Fall 1: TOP112 auf Byte 20, TOP66 entfallen ==\n");
  int z_top112 = zeile_von("Alt_External_Sensor");
  pruefe(z_top112 >= 0, "Alt_External_Sensor vorhanden");
  if (z_top112 < 0)
  {
    printf("\nFEHLGESCHLAGEN - ohne die Tabellenzeile ist der Rest sinnlos\n");
    return 1;
  }
  const StateTopic &t112 = stateTopics[z_top112];
  snprintf(text, sizeof text, "%s ist TOP%u auf Byte %u mit getBit3and4 (erwartet TOP112/20)",
           t112.name, t112.number, t112.pos);
  pruefe(t112.number == 112 && t112.pos == 20 && t112.decode == getBit3and4, text);
  pruefe(state_topic_index(66) < 0, "TOP66 gibt es nicht mehr (state_topic_index)");
  pruefe(zeile_von("Low_Pressure") < 0, "Low_Pressure gibt es nicht mehr (Name)");

  printf("\n== Fall 2: gesendet -> Rohbyte -> zurueckgelesen ==\n");
  for (int aussen = 0; aussen <= 1; aussen++)
  {
    byte b = merge(0, 0x30, aussen, 16); // SET40 AltExternalSensor
    snprintf(text, sizeof text, "AltExternalSensor=%d -> 0x%02X, gelesen %d (%s)",
             aussen, b, wert_der_zeile(z_top112, b), text_der_zeile(z_top112, b));
    pruefe(wert_der_zeile(z_top112, b) == aussen, text);
    pruefe(strcmp(text_der_zeile(z_top112, b), aussen ? "On" : "Off") == 0,
           "  Klartext Alt_External_Sensor ist Off/On");
  }

  printf("\n== Fall 3: gemessener Rohwert (On, beide Stufen 2026-09-19) und Nachbarn ==\n");
  // 0x2A = 0b00101010: Bitpaar1+2=00 (Bit 1 = 0, Wasser), Bit3+4=10 (On),
  // Bit5+6=10 (Frostschutz), Bit7+8=10 (Optionsplatine). Ohne das
  // AltExternalSensor-Feld selbst (Maske 0x30 ausgeblendet) bleibt 0x0A.
  const byte nachbarn_gemessen = 0x0A;
  const byte roh_on = (byte)(nachbarn_gemessen | 0x20);
  const byte roh_off = (byte)(nachbarn_gemessen | 0x10);
  snprintf(text, sizeof text, "gemessener Rohwert On 0x%02X (erwartet 0x2A)", roh_on);
  pruefe(roh_on == 0x2A, text);
  pruefe(wert_der_zeile(z_top112, roh_on) == 1, "  gemessener Rohwert dekodiert als On");
  pruefe(strcmp(text_der_zeile(z_top112, roh_on), "On") == 0, "  Klartext On");
  // Gegenstueck: dieselben Nachbarn, nur das eigene Feld auf Off - noch nicht
  // an der Anlage gemessen, folgt aber zwingend aus derselben Kodierung.
  snprintf(text, sizeof text, "dieselben Nachbarn mit Off 0x%02X (erwartet 0x1A)", roh_off);
  pruefe(roh_off == 0x1A, text);
  pruefe(wert_der_zeile(z_top112, roh_off) == 0, "  dekodiert als Off");
  pruefe(strcmp(text_der_zeile(z_top112, roh_off), "Off") == 0, "  Klartext Off");

  // Alle Kombinationen von Bitpaar 1+2, Frostschutz (5+6) und Optionsplatine
  // (7+8) durchlaufen - TOP112 darf sich nur mit seinem eigenen Feld (3+4)
  // aendern. Deckt damit auch die gemessene Kombination (b00/b10/b10) mit ab.
  int gepruefte_kombinationen = 0;
  for (int bit12 = 0; bit12 <= 3; bit12++)
  {
    for (int frost = 0; frost <= 3; frost++)
    {
      for (int option = 0; option <= 3; option++)
      {
        byte nachbarn = (byte)((bit12 << 6) | (frost << 2) | option);
        byte mit_on = (byte)((nachbarn & (byte)~0x30) | 0x20);
        byte mit_off = (byte)((nachbarn & (byte)~0x30) | 0x10);
        if (wert_der_zeile(z_top112, mit_on) != 1 || wert_der_zeile(z_top112, mit_off) != 0)
        {
          snprintf(text, sizeof text,
                   "Nachbarn 0x%02X: On/Off bleiben unabhaengig (Rohbyte 0x%02X/0x%02X)",
                   nachbarn, mit_on, mit_off);
          pruefe(false, text);
        }
        gepruefte_kombinationen++;
      }
    }
  }
  snprintf(text, sizeof text, "%d Nachbar-Kombinationen liessen TOP112 unberuehrt (4x4x4)",
           gepruefte_kombinationen);
  pruefe(gepruefte_kombinationen == 64, text);

  printf("\n== Fall 4: keine Kodierung tritt aus der Maske 0x30 heraus ==\n");
  for (int v = 0; v <= 1; v++) // min..max aus setCommands[]
  {
    byte set_byte = (byte)((v + 1) * 16); // SET40, param=16
    snprintf(text, sizeof text, "AltExternalSensor Wert %d -> 0x%02X liegt ganz in Maske 0x30",
             v, set_byte);
    pruefe((set_byte & (byte)~0x30) == 0, text);
  }

  printf("\n%s (%d Fehler)\n", fehler ? "FEHLGESCHLAGEN" : "ALLE ZUSICHERUNGEN ERFUELLT", fehler);
  return fehler != 0;
}
