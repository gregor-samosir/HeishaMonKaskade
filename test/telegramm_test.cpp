// Nachweis fuer die Telegrammpruefung aus 3.6.0 (src/telegram.h).
//
// Geprueft wird der Code, der auch auf dem Geraet laeuft: die Datei wird hier
// direkt eingebunden, es gibt keine Nachbildung, die auseinanderlaufen kann.
//
// Zwei Fragen beantwortet dieser Test:
//  1. Wird ein echtes Antworttelegramm weiterhin angenommen? (keine falsch
//     negativen - sonst stuende die Anlage still)
//  2. Wie viel Muell haette die alte Regel durchgelassen, den die neue
//     abweist? (der Fehlerpfad, ueber den falsche Messwerte in die
//     Kaskadenregelung geraten konnten)
//
// Bauen und ausfuehren:
//   c++ -std=c++17 -O2 -Wall -o /tmp/telegramm_test test/telegramm_test.cpp
//   /tmp/telegramm_test          (Rueckgabewert != 0 = Test fehlgeschlagen)

#include <cstdio>
#include <cstring>
#include <cstdint>

#include "../src/telegram.h"

#define MAXDATASIZE 256 // wie in HeishaMon.h - Groesse des Empfangspuffers

static int fehler = 0;

// eine Zusicherung mit Klartext, damit der CI-Log ohne Debugger lesbar ist
static void pruefe(bool bedingung, const char *was)
{
  printf("  [%s] %s\n", bedingung ? "ok " : "FEHLER", was);
  if (!bedingung)
    fehler++;
}

static void pruefe_ergebnis(TelegramCheck ist, TelegramCheck soll, const char *was)
{
  bool ok = (ist == soll);
  printf("  [%s] %-58s (erwartet %s, ist %s)\n", ok ? "ok " : "FEHLER", was,
         telegram_check_text(soll), telegram_check_text(ist));
  if (!ok)
    fehler++;
}

// Echtes Antworttelegramm der Waermepumpe, 203 Bytes, Pruefsumme 0.
// Quelle: ProtocolByteDecrypt.md, "Panasonic answer example".
static const uint8_t antwort[TELEGRAM_DATA_LEN] = {
    0x71, 0xC8, 0x01, 0x10, 0x56, 0x55, 0x62, 0x49, 0x00, 0x05, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x19, 0x15, 0x11, 0x55,
    0x16, 0x5E, 0x55, 0x05, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x80, 0x8F, 0x80, 0x8A, 0xB2, 0x71, 0x71, 0x97, 0x99, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x85,
    0x15, 0x8A, 0x85, 0x85, 0xD0, 0x7B, 0x78, 0x1F, 0x7E, 0x1F, 0x1F, 0x79,
    0x79, 0x8D, 0x8D, 0x9E, 0x96, 0x71, 0x8F, 0xB7, 0xA3, 0x7B, 0x8F, 0x8E,
    0x85, 0x80, 0x8F, 0x8A, 0x94, 0x9E, 0x8A, 0x8A, 0x94, 0x9E, 0x82, 0x90,
    0x8B, 0x05, 0x65, 0x78, 0xC1, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x55, 0x56, 0x55, 0x21, 0x53, 0x15, 0x5A, 0x05, 0x12, 0x12,
    0x19, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE2, 0xCE, 0x0D,
    0x71, 0x81, 0x72, 0xCE, 0x0C, 0x92, 0x81, 0xB0, 0x00, 0xAA, 0x7C, 0xAB,
    0xB0, 0x32, 0x32, 0x9C, 0xB6, 0x32, 0x32, 0x32, 0x80, 0xB7, 0xAF, 0xCD,
    0x9A, 0xAC, 0x79, 0x80, 0x77, 0x80, 0xFF, 0x91, 0x01, 0x29, 0x59, 0x00,
    0x00, 0x3B, 0x0B, 0x1C, 0x51, 0x59, 0x01, 0x36, 0x79, 0x01, 0x01, 0xC3,
    0x02, 0x00, 0xDD, 0x02, 0x00, 0x05, 0x00, 0x00, 0x01, 0x00, 0x00, 0x06,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x0A, 0x14, 0x00, 0x00, 0x00, 0x77};

// Abfragetelegramm, das die Firmware selbst sendet (111 Bytes, Typ 0x71):
// richtiger Typ, falsche Laenge - genau der Fall, den ein Echo auf der
// Leitung erzeugen wuerde
static uint8_t abfrage[111];

// Die Regel VOR 3.6.0: vollstaendig, sobald die Laenge zum Laengenbyte passt,
// und dann entscheidet allein die Pruefsumme.
static bool alte_regel(const uint8_t *data, unsigned int len)
{
  if (len < 2)
    return false;
  if (len != ((unsigned int)data[1] + 3u))
    return false;
  return telegram_checksum_ok(data, len);
}

// deterministischer Zufall: gleiche Zahlen auf jedem Rechner und in der CI,
// damit die Trefferzahlen unten reproduzierbar sind
static uint32_t rnd_state = 20260813u;
static uint8_t rnd_byte()
{
  rnd_state = rnd_state * 1664525u + 1013904223u;
  return (uint8_t)(rnd_state >> 24);
}

int main()
{
  // Abfragetelegramm aufbauen: 0x71 0x6C ... + Pruefsumme
  memset(abfrage, 0, sizeof abfrage);
  abfrage[0] = 0x71;
  abfrage[1] = 0x6C;
  abfrage[2] = 0x01;
  abfrage[3] = 0x10;
  uint8_t chk = 0;
  for (unsigned int i = 0; i < sizeof(abfrage) - 1; i++)
    chk = (uint8_t)(chk + abfrage[i]);
  abfrage[sizeof(abfrage) - 1] = (uint8_t)((chk ^ 0xFF) + 1);

  printf("== Fall 1: echtes Antworttelegramm wird angenommen ==\n");
  pruefe_ergebnis(check_telegram(antwort, TELEGRAM_DATA_LEN), TELEGRAM_OK,
                  "203 Bytes, Typ 0x71, Laengenbyte 0xC8, Pruefsumme 0");
  pruefe(alte_regel(antwort, TELEGRAM_DATA_LEN), "die alte Regel nahm es ebenfalls an (kein Verhaltenswechsel im Normalbetrieb)");

  printf("\n== Fall 2: jedes Nutzbyte einzeln variiert - Pruefsumme nachgezogen ==\n");
  // Belegt, dass die neue Pruefung an den DATEN nichts entscheidet: nur Typ,
  // Laenge und Pruefsumme zaehlen. Sonst koennten gueltige Messwerte
  // (z. B. eine neue Temperatur) faelschlich verworfen werden.
  int verworfen = 0;
  for (unsigned int pos = 4; pos < TELEGRAM_DATA_LEN - 1; pos++)
  {
    for (int delta = 1; delta < 256; delta += 37) // Stichprobe ueber den Wertebereich
    {
      uint8_t kopie[TELEGRAM_DATA_LEN];
      memcpy(kopie, antwort, sizeof kopie);
      kopie[pos] = (uint8_t)(kopie[pos] + delta);
      kopie[TELEGRAM_DATA_LEN - 1] = (uint8_t)(kopie[TELEGRAM_DATA_LEN - 1] - delta); // Pruefsumme korrigieren
      if (check_telegram(kopie, TELEGRAM_DATA_LEN) != TELEGRAM_OK)
        verworfen++;
    }
  }
  printf("  %d Varianten geprueft, davon verworfen: %d\n", (198 * 7), verworfen);
  pruefe(verworfen == 0, "kein gueltiges Telegramm wird wegen seines Inhalts verworfen");

  printf("\n== Fall 3: die Faelle, die vorher durchrutschen konnten ==\n");
  {
    // (a) Abfrage-Echo: richtiger Typ, aber 111 statt 203 Bytes. Die alte
    // Regel haette es als vollstaendig UND pruefsummenrichtig angenommen und
    // an den Decoder gegeben, der bis Byte 202 liest.
    pruefe(alte_regel(abfrage, sizeof abfrage), "alte Regel: 111-Byte-Echo galt als gueltig (Lesen ueber das Pufferende)");
    pruefe_ergebnis(check_telegram(abfrage, sizeof abfrage), TELEGRAM_WRONG_TYPE,
                    "neue Regel weist das 111-Byte-Echo ab");
  }
  {
    // (b) Kommandotelegramm (0xF1) auf der Leitung
    uint8_t kommando[TELEGRAM_DATA_LEN];
    memcpy(kommando, antwort, sizeof kommando);
    kommando[0] = 0xF1;
    kommando[TELEGRAM_DATA_LEN - 1] = (uint8_t)(kommando[TELEGRAM_DATA_LEN - 1] - 0x80); // Pruefsumme nachziehen
    pruefe(alte_regel(kommando, TELEGRAM_DATA_LEN), "alte Regel: Typ 0xF1 galt als Messdaten");
    pruefe_ergebnis(check_telegram(kommando, TELEGRAM_DATA_LEN), TELEGRAM_WRONG_TYPE,
                    "neue Regel weist Typ 0xF1 ab");
  }
  {
    // (c) unvollstaendige Antwort - Waermepumpe war langsamer als der 500-ms-Timer
    pruefe_ergebnis(check_telegram(antwort, 150), TELEGRAM_INCOMPLETE, "150 von 203 Bytes: unvollstaendig");
    pruefe_ergebnis(check_telegram(antwort, 1), TELEGRAM_INCOMPLETE, "1 Byte: unvollstaendig");
    pruefe_ergebnis(check_telegram(antwort, 0), TELEGRAM_INCOMPLETE, "0 Bytes: unvollstaendig (kein Zugriff auf data[1])");
  }
  {
    // (d) zu viele Bytes: Antwort plus ein Nachzuegler
    uint8_t zulang[TELEGRAM_DATA_LEN + 1];
    memcpy(zulang, antwort, TELEGRAM_DATA_LEN);
    zulang[TELEGRAM_DATA_LEN] = 0x00;
    pruefe_ergebnis(check_telegram(zulang, TELEGRAM_DATA_LEN + 1), TELEGRAM_TOO_LONG, "204 Bytes: zu lang");
  }
  {
    // (e) Uebertragungsfehler in einem Nutzbyte
    uint8_t kaputt[TELEGRAM_DATA_LEN];
    memcpy(kaputt, antwort, sizeof kaputt);
    kaputt[42] = (uint8_t)(kaputt[42] + 1);
    pruefe_ergebnis(check_telegram(kaputt, TELEGRAM_DATA_LEN), TELEGRAM_BAD_CHECKSUM, "ein gekipptes Byte: Pruefsummenfehler");
  }

  printf("\n== Fall 4: verschobener Bytestrom (Restdaten im Empfangspuffer) ==\n");
  // Nachbildung des realen Ablaufs: nach einem Serial-Timeout stehen k Bytes
  // einer alten Antwort im UART-Puffer, danach kommt die naechste Antwort.
  // readSerial las beides am Stueck. Ab 3.6.0 raeumt flush_serial_input() den
  // Puffer vor dem Senden - dieser Fall belegt, was passierte, solange er
  // stehen blieb, und dass die Pruefung ihn zusaetzlich abfaengt.
  int alt_akzeptiert = 0, neu_akzeptiert = 0, ueberlauf = 0, gesamt = 0;
  for (unsigned int k = 1; k < TELEGRAM_DATA_LEN; k++)
  {
    uint8_t puffer[MAXDATASIZE];
    unsigned int len = 0;
    for (unsigned int i = TELEGRAM_DATA_LEN - k; i < TELEGRAM_DATA_LEN && len < MAXDATASIZE; i++)
      puffer[len++] = antwort[i]; // Rest der vorigen Antwort
    for (unsigned int i = 0; i < TELEGRAM_DATA_LEN && len < MAXDATASIZE; i++)
      puffer[len++] = antwort[i]; // Anfang der neuen Antwort
    gesamt++;
    if (len >= MAXDATASIZE)
    {
      ueberlauf++; // greift schon der Ueberlaufschutz in readSerial
      continue;
    }
    if (alte_regel(puffer, len))
      alt_akzeptiert++;
    if (check_telegram(puffer, len) == TELEGRAM_OK)
      neu_akzeptiert++;
  }
  printf("  %d Verschiebungen, davon %d schon durch den Ueberlaufschutz erledigt\n", gesamt, ueberlauf);
  printf("  alte Regel akzeptiert: %d   neue Regel akzeptiert: %d\n", alt_akzeptiert, neu_akzeptiert);
  pruefe(neu_akzeptiert == 0, "kein verschobener Strom wird als Messdaten angenommen");

  printf("\n== Fall 5: Muell, dessen Laenge zum Laengenbyte passt (der 1/256-Pfad) ==\n");
  // Der ungünstigste Fall: die Laenge stimmt zufaellig mit dem Laengenbyte
  // ueberein. Dann entschied vorher allein die Pruefsumme - also 1 von 256.
  const int versuche = 200000;
  int alt = 0, neu = 0;
  for (int n = 0; n < versuche; n++)
  {
    uint8_t puffer[MAXDATASIZE];
    unsigned int len = 4u + (rnd_byte() % 250u); // 4..253 Bytes
    for (unsigned int i = 0; i < len; i++)
      puffer[i] = rnd_byte();
    puffer[1] = (uint8_t)(len - 3); // Laengenbyte passend faelschen
    if (alte_regel(puffer, len))
      alt++;
    if (check_telegram(puffer, len) == TELEGRAM_OK)
      neu++;
  }
  printf("  %d Zufallspuffer: alte Regel akzeptiert %d (%.3f %%, erwartet ~1/256 = 0.391 %%)\n",
         versuche, alt, 100.0 * alt / versuche);
  printf("  neue Regel akzeptiert %d (%.4f %%) - noetig sind zusaetzlich Typ 0x71 UND Laengenbyte 0xC8\n",
         neu, 100.0 * neu / versuche);
  pruefe(alt > versuche / 512, "die alte Regel liess tatsaechlich in der Groessenordnung 1/256 durch");
  pruefe(neu * 100 < alt, "die neue Regel laesst um mindestens zwei Groessenordnungen weniger durch");

  printf("\n%s (%d Fehler)\n", fehler == 0 ? "ALLE ZUSICHERUNGEN ERFUELLT" : "FEHLGESCHLAGEN", fehler);
  return (fehler == 0) ? 0 : 1;
}
