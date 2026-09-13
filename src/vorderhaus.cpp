#include "HeishaMon.h"
#include "knxtunnel.h"

/*****************************************************************************/
/* Der Notbetriebsschritt "Vorderhaus" - der Netzteil (seit 3.21.0)          */
/*                                                                           */
/* Die Regeln stehen arduino-frei in knxtunnel.h und laufen dort durch den   */
/* Hosttest: Rahmen, Sequenzregel, welche Antwort zaehlt, das Urteil der     */
/* Rueckleseregel A. Hier steht nur, was ohne Netz nicht geht: der Socket,   */
/* das Warten, das Quittieren und die Logzeilen. Der Automat in notbetrieb.h */
/* sieht davon nur das Ergebnis.                                             */
/*                                                                           */
/* Referenz ist test/knx_tunnel.py 1.6.0, 'mischer --bewegung'; die Namen    */
/* unten folgen dem Werkzeug, wo es geht (verbinden, lesen, trennen).        */
/*                                                                           */
/* NUR KURZE VERBINDUNGEN (Analyse-KNX-Vorderhaus.md, Abschnitt 8). Jede     */
/* Verbindung ist ein geschlossener Austausch innerhalb EINES Aufrufs; kein  */
/* Tunnel bleibt ueber mehrere loop()-Durchlaeufe offen. Ein offener Tunnel  */
/* muss jedes Bustelegramm binnen 1 s quittieren, die Schnittstelle          */
/* wiederholt einmal und trennt dann - und die MQTT-Wiederverbindung         */
/* blockiert loop() im Notbetriebsfall bis zu 2 s, wiederholt. Ein Tunnel,   */
/* der ueber sie hinweg offen bliebe, verpasste die Frist fast sicher.       */
/*****************************************************************************/

/*****************************************************************************/
/* WARUM DER AUSTAUSCH loop() BLOCKIEREN DARF - und wie lange hoechstens     */
/*                                                                           */
/* Dieselbe Abwaegung wie beim Hydraulikschritt (notbetrieb.cpp,             */
/* Ablauf-Notbetrieb.md Abschnitt 1a): Solange der Austausch laeuft, stehen  */
/* read_pana_data, timeout_serial, der Webserver und MQTT. Der UART-Puffer   */
/* fasst 256 Byte, ein Telegramm hat 203 - verloren geht hoechstens eine     */
/* Leserunde, kein Kommando (test/README.md, dasselbe am 2026-08-27).        */
/*                                                                           */
/* Der Schritt steht ZULETZT, hinter Heatpump = 1, und das ist zurueck-      */
/* gelesen: Kein Kommando an die Waermepumpe ist unterwegs, kein Sammel-     */
/* fenster offen. Eine ausfallende Leserunde verschiebt nur den Abfrage-     */
/* zyklus einer Anlage, die schon laeuft.                                    */
/*                                                                           */
/* Die Obergrenzen, aus den Fristen in knxtunnel.h:                          */
/*  - Befehlsverbindung: KNX_BEFEHL_DECKEL_MS + KNX_FRIST_TRENNEN_MS         */
/*    = 20 s + 0,5 s = 20,5 s. Erreicht nur, wenn die Schnittstelle          */
/*    quittiert, aber kein Aktor antwortet, jede Wartezeit auslaeuft (16 s   */
/*    Fenster) und dazu Quittungen wiederholt werden muessen. Der Fall endet */
/*    ohnehin in ROT.                                                        */
/*  - Regelfall: rund 0,3 s - ein Dutzend Telegramme mit je 2 ms Quittung,   */
/*    fuenf Antworten zu je 55-93 ms, die Pumpe meldet nach 55-120 ms. Laeuft */
/*    die Pumpe schon, meldet sie nichts: dann 1 s mehr, danach wird gelesen.*/
/*  - Zweiter Druck, Mischer steht schon auf 128: die 2 s Bewegungsfrist     */
/*    laufen aus, dann Bewegung und Status lesen - rund 2,5 s.               */
/*  - Schnittstelle stumm: 1 s CONNECT, dann ROT.                            */
/*  - Abfrage im Rueckfall: KNX_ABFRAGE_DECKEL_MS + 0,5 s = 4,5 s, im        */
/*    Regelfall rund 0,1 s, alle 10 s.                                       */
/*                                                                           */
/* Nebenwirkung im Test mit lebendem Broker: Eine Blockade laenger als das   */
/* MQTT-Keepalive kann die Verbindung kosten; loop() verbindet danach neu.   */
/* Im eigentlichen Notbetriebsfall ist der Broker ohnehin weg.               */
/*                                                                           */
/* Eine asynchrone Loesung (eigener Task) ist verworfen, aus demselben Grund */
/* wie beim Hydraulikschritt: Sie brachte Nebenlaeufigkeit in einen Automaten*/
/* der vollstaendig aus loop() getrieben wird, fuer einen Fehlerfall, der    */
/* ohnehin in ROT endet.                                                     */
/*****************************************************************************/

/*****************************************************************************/
/* Der lokale Port - FEST, nicht vom System vergeben                         */
/*                                                                           */
/* Das CONNECT_REQUEST nennt der Schnittstelle, wohin sie antworten soll     */
/* (HPAI), also braucht die Firmware ihren eigenen Port. WiFiUDP bindet mit  */
/* Port 0 zwar einen freien, gibt ihn aber nicht heraus (NetworkUdp.cpp,     */
/* pioarduino 55.3.311: server_port bleibt 0, kein getsockname). Ein fester  */
/* Port ist die einfachere Loesung: Jede Verbindung oeffnet und schliesst    */
/* ihren Socket selbst, und ein verspaetetes Datagramm einer vorigen         */
/* Verbindung traegt deren Kanal und wird verworfen.                         */
/*                                                                           */
/* 3672 und nicht 3671: So ist in jedem Mitschnitt auf einen Blick zu        */
/* sehen, welche Seite die Schnittstelle ist.                                */
/*****************************************************************************/
#define VORDERHAUS_LOKALER_PORT 3672

/*****************************************************************************/
/* Empfangspuffer                                                            */
/*                                                                           */
/* Die eigenen Telegramme sind hoechstens 22 Byte lang, aber die Schnitt-    */
/* stelle reicht JEDES Bustelegramm an jeden Tunnel weiter, und jedes muss   */
/* quittiert werden - sonst wiederholt sie und trennt. Ein erweiterter       */
/* Rahmen, der nicht in den Puffer passte, fiele beim Laengenfeld durch und  */
/* bliebe unquittiert. 320 Byte decken jeden Tunnelrahmen ab, den ein        */
/* KNX-Bus liefern kann; auf dem Stack von loop() ist das unkritisch.        */
/*****************************************************************************/
#define VORDERHAUS_EMPFANG_MAX 320

/*****************************************************************************/
/* Der Zustand einer Verbindung                                              */
/*                                                                           */
/* Ein einziger Verteiler (verteile()) nimmt alles an, was hereinkommt, und  */
/* traegt es hier ein - auch waehrend auf etwas ganz anderes gewartet wird.  */
/* Zwei Beobachtungen laufen dabei staendig mit, weil die Meldung eintreffen */
/* kann, BEVOR auf sie gewartet wird:                                        */
/*  - bewegung_eins: Der Mischeraktor meldet "faehrt" rund 0,1 s nach dem    */
/*    Positionsbefehl, also womoeglich waehrend das Lesen des Eingangs noch  */
/*    laeuft. Das Werkzeug zaehlt deshalb alles seit der Marke vor den       */
/*    Befehlen - hier wird das Flag vor den Befehlen geloescht.              */
/*  - pumpe_meldung: der spontane Status nach dem Schaltbefehl.              */
/*****************************************************************************/
struct VorderhausTunnel
{
  WiFiUDP udp;
  IPAddress ziel_ip;   // die Schnittstelle, Steuerendpunkt
  uint16_t ziel_port;
  IPAddress daten_ip;  // Datenendpunkt - aus der CONNECT_RESPONSE, nie angenommen
  uint16_t daten_port;
  uint8_t lokal_ip[4];
  uint8_t kanal;
  uint8_t tx_seq;
  uint8_t rx_seq;
  bool verbunden;
  bool gestoert;             // Quittung fehlte, Deckel erreicht, Gegenstelle trennte
  bool gegenstelle_getrennt; // die Schnittstelle hat von sich aus getrennt
  uint32_t start;            // Beginn der Verbindung - fuer den Deckel
  uint32_t deckel;

  // Was gerade erwartet wird; der Verteiler traegt Treffer ein
  bool connect_da;
  KnxConnectAntwort connect;
  bool trennen_da;
  bool ack_da;
  uint8_t ack_seq;
  uint8_t ack_status;
  uint16_t lese_ga;    // 0 = keine Lesefrage offen (0/0/0 ist die Broadcast-Adresse)
  uint16_t lese_aktor;
  bool lese_ein_byte;
  int lese_wert;       // KNX_NICHT_GEFRAGT, solange keine passende Antwort da ist

  // Beobachtungen, die jede Wartezeit begleiten
  bool bewegung_eins;
  uint32_t bewegung_zeit;
  int pumpe_meldung;

  // Zaehler fuer die Logzeile
  unsigned fremde_antworten; // Antworten anderer Quellen auf eigene Lesefragen (openknx)
  unsigned bustelegramme;    // mitgelesen und quittiert
  unsigned cons;             // L_Data.con - mit Quelle 1.1.250 kommen keine
};

static VorderhausTunnel tunnel;

// Der erste Grund, aus dem eine Verbindung gestoert ist - fuer die Logzeile.
// Nur der erste zaehlt: Alles danach ist Folge.
static char vorderhausStoerung[72] = "";

static void stoerung(VorderhausTunnel &t, const char *text)
{
  if (!t.gestoert)
    (void)snprintf(vorderhausStoerung, sizeof(vorderhausStoerung), "%s", text);
  t.gestoert = true;
}

static void tunnel_leeren(VorderhausTunnel &t)
{
  t.kanal = 0;
  t.tx_seq = 0;
  t.rx_seq = 0;
  t.verbunden = false;
  t.gestoert = false;
  t.gegenstelle_getrennt = false;
  t.start = 0;
  t.deckel = 0;
  t.connect_da = false;
  memset(&t.connect, 0, sizeof(t.connect));
  t.trennen_da = false;
  t.ack_da = false;
  t.ack_seq = 0;
  t.ack_status = 0;
  t.lese_ga = 0;
  t.lese_aktor = 0;
  t.lese_ein_byte = false;
  t.lese_wert = KNX_NICHT_GEFRAGT;
  t.bewegung_eins = false;
  t.bewegung_zeit = 0;
  t.pumpe_meldung = KNX_NICHT_GEFRAGT;
  t.fremde_antworten = 0;
  t.bustelegramme = 0;
  t.cons = 0;
  vorderhausStoerung[0] = '\0';
}

/*****************************************************************************/
/* Frist innerhalb des Deckels                                               */
/*                                                                           */
/* Jede Wartezeit ist min(eigene Frist, Rest des Deckels). Ist der Deckel    */
/* erreicht, ist die Verbindung gestoert - siehe knxtunnel.h, warum es einen */
/* harten Deckel statt einer Summenrechnung gibt.                            */
/*****************************************************************************/
static uint32_t frist_im_deckel(VorderhausTunnel &t, uint32_t frist)
{
  const uint32_t rest = knx_restzeit(t.start, millis(), t.deckel);
  if (rest == 0)
  {
    stoerung(t, "Deckel der Verbindung erreicht");
    return 0;
  }
  return (rest < frist) ? rest : frist;
}

static bool sende(VorderhausTunnel &t, const IPAddress &ip, uint16_t port, const uint8_t *buf, size_t n)
{
  if (n == 0)
    return false; // der Rahmen passte nicht - knxtunnel.h liefert dann 0
  if (!t.udp.beginPacket(ip, port))
    return false;
  (void)t.udp.write(buf, n);
  return t.udp.endPacket() == 1;
}

/*****************************************************************************/
/* Ein eingehendes TUNNELING_REQUEST: quittieren, dann auswerten             */
/*                                                                           */
/* Die Quittung geht VOR jeder Auswertung raus und haengt allein an der      */
/* Sequenzregel - auch fuer Telegramme, die niemand hier braucht, und fuer   */
/* solche, deren cEMI nicht zu lesen ist. Fremde Kanaele bleiben ohne        */
/* Quittung (knx_sequenz_regel()).                                           */
/*****************************************************************************/
static void tunnel_empfangen(VorderhausTunnel &t, const uint8_t *rumpf, size_t rumpf_len)
{
  uint8_t kanal = 0, seq = 0;
  const uint8_t *cemi = nullptr;
  size_t cemi_len = 0;
  if (!t.verbunden || !knx_zerlege_tunneling_request(rumpf, rumpf_len, &kanal, &seq, &cemi, &cemi_len))
    return;

  const KnxSequenz regel = knx_sequenz_regel(t.kanal, t.rx_seq, kanal, seq);
  if (regel == KNX_SEQ_VERWERFEN)
    return;
  uint8_t ack[10];
  (void)sende(t, t.daten_ip, t.daten_port, ack, knx_baue_tunneling_ack(ack, sizeof(ack), t.kanal, seq, 0));
  if (regel == KNX_SEQ_NUR_QUITTIEREN)
    return; // Wiederholung - schon verarbeitet
  t.rx_seq = (uint8_t)(t.rx_seq + 1);

  KnxCemi r;
  if (!knx_zerlege_cemi(cemi, cemi_len, &r))
    return;
  if (r.code == KNX_L_DATA_CON)
  {
    t.cons++; // entscheidet nichts (knx_bestaetigt_ok() in knxtunnel.h)
    return;
  }
  if (r.code != KNX_L_DATA_IND)
    return;
  t.bustelegramme++;

  int wert = 0;
  // Die offene Lesefrage - nur der genannte Aktor zaehlt. Eine Antwort von
  // anderswo (openknx aus seinem Zwischenspeicher) wird mitgezaehlt, damit das
  // Log zeigt, dass der Filter gegriffen hat.
  if (t.lese_ga != 0 && t.lese_wert == KNX_NICHT_GEFRAGT)
  {
    if (knx_leseantwort(&r, t.lese_ga, t.lese_aktor, t.lese_ein_byte, &wert))
      t.lese_wert = wert;
    else if (knx_gruppe(&r) && r.ziel == t.lese_ga && r.apci == KNX_APCI_RESPONSE)
      t.fremde_antworten++;
  }

  // Die beiden Beobachtungen (siehe VorderhausTunnel)
  if (!t.bewegung_eins && knx_meldung(&r, KNX_GA_BEWEGUNG, KNX_AKTOR_MISCHER, false, &wert) && wert == 1)
  {
    t.bewegung_eins = true;
    t.bewegung_zeit = millis();
  }
  if (knx_meldung(&r, KNX_GA_PUMPE_STATUS, KNX_AKTOR_PUMPE, false, &wert))
    t.pumpe_meldung = wert;
}

/*****************************************************************************/
/* Der Verteiler - alles, was von der Schnittstelle kommt                    */
/*****************************************************************************/
static void verteile(VorderhausTunnel &t, const uint8_t *dg, size_t len, uint16_t absender_port)
{
  uint16_t dienst = 0;
  const uint8_t *rumpf = nullptr;
  size_t rumpf_len = 0;
  if (!knx_zerlege_kopf(dg, len, &dienst, &rumpf, &rumpf_len))
    return; // unlesbar - verwerfen, nichts abbrechen

  switch (dienst)
  {
  case KNX_CONNECT_RESPONSE:
    t.connect_da = knx_zerlege_connect_response(rumpf, rumpf_len, &t.connect);
    break;

  case KNX_DISCONNECT_RESPONSE:
    t.trennen_da = true;
    break;

  case KNX_TUNNELING_ACK:
  {
    uint8_t kanal = 0, seq = 0, status = 0;
    if (t.verbunden && knx_zerlege_tunneling_ack(rumpf, rumpf_len, &kanal, &seq, &status) && kanal == t.kanal)
    {
      t.ack_da = true;
      t.ack_seq = seq;
      t.ack_status = status;
    }
    break;
  }

  // Die Schnittstelle trennt von sich aus, wenn eine Quittung zweimal
  // ausblieb. Antworten und aufhoeren - weiterzusenden hiesse, in einen Kanal
  // zu schreiben, den es nicht mehr gibt.
  case KNX_DISCONNECT_REQUEST:
  {
    uint8_t kanal = 0;
    if (t.verbunden && knx_zerlege_disconnect_request(rumpf, rumpf_len, &kanal) && kanal == t.kanal)
    {
      uint8_t antwort[8];
      (void)sende(t, t.ziel_ip, absender_port, antwort,
                  knx_baue_disconnect_response(antwort, sizeof(antwort), t.kanal, 0));
      t.gegenstelle_getrennt = true;
      stoerung(t, "die Schnittstelle hat die Verbindung beendet");
    }
    break;
  }

  case KNX_TUNNELING_REQUEST:
    tunnel_empfangen(t, rumpf, rumpf_len);
    break;

  default:
    break; // Dienste, die dieser Client nicht benutzt
  }
}

/*****************************************************************************/
/* Warten - und dabei alles annehmen und quittieren                          */
/*                                                                           */
/* Kein delay() ueber die ganze Frist: Auch waehrend des Wartens muss jedes  */
/* Bustelegramm binnen 1 s quittiert werden. delay(1) je leerem Durchgang    */
/* gibt dem WLAN-Stack die Gelegenheit, die er braucht.                      */
/*                                                                           */
/* Nach JEDEM Datagramm clear(): parsePacket() liefert 0, solange vom        */
/* vorigen Datagramm noch Bytes im Empfangspuffer liegen (NetworkUdp.cpp,    */
/* "if (rx_buffer) return 0"). Ohne clear() verstummte der Empfang nach dem  */
/* ersten Datagramm, das groesser als der Puffer war - still, und die        */
/* Schnittstelle trennte mangels Quittung. clear() und nicht flush(): Im     */
/* Core 3.3 ist flush() als veraltet markiert und meint nach Print das       */
/* Senden; clear() leert ausdruecklich den Empfang.                          */
/*                                                                           */
/* Nur Datagramme der Schnittstelle zaehlen (Steuer- oder Datenendpunkt).    */
/* Die Zeit rechnet ueber die Differenz und uebersteht den millis()-         */
/* Ueberlauf.                                                                */
/*****************************************************************************/
enum VorderhausWarten
{
  VH_WARTE_CONNECT,
  VH_WARTE_TRENNEN,
  VH_WARTE_ACK,
  VH_WARTE_LESEN,
  VH_WARTE_BEWEGUNG,
  VH_WARTE_PUMPE
};

static bool erfuellt(const VorderhausTunnel &t, VorderhausWarten was, uint8_t seq)
{
  switch (was)
  {
  case VH_WARTE_CONNECT:
    return t.connect_da;
  case VH_WARTE_TRENNEN:
    return t.trennen_da;
  case VH_WARTE_ACK:
    return t.ack_da && t.ack_seq == seq;
  case VH_WARTE_LESEN:
    return t.lese_wert != KNX_NICHT_GEFRAGT;
  case VH_WARTE_BEWEGUNG:
    return t.bewegung_eins;
  case VH_WARTE_PUMPE:
    return knx_pumpe_ein(t.pumpe_meldung);
  }
  return false;
}

static bool warte(VorderhausTunnel &t, VorderhausWarten was, uint32_t frist_ms, uint8_t seq = 0)
{
  const uint32_t beginn = millis();
  uint8_t dg[VORDERHAUS_EMPFANG_MAX];
  for (;;)
  {
    if (erfuellt(t, was, seq))
      return true;
    // Hat die Schnittstelle getrennt, kommt nichts mehr - ausser beim Trennen
    // selbst, das ohnehin gleich endet.
    if (t.gegenstelle_getrennt && was != VH_WARTE_TRENNEN)
      return false;
    if ((uint32_t)(millis() - beginn) >= frist_ms)
      return false;

    const int n = t.udp.parsePacket();
    if (n <= 0)
    {
      delay(1);
      continue;
    }
    const int gelesen = t.udp.read(dg, sizeof(dg));
    const IPAddress absender = t.udp.remoteIP();
    const uint16_t absender_port = t.udp.remotePort();
    t.udp.clear(); // Rest verwerfen - siehe oben
    if (gelesen <= 0)
      continue;
    if (absender != t.ziel_ip && !(t.verbunden && absender == t.daten_ip))
      continue; // nicht von der Schnittstelle
    verteile(t, dg, (size_t)gelesen, absender_port);
  }
}

/*****************************************************************************/
/* Ein Gruppentelegramm senden - mit Quittung und genau einer Wiederholung   */
/*                                                                           */
/* Frist und Wiederholung nach Spezifikation. Bleibt auch die zweite         */
/* Quittung aus, ist die Verbindung gestoert: Der Aufrufer bricht ab,        */
/* trennt und meldet ROT. Auf eine L_Data.con wird NICHT gewartet - mit      */
/* Quelle 1.1.250 kommt keine, und das Warten kostete je Telegramm die ganze */
/* Frist (Analyse, Abschnitt 8). Das Urteil faellt die Ruecklesung.          */
/*****************************************************************************/
static bool telegramm(VorderhausTunnel &t, uint16_t ga, uint16_t apci, uint8_t klein, const uint8_t *daten,
                      size_t daten_len)
{
  if (!t.verbunden || t.gestoert)
    return false;

  uint8_t dg[32];
  const size_t n = knx_baue_gruppentelegramm(dg, sizeof(dg), t.kanal, t.tx_seq, KNX_QUELLE, ga, apci, klein,
                                             daten, daten_len);
  if (n == 0)
  {
    stoerung(t, "Telegramm nicht zu bauen");
    return false;
  }

  for (unsigned versuch = 1; versuch <= 2; versuch++)
  {
    const uint32_t frist = frist_im_deckel(t, KNX_FRIST_ACK_MS);
    if (frist == 0)
      return false; // Deckel erreicht, stoerung() ist schon gesetzt
    t.ack_da = false;
    // Scheitert schon das Senden (WLAN-Aussetzer), zaehlt das wie eine
    // fehlende Quittung - die Wiederholung ist dieselbe.
    if (sende(t, t.daten_ip, t.daten_port, dg, n) && warte(t, VH_WARTE_ACK, frist, t.tx_seq))
    {
      if (t.ack_status != 0)
      {
        char text[48];
        (void)snprintf(text, sizeof(text), "Quittung mit Fehler 0x%02X", t.ack_status);
        stoerung(t, text);
        return false;
      }
      t.tx_seq = (uint8_t)(t.tx_seq + 1);
      return true;
    }
    if (t.gegenstelle_getrennt)
      return false;
  }

  char ga_text[12], text[64];
  knx_ga_text(ga, ga_text, sizeof(ga_text));
  (void)snprintf(text, sizeof(text), "keine Quittung auf %s nach der Wiederholung", ga_text);
  stoerung(t, text);
  return false;
}

/*****************************************************************************/
/* Ein Objekt lesen - es zaehlt nur die Antwort des genannten Aktors         */
/*                                                                           */
/* Die Lesefrage wird VOR dem Senden eingetragen: Die Antwort des Aktors     */
/* darf auch vor der Quittung eintreffen und wird trotzdem gefunden. Das     */
/* Warten endet an der Antwort, nicht am Fenster.                            */
/*                                                                           */
/* Rueckgabe: der Wert, oder KNX_KEINE_ANTWORT.                              */
/*****************************************************************************/
static int lesen(VorderhausTunnel &t, uint16_t ga, uint16_t aktor, bool ein_byte)
{
  t.lese_ga = ga;
  t.lese_aktor = aktor;
  t.lese_ein_byte = ein_byte;
  t.lese_wert = KNX_NICHT_GEFRAGT;

  if (telegramm(t, ga, KNX_APCI_READ, 0, nullptr, 0))
    (void)warte(t, VH_WARTE_LESEN, frist_im_deckel(t, KNX_FRIST_LESEN_MS));

  const int wert = (t.lese_wert == KNX_NICHT_GEFRAGT) ? KNX_KEINE_ANTWORT : t.lese_wert;
  t.lese_ga = 0; // keine offene Frage mehr
  return wert;
}

/*****************************************************************************/
/* Verbinden                                                                 */
/*                                                                           */
/* Kanal und Datenendpunkt kommen AUS DER ANTWORT - die Schnittstelle        */
/* vergibt beide selbst (Analyse, Stufe 0). Der Deckel beginnt hier.         */
/*****************************************************************************/
static bool verbinden(VorderhausTunnel &t, const uint8_t ip[4], uint16_t port, uint32_t deckel)
{
  tunnel_leeren(t);
  t.ziel_ip = IPAddress(ip[0], ip[1], ip[2], ip[3]);
  t.ziel_port = port;
  t.start = millis();
  t.deckel = deckel;

  const IPAddress lokal = WiFi.localIP();
  for (int i = 0; i < 4; i++)
    t.lokal_ip[i] = lokal[i];

  if (!t.udp.begin(lokal, VORDERHAUS_LOKALER_PORT))
  {
    stoerung(t, "UDP-Socket nicht zu oeffnen");
    return false;
  }

  uint8_t dg[32];
  const size_t n = knx_baue_connect_request(dg, sizeof(dg), t.lokal_ip, VORDERHAUS_LOKALER_PORT, t.lokal_ip,
                                            VORDERHAUS_LOKALER_PORT);
  if (!sende(t, t.ziel_ip, t.ziel_port, dg, n) ||
      !warte(t, VH_WARTE_CONNECT, frist_im_deckel(t, KNX_FRIST_CONNECT_MS)))
  {
    stoerung(t, "keine Antwort auf CONNECT");
    t.udp.stop();
    return false;
  }
  if (t.connect.status != 0)
  {
    // "alle Tunnel belegt" ist ein anderer Fall als "stumm" - openknx haelt
    // einen, eine offene ETS einen weiteren. Der Code steht im Log.
    char text[64];
    (void)snprintf(text, sizeof(text), "Verbindung abgelehnt, Code 0x%02X%s", t.connect.status,
                   t.connect.status == KNX_E_NO_MORE_CONNECTIONS ? " (alle Tunnel belegt)" : "");
    stoerung(t, text);
    t.udp.stop();
    return false;
  }

  t.kanal = t.connect.kanal;
  if (knx_datenendpunkt_ist_steuer(&t.connect))
  {
    t.daten_ip = t.ziel_ip;
    t.daten_port = t.ziel_port;
  }
  else
  {
    t.daten_ip = IPAddress(t.connect.daten_ip[0], t.connect.daten_ip[1], t.connect.daten_ip[2],
                           t.connect.daten_ip[3]);
    t.daten_port = t.connect.daten_port;
  }
  t.verbunden = true;
  return true;
}

/*****************************************************************************/
/* Trennen - immer, auch im Fehlerfall                                       */
/*                                                                           */
/* Ausserhalb des Deckels, mit eigener kurzer Frist: Den Tunnel freizugeben  */
/* ist auch dann richtig, wenn der Deckel erreicht ist. Bleibt die Antwort   */
/* aus, raeumt die Schnittstelle nach 120 s selbst ab.                       */
/*****************************************************************************/
static void trennen(VorderhausTunnel &t)
{
  if (t.verbunden && !t.gegenstelle_getrennt)
  {
    uint8_t dg[16];
    t.trennen_da = false;
    if (sende(t, t.ziel_ip, t.ziel_port, dg,
              knx_baue_disconnect_request(dg, sizeof(dg), t.kanal, t.lokal_ip, VORDERHAUS_LOKALER_PORT)))
      (void)warte(t, VH_WARTE_TRENNEN, KNX_FRIST_TRENNEN_MS);
  }
  t.verbunden = false;
  t.udp.stop();
}

/*****************************************************************************/
/* Die Einstellung lesen                                                     */
/*                                                                           */
/* Bei jedem Aufruf neu, nicht einmal beim Start: Wer die Adresse auf der    */
/* Einstellungsseite korrigiert, soll ohne Neustart weiterdruecken koennen.  */
/* Das Lesen ist ein Durchgang ueber hoechstens 40 Zeichen.                  */
/*****************************************************************************/
bool vorderhaus_eingerichtet(void)
{
  uint8_t ip[4];
  uint16_t port;
  return knx_schnittstelle_lesen(knx_schnittstelle, ip, &port);
}

// Kurzform des Mischerurteils fuer die Logzeilen
static const char *mischer_text(KnxMischerUrteil u)
{
  switch (u)
  {
  case KNX_MISCHER_GRUEN_BEWEGUNG:
    return "faehrt auf 128";
  case KNX_MISCHER_GRUEN_AM_ZIEL:
    return "stand schon auf 128";
  case KNX_MISCHER_AUSSTEHEND:
    return "fuhr schon oder schwieg - Endstellung entscheidet";
  case KNX_MISCHER_ROT_STEHT:
    return "faehrt nicht und steht nicht auf 128";
  case KNX_MISCHER_ROT_EINGANG:
    return "Positionsbefehl steht nicht im Aktor";
  }
  return "?";
}

/*****************************************************************************/
/* Der Schritt: eine Befehlsverbindung nach Rueckleseregel A                 */
/*                                                                           */
/* Ablauf (Arbeitsplan-KNX-Vorderhaus.md, "Regel A als Ablauf"):             */
/*  1. verbinden                                                             */
/*  2. Bewegung VOR den Befehlen lesen                                       */
/*  3. AUF 0, ZU 0, Position 128 - dann erheben, was knx_mischer_frage()     */
/*     verlangt; bei ROT genau EINE Wiederholung aller drei Telegramme       */
/*  4. Pumpe ein, Ruecklesung (spontan, sonst lesen), eine Wiederholung -    */
/*     IMMER, auch nach ROT des Mischers (Owner 2026-09-13)                  */
/*  5. trennen                                                               */
/*                                                                           */
/* Rueckgabe GRUEN, ROT oder AUSSTEHEND (Rueckfall: der Automat fragt dann   */
/* im Takt vorderhaus_abfragen()).                                           */
/*                                                                           */
/* Ins MQTT-Log gehen ZWEI Zeilen, nicht mehr: Der Logring fasst 32, und ein */
/* Heizen-Lauf belegt schon rund 15. Die Einzelheiten stehen im Telnet-Log.  */
/*****************************************************************************/
KnxVorderhaus vorderhaus_absetzen(void)
{
  char log_line[128];
  const uint32_t beginn = millis();

  // Ohne gueltige Einstellung kein Versuch - ROT, kein stilles Entfallen
  // (Owner 2026-09-12). Der Feldinhalt steht mit im Log: Ein Tippfehler ist
  // der naheliegende Grund, und so ist er ohne die Einstellungsseite zu sehen.
  uint8_t ip[4];
  uint16_t port;
  if (!knx_schnittstelle_lesen(knx_schnittstelle, ip, &port))
  {
    (void)snprintf(log_line, sizeof(log_line), "Notbetrieb: Vorderhaus - keine gueltige KNX-Adresse (\"%.40s\")",
                   knx_schnittstelle);
    write_mqtt_log(log_line);
    return KNX_VH_ROT;
  }
  if (WiFi.status() != WL_CONNECTED)
  {
    write_mqtt_log((char *)"Notbetrieb: Vorderhaus - kein WLAN");
    return KNX_VH_ROT;
  }

  // 1. verbinden
  if (!verbinden(tunnel, ip, port, KNX_BEFEHL_DECKEL_MS))
  {
    (void)snprintf(log_line, sizeof(log_line), "Notbetrieb: Vorderhaus - KNX %u.%u.%u.%u: %s", ip[0], ip[1],
                   ip[2], ip[3], vorderhausStoerung);
    write_mqtt_log(log_line);
    return KNX_VH_ROT;
  }
  (void)snprintf(log_line, sizeof(log_line), "Vorderhaus: verbunden, Kanal %u", tunnel.kanal);
  write_telnet_log(log_line);

  // 2. Bewegung vor den Befehlen - entscheidet, ob die schnelle Regel gilt
  KnxMischerBeleg beleg;
  knx_mischer_beleg_leeren(&beleg);
  beleg.vorher = lesen(tunnel, KNX_GA_BEWEGUNG, KNX_AKTOR_MISCHER, false);

  // 3. Mischer, mit genau einer Wiederholung
  KnxMischerUrteil mischer = KNX_MISCHER_ROT_EINGANG;
  unsigned versuche = 0;
  int32_t bewegung_nach_ms = -1; // fuers Log: Laufzeit der spontanen Meldung
  for (unsigned versuch = 1; versuch <= 2 && !tunnel.gestoert; versuch++)
  {
    versuche = versuch;
    knx_mischer_neuer_versuch(&beleg);
    tunnel.bewegung_eins = false; // nur Meldungen SEIT diesen Befehlen zaehlen

    // Die Zwangsstellungen zuerst - sie gehen vor, ein Positionsbefehl allein
    // bliebe wirkungslos. Alle drei wiederholen, wenn es sein muss: Welches
    // fehlte, ist nicht zu erkennen, und derselbe Wert noch einmal schadet nicht.
    const uint8_t position = KNX_MISCHER_POSITION;
    if (!telegramm(tunnel, KNX_GA_ZWANG_AUF, KNX_APCI_WRITE, 0, nullptr, 0) ||
        !telegramm(tunnel, KNX_GA_ZWANG_ZU, KNX_APCI_WRITE, 0, nullptr, 0) ||
        !telegramm(tunnel, KNX_GA_POS_EINGANG, KNX_APCI_WRITE, 0, &position, 1))
      break; // Verbindung gestoert
    const uint32_t positionsbefehl = millis();

    // Erheben, was die Regel verlangt - hoechstens vier Fragen, die Schleife
    // hat doppelte Reserve und ist nur die Absicherung gegen einen Fehler in
    // knx_mischer_frage(), der sonst loop() festhielte.
    for (unsigned runde = 0; runde < 8 && !tunnel.gestoert; runde++)
    {
      const KnxMischerFrage frage = knx_mischer_frage(&beleg);
      if (frage == KNX_FRAGE_FERTIG)
        break;
      switch (frage)
      {
      case KNX_FRAGE_EINGANG:
        beleg.eingang = lesen(tunnel, KNX_GA_POS_EINGANG, KNX_AKTOR_MISCHER, true);
        break;
      case KNX_FRAGE_BEWEGUNG_ABWARTEN:
        beleg.spontan = warte(tunnel, VH_WARTE_BEWEGUNG, frist_im_deckel(tunnel, KNX_FRIST_BEWEGUNG_MS)) ? 1 : 0;
        if (beleg.spontan == 1)
          bewegung_nach_ms = (int32_t)(tunnel.bewegung_zeit - positionsbefehl);
        break;
      case KNX_FRAGE_BEWEGUNG_LESEN:
        beleg.bewegung = lesen(tunnel, KNX_GA_BEWEGUNG, KNX_AKTOR_MISCHER, false);
        break;
      case KNX_FRAGE_STATUS_LESEN:
        beleg.status = lesen(tunnel, KNX_GA_POS_STATUS, KNX_AKTOR_MISCHER, true);
        break;
      default:
        break;
      }
    }
    mischer = knx_mischer_urteil(&beleg);

    // Jeder Versuch mit allem, was erhoben wurde: -2 = nicht gefragt,
    // -1 = keine Antwort. Das ist die Zeile, die im Test zaehlt.
    (void)snprintf(log_line, sizeof(log_line),
                   "Vorderhaus Versuch %u: vorher %d, Eingang %d, spontan %d (%ld ms), Bewegung %d, Status %d",
                   versuch, beleg.vorher, beleg.eingang, beleg.spontan, (long)bewegung_nach_ms, beleg.bewegung,
                   beleg.status);
    write_telnet_log(log_line);
    if (!knx_mischer_rot(mischer))
      break;
  }

  // 4. Pumpe - immer, auch nach ROT des Mischers
  bool pumpe_ein = false;
  const char *pumpe_wie = "nicht bestaetigt";
  for (unsigned versuch = 1; versuch <= 2 && !tunnel.gestoert; versuch++)
  {
    tunnel.pumpe_meldung = KNX_NICHT_GEFRAGT;
    if (!telegramm(tunnel, KNX_GA_PUMPE, KNX_APCI_WRITE, 1, nullptr, 0))
      break;
    // Laeuft die Pumpe schon, meldet der Aktor auf denselben Wert nichts
    // (2026-09-12) - dann nach der Frist aktiv lesen.
    if (warte(tunnel, VH_WARTE_PUMPE, frist_im_deckel(tunnel, KNX_FRIST_PUMPE_MS)))
    {
      pumpe_ein = true;
      pumpe_wie = "spontan gemeldet";
      break;
    }
    const int status = lesen(tunnel, KNX_GA_PUMPE_STATUS, KNX_AKTOR_PUMPE, false);
    if (knx_pumpe_ein(status))
    {
      pumpe_ein = true;
      pumpe_wie = "gelesen";
      break;
    }
    (void)snprintf(log_line, sizeof(log_line), "Vorderhaus Pumpe Versuch %u: Status %d statt 1", versuch, status);
    write_telnet_log(log_line);
  }

  // 5. trennen - immer
  const bool verbindung_ok = !tunnel.gestoert;
  trennen(tunnel);
  const uint32_t dauer = millis() - beginn;

  (void)snprintf(log_line, sizeof(log_line),
                 "Vorderhaus: %lu ms, %u Bustelegramme quittiert, %u fremde Antworten (openknx, zaehlen nicht), %u con",
                 (unsigned long)dauer, tunnel.bustelegramme, tunnel.fremde_antworten, tunnel.cons);
  write_telnet_log(log_line);

  // Die zwei Zeilen fuers MQTT-Log: was mit Mischer und Pumpe ist, und das
  // Ergebnis. Bei gestoerter Verbindung steht der erste Grund dabei.
  const KnxVorderhaus ergebnis = knx_vorderhaus_ergebnis(verbindung_ok, mischer, pumpe_ein);
  if (!verbindung_ok)
    (void)snprintf(log_line, sizeof(log_line), "Notbetrieb: Vorderhaus - %.60s", vorderhausStoerung);
  else
    (void)snprintf(log_line, sizeof(log_line), "Notbetrieb: Vorderhaus - Mischer %s (%u. Versuch), Pumpe %s",
                   mischer_text(mischer), versuche, pumpe_ein ? pumpe_wie : "meldet nicht ein");
  write_mqtt_log(log_line);
  (void)snprintf(log_line, sizeof(log_line), "Notbetrieb: Vorderhaus %s nach %lu,%lu s",
                 ergebnis == KNX_VH_GRUEN ? "bestaetigt"
                 : ergebnis == KNX_VH_AUSSTEHEND ? "- Mischer wird alle 10 s abgefragt"
                                                 : "NICHT umgestellt",
                 (unsigned long)(dauer / 1000u), (unsigned long)((dauer % 1000u) / 100u));
  write_mqtt_log(log_line);
  return ergebnis;
}

/*****************************************************************************/
/* Der Rueckfall: eine kurze Abfrage des Mischerstatus                       */
/*                                                                           */
/* Eine eigene Verbindung je Abfrage - verbinden, 6/4/12 lesen, trennen. Nur */
/* der Mischeraktor zaehlt. Rueckgabe true, sobald er 128 +/- 2 meldet.      */
/*                                                                           */
/* Ins MQTT-Log nur die Ankunft; jede einzelne Abfrage steht im Telnet-Log - */
/* bei 10 s Takt ueber bis zu vier Minuten waeren das sonst 22 Zeilen im     */
/* Ring, der 32 fasst.                                                       */
/*****************************************************************************/
bool vorderhaus_abfragen(void)
{
  char log_line[128];
  uint8_t ip[4];
  uint16_t port;
  if (!knx_schnittstelle_lesen(knx_schnittstelle, ip, &port) || WiFi.status() != WL_CONNECTED)
    return false; // das Timeout des Schritts meldet ROT

  if (!verbinden(tunnel, ip, port, KNX_ABFRAGE_DECKEL_MS))
  {
    (void)snprintf(log_line, sizeof(log_line), "Vorderhaus Abfrage: %s", vorderhausStoerung);
    write_telnet_log(log_line);
    return false;
  }
  const int status = lesen(tunnel, KNX_GA_POS_STATUS, KNX_AKTOR_MISCHER, true);
  const unsigned fremde = tunnel.fremde_antworten;
  trennen(tunnel);

  (void)snprintf(log_line, sizeof(log_line), "Vorderhaus Abfrage: Status %d, %u fremde Antworten", status, fremde);
  write_telnet_log(log_line);
  if (!knx_mischer_am_ziel(status))
    return false;

  (void)snprintf(log_line, sizeof(log_line), "Notbetrieb: Vorderhaus - Mischer meldet %d, bestaetigt", status);
  write_mqtt_log(log_line);
  return true;
}
