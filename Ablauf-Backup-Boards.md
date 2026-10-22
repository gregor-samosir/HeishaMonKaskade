# Backup-Boards — der Notanker der Kaskade

Zwei ESP32-S3-Boards liegen als fertige Ersatzplatinen bereit, eines je Stufe.
Sie tragen die produktive Firmware ihrer Stufe und sind bis auf zwei Felder in
der `config.json` mit dem laufenden Board identisch. Dieses Dokument hält fest,
was bei jeder Firmware-Änderung mit ihnen passiert und was im Ernstfall zu tun
ist.

**Stand:** 2026-08-24, zuletzt fortgeschrieben am 2026-08-27. Ersetzt die bisherige Rückfallebene aus ESP8266-Boards und
einem Vollarchiv aller `firmware.bin` — beides wird nicht mehr gepflegt. Der
ESP8266-Pfad ist mit **3.16.0** aus dem Repo entfernt
([`Vorhaben-Nur-ESP32-Pfad.md`](Vorhaben-Nur-ESP32-Pfad.md)).

**Beide Backup-Boards stehen seit dem 2026-08-27** mit Port 1884 und eigenem
Hostnamen (Protokoll unten). **Firmware:** immer dieselbe Version wie die
produktiven Boards — sie werden nach jeder Abnahme nachgezogen (Schritt 6 unter
„Bei jeder Firmware-Änderung"). Eine Versionsnummer steht hier bewusst nicht
mehr: Sie stand auf 3.19.0, als längst 3.22.0 lief.

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

## Kennwerte der Boards

Board | USB-Port | MAC | IP | Einstellung
:--- | :--- | :--- | :--- | :---
`HeishaMon32_h1b` | `usbmodem11301` | `e8:f6:0a:80:1d:48` | 192.168.2.194 | Heisha Stufe 1, Port 1884
`HeishaMon32_h2b` | `usbmodem11401` | `1c:db:d4:bc:61:c8` | 192.168.2.166 | Heisha Stufe 2, Port 1884

Bei jeder Version nachgezogen, immer per OTA über die reservierte IP, ohne die
Boards zu öffnen.
Die Prüfung nach dem Nachziehen ist immer dieselbe: Version, Hostname,
`mqtt_port = 1884`, seit 3.22.0 dazu die KNX-Schnittstelle.
Ein Board ohne Broker meldet auf `/notbetrieb/status` erwartungsgemäß fehlende
Werte und Sperre 1 — das ist der stillgelegte Zustand, kein Befund.

Beide gemeinsame Werte: Broker `192.168.2.147`, Hydraulik-Switch
`192.168.2.180`, KNX-Schnittstelle `192.168.2.127` (seit 3.22.0 an beiden
Stufen Pflicht, weil der Vorderhausschritt in beiden Folgen steht), MQTT-Benutzer und -Passwort leer — abgelesen an den laufenden
Boards `.120` und `.122`.

## Bei jeder Firmware-Änderung

1. **Rettungsanker** als **annotierten** Tag setzen (`git tag -a
   rettungsanker-JJJJ-MM-TT -m "…"`), auf einem Branch arbeiten. Annotiert
   gilt für jeden Tag, auch für die Versions-Tags: Ein einfacher Tag auf einen
   Commit mit alter Adresse wird von GitHub abgelehnt, ein annotierter nicht —
   mit einer einzigen Regel muss niemand prüfen, welcher Fall vorliegt.
   `v3.19.0`, `v3.21.0` und `v3.22.0` sind noch einfache Tags; sie bleiben so,
   weil an ihnen Releases hängen.
2. `./test/hosttests.sh` (alle Hosttests, dieselbe Liste wie die CI), danach
   alle Envs bauen.
3. Wird ein Gerät zum Testen gebraucht, dient **ein Backup-Board** als
   Prüfling — mit der Stufen-Firmware, stillgelegt über Port 1884.
4. **Merge und Tag, vor dem OTA** (Owner-Entscheid 2026-09-17).
   `git merge --no-ff` nach `main`, annotierter Tag `vX.Y.Z` auf den
   Merge-Commit, dann **von `main` aus** bauen. So stammt die Firmware auf den
   Geräten genau aus dem getaggten Stand. **Noch nicht pushen** — scheitert die
   Abnahme, ist nichts öffentlich, und der Rettungsanker aus Schritt 1 ist der
   Rückweg.
5. Baseline mit `test/tablesnap.py` ziehen, OTA auf Stufe 1, abnehmen, dann
   Stufe 2 (Verfahren siehe [`test/README.md`](test/README.md)). Bringt die
   Version ein neues Pflichtfeld mit, wird es direkt nach dem OTA in
   `/settings` gesetzt — seit 3.22.0 gilt das für `knx_schnittstelle` an
   **beiden** Stufen.
6. **Backups direkt nach der Abnahme nachziehen** — beide Boards anstecken, per
   OTA auf dieselbe Version bringen, Port, Hostname und KNX-Schnittstelle
   prüfen, wieder stromlos einlagern. Dieser Schritt ist zugleich der wiederkehrende
   Lebendtest der Ersatzplatinen.
7. **Push und CI.** `main` und den Versions-Tag **einzeln** pushen
   (`git push origin main`, `git push origin vX.Y.Z`) — **nie
   `git push --follow-tags`**: Er schöbe alle annotierten Tags mit hoch, also
   auch die Rettungsanker, die bewusst lokal bleiben. Je Push auf `main` laufen
   zwei Workflows, `CI` und `CodeQL`; maßgeblich ist der Lauf `CI` zum
   Merge-Commit.
8. **Releases.** Öffentlich `vX.Y.Z` mit den Befunden, **ohne** Binaries
   (Passwörter im Abbild). Privat in `HeishaMon-Rollback`: beide Abbilder
   **und** `platformio_user_env_vX.Y.Z.ini` als Assets, den Vorgänger auf
   „Rückfallstand vor X.Y.Z" umbenennen, das Release davor löschen (der Tag
   bleibt). Ein `cp` in den lokalen Ordner `~/HeishaMon-Rollback/` ist
   **keine** Sicherung — nachprüfen mit `gh release view vX.Y.Z --json assets`.
   Einzelheiten stehen im README der privaten Ablage.

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

Zwei Handgriffe — der erste am Board, der zweite vom Handy aus:

1. Am defekten Board den Stecker aus der Buchse `HEATPUMP` ziehen, am Backup
   einstecken. Über denselben Stecker wird das Board versorgt — ein zweites
   Kabel gibt es nicht, „stromlos" und „an WP-Kabel" sind derselbe Handgriff.
   Das Kabel bleibt an der Wärmepumpe, und **die Wärmepumpe bleibt eingeschaltet**
   (Owner-Bestätigung 2026-09-09); abgeschaltet werden muss sie dafür nicht.
2. `http://<IP-Backup>/settings` öffnen, **MQTT-Port auf 1883** setzen, speichern.
   Login ist **`admin` mit dem OTA-Passwort** — `/settings` hängt nicht am
   Notbetriebs-Login.

Der Hostname bleibt wie er ist. Danach im Router die Reservierung nicht
anfassen — das Backup behält seine eigene Adresse und ist ab sofort das
produktive Board seiner Stufe.

**Für die Familie steht derselbe Ablauf als Blatt 3 in der Notfallbox:**
`~/nodered-flows/doku/ernstfall/NOTFALL-3-ERSATZ-BOARD.md`. Es steigt beim Symptom
ein (eine der beiden Bridge-Seiten `.120`/`.122` lädt nicht, die andere schon),
wählt das Ersatz-Board über die IP auf dem Etikett, prüft die Stufe auf der
Startseite gegen und stellt den Port über das Settings-Formular um. Blatt 1 und 2
verweisen darauf. **Ändert sich hier etwas am Ablauf, muss Blatt 3 mit** — sonst
steht in der Notfallbox ein Ausdruck, der nicht mehr stimmt.

## Was damit entfällt

* **Der ESP8266-Pfad.** Die vier `d1_mini_*`-Envs, `esp8266_base`,
  `stage_test_esp8266` und die Plattformschicht dafür sind mit 3.16.0 entfernt.
  Die CI baut seitdem sechs Envs.
* **Das Binärarchiv der Rückfallstände** — als *Archiv*. Am 2026-08-27 auf zwei
  Releases eingedampft: das aktuelle und das davor. Elf ältere Releases samt ihren
  Tags und 62 lokale Abbilder sind gelöscht, 60 MB auf 5,8 MB. Am 2026-08-31
  wieder auf zwei gebracht (v3.19.0 und v3.18.0; v3.15.0 bis v3.17.0 samt Tags
  entfernt) — die Regel gilt fortlaufend, nicht einmalig.

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
