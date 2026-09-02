#pragma once
#include <string>

/*****************************************************************************/
/* Die feststehenden MQTT-Pfade dieser Stufe                                 */
/*                                                                           */
/* Hier stehen nur noch die Pfad-WURZELN. Die Namen der einzelnen Topics      */
/* stehen in den Tabellen, die sie ohnehin beschreiben:                       */
/*   state/<Name>  ->  stateTopics[] in decode.cpp                            */
/*   set/<Name>    ->  setCommands[] in commands.cpp                          */
/* Bis 3.4.1 gab es hier zusaetzlich 32 Konstanten Topics::SET1..SET34, die   */
/* jeden Set-Topic-Namen ein zweites Mal fuehrten - siehe commands.cpp.       */
/*****************************************************************************/
class Topics
{
public:
    static const std::string STATE; // <prefix>/state  - Wurzel der Messwerte
    static const std::string SET;   // <prefix>/set    - Wurzel der Set-Kommandos
    static const std::string LOG;
    static const std::string WILL;

    // <prefix>/info - Wurzel der Langzeit-Telemetrie (3.20.0, M3 der
    // Codedurchsicht 2026-09-02). Auch hier steht nur die WURZEL: Die acht
    // Blattnamen haengt publish_info_*() in HeishaMon.cpp mit snprintf an,
    // wie es publish_heatpump_data() fuer die Messwerte tut. Acht weitere
    // std::string-Konstanten waeren acht Heap-Allokationen fuer Pfade, die
    // sich nie aendern.
    //
    // LOG und WILL liegen ebenfalls unter dieser Wurzel und stehen nur
    // deshalb als eigene Konstanten da, weil es sie schon vor ihr gab.
    static const std::string INFO;

    // <prefix>/notbetrieb - Werte, die die Firmware fuer den Notbetrieb im RAM
    // haelt und NIE von sich aus an die Waermepumpe schickt. Eigener Zweig und
    // nicht etwa ein Sonderfall unter set/, damit die Trennung schon am Topic
    // sichtbar ist: was hier hereinkommt, wird gemerkt, nicht ausgefuehrt.
    static const std::string NOTBETRIEB;
};
