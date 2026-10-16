#pragma once
#include <stdint.h>

/*****************************************************************************/
/* Regeln der nicht blockierenden Telnet-Ausgabe (S2, 3.24.0)                */
/*                                                                           */
/* Bewusst frei von Arduino-Abhaengigkeiten, gleiches Muster wie             */
/* sendwindow.h und karenz.h: derselbe Code laeuft in der Firmware           */
/* (telnetlog.cpp) und im Hosttest (test/telnetlog_test.cpp). Hier steht nur */
/* die Entscheidung, wann ein Client als tot gilt - das Schreiben auf den    */
/* Socket steht in telnetlog.cpp.                                            */
/*                                                                           */
/* WARUM ES DAS GIBT (Befund S2 der Durchsicht 2026-10-15):                  */
/*                                                                           */
/* Bis 3.23.2 schrieb die Bibliothek TelnetStream ueber                      */
/* NetworkClient::write() des Cores. Ist der Sendepuffer des Clients voll,   */
/* wartet das bis zu 10 x 1 s JE LOGZEILE, und die Bibliothek tut das nach-  */
/* einander fuer jeden der bis zu fuenf Clients. loop() stand dann Minuten:  */
/* keine Abfrage der Waermepumpe, kein MQTT-Keepalive, keine Notbetriebs-    */
/* seite. Port 23 hat keine Anmeldung. Am Pruefling mit 3.23.2 gemessen: ein */
/* Client, der nicht liest, liess die Startseite 90 s lang unbeantwortet.    */
/*                                                                           */
/* DIE REGEL: Die Firmware wartet nie auf einen Client.                      */
/*  - Geht eine Ausgabe nicht (oder nur zum Teil) in den Sendepuffer, wird   */
/*    der Rest verworfen und gezaehlt. Eine zerschnittene Logzeile ist       */
/*    besser als ein stehender loop() (Owner-Entscheid 2026-10-16).          */
/*  - Gelingt eine Ausgabe wieder, meldet eine <INF>-Zeile, wie viele        */
/*    Zeilen gefehlt haben.                                                  */
/*  - Liest ein Client 15 s lang nichts mehr, wird er getrennt.              */
/*                                                                           */
/* WARUM EINE ZEITREGEL UND KEINE ZAEHLREGEL: Im Leerlauf kommen rund drei   */
/* Zeilen je 5 s, mit viel Betrieb zehn je Sekunde. "Nach 20 Fehlversuchen"  */
/* waere im ersten Fall nach einer Minute, im zweiten nach zwei Sekunden     */
/* getrennt. 15 s gelten fuer beide gleich.                                  */
/*                                                                           */
/* UEBERLAUF (Lehre aus S1, 3.23.1): Die Uhr der Serie startet erst mit dem  */
/* ersten Fehlschlag und wird mit der ersten gelungenen Ausgabe verworfen.   */
/* Verglichen wird nur waehrend einer Serie, und die endet mit dem Trennen   */
/* nach hoechstens 15 s plus dem Abstand der Aufrufe. Es gibt keine Frist,   */
/* die nach Ablauf weiter geprueft wird. Ohne Serie ist ein Client nie tot - */
/* auch nicht nach 50 Tagen ohne eine einzige Ausgabe (Debug-Log aus).       */
/* Die Differenz ist unsigned (uint32_t): richtig ueber den Ueberlauf bis zur */
/* vollen Breite von 49,7 Tagen. Liegt zwischen erstem Fehlschlag und        */
/* naechstem Aufruf mehr als das, sieht die Differenz klein aus und der      */
/* Client wird erst beim naechsten Fehlschlag getrennt - bewusst hingenommen,*/
/* weil dazu das Log 49,7 Tage lang schweigen muesste.                       */
/*                                                                           */
/* uint32_t und NICHT unsigned long: auf dem ESP32 sind beide 32 Bit, auf    */
/* dem Mac ist unsigned long 64 Bit, und der Hosttest wuerde den Ueberlauf   */
/* gegen eine Breite pruefen, die es auf dem Geraet nicht gibt.              */
/*****************************************************************************/

#define TELNET_STILL_MAX_MS 15000u // ms ohne gelungene Ausgabe, dann wird getrennt

struct TelnetClientZustand
{
  uint32_t serieBeginn; // millis() des ersten Fehlschlags der laufenden Serie
  uint32_t verworfen;   // Zeilen, die seit der letzten gelungenen Ausgabe fehlten
  bool serie;           // laeuft eine Serie von Fehlschlaegen?
};

/*****************************************************************************/
/* Neuer Client: alles zuruecksetzen                                         */
/*****************************************************************************/
inline void telnet_neuer_client(TelnetClientZustand *z)
{
  if (!z)
    return;
  z->serieBeginn = 0;
  z->verworfen = 0;
  z->serie = false;
}

/*****************************************************************************/
/* Eine Ausgabe ging nicht (vollstaendig) in den Sendepuffer                 */
/*****************************************************************************/
inline void telnet_verworfen(TelnetClientZustand *z, uint32_t jetzt)
{
  if (!z)
    return;
  if (!z->serie)
  {
    z->serie = true;
    z->serieBeginn = jetzt;
  }
  // saettigen statt ueberlaufen: der Zaehler ist nur eine Auskunft
  if (z->verworfen < UINT32_MAX)
    z->verworfen++;
}

/*****************************************************************************/
/* Eine Ausgabe ist vollstaendig im Sendepuffer gelandet                     */
/*                                                                           */
/* Beendet die Serie und liefert die Zahl der Zeilen, die zuvor fehlten      */
/* (0 = nichts zu melden). Der Aufrufer schreibt damit die <INF>-Zeile.      */
/*****************************************************************************/
inline uint32_t telnet_gelungen(TelnetClientZustand *z)
{
  if (!z)
    return 0;
  const uint32_t fehlten = z->verworfen;
  z->serie = false;
  z->verworfen = 0;
  return fehlten;
}

/*****************************************************************************/
/* Gilt der Client als tot?                                                  */
/*****************************************************************************/
inline bool telnet_tot(const TelnetClientZustand *z, uint32_t jetzt)
{
  if (!z || !z->serie)
    return false;
  return (uint32_t)(jetzt - z->serieBeginn) >= TELNET_STILL_MAX_MS;
}
