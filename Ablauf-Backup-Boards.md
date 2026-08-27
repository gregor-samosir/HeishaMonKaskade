# Backup-Boards — der Notanker der Kaskade

Zwei ESP32-S3-Boards liegen als fertige Ersatzplatinen bereit, eines je Stufe.
Sie tragen die produktive Firmware ihrer Stufe und sind bis auf zwei Felder in
der `config.json` mit dem laufenden Board identisch. Dieses Dokument hält fest,
wie sie eingerichtet werden, was bei jeder Firmware-Änderung mit ihnen passiert
und was im Ernstfall zu tun ist.

**Stand:** 2026-08-24, fortgeschrieben am 2026-08-27. Ersetzt die bisherige
Rückfallebene aus archivierten `firmware.bin`-Dateien und ESP8266-Boards —
beides wird nicht mehr gepflegt. Der ESP8266-Pfad ist mit **3.16.0** aus dem
Repo entfernt ([`Vorhaben-Nur-ESP32-Pfad.md`](Vorhaben-Nur-ESP32-Pfad.md)); die
vier OTA-Abbilder der 3.15.0 liegen eingefroren im privaten Release und bleiben
flashbar, bis die Backup-Boards stehen.

## Die vier Boards

Rolle | Env | Prefix | Hostname | MQTT-Port
:--- | :--- | :--- | :--- | ---:
Produktiv Stufe 1 (WP1) | `heishamon_esp32_h1_ota` | `panasonic_heat_pump` | `HeishaMon32_h1` | 1883
Produktiv Stufe 2 (WP2) | `heishamon_esp32_h2_ota` | `panasonic_heat_pump2` | `HeishaMon32_h2` | 1883
**Backup Stufe 1** | derselbe Build wie oben | `panasonic_heat_pump` | `HeishaMon32_h1b` | **1884**
**Backup Stufe 2** | derselbe Build wie oben | `panasonic_heat_pump2` | `HeishaMon32_h2b` | **1884**

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

1. **Erstflash über USB** — zwingend, die Original-Firmware bringt eine andere
   Partitionstabelle mit, die per OTA nicht getauscht werden kann:
   `pio run -e heishamon_esp32_h1_usb -t upload` (bzw. `_h2_usb`).
2. **Setup-Hotspot** `HeishaMon-Setup` (WPA2-Passwort aus der Notfallunterlage),
   Portal unter `192.168.4.1`, Timeout 180 s. Die WLAN-Daten der
   Original-Firmware werden **nicht** übernommen, das LittleFS ist leer.
3. Im Portal eintragen: WLAN, Broker und MQTT-Zugangsdaten wie beim produktiven
   Board (dort unter `/settings` abzulesen), **`mqtt port` sofort auf 1884**,
   Hostname `HeishaMon32_h1b` bzw. `HeishaMon32_h2b`.
4. **DHCP-Reservierung** im Router setzen, sobald das Board seine Adresse
   bekommen hat.
5. IP und das Notbetriebspasswort auf das ausgedruckte Notfallblatt der Familie
   schreiben. Erst damit ist das Board fertig.
6. Stromlos in den Schrank. Der Funktionsnachweis kommt bei jeder Änderung
   ohnehin von selbst (Schritt 5 unten).

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
* **Das Binärarchiv der Rückfallstände.** Ein Rollback heißt jetzt: Board
  tauschen oder aus dem Git-Tag neu bauen. Firmware-Binaries mit AP- und
  Notbetriebspasswort müssen dafür nicht mehr aufbewahrt werden.

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
