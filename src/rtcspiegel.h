#pragma once
#include <stdint.h>
#include <string.h>

#include "notbetrieb.h" // NotbetriebRolle, NotbetriebSpeicher, NOTBETRIEB_MAX_WERTE

/*****************************************************************************/
/* Was einen Software-Reset ueberleben darf - und was nicht                  */
/*                                                                           */
/* Bewusst frei von Arduino-Abhaengigkeiten, gleiches Muster wie             */
/* sendwindow.h, telegram.h, verbindung.h und notbetrieb.h: derselbe Code    */
/* laeuft in der Firmware und im Hosttest (test/rtcspiegel_test.cpp). Auf    */
/* dem Geraet liegt die Struktur in RTC_NOINIT_ATTR; hier steht nur die      */
/* Regel, wann ihr Inhalt zu trauen ist.                                     */
/*                                                                           */
/* WARUM ES DAS GIBT (M2 und M3 der Codedurchsicht 2026-09-02):              */
/*                                                                           */
/* Die Notbetriebswerte liegen seit 3.12.0 nur im RAM. Der Owner-Entscheid   */
/* vom 2026-08-20 lautete: keine Datei auf LittleFS. Die Begruendung war,    */
/* dass ein Neustart bei weggefallenem Broker einen Stromausfall bedeutet -  */
/* und der laeuft ohne Notbetrieb, weil die Waermepumpe selbst laenger       */
/* braucht als die Steuerung.                                                */
/*                                                                           */
/* Diese Annahme traegt nicht mehr, weil die FIRMWARE SELBST neu startet:    */
/*   - der WLAN-Watchdog nach 5 min ohne WLAN (WIFI_REBOOT_TIMEOUT, 3.5.0,   */
/*     also aelter als der Notbetrieb),                                      */
/*   - /reboot und jedes OTA.                                                */
/* Der unguenstige Fall: Der Server im Keller ist tot - das IST der          */
/* Notbetriebsfall -, und der WLAN-Router startet neu oder bekommt ein       */
/* Update, das laenger als 5 min dauert. Die Bridge startet neu, die Werte   */
/* sind weg, und der Knopf meldet "Nicht bereit - es fehlen Werte" genau     */
/* dann, wenn jemand ihn braucht.                                            */
/*                                                                           */
/* DIE TRENNLINIE DES OWNER-ENTSCHEIDS BLEIBT DABEI EXAKT ERHALTEN. Der      */
/* RTC-Speicher ueberlebt ESP.restart(), den Watchdog-Reset und das OTA,     */
/* NICHT aber das Stromlosmachen. Kein Flash-Schreibzugriff, keine Datei,    */
/* kein Verschleiss - und nach echtem Stromausfall ist er leer, wie          */
/* entschieden.                                                             */
/*                                                                           */
/* Im selben Block liegt der BOOTZAEHLER (M3). Er zaehlt damit genau die     */
/* Neustarts, die der Spiegel ueberlebt - also die Software-Resets - und     */
/* steht nach einem Stromausfall auf 1. Zusammen mit esp_reset_reason()      */
/* unterscheidet das "Strom war weg" von "die Firmware hat neu gestartet".   */
/* Ohne das sieht ein Geraet, das alle paar Tage neu startet, von aussen     */
/* gesund aus: Das LWT kommt binnen Sekunden zurueck, und "WLAN war X s weg" */
/* wird nur gemeldet, wenn das Geraet die Zeit OHNE Neustart ueberstanden    */
/* hat.                                                                      */
/*                                                                           */
/* WAS DIESER HEADER NICHT PRUEFT: ob ein einzelner Wert im erlaubten        */
/* Bereich liegt. Die Grenzen stehen in setCommands[] (commands.cpp) und     */
/* sind ohne Arduino nicht erreichbar. Die Firmware laesst deshalb jeden     */
/* uebernommenen Wert erneut durch set_command_range() und                   */
/* notbetrieb_wert_annehmen() laufen - denselben Weg, den auch eine frische  */
/* MQTT-Nachricht geht. Hier faellt nur die Entscheidung, ob der Spiegel     */
/* ueberhaupt anzufassen ist.                                                */
/*****************************************************************************/

/*****************************************************************************/
/* Magic mit Layoutnummer im untersten Byte                                  */
/*                                                                           */
/* Das Magic allein wuerde nur "hier stand schon einmal etwas" belegen. Nach */
/* einem OTA kann im RTC-Speicher aber ein Spiegel der VORHERIGEN Firmware   */
/* liegen - mit derselben Kennung und einer anderen Struktur, wenn sich      */
/* NOTBETRIEB_MAX_WERTE oder die Feldreihenfolge geaendert haben. Die        */
/* Pruefsumme faellt darauf herein, weil sie nur ueber die Felder rechnet,   */
/* die diese Firmware kennt.                                                 */
/*                                                                           */
/* DESHALB: Wer an RtcSpiegel etwas aendert - Feld dazu, Feld raus, anderer  */
/* Typ, andere Reihenfolge - erhoeht das unterste Byte. Ein Spiegel der      */
/* Vorversion gilt dann als ungueltig und wird verworfen, statt falsch       */
/* gelesen zu werden. Der Preis ist ein einziger Boot ohne gehaltene Werte,  */
/* genau der Boot, in dem die neue Firmware ohnehin gerade eingespielt wird. */
/*****************************************************************************/
#define RTC_SPIEGEL_MAGIC 0x4E425301u // 'NBS' + Layout 01

/*****************************************************************************/
/* Der Spiegel                                                               */
/*                                                                           */
/* Feste Breiten statt int/unsigned: Die Struktur liegt in einem Speicher,   */
/* den eine ANDERE Uebersetzung derselben Firmware wieder liest (OTA). Was   */
/* der Compiler fuer "int" haelt, gehoert nicht in ein Format, das ueber     */
/* einen Neustart hinweg gelten soll.                                        */
/*                                                                           */
/* werte[] und gesetzt sind die Kopie aus NotbetriebSpeicher. rolle steht    */
/* mit dabei, damit ein Spiegel der jeweils anderen Stufe auffaellt: Die     */
/* Backup-Boards tragen abwechselnd beide Rollen, und ein Wertesatz fuer     */
/* Warmwasser in einer Heizen-Firmware waere kein Fehler, den man sieht -    */
/* die Namen der vier Werte sind verschieden, aber die Bitmaske passt.       */
/*****************************************************************************/
struct RtcSpiegel
{
    uint32_t magic;
    uint8_t rolle;        // NotbetriebRolle, auf 8 Bit festgelegt
    uint8_t gesetzt;      // Bitmaske wie in NotbetriebSpeicher
    uint16_t bootzaehler; // saettigt bei 65535, laeuft nicht ueber
    int32_t werte[NOTBETRIEB_MAX_WERTE];
    uint32_t pruefsumme; // ueber alle Felder darueber
};

/*****************************************************************************/
/* Pruefsumme, feldweise gerechnet                                           */
/*                                                                           */
/* FNV-1a ueber die EINZELNEN Felder und nicht ueber den rohen Speicher:     */
/* Fuellbytes zwischen den Feldern haetten im RTC-Speicher einen beliebigen  */
/* Inhalt, und der Hosttest auf dem Mac wuerde sie anders legen als der      */
/* Compiler fuer den ESP32. Eine Pruefsumme, die vom Padding abhaengt, waere */
/* auf dem Geraet zufaellig und im Test gruen.                               */
/*****************************************************************************/
inline uint32_t rtc_pruefsumme(const RtcSpiegel *sp)
{
    if (!sp)
        return 0;

    uint32_t h = 2166136261u; // FNV-1a Startwert
    const uint32_t felder[] = {
        sp->magic,
        (uint32_t)sp->rolle,
        (uint32_t)sp->gesetzt,
        (uint32_t)sp->bootzaehler,
    };
    for (unsigned i = 0; i < sizeof(felder) / sizeof(felder[0]); i++)
    {
        for (unsigned b = 0; b < 4; b++)
        {
            h ^= (felder[i] >> (8u * b)) & 0xFFu;
            h *= 16777619u;
        }
    }
    for (unsigned i = 0; i < NOTBETRIEB_MAX_WERTE; i++)
    {
        const uint32_t w = (uint32_t)sp->werte[i];
        for (unsigned b = 0; b < 4; b++)
        {
            h ^= (w >> (8u * b)) & 0xFFu;
            h *= 16777619u;
        }
    }
    return h;
}

/*****************************************************************************/
/* Siegeln: Magic setzen und die Pruefsumme neu rechnen                      */
/*                                                                           */
/* Nach JEDER Aenderung an einem Feld aufzurufen. Ein Spiegel, der einmal    */
/* ungesiegelt bleibt, gilt beim naechsten Boot als kaputt - das ist die     */
/* richtige Richtung des Fehlers, kostet aber die gehaltenen Werte.          */
/*****************************************************************************/
inline void rtc_siegeln(RtcSpiegel *sp)
{
    if (!sp)
        return;
    sp->magic = RTC_SPIEGEL_MAGIC;
    sp->pruefsumme = rtc_pruefsumme(sp);
}

/*****************************************************************************/
/* Leeren - alles auf einen definierten Anfangszustand                       */
/*****************************************************************************/
inline void rtc_leeren(RtcSpiegel *sp, NotbetriebRolle rolle)
{
    if (!sp)
        return;
    sp->rolle = (uint8_t)rolle;
    sp->gesetzt = 0;
    sp->bootzaehler = 0;
    for (unsigned i = 0; i < NOTBETRIEB_MAX_WERTE; i++)
        sp->werte[i] = 0;
    rtc_siegeln(sp);
}

/*****************************************************************************/
/* Ist dem Inhalt zu trauen?                                                 */
/*                                                                           */
/* Vier Bedingungen, alle noetig:                                            */
/*   1. Magic samt Layoutnummer - "hier stand ein Spiegel DIESER Struktur"   */
/*   2. dieselbe Rolle - kein Wertesatz der anderen Stufe                    */
/*   3. keine gesetzten Bits jenseits der Wertezahl dieser Rolle. Warmwasser */
/*      hat einen Wert, Heizen vier; ein Bit 5 gehoert zu keinem Wert und    */
/*      wuerde spaeter aus notbetrieb_vollstaendig() ein falsches "ja"       */
/*      machen.                                                              */
/*   4. Pruefsumme - faengt den Bitkipper im RTC-Speicher und den halb       */
/*      geschriebenen Stand ab, wenn der Reset mitten in eine Aenderung      */
/*      faellt.                                                              */
/*****************************************************************************/
inline bool rtc_gueltig(const RtcSpiegel *sp, NotbetriebRolle erwartete_rolle)
{
    if (!sp)
        return false;
    if (sp->magic != RTC_SPIEGEL_MAGIC)
        return false;
    if (sp->rolle != (uint8_t)erwartete_rolle)
        return false;

    const unsigned n = notbetrieb_wert_anzahl(erwartete_rolle);
    const uint8_t erlaubt = (uint8_t)((1u << n) - 1u);
    if ((sp->gesetzt & (uint8_t)~erlaubt) != 0)
        return false;

    return sp->pruefsumme == rtc_pruefsumme(sp);
}

/*****************************************************************************/
/* Der Boot-Schritt - ein einziger Einstiegspunkt                            */
/*                                                                           */
/* Beim Start genau einmal aufzurufen. Danach ist der Spiegel in JEDEM Fall  */
/* gesiegelt und in sich stimmig, egal was vorher im RTC-Speicher stand.     */
/*                                                                           */
/* Rueckgabe true heisst: Der Inhalt stammt aus dem Lauf davor und die Werte */
/* sind es wert, angesehen zu werden. Was mit ihnen geschieht, entscheidet   */
/* der Aufrufer - jeder einzelne laeuft danach noch durch die Grenzen aus    */
/* setCommands[].                                                            */
/*                                                                           */
/* Der Bootzaehler wird in BEIDEN Faellen erhoeht. Nach einem Stromausfall   */
/* ist er unmittelbar davor auf 0 gesetzt worden und steht damit auf 1 -     */
/* genau die Unterscheidung, die M3 haben will. Er saettigt bei 65535 statt  */
/* auf 0 zu springen: Ein Zaehler, der ueberlaeuft, meldet ausgerechnet nach */
/* sehr vielen Neustarts wieder "alles in Ordnung".                          */
/*****************************************************************************/
inline bool rtc_spiegel_boot(RtcSpiegel *sp, NotbetriebRolle rolle)
{
    if (!sp)
        return false;

    const bool gueltig = rtc_gueltig(sp, rolle);
    if (!gueltig)
        rtc_leeren(sp, rolle); // setzt bootzaehler auf 0

    if (sp->bootzaehler < UINT16_MAX)
        sp->bootzaehler++;

    rtc_siegeln(sp);
    return gueltig;
}

/*****************************************************************************/
/* Die gehaltenen Werte in den Spiegel schreiben                             */
/*                                                                           */
/* Aus notbetrieb_mqtt_annehmen() nach jeder uebernommenen Aenderung         */
/* aufgerufen. Der Bootzaehler bleibt dabei unberuehrt - er gehoert nicht    */
/* zum Notbetrieb, liegt aber im selben gesiegelten Block und wuerde sonst   */
/* bei jedem eintreffenden Kurvenwert verlorengehen.                         */
/*****************************************************************************/
inline void rtc_werte_spiegeln(RtcSpiegel *sp, const NotbetriebSpeicher *quelle,
                               NotbetriebRolle rolle)
{
    if (!sp || !quelle)
        return;

    sp->rolle = (uint8_t)rolle;
    sp->gesetzt = quelle->gesetzt;
    for (unsigned i = 0; i < NOTBETRIEB_MAX_WERTE; i++)
        sp->werte[i] = (int32_t)quelle->werte[i];

    rtc_siegeln(sp);
}
