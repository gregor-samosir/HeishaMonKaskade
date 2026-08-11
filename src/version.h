#pragma once
// Changelog:
// 3.4.0 - Zone 2 entfernt. Diese Anlagen haben keine Zone 2, die Topics
//         trugen also nur dekodiertes Rauschen und legten im ioBroker
//         13 Objekte an, die niemand deuten kann.
//         Weg sind: TOP34, TOP35, TOP37, TOP43, TOP57 und TOP82-89 sowie
//         die Set-Kommandos SET7/SET8 (Z2HeatRequestTemperature,
//         Z2CoolRequestTemperature; in Node-RED nie benutzt, vom Betreiber
//         bestaetigt). NUMBEROFTOPICS 99 -> 86.
//         Die Nummerierung hat dadurch LUECKEN, und das ist Absicht: Dank
//         'number' als Datenfeld (s. 3.3.0) behaelt jedes verbliebene Topic
//         seine bisherige Nummer. TOP36 heisst weiter TOP36 - in
//         MQTT-Topics.md, in alten Mitschnitten und im Original-Projekt.
//         Bitte nicht durchnummerieren. Entsprechende Warnhinweise stehen
//         ueber beiden Tabellen (decode.cpp, commands.cpp).
//         Nachweis mit test/decode_vergleich.py --entfallen Z2_ gegen den
//         Stand vor 3.3.0: 65016 Zeilen identisch, 86 Topics, exakt die 13
//         Zone-2-Topics fehlen und sonst nichts.
//         NACH dem Flashen beider Stufen: test/retained_loeschen.py (neu)
//         ausfuehren. Die Firmware publiziert mit Retain-Flag, der Broker
//         liefert die 13 alten Werte sonst weiter an jeden neuen Abonnenten
//         aus - das Topic verschwindet nicht, es friert ein.
//         Groessen gegenueber 3.3.0: ESP8266 RAM -960 B, Flash -824 B;
//         ESP32 RAM -256 B, Flash -872 B.
// 3.3.0 - Die vier positionsgleichen State-Topic-Tabellen in decode.cpp durch
//         EINE Tabelle ersetzt (struct StateTopic): Name, Quellbyte,
//         Dekodierer und Einheit eines Topics stehen jetzt in einer Zeile
//         statt ueber topicNames/topicBytes/topicFunctions/topicDescription
//         verteilt, die nur ueber die Position und einen // TOPn-Kommentar
//         zusammenhingen. Die 99 States::TOPn-Deklarationen in Topics.h/.cpp
//         entfallen ersatzlos, die Namen stehen in der Tabelle.
//         REIN INTERNER UMBAU - keine Verhaltensaenderung: gleiche Topics,
//         gleiche Namen, gleiche Werte (Nachweis s. unten).
//         Zwei Fallen sind dabei strukturell verschwunden:
//         - 'number' ist ein DATENFELD, nicht der Array-Index. Zeilen koennen
//           entfallen, ohne dass sich die TOP-Nummern der uebrigen
//           verschieben - die Nummern stehen so in MQTT-Topics.md.
//         - getTopicPayload entschied vorher per switch ueber fest
//           verdrahtete TOP-Nummern (case 44:, case 90: ...), welches Topic
//           mehrere Bytes braucht. Jede Verschiebung der Nummerierung haette
//           diese Marken stillschweigend auf andere Topics zeigen lassen, und
//           zwar OHNE Compilerfehler. Jetzt bringt die Zeile ihren
//           Dekodierer selbst mit.
//         Ausserdem: unknown() entfernt (war nur Platzhalter fuer die
//         Mehrbyte-Topics und wurde nie aufgerufen), Bereichspruefung in
//         getTopicPayload, sprintf -> snprintf im Publish-Log.
//         Nachweis mit test/decode_vergleich.py: alter und neuer Stand auf
//         dem Mac uebersetzt und mit denselben 756 Telegrammen gefuettert
//         (jeder Bytewert 0..255 plus 500 Pseudozufallstelegramme) -
//         74844 Zeilen aus Nummer, Name, Wert und Einheit identisch.
//         Groessen: ESP8266 RAM +296 B (const-Tabellen liegen dort im RAM;
//         die Feldreihenfolge im struct ist deshalb auf wenig Padding
//         ausgelegt), Flash -712 B. ESP32 RAM -1184 B, Flash -176 B.
// 3.2.2 - Wertebereiche der beiden Kurven-OutsideHigh-Parameter an der
//         Anlage ausgemessen statt aus Quellen uebernommen:
//           Z1HeatCurveOutsideHighTemp   war 15..35 -> jetzt -15..15
//           Z1CoolCurveOutsideHighTemp   war 20..30 -> jetzt  15..30
//         Der Heiz-Bereich lag komplett auf der falschen Seite: gueltig
//         ist alles BIS 15, nicht AB 15 - von 21 erlaubten Werten war
//         genau einer gueltig, und das fiel nur auf, weil die
//         Konfiguration zufaellig exakt diesen einen nutzt.
//         Die WP klemmt ausserhalb liegende Werte kommentarlos auf den
//         jeweiligen Rand (gemessen: -20 -> -15, 20/25/30/35 -> 15,
//         10 -> 15, 31/32/35/40 -> 30). Ohne die Korrektur haette die
//         Firmware solche Werte weitergereicht und sie waeren lautlos
//         verschwunden.
//         Werkzeug dafuer: test/kurven_grenzen.py. Recherche vorab ergab,
//         dass das Original-HeishaMon-Projekt ueberhaupt keine
//         Bereichspruefung kennt (cmd[75] = wert + 128 ungefiltert) - die
//         verbreiteten Bereiche stammen also nicht von dort.
// 3.2.1 - Wertebereich von SET34 (Z1CoolCurveOutsideHighTemp) von 30-40
//         auf 20-30 korrigiert. Die Waermepumpe klemmt jeden Wert
//         darueber still auf 30 - an beiden Geraeten gemessen, 31/32/35
//         und 40 kamen alle als 30 zurueck, ohne Fehlermeldung der WP.
//         Vorher haette die Firmware solche Werte anstandslos
//         weitergereicht und sie waeren lautlos verschwunden.
//         Geprueft und ausgeschlossen: der Decoder klemmt nichts, alle
//         Kurven-Topics nutzen getIntMinus128 (reine Subtraktion), im
//         gesamten decode.cpp gibt es keine Begrenzung. Der Wert 30
//         steht also wirklich so in der Waermepumpe.
// 3.2.0 - Acht neue Set-Kommandos fuer die Zone-1-Heiz- und -Kuehlkurve
//         (SET27-SET34, Byte 75-78 und 86-89). Zweck: Notbetrieb bei
//         Ausfall der Node-RED-Kaskadensteuerung. Die Kurvenwerte
//         werden dort gepflegt und bei jeder Anpassung in die WPs
//         geschrieben; faellt die Steuerung aus, wird am Bedienterminal
//         von Direkt- auf Kurvenbetrieb umgeschaltet und die Anlage
//         laeuft mit denselben Werten weiter.
//         Alle acht Bytes waren frei - keine Feldkonflikte.
//         MQTT-Topics.md nachgezogen: enthielt nur SET1-SET19, dazu
//         veraltete Topic-Namen (SetHeatpump statt set/Heatpump) und
//         vertauschte Beschreibungen bei TOP31/TOP32 und TOP74/TOP75
//         (Outside_High ist die HOEHERE Aussentemperatur, an der Anlage
//         geprueft). Jetzt vollstaendig inkl. Byte-Spalte.
// 3.1.1 - Web-Tabelle wird in TCP-grossen Bloecken statt zeilenweise
//         gesendet. Ein sendContent() pro Zeile kostete auf dem ESP32
//         je einen Netzwerk-Roundtrip (~20 ms), 99 Zeilen also ~1,9 s
//         sichtbares "... Loading ...". Der ESP8266-Core buendelt
//         Schreibvorgaenge selbst und zeigte den Effekt nie. Jetzt
//         werden die Zeilen in einem 1400-Byte-Puffer gesammelt
//         (knapp unter der TCP-MSS) - aus 99 Sendevorgaengen werden
//         etwa sechs. Reine Anzeige-Optimierung, der Waermepumpen-
//         Pfad ist nicht betroffen (nachgemessen: HTTP-Auslieferung
//         stoerte den 6-s-Abfragetakt auch vorher nicht).
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
static const char* heishamon_version = "3.4.0";
