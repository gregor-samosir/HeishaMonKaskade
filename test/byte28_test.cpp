// Nachweis der Byte-28-Kodierung OHNE Geraet: Merge-Logik aus commands.cpp und
// die beiden Dekodierer aus decode.cpp nebeneinandergelegt. Belegt zweierlei:
// (1) jeder gesendete Wert kommt beim Rueckgelesenen wieder heraus,
// (2) ein Kommando auf den einen Kreis laesst den anderen unberuehrt.
#include <cstdio>
typedef unsigned char byte;

// aus commands.cpp: (value + 1) * param, danach bitgenau eingesetzt
static byte merge(byte b, byte mask, int value, int param) {
  byte set_byte = (byte)((value + 1) * param);
  return (byte)((b & ~mask) | (set_byte & mask));
}
// aus decode.cpp
static int getBit7and8(byte in) { return (in & 0b11) - 1; }        // TOP76 Heating_Mode
static int getBit5and6(byte in) { return ((in >> 2) & 0b11) - 1; } // TOP81 Cooling_Mode

static int fehler = 0;
static void pruefe(const char *was, int ist, int soll) {
  bool ok = (ist == soll);
  if (!ok) fehler++;
  printf("  [%s] %-46s ist %d, erwartet %d\n", ok ? "ok " : "FEHL", was, ist, soll);
}

int main() {
  printf("== Kodierung: gesendet -> Rohbyte -> zurueckgelesen ==\n");
  for (int h = 0; h <= 1; h++) {
    for (int c = 0; c <= 1; c++) {
      byte b = 0;
      b = merge(b, 0x03, h, 1);  // SET35 HeatingMode
      b = merge(b, 0x0C, c, 4);  // SET36 CoolingMode
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
    byte b = merge(merge(0, 0x03, r.heat, 1), 0x0C, r.cool, 4);
    snprintf(t, sizeof(t), "0x%02X aus SET35/SET36 rekonstruiert", r.roh);
    pruefe(t, b, r.roh & 0x0F);
  }

  printf("\n== Nachbarschutz: ein Kreis geschaltet, der andere unberuehrt ==\n");
  // Ausgangslage wie an der Anlage: beide auf Direkt (0x0A)
  byte b = 0x0A;
  byte nur_cool = merge(b, 0x0C, 0, 4);   // set/CoolingMode 0 (Schritt 2)
  pruefe("0x0A + CoolingMode 0 -> Rohbyte", nur_cool, 0x06);
  pruefe("  Heating_Mode bleibt Direkt", getBit7and8(nur_cool), 1);
  pruefe("  Cooling_Mode wird Kurve", getBit5and6(nur_cool), 0);
  byte nur_heat = merge(b, 0x03, 0, 1);
  pruefe("0x0A + HeatingMode 0 -> Rohbyte", nur_heat, 0x09);
  pruefe("  Cooling_Mode bleibt Direkt", getBit5and6(nur_heat), 1);
  pruefe("  Heating_Mode wird Kurve", getBit7and8(nur_heat), 0);

  printf("\n== Gegenprobe: ohne Maske faellt genau das um ==\n");
  byte ohne_maske = merge(b, 0xFF, 0, 4);  // wie SET36, aber Maske 0xFF
  pruefe("0x0A + CoolingMode 0 ohne Maske", ohne_maske, 0x04);
  pruefe("  Heating_Mode waere zerstoert (-1)", getBit7and8(ohne_maske), -1);

  printf("\n%s (%d Fehler)\n", fehler ? "FEHLGESCHLAGEN" : "ALLE ZUSICHERUNGEN ERFUELLT", fehler);
  return fehler != 0;
}
