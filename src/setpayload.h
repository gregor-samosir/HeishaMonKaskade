#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*****************************************************************************/
/* Nutzlast eines Set-Befehls und Buchfuehrung ueber abgelehnte (3.25.0, S6) */
/*                                                                           */
/* Bewusst frei von Arduino-Abhaengigkeiten, gleiches Muster wie             */
/* sendwindow.h: derselbe Code laeuft in der Firmware (commands.cpp, loop)   */
/* und im Hosttest (test/setpayload_test.cpp).                               */
/*                                                                           */
/* WARUM ES DAS GIBT                                                         */
/* build_heatpump_command() lehnt jeden Payload ab, der keine Ganzzahl ist   */
/* ("33.5", "1e1", ""), und jeden Wert ausserhalb von min/max. Der Befehl    */
/* geht verloren, die Waermepumpe behaelt ihren alten Wert, und der set-     */
/* Datenpunkt im ioBroker zeigt weiter den eingetippten. Die Ablehnung       */
/* stand nur als "Error:"-Zeile im Logring (32 Zeilen, binnen Minuten vom    */
/* Re-Assert verdraengt). Vom 2026-10-10 bis 2026-10-15 verwarf die Bridge   */
/* so die Dezimalwerte des Notbetriebszweigs, ohne dass es jemand bemerkte.  */
/* Jetzt gibt es einen Zaehler und den letzten abgelehnten Befehl - eine     */
/* Stelle fuer alle SET-Befehle, ohne je Kanal eine Ruecklesung zu pflegen.  */
/*                                                                           */
/* VERHALTEN BLEIBT, WAS ES WAR                                              */
/* Die Ganzzahlregel ist die des bisherigen strtol(msg, &end, 10): Fuehrender */
/* Leerraum und ein Vorzeichen sind erlaubt (" 33", "+33"), jeder Rest nach  */
/* der Zahl lehnt ab ("33 ", "33.5", "1e1"), "0x1A" scheitert an der Basis   */
/* 10, "" und "-" haben keine Ziffer. Neu ist nur, dass die Regel testbar    */
/* ist. Ein Ueberlauf wird auf int32_t geklemmt: auf dem ESP32 liefert       */
/* strtol dann LONG_MAX (32 Bit), auf dem Mac waere long 64 Bit - die Klemme */
/* gibt dem Hosttest dasselbe Ergebnis wie dem Geraet. Der Bereichstest der  */
/* Tabelle (min/max) lehnt den geklemmten Wert danach ab.                    */
/*****************************************************************************/

enum SetPayloadErgebnis
{
  SETPAYLOAD_OK = 0,
  SETPAYLOAD_KEINE_GANZZAHL
};

inline SetPayloadErgebnis setpayload_ganzzahl(const char *msg, int32_t *wert)
{
  if (!msg || !wert)
    return SETPAYLOAD_KEINE_GANZZAHL;

  char *ende = nullptr;
  const long long roh = strtoll(msg, &ende, 10);
  if (ende == msg || *ende != '\0')
    return SETPAYLOAD_KEINE_GANZZAHL;

  // auf int32_t klemmen (siehe Kopf), kein stilles Abschneiden der Bits
  if (roh > INT32_MAX)
    *wert = INT32_MAX;
  else if (roh < INT32_MIN)
    *wert = INT32_MIN;
  else
    *wert = (int32_t)roh;
  return SETPAYLOAD_OK;
}

/*****************************************************************************/
/* Buchfuehrung ueber abgelehnte Befehle                                     */
/*                                                                           */
/* letzter: "Name=Wert", auf 64 Zeichen gekuerzt. Name ist der Teil des      */
/* Topics hinter dem letzten "/" - die Praefix-Wurzel steht ohnehin in jedem */
/* anderen Topic. Der Wert kommt vom Broker und ist Fremdtext: Alles ausser  */
/* druckbarem ASCII wird zu '?', damit weder Steuerzeichen noch halbe        */
/* UTF-8-Folgen in einem retained Topic landen.                              */
/*                                                                           */
/* Veroeffentlicht wird NICHT aus dem MQTT-Callback: PubSubClient teilt       */
/* einen Puffer fuer ein- und ausgehende Pakete, topic und msg zeigen        */
/* hinein - ein publish() dort uebersaehe seine eigenen Argumente. Der       */
/* Callback setzt nur "neu"; loop() fragt setabgelehnt_faellig() und          */
/* veroeffentlicht. Der Mindestabstand schuetzt den Broker vor einem          */
/* Schwall abgelehnter Befehle (jeder Re-Assert-Takt kann dutzende bringen). */
/*****************************************************************************/
#define SETABGELEHNT_LETZTER_MAX 64u        // Zeichen im Topic set_abgelehnt_letzter
#define SETABGELEHNT_MINDESTABSTAND_MS 2000u // frueheste Wiederholung des Veroeffentlichens

struct SetAbgelehnt
{
  uint32_t zaehler;                            // abgelehnte Befehle seit dem Start
  char letzter[SETABGELEHNT_LETZTER_MAX + 1u]; // "Name=Wert", nullterminiert
  bool neu;                                    // seit dem letzten Veroeffentlichen geaendert
  bool hat_veroeffentlicht;                    // letzte_pub gilt erst danach
  uint32_t letzte_pub;                         // millis() des letzten Veroeffentlichens
};

inline void setabgelehnt_start(SetAbgelehnt *s)
{
  if (!s)
    return;
  s->zaehler = 0;
  s->letzter[0] = '\0';
  s->neu = false;
  s->hat_veroeffentlicht = false;
  s->letzte_pub = 0;
}

inline void setabgelehnt_merken(SetAbgelehnt *s, const char *topic, const char *wert)
{
  if (!s)
    return;

  // Zaehler saettigt, ein Wrap auf 0 sah aus wie ein Neustart
  if (s->zaehler != UINT32_MAX)
    s->zaehler++;

  // Name = Rest hinter dem letzten '/'; ohne Topic ein Platzhalter
  const char *name = topic ? topic : "?";
  const char *schraeg = strrchr(name, '/');
  if (schraeg)
    name = schraeg + 1;
  if (!wert)
    wert = "";

  // Zeichen fuer Zeichen kopieren und gleich klaeren - ein snprintf mit %s
  // wuerde Steuerzeichen durchlassen
  size_t n = 0;
  for (const char *p = name; *p != '\0' && n < SETABGELEHNT_LETZTER_MAX; p++)
    s->letzter[n++] = ((unsigned char)*p >= 0x20u && (unsigned char)*p <= 0x7Eu) ? *p : '?';
  if (n < SETABGELEHNT_LETZTER_MAX)
    s->letzter[n++] = '=';
  for (const char *p = wert; *p != '\0' && n < SETABGELEHNT_LETZTER_MAX; p++)
    s->letzter[n++] = ((unsigned char)*p >= 0x20u && (unsigned char)*p <= 0x7Eu) ? *p : '?';
  s->letzter[n] = '\0';

  s->neu = true;
}

// Soll jetzt veroeffentlicht werden? Die Differenz laeuft in unsigned-
// Arithmetik. Die Frist wird nur geprueft, solange "neu" ansteht; liegt das
// letzte Veroeffentlichen mehr als 2^32 ms zurueck, kann die Differenz in das
// 2-s-Fenster zurueckfallen und das Veroeffentlichen um hoechstens 2 s
// verzoegern - das ist harmlos, im Gegensatz zu einer Sperre.
inline bool setabgelehnt_faellig(const SetAbgelehnt *s, uint32_t jetzt)
{
  if (!s || !s->neu)
    return false;
  if (!s->hat_veroeffentlicht)
    return true;
  return (uint32_t)(jetzt - s->letzte_pub) >= SETABGELEHNT_MINDESTABSTAND_MS;
}

inline void setabgelehnt_veroeffentlicht(SetAbgelehnt *s, uint32_t jetzt)
{
  if (!s)
    return;
  s->neu = false;
  s->hat_veroeffentlicht = true;
  s->letzte_pub = jetzt;
}
