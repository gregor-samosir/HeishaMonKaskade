#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

/*****************************************************************************/
/* Ist die Hausteuerung noch da? - Zeitregeln der Verbindungswacht           */
/*                                                                           */
/* Bewusst frei von Arduino-Abhaengigkeiten, gleiches Muster wie             */
/* sendwindow.h, telegram.h und notbetrieb.h: derselbe Code laeuft in der    */
/* Firmware und im Hosttest (test/verbindung_test.cpp).                      */
/*                                                                           */
/* WARUM ES DAS GIBT (Owner-Beobachtung 2026-08-21, Vorhaben-Notbetrieb-     */
/* Weboberflaeche.md Abschnitt 9): Waehrend der Broker weg war, heizte die   */
/* Waermepumpe einfach weiter, mit dem zuletzt gesetzten Sollwert. Kein      */
/* Alarm, kein Hinweis, nichts. Der Ausfall wirkt sich erst mit Verzoegerung */
/* aus - im Sommer ueber Tage, im Januar ueber Stunden, wenn der Sollwert    */
/* der fallenden Aussentemperatur nicht mehr nachgefuehrt wird.              */
/*                                                                           */
/* Die Firmware WEISS es laengst (mqtt_client.connected() ist false, der     */
/* Reconnect laeuft im Backoff ins Leere), sie sagt es nur niemandem ausser  */
/* im MQTT-Log - und das geht in genau dieser Lage per Definition ins Leere. */
/* Diese Datei haelt fest, SEIT WANN, und beantwortet die eine Frage, die    */
/* jemand vor der Weboberflaeche hat: brauche ich den Notbetriebsknopf?      */
/*                                                                           */
/* Der Notbetriebsknopf selbst haengt NICHT an dieser Wacht - er ist von ihr */
/* vollstaendig unabhaengig und funktioniert auch, wenn hier etwas falsch    */
/* stuende. Das hier ist reine Auskunft.                                     */
/*****************************************************************************/

/*****************************************************************************/
/* Karenz: 5 Minuten                                                         */
/*                                                                           */
/* Unterhalb davon wird NICHTS gemeldet. Der Grund ist nicht der WLAN-       */
/* Wackler allein: Ein Neustart des ioBroker-Adapters oder des Containers    */
/* auf der Synology dauert regelmaessig ein bis zwei Minuten, und in dieser  */
/* Zeit ist der Broker weg, ohne dass etwas kaputt waere. Eine Stoermeldung, */
/* die von selbst wieder verschwindet, erzieht die Familie dazu, sie zu      */
/* uebersehen - und dann wird auch die echte uebersehen.                     */
/*                                                                           */
/* Nach oben ist die Karenz durch den Zweck begrenzt: Wer nach fuenf Minuten */
/* Ausfall auf die Seite schaut, hat einen echten Grund dafuer.              */
/*                                                                           */
/* Der Reconnect-Backoff (MQTT_RECONNECT_MIN 5 s bis MQTT_RECONNECT_MAX      */
/* 60 s) liegt vollstaendig darunter: In fuenf Minuten hat die Firmware      */
/* mindestens acht Verbindungsversuche hinter sich.                          */
/*****************************************************************************/
#define VERBINDUNG_KARENZ_MS 300000u

/*****************************************************************************/
/* Deckel der Ausfalldauer: 30 Tage                                          */
/*                                                                           */
/* millis() laeuft nach 49,7 Tagen ueber. Ohne Deckel stuende auf der Seite  */
/* nach einem sehr langen Ausfall wieder "seit 3 Minuten nicht erreichbar" - */
/* eine Luege genau in dem Moment, in dem die Anzeige zaehlt, und schlimmer  */
/* als gar keine Angabe. Ab 30 Tagen wird deshalb nicht mehr weitergezaehlt, */
/* sondern "mehr als 30 Tagen" gemeldet.                                     */
/*                                                                           */
/* 30 Tage liegen mit Absicht deutlich vor der Naht: Die Wacht muss die      */
/* Ueberschreitung nur irgendwann in den 19,7 Tagen dazwischen bemerken, und */
/* sie wird aus loop() im Millisekundentakt nachgefuehrt. (Unter der HALBEN  */
/* millis()-Breite von 24,85 Tagen liegt der Deckel damit nicht - das muss   */
/* er auch nicht: Die unsigned-Differenz ist bis zur vollen Naht eindeutig.) */
/*                                                                           */
/* 30 * 24 * 3600 * 1000 = 2.592.000.000 - passt in uint32_t (max 4.294 Mrd).*/
/*****************************************************************************/
#define VERBINDUNG_DECKEL_MS 2592000000u
#define VERBINDUNG_DECKEL_TAGE 30u

/*****************************************************************************/
/* Die vier Lagen                                                            */
/*                                                                           */
/* KARENZ ist bewusst ein eigener Wert und nicht mit VERBUNDEN               */
/* zusammengefasst: Die Seite zeigt in beiden Faellen dasselbe, aber wer die */
/* Statusroute liest, soll den Unterschied sehen koennen.                    */
/*                                                                           */
/* SEIT_NEUSTART ist der ehrliche Sonderfall: Hatte die Firmware seit dem    */
/* Einschalten NIE eine Verbindung, ist die wahre Ausfalldauer unbekannt -   */
/* der Ausfall kann viel aelter sein als das Geraet. "seit 7 Minuten" waere  */
/* dann falsch. Die Seite sagt in diesem Fall "seit dem Neustart".           */
/*****************************************************************************/
enum VerbindungsLage
{
  VERBINDUNG_VERBUNDEN = 0,
  VERBINDUNG_KARENZ = 1,               // getrennt, aber noch innerhalb der Karenz
  VERBINDUNG_GESTOERT = 2,             // getrennt, Karenz vorbei, Dauer bekannt
  VERBINDUNG_GESTOERT_SEIT_NEUSTART = 3 // getrennt, Karenz vorbei, nie verbunden
};

/*****************************************************************************/
/* Der Zustand der Wacht                                                     */
/*                                                                           */
/* ausfall_ms wird FORTGESCHRIEBEN und nicht bei jeder Abfrage neu gerechnet.*/
/* Das ist der Kern der Ueberlaufsicherheit: (now - getrennt_seit) wird nach */
/* 49,7 Tagen wieder klein, ein bei der Abfrage gerechneter Wert fiele damit */
/* unter die Karenz zurueck und die Stoermeldung verschwaende von selbst.    */
/* Die beiden Merker karenz_ueberschritten und ueber_deckel koennen dagegen  */
/* nur in eine Richtung kippen und werden erst beim Verbindungsaufbau        */
/* zurueckgesetzt.                                                           */
/*****************************************************************************/
struct VerbindungsWacht
{
  uint32_t getrennt_seit;     // millis() im Moment des Verbindungsverlusts
  uint32_t ausfall_ms;        // fortgeschriebene Dauer, gedeckelt
  bool getrennt;              // aktueller Zustand
  bool je_verbunden;          // seit dem Neustart schon einmal verbunden gewesen
  bool karenz_ueberschritten; // Karenz einmal ueberschritten (kippt nicht zurueck)
  bool ueber_deckel;          // Dauer hat den 30-Tage-Deckel erreicht
};

/*****************************************************************************/
/* Anfangszustand: getrennt, noch nie verbunden                              */
/*                                                                           */
/* Das ist die Wahrheit im Moment des Einschaltens - setupMqtt() versucht    */
/* die erste Verbindung erst danach. Wuerde hier "verbunden" stehen, meldete */
/* die Seite eines Geraets, das den Broker nie erreicht hat, dauerhaft alles */
/* in Ordnung.                                                                */
/*****************************************************************************/
inline void verbindung_init(VerbindungsWacht *w, uint32_t now)
{
  if (!w)
    return;
  w->getrennt_seit = now;
  w->ausfall_ms = 0;
  w->getrennt = true;
  w->je_verbunden = false;
  w->karenz_ueberschritten = false;
  w->ueber_deckel = false;
}

/*****************************************************************************/
/* Zustand nachfuehren - aus loop() in jedem Durchlauf                       */
/*                                                                           */
/* Rueckgabe true genau dann, wenn die Verbindung in DIESEM Aufruf           */
/* zurueckgekehrt ist und der Ausfall die Karenz ueberschritten hatte; dann  */
/* steht in *ausfall_s die Dauer in Sekunden. Das ist der Ausloeser fuer die */
/* Log-Zeile nach der Rueckkehr - dasselbe Muster wie wifiOutageSeconds.     */
/* Ein kurzer Aussetzer unterhalb der Karenz meldet nichts: Er war schon auf */
/* der Seite kein Thema und soll es im Log auch nicht sein.                   */
/*                                                                            */
/* Die Differenz laeuft in unsigned-Arithmetik (now - getrennt_seit) und ist  */
/* damit ueber die millis()-Naht hinweg richtig - siehe sendwindow.h, wo      */
/* dieselbe Regel mit einer Gegenprobe belegt ist.                            */
/*****************************************************************************/
inline bool verbindung_nachfuehren(VerbindungsWacht *w, bool verbunden,
                                   uint32_t now, uint32_t *ausfall_s)
{
  if (!w)
    return false;

  if (verbunden)
  {
    bool meldenswert = w->getrennt && w->karenz_ueberschritten;
    if (meldenswert && ausfall_s)
    {
      *ausfall_s = w->ausfall_ms / 1000u;
    }
    // Verbindung steht: alle Merker zurueck auf Anfang
    w->getrennt = false;
    w->je_verbunden = true;
    w->ausfall_ms = 0;
    w->karenz_ueberschritten = false;
    w->ueber_deckel = false;
    return meldenswert;
  }

  if (!w->getrennt) // erster Durchlauf ohne Verbindung: Zeitpunkt merken
  {
    w->getrennt = true;
    w->getrennt_seit = now;
    w->ausfall_ms = 0;
    return false;
  }

  // Dauer fortschreiben. Der Deckel kippt nur einmal und nie zurueck; ohne
  // ihn liefe der Wert nach 49,7 Tagen ueber und finge wieder bei 0 an.
  uint32_t verstrichen = (uint32_t)(now - w->getrennt_seit);
  if (w->ueber_deckel || verstrichen >= VERBINDUNG_DECKEL_MS)
  {
    w->ueber_deckel = true;
    w->ausfall_ms = VERBINDUNG_DECKEL_MS;
  }
  else
  {
    w->ausfall_ms = verstrichen;
  }

  if (w->ausfall_ms >= VERBINDUNG_KARENZ_MS)
  {
    w->karenz_ueberschritten = true;
  }
  return false;
}

/*****************************************************************************/
/* Welche Lage gilt gerade?                                                  */
/*                                                                           */
/* Liest nur die fortgeschriebenen Merker aus und rechnet selbst NICHT mit   */
/* der Zeit - deshalb kann die Antwort zwischen zwei Nachfuehrungen nicht    */
/* springen, und der Ueberlauf ist hier kein Thema mehr.                     */
/*****************************************************************************/
inline VerbindungsLage verbindung_lage(const VerbindungsWacht *w)
{
  if (!w || !w->getrennt)
    return VERBINDUNG_VERBUNDEN;
  if (!w->karenz_ueberschritten)
    return VERBINDUNG_KARENZ;
  return w->je_verbunden ? VERBINDUNG_GESTOERT : VERBINDUNG_GESTOERT_SEIT_NEUSTART;
}

/*****************************************************************************/
/* Ausfalldauer in Sekunden                                                  */
/*                                                                           */
/* Nur fuer die Anzeige gedacht. Bei bestehender Verbindung 0 - eine         */
/* "Dauer" gaebe es dort nicht, und 0 ist der einzige Wert, der nicht        */
/* versehentlich als Zeitangabe durchgeht.                                   */
/*****************************************************************************/
inline uint32_t verbindung_ausfall_sekunden(const VerbindungsWacht *w)
{
  if (!w || !w->getrennt)
    return 0;
  return w->ausfall_ms / 1000u;
}

/*****************************************************************************/
/* Die Dauer in Worten - passt in "seit ... nicht erreichbar"                */
/*                                                                           */
/* Deshalb Dativ ("seit 1 Stunde", "seit 2 Stunden", "seit 1 Tag", "seit 3   */
/* Tagen") und keine Nachkommastellen: Wer vor der Seite steht, will wissen, */
/* ob das Minuten oder Tage sind, nicht ob es 14,3 Minuten waren.            */
/*                                                                           */
/* Die Schwellen sind so gewaehlt, dass die groebere Einheit erst uebernimmt,*/
/* wenn sie mindestens zwei ihrer Einheiten voll hat: bis 89 Minuten wird in */
/* Minuten gezaehlt, bis 47 Stunden in Stunden. "seit 1 Stunde" kann so gar  */
/* nicht entstehen, "seit 90 Minuten" ist die genauere Auskunft.             */
/*                                                                           */
/* Die Singularformen bleiben trotzdem drin: Sie kosten nichts und die       */
/* Funktion soll fuer jeden Eingabewert einen richtigen Satz liefern, auch   */
/* wenn ein Aufrufer sie spaeter ohne die Schwellen benutzt.                 */
/*****************************************************************************/
inline void verbindung_dauer_text(char *out, size_t len, uint32_t sekunden,
                                  bool ueber_deckel)
{
  if (!out || len == 0)
    return;

  if (ueber_deckel)
  {
    (void)snprintf(out, len, "mehr als %u Tagen", VERBINDUNG_DECKEL_TAGE);
    return;
  }

  if (sekunden < 5400u) // unter 90 Minuten
  {
    uint32_t minuten = sekunden / 60u;
    (void)snprintf(out, len, "%u %s", minuten, (minuten == 1u) ? "Minute" : "Minuten");
    return;
  }

  if (sekunden < 172800u) // unter 48 Stunden
  {
    uint32_t stunden = sekunden / 3600u;
    (void)snprintf(out, len, "%u %s", stunden, (stunden == 1u) ? "Stunde" : "Stunden");
    return;
  }

  uint32_t tage = sekunden / 86400u;
  (void)snprintf(out, len, "%u %s", tage, (tage == 1u) ? "Tag" : "Tagen");
}
