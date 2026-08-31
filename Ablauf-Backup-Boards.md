# Backup-Boards — der Notanker der Kaskade

Zwei ESP32-S3-Boards liegen als fertige Ersatzplatinen bereit, eines je Stufe.
Sie tragen die produktive Firmware ihrer Stufe und sind bis auf zwei Felder in
der `config.json` mit dem laufenden Board identisch. Dieses Dokument hält fest,
wie sie eingerichtet werden, was bei jeder Firmware-Änderung mit ihnen passiert
und was im Ernstfall zu tun ist.

**Stand:** 2026-08-24, fortgeschrieben am 2026-08-27 nach der
Erstinbetriebnahme. Ersetzt die bisherige Rückfallebene aus ESP8266-Boards und
einem Vollarchiv aller `firmware.bin` — beides wird nicht mehr gepflegt. Der
ESP8266-Pfad ist mit **3.16.0** aus dem Repo entfernt
([`Vorhaben-Nur-ESP32-Pfad.md`](Vorhaben-Nur-ESP32-Pfad.md)).

**Beide Backup-Boards stehen seit dem 2026-08-27** mit Port 1884 und eigenem
Hostnamen (Protokoll unten). **Firmware: 3.19.0** — nachgezogen am 2026-08-31
über die reservierten IPs, Port und Hostname dabei unverändert geprüft
(`h1b` → 1884, `h2b` → 1884, beide mit ihrem eigenen Hostnamen).

Die Rückfallebene ist damit **zweiteilig**, und beide Teile werden gebraucht:
die Boards gegen den Hardware-Ausfall, das Abbild der Vorversion gegen einen
Firmware-Fehler. Warum das eine das andere nicht ersetzt, steht unten unter
„Was damit entfällt".

## Die vier Boards

Rolle | Env | Prefix | Hostname | IP | MQTT-Port
:--- | :--- | :--- | :--- | :--- | ---:
Produktiv Stufe 1 (WP1) | `heishamon_esp32_h1_ota` | `panasonic_heat_pump` | `HeishaMon32_h1` | 192.168.2.120 | 1883
Produktiv Stufe 2 (WP2) | `heishamon_esp32_h2_ota` | `panasonic_heat_pump2` | `HeishaMon32_h2` | 192.168.2.122 | 1883
**Backup Stufe 1** | derselbe Build wie oben | `panasonic_heat_pump` | `HeishaMon32_h1b` | 192.168.2.194 | **1884**
**Backup Stufe 2** | derselbe Build wie oben | `panasonic_heat_pump2` | `HeishaMon32_h2b` | 192.168.2.166 | **1884**

Es gibt **keinen eigenen Backup-Build**. Ein Backup-Board unterscheidet sich vom
produktiven Board nur durch seine `config.json`, und dort nur in `mqtt_port` und
`wifi_hostname`. Alles andere — Broker, Zugangsdaten, WLAN — ist gleich.

## Warum Port 1884 und ein eigener Hostname

Der MQTT-Prefix ist ein Build-Flag (`HEISHA_MQTT_PREFIX` in `platformio.ini`).
Ein Backup mit Stufen-Firmware sitzt also auf denselben Topics wie das laufende
Board — es darf den Broker deshalb nie erreichen, solange es Backup ist.

* **`mqtt_port = 1884`** ist die Barriere. Auf dem toten Port antwortet der
  Broker mit einem sofortigen Connection-refused; es gibt weder ein Publish noch
  ein Subscribe und keinen Timeout-Hänger. Das Verfahren ist beim Boardtausch im
  August 2026 erprobt worden.
* **Abweichender Hostname** ist die zweite, kostenlose Sicherung. Der Hostname
  ist zugleich die MQTT-Client-ID (`mqtt_client.connect(wifi_hostname, …)` in
  [`src/HeishaMon.cpp`](src/HeishaMon.cpp)). Zwei Boards mit derselben Client-ID
  würden sich beim Broker gegenseitig hinauswerfen — mit eigenem Hostnamen kann
  das selbst dann nicht passieren, wenn der Port einmal falsch steht.

Das LWT-Topic hängt am Prefix, nicht am Hostnamen. Der abweichende Hostname darf
deshalb auch im Ernstfall stehen bleiben; ioBroker merkt davon nichts.

## Einrichtung eines neuen Backup-Boards

> **Korrigiert am 2026-08-27 an den zwei echten Boards.** Hier stand bis dahin,
> der USB-Erstflash lasse ein leeres LittleFS zurück und das Board komme im
> Setup-Hotspot hoch, die WLAN-Daten der Original-Firmware würden **nicht**
> übernommen. **Das stimmt nicht.** Beide Boards übernahmen die `config.json`
> der Original-Firmware samt WLAN-Zugang, gingen ohne Portal direkt ins Netz
> und waren sofort per HTTP erreichbar. Der Ablauf unten ist der nachgemessene.

Der Erstflash über USB ist zwingend: Die Original-Firmware bringt eine andere
Partitionstabelle mit, die per OTA nicht getauscht werden kann. Das LittleFS
liegt bei beiden an derselben Stelle und überlebt den Flash — deshalb greift
`resetSettings()` in [`src/webfunctions.cpp`](src/webfunctions.cpp) nicht, und
deshalb ist der erste Flash **nicht** der der Stufe:

**Erster Flash ist die Test-Firmware.** Sie trägt das Prefix
`panasonic_heat_pump32`; das Board kann damit nichts anrichten, egal was in der
übernommenen `config.json` steht. Die Stufen-Firmware kommt erst, wenn Port und
Hostname nachweislich stehen.

1. **Test-Firmware über USB:**
   `pio run -e heishamon_esp32_usb -t upload --upload-port /dev/cu.usbmodemXXXXX`
2. **Serielle Konsole mitlesen** (115200 Baud). Das Board gibt die geladene
   `config.json` und seine IP aus. Nur wenn die Datei unbrauchbar ist, ruft die
   Firmware `resetSettings()` und öffnet den Setup-Hotspot `HeishaMon-Setup`
   (Portal `192.168.4.1`, Timeout 180 s, WPA2-Passwort aus der
   Notfallunterlage) — dann sind WLAN und die Felder aus Schritt 3 dort
   einzutragen.
3. **Einstellungen per HTTP setzen, in zwei Schritten.** Die Reihenfolge ist
   kein Formalismus: Nach Schritt 3a ist der Port tot, erst 3b trägt den Broker
   ein. So gibt es keinen Moment, in dem eine Broker-Adresse steht und der Port
   noch auf dem Default 1883 hängt.

   ```bash
   # 3a - Sperre und Identität (Werte für Stufe 2: HeishaMon32_h2b)
   curl -u admin:<ota-pw> "http://<IP>/settings?mqtt_port=1884&wifi_hostname=HeishaMon32_h1b&hydraulik_switch=<Tasmota-IP>"
   # 3b - erst jetzt der Broker
   curl -u admin:<ota-pw> "http://<IP>/settings?mqtt_server=<Broker-IP>"
   ```

   `/settings` schreibt die `config.json` bei jedem Aufruf vollständig neu und
   übernimmt dabei nur die sieben bekannten Felder — die Reste der
   Original-Firmware (`ntp_servers`, `use_1wire`, `s0_*` …) verschwinden von
   selbst. Nicht übergebene Felder behalten ihren Wert. `new_ota_password`
   deshalb **nicht** mitschicken, sonst verlangt der Handler das aktuelle
   Passwort zur Gegenprobe. Jeder Aufruf startet das Gerät neu.
4. **Nachweis, dass die Sperre wirkt** — in der seriellen Konsole:
   `NetworkClient.cpp:278] connect(): socket error … "Connection reset by peer"`
   im 10-Sekunden-Takt. Vor Schritt 3b steht dort `DNS Failed for ''`, danach
   das Connection-refused. Beides heißt: kein Publish, kein Subscribe.
5. **Stufen-Firmware per OTA**, nicht über USB:
   `pio run -e heishamon_esp32_h1_ota -t upload --upload-port <IP>` (bzw. `_h2_ota`).
   OTA fasst das LittleFS garantiert nicht an, und der Weg ist derselbe, den
   das Nachziehen später braucht — er ist damit gleich mitgeprüft.
6. **Gegenprobe über HTTP**, bevor das Board weggelegt wird:
   `/settings` muss Port **1884** und den Hostnamen `…_h1b`/`…_h2b` zeigen, die
   Startseite die richtige Stufe (`Heisha Stufe 1`/`2`) und die Version.
7. **DHCP-Reservierung** im Router auf die MAC setzen.
8. IP und das Notbetriebspasswort auf das ausgedruckte Notfallblatt der Familie
   schreiben. Erst damit ist das Board fertig.
9. Stromlos in den Schrank. Der Funktionsnachweis kommt bei jeder Änderung
   ohnehin von selbst (Schritt 5 unten).

### Warum der Umweg über die Test-Firmware

Ohne ihn wäre der erste Flash die Stufen-Firmware — und das Board käme mit
**produktivem Prefix** im WLAN hoch, konfiguriert nach einer `config.json`, die
niemand gesehen hat. Ob es dann den Broker erreicht, entscheidet allein, was die
Original-Firmware dort hinterlassen hat. Am 2026-08-27 war `mqtt_server` bei
beiden Boards leer, der Port stand auf dem Default 1883. Das ging gut — aber es
ging gut aus Zufall, nicht aus Konstruktion. Ein Board mit eingetragenem Broker
hätte sofort auf den produktiven Topics gesessen, ohne an einer Wärmepumpe zu
hängen.

### Protokoll der Erstinbetriebnahme (2026-08-27)

Board | USB-Port | MAC | IP | Ergebnis
:--- | :--- | :--- | :--- | :---
`HeishaMon32_h1b` | `usbmodem11301` | `e8:f6:0a:80:1d:48` | 192.168.2.194 | 3.16.0, Heisha Stufe 1, Port 1884
`HeishaMon32_h2b` | `usbmodem11401` | `1c:db:d4:bc:61:c8` | 192.168.2.166 | 3.16.0, Heisha Stufe 2, Port 1884

Seither nachgezogen: **3.17.0** (2026-08-28) und **3.18.0** (2026-08-30), beide
Male per OTA über die reservierte IP, ohne die Boards zu öffnen. Die Prüfung
nach dem Nachziehen ist immer dieselbe: Version, Hostname, `mqtt_port = 1884`.
Ein Board ohne Broker meldet auf `/notbetrieb/status` erwartungsgemäß fehlende
Werte und Sperre 1 — das ist der stillgelegte Zustand, kein Befund.

Beide gemeinsame Werte: Broker `192.168.2.147`, Hydraulik-Switch
`192.168.2.180`, MQTT-Benutzer und -Passwort leer — abgelesen an den laufenden
Boards `.120` und `.122`, die während der ganzen Inbetriebnahme unberührt
weiterliefen (gegengeprüft über `/tablerefresh` an Stufe 1).

## Bei jeder Firmware-Änderung

1. Rettungsanker-Tag setzen, auf einem Branch arbeiten.
2. Hosttestliste der CI vollständig lokal fahren, danach alle Envs bauen.
3. Wird ein Gerät zum Testen gebraucht, dient **ein Backup-Board** als
   Prüfling — mit der Stufen-Firmware, stillgelegt über Port 1884.
4. Baseline mit `test/tablesnap.py` ziehen, OTA auf Stufe 1, abnehmen, dann
   Stufe 2 (Verfahren siehe [`test/README.md`](test/README.md)).
5. **Backups direkt nach der Abnahme nachziehen** — beide Boards anstecken, per
   OTA auf dieselbe Version bringen, Port und Hostname prüfen, wieder
   stromlos einlagern. Dieser Schritt ist zugleich der wiederkehrende
   Lebendtest der Ersatzplatinen.

Nachziehen ohne eigenes Env, über die reservierte IP des Backups:

```bash
pio run -e heishamon_esp32_h1_ota -t upload --upload-port <IP-Backup-h1>
```

## Rückweg nach einem Test

Ein Board, das als Prüfling gedient hat, ist erst wieder Backup, wenn diese drei
Zeilen erledigt sind:

```bash
# 1. Stufen-Firmware drauf (falls zwischendurch etwas anderes lief)
pio run -e heishamon_esp32_h1_ota -t upload --upload-port <IP-Backup-h1>

# 2. MQTT stilllegen
curl -u admin:<pw> "http://<IP-Backup-h1>/settings?mqtt_port=1884"

# 3. Nachweisen, dass 1884 wirklich steht
curl -s -u admin:<pw> "http://<IP-Backup-h1>/settings" | grep mqtt_port
```

`/settings` übernimmt die übrigen Felder unverändert, es genügt der eine
Parameter.

## Im Ernstfall

Zwei Handgriffe, vom Handy aus machbar:

1. Defektes Board stromlos, Backup an WP-Kabel und Strom.
2. `http://<IP-Backup>/settings` öffnen, **MQTT-Port auf 1883** setzen, speichern.

Der Hostname bleibt wie er ist. Danach im Router die Reservierung nicht
anfassen — das Backup behält seine eigene Adresse und ist ab sofort das
produktive Board seiner Stufe.

## Was damit entfällt

* **Der ESP8266-Pfad.** Die vier `d1_mini_*`-Envs, `esp8266_base`,
  `stage_test_esp8266` und die Plattformschicht dafür sind mit 3.16.0 entfernt.
  Die CI baut seitdem sechs Envs.
* **Das Binärarchiv der Rückfallstände** — als *Archiv*. Am 2026-08-27 auf zwei
  Releases eingedampft: das aktuelle und das davor. Elf ältere Releases samt ihren
  Tags und 62 lokale Abbilder sind gelöscht, 60 MB auf 5,8 MB.

### Richtigstellung (2026-08-27): ein Rückfallstand bleibt nötig

Hier stand, ein Rollback heiße jetzt „Board tauschen oder aus dem Git-Tag neu
bauen", Binaries müssten nicht mehr aufbewahrt werden. **Das greift zu kurz.**
Die Backup-Boards decken den *Hardware*-Ausfall ab — den *Firmware*-Rollback
decken sie konstruktiv nicht ab: Nach der Regel oben („Backups direkt nach der
Abnahme nachziehen, kein Sicherheitsversatz") tragen nach jedem Update alle vier
Boards dieselbe Version. Ein Fehler, der erst nach Tagen auffällt, steckt dann
in allen vieren.

Deshalb bleibt **die jeweils vorige Version** als Abbild liegen. Alles davor
kann weg — auf 3.7.0 will niemand zurück.

Und der zweite Weg, „aus dem Git-Tag neu bauen", hatte eine Lücke, die niemand
notiert hatte: Er setzt `platformio_user_env.ini` voraus, und die lag bis zum
2026-08-27 **ausschließlich lokal** auf dem Entwicklungsrechner — in keinem der
beiden Repos. Ohne sie entsteht zwar eine lauffähige Firmware, aber mit anderem
AP- und Notbetriebspasswort, und das ausgedruckte Blatt in der Notfallbox wäre
falsch. Sie hängt jetzt als Asset am Release und gehört künftig an jedes;
Begründung und Ablauf stehen im README der privaten Ablage.

### Richtigstellung (2026-08-25): der Test-Prefix bleibt

An dieser Stelle stand, mit dem toten Port entfalle auch der eigene Test-Prefix
`panasonic_heat_pump32` samt `stage_test_esp32` und den Envs
`heishamon_esp32_usb`/`_ota`. **Das gilt nicht.** Ein toter MQTT-Port sperrt ein
Board gegen den Broker — aber genau deshalb ersetzt er nicht die Möglichkeit,
mit **erreichbarem** Broker zu testen, ohne auf den produktiven Topics zu
sitzen. Das Prüfstand-Protokoll vom 2026-08-21
([`Vorhaben-Notbetrieb-Weboberflaeche.md`](Vorhaben-Notbetrieb-Weboberflaeche.md))
brauchte genau das.

Test-Prefix und die beiden Test-Envs bleiben deshalb erhalten. Sie sind der
Grund, warum ein geliehenes Backup-Board **doppelt** gesperrt ist: falsches
Prefix *und* toter Port. Der vollständige Ausleih- und Rückgabeablauf steht in
[`test/README.md`](test/README.md) unter „Prüfstand aufsetzen"; die Reihenfolge
bei der Rückgabe ist sicherheitskritisch.
