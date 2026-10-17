#pragma once
#include <stdint.h>

/*****************************************************************************/
/* Gesundheit der seriellen Strecke zur Waermepumpe (3.25.0, S4)             */
/*                                                                           */
/* Bewusst frei von Arduino-Abhaengigkeiten, gleiches Muster wie karenz.h:   */
/* derselbe Code laeuft in der Firmware (readSerial, timeout_serial, loop)   */
/* und im Hosttest (test/serielle_strecke_test.cpp). Nur so ist der          */
/* Ueberlauf von millis() ueberhaupt pruefbar.                               */
/*                                                                           */
/* WARUM ES DAS GIBT                                                         */
/* actual_data[] wird nur bei einem gueltigen Telegramm fortgeschrieben.     */
/* Reisst die Strecke ab (Kabel, Pegelwandler, WP-Platine), bleiben die      */
/* letzten Werte stehen - auf der Webseite, retained beim Broker und als     */
/* Ruecklesequelle des Notbetriebs. Hier steht, wie lange das letzte         */
/* gueltige Telegramm her ist, und wie viele Abfragen gelangen, verworfen    */
/* wurden oder unbeantwortet blieben.                                        */
/*                                                                           */
/* DREI ZAEHLER, EIN ERGEBNIS JE ABFRAGE                                     */
/* ok       gueltiges Telegramm                                              */
/* err      Bytes kamen, aber kein gueltiges Telegramm (Pruefsumme, Typ,     */
/*          unvollstaendig, zu lang, Pufferueberlauf)                        */
/* timeout  gar kein Byte bis zum Serial-Timeout: die Leitung ist stumm      */
/* Die Unterscheidung ist Absicht: "stumm" und "liefert Muell" haben         */
/* verschiedene Ursachen (Kabel/Pegelwandler gegen Baudrate/Stoerung).       */
/* Ein Fehlerzyklus zaehlt GENAU EINMAL: Nach err laeuft der Serial-Timeout  */
/* trotzdem noch ab (die Abfrage gilt erst dann als beendet) - ohne den      */
/* Merker abfrage_fehler wuerde derselbe Zyklus zusaetzlich als timeout      */
/* gezaehlt.                                                                 */
/*                                                                           */
/* WARUM ALTER MIT VORAB STATT FRIST ODER FLAG                               */
/* Die Lehre aus S1 (karenz.h): Eine Zeitdifferenz, die auch nach Ablauf     */
/* weiter ausgewertet wird, kippt nach einer Wrap-Grenze. Hier ist das       */
/* Gegenteil gewollt - das Alter soll bei einem Kabelabriss ueber Wochen     */
/* richtig weiterzaehlen (info/telegram_age, Sperre im Notbetrieb). Deshalb  */
/* faltet tick() je 2^30 ms (12,4 Tage) in ein 64-Bit-Vorab und schiebt den  */
/* Bezug nach: Die 32-Bit-Differenz bleibt unter der Naht, das Alter ist     */
/* ueber den millis()-Ueberlauf hinweg monoton.                              */
/*                                                                           */
/* VORBEDINGUNG                                                              */
/* serielle_strecke_tick() muss haeufiger als alle 2^30 ms laufen (die       */
/* Firmware ruft es in JEDEM loop()-Durchlauf). serielle_strecke_alter_ms()  */
/* ruft es selbst mit auf; ein Aufruf davor ist aber der klare Weg.          */
/*                                                                           */
/* uint32_t und NICHT unsigned long: auf dem ESP32 sind beide 32 Bit, auf    */
/* dem Mac ist unsigned long 64 Bit - der Hosttest wuerde den Ueberlauf      */
/* sonst mit einer Breite pruefen, die es auf dem Geraet nicht gibt.         */
/*****************************************************************************/

// Ab dieser Zeit ohne gueltiges Telegramm gilt die Strecke als stumm
// (Owner-Entscheid 2026-10-16). Der Abfragezyklus liegt bei rund 6 s, 30 s
// sind also fuenf ausgebliebene Antworten - ein einzelner Aussetzer, auch
// zwei, loesen nichts aus.
#define SERIELL_STUMM_SCHWELLE_MS 30000u

// Breite, die tick() je Schritt in das 64-Bit-Vorab faltet: 2^30 ms
#define SERIELL_FALTUNG_MS 0x40000000u

struct SerielleStrecke
{
  uint32_t ok;      // gueltige Telegramme seit dem Start (saettigt, kein Wrap)
  uint32_t err;     // verworfene Telegramme seit dem Start
  uint32_t timeout; // Abfragen ohne ein einziges Byte seit dem Start
  uint32_t bezug;   // millis() des letzten gueltigen Telegramms; davor: Start
  uint64_t vorab;   // schon gefaltete Millisekunden seit "bezug"
  bool abfrage_fehler;  // in der laufenden Abfrage schon als err gezaehlt
  bool stumm_gemeldet;  // Stand, den serielle_strecke_wechsel() zuletzt kannte
};

// Zaehler saettigen bei UINT32_MAX - ein Wrap auf 0 sah aus wie ein Neustart
inline uint32_t serielle_strecke_plus1(uint32_t zaehler)
{
  return (zaehler == UINT32_MAX) ? zaehler : (uint32_t)(zaehler + 1u);
}

/*****************************************************************************/
/* Beim Start: alles auf 0, das Alter zaehlt ab jetzt                        */
/*                                                                           */
/* Vor dem ersten Telegramm bezieht sich das Alter auf den Start. Eine       */
/* Strecke, die nach dem Booten nie antwortet, wird so nach 30 s stumm -     */
/* statt "unbekannt" zu bleiben und die Sperre nie zu ausloesen.             */
/*****************************************************************************/
inline void serielle_strecke_start(SerielleStrecke *s, uint32_t jetzt)
{
  if (!s)
    return;
  s->ok = 0;
  s->err = 0;
  s->timeout = 0;
  s->bezug = jetzt;
  s->vorab = 0;
  s->abfrage_fehler = false;
  s->stumm_gemeldet = false;
}

/*****************************************************************************/
/* Faltung gegen den Ueberlauf - siehe Kopf                                  */
/*****************************************************************************/
inline void serielle_strecke_tick(SerielleStrecke *s, uint32_t jetzt)
{
  if (!s)
    return;
  while ((uint32_t)(jetzt - s->bezug) >= SERIELL_FALTUNG_MS)
  {
    s->vorab += SERIELL_FALTUNG_MS;
    s->bezug = (uint32_t)(s->bezug + SERIELL_FALTUNG_MS);
  }
}

/*****************************************************************************/
/* Alter des letzten gueltigen Telegramms in Millisekunden                   */
/*****************************************************************************/
inline uint64_t serielle_strecke_alter_ms(SerielleStrecke *s, uint32_t jetzt)
{
  if (!s)
    return UINT64_MAX; // ohne Zustand: lieber "ewig" als "frisch"
  serielle_strecke_tick(s, jetzt);
  return s->vorab + (uint64_t)(uint32_t)(jetzt - s->bezug);
}

// Alter in Sekunden fuer info/telegram_age; saettigt bei UINT32_MAX
inline uint32_t serielle_strecke_alter_s(SerielleStrecke *s, uint32_t jetzt)
{
  const uint64_t sekunden = serielle_strecke_alter_ms(s, jetzt) / 1000u;
  return (sekunden > UINT32_MAX) ? UINT32_MAX : (uint32_t)sekunden;
}

/*****************************************************************************/
/* Ist die Strecke stumm? (Alter >= Schwelle)                                */
/*****************************************************************************/
inline bool serielle_strecke_stumm(SerielleStrecke *s, uint32_t jetzt)
{
  return serielle_strecke_alter_ms(s, jetzt) >= SERIELL_STUMM_SCHWELLE_MS;
}

/*****************************************************************************/
/* Ein gueltiges Telegramm ist eingegangen                                   */
/*****************************************************************************/
inline void serielle_strecke_ok(SerielleStrecke *s, uint32_t jetzt)
{
  if (!s)
    return;
  s->ok = serielle_strecke_plus1(s->ok);
  s->bezug = jetzt;
  s->vorab = 0;
}

/*****************************************************************************/
/* Eine neue Abfrage geht raus - der Merker gilt je Abfrage                  */
/*****************************************************************************/
inline void serielle_strecke_abfrage_beginn(SerielleStrecke *s)
{
  if (s)
    s->abfrage_fehler = false;
}

/*****************************************************************************/
/* Bytes kamen, aber kein gueltiges Telegramm - einmal je Abfrage            */
/*****************************************************************************/
inline void serielle_strecke_fehler(SerielleStrecke *s)
{
  if (!s || s->abfrage_fehler)
    return;
  s->abfrage_fehler = true;
  s->err = serielle_strecke_plus1(s->err);
}

/*****************************************************************************/
/* Der Serial-Timeout ist abgelaufen, die Abfrage ist unbeantwortet          */
/*                                                                           */
/* Aufzurufen nur, wenn die Abfrage noch offen war (serialquerysent). Hat    */
/* sie schon als err gezaehlt, ist sie beantwortet - mit Muell, aber nicht   */
/* stumm - und zaehlt nicht noch einmal.                                     */
/*****************************************************************************/
inline void serielle_strecke_timeout(SerielleStrecke *s)
{
  if (!s || s->abfrage_fehler)
    return;
  s->timeout = serielle_strecke_plus1(s->timeout);
}

/*****************************************************************************/
/* Hat sich der Zustand stumm/lebendig seit dem letzten Aufruf geaendert?    */
/*                                                                           */
/* Fuehrt nach, damit jeder Wechsel genau einmal gemeldet wird (MQTT-Log     */
/* und sofortiges Veroeffentlichen der Zaehler). *stumm_aus bekommt den      */
/* neuen Zustand. Eine Strecke, die nach dem Start sofort antwortet, meldet  */
/* nichts: der Anfangsstand ist "lebendig".                                  */
/*****************************************************************************/
inline bool serielle_strecke_wechsel(SerielleStrecke *s, uint32_t jetzt, bool *stumm_aus)
{
  if (!s)
    return false;
  const bool stumm = serielle_strecke_stumm(s, jetzt);
  if (stumm == s->stumm_gemeldet)
    return false;
  s->stumm_gemeldet = stumm;
  if (stumm_aus)
    *stumm_aus = stumm;
  return true;
}
