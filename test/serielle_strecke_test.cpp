// Nachweis fuer die Regeln der seriellen Strecke (src/serielle_strecke.h,
// 3.25.0, Befund S4 der Durchsicht 2026-10-15).
//
// Geprueft wird der Code, der auch auf dem Geraet laeuft: die Datei wird hier
// direkt eingebunden, es gibt keine Nachbildung. Dasselbe Muster wie
// test/karenz_test.cpp und test/telnetlog_test.cpp.
//
// Der Anlass: Reisst die Strecke zur Waermepumpe ab, blieben bis 3.24.0 die
// letzten Werte stehen, ohne dass irgendwo eine Zahl davon erzaehlte. Jetzt
// gibt es Zaehler (ok, err, timeout) und das Alter des letzten gueltigen
// Telegramms; ab 30 s gilt die Strecke als stumm.
//
// Fall 7 und 8 pruefen das Alter ueber die Naeht des millis()-Ueberlaufs und
// der Faltung (2^30 ms). Fall 9 baut den Fehler der vorzeichenbehafteten Frist
// (Form von S1) als Gegenbeispiel nach und belegt damit, dass Fall 8 die Naht
// wirklich trifft.
//
// Bauen und ausfuehren:
//   c++ -std=c++17 -O2 -Wall -o /tmp/serielle_strecke_test test/serielle_strecke_test.cpp
//   /tmp/serielle_strecke_test   (Rueckgabewert != 0 = Test fehlgeschlagen)

#include <cstdio>
#include <cstdint>

#include "../src/serielle_strecke.h"

static int fehler = 0;

// eine Zusicherung mit Klartext, damit der CI-Log ohne Debugger lesbar ist
static void pruefe(bool bedingung, const char *was)
{
  printf("  [%s] %s\n", bedingung ? "ok " : "FEHLER", was);
  if (!bedingung)
    fehler++;
}

// 2^31 ms: die Naht der vorzeichenbehafteten Regel
static const uint32_t NAHT = 0x80000000u;

int main()
{
  SerielleStrecke s;

  printf("Fall 1: Start - alles 0, Alter zaehlt ab dem Start, nichts stumm\n");
  serielle_strecke_start(&s, 1000u);
  pruefe(s.ok == 0 && s.err == 0 && s.timeout == 0, "Zaehler 0");
  pruefe(serielle_strecke_alter_ms(&s, 1000u) == 0, "Alter 0 im Startmoment");
  pruefe(serielle_strecke_alter_s(&s, 1000u + 12345u) == 12, "12345 ms -> 12 s (abgerundet)");
  pruefe(!serielle_strecke_stumm(&s, 1000u + 29999u), "29,999 s: noch nicht stumm");
  pruefe(serielle_strecke_stumm(&s, 1000u + 30000u), "30,000 s: stumm (Grenze gilt)");

  printf("Fall 2: nach dem Start nie ein Telegramm - bleibt stumm, ok bleibt 0\n");
  pruefe(serielle_strecke_stumm(&s, 1000u + 600000u), "nach 10 min stumm");
  pruefe(s.ok == 0, "ok bleibt 0");

  printf("Fall 3: ein gueltiges Telegramm setzt das Alter zurueck\n");
  serielle_strecke_ok(&s, 61000u);
  pruefe(s.ok == 1, "ok = 1");
  pruefe(serielle_strecke_alter_ms(&s, 61000u) == 0, "Alter 0");
  pruefe(!serielle_strecke_stumm(&s, 61000u + 29999u), "29,999 s danach: lebendig");
  pruefe(serielle_strecke_stumm(&s, 61000u + 30000u), "30,000 s danach: stumm");

  printf("Fall 4: Fehlerzyklus zaehlt genau einmal (err ODER timeout)\n");
  serielle_strecke_start(&s, 0u);
  // Abfrage 1: Bytes kamen, aber verworfen -> err; der Serial-Timeout kommt
  // danach trotzdem noch und darf nicht zusaetzlich zaehlen
  serielle_strecke_abfrage_beginn(&s);
  serielle_strecke_fehler(&s);
  serielle_strecke_timeout(&s);
  pruefe(s.err == 1 && s.timeout == 0, "verworfenes Telegramm: err 1, timeout 0");
  // ein zweiter Fehler in derselben Abfrage (z. B. Pufferueberlauf nach
  // unvollstaendigem Lesen) zaehlt ebenfalls nicht doppelt
  serielle_strecke_abfrage_beginn(&s);
  serielle_strecke_fehler(&s);
  serielle_strecke_fehler(&s);
  pruefe(s.err == 2, "zweimal fehler() in einer Abfrage = ein err");
  // Abfrage 3: kein Byte -> timeout
  serielle_strecke_abfrage_beginn(&s);
  serielle_strecke_timeout(&s);
  pruefe(s.err == 2 && s.timeout == 1, "stumme Abfrage: timeout 1, err unveraendert");
  // Gegenprobe: ohne den Merker wuerde Fall 4a doppelt zaehlen. Nachgebaut:
  // abfrage_beginn() auch zwischen fehler() und timeout() ruft den Merker leer.
  serielle_strecke_abfrage_beginn(&s);
  serielle_strecke_fehler(&s);
  serielle_strecke_abfrage_beginn(&s); // so, als gaebe es den Merker nicht
  serielle_strecke_timeout(&s);
  pruefe(s.err == 3 && s.timeout == 2, "Gegenprobe: ohne Merker zaehlte der Zyklus doppelt");

  printf("Fall 5: Zaehler saettigen statt auf 0 zu springen\n");
  serielle_strecke_start(&s, 0u);
  s.ok = UINT32_MAX - 1u;
  serielle_strecke_ok(&s, 10u);
  pruefe(s.ok == UINT32_MAX, "UINT32_MAX - 1 -> UINT32_MAX");
  serielle_strecke_ok(&s, 20u);
  pruefe(s.ok == UINT32_MAX, "bleibt bei UINT32_MAX, kein Wrap auf 0");
  s.err = UINT32_MAX;
  serielle_strecke_abfrage_beginn(&s);
  serielle_strecke_fehler(&s);
  pruefe(s.err == UINT32_MAX, "err saettigt ebenso");
  s.timeout = UINT32_MAX;
  serielle_strecke_abfrage_beginn(&s);
  serielle_strecke_timeout(&s);
  pruefe(s.timeout == UINT32_MAX, "timeout saettigt ebenso");

  printf("Fall 6: Wechsel stumm <-> lebendig wird je Wechsel genau einmal gemeldet\n");
  serielle_strecke_start(&s, 5000u);
  bool stumm = false;
  pruefe(!serielle_strecke_wechsel(&s, 5000u + 10000u, &stumm), "10 s: kein Wechsel (Anfangsstand lebendig)");
  pruefe(serielle_strecke_wechsel(&s, 5000u + 30000u, &stumm) && stumm, "30 s: Wechsel auf stumm");
  pruefe(!serielle_strecke_wechsel(&s, 5000u + 40000u, &stumm), "40 s: nicht erneut gemeldet");
  serielle_strecke_ok(&s, 5000u + 41000u);
  pruefe(serielle_strecke_wechsel(&s, 5000u + 41000u, &stumm) && !stumm, "Telegramm: Wechsel auf lebendig");
  pruefe(!serielle_strecke_wechsel(&s, 5000u + 42000u, &stumm), "danach nichts mehr");

  printf("Fall 7: Alter ueber den millis()-Ueberlauf (Telegramm kurz vor 2^32)\n");
  const uint32_t knapp = 0xFFFFFFFFu - 9999u; // 10 s vor dem Ueberlauf
  serielle_strecke_start(&s, 0u);
  serielle_strecke_ok(&s, knapp);
  pruefe(!serielle_strecke_stumm(&s, knapp + 20000u), "20 s danach (jetzt hinter dem Ueberlauf): lebendig");
  pruefe(serielle_strecke_stumm(&s, knapp + 30000u), "30 s danach: stumm");
  pruefe(serielle_strecke_alter_s(&s, knapp + 45000u) == 45, "Alter 45 s ueber die Naht hinweg");

  printf("Fall 8: Alter ueber die Faltung (2^30 ms) und die Naht 2^31 ms hinweg\n");
  serielle_strecke_start(&s, 0u);
  serielle_strecke_ok(&s, 123456u);
  // Wochenlang kein Telegramm; loop() ruft tick() laufend (hier: im Takt von
  // einer Stunde, deutlich haeufiger als die Vorbedingung verlangt).
  uint32_t jetzt = 123456u;
  const uint64_t stunde = 3600000u;
  uint64_t verstrichen = 0;
  bool ueberall_stumm = true;
  for (int i = 0; i < 24 * 40; i++) // 40 Tage - ueber 2^31 ms (24,86 d) hinaus
  {
    jetzt = (uint32_t)(jetzt + stunde);
    verstrichen += stunde;
    serielle_strecke_tick(&s, jetzt);
    if (!serielle_strecke_stumm(&s, jetzt))
      ueberall_stumm = false;
  }
  pruefe(ueberall_stumm, "40 Tage lang durchgehend stumm (kippt an keiner Naht zurueck)");
  pruefe(serielle_strecke_alter_ms(&s, jetzt) == verstrichen, "Alter in ms stimmt exakt nach 40 Tagen");
  pruefe(serielle_strecke_alter_s(&s, jetzt) == (uint32_t)(verstrichen / 1000u), "Alter in s stimmt");
  serielle_strecke_ok(&s, jetzt);
  pruefe(!serielle_strecke_stumm(&s, jetzt), "ein Telegramm beendet das sofort");
  pruefe(serielle_strecke_alter_ms(&s, jetzt) == 0, "Alter wieder 0");

  printf("Fall 9: Gegenbeispiel - die vorzeichenbehaftete Frist kippt an der Naht\n");
  // Die Form von S1: Frist = letztes Telegramm + 30 s, geprueft mit
  // (long)(jetzt - frist) >= 0. Nach 2^31 ms ist die Differenz wieder negativ,
  // die Strecke gaelte als "lebendig" - das zeigt, dass Fall 8 die Naht trifft.
  const uint32_t letzte = 1000u;
  const uint32_t frist = letzte + 30000u;
  const uint32_t nach_naht = letzte + NAHT + 40000u; // Frist + 2^31 ms + 10 s
  const bool alt_stumm = ((int32_t)(nach_naht - frist) >= 0);
  pruefe(!alt_stumm, "vorzeichenbehaftet: nach 2^31 ms faelschlich nicht stumm (das ist der Fehler)");
  serielle_strecke_start(&s, 0u);
  serielle_strecke_ok(&s, letzte);
  // die Firmware ruft tick() laufend; in grossen Schritten bis hinter die Naht
  for (uint64_t t = letzte; t < (uint64_t)letzte + NAHT + 40000u; t += 600000u)
    serielle_strecke_tick(&s, (uint32_t)t);
  pruefe(serielle_strecke_stumm(&s, nach_naht), "mit Faltung: nach 2^31 ms weiter stumm");

  printf("Fall 10: Nullzeiger sind harmlos und liefern nie \"frisch\"\n");
  serielle_strecke_start(nullptr, 0u);
  serielle_strecke_tick(nullptr, 0u);
  serielle_strecke_ok(nullptr, 0u);
  serielle_strecke_fehler(nullptr);
  serielle_strecke_timeout(nullptr);
  serielle_strecke_abfrage_beginn(nullptr);
  pruefe(serielle_strecke_stumm(nullptr, 0u), "ohne Zustand gilt die Strecke als stumm");
  pruefe(!serielle_strecke_wechsel(nullptr, 0u, nullptr), "ohne Zustand kein Wechsel");

  printf("\n%s (%d Fehler)\n", fehler == 0 ? "ALLE FAELLE OK" : "FEHLGESCHLAGEN", fehler);
  return fehler == 0 ? 0 : 1;
}
