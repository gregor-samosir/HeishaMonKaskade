# Arbeitsplan: KNX-Schritt „Vorderhaus“ in der Firmware

> **Übergabe für eine neue Session — Stand 2026-09-12 abends.**
> Recherche, Vorabtest, Entwurf und Rückleseregel sind abgeschlossen und
> vom Owner entschieden. Die Firmware steht unverändert auf 3.20.0. Offen ist
> die Umsetzung; dieser Plan führt vom ersten Befehl bis zum Rollout.
> **Ergebnis: offen.**
>
> **2026-09-13:** E1–E4 und drei Folgepunkte entschieden (Abschnitt
> „Entschieden am 2026-09-13“), dazu die Farbe des Vorderhausfalls neu:
> GRÜN mit orangem Hinweisfeld (siehe dort). Auf dem Branch `knx-vorderhaus`
> sind die Schritte 1–7 erledigt (Firmware 3.21.0, alle Hosttests grün);
> offen sind Schritt 8 (alle Envs), 9 (Prüfling, Anlage) und 10 (Merge,
> Rollout).
>
> **2026-09-13 abends:** Schritt 8 erledigt, der Testzugang für Schritt 9
> gebaut. **Nächste Session: Abschnitt „Übergabe für die Test-Session“
> direkt hierunter.**
>
> **2026-09-13 spät: Schritt 9 erledigt.** Alle 13 Simulatorläufe und drei
> Läufe an Mischer und Pumpe (Regelfall, zweiter Druck, Rückfall) wie
> erwartet; die Anlage ist zurückgestellt, h1b trägt wieder den Stufen-Build
> 3.21.0 mit `knx_schnittstelle = 192.168.2.127`. Protokoll in
> `Ablauf-Notbetrieb.md`, Abschnitt 1c.
>
> **Ergebnis (2026-09-13 abends): ausgerollt.** Schritt 10 erledigt — nach
> `main` gemergt, Tag `v3.21.0`, per OTA auf H1 und H2. Abnahme mit
> `tablesnap.py`: Tabellenaufbau auf beiden Stufen identisch, abweichend nur
> je ein laufender Messwert; Notbetriebsknopf bereit (H1 11 Schritte, H2 6).
> An H1 ist `knx_schnittstelle = 192.168.2.127` gesetzt; an H2 nicht, weil
> 3.21.0 den Schritt dort nicht kannte. Ein ganzer Notbetriebslauf an H1
> entfiel (Owner-Entscheid). **Offen außerhalb dieses Repos:** die Anleitung
> zum Mischer (Owner), danach die Arbeitsanweisung für den KNX-Re-Assert in
> `nodered-flows` (Claude).
>
> **2026-09-14: Nachtrag 3.22.0 (Branch `knx-vorderhaus-h2`).** Der Owner
> hat bei der Durchsicht bemerkt, dass der Schritt nur an H1 läuft. „Nur
> Rolle Heizen“ stand als Owner-Entscheid in der Tabelle unten, war aber eine
> Annahme des Entwurfs (Analyse §8) und nie vorgelegt worden. Neu entschieden:
> **Der Schritt läuft an beiden Stufen**, weil niemand weiß, welcher Knopf im
> Ernstfall zuerst gedrückt wird — ein zweiter Lauf ist unschädlich. **Fällig
> ist er nur bei Heizbetrieb** (TOP101 = 0): An H2 läuft die Warmwasser-Folge
> auch im Kühlbetrieb, dort entfällt er mit Hinweis unter GRÜN statt die
> Zwangsstellung des Sommers aufzuheben. **`knx_schnittstelle` ist damit an
> allen vier Boards Pflicht** und wird bei jedem Rollout gesetzt.

## Übergabe für die Test-Session (Schritt 9)

**Lage.** Branch `knx-vorderhaus` ist ausgecheckt, neun Commits über `main`,
nichts gepusht, nichts geflasht. Firmware 3.21.0, Schritte 1–8 erledigt; die
Nachweise stehen in den Commit-Messages und in `src/version.h`. Die Session
dafür wurde getrennt, um einem Auto-Compact zuvorzukommen — hier steht alles,
was sie braucht.

**Das Board: h1b** (Backup-Board der Stufe 1), vom Owner per USB an den Mac
gehängt. Solange es Prüfling ist, hat Stufe 1 keinen Notanker.

1. **Port prüfen:** `~/.platformio/penv/bin/pio device list`. Der Owner sah
   h1b am 2026-09-13 abends als **`/dev/cu.usbmodem1101`**;
   `platformio_user_env.ini` erwartet `/dev/cu.usbmodem11101` (der Mac
   nummeriert neu). Beim Flashen deshalb `--upload-port
   /dev/cu.usbmodem1101` angeben — die Datei nicht ändern.
2. **Flashen:** `pio run -e heishamon_esp32_usb -t upload` — Prefix
   `panasonic_heat_pump32`, mit Testzugang. Der MQTT-Port in der
   `config.json` bleibt **1884**: Der Test braucht keinen Broker, das Board
   bleibt doppelt gesperrt (`test/README.md`, „Prüfstand aufsetzen“).
3. **Adresse des Prüflings:** DHCP; mDNS `HeishaMon32_h1b.local`, sonst im
   Router. Achtung: `heishamon_esp32_ota` zielt auf `heishamon32.local`,
   nicht auf h1b — zum Nachflashen USB nehmen oder `--upload-port` setzen.
4. **Einstellung:** In `/settings` des Prüflings `knx_schnittstelle =
   192.168.2.142` (LAN-Adresse des Macs, en0, am 2026-09-13 — vorher mit
   `ipconfig getifaddr en0` gegenprüfen). Speichern startet neu. Das Feld
   des Hydraulik-Switch nicht anfassen; der Testzugang benutzt es nicht.
5. **Simulator auf dem Mac:** `python3 -u test/knx_tunnel.py simulator
   192.168.2.142 [--start …] [--fehler …]` im Hintergrund, Ausgabe in eine
   Datei; er läuft bis Strg-C. Je Lauf neu starten, damit die Ausgangslage
   stimmt.
6. **Lauf anstoßen und ablesen:** `curl -u notbetrieb:<Passwort> -X POST
   http://<Prüfling>/vorderhaus/pruefen`, Stand mit demselben Aufruf ohne
   `-X POST`. Das Passwort ist `HEISHA_NOTBETRIEB_PASSWORD` aus
   `platformio_user_env.ini` — nicht in Ausgaben oder Logs schreiben.
   Zeilen unter `/log`, Einzelheiten („Vorderhaus Versuch …“, „fremde
   Antworten“) per `test/telnet_mitschnitt.py <Prüfling> <Sekunden>`.
7. **Reihenfolge:** erst die 13 Simulator-Läufe aus der Soll-Tabelle in
   Schritt 9. Jede Abweichung vom Soll ist ein Befund — erst verstehen, dann
   weiter. Danach `knx_schnittstelle = 192.168.2.127` und die Läufe am echten
   Mischer: vorher den Ist-Zustand lesen (`knx_tunnel.py lesen 192.168.2.127
   6/4/12 6/4/13 6/4/14 6/4/16 6/4/17 6/4/21 --alle`), **jeden Lauf einzeln
   ankündigen und vom Owner freigeben lassen**, `knx_tunnel.py mischer …
   --mithoeren` oder der ETS-Busmonitor als Gegenprobe. Freigegeben, solange
   die Anlage im Modus „Nur Warmwasser“ ohne Wärmeanforderung steht.
   Zurückstellen wie am 2026-09-12, jeder Eingriff einzeln.
8. **Rückgabe von h1b — entschieden: 3.21.0** (Owner 2026-09-13 abends).
   Zuerst in `/settings` `knx_schnittstelle = 192.168.2.127` setzen (die
   echte Schnittstelle; der Port bleibt 1884), dann `pio run -e
   heishamon_esp32_h1_usb -t upload --upload-port /dev/cu.usbmodem1101`
   vom Branch. Danach `/settings` gegenprüfen: Port 1884, Hostname
   `HeishaMon32_h1b`, KNX-Schnittstelle `192.168.2.127`.
9. **Danach:** Ergebnisse in `version.h` (Nachweis), `Ablauf-Notbetrieb.md`
   Abschnitt 1c („am Gerät abgenommen“) und hier im Kopf. Dann Schritt 10.
   Die Anleitung zum Mischer ist **keine** Voraussetzung mehr — der Owner
   schreibt sie nach dem Rollout.

## Worum es geht

Fällt die übergeordnete Steuerung aus, stellt der Notbetrieb die
Wärmepumpen um — das Vorderhaus bleibt dabei bisher außen vor, weil sein
Mischer und seine Pumpe am KNX-Bus hängen und auf dem letzten Wert stehen
bleiben. Der neue Schritt schickt per KNXnet/IP-Tunneling vier Telegramme an
die Schnittstelle 192.168.2.127: Zwangsstellungen zurück, Mischer auf 50 %,
Pumpe ein — und liest am Aktor zurück.

## Lies zuerst — in dieser Reihenfolge

1. `CLAUDE.md` des Repos, besonders die harten Regeln (Rettungsanker,
   `doku-intern/`, keine Binaries, kein Gerätezugriff ohne Auftrag).
2. [`Analyse-KNX-Vorderhaus.md`](Analyse-KNX-Vorderhaus.md), Abschnitt 8 ganz
   (Gruppenadressen, Stellmotor, „nur kurze Verbindungen“, Mischerlauf,
   „Bestätigung über die Bewegungsmeldung“, Rückleseregel A) und Abschnitt 7
   für die Rohbytes.
3. [`test/knx_tunnel.py`](test/knx_tunnel.py) 1.6.0 — **die Referenz.**
   `befehl_mischer()` mit `_bestaetigung()` ist Regel A in ausführbarer
   Form; die Rahmenfunktionen (`baue_*`, `zerlege_*`) sind die Vorlage für
   den Header; `selbsttest()` enthält die Sollwerte.
4. `src/notbetrieb.h` (Schrittfolge, Automat, Abbruchgründe) und
   `src/notbetrieb.cpp` ab `hydraulik_kommando()` — der Hydraulikschritt ist
   das Muster, dem der neue Schritt folgt.
5. [`Ablauf-Notbetrieb.md`](Ablauf-Notbetrieb.md), Abschnitt 1a, besonders
   „Warum der Request `loop()` blockieren darf“.

## Entschieden — nicht neu aufmachen

| Entscheid | Quelle |
| --- | --- |
| Eigener minimaler Tunnel-Client, keine Bibliothek, kein Routing | Analyse §2–4 |
| Weg A: **ein** Schritt am Ende der Folge, nach `Heatpump = 1`. *Bis 3.21.0 stand hier „nur Rolle Heizen“ — das war eine Annahme des Entwurfs und fälschlich als Owner-Entscheid geführt. Seit 3.22.0 in beiden Rollen, fällig nur bei Heizbetrieb (Owner 2026-09-14).* | Analyse §8, Owner 2026-09-12 und 2026-09-14 |
| ROT-Wortlaut: „Die Wärmepumpen laufen im Notbetrieb, nur das Vorderhaus ließ sich nicht umstellen“ | Owner 2026-09-12 |
| Fehlende KNX-Einstellung = ROT, kein stilles Entfallen | Owner 2026-09-12 |
| Quelladresse 1.1.250 im Telegramm | Owner 2026-09-12, Analyse §8 |
| Nur kurze Verbindungen — kein Tunnel bleibt über Sekunden offen | Analyse §8 |
| Die Busbestätigung (L_Data.con) entscheidet nichts; mit Quelle 1.1.250 kommt keine | Analyse §7/§8 |
| Nur Antworten der Aktoradressen zählen (1.1.39 Mischer, 1.1.60 Pumpe) | Analyse §8 |
| **Rückleseregel A** für den Mischer samt der vier Festlegungen beim Einarbeiten | Analyse §8, Owner 2026-09-12 abends |

## Die Parameter

| Was | Wert |
| --- | --- |
| Schnittstelle | 192.168.2.127:3671, reine Schnittstelle, 5 Tunnel, kein KNX IP Secure |
| Quelle im Telegramm | 1.1.250 |
| Zwangsstellung AUF / ZU | 6/4/17 / 6/4/16, DPT 1, schreiben 0 |
| Position Eingang | 6/4/13, DPT 5, schreiben 128, **zurücklesen** |
| Position Status | 6/4/12, DPT 5, lesen |
| Position Bewegung | 6/4/14, DPT 1, lesen / spontan |
| Pumpe schalten / Status | 6/4/20 / 6/4/21, DPT 1 |
| Aktoren | 1.1.39 Mischer, 1.1.60 Pumpe; 1.1.245 ist openknx und zählt nie |
| Toleranz Status | 128 ± 2 |
| Rückfall-Frist (Mischer fuhr schon) | 220 s |

Gemessene Zeiten als Maßstab für die Fristen: CONNECT_RESPONSE 1–4 ms,
TUNNELING_ACK 2 ms, Antwort eines Aktors auf Lesen 55–82 ms, Bewegung 1
93 ms nach dem Positionsbefehl, Pumpenstatus 55–120 ms nach dem Befehl.

## Regel A als Ablauf des Schritts

Eine kurze Verbindung beim Absetzen, wie der Hydraulikschritt:

1. CONNECT. Kanal und Datenendpunkt **aus der Antwort übernehmen** — die
   Schnittstelle vergibt beide selbst.
2. 6/4/14 lesen. `schnell` = der Mischeraktor antwortet mit 0. Keine Antwort
   zählt wie 1.
3. 6/4/17 = 0, 6/4/16 = 0, 6/4/13 = 128 schreiben. Nicht auf eine con warten.
4. 6/4/13 lesen. Keine Antwort oder ≠ 128: Schritt 3 einmal wiederholen,
   danach ROT.
5. `schnell`: bis 2 s auf Bewegung 1 vom Aktor warten, sonst 6/4/14 lesen; 1
   ist GRÜN. Sonst 6/4/12 lesen; 128 ± 2 ist GRÜN (Mischer stand schon
   dort). Sonst Schritt 3 einmal wiederholen, danach ROT.
   Nicht `schnell`: der Mischer bleibt **ausstehend** → Rückfall (E2).
6. 6/4/20 = 1, Rücklesung 6/4/21 vom Pumpenaktor (spontan, sonst lesen).
   Abweichung: einmal wiederholen, danach ROT. **Immer**, auch wenn der
   Mischer in Schritt 4 oder 5 ROT ergab (entschieden 2026-09-13).
7. DISCONNECT — immer, auch im Fehlerfall.

Während jeder Wartezeit jedes eingehende TUNNELING_REQUEST des eigenen
Kanals binnen 1 s quittieren; fremde Kanäle verwerfen, ohne zu quittieren.
Die Sequenzregel steht in `Tunnel._tunnel_empfangen()`.

## Entschieden am 2026-09-13

Einzeln mit dem Owner, jeweils aus der Vorlage im nächsten Abschnitt.

| Punkt | Entscheid |
| --- | --- |
| **E1** Einstellungen | **a.** Ein Feld `knx_schnittstelle`, **nur eine IP**, wahlweise mit `:Port` (Standard 3671). Quelle, die sieben Gruppenadressen und die zwei Aktoradressen stehen als Konstanten im Header und laufen durch den Hosttest. Kein Gerätename: Ob die Namensauflösung im Notbetriebsfall mit ausfällt, hängt davon ab, wo sie läuft; eine IP ist streng prüfbar. Leer oder ungültig: Warnung beim Start, ROT beim Druck. Preis: Eine Gruppenadresse, die sich in der ETS ändert, braucht einen neuen Build — das steht in `README.md` und `Ablauf-Notbetrieb.md`, damit der ETS-Fachmann es findet. Ein Tippfehler in einer GA auf der Einstellungsseite hätte Schreibtelegramme an eine fremde Gruppe geschickt. |
| **E2** Rückfall | **a.** Timeout je Schritttyp. Der Vorderhausschritt bekommt 240 s — die 220 s der Rückfallfrist plus der Austausch beim Absetzen — und das ist seine **einzige** Frist: keine zweite Uhr neben dem Schritt-Timeout, sonst hinge das Ergebnis davon ab, welche zuerst abläuft. Gesamtdeckel bleibt abgeleitet, als Summe der Schritt-Timeouts: Heizen 440 s, Warmwasser unverändert 120 s. Im Rückfall fragt der Tick alle 10 s in einer eigenen kurzen Verbindung 6/4/12 ab, bis 1.1.39 128 ± 2 meldet. |
| **E3** Text unter dem Satz | **c, Wortlaut des Owners:** „Der Mischer im Vorderhaus bleibt so eingestellt, wie die Steuerung es zuletzt vorgegeben hat. Ein zweiter Druck auf diesen Knopf kann eventuell die Einstellungen vornehmen. Wird es im Vorderhaus zu kalt oder zu warm, lies bitte die Anleitungen zum Mischer in den Unterlagen zum Notbetrieb.“ Darüber steht der Satz vom 2026-09-12. |
| **E4** Test am Gerät | **a, erweitert.** Der Plan übersah: **Am Prüfling ist der Knopf gesperrt** — ohne Wärmepumpe kein TOP101 (`test/README.md`, „Verbindungsanzeige am Prüfstand“), und ohne Rücklesung bräche ein Lauf an Schritt 2 ab; Schritt 11 wird dort nie erreicht. Deshalb: `knx_tunnel.py simulator` auf der LAN-Adresse des Macs, und **ein Testzugang nur im Prüflings-Build** (Flag in `[stage_test_esp32]`), der allein den Vorderhausschritt ausführt — derselbe Code wie Schritt 11, samt Rückfall, ohne Sperre, ohne Wärmepumpe, ohne Hydraulik-Switch. Die produktiven Builds enthalten ihn nicht; das wird am Build nachgewiesen. |
| E4, Angebot des Owners | Tests **am echten Mischer und an der echten Pumpe** sind erlaubt: Die Anlage steht im Heizbetrieb, Modus „Nur Warmwasser“, ohne Wärmeanforderung; die Pumpe dreht das Wasser nur im Kreis, der Mischer darf beliebig fahren. Jeder Lauf wird einzeln aufgerufen und vorher angekündigt. openknx darf zeitweise angehalten werden — vorgesehen ist es nicht: Mit laufendem openknx ist der Test strenger, weil die Firmware dessen schnellere Antworten verwerfen muss. |
| Wartezeit auf die Bewegung | **2 s**, wie Regel A (Analyse §8) und `knx_tunnel.py` 1.6.0. Die „1 s“ in Schritt 2 dieses Plans war ein Übertragungsfehler. Im Regelfall endet das Warten mit der Meldung nach rund 0,1 s. |
| Pumpe bei ROT des Mischers | **Immer einschalten**, wie das Referenzwerkzeug. Mit laufender Pumpe bekommt das Vorderhaus Wärme nach der letzten Mischerstellung der Steuerung (in der Feuerübung 132); ohne sie gar keine. Steht der Mischer auf AUF, kommt der Vorlauf der Notbetriebskurve an — für die Fußbodenheizung ausgelegt. Der Schritt meldet trotzdem den Fehler. |
| Anzeige des Vorderhausfalls | **Amberfarbenes Feld** (`w3-amber`, `#ffc107`, schwarze Schrift — neu im eingebetteten CSS), Überschrift „Teilweise umgestellt“ (Arbeitsstand, in Schritt 5 änderbar). ROT hieße „Plan B am Bedienfeld“, und das trifft nicht zu; `w3-orange` hat weiße Schrift bei rund 2 : 1 Kontrast. **Intern bleibt es ROT** mit dem Grund `NOTBETRIEB_GRUND_VORDERHAUS`: Der Knopf kommt nach dem Lauf von selbst zurück (passt zum Text), Statusroute und Logzeile ändern ihren Aufbau nicht. |
| Anleitung zum Mischer | Die **Anleitung zum Mischer** in den Notbetriebsunterlagen (`nodered-flows`), auf die der Seitentext verweist — heute führen die Unterlagen den Mischer nur als „bleibt stehen“ (FEUERUEBUNG.md, F6). **Schreibt der Owner nach dem Rollout** (Entscheid 2026-09-13 abends); bis dahin verweist der Satz ins Leere. |
| Re-Assert in `nodered-flows` | Claude schreibt dafür eine **Arbeitsanweisung für nodered-flows**, wenn dieses Vorhaben abgeschlossen ist (Owner 2026-09-13 abends). |
| Rückgabe von h1b | h1b bekommt nach den Tests **3.21.0** (Owner 2026-09-13 abends). |

## Die Vorlage dazu (Stand 2026-09-12)

**E1 — Wo stehen Adressen und Gruppenadressen?** Die Analyse nennt
Einstellungen „wie `hydraulik_switch`“; das wären zwölf neue Felder in
WiFiManager, `config.json` und Einstellungsseite (`webfunctions.cpp`).

- **a (empfohlen):** ein Feld `knx_schnittstelle` (IP, optional `:Port`);
  Quelle, Gruppenadressen und Aktoradressen als Konstanten im Header.
  Wenige Felder, die Konstanten sind im Hosttest prüfbar, und die
  Gruppenadressen ändern sich nur mit ETS-Arbeit, die ohnehin einen Fachmann
  braucht. Leer = ROT.
- b: alle zwölf Werte als Felder. Änderbar ohne Build, aber viel Oberfläche
  für wenige Werte, die sich nie ändern.

**E2 — Der Rückfall braucht bis zu 220 s, der Automat kennt 20 s je
Schritt.** `NOTBETRIEB_SCHRITT_TIMEOUT_MS` gilt heute für jeden Schritt, und
der Gesamtdeckel ist `Schrittzahl × 20 s` (`notbetrieb.h`, Zeitregeln).

- **a (empfohlen):** Timeout je Schritttyp (Vorderhaus 240 s), Gesamtdeckel
  als Summe der Schritt-Timeouts — bleibt abgeleitet. Im Rückfall fragt der
  Tick alle 10 s in einer eigenen kurzen Verbindung 6/4/12 ab, bis 128 ± 2.
  Der Regelfall ist davon nicht betroffen.
- b: Rückfall = ROT mit eigenem Text. Einfach, widerspricht aber Regel A.

**E3 — Was steht unter dem ROT-Wortlaut?** Der Owner-Satz sagt, was los
ist, aber nicht, was zu tun ist. Beim Hydraulikschritt steht „Danach diesen
Knopf noch einmal drücken“ darunter.

- **a (empfohlen):** derselbe Zusatz. Ein zweiter Lauf setzt dieselben
  Werte noch einmal; mit laufender Wärmepumpe ist das allerdings nicht
  gemessen.
- b: nur der Owner-Satz.

**E4 — Wie wird am Prüfling getestet, ohne den echten Mischer zu bewegen?**

- **a (empfohlen):** `knx_tunnel.py` bekommt einen Befehl `simulator`, der
  den vorhandenen Simulator auf der LAN-Adresse des Macs anbietet. Der
  Prüfling bekommt dessen Adresse als Schnittstelle — alle Zweige von Regel A
  lassen sich dann ohne Bus durchspielen. Danach ein einzelner Lauf gegen die
  echte Schnittstelle mit Owner-Freigabe.
- b: nur gegen die echte Schnittstelle, bei ausgeschalteter Kaskade.

## Arbeitsschritte

Jeder Schritt endet mit seinem Prüfkriterium. Commits je Schritt auf dem
Branch.

0. **Rettungsanker und Branch.** Tag `rettungsanker-JJJJ-MM-TT` auf den
   aktuellen `main`, Branch `knx-vorderhaus`. E1–E4 vorher entscheiden.
1. **`src/knxtunnel.h`, arduino-frei.** Rahmen bauen und zerlegen (KNXnet/IP-
   Kopf, HPAI, CONNECT, TUNNELING_REQUEST/ACK, DISCONNECT, cEMI), GA- und
   PA-Kodierung, `bestaetigt_ok` nur über Bit 0 von ctrl1, und das Urteil von
   Regel A als reine Funktion über die gelesenen Werte. Hosttest
   `test/knx_test.cpp` gegen die Sollwerte: die xknx-Vektoren und die
   Anlagen-Rohbytes aus `selbsttest()` Teil 1–3 sowie die Tabellen in
   Analyse §7/§8; dazu die Zweige von Regel A wie in den Mischerfällen des
   Selbsttests. In die Hosttest-Liste in `.github/workflows/main.yml`,
   mit Begründungskommentar wie die anderen.
   *Prüfkriterium:* Hosttest grün, jeder Rahmen byteweise gleich den
   Sollwerten.
2. **Netzteil**, eigene Datei (Vorschlag `src/vorderhaus.cpp`), aufgerufen aus
   `notbetrieb_schritt_absetzen()`. `WiFiUDP`, Ablauf wie oben, jede Frist
   begrenzt (CONNECT 1 s, ACK 1 s mit einer Wiederholung, Lesen 1 s — endet
   an der Antwort des Aktors, nicht am Fenster —, Bewegung spontan 2 s wie
   Regel A).
   Beim ersten fehlenden ACK abbrechen, DISCONNECT, ROT. Logzeilen wie beim
   Hydraulikschritt: was geantwortet hat und was nicht.
   *Prüfkriterium:* Die größte Blockade von `loop()` ist ausgerechnet und
   im Kommentar begründet, wie in Ablauf-Notbetrieb §1a.
3. **Automat.** `NB_SCHRITT_VORDERHAUS` in `NotbetriebSchrittTyp`, als letzter
   Eintrag in `NOTBETRIEB_SCHRITTE_HEIZEN`; `NOTBETRIEB_GRUND_VORDERHAUS` und
   `notbetrieb_grund_fuer_schritt()`; E2 umsetzen. In `notbetrieb.cpp` eine
   Bestätigung wie `hydraulikBestaetigt`, beim Start jedes Laufs gelöscht;
   ROT sofort, wenn das Absetzen scheitert. Die Mindestwarte
   (`NOTBETRIEB_SCHRITT_MINDESTWARTE_MS`, 8 s) gilt auch hier — der Schritt
   kostet im Regelfall also rund 8 s, nicht die Sekunde des Austauschs.
   *Prüfkriterium:* `notbetrieb_test` deckt den neuen Schritt, den Grund und
   den Deckel ab, samt `millis()`-Überlauf im Rückfall.
4. **Einstellung** nach E1, mit der Start-Warnung wie beim
   Hydraulik-Switch (`notbetrieb.cpp`, „keine Adresse fuer den
   Hydraulik-Switch“). `jsonDoc.overflowed()` bleibt die Absicherung.
5. **Seite.** `NB_TXT_VORDERHAUS` nach `NB_TXT_HYDRAULIK` in
   `webfunctions.cpp`, der ROT-Zweig wählt nach dem Grund; beim Grund
   Vorderhaus das Amberfeld samt `w3-amber` im eingebetteten CSS. Im Rückfall
   steht bis zu vier Minuten „läuft“ — dort ergänzen, dass die Wärmepumpen
   schon laufen und nur der Mischer noch fährt. Dauertext und Laufzeiten
   prüfen. `css_klassen_test.py` muss grün bleiben.
6. **Hosttests anpassen,** die an der Schrittfolge hängen
   (`test/notbetrieb_test.cpp`): „Heizen hat zehn Schritte“, „Schritt 10
   schaltet die Anlage ein“, die Schleife „letzter Schritt jeder Rolle ist
   `Heatpump`“ (gilt dann nur noch für Wasser; für Heizen: `Heatpump` direkt
   vor dem Vorderhausschritt) und die Deckelwerte.
7. **Doku.** `src/version.h`: 3.21.0 mit Problem, Nachweis, RAM/Flash-Delta.
   `Ablauf-Notbetrieb.md`: Phase 2 und „Die zehn Schritte“, neuer Abschnitt
   „1c. Der Vorderhausschritt“, Abschnitt 3 (die Rückkehr der Steuerung holt
   den Mischer erst mit dem KNX-Re-Assert zurück). `README.md` → „Aufbau“:
   neue Dateien. Die Liste der arduino-freien Header in `CLAUDE.md`. Im Kopf
   der Analyse das Ergebnis.
8. **Bauen und testen:** alle Envs, alle Hosttests (Befehle in `CLAUDE.md`,
   Liste in `main.yml`), `knx_tunnel.py selbsttest`.
9. **Prüfling** (E4, entschieden 2026-09-13). `knx_tunnel.py` bekommt den
   Befehl `simulator`; der Prüflings-Build den Testzugang, der nur den
   Vorderhausschritt ausführt. Zuerst alle Zweige von Regel A gegen den
   Simulator. Danach gegen die echte Schnittstelle — **nur Mischer und
   Pumpe**, keine Wärmepumpe, kein Hydraulik-Switch —, jeder Lauf einzeln
   angekündigt: schneller Weg, „steht schon am Ziel“ (zweiter Druck),
   Rückfall kurz nach einem Wechsel der Zwangsstellung, Pumpe. Busmonitor
   und `knx_tunnel.py mischer … --mithoeren` oder `lesen --alle` als
   Gegenprobe. Zurückstellen wie am 2026-09-12, jeder Eingriff einzeln: AUF
   bzw. ZU wie vorgefunden, Eingang auf den Vorwert, Pumpe wie vorgefunden.
   Ob beim Rollout zusätzlich ein ganzer Lauf an H1 stattfindet, entscheidet
   der Owner.

   **So läuft er ab (vorbereitet 2026-09-13):**
   - Backup-Board leihen nach `test/README.md`, „Prüfstand aufsetzen“,
     Schritte 1–2 (`pio run -e heishamon_esp32_usb -t upload`). Den
     MQTT-Port **nicht** auf 1883 stellen: Der Test braucht keinen Broker,
     das Board bleibt doppelt gesperrt, und die Rückgabe ist nur das
     Zurückflashen der Stufen-Firmware.
   - In `/settings` `knx_schnittstelle` auf die LAN-Adresse des Macs setzen
     (Simulator), später auf `192.168.2.127`. Speichern startet neu.
   - Simulator: `./test/knx_tunnel.py simulator <Mac-IP> [--start …]
     [--fehler …]`. Lauf anstoßen: `curl -u notbetrieb:<Passwort> -X POST
     http://<Prüfling>/vorderhaus/pruefen`, Stand mit `GET`, Einzelheiten
     unter `/log` und per `test/telnet_mitschnitt.py`.
   - Die Simulator-Läufe und ihr Soll:

     | Aufruf | Soll |
     | --- | --- |
     | `--start zu` | GRÜN, Mischer fährt, Pumpe spontan gemeldet |
     | `--start steht128` | GRÜN über den Status (zweiter Druck), rund 2,5 s |
     | `--start faehrt` | Rückfall, GRÜN nach der Ankunft (rund 60 s) |
     | `--fehler zwang_klemmt` | ROT nach einer Wiederholung, Pumpe trotzdem ein |
     | `--fehler pos_verloren` | ROT über den Eingang, eine Wiederholung |
     | `--fehler mischer_stumm` | Rückfall (Vorab-Lesung schweigt), dann ROT: Eingang stumm |
     | `--fehler openknx` | GRÜN; im Telnet-Log „fremde Antworten“ > 0 |
     | `--fehler pumpe_ohne_meldung` | GRÜN, Pumpe „gelesen“ statt „spontan“ |
     | `--fehler pumpe_stumm` | ROT: Pumpe meldet nicht ein |
     | `--fehler ack_verlieren` | GRÜN, eine Quittung wiederholt |
     | `--fehler belegt` | ROT sofort, „alle Tunnel belegt“ im Log |
     | `--fehler stumm` | ROT nach 1 s, „keine Antwort auf CONNECT“ |
     | leeres Feld `knx_schnittstelle` | ROT sofort, Startwarnung im Serial |

10. **Merge und Rollout** erst nach Schritt 8 und 9 vollständig. Abnahme mit
    `test/tablesnap.py` gegen den Stand davor. Die Anleitung zum Mischer
    folgt danach durch den Owner; danach schreibt Claude die
    Arbeitsanweisung für den KNX-Re-Assert in `nodered-flows`.

## Fallen, die schon einmal zugeschnappt sind

- **openknx antwortet schneller als der Aktor** — auf 6/4/12 und 6/4/14 aus
  dem Zwischenspeicher, auf 6/4/14 dreifach. Wer die erste Antwort nimmt,
  meldet im Test GRÜN, und im Ernstfall ist openknx gar nicht da.
- **Mit Quelle 1.1.250 kommt keine L_Data.con.** Wer darauf wartet, verliert
  je Telegramm die ganze Frist; wer sie verlangt, meldet immer ROT.
- **ctrl1 der con kommt als `9c`, gesendet wurde `bc`.** Positiv/negativ
  steht allein in Bit 0.
- **Der Aktor speichert die Position auch unter Zwangsstellung** und fährt
  dann nicht. Der zurückgelesene Eingang allein belegt deshalb nichts.
- **In die Endlage läuft die Bewegung immer 144 s**, der Status meldet schon
  nach der proportionalen Fahrzeit. Zur Mitte gibt es keinen Nachlauf.
- **Ein ETS-Download setzt den Aktor zurück** (Eingang 0, Position ungültig);
  die nächste Fahrt läuft die volle Zeit samt Nachlauf.
- **MQTT-Reconnect blockiert `loop()` bis 2 s**, im Notbetriebsfall
  wiederholt. Deshalb nie einen Tunnel über mehrere `loop()`-Durchläufe
  offen halten.
- **Die InfluxDB schreibt openknx-Werte jede Minute neu.** Ein Eintrag ist
  kein Telegramm; ob etwas auf dem Bus war, zeigen `ts`/`lc` am Datenpunkt.

## Nicht in diesem Plan

- **Re-Assert der KNX-Befehle in `nodered-flows`** (Pumpe, Zwangsstellung).
  Ohne ihn holt die zurückkehrende Steuerung Mischer und Pumpe nicht von
  selbst zurück — Voraussetzung dafür, dass der Normalzustand wiederkommt.
- **Notfallanleitung** (`nodered-flows`, `FEUERUEBUNG.md`): Fällt nur
  Node-RED aus und ioBroker läuft, regelt `HKMregelung.js` die Position beim
  nächsten Regelschritt wieder weg (Analyse §6).

## Stand der Commits bei der Übergabe

Alle auf `main`; am 2026-09-13 stand `main` gleich `origin/main`.

| Commit | Inhalt |
| --- | --- |
| `b8cc042` | `knx_tunnel.py` 1.5.0: Bestätigung über die Bewegungsmeldung |
| `278e42e` | Analyse: Bewegungsmeldung an der Anlage erprobt |
| `0ce86c0` | `knx_tunnel.py` 1.6.0: Rückleseregel A als Referenz |
| (dieser) | Analyse: Entscheid A; dieser Arbeitsplan |
