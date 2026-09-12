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

**Vorabtest abgeschlossen (2026-09-12, Abschnitt 7):** Der kurzlebige Tunnel
trägt, der Pumpenstatus ist aktiv lesbar, und Schalten samt Rücklesung
funktioniert. Wichtigster Befund: Eine **negative Busbestätigung heißt nicht,
dass das Telegramm verloren ging** — die Rücklesung entscheidet. Die
Quelladresse **1.1.250** lässt sich vorgeben und steht so auf dem Bus; sie
kostet die Busbestätigung (Abschnitt 8).

**Entwurf steht, an der Anlage erprobt (Abschnitt 8):** ein Schritt am Ende
der Heizen-Folge (Weg A). Der Mischerlauf mit genau dieser Folge war am
2026-09-12 grün; der Mischer meldete 128 nach 59,2 s.

**Bewegungsmeldung erprobt (2026-09-12 abends, Abschnitt 8):** Der Aktor
meldet über 6/4/14 rund 0,1 s nach dem Positionsbefehl, dass er fährt. Damit
lässt sich der Mischer in Sekunden bestätigen statt nach bis zu 60 s — sofern
zusätzlich der Positionseingang zurückgelesen wird und die Bewegung vor den
Befehlen auf 0 stand.

**Entschieden (Owner, 2026-09-12 abends): Rückleseregel A** für den
Mischer (Abschnitt 8). Referenz ist `knx_tunnel.py` 1.6.0,
`mischer --bewegung`.

**Offen:** die Umsetzung in der Firmware nach
[`Arbeitsplan-KNX-Vorderhaus.md`](Arbeitsplan-KNX-Vorderhaus.md) und der
Re-Assert für die KNX-Befehle in `nodered-flows` (Abschnitt 9).

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

**Stufe 2 — Ziel erreicht, das Werkzeug meldete trotzdem Fehler
(2026-09-12, 14:43 UTC).** Die Pumpe war genau einmal 5,06 s aus und läuft
wieder. Der Ablauf, aus dem Werkzeug und aus der InfluxDB-Historie von
openknx:

| Zeit (UTC) | Ereignis | Beleg |
| --- | --- | --- |
| 14:43:28,21 | Lesen: Status 1 | Werkzeug, Historie |
| 14:43:28,21 | Schalten = 0, L_Data.con **positiv** (`9c`) | Werkzeug; openknx empfängt 0 |
| 14:43:28,28 | Status 0, vom Aktor spontan gemeldet, 75 ms nach dem Befehl | Werkzeug, Historie |
| bis 14:43:33,2 | 5 s halten; vier Telegramme anderer Gruppen quittiert, **kein** fremder Schreibzugriff auf 6/4/20 | Werkzeug |
| 14:43:33,29 | Schalten = 1, L_Data.con **negativ** (`9d`, Bit 0 gesetzt) — das Werkzeug bricht ab und nennt die Rückstellzeile | Werkzeug; openknx empfängt 1 |
| 14:43:33,35 | Status 1, 55 ms nach dem Befehl | Historie |
| 14:44:06 | Kontrolle per `lesen`: Status 1 | Werkzeug, ioBroker |

Die beiden Schreibtelegramme und ihre Bestätigungen:

| Richtung | Rohbytes | Bedeutung |
| --- | --- | --- |
| → | `06 10 04 20 00 15 04 82 01 00 11 00 bc e0 00 00 34 14 01 00 80` | GroupValueWrite 6/4/20 = 0 |
| ← | `06 10 04 20 00 15 04 82 03 00 2e 00 9c e0 11 94 34 14 01 00 80` | L_Data.con positiv |
| → | `06 10 04 20 00 15 04 82 02 00 11 00 bc e0 00 00 34 14 01 00 81` | GroupValueWrite 6/4/20 = 1 |
| ← | `06 10 04 20 00 15 04 82 08 00 2e 00 9d e0 11 94 34 14 01 00 81` | L_Data.con **negativ** |

**Befund: Eine negative L_Data.con heißt nicht „nicht angekommen".** Das
Rückstelltelegramm lag auf dem Bus — openknx hat es über seinen eigenen Tunnel
empfangen, und der Aktor hat geschaltet. Warum die Schnittstelle trotzdem einen
Fehler meldet, ist offen. Eine naheliegende, **ungeprüfte** Vermutung: Das
Werkzeug sendet wie xknx mit gesetztem „nicht wiederholen"-Bit (ctrl1 `bc`,
Bit 5). Fehlt dann ein einziges Layer-2-ACK auf dem TP-Bus, meldet die
Schnittstelle sofort einen Fehler, statt das Telegramm zu wiederholen.

**Folge für das Werkzeug:** `schalten` hat bei der negativen Bestätigung
abgebrochen, statt zurückzulesen, und meldete „möglicherweise noch im
Testzustand", obwohl die Pumpe schon wieder lief. Das ist sicher, sagt aber
nichts über den tatsächlichen Zustand. Die Klärung kam erst aus einem zweiten
`lesen` und aus ioBroker.

**Folge für die Firmware:** siehe Abschnitt 8, Rückleseregel.

**Messfalle in der Historie:** Die InfluxDB schreibt die openknx-Datenpunkte
auch unverändert alle 60 s neu. Diese Einträge sind keine Telegramme. Ob
wirklich geschaltet wurde, zeigt `ts` bzw. `lc` am Datenpunkt — `ts` von
`MischerPumpe_Schalten` stand vor dem Test auf 12:53, obwohl die Historie
jede Minute einen Eintrag hat.

## 8. Entwurf des Firmwareschritts „Vorderhaus"

Vorgesehen für die Folge der Stufe 1 (Rolle Heizen), weil nur sie den
Heizkreis versorgt.

### Die Gruppenadressen

Vom Owner am 2026-09-12 genannt; den Datentyp führt openknx:

| Objekt | GA | DPT in openknx | im Notbetrieb |
| --- | --- | --- | --- |
| `MischerMotor_Zwangsstellung_AUF` (100 %) | 6/4/17 | 1.001 | 0 |
| `MischerMotor_Zwangsstellung_ZU` (0 %) | 6/4/16 | 1.001 | 0 |
| `MischerMotor_Position_Eingang` | 6/4/13 | 5.004, Rohwert 0–255 | **128** ≈ 50 % |
| `MischerMotor_Position_Status` | 6/4/12 | 5.004 | Rücklesung |
| `MischerMotor_Position_Bewegung` | 6/4/14 | 1.001, Flags K/L/Ü | Rücklesung — seit 2026-09-12 abends, siehe „Bestätigung über die Bewegungsmeldung“ |
| `MischerPumpe_Schalten` | 6/4/20 | 1.001 | 1 |
| `MischerPumpe_Status` | 6/4/21 | 1.001 | Rücklesung |

Reihenfolge der Telegramme: erst beide Zwangsstellungen zurücknehmen — sie
gehen vor (Owner-Antwort 2) —, dann die Position, dann die Pumpe.

Wie nötig das Zurücknehmen ist, zeigt der Stand am Tag der Aufnahme: Seit
12:53 UTC stand `Zwangsstellung_ZU` auf 1 und der Mischer auf 0. Die
Steuerung wechselt im Sommer mehrmals täglich zwischen Zwangsstellung AUF und
ZU. Ein Positionsbefehl allein bliebe dagegen wirkungslos.

### Wie der Stellmotor meldet

Aus der openknx-Historie vom 08. bis 12.09.:

| Vorgang | Weg | Statusmeldung nach | proportional erwartet |
| --- | --- | --- | --- |
| Zwangsstellung ZU bzw. AUF (11.09. 06:50; 12.09. 09:12, 11:19, 12:53) | voller Hub | 120,3–120,4 s | 120 s |
| Zwangsstellung AUF, 11.09. 07:00, von 46 | 209/255 | 98,7 s | 98,4 s |
| Zwangsstellung erneut gesendet, Mischer steht schon dort | 0 | 0,4 s | sofort |
| Regelung im Minutentakt, je 2/255 | 2/255 | rund 1 s | 0,9 s |

**Der Motor fährt streng proportional, 120 s für den vollen Hub, und meldet
erst in der Zielstellung** — Zwischenwerte gibt es nicht (Owner, bestätigt
durch die Historie). Steht er schon am Ziel, meldet er sofort. Der gemeldete
Wert trifft den Befehl exakt (250 → 250). Weil 50 % die Mitte ist, dauert der
Weg dorthin **höchstens rund 60 s**, von welcher Endlage auch immer.

### Warum der Schritt keinen Tunnel offen halten darf

Im Notbetriebsfall fehlt der Broker, und `loop()` versucht laufend, ihn
wieder zu erreichen: der erste Versuch nach 5 s, dann mit wachsendem Abstand
bis zum Minutentakt (`MQTT_RECONNECT_MIN`/`_MAX`). Jeder Versuch blockiert
bis zu 2 s (`MQTT_SOCKET_TIMEOUT_S`), je nach Ausfallart durch den
TCP-Verbindungsaufbau womöglich länger. Ein offener Tunnel muss aber jedes
Bustelegramm binnen 1 s quittieren; die Schnittstelle wiederholt einmal und
trennt dann. **Ein Tunnel, der 60 s auf die Meldung des Mischers wartet,
verpasste diese Frist fast sicher.**

Daraus folgt die Bauregel: **nur kurze Verbindungen**, wie im Werkzeug — jede
ein geschlossener Austausch von Zehntelsekunden, der nicht von einem anderen
blockierenden Aufruf unterbrochen werden kann. Den Mischerstatus holt der
Schritt deshalb durch aktives Lesen in eigenen kurzen Verbindungen, statt auf
die spontane Meldung zu warten.

**Belegt am 2026-09-12: Der Positionsstatus ist lesbar — aber nicht nur
vom Aktor.** `lesen 192.168.2.127 6/4/12 6/4/21 --alle`, zwei
Lesetelegramme, jeweils das ganze Fenster abgewartet:

| GA | Antwort von | nach dem Senden |
| --- | --- | --- |
| 6/4/12 | 1.1.245 — **openknx**, aus seinem Zwischenspeicher | 28 ms |
| 6/4/12 | **1.1.39** — der Mischeraktor | 75 ms |
| 6/4/21 | 1.1.60 — der Pumpenaktor, sonst niemand | 55 ms |

Ein erster Lauf ohne `--alle` hatte nur die schnellste Antwort genommen, die
von openknx, und GRÜN gemeldet — eine Messfalle: Im Notbetriebsfall ist
openknx gar nicht da, im Test antwortet es zuerst. **Der Firmwareschritt
zählt deshalb nur Antworten der Aktoradressen** (1.1.39 Mischer, 1.1.60
Pumpe).

### Mischerlauf nach Weg A an der Anlage

Am 2026-09-12 mit Owner-Freigabe gelaufen. Der Regler war gesperrt, die
Zwangsstellung ZU aktiv. Aufruf: `knx_tunnel.py` 1.4.0, `mischer
192.168.2.127 --quelle 1.1.250 --mithoeren`, also genau die Folge des
geplanten Schritts, dazu ein mithörender Tunnel.

| t | Ereignis |
| ---: | :--- |
| 0 s | Ausgangsstellung 0, gelesen vom Aktor 1.1.39 |
| 4,0–7,1 s | 1.1.250 → 6/4/17 = 0, 6/4/16 = 0, 6/4/13 = 128, 6/4/20 = 1 — alle vom Mithörer auf dem Bus gesehen, keine L_Data.con |
| 11,1 s | Pumpenstatus aktiv gelesen: 1 von 1.1.60. Auf das Schreiben desselben Werts kam keine spontane Meldung |
| 22–64 s | vier Abfragen: Aktor **und** openknx melden weiter 0 — während der Fahrt steht die alte Stellung im Status |
| **65,2 s** | **1.1.39 → 6/4/12 write 128**, die spontane Meldung am Ziel — **59,2 s** nach dem Positionsbefehl, erwartet 60,2 s |
| 78 s | Abfrage: 128 von 1.1.39 → **GRÜN** |

Damit ist belegt: **1.1.39 ist der Mischeraktor**, die Zwangsstellung lässt
sich über den Bus zurücknehmen, und Weg A trägt an der Anlage so, wie er
entworfen ist.

**Zurückgestellt** in zwei einzelnen Aufrufen: erst Zwangsstellung ZU = 1
(15:54:51 UTC), dann der Positionseingang auf 46, den letzten Wert des
Reglers (15:55:07 UTC) — bei aktiver Zwangsstellung wird er nur hinterlegt.
Der Mischer meldete 0 um 15:55:51, **60,1 s** nach dem ZU-Befehl (128 → 0,
erwartet 60,2 s). Kontrolle in ioBroker und per `lesen --alle`: AUF 0, ZU 1,
Eingang 46, Status 0, Pumpe 1, Regler weiter auf ForcedState 2 — wie vor dem
Test.

**Was daraus für die Firmware folgt:**

- **Nur Antworten der Aktoren zählen**, deren Adressen in die Einstellungen
  kommen — sonst meldet jeder Test mit lebendem ioBroker GRÜN aus dem
  Zwischenspeicher.
- **Bei Quelle 1.1.250 nicht auf die L_Data.con warten** — sie kommt nie. Im
  Lauf kostete das vergebliche Warten 1 s je Telegramm.
- **Die spontane Meldung am Ziel sieht keine kurze Abfrageverbindung**; die
  aktive Abfrage findet die neue Stellung erst beim nächsten Takt. Bei 10 s
  Takt kommt GRÜN also bis zu rund 14 s nach der Ankunft — verkraftbar.

Rücklesung: `MischerPumpe_Status` = 1 passt in das Schritt-Timeout — das
Objekt ist aktiv lesbar, und der Aktor meldet rund 120 ms nach dem Befehl
zurück (Stufe 1).

**Rückleseregel aus Stufe 2: Die Rücklesung am Aktor ist das Urteil, nicht
die L_Data.con.** Eine negative Bestätigung wird geloggt, danach wird trotzdem
zurückgelesen. Passt der Status, ist der Schritt erledigt. Passt er nicht,
wird das Telegramm einmal wiederholt — ein Schalttelegramm auf denselben Wert
ist unschädlich. Erst wenn auch dann die Rücklesung nicht passt, endet der
Schritt ROT. Ob das „nicht wiederholen"-Bit im Request gelöscht werden soll,
damit die Schnittstelle selbst wiederholt, ist beim Umsetzen zu entscheiden.
Für den Mischer gilt dieselbe Regel. Eine Bewegungsrichtung lässt sich nicht
prüfen, weil der Aktor keine Zwischenwerte meldet — es bleibt die Endlage,
und die ist nach höchstens rund 60 s erreicht.

### Bestätigung über die Bewegungsmeldung (6/4/14)

**Owner-Vorschlag vom 2026-09-12 abends:** Der Aktor hat ein weiteres
Objekt, `MischerMotor_Position_Bewegung` (6/4/14, DPT 1.001, Flags K/L/Ü),
das von Losfahren bis Ziel auf 1 steht. Der Owner hat es in der ETS angelegt
und per Download (18:43 UTC) aktiviert. Statt bis zu 60 s auf die
Endstellung zu warten, soll der Schritt nur kurz auf diese Meldung warten.

Erprobt mit `knx_tunnel.py` 1.5.0 (`mischer --bewegung`). Die Kaskade war
aus, der Regler gesperrt (`HKM_ForcedState_Input` = 1, Zwangsstellung AUF,
Mischer auf 255), die Pumpe blieb unberührt (`--ohne-pumpe`). Der Owner hat
parallel im ETS-Busmonitor mitgelesen.

**Vorab aus der openknx-Historie, ohne ein Schreibtelegramm:**

- Bewegung 1 kam bei den Zwangsstellungswechseln des Abends 230–330 ms nach
  dem Setzen in ioBroker. Darin steckt die Sendeverzögerung von openknx; am
  Bus gemessen sind es rund 0,1 s (Läufe 1 und 3).
- **In die Endlage fährt der Aktor immer die volle Zeit plus 20 %:** Status
  nach der proportionalen Fahrzeit, Bewegung 0 erst nach 144 s — beim
  vollen Hub (Status nach 120,4 s) wie beim halben (Lauf 3, Status nach
  59,7 s). Zur Mitte gibt es keinen Nachlauf (Lauf 1).
- **Der Download setzt den Aktor zurück:** Positionseingang 0 (ioBroker
  führte noch 46) und `MischerMotor_Position_Ungültig` (6/4/18) = 1 von
  18:43:44 bis 19:13:40 UTC. Die nächste Fahrt lief die volle Zeit samt
  Nachlauf — auch nach einem Richtungswechsel —, erst dann meldete der Aktor
  und nahm „ungültig“ zurück.
- openknx beantwortet das Lesen von 6/4/14 dreifach aus seinem
  Zwischenspeicher. Es zählt wie bei 6/4/12 nur der Aktor 1.1.39.

**Die Läufe, jeder einzeln freigegeben.** Die Befehle gingen mit Quelle
1.1.250 hinaus, wie später aus der Firmware; die beiden Rückstellungen ohne
vorgegebene Quelle, damit die Busbestätigung kommt (Zwangsstellung und
Eingang haben kein eigenes Statusobjekt).

| Lauf | Was | Ergebnis |
| --- | --- | --- |
| 1 | Weg A ab 255: AUF 0, ZU 0, Position 128 | **GRÜN.** Auf das Zurücknehmen von AUF und ZU **keine** Bewegung, obwohl im Eingang 0 stand. Bewegung 1 **93 ms** nach dem Positionsbefehl (Bus). Eingang liest 128 zurück. Status 128 nach 59,65 s, Bewegung 0 100 ms danach |
| 2 | derselbe Aufruf, Mischer schon auf 128 | **GRÜN über den Status.** Keine Bewegungsmeldung, auch keine spontane Statusmeldung |
| 3 | Rückstellung AUF = 1, Busbestätigung positiv | Bewegung 1 nach 76 ms, Status 255 nach 59,7 s, Bewegung 0 erst nach **144,1 s** |
| 4 | Negativprobe: nur Position 100, AUF bleibt aktiv | **ROT, wie erwartet:** keine Bewegung, Status 255. **Der Eingang liest 100 zurück** — der Aktor speichert die Position auch unter Zwangsstellung |
| 5 | Rückstellung Eingang = 0, Busbestätigung positiv | Eingang 0, vom Aktor zurückgelesen |

Endstand wie vor dem Test: AUF 1, ZU 0, Eingang 0, Status 255, Bewegung 0,
Position gültig, Pumpe aus, Regler auf ForcedState 1.

**Was daraus folgt:**

- **Die Bewegung kommt schnell genug** für dieselbe kurze Verbindung, in der
  die Befehle hinausgehen.
- **Das Zurücknehmen der Zwangsstellung allein lässt den Mischer nicht
  fahren** (Lauf 1). Eine Bewegung nach den Befehlen stammt also vom
  Positionsbefehl — sofern der Mischer vorher stand.
- **Die Bewegung allein reicht nicht; es braucht zwei Belege:**
  - Eingang = 128: Der Positionsbefehl steht im Aktor. Weil der Aktor ihn
    auch unter Zwangsstellung speichert (Lauf 4), belegt das allein nicht,
    dass er wirkt.
  - Bewegung = 1 — oder Status = 128, wenn der Mischer schon dort steht und
    deshalb nicht fährt (Lauf 2): Der Befehl wirkt, die Zwangsstellung ist
    zurückgenommen.
- **Eine Lücke bleibt, wenn der Mischer beim Start schon fährt.** Nach einem
  Zwangsstellungswechsel steht Bewegung bis zu 144 s auf 1. Ginge dann das
  Zurücknehmen der Zwangsstellung verloren, läse der Schritt Bewegung 1 und
  Eingang 128 und meldete GRÜN, während der Mischer in die Endlage fährt.
  Schließen lässt sie sich, indem der Schritt die Bewegung **vor** den
  Befehlen liest.

**Rückleseregel A — entschieden (Owner, 2026-09-12 abends):**

1. In der Befehlsverbindung zuerst die Bewegung lesen (nur 1.1.39).
2. Zwangsstellungen zurücknehmen, Position senden.
3. Eingang lesen. Weicht er ab: einmal wiederholen, dann ROT.
4. Stand die Bewegung vorher auf 0: bis 2 s auf Bewegung 1 warten, sonst
   den Status lesen. Bewegung 1 oder Status 128 ± 2 ist GRÜN; sonst einmal
   wiederholen, dann ROT.
5. Stand sie vorher auf 1: wie bisher in kurzen Verbindungen den Status
   abfragen, bis er 128 meldet. Die Frist muss dann den laufenden
   Endlagenlauf abdecken.

Beim Einarbeiten festgelegt, jeweils zur sicheren Seite hin:

- **Antwortet der Aktor auf die Vorab-Lesung nicht**, zählt das wie
  Bewegung 1 — dann entscheidet die Endstellung.
- **Antwortet er auf das Lesen des Eingangs nicht**, zählt das als
  Abweichung.
- **Die Wiederholung umfasst alle drei Mischertelegramme** (AUF 0, ZU 0,
  Position): Derselbe Wert noch einmal ist unschädlich, und welches der drei
  fehlte, ist nicht zu erkennen.
- **Die Frist des Rückfalls beträgt 220 s**: bis zu 144 s laufender
  Endlagenlauf, danach der halbe Hub (60 s), dazu Reserve.

Im Regelfall (Mischer steht) dauert der Mischerteil damit wenige Sekunden
statt bis zu 60 s plus Abfragetakt. Die Firmware braucht dafür zusätzlich
die Gruppenadresse 6/4/14. Referenz ist `knx_tunnel.py` 1.6.0,
`mischer --bewegung`; der Selbsttest spielt alle Zweige gegen den
Simulator durch.

### Zwei Wege, den Schritt einzubauen — entschieden: A (Owner, 2026-09-12)

| | **A: ein Schritt am Ende** (empfohlen) | B: früh senden, am Ende prüfen |
| --- | --- | --- |
| Position | nach `Heatpump = 1` | Senden nach der Hydraulik, Prüfen nach `Heatpump = 1` |
| Ablauf | eine kurze Verbindung: beide Zwangsstellungen, Position, Pumpe, Pumpenstatus zurücklesen; dann alle 10 s den Mischerstatus lesen, bis 128 ± 2, höchstens 90 s | wie A, nur läuft der Mischer während der übrigen Schritte (64 s dazwischen) |
| Dauer des Laufs | rund 80 s plus bis zu 60 s | rund 88 s |
| KNX-Fehler | ROT am Ende — die Wärmepumpen stehen dann schon im Notbetrieb | bricht den Lauf ab, bevor die Wärmepumpen umgestellt sind — außer der Automat lernt „Fehler ohne Abbruch" |
| Automat | ein neuer Schritttyp mit eigenem Timeout | zwei neue Schritttypen, der zweite bezieht sich auf den ersten |

**Empfehlung A.** Eine Störung am KNX — Schnittstelle, Bus, Einstellung —
darf den Notbetrieb der Wärmepumpen nicht verhindern; der Hauptteil des
Hauses bekäme sonst auch keine Wärme. Die zusätzliche Minute am Ende kostet
nichts, weil der Kompressor ohnehin erst rund drei Minuten nach dem
Einschalten hochfährt. Die ROT-Meldung braucht dann einen eigenen Wortlaut,
wie beim Hydraulikschritt: Die Wärmepumpen laufen, nur das Vorderhaus ließ
sich nicht umstellen.

**Entschieden am 2026-09-12:** Weg A, und die ROT-Meldung sagt: Die
Wärmepumpen laufen im Notbetrieb, nur das Vorderhaus ließ sich nicht
umstellen.

Außerdem:

- **Nicht eingerichtet** (keine KNX-Adresse in den Einstellungen): **ROT**,
  wie beim Hydraulikschritt ohne Switch-Adresse. Owner: gebaut wird für genau
  diese Anlage, auf andere HeishaMon-Anwender wird keine Rücksicht genommen.
  Mein Vorschlag, den Schritt dann still entfallen zu lassen, ist damit vom
  Tisch.
- **Die Zwangsstellungen haben kein Statusobjekt.** Ihr Zurücknehmen bleibt
  blind, wird aber mitgeprüft: Bleibt eine aktiv, erreicht der Mischer die
  128 nicht, und die Rücklesung scheitert.

**Test mit lebender Steuerung:** Steht `HKM_ForcedState_Input` auf Auto,
regelt `HKMregelung.js` die Position beim nächsten Regelschritt wieder weg;
steht eine Zwangsstellung an (wie am 12.09.), ist der Regler gesperrt, und
die Steuerung setzt die Zwangsstellung erst beim nächsten Wechsel neu. Wie
beim Hydraulikschritt lässt sich der Schritt mit lebender Steuerung also nur
auf Ausführung prüfen, nicht auf Dauerwirkung.

In der Firmware: IP, Port, Quelladresse und die sechs Gruppenadressen in den
Einstellungen (wie `hydraulik_switch`).

**Quelladresse (Owner-Wunsch 2026-09-12: 1.1.250 statt der vergebenen
Tunneladresse 1.1.148).** KNXnet/IP kennt zwei Wege, und openknx zeigt, welcher
hier trägt:

- **Quelladresse im Telegramm.** So macht es openknx (`eibadr = 1.1.245` in
  der Adapterkonfiguration), und so setzt sie `knx_tunnel.py --quelle`.
  **Belegt: Die Schnittstelle übernimmt sie auf den Bus** (Gegenprobe
  unten). Dass in Stufe 2 ein Telegramm von 1.1.245 mitlief, war dafür noch
  kein Beleg — die Tunneladressen der Schnittstelle liegen nicht am Stück
  (1.1.148, 1.1.247), 1.1.245 kann ebenso gut der Tunnel von openknx sein.
- **Einen bestimmten Tunnel anfordern** (erweiterte CRI, Tunnelling v2). Das
  setzt voraus, dass 1.1.250 als Tunneladresse der Schnittstelle projektiert
  ist, und läuft üblicherweise über TCP — xknx bietet es nur dort an. In
  openknx wäre das `tunnelInterfaceAddress`, und das ist leer. Wird hier nicht
  gebraucht, solange der erste Weg trägt.

Voraussetzung für beide: 1.1.250 ist an keinem anderen Gerät vergeben —
vom Owner am 2026-09-12 bestätigt.

**Nachweis vom 2026-09-12: Das Telegramm geht raus, die Bestätigung bleibt
aus.** `lesen 192.168.2.127 6/4/21 --quelle 1.1.250`, nur ein Lesetelegramm:

| Richtung | Rohbytes | Bedeutung |
| --- | --- | --- |
| → | `06 10 04 20 00 15 04 92 00 00 11 00 bc e0 11 fa 34 15 01 00 00` | GroupValueRead 6/4/21, Quelle 1.1.250 |
| ← | `06 10 04 21 00 0a 04 92 00 00` | TUNNELING_ACK nach 2 ms |
| ← | `06 10 04 20 00 15 04 92 00 00 29 00 bc e0 11 3c 34 15 01 00 41` | Antwort des Pumpenaktors nach 55 ms: 6/4/21 = 1 |
| — | — | **keine L_Data.con**, auch nicht nach 3 s |

Das Lesetelegramm lag also auf dem Bus — der Aktor hat es beantwortet, mit
derselben Laufzeit wie in Stufe 1. Die L_Data.con, die mit Quelle 0.0.0 in
jedem Lauf kam, blieb aus; das Werkzeug meldete deshalb FEHLER. Die
Schnittstelle stellt die Bestätigung offenbar nur zu, wenn die Quelle ihre
Tunneladresse ist. openknx fällt das nicht auf: Es wartet laut Konfiguration
gar nicht auf Bestätigungen (`waitForAck = False`).

**Gegenprobe vom 2026-09-12 — 1.1.250 steht auf dem Bus.** Ohne con ist die
Quelle vom eigenen Tunnel aus nicht zu sehen; deshalb hörte ein zweiter
Tunnel mit (`knx_tunnel.py` 1.2.0, `lesen 192.168.2.127 6/4/21 --quelle
1.1.250 --gegenprobe`, wieder nur ein Lesetelegramm):

| Tunnel | Kanal | Tunneladresse | sah |
| --- | --- | --- | --- |
| sendend | 178 | 1.1.148 | Antwort 1.1.60 → 6/4/21 = 1 nach 56 ms; keine L_Data.con |
| Gegenprobe | 179 | 1.1.247 | **1.1.250 → 6/4/21 read**, dann 1.1.60 → 6/4/21 response 1 |

Die Schnittstelle übernimmt die vorgegebene Quelle also unverändert und
reicht das Telegramm an die anderen Tunnel weiter; nur die L_Data.con an den
sendenden Tunnel entfällt. Das eigene Telegramm kommt beim sendenden Tunnel
auch nicht als L_Data.ind zurück.

**Folge für die Firmware:** Eine vorgegebene Quelle kostet die
Busbestätigung. Nach der Rückleseregel ist das verkraftbar — dann darf aber
auch eine **fehlende** con kein Abbruchgrund sein, nicht nur eine negative. Der Aufbau und das Zerlegen der Rahmen kommen in
einen arduino-freien Header mit Hosttest gegen die Sollwerte aus
`knx_tunnel.py`.

## 9. Folgeaufgaben

- ~~**Vorabtest Stufen 0–2**~~ — erledigt am 2026-09-12, Abschnitt 7.
- ~~**`knx_tunnel.py schalten`**: nach einer negativen Bestätigung
  zurücklesen~~ — erledigt in 1.1.0: Die Rücklesung entscheidet, bei
  Abweichung genau eine Wiederholung; `schreiben --status`; neu `--quelle`.
- ~~**Nachweis Quelladresse 1.1.250**~~ — erledigt am 2026-09-12: steht so
  auf dem Bus, ohne L_Data.con (Abschnitt 8, Gegenprobe mit 1.2.0).
- **Re-Assert für die KNX-Befehle in `nodered-flows`** (Pumpe, Zwangsstellung).
  Er ist Voraussetzung dafür, dass die Steuerung nach dem Notbetrieb den
  Normalzustand selbst wiederherstellt.
- ~~**Mischeradressen aus openknx übernehmen**~~ — erledigt am 2026-09-12
  (Abschnitt 8).
- ~~**Positionsstatus lesbar?**~~ — ja, vom Aktor 1.1.39 und von openknx;
  nur der Aktor zählt (Abschnitt 8).
- ~~**Owner-Entscheid**~~ — Weg A, ROT-Meldung wie vorgeschlagen, fehlende
  Einstellung ist ROT (Abschnitt 8).
- ~~**Mischerlauf nach Weg A an der Anlage**~~ — grün am 2026-09-12
  (Abschnitt 8).
- ~~**Bestätigung über die Bewegungsmeldung 6/4/14 erproben**~~ — am
  2026-09-12 abends in fünf Läufen erledigt (Abschnitt 8): trägt, braucht
  aber das Zurücklesen des Eingangs und die Bewegung vor den Befehlen.
- ~~**Owner-Entscheid: Rückleseregel des Mischers**~~ — entschieden am
  2026-09-12 abends: Regel A (Abschnitt 8), Werkzeug 1.6.0.
- **Umsetzung in der Firmware** nach
  [`Arbeitsplan-KNX-Vorderhaus.md`](Arbeitsplan-KNX-Vorderhaus.md): neuer
  Schritttyp am Ende der Heizen-Folge,
  Bau der Telegramme in einem arduino-freien Header mit Hosttest gegen die
  Sollwerte aus `knx_tunnel.py`; Einstellungen für IP, Port, Quelle, die
  sechs Gruppenadressen (sieben mit 6/4/14) und die zwei Aktoradressen.
  Vorher Rettungsanker und Branch.

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
