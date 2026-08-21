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
/* ZWEI AUSFAELLE, EINE FOLGE. Beobachtet wurde der erste, gemeint sind      */
/* beide:                                                                    */
/*                                                                           */
/*   1. Der Broker ist weg (ioBroker aus). Die Firmware weiss das laengst -  */
/*      mqtt_client.connected() ist false, der Reconnect laeuft im Backoff   */
/*      ins Leere -, sie sagte es nur niemandem ausser im MQTT-Log, und das  */
/*      geht in genau dieser Lage per Definition ins Leere.                  */
/*   2. Der Broker laeuft, aber die Kaskadenregelung rechnet nicht mehr      */
/*      (Node-RED-Container weg, Flow im Fehler). Von aussen sieht alles     */
/*      gesund aus; die Waermepumpe bekommt trotzdem keine Vorgaben mehr.    */
/*                                                                           */
/* Fuer den Menschen vor der Weboberflaeche ist die Folge dieselbe: Niemand  */
/* fuehrt den Sollwert nach. Beide Faelle laufen deshalb ueber DIESELBE      */
/* Anzeige, nur mit unterschiedlichem Text - wer zum Server im Keller laeuft, */
/* soll wissen, ob dort ueberhaupt etwas zu holen ist.                       */
/*                                                                           */
/* Der Notbetriebsknopf haengt NICHT an dieser Wacht - er ist von ihr        */
/* vollstaendig unabhaengig und funktioniert auch, wenn hier etwas falsch    */
/* stuende. Das hier ist reine Auskunft.                                     */
/*****************************************************************************/

/*****************************************************************************/
/* Karenz fuer den Broker-Ausfall: 5 Minuten                                 */
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
/* Karenz fuer die stumme Steuerung: 12 Minuten                              */
/*                                                                           */
/* Der Re-Assert des Hauptmodus-Verteilers kommt alle 300,0 s - am           */
/* 2026-08-21 an H2 gemessen, nicht angenommen: zwei Takte im Abstand von    */
/* exakt 300,0 s, je Takt sieben empfangene Kommandos. Ein einzelner         */
/* ausgefallener Takt ist noch kein Ausfall; zwoelf Minuten decken zwei      */
/* verpasste Takte samt Reserve ab.                                          */
/*                                                                           */
/* Deutlich groesser als die Broker-Karenz zu sein ist kein Zufall: Hier     */
/* wird auf ein AUSBLEIBEN gewartet, und das ist die unsicherere Aussage.    */
/*****************************************************************************/
#define VERBINDUNG_STUMM_KARENZ_MS 720000u

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
/* Der Kern: ein laufender Ausfall                                           */
/*                                                                           */
/* Beide Faelle - Broker weg und Steuerung stumm - brauchen dasselbe: einen  */
/* Beginn, eine mitlaufende Dauer und die Frage, ob die Karenz ueberschritten */
/* ist. Das steht deshalb EINMAL hier und nicht zweimal nebeneinander; die   */
/* Ueberlauffestigkeit ist der subtile Teil, und zweimal hingeschrieben ist  */
/* zweimal Gelegenheit, sie falsch zu machen.                                */
/*                                                                           */
/* dauer_ms wird FORTGESCHRIEBEN und nicht bei jeder Abfrage neu gerechnet.  */
/* Das ist der Kern der Ueberlaufsicherheit: (now - seit) wird nach 49,7     */
/* Tagen wieder klein, ein bei der Abfrage gerechneter Wert fiele damit      */
/* unter die Karenz zurueck und die Stoermeldung verschwaende von selbst.    */
/* Die Merker karenz_ueber und ueber_deckel koennen dagegen nur in eine      */
/* Richtung kippen und gehen erst beim Zuruecksetzen wieder auf false.       */
/*****************************************************************************/
struct Ausfall
{
  uint32_t seit;     // millis() im Moment des Ausfallbeginns
  uint32_t dauer_ms; // fortgeschriebene Dauer, gedeckelt
  bool laeuft;       // Ausfall ist im Gange
  bool karenz_ueber; // Karenz einmal ueberschritten (kippt nicht zurueck)
  bool ueber_deckel; // Dauer hat den 30-Tage-Deckel erreicht
};

// Kein Ausfall: alle Merker zurueck auf Anfang.
inline void ausfall_zuruecksetzen(Ausfall *a)
{
  if (!a)
    return;
  a->seit = 0;
  a->dauer_ms = 0;
  a->laeuft = false;
  a->karenz_ueber = false;
  a->ueber_deckel = false;
}

// Ausfall beginnt jetzt. Ein bereits laufender Ausfall wird NICHT neu
// gestartet - sonst setzte jeder Aufruf die Uhr zurueck und die Dauer bliebe
// ewig bei null.
inline void ausfall_beginnen(Ausfall *a, uint32_t now)
{
  if (!a || a->laeuft)
    return;
  ausfall_zuruecksetzen(a);
  a->laeuft = true;
  a->seit = now;
}

// Dauer fortschreiben. Muss haeufiger als alle 19,7 Tage aufgerufen werden,
// damit der Deckel die Ueberlaufnaht sicher vor der Differenz erreicht - aus
// loop() heraus ist das reichlich erfuellt.
inline void ausfall_fortschreiben(Ausfall *a, uint32_t now, uint32_t karenz)
{
  if (!a || !a->laeuft)
    return;

  uint32_t verstrichen = (uint32_t)(now - a->seit);
  if (a->ueber_deckel || verstrichen >= VERBINDUNG_DECKEL_MS)
  {
    a->ueber_deckel = true;
    a->dauer_ms = VERBINDUNG_DECKEL_MS;
  }
  else
  {
    a->dauer_ms = verstrichen;
  }

  if (a->dauer_ms >= karenz)
  {
    a->karenz_ueber = true;
  }
}

// Meldenswert ist ein beendeter Ausfall nur, wenn er die Karenz ueberschritten
// hatte - was auf der Seite nie stand, soll auch nicht ins Log.
inline bool ausfall_meldenswert(const Ausfall *a)
{
  return a && a->laeuft && a->karenz_ueber;
}

inline uint32_t ausfall_sekunden(const Ausfall *a)
{
  return (a && a->laeuft) ? (a->dauer_ms / 1000u) : 0u;
}

/*****************************************************************************/
/* Die fuenf Lagen                                                           */
/*                                                                           */
/* KARENZ ist bewusst ein eigener Wert und nicht mit VERBUNDEN               */
/* zusammengefasst: Die Seite zeigt in beiden Faellen dasselbe, aber wer die */
/* Statusroute liest, soll den Unterschied sehen koennen.                    */
/*                                                                           */
/* SEIT_NEUSTART ist der ehrliche Sonderfall: Hatte die Firmware seit dem    */
/* Einschalten NIE eine Verbindung, ist die wahre Ausfalldauer unbekannt -   */
/* der Ausfall kann viel aelter sein als das Geraet. "seit 7 Minuten" waere  */
/* dann falsch. Die Seite sagt in diesem Fall "seit dem Neustart".           */
/*                                                                           */
/* STEUERUNG_STUMM ist der zweite Ausfall: Broker da, aber seit ueber zwoelf */
/* Minuten kein Kommando. Die Dauer ist hier IMMER ehrlich, auch ohne je     */
/* empfangenes Kommando - die Uhr laeuft ab dem Verbindungsaufbau, und der   */
/* Satz lautet "sendet seit X keine Vorgaben", nicht "keine Vorgaben mehr".  */
/*****************************************************************************/
enum VerbindungsLage
{
  VERBINDUNG_VERBUNDEN = 0,
  VERBINDUNG_KARENZ = 1,                // getrennt, aber noch innerhalb der Karenz
  VERBINDUNG_GESTOERT = 2,              // getrennt, Karenz vorbei, Dauer bekannt
  VERBINDUNG_GESTOERT_SEIT_NEUSTART = 3, // getrennt, Karenz vorbei, nie verbunden
  VERBINDUNG_STEUERUNG_STUMM = 4        // verbunden, aber keine Vorgaben mehr
};

/*****************************************************************************/
/* Der Zustand der Wacht                                                     */
/*****************************************************************************/
struct VerbindungsWacht
{
  Ausfall broker;    // Broker nicht erreichbar
  Ausfall stumm;     // Broker erreichbar, aber keine Vorgaben
  bool je_verbunden; // seit dem Neustart schon einmal verbunden gewesen
};

/*****************************************************************************/
/* Anfangszustand: getrennt, noch nie verbunden                              */
/*                                                                           */
/* Das ist die Wahrheit im Moment des Einschaltens - setupMqtt() versucht    */
/* die erste Verbindung erst danach. Wuerde hier "verbunden" stehen, meldete */
/* die Seite eines Geraets, das den Broker nie erreicht hat, dauerhaft alles */
/* in Ordnung.                                                               */
/*                                                                           */
/* Die Stumm-Uhr laeuft dabei NICHT: Ohne Verbindung kann per Definition     */
/* kein Kommando kommen, Stille ist dort keine Aussage ueber die Steuerung.  */
/*****************************************************************************/
inline void verbindung_init(VerbindungsWacht *w, uint32_t now)
{
  if (!w)
    return;
  ausfall_zuruecksetzen(&w->broker);
  ausfall_zuruecksetzen(&w->stumm);
  ausfall_beginnen(&w->broker, now);
  w->je_verbunden = false;
}

/*****************************************************************************/
/* Ein Set-Kommando ist eingetroffen - der Herzschlag der Steuerung          */
/*                                                                           */
/* Rueckgabe: die Dauer der Stille in Sekunden, wenn sie die Karenz          */
/* ueberschritten hatte (fuer die Log-Zeile), sonst 0.                       */
/*                                                                           */
/* WAS ZAEHLT UND WAS NICHT - hier steckt die eigentliche Ueberlegung:       */
/*                                                                           */
/* Der Aufruf gehoert in mqtt_callback() NACH die SUBSCRIBE_GRACE-Pruefung,  */
/* nicht davor. Der Grund ist genau die Wiedereinspielung, wegen der es die  */
/* Karenzzeit gibt: Der ioBroker-Adapter schickt jedem neuen Abonnenten die  */
/* gespeicherten Werte aller Set-Topics - und zwar AUCH DANN, wenn Node-RED  */
/* laengst tot ist. Dieser Schwall ist also kein Lebenszeichen der           */
/* Kaskadenregelung, sondern nur eines des Brokers, und den beobachtet       */
/* bereits die andere Uhr. Wuerde er mitzaehlen, verstummte die Meldung nach */
/* jedem Reconnect fuer zwoelf Minuten, ohne dass sich etwas geaendert haette.*/
/*                                                                           */
/* Ebenfalls NICHT zaehlt der Notbetriebszweig: Er wird ohnehin vor der      */
/* Karenzpruefung abschliessend behandelt, und seine Werte kommen nur bei    */
/* Aenderung und beim Reconnect - kein Takt, aus dem sich etwas ableiten     */
/* liesse.                                                                   */
/*                                                                           */
/* Ein Kommando, das die Firmware danach VERWIRFT (unbekanntes Topic,        */
/* Bereichsfehler), zaehlt dagegen sehr wohl: Die Steuerung hat gesendet,    */
/* sie lebt. Deshalb steht der Aufruf vor build_heatpump_command().          */
/*****************************************************************************/
inline uint32_t verbindung_set_empfangen(VerbindungsWacht *w, uint32_t now)
{
  if (!w)
    return 0;

  uint32_t gemeldet = ausfall_meldenswert(&w->stumm) ? ausfall_sekunden(&w->stumm) : 0u;

  // Die Uhr laeuft ab jetzt neu - der naechste Takt wird binnen 300 s erwartet
  ausfall_zuruecksetzen(&w->stumm);
  ausfall_beginnen(&w->stumm, now);
  return gemeldet;
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
/* Die Stumm-Uhr laeuft nur bei stehender Verbindung. Sie startet mit dem     */
/* Verbindungsaufbau und nicht mit dem Einschalten: Ohne Broker sagt Stille   */
/* nichts ueber die Steuerung aus, und die Uhr weiterlaufen zu lassen wuerde  */
/* nach der Rueckkehr des Brokers sofort eine zweite Stoermeldung erzeugen.   */
/*****************************************************************************/
inline bool verbindung_nachfuehren(VerbindungsWacht *w, bool verbunden,
                                   uint32_t now, uint32_t *ausfall_s)
{
  if (!w)
    return false;

  if (verbunden)
  {
    bool meldenswert = ausfall_meldenswert(&w->broker);
    if (meldenswert && ausfall_s)
    {
      *ausfall_s = ausfall_sekunden(&w->broker);
    }
    if (w->broker.laeuft) // Verbindung gerade zurueckgekehrt
    {
      ausfall_zuruecksetzen(&w->broker);
      w->je_verbunden = true;
      // Ab hier ist Stille eine Aussage - vorher war sie es nicht
      ausfall_beginnen(&w->stumm, now);
    }
    ausfall_fortschreiben(&w->stumm, now, VERBINDUNG_STUMM_KARENZ_MS);
    return meldenswert;
  }

  // Ohne Verbindung wird nur der Broker-Ausfall gezaehlt
  ausfall_zuruecksetzen(&w->stumm);
  ausfall_beginnen(&w->broker, now);
  ausfall_fortschreiben(&w->broker, now, VERBINDUNG_KARENZ_MS);
  return false;
}

/*****************************************************************************/
/* Welche Lage gilt gerade?                                                  */
/*                                                                           */
/* Liest nur die fortgeschriebenen Merker aus und rechnet selbst NICHT mit   */
/* der Zeit - deshalb kann die Antwort zwischen zwei Nachfuehrungen nicht    */
/* springen, und der Ueberlauf ist hier kein Thema mehr.                     */
/*                                                                           */
/* Der Broker-Ausfall hat Vorrang vor der stummen Steuerung. Ohne Broker     */
/* KANN kein Kommando kommen - beides zu melden waere doppelt gemoppelt und  */
/* wuerde jemanden zum Server schicken, um dort nach dem falschen Fehler zu  */
/* suchen.                                                                   */
/*****************************************************************************/
inline VerbindungsLage verbindung_lage(const VerbindungsWacht *w)
{
  if (!w)
    return VERBINDUNG_VERBUNDEN;

  if (w->broker.laeuft)
  {
    if (!w->broker.karenz_ueber)
      return VERBINDUNG_KARENZ;
    return w->je_verbunden ? VERBINDUNG_GESTOERT : VERBINDUNG_GESTOERT_SEIT_NEUSTART;
  }

  if (w->stumm.laeuft && w->stumm.karenz_ueber)
    return VERBINDUNG_STEUERUNG_STUMM;

  return VERBINDUNG_VERBUNDEN;
}

/*****************************************************************************/
/* Ausfalldauer in Sekunden - passend zur gemeldeten Lage                    */
/*                                                                           */
/* Nur fuer die Anzeige gedacht. Bei ungestoerter Lage 0 - eine "Dauer"      */
/* gaebe es dort nicht, und 0 ist der einzige Wert, der nicht versehentlich  */
/* als Zeitangabe durchgeht.                                                 */
/*****************************************************************************/
inline uint32_t verbindung_ausfall_sekunden(const VerbindungsWacht *w)
{
  if (!w)
    return 0;
  if (w->broker.laeuft)
    return ausfall_sekunden(&w->broker);
  if (w->stumm.laeuft && w->stumm.karenz_ueber)
    return ausfall_sekunden(&w->stumm);
  return 0;
}

// Gilt fuer die gerade gemeldete Lage der 30-Tage-Deckel?
inline bool verbindung_ueber_deckel(const VerbindungsWacht *w)
{
  if (!w)
    return false;
  if (w->broker.laeuft)
    return w->broker.ueber_deckel;
  if (w->stumm.laeuft && w->stumm.karenz_ueber)
    return w->stumm.ueber_deckel;
  return false;
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
