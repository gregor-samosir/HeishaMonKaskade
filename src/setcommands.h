#pragma once
#include <stdint.h>
#include <string.h>

/*****************************************************************************/
/* Der Kommandopfad: Tabelle, Umrechnung, Maske, Konflikt (3.25.1, C1)       */
/*                                                                           */
/* Bewusst frei von Arduino-Abhaengigkeiten, gleiches Muster wie             */
/* setpayload.h und sendwindow.h: derselbe Code laeuft in der Firmware       */
/* (commands.cpp, HeishaMon.cpp) und im Hosttest (test/setcommands_test.cpp, */
/* merge_test.cpp, byte9/20/28_test.cpp).                                    */
/*                                                                           */
/* WARUM ES DAS GIBT                                                         */
/* Bis 3.25.0 stand die Tabelle samt Umrechnung in commands.cpp, das an der  */
/* Arduino-Welt haengt. Die Tests bildeten die Merge-Zeile nach und trugen   */
/* Masken und Parameter als eigene Konstanten: Ein falscher Parameter in der */
/* Tabelle (z. B. 64 statt 16 bei SET40, Maske 0x30) ergab 0x40 & 0x30 = 0 - */
/* das Kommando sendete still "keine Aenderung", und alle Tests blieben      */
/* gruen. Jetzt rufen die Tests den echten Code, und setcommands_pruefen()   */
/* haelt die Tabelle zur UEBERSETZUNGSZEIT zusammen: Eine Zeile, die gegen   */
/* eine der Regeln unten verstoesst, bricht den Firmware-Build.              */
/*                                                                           */
/* VERHALTEN BLEIBT, WAS ES WAR                                              */
/* Die Umrechnung und die Maskenzeile sind die aus commands.cpp 3.25.0,      */
/* Zeichen fuer Zeichen; belegt durch die Vorher/Nachher-Liste aller Paare   */
/* (SET, Wert) -> (Byte, Maske, Rohwert) im Arbeitsplan-Kommandopfad-3.25.1. */
/*****************************************************************************/

// Laenge des Kommandotelegramms ohne Pruefsumme: Indizes 0..109. Vor 3.25.1
// stand die 110 in HeishaMon.h; die Tabellenpruefung unten braucht sie, und
// eine zweite Definition waere eine zweite Wahrheit. HeishaMon.h bindet diesen
// Header ein.
#ifndef QUERYSIZE
#define QUERYSIZE 110
#endif

/*****************************************************************************/
/* Set command table                                                         */
/* one row per set topic: target position in mainCommand, allowed value     */
/* range and the conversion rule from mqtt value to protocol byte           */
/*****************************************************************************/

// conversion rules from mqtt value to protocol byte
// uint8_t statt int: das enum steckt in jeder Tabellenzeile, und jedes
// gesparte Byte zaehlt dort mal die Zeilenzahl (vgl. Feldreihenfolge in
// decode.h)
enum ConvType : uint8_t
{
  CONV_ADD,    // set_byte = value + param
  CONV_MUL,    // set_byte = value * param
  CONV_MUL_INC,// set_byte = (value + 1) * param
  CONV_OPMODE  // set_byte = opModeBytes[value]
};

// protocol bytes for OperationMode 0-6 (heat, cool, auto, dhw, heat+dhw, cool+dhw, auto+dhw)
inline constexpr uint8_t opModeBytes[] = {18, 19, 24, 33, 34, 35, 40};
inline constexpr int OPMODE_ANZAHL = (int)(sizeof(opModeBytes) / sizeof(opModeBytes[0]));

struct SetCommand
{
  uint8_t number;   // SETn, for logging only
  uint8_t pos;      // target position in mainCommand
  uint8_t mask;     // bits this field owns inside that byte (see below)
  ConvType conv;    // conversion rule
  const char *name; // mqtt topic name below <prefix>/set/
  int min;          // allowed value range (inclusive)
  int max;
  int param;        // offset or factor for the conversion
};

/*****************************************************************************/
/* Why the mask column exists                                                */
/*                                                                           */
/* Several set topics share one protocol byte, each owning a different group */
/* of bits (0 in a group means "no change" for that field). Writing the byte */
/* as a whole therefore silently wipes every other field in it - two set     */
/* commands arriving inside the same 500 ms window would cancel each other.  */
/* Applying the value through its mask keeps neighbouring fields intact:     */
/*                                                                           */
/*   byte 4:  Heatpump 0x03  |  WaterPump 0x30  |  ForceDHW 0xC0             */
/*   byte 5:  ForceHeater 0x0C  |  HolidayMode 0x30                          */
/*   byte 8:  ForceDefrost 0x02  |  ForceSterilization 0x04                  */
/*   byte 9:  DHWHeaterState 0x0C  |  RoomHeaterState 0x03                   */
/*   byte 20: AltExternalSensor 0x30                                         */
/*   byte 28: CoolingMode 0x0C  |  HeatingMode 0x03                          */
/*                                                                           */
/* byte 20 also carries installer fields with no set command (frost          */
/* protection, option board); 0 in an untouched group means "no change"      */
/* there too. Bit 1 (water/glycol) is a single bit, and every SET40          */
/* command sends 0 there (this system runs on water, see E7).                */
/*                                                                           */
/* Exception - byte 7: QuietMode ((n+1)*8) and PowerfulMode (73..76) really  */
/* do overlap in bit 3, PowerfulMode carries an implicit "quiet off". That   */
/* is a protocol property, not an implementation flaw, so both keep mask     */
/* 0xFF and behave exactly as before (last one wins). The conflict warning   */
/* below makes it visible if it ever happens.                               */
/*****************************************************************************/

/*****************************************************************************/
/* Warum der Topic-Name in der Tabelle steht                                 */
/*                                                                           */
/* Bis 3.4.1 fuehrte jeder Set-Topic-Name drei Leben: eine Deklaration in    */
/* Topics.h, die Definition in Topics.cpp und den Verweis darauf hier. Dazu  */
/* kam ein handgeschriebener subscribe-Aufruf in mqtt_reconnect(). Wer den   */
/* vergass, bekam KEINEN Compilerfehler - das Topic war einfach stumm.       */
/* Jetzt steht der Name genau hier, und subscribe_set_topics() laeuft ueber  */
/* dieselbe Tabelle. Ein neues Set-Kommando ist damit eine Zeile.            */
/* Dasselbe Muster wie bei den State-Topics (stateTopics[] in decode.cpp):    */
/* die Tabelle traegt den Namen, die Pfadwurzel kommt aus Topics.cpp.        */
/*****************************************************************************/

/*****************************************************************************/
/* Warum die Nummerierung Luecken hat                                        */
/*                                                                           */
/* SET7 und SET8 waren die Zone-2-Anforderungstemperaturen und sind in 3.4.0 */
/* entfallen - diese Anlage hat keine Zone 2, und in Node-RED wurden sie nie */
/* benutzt. Die uebrigen Nummern sind ABSICHTLICH stehen geblieben: 'number' */
/* ist ein Datenfeld, kein Index, und SET9 heisst in MQTT-Topics.md und in    */
/* allen Notizen weiter SET9. Bitte nicht "aufraeumen" - eine Umnummerierung */
/* macht jede Doku und jeden Mitschnitt von vorher falsch.                   */
/*****************************************************************************/

inline constexpr SetCommand setCommands[] = {
    // Nr pos mask conversion    topic-name                     min  max  param
    { 1,  4, 0x03, CONV_ADD,     "Heatpump",                      0,   1,   1}, // off=1 on=2
    { 2,  5, 0x30, CONV_MUL_INC, "HolidayMode",                   0,   1,  16}, // off=16 on=32
    { 3,  7, 0xFF, CONV_MUL_INC, "QuietMode",                     0,   3,   8}, // level -> (n+1)*8
    { 4,  7, 0xFF, CONV_ADD,     "PowerfulMode",                  0,   3,  73}, // 0/30/60/90 min
    { 5, 38, 0xFF, CONV_ADD,     "Z1HeatRequestTemperature",     -5,  65, 128}, // shift or direct temp
    { 6, 39, 0xFF, CONV_ADD,     "Z1CoolRequestTemperature",     -5,  65, 128}, // shift or direct temp
    { 9,  6, 0xFF, CONV_OPMODE,  "OperationMode",                 0,   6,   0}, // via lookup
    {10,  4, 0xC0, CONV_MUL_INC, "ForceDHW",                      0,   1,  64}, // off=64 on=128
    {11, 42, 0xFF, CONV_ADD,     "DHWTemp",                      40,  75, 128}, // DHW target temp
    {12,  8, 0x02, CONV_MUL,     "ForceDefrost",                  0,   1,   2}, // off=0 on=2
    {13,  8, 0x04, CONV_MUL,     "ForceSterilization",            0,   1,   4}, // off=0 on=4
    {14,  4, 0x30, CONV_MUL_INC, "WaterPump",                     0,   2,  16}, // off/on/airpurge
    {15, 45, 0xFF, CONV_ADD,     "WaterPumpSpeed",               65, 254,   1}, // PumpSpeedMax
    {16, 84, 0xFF, CONV_ADD,     "HeatDelta",                     1,  15, 128},
    {17, 94, 0xFF, CONV_ADD,     "CoolDelta",                     1,  15, 128},
    {18, 99, 0xFF, CONV_ADD,     "DHWHeatDelta",                -15,  15, 128},
    {19, 98, 0xFF, CONV_ADD,     "DHWHeatupTime",                 5, 240,   1}, // minutes
    {20, 85, 0xFF, CONV_ADD,     "HeaterOnOutdoorTemp",         -20,  35, 128},
    {21, 83, 0xFF, CONV_ADD,     "HeatingOffOutdoorTemp",         5,  35, 128},
    {22, 72, 0xFF, CONV_ADD,     "SGReadyCapacity1Heat",          0, 254,   1},
    {23, 71, 0xFF, CONV_ADD,     "SGReadyCapacity1DHW",           0, 254,   1},
    {24, 74, 0xFF, CONV_ADD,     "SGReadyCapacity2Heat",          0, 254,   1},
    {25, 73, 0xFF, CONV_ADD,     "SGReadyCapacity2DHW",           0, 254,   1},
    {26, 97, 0xFF, CONV_ADD,     "DHWRoomMaxTime",                0, 254,   1}, // steps of 30 min
    // Zone 1 heating curve: two points, each an outside temperature paired
    // with the flow target at that point. Kept in sync from Node-RED so the
    // heatpump can run on its own curve if the cascade control is unavailable.
    // Read back via TOP29/TOP30/TOP32/TOP31 respectively.
    //
    // High/Low does NOT refer to the same axis in both pairs: Outside_* names
    // outside temperatures, Target_* names flow temperatures, and they cross
    // over - TargetHigh is the flow when it is COLD ("VL kalt"), TargetLow the
    // flow when it is WARM ("VL warm"). Measured 2026-08-20, see MQTT-Topics.md.
    {27, 75, 0xFF, CONV_ADD,     "Z1HeatCurveTargetHighTemp",    20,  55, 128},
    {28, 76, 0xFF, CONV_ADD,     "Z1HeatCurveTargetLowTemp",     20,  55, 128},
    {29, 77, 0xFF, CONV_ADD,     "Z1HeatCurveOutsideLowTemp",   -15,  15, 128},
    // -15..15, NICHT 15..35: an der Anlage ausgemessen. Werte ueber 15
    // werden still verworfen, unter -15 auf -15 geklemmt. Der frueher
    // angenommene Bereich lag komplett auf der falschen Seite - von 21
    // erlaubten Werten war genau einer gueltig.
    {30, 78, 0xFF, CONV_ADD,     "Z1HeatCurveOutsideHighTemp",  -15,  15, 128},
    // Zone 1 cooling curve, same idea. Read back via TOP72/TOP73/TOP75/TOP74.
    {31, 86, 0xFF, CONV_ADD,     "Z1CoolCurveTargetHighTemp",     5,  20, 128},
    {32, 87, 0xFF, CONV_ADD,     "Z1CoolCurveTargetLowTemp",      5,  20, 128},
    // 15..30, Untergrenze berichtigt 2026-08-25: die Klemm-Messung vom
    // 2026-08-10 hatte hier 20 ergeben, das Bedienterminal nennt aber 15 -
    // und genau 15 nimmt die WP am Terminal auch an. Bis 3.14.1 wies die
    // Firmware den Wert selbst ab, bevor das Kommando zur WP ging.
    {33, 88, 0xFF, CONV_ADD,     "Z1CoolCurveOutsideLowTemp",    15,  30, 128},
    // 15..30, ebenfalls ausgemessen: darueber klemmt die WP auf 30,
    // darunter auf 15 - beides ohne jede Rueckmeldung. Genau die Art
    // stiller Verluste, die mit 3.1.0 beseitigt wurde.
    {34, 89, 0xFF, CONV_ADD,     "Z1CoolCurveOutsideHighTemp",   15,  30, 128},
    // Betriebsart je Kreis: 0 = Kompensationskurve, 1 = Direktvorgabe.
    // Byte 28 traegt BEIDE Betriebsarten in zwei Bitfeldern - die Masken sind
    // hier deshalb Pflicht, nicht Kosmetik: ohne sie schaltete ein Kuehl-
    // Kommando die Heizung mit um. Rueckgelesen ueber TOP76 Heating_Mode
    // (getBit7and8) und TOP81 Cooling_Mode (getBit5and6); die Kodierung ist
    // aus genau diesen Dekodierern zurueckgerechnet:
    //   HeatingMode 0 -> (0+1)*1 = 0b01, gelesen (0b01 & 0b11) - 1     = 0
    //   HeatingMode 1 -> (1+1)*1 = 0b10, gelesen (0b10 & 0b11) - 1     = 1
    //   CoolingMode 0 -> (0+1)*4 = 0b0100, gelesen ((4>>2) & 0b11) - 1 = 0
    //   CoolingMode 1 -> (1+1)*4 = 0b1000, gelesen ((8>>2) & 0b11) - 1 = 1
    // ACHTUNG, NICHT FOLGENLOS UMKEHRBAR: Ein Wechsel von Direkt auf Kurve
    // setzt die vier Kurvenpunkte des betroffenen Kreises auf die Panasonic-
    // Werksvorgaben zurueck, und das Zurueckschalten stellt sie NICHT wieder
    // her. Ausserdem uebernimmt der Direktsollwert dabei den unteren
    // Kurvenpunkt. Wer hierher schreibt, muss die Kurve danach aus dem
    // ioBroker nachziehen (test/kurven_sync.py) - siehe test/README.md.
    {35, 28, 0x03, CONV_MUL_INC, "HeatingMode",                   0,   1,   1},
    {36, 28, 0x0C, CONV_MUL_INC, "CoolingMode",                   0,   1,   4},
    // Heizstab (3.17.0). Byte 9 traegt BEIDE Freigaben in zwei Bitfeldern, die
    // Masken sind hier deshalb Pflicht: ohne sie legte ein Raumheiz-Kommando die
    // Warmwasser-Freigabe mit um. Rueckgelesen ueber TOP59 Room_Heater_State
    // (getBit7and8) und TOP58 DHW_Heater_State (getBit5and6), Klartext
    // Blocked/Free; die Kodierung ist aus genau diesen Dekodierern
    // zurueckgerechnet:
    //   RoomHeaterState 0 -> (0+1)*1 = 0b0001, gelesen (0b01 & 0b11) - 1   = 0
    //   RoomHeaterState 1 -> (1+1)*1 = 0b0010, gelesen (0b10 & 0b11) - 1   = 1
    //   DHWHeaterState  0 -> (0+1)*4 = 0b0100, gelesen ((4>>2) & 0b11) - 1 = 0
    //   DHWHeaterState  1 -> (1+1)*4 = 0b1000, gelesen ((8>>2) & 0b11) - 1 = 1
    //
    // FREI HEISST NICHT AN. Die Freigabe ist Bedingung (a) von sechs, die
    // uebrigen fuenf entscheidet die Waermepumpe selbst: 30 min seit Kompressor
    // thermo-on, 9 min seit Pumpenstart, Aussentemperatur unter SET20
    // HeaterOnOutdoorTemp, Vorlauf 4 K unter Soll, 20 min seit dem letzten
    // Heizstab-Aus. Eine Steuerung muss deshalb vorausschauend freigeben;
    // schnelles Ein/Aus bringt nichts. Ausfuehrlich in Vorhaben-HeaterSet.md.
    {37,  9, 0x03, CONV_MUL_INC, "RoomHeaterState",               0,   1,   1}, // blockiert=1 frei=2
    {38,  9, 0x0C, CONV_MUL_INC, "DHWHeaterState",                0,   1,   4}, // blockiert=4 frei=8
    // ForceHeater ist ein ZUSTAND, kein Impuls wie SET12 ForceDefrost - wer ihn
    // setzt, muss ihn auch zuruecknehmen. UND ER STARTET DIE UMWAELZPUMPE: am
    // 2026-08-28 gemessen laeuft sie an, sobald TOP68 aktiv wird, laeuft weiter,
    // nachdem der Heizstab abgeschaltet hat, und stoppt erst mit ForceHeater 0.
    // Ein vergessenes Kommando laesst also die Pumpe dauerhaft laufen. Byte 5
    // teilt er sich mit SET2
    // HolidayMode (0x30), rueckgelesen ueber TOP68 Force_Heater_State
    // (Inactive/Active). Das Servicehandbuch (12.9) beschreibt ihn als
    // Ersatzwaermequelle bei einer Stoerung der Waermepumpe; am 2026-08-28 am
    // Bedienpanel gegengeprueft: bei AUSGESCHALTETER Waermepumpe laesst er sich
    // auch ohne anliegende Stoerung einschalten, bei laufendem Betrieb lehnt das
    // Bedienteil die Anforderung ab.
    {39,  5, 0x0C, CONV_MUL_INC, "ForceHeater",                   0,   1,   4}, // aus=4 an=8
    // AltExternalSensor (3.23.0). Installer-Einstellung: On schaltet den
    // eingebauten Aussenfuehler ab, der externe Fuehler auf dem Flachdach gilt
    // dann fuer alle Regelvorgaenge - im Sommer realistischer (kein Sonnen-
    // einfluss), im Winter ist der Gehaeusefuehler fuer die Abtausteuerung
    // aussagekraeftiger. On setzt einen angeschlossenen externen Fuehler voraus
    // (Owner-Entscheid E1, 2026-09-19: an beiden Stufen vorhanden).
    // Umgeschaltet wird nur noch ueber diesen ioBroker-Datenpunkt, nicht mehr
    // am Bedienteil: Der ioBroker spielt beim Verbinden den gespeicherten Wert
    // wieder ein (SUBSCRIBE_GRACE, 3.6.1), der Datenpunkt muss deshalb immer
    // den gewollten Zustand tragen. Der Notbetrieb fasst diese Einstellung
    // nicht an (Owner-Entscheid E3).
    // Rueckgelesen ueber TOP112 Alt_External_Sensor (getBit3and4), Klartext
    // Off/On; die Kodierung ist aus genau diesem Dekodierer zurueckgerechnet:
    //   AltExternalSensor 0 -> (0+1)*16 = 0x10, gelesen ((0x10>>4) & 0b11) - 1 = 0
    //   AltExternalSensor 1 -> (1+1)*16 = 0x20, gelesen ((0x20>>4) & 0b11) - 1 = 1
    {40, 20, 0x30, CONV_MUL_INC, "AltExternalSensor",             0,   1,  16}, // aus=16 an=32
};

inline constexpr unsigned int SETCOMMANDCOUNT = sizeof(setCommands) / sizeof(setCommands[0]);

/*****************************************************************************/
/* Die leere Vorlage des Kommandotelegramms                                  */
/*                                                                           */
/* Kopf F1 6C 01 10, danach lauter Nullen ("keine Aenderung" in jedem Feld). */
/* Bis 3.25.0 stand sie zweimal im RAM: als 110-Byte-Initialisierer von      */
/* mainCommand (HeishaMon.cpp) und als cleanCommand, das setup() zur         */
/* Laufzeit daraus kopierte (C5c). Jetzt gibt es sie einmal, im Flash;       */
/* mainCommand startet leer und wird in setup() daraus gefuellt, und         */
/* kommando_zuruecksetzen() stellt sie nach jedem Senden wieder her.         */
/*****************************************************************************/
inline constexpr uint8_t KOMMANDO_VORLAGE[QUERYSIZE] = {0xF1, 0x6C, 0x01, 0x10};

// Puffer und beanspruchte Bits zugleich zuruecksetzen - beides gehoert
// zusammen (usedMask beschreibt, was in mainCommand schon steht). nullptr
// wird nicht angefasst; Rueckgabe false, wenn einer fehlt.
inline bool kommando_zuruecksetzen(uint8_t *befehl, uint8_t *belegt)
{
  if (!befehl || !belegt)
    return false;
  memcpy(befehl, KOMMANDO_VORLAGE, QUERYSIZE);
  memset(belegt, 0, QUERYSIZE);
  return true;
}

/*****************************************************************************/
/* Nachschlagen, Umrechnen, Anwenden                                         */
/*****************************************************************************/

// Zeichenketten in constexpr vergleichen (strcmp ist es nicht)
constexpr bool setcommand_namen_gleich(const char *a, const char *b)
{
  if (!a || !b)
    return false;
  while (*a && *a == *b)
  {
    a++;
    b++;
  }
  return *a == *b;
}

// Zeile zum Topic-Namen (der Teil hinter "<prefix>/set/"), sonst nullptr.
// setCommands[] ist die einzige Tabelle der Set-Topics.
inline const SetCommand *setcommand_finden(const char *name)
{
  if (!name)
    return nullptr;
  for (unsigned int i = 0; i < SETCOMMANDCOUNT; i++)
  {
    if (setcommand_namen_gleich(setCommands[i].name, name))
      return &setCommands[i];
  }
  return nullptr;
}

// Liegt der Wert im erlaubten Bereich der Zeile? Das ist die Bereichspruefung
// aus build_heatpump_command(); anwenden() verlangt sie ebenfalls.
constexpr bool setcommand_wert_ok(const SetCommand &cmd, int32_t wert)
{
  return wert >= cmd.min && wert <= cmd.max;
}

// Rohwert VOR dem Abschneiden auf ein Byte, -1 bei ungueltiger Umrechnung.
// Getrennt vom Abschneiden, damit die Tabellenpruefung einen Wert erkennt, der
// in 0..255 nicht mehr hineinpasst - vor 3.25.1 wurde er still beschnitten.
constexpr int setcommand_roh(const SetCommand &cmd, int wert)
{
  switch (cmd.conv)
  {
  case CONV_ADD:
    return wert + cmd.param;
  case CONV_MUL:
    return wert * cmd.param;
  case CONV_MUL_INC:
    return (wert + 1) * cmd.param;
  case CONV_OPMODE:
    // Index gegen die Tabellengroesse: ausserhalb gibt es keinen Rohwert
    return (wert >= 0 && wert < OPMODE_ANZAHL) ? (int)opModeBytes[wert] : -1;
  }
  return -1;
}

// Der Wert, der ins Telegramm kommt: Rohwert auf ein Byte, wie bisher
// (set_byte war ein byte). Voraussetzung: setcommand_wert_ok().
constexpr uint8_t setcommand_kodieren(const SetCommand &cmd, int wert)
{
  return (uint8_t)setcommand_roh(cmd, wert);
}

enum SetAnwendung : uint8_t
{
  SETANWENDEN_OK = 0,   // Feld eingesetzt, kein anderes Feld im Weg
  SETANWENDEN_KONFLIKT, // eingesetzt, aber ein Feld mit denselben Bits stand schon im Puffer
  SETANWENDEN_ABGELEHNT // nichts geaendert: Wert ausserhalb des Bereichs, nullptr oder pos ausserhalb des Puffers
};

/*****************************************************************************/
/* Ein Feld bitgenau in den Befehlspuffer einsetzen                          */
/*                                                                           */
/* Mehrere Set-Topics teilen sich ein Protokollbyte, jedes mit eigener Maske */
/* (siehe Kommentar an der Tabelle). Das ganze Byte zu schreiben, loeschte   */
/* alle anderen Felder darin - zwei Kommandos im selben 500-ms-Fenster       */
/* hoben sich auf. Deshalb: erst die eigenen Bits loeschen, dann den Wert    */
/* durch die Maske einsetzen. Das Loeschen ist es auch, was ein wiederholtes */
/* Kommando sein eigenes altes Ergebnis ueberschreiben laesst statt es zu    */
/* ver-ODERn.                                                                */
/*                                                                           */
/* belegt[] (usedMask) merkt, welche Bits seit dem letzten Senden schon      */
/* beansprucht wurden. Ueberlappt das neue Feld damit, ist das ein           */
/* Konflikt: Zwei verschiedene Felder wollten dieselben Bits, "der Letzte    */
/* gewinnt" - wie bisher, aber nicht mehr still. Dasselbe Feld zweimal       */
/* setzen meldet ebenfalls KONFLIKT (die Bits waren beansprucht); die        */
/* Firmware behandelt beides gleich und warnt.                               */
/*****************************************************************************/
inline SetAnwendung setcommand_anwenden(const SetCommand &cmd, int32_t wert, uint8_t *befehl, uint8_t *belegt)
{
  // Bounds: nichts schreiben, wenn der Aufrufer Unfug uebergibt
  if (!befehl || !belegt || cmd.pos >= QUERYSIZE || !setcommand_wert_ok(cmd, wert))
    return SETANWENDEN_ABGELEHNT;

  const uint8_t set_byte = setcommand_kodieren(cmd, (int)wert);
  const bool konflikt = (belegt[cmd.pos] & cmd.mask) != 0;
  belegt[cmd.pos] |= cmd.mask;
  befehl[cmd.pos] = (uint8_t)((befehl[cmd.pos] & ~cmd.mask) | (set_byte & cmd.mask));
  return konflikt ? SETANWENDEN_KONFLIKT : SETANWENDEN_OK;
}

/*****************************************************************************/
/* Tabellenpruefung zur Uebersetzungszeit                                    */
/*                                                                           */
/* Die Regeln, die eine Zeile erfuellen muss, damit ihr Kommando wirkt:      */
/*                                                                           */
/*   NAME        Name vorhanden, Nummer > 0, beide in der Tabelle einmalig   */
/*   POSITION    pos im Telegramm: 4..QUERYSIZE-1 (Bytes 0..3 sind der Kopf) */
/*   MASKE       Maske ungleich 0                                            */
/*   BEREICH     min <= max, hoechstens 256 Werte (begrenzt die Rechenzeit)  */
/*   OPMODE      bei CONV_OPMODE liegt der Bereich in 0..6                   */
/*   ROHWERT     fuer JEDEN Wert von min bis max liegt der Rohwert in 0..255 */
/*               (sonst wuerde er beim Abschneiden still verfaelscht)        */
/*   AUSSERHALB  und er liegt vollstaendig innerhalb der Maske - sonst       */
/*               schneidet die Maske Bits ab, im Extremfall alle: das ist    */
/*               der Fall "64 statt 16 bei SET40"                            */
/*   UEBERLAPP   zwei Zeilen am selben Byte teilen keine Maskenbits, ausser  */
/*               dem dokumentierten Paar QuietMode/PowerfulMode auf Byte 7   */
/*               (beide 0xFF, ein Protokollmerkmal, siehe Tabellenkommentar) */
/*                                                                           */
/* Das Ergebnis nennt Regel und Zeile; die Hosttests drucken beides. Beim    */
/* Uebersetzen sagt static_assert nur, DASS eine Regel bricht - welche und   */
/* wo, zeigt ./test/hosttests.sh (setcommands_test, Fall 1).                 */
/*****************************************************************************/
enum SetPruefRegel : uint8_t
{
  SETPRUEF_OK = 0,
  SETPRUEF_NAME,
  SETPRUEF_POSITION,
  SETPRUEF_MASKE,
  SETPRUEF_BEREICH,
  SETPRUEF_OPMODE,
  SETPRUEF_ROHWERT,
  SETPRUEF_AUSSERHALB,
  SETPRUEF_UEBERLAPP
};

struct SetPruefung
{
  SetPruefRegel regel; // SETPRUEF_OK, wenn alles stimmt
  unsigned int zeile;  // Index in der Tabelle; nur gueltig, wenn regel != SETPRUEF_OK
};

// Byte, auf dem sich QuietMode (SET3) und PowerfulMode (SET4) ueberlappen duerfen
inline constexpr uint8_t SETPRUEF_UEBERLAPP_BYTE = 7;

constexpr SetPruefung setcommands_pruefen(const SetCommand *tab, unsigned int n)
{
  if (!tab)
    return {SETPRUEF_NAME, 0};

  for (unsigned int i = 0; i < n; i++)
  {
    const SetCommand &c = tab[i];

    // NAME: vorhanden, nicht leer, Nummer und Name einmalig
    if (!c.name || c.name[0] == '\0' || c.number == 0)
      return {SETPRUEF_NAME, i};
    for (unsigned int j = 0; j < i; j++)
    {
      if (tab[j].number == c.number || setcommand_namen_gleich(tab[j].name, c.name))
        return {SETPRUEF_NAME, i};
    }

    // POSITION und MASKE
    if (c.pos < 4 || c.pos >= QUERYSIZE)
      return {SETPRUEF_POSITION, i};
    if (c.mask == 0)
      return {SETPRUEF_MASKE, i};

    // BEREICH, bevor darueber gerechnet wird
    if (c.min > c.max || (long long)c.max - (long long)c.min >= 256)
      return {SETPRUEF_BEREICH, i};
    if (c.conv == CONV_OPMODE && (c.min < 0 || c.max >= OPMODE_ANZAHL))
      return {SETPRUEF_OPMODE, i};

    // ROHWERT und AUSSERHALB: jeder erlaubte Wert, nicht nur die Raender -
    // eine Tabelle mit Luecken (OperationMode) ist nicht linear
    for (int v = c.min; v <= c.max; v++)
    {
      const int roh = setcommand_roh(c, v);
      if (roh < 0 || roh > 255)
        return {SETPRUEF_ROHWERT, i};
      if ((roh & ~(int)c.mask & 0xFF) != 0)
        return {SETPRUEF_AUSSERHALB, i};
    }

    // UEBERLAPP gegen alle frueheren Zeilen
    for (unsigned int j = 0; j < i; j++)
    {
      if (tab[j].pos == c.pos && (tab[j].mask & c.mask) != 0)
      {
        const bool erlaubt = (c.pos == SETPRUEF_UEBERLAPP_BYTE) && tab[j].mask == 0xFF && c.mask == 0xFF;
        if (!erlaubt)
          return {SETPRUEF_UEBERLAPP, i};
      }
    }
  }
  return {SETPRUEF_OK, 0};
}

// Ein Tippfehler in der Tabelle bricht hier den Build, nicht erst die Anlage.
// SETCOMMANDS_OHNE_STATIC_ASSERT definiert nur test/setcommands_test.cpp: Es
// soll bei einer kaputten Tabelle UEBERSETZEN koennen, um Regel und Zeile zu
// nennen (Fall 1). Jeder andere Einbinder - Firmware und die uebrigen Tests -
// bricht am static_assert.
#ifndef SETCOMMANDS_OHNE_STATIC_ASSERT
static_assert(setcommands_pruefen(setCommands, SETCOMMANDCOUNT).regel == SETPRUEF_OK,
              "setCommands[] verletzt eine Tabellenregel - ./test/hosttests.sh nennt Regel und Zeile (setcommands_test, Fall 1)");
#endif
