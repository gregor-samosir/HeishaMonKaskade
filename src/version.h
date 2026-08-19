#pragma once
// Changelog:
// 3.8.2 - Zwei Stellen im MQTT-Sendepfad, an denen ein Abonnent einen
//         veralteten oder falschen Zustand bekommt. Das sind Massnahme 2 und
//         Kleinpunkt K3 aus dem Massnahmenplan zur Codedurchsicht vom
//         2026-08-18. Gemeinsam in einer Version, weil es derselbe Fehler in
//         zwei Auspraegungen ist: der Broker haelt einen Wert fuer aktuell,
//         den das Geraet laengst ueberholt hat. Keine Aenderung am Protokoll,
//         an den Topics oder an der Dekodierung.
//
//         (1) LWT "ONLINE" JETZT RETAINED (Massnahme 2). mqtt_reconnect()
//         publizierte "Online" ohne Retain-Flag, waehrend das Will "Offline"
//         retained beim Broker liegt. Auf einem echten Broker bleibt damit
//         nach jedem Reconnect "Offline" als gespeicherter Wert stehen: wer
//         sich spaeter verbindet - Node-RED-Neustart, Kaskaden-Waechter -
//         bekommt beim Subscribe "Offline" geliefert und haelt die Stufe fuer
//         tot, obwohl sie laeuft. Der ioBroker-MQTT-Adapter verdeckt das
//         heute, weil er neue Abonnenten aus seiner State-DB bedient; der
//         Fehler ist also vorhanden, aber unsichtbar - und schlaegt genau dann
//         zu, wenn der Umzug auf einen mosquitto kommt.
//
//         (2) FEHLGESCHLAGENES STATE-PUBLISH WIRD WIEDERHOLT (K3).
//         publish_heatpump_data() schrieb den Wert in den Vergleichspuffer
//         actual_data und warf den Rueckgabewert von publish() weg. Schlug das
//         Senden fehl, galt der Wert trotzdem als gesendet. Der praktisch
//         relevante Fall ist nicht der einzelne verlorene Publish, sondern die
//         MQTT-Unterbrechung: die Schleife laeuft weiter und schreibt den
//         Puffer fort, nach dem Reconnect gilt jeder zwischenzeitlich
//         geaenderte Wert als gesendet - beim Broker steht bis zu 5 min lang
//         der alte, retained. Jetzt merkt sich eine Marke, dass etwas
//         liegenblieb, und der naechste Durchlauf (5 s) schickt die Tabelle
//         erneut. Eine Marke fuer die ganze Tabelle statt einer je Zeile:
//         ein Publish scheitert praktisch nur bei fehlender Verbindung, und
//         dann ist ohnehin alles betroffen (1 Byte RAM statt 90).
//         Die <PUB>-Zeile im Log bleibt an die echte Wertaenderung geknuepft -
//         sonst fuellt eine laufende Wiederholung das Log alle 5 s.
//
//         Nachweise, ohne Hardware:
//         - Alle zehn Envs gebaut, RAM/Flash-Delta siehe unten.
//         - Die vier Hosttests (merge, telegramm, sendwindow, byte110) laufen
//           unveraendert gruen.
//         - Bewusste Entscheidung: der Retain-Nachweis gegen einen echten
//           Broker (neuer Abonnent auf <prefix>/info/LWT muss retained
//           "Online" bekommen; Geraet stromlos -> nach Keepalive "Offline")
//           steht aus und wandert in die Vorbereitung des mosquitto-Umzugs -
//           hier gibt es keinen Broker, der Retain ueberhaupt zeigt.
//
//         RAM/Flash gegen 3.8.1, alle zehn Envs gebaut:
//         - ESP32 (sechs Envs):   RAM +/-0, Flash +40 B (1174085 -> 1174125)
//         - ESP8266 (vier Envs):  RAM +/-0, Flash +48 B (459015 -> 459063,
//           d1_mini_test 459023 -> 459071)
//         Die Marke ist ein static bool und geht auf beiden Plattformen in
//         vorhandenem Ausrichtungs-Verschnitt auf, deshalb 0 Byte RAM.
//
// 3.8.1 - Zwei offene Zugangswege dichtgemacht: der Setup-Hotspot bekommt WPA2,
//         der Telnet-Port verliert das Reboot-Kommando. Das sind Massnahme 1
//         und Kleinpunkt K1 aus dem Massnahmenplan zur Codedurchsicht vom
//         2026-08-18 (Massnahme 2, das retained "Online", ist bewusst NICHT
//         dabei - sie lohnt erst mit einem echten Broker). Keine Aenderung am
//         Protokoll, an den Topics oder an der Dekodierung.
//
//         (1) SETUP-AP MIT PASSWORT. wifiManager.autoConnect oeffnete das
//         Konfigurationsportal als OFFENEN AP - mit den echten Werten in den
//         Feldern, OTA- und MQTT-Passwort eingeschlossen. Das ist kein
//         Erstboot-Thema: der WLAN-Watchdog startet nach 5 min Ausfall neu,
//         autoConnect scheitert nach 10 s und macht dann fuer 180 s den AP
//         auf - zyklisch, solange ein Router-Ausfall dauert. In diesem Fenster
//         konnte jeder in Funkreichweite die Zugangsdaten mitlesen oder dem
//         Geraet einen fremden MQTT-Broker unterschieben, und zwar ausgerechnet
//         dann, wenn die Anlage ohnehin gestoert ist.
//         Das Passwort kommt als Build-Flag HEISHA_AP_PASSWORD aus
//         platformio_user_env.ini (neue Sektion [ap_defaults], nicht in git) -
//         gleiches Muster wie Ports, IPs und OTA-Passwort. Beide Board-Basen
//         binden die Sektion ein, das Flag gilt damit fuer alle zehn Envs. Im
//         Code steht ein Fallback wie bei HEISHA_HOSTNAME; er liegt im
//         oeffentlichen Repo und schuetzt entsprechend wenig, deshalb gibt der
//         Build eine #warning aus, wenn das Flag fehlt.
//         Zwei static_assert halten die WPA2-Grenzen (8 bis 63 Zeichen): ein zu
//         kurzes Passwort verwirft der WiFiManager STILL und macht den AP
//         wieder offen auf - das waere an der Waermepumpe nicht aufgefallen.
//
//         (2) TELNET STARTET NICHT MEHR NEU (K1). Port 23 hat keine Anmeldung,
//         der Web-/reboot dagegen schon. Ein Tastendruck 'R' konnte also ohne
//         jede Legitimation die Waermepumpe fuer die Dauer des Boots von der
//         Steuerung trennen. Die Taste bleibt belegt und verweist jetzt auf
//         http://<ip>/reboot, damit der Weg im Ernstfall nicht zu erraten ist.
//         Die Umschalter L/D/H und die Abfragen M/W/I bleiben unveraendert -
//         sie werden fuer die Abnahme nach dem Flashen gebraucht.
//
//         Nachweise, ohne Hardware:
//         - Gegenprobe zum static_assert: mit einem vierstelligen Passwort
//           bricht der Build ab ("braucht mindestens 8 Zeichen (WPA2)").
//         - Gegenprobe zum Fallback: ohne das Build-Flag uebersetzt es weiter,
//           gibt aber die #warning aus.
//         - pio project config: alle zehn Envs fuehren HEISHA_AP_PASSWORD.
//         - Die vier Hosttests (merge, telegramm, sendwindow, byte110) laufen
//           unveraendert gruen; angefasst wurde nichts, was sie pruefen.
//
//         AM GERAET NACHGEWIESEN 2026-08-18, D1 mini am USB (Env d1_mini_test,
//         Prefix panasonic_heat_pump_test, MQTT-Server bewusst leer - so
//         entstehen im ioBroker keine Testobjekte). Flash vorher komplett
//         geloescht, damit das Geraet garantiert ins Portal geht:
//         - Der AP "HeishaMon-Setup" verlangt ein Passwort und nimmt es an;
//           danach WLAN konfiguriert, Geraet unter 192.168.2.198 im Netz,
//           config.json gespeichert (Owner-Bestaetigung 07:34).
//         - Nebenbei bestaetigt sich der Befund selbst: das Portal lief in
//           Zyklen von 180 s ("config portal has timed out" -> "failed to
//           connect and hit timeout" -> Neustart -> "StartAP with SSID"). Genau
//           dieses Fenster stand vorher offen.
//         - Telnet 192.168.2.198:23: 'R' bringt die Hinweiszeile, die
//           Verbindung bleibt stehen, 'M' und 'I' antworten auf DERSELBEN
//           Verbindung weiter (Memory 81, localIP 192.168.2.198). Auf der
//           seriellen Konsole kam dabei keine einzige Zeile - bei einem
//           Neustart waeren die Bootmeldungen erschienen.
//         - Der Ersatzweg ist geprueft: /reboot ohne Login antwortet 401, mit
//           Login 200 und das Geraet startet neu (07:36:37) und kommt mit
//           gespeicherter Konfiguration wieder hoch. Web-UI meldet 3.8.1.
//
//         FOLGEAUFGABE ERLEDIGT (Owner-Bestaetigung 2026-08-18): Das
//         AP-Passwort steht in der Offline-Notfallliste. Es gehoert dorthin und
//         nicht hierher - ohne es ist bei WLAN-Ausfall genau der Rettungsweg
//         versperrt, und im oeffentlichen Repo hat es nichts zu suchen.
//
//         AUSGEROLLT 2026-08-18: beide Stufen per OTA auf 3.8.1
//         (heishamon_esp32_h1_ota 07:45, _h2_ota 07:48; beide Anlagen standen
//         zu dem Zeitpunkt still, Heatpump_State Off, Kompressor 0 Hz).
//         Abnahme nach dem ueblichen Verfahren, /tablerefresh vor und nach dem
//         Flash: je 90 Topics verglichen, Nummer und Name deckungsgleich, an
//         H1 null Abweichungen, an H2 nur Pump_Flow 16,24 -> 15,89 l/min und
//         DHW_Temp 43 -> 44 Grad - laufende Messwerte, keine Sollwerte. Die
//         kritischen Sollwerte (Quiet_Mode_Level, Z1_Heat_Request_Temp,
//         Z1_Heat_Curve_Target_High_Temp) stehen unveraendert, die
//         SUBSCRIBE_GRACE aus 3.6.1 hat also auch bei diesem Reboot gegriffen.
//         MQTT-Seite getrennt geprueft: beide Praefixe publizieren frisch
//         (ack=true, Zeitstempel unter einer Minute), info/LWT steht auf beiden
//         Stufen auf "Online". Telnet-Stichprobe an beiden Geraeten: 'R' bringt
//         die Hinweiszeile, 'M' antwortet danach weiter (Memory 75).
//         Rollback-Binaries liegen als heishamon_esp32_h[12]_ota_v3.8.1.bin
//         neben denen von 3.8.0.
//
//         Groessen gegenueber 3.8.0 (alle zehn Envs gebaut):
//         ESP32 RAM +-0 B, Flash +112 B; ESP8266 RAM +112 B, Flash +104 B.
//         Die 112 Byte RAM auf dem ESP8266 sind die neue Telnet-Hinweiszeile
//         und das AP-Passwort - Zeichenketten ohne PROGMEM liegen dort im RAM,
//         wie bei allen bestehenden Meldungen auch. Belegung danach:
//         ESP8266 55,9 % RAM, ESP32 17,4 %.
//
// 3.8.0 - Die drei offenen Punkte der Codedurchsicht vom 2026-08-12 abgeraeumt.
//         Keine Aenderung am Protokoll, an den Topics oder an der Dekodierung -
//         nur der Sendepfad. Nachweis: test/decode_vergleich.py --basis v3.7.0
//         meldet 68040 Zeilen ueber 90 Topics identisch.
//
//         (1) DECKEL FUERS SAMMELFENSTER. Jedes eintreffende SET stiess den
//         500-ms-Timer neu an, damit mehrere Felder in ein Telegramm wandern
//         (register_new_command). Kommen die SETs dichter als 500 ms, wurde das
//         Fenster damit unbegrenzt verlaengert - Senden UND Abfrage standen
//         still, und zwar ohne jede Logzeile. Auffallen wuerde es erst daran,
//         dass die Messwerte nicht mehr nachziehen. Ab COMMAND_WINDOW_MAX
//         (2000 ms) wird nicht mehr verlaengert; der laufende Timer feuert von
//         selbst, das Telegramm geht spaetestens 2499 ms nach dem ersten SET
//         raus. Das eintreffende Feld geht dabei nicht verloren - es steht zu
//         diesem Zeitpunkt schon in mainCommand und faehrt mit.
//         Am Normalbetrieb aendert sich nichts: der 5-min-Re-Assert der
//         Node-RED-Steuerung schickt seine sechs Kanaele im Millisekunden-
//         abstand, die landen weiter alle in einem Telegramm.
//
//         (2) serialquerysent WIRD VOR DEM SENDEN GEPRUEFT. Das Flag hiess
//         schon immer "mutex", wurde aber nur gelesen, nicht beachtet. Traf ein
//         SET 0 bis 500 ms nach dem Absenden einer Abfrage ein, feuerte der
//         Sammeltimer kurz VOR dem Lesetimer: send_pana_command warf ueber
//         flush_serial_input die bereits vollstaendig eingetroffene Antwort weg,
//         und die anschliessende Leserunde lief ins Leere ("Telegramm
//         verworfen", eine Messrunde verloren). Jetzt wird das Senden statt
//         dessen um COMMANDTIMER verschoben. timeout_serial gibt die Leitung
//         nach SERIALTIMEOUT (600 ms) in jedem Fall frei, das dauert also
//         hoechstens zwei Runden. COMMAND_DEFER_MAX (4) ist der Notausgang,
//         falls das Flag doch haengt: dann wird wie bisher gesendet und die
//         Warnung geht ins MQTT-Log - im Betrieb darf sie nie auftauchen.
//
//         (3) getLeft5bits ENTFERNT (decode.cpp/decode.h). Deklariert und
//         definiert, aber von keiner Tabellenzeile benutzt - toter Code seit
//         der Tabellenumstellung in 3.3.0. Owner-Entscheid 2026-08-13, beim
//         naechsten Umbau mitzunehmen. initialQuery bleibt dagegen bewusst
//         stehen, Begruendung steht als Kommentar im Code.
//
//         Die beiden Zeitregeln stehen in der neuen Datei src/sendwindow.h,
//         gleiches Muster wie src/telegram.h seit 3.6.0: Firmware und Hosttest
//         benutzen dieselbe Fassung, es gibt keine zweite Wahrheit. Sie rechnen
//         bewusst in uint32_t und nicht in unsigned long - auf beiden Chips ist
//         das dasselbe wie millis(), auf dem Mac waere unsigned long 64 Bit und
//         der Ueberlauftest wuerde gegen nichts pruefen.
//
//         Nachweis, ohne Hardware:
//         - test/sendwindow_test.cpp (neu, laeuft in der CI mit): Raender des
//           Deckels, Terminierung unter SET-Stroemen mit 1/100/400 ms Abstand,
//           die zugesagte Obergrenze ueber alle Abstaende von 1 bis 3000 ms,
//           der millis()-Ueberlauf nach 49,7 Tagen samt Gegenprobe gegen die
//           naive Schreibweise (now < start + limit), und dass das
//           Verschiebefenster laenger ist als SERIALTIMEOUT. Gegenprobe
//           gemacht: mit ungedeckeltem Fenster faellt der Test mit 10
//           Abweichungen durch.
//         - Beim Aufschreiben der Zusicherung fiel auf, dass die Obergrenze
//           2499 ms betraegt und nicht 2500: der letzte Anstoss kann hoechstens
//           bei COMMAND_WINDOW_MAX - 1 liegen. Im Test steht jetzt der
//           hergeleitete Wert.
//
//         WAS DER HOSTTEST NICHT ABDECKT: die Zustandsuebergaenge in
//         HeishaMon.cpp selbst (Ticker, Serial, Flags) sind nicht ohne die
//         halbe Arduino-Welt uebersetzbar. Beim Abnahmetest ist deshalb auf
//         zwei Logzeilen zu achten - "Lesefenster laeuft noch, Senden
//         verschoben" darf vereinzelt nach einem SET auftreten (das ist der
//         behobene Fall), "Sammelfenster am Deckel" und die erzwungene
//         Warnung duerfen im Normalbetrieb gar nicht kommen.
//
//         Groessen gegenueber 3.7.0 (alle 10 Envs gebaut):
//         ESP32 RAM +16 B, Flash +368 B; ESP8266 RAM +192 B, Flash +352 B.
//         Die 192 Byte auf dem ESP8266 sind fast vollstaendig die drei neuen
//         Logtexte - Zeichenketten ohne PROGMEM liegen dort im RAM, wie bei
//         allen bestehenden Meldungen auch. Belegung danach: ESP8266 55,8 %
//         RAM, ESP32 17,4 %.
//
// 3.7.0 - Vier neue State-Topics aus Byte 110: die IST-Zustaende der
//         Waermepumpe (TOP99 Quiet_Mode_Active, TOP100 Powerful_Mode_Active,
//         TOP101 Heat_Cool_SW_State, TOP102 External_SW_State).
//         NUMBEROFTOPICS 86 -> 90. Keine Aenderung an bestehenden Topics, am
//         Protokoll oder an der Set-Seite - es wird nur ein bisher
//         undekodiertes Byte des Antworttelegramms mit ausgewertet.
//
//         Wozu: TOP101 meldet, ob die Anlage TATSAECHLICH heizt oder kuehlt -
//         unabhaengig davon, wer umgeschaltet hat (KNX-Aktor, MQTT-SET9 oder
//         Bedienterminal). Byte 6 (TOP4 Operating_Mode_State) zeigt dagegen
//         nur den zuletzt kommandierten Modus. Damit laesst sich der KNX-Aktor
//         als Statusquelle der Kaskadensteuerung ersetzen.
//
//         Byte 110 ist im Original-HeishaMon nicht dekodiert. Die Bitzuordnung
//         stammt aus ProtocolByteDecrypt.md und ist am 2026-08-15 an WP1
//         empirisch belegt: Stufentest Quiet 0->1->2->3->0 (23:04-23:07, plus
//         Gegenprobe 23:28 bei Stufe 3) sowie beide Moduswechsel, Cool->Heat
//         ueber KNX (21:41:18) und Heat->Cool ueber MQTT SET9 (23:18:52), in
//         beiden Faellen synchron zu Operating_Mode_State. Zwei Vorbehalte
//         stehen so auch in MQTT-Topics.md: TOP99 meldet nur AN/AUS und keine
//         Stufe (die bleibt in TOP18), und TOP102 ist an dieser Anlage gar
//         nicht pruefbar - der External-SW-Eingang ist hier nicht belegt, das
//         Feld bleibt dauerhaft auf b01. Benutzt wird der externe
//         Kompressor-Schalter, fuer den in den 203 Bytes kein Statusbyte zu
//         finden war (betaetigt waehrend der Messungen, keine Reaktion).
//         NACHGETRAGEN 2026-08-16 an der laufenden 3.7.0: TOP100 ist in beiden
//         Zustaenden belegt. Nach set/PowerfulMode 1 (SET4) meldete die WP im
//         naechsten Zyklus TOP17 Powerful_Mode_Time 1 (30 min) UND TOP100
//         Powerful_Mode_Active 1 - Bits 3&4 verhalten sich wie erwartet.
//
//         Die beiden neuen desc-Arrays haben DREI Elemente ("Off"/"On"/
//         "unknown"), obwohl nur zwei Zustaende vorkommen koennen sollten. Die
//         Web-Tabelle (webfunctions.cpp) faengt nur negative Indizes ab, nach
//         oben gibt es keine Grenze und das struct fuehrt keine Array-Laenge
//         mit. Ein 2-Bit-Feld kann aber b11 liefern - das ergibt Index 2 und
//         haette hinter dem Array gelesen. Mit drei Elementen ist der gesamte
//         moegliche Bereich -1..2 gedeckt. Dieselbe Luecke haben aeltere
//         2-Bit-Topics (z. B. ThreeWay_Valve_State mit zweielementigem
//         Valve[]) - Altbestand, bewusst nicht in dieser Aenderung angefasst.
//
//         Nachweise, beide ohne Hardware:
//         - test/byte110_test.cpp (neu): uebersetzt src/decode.cpp mit und
//           prueft ueber getTopicPayload(), dass jedes der vier Topics ueber
//           alle 256 Rohwerte genau seine zwei Bits liest, dass die Klartexte
//           der belegten Zustaende stimmen (0x55 Grundzustand, 0x95 Quiet an,
//           0x59 Kuehlen) und dass der Anzeigeindex nie aus -1..2 laeuft.
//           Gegenprobe gemacht: mit vertauschter Bitgruppe schlaegt der Test
//           mit 192 Abweichungen fehl.
//         - test/decode_vergleich.py --neu ...: 65016 Zeilen ueber 86
//           bestehende Topics identisch zu 3.6.1, die vier neuen ausgeblendet.
//           Neue Option --neu als Gegenstueck zu --entfallen.
//         Die Arduino-Ersatzheader liegen jetzt als Dateien in test/stubs/
//         statt als Zeichenketten in decode_vergleich.py - beide Hosttests
//         benutzen dieselbe Fassung.
//
//         Groessen gegenueber 3.6.1: ESP8266 RAM +232 B, Flash +176 B;
//         ESP32 RAM +80 B, Flash +204 B (vier Tabellenzeilen samt Namen; die
//         const-Tabelle liegt auf dem ESP8266 im RAM).
//
//         NACH dem Flashen: Heat_Cool_SW_State gegen den KNX-Aktor
//         gegenpruefen, bevor die Statusquelle umgestellt wird.
// 3.6.1 - Der Broker spielt beim Verbinden alle Set-Topics wieder ein - die
//         Firmware verwirft sie jetzt (SUBSCRIBE_GRACE, 5 s ab SUBACK).
//
//         Gefunden am 2026-08-13 mit einem gezielten Reboot von Stufe 1, weil
//         der Fluestermodus nach jedem Neustart auf 0 stand. Ablauf: beim
//         Booten abonniert mqtt_reconnect() die 32 Set-Topics; der
//         ioBroker-MQTT-Adapter beantwortet jedes neue Abonnement aus seiner
//         Objektdatenbank und schickt den gespeicherten Wert JEDES Set-Topics.
//         Die Firmware sah 32 frische Kommandos, alle im selben 500-ms-
//         Sammelfenster, und schickte sie als EIN Telegramm an die
//         Waermepumpe. Zwei davon taten weh:
//
//         (1) set/Z1HeatCurveTargetHighTemp stand seit dem 10.08. auf 55 -
//             der Werksvorgabe, die die WP beim Moduswechsel selbst gesetzt
//             hatte. Dieser Kurvenpunkt IST in der WP der Vorlauf-Sollwert
//             (SET5): nach jedem Neustart sprang die Solltemperatur von 20
//             auf 55 Grad, bis die Node-RED-Steuerung sie nach gut drei
//             Minuten zurueckschrieb. Unbemerkt geblieben ist das nur, weil
//             die Anlage im Kuehlbetrieb lief.
//         (2) QuietMode 3 (Byte 7 = 32) und PowerfulMode 0 (Byte 7 = 73)
//             ueberlappen im Protokoll und werden beide mit Maske 0xFF
//             geschrieben - der letzte gewinnt, und PowerfulMode traegt ein
//             implizites "quiet aus". Der Fluestermodus fiel deshalb bei
//             jedem Neustart auf 0.
//
//         Ueber das Retain-Bit ist das nicht zu filtern: Der Adapter sendet
//         die Wiedereinspielung mit retain=0, und PubSubClient reicht das
//         Flag ohnehin nicht an den Callback durch. Deshalb die Karenzzeit -
//         der Schwall kommt unmittelbar nach dem SUBACK. Ein in diesem
//         Fenster wirklich gemeintes Kommando geht verloren; der 5-Minuten-
//         Re-Assert der Steuerung holt es nach. Verworfene Topics stehen
//         einzeln im Telnet-Log, ihre Zahl als eine Zeile im MQTT-Log.
//
//         Messschrieb des Nachweises (H1, Reboot 14:07:44):
//           14:07:58  Quiet_Mode_Level 3 -> 0
//           14:07:58  Z1_Heat_Curve_Target_High_Temp 20 -> 55
//           14:08:04  Z1_Heat_Request_Temp 20 -> 55
//           14:11:18  von Node-RED zurueckgesetzt auf 20
//           14:11:57  Quiet_Mode_Level zurueck auf 3
//         Dasselbe Muster beim OTA um 13:18 und bei den Neustarts am 08.08.
//         und 11.08. Stufe 2 zeigte es nicht - dort steht im Set-Objekt
//         derselbe Wert, der ohnehin aktiv ist.
// 3.6.0 - Die vier verbliebenen Befunde der Codedurchsicht vom 2026-08-12.
//         KEINE Aenderung an Topic-Namen, Wertebereichen oder am Protokoll.
//
//         (1) readSerial() prueft jetzt Telegrammtyp UND Laenge, nicht mehr
//             nur die Pruefsumme. Das war der einzige gefundene Weg, auf dem
//             FALSCHE MESSWERTE in die Kaskadenregelung geraten konnten:
//             bisher galt ein Telegramm als vollstaendig, sobald die Anzahl
//             gelesener Bytes zum Laengenbyte passte - danach entschied allein
//             die 8-Bit-Pruefsumme, also 1 von 256. Ein um n Bytes
//             verschobener Strom (Rest einer abgebrochenen Antwort im
//             UART-Puffer) konnte damit als Messdaten durchgehen, retained im
//             ioBroker landen und die Regelung fuettern. Die Regel steht in
//             src/telegram.h und gilt genau einem Telegramm: Typ 0x71,
//             Laengenbyte 0xC8, 203 Bytes (ProtocolByteDecrypt.md, "Panasonic
//             answer example"); der Decoder liest bis Byte 202, jedes
//             kuerzere Telegramm wurde vorher ueber das Pufferende hinaus
//             ausgewertet.
//             Dazu leert flush_serial_input() den UART-Empfangspuffer VOR
//             jedem Senden - so entstehen die verschobenen Stroeme gar nicht
//             erst. Verworfene Telegramme werden mit Typ und Laenge geloggt.
//
//         (2) mqtt_reconnect() haelt die loop() nicht mehr an. PubSubClient
//             wartet ohne setSocketTimeout 15 s je Versuch, und zwar mitten im
//             5-s-Abfragetakt - bei einem ioBroker-Neustart stand die
//             Abfrage der Waermepumpe sekundenweise still. Jetzt 2 s Timeout,
//             Backoff von 5 s auf bis zu 60 s, und ohne WLAN wird gar nicht
//             erst verbunden (der Versuch koennte nur scheitern).
//
//         (3) WLAN-Watchdog (check_wifi). Bisher gab es keine Pruefung: fiel
//             das WLAN dauerhaft aus, lief das Geraet blind weiter - es fragte
//             die Waermepumpe ab, konnte aber weder Messwerte melden noch
//             Sollwerte empfangen. Der Fallback des WiFiManagers greift nur
//             beim Booten. Jetzt: nach 30 s ohne Verbindung WiFi.reconnect(),
//             nach 5 min Neustart. Fuer die Waermepumpe ungefaehrlich, sie
//             behaelt ihre Sollwerte, und die Node-RED-Steuerung schreibt sie
//             ohnehin alle 5 min neu. Die Ausfalldauer wird gemerkt und nach
//             der naechsten MQTT-Verbindung als Logzeile gemeldet.
//
//         (4) Die Hosttests pruefen ihre Ergebnisse selbst. merge_test.cpp gab
//             seine Zahlen bisher nur aus - der CI-Schritt fing damit nur
//             Uebersetzungsfehler und Abstuerze ab. Jetzt hat jede Zeile eine
//             Zusicherung und der Rueckgabewert bricht die CI. Neu dazu
//             test/telegramm_test.cpp, das src/telegram.h direkt einbindet
//             (kein nachgebauter Zwilling, der auseinanderlaufen kann).
//
//         Nachweis zu (1): 1386 Telegrammvarianten mit nachgezogener
//         Pruefsumme werden weiterhin angenommen - die Pruefung entscheidet
//         nichts anhand der Daten. Abgewiesen werden dagegen das 111-Byte-
//         Abfrageecho, Typ 0xF1, 204 Bytes, unvollstaendige Antworten und alle
//         202 moeglichen Verschiebungen des Antworttelegramms. Ueber 200000
//         Zufallspuffer, deren Laenge zum Laengenbyte passt: alte Regel 817
//         Annahmen (0,41 % = die erwarteten 1/256), neue Regel 0.
//         Alle 10 Envs gebaut. Groessen gegenueber 3.5.0: ESP8266 RAM +244 B,
//         Flash +780 B; ESP32 RAM +16 B, Flash +676 B - im Wesentlichen die
//         neuen Logtexte (Stringliterale liegen auf dem ESP8266 im RAM).
//
//         Abnahme an Stufe 1 (2026-08-13, 7-Minuten-Telnet-Mitschnitt):
//         68 Abfragen + 4 Kommandos = 72 gueltige Telegramme, KEIN verworfenes
//         Telegramm, keine Restdaten, kein Serial-Timeout, kein MQTT-Reconnect,
//         Zyklusabstand konstant 6 s. Damit ist auch die offene Frage
//         beantwortet, ob die Waermepumpe ein Kommando (0xF1) mit einem
//         anderen Telegrammtyp quittiert: Sie antwortet mit demselben
//         203-Byte-0x71-Telegramm wie auf eine Abfrage. Belegt am QuietMode
//         (SET3 0->1->0, "Send command" direkt gefolgt von "Valid data",
//         TOP18 zog nach) und am 6-Kanal-Re-Assert der Node-RED-Steuerung
//         (6 Callbacks in einem 500-ms-Fenster -> ein Kommandotelegramm).
// 3.5.0 - Drei Haertungen aus der Codedurchsicht. KEINE Aenderung an Topic-
//         Namen, Wertebereichen oder am Protokoll (Nachweis s. unten).
//
//         (1) Set-Topics haben nur noch EINE Wahrheit: setCommands in
//             commands.cpp. Bisher fuehrte jeder Name drei Leben - Deklaration
//             in Topics.h, Definition in Topics.cpp, Verweis in der Tabelle -
//             und dazu kam ein handgeschriebener subscribe-Aufruf je Topic in
//             mqtt_reconnect(). Wer den vergass, bekam KEINEN Compilerfehler:
//             das Topic war einfach stumm, die Waermepumpe folgte einem
//             Kommando nicht mehr und niemand haette gewusst, warum.
//             Jetzt steht der Name in der Tabellenzeile (gleiches Muster wie
//             stateTopics in decode.cpp), und subscribe_set_topics() laeuft
//             ueber dieselbe Tabelle. Ein neues Set-Kommando ist eine Zeile.
//             Topics.h behaelt nur die Pfadwurzeln (STATE, SET, LOG, WILL).
//
//         (2) sprintf -> snprintf in allen Logpfaden. Der Fall in
//             build_heatpump_command war kein Schoenheitsfehler: die Meldung
//             "Invalid integer value ..." brauchte bei maximaler MQTT-Payload
//             303 Bytes in einem 256-Byte-Puffer auf dem Stack. Ein langer,
//             nicht numerischer Wert auf einem set-Topic - etwa ein JSON-
//             Objekt aus einem verbauten Node-RED-Flow - konnte das Geraet
//             damit zum Absturz bringen. Payload und Topic sind im Format
//             zusaetzlich begrenzt (%.32s/%.64s), damit die Meldung kurz
//             bleibt und in ein MQTT-Paket passt.
//
//         (3) config.json: ein fehlender Schluessel liess das Geraet beim
//             Booten abstuerzen. strncpy(dst, jsonDoc[key], 39) bekommt von
//             ArduinoJson einen NULLZEIGER, wenn der Schluessel fehlt - das
//             Ergebnis ist eine Boot-Endlosschleife, die nur per USB an der
//             Waermepumpe zu beheben ist. Jetzt bleibt der einkompilierte
//             Standardwert stehen und das Geraet kommt hoch (loadConfigValue).
//             Der realistische Ausloeser ist nicht die Handbearbeitung der
//             Datei, sondern ein SPAETERER Firmware-Stand, der ein neues Feld
//             liest: die config.json auf dem Geraet kennt es dann nicht, und
//             beide Kaskadenstufen wuerden direkt nach dem OTA gleichzeitig
//             ausfallen. Dazu: Puffer beim Einlesen selbst terminiert (las
//             sonst hinter dem Dateiende weiter), Feldlaengen als
//             CONFIG_FIELD_LEN/CONFIG_PORT_LEN an einer Stelle statt 39/40
//             und 5/6 ueber ein Dutzend Stellen verstreut, strlcpy statt
//             strncpy samt Hand-Terminierung.
//
//         Nachweis zu (1): alle 32 vollstaendigen Topic-Pfade samt ihrer
//         Zuordnung auf (Protokollbyte, Maske, min, max, Umrechnung,
//         Parameter) gegen den Stand von 3.4.1 verglichen - identisch, keine
//         Abweichung. Zusaetzlich geprueft, dass die neue Match-Logik
//         (Wurzel pruefen, abschneiden, Namen vergleichen) jeden Pfad findet
//         und Muell abweist (Wurzel ohne Namen, unbekannter Name,
//         state-Pfad, Leerstring).
//         Beide Plattformen gebaut. Groessen gegenueber 3.4.1:
//         ESP8266 RAM -928 B, Flash -1992 B; ESP32 RAM -768 B, Flash -2988 B.
//         Die 32 std::string-Objekte entfallen samt der Heap-Allokation fuer
//         ihren Pfadtext - Letztere taucht in diesen Zahlen nicht auf und war
//         auf dem ESP8266 dauerhaft belegt.
// 3.4.1 - Stufe 2 laeuft jetzt ebenfalls auf dem offiziellen HeishaMon-
//         ESP32-S3-Board (loest den D1 mini H2 ab). KEINE Aenderung an der
//         Firmware-Logik - nur zwei neue Envs in platformio.ini:
//         heishamon_esp32_h2_usb (Erstflash ueber USB, weil die
//         Partitionstabelle der Original-Firmware getauscht werden muss)
//         und heishamon_esp32_h2_ota (alle spaeteren Updates, 192.168.2.122).
//         Der abgeloeste D1 mini H2 (192.168.2.193) ist Reserve und gehoert
//         STROMLOS - sonst hoeren zwei Geraete auf dieselben Set-Topics.
//         Beide Stufen auf denselben Stand gebracht, damit die Versions-
//         anzeige weiter als Flash-Nachweis taugt.
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
//         ausfuehren. Die Firmware publiziert mit Retain-Flag; ein normaler
//         Broker liefert die 13 alten Werte sonst weiter an jeden neuen
//         Abonnenten aus - das Topic verschwindet nicht, es friert ein.
//         HIER aber nur die halbe Miete, am 2026-08-11 nachgemessen: Der
//         Broker ist der ioBroker-MQTT-Adapter im SERVER-Modus, kein
//         eigenstaendiger Broker. Er bedient Abonnenten aus seiner
//         Objektdatenbank mit retain=0, das Loeschen setzt die States also
//         nur auf null. Weg sind die Topics erst, wenn die Objekte
//         mqtt.0.panasonic_heat_pump*.state.Z2_* im ioBroker-Admin
//         geloescht werden - die simple-api auf 8087 kann das nicht.
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
static const char* heishamon_version = "3.8.2";
