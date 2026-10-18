// Nachweis der Byte-28-Kodierung OHNE Geraet: Merge-Logik aus setcommands.h (der
// echte Code, keine Nachbildung) und die beiden Dekodierer aus decode.cpp
// nebeneinandergelegt. Belegt zweierlei:
// (1) jeder gesendete Wert kommt beim Rueckgelesenen wieder heraus,
// (2) ein Kommando auf den einen Kreis laesst den anderen unberuehrt.
#include <cstdio>
#include <cstdint>
#include <cstring>
typedef unsigned char byte;
#include "../src/setcommands.h"

// aus decode.cpp
static int getBit7and8(byte in) { return (in & 0b11) - 1; }        // TOP76 Heating_Mode
static int getBit5and6(byte in) { return ((in >> 2) & 0b11) - 1; } // TOP81 Cooling_Mode

static int fehler = 0;
static void pruefe(const char *was, int ist, int soll) {
  bool ok = (ist == soll);
  if (!ok) fehler++;
  printf("  [%s] %-46s ist %d, erwartet %d\n", ok ? "ok " : "FEHL", was, ist, soll);
}

// Der ECHTE Code aus setcommands.h (bis 3.25.0 stand hier eine Nachbildung der
// Merge-Zeile): einen Wert unter seinem Set-Topic-Namen in ein Byte einsetzen.
// Frischer Puffer je Aufruf, damit kein Konflikt entsteht - es geht um die
// Maske, nicht um die Belegung. Eine fehlende Tabellenzeile ist ein Fehler.
static byte merge(byte b, const char *set_name, int value)
{
  const SetCommand *c = setcommand_finden(set_name);
  if (!c)
  {
    printf("  [FEHLER] %s steht nicht in setCommands[]\n", set_name);
    fehler++;
    return b;
  }
  byte befehl[QUERYSIZE] = {0};
  byte belegt[QUERYSIZE] = {0};
  befehl[c->pos] = b;
  (void)setcommand_anwenden(*c, value, befehl, belegt);
  return befehl[c->pos];
}

// Dasselbe mit ANDERER Maske, fuer die Gegenproben "ohne Maske": Die Zeile
// wird kopiert und nur die Maske getauscht, angewendet wird wieder der echte Code.
static byte merge_mit_maske(byte b, const char *set_name, byte maske, int value)
{
  const SetCommand *c = setcommand_finden(set_name);
  if (!c)
  {
    printf("  [FEHLER] %s steht nicht in setCommands[]\n", set_name);
    fehler++;
    return b;
  }
  SetCommand kopie = *c;
  kopie.mask = maske;
  byte befehl[QUERYSIZE] = {0};
  byte belegt[QUERYSIZE] = {0};
  befehl[kopie.pos] = b;
  (void)setcommand_anwenden(kopie, value, befehl, belegt);
  return befehl[kopie.pos];
}

int main() {
  printf("== Kodierung: gesendet -> Rohbyte -> zurueckgelesen ==\n");
  for (int h = 0; h <= 1; h++) {
    for (int c = 0; c <= 1; c++) {
      byte b = 0;
      b = merge(b, "HeatingMode", h);  // SET35
      b = merge(b, "CoolingMode", c);  // SET36
      char t[80];
      snprintf(t, sizeof(t), "Heat=%d Cool=%d -> 0x%02X, Heat gelesen", h, c, b);
      pruefe(t, getBit7and8(b), h);
      snprintf(t, sizeof(t), "Heat=%d Cool=%d -> 0x%02X, Cool gelesen", h, c, b);
      pruefe(t, getBit5and6(b), c);
    }
  }

  printf("\n== Gegenprobe gegen die vier Rohwerte aus ProtocolByteDecrypt.md ==\n");
  struct { byte roh; int heat; int cool; } ref[] = {
    {0x05, 0, 0}, {0x09, 0, 1}, {0x06, 1, 0}, {0x0A, 1, 1},
  };
  for (auto &r : ref) {
    char t[80];
    snprintf(t, sizeof(t), "0x%02X Heating_Mode", r.roh);
    pruefe(t, getBit7and8(r.roh), r.heat);
    snprintf(t, sizeof(t), "0x%02X Cooling_Mode", r.roh);
    pruefe(t, getBit5and6(r.roh), r.cool);
    // dieselbe Kombination muss aus den Set-Kommandos entstehen
    byte b = merge(merge(0, "HeatingMode", r.heat), "CoolingMode", r.cool);
    snprintf(t, sizeof(t), "0x%02X aus SET35/SET36 rekonstruiert", r.roh);
    pruefe(t, b, r.roh & 0x0F);
  }

  printf("\n== Nachbarschutz: ein Kreis geschaltet, der andere unberuehrt ==\n");
  // Ausgangslage wie an der Anlage: beide auf Direkt (0x0A)
  byte b = 0x0A;
  byte nur_cool = merge(b, "CoolingMode", 0);   // set/CoolingMode 0 (Schritt 2)
  pruefe("0x0A + CoolingMode 0 -> Rohbyte", nur_cool, 0x06);
  pruefe("  Heating_Mode bleibt Direkt", getBit7and8(nur_cool), 1);
  pruefe("  Cooling_Mode wird Kurve", getBit5and6(nur_cool), 0);
  byte nur_heat = merge(b, "HeatingMode", 0);
  pruefe("0x0A + HeatingMode 0 -> Rohbyte", nur_heat, 0x09);
  pruefe("  Cooling_Mode bleibt Direkt", getBit5and6(nur_heat), 1);
  pruefe("  Heating_Mode wird Kurve", getBit7and8(nur_heat), 0);

  printf("\n== Gegenprobe: ohne Maske faellt genau das um ==\n");
  byte ohne_maske = merge_mit_maske(b, "CoolingMode", 0xFF, 0);  // wie SET36, aber Maske 0xFF
  pruefe("0x0A + CoolingMode 0 ohne Maske", ohne_maske, 0x04);
  pruefe("  Heating_Mode waere zerstoert (-1)", getBit7and8(ohne_maske), -1);

  printf("\n%s (%d Fehler)\n", fehler ? "FEHLGESCHLAGEN" : "ALLE ZUSICHERUNGEN ERFUELLT", fehler);
  return fehler != 0;
}
