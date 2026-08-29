# Ablauf des Notbetriebs — die Schritte mit Zeiten

Was in der Firmware passiert, wenn jemand den Notbetriebsknopf drückt, und was
passiert, wenn die Kaskadensteuerung zurückkommt. Beide Abläufe Schritt für
Schritt, jeweils mit der Zeit ab dem auslösenden Ereignis.

**Stand:** 2026-08-27, Firmware 3.15.0 (Hydraulikschritt; 3.14.1 lief seit dem
2026-08-23 auf beiden Stufen).
Quelle sind der Code — [`src/notbetrieb.h`](src/notbetrieb.h),
[`src/notbetrieb.cpp`](src/notbetrieb.cpp), [`src/HeishaMon.cpp`](src/HeishaMon.cpp) —
und die Messläufe vom 2026-08-21, protokolliert in
[`Vorhaben-Notbetrieb-Weboberflaeche.md`](Vorhaben-Notbetrieb-Weboberflaeche.md)
Abschnitt 10.

**Zwei Arten von Zeitangaben stehen hier nebeneinander**, und die Unterscheidung
trägt das ganze Dokument:

* **Regelzeit** — so steht sie in der Firmware und gilt bei jedem Lauf. Die
  Schrittabstände von 8 s sind Regelzeit: die Mindestwartezeit, nicht die
  Antwortzeit der Wärmepumpe. Die hat jeden Schritt schon vorher zurückgemeldet.
* **Messzeit** — an der Anlage beobachtet, im Text immer als „gemessen"
  gekennzeichnet. Sie belegt die Regelzeit, ersetzt sie aber nicht.

## Übersicht

Ereignis | Dauer | Wer treibt den Ablauf
:--- | ---: | :---
Notbetrieb einschalten, Stufe 1 (Heizen) | **72 s** | die Firmware, Schritt für Schritt
Notbetrieb einschalten, Stufe 2 (Warmwasser) | **40 s** | die Firmware, Schritt für Schritt
Rückkehr der Steuerung | **bis 5 min** bis zur Übernahme, rund 10 min bis alles steht | Node-RED — **die Firmware tut nichts**

---

# 1. Notbetrieb einschalten — Stufe 1, Rolle Heizen

Neun Schritte, t = 0 ist der Klick auf den Knopf. **Schritt 1 stellt seit 3.15.0
die Hydraulik** ([Abschnitt 1a](#1a-der-hydraulikschritt)), die übrigen acht
gehen an die Wärmepumpe.

## Phase 1 — Auslösen (t = 0)

Alles in dieser Phase passiert im selben Moment. Der HTTP-Handler stößt nur an
und antwortet sofort; er wartet auf nichts.

t | Was passiert | Wo
:--- | :--- | :---
0 s | `POST /notbetrieb/start` trifft ein | [`webfunctions.cpp:893`](src/webfunctions.cpp#L893)
0 s | **Sperre serverseitig geprüft.** Fehlen gehaltene Werte, oder steht TOP101 nicht sauber auf 0, endet der Ablauf hier: Logzeile, 303 zurück auf die Seite, kein Kommando an die Wärmepumpe | [`notbetrieb.cpp:423`](src/notbetrieb.cpp#L423)
0 s | Zustand BEREIT → LÄUFT, Schrittzähler auf 1, Lauf- und Schrittuhr gestartet, das Ergebnis des vorigen Laufs verworfen | `notbetrieb_start()`
0 s | MQTT-Log: „NOTBETRIEB ausgeloest ueber die Weboberflaeche" |
0 s | 303 zurück auf `/notbetrieb`. **Der Handler setzt keinen Schritt mehr ab** | `notbetrieb_starten()`
+ ein `loop()`-Durchlauf | Schritt 1 geht raus. Alles Weitere tickt ebenfalls aus `loop()` | `notbetrieb_loop()`

**Seit 3.15.0 setzt der Webhandler den ersten Schritt nicht mehr selbst ab.**
Bis 3.14.2 tat er es — für ein Set-Kommando eine Sache von Mikrosekunden.
Schritt 1 ist jetzt der Hydraulikschritt und damit ein HTTP-Request von bis zu
1,5 s; läge er im Handler, hinge der Browser so lange an einer Seite, die noch
nichts anzeigen kann. Der Versatz ist ein `loop()`-Durchlauf, also
Millisekunden.

Die Sperre wird hier **noch einmal** geprüft, obwohl die Seite den Knopf schon
versteckt: Ein POST lässt sich auch ohne die Seite absetzen, und zwischen dem
Aufbau der Seite und dem Klick können Minuten liegen.

## Phase 2 — Die Schrittfolge (0 bis 72 s)

### Der Rhythmus eines einzelnen Schritts

Jeder der acht **Set-Schritte** durchläuft dieselben Stationen; der
Hydraulikschritt hat seinen eigenen Rhythmus und steht in Abschnitt 1a. Die
Zeitangaben sind relativ zum Beginn des Schritts.

Zeit im Schritt | Was passiert
:--- | :---
0 ms | Kommando durch `build_heatpump_command()` — Bereichsprüfung, Maskenmerge in `mainCommand`, Sammelfenster geöffnet
+0,5 s | Telegramm geht an die Wärmepumpe (`COMMANDTIMER`). Läuft gerade ein Lesefenster, wird je Runde um 0,5 s verschoben, höchstens viermal
+2 bis 8 s | Die Wärmepumpe übernimmt das Kommando (KNX-Messung 2026-08-16); der Abfragezyklus liest den zugehörigen TOP alle 5 bis 6 s zurück
laufend | `notbetrieb_loop()` vergleicht den zurückgelesenen TOP mit dem Sollwert — leerer Text zählt dabei **nicht** als Bestätigung
**+8 s** | Frühestens jetzt gilt der Schritt als erledigt und der nächste geht raus. Die Mindestwartezeit verhindert, dass ein Rückgabewert von *vor* dem Kommando den Schritt bestätigt
+20 s | Kam der Wert bis dahin nicht zurück: ROT, sofortiger Abbruch

Die Schritte laufen **einzeln** und nicht in einem Sammelfenster. Ein Handler,
der sieben Kommandos hintereinander absetzt, packt sie alle in *ein* Telegramm —
dann konkurriert das Kurvenschreiben mit dem Werks-Reset des Moduswechsels, und
welcher gewinnt, ist unbekannt.

### Die neun Schritte

Nr | t ab Klick | Kommando | TOP | Warum an dieser Stelle
---: | ---: | :--- | ---: | :---
**1** | **0 s** | **Hydraulik auf AUS (1-stufig)** | — | **Ganz vorn**: Bricht er ab, ist an der Wärmepumpe noch nichts verstellt. Bestätigt durch `{"POWER":"OFF"}` vom Switch
2 | +8 s | `OperationMode` = 0 (Heat only) | 4 | Vor allem anderen an der WP, sonst schaltet der Knopf am Ende eine Anlage ein, die auf Kühlen steht
3 | +16 s | `HeatingMode` = 0 (Comp. Curve) | 76 | Der Moduswechsel setzt die Kurvenpunkte auf die Panasonic-Werksvorgaben zurück — deshalb **vor** der Kurve
4 | +24 s | `Z1HeatCurveTargetHighTemp` („VL kalt") | 29 | Erst jetzt hält die Kurve; vorher geschrieben wäre sie umsonst
5 | +32 s | `Z1HeatCurveTargetLowTemp` („VL warm") | 30 |
6 | +40 s | `Z1HeatCurveOutsideLowTemp` („AT kalt") | 32 |
7 | +48 s | `Z1HeatCurveOutsideHighTemp` („AT warm") | 31 |
8 | +56 s | `WaterPump` = 0 (auto) | 104 | Die Steuerung lässt die Pumpe im Umpumpbetrieb auf **Fix** laufen; im Notbetrieb gehört sie zurück auf bedarfsgeregelt. **Hinter** allen Moduswechseln, damit keiner sie zurückstellt
9 | +64 s | `Heatpump` = 1 | 0 | **Zuletzt** — erst wenn Hydraulik, Betriebsart, Kurve und Pumpe stehen, darf die Anlage anlaufen
— | **+72 s** | **GRÜN** | | alle neun Schritte bestätigt

Legt jemand den KNX-Schalter mitten im Lauf auf Kühlen, bricht der Lauf sofort
ab — noch vor jeder anderen Prüfung. Ein bestätigter Schritt ist in einer
kühlenden Anlage nichts wert.

## Phase 3 — Ergebnis und Verfall

t | Was passiert
:--- | :---
+72 s | **GRÜN**, Zeitstempel gesetzt, MQTT-Log „Notbetrieb GRUEN: alle Schritte zurueckgelesen"
alle 2 s | Der Browser holt `/notbetrieb/status` und schreibt die Anzeige fort — auch die Sperre, der Knopf gibt sich also von selbst frei
GRÜN + 15 min | Die Anzeige fällt auf BEREIT zurück und der Knopf steht wieder da. Im MQTT-Log bleibt der Lauf vollständig nachlesbar
danach | Die Firmware sendet **nichts** nach. Die Wärmepumpe fährt ihre Kurve allein weiter

**GRÜN heißt zurückgelesen, nicht „es wird warm".** Der Knopf meldet GRÜN,
sobald alle Schritte bestätigt sind — ob daraus Wärme wird, entscheidet die
Wärmepumpe. In Etappe 5 blieb die Anlage bei 0 Hz, weil die KNX-Freigabe für den
Kompressor fehlte; erst Etappe 6 hat mit freigegebenem Kompressor die ganze
Kette gezeigt: Knopf → Kurve → Wärme.

**Seit dem 2026-08-29 ist genau dieser Fall der Ausnahmefall** (Owner-Entscheid):
Der KNX-Kanal der Kompressorfreigabe steht dauerhaft auf True. Umgebaut wurde
nichts — der Kanal lässt sich im Flow weiter abschalten, und **für Wartung ist
das jetzt sein einziger Zweck**. Wer im Notbetrieb GRÜN sieht und die Anlage
bleibt trotzdem kalt, sucht deshalb nicht mehr zuerst beim Kompressorkontakt,
sondern fragt: Steht die Wartungsabschaltung? Aus demselben Grund nennt das
grüne Panel den Kompressor nicht mehr (neuer Wortlaut vom Familienrat,
2026-08-29, ausgeliefert mit der nächsten Version).

---

# 1a. Der Hydraulikschritt

Er steht seit 3.15.0 in **beiden** Schrittfolgen an Position 1 und ist der
einzige Schritt, der nicht mit der Wärmepumpe spricht.

## Warum er sein muss

Der Notbetrieb setzt hydraulisch **1-stufigen** Betrieb voraus. Steht die
Hydraulik auf 2-stufig, während eine Stufe im Warmwasser-Notbetrieb läuft,
**schiebt der Warmwasserbetrieb bis zu 57 °C in den Heizkreis** — die
Fußbodenheizung verträgt das nicht gut (Owner, 2026-08-26). Im Normalbetrieb
schaltet die Kaskadensteuerung den Switch; im Notbetriebsfall ist genau die weg.

## Was er tut

Ein Sonoff TH mit Tasmota, Adresse aus den Einstellungen (`hydraulik_switch`).
**EIN = 2-stufig, AUS = 1-stufig.**

Nr | Was | Antwort | Folge
---: | :--- | :--- | :---
1 | `GET /cm?cmnd=Power` | `{"POWER":"OFF"}` | schon 1-stufig, Schritt erledigt, Logzeile „stand bereits"
1 | `GET /cm?cmnd=Power` | `{"POWER":"ON"}` | weiter mit 2
1 | `GET /cm?cmnd=Power` | keine oder unverständliche Antwort | **Abbruch**, kein zweiter Request
2 | `GET /cm?cmnd=Power%20Off` | `{"POWER":"OFF"}` | Schritt erledigt
2 | `GET /cm?cmnd=Power%20Off` | alles andere | **Abbruch**

Der Zustand wird **vorher gelesen und nicht blind geschaltet**, obwohl ein
einzelnes `Power Off` beide Fälle abdecken würde: Tasmota antwortet auch dann
mit `"OFF"`, wenn der Schalter schon aus war. Nur so steht im Log, ob
tatsächlich umgeschaltet wurde.

**Die Antwort kommt chunked, ohne `Content-Length`** (am 2026-08-27 an Tasmota
12.0.2 gemessen). Die Firmware liest sie deshalb byteweise in einen festen
Puffer und hört auf, sobald `"OFF"` oder `"ON"` darin steht — ein `readBytes()`
auf Puffergröße hätte bei einer 15 Byte langen Antwort jedes Mal das volle
Timeout abgesessen, und `getString()` hätte auf dem Heap in der Größe der
Antwort allokiert.

**Gewartet wird dabei so lange wie für den ganzen Request**, nicht kürzer. Auch
das ist eine Lehre vom 2026-08-27: Hier stand zuerst eine eigene Frist von
300 ms, weil der Rumpf „praktisch immer schon im Puffer liegt". Fürs Lesen
stimmt das, fürs **Schalten** nicht — auf `Power Off` legt Tasmota erst das
Relais um und veröffentlicht den neuen Zustand per MQTT, bevor es antwortet. Der
Lauf endete dann mit `liess sich nicht schalten (HTTP 200)`: Die Verbindung
stand, der Switch hatte sauber gearbeitet, und die Firmware hatte die Antwort
verpasst.

Der Schritt hält die **Mindestwartezeit von 8 s** ein wie jeder andere, obwohl
er in Millisekunden fertig ist: Der Automat kennt genau einen Rhythmus, und die
8 s fallen ohnehin in die 90 s der Stellantriebe.

## Die Umwälzpumpe gehört mit zurück

Der Hydraulikschritt hat einen Zwilling am anderen Ende der Folge: **`WaterPump`
= 0.** Am 2026-08-27 an beiden Stufen beobachtet — nach dem Umschalten auf
2-stufiges Umpumpen stand TOP104 auf `1 Fix` und die Pumpe lief mit rund
16 l/min dauerhaft durch. Gesetzt hat das die Kaskadensteuerung, also genau die,
die im Notbetriebsfall weg ist.

An der Anlage belegt: TOP104 `1 Fix` → `0 Auto`, Pump_Flow 16,24 → 0,13 l/min.
Der Re-Assert stellt beides hinterher wieder her — derselbe Kreislauf wie bei
allen anderen Werten.

## Warum die 90 s keine Wartezeit erzwingen

Das Relais bestätigt sich sofort, die beiden motorischen Stellantriebe brauchen
je 90 s, und `Heatpump = 1` geht an Stufe 2 schon nach 24 s raus. **Der
Kompressor braucht trotzdem länger als die Ventile:** Nach dem Einschalten
vergehen rund drei Minuten, bis die Wärmepumpe ihn hochfährt; zunächst läuft nur
die Umwälzpumpe an (Owner, 2026-08-26).

**Der Beleg ist der Normalbetrieb selbst.** Dort sendet die Kaskadensteuerung
ihre Kommandos an die Wärmepumpen **gleichzeitig** mit dem Switch-Kommando —
dieselbe Konstellation, seit jeher, ohne Schaden. Der Notbetrieb ist der
günstigere Fall: Zwischen Hydraulikschritt und `Heatpump = 1` liegen 16 s
(Stufe 2) beziehungsweise 48 s (Stufe 1) zusätzlicher Vorsprung.

## Warum der Request `loop()` blockieren darf

`HTTPClient.GET()` hält `loop()` an, bis die Antwort da ist oder das Timeout
greift: kein `read_pana_data`, kein `timeout_serial`, kein Webserver. Der
UART-Empfangspuffer fasst 256 Byte, ein Telegramm hat 203 — ein zweites ginge
verloren.

Genau deshalb steht der Schritt **vorn**: In diesem Moment ist kein Kommando an
die Wärmepumpe unterwegs und kein Sammelfenster offen. Eine einmalige Blockade
trifft nur den Abfragezyklus, der ohnehin nur liest, und verschiebt ihn.

Damit sie im Fehlerfall nicht ausufert:

* **Timeout 5000 ms.** Der Entwurf sah 1500 ms vor („antwortet er in 1,5 s
  nicht, antwortet er nicht"). Der Prüflauf am 2026-08-27 hat das widerlegt:
  Beim **ersten** Kontakt nach einem Neustart scheiterte der Verbindungsaufbau
  reproduzierbar (`HTTP -1`), während jeder weitere Lauf durchlief. Das ist der
  Regelfall, nicht der Sonderfall — nach einem Stromausfall startet die Bridge
  neu und hat mit dem Switch noch nie gesprochen.
* **Höchstens ein Timeout je Lauf** — läuft schon der Lesevorgang ins Leere,
  kommt der zweite Request gar nicht erst.
* **Kein Wiederholungsversuch** — der Mensch steht vor der Seite und drückt
  erneut, das ist der bessere Wiederholungsversuch.

## Die Fehlerfälle

Lage | Zustand | Was der Mensch sieht
:--- | :--- | :---
Switch stand schon auf AUS | läuft weiter | nichts Besonderes; im Log „Hydraulik stand bereits auf 1-stufig"
Switch antwortet nicht (Timeout) | **ROT nach rund 5 s** | die Meldung unten
Switch antwortet, meldet aber weiter `"ON"` | **ROT** | die Meldung unten
Switch antwortet unverständlich | **ROT** | die Meldung unten
keine Adresse eingetragen | **ROT sofort** | die Meldung unten

> **Die Umschaltung der Hydraulik ist fehlgeschlagen, bitte den Switch im
> Waschraum von Hand auf AUS schalten**

Sie tritt an die Stelle des generischen „Hat nicht geklappt" — der Weg zurück
führt hier über einen Schalter im Haus und nicht über das Bedienfeld der
Wärmepumpe, an der nichts verstellt worden ist.

**Der Knopf kommt nach ROT von selbst zurück**, ohne Neuladen und ohne die 15
Minuten der Verfallszeit abzuwarten: Die Seite blendet ihn nur bei „läuft" und
GRÜN aus. Wer den Schalter von Hand legt und zurückkommt, drückt erneut — der
Lesevorgang meldet dann `"OFF"`, und die Folge läuft durch.

## Wer zurück auf 2-stufig schaltet

**Die Kaskadensteuerung, nicht die Firmware** — wie bei allen anderen Werten
auch (Entscheidung 7, Abschnitt 3). Ihr Re-Assert liest den Switch zurück,
stellt bei Abweichung nach und meldet sie.

Dabei gilt seit dem 2026-08-26 eine **Frische-Bedingung**: Ein Schaltbefehl geht
nur raus, wenn der Betriebsmodus von Stufe 2 nicht älter als zwölf Minuten ist.
Der Grund ist, dass die beiden Befehlswege den ioBroker über **verschiedene
Adapter** verlassen — die Wärmepumpenkommandos über `mqtt` (Port 1883, zugleich
der Broker der Bridges), das Switch-Kommando über `sonoff` (Port 1886). Genau
der Ausfall, für den der Notbetriebsknopf gebaut ist, trifft den `mqtt`-Adapter;
der `sonoff`-Adapter läuft weiter. Ein Re-Assert, der stur alle fünf Minuten
schaltet, legte die Hydraulik mitten im Notbetrieb zurück auf 2-stufig — der
Schaden von oben, nur von der anderen Seite verursacht.

---

# 2. Notbetrieb einschalten — Stufe 2, Rolle Warmwasser

Derselbe Automat, dieselben Regelzeiten, nur eine kürzere Folge: Warmwasser
braucht keine Kurve.

Nr | t ab Klick | Kommando | TOP | Anmerkung
---: | ---: | :--- | ---: | :---
**1** | **0 s** | **Hydraulik auf AUS (1-stufig)** | — | derselbe Schritt wie an Stufe 1 — und hier der wichtigere, siehe Abschnitt 1a
2 | +8 s | `OperationMode` = 3 (DHW only) | 4 | trägt auch im Kühlbetrieb — am 2026-08-20 an H2 gemessen (M3)
3 | +16 s | `DHWTemp` | 9 | der einzige gehaltene Wert dieser Rolle
4 | +24 s | `WaterPump` = 0 (auto) | 104 | wie an Stufe 1 — die Pumpe zurück auf bedarfsgeregelt
5 | +32 s | `Heatpump` = 1 | 0 |
— | **+40 s** | **GRÜN** | | an H2 gemessen: **GRÜN nach 43 s** (2026-08-27)

**Der Unterschied, der zählt:** TOP101 ist für diese Rolle **keine**
Freigabebedingung. Der Knopf an Stufe 2 funktioniert also auch im Sommer, wenn
die Anlage auf Kühlen steht — und genau dafür ist er gedacht. Der Gesamtdeckel
liegt hier bei 100 s statt 180 s, weil er sich aus der Schrittzahl ableitet.

---

# 3. Rückkehr der übergeordneten Steuerung

**Die Firmware schaltet nichts zurück.** Es gibt keinen Knopf „Notbetrieb aus"
und keinen Rückschaltpfad im Code (Entscheidung 7, dreifach an der Anlage
belegt). Zurück holt die Anlage der 5-Minuten-Re-Assert der Kaskadensteuerung.
Was HeishaMon in diesem Ablauf tut, ist ausschließlich: sich wieder verbinden
und die eintreffenden Kommandos weiterreichen.

t = 0 ist der Moment, in dem der Broker wieder läuft. **War der Broker nie weg**
und nur Node-RED still, entfällt Phase 1 und der Ablauf beginnt bei Phase 2.

## Phase 1 — Die Firmware verbindet sich wieder (0 bis 60 s)

t | Was passiert
:--- | :---
0 s | `mqtt.0` läuft wieder — die Firmware merkt davon zunächst nichts
0 bis 60 s | Reconnect-Backoff: erster Versuch 5 s nach dem Abriss, danach Verdopplung bis zum Deckel von 60 s. **Gemessen: 52 s**
**T** | `connect()` mit LWT, „Online" retained veröffentlicht, die 32 Set-Topics und der Notbetriebszweig abonniert
T | Karenzfenster auf T + 5 s gesetzt (`SUBSCRIBE_GRACE`)
T | Der Broker-Ausfall gilt als beendet. Lag er über der Karenz von 5 min: Log „Hausteuerung war X s nicht erreichbar", die Störmeldung auf der Seite verschwindet. Die Stumm-Uhr startet **jetzt erst**
T bis T+5 s | **Der Wiedereinspiel-Schwall.** Der Adapter schickt jedem neuen Abonnenten die gespeicherten Werte: `notbetrieb/*` wird angenommen — die Kurvenwerte sind binnen Sekunden wieder im RAM —, alle `set/*` werden verworfen und gezählt. **Gemessen: 34 verworfene Kommandos**
T + 5 s | Log: „N wiedereingespielte Set-Kommandos nach dem Verbinden verworfen"

Dass der Notbetriebszweig die Karenz **passieren** darf, ist kein Versehen,
sondern der Mechanismus: Nur so sind die Kurvenwerte nach jedem Neustart und
jedem Reconnect ohne Zutun von Node-RED wieder da.

## Phase 2 — Die Steuerung übernimmt (bis 5 min später)

t ab Reconnect | Akteur | Was passiert
:--- | :--- | :---
0 bis 5 min | **Node-RED** | Der erste echte Re-Assert (Takt 300,0 s, an H2 gemessen). **Gemessen: 39 s**
+0 ms | Firmware | Herzschlag vermerkt (Stumm-Uhr läuft neu), Abfragetimer gestoppt, alle Kommandos des Takts in **ein** Sammelfenster
+0,5 bis 2,5 s | Firmware | Ein Telegramm mit `HeatingMode` = 1, `OperationMode`, `Heatpump` und den Sollwerten
+4 bis 9 s | Wärmepumpe | Zurück im Direktbetrieb: TOP76 0 → 1, TOP0 1 → 0
+4 s nach dem Moduswechsel | Wärmepumpe | **Werks-Reset ein zweites Mal** — TOP27/29/30/42 springen auf 35, TOP32 auf −5, TOP28/72 auf 10

Der Werks-Reset läuft in **beide** Richtungen, beim Hin- wie beim
Zurückschalten. Er ist der Grund, warum die Anlage nach der Rückkehr noch einige
Minuten mit falschen Sollwerten läuft.

## Phase 3 — Aufräumen (bis rund 10 min)

t ab Reconnect | Akteur | Was passiert
:--- | :--- | :---
+ ein weiterer Takt (bis 5 min) | **Node-RED** | Sollwerte zurückgeholt. Gemessen: 01:11:35 (Etappe 5) und 01:53:40 (Etappe 6)
danach | [`test/kurven_sync.py`](test/kurven_sync.py) | Kurvenpunkte wiederhergestellt — **nicht** durch die Firmware
GRÜN + 15 min | Firmware | Die Anzeige fällt auf BEREIT. Das ist der **einzige** Vorgang, mit dem HeishaMon den Notbetrieb selbst „beendet"

---

# 4. Die Messbelege

Alle Läufe an der Anlage, protokolliert in
[`Vorhaben-Notbetrieb-Weboberflaeche.md`](Vorhaben-Notbetrieb-Weboberflaeche.md)
Abschnitt 10.

**Sie stammen aus der Zeit vor dem Hydraulikschritt** (Firmware 3.12.0), also
aus sieben- bzw. dreischrittigen Folgen. Ihre Aussage trägt trotzdem: Was sie
belegen, ist der Rhythmus von 8 s je Schritt und die Herkunft der Kurvenwerte
aus dem RAM — beides hat 3.15.0 nicht angefasst. Die Gesamtzeiten liegen seither
um zwei Schritte höher (72 s statt 56 s, 40 s statt 24 s) — Hydraulik am Anfang,
Umwälzpumpe vor dem Einschalten.

Lauf | Datum | Was er belegt | Ergebnis
:--- | :--- | :--- | :---
Etappe 5, Lauf A | 2026-08-21 | Die Sperre wirkt serverseitig | POST abgewiesen, TOP4/TOP101/TOP0 30 s später unverändert
Etappe 5, Lauf B | 2026-08-21 | Die Schrittfolge an H1 | **GRÜN nach 57 s**, jeder Schritt genau 8 s
Etappe 5, Rückweg | 2026-08-21 | Die Rückkehr durch den Re-Assert | 9 s nach dem Takt von 01:06:24 war die Anlage von allein zurück
Etappe 6 | 2026-08-21 | Der Knopf **ohne Broker** | **GRÜN nach 58 s**, Kompressor 26 → 33 Hz
Etappe 6, Rückweg | 2026-08-21 | Reconnect nach 8 min Ausfall | 52 s Backoff, Re-Assert 39 s später, Sollwerte nach 6:17 min
Warmwasser an H2 | 2026-08-21 | Die kurze Folge im Kühlbetrieb | **GRÜN nach 24 s**

**Der tragende Beleg ist in beiden Läufen die 34.** Diesen Kurvenwert schreibt
sonst niemand — `kurven_sync.py` lässt ihn im Direktbetrieb bewusst aus, und vor
dem Lauf stand dort eine 20. Er kann nur aus dem RAM der Firmware gekommen sein.
In Etappe 6 war der Broker dabei seit vier Minuten tot; es gab keine Leitung,
aus der er sonst hätte kommen können.

**Eine Regel fürs Messen, die aus Etappe 6 stammt:** Im Ausfallfall ist der
ioBroker **keine gültige Messquelle**. Sein Datenpunkt stand seit dem
Broker-Stopp eingefroren auf dem Wert von *vor* dem Notbetrieb und sah dabei aus
wie ein aktueller Wert. Dann zählt nur, was die Bridge selbst über
`/tablerefresh` herausgibt.

---

# 5. Was das für den Betrieb heißt

**Kommt die Steuerung kurz nach dem Notbetrieb zurück, läuft der Werks-Reset
mit.** Zwischen GRÜN und dem ersten Re-Assert steht die Anlage im Kurvenbetrieb;
das Zurückschalten setzt den Sollwert auf 35 °C, und dort bleibt er bis zum
übernächsten Takt — knapp fünf Minuten. In Etappe 6 war das als
`Heat_Energy_Production` von 2400 W statt 200 W sichtbar. Es läuft von allein
aus, aber wer in dieser Zeit auf die Werte schaut, sollte wissen, woher sie
kommen.

**GRÜN steht nach dem Ende noch bis zu 15 Minuten da.** Die Anzeige ist an
dieser Stelle bewusst kein Abbild des Anlagenzustands: Sie sagt „dieser Lauf ist
durchgelaufen", nicht „der Notbetrieb ist gerade aktiv". Die Anlage kann längst
wieder in Steuerungshand sein, während auf der Seite noch GRÜN steht. Wer wissen
will, was gerade gilt, liest `Heating_Mode` (TOP76) auf der Startseite.

**Im Sommer schaltet Stufe 1 nicht.** Der Notbetrieb der Rolle Heizen ist auf
den Winter beschränkt (Owner-Entscheidung 2026-08-23): TOP101 muss sauber auf 0
stehen, sonst bleibt der Knopf gesperrt. Im Sommer trägt Stufe 2 mit Warmwasser,
und die läuft auch im Kühlbetrieb.
