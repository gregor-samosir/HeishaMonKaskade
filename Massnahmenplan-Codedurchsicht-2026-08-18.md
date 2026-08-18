# Maßnahmenplan zur Codedurchsicht vom 2026-08-18

Ergebnis der Durchsicht der gesamten Firmware (`src/`), der PlatformIO-Konfiguration
und der Testwerkzeuge, mit Fokus auf dem ESP32-Teil, den Eigenheiten der
Panasonic-Schnittstelle und dem Zusammenspiel mit der Node-RED-Steuerung.
Die vier Hosttests (`merge_test`, `telegramm_test`, `sendwindow_test`,
`byte110_test`) liefen dabei lokal grün.

**Gesamtbild:** Sendepfad, Telegrammprüfung, Bitmasken-Merge und die
überlaufsichere Zeitrechnung sind belastbar — die Timer-Kette
(Mainquery → Sammelfenster → Senden → Lesen/Timeout → Mainquery) terminiert in
jedem geprüften Pfad. Es bleiben drei Befunde und vier Kleinpunkte.

## Vorgehen

Nach Projektkonvention ([Rettungsanker](README.md)):

1. Vor der Umsetzung Git-Tag auf den Ausgangsstand setzen (Rückfallebene).
2. Je Maßnahme ein eigener Branch, eigene Versionsnummer, Changelog-Eintrag in
   `src/version.h` mit Problem, Nachweis und RAM/Flash-Delta (alle 10 Envs bauen).
3. Abnahme nach dem Flashen wie üblich: Baseline über `/tablerefresh` ziehen und
   zeilenweise gegen den Stand nach dem OTA halten.

Vorgeschlagene Bündelung:

| Version | Inhalt | Art |
| --- | --- | --- |
| 3.8.1 | Maßnahme 1 + 2 (je eine Zeile, kein Protokoll-/Topic-Einfluss) | Fix |
| 3.9.0 | Maßnahme 3 (Tabellenerweiterung + Testausbau) | Umbau |
| — | Kleinpunkte: erst Entscheidung, dann ggf. eigene Version | Entscheid |

---

## Maßnahme 1 — Setup-AP mit Passwort schützen

**Priorität: hoch. Aufwand: eine Zeile.**

**Problem:** `wifiManager.autoConnect("HeishaMon-Setup")` in
`src/webfunctions.cpp` öffnet das Konfigurationsportal als **offenen** AP, und
die Parameterfelder sind mit den echten Werten vorbefüllt — darunter
OTA-Passwort und MQTT-Passwort. Das ist kein Erstboot-Thema: Der
WLAN-Watchdog startet nach 5 min Ausfall neu, `autoConnect` scheitert nach
10 s und öffnet dann für 180 s das offene Portal — zyklisch für die Dauer
eines Router-Ausfalls. Jeder in Funkreichweite kann in diesem Fenster die
Zugangsdaten lesen oder dem Gerät einen fremden MQTT-Broker unterschieben.

**Fix:** `autoConnect("HeishaMon-Setup", "<AP-Passwort>")` — WPA2, mindestens
8 Zeichen. Das Passwort gehört **nicht** in git, sondern als Build-Flag in
`platformio_user_env.ini` (gleiches Muster wie Ports und IPs), mit Fallback im
Code wie bei `HEISHA_HOSTNAME`.

**Nachweis:** Am ESP32-Testgerät (`heishamon_esp32_usb`, eigenes MQTT-Prefix)
die hinterlegte SSID unerreichbar machen → das Portal muss WPA2 verlangen;
mit Passwort verbinden und prüfen, dass die Konfiguration weiter funktioniert.

**Folgeaufgabe:** AP-Passwort in die Notfall-Unterlage für die Familie
aufnehmen — sonst ist im Ernstfall genau der Rettungsweg versperrt.

---

## Maßnahme 2 — LWT „Online" mit Retain-Flag publizieren

**Priorität: mittel (wird hoch, sobald ein echter Broker kommt). Aufwand: eine Zeile.**

**Problem:** `mqtt_reconnect()` in `src/HeishaMon.cpp` publiziert `Online`
ohne Retain-Flag, während das Will `Offline` retained hinterlegt ist. Auf
einem echten Broker bleibt nach jedem Reconnect `Offline` als retained Wert
stehen: Ein Abonnent, der sich später verbindet (Node-RED-Neustart, Wächter),
hält die Stufe für tot, obwohl sie läuft. Heute maskiert der
ioBroker-MQTT-Adapter das (er bedient neue Abonnenten aus seiner State-DB,
siehe `test/README.md`), aber der Umzug auf einen mosquitto steht als Option
im Raum — und der Kaskaden-Wächter ist genau der Konsument, der darauf
hereinfiele.

**Fix:** `mqtt_client.publish(Topics::WILL.c_str(), "Online", true)`.

**Nachweis:** Gegen einen lokalen mosquitto (Container, docker-compose):
Testgerät verbinden lassen, danach mit einem **neuen** Abonnenten
`<prefix>/info/LWT` abonnieren → es muss retained `Online` kommen; Testgerät
stromlos machen → nach Ablauf des Keepalive retained `Offline`.

---

## Maßnahme 3 — `desc[]`-Zugriff nach oben begrenzen

**Priorität: mittel. Aufwand: Tabellenerweiterung + Testausbau (halber Tag).**

**Problem:** `handleTableRefresh()` in `src/webfunctions.cpp` fängt beim
Klartext-Nachschlag nur **negative** Indizes ab. Der Kommentar in
`src/decode.cpp` benennt die Lücke selbst, geschlossen wurde sie aber nur
für die beiden Byte-110-Arrays (drittes Element „unknown"). Offen bleiben:

* alle 2-Bit-Felder mit 2er-Arrays — Rohwert `b11` ergibt Index 2
  (`OffOn`, `DisabledEnabled`, `Valve`, `BlockedFree`, `InactiveActive`,
  `HeatCoolModeDesc`; z. B. TOP0, TOP20, TOP58–61),
* die 3-Bit-Dekodierer von TOP17/TOP18 — Index bis 6 bei 4er-Arrays
  (`Powerfulmode`, `Quietmode`).

Gelesen wird dann ein wilder `const char*`, formatiert per `%s` — das kann
das Gerät **an der laufenden Wärmepumpe** abstürzen lassen, sobald ein
Browser die Seite offen hat und die WP einen unerwarteten Rohwert liefert.
Die Erfahrung dieses Projekts (Byte 110, Kurvengrenzen) zeigt, dass genau das
vorkommt: Die WP liefert Werte außerhalb des bisher Beobachteten.

**Fix (im Stil des Projekts, zwei gleichwertige Wege — vor Umsetzung wählen):**

* **A — Zählspalte:** `desc_count` als weiteres `byte`-Feld in `StateTopic`,
  `handleTableRefresh` klemmt auf `value < desc_count`. Vorteil: eine
  zentrale Prüfung; Nachteil: 90 Zeilen anfassen, auf dem ESP8266 je Zeile
  ggf. Padding (Feldreihenfolge beachten, siehe Kommentar in `decode.h`).
* **B — Arrays auffüllen:** jedes Klartext-Array bis zum Maximalindex seines
  Dekodierers mit `"unknown"` auffüllen (wie bei Byte 110 geschehen).
  Vorteil: kein Strukturumbau; Nachteil: die Zusicherung steht nicht im Code,
  sondern muss per Test gehalten werden.

In beiden Fällen: **keine Änderung am Dekodierpfad selbst** — die
MQTT-Werte bleiben identisch.

**Nachweis:**

* `byte110_test.cpp` Fall 4 („Anzeigeindex bleibt im Array") auf die
  **gesamte** Tabelle ausweiten: für jede Zeile alle 256 Rohwerte durch den
  echten Dekodierer schicken, der entstehende Index muss innerhalb des
  Arrays bleiben (bzw. unter `desc_count`). Gegenprobe: mit einem bewusst
  zu kurzen Array muss der Test brechen.
* `test/decode_vergleich.py --basis v3.8.0`: alle Zeilen identisch —
  belegt, dass sich an den publizierten Werten nichts geändert hat.
* RAM/Flash-Delta je Env in den Changelog (ESP8266 im Blick behalten:
  const-Tabellen liegen dort im RAM).

---

## Kleinpunkte — erst entscheiden, dann umsetzen

**K1 — Telnet (Port 23) ohne Auth, `R` löst Reboot aus.** Der Web-`/reboot`
verlangt Login, Telnet nicht — inkonsequent. Optionen: (a) bewusst so lassen
(Heimnetz, dokumentieren), (b) Reboot-Kommando aus dem Telnet-Menü nehmen,
(c) Telnet abschaltbar machen. Empfehlung: (b) — kostet nichts, die
Log-Toggles bleiben nutzbar, Reboot geht weiter über die geschützte Web-UI.

**K2 — `SUBSCRIBE_GRACE` (5 s) ist lastabhängig.** Braucht der
ioBroker-Replay nach dem SUBACK einmal länger als 5 s (großer Objektbaum,
Systemlast), laufen alte Sollwerte wieder durch — der gemessene
55-Grad-Fall. Kein Codefehler; die Bilanzmeldung im MQTT-Log ist das
Frühwarnsignal. Maßnahme: keine Codeänderung, aber beim nächsten
ioBroker-Update einmal bewusst aufs Log schauen, ob die Bilanzmeldung
nach dem Reconnect **vor** den ersten echten Kommandos kommt.

**K3 — State-Publish-Rückgabewert unbeachtet** (`publish_heatpump_data`).
Schlägt ein Publish fehl, gilt der Wert trotzdem als gesendet und kommt erst
mit der nächsten Änderung oder dem 5-min-Vollupdate wieder. Das Vollupdate
deckelt den Schaden auf 5 min — bewusst so lassen ist vertretbar.
Entscheid dokumentieren, keine Änderung vorgeschlagen.

**K4 — `write_mqtt_log` gegen den PubSubClient-Puffer.** `log_msg` fasst
256 Bytes, der Paketpuffer aber auch nur 256 inklusive Topic und Header —
Meldungen über ~225 Zeichen würden still verworfen. Aktuell erreicht keine
Meldung die Grenze. Billigste Absicherung, falls gewünscht: Rückgabewert von
`publish()` prüfen und im Telnet-Log vermerken. Sonst: als bekannte Grenze
hier dokumentiert lassen.

---

## Ausdrücklich geprüft und in Ordnung

Damit die nächste Durchsicht nicht von vorn anfängt — diese Punkte wurden
gezielt untersucht und **nicht** bemängelt:

* Telegramm-Längen und alle Byte-Positionen der Tabellen (max. Position 198
  bei 203 Bytes), `mainQuery`/`mainCommand` exakt 110 Bytes.
* Masken und Konvertierung aller 32 Set-Kommandos gegen ihre Bitgruppen.
* Die Opmode-Asymmetrie (Set 24/40, Read 25/41) — Protokolleigenschaft,
  deckungsgleich mit dem Original.
* `millis()`-Arithmetik an allen Vergleichsstellen (Karenzfenster,
  Sammelfenster, Watchdog, Reconnect-Backoff).
* Timer-Kette auf Abrisspfade: terminiert auch bei fehlgeschlagenem
  `build_heatpump_command`, Deferral am Limit und aktivem Karenzfenster.
* ESP32-Spezifika: `WiFi.setSleep(false)`, RX-Puffer 256 ≥ 203,
  USB-CDC-Konsole parallel zur WP-UART, Partitionswahl fürs 4-MB-Board.
