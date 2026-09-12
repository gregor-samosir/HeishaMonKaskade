# Analyse: KNX-Telegramme für das Vorderhaus aus dem Notbetrieb

**Stand: 2026-09-12. Entscheidungsgrundlage und Testplan; an der Firmware ist
nichts geändert.** Die Frage lautet: Kann der Notbetrieb die Versorgung des
Vorderhauses (VH) mit übernehmen — Mischer auf 50 %, Mischerpumpe ein —, indem
HeishaMon die nötigen KNX-Telegramme per KNXnet/IP-Tunneling an die
Schnittstelle 192.168.2.127 schickt?

**Ergebnis der Recherche:** Für Arduino/ESP32 gibt es keine fertige, gepflegte
Bibliothek, die als **Tunnel-Client** an einer KNX-IP-Schnittstelle arbeitet.
Der Weg ist ein eigener, minimaler Client — zuerst als Python-Werkzeug
[`test/knx_tunnel.py`](test/knx_tunnel.py) an der Anlage erprobt, danach als
arduino-freier Header in der Firmware.

**Entschieden (Owner, 2026-09-12):** Der KNX-Teil kommt als zusätzlicher
Schritt in die Schrittfolge des Notbetriebs (Abschnitt 5).

**Offen:** die drei Stufen des Vorabtests an der Anlage (Abschnitt 7) und
danach der Entwurf des Firmwareschritts (Abschnitt 8).

---

## 1. Ausgangslage

Das Vorderhaus hängt über einen eigenen Mischer (HKM) mit Mischerpumpe am
Heizkreis. Beide sitzen am KNX-Bus. Im Normalbetrieb stellt `HKMregelung.js`
(ioBroker-JavaScript) den Mischer, und die Kaskaden Logik (Node-RED) schaltet
die Pumpe.

Fällt die übergeordnete Steuerung aus, bleiben die KNX-Aktoren auf ihrem
letzten Wert. Eigene Rückfallwerte haben sie nicht
(`~/nodered-flows/doku/referenz/FEUERUEBUNG.md`, Abschnitt B). Manuelle
Bedienelemente für Mischer und Pumpe gibt es nicht. Der Bus selbst läuft
weiter, er hat eine eigene Spannungsversorgung.

**Eine neue Netzvoraussetzung entsteht dadurch nicht.** Der Notbetrieb braucht
das Hausnetz schon heute: Der Hydraulikschritt schaltet den Tasmota-Switch per
HTTP ([`Ablauf-Notbetrieb.md`](Ablauf-Notbetrieb.md), Abschnitt 1a). Neu ist
nur ein zweites Fremdgerät, die KNX-IP-Schnittstelle.

## 2. Routing oder Tunneling

Die beiden Wege von KNXnet/IP trennen die Kandidaten sofort:

- **Routing:** UDP-Multicast an 224.0.23.12:3671, ohne Verbindung. Das braucht
  einen KNX-IP-**Router**, keine reine Schnittstelle. Multicast über WLAN ist
  zudem unzuverlässig (IGMP-Snooping), und niemand bestätigt ein Telegramm.
- **Tunneling:** Unicast-UDP an die Schnittstelle, mit Verbindungsaufbau,
  Sequenznummern, ACK und einer Bestätigung vom Bus (L_Data.con). Die Zahl der
  Tunnel ist je Gerät begrenzt.

Hier liegt eine reine Schnittstelle — es kommt also nur Tunneling in Frage.

## 3. Was es gibt

Stand der Recherche vom 2026-09-12, Web und GitHub:

| Projekt | Weg | Tunnel-Client? | Stand | Eignung |
| --- | --- | --- | --- | --- |
| Tasmota KNX (Basis esp-knx-ip) | Routing | nein — laut Maintainer „not supported and there is no plans" | aktiv | nein |
| envy/esp-knx-ip | Routing | nein | 150★, zuletzt 2022, MIT, ESP8266 | nein |
| jamel-86/ESP32_KNX_IP_Library | Routing | nein | ESP32-Port davon, 2023, MIT | nein |
| thelsing/knx, OpenKNX/knx | Routing plus Tunnel-*Server* | nein — `ConnectRequest` wird beantwortet, nie gesendet | 351★ bzw. aktiv, GPL-3.0 | nein: vollständiges KNX-Gerät, braucht ETS für physikalische Adresse und Gruppenadressen |
| loddeknx/esp32knx | Tunneling | ja — Connect, ACK, Heartbeat, Disconnect | 10★, seit 2018 unberührt, MIT, ESP-IDF-Programm mit eigenen Tasks | nur als Vorlage |
| cc90202/knx-pico | Tunneling | ja | Rust/Embassy, RP2350; ESP32-C3/C6 nur geplant | nein: andere Toolchain |
| McOrvas/sonoff-knxd | TCP an knxd | — | ESP8266, GPL-3.0 | nein: braucht einen knxd-Server, der mit der Synology ausfällt |
| ESPHome | — | — | nur ein Feature-Request und ein fast leeres Community-Repo | nein |

## 4. Drei Wege

1. **Eigener minimaler Tunnel-Client — gewählt.** CONNECT, pro Telegramm ein
   TUNNELING_REQUEST mit cEMI-L_Data.req, auf ACK und L_Data.con warten,
   DISCONNECT. Die Verbindung lebt nur Sekunden: Ein Heartbeat ist unnötig, und
   der Tunnel ist gleich wieder frei. L_Data.con belegt, dass das Telegramm auf
   dem Bus war; die Statusobjekte der Aktoren liefern die Rücklesung — dieselbe
   Systematik wie bei den Schritten an der Wärmepumpe. Keine neue Abhängigkeit,
   `WiFiUDP` genügt. Flash (61,7 %) und RAM (18,8 %) sind kein Hindernis.
2. **Routing mit esp-knx-ip.** Scheidet an der reinen Schnittstelle aus und
   wäre blind — widerspricht „Rücklesung statt blind schreiben".
3. **OpenKNX/thelsing als KNX-Gerät.** ETS-Projektierung und physikalische
   Adresse für ein paar Notfall-Telegramme — weit überdimensioniert und von der
   Familie nicht zu warten.

## 5. Was die Anlage vorgibt — Owner-Antworten vom 2026-09-12

| Frage | Antwort | Folge |
| --- | --- | --- |
| Welche Schnittstelle? | Reine Schnittstelle, 5 Tunnel, einen belegt openknx, **kein KNX IP Secure** | Der Minimal-Client reicht. HeishaMon belegt kurzzeitig einen zweiten Tunnel; eine offene ETS wäre der dritte. |
| Geht die Zwangsstellung vor? | Ja — sie muss vor dem Positionskommando zurückgenommen werden | Der Schritt schickt vier Telegramme statt zwei (Abschnitt 8). |
| Wer holt die Werte nach dem Notbetrieb zurück? | Dazu braucht es einen noch zu bauenden Re-Assert für die KNX-Befehle in `nodered-flows` | Folgeaufgabe dort (Abschnitt 9). Heute schreibt die Kaskaden Logik `MischerPumpe_Schalten` nur bei Ereignissen. |
| Gruppenadressen? | Alle in openknx hinterlegt und bekannt. Pumpe schalten **6/4/20**, Pumpe Status **6/4/21**; Schnittstelle 192.168.2.127:3671 | Für den Vorabtest reichen die beiden Pumpenadressen. Die Mischeradressen werden beim Umsetzen aus openknx übernommen. |
| Wie wird es eingebunden? | Als zusätzlicher Schritt | Kein zweiter Knopf; Entscheidung 7 des Notbetriebsvorhabens bleibt unberührt. |

## 6. Was `nodered-flows` dazu sagt

**Der Positionseingang läuft von 0 bis 255.** So schreibt ihn `HKMregelung.js`
(`POS_MAX = 255`), und so wird er gemessen (`MischerMotor_Position_Status`).
50 % sind also 128.

**Der Regler schreibt nur bei Änderung und rechnet von der realen Position
aus.** Die Position geht nur auf den Bus, wenn der Regelschritt eine neue
Stellgröße ergibt — und nur, wenn der Mischer nicht über
`HKM_ForcedState_Input` gesperrt ist (`HKMregelung.js`, §5c). Die
Zwangsstellungsobjekte schreibt er nur, wenn sich dieser Wert ändert.

Daraus folgt für die zwei Ausfallarten:

- **Ganzer Ausfall (ioBroker weg):** Niemand schreibt dazwischen. Die 50 %
  bleiben stehen, bis die Steuerung zurückkommt.
- **Nur Node-RED weg, ioBroker läuft:** `HKMregelung.js` regelt weiter und
  übernimmt beim nächsten Regelschritt mit Änderung wieder die Position. Das
  ist dann eher erwünscht als ein Fehler, gehört aber in die Notfallanleitung.

**Die Pumpe wird ereignisgesteuert geschaltet.** `MischerPumpe_Schalten` kommt
aus der Kaskaden Logik, ohne zyklischen Re-Assert. Deshalb holt eine
zurückkehrende Steuerung die Pumpe nicht von selbst zurück — siehe Antwort 3.

**`MischerPumpe_Status` speist die Wärmemengenzählung des VH**
(`energiezaehler_th/VH.js`, pauschal 1,2 m³/h, solange der Status an ist).
Die Sekunden des Vorabtests ergeben einen vernachlässigbaren Zählerbeitrag.

## 7. Der Vorabtest

Er läuft vom Mac aus, ohne ESP32 und ohne Flash. Der Mac hängt ohne Gateway
im selben Subnetz wie die Schnittstelle. Das Werkzeug spricht genau das
Protokoll, das später in die Firmware kommt; seine Bytefolgen werden die
Sollwerte für den Hosttest des Firmware-Headers.

**Warum ein eigenes Werkzeug und nicht ETS-Gruppenmonitor oder xknx:** Die
Umgebung ist schon belegt, denn openknx schreibt dieselben Gruppenadressen
über dieselbe Schnittstelle. Unbelegt ist nur der eigene Minimal-Client.
xknx dient nur als Gegenprobe, falls das Werkzeug scheitert — dann ist klar,
ob der Fehler beim Werkzeug oder an der Schnittstelle liegt.

**Ohne Anlage geprüft (2026-09-12):** `./knx_tunnel.py selbsttest` ist grün.
Die Rahmen stimmen byteweise mit den Rohbytes aus den Tests von xknx überein,
die Abläufe laufen gegen einen Simulator — auch verlorenes ACK, fremder
Schreibzugriff, negative Bestätigung und ein Statusobjekt ohne Leseantwort.
Das belegt die Logik des Werkzeugs, nicht das Verhalten der echten
Schnittstelle.

### Die drei Stufen — jede einzeln aufrufen

| Stufe | Aufruf | Busverkehr | Beantwortet |
| --- | --- | --- | --- |
| 0 | `./knx_tunnel.py verbinden 192.168.2.127` | keiner | Nimmt die Schnittstelle einen kurzlebigen Tunnel ohne Heartbeat an, welche Tunneladresse vergibt sie, gibt sie ihn sauber frei? |
| 1 | `./knx_tunnel.py lesen 192.168.2.127 6/4/21` | ein Lesetelegramm, kein Zustandswechsel | Der ganze Pfad hin und zurück. **Und: Antwortet das Statusobjekt auf Lesen?** Wenn nicht, muss der Firmwareschritt auf die spontane Statusmeldung warten, statt aktiv zu lesen — das ändert den Entwurf. |
| 2 | `./knx_tunnel.py schalten 192.168.2.127 6/4/20 6/4/21` | zwei Schreibtelegramme, Pumpe 5 s im anderen Zustand | Das Muster des späteren Schritts: senden, Bestätigung vom Bus, Rücklesung am Aktor, dann zurück. |

In Stufe 2 schaltet die **Pumpe**, nicht der Mischer. Beim Mischer kann ein
Regelschritt von `HKMregelung.js` mitten in die Rücklesung fallen, und das
Ergebnis wäre nicht eindeutig. Dass die Zwangsstellung vorgeht, ist bestätigt
und muss nicht gemessen werden. Und der 1-Byte-Wert unterscheidet sich vom
Pumpenbit nur in der Kodierung, die der Selbsttest prüft.

**Gegenprobe ohne Zusatzaufwand:** openknx hört mit. In Stufe 2 muss
`openknx.0.Kaskade.MischerPumpe_Status` im ioBroker kurz umspringen und
zurückkehren.

### Was das Werkzeug in Stufe 2 selbst absichert

- **Fremder Schreibzugriff.** Die Kaskaden Logik schreibt ereignisgesteuert
  und holt nichts zurück. Schaltet sie während des Tests selbst, würde ein
  blindes Zurückstellen ihren neuen Befehl überschreiben, und der bliebe bis
  zum nächsten Ereignis falsch. Das Werkzeug hört deshalb auf 6/4/20 mit.
  Kommt ein Schreibtelegramm von fremder Quelladresse, stellt es **nicht**
  zurück und endet mit Exit-Code 3. Ein Restfenster von Millisekunden zwischen
  dieser Prüfung und dem Zurückstellen bleibt und ist im Code benannt.
- **Abbruch zwischen Umschalten und Zurückstellen.** Das Werkzeug gibt vor dem
  Schalten die Zeile zum manuellen Zurückstellen aus und wiederholt sie bei
  jedem Abbruch. Ein nicht abgemeldeter Tunnel ist dagegen harmlos: Drei von
  fünf bleiben frei, und die Schnittstelle räumt ihn nach 120 s ab.
- **Kein Testfenster nötig.** Der 5-min-Re-Assert betrifft die set-Topics der
  Wärmepumpen, nicht KNX.

### Was der Vorabtest nicht abdeckt

- WLAN statt LAN. Unicast-UDP hat mit WLAN kein Multicast-Problem; belegt wird
  es erst am Prüfling.
- Die Mischertelegramme (Zwangsstellung, Position) an der Anlage. Sie kommen
  mit der Umsetzung, am Prüfling und im Testfenster der Mischerregelung.

### Ergebnisse

**Stufe 0 — grün (2026-09-12, vom Mac 192.168.2.142).** Die Schnittstelle
nimmt den kurzlebigen Tunnel ohne Heartbeat an und gibt ihn sauber frei. Jede
Antwort kam binnen 1–4 ms, der ganze Lauf dauerte 15 ms.

| Richtung | Rohbytes | Bedeutung |
| --- | --- | --- |
| → | `06 10 02 05 00 1a 08 01 c0 a8 02 8e d6 ca 08 01 c0 a8 02 8e d6 ca 04 04 02 00` | CONNECT_REQUEST, Steuer- und Datenendpunkt 192.168.2.142:54986 |
| ← | `06 10 02 06 00 14 e2 00 08 01 c0 a8 02 7f 0e 57 04 04 11 94` | CONNECT_RESPONSE: Kanal 226, Status ok, Datenendpunkt 192.168.2.127:3671, Tunneladresse 1.1.148 |
| → | `06 10 02 07 00 10 e2 00 08 01 c0 a8 02 8e d6 ca` | CONNECTIONSTATE_REQUEST |
| ← | `06 10 02 08 00 08 e2 00` | CONNECTIONSTATE_RESPONSE: ok |
| → | `06 10 02 09 00 10 e2 00 08 01 c0 a8 02 8e d6 ca` | DISCONNECT_REQUEST |
| ← | `06 10 02 0a 00 08 e2 00` | DISCONNECT_RESPONSE: ok |

Zwei Dinge daraus für die Firmware: Die Schnittstelle vergibt den Kanal selbst
(hier 226, nicht 1) und liefert den Datenendpunkt ausdrücklich mit — beides
muss aus der CONNECT_RESPONSE übernommen und darf nicht angenommen werden.
Bustelegramme kamen in den 15 ms keine herein; über den Empfangspfad sagt
Stufe 0 deshalb nichts, das belegt erst Stufe 1.

**Stufe 1 — grün (2026-09-12, 14:40 UTC).** Ein Lesetelegramm an 6/4/21, die
Pumpe lief (Status 1). Antwort vom Pumpenaktor 1.1.60 nach 55 ms, der ganze
Lauf dauerte 71 ms.

| Richtung | Rohbytes | Bedeutung |
| --- | --- | --- |
| → | `06 10 04 20 00 15 04 42 00 00 11 00 bc e0 00 00 34 15 01 00 00` | TUNNELING_REQUEST, Kanal 66, Sequenz 0: L_Data.req GroupValueRead 6/4/21 |
| ← | `06 10 04 21 00 0a 04 42 00 00` | TUNNELING_ACK nach 2 ms |
| ← | `06 10 04 20 00 15 04 42 00 00 2e 00 9c e0 11 94 34 15 01 00 00` | L_Data.con nach 22 ms: positiv, Quelle 1.1.148 eingesetzt |
| → | `06 10 04 21 00 0a 04 42 00 00` | unser ACK |
| ← | `06 10 04 20 00 15 04 42 01 00 29 00 bc e0 11 3c 34 15 01 00 41` | L_Data.ind nach 55 ms: GroupValueResponse 6/4/21 = 1 von 1.1.60 |
| → | `06 10 04 21 00 0a 04 42 01 00` | unser ACK |

Was daraus für die Firmware folgt:

- **Das Statusobjekt der Pumpe hat das L-Flag.** Der Schritt kann aktiv
  zurücklesen und muss nicht auf eine spontane Meldung warten — die offene
  Entwurfsfrage aus Stufe 1 ist damit beantwortet.
- **Die L_Data.con trägt ctrl1 = `9c`, nicht das gesendete `bc`.** Die
  Schnittstelle ändert das Wiederholungsbit. Ob die Bestätigung positiv ist,
  steht allein in Bit 0; ein Vergleich des ganzen Bytes würde eine positive
  Bestätigung als Fehler lesen.
- **Der Kanal wechselt bei jeder Verbindung** (Stufe 0: 226, jetzt 66), die
  Tunneladresse 1.1.148 bleibt. Die Sequenzzähler beginnen in beiden
  Richtungen je Verbindung bei 0.

**Gegenprobe in ioBroker (simple-api, nur lesend):**
`openknx.0.Kaskade.MischerPumpe_Status` stand auf `true`, Adresse 6/4/21,
DPT 1.001; der Zeitstempel `ts` sprang auf 14:40:07,85 UTC, in das Fenster des
Laufs — openknx hat die Antwort auf dem Bus mitgelesen.
`MischerPumpe_Schalten` (6/4/20, DPT 1.001) stand auf `true`. Nebenbei ein
Messwert, den openknx schon hatte: Die letzte Änderung des Schaltobjekts war
09:12:40,768, die des Status 09:12:40,891 — **der Aktor meldet rund 120 ms
nach dem Schaltbefehl zurück.** Und `ts` des Schaltobjekts wurde um 12:53
ohne Wertänderung erneuert: Auf 6/4/20 kommen also auch zwischendurch
Schreibtelegramme an, der Schutz gegen fremde Schreibzugriffe in Stufe 2 hat
einen realen Anlass.

**Für Stufe 2 heißt das:** Die Pumpe läuft. Der Test schaltet sie für 5 s
**aus** und danach wieder ein.

Stufe 2: noch nicht gelaufen.

## 8. Skizze des späteren Schritts — vorläufig

Vorgesehen für die Folge der Stufe 1 (Rolle Heizen), weil nur sie den
Heizkreis versorgt. Reihenfolge der Telegramme:

1. `MischerMotor_Zwangsstellung_AUF` = 0
2. `MischerMotor_Zwangsstellung_ZU` = 0
3. `MischerMotor_Position_Eingang` = 128
4. `MischerPumpe_Schalten` = 1

Rücklesung: `MischerPumpe_Status` = 1 passt in das Schritt-Timeout — das
Objekt ist aktiv lesbar, und der Aktor meldet rund 120 ms nach dem Befehl
zurück (Stufe 1).
`MischerMotor_Position_Status` bewegt sich mit der Laufzeit des Stellmotors;
ob der Schritt auf die Endlage wartet oder nur die Bewegungsrichtung prüft,
ist nach dem Vorabtest festzulegen.

In der Firmware: IP, Port und die sechs Gruppenadressen in den Einstellungen
(wie `hydraulik_switch`). Der Aufbau und das Zerlegen der Rahmen kommen in
einen arduino-freien Header mit Hosttest gegen die Sollwerte aus
`knx_tunnel.py`.

## 9. Folgeaufgaben

- **Vorabtest Stufen 0–2** an der Anlage, einzeln aufgerufen; Ergebnisse in
  Abschnitt 7 nachtragen.
- **Re-Assert für die KNX-Befehle in `nodered-flows`** (Pumpe, Zwangsstellung).
  Er ist Voraussetzung dafür, dass die Steuerung nach dem Notbetrieb den
  Normalzustand selbst wiederherstellt.
- **Mischeradressen aus openknx übernehmen** (Zwangsstellung AUF/ZU,
  Positionseingang, Positionsstatus).
- **Rückleseregel für die Mischerposition** festlegen (Abschnitt 8).

## 10. Quellen

- [Tasmota KNX-Doku](https://tasmota.github.io/docs/KNX/),
  [Discussion #13806](https://github.com/arendst/Tasmota/discussions/13806),
  [Issue #6631](https://github.com/arendst/Tasmota/issues/6631)
- [envy/esp-knx-ip](https://github.com/envy/esp-knx-ip),
  [jamel-86/ESP32_KNX_IP_Library](https://github.com/jamel-86/ESP32_KNX_IP_Library)
- [thelsing/knx](https://github.com/thelsing/knx),
  [OpenKNX/knx](https://github.com/OpenKNX/knx)
- [loddeknx/esp32knx](https://github.com/loddeknx/esp32knx),
  [cc90202/knx-pico](https://github.com/cc90202/knx-pico),
  [McOrvas/sonoff-knxd](https://github.com/McOrvas/sonoff-knxd)
- [yannick/esphome-knx](https://github.com/yannick/esphome-knx),
  [ESPHome Feature-Request #307](https://github.com/esphome/feature-requests/issues/307)
- Sollwerte des Selbsttests: [XKNX/xknx](https://github.com/XKNX/xknx),
  `test/knxip_tests/` und `test/cemi_tests/cemi_frame_test.py`
