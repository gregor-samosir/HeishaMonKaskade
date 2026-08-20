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
/* Zyklen Reserve; ein vollstaendiger Heizen-Lauf braucht real etwa 36 s.    */
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
/* Reserve ab. Damit dauert ein Heizen-Lauf rund 48 statt 36 s - das ist der  */
/* Preis dafuer, dass GRUEN wirklich "zurueckgelesen" heisst.                 */
/*****************************************************************************/
#define NOTBETRIEB_SCHRITT_MINDESTWARTE_MS 8000u

/*****************************************************************************/
/* Ein Schritt der Folge                                                     */
/*                                                                           */
/* set_name  - Topic-Name unter <prefix>/set/, wird an Topics::SET gehaengt  */
/* top       - TOP-Nummer, an der zurueckgelesen wird. GRUEN heisst          */
/*             zurueckgelesen, nicht abgesendet - ein GRUEN, das nur         */
/*             "gesendet" bedeutet, waere in dieser Lage wertlos.            */
/* wert_index - Index in die gehaltenen Werte, oder NOTBETRIEB_FESTER_WERT   */
/* fester_wert - gilt nur bei NOTBETRIEB_FESTER_WERT                         */
/*****************************************************************************/
#define NOTBETRIEB_FESTER_WERT (-1)

struct NotbetriebSchritt
{
    const char *set_name;
    int top;
    int wert_index;
    int fester_wert;
};

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
    "Z1HeatCurveTargetHighTemp", // Vorlauf bei KAELTE - gehoert zu OutsideLow
    "Z1HeatCurveTargetLowTemp",  // Vorlauf bei WAERME - gehoert zu OutsideHigh
    "Z1HeatCurveOutsideLowTemp",
    "Z1HeatCurveOutsideHighTemp"};

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
/*****************************************************************************/
static const NotbetriebSchritt NOTBETRIEB_SCHRITTE_HEIZEN[] = {
    {"HeatingMode", 76, NOTBETRIEB_FESTER_WERT, 0}, // erst umschalten
    {"Z1HeatCurveTargetHighTemp", 29, 0, 0},        // dann die Kurve
    {"Z1HeatCurveTargetLowTemp", 30, 1, 0},
    {"Z1HeatCurveOutsideLowTemp", 32, 2, 0},
    {"Z1HeatCurveOutsideHighTemp", 31, 3, 0},
    {"Heatpump", 0, NOTBETRIEB_FESTER_WERT, 1}}; // zuletzt einschalten

/*****************************************************************************/
/* Warmwasser braucht keine Kurve - der Knopf dort ist deutlich einfacher.   */
/* OperationMode 3 (DHW only) traegt auch dann, wenn die Anlage vom          */
/* KNX-Aktor auf Kuehlen steht - also im Sommerfall, fuer den der Notbetrieb */
/* an Stufe 2 ueberhaupt gedacht ist (gemessen 2026-08-20 an H2, M3).        */
/*****************************************************************************/
static const NotbetriebSchritt NOTBETRIEB_SCHRITTE_WASSER[] = {
    {"OperationMode", 4, NOTBETRIEB_FESTER_WERT, 3},
    {"DHWTemp", 9, 0, 0},
    {"Heatpump", 0, NOTBETRIEB_FESTER_WERT, 1}};

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
    NOTBETRIEB_TU_NICHTS = 0, // warten, der Schritt laeuft noch
    NOTBETRIEB_SENDEN = 1,    // aktuellen Schritt absetzen
    NOTBETRIEB_FERTIG = 2,    // gerade GRUEN geworden
    NOTBETRIEB_ABBRUCH = 3    // gerade ROT geworden
};

struct NotbetriebLauf
{
    uint8_t zustand;
    uint8_t schritt;
    uint32_t schritt_start;
    uint32_t lauf_start;
};

inline void notbetrieb_lauf_leeren(NotbetriebLauf *lauf)
{
    if (!lauf)
        return;
    lauf->zustand = NOTBETRIEB_BEREIT;
    lauf->schritt = 0;
    lauf->schritt_start = 0;
    lauf->lauf_start = 0;
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
/* Die Vollstaendigkeit der Werte wird hier NICHT geprueft: Das entscheidet  */
/* die Seite, bevor sie den Knopf ueberhaupt anbietet. Ein Automat, der      */
/* stumm nichts tut, waere schwerer zu deuten als ein gesperrter Knopf mit   */
/* Klartext.                                                                 */
/*****************************************************************************/
inline NotbetriebAktion notbetrieb_start(NotbetriebLauf *lauf, uint32_t jetzt)
{
    if (!lauf || lauf->zustand == NOTBETRIEB_LAEUFT)
        return NOTBETRIEB_TU_NICHTS;
    lauf->zustand = NOTBETRIEB_LAEUFT;
    lauf->schritt = 0;
    lauf->schritt_start = jetzt;
    lauf->lauf_start = jetzt;
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
/* Reihenfolge der Pruefungen: Bestaetigung vor Timeout. Trifft die          */
/* Rueckmeldung in derselben Runde ein, in der das Timeout ablaeuft, gilt    */
/* der Schritt als geschafft - ein knapp erreichtes Ziel ist erreicht.       */
/*****************************************************************************/
inline NotbetriebAktion notbetrieb_tick(NotbetriebLauf *lauf, NotbetriebRolle rolle,
                                        uint32_t jetzt, bool schritt_bestaetigt)
{
    if (!lauf || lauf->zustand != NOTBETRIEB_LAEUFT)
        return NOTBETRIEB_TU_NICHTS;

    // Schritt geschafft? Erst nach der Mindestwartezeit - vorher koennte die
    // Rueckmeldung noch vom Zustand VOR dem Kommando stammen (siehe
    // NOTBETRIEB_SCHRITT_MINDESTWARTE_MS oben).
    if (schritt_bestaetigt &&
        (uint32_t)(jetzt - lauf->schritt_start) >= NOTBETRIEB_SCHRITT_MINDESTWARTE_MS)
    {
        lauf->schritt++;
        if (lauf->schritt >= notbetrieb_schritt_anzahl(rolle))
        {
            lauf->zustand = NOTBETRIEB_GRUEN;
            return NOTBETRIEB_FERTIG;
        }
        lauf->schritt_start = jetzt;
        return NOTBETRIEB_SENDEN;
    }

    // Notausgang: der Automat haengt (siehe Zeitregeln oben)
    if ((uint32_t)(jetzt - lauf->lauf_start) >= notbetrieb_gesamtdeckel_ms(rolle))
    {
        lauf->zustand = NOTBETRIEB_ROT;
        return NOTBETRIEB_ABBRUCH;
    }

    // Der uebliche Weg zu ROT: ein Schritt kam nicht zurueck.
    // Abgebrochen wird SOFORT und nicht weitergemacht - steht die Betriebsart
    // nicht, sind die Kurvenwerte danach sinnlos.
    if ((uint32_t)(jetzt - lauf->schritt_start) >= NOTBETRIEB_SCHRITT_TIMEOUT_MS)
    {
        lauf->zustand = NOTBETRIEB_ROT;
        return NOTBETRIEB_ABBRUCH;
    }

    return NOTBETRIEB_TU_NICHTS;
}

/*****************************************************************************/
/* Ist ein Schritt zurueckgelesen?                                           */
/*                                                                           */
/* Die Firmware haelt die Rueckmeldungen der Waermepumpe als TEXT in          */
/* actual_data[]. Der naive Vergleich waere atoi(text) == soll - und der      */
/* hat eine Falle, die genau das Signal zerstoert, auf das es hier ankommt:   */
/* atoi("") liefert 0. Ein TOP, das noch nie empfangen wurde, wuerde damit    */
/* jeden Schritt mit dem Sollwert 0 sofort bestaetigen - und der erste        */
/* Schritt der Heizen-Folge ist HeatingMode = 0. Der Knopf wuerde GRUEN       */
/* melden, ohne dass die Waermepumpe je geantwortet hat.                      */
/*                                                                           */
/* Deshalb: leerer Text gilt NICHT als Bestaetigung, und was keine saubere    */
/* ganze Zahl ist, auch nicht. Fuehrende und nachlaufende Leerzeichen sind    */
/* erlaubt, sonst nichts.                                                    */
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
/* Welchen Wert traegt der aktuelle Schritt?                                 */
/*                                                                           */
/* Rueckgabe false, wenn der Schritt einen gehaltenen Wert braucht, der      */
/* nicht vorliegt - dann darf nicht gesendet werden.                         */
/*****************************************************************************/
inline bool notbetrieb_schritt_wert(const NotbetriebLauf *lauf, NotbetriebRolle rolle,
                                    const NotbetriebSpeicher *sp, int *wert_out)
{
    if (!lauf || !sp || !wert_out)
        return false;
    const NotbetriebSchritt *s = notbetrieb_schritt(rolle, lauf->schritt);
    if (!s)
        return false;

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
