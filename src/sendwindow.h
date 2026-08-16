#pragma once
#include <stdint.h>

/*****************************************************************************/
/* Zeitregeln des Kommando-Sammelfensters                                    */
/*                                                                           */
/* Bewusst frei von Arduino-Abhaengigkeiten, gleiches Muster wie telegram.h: */
/* derselbe Code laeuft in der Firmware (register_new_command,               */
/* send_pana_command) und im Hosttest (test/sendwindow_test.cpp). Nur so     */
/* ist der Ueberlauf von millis() nach 49,7 Tagen ueberhaupt pruefbar - an   */
/* der Anlage waere er nicht abzuwarten.                                     */
/*                                                                           */
/* Zwei Regeln, zwei Probleme:                                              */
/*                                                                           */
/* 1. COMMAND_WINDOW_MAX - Deckel fuer das Sammelfenster.                   */
/*    Jedes eintreffende SET stiess bis 3.7.0 den 500-ms-Timer neu an, damit */
/*    mehrere Felder in EIN Telegramm wandern. Ein SET-Strom mit weniger als */
/*    500 ms Abstand verlaengerte das Fenster damit unbegrenzt und hielt     */
/*    Senden UND Abfrage an - der Zyklus stand, ohne dass etwas im Log       */
/*    aufgefallen waere. Ab dem Deckel wird nicht mehr verlaengert; das      */
/*    Telegramm geht spaetestens COMMAND_WINDOW_MAX + COMMANDTIMER nach dem  */
/*    ersten SET raus (2,5 s). Spaeter eintreffende Felder oeffnen das       */
/*    naechste Fenster.                                                      */
/*                                                                           */
/* 2. COMMAND_DEFER_MAX - Obergrenze fuers Verschieben des Sendens.         */
/*    Faellt der Sendezeitpunkt in ein laufendes Lesefenster, wird das       */
/*    Senden verschoben statt die schon eingetroffene Antwort ueber          */
/*    flush_serial_input() wegzuwerfen. Weil timeout_serial die Leitung nach */
/*    SERIALTIMEOUT (600 ms) in jedem Fall wieder freigibt, darf das nur ein */
/*    bis zwei Runden dauern - die Grenze ist der Notausgang, falls das Flag */
/*    doch einmal haengen bleibt. Ohne sie koennte ein Kommando unbegrenzt   */
/*    liegenbleiben.                                                         */
/*****************************************************************************/

#define COMMAND_WINDOW_MAX 2000u // ms, Deckel fuer das Sammelfenster
#define COMMAND_DEFER_MAX 4u     // Runden, danach wird trotzdem gesendet

/*****************************************************************************/
/* Darf das Sammelfenster noch einmal verlaengert werden?                    */
/*                                                                           */
/* Die Subtraktion laeuft absichtlich in unsigned-Arithmetik: sie liefert    */
/* auch ueber den millis()-Ueberlauf hinweg die richtige Differenz, solange  */
/* das Fenster kuerzer ist als der halbe Zaehlerbereich (hier: 2 s gegen     */
/* 24,9 Tage). Ein Vergleich der Zeitpunkte selbst (now < start + limit)     */
/* waere an dieser Stelle falsch.                                            */
/*                                                                           */
/* uint32_t und NICHT unsigned long, obwohl millis() unsigned long liefert:  */
/* auf ESP8266 und ESP32 sind beide 32 Bit, auf dem Mac ist unsigned long    */
/* aber 64 Bit. Der Hosttest wuerde den Ueberlauf sonst mit einer Breite     */
/* pruefen, die es auf dem Geraet gar nicht gibt - also gegen nichts.        */
/*****************************************************************************/
inline bool send_window_may_extend(uint32_t now, uint32_t window_start, uint32_t limit)
{
  return (uint32_t)(now - window_start) < limit;
}

/*****************************************************************************/
/* Darf das Senden noch eine Runde verschoben werden?                        */
/*****************************************************************************/
inline bool send_may_defer(unsigned int deferrals, unsigned int limit)
{
  return deferrals < limit;
}
