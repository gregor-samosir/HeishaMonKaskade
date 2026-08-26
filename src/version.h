#pragma once
// Changelog:
// 3.15.0 - DER NOTBETRIEB STELLT DIE HYDRAULIK SELBST AUF 1-STUFIG. Neuer
//         Schritt 1 in BEIDEN Schrittfolgen: Die Firmware legt den
//         Tasmota-Switch der Hydraulik auf AUS und bricht ab, wenn das nicht
//         gelingt. Vorhaben-Hydraulik-Notbetrieb.md.
//
//         WARUM. Der Notbetrieb setzt hydraulisch 1-stufigen Betrieb voraus,
//         und bisher stellte das niemand sicher. Steht die Hydraulik auf
//         2-stufig, waehrend eine Stufe im Warmwasser-Notbetrieb laeuft,
//         schiebt der Warmwasserbetrieb bis zu 57 C in den Heizkreis - die
//         Fussbodenheizung vertraegt das nicht (Owner, 2026-08-26). Das ist
//         kein Randfall, sondern der Regelfall des Notbetriebs an Stufe 2:
//         Der Warmwasserknopf ist der, der im Sommer traegt, und der, den
//         jemand aus der Familie im Ernstfall druecken soll. Im Normalbetrieb
//         schaltet die Kaskadensteuerung den Switch - und genau die ist im
//         Notbetriebsfall weg.
//
//         GANZ VORN, vor OperationMode. Zwei Gruende: Bricht der Schritt ab,
//         steht die Waermepumpe genau so da wie vorher (kein Kommando
//         abgesetzt, kein Sammelfenster offen, kein halber Notbetrieb zum
//         Aufraeumen). Und die 90 s der beiden Stellantriebe laufen ab dem
//         fruehestmoeglichen Moment, parallel zur restlichen Schrittfolge.
//
//         DIE 90 s ERZWINGEN KEINE WARTEZEIT. Die Waermepumpe braucht nach
//         dem Einschalten rund drei Minuten bis zum Kompressor; zunaechst
//         laeuft nur die Umwaelzpumpe (Owner, 2026-08-26). Der Beleg ist der
//         Normalbetrieb selbst: Dort gehen Switch- und WP-Kommandos
//         GLEICHZEITIG raus, seit jeher und ohne Schaden. Der Notbetrieb hat
//         zwischen beiden sogar 16 s (Wasser) bzw. 48 s (Heizen) Vorsprung.
//
//         ABBRUCH STATT WARNUNG. Antwortet der Switch nicht, meldet er
//         weiterhin ON oder etwas Undeutbares, endet der Lauf in ROT - mit
//         einer eigenen Meldung, die den Schalter im Waschraum nennt und
//         nicht auf das Bedienfeld der Waermepumpe verweist (dort ist nichts
//         verstellt worden). Der Knopf steht nach ROT sofort wieder da: Wer
//         den Schalter von Hand legt und wiederkommt, drueckt erneut, der
//         Lesevorgang meldet dann OFF, und die Folge laeuft durch.
//
//         ERST LESEN, DANN NUR BEI BEDARF SCHALTEN. Ein einzelnes "Power Off"
//         wuerde beide Faelle abdecken (Tasmota antwortet auch dann mit OFF,
//         wenn der Schalter schon aus war). Der Unterschied steht trotzdem im
//         Log - nur so ist nachlesbar, ob tatsaechlich umgeschaltet wurde.
//
//         DER BLOCKIERENDE REQUEST ist Absicht. HTTPClient::GET() haelt
//         loop() an, bis die Antwort da ist oder das Timeout greift (1,5 s
//         statt der voreingestellten 5 s). Genau deshalb steht der Schritt
//         vorn: Es ist kein Kommando an die Waermepumpe unterwegs und kein
//         Sammelfenster offen, die Blockade trifft nur den Abfragezyklus, der
//         ohnehin nur liest. Hoechstens ein Timeout je Lauf, kein
//         Wiederholungsversuch - der Mensch vor der Seite ist der bessere.
//         Eine asynchrone Loesung ist verworfen: Nebenlaeufigkeit in einem
//         Automaten, der vollstaendig aus loop() getrieben wird, gegen 1,5 s
//         in einem Fall, der ohnehin im Abbruch endet.
//
//         GEMESSEN AM ECHTEN SWITCH (2026-08-27, Tasmota 12.0.2): Die Antwort
//         auf /cm?cmnd=Power kommt CHUNKED, ohne Content-Length. Das hat drei
//         naheliegende Zeilen ausgeschlossen. getSize() liefert -1, also gibt
//         es kein "lies so viele Byte wie angekuendigt". readBytes(puffer, 63)
//         wartet, bis 63 Byte da sind oder das Timeout ablaeuft - bei einer
//         15 Byte langen Antwort saesse jeder Lauf die vollen 1,5 s ab, auch
//         wenn alles klappt. Und getString() loest die Chunks zwar sauber auf,
//         allokiert aber in der Groesse der Antwort auf dem Heap; zeigt die
//         eingetragene Adresse versehentlich auf einen richtigen Webserver,
//         waere das eine ganze HTML-Seite auf einem ESP8266 mit rund 30 kB
//         freiem Heap. Gelesen wird deshalb byteweise in einen festen Puffer,
//         mit harter Frist (300 ms) und Schluss, sobald "OFF" oder "ON"
//         dasteht. Die Chunk-Laengen im rohen Strom stoeren dabei nicht.
//
//         DER ERSTE SCHRITT GEHT JETZT AUS loop() RAUS, nicht mehr aus dem
//         POST-Handler. Bis 3.14.2 setzte notbetrieb_starten() das erste
//         Kommando selbst ab - fuer ein Set-Kommando eine Sache von
//         Mikrosekunden, fuer einen HTTP-Request von bis zu 1,5 s eine
//         haengende Seite. NotbetriebLauf bekommt dafuer das Feld
//         schritt_gesendet; ohne es koennte ein TOP, das den Sollwert
//         zufaellig schon traegt, einen nie abgesetzten Schritt bestaetigen.
//
//         NEU AUF DER SEITE: Der Abbruchgrund. Feld 9 des Statusstrings,
//         hinten angehaengt - die Felder 5 bis 8 muessen stehen bleiben, weil
//         die Startseite dieselbe Route liest. Aus ihm macht die Seite bei
//         Hydraulik-Abbruechen den Satz aus Abschnitt 8 des Vorhabens statt
//         des generischen "Hat nicht geklappt".
//
//         NEUE EINSTELLUNG: hydraulik_switch (IP oder Hostname des Tasmota).
//         Sie gehoert NICHT fest in den Code - sie steht in derselben
//         Groessenordnung wie der MQTT-Broker und wird sich irgendwann
//         aendern. Leeres Feld heisst "nicht eingerichtet": Der Notbetrieb
//         bricht dann im ersten Schritt ab, statt die Hydraulik ungeprueft zu
//         lassen. Das JSON-Dokument der Settings-Seite waechst dafuer von 512
//         auf 1024 Byte - mit dem siebten Feld haetten 512 nicht mehr sicher
//         gereicht, und ein Ueberlauf schriebe die config.json still
//         unvollstaendig.
//
//         ZURUECK SCHALTET WEITERHIN NIEMAND IN DER FIRMWARE (Entscheidung 7).
//         Das macht der Re-Assert der Kaskadensteuerung, seit dem 2026-08-26
//         mit einer Frische-Bedingung: Ein Schaltbefehl an den Switch geht nur
//         raus, wenn der Betriebsmodus von Stufe 2 nicht aelter als zwoelf
//         Minuten ist. Der Grund ist, dass Wärmepumpen- und Switch-Kommandos
//         den ioBroker ueber VERSCHIEDENE Adapter verlassen (mqtt 1883 gegen
//         sonoff 1886) - faellt der eine aus, laeuft der andere weiter und
//         legte die Hydraulik mitten im Notbetrieb zurueck auf 2-stufig.
//
//         GROESSE (.elf, text/data/bss gegen 3.14.2):
//           esp32_h1_ota  1018319/221386/2192879 -> 1034715/225330/2192919
//           esp32_h2_ota  1017983/221018/2192879 -> 1034371/225042/2192919
//           d1_mini_h1    469923/2440/31216      -> 476267/2440/31272
//         Der Zuwachs von rund 20 kB (ESP32) bzw. 6 kB (ESP8266) Flash ist
//         fast vollstaendig der HTTPClient; im RAM sind es 40 bzw. 56 Byte.
//
//         MITGEZOGEN: test/notbetrieb_test.cpp (neuer Abschnitt 9a, alle
//         Schrittindizes um eins verschoben), Ablauf-Notbetrieb.md und
//         README.md.
//
// 3.14.2 - Der Kuehlkurven-Aussenpunkt Z1CoolCurveOutsideLowTemp (SET33) war
//         auf 20..30 begrenzt. Richtig ist 15..30. Die Firmware wies 15 mit
//         "Value 15 out of range [20..30]" ab, bevor das Kommando ueberhaupt
//         zur WP ging - ein Wert, den die Anlage annimmt, kam so nie an.
//
//         DER NACHWEIS kommt vom Geraet selbst: Am Bedienterminal laesst sich
//         der untere Punkt der Kuehlkurve auf 15 C stellen, die WP uebernimmt
//         und zeigt ihn (pictures/Kuehlen_Kurve.png, 2026-08-25). Die X-Achse
//         des Kurvendialogs nennt 15..30 - genau die Unschaerfe, die in
//         test/README.md seit dem 2026-08-11 als offen vermerkt war.
//
//         WARUM DIE MESSUNG SIE NICHT FAND. kurven_grenzen.py prueft von der
//         hinterlegten Grenze aus nach aussen. Ein zu ENG gesetzter Bereich
//         faellt dabei grundsaetzlich nicht auf, weil unterhalb der Annahme
//         nie geschrieben wird. Bei zu WEIT gesetzten Bereichen (3.2.x, Heat/
//         Cool OutsideHigh) hat dieselbe Messung die Fehler gefunden. Lehre:
//         Die Bereichsangabe des Bedienterminals schlaegt die Klemm-Messung,
//         wenn beide auseinandergehen.
//
//         MITGEZOGEN: test/kurven_sync.py und test/kurven_grenzen.py (beide
//         halten denselben Bereich als Spiegel von commands.cpp), sowie
//         MQTT-Topics.md und test/README.md.
//
// 3.14.1 - Der Kurvenhinweis aus 3.14.0 hatte keine Farbe. Die Klasse
//         w3-pale-yellow stand im HTML, aber nicht im mitgelieferten CSS -
//         das Feld waere auf dem Geraet ohne Hintergrund erschienen.
//         Nachgetragen als ".w3-pale-yellow{background:#ffffcc;color:#000}",
//         hinter .w3-button wie alle Farbklassen (dessen background:inherit
//         hat dieselbe Spezifitaet und wuerde eine vorher stehende Farbe
//         ueberschreiben).
//
//         GEFUNDEN HAT ES DIE CI, nicht der Blick auf die Seite:
//         test/css_klassen_test.py vergleicht die benutzten Klassen gegen die
//         definierten. Der Test lief vor dem Rollout von 3.14.0 nicht mit -
//         die Firmware war da schon auf beiden Stufen. Lehre fuer den
//         naechsten Rollout: die Hosttests der CI vorher vollstaendig lokal
//         fahren, nicht nur die des angefassten Bereichs.
//
//         Die neue Klasse steht ausserdem in FARBKLASSEN des Tests, damit
//         auch fuer sie geprueft wird, dass sie hinter .w3-button liegt.
//
// 3.14.0 - Die Weboberflaeche und das Kurvenwerkzeug melden eine VERDREHTE
//         Heizkurve. Neue Regel notbetrieb_kurve_pruefen() in notbetrieb.h,
//         hosttestbar wie alles dort.
//
//         WARUM. Die vier Kurvenwerte koennen einzeln im erlaubten Bereich
//         liegen und trotzdem eine unsinnige Kurve ergeben. Der Grund ist die
//         Ueberkreuzung der Namen: Panasonics Target_High/Low benennt die
//         VORLAUFhoehe, der ioBroker-Konfigbaum benennt mit Hi/Lo die
//         AUSSENtemperatur - "vlLo" gehoert also nach "TargetHigh". Bis zum
//         2026-08-20 spiegelte kurven_sync.py die Kurve deshalb verdreht, und
//         kein Bereichstest konnte das finden: 26 und 34 sind beide gueltig.
//         Owner-Rueckfrage 2026-08-23: Genau diese Namensgebung fuehrt beim
//         Lesen zuverlaessig in die Irre. Beschriftet ist sie seither
//         durchgaengig (VL kalt / VL warm / AT kalt / AT warm); diese Version
//         legt die Pruefung daneben, weil Beschriftung eine Verwechslung
//         sichtbar macht, aber nicht verhindert.
//
//         DIE REGEL. Eine Heizkurve faellt mit steigender Aussentemperatur,
//         also VL kalt >= VL warm. Gleichheit ist erlaubt - eine flache
//         Vorgabe ist zulaessig, die Kuehlkurve dieser Anlage faehrt genau so
//         (20 C bei 20 wie bei 30 C). Die Aussenpunkte muessen dagegen echt
//         auseinanderliegen: Zwei Stuetzpunkte auf derselben Temperatur
//         ergeben keine Kurve. Unvollstaendige Saetze und die Rolle
//         Warmwasser melden OK - dort gibt es nichts zu pruefen.
//
//         WARNEN, NICHT SPERREN. Der Knopf bleibt bedienbar. Ein Notbetrieb
//         auf verdrehter Kurve ist immer noch besser als keiner, und die
//         Regel kennt die Absicht des Betreibers nicht. Gesperrt wird
//         weiterhin nur, was nachweislich nicht funktioniert: fehlende Werte
//         und der Kuehlbetrieb. Die Sperrfarbe orange bleibt der echten
//         Sperre vorbehalten, der Hinweis ist blassgelb.
//
//         WO ER AUFTAUCHT. Auf der Notbetriebsseite als eigenes Feld ueber
//         dem Statusbereich - serverseitig fertig aufgebaut und im 2-s-Takt
//         nachgefuehrt, gleiche Machart wie der Sperrhinweis. Im MQTT-Log
//         beim WECHSEL der Beurteilung, mit den Zahlen in der Zeile; nur beim
//         Wechsel, weil der Broker nach jedem Reconnect alle Werte erneut
//         einspielt. Und in test/kurven_sync.py, das eine verdrehte Kurve
//         jetzt gar nicht erst spiegelt (--kurve-ignorieren hebt das auf).
//
//         STATUSROUTE. /notbetrieb/status liefert ein achtes Feld:
//         Zustand;Schritt;Schritte;fehlend;Sperre;Lage;Dauertext;
//         Kurvenwarnung. 0 in Ordnung, 1 Vorlaeufe vertauscht, 2
//         Aussenpunkte vertauscht oder gleich. Es haengt HINTEN an, damit
//         Lage und Dauertext auf den Indizes 5 und 6 stehen bleiben - die
//         Startseite liest dieselbe Route.
//
//         GROESSE. ESP8266 RAM +648 B (60,4 % -> 61,2 %), Flash +1464 B;
//         ESP32-S3 RAM +8 B, Flash +1500 B (je gegen 3.13.0, Stufe 1).
//
//         NICHT GEAENDERT. Keine Zuordnung, kein Topic-Name, keine
//         Schrittfolge. Wer 3.13.0 fahren will, verliert nur den Hinweis.
//
// 3.13.0 - Die Weboberflaeche sagt, wenn die Hausteuerung ausgefallen ist -
//         und sie unterscheidet dabei ZWEI Ausfaelle: Broker weg, und Broker
//         da, aber die Kaskadenregelung rechnet nicht mehr. Dazu zwei
//         Korrekturen an der Notbetriebsseite: der Knopf ist blau statt rot,
//         und "Laeuft..." heisst jetzt "Konfiguration Notbetrieb laeuft".
//
//         WARUM. Owner-Beobachtung 2026-08-21, mitten im Nachweis zu 3.12.0:
//         Waehrend der Broker weg war, heizte die Waermepumpe einfach weiter,
//         mit dem zuletzt gesetzten Sollwert. Kein Alarm, kein Hinweis,
//         nichts. Der Ausfall wirkt sich erst mit Verzoegerung aus - im
//         Sommer ueber Tage, im Januar ueber Stunden, wenn der Sollwert der
//         fallenden Aussentemperatur nicht mehr nachgefuehrt wird. Damit
//         stand der Notbetriebsknopf aus 3.12.0 auf einem stillen Fundament:
//         Er funktioniert, aber jemand muss auf die Idee kommen, ihn zu
//         suchen. Nachgesehen und bestaetigt: Die Oberflaeche zeigte den
//         Ausfall NIRGENDS an - weder auf der Startseite noch sonstwo. Selbst
//         wer gezielt nachschaute, sah ihn nicht. Die Firmware wusste es (der
//         Reconnect laeuft im Backoff ins Leere), sie sagte es nur niemandem
//         ausser im MQTT-Log - und das geht in genau dieser Lage ins Leere.
//
//         WAS ANGEZEIGT WIRD. "Hausteuerung seit 14 Minuten nicht
//         erreichbar", dazu der Satz, was die Waermepumpe gerade tut. Der
//         zweite Satz ist der wichtigere: Er beantwortet die Frage, die der
//         erste aufwirft. Auf der NOTBETRIEBSSEITE steht auch im Normalfall
//         eine Zeile ("Hausteuerung: verbunden", grau und klein) - dort steht
//         die Entscheidung an, ob der Knopf gedrueckt werden muss, und ein
//         ruhiges "verbunden" verhindert die haeufigere Fehlentscheidung. Auf
//         der STARTSEITE steht nur der Stoerfall; sie ist ein
//         Nachschauwerkzeug, keine Statusampel.
//
//         KARENZ VON 5 MINUTEN. Unterhalb davon wird nichts gemeldet. Der
//         Grund ist nicht der WLAN-Wackler allein: Ein Neustart des
//         ioBroker-Adapters oder des Containers auf der Synology dauert
//         regelmaessig ein bis zwei Minuten. Eine Stoermeldung, die von
//         selbst wieder verschwindet, erzieht die Familie dazu, sie zu
//         uebersehen - und dann wird auch die echte uebersehen. Der
//         Reconnect-Backoff (5 s bis 60 s) liegt vollstaendig darunter.
//
//         GEMESSEN WIRD DIE MQTT-VERBINDUNG, nicht das WLAN. Der Ausfall, um
//         den es geht, ist der des ioBroker - und der MQTT-Broker IST der
//         ioBroker-Adapter. Ohne WLAN waere auch die Weboberflaeche weg, die
//         diese Auskunft anzeigen soll.
//
//         DER ZWEITE AUSFALL: DIE STUMME STEUERUNG. Broker erreichbar, aber
//         seit ueber zwoelf Minuten kein Kommando - dann rechnet die
//         Kaskadenregelung nicht mehr (Node-RED-Container weg, Flow im
//         Fehler). Von aussen sieht alles gesund aus, die Waermepumpe bekommt
//         trotzdem keine Vorgaben. Die Seite sagt dann ausdruecklich
//         "Hausteuerung erreichbar, sendet aber seit 23 Minuten keine
//         Vorgaben" - wer zum Server im Keller laeuft, soll wissen, ob dort
//         ueberhaupt etwas zu holen ist.
//
//         DIE ZWOELF MINUTEN SIND GERECHNET, NICHT GERATEN. Der Re-Assert des
//         Hauptmodus-Verteilers kommt alle 300,0 s - am 2026-08-21 an H2
//         gemessen (zwei Takte, Abstand exakt 300,0 s, je Takt sieben
//         empfangene Kommandos). Ein einzelner verpasster Takt ist noch kein
//         Ausfall; zwoelf Minuten decken zwei verpasste Takte samt Reserve ab.
//
//         WAS ALS HERZSCHLAG ZAEHLT. Der Aufruf steht in mqtt_callback()
//         NACH der SUBSCRIBE_GRACE-Pruefung. Der Grund ist genau die
//         Wiedereinspielung, wegen der es die Karenzzeit gibt: Der
//         ioBroker-Adapter schickt jedem neuen Abonnenten die gespeicherten
//         Werte aller Set-Topics, AUCH wenn Node-RED laengst tot ist. Dieser
//         Schwall ist kein Lebenszeichen der Kaskadenregelung, sondern nur
//         eines des Brokers - und den beobachtet bereits die andere Uhr.
//         Zaehlte er mit, verstummte die Meldung nach jedem Reconnect fuer
//         zwoelf Minuten, ohne dass sich etwas geaendert haette. Ein Kommando,
//         das die Firmware danach VERWIRFT (unbekanntes Topic,
//         Bereichsfehler), zaehlt dagegen sehr wohl: Die Steuerung hat
//         gesendet, sie lebt.
//
//         VORRANG UND EINE UHR, DIE STILLSTEHT. Ist der Broker weg, gilt der
//         Broker-Ausfall - beides zu melden wuerde jemanden zum Server
//         schicken, um dort nach dem falschen Fehler zu suchen. Die Stumm-Uhr
//         laeuft deshalb nur bei stehender Verbindung und startet mit dem
//         Verbindungsaufbau. Ohne diese Regel meldete die Seite unmittelbar
//         nach der Rueckkehr des Brokers sofort einen zweiten Fehler, den es
//         nie gab.
//
//         DER SONDERFALL "NIE VERBUNDEN". Hatte die Firmware seit dem
//         Einschalten nie eine Verbindung, ist die wahre Ausfalldauer
//         unbekannt - der Broker kann seit Tagen weg sein, das Geraet ist nur
//         gerade neu gestartet. Die Seite schreibt dann "seit dem Neustart
//         dieses Geraets" statt einer Minutenzahl, die gelogen waere.
//
//         DER KNOPF IST BLAU. Rot hiess auf der Notbetriebsseite zweierlei -
//         der Knopf im Sinne von "druck mich", das Ergebnisfeld ROT im Sinne
//         von "hat nicht geklappt". Am 2026-08-21 hat das prompt zu einer
//         Verwechslung gefuehrt. Fuer eine Seite, deren ganze Rueckmeldung
//         aus einer Ampel besteht, darf Rot nur eines bedeuten. Auffaellig
//         bleibt der Knopf ueber Groesse und Polsterung.
//
//         DIE SEITE SPRICHT DEUTSCH. webHeader traegt jetzt ein charset, und
//         die Texte der Notbetriebsseite haben Umlaute. Diese eine Seite
//         liest im Ernstfall jemand aus der Familie, nicht der Entwickler.
//         Die uebrigen Seiten (Home, Settings, Firmware) bleiben unberuehrt.
//
//         STATUSROUTE ERWEITERT. /notbetrieb/status liefert zwei Felder mehr:
//         Zustand;Schritt;Schritte;fehlend;Sperre;Lage;Dauertext. Lage: 0
//         verbunden, 1 Karenz, 2 Broker weg, 3 seit dem Neustart nie
//         verbunden, 4 Steuerung stumm. Die
//         Textform der Dauer kommt fertig von dort - gerechnet wird sie in
//         verbindung.h, also an einer Stelle und vom Hosttest abgedeckt. Die
//         Startseite fragt dieselbe Route im 30-s-Takt der Tabelle ab; eine
//         zweite Route fuer zwei Felder waere auf einem ESP8266 der teurere
//         Weg.
//
//         GROESSE. ESP32-S3 RAM +32 B, Flash +3224 B; ESP8266 RAM +768 B
//         (59,5 % -> 60,4 %), Flash +3224 B (je gegen 3.12.0, Stufe 1).
//
//         REGELN HOSTTESTBAR. src/verbindung.h ist arduino-frei wie
//         sendwindow.h, telegram.h und notbetrieb.h;
//         test/verbindung_test.cpp bindet es direkt ein und steht bei 95
//         Zusicherungen. Beide Uhren teilen sich denselben Kern (struct
//         Ausfall) - die Ueberlauffestigkeit ist der subtile Teil, und
//         zweimal hingeschrieben waere zweimal Gelegenheit, sie falsch zu
//         machen. Fuenf Gegenproben gefahren: Karenz auf 1 min verstellt = 6
//         Abweichungen; Dauer gerechnet statt fortgeschrieben = 6 (Text nach
//         49,7 Tagen Ausfall dann "1 Minute" - genau die Falschauskunft, die
//         der Deckel verhindert); Stumm-Uhr laeuft ohne Verbindung weiter = 6
//         (die Seite meldete dann direkt nach der Rueckkehr des Brokers einen
//         zweiten Fehler); Stumm-Karenz auf 4 min = 7; Vorrang umgedreht = 1.
//         Die letzte Gegenprobe deckte eine Luecke auf: Der Vorrang in
//         verbindung_lage() war zunaechst gar nicht geprueft, weil die Uhren
//         im Betrieb nie gleichzeitig laufen. Er steht als zweite Sicherung
//         weiter da und wird jetzt mit einem von Hand gebauten Zustand
//         belegt.
//         NICHT abgedeckt vom Hosttest: die Anbindung in HeishaMon.cpp
//         (mqtt_client.connected() als Eingang, der Aufruf in mqtt_callback)
//         und die Anzeige selbst. Beide gehoeren in den Abnahmetest - und der
//         hat sich gelohnt, siehe unten.
//
//         AM PRUEFSTAND NACHGEWIESEN (2026-08-21, 14:08-15:02), ohne Eingriff
//         an H1/H2 und ohne Testfenster: alle sechs Lagen, beide Karenzen auf
//         die Sekunde (Stumm-Karenz zweimal, Broker-Karenz einmal), der
//         Vorrang (14 min ohne Broker -> weiterhin "nicht erreichbar"), beide
//         Logzeilen mit stimmigen Dauern und die Rueckkehr ohne zweite
//         Stoermeldung. Protokoll in test/README.md und im Vorhaben.
//
//         DABEI GEFUNDEN UND BEHOBEN - der Grund, warum es den Abnahmetest
//         gibt: Die Herzschlag-Meldung stand zunaechst im mqtt_callback. Dort
//         darf sie nicht stehen. write_mqtt_log() ruft mqtt_client.publish(),
//         und PubSubClient benutzt fuer Senden und Empfangen DENSELBEN Puffer
//         - genau den, in den `topic` und `payload` waehrend des Callbacks
//         zeigen. Aus "panasonic_heat_pump_test/set/QuietMode" wurde "0Q", die
//         Firmware meldete "Unknown set topic 0Q", und das Kommando ging
//         verloren. Betroffen waere ausgerechnet das erste Kommando nach dem
//         Ende einer Stumm-Meldung gewesen. Jetzt merkt sich der Callback nur
//         die Dauer (stilleBeendetSekunden), geloggt wird aus loop() - dasselbe
//         Muster wie wifiOutageSeconds. Merke fuer kuenftige Aenderungen an
//         mqtt_callback(): dort nicht loggen.
//
// 3.12.0 - Der Notbetrieb ist ueber die Weboberflaeche schaltbar. Ein Knopf auf
//         /notbetrieb stellt die Waermepumpe auf ihre eigene Heizkurve (Stufe 1)
//         bzw. auf reinen Warmwasserbetrieb (Stufe 2) und schaltet sie ein -
//         ohne ioBroker, ohne Node-RED, ohne MQTT-Broker.
//
//         WARUM DAS NOETIG WAR. Seit 3.11.0 ist die Betriebsart fernschaltbar,
//         aber nur ueber MQTT - und der Broker IST der ioBroker-Adapter. Faellt
//         der ioBroker aus, fehlt nicht nur der Absender des Kommandos, sondern
//         der Uebertragungsweg selbst. Der eigene Webserver der Firmware ist der
//         Weg, der in diesem Fall noch uebrig bleibt. Die Aussage im Changelog
//         zu 3.11.0, damit sei der Notbetrieb "vollstaendig fernschaltbar",
//         galt also nur, solange ein Broker erreichbar war.
//
//         WOHER DIE KURVENWERTE KOMMEN. Ueber einen eigenen Topic-Zweig
//         <prefix>/notbetrieb/, den Node-RED bei Aenderung beschickt und den die
//         Firmware im RAM haelt. Diese Werte gehen NIE an die Waermepumpe -
//         ausser wenn der Knopf gedrueckt wird. Keine Datei auf LittleFS: Der
//         ioBroker-Adapter spielt jedem neuen Abonnenten die gespeicherten Werte
//         ein, die Werte sind nach einem Neustart also binnen Sekunden wieder da.
//         Dafuer nimmt der Zweig die SUBSCRIBE_GRACE aus 3.6.1 ausdruecklich aus
//         - dieselbe Wiedereinspielung, die dort die Gefahr ist, ist hier der
//         Mechanismus.
//
//         GRUEN HEISST ZURUECKGELESEN, NICHT ABGESENDET. Die Schritte laufen
//         einzeln aus loop() (senden -> zuruecklesen -> naechster Schritt),
//         nicht in einem Sammelfenster: Sonst konkurriert das Kurvenschreiben
//         mit dem Werks-Reset des Moduswechsels. Jeder Schritt gilt fruehestens
//         8 s nach seinem Kommando als bestaetigt, sonst koennte ein veralteter
//         Ruecklesewert einen Schritt abhaken, den der Werks-Reset danach
//         ueberschreibt. Schritt-Timeout 20 s, Gesamtdeckel = Schrittzahl x
//         Timeout. Ein Heizen-Lauf braucht real 57-58 s.
//
//         DIE BETRIEBSART IST DIE FREIGABE. Der externe KNX-Schalter gibt die
//         Richtung vor: Steht die Anlage auf Kuehlen, verwirft sie jeden
//         Heizmodus stillschweigend - am 2026-08-20 an Stufe 1 gemessen, das
//         Kommando ging nachweislich durch Bereichspruefung, Maskenmerge und
//         Telegramm. Der Knopf der Rolle Heizen ist deshalb nur frei, wenn
//         Heat_Cool_SW_State (TOP101) sich sauber als 0 liest; alles andere
//         (1, 2, -1, nie empfangen) sperrt mit Klartext auf der Seite. Meldet
//         die Anlage mitten im Lauf ausdruecklich Kuehlen, bricht die Folge ab.
//         Stufe 2 ist nicht betroffen: OperationMode 3 traegt auch im
//         Kuehlbetrieb.
//
//         EIGENER ZUGANG STATT DES OTA-PASSWORTS. /notbetrieb und
//         /notbetrieb/start verlangen Benutzer "notbetrieb" und ein eigenes,
//         einkompiliertes Passwort (HEISHA_NOTBETRIEB_PASSWORD aus
//         platformio_user_env.ini). Der Knopf steht mit Passwort in der
//         ausgedruckten Notfallanleitung der Familie - dasselbe Blatt haette
//         sonst auch den Firmware-Upload und die MQTT-Zugangsdaten geoeffnet.
//         Die Statusroute /notbetrieb/status bleibt bewusst ohne Anmeldung: Sie
//         gibt nur Zustand und Schrittzahl heraus und wird alle zwei Sekunden
//         abgefragt.
//
//         AN DER ANLAGE BELEGT, NICHT NUR IM HOSTTEST. In der Nacht zum
//         2026-08-21 an beiden Stufen:
//           - Stufe 1 im Kuehlbetrieb: Knopf gesperrt, POST abgewiesen, kein
//             Kommando an die Waermepumpe.
//           - Stufe 1 im Heizbetrieb: GRUEN nach 57 s, Heating_Mode 1 -> 0,
//             Z1_Heat_Curve_Target_High_Temp 20 -> 34, Heatpump 0 -> 1.
//           - Stufe 1 OHNE erreichbaren Broker (mqtt.0 gestoppt): GRUEN nach
//             58 s, TargetHigh 26 -> 34 aus dem RAM, Kompressor 26 -> 33 Hz.
//             Danach meldete sich die Bridge 52 s nach dem Broker-Start von
//             allein wieder an.
//           - Stufe 2 im Kuehlbetrieb: GRUEN nach 24 s, DHWTemp 45 -> 48,
//             Heatpump 0 -> 1, Sperre bleibt frei (Rollentrennung).
//         Die Rueckkehr macht Node-RED: eine Zeile set/HeatingMode 1 im
//         5-min-Re-Assert, an einen Herzschlag gebunden. Ein Knopf
//         "Notbetrieb aus" waere die falsche Aktion - beim Zurueckschalten
//         uebernimmt der Sollwert den unteren Kurvenpunkt.
//
//         GROESSE. ESP32-S3 RAM +72 B, Flash +9236 B; ESP8266 RAM +2536 B,
//         Flash +8120 B (je gegen 3.11.0, Stufe 1). Der RAM-Zuwachs auf dem
//         ESP8266 faellt auf und hat einen bekannten Grund: Die Tabellen in
//         notbetrieb.h sind "static const" im Header und liegen damit einmal je
//         Uebersetzungseinheit im RAM - der ESP8266 legt const-Zeigerarrays
//         nicht von selbst ins Flash. 59,4 % statt 56,3 % ist verkraftbar, der
//         ESP8266 ist ohnehin nur die Rueckfallebene; auf den produktiven
//         ESP32-Boards spielt es keine Rolle.
//
//         REGELN HOSTTESTBAR. src/notbetrieb.h ist arduino-frei wie
//         sendwindow.h und telegram.h; test/notbetrieb_test.cpp bindet es
//         direkt ein und steht bei 159 Zusicherungen (Vollstaendigkeit,
//         Bereichsgrenzen, Karenz-Ausnahme, Schrittfolge, Zustandsautomat,
//         millis()-Ueberlauf, Freigabe ueber TOP101, Anzeigeverfall).
//
// 3.11.0 - Zwei neue Set-Kommandos: SET35 HeatingMode und SET36 CoolingMode
//         (Byte 28, je 0 = Kompensationskurve, 1 = Direktvorgabe). Damit ist
//         die Betriebsart erstmals fernschaltbar; bisher ging das nur am
//         Bedienterminal. Reine Schreibseite - an den 92 State-Topics und am
//         Dekodierpfad aendert sich nichts.
//
//         WARUM. Der Notbetrieb war bis hierher nur halb automatisiert: Faellt
//         die Node-RED-Kaskadensteuerung aus, soll die Waermepumpe auf ihrer
//         eigenen Heizkurve weiterlaufen. Die Kurvenwerte werden dafuer schon
//         laufend gespiegelt (SET27-SET34, test/kurven_sync.py) - aber die
//         Umschaltung selbst musste ein Mensch machen. Genau dieser Handgriff
//         blieb liegen, wenn niemand im Haus ist.
//
//         MASKEN SIND HIER PFLICHT. Byte 28 traegt beide Betriebsarten in zwei
//         Bitfeldern (Bits 7+8 Heizen, Bits 5+6 Kuehlen). Ohne bitgenaue Maske
//         schaltet ein Kuehl-Kommando die Heizung mit um - genau die
//         Fehlerklasse, die 3.1.0 beseitigt hat. Deshalb 0x03 und 0x0C.
//
//         NEBENWIRKUNG, DIE BLEIBT. Ein Wechsel von Direkt auf Kurve setzt die
//         vier Kurvenpunkte des betroffenen Kreises auf die Panasonic-Werks-
//         vorgaben zurueck; das Zurueckschalten stellt sie NICHT wieder her,
//         und der Direktsollwert uebernimmt dabei den unteren Kurvenpunkt. Das
//         ist eine Eigenschaft der Waermepumpe, nicht der Firmware - wer
//         schaltet, muss die Kurve danach aus dem ioBroker nachziehen.
//
//         AM GERAET GEMESSEN, NICHT ABGELEITET. Am 2026-08-19 an Stufe 1 bei
//         stehender Anlage (Heatpump_State 0, Compressor_Freq 0). Die offene
//         Frage war, ob die Waermepumpe Byte 28 im Kommandotelegramm ueberhaupt
//         annimmt - das Original-Projekt hat kein Kommando dafuer, es gab also
//         keine Fremderfahrung. Sie nimmt es an: set/CoolingMode 0 liess Byte 28
//         im laufenden Mitschnitt von 0x0A auf 0x06 wandern (byte_monitor.py,
//         11 Telegramme, Flanke zwischen dem fuenften und sechsten). Bits 5+6
//         gingen von b10 auf b01, Bits 7+8 blieben auf b10 stehen - die Maske
//         greift bitgenau. TOP81 meldete 0, TOP76 unveraendert 1.
//         set/CoolingMode 1 stellte 0x0A und TOP81 = 1 wieder her.
//
//         EIN BEFUND WAR NEU. Erwartet war der Kurven-Reset (TOP72/TOP73 auf
//         die Werks-Kuehlkurve 15/10, TOP28 im Kurvenbetrieb 0). Nicht erwartet
//         war, dass dasselbe Kommando den HEIZ-Sollwert mitzieht: TOP27 sprang
//         von 20 auf 35 - den Werkswert der HEIZkurve bei +15 C -, obwohl TOP76
//         durchgehend auf Direkt stand und die Heizseite nie geschaltet wurde.
//
//         VIER LAEUFE, BEIDE KOMMANDOS, BEIDE BETRIEBSMODI.
//           1) Heizbetrieb, CoolingMode 0: 0x0A -> 0x06, TOP81 auf 0, TOP76 blieb
//           2) Kuehlbetrieb, CoolingMode 0: 0x0A -> 0x06, identisch zu Lauf 1
//           3) Heizbetrieb, HeatingMode 0: 0x0A -> 0x09, TOP76 auf 0, TOP81 blieb
//           4) Heizbetrieb, BEIDE 0:       0x0A -> 0x05, TOP76 und TOP81 auf 0
//         Lauf 2 klaert die Deutung des Nebenbefunds: Nach Lauf 1 war offen, ob
//         die Waermepumpe immer beide Kreise anfasst oder nur den gerade
//         aktiven - der mitgewanderte Sollwert war ja der des aktiven Modus. Im
//         Kuehlbetrieb sprang TOP27 WIEDER auf 35, obwohl der Heizkreis diesmal
//         nicht der aktive war. Es sind IMMER beide Kreise.
//         Lauf 3 belegt SET35 und ist zu Lauf 1 spiegelbildlich: der KUEHL-
//         Sollwert TOP28 sprang auf 10, und die Heizkurve stand danach
//         vollstaendig auf den Werksvorgaben, inklusive des Aussenpunkts
//         (TOP32 auf -5) - den konnte der Kuehl-Lauf nicht zeigen, weil
//         TOP74/TOP75 dort schon auf Werk standen.
//         LAUF 4 IST DER FALL, DEN DER NOTBETRIEB FAEHRT. Die Kaskadensteuerung
//         sendet beide Kommandos aus derselben Flow-Ausfuehrung; sie landen im
//         selben 500-ms-Sammelfenster und werden zu EINEM Telegramm, in dem
//         beide Bitfelder gleichzeitig einen Wechsel verlangen. Diesen Fall
//         hatte die WP nie gesehen - in den Laeufen 1-3 stand das andere Feld
//         auf b00 = "keine Aenderung". Sie nimmt beide an: TOP76 und TOP81
//         gingen zusammen auf 0, das Zurueckschalten ebenso kombiniert von 0x05
//         auf 0x0A. Umschalten geht also in einem Rutsch, ohne die Kommandos
//         zeitlich zu trennen. Die Firmware-Seite war ohnehin belegt -
//         byte28_test.cpp baut 0x05 aus denselben zwei Merge-Aufrufen, und weil
//         0x03 und 0x0C disjunkt sind, schlaegt die Konfliktwarnung nicht an.
//
//         Damit sind ALLE VIER Rohwerte aus ProtocolByteDecrypt.md am Geraet
//         erzeugt: 0x0A, 0x06, 0x09 und 0x05. Lauf 4 zeigt zusaetzlich beide
//         Werkskurven in einer Momentaufnahme und belegt den Roundtrip-Verlust
//         ueber den Kommandopfad (nach dem Zurueckschalten standen TOP27 auf 35
//         und TOP28 auf 10, die Sollwerte hatten die unteren Kurvenpunkte
//         uebernommen). Wer SET35 oder SET36 benutzt, muss danach die
//         Kurve beider Kreise nachziehen; die Sollwerte holt der 5-min-
//         Re-Assert der Kaskadensteuerung selbst zurueck, sofern sie gerade
//         aktiv regelt (in den Laeufen 2 und 3 im Mitschnitt gesehen).
//
//         Der Ausgangszustand wurde nach allen vier Laeufen vollstaendig
//         wiederhergestellt (Kurve ueber kurven_sync.py). Tabellen und Rohdaten
//         in test/README.md und SET-TOP-Zuordnung.md Fussnote 6.
//
//         OHNE GERAET ABGESICHERT. test/byte28_test.cpp legt die Merge-Logik aus
//         commands.cpp und die beiden Dekodierer aus decode.cpp nebeneinander:
//         30 Zusicherungen ueber alle vier Kombinationen, die vier Rohwerte aus
//         ProtocolByteDecrypt.md und den Nachbarschutz, samt Gegenprobe, dass
//         dieselbe Operation ohne Maske Heating_Mode auf -1 zerstoeren wuerde.
//         decode_vergleich.py gegen v3.10.0: 69552 Zeilen identisch, alle 92
//         Topics in Nummer, Name, Wert und Einheit gleich. decode_hosttest.sh
//         unveraendert gruen. Alle 10 Envs gebaut. Groessen ggue. 3.10.0:
//         ESP32 RAM +0 B/Flash +68 B, ESP8266 RAM +64 B/Flash +64 B.
//
//         ABNAHME NACH DEM OTA. tablesnap.py vor und nach dem Flashen: nur
//         laufende Messwerte abweichend (Aussentemperatur 25 -> 26 C, Vorlauf
//         und Ruecklauf +0,25 bis +0,75 K), keine strukturelle Abweichung.
//
// 3.10.0 - Zwei neue State-Topics: TOP103 Pump_Duty_Max und TOP104
//         Water_Pump_Mode. Damit haben alle Set-Kommandos, fuer die es
//         ueberhaupt ein Antwortbyte gibt, eine Rueckmeldung. Reine Leseseite -
//         am Schreibpfad und an den 90 bestehenden Topics aendert sich nichts
//         (Nachweis decode_vergleich.py --neu).
//
//         WARUM. SET14 WaterPump und SET15 WaterPumpSpeed waren bis hierher die
//         einzigen Kommandos, die sich nicht zurueckpruefen liessen. Die
//         Waermepumpe quittiert nichts und klemmt Werte ausserhalb ihres
//         Bereichs kommentarlos auf den naechsten Rand; ohne Rueckmeldung
//         schreibt eine Steuerung dort blind. Die vollstaendige Gegenueber-
//         stellung steht in SET-TOP-Zuordnung.md.
//
//         GEMESSEN, NICHT ABGELEITET. Beide Byte-Positionen sind am 2026-08-19
//         an WP1 im Hexlog nachgewiesen worden - Wert verstellt, Rohbyte
//         beobachtet, zurueckgestellt (test/byte_monitor.py):
//           Byte 45: set/WaterPumpSpeed 100 -> 110 -> 100 ergab 0x65 -> 0x6F ->
//             0x65. Zweite Kontrolle ohne Schreibvorgang: Stufe 2 ist auf 125
//             konfiguriert und zeigt 0x7E.
//           Byte  4: set/WaterPump 0 -> 1 -> 0 ergab in den Bits 3+4
//             b01 -> b10 -> b01, Rohbyte 0x55 -> 0x65 -> 0x55.
//
//         PUMP_DUTY_MAX, NICHT MAX_PUMP_SPEED. Byte 45 ist die Obergrenze, bis
//         zu der die Pumpe modulieren darf, keine Drehzahl. Bei laufender Pumpe
//         folgte TOP92 Pump_Duty der Grenze exakt: 100 -> Duty 100 bei
//         2300 1/min und 11,95 l/min, 80 -> Duty 80 bei 1500 1/min und
//         6,93 l/min. Der Kommandoname WaterPumpSpeed bleibt unveraendert,
//         weil die Kaskadensteuerung darauf schreibt.
//
//         DER DRITTE PUMPENMODUS IST UNGEMESSEN. WaterPumpMode[] fuehrt
//         "Air purge" fuer b11 aus ProtocolByteDecrypt.md; gemessen sind "Auto"
//         (b01) und "Fix" (b10). Den dritten Zustand herzustellen hiesse, an
//         einer intakten Anlage eine Entlueftungsroutine auszuloesen. Wie bei
//         TOP102 External_SW_State bleibt damit ein Zustand dokumentiert, aber
//         unbelegt.
//
//         NUMBEROFTOPICS 90 -> 92. Die Nummern 103 und 104 schliessen an TOP102
//         an; die Luecken der Nummerierung (Zone 2) bleiben wie sie sind.
//
//         NACHWEIS. decode_vergleich.py gegen v3.9.0: 68040 Zeilen identisch,
//         die 90 bestehenden Topics in Nummer, Name, Wert und Einheit gleich
//         (1512 Zeilen der zwei neuen Topics als erwartet ausgeblendet).
//         decode_hosttest.sh: alle 92 Zeilen mit nullptr abgeschlossen,
//         Water_Pump_Mode hoechster Index 2 bei 3 Eintraegen. Alle 10 Envs
//         gebaut. Groessen ggue. 3.9.0: ESP32 RAM +48 B/Flash +140 B,
//         ESP8266 RAM +144 B/Flash +112 B.
//
// 3.9.0 - Die Weboberflaeche kann nicht mehr ueber ein Klartext-Array hinaus
//         lesen. Das ist Massnahme 3 aus dem Massnahmenplan zur Codedurchsicht
//         vom 2026-08-18, umgesetzt als Weg B (Listen auffuellen) mit einem
//         Zusatz, siehe unten. Am Dekodierpfad aendert sich NICHTS - die
//         publizierten MQTT-Werte sind Byte fuer Byte dieselben (Nachweis
//         decode_vergleich.py). Betroffen ist allein die Anzeige unter
//         /tablerefresh.
//
//         PROBLEM. handleTableRefresh() schlug den Klartext mit dem
//         dekodierten Wert als Index nach und pruefte nur nach unten (-1 fuer
//         unbekannte Rohwerte). Nach oben gab es keine Grenze, und das struct
//         fuehrt keine Array-Laenge mit. Die 2-Bit-Dekodierer liefern nach
//         ihrem -1 aber bis Index 2, die 3-Bit-Dekodierer bis Index 6 -
//         waehrend die meisten Listen nur zwei bzw. vier Eintraege hatten.
//         Ein Rohwert b11 von der Waermepumpe ergab damit einen wilden
//         const char*, den %s formatierte: Absturz AN DER LAUFENDEN ANLAGE,
//         sobald ein Browser die Seite offen hat. Betroffen waren 22 Zeilen,
//         darunter TOP0 (Heatpump_State), TOP17/18 (Powerful/Quiet) und
//         TOP58-61. Fuer die vier Byte-110-Zeilen war die Luecke seit 3.7.0
//         geschlossen, fuer den Rest nicht - der Kommentar in decode.cpp
//         benannte sie selbst.
//
//         FIX, zwei Teile. (1) Jede Liste deckt jetzt den gesamten
//         Indexbereich ihres Dekodierers ab, aufgefuellt mit "unknown" -
//         drei Eintraege bei 2-Bit-, sieben bei 3-Bit-Feldern. (2) Zusaetzlich
//         endet JEDE Liste mit nullptr, und der Nachschlag laeuft ueber die
//         neue Funktion desc_text() in decode.h, die bis zum gesuchten Index
//         hochzaehlt statt direkt zuzugreifen.
//         Warum der Zusatz: Weg B allein haette die Zusicherung nur im Test
//         stehen, nicht im Code - und der Test braucht die Laenge, die das
//         struct nicht kennt. Der nullptr-Abschluss loest beides. Er macht die
//         Grenze im Code wirksam (ein kuenftiger Dekodierer mit groesserem
//         Index zeigt eine leere Zelle, statt das Geraet neu zu starten) und
//         erlaubt dem Hosttest, die Laenge jeder Liste selbst zu bestimmen -
//         ohne eine zweite Liste, die hinter der ersten zurueckbleiben kann.
//         Weg A (desc_count als Feld im StateTopic) wurde verworfen: 90
//         angefasste Tabellenzeilen und Padding-Risiko auf dem ESP8266 fuer
//         dasselbe Ergebnis.
//         desc_text() steht als inline-Funktion im Header, damit Firmware und
//         Hosttest dieselbe Regel benutzen - byte110_test.cpp hatte die
//         Anzeigelogik bis hierher NACHGEBILDET, die Nachbildung entfaellt.
//
//         NACHWEIS.
//         - byte110_test Fall 6 (neu): jede der 90 Zeilen, alle 256 Rohwerte
//           durch den echten Dekodierer; der entstehende Index muss in seiner
//           Liste liegen und dort einen Text finden. 22 Klartext-Zeilen
//           geprueft, hoechster Index je Zeile gegen die Listenlaenge.
//           Ausserdem: alle 90 Listen sind mit nullptr abgeschlossen.
//         - Gegenprobe, wie im Massnahmenplan verlangt: OffOn und Quietmode
//           testweise auf die alte Laenge gekuerzt -> Fall 6 schlaegt an
//           ("Rohwert 0x03 ergibt Index 2, Liste hat 2"), Rueckgabewert 1.
//           Nach dem Rueckbau wieder gruen. Zusaetzlich prueft der Test die
//           Regel an einer bewusst zu kurzen Liste direkt.
//         - decode_vergleich.py --basis v3.8.2: 68040 Zeilen, IDENTISCH -
//           90 Topics, Nummer, Name, Wert und Einheit gleich.
//         - Die drei uebrigen Hosttests (merge, telegramm, sendwindow) gruen.
//         - Alle zehn Envs gebaut. RAM/Flash gegen 3.8.2:
//           ESP32   RAM +160 B (56976 -> 57136), Flash +188 B
//           ESP8266 RAM +152 B (45800 -> 45952), Flash +184 B
//           Das sind die 38 zusaetzlichen Zeiger (26 Abschluesse, 12
//           Auffuellungen) a 4 Byte - auf dem ESP8266 liegen die Listen im
//           RAM. 56,1 % statt 55,9 % belegt.
//
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
static const char* heishamon_version = "3.15.0";
