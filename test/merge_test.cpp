// Nachbildung der Merge-Logik aus commands.cpp, um den Fix vor dem Flashen
// zu belegen. Werte 1:1 aus der setCommands-Tabelle uebernommen.
//
// Seit 3.6.0 prueft das Programm seine Ergebnisse selbst: jede Zeile hat eine
// Zusicherung, und der Rueckgabewert ist != 0, sobald eine davon bricht.
// Vorher gab es nur Ausgaben - der CI-Schritt fing damit lediglich
// Uebersetzungsfehler und Abstuerze ab.
//
// Bauen und ausfuehren:
//   c++ -std=c++17 -O2 -Wall -o /tmp/merge_test test/merge_test.cpp
//   /tmp/merge_test
#include <cstdio>
#include <cstring>

typedef unsigned char byte;

enum ConvType { CONV_ADD, CONV_MUL, CONV_MUL_INC, CONV_OPMODE };
static const byte opModeBytes[] = {18, 19, 24, 33, 34, 35, 40};

struct Field { const char *name; byte pos; byte mask; ConvType conv; int param; };

// die sechs Kanaele, die der Node-RED-Verteiler pro WP gleichzeitig ausgibt
static const Field Heatpump   = {"Heatpump",   4,  0x03, CONV_ADD,     1};
static const Field WaterPump  = {"WaterPump",  4,  0x30, CONV_MUL_INC, 16};
static const Field ForceDHW   = {"ForceDHW",   4,  0xC0, CONV_MUL_INC, 64};
static const Field OpMode     = {"OperationMode", 6, 0xFF, CONV_OPMODE, 0};
static const Field Z1Heat     = {"Z1HeatReq",  38, 0xFF, CONV_ADD,   128};
static const Field Z1Cool     = {"Z1CoolReq",  39, 0xFF, CONV_ADD,   128};
static const Field PumpSpeed  = {"PumpSpeed",  45, 0xFF, CONV_ADD,     1};

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
  bool ok = (ist == soll);
  printf("  [%s] %-52s (erwartet 0x%02X, ist 0x%02X)\n", ok ? "ok " : "FEHLER", was, soll, ist);
  if (!ok)
    fehler++;
}

static byte encode(const Field &f, int v)
{
  switch (f.conv) {
    case CONV_ADD:     return (byte)(v + f.param);
    case CONV_MUL:     return (byte)(v * f.param);
    case CONV_MUL_INC: return (byte)((v + 1) * f.param);
    case CONV_OPMODE:  return opModeBytes[v];
  }
  return 0;
}

static byte buf[110];

static void applyNew(const Field &f, int v)   // 3.1.0
{ buf[f.pos] = (buf[f.pos] & ~f.mask) | (encode(f, v) & f.mask); }

static void applyOld(const Field &f, int v)   // 3.0.1
{ buf[f.pos] = encode(f, v); }

static byte sumFrom4()   // die alte calculate_commandset-Heuristik
{ byte s = 0; for (int i = 4; i < 110; i++) s += buf[i]; return s; }

// alle Bytes ausser den genannten muessen 0 bleiben - belegt, dass die
// Maskenlogik keine Nachbarbytes anfasst
static bool nurDieseBytesGesetzt(const int *positionen, int anzahl)
{
  for (int i = 0; i < 110; i++) {
    bool erlaubt = false;
    for (int p = 0; p < anzahl; p++)
      if (positionen[p] == i) erlaubt = true;
    if (!erlaubt && buf[i] != 0) return false;
  }
  return true;
}

int main()
{
  printf("== Fall 1: Heatpump=1 + WaterPump=0 (der gemeldete Fehler) ==\n");
  memset(buf, 0, sizeof buf);
  applyOld(Heatpump, 1); applyOld(WaterPump, 0);
  printf("  3.0.1  Byte 4 = 0x%02X   -> Heatpump-Bits: %d (0 = verloren!)\n", buf[4], buf[4] & 0x03);
  pruefe((buf[4] & 0x03) == 0, "3.0.1 verlor die Heatpump-Bits (Fehler reproduziert)");
  memset(buf, 0, sizeof buf);
  applyNew(Heatpump, 1); applyNew(WaterPump, 0);
  printf("  3.1.0  Byte 4 = 0x%02X   -> Heatpump-Bits: %d, WaterPump-Bits: %d\n",
         buf[4], buf[4] & 0x03, (buf[4] & 0x30) >> 4);
  pruefe_byte(buf[4], 0x12, "3.1.0: Heatpump 1 (Bits 2) und WaterPump 0 (Bits 1)");
  pruefe((buf[4] & 0x03) == 2 && ((buf[4] & 0x30) >> 4) == 1, "beide Felder stehen unabhaengig voneinander im Byte");

  printf("\n== Fall 2: alle drei Felder von Byte 4 ==\n");
  memset(buf, 0, sizeof buf);
  applyNew(Heatpump, 1); applyNew(WaterPump, 0); applyNew(ForceDHW, 0);
  printf("  3.1.0  Byte 4 = 0x%02X\n", buf[4]);
  pruefe_byte(buf[4], 0x52, "Heatpump|WaterPump|ForceDHW = 2|16|64");
  { const int p[] = {4}; pruefe(nurDieseBytesGesetzt(p, 1), "kein Nachbarbyte veraendert"); }

  printf("\n== Fall 3: dasselbe Feld zweimal - darf nicht ver-ODERt werden ==\n");
  memset(buf, 0, sizeof buf);
  applyNew(Heatpump, 1); applyNew(WaterPump, 0); applyNew(Heatpump, 0);
  printf("  3.1.0  Byte 4 = 0x%02X   -> Heatpump-Bits: %d, WaterPump-Bits: %d\n",
         buf[4], buf[4] & 0x03, (buf[4] & 0x30) >> 4);
  pruefe_byte(buf[4], 0x11, "zweites Heatpump=0 ueberschreibt das erste, WaterPump bleibt");
  pruefe((buf[4] & 0x03) == 1, "Heatpump-Bits = 1 (aus), nicht 3 (ver-ODERt)");

  printf("\n== Fall 4: voller Verteiler-Satz, 6 Kanaele gleichzeitig ==\n");
  memset(buf, 0, sizeof buf);
  applyNew(Z1Heat, 35); applyNew(Z1Cool, 18); applyNew(Heatpump, 1);
  applyNew(OpMode, 4);  applyNew(WaterPump, 0); applyNew(PumpSpeed, 125);
  printf("  Byte  4 = 0x%02X  Byte 6 = 0x%02X  Byte 38 = 0x%02X  Byte 39 = 0x%02X  Byte 45 = 0x%02X\n",
         buf[4], buf[6], buf[38], buf[39], buf[45]);
  pruefe_byte(buf[4],  0x12, "Byte 4: Heatpump 1 + WaterPump 0");
  pruefe_byte(buf[6],  0x22, "Byte 6: OperationMode 4 (Heat+DHW) -> 34");
  pruefe_byte(buf[38], 0xA3, "Byte 38: Z1Heat 35 -> 163");
  pruefe_byte(buf[39], 0x92, "Byte 39: Z1Cool 18 -> 146");
  pruefe_byte(buf[45], 0x7E, "Byte 45: PumpSpeed 125 -> 126");
  { const int p[] = {4, 6, 38, 39, 45}; pruefe(nurDieseBytesGesetzt(p, 5), "genau diese fuenf Bytes sind belegt, sonst nichts"); }

  printf("\n== Fall 5: Bytesummen-Heuristik der 3.0.1 ==\n");
  memset(buf, 0, sizeof buf);
  applyOld(Z1Heat, 20); applyOld(Z1Cool, 20); applyOld(PumpSpeed, 79);
  printf("  Z1Heat=20, Z1Cool=20, PumpSpeed=79 -> Summe ab Byte 4 = %d\n", sumFrom4());
  pruefe(sumFrom4() == 120, "148+148+80 = 376, als Byte 120 -> waere gesendet worden");
  static const Field ForceDefrost = {"ForceDefrost", 8, 0x02, CONV_MUL, 2};
  memset(buf, 0, sizeof buf);
  applyOld(ForceDefrost, 0);
  printf("  SetForceDefrost 0 (ausschalten)   -> Summe ab Byte 4 = %d\n", sumFrom4());
  pruefe(sumFrom4() == 0, "SetForceDefrost 0 ergibt Summe 0 - 3.0.1 verwarf das Kommando (Fehler reproduziert)");

  printf("\n== Fall 6: gibt es einen REALEN 6-Kanal-Satz, dessen Summe auf 0 umschlaegt? ==\n");
  // Suchraum: die Werte, die der Verteiler tatsaechlich ausgeben kann
  int treffer = 0;
  const int opmodes[] = {0, 3, 4, 5};            // Modi der Kaskade
  for (int h = 20; h <= 57; h++)                  // clampHeat
    for (int c = 10; c <= 20; c++)                // clampCool
      for (int oi = 0; oi < 4; oi++)
        for (int p = 65; p <= 254; p++) {         // PumpSpeed-Bereich
          memset(buf, 0, sizeof buf);
          applyNew(Heatpump, 1); applyNew(WaterPump, 0);
          applyNew(OpMode, opmodes[oi]); applyNew(Z1Heat, h);
          applyNew(Z1Cool, c); applyNew(PumpSpeed, p);
          if (sumFrom4() == 0) {
            if (treffer < 3)
              printf("  Heat=%d Cool=%d OpMode=%d PumpSpeed=%d -> Summe 0 -> 3.0.1 verwirft das GANZE Telegramm\n",
                     h, c, opmodes[oi], p);
            treffer++;
          }
        }
  printf("  Treffer im realen Wertebereich: %d von %d Kombinationen\n",
         treffer, 38 * 11 * 4 * 190);
  // Der Suchraum ist vollstaendig durchgezaehlt, die Zahl ist damit fest.
  // Sie belegt, dass der Fehler der 3.0.1 im Normalbetrieb erreichbar war -
  // aendert sie sich, hat sich an der Kodierung etwas verschoben.
  pruefe(treffer == 1672, "1672 reale Kanalsaetze waren von der Summen-Heuristik betroffen (317680/256 = 1241 zu erwarten)");

  printf("\n%s (%d Fehler)\n", fehler == 0 ? "ALLE ZUSICHERUNGEN ERFUELLT" : "FEHLGESCHLAGEN", fehler);
  return (fehler == 0) ? 0 : 1;
}
