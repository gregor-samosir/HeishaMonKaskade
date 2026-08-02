#pragma once
// Changelog:
// 2.3.0 - WiFiManager 0.16 -> 2.0.17 (RAM -3%, Strings in PROGMEM),
//         Decoder komplett String-frei: alle Topic-Funktionen schreiben
//         per snprintf/dtostrf in feste Puffer statt String-Objekte zu
//         allozieren (keine Heap-Allokationen mehr im 5s-Decode-Pfad)
// 2.2.0 - Groessere Umbauten: Set-Kommandos komplett tabellengetrieben
//         (setCommands), Web-UI ohne CDN (Inline-CSS, fetch statt jQuery),
//         actual_data als char-Array + Publish ohne Heap-Allokationen,
//         Out-of-bounds-Read bei topicDescription[-1] behoben,
//         ArduinoJson auf 6.21 (bewusst nicht 7, ESP8266-Footprint)
// 2.1.0 - Wertebereichs-Validierung fuer alle Set-Kommandos (negative Shifts
//         jetzt erlaubt), NTP-Timeout statt Endlosschleife, korrekte
//         Sommer-/Winterzeit, mDNS-Fehler nicht mehr fatal, HTTP-Auth fuer
//         /settings /reboot /togglelog /toggledebug, MQTT-Passwort nicht mehr
//         im HTML, pragma once + Lookup-Tabellen einmalig in decode.cpp
// 2.0.1 - Bugfixes: uninitialisiertes set_pos/set_byte bei unbekanntem Set-Topic,
//         Query-Zyklus blieb nach ungueltigem MQTT-Wert stehen,
//         Bounds-Check fuer den seriellen Empfangspuffer
// 2.0.0 - Stand vor Bugfix-Session (Tag: rettungsanker-2026-08-01)
static const char* heishamon_version = "2.3.0";
