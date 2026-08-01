#pragma once

// sentinel: no set topic matched, mainCommand must not be written
#define COMMANDPOS_UNSET 0xFF

// returns true if a command was registered, false on any error
bool build_heatpump_command(char *, char *);
