#pragma once
#include <PubSubClient.h>

// returns true if a command was registered, false on any error
bool build_heatpump_command(char *, char *);

// abonniert alle Set-Topics aus setCommands[] (commands.cpp) - die Tabelle ist
// die einzige Liste der Set-Topics. false, wenn ein subscribe fehlschlug.
bool subscribe_set_topics(PubSubClient &);
