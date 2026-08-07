#pragma once
// Changelog:
// 3.1.0 - Set-Kommandos werden bitgenau in den Puffer gemischt statt
//         byteweise zugewiesen: mehrere SET-Topics, die sich ein
//         Protokollbyte teilen (Byte 4 Heatpump/WaterPump/ForceDHW,
//         Byte 8 ForceDefrost/ForceSterilization), loeschten sich
//         bisher gegenseitig, wenn sie im selben 500-ms-Fenster
//         eintrafen - genau der Fall beim 5-min-Re-Assert der
//         Node-RED-Kaskade (6 SETs pro WP gleichzeitig).
//         Neue Maskenspalte in setCommands, Konflikt-Warnung im Log.
//         Ausnahme Byte 7 (QuietMode/PowerfulMode ueberlappen im
//         Protokoll selbst): Verhalten unveraendert, nur Warnung.
//         Zusaetzlich: explizites setDataPending-Flag statt der
//         Bytesummen-Heuristik calculate_commandset() - die konnte
//         "leerer Puffer" nicht von "Summe auf 256 umgeschlagen"
//         unterscheiden und verwarf still ganze Kommandotelegramme
//         (betraf auch SetForceDefrost 0 / SetForceSterilization 0).
// 3.0.1 - ESP32: WiFi-Modem-Sleep deaktiviert (eingehende Verbindungen),
//         OTA-Env heishamon_esp32_ota, OTA-Weg verifiziert
// 3.0.0 - ESP32-S3-Port: eine Codebasis fuer D1 mini (ESP8266) und das
//         offizielle HeishaMon-ESP32-Board. Plattformschicht in
//         HeishaMon.h, Waermepumpe auf ESP32 an eigener Serial1
//         (RX18/TX17), USB-Debug parallel, serial_data auf uint8_t,
//         Env heishamon_esp32_usb (4MB Flash, min_spiffs)
// 2.3.1 - SUB-Log zeigt wie PUB nur noch den Topic-Namen statt des
//         kompletten MQTT-Pfads
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
static const char* heishamon_version = "3.1.0";
