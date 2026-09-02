#pragma once
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/*****************************************************************************/
/* Notbetrieb: Werte halten, Schrittfolge fahren                             */
/*                                                                           */
/* Bewusst frei von Arduino-Abhaengigkeiten, gleiches Muster wie             */
/* sendwindow.h und telegram.h: derselbe Code laeuft in der Firmware und im  */
/* Hosttest (test/notbetrieb_test.cpp). Was hier steht, ist ohne Geraet      */
/* pruefbar - und genau das sind die Regeln, an denen der Knopf scheitern    */
/* wuerde, ohne dass es jemand merkt.                                        */
/*                                                                           */
/* Hintergrund: Vorhaben-Notbetrieb-Weboberflaeche.md. Kurz gefasst - faellt */
/* der ioBroker aus, faellt auch der MQTT-Broker aus (der Broker IST der     */
/* ioBroker-Adapter). Die Firmware muss die Kurvenwerte dann schon kennen.   */
/* Sie bekommt sie ueber einen eigenen Topic-Zweig <prefix>/notbetrieb/ und  */
/* haelt sie im RAM; an die Waermepumpe gehen sie erst, wenn der Knopf auf   */
/* der Weboberflaeche gedrueckt wird.                                        */
/*****************************************************************************/

/*****************************************************************************/
/* Die Rolle der Stufe                                                       */
/*                                                                           */
/* Kommt in der Firmware aus einem Build-Flag (NOTBETRIEB_ROLLE_WASSER, sonst*/
/* Heizen) - eine gemeinsame Firmware fuer beide Stufen gibt es ohnehin      */
/* nicht, solange der MQTT-Prefix ein Build-Flag ist. Hier steht sie als     */
/* Laufzeitwert und NICHT als #ifdef, damit der Hosttest beide Rollen in     */
/* einem Lauf pruefen kann. Auf dem Geraet ist der Wert konstant.            */
/*****************************************************************************/
enum NotbetriebRolle
{
    NOTBETRIEB_HEIZEN = 0, // Stufe 1: Kurvenbetrieb, Anlage an
    NOTBETRIEB_WASSER = 1  // Stufe 2: DHW only, Speichertemperatur, Anlage an
};

/*****************************************************************************/
/* Zeitregeln                                                                */
/*                                                                           */
/* Die Waermepumpe uebernimmt ein Kommando in 2-8 s (KNX-Messung 2026-08-16),*/
/* der Abfragezyklus liegt bei rund 6 s. 20 s je Schritt sind damit gut drei */
/* Zyklen Reserve; ein vollstaendiger Heizen-Lauf dauert seit 3.18.0 80 s    */
/* (zehn Schritte, davon der erste die Hydraulik), ein Warmwasser-Lauf 48 s. */
/*                                                                           */
/* DER LANGSAMSTE KANAL IST SEIT 3.18.0 SET39 ForceHeater. Beim EINSCHALTEN  */
/* lag die Uebernahme in einem Lauf bei einer knappen halben Minute          */
/* (MQTT-Topics.md, 2026-08-28) - die Waermepumpe prueft dort erst ihre      */
/* eigenen Bedingungen. Die RUECKNAHME, um die es im Notbetrieb allein geht, */
/* lag dagegen bei 7 s an beiden Stufen (nodered-flows/HEIZSTAB-MODI.md §5,  */
/* 2026-08-30). Die 20 s bleiben deshalb unveraendert: Kommt die Ruecknahme  */
/* wider Erwarten nicht zurueck, ist ROT die richtige Antwort - an der       */
/* Waermepumpe ist dann nichts verstellt, und der Mensch drueckt erneut.     */
/*                                                                           */
/* Der Gesamtdeckel ist ABGELEITET (Schrittzahl * Schritt-Timeout) und nicht */
/* frei gewaehlt. Ein kleinerer Deckel wuerde einen Lauf abbrechen, den die  */
/* Schritt-Timeouts noch gar nicht aufgegeben haben - dann haengt das        */
/* Ergebnis davon ab, welche Regel zufaellig zuerst greift. So ist der       */
/* Deckel das, was er sein soll: ein Notausgang, falls der Automat haengt,   */
/* nicht der normale Weg zu ROT.                                             */
/*****************************************************************************/
#define NOTBETRIEB_SCHRITT_TIMEOUT_MS 20000u

/*****************************************************************************/
/* Mindestwartezeit, bevor eine Rueckmeldung als Bestaetigung zaehlt          */
/*                                                                           */
/* Ohne sie kann ein VERALTETER Rueckgabewert einen Schritt bestaetigen, der  */
/* gerade erst abgesetzt wurde. Der gefaehrliche Fall steht in der            */
/* Schrittfolge selbst: Das Umschalten auf Kurvenbetrieb setzt die vier       */
/* Kurvenpunkte auf die Panasonic-Werksvorgaben zurueck. Traegt actual_data   */
/* im Moment von Schritt 2 noch den Kurvenwert von vorher, gilt der Schritt   */
/* sofort als erledigt - und der Werks-Reset ueberschreibt ihn danach. Der    */
/* Knopf meldete GRUEN, und die Anlage fuehre die Werkskurve mit 55 C bei     */
/* -5 C.                                                                      */
/*                                                                            */
/* Der Abfragezyklus liegt bei rund 6 s; 8 s decken einen vollen Zyklus plus  */
/* Reserve ab. Das ist der Preis dafuer, dass GRUEN wirklich "zurueckgelesen" */
/* heisst. Der Gesamtdeckel folgt der Schrittzahl und liegt seit 3.18.0 bei   */
/* 200 s (Heizen, zehn Schritte) bzw. 120 s (Wasser, sechs).                  */
/*****************************************************************************/
#define NOTBETRIEB_SCHRITT_MINDESTWARTE_MS 8000u

/*****************************************************************************/
/* Wie lange ein Ergebnis auf der Seite stehen bleibt                        */
/*                                                                           */
/* GRUEN und ROT blieben bis zum 2026-08-21 stehen, bis jemand erneut         */
/* drueckte. Wer die Seite am naechsten Tag oeffnete, sah das ROT von gestern */
/* und musste raten, ob gerade etwas schiefgegangen ist. Nach 15 Minuten      */
/* faellt die Anzeige deshalb auf BEREIT zurueck und der Knopf steht wieder   */
/* da; im MQTT-Log bleibt der Lauf vollstaendig nachlesbar.                   */
/*                                                                           */
/* 15 Minuten sind laenger als jeder Lauf (Deckel 200 s) und kurz genug, dass */
/* niemand ein fremdes Ergebnis fuer seines haelt.                            */
/*****************************************************************************/
#define NOTBETRIEB_ANZEIGE_VERFALL_MS 900000u

/*****************************************************************************/
/* Die Betriebsrichtung der Anlage - TOP101 Heat_Cool_SW_State (Byte 110)    */
/*                                                                           */
/* Am 2026-08-20 an H1 gemessen und vom Owner bestaetigt: Der externe         */
/* KNX-Schalter gibt die Richtung vor. Steht die Anlage auf Kuehlen, nimmt    */
/* sie ueber MQTT KEINEN Heizmodus an - set/OperationMode 0 ging nachweislich */
/* durch Bereichspruefung, Maskenmerge und Telegramm, und die Waermepumpe     */
/* verwarf es stillschweigend. Der Lauf endete nach 20 s mit ROT, ohne dass   */
/* jemand haette sehen koennen, warum.                                        */
/*                                                                           */
/* Deshalb ist TOP101 die Freigabebedingung fuer die Rolle Heizen. TOP4       */
/* (Operating_Mode_State) taugt dafuer nicht: Es zeigt den zuletzt            */
/* KOMMANDIERTEN Modus, nicht den tatsaechlichen - im M3-Lauf blieb es auf 3, */
/* waehrend die Anlage im Kuehlzweig stand.                                   */
/*                                                                           */
/* Die Rolle Warmwasser ist NICHT betroffen: OperationMode 3 (DHW only)       */
/* traegt auch im Kuehlbetrieb, am 2026-08-20 an H2 gemessen (M3).            */
/*****************************************************************************/
#define NOTBETRIEB_TOP_HEIZ_KUEHL 101

/*****************************************************************************/
/* Was fuer eine Art Schritt ist das? (seit 3.15.0)                          */
/*                                                                           */
/* Bis 3.14.2 war jeder Schritt ein Set-Kommando an die Waermepumpe, das an  */
/* einem TOP zurueckgelesen wird. Der Hydraulikschritt ist der erste, der    */
/* etwas ANDERES tut: Er legt einen Tasmota-Switch im Hausnetz auf AUS und   */
/* bekommt seine Bestaetigung aus dessen HTTP-Antwort, nicht aus             */
/* actual_data[].                                                           */
/*                                                                          */
/* Der Automat kennt trotzdem nur "Schritt vom Typ X, bestaetigt ja/nein" - */
/* der Netzzugriff steht ausschliesslich in notbetrieb.cpp. Nur so bleibt    */
/* dieser Header arduino-frei und im Hosttest pruefbar.                      */
/*****************************************************************************/
enum NotbetriebSchrittTyp
{
    NB_SCHRITT_SET = 0,      // Set-Kommando an die WP, Ruecklesung ueber den TOP
    NB_SCHRITT_HYDRAULIK = 1 // Tasmota-Switch auf AUS = Hydraulik 1-stufig
};

/*****************************************************************************/
/* Ein Schritt der Folge                                                     */
/*                                                                           */
/* typ       - siehe oben. Bei NB_SCHRITT_HYDRAULIK sind top, wert_index und */
/*             fester_wert BEDEUTUNGSLOS (top = -1, damit ein versehentliches */
/*             Nachschlagen in stateTopics[] auffaellt statt Zeile 0 zu      */
/*             lesen).                                                       */
/* set_name  - Topic-Name unter <prefix>/set/, wird an Topics::SET gehaengt  */
/*             Beim Hydraulikschritt ist es nur der Name fuer die Logzeile.  */
/* top       - TOP-Nummer, an der zurueckgelesen wird. GRUEN heisst          */
/*             zurueckgelesen, nicht abgesendet - ein GRUEN, das nur         */
/*             "gesendet" bedeutet, waere in dieser Lage wertlos.            */
/* wert_index - Index in die gehaltenen Werte, oder NOTBETRIEB_FESTER_WERT   */
/* fester_wert - gilt nur bei NOTBETRIEB_FESTER_WERT                         */
/*****************************************************************************/
#define NOTBETRIEB_FESTER_WERT (-1)

struct NotbetriebSchritt
{
    uint8_t typ;
    const char *set_name;
    int top;
    int wert_index;
    int fester_wert;
};

/*****************************************************************************/
/* Der Name des Hydraulikschritts - fuer Logzeilen und die Seite             */
/*                                                                           */
/* Er steht hier und nicht als Zeichenkette in der Tabelle, weil ihn auch    */
/* der Hosttest kennt: Nur so ist pruefbar, dass der Schritt an Position 1   */
/* steht, ohne die Zeichenkette ein zweites Mal hinzuschreiben.              */
/*****************************************************************************/
#define NOTBETRIEB_HYDRAULIK_NAME "Hydraulik 1-stufig"

/*****************************************************************************/
/* Die gehaltenen Werte je Rolle                                             */
/*                                                                           */
/* Die Namen sind identisch mit den Set-Kommandos - so gilt fuer den         */
/* notbetrieb-Zweig dieselbe Bereichspruefung wie fuer set, ohne dass die    */
/* Grenzen hier ein zweites Mal stehen muessten. Sie stehen ausschliesslich  */
/* in setCommands[] (commands.cpp); die Firmware schlaegt sie dort nach und  */
/* reicht sie an notbetrieb_wert_annehmen() weiter. Zwei Tabellen mit        */
/* denselben Zahlen waeren zwei Tabellen, die auseinanderlaufen koennen.     */
/*****************************************************************************/
static const char *const NOTBETRIEB_WERTE_HEIZEN[] = {
    "Z1HeatCurveTargetHighTemp", // "VL kalt" - Vorlauf bei KAELTE, zu OutsideLow
    "Z1HeatCurveTargetLowTemp",  // "VL warm" - Vorlauf bei WAERME, zu OutsideHigh
    "Z1HeatCurveOutsideLowTemp", // "AT kalt"
    "Z1HeatCurveOutsideHighTemp"}; // "AT warm"

static const char *const NOTBETRIEB_WERTE_WASSER[] = {
    "DHWTemp"};

static const unsigned NOTBETRIEB_ANZAHL_HEIZEN =
    sizeof(NOTBETRIEB_WERTE_HEIZEN) / sizeof(NOTBETRIEB_WERTE_HEIZEN[0]);
static const unsigned NOTBETRIEB_ANZAHL_WASSER =
    sizeof(NOTBETRIEB_WERTE_WASSER) / sizeof(NOTBETRIEB_WERTE_WASSER[0]);

/*****************************************************************************/
/* Die Schrittfolgen                                                         */
/*                                                                           */
/* Die Reihenfolge ist keine Kosmetik: Das Umschalten auf Kurvenbetrieb      */
/* setzt die Kurvenpunkte auf die Panasonic-Werksvorgaben zurueck (gemessen  */
/* 2026-08-11 und 2026-08-20: 55 C bei -5 C, 35 C bei +15 C). Wer die Kurve  */
/* VOR dem Umschalten schreibt, schreibt sie umsonst.                        */
/*                                                                           */
/* Dass die vier Kurvenpunkte im Kurvenbetrieb ueberhaupt angenommen werden, */
/* ist am 2026-08-20 an WP1 belegt (M1a) - auch der obere Punkt, den         */
/* kurven_sync.py im Direktbetrieb bewusst auslaesst.                        */
/*                                                                           */
/* DIE BETRIEBSART STEHT VORN (seit 2026-08-20). Ohne sie schaltet der Knopf */
/* eine Anlage ein, die auf Kuehlen steht: Am 2026-08-20 stand H1 auf        */
/* Operating_Mode_State = 1 (Cool only), weil die Kaskadensteuerung die      */
/* Betriebsart selbst per set/OperationMode fuehrt und im Sommer auf Kuehlen */
/* stellt. Faellt der ioBroker in so einem Moment aus, bleibt dieser Zustand */
/* stehen - der Notbetrieb haette GRUEN gemeldet und gekuehlt. Die           */
/* Warmwasser-Folge unten macht es seit jeher richtig (OperationMode = 3);   */
/* die Heizen-Folge zog nach.                                                */
/*                                                                           */
/* Sie steht VOR dem Moduswechsel und damit vor der Kurve: Ob ein Wechsel    */
/* der Betriebsart die Kurvenpunkte ebenfalls anfasst, ist nicht gemessen -  */
/* an dieser Stelle kann er keinen geschriebenen Wert mehr zerstoeren.       */
/*                                                                          */
/* DIE HYDRAULIK STEHT SEIT 3.15.0 GANZ VORN, vor der Betriebsart. Zwei      */
/* Gruende (Vorhaben-Hydraulik-Notbetrieb.md, Abschnitt 4):                  */
/*                                                                          */
/*  - Es ist noch nichts verstellt. Bricht der Schritt ab, steht die         */
/*    Waermepumpe genau so da wie vorher: kein Kommando abgesetzt, kein      */
/*    Sammelfenster offen, kein halber Notbetrieb zum Aufraeumen.            */
/*  - Die 90 s Laufzeit der beiden Stellantriebe beginnen im frueheste-      */
/*    moeglichen Moment und laufen parallel zur restlichen Schrittfolge ab.  */
/*                                                                          */
/* WARUM UEBERHAUPT: Steht die Hydraulik auf 2-stufig, waehrend eine Stufe   */
/* im Warmwasser-Notbetrieb laeuft, schiebt der Warmwasserbetrieb bis zu     */
/* 57 C in den Heizkreis. Die Fussbodenheizung vertraegt das nicht (Owner,   */
/* 2026-08-26). Im Normalbetrieb stellt die Kaskadensteuerung den Switch -   */
/* und genau die ist im Notbetriebsfall weg.                                 */
/*                                                                          */
/* Der Schritt steht in BEIDEN Folgen: Welchen Knopf jemand zuerst drueckt,  */
/* weiss niemand, und ein doppeltes AUS schadet nicht.                       */
/*                                                                          */
/* DIE UMWAELZPUMPE GEHOERT AUF AUTO (WaterPump = 0, seit 3.15.0). Am        */
/* 2026-08-27 an beiden Stufen beobachtet: Nach dem Umschalten auf           */
/* 2-stufiges Umpumpen stand TOP104 auf 1 (Fix) und die Pumpe lief dauerhaft */
/* durch - gesetzt von der Kaskadensteuerung, die im Notbetriebsfall weg     */
/* ist. Bliebe das so, liefe die Pumpe im Notbetrieb ohne Not weiter.        */
/*                                                                          */
/* Der Schritt steht VOR dem Einschalten und HINTER allen Moduswechseln: Ob  */
/* ein Wechsel der Betriebsart den Pumpenmodus zurueckstellt, ist nicht      */
/* gemessen - an dieser Stelle kann er ihn jedenfalls nicht mehr             */
/* ueberschreiben. Dieselbe Vorsicht wie bei den Kurvenpunkten, und aus      */
/* demselben Grund: Der Werks-Reset des Moduswechsels ist belegt.            */
/*                                                                          */
/* DER HEIZSTAB WIRD SEIT 3.18.0 ZURUECKGENOMMEN (ForceHeater = 0, Position  */
/* 2 in beiden Folgen). Seit dem 2026-08-30 benutzt die Kaskadensteuerung    */
/* SET39 im Regelbetrieb: Drei Heizmodi ersetzen den Verdichter durch den    */
/* Backup-Heizstab (3 kW je Stufe, 6 kW mit beiden). Sie setzen dabei        */
/* Heatpump = 0, weil die Waermepumpe SET39 nur bei ausgeschalteter Einheit  */
/* annimmt - das Abschalten ist dort Zweck, nicht Nebenwirkung.             */
/*                                                                          */
/* SET39 IST EIN ZUSTAND, DEN NIEMAND AUTOMATISCH ZURUECKNIMMT. Ohne diesen  */
/* Schritt stuende ForceHeater weiter auf 1, waehrend der letzte Schritt die */
/* Anlage einschaltet - und die Umwaelzpumpe haengt am Kommando, nicht am    */
/* Stab: Sie laeuft weiter, auch nachdem der Heizstab thermisch abgeschaltet */
/* hat, und stoppt erst mit ForceHeater = 0 (gemessen 2026-08-28). Im        */
/* eigentlichen Notbetriebsfall - die Steuerung ist tot - raeumt das sonst   */
/* niemand auf.                                                             */
/*                                                                          */
/* WARUM POSITION 2 und nicht weiter hinten: Bricht der Lauf hier ab, tut    */
/* die Anlage nichts mehr - Stab aus, Pumpe aus, Waermepumpe aus. Das ist    */
/* die sichere Seite. Ein etwaiger Werks-Reset der folgenden Moduswechsel    */
/* kann diesen Wert ausserdem nicht zerstoeren: Er wirkte in dieselbe        */
/* Richtung. Deshalb gilt hier NICHT die Vorsicht der Kurvenpunkte, die      */
/* hinter den Moduswechsel gehoeren.                                        */
/*                                                                          */
/* DER SCHRITT KOSTET IM REGELFALL 8 s. Steht TOP68 schon auf 0 - also       */
/* immer, wenn kein Heizstab-Modus lief -, ist er nach der Mindestwarte      */
/* bestaetigt und die Folge laeuft weiter.                                  */
/*                                                                          */
/* WAS ER NICHT KANN: Die Bridge spricht nur mit IHRER Waermepumpe. Lief die */
/* Anlage im 6-kW-Modus, steht SET39 auch an der anderen Stufe - dort muss   */
/* der Knopf ebenfalls gedrueckt werden, sonst laeuft deren Umwaelzpumpe     */
/* weiter (Ablauf-Notbetrieb.md, Abschnitt 1b).                             */
/*****************************************************************************/
static const NotbetriebSchritt NOTBETRIEB_SCHRITTE_HEIZEN[] = {
    {NB_SCHRITT_HYDRAULIK, NOTBETRIEB_HYDRAULIK_NAME, -1, NOTBETRIEB_FESTER_WERT, 0},
    {NB_SCHRITT_SET, "ForceHeater", 68, NOTBETRIEB_FESTER_WERT, 0}, // Heizstab zurueck
    {NB_SCHRITT_SET, "OperationMode", 4, NOTBETRIEB_FESTER_WERT, 0}, // Heat only
    {NB_SCHRITT_SET, "HeatingMode", 76, NOTBETRIEB_FESTER_WERT, 0},  // dann umschalten
    {NB_SCHRITT_SET, "Z1HeatCurveTargetHighTemp", 29, 0, 0},         // dann die Kurve: "VL kalt"
    {NB_SCHRITT_SET, "Z1HeatCurveTargetLowTemp", 30, 1, 0},          // "VL warm"
    {NB_SCHRITT_SET, "Z1HeatCurveOutsideLowTemp", 32, 2, 0},         // "AT kalt"
    {NB_SCHRITT_SET, "Z1HeatCurveOutsideHighTemp", 31, 3, 0},        // "AT warm"
    {NB_SCHRITT_SET, "WaterPump", 104, NOTBETRIEB_FESTER_WERT, 0},   // Pumpe auf auto
    {NB_SCHRITT_SET, "Heatpump", 0, NOTBETRIEB_FESTER_WERT, 1}}; // zuletzt einschalten

/*****************************************************************************/
/* Warmwasser braucht keine Kurve - der Knopf dort ist deutlich einfacher.   */
/* OperationMode 3 (DHW only) traegt auch dann, wenn die Anlage vom          */
/* KNX-Aktor auf Kuehlen steht - also im Sommerfall, fuer den der Notbetrieb */
/* an Stufe 2 ueberhaupt gedacht ist (gemessen 2026-08-20 an H2, M3).        */
/*                                                                          */
/* Der Heizstabschritt steht auch hier an Position 2: Der 6-kW-Modus setzt   */
/* SET39 an BEIDEN Stufen, und welchen Knopf jemand zuerst drueckt, weiss    */
/* niemand - dieselbe Ueberlegung wie beim Hydraulikschritt.                 */
/*****************************************************************************/
static const NotbetriebSchritt NOTBETRIEB_SCHRITTE_WASSER[] = {
    {NB_SCHRITT_HYDRAULIK, NOTBETRIEB_HYDRAULIK_NAME, -1, NOTBETRIEB_FESTER_WERT, 0},
    {NB_SCHRITT_SET, "ForceHeater", 68, NOTBETRIEB_FESTER_WERT, 0}, // Heizstab zurueck
    {NB_SCHRITT_SET, "OperationMode", 4, NOTBETRIEB_FESTER_WERT, 3},
    {NB_SCHRITT_SET, "DHWTemp", 9, 0, 0},
    {NB_SCHRITT_SET, "WaterPump", 104, NOTBETRIEB_FESTER_WERT, 0}, // Pumpe auf auto
    {NB_SCHRITT_SET, "Heatpump", 0, NOTBETRIEB_FESTER_WERT, 1}};

static const unsigned NOTBETRIEB_SCHRITTE_HEIZEN_N =
    sizeof(NOTBETRIEB_SCHRITTE_HEIZEN) / sizeof(NOTBETRIEB_SCHRITTE_HEIZEN[0]);
static const unsigned NOTBETRIEB_SCHRITTE_WASSER_N =
    sizeof(NOTBETRIEB_SCHRITTE_WASSER) / sizeof(NOTBETRIEB_SCHRITTE_WASSER[0]);

/*****************************************************************************/
/* Zugriff auf die Tabellen der jeweiligen Rolle                             */
/*****************************************************************************/
inline unsigned notbetrieb_wert_anzahl(NotbetriebRolle rolle)
{
    return (rolle == NOTBETRIEB_WASSER) ? NOTBETRIEB_ANZAHL_WASSER : NOTBETRIEB_ANZAHL_HEIZEN;
}

inline const char *notbetrieb_wert_name(NotbetriebRolle rolle, unsigned index)
{
    if (index >= notbetrieb_wert_anzahl(rolle))
        return 0;
    return (rolle == NOTBETRIEB_WASSER) ? NOTBETRIEB_WERTE_WASSER[index]
                                        : NOTBETRIEB_WERTE_HEIZEN[index];
}

inline unsigned notbetrieb_schritt_anzahl(NotbetriebRolle rolle)
{
    return (rolle == NOTBETRIEB_WASSER) ? NOTBETRIEB_SCHRITTE_WASSER_N : NOTBETRIEB_SCHRITTE_HEIZEN_N;
}

inline const NotbetriebSchritt *notbetrieb_schritt(NotbetriebRolle rolle, unsigned index)
{
    if (index >= notbetrieb_schritt_anzahl(rolle))
        return 0;
    return (rolle == NOTBETRIEB_WASSER) ? &NOTBETRIEB_SCHRITTE_WASSER[index]
                                        : &NOTBETRIEB_SCHRITTE_HEIZEN[index];
}

/*****************************************************************************/
/* Der Speicher der gehaltenen Werte                                         */
/*                                                                           */
/* Wenige Bytes RAM plus eine Bitmaske "ist gesetzt". Keine Datei auf        */
/* LittleFS (Owner-Entscheidung 2026-08-20): Die Werte ueberleben im RAM     */
/* jeden Broker-Ausfall und sind nach einem Neustart binnen Sekunden wieder  */
/* da, weil der ioBroker-Adapter jedem neuen Abonnenten den gespeicherten    */
/* Wert jedes Topics einspielt. Startet der ESP neu, WAEHREND der Broker weg */
/* ist, liegt ein Stromausfall vor - dann braucht die Waermepumpe selbst     */
/* laenger als die Steuerung, und dieser Fall laeuft ohne Notbetrieb.        */
/*****************************************************************************/
#define NOTBETRIEB_MAX_WERTE 4 // Heizen hat die laengere Liste

struct NotbetriebSpeicher
{
    int werte[NOTBETRIEB_MAX_WERTE];
    uint8_t gesetzt; // Bitmaske, Bit n = Wert n liegt vor
};

inline void notbetrieb_speicher_leeren(NotbetriebSpeicher *sp)
{
    if (!sp)
        return;
    for (unsigned i = 0; i < NOTBETRIEB_MAX_WERTE; i++)
        sp->werte[i] = 0;
    sp->gesetzt = 0;
}

/*****************************************************************************/
/* Einen Wert aus dem notbetrieb-Zweig annehmen                              */
/*                                                                           */
/* min/max kommen aus setCommands[] und werden von der Firmware nachge-      */
/* schlagen - siehe Kommentar bei den Wertetabellen oben. Ein Wert ausser-   */
/* halb der Grenzen wird VERWORFEN und nicht geklemmt: Ein stillschweigend   */
/* zurechtgebogener Kurvenpunkt waere im Notfall schlimmer als ein gesperrter*/
/* Knopf, weil niemand ihn nachpruefen wuerde.                               */
/*                                                                           */
/* Rueckgabe: true, wenn der Wert uebernommen wurde.                         */
/*****************************************************************************/
inline bool notbetrieb_wert_annehmen(NotbetriebSpeicher *sp, NotbetriebRolle rolle,
                                     const char *name, int wert, int min, int max)
{
    if (!sp || !name)
        return false;
    if (wert < min || wert > max)
        return false;

    const unsigned n = notbetrieb_wert_anzahl(rolle);
    for (unsigned i = 0; i < n; i++)
    {
        if (strcmp(name, notbetrieb_wert_name(rolle, i)) == 0)
        {
            sp->werte[i] = wert;
            sp->gesetzt |= (uint8_t)(1u << i);
            return true;
        }
    }
    return false; // Name gehoert nicht zu dieser Rolle
}

/*****************************************************************************/
/* Liegen alle Werte vor?                                                    */
/*                                                                           */
/* Unvollstaendig heisst gesperrt, mit Klartext auf der Seite. Lieber gar    */
/* nicht schalten als auf die Panasonic-Werkskurve - die faehrt bei -5 C     */
/* Aussentemperatur 55 C Vorlauf, weit jenseits der Estrichgrenze.           */
/*****************************************************************************/
inline bool notbetrieb_vollstaendig(const NotbetriebSpeicher *sp, NotbetriebRolle rolle)
{
    if (!sp)
        return false;
    const unsigned n = notbetrieb_wert_anzahl(rolle);
    const uint8_t voll = (uint8_t)((1u << n) - 1u);
    return (sp->gesetzt & voll) == voll;
}

/*****************************************************************************/
/* Plausibilitaet der Kurve: zeigt sie in die richtige Richtung?             */
/*                                                                           */
/* Die vier Werte koennen einzeln im erlaubten Bereich liegen und trotzdem   */
/* eine unsinnige Kurve ergeben - genau dann, wenn "VL kalt" und "VL warm"   */
/* vertauscht sind. Der Fall ist nicht konstruiert: Panasonics Target_High/  */
/* Low benennt die VORLAUFhoehe, der ioBroker-Konfigbaum benennt mit Hi/Lo   */
/* die AUSSENtemperatur, und beide Paare liegen ueber Kreuz (vlLo ->         */
/* TargetHigh). Bis zum 2026-08-20 spiegelte kurven_sync.py die Kurve        */
/* deshalb verdreht, und niemandem fiel es auf - die Werte waren ja gueltig. */
/*                                                                           */
/* Eine Heizkurve faellt mit steigender Aussentemperatur, also VL kalt >=    */
/* VL warm. GLEICHHEIT IST ERLAUBT: Eine flache Vorgabe ist zulaessig - die  */
/* Kuehlkurve dieser Anlage faehrt genau so (20 C bei 20 wie bei 30 C).      */
/*                                                                           */
/* Das Ergebnis WARNT, es sperrt nicht. Ein Notbetrieb mit verdrehter Kurve  */
/* ist immer noch besser als keiner, und diese Regel kennt die Absicht des   */
/* Betreibers nicht. Gesperrt wird nur, was nachweislich nicht funktioniert: */
/* fehlende Werte und der Kuehlbetrieb (notbetrieb_sperrgrund()).            */
/*                                                                           */
/* Unvollstaendige Saetze und die Rolle Warmwasser melden OK - dort gibt es  */
/* nichts zu pruefen, und eine Warnung neben "Nicht bereit" waere Laerm.     */
/*****************************************************************************/
enum NotbetriebKurvenWarnung
{
    NOTBETRIEB_KURVE_OK = 0,               // plausibel, oder nichts zu pruefen
    NOTBETRIEB_KURVE_VORLAUF_VERDREHT = 1, // VL kalt < VL warm
    NOTBETRIEB_KURVE_AUSSEN_VERDREHT = 2   // AT kalt >= AT warm
};

inline NotbetriebKurvenWarnung notbetrieb_kurve_pruefen(const NotbetriebSpeicher *sp,
                                                        NotbetriebRolle rolle)
{
    if (rolle == NOTBETRIEB_WASSER || !notbetrieb_vollstaendig(sp, rolle))
        return NOTBETRIEB_KURVE_OK;

    // Reihenfolge wie in NOTBETRIEB_WERTE_HEIZEN - die Indizes stehen nur hier
    // und in jener Tabelle, deshalb die Namen daneben.
    const int vl_kalt = sp->werte[0]; // Z1HeatCurveTargetHighTemp
    const int vl_warm = sp->werte[1]; // Z1HeatCurveTargetLowTemp
    const int at_kalt = sp->werte[2]; // Z1HeatCurveOutsideLowTemp
    const int at_warm = sp->werte[3]; // Z1HeatCurveOutsideHighTemp

    // Der Vorlauf zuerst: Das ist der Verwechslungsfall, um den es geht.
    if (vl_kalt < vl_warm)
        return NOTBETRIEB_KURVE_VORLAUF_VERDREHT;
    // Zwei Stuetzpunkte auf derselben Aussentemperatur ergeben keine Kurve -
    // anders als beim Vorlauf ist Gleichheit hier bereits entartet.
    if (at_kalt >= at_warm)
        return NOTBETRIEB_KURVE_AUSSEN_VERDREHT;
    return NOTBETRIEB_KURVE_OK;
}

/*****************************************************************************/
/* Die Karenzzeit-Ausnahme                                                   */
/*                                                                           */
/* Nach jedem Verbinden verwirft mqtt_callback() fuer SUBSCRIBE_GRACE alles, */
/* was hereinkommt - der ioBroker-Adapter spielt einem neuen Abonnenten die  */
/* gespeicherten Werte jedes Set-Topics ein, und ein Kurvenwert vom Vortag   */
/* setzte so nach jedem Neustart den Vorlauf-Sollwert auf 55 C (gemessen     */
/* 2026-08-13).                                                              */
/*                                                                           */
/* Fuer den notbetrieb-Zweig ist dieselbe Wiedereinspielung kein Problem,    */
/* sondern DER MECHANISMUS: nur so sind die Werte nach jedem Neustart binnen */
/* Sekunden wieder da. Die Gefahr, die zur Karenzzeit gefuehrt hat, kann     */
/* hier nicht auftreten - diese Werte gehen nie ungefragt an die Waermepumpe.*/
/*                                                                           */
/* Wird diese Ausnahme vergessen, funktioniert der Knopf im Labor und nach   */
/* jedem Neustart nicht mehr. Deshalb steht sie hier und nicht als drei      */
/* Zeilen im Callback: hier ist sie hosttestbar.                             */
/*                                                                           */
/* Geprueft wird auf die Wurzel PLUS Trennzeichen, damit ein Topic wie       */
/* "<prefix>/notbetriebXY/..." nicht faelschlich durchrutscht.               */
/*****************************************************************************/
inline bool notbetrieb_ist_notbetrieb_topic(const char *topic, const char *wurzel)
{
    if (!topic || !wurzel)
        return false;
    const size_t len = strlen(wurzel);
    if (len == 0)
        return false;
    if (strncmp(topic, wurzel, len) != 0)
        return false;
    return topic[len] == '/' && topic[len + 1] != '\0';
}

/*****************************************************************************/
/* Den Namen hinter der Wurzel herausziehen ("<wurzel>/DHWTemp" -> "DHWTemp")*/
/* Rueckgabe: Zeiger in den Topic-String, oder 0 wenn das Topic nicht passt. */
/*****************************************************************************/
inline const char *notbetrieb_name_aus_topic(const char *topic, const char *wurzel)
{
    if (!notbetrieb_ist_notbetrieb_topic(topic, wurzel))
        return 0;
    return topic + strlen(wurzel) + 1;
}

/*****************************************************************************/
/* Ist ein Wert zurueckgelesen?                                              */
/*                                                                           */
/* Die Firmware haelt die Rueckmeldungen der Waermepumpe als TEXT in          */
/* actual_data[]. Der naive Vergleich waere atoi(text) == soll - und der      */
/* hat eine Falle, die genau das Signal zerstoert, auf das es hier ankommt:   */
/* atoi("") liefert 0. Ein TOP, das noch nie empfangen wurde, wuerde damit    */
/* jeden Schritt mit dem Sollwert 0 sofort bestaetigen - und der erste        */
/* Schritt der Heizen-Folge ist OperationMode = 0. Der Knopf wuerde GRUEN     */
/* melden, ohne dass die Waermepumpe je geantwortet hat.                      */
/*                                                                           */
/* Deshalb: leerer Text gilt NICHT als Bestaetigung, und was keine saubere    */
/* ganze Zahl ist, auch nicht. Fuehrende und nachlaufende Leerzeichen sind    */
/* erlaubt, sonst nichts.                                                    */
/*                                                                           */
/* Dieselbe Regel traegt seit dem 2026-08-21 auch die Freigabe unten: "TOP101 */
/* liest sich sauber als 0" ist genau derselbe Vorgang wie "ein Schritt ist   */
/* zurueckgelesen". Zwei Zahlenparser fuer dieselbe Frage waeren zwei Stellen,*/
/* an denen der leere Text unterschiedlich behandelt werden koennte.          */
/*****************************************************************************/
inline bool notbetrieb_rueckgelesen(const char *ist_text, int soll)
{
    if (!ist_text || ist_text[0] == '\0')
        return false;

    // fuehrende Leerzeichen ueberspringen, danach muss eine Zahl kommen
    const char *p = ist_text;
    while (*p == ' ' || *p == '\t')
        p++;
    if (*p == '\0')
        return false;

    char *ende = 0;
    const long wert = strtol(p, &ende, 10);
    if (ende == p)
        return false; // gar keine Ziffer gefunden

    // nachlaufend sind nur Leerzeichen erlaubt - "1.5" oder "2 Hz" sind keine
    // Bestaetigung fuer 1 bzw. 2
    while (*ende == ' ' || *ende == '\t' || *ende == '\r' || *ende == '\n')
        ende++;
    if (*ende != '\0')
        return false;

    return wert == (long)soll;
}

/*****************************************************************************/
/* Die Freigabe: steht die Anlage nachweislich auf Heizen?                   */
/*                                                                           */
/* Owner-Entscheidung 2026-08-21: ALLES ausser einer sauber gelesenen 0 gilt  */
/* als "nicht Heizen". Das deckt vier Faelle mit einer Regel ab:              */
/*                                                                           */
/*   "0"   Heizen          -> frei                                            */
/*   "1"   Kuehlen         -> gesperrt (die WP nimmt keinen Heizmodus an)     */
/*   "2"   unknown         -> gesperrt (Rohwert b11, Zustand unbekannt)       */
/*   "-1"  Feld leer       -> gesperrt (die WP liefert keine Aussage)         */
/*   ""    nie empfangen   -> gesperrt (keine Verbindung zur Waermepumpe)     */
/*                                                                            */
/* Der letzte Fall ist der Grund fuer die Strenge: Ohne Rueckmeldung der      */
/* Waermepumpe erreicht auch kein Kommando sie, der Lauf endete nach 20 s in  */
/* ROT. Ein Knopf, der in dieser Lage zum Druecken einlaedt, verspricht       */
/* etwas, das er nicht halten kann - und zwar genau dann, wenn jemand darauf  */
/* angewiesen ist. Preis: Am Pruefstand ohne Waermepumpe ist der Knopf        */
/* dauerhaft gesperrt; der Fehlerpfad ist dort nur noch ueber den Hosttest zu */
/* pruefen.                                                                   */
/*****************************************************************************/
inline bool notbetrieb_heizbetrieb_belegt(const char *heiz_kuehl_text)
{
    return notbetrieb_rueckgelesen(heiz_kuehl_text, 0);
}

/*****************************************************************************/
/* Meldet die Anlage AUSDRUECKLICH Kuehlen?                                  */
/*                                                                           */
/* Nicht die Umkehrung der Freigabe oben: Fuer den Abbruch eines LAUFENDEN    */
/* Vorgangs zaehlt nur die klare 1. Ein einzelner Aussetzer (leeres Feld,     */
/* "unknown") wuerde sonst einen Lauf zerreissen, der gerade sauber laeuft -  */
/* und ein abgebrochener Lauf laesst die Anlage im halb geschalteten Zustand  */
/* stehen. Zum Starten ist Strenge richtig, zum Abbrechen Zurueckhaltung.     */
/*****************************************************************************/
inline bool notbetrieb_kuehlbetrieb_gemeldet(NotbetriebRolle rolle, const char *heiz_kuehl_text)
{
    if (rolle == NOTBETRIEB_WASSER)
        return false; // Warmwasser laeuft auch im Kuehlbetrieb (M3)
    return notbetrieb_rueckgelesen(heiz_kuehl_text, 1);
}

/*****************************************************************************/
/* Warum ist der Knopf gesperrt?                                             */
/*                                                                           */
/* Eine Stelle, die diese Frage beantwortet - die Seite, die Statusroute und  */
/* der POST-Handler lesen dieselbe Antwort. Ohne das koennte die Seite einen  */
/* Knopf zeigen, den der Handler ablehnt, oder umgekehrt.                     */
/*                                                                           */
/* Reihenfolge: Fehlende Werte zuerst. Beides sind Sackgassen, aber die       */
/* fehlenden Werte sind der tiefer liegende Mangel - wer sie behebt, steht    */
/* danach immer noch vor der Betriebsart, waehrend umgekehrt der Umweg ueber  */
/* den KNX-Schalter vergeblich waere.                                         */
/*****************************************************************************/
enum NotbetriebSperre
{
    NOTBETRIEB_FREI = 0,             // der Knopf darf gedrueckt werden
    NOTBETRIEB_SPERRE_WERTE = 1,     // es fehlen gehaltene Werte
    NOTBETRIEB_SPERRE_HEIZBETRIEB = 2 // die Anlage steht nicht auf Heizen
};

inline NotbetriebSperre notbetrieb_sperrgrund(NotbetriebRolle rolle,
                                              const NotbetriebSpeicher *sp,
                                              const char *heiz_kuehl_text)
{
    if (!notbetrieb_vollstaendig(sp, rolle))
        return NOTBETRIEB_SPERRE_WERTE;
    if (rolle != NOTBETRIEB_WASSER && !notbetrieb_heizbetrieb_belegt(heiz_kuehl_text))
        return NOTBETRIEB_SPERRE_HEIZBETRIEB;
    return NOTBETRIEB_FREI;
}

/*****************************************************************************/
/* Der Zustandsautomat                                                       */
/*                                                                           */
/* Die Schritte laufen EINZELN, nicht in einem Sammelfenster. Ein Webhandler,*/
/* der sechs Kommandos hintereinander absetzt, packt sie alle in EIN         */
/* Telegramm (Deckel 2 s, sendwindow.h) - dann konkurriert das Kurven-       */
/* schreiben mit dem Werks-Reset des Moduswechsels, und welcher gewinnt, ist */
/* unbekannt. Deshalb: senden -> zurueckzulesen -> naechster Schritt, getickt  */
/* aus loop(). Der HTTP-Handler stoesst nur an und antwortet sofort.         */
/*****************************************************************************/
enum NotbetriebZustand
{
    NOTBETRIEB_BEREIT = 0, // nichts laeuft
    NOTBETRIEB_LAEUFT = 1, // Schrittfolge unterwegs, Seite zeigt "laeuft..."
    NOTBETRIEB_GRUEN = 2,  // alle Schritte zurueckgelesen
    NOTBETRIEB_ROT = 3     // abgebrochen - es gilt Plan B am Bedienpanel
};

enum NotbetriebAktion
{
    NOTBETRIEB_TU_NICHTS = 0,     // warten, der Schritt laeuft noch
    NOTBETRIEB_SENDEN = 1,        // aktuellen Schritt absetzen
    NOTBETRIEB_FERTIG = 2,        // gerade GRUEN geworden
    NOTBETRIEB_ABBRUCH = 3,       // gerade ROT geworden: ein Schritt kam nicht zurueck
    NOTBETRIEB_ABBRUCH_KUEHLEN = 4 // gerade ROT geworden: die Anlage meldet Kuehlen
};

/*****************************************************************************/
/* WARUM ein Lauf abgebrochen wurde (seit 3.15.0)                            */
/*                                                                           */
/* Bis 3.14.2 zeigte die Seite bei ROT immer denselben Satz "Hat nicht       */
/* geklappt". Das reicht nicht mehr, seit der erste Schritt die Hydraulik    */
/* stellt: Dort fuehrt der Weg zurueck ueber einen Schalter im Waschraum,    */
/* und wer das nicht weiss, kann den Notbetrieb nicht retten. Der Grund      */
/* wandert deshalb ueber den Statusstring auf die Seite.                     */
/*                                                                           */
/* Er steht im Lauf und nicht im Rueckgabewert des Ticks: Die Seite fragt    */
/* alle zwei Sekunden nach, der Tick liefert seine Aktion aber genau einmal. */
/*****************************************************************************/
enum NotbetriebAbbruchgrund
{
    NOTBETRIEB_GRUND_KEINER = 0,   // laeuft noch, oder GRUEN
    NOTBETRIEB_GRUND_TIMEOUT = 1,  // ein Schritt kam nicht zurueck
    NOTBETRIEB_GRUND_KUEHLEN = 2,  // die Anlage meldet Kuehlbetrieb (TOP101)
    NOTBETRIEB_GRUND_HYDRAULIK = 3 // der Switch liess sich nicht auf AUS legen
};

/*****************************************************************************/
/* Welcher Abbruchgrund gehoert zu einem Schritt, der nicht zurueckkam?      */
/*                                                                           */
/* Eine Stelle fuer beide Zeitgrenzen (Schritt-Timeout und Gesamtdeckel):    */
/* Zwei Regeln fuer dieselbe Frage waeren zwei Stellen, an denen derselbe    */
/* Schritt einmal so und einmal anders gemeldet wird.                        */
/*****************************************************************************/
inline NotbetriebAbbruchgrund notbetrieb_grund_fuer_schritt(NotbetriebRolle rolle,
                                                            unsigned schritt)
{
    const NotbetriebSchritt *s = notbetrieb_schritt(rolle, schritt);
    if (s && s->typ == NB_SCHRITT_HYDRAULIK)
        return NOTBETRIEB_GRUND_HYDRAULIK;
    return NOTBETRIEB_GRUND_TIMEOUT;
}

/*****************************************************************************/
/* Der laufende Vorgang                                                      */
/*                                                                           */
/* schritt_gesendet unterscheidet "der Schritt ist abgesetzt und wartet auf  */
/* seine Bestaetigung" von "er steht noch aus". Seit 3.15.0 wird das         */
/* gebraucht: Bis 3.14.2 setzte der POST-Handler den ersten Schritt selbst   */
/* ab, was fuer ein Set-Kommando in Mikrosekunden erledigt ist. Der          */
/* Hydraulikschritt macht daraus einen HTTP-Request von bis zu 1,5 s - so    */
/* lange darf kein Webhandler haengen. Also setzt ihn jetzt der Tick aus     */
/* loop() ab, und dieses Flag sagt ihm, dass noch etwas aussteht.            */
/*                                                                           */
/* Ohne das Flag haette der Tick zwei Moeglichkeiten, und beide waeren       */
/* falsch: entweder nie senden (jeder Lauf liefe in den Timeout) oder blind  */
/* auf die Ruecklesung schauen - dann koennte ein TOP, das den Sollwert      */
/* zufaellig schon traegt, einen nie abgesetzten Schritt bestaetigen.        */
/*****************************************************************************/
struct NotbetriebLauf
{
    uint8_t zustand;
    uint8_t schritt;
    uint8_t schritt_gesendet; // ist der aktuelle Schritt schon abgesetzt?
    uint8_t grund;            // NotbetriebAbbruchgrund, gueltig bei ROT
    uint32_t schritt_start;
    uint32_t lauf_start;
    uint32_t ende; // Zeitpunkt von GRUEN/ROT, fuer den Anzeigeverfall
};

inline void notbetrieb_lauf_leeren(NotbetriebLauf *lauf)
{
    if (!lauf)
        return;
    lauf->zustand = NOTBETRIEB_BEREIT;
    lauf->schritt = 0;
    lauf->schritt_gesendet = 0;
    lauf->grund = NOTBETRIEB_GRUND_KEINER;
    lauf->schritt_start = 0;
    lauf->lauf_start = 0;
    lauf->ende = 0;
}

/*****************************************************************************/
/* Einen Lauf abschliessen - GRUEN oder ROT, mit Zeitstempel                 */
/*                                                                           */
/* Immer ueber diese Stelle, nie durch direktes Setzen von 'zustand': Ohne    */
/* den Zeitstempel wuesste der Anzeigeverfall unten nicht, wann das Ergebnis  */
/* entstanden ist, und ein ROT bliebe entweder ewig stehen oder verschwaende  */
/* sofort. Die Firmware hat mehrere Abbruchstellen (notbetrieb.cpp), deshalb  */
/* steht das hier und nicht nur im Tick.                                      */
/*                                                                            */
/* Der Grund ist seit 3.15.0 ein PFLICHTARGUMENT und hat bewusst keinen       */
/* Vorgabewert: Jede Abbruchstelle soll sich entscheiden muessen, was auf der */
/* Seite steht. Bei GRUEN ist NOTBETRIEB_GRUND_KEINER der richtige Wert.      */
/*****************************************************************************/
inline void notbetrieb_abschluss(NotbetriebLauf *lauf, NotbetriebZustand zustand,
                                 uint32_t jetzt, NotbetriebAbbruchgrund grund)
{
    if (!lauf)
        return;
    lauf->zustand = (uint8_t)zustand;
    lauf->grund = (uint8_t)grund;
    lauf->ende = jetzt;
}

/*****************************************************************************/
/* Anzeigeverfall: ein Ergebnis von gestern ist keine Auskunft mehr          */
/*                                                                           */
/* Rueckgabe: true, wenn die Anzeige gerade auf BEREIT zurueckgefallen ist.   */
/* Ein laufender Vorgang wird nie angefasst - der Deckel bricht ihn ab, nicht */
/* der Verfall. Die Zeitrechnung laeuft ueber die Differenz und uebersteht    */
/* damit den millis()-Ueberlauf nach 49,7 Tagen.                              */
/*****************************************************************************/
inline bool notbetrieb_verfall_pruefen(NotbetriebLauf *lauf, uint32_t jetzt)
{
    if (!lauf)
        return false;
    if (lauf->zustand != NOTBETRIEB_GRUEN && lauf->zustand != NOTBETRIEB_ROT)
        return false;
    if ((uint32_t)(jetzt - lauf->ende) < NOTBETRIEB_ANZEIGE_VERFALL_MS)
        return false;
    notbetrieb_lauf_leeren(lauf);
    return true;
}

/*****************************************************************************/
/* Gesamtdeckel - abgeleitet, siehe Zeitregeln oben                          */
/*****************************************************************************/
inline uint32_t notbetrieb_gesamtdeckel_ms(NotbetriebRolle rolle)
{
    return (uint32_t)notbetrieb_schritt_anzahl(rolle) * NOTBETRIEB_SCHRITT_TIMEOUT_MS;
}

/*****************************************************************************/
/* Ist die Zeitregel in sich stimmig?                                         */
/*                                                                            */
/* Waere die Mindestwarte nicht kleiner als das Schritt-Timeout, koennte kein */
/* Schritt je bestaetigt werden - jeder Lauf endete in ROT. Das faellt hier   */
/* beim Uebersetzen auf, nicht erst an der Anlage.                            */
/*****************************************************************************/
static_assert(NOTBETRIEB_SCHRITT_MINDESTWARTE_MS < NOTBETRIEB_SCHRITT_TIMEOUT_MS,
              "Mindestwarte muss kleiner als das Schritt-Timeout sein");

/*****************************************************************************/
/* Lauf starten                                                              */
/*                                                                           */
/* Laeuft schon einer, passiert nichts - ein zweiter Klick darf die Folge    */
/* nicht mittendrin neu anstossen. Nach GRUEN oder ROT ist ein neuer Lauf    */
/* erlaubt.                                                                  */
/*                                                                           */
/* Die Sperrgruende (fehlende Werte, Betriebsart) werden hier NICHT geprueft:*/
/* Das tut notbetrieb_starten() in der Firmware, bevor es hierher kommt -    */
/* dort liegen die Rueckmeldungen der Waermepumpe. Ein Automat, der stumm    */
/* nichts tut, waere schwerer zu deuten als ein gesperrter Knopf mit         */
/* Klartext.                                                                 */
/*                                                                          */
/* NOTBETRIEB_SENDEN heisst hier "Schritt 1 ist abzusetzen" - ABGESETZT wird */
/* er seit 3.15.0 aber nicht mehr vom Aufrufer, sondern vom naechsten Tick   */
/* aus loop() (siehe schritt_gesendet oben). Der Rueckgabewert bleibt, weil  */
/* er die einzige Auskunft ist, ob der Klick ueberhaupt einen Lauf angestos- */
/* sen hat.                                                                  */
/*****************************************************************************/
inline NotbetriebAktion notbetrieb_start(NotbetriebLauf *lauf, uint32_t jetzt)
{
    if (!lauf || lauf->zustand == NOTBETRIEB_LAEUFT)
        return NOTBETRIEB_TU_NICHTS;
    lauf->zustand = NOTBETRIEB_LAEUFT;
    lauf->schritt = 0;
    lauf->schritt_gesendet = 0; // der Tick setzt ihn ab, nicht der Webhandler
    lauf->grund = NOTBETRIEB_GRUND_KEINER;
    lauf->schritt_start = jetzt;
    lauf->lauf_start = jetzt;
    lauf->ende = 0; // das Ergebnis des vorigen Laufs gilt nicht mehr
    return NOTBETRIEB_SENDEN;
}

/*****************************************************************************/
/* Ein Tick aus loop()                                                       */
/*                                                                           */
/* schritt_bestaetigt liefert die Firmware aus actual_data[] (Vergleich des  */
/* TOP-Werts mit dem gesendeten Wert). Der Header bleibt damit frei von      */
/* actual_data, und der Hosttest kann jede Reihenfolge durchspielen.         */
/*                                                                           */
/* Die Zeitvergleiche laufen in unsigned-Arithmetik ueber die Differenz -    */
/* wie in sendwindow.h, damit sie den millis()-Ueberlauf nach 49,7 Tagen     */
/* ueberstehen. uint32_t und nicht unsigned long: auf dem Mac waere das      */
/* 64 Bit, der Hosttest wuerde den Ueberlauf dann gegen nichts pruefen.      */
/*                                                                           */
/* Reihenfolge der Pruefungen: Erst der Kuehl-Abbruch, dann das Absetzen     */
/* eines noch ausstehenden Schritts, dann die Bestaetigung, dann die         */
/* Zeitgrenzen. Der Kuehl-Abbruch steht vorn, weil ein bestaetigter          */
/* Schritt in einer kuehlenden Anlage nichts wert ist - die Folge duerfte     */
/* dann keinesfalls bis zum letzten Schritt laufen und die Anlage             */
/* einschalten. Bestaetigung vor Timeout: Trifft die Rueckmeldung in          */
/* derselben Runde ein, in der das Timeout ablaeuft, gilt der Schritt als     */
/* geschafft - ein knapp erreichtes Ziel ist erreicht.                        */
/*                                                                            */
/* heiz_kuehl_text ist der Rohtext von TOP101 aus actual_data[]. 0 (Nullzeiger)*/
/* heisst "keine Aussage" und bricht nichts ab.                               */
/*****************************************************************************/
inline NotbetriebAktion notbetrieb_tick(NotbetriebLauf *lauf, NotbetriebRolle rolle,
                                        uint32_t jetzt, bool schritt_bestaetigt,
                                        const char *heiz_kuehl_text)
{
    if (!lauf || lauf->zustand != NOTBETRIEB_LAEUFT)
        return NOTBETRIEB_TU_NICHTS;

    // Die Anlage meldet mitten im Lauf Kuehlen - der KNX-Schalter ist umgelegt
    // worden, oder er stand nie auf Heizen. Weiterzumachen hiesse, am Ende eine
    // kuehlende Anlage einzuschalten; genau das soll der Knopf verhindern.
    if (notbetrieb_kuehlbetrieb_gemeldet(rolle, heiz_kuehl_text))
    {
        notbetrieb_abschluss(lauf, NOTBETRIEB_ROT, jetzt, NOTBETRIEB_GRUND_KUEHLEN);
        return NOTBETRIEB_ABBRUCH_KUEHLEN;
    }

    // Der aktuelle Schritt steht noch aus - das ist die Lage unmittelbar nach
    // dem Klick, seit der Webhandler den ersten Schritt nicht mehr selbst
    // absetzt (siehe schritt_gesendet). Die Schrittuhr laeuft ab HIER und
    // nicht ab dem Klick: Sonst zaehlte die Mindestwartezeit schon, bevor das
    // Kommando ueberhaupt unterwegs war.
    if (!lauf->schritt_gesendet)
    {
        lauf->schritt_gesendet = 1;
        lauf->schritt_start = jetzt;
        return NOTBETRIEB_SENDEN;
    }

    // Schritt geschafft? Erst nach der Mindestwartezeit - vorher koennte die
    // Rueckmeldung noch vom Zustand VOR dem Kommando stammen (siehe
    // NOTBETRIEB_SCHRITT_MINDESTWARTE_MS oben).
    if (schritt_bestaetigt &&
        (uint32_t)(jetzt - lauf->schritt_start) >= NOTBETRIEB_SCHRITT_MINDESTWARTE_MS)
    {
        lauf->schritt++;
        if (lauf->schritt >= notbetrieb_schritt_anzahl(rolle))
        {
            notbetrieb_abschluss(lauf, NOTBETRIEB_GRUEN, jetzt, NOTBETRIEB_GRUND_KEINER);
            return NOTBETRIEB_FERTIG;
        }
        lauf->schritt_start = jetzt;
        lauf->schritt_gesendet = 1; // dieser Tick erteilt den Auftrag mit
        return NOTBETRIEB_SENDEN;
    }

    // Notausgang: der Automat haengt (siehe Zeitregeln oben)
    if ((uint32_t)(jetzt - lauf->lauf_start) >= notbetrieb_gesamtdeckel_ms(rolle))
    {
        notbetrieb_abschluss(lauf, NOTBETRIEB_ROT, jetzt,
                             notbetrieb_grund_fuer_schritt(rolle, lauf->schritt));
        return NOTBETRIEB_ABBRUCH;
    }

    // Der uebliche Weg zu ROT: ein Schritt kam nicht zurueck.
    // Abgebrochen wird SOFORT und nicht weitergemacht - steht die Betriebsart
    // nicht, sind die Kurvenwerte danach sinnlos.
    if ((uint32_t)(jetzt - lauf->schritt_start) >= NOTBETRIEB_SCHRITT_TIMEOUT_MS)
    {
        notbetrieb_abschluss(lauf, NOTBETRIEB_ROT, jetzt,
                             notbetrieb_grund_fuer_schritt(rolle, lauf->schritt));
        return NOTBETRIEB_ABBRUCH;
    }

    return NOTBETRIEB_TU_NICHTS;
}

/*****************************************************************************/
/* Welchen Wert traegt der aktuelle Schritt?                                 */
/*                                                                           */
/* Rueckgabe false, wenn der Schritt einen gehaltenen Wert braucht, der      */
/* nicht vorliegt - dann darf nicht gesendet werden.                         */
/*                                                                           */
/* ACHTUNG: Ein Hydraulikschritt liefert IMMER false - er hat keinen Wert,   */
/* den man senden koennte. Wer ihn hier hineinreicht, hat den Typ nicht      */
/* geprueft; das ist ein Fehler und soll wie einer aussehen, statt still     */
/* eine 0 zu liefern. notbetrieb.cpp verzweigt vorher nach Typ.              */
/*****************************************************************************/
inline bool notbetrieb_schritt_wert(const NotbetriebLauf *lauf, NotbetriebRolle rolle,
                                    const NotbetriebSpeicher *sp, int *wert_out)
{
    if (!lauf || !sp || !wert_out)
        return false;
    const NotbetriebSchritt *s = notbetrieb_schritt(rolle, lauf->schritt);
    if (!s)
        return false;
    if (s->typ == NB_SCHRITT_HYDRAULIK)
        return false; // kein Set-Kommando, kein Wert - siehe Kommentar oben

    if (s->wert_index == NOTBETRIEB_FESTER_WERT)
    {
        *wert_out = s->fester_wert;
        return true;
    }
    if (s->wert_index < 0 || (unsigned)s->wert_index >= notbetrieb_wert_anzahl(rolle))
        return false;
    if ((sp->gesetzt & (uint8_t)(1u << s->wert_index)) == 0)
        return false;

    *wert_out = sp->werte[s->wert_index];
    return true;
}
