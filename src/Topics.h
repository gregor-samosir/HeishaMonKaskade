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
};
