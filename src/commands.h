#pragma once
#include <PubSubClient.h>

// returns true if a command was registered, false on any error
bool build_heatpump_command(char *, char *);

// abonniert alle Set-Topics aus setCommands[] (commands.cpp) - die Tabelle ist
// die einzige Liste der Set-Topics. false, wenn ein subscribe fehlschlug.
bool subscribe_set_topics(PubSubClient &);

// Bereichsgrenzen eines Set-Kommandos nachschlagen. Gibt es fuer den
// Notbetrieb: Dessen Werte tragen dieselben Namen wie die Set-Kommandos und
// muessen denselben Bereich einhalten. Ohne diese Funktion braeuchte
// notbetrieb.h eine zweite Tabelle mit denselben Zahlen - und zwei Tabellen
// koennen auseinanderlaufen. false, wenn der Name nicht in setCommands[] steht.
bool set_command_range(const char *name, int *min_out, int *max_out);
