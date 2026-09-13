// Nachweis fuer den KNX-Tunnel des Notbetriebsschritts "Vorderhaus" aus 3.21.0
// (src/knxtunnel.h).
//
// Geprueft wird der Code, der auch auf dem Geraet laeuft: die Datei wird hier
// direkt eingebunden, es gibt keine Nachbildung, die auseinanderlaufen kann.
// Gleiches Muster wie test/notbetrieb_test.cpp und test/verbindung_test.cpp.
//
// Worum es geht: Der Schritt laeuft nur im Ernstfall. Ein falsches Byte faellt
// dann erst auf, wenn niemand mehr nachsehen kann - oder nie, wenn ein Filter
// die Antwort von openknx mitzaehlt: Der meldet im Test GRUEN aus dessen
// Zwischenspeicher, und im Ernstfall ist openknx gar nicht da. Deshalb:
//
//  1. Jeder Rahmen byteweise gegen eine UNABHAENGIGE Referenz - die Rohbytes
//     aus den Tests von xknx (github.com/XKNX/xknx, test/knxip_tests/ und
//     test/cemi_tests/cemi_frame_test.py) und die Mitschnitte an der Anlage
//     vom 2026-09-12 (Analyse-KNX-Vorderhaus.md, Abschnitte 7 und 8). Dieselben
//     Sollwerte stehen im Selbsttest von test/knx_tunnel.py 1.6.0, Teil 1-3.
//  2. Das Zerlegen haelt kaputten Datagrammen stand.
//  3. Die Sequenzregel beim Quittieren, samt Ueberlauf 255 -> 0.
//  4. Nur Antworten der Aktoren zaehlen, openknx nie.
//  5. Jeder Zweig der Rueckleseregel A - dieselben Faelle wie die
//     Mischerlaeufe im Selbsttest von knx_tunnel.py, hier samt der
//     Reihenfolge, in der gelesen wird.
//  6. Pumpe und Gesamtergebnis, die Einstellung knx_schnittstelle und die
//     Zeitrechnung ueber den millis()-Ueberlauf nach 49,7 Tagen.
//
// Bauen und ausfuehren:
//   c++ -std=c++17 -O2 -Wall -o /tmp/knx_test test/knx_test.cpp
//   /tmp/knx_test         (Rueckgabewert != 0 = Test fehlgeschlagen)

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>

#include "../src/knxtunnel.h"

static int fehler = 0;

// eine Zusicherung mit Klartext, damit der CI-Log ohne Debugger lesbar ist
static void pruefe(bool bedingung, const char *was)
{
  printf("  [%s] %s\n", bedingung ? "ok " : "FEHLER", was);
  if (!bedingung)
    fehler++;
}

static void pruefe_zahl(long ist, long soll, const char *was)
{
  const bool ok = (ist == soll);
  printf("  [%s] %-62s (erwartet %ld, ist %ld)\n", ok ? "ok " : "FEHLER", was, soll, ist);
  if (!ok)
    fehler++;
}

// Hexfolge wie im Mitschnitt ("06 10 02 05 ...") in Bytes; alles ausser
// Hexziffern trennt nur und wird uebergangen.
static size_t hex(const char *text, uint8_t *out, size_t cap)
{
  size_t n = 0;
  int halb = -1;
  for (const char *p = text; *p; p++)
  {
    int v;
    if (*p >= '0' && *p <= '9')
      v = *p - '0';
    else if (*p >= 'a' && *p <= 'f')
      v = *p - 'a' + 10;
    else if (*p >= 'A' && *p <= 'F')
      v = *p - 'A' + 10;
    else
      continue;
    if (halb < 0)
    {
      halb = v;
    }
    else
    {
      if (n < cap)
        out[n++] = (uint8_t)((halb << 4) | v);
      halb = -1;
    }
  }
  return n;
}

static std::string als_hex(const uint8_t *b, size_t n)
{
  std::string s;
  char z[4];
  for (size_t i = 0; i < n; i++)
  {
    snprintf(z, sizeof(z), i ? " %02x" : "%02x", b[i]);
    s += z;
  }
  return s;
}

// Der gebaute Rahmen muss byteweise dem Sollwert gleichen - bei Abweichung
// stehen beide im Log, damit die Stelle ohne Debugger zu finden ist.
static void pruefe_bytes(const uint8_t *ist, size_t ist_len, const char *soll_hex, const char *was)
{
  uint8_t soll[64];
  const size_t n = hex(soll_hex, soll, sizeof(soll));
  const bool ok = (ist_len == n) && memcmp(ist, soll, n) == 0;
  printf("  [%s] %s\n", ok ? "ok " : "FEHLER", was);
  if (!ok)
  {
    printf("         soll: %s\n         ist:  %s\n", als_hex(soll, n).c_str(),
           als_hex(ist, ist_len).c_str());
    fehler++;
  }
}

// Ein empfangenes TUNNELING_REQUEST vollstaendig zerlegen: Kopf, Verbindungs-
// kopf, cEMI. So laeuft es auch in vorderhaus.cpp.
static bool empfangen(const char *dg_hex, uint8_t *kanal, uint8_t *seq, KnxCemi *r)
{
  uint8_t dg[64];
  const size_t len = hex(dg_hex, dg, sizeof(dg));
  uint16_t dienst = 0;
  const uint8_t *rumpf = nullptr;
  size_t rumpf_len = 0;
  if (!knx_zerlege_kopf(dg, len, &dienst, &rumpf, &rumpf_len) || dienst != KNX_TUNNELING_REQUEST)
    return false;
  const uint8_t *cemi = nullptr;
  size_t cemi_len = 0;
  if (!knx_zerlege_tunneling_request(rumpf, rumpf_len, kanal, seq, &cemi, &cemi_len))
    return false;
  return knx_zerlege_cemi(cemi, cemi_len, r);
}

static const uint8_t IP_MAC[4] = {192, 168, 2, 142}; // der Mac beim Vorabtest

/*****************************************************************************/
/* 1. Rahmen gegen xknx                                                      */
/*****************************************************************************/
static void teil_xknx(void)
{
  printf("\n1. Rahmen gegen die Rohbytes aus den Tests von xknx\n");
  uint8_t out[64];
  size_t n;

  const uint8_t ip42[4] = {192, 168, 42, 1};
  n = knx_baue_connect_request(out, sizeof(out), ip42, 33941, ip42, 52393);
  pruefe_bytes(out, n, "06 10 02 05 00 1A 08 01 C0 A8 2A 01 84 95 08 01 C0 A8 2A 01 CC A9 04 04 02 00",
               "CONNECT_REQUEST wie xknx");

  {
    uint8_t dg[32];
    const size_t len = hex("06 10 02 06 00 14 01 00 08 01 C0 A8 2A 0A 0E 57 04 04 11 FF", dg, sizeof(dg));
    uint16_t dienst = 0;
    const uint8_t *rumpf = nullptr;
    size_t rumpf_len = 0;
    KnxConnectAntwort a;
    const bool ok = knx_zerlege_kopf(dg, len, &dienst, &rumpf, &rumpf_len) &&
                    dienst == KNX_CONNECT_RESPONSE && knx_zerlege_connect_response(rumpf, rumpf_len, &a);
    pruefe(ok && a.kanal == 1 && a.status == 0 && a.daten_ip[0] == 192 && a.daten_ip[1] == 168 &&
               a.daten_ip[2] == 42 && a.daten_ip[3] == 10 && a.daten_port == 3671 && a.adresse == 0x11FF,
           "CONNECT_RESPONSE wie xknx: Kanal 1, 192.168.42.10:3671, 1.1.255");
  }

  n = knx_baue_gruppentelegramm(out, sizeof(out), 1, 23, 0, knx_ga(9, 0, 8), KNX_APCI_WRITE, 1, nullptr, 0);
  pruefe_bytes(out, n, "06 10 04 20 00 15 04 01 17 00 11 00 BC E0 00 00 48 08 01 00 81",
               "TUNNELING_REQUEST GroupValueWrite 9/0/8 = 1 wie xknx");

  n = knx_baue_tunneling_ack(out, sizeof(out), 0x2A, 0x17, 0);
  pruefe_bytes(out, n, "06 10 04 21 00 0A 04 2A 17 00", "TUNNELING_ACK wie xknx");

  const uint8_t ip200[4] = {192, 168, 200, 12};
  n = knx_baue_disconnect_request(out, sizeof(out), 0x15, ip200, 50100);
  pruefe_bytes(out, n, "06 10 02 09 00 10 15 00 08 01 C0 A8 C8 0C C3 B4", "DISCONNECT_REQUEST wie xknx");

  {
    uint8_t c[16];
    const size_t len = hex("2e00bcf011fd094e0103f1", c, sizeof(c));
    KnxCemi r;
    pruefe(knx_zerlege_cemi(c, len, &r) && r.code == KNX_L_DATA_CON && r.quelle == 0x11FD &&
               r.ziel == knx_ga(1, 1, 78) && knx_bestaetigt_ok(r.ctrl1),
           "L_Data.con aus dem xknx-Mitschnitt (1.1.253 -> 1/1/78, positiv)");
  }
}

/*****************************************************************************/
/* 2. Rohbytes von der Anlage (2026-09-12)                                   */
/*****************************************************************************/
static void teil_anlage(void)
{
  printf("\n2. Rohbytes aus den Mitschnitten an der Anlage (2026-09-12)\n");
  uint8_t out[64];
  size_t n;
  uint8_t kanal = 0, seq = 0;
  KnxCemi r;

  // Stufe 0 - vom Mac 192.168.2.142:54986, derselbe Endpunkt zweimal, wie
  // die Firmware ihn schickt
  n = knx_baue_connect_request(out, sizeof(out), IP_MAC, 54986, IP_MAC, 54986);
  pruefe_bytes(out, n, "06 10 02 05 00 1a 08 01 c0 a8 02 8e d6 ca 08 01 c0 a8 02 8e d6 ca 04 04 02 00",
               "Stufe 0: CONNECT_REQUEST, Steuer- und Datenendpunkt gleich");
  {
    uint8_t dg[32];
    const size_t len = hex("06 10 02 06 00 14 e2 00 08 01 c0 a8 02 7f 0e 57 04 04 11 94", dg, sizeof(dg));
    uint16_t dienst = 0;
    const uint8_t *rumpf = nullptr;
    size_t rumpf_len = 0;
    KnxConnectAntwort a;
    const bool ok = knx_zerlege_kopf(dg, len, &dienst, &rumpf, &rumpf_len) &&
                    dienst == KNX_CONNECT_RESPONSE && knx_zerlege_connect_response(rumpf, rumpf_len, &a);
    pruefe(ok && a.kanal == 226 && a.status == 0 && a.daten_ip[3] == 127 && a.daten_port == 3671 &&
               a.adresse == knx_pa(1, 1, 148) && !knx_datenendpunkt_ist_steuer(&a),
           "Stufe 0: CONNECT_RESPONSE - Kanal 226 und Datenendpunkt 192.168.2.127:3671 aus der Antwort");
  }
  n = knx_baue_disconnect_request(out, sizeof(out), 0xE2, IP_MAC, 54986);
  pruefe_bytes(out, n, "06 10 02 09 00 10 e2 00 08 01 c0 a8 02 8e d6 ca", "Stufe 0: DISCONNECT_REQUEST");
  n = knx_baue_disconnect_response(out, sizeof(out), 0xE2, 0);
  pruefe_bytes(out, n, "06 10 02 0a 00 08 e2 00", "Stufe 0: DISCONNECT_RESPONSE, Aufbau wie die der Schnittstelle");

  // Stufe 1 - Lesen 6/4/21, Kanal 66
  n = knx_baue_gruppentelegramm(out, sizeof(out), 0x42, 0, 0, KNX_GA_PUMPE_STATUS, KNX_APCI_READ, 0, nullptr, 0);
  pruefe_bytes(out, n, "06 10 04 20 00 15 04 42 00 00 11 00 bc e0 00 00 34 15 01 00 00",
               "Stufe 1: TUNNELING_REQUEST GroupValueRead 6/4/21");
  {
    uint8_t dg[16];
    const size_t len = hex("06 10 04 21 00 0a 04 42 00 00", dg, sizeof(dg));
    uint16_t dienst = 0;
    const uint8_t *rumpf = nullptr;
    size_t rumpf_len = 0;
    uint8_t st = 0xFF;
    pruefe(knx_zerlege_kopf(dg, len, &dienst, &rumpf, &rumpf_len) && dienst == KNX_TUNNELING_ACK &&
               knx_zerlege_tunneling_ack(rumpf, rumpf_len, &kanal, &seq, &st) && kanal == 0x42 &&
               seq == 0 && st == 0,
           "Stufe 1: TUNNELING_ACK der Schnittstelle - Kanal 66, Sequenz 0, ok");
  }
  pruefe(empfangen("06 10 04 20 00 15 04 42 00 00 2e 00 9c e0 11 94 34 15 01 00 00", &kanal, &seq, &r) &&
             kanal == 0x42 && seq == 0 && r.code == KNX_L_DATA_CON && knx_bestaetigt_ok(r.ctrl1) &&
             r.quelle == knx_pa(1, 1, 148),
         "Stufe 1: L_Data.con mit ctrl1 9c (gesendet bc) - positiv, Quelle 1.1.148 eingesetzt");
  {
    int wert = -9;
    pruefe(empfangen("06 10 04 20 00 15 04 42 01 00 29 00 bc e0 11 3c 34 15 01 00 41", &kanal, &seq, &r) &&
               seq == 1 && knx_leseantwort(&r, KNX_GA_PUMPE_STATUS, KNX_AKTOR_PUMPE, false, &wert) &&
               wert == 1,
           "Stufe 1: GroupValueResponse 6/4/21 = 1 vom Pumpenaktor 1.1.60 zaehlt");
  }
  n = knx_baue_tunneling_ack(out, sizeof(out), 0x42, 0, 0);
  pruefe_bytes(out, n, "06 10 04 21 00 0a 04 42 00 00", "Stufe 1: unser ACK auf Sequenz 0");
  n = knx_baue_tunneling_ack(out, sizeof(out), 0x42, 1, 0);
  pruefe_bytes(out, n, "06 10 04 21 00 0a 04 42 01 00", "Stufe 1: unser ACK auf Sequenz 1");

  // Stufe 2 - Schalten 6/4/20, Kanal 130
  n = knx_baue_gruppentelegramm(out, sizeof(out), 0x82, 1, 0, KNX_GA_PUMPE, KNX_APCI_WRITE, 0, nullptr, 0);
  pruefe_bytes(out, n, "06 10 04 20 00 15 04 82 01 00 11 00 bc e0 00 00 34 14 01 00 80",
               "Stufe 2: GroupValueWrite 6/4/20 = 0");
  n = knx_baue_gruppentelegramm(out, sizeof(out), 0x82, 2, 0, KNX_GA_PUMPE, KNX_APCI_WRITE, 1, nullptr, 0);
  pruefe_bytes(out, n, "06 10 04 20 00 15 04 82 02 00 11 00 bc e0 00 00 34 14 01 00 81",
               "Stufe 2: GroupValueWrite 6/4/20 = 1");
  pruefe(empfangen("06 10 04 20 00 15 04 82 03 00 2e 00 9c e0 11 94 34 14 01 00 80", &kanal, &seq, &r) &&
             knx_bestaetigt_ok(r.ctrl1) && r.klein == 0,
         "Stufe 2: L_Data.con 9c - positiv");
  pruefe(empfangen("06 10 04 20 00 15 04 82 08 00 2e 00 9d e0 11 94 34 14 01 00 81", &kanal, &seq, &r) &&
             !knx_bestaetigt_ok(r.ctrl1) && r.klein == 1,
         "Stufe 2: L_Data.con 9d - negativ, allein an Bit 0 erkannt");

  // Nachweis der Quelladresse 1.1.250 (Abschnitt 8), Kanal 146
  n = knx_baue_gruppentelegramm(out, sizeof(out), 0x92, 0, KNX_QUELLE, KNX_GA_PUMPE_STATUS, KNX_APCI_READ, 0,
                                nullptr, 0);
  pruefe_bytes(out, n, "06 10 04 20 00 15 04 92 00 00 11 00 bc e0 11 fa 34 15 01 00 00",
               "Quelle 1.1.250: GroupValueRead 6/4/21 wie an der Anlage gesendet");
  {
    int wert = -9;
    pruefe(empfangen("06 10 04 20 00 15 04 92 00 00 29 00 bc e0 11 3c 34 15 01 00 41", &kanal, &seq, &r) &&
               kanal == 0x92 && knx_leseantwort(&r, KNX_GA_PUMPE_STATUS, KNX_AKTOR_PUMPE, false, &wert) &&
               wert == 1,
           "Quelle 1.1.250: Antwort des Pumpenaktors nach 55 ms, ohne L_Data.con davor");
  }
}

/*****************************************************************************/
/* 3. Nach Spezifikation: 1-Byte-Wert, Adressen, die Telegramme des Schritts */
/*****************************************************************************/
static void teil_spezifikation(void)
{
  printf("\n3. Nach Spezifikation - 1-Byte-Wert, Adressen, die Telegramme des Schritts\n");
  uint8_t out[64];
  size_t n;
  const uint8_t pos = 128;

  n = knx_baue_cemi(out, sizeof(out), KNX_L_DATA_REQ, 0, 0x3414, KNX_APCI_WRITE, 0, &pos, 1, KNX_CTRL1_SENDEN);
  pruefe_bytes(out, n, "11 00 BC E0 00 00 34 14 02 00 80 80", "cEMI GroupValueWrite 1 Byte (DPT 5, 128 = 50 %)");
  n = knx_baue_cemi(out, sizeof(out), KNX_L_DATA_REQ, 0, 0x3415, KNX_APCI_READ, 0, nullptr, 0, KNX_CTRL1_SENDEN);
  pruefe_bytes(out, n, "11 00 BC E0 00 00 34 15 01 00 00", "cEMI GroupValueRead");
  n = knx_baue_cemi(out, sizeof(out), KNX_L_DATA_REQ, KNX_QUELLE, 0x3415, KNX_APCI_READ, 0, nullptr, 0,
                    KNX_CTRL1_SENDEN);
  pruefe_bytes(out, n, "11 00 BC E0 11 FA 34 15 01 00 00", "cEMI GroupValueRead mit Quelle 1.1.250");

  // Die vier Schreibtelegramme des Schritts, so wie vorderhaus.cpp sie baut
  n = knx_baue_gruppentelegramm(out, sizeof(out), 0x42, 1, KNX_QUELLE, KNX_GA_ZWANG_AUF, KNX_APCI_WRITE, 0,
                                nullptr, 0);
  pruefe_bytes(out, n, "06 10 04 20 00 15 04 42 01 00 11 00 bc e0 11 fa 34 11 01 00 80",
               "Schritt: Zwangsstellung AUF 6/4/17 = 0, Quelle 1.1.250");
  n = knx_baue_gruppentelegramm(out, sizeof(out), 0x42, 2, KNX_QUELLE, KNX_GA_ZWANG_ZU, KNX_APCI_WRITE, 0,
                                nullptr, 0);
  pruefe_bytes(out, n, "06 10 04 20 00 15 04 42 02 00 11 00 bc e0 11 fa 34 10 01 00 80",
               "Schritt: Zwangsstellung ZU 6/4/16 = 0");
  n = knx_baue_gruppentelegramm(out, sizeof(out), 0x42, 3, KNX_QUELLE, KNX_GA_POS_EINGANG, KNX_APCI_WRITE, 0,
                                &pos, 1);
  pruefe_bytes(out, n, "06 10 04 20 00 16 04 42 03 00 11 00 bc e0 11 fa 34 0d 02 00 80 80",
               "Schritt: Position 6/4/13 = 128 (ein Datenbyte, Gesamtlaenge 22)");
  n = knx_baue_gruppentelegramm(out, sizeof(out), 0x42, 4, KNX_QUELLE, KNX_GA_PUMPE, KNX_APCI_WRITE, 1,
                                nullptr, 0);
  pruefe_bytes(out, n, "06 10 04 20 00 15 04 42 04 00 11 00 bc e0 11 fa 34 14 01 00 81",
               "Schritt: Pumpe 6/4/20 = 1");

  // Adressen hin und zurueck
  char text[16];
  knx_ga_text(KNX_GA_PUMPE, text, sizeof(text));
  pruefe(KNX_GA_PUMPE == 0x3414 && strcmp(text, "6/4/20") == 0, "GA 6/4/20 <-> 0x3414");
  knx_pa_text(KNX_QUELLE, text, sizeof(text));
  pruefe(KNX_QUELLE == 0x11FA && strcmp(text, "1.1.250") == 0, "PA 1.1.250 <-> 0x11FA");
  knx_pa_text(KNX_AKTOR_MISCHER, text, sizeof(text));
  pruefe(strcmp(text, "1.1.39") == 0, "Mischeraktor 1.1.39");
  knx_ga_text(knx_ga(31, 7, 255), text, sizeof(text));
  pruefe(strcmp(text, "31/7/255") == 0, "groesste Gruppenadresse 31/7/255");

  // Die Fristen, wie entschieden - eine stille Aenderung faellt hier auf
  pruefe_zahl(KNX_FRIST_BEWEGUNG_MS, 2000, "Bewegung spontan: 2 s wie Regel A (Owner 2026-09-13)");
  pruefe_zahl(KNX_RUECKFALL_FRIST_MS, 220000, "Rueckfall: 220 s (144 s Endlagenlauf + 60 s + Reserve)");
  pruefe_zahl(KNX_RUECKFALL_TAKT_MS, 10000, "Rueckfall: Abfrage alle 10 s");
  pruefe_zahl(KNX_BEFEHL_FENSTER_MS, 16000, "Fenster der Befehlsverbindung im unguenstigsten Fall");
  pruefe_zahl(KNX_RUECKFALL_FRIST_MS + KNX_BEFEHL_DECKEL_MS, 240000,
              "Rueckfall plus Deckel des Austauschs = 240 s (E2, Timeout des Schritts)");
}

/*****************************************************************************/
/* 4. Kaputte Datagramme                                                     */
/*****************************************************************************/
static void teil_robust(void)
{
  printf("\n4. Kaputte und fremde Datagramme werden verworfen, nicht gelesen\n");
  uint8_t dg[64];
  size_t len;
  uint16_t dienst;
  const uint8_t *rumpf;
  size_t rumpf_len;

  len = hex("06 10 04 21 00", dg, sizeof(dg));
  pruefe(!knx_zerlege_kopf(dg, len, &dienst, &rumpf, &rumpf_len), "Kopf kuerzer als 6 Byte");
  len = hex("06 11 04 21 00 0a 04 42 00 00", dg, sizeof(dg));
  pruefe(!knx_zerlege_kopf(dg, len, &dienst, &rumpf, &rumpf_len), "falsche Protokollversion");
  len = hex("06 10 04 21 00 0b 04 42 00 00", dg, sizeof(dg));
  pruefe(!knx_zerlege_kopf(dg, len, &dienst, &rumpf, &rumpf_len), "Laengenfeld groesser als das Datagramm");
  len = hex("06 10 04 21 00 05 04 42 00 00", dg, sizeof(dg));
  pruefe(!knx_zerlege_kopf(dg, len, &dienst, &rumpf, &rumpf_len), "Laengenfeld kleiner als der Kopf");
  len = hex("06 10 04 21 00 0a 04 42 00 00 ff ff", dg, sizeof(dg));
  pruefe(knx_zerlege_kopf(dg, len, &dienst, &rumpf, &rumpf_len) && rumpf_len == 4,
         "Bytes hinter dem Laengenfeld zaehlen nicht zum Rumpf");
  pruefe(!knx_zerlege_kopf(nullptr, 10, &dienst, &rumpf, &rumpf_len), "Nullzeiger");

  KnxConnectAntwort a;
  len = hex("e2 24", dg, sizeof(dg));
  pruefe(knx_zerlege_connect_response(dg, len, &a) && a.status == KNX_E_NO_MORE_CONNECTIONS && a.kanal == 0xE2,
         "CONNECT_RESPONSE mit Ablehnung ist gueltig - der Code steht fuers Log bereit");
  len = hex("e2 00 08 01 c0 a8 02 7f 0e 57", dg, sizeof(dg));
  pruefe(!knx_zerlege_connect_response(dg, len, &a), "CONNECT_RESPONSE ok, aber ohne CRD");
  len = hex("e2 00 08 02 c0 a8 02 7f 0e 57 04 04 11 94", dg, sizeof(dg));
  pruefe(!knx_zerlege_connect_response(dg, len, &a), "CONNECT_RESPONSE mit ungueltigem HPAI");
  len = hex("e2 00 08 01 c0 a8 02 7f 0e 57 04 03 11 94", dg, sizeof(dg));
  pruefe(!knx_zerlege_connect_response(dg, len, &a), "CONNECT_RESPONSE, deren CRD keinen Tunnel beschreibt");
  len = hex("e2 00 08 01 00 00 00 00 00 00 04 04 11 94", dg, sizeof(dg));
  pruefe(knx_zerlege_connect_response(dg, len, &a) && knx_datenendpunkt_ist_steuer(&a),
         "Datenendpunkt 0.0.0.0:0 heisst: derselbe wie der Steuerendpunkt");

  KnxCemi r;
  len = hex("29 00 bc e0 11 3c 34", dg, sizeof(dg));
  pruefe(!knx_zerlege_cemi(dg, len, &r), "cEMI zu kurz");
  len = hex("29 00 bc e0 11 3c 34 15 02 00 41", dg, sizeof(dg));
  pruefe(!knx_zerlege_cemi(dg, len, &r), "NPDU-Laenge 2, aber kein Datenbyte");
  len = hex("29 02 aa bb bc e0 11 27 34 0c 02 00 40 80", dg, sizeof(dg));
  pruefe(knx_zerlege_cemi(dg, len, &r) && r.quelle == KNX_AKTOR_MISCHER && r.ziel == KNX_GA_POS_STATUS &&
             r.apci == KNX_APCI_RESPONSE && r.laenge == 2 && r.daten[0] == 128,
         "cEMI mit 2 Byte Zusatzinformation: sie wird uebersprungen");
  len = hex("29 10 bc e0", dg, sizeof(dg));
  pruefe(!knx_zerlege_cemi(dg, len, &r), "Zusatzinformation laenger als das Datagramm");

  uint8_t kanal, seq;
  const uint8_t *cemi;
  size_t cemi_len;
  len = hex("05 42 00 00 29 00", dg, sizeof(dg));
  pruefe(!knx_zerlege_tunneling_request(dg, len, &kanal, &seq, &cemi, &cemi_len),
         "Verbindungskopf mit falscher Laenge");
  uint8_t st;
  len = hex("04 42 00", dg, sizeof(dg));
  pruefe(!knx_zerlege_tunneling_ack(dg, len, &kanal, &seq, &st), "TUNNELING_ACK zu kurz");

  // Bauen mit zu kleinem Puffer liefert 0 - der Aufrufer sendet dann nichts
  uint8_t klein[10];
  pruefe(knx_baue_connect_request(klein, sizeof(klein), IP_MAC, 1, IP_MAC, 1) == 0,
         "CONNECT_REQUEST passt nicht in 10 Byte -> 0");
  pruefe(knx_baue_gruppentelegramm(klein, sizeof(klein), 1, 0, 0, KNX_GA_PUMPE, KNX_APCI_READ, 0, nullptr, 0) == 0,
         "TUNNELING_REQUEST passt nicht in 10 Byte -> 0");
  uint8_t gross[64];
  uint8_t zuviel[15] = {0};
  pruefe(knx_baue_cemi(gross, sizeof(gross), KNX_L_DATA_REQ, 0, 1, KNX_APCI_WRITE, 0, zuviel, 15,
                       KNX_CTRL1_SENDEN) == 0,
         "15 Datenbytes passen in keinen Standardrahmen -> 0");
}

/*****************************************************************************/
/* 5. Die Sequenzregel                                                       */
/*****************************************************************************/
static void teil_sequenz(void)
{
  printf("\n5. Sequenzregel beim Quittieren (knx_tunnel.py, Tunnel._tunnel_empfangen)\n");
  pruefe(knx_sequenz_regel(0x42, 5, 0x42, 5) == KNX_SEQ_VERARBEITEN, "erwartete Nummer: quittieren und verarbeiten");
  pruefe(knx_sequenz_regel(0x42, 5, 0x42, 4) == KNX_SEQ_NUR_QUITTIEREN,
         "vorige Nummer: nur quittieren - die Schnittstelle wiederholt, weil unser ACK verloren ging");
  pruefe(knx_sequenz_regel(0x42, 5, 0x42, 6) == KNX_SEQ_VERWERFEN, "uebernaechste Nummer: verwerfen, ohne ACK");
  pruefe(knx_sequenz_regel(0x42, 5, 0x43, 5) == KNX_SEQ_VERWERFEN,
         "fremder Kanal: verwerfen, OHNE ACK - auch bei passender Nummer");
  pruefe(knx_sequenz_regel(0x42, 0, 0x42, 255) == KNX_SEQ_NUR_QUITTIEREN, "Ueberlauf: erwartet 0, vorige ist 255");
  pruefe(knx_sequenz_regel(0x42, 255, 0x42, 255) == KNX_SEQ_VERARBEITEN, "Ueberlauf: erwartet 255");
  pruefe(knx_sequenz_regel(0x42, 255, 0x42, 0) == KNX_SEQ_VERWERFEN, "Ueberlauf: 0 ist vor 255 noch nicht dran");
}

/*****************************************************************************/
/* 6. Welche Antwort zaehlt                                                  */
/*****************************************************************************/
static KnxCemi ind(uint16_t quelle, uint16_t ga, uint16_t apci, int wert, bool ein_byte)
{
  uint8_t c[32];
  const uint8_t b = (uint8_t)wert;
  const size_t n = knx_baue_cemi(c, sizeof(c), KNX_L_DATA_IND, quelle, ga, apci, ein_byte ? 0 : (uint8_t)wert,
                                 ein_byte ? &b : nullptr, ein_byte ? 1 : 0, 0xBC);
  KnxCemi r;
  knx_zerlege_cemi(c, n, &r);
  return r;
}

static void teil_filter(void)
{
  printf("\n6. Nur Antworten der Aktoren zaehlen - openknx (1.1.245) nie\n");
  int w = -9;
  KnxCemi r;

  r = ind(KNX_OPENKNX, KNX_GA_POS_STATUS, KNX_APCI_RESPONSE, 128, true);
  pruefe(!knx_leseantwort(&r, KNX_GA_POS_STATUS, KNX_AKTOR_MISCHER, true, &w),
         "6/4/12 = 128 von openknx aus dem Zwischenspeicher zaehlt nicht");
  r = ind(KNX_AKTOR_MISCHER, KNX_GA_POS_STATUS, KNX_APCI_RESPONSE, 128, true);
  pruefe(knx_leseantwort(&r, KNX_GA_POS_STATUS, KNX_AKTOR_MISCHER, true, &w) && w == 128,
         "6/4/12 = 128 vom Mischeraktor 1.1.39 zaehlt");
  r = ind(KNX_OPENKNX, KNX_GA_BEWEGUNG, KNX_APCI_RESPONSE, 0, false);
  pruefe(!knx_leseantwort(&r, KNX_GA_BEWEGUNG, KNX_AKTOR_MISCHER, false, &w),
         "6/4/14 = 0 von openknx zaehlt nicht (an der Anlage dreifach beantwortet)");
  r = ind(KNX_AKTOR_MISCHER, KNX_GA_BEWEGUNG, KNX_APCI_WRITE, 1, false);
  pruefe(knx_meldung(&r, KNX_GA_BEWEGUNG, KNX_AKTOR_MISCHER, false, &w) && w == 1,
         "spontane Bewegung 1 (GroupValueWrite) vom Mischeraktor zaehlt als Meldung");
  pruefe(!knx_leseantwort(&r, KNX_GA_BEWEGUNG, KNX_AKTOR_MISCHER, false, &w),
         "... aber nicht als Leseantwort");
  r = ind(KNX_AKTOR_MISCHER, KNX_GA_BEWEGUNG, KNX_APCI_RESPONSE, 1, false);
  pruefe(!knx_meldung(&r, KNX_GA_BEWEGUNG, KNX_AKTOR_MISCHER, false, &w),
         "eine Leseantwort ist keine spontane Meldung");
  r = ind(KNX_AKTOR_MISCHER, KNX_GA_POS_EINGANG, KNX_APCI_RESPONSE, 128, true);
  pruefe(!knx_leseantwort(&r, KNX_GA_POS_STATUS, KNX_AKTOR_MISCHER, true, &w),
         "Antwort auf eine andere Gruppenadresse zaehlt nicht");
  r = ind(KNX_AKTOR_MISCHER, KNX_GA_POS_STATUS, KNX_APCI_RESPONSE, 1, false);
  pruefe(!knx_leseantwort(&r, KNX_GA_POS_STATUS, KNX_AKTOR_MISCHER, true, &w),
         "1-Bit-Wert auf dem 1-Byte-Objekt zaehlt nicht");
  r = ind(KNX_AKTOR_PUMPE, KNX_GA_PUMPE_STATUS, KNX_APCI_WRITE, 1, false);
  pruefe(knx_meldung(&r, KNX_GA_PUMPE_STATUS, KNX_AKTOR_PUMPE, false, &w) && w == 1,
         "spontaner Pumpenstatus 1 vom Pumpenaktor 1.1.60 zaehlt");
  pruefe(!knx_meldung(&r, KNX_GA_PUMPE_STATUS, KNX_AKTOR_MISCHER, false, &w),
         "... aber nicht, wenn der Mischeraktor erwartet wird");

  // Eine L_Data.con traegt dieselben Felder wie eine Antwort - und ist doch
  // nur die Kopie des eigenen Telegramms
  uint8_t c[32];
  const uint8_t b = 128;
  const size_t n = knx_baue_cemi(c, sizeof(c), KNX_L_DATA_CON, KNX_AKTOR_MISCHER, KNX_GA_POS_STATUS,
                                 KNX_APCI_RESPONSE, 0, &b, 1, 0x9C);
  knx_zerlege_cemi(c, n, &r);
  pruefe(!knx_leseantwort(&r, KNX_GA_POS_STATUS, KNX_AKTOR_MISCHER, true, &w),
         "eine L_Data.con zaehlt nie als Antwort");
}

/*****************************************************************************/
/* 7. Rueckleseregel A                                                       */
/*                                                                           */
/* Ein gespielter Aktor beantwortet jede Frage aus einer Tabelle. Mitgeschrie-*/
/* ben wird, WAS in welcher Reihenfolge erhoben wurde: E = Eingang lesen,     */
/* A = auf die spontane Bewegung warten, B = Bewegung lesen, S = Status lesen.*/
/* So prueft der Test nicht nur das Urteil, sondern auch, dass die Firmware   */
/* nie mehr liest als noetig - und nie weniger.                               */
/*****************************************************************************/
struct Aktor
{
  int vorher, eingang, spontan, bewegung, status;
};

static KnxMischerUrteil spiele(KnxMischerBeleg *b, const Aktor &a, std::string *spur)
{
  for (int runde = 0; runde < 10; runde++)
  {
    const KnxMischerFrage f = knx_mischer_frage(b);
    if (f == KNX_FRAGE_FERTIG)
      break;
    switch (f)
    {
    case KNX_FRAGE_EINGANG:
      b->eingang = a.eingang;
      *spur += 'E';
      break;
    case KNX_FRAGE_BEWEGUNG_ABWARTEN:
      b->spontan = a.spontan;
      *spur += 'A';
      break;
    case KNX_FRAGE_BEWEGUNG_LESEN:
      b->bewegung = a.bewegung;
      *spur += 'B';
      break;
    case KNX_FRAGE_STATUS_LESEN:
      b->status = a.status;
      *spur += 'S';
      break;
    default:
      *spur += '?';
      return knx_mischer_urteil(b);
    }
  }
  return knx_mischer_urteil(b);
}

static void fall(const Aktor &a, KnxMischerUrteil soll_urteil, const char *soll_spur, const char *was)
{
  KnxMischerBeleg b;
  knx_mischer_beleg_leeren(&b);
  b.vorher = a.vorher;
  std::string spur;
  const KnxMischerUrteil u = spiele(&b, a, &spur);
  const bool ok = (u == soll_urteil) && spur == soll_spur;
  printf("  [%s] %s\n", ok ? "ok " : "FEHLER", was);
  if (!ok)
  {
    printf("         Urteil soll %d, ist %d; Spur soll \"%s\", ist \"%s\"\n", (int)soll_urteil, (int)u, soll_spur,
           spur.c_str());
    fehler++;
  }
}

static void teil_regel_a(void)
{
  printf("\n7. Rueckleseregel A - jeder Zweig, samt Reihenfolge des Lesens\n");
  const int N = KNX_NICHT_GEFRAGT;
  const int K = KNX_KEINE_ANTWORT;

  // Die schnelle Regel
  fall({0, 128, 1, N, N}, KNX_MISCHER_GRUEN_BEWEGUNG, "EA",
       "Lauf 1: Bewegung vorher 0, Eingang 128, Bewegung 1 spontan -> GRUEN");
  fall({0, 128, 0, 1, N}, KNX_MISCHER_GRUEN_BEWEGUNG, "EAB",
       "keine spontane Meldung, Bewegung aktiv gelesen 1 -> GRUEN");
  fall({0, 128, 0, 0, 128}, KNX_MISCHER_GRUEN_AM_ZIEL, "EABS",
       "Lauf 2: steht schon auf 128, faehrt nicht, meldet nichts -> GRUEN ueber den Status");
  fall({0, 128, 0, K, 128}, KNX_MISCHER_GRUEN_AM_ZIEL, "EABS",
       "Bewegung nicht beantwortet, Status 128 -> GRUEN ueber den Status");
  fall({0, 128, 0, 0, 130}, KNX_MISCHER_GRUEN_AM_ZIEL, "EABS", "Status 130: an der Toleranzgrenze -> GRUEN");
  fall({0, 128, 0, 0, 126}, KNX_MISCHER_GRUEN_AM_ZIEL, "EABS", "Status 126: an der Toleranzgrenze -> GRUEN");
  fall({0, 128, 0, 0, 131}, KNX_MISCHER_ROT_STEHT, "EABS", "Status 131: knapp daneben -> ROT");
  fall({0, 128, 0, 0, 125}, KNX_MISCHER_ROT_STEHT, "EABS", "Status 125: knapp daneben -> ROT");
  fall({0, 128, 0, 0, 0}, KNX_MISCHER_ROT_STEHT, "EABS",
       "Zwangsstellung ZU klemmt: Eingang 128, keine Bewegung, Status 0 -> ROT");
  fall({0, 128, 0, 0, K}, KNX_MISCHER_ROT_STEHT, "EABS", "keine Bewegung, Status unbeantwortet -> ROT");

  // Der Eingang ist Pflicht - und entscheidet vor allem anderen
  fall({0, 100, N, N, 255}, KNX_MISCHER_ROT_EINGANG, "E",
       "Lauf 4 (Negativprobe): Eingang 100 -> ROT, nichts weiter gelesen");
  fall({0, 46, 1, 1, N}, KNX_MISCHER_ROT_EINGANG, "E",
       "Positionsbefehl verloren (Eingang 46) - auch wenn der Mischer faehrt: ROT");
  fall({0, K, 1, 1, 128}, KNX_MISCHER_ROT_EINGANG, "E",
       "Eingang unbeantwortet zaehlt als Abweichung -> ROT");

  // Der Rueckfall
  fall({1, 128, N, N, N}, KNX_MISCHER_AUSSTEHEND, "E",
       "Mischer faehrt schon (Bewegung vorher 1): Eingang passt -> Rueckfall, Endstellung entscheidet");
  fall({K, 128, N, N, N}, KNX_MISCHER_AUSSTEHEND, "E",
       "Aktor schweigt auf die Vorab-Lesung -> zaehlt wie Bewegung 1, Rueckfall");
  fall({N, 128, N, N, N}, KNX_MISCHER_AUSSTEHEND, "E",
       "Vorab-Lesung nie erhoben -> auch Rueckfall, die sichere Seite");
  fall({1, 46, N, N, N}, KNX_MISCHER_ROT_EINGANG, "E", "Rueckfall, aber Eingang falsch -> ROT, nicht ausstehend");
  fall({2, 128, N, N, N}, KNX_MISCHER_AUSSTEHEND, "E", "unsinniger Bewegungswert vorher -> Rueckfall");

  // Die eine Wiederholung: Nur der Stand VOR dem ersten Befehl bleibt
  {
    KnxMischerBeleg b;
    knx_mischer_beleg_leeren(&b);
    b.vorher = 0;
    std::string spur;
    const KnxMischerUrteil u1 = spiele(&b, {0, 46, N, N, N}, &spur); // erster Satz verloren
    knx_mischer_neuer_versuch(&b);
    const bool geleert = b.vorher == 0 && b.eingang == N && b.spontan == N && b.bewegung == N && b.status == N;
    spur += '|';
    const KnxMischerUrteil u2 = spiele(&b, {0, 128, 1, N, N}, &spur);
    pruefe(u1 == KNX_MISCHER_ROT_EINGANG && geleert && u2 == KNX_MISCHER_GRUEN_BEWEGUNG && spur == "E|EA",
           "erster Satz Mischertelegramme verloren -> Wiederholung mit frischem Beleg, Bewegung vorher bleibt -> GRUEN");
  }

  // Die Hilfsregeln einzeln
  pruefe(!knx_mischer_am_ziel(K) && !knx_mischer_am_ziel(N) && !knx_mischer_am_ziel(-5) &&
             !knx_mischer_am_ziel(256) && !knx_mischer_am_ziel(384),
         "am Ziel: keine Antwort und Werte ausserhalb 0-255 zaehlen nie");
  pruefe(knx_mischer_rot(KNX_MISCHER_ROT_EINGANG) && knx_mischer_rot(KNX_MISCHER_ROT_STEHT) &&
             !knx_mischer_rot(KNX_MISCHER_AUSSTEHEND) && !knx_mischer_rot(KNX_MISCHER_GRUEN_AM_ZIEL),
         "ROT sind genau die beiden ROT-Urteile");
  pruefe(knx_mischer_frage(nullptr) == KNX_FRAGE_FERTIG && knx_mischer_urteil(nullptr) == KNX_MISCHER_ROT_EINGANG,
         "Nullzeiger: nichts fragen, ROT");
}

/*****************************************************************************/
/* 8. Pumpe und Gesamtergebnis                                               */
/*****************************************************************************/
static void teil_ergebnis(void)
{
  printf("\n8. Pumpe und Gesamtergebnis des Schritts\n");
  pruefe(knx_pumpe_ein(1) && !knx_pumpe_ein(0) && !knx_pumpe_ein(KNX_KEINE_ANTWORT) && !knx_pumpe_ein(2),
         "Pumpe ein heisst: der Aktor meldet genau 1");
  pruefe(knx_vorderhaus_ergebnis(true, KNX_MISCHER_GRUEN_BEWEGUNG, true) == KNX_VH_GRUEN, "Mischer faehrt, Pumpe ein -> GRUEN");
  pruefe(knx_vorderhaus_ergebnis(true, KNX_MISCHER_GRUEN_AM_ZIEL, true) == KNX_VH_GRUEN, "Mischer am Ziel, Pumpe ein -> GRUEN");
  pruefe(knx_vorderhaus_ergebnis(true, KNX_MISCHER_AUSSTEHEND, true) == KNX_VH_AUSSTEHEND,
         "Rueckfall, Pumpe ein -> ausstehend, Abfragen im Takt");
  pruefe(knx_vorderhaus_ergebnis(true, KNX_MISCHER_AUSSTEHEND, false) == KNX_VH_ROT,
         "Rueckfall, aber Pumpe meldet nicht ein -> sofort ROT, Warten aenderte nichts");
  pruefe(knx_vorderhaus_ergebnis(true, KNX_MISCHER_ROT_STEHT, true) == KNX_VH_ROT,
         "Mischer ROT, Pumpe trotzdem ein (Owner 2026-09-13) -> Schritt ROT");
  pruefe(knx_vorderhaus_ergebnis(true, KNX_MISCHER_GRUEN_BEWEGUNG, false) == KNX_VH_ROT, "Pumpe meldet nicht ein -> ROT");
  pruefe(knx_vorderhaus_ergebnis(false, KNX_MISCHER_GRUEN_BEWEGUNG, true) == KNX_VH_ROT,
         "Verbindung gestoert -> ROT, gleich was vorher gelesen wurde");
}

/*****************************************************************************/
/* 9. Die Einstellung knx_schnittstelle                                      */
/*****************************************************************************/
static void gut(const char *text, uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint16_t port)
{
  uint8_t ip[4] = {0, 0, 0, 0};
  uint16_t p = 0;
  const bool ok = knx_schnittstelle_lesen(text, ip, &p) && ip[0] == a && ip[1] == b && ip[2] == c &&
                  ip[3] == d && p == port;
  printf("  [%s] \"%s\" -> %u.%u.%u.%u:%u\n", ok ? "ok " : "FEHLER", text, a, b, c, d, port);
  if (!ok)
    fehler++;
}

static void schlecht(const char *text)
{
  uint8_t ip[4] = {9, 9, 9, 9};
  uint16_t p = 99;
  const bool abgelehnt = !knx_schnittstelle_lesen(text, ip, &p);
  // Eine Ablehnung darf die Ausgabe nicht halb beschreiben
  const bool unberuehrt = ip[0] == 9 && ip[3] == 9 && p == 99;
  printf("  [%s] \"%s\" abgelehnt\n", (abgelehnt && unberuehrt) ? "ok " : "FEHLER", text);
  if (!abgelehnt || !unberuehrt)
    fehler++;
}

static void teil_einstellung(void)
{
  printf("\n9. Einstellung knx_schnittstelle: nur eine IP, wahlweise mit :Port (E1)\n");
  gut("192.168.2.127", 192, 168, 2, 127, 3671);
  gut("192.168.2.127:3671", 192, 168, 2, 127, 3671);
  gut("  192.168.2.142:50000 \r\n", 192, 168, 2, 142, 50000);
  gut("10.0.0.1:1", 10, 0, 0, 1, 1);
  gut("1.2.3.4:65535", 1, 2, 3, 4, 65535);
  gut("192.168.2.0", 192, 168, 2, 0, 3671);

  schlecht("");
  schlecht("   ");
  schlecht("192.168.2");
  schlecht("192.168.2.127.1");
  schlecht("192.168.2.256");
  schlecht("192.168..127");
  schlecht("192.168.2.-1");
  schlecht("1234.1.1.1");
  schlecht("192.168.002.127");
  schlecht("192.168.2.127:");
  schlecht("192.168.2.127:0");
  schlecht("192.168.2.127:65536");
  schlecht("192.168.2.127:03671");
  schlecht("192.168.2.127:3671:1");
  schlecht(":3671");
  schlecht("192.168.2.127 x");
  schlecht("192.168.2 .127");
  schlecht("knx-schnittstelle.local");
  schlecht("http://192.168.2.127");
  schlecht("0.0.0.0");
  schlecht("127.0.0.1");
  schlecht("224.0.23.12");
  schlecht("255.255.255.255");

  uint8_t ip[4];
  uint16_t p;
  pruefe(!knx_schnittstelle_lesen(nullptr, ip, &p), "Nullzeiger abgelehnt");
}

/*****************************************************************************/
/* 10. Zeitrechnung ueber den millis()-Ueberlauf                             */
/*****************************************************************************/
static void teil_zeit(void)
{
  printf("\n10. Zeitrechnung ueber den millis()-Ueberlauf nach 49,7 Tagen\n");
  pruefe_zahl(knx_restzeit(1000, 1000, KNX_FRIST_LESEN_MS), 1000, "Restzeit zu Beginn: die ganze Frist");
  pruefe_zahl(knx_restzeit(1000, 1250, KNX_FRIST_LESEN_MS), 750, "Restzeit nach 250 ms");
  pruefe_zahl(knx_restzeit(1000, 2000, KNX_FRIST_LESEN_MS), 0, "Frist genau abgelaufen: 0");
  pruefe_zahl(knx_restzeit(1000, 900000, KNX_FRIST_LESEN_MS), 0, "Frist lange abgelaufen: 0, kein Unterlauf");

  // Start kurz vor der Naht, jetzt kurz dahinter: vergangen sind 6144 ms
  const uint32_t vor_naht = 0xFFFFF000u;
  const uint32_t nach_naht = 0x00000800u;
  pruefe_zahl(knx_restzeit(vor_naht, nach_naht, KNX_BEFEHL_DECKEL_MS), 20000 - 6144,
              "Deckel ueber die Naht: 20000 - 6144 ms");
  pruefe_zahl(knx_restzeit(vor_naht, nach_naht, KNX_FRIST_LESEN_MS), 0,
              "Lesefrist ueber die Naht abgelaufen - nicht 49,7 Tage offen");

  pruefe(!knx_abfrage_faellig(1000, 1000 + KNX_RUECKFALL_TAKT_MS - 1), "Abfrage 1 ms vor dem Takt: noch nicht");
  pruefe(knx_abfrage_faellig(1000, 1000 + KNX_RUECKFALL_TAKT_MS), "Abfrage genau im Takt: faellig");
  pruefe(!knx_abfrage_faellig(0xFFFFF000u, 0xFFFFF000u + 5000u), "ueber die Naht: nach 5 s noch nicht faellig");
  pruefe(knx_abfrage_faellig(0xFFFFE000u, 0xFFFFE000u + 10000u), "ueber die Naht: nach 10 s faellig");
}

int main(void)
{
  printf("knx_test - KNX-Tunnel des Notbetriebsschritts Vorderhaus (src/knxtunnel.h)\n");
  teil_xknx();
  teil_anlage();
  teil_spezifikation();
  teil_robust();
  teil_sequenz();
  teil_filter();
  teil_regel_a();
  teil_ergebnis();
  teil_einstellung();
  teil_zeit();

  printf("\n%s: %d Fehler\n", fehler == 0 ? "GRUEN" : "ROT", fehler);
  return fehler == 0 ? 0 : 1;
}
