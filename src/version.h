#pragma once
// Changelog:
// 2.0.1 - Bugfixes: uninitialisiertes set_pos/set_byte bei unbekanntem Set-Topic,
//         Query-Zyklus blieb nach ungueltigem MQTT-Wert stehen,
//         Bounds-Check fuer den seriellen Empfangspuffer
// 2.0.0 - Stand vor Bugfix-Session (Tag: rettungsanker-2026-08-01)
static const char* heishamon_version = "2.0.1";
