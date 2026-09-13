#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/*****************************************************************************/
/* KNXnet/IP-Tunneling fuer den Notbetriebsschritt "Vorderhaus" (seit 3.21.0) */
/*                                                                           */
/* Faellt die uebergeordnete Steuerung aus, bleiben Mischer und Mischerpumpe */
/* des Vorderhauses auf ihrem letzten Wert stehen - beide haengen am KNX-Bus */
/* und haben keine eigenen Rueckfallwerte. Der letzte Schritt der Heizen-    */
/* Folge schickt deshalb per Tunneling Telegramme an die KNX-IP-             */
/* Schnittstelle: Zwangsstellungen zurueck, Mischer auf 50 %, Pumpe ein -    */
/* und liest am Aktor zurueck.                                               */
/*                                                                           */
/* Bewusst frei von Arduino-Abhaengigkeiten, gleiches Muster wie             */
/* notbetrieb.h und telegram.h: Firmware (vorderhaus.cpp) und Hosttest       */
/* (test/knx_test.cpp) binden dieselbe Datei ein. Hier steht alles, was ohne */
/* Netz pruefbar ist: Rahmen bauen und zerlegen, die Sequenzregel, welche    */
/* Antwort zaehlt, das Urteil der Rueckleseregel A, die Einstellung und die  */
/* Fristen. Der Netzverkehr selbst (WiFiUDP, Warten, Quittieren) steht in    */
/* vorderhaus.cpp.                                                           */
/*                                                                           */
/* REFERENZ ist test/knx_tunnel.py 1.6.0. Die Rahmen hier muessen byteweise  */
/* denen des Werkzeugs gleichen, und die gleichen den Rohbytes, die am       */
/* 2026-09-12 an der Anlage mitgeschnitten wurden (Analyse-KNX-Vorderhaus.md,*/
/* Abschnitte 7 und 8). Der Hosttest prueft genau das.                       */
/*****************************************************************************/

/*****************************************************************************/
/* Adressen kodieren                                                         */
/*                                                                           */
/* Gruppenadresse dreistufig Haupt/Mitte/Unter: 5 + 3 + 8 Bit.              */
/* Physikalische Adresse Bereich.Linie.Geraet: 4 + 4 + 8 Bit.               */
/* constexpr, damit die Konstanten unten zur Uebersetzungszeit feststehen    */
/* und per static_assert gegen die Hexwerte aus den Mitschnitten gehalten    */
/* werden. Die Masken schneiden einen Tippfehler wie 6/8/0 still ab - genau  */
/* deshalb stehen die static_asserts daneben.                                */
/*****************************************************************************/
constexpr uint16_t knx_ga(unsigned haupt, unsigned mitte, unsigned unter)
{
    return (uint16_t)(((haupt & 0x1Fu) << 11) | ((mitte & 0x07u) << 8) | (unter & 0xFFu));
}

constexpr uint16_t knx_pa(unsigned bereich, unsigned linie, unsigned geraet)
{
    return (uint16_t)(((bereich & 0x0Fu) << 12) | ((linie & 0x0Fu) << 8) | (geraet & 0xFFu));
}

/*****************************************************************************/
/* Die Adressen dieser Anlage - feste Werte, keine Einstellungen             */
/*                                                                           */
/* Owner-Entscheid E1 (2026-09-13): Nur die Schnittstelle ist eine           */
/* Einstellung (knx_schnittstelle). Alles hier aendert sich nur, wenn jemand */
/* die Anlage in der ETS umprojektiert - dann muss diese Datei mit, und das  */
/* steht in README.md und Ablauf-Notbetrieb.md. Als Einstellung waere jede   */
/* Gruppenadresse ein Feld, in dem ein Tippfehler Schreibtelegramme an eine  */
/* FREMDE Gruppe schickt. Hier stehen sie einmal, gegen die Mitschnitte      */
/* geprueft.                                                                 */
/*                                                                           */
/* Gruppenadressen: Owner 2026-09-12, Datentypen aus openknx (Analyse,       */
/* Abschnitt 8). Reihenfolge der Telegramme im Schritt: erst beide           */
/* Zwangsstellungen zurueck - sie gehen vor, ein Positionsbefehl allein      */
/* bliebe wirkungslos -, dann die Position, dann die Pumpe.                  */
/*****************************************************************************/
static constexpr uint16_t KNX_GA_ZWANG_AUF = knx_ga(6, 4, 17);    // DPT 1, im Notbetrieb 0
static constexpr uint16_t KNX_GA_ZWANG_ZU = knx_ga(6, 4, 16);     // DPT 1, im Notbetrieb 0
static constexpr uint16_t KNX_GA_POS_EINGANG = knx_ga(6, 4, 13);  // DPT 5, 128 schreiben, zuruecklesen
static constexpr uint16_t KNX_GA_POS_STATUS = knx_ga(6, 4, 12);   // DPT 5, lesen
static constexpr uint16_t KNX_GA_BEWEGUNG = knx_ga(6, 4, 14);     // DPT 1, 1 = faehrt; lesen oder spontan
static constexpr uint16_t KNX_GA_PUMPE = knx_ga(6, 4, 20);        // DPT 1, im Notbetrieb 1
static constexpr uint16_t KNX_GA_PUMPE_STATUS = knx_ga(6, 4, 21); // DPT 1, Ruecklesung

/*****************************************************************************/
/* Nur Antworten DIESER Aktoren zaehlen                                      */
/*                                                                           */
/* openknx (1.1.245) beantwortet das Lesen von 6/4/12 und 6/4/14 aus seinem  */
/* Zwischenspeicher, und zwar SCHNELLER als der Aktor, auf 6/4/14 sogar      */
/* dreifach (2026-09-12). Wer die erste Antwort nimmt, meldet im Test GRUEN  */
/* - und im Ernstfall ist openknx gar nicht da. knx_leseantwort() und        */
/* knx_meldung() unten pruefen deshalb die Quelle.                           */
/*****************************************************************************/
static constexpr uint16_t KNX_AKTOR_MISCHER = knx_pa(1, 1, 39); // beantwortete 6/4/12 selbst
static constexpr uint16_t KNX_AKTOR_PUMPE = knx_pa(1, 1, 60);   // einzige Antwort auf 6/4/21
static constexpr uint16_t KNX_OPENKNX = knx_pa(1, 1, 245);      // zaehlt NIE - nur fuer die Logzeile

/*****************************************************************************/
/* Die Quelladresse im Telegramm                                             */
/*                                                                           */
/* Owner-Wunsch 2026-09-12: 1.1.250 statt der Tunneladresse, die die         */
/* Schnittstelle vergibt. Sie uebernimmt die Adresse unveraendert auf den    */
/* Bus (Gegenprobe mit einem zweiten Tunnel, Analyse Abschnitt 8) - liefert  */
/* dann aber KEINE L_Data.con an den sendenden Tunnel. Wer auf die con       */
/* wartet, verliert je Telegramm die ganze Frist; wer sie verlangt, meldet   */
/* immer ROT. Die con entscheidet hier deshalb nichts, das Urteil faellt die */
/* Ruecklesung am Aktor. 1.1.250 ist an keinem anderen Geraet vergeben       */
/* (Owner bestaetigt).                                                       */
/*****************************************************************************/
static constexpr uint16_t KNX_QUELLE = knx_pa(1, 1, 250);

/*****************************************************************************/
/* Sollstellung des Mischers                                                 */
/*                                                                           */
/* Der Positionseingang laeuft 0-255 (HKMregelung.js: POS_MAX = 255), 128    */
/* sind also 50 %. Der Aktor meldet den Befehl exakt zurueck (250 -> 250);   */
/* die Toleranz von 2 ist Reserve, keine gemessene Streuung.                 */
/*****************************************************************************/
static constexpr uint8_t KNX_MISCHER_POSITION = 128;
static constexpr uint8_t KNX_MISCHER_TOLERANZ = 2;

#define KNX_PORT_STANDARD 3671

// Die Hexwerte stehen so in den Mitschnitten (Analyse, Abschnitte 7 und 8)
// bzw. im Simulator von knx_tunnel.py - ein Zahlendreher faellt hier beim
// Uebersetzen auf, nicht erst im Ernstfall.
static_assert(KNX_GA_ZWANG_AUF == 0x3411, "6/4/17");
static_assert(KNX_GA_ZWANG_ZU == 0x3410, "6/4/16");
static_assert(KNX_GA_POS_EINGANG == 0x340D, "6/4/13");
static_assert(KNX_GA_POS_STATUS == 0x340C, "6/4/12");
static_assert(KNX_GA_BEWEGUNG == 0x340E, "6/4/14");
static_assert(KNX_GA_PUMPE == 0x3414, "6/4/20 - Rohbytes Stufe 2: 34 14");
static_assert(KNX_GA_PUMPE_STATUS == 0x3415, "6/4/21 - Rohbytes Stufe 1: 34 15");
static_assert(KNX_AKTOR_MISCHER == 0x1127, "1.1.39");
static_assert(KNX_AKTOR_PUMPE == 0x113C, "1.1.60 - Rohbytes Stufe 1: 11 3c");
static_assert(KNX_QUELLE == 0x11FA, "1.1.250 - Rohbytes Nachweis Quelle: 11 fa");

/*****************************************************************************/
/* Die Fristen                                                               */
/*                                                                           */
/* Gemessen am 2026-09-12 (Analyse, Abschnitte 7 und 8): CONNECT_RESPONSE    */
/* 1-4 ms, TUNNELING_ACK 2 ms, Antwort eines Aktors auf Lesen 55-82 ms,      */
/* Bewegung 1 76-93 ms nach dem Positionsbefehl, Pumpenstatus 55-120 ms      */
/* nach dem Befehl. Die Fristen liegen um mehr als eine Groessenordnung      */
/* darueber und ENDEN AN DER ANTWORT, nicht am Fenster: Im Regelfall kostet  */
/* der ganze Austausch Zehntelsekunden.                                      */
/*                                                                           */
/* KNX_FRIST_ACK_MS ist die Frist der Spezifikation (1 s, genau eine         */
/* Wiederholung). KNX_FRIST_BEWEGUNG_MS ist die Frist der Rueckleseregel A,  */
/* wie entschieden (Owner 2026-09-12, bestaetigt 2026-09-13) - voll          */
/* ausgeschoepft nur, wenn der Mischer nicht losfaehrt, weil er schon am     */
/* Ziel steht.                                                               */
/*                                                                           */
/* KNX_FRIST_PUMPE_MS: Warten auf die spontane Statusmeldung der Pumpe.      */
/* Laeuft sie schon, meldet der Aktor auf denselben Wert NICHTS (2026-09-12) */
/* - dann wird nach dieser Frist aktiv gelesen.                              */
/*                                                                           */
/* KNX_FRIST_TRENNEN_MS: Die DISCONNECT_RESPONSE ist Hoeflichkeit, kein      */
/* Beleg. Bleibt sie aus, raeumt die Schnittstelle den Tunnel nach 120 s     */
/* selbst ab, und vier von fuenf Tunneln bleiben frei.                       */
/*****************************************************************************/
#define KNX_FRIST_CONNECT_MS 1000u
#define KNX_FRIST_ACK_MS 1000u
#define KNX_FRIST_LESEN_MS 1000u
#define KNX_FRIST_BEWEGUNG_MS 2000u
#define KNX_FRIST_PUMPE_MS 1000u
#define KNX_FRIST_TRENNEN_MS 500u

/*****************************************************************************/
/* Der Deckel einer Verbindung - und warum es einen braucht                  */
/*                                                                           */
/* Jede Verbindung ist ein geschlossener Austausch, der loop() blockiert -   */
/* NIE bleibt ein Tunnel ueber mehrere loop()-Durchlaeufe offen (Analyse     */
/* Abschnitt 8): Die MQTT-Wiederverbindung blockiert im Notbetriebsfall bis  */
/* 2 s, ein offener Tunnel muss aber jedes Bustelegramm binnen 1 s           */
/* quittieren.                                                               */
/*                                                                           */
/* Die Fenster der Befehlsverbindung addieren sich im unguenstigsten Fall -  */
/* jede Leseanfrage bleibt unbeantwortet, jede Wartezeit laeuft aus, Mischer */
/* und Pumpe brauchen beide ihre Wiederholung - zu KNX_BEFEHL_FENSTER_MS.    */
/* Dazu kaemen die ACKs, und die lassen sich nicht sinnvoll aufaddieren: 17  */
/* Telegramme mit je einer ACK-Wiederholung waeren weitere 17 s. Statt       */
/* einer Rechnung, die niemand glaubt, gibt es einen HARTEN Deckel: Ist er   */
/* erreicht, bricht die Verbindung ab (DISCONNECT, ROT). Er liegt 4 s ueber  */
/* den Fenstern - Platz fuer einige ACK-Wiederholungen; mehr davon heisst,   */
/* die Verbindung ist gestoert, und dann ist ROT die richtige Antwort.       */
/*                                                                           */
/* Die groesste Blockade von loop() ist damit KNX_BEFEHL_DECKEL_MS plus      */
/* KNX_FRIST_TRENNEN_MS = 20,5 s, begruendet in vorderhaus.cpp.              */
/*****************************************************************************/
static constexpr uint32_t KNX_BEFEHL_FENSTER_MS =
    KNX_FRIST_CONNECT_MS                                   // Verbindung aufbauen
    + KNX_FRIST_LESEN_MS                                   // Bewegung VOR den Befehlen
    + 2u * (KNX_FRIST_LESEN_MS                             // je Mischerversuch: Eingang,
            + KNX_FRIST_BEWEGUNG_MS                        //   spontane Bewegung,
            + KNX_FRIST_LESEN_MS                           //   Bewegung lesen,
            + KNX_FRIST_LESEN_MS)                          //   Status lesen
    + 2u * (KNX_FRIST_PUMPE_MS + KNX_FRIST_LESEN_MS);      // je Pumpenversuch
#define KNX_BEFEHL_DECKEL_MS 20000u

// Die kurze Abfrageverbindung im Rueckfall: verbinden, 6/4/12 lesen.
static constexpr uint32_t KNX_ABFRAGE_FENSTER_MS = KNX_FRIST_CONNECT_MS + KNX_FRIST_LESEN_MS;
#define KNX_ABFRAGE_DECKEL_MS 4000u

static_assert(KNX_BEFEHL_FENSTER_MS < KNX_BEFEHL_DECKEL_MS,
              "Der Deckel der Befehlsverbindung muss ueber der Summe ihrer Fenster liegen");
static_assert(KNX_ABFRAGE_FENSTER_MS < KNX_ABFRAGE_DECKEL_MS,
              "Der Deckel der Abfrage muss ueber der Summe ihrer Fenster liegen");

/*****************************************************************************/
/* Der Rueckfall der Regel A                                                 */
/*                                                                           */
/* Faehrt der Mischer beim Druck schon (Endlagenlauf nach einem Wechsel der  */
/* Zwangsstellung, bis 144 s) oder schweigt der Aktor auf die Vorab-Lesung,  */
/* entscheidet die Endstellung: bis zu 144 s laufender Endlagenlauf, danach  */
/* der halbe Hub (60 s), dazu Reserve - 220 s. Abgefragt wird alle 10 s in   */
/* einer eigenen kurzen Verbindung; die spontane Meldung am Ziel sieht keine */
/* davon, GRUEN kommt also bis zu rund 14 s nach der Ankunft.                */
/*                                                                           */
/* Die 220 s sind KEINE eigene Uhr in der Firmware (Owner-Entscheid E2,      */
/* 2026-09-13). Sie gehen in das Schritt-Timeout des Vorderhausschritts ein  */
/* (notbetrieb.h), und das ist die einzige Frist: Zwei Uhren nebeneinander   */
/* liessen das Ergebnis davon abhaengen, welche zuerst ablaeuft.             */
/*****************************************************************************/
#define KNX_RUECKFALL_FRIST_MS 220000u
#define KNX_RUECKFALL_TAKT_MS 10000u

/*****************************************************************************/
/* KNXnet/IP und cEMI - die benutzten Kennungen                              */
/*****************************************************************************/
static constexpr uint16_t KNX_CONNECT_REQUEST = 0x0205;
static constexpr uint16_t KNX_CONNECT_RESPONSE = 0x0206;
static constexpr uint16_t KNX_DISCONNECT_REQUEST = 0x0209;
static constexpr uint16_t KNX_DISCONNECT_RESPONSE = 0x020A;
static constexpr uint16_t KNX_TUNNELING_REQUEST = 0x0420;
static constexpr uint16_t KNX_TUNNELING_ACK = 0x0421;

static constexpr uint8_t KNX_L_DATA_REQ = 0x11;
static constexpr uint8_t KNX_L_DATA_CON = 0x2E;
static constexpr uint8_t KNX_L_DATA_IND = 0x29;

static constexpr uint16_t KNX_APCI_READ = 0x000;
static constexpr uint16_t KNX_APCI_RESPONSE = 0x040;
static constexpr uint16_t KNX_APCI_WRITE = 0x080;

// Statuscode der CONNECT_RESPONSE, den die Logzeile beim Namen nennen soll:
// Alle fuenf Tunnel sind belegt (openknx haelt einen, eine offene ETS einen).
static constexpr uint8_t KNX_E_NO_MORE_CONNECTIONS = 0x24;

/*****************************************************************************/
/* ctrl1 und ctrl2 der eigenen Telegramme                                    */
/*                                                                           */
/* ctrl1 0xBC: Standardrahmen, "nicht wiederholen", Broadcast, Prioritaet    */
/* niedrig - so senden knx_tunnel.py und xknx, und so lief jedes Telegramm   */
/* an der Anlage. Die Analyse liess offen, ob das Wiederholbit geloescht     */
/* werden soll, damit die Schnittstelle bei einem fehlenden Layer-2-ACK      */
/* selbst wiederholt (Abschnitt 8). Es bleibt GESETZT: Die Wiederholung      */
/* uebernimmt Regel A eine Ebene hoeher, nach der Ruecklesung, und 0x9C ist  */
/* an dieser Schnittstelle nie gemessen.                                     */
/*                                                                           */
/* ctrl2 0xE0: Ziel ist eine Gruppe, Routingzaehler 6.                       */
/*****************************************************************************/
static constexpr uint8_t KNX_CTRL1_SENDEN = 0xBC;
static constexpr uint8_t KNX_CTRL2_GRUPPE = 0xE0;

/*****************************************************************************/
/* Rahmen bauen                                                              */
/*                                                                           */
/* Jede Funktion schreibt in einen Puffer des Aufrufers und liefert die      */
/* Laenge - oder 0, wenn der Puffer nicht reicht oder ein Argument fehlt.    */
/* 0 ist nie eine gueltige Laenge; der Aufrufer sendet dann nichts. Kein     */
/* Heap: Die Firmware kommt seit 2.3.0 in den heissen Pfaden ohne aus.       */
/*****************************************************************************/
inline size_t knx_kopf(uint8_t *out, size_t cap, uint16_t dienst, uint16_t gesamt)
{
    if (!out || cap < 6)
        return 0;
    out[0] = 0x06; // Kopflaenge
    out[1] = 0x10; // KNXnet/IP 1.0
    out[2] = (uint8_t)(dienst >> 8);
    out[3] = (uint8_t)(dienst & 0xFF);
    out[4] = (uint8_t)(gesamt >> 8);
    out[5] = (uint8_t)(gesamt & 0xFF);
    return 6;
}

// HPAI: Laenge 8, IPv4/UDP, Adresse, Port
inline size_t knx_hpai(uint8_t *out, size_t cap, const uint8_t ip[4], uint16_t port)
{
    if (!out || !ip || cap < 8)
        return 0;
    out[0] = 0x08;
    out[1] = 0x01;
    memcpy(out + 2, ip, 4);
    out[6] = (uint8_t)(port >> 8);
    out[7] = (uint8_t)(port & 0xFF);
    return 8;
}

/*****************************************************************************/
/* CONNECT_REQUEST                                                           */
/*                                                                           */
/* Die Firmware benutzt EINEN Socket als Steuer- und Datenendpunkt und       */
/* uebergibt beide Male dieselbe Adresse. Getrennt sind sie hier nur, weil   */
/* der Sollwert aus xknx zwei verschiedene Ports traegt.                     */
/* CRI: Laenge 4, TUNNEL_CONNECTION, TUNNEL_LINKLAYER, reserviert.           */
/*****************************************************************************/
inline size_t knx_baue_connect_request(uint8_t *out, size_t cap,
                                       const uint8_t steuer_ip[4], uint16_t steuer_port,
                                       const uint8_t daten_ip[4], uint16_t daten_port)
{
    const size_t n = 6 + 8 + 8 + 4;
    if (!out || cap < n || !steuer_ip || !daten_ip)
        return 0;
    knx_kopf(out, cap, KNX_CONNECT_REQUEST, (uint16_t)n);
    knx_hpai(out + 6, cap - 6, steuer_ip, steuer_port);
    knx_hpai(out + 14, cap - 14, daten_ip, daten_port);
    out[22] = 0x04;
    out[23] = 0x04;
    out[24] = 0x02;
    out[25] = 0x00;
    return n;
}

inline size_t knx_baue_disconnect_request(uint8_t *out, size_t cap, uint8_t kanal,
                                          const uint8_t steuer_ip[4], uint16_t steuer_port)
{
    const size_t n = 6 + 2 + 8;
    if (!out || cap < n || !steuer_ip)
        return 0;
    knx_kopf(out, cap, KNX_DISCONNECT_REQUEST, (uint16_t)n);
    out[6] = kanal;
    out[7] = 0x00;
    knx_hpai(out + 8, cap - 8, steuer_ip, steuer_port);
    return n;
}

// Antwort auf ein DISCONNECT_REQUEST der Schnittstelle - sie trennt von sich
// aus, wenn eine Quittung zweimal ausblieb.
inline size_t knx_baue_disconnect_response(uint8_t *out, size_t cap, uint8_t kanal, uint8_t status)
{
    const size_t n = 6 + 2;
    if (!out || cap < n)
        return 0;
    knx_kopf(out, cap, KNX_DISCONNECT_RESPONSE, (uint16_t)n);
    out[6] = kanal;
    out[7] = status;
    return n;
}

inline size_t knx_baue_tunneling_ack(uint8_t *out, size_t cap, uint8_t kanal, uint8_t seq,
                                     uint8_t status)
{
    const size_t n = 6 + 4;
    if (!out || cap < n)
        return 0;
    knx_kopf(out, cap, KNX_TUNNELING_ACK, (uint16_t)n);
    out[6] = 0x04; // Laenge des Verbindungskopfs
    out[7] = kanal;
    out[8] = seq;
    out[9] = status;
    return n;
}

/*****************************************************************************/
/* cEMI-L_Data fuer einen Gruppendienst                                      */
/*                                                                           */
/* Werte bis 6 Bit (DPT 1) stecken im APCI-Byte (NPDU-Laenge 1), laengere    */
/* folgen dahinter (DPT 5: Laenge 2, ein Datenbyte). daten_len > 14 passt    */
/* in keinen Standardrahmen und wird abgelehnt.                              */
/*****************************************************************************/
inline size_t knx_baue_cemi(uint8_t *out, size_t cap, uint8_t code, uint16_t quelle, uint16_t ga,
                            uint16_t apci, uint8_t klein, const uint8_t *daten, size_t daten_len,
                            uint8_t ctrl1)
{
    if (!out || daten_len > 14 || (daten_len > 0 && !daten))
        return 0;
    const size_t n = 9 + 2 + daten_len;
    if (cap < n)
        return 0;
    out[0] = code;
    out[1] = 0x00; // keine Zusatzinformation
    out[2] = ctrl1;
    out[3] = KNX_CTRL2_GRUPPE;
    out[4] = (uint8_t)(quelle >> 8);
    out[5] = (uint8_t)(quelle & 0xFF);
    out[6] = (uint8_t)(ga >> 8);
    out[7] = (uint8_t)(ga & 0xFF);
    out[8] = (uint8_t)(1 + daten_len);
    out[9] = (uint8_t)((apci >> 8) & 0x03);
    if (daten_len == 0)
    {
        out[10] = (uint8_t)((apci & 0xC0) | (klein & 0x3F));
    }
    else
    {
        out[10] = (uint8_t)(apci & 0xC0);
        memcpy(out + 11, daten, daten_len);
    }
    return n;
}

inline size_t knx_baue_tunneling_request(uint8_t *out, size_t cap, uint8_t kanal, uint8_t seq,
                                         const uint8_t *cemi, size_t cemi_len)
{
    if (!out || !cemi || cemi_len == 0)
        return 0;
    const size_t n = 6 + 4 + cemi_len;
    if (cap < n || n > 0xFFFF)
        return 0;
    knx_kopf(out, cap, KNX_TUNNELING_REQUEST, (uint16_t)n);
    out[6] = 0x04;
    out[7] = kanal;
    out[8] = seq;
    out[9] = 0x00;
    memcpy(out + 10, cemi, cemi_len);
    return n;
}

/*****************************************************************************/
/* Ein Gruppentelegramm des Schritts in einem Zug                            */
/*                                                                           */
/* Das ist der Weg, den vorderhaus.cpp nimmt: L_Data.req mit Quelle 1.1.250  */
/* und ctrl1 0xBC, verpackt in ein TUNNELING_REQUEST. Lesen: apci READ,      */
/* ohne Wert. Schreiben 1 Bit: apci WRITE, klein = 0/1. Schreiben 1 Byte:    */
/* apci WRITE, ein Datenbyte.                                                */
/*****************************************************************************/
inline size_t knx_baue_gruppentelegramm(uint8_t *out, size_t cap, uint8_t kanal, uint8_t seq,
                                        uint16_t quelle, uint16_t ga, uint16_t apci, uint8_t klein,
                                        const uint8_t *daten, size_t daten_len)
{
    uint8_t cemi[32];
    const size_t cemi_len = knx_baue_cemi(cemi, sizeof(cemi), KNX_L_DATA_REQ, quelle, ga, apci,
                                          klein, daten, daten_len, KNX_CTRL1_SENDEN);
    if (cemi_len == 0)
        return 0;
    return knx_baue_tunneling_request(out, cap, kanal, seq, cemi, cemi_len);
}

/*****************************************************************************/
/* Rahmen zerlegen                                                           */
/*                                                                           */
/* Alles, was vom Netz kommt, ist unzuverlaessig: Jede Laenge wird gegen die */
/* tatsaechliche Datagrammlaenge geprueft, bevor ein Byte gelesen wird.      */
/* false heisst "verwerfen" - der Aufrufer zaehlt das Datagramm nicht, bricht */
/* aber auch nichts ab.                                                      */
/*****************************************************************************/
inline bool knx_zerlege_kopf(const uint8_t *dg, size_t len, uint16_t *dienst,
                             const uint8_t **rumpf, size_t *rumpf_len)
{
    if (!dg || !dienst || !rumpf || !rumpf_len)
        return false;
    if (len < 6 || dg[0] != 0x06 || dg[1] != 0x10)
        return false;
    const size_t gesamt = ((size_t)dg[4] << 8) | dg[5];
    if (gesamt < 6 || gesamt > len)
        return false; // Laengenfeld passt nicht zum Datagramm
    *dienst = (uint16_t)((dg[2] << 8) | dg[3]);
    *rumpf = dg + 6;
    *rumpf_len = gesamt - 6;
    return true;
}

/*****************************************************************************/
/* CONNECT_RESPONSE                                                          */
/*                                                                           */
/* Kanal UND Datenendpunkt kommen aus der Antwort - die Schnittstelle        */
/* vergibt beide selbst (Stufe 0: Kanal 226, Stufe 1: 66; der Datenendpunkt  */
/* steht ausdruecklich drin). Angenommen werden darf keins von beiden.       */
/*                                                                           */
/* Eine Ablehnung (status != 0) ist eine GUELTIGE Antwort: Rueckgabe true,   */
/* nur kanal und status sind gesetzt. Der Aufrufer nennt den Code im Log -   */
/* "alle Tunnel belegt" ist ein anderer Fall als "Schnittstelle stumm".      */
/*****************************************************************************/
struct KnxConnectAntwort
{
    uint8_t kanal;
    uint8_t status;
    uint8_t daten_ip[4];
    uint16_t daten_port;
    uint16_t adresse; // Tunneladresse, vergibt die Schnittstelle (nur fuers Log)
};

inline bool knx_zerlege_connect_response(const uint8_t *rumpf, size_t len, KnxConnectAntwort *a)
{
    if (!rumpf || !a || len < 2)
        return false;
    memset(a, 0, sizeof(*a));
    a->kanal = rumpf[0];
    a->status = rumpf[1];
    if (a->status != 0)
        return true;
    if (len < 14)
        return false; // ohne Datenendpunkt und CRD nicht zu gebrauchen
    if (rumpf[2] != 0x08 || rumpf[3] != 0x01)
        return false; // HPAI ungueltig
    memcpy(a->daten_ip, rumpf + 4, 4);
    a->daten_port = (uint16_t)((rumpf[8] << 8) | rumpf[9]);
    if (rumpf[10] != 0x04 || rumpf[11] != 0x04)
        return false; // CRD beschreibt keinen Tunnel
    a->adresse = (uint16_t)((rumpf[12] << 8) | rumpf[13]);
    return true;
}

// Datenendpunkt 0.0.0.0 oder Port 0 heisst nach Spezifikation "derselbe wie
// der Steuerendpunkt" - dann geht der Datenverkehr an die Schnittstelle selbst.
inline bool knx_datenendpunkt_ist_steuer(const KnxConnectAntwort *a)
{
    if (!a)
        return true;
    const bool ip_leer = a->daten_ip[0] == 0 && a->daten_ip[1] == 0 &&
                         a->daten_ip[2] == 0 && a->daten_ip[3] == 0;
    return ip_leer || a->daten_port == 0;
}

// DISCONNECT_REQUEST der Schnittstelle: nur der Kanal interessiert
inline bool knx_zerlege_disconnect_request(const uint8_t *rumpf, size_t len, uint8_t *kanal)
{
    if (!rumpf || !kanal || len < 2)
        return false;
    *kanal = rumpf[0];
    return true;
}

inline bool knx_zerlege_tunneling_ack(const uint8_t *rumpf, size_t len, uint8_t *kanal,
                                      uint8_t *seq, uint8_t *status)
{
    if (!rumpf || !kanal || !seq || !status || len < 4 || rumpf[0] != 0x04)
        return false;
    *kanal = rumpf[1];
    *seq = rumpf[2];
    *status = rumpf[3];
    return true;
}

inline bool knx_zerlege_tunneling_request(const uint8_t *rumpf, size_t len, uint8_t *kanal,
                                          uint8_t *seq, const uint8_t **cemi, size_t *cemi_len)
{
    if (!rumpf || !kanal || !seq || !cemi || !cemi_len || len < 4 || rumpf[0] != 0x04)
        return false;
    *kanal = rumpf[1];
    *seq = rumpf[2];
    *cemi = rumpf + 4;
    *cemi_len = len - 4;
    return true;
}

/*****************************************************************************/
/* cEMI zerlegen                                                             */
/*****************************************************************************/
struct KnxCemi
{
    uint8_t code;  // L_Data.req/.con/.ind
    uint8_t ctrl1;
    uint8_t ctrl2;
    uint16_t quelle;
    uint16_t ziel;
    uint16_t apci;
    uint8_t laenge;     // NPDU-Laenge: 1 = Wert im APCI-Byte, > 1 = Datenbytes folgen
    uint8_t klein;      // Wert bis 6 Bit (nur bei laenge == 1)
    uint8_t daten[14];  // Datenbytes (nur bei laenge > 1)
    uint8_t daten_len;
};

inline bool knx_zerlege_cemi(const uint8_t *c, size_t len, KnxCemi *r)
{
    if (!c || !r || len < 2)
        return false;
    memset(r, 0, sizeof(*r));
    const size_t i = 2 + (size_t)c[1]; // Zusatzinformation ueberspringen
    if (len < i + 8)
        return false;
    const uint8_t laenge = c[i + 6];
    const uint8_t *apdu = c + i + 7;
    const size_t apdu_len = len - (i + 7);
    if (laenge < 1 || apdu_len < (size_t)laenge + 1)
        return false; // NPDU-Laenge passt nicht zum Datagramm
    if ((size_t)laenge - 1 > sizeof(r->daten))
        return false; // laenger als ein Standardrahmen
    r->code = c[0];
    r->ctrl1 = c[i];
    r->ctrl2 = c[i + 1];
    r->quelle = (uint16_t)((c[i + 2] << 8) | c[i + 3]);
    r->ziel = (uint16_t)((c[i + 4] << 8) | c[i + 5]);
    r->apci = (uint16_t)((((apdu[0] & 0x03) << 8) | apdu[1]) & 0x3C0);
    r->laenge = laenge;
    if (laenge == 1)
    {
        r->klein = (uint8_t)(apdu[1] & 0x3F);
    }
    else
    {
        r->daten_len = (uint8_t)(laenge - 1);
        memcpy(r->daten, apdu + 2, r->daten_len);
    }
    return true;
}

inline bool knx_gruppe(const KnxCemi *r)
{
    return r && (r->ctrl2 & 0x80);
}

/*****************************************************************************/
/* Ist eine L_Data.con positiv?                                              */
/*                                                                           */
/* NUR Bit 0 von ctrl1 zaehlt (gesetzt = Fehler). Nie das ganze Byte          */
/* vergleichen: Die Schnittstelle schickt 9c zurueck, wo bc gesendet wurde    */
/* (Stufe 1) - ein Vergleich laese jede positive Bestaetigung als Fehler.     */
/*                                                                           */
/* Die Firmware ENTSCHEIDET damit nichts (siehe KNX_QUELLE): Mit Quelle       */
/* 1.1.250 kommt gar keine con, und eine negative hiess an der Anlage nicht   */
/* "verloren" - das Telegramm lag auf dem Bus, der Aktor schaltete (Stufe 2).*/
/* Die Funktion gibt es fuer die Logzeile, falls doch eine eintrifft.         */
/*****************************************************************************/
inline bool knx_bestaetigt_ok(uint8_t ctrl1)
{
    return (ctrl1 & 0x01) == 0;
}

/*****************************************************************************/
/* Die Sequenzregel fuer eingehende TUNNELING_REQUESTs                       */
/*                                                                           */
/* Die Schnittstelle schickt jedes Bustelegramm an jeden Tunnel und          */
/* wiederholt es, wenn die Quittung binnen 1 s ausbleibt; nach der           */
/* Wiederholung trennt sie. Deshalb quittiert die Firmware in JEDER          */
/* Wartezeit, auch waehrend sie auf etwas ganz anderes wartet.               */
/*                                                                           */
/* Regel der Spezifikation (knx_tunnel.py, Tunnel._tunnel_empfangen()):      */
/*  - die erwartete Nummer: quittieren und verarbeiten, Zaehler weiter;      */
/*  - die vorige: nur quittieren - unsere Quittung ging verloren, die        */
/*    Schnittstelle wiederholt; ein zweites Verarbeiten zaehlte dieselbe     */
/*    Antwort doppelt;                                                       */
/*  - alles andere, und jeder fremde Kanal: verwerfen, OHNE Quittung.        */
/* Die Zaehler laufen 8 Bit und rollen von 255 auf 0.                        */
/*****************************************************************************/
enum KnxSequenz
{
    KNX_SEQ_VERWERFEN = 0,
    KNX_SEQ_VERARBEITEN = 1,
    KNX_SEQ_NUR_QUITTIEREN = 2
};

inline KnxSequenz knx_sequenz_regel(uint8_t kanal_eigen, uint8_t rx_erwartet, uint8_t kanal,
                                    uint8_t seq)
{
    if (kanal != kanal_eigen)
        return KNX_SEQ_VERWERFEN;
    if (seq == rx_erwartet)
        return KNX_SEQ_VERARBEITEN;
    if (seq == (uint8_t)(rx_erwartet - 1))
        return KNX_SEQ_NUR_QUITTIEREN;
    return KNX_SEQ_VERWERFEN;
}

/*****************************************************************************/
/* Welche Antwort zaehlt?                                                    */
/*                                                                           */
/* Zwei Faelle, beide nur vom genannten Aktor und nur als L_Data.ind - eine  */
/* con ist die Kopie des EIGENEN Telegramms und traegt die eigene Quelle:    */
/*  - knx_leseantwort(): GroupValueResponse auf ein eigenes Lesetelegramm;   */
/*  - knx_meldung(): GroupValueWrite, die spontane Meldung des Aktors        */
/*    (Bewegung 1 beim Losfahren, Pumpenstatus nach dem Schalten).           */
/* Das ist dieselbe Trennung wie in knx_tunnel.py (lesen_alle bzw.           */
/* bewegung_eins/warte_auf_status).                                          */
/*                                                                           */
/* ein_byte: 6/4/12 und 6/4/13 sind DPT 5 (NPDU-Laenge 2), 6/4/14 und        */
/* 6/4/21 DPT 1 (Laenge 1). Ein Wert im falschen Format zaehlt nicht - er    */
/* kaeme nicht von dem Objekt, das hier gemeint ist.                         */
/*****************************************************************************/
inline bool knx_aktorwert(const KnxCemi *r, uint16_t ga, uint16_t aktor, uint16_t apci,
                          bool ein_byte, int *wert)
{
    if (!r || !wert)
        return false;
    if (r->code != KNX_L_DATA_IND || !knx_gruppe(r))
        return false;
    if (r->ziel != ga || r->quelle != aktor || r->apci != apci)
        return false;
    if (ein_byte)
    {
        if (r->laenge != 2)
            return false;
        *wert = r->daten[0];
    }
    else
    {
        if (r->laenge != 1)
            return false;
        *wert = r->klein;
    }
    return true;
}

inline bool knx_leseantwort(const KnxCemi *r, uint16_t ga, uint16_t aktor, bool ein_byte, int *wert)
{
    return knx_aktorwert(r, ga, aktor, KNX_APCI_RESPONSE, ein_byte, wert);
}

inline bool knx_meldung(const KnxCemi *r, uint16_t ga, uint16_t aktor, bool ein_byte, int *wert)
{
    return knx_aktorwert(r, ga, aktor, KNX_APCI_WRITE, ein_byte, wert);
}

/*****************************************************************************/
/* Rueckleseregel A des Mischers (Owner-Entscheid 2026-09-12 abends)         */
/*                                                                           */
/* Warum zwei Belege: Die Bewegungsmeldung allein reicht nicht. Der Aktor    */
/* speichert die Position auch unter Zwangsstellung und faehrt dann nicht    */
/* (Lauf 4) - der zurueckgelesene Eingang belegt nur, dass der Befehl im     */
/* Aktor steht. Bewegung 1, oder der Status am Ziel, wenn der Mischer schon  */
/* dort steht und deshalb nicht faehrt (Lauf 2), belegt, dass er WIRKT.      */
/*                                                                           */
/* Warum die Bewegung VOR den Befehlen: Nach einem Wechsel der Zwangs-       */
/* stellung steht sie bis zu 144 s auf 1. Ginge dann das Zuruecknehmen der   */
/* Zwangsstellung verloren, laese der Schritt Bewegung 1 und Eingang 128     */
/* und meldete GRUEN, waehrend der Mischer in die Endlage faehrt. Die        */
/* schnelle Regel gilt deshalb nur, wenn der Aktor vorher 0 gemeldet hat.    */
/*                                                                           */
/* Beim Einarbeiten festgelegt, jeweils zur sicheren Seite:                  */
/*  - keine Antwort auf die Vorab-Lesung zaehlt wie Bewegung 1 (Rueckfall);  */
/*  - keine Antwort auf das Lesen des Eingangs zaehlt als Abweichung;        */
/*  - die Wiederholung umfasst alle drei Mischertelegramme (vorderhaus.cpp). */
/*                                                                           */
/* Referenz: knx_tunnel.py 1.6.0, _bestaetigung(). Hier ist die Regel in     */
/* zwei reine Funktionen zerlegt, damit der Hosttest jeden Zweig ohne Netz   */
/* durchspielt: knx_mischer_frage() sagt, was als Naechstes zu erheben ist,  */
/* knx_mischer_urteil() urteilt ueber das Erhobene. Die Firmware erhebt nur, */
/* was gefragt wird - und liest damit nie mehr als das Werkzeug.             */
/*****************************************************************************/
#define KNX_NICHT_GEFRAGT (-2) // der Wert wurde (noch) nicht erhoben
#define KNX_KEINE_ANTWORT (-1) // gefragt, der Aktor hat binnen der Frist nicht geantwortet

struct KnxMischerBeleg
{
    int vorher;   // 6/4/14 vor den Befehlen: 0, 1 oder KNX_KEINE_ANTWORT
    int eingang;  // 6/4/13 nach den Befehlen: 0-255 oder KNX_KEINE_ANTWORT
    int spontan;  // spontane Bewegung 1 seit den Befehlen: 1, oder 0 = Frist ohne Meldung
    int bewegung; // 6/4/14 aktiv gelesen
    int status;   // 6/4/12 aktiv gelesen: 0-255 oder KNX_KEINE_ANTWORT
};

inline void knx_mischer_beleg_leeren(KnxMischerBeleg *b)
{
    if (!b)
        return;
    b->vorher = KNX_NICHT_GEFRAGT;
    b->eingang = KNX_NICHT_GEFRAGT;
    b->spontan = KNX_NICHT_GEFRAGT;
    b->bewegung = KNX_NICHT_GEFRAGT;
    b->status = KNX_NICHT_GEFRAGT;
}

// Vor der einen Wiederholung: Alles, was nach den Befehlen erhoben wurde,
// gilt nicht mehr - die Bewegung von VORHER bleibt, sie beschreibt den Stand
// vor dem ersten Befehl und damit die Frage, ob die schnelle Regel gilt.
inline void knx_mischer_neuer_versuch(KnxMischerBeleg *b)
{
    if (!b)
        return;
    const int vorher = b->vorher;
    knx_mischer_beleg_leeren(b);
    b->vorher = vorher;
}

// Die schnelle Regel gilt nur bei einer ausdruecklichen 0 vom Aktor. Auch ein
// nie erhobener Wert zaehlt als "nein": Der Rueckfall ist die sichere Seite.
inline bool knx_mischer_schnell(const KnxMischerBeleg *b)
{
    return b && b->vorher == 0;
}

// Status am Ziel: 128 +/- 2. Werte ausserhalb 0-255 kann ein DPT-5-Objekt nicht
// liefern - sie zaehlen nicht, auch KNX_KEINE_ANTWORT nicht.
inline bool knx_mischer_am_ziel(int status)
{
    if (status < 0 || status > 255)
        return false;
    const int abstand = (status > KNX_MISCHER_POSITION) ? status - KNX_MISCHER_POSITION
                                                        : KNX_MISCHER_POSITION - status;
    return abstand <= KNX_MISCHER_TOLERANZ;
}

enum KnxMischerFrage
{
    KNX_FRAGE_FERTIG = 0,            // genug erhoben, jetzt urteilen
    KNX_FRAGE_EINGANG = 1,           // 6/4/13 lesen
    KNX_FRAGE_BEWEGUNG_ABWARTEN = 2, // bis KNX_FRIST_BEWEGUNG_MS auf die spontane Meldung warten
    KNX_FRAGE_BEWEGUNG_LESEN = 3,    // 6/4/14 lesen
    KNX_FRAGE_STATUS_LESEN = 4       // 6/4/12 lesen
};

inline KnxMischerFrage knx_mischer_frage(const KnxMischerBeleg *b)
{
    if (!b)
        return KNX_FRAGE_FERTIG;
    // 1. Der Eingang ist in BEIDEN Faellen Pflicht
    if (b->eingang == KNX_NICHT_GEFRAGT)
        return KNX_FRAGE_EINGANG;
    if (b->eingang != KNX_MISCHER_POSITION)
        return KNX_FRAGE_FERTIG; // ROT - weiteres Lesen aendert daran nichts
    // 2. Fuhr der Mischer schon (oder schwieg er), entscheidet die Endstellung
    if (!knx_mischer_schnell(b))
        return KNX_FRAGE_FERTIG;
    // 3. Kurz auf die spontane Meldung warten, sonst aktiv nachlesen
    if (b->spontan == KNX_NICHT_GEFRAGT)
        return KNX_FRAGE_BEWEGUNG_ABWARTEN;
    if (b->spontan == 1)
        return KNX_FRAGE_FERTIG;
    if (b->bewegung == KNX_NICHT_GEFRAGT)
        return KNX_FRAGE_BEWEGUNG_LESEN;
    if (b->bewegung == 1)
        return KNX_FRAGE_FERTIG;
    // 4. Keine Bewegung: Steht er schon am Ziel, faehrt er nicht (Lauf 2)
    if (b->status == KNX_NICHT_GEFRAGT)
        return KNX_FRAGE_STATUS_LESEN;
    return KNX_FRAGE_FERTIG;
}

enum KnxMischerUrteil
{
    KNX_MISCHER_ROT_EINGANG = 0,    // Eingang falsch oder stumm - der Befehl steht nicht im Aktor
    KNX_MISCHER_ROT_STEHT = 1,      // keine Bewegung und nicht am Ziel - Zwangsstellung noch aktiv?
    KNX_MISCHER_GRUEN_BEWEGUNG = 2, // faehrt, Eingang passt
    KNX_MISCHER_GRUEN_AM_ZIEL = 3,  // stand schon am Ziel, Eingang passt
    KNX_MISCHER_AUSSTEHEND = 4      // Rueckfall: die Endstellung entscheidet, in eigenen Abfragen
};

inline KnxMischerUrteil knx_mischer_urteil(const KnxMischerBeleg *b)
{
    if (!b || b->eingang != KNX_MISCHER_POSITION)
        return KNX_MISCHER_ROT_EINGANG;
    if (!knx_mischer_schnell(b))
        return KNX_MISCHER_AUSSTEHEND;
    if (b->spontan == 1 || b->bewegung == 1)
        return KNX_MISCHER_GRUEN_BEWEGUNG;
    if (knx_mischer_am_ziel(b->status))
        return KNX_MISCHER_GRUEN_AM_ZIEL;
    return KNX_MISCHER_ROT_STEHT;
}

inline bool knx_mischer_rot(KnxMischerUrteil u)
{
    return u == KNX_MISCHER_ROT_EINGANG || u == KNX_MISCHER_ROT_STEHT;
}

/*****************************************************************************/
/* Das Ergebnis des Schritts                                                 */
/*                                                                           */
/* Die Pumpe wird IMMER geschaltet, auch wenn der Mischer ROT ergab (Owner-  */
/* Entscheid 2026-09-13): Mit laufender Pumpe bekommt das Vorderhaus Waerme  */
/* nach der letzten Mischerstellung der Steuerung, ohne sie gar keine. Das   */
/* Ergebnis bleibt trotzdem ROT - der Mischer steht nicht, wo er soll.       */
/*                                                                           */
/* AUSSTEHEND gibt es nur, wenn alles andere stimmt: Meldet die Pumpe nicht  */
/* ein, ist der Schritt ROT, gleich wo der Mischer ankommt - auf die         */
/* Endstellung zu warten, aenderte daran nichts.                             */
/*****************************************************************************/
enum KnxVorderhaus
{
    KNX_VH_ROT = 0,
    KNX_VH_GRUEN = 1,
    KNX_VH_AUSSTEHEND = 2 // Rueckfall: Abfragen im Takt, bis 128 +/- 2 oder das Schritt-Timeout
};

inline bool knx_pumpe_ein(int status)
{
    return status == 1;
}

inline KnxVorderhaus knx_vorderhaus_ergebnis(bool verbindung_ok, KnxMischerUrteil mischer,
                                             bool pumpe_ein)
{
    if (!verbindung_ok || !pumpe_ein || knx_mischer_rot(mischer))
        return KNX_VH_ROT;
    if (mischer == KNX_MISCHER_AUSSTEHEND)
        return KNX_VH_AUSSTEHEND;
    return KNX_VH_GRUEN;
}

/*****************************************************************************/
/* Die Einstellung knx_schnittstelle lesen (Owner-Entscheid E1)              */
/*                                                                           */
/* Erlaubt ist NUR eine IPv4-Adresse, wahlweise mit ":Port" - kein           */
/* Geraetename: Ob die Namensaufloesung im Notbetriebsfall mit ausfaellt,    */
/* haengt davon ab, wo sie laeuft, und eine Adresse laesst sich streng       */
/* pruefen. Fuehrende und nachlaufende Leerzeichen sind erlaubt (kopierte    */
/* Adressen), sonst nichts.                                                  */
/*                                                                           */
/* Abgelehnt wird auch, was eine Adresse nur scheinbar ist:                  */
/*  - fuehrende Nullen ("192.168.002.127"): inet_aton liest sie oktal, und   */
/*    welche Adresse gemeint war, weiss niemand;                             */
/*  - 0.x.x.x, 127.x.x.x und ab 224.x.x.x: "dieses Netz", das Geraet selbst, */
/*    Multicast und Broadcast. 224.0.23.12 ist die Routing-Adresse - hier    */
/*    haengt eine reine Schnittstelle, und die spricht nur Tunneling.        */
/*                                                                           */
/* Rueckgabe false heisst "nicht eingerichtet": Warnung beim Start, ROT beim */
/* Druck (Owner 2026-09-12 - kein stilles Entfallen des Schritts).           */
/*****************************************************************************/
inline bool knx_zahl_lesen(const char **p, const char *ende, unsigned max_ziffern, unsigned *wert)
{
    const char *q = *p;
    const char *anfang = q;
    unsigned w = 0;
    unsigned ziffern = 0;
    while (q < ende && *q >= '0' && *q <= '9')
    {
        if (++ziffern > max_ziffern)
            return false;
        w = w * 10u + (unsigned)(*q - '0');
        q++;
    }
    if (ziffern == 0 || (ziffern > 1 && *anfang == '0'))
        return false;
    *p = q;
    *wert = w;
    return true;
}

inline bool knx_schnittstelle_lesen(const char *text, uint8_t ip_out[4], uint16_t *port_out)
{
    if (!text || !ip_out || !port_out)
        return false;

    // Rand abschneiden - nur Leerraum, sonst nichts
    const char *p = text;
    while (*p == ' ' || *p == '\t')
        p++;
    const char *ende = p + strlen(p);
    while (ende > p && (ende[-1] == ' ' || ende[-1] == '\t' || ende[-1] == '\r' || ende[-1] == '\n'))
        ende--;

    // vier Oktette, durch Punkte getrennt
    uint8_t ip[4];
    for (int i = 0; i < 4; i++)
    {
        if (i > 0)
        {
            if (p >= ende || *p != '.')
                return false;
            p++;
        }
        unsigned oktett = 0;
        if (!knx_zahl_lesen(&p, ende, 3, &oktett) || oktett > 255)
            return false;
        ip[i] = (uint8_t)oktett;
    }

    // wahlweise ":Port"
    unsigned port = KNX_PORT_STANDARD;
    if (p < ende)
    {
        if (*p != ':')
            return false;
        p++;
        if (!knx_zahl_lesen(&p, ende, 5, &port) || port < 1 || port > 65535)
            return false;
    }
    if (p != ende)
        return false; // Rest wie "x", ":3671:1" oder ein zweiter Doppelpunkt

    if (ip[0] == 0 || ip[0] == 127 || ip[0] >= 224)
        return false;

    memcpy(ip_out, ip, 4);
    *port_out = (uint16_t)port;
    return true;
}

/*****************************************************************************/
/* Zeitrechnung                                                              */
/*                                                                           */
/* Beide rechnen ueber die Differenz in uint32_t und ueberstehen damit den   */
/* millis()-Ueberlauf nach 49,7 Tagen - wie sendwindow.h und notbetrieb.h.   */
/* uint32_t und nicht unsigned long: auf dem Mac waere das 64 Bit, und der   */
/* Hosttest pruefte den Ueberlauf gegen nichts.                              */
/*****************************************************************************/

// Restzeit einer Frist; 0 heisst abgelaufen. Jede Wartezeit innerhalb einer
// Verbindung ist min(eigene Frist, Restzeit des Deckels).
inline uint32_t knx_restzeit(uint32_t start, uint32_t jetzt, uint32_t dauer)
{
    const uint32_t vergangen = jetzt - start;
    return (vergangen >= dauer) ? 0u : dauer - vergangen;
}

// Rueckfall: ist die naechste Abfrage des Mischerstatus faellig?
inline bool knx_abfrage_faellig(uint32_t letzte, uint32_t jetzt)
{
    return (uint32_t)(jetzt - letzte) >= KNX_RUECKFALL_TAKT_MS;
}

/*****************************************************************************/
/* Adressen als Text - fuer die Logzeilen                                    */
/*****************************************************************************/
inline void knx_ga_text(uint16_t ga, char *buf, size_t len)
{
    if (!buf || len == 0)
        return;
    (void)snprintf(buf, len, "%u/%u/%u", (unsigned)((ga >> 11) & 0x1F), (unsigned)((ga >> 8) & 0x07),
                   (unsigned)(ga & 0xFF));
}

inline void knx_pa_text(uint16_t pa, char *buf, size_t len)
{
    if (!buf || len == 0)
        return;
    (void)snprintf(buf, len, "%u.%u.%u", (unsigned)((pa >> 12) & 0x0F), (unsigned)((pa >> 8) & 0x0F),
                   (unsigned)(pa & 0xFF));
}
