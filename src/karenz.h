#pragma once
#include <stdint.h>

/*****************************************************************************/
/* Karenzfenster nach dem Verbinden mit dem Broker                           */
/*                                                                           */
/* Bewusst frei von Arduino-Abhaengigkeiten, gleiches Muster wie             */
/* sendwindow.h: derselbe Code laeuft in der Firmware (mqtt_callback, loop)  */
/* und im Hosttest (test/karenz_test.cpp). Nur so ist der Ueberlauf von      */
/* millis() ueberhaupt pruefbar - an der Anlage waere er nicht abzuwarten.   */
/*                                                                           */
/* WARUM ES DAS FENSTER GIBT (3.6.1)                                         */
/* Der ioBroker-MQTT-Adapter beantwortet ein neues Abonnement aus seiner     */
/* Objektdatenbank und schickt den gespeicherten Wert JEDES Set-Topics -     */
/* teils Monate alt. Ohne die Sperre laufen sie als frische Kommandos in     */
/* die Waermepumpe (gemessen 2026-08-13: ein Kurvenwert vom 10.08. setzte    */
/* nach jedem Neustart den Vorlauf-Sollwert auf 55 Grad). Der Schwall kommt  */
/* unmittelbar nach dem SUBACK, 5 s sind reichlich bemessen.                 */
/*                                                                           */
/* WARUM ES ZWEIMAL GEOEFFNET WIRD (3.20.0, M1)                              */
/* In mqtt_reconnect() sobald die Set-Topics abonniert sind, und ein zweites */
/* Mal am Ende von setup(): Der Schwall liegt bis zum ersten loop() im       */
/* TCP-Puffer, und setupTime() wartet dazwischen bis zu 30 s auf NTP. Das    */
/* zweite Oeffnen misst ab dem Zeitpunkt, an dem der Schwall wirklich        */
/* gelesen wird. Beide Aufrufe muessen erhalten bleiben.                     */
/*                                                                           */
/* WARUM STARTZEIT PLUS MERKER STATT FRIST (3.23.1)                          */
/* Bis 3.23.0 stand hier eine Frist (millis() + 5000) und die Pruefung       */
/* (long)(millis() - frist) < 0. Das Idiom ist nur fuer Fristen sicher, die  */
/* nach Ablauf nicht weiter geprueft werden. Diese hier wurde es: Nach       */
/* 2^31 ms = 24,86 Tagen ohne Reconnect wird die Differenz wieder negativ,   */
/* das Fenster meldet "offen", und JEDES Set-Kommando wird verworfen - still, */
/* je Kommando nur eine Telnet-Zeile, bis zum naechsten Reconnect.           */
/* Ein Merker beendet das: Beim ersten Aufruf nach Ablauf schliesst er das   */
/* Fenster, danach wird nichts mehr verglichen.                              */
/*                                                                           */
/* VORBEDINGUNG                                                              */
/* karenz_offen() muss haeufiger als alle 49,7 Tage (2^32 ms) aufgerufen     */
/* werden, sonst laeuft die unsigned Differenz einmal voll herum und das     */
/* Fenster waere wieder offen. loop() ruft die Regel deshalb in JEDEM        */
/* Durchlauf auf (Bilanzmeldung), nicht nur bei verworfenen Kommandos.       */
/*                                                                           */
/* uint32_t und NICHT unsigned long: auf dem ESP32 sind beide 32 Bit, auf    */
/* dem Mac ist unsigned long 64 Bit - der Hosttest wuerde den Ueberlauf      */
/* sonst mit einer Breite pruefen, die es auf dem Geraet nicht gibt.         */
/*****************************************************************************/

#define SUBSCRIBE_GRACE 5000u // ms, Dauer des Karenzfensters nach dem Verbinden

struct Karenzfenster
{
  uint32_t beginn; // millis() beim Oeffnen
  bool offen;      // true, bis karenz_offen() den Ablauf festgestellt hat
};

/*****************************************************************************/
/* Fenster (neu) oeffnen. Ist es schon offen, zaehlt ab jetzt - das ist die  */
/* zweite Armierung am Ende von setup().                                     */
/*****************************************************************************/
inline void karenz_oeffnen(Karenzfenster *k, uint32_t jetzt)
{
  if (!k)
    return;
  k->beginn = jetzt;
  k->offen = true;
}

/*****************************************************************************/
/* Ist das Fenster noch offen?                                               */
/*                                                                           */
/* Fuehrt nach UND antwortet: schliesst beim ersten Aufruf nach Ablauf. Die  */
/* Differenz laeuft in unsigned-Arithmetik und ist ueber den millis()-       */
/* Ueberlauf hinweg richtig, solange der Aufruf innerhalb der Vorbedingung   */
/* liegt (siehe Kopf). Ein Nullzeiger gilt als geschlossen.                  */
/*****************************************************************************/
inline bool karenz_offen(Karenzfenster *k, uint32_t jetzt, uint32_t dauer)
{
  if (!k)
    return false;
  if (k->offen && (uint32_t)(jetzt - k->beginn) >= dauer)
    k->offen = false; // Merker: ab hier wird nichts mehr verglichen
  return k->offen;
}
