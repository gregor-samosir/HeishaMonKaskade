// Nachbildung der Merge-Logik aus commands.cpp, um den Fix vor dem Flashen
// zu belegen. Werte 1:1 aus der setCommands-Tabelle uebernommen.
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

int main()
{
  printf("== Fall 1: Heatpump=1 + WaterPump=0 (der gemeldete Fehler) ==\n");
  memset(buf, 0, sizeof buf);
  applyOld(Heatpump, 1); applyOld(WaterPump, 0);
  printf("  3.0.1  Byte 4 = 0x%02X   -> Heatpump-Bits: %d (0 = verloren!)\n", buf[4], buf[4] & 0x03);
  memset(buf, 0, sizeof buf);
  applyNew(Heatpump, 1); applyNew(WaterPump, 0);
  printf("  3.1.0  Byte 4 = 0x%02X   -> Heatpump-Bits: %d, WaterPump-Bits: %d\n\n",
         buf[4], buf[4] & 0x03, (buf[4] & 0x30) >> 4);

  printf("== Fall 2: alle drei Felder von Byte 4 ==\n");
  memset(buf, 0, sizeof buf);
  applyNew(Heatpump, 1); applyNew(WaterPump, 0); applyNew(ForceDHW, 0);
  printf("  3.1.0  Byte 4 = 0x%02X   (erwartet 0x52 = 2|16|64)\n\n", buf[4]);

  printf("== Fall 3: dasselbe Feld zweimal - darf nicht ver-ODERt werden ==\n");
  memset(buf, 0, sizeof buf);
  applyNew(Heatpump, 1); applyNew(WaterPump, 0); applyNew(Heatpump, 0);
  printf("  3.1.0  Byte 4 = 0x%02X   -> Heatpump-Bits: %d (erwartet 1 = off), WaterPump-Bits: %d (erhalten)\n\n",
         buf[4], buf[4] & 0x03, (buf[4] & 0x30) >> 4);

  printf("== Fall 4: voller Verteiler-Satz, 6 Kanaele gleichzeitig ==\n");
  memset(buf, 0, sizeof buf);
  applyNew(Z1Heat, 35); applyNew(Z1Cool, 18); applyNew(Heatpump, 1);
  applyNew(OpMode, 4);  applyNew(WaterPump, 0); applyNew(PumpSpeed, 125);
  printf("  Byte  4 = 0x%02X  (Heatpump+WaterPump)\n", buf[4]);
  printf("  Byte  6 = 0x%02X  (OperationMode 4 = Heat+DHW -> 34)\n", buf[6]);
  printf("  Byte 38 = 0x%02X  (Z1Heat 35 -> 163)\n", buf[38]);
  printf("  Byte 39 = 0x%02X  (Z1Cool 18 -> 146)\n", buf[39]);
  printf("  Byte 45 = 0x%02X  (PumpSpeed 125 -> 126)\n\n", buf[45]);

  printf("== Fall 5: Bytesummen-Heuristik der 3.0.1 ==\n");
  memset(buf, 0, sizeof buf);
  applyOld(Z1Heat, 20); applyOld(Z1Cool, 20); applyOld(PumpSpeed, 79);
  printf("  Z1Heat=20, Z1Cool=20, PumpSpeed=79 -> Summe ab Byte 4 = %d\n", sumFrom4());
  printf("  3.0.1 haette das %s\n", sumFrom4() == 0 ? "als LEEREN Puffer verworfen und nur eine Query gesendet!"
                                                    : "korrekt als Kommando gesendet");
  static const Field ForceDefrost = {"ForceDefrost", 8, 0x02, CONV_MUL, 2};
  memset(buf, 0, sizeof buf);
  applyOld(ForceDefrost, 0);
  printf("  SetForceDefrost 0 (ausschalten)   -> Summe ab Byte 4 = %d -> %s\n",
         sumFrom4(), sumFrom4() == 0 ? "verworfen (Bug)" : "gesendet");

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
  return 0;
}
