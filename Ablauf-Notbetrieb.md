# Ablauf des Notbetriebs — die Schritte mit Zeiten

Was in der Firmware passiert, wenn jemand den Notbetriebsknopf drückt, und was
passiert, wenn die Kaskadensteuerung zurückkommt. Beide Abläufe Schritt für
Schritt, jeweils mit der Zeit ab dem auslösenden Ereignis.

**Stand:** 2026-08-30, Firmware 3.18.0 (Heizstabschritt; 3.17.0 lief seit dem
2026-08-28 auf allen vier Boards).
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
Notbetrieb einschalten, Stufe 1 (Heizen) | **80 s** | die Firmware, Schritt für Schritt
Notbetrieb einschalten, Stufe 2 (Warmwasser) | **48 s** | die Firmware, Schritt für Schritt
Rückkehr der Steuerung | **bis 5 min** bis zur Übernahme, rund 10 min bis alles steht | Node-RED — **die Firmware tut nichts**

---

# 1. Notbetrieb einschalten — Stufe 1, Rolle Heizen

Zehn Schritte, t = 0 ist der Klick auf den Knopf. **Schritt 1 stellt seit 3.15.0
die Hydraulik** ([Abschnitt 1a](#1a-der-hydraulikschritt)), **Schritt 2 nimmt seit
3.18.0 den Heizstab zurück** ([Abschnitt 1b](#1b-der-heizstabschritt)), die
übrigen acht gehen an die Wärmepumpe.

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

## Phase 2 — Die Schrittfolge (0 bis 80 s)

### Der Rhythmus eines einzelnen Schritts

Jeder der neun **Set-Schritte** durchläuft dieselben Stationen; der
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

### Die zehn Schritte

Nr | t ab Klick | Kommando | TOP | Warum an dieser Stelle
---: | ---: | :--- | ---: | :---
**1** | **0 s** | **Hydraulik auf AUS (1-stufig)** | — | **Ganz vorn**: Bricht er ab, ist an der Wärmepumpe noch nichts verstellt. Bestätigt durch `{"POWER":"OFF"}` vom Switch
**2** | **+8 s** | **`ForceHeater` = 0 (Heizstab aus)** | **68** | **Vor allem anderen an der WP**: Bricht er ab, tut die Anlage nichts mehr — Stab aus, Umwälzpumpe aus, Wärmepumpe aus. Siehe [Abschnitt 1b](#1b-der-heizstabschritt)
3 | +16 s | `OperationMode` = 0 (Heat only) | 4 | Sonst schaltet der Knopf am Ende eine Anlage ein, die auf Kühlen steht
4 | +24 s | `HeatingMode` = 0 (Comp. Curve) | 76 | Der Moduswechsel setzt die Kurvenpunkte auf die Panasonic-Werksvorgaben zurück — deshalb **vor** der Kurve
5 | +32 s | `Z1HeatCurveTargetHighTemp` („VL kalt") | 29 | Erst jetzt hält die Kurve; vorher geschrieben wäre sie umsonst
6 | +40 s | `Z1HeatCurveTargetLowTemp` („VL warm") | 30 |
7 | +48 s | `Z1HeatCurveOutsideLowTemp` („AT kalt") | 32 |
8 | +56 s | `Z1HeatCurveOutsideHighTemp` („AT warm") | 31 |
9 | +64 s | `WaterPump` = 0 (auto) | 104 | Die Steuerung lässt die Pumpe im Umpumpbetrieb auf **Fix** laufen; im Notbetrieb gehört sie zurück auf bedarfsgeregelt. **Hinter** allen Moduswechseln, damit keiner sie zurückstellt
10 | +72 s | `Heatpump` = 1 | 0 | **Zuletzt** — erst wenn Hydraulik, Heizstab, Betriebsart, Kurve und Pumpe stehen, darf die Anlage anlaufen
— | **+80 s** | **GRÜN** | | alle zehn Schritte bestätigt

Legt jemand den KNX-Schalter mitten im Lauf auf Kühlen, bricht der Lauf sofort
ab — noch vor jeder anderen Prüfung. Ein bestätigter Schritt ist in einer
kühlenden Anlage nichts wert.

## Phase 3 — Ergebnis und Verfall

t | Was passiert
:--- | :---
+80 s | **GRÜN**, Zeitstempel gesetzt, MQTT-Log „Notbetrieb GRUEN: alle Schritte zurueckgelesen"
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
je 90 s, und `Heatpump = 1` geht an Stufe 2 schon nach 40 s raus. **Der
Kompressor braucht trotzdem länger als die Ventile:** Nach dem Einschalten
vergehen rund drei Minuten, bis die Wärmepumpe ihn hochfährt; zunächst läuft nur
die Umwälzpumpe an (Owner, 2026-08-26).

**Der Beleg ist der Normalbetrieb selbst.** Dort sendet die Kaskadensteuerung
ihre Kommandos an die Wärmepumpen **gleichzeitig** mit dem Switch-Kommando —
dieselbe Konstellation, seit jeher, ohne Schaden. Der Notbetrieb ist der
günstigere Fall: Zwischen Hydraulikschritt und `Heatpump = 1` liegen 40 s
(Stufe 2) beziehungsweise 72 s (Stufe 1) zusätzlicher Vorsprung.

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

## Im Test stellt die lebende Steuerung binnen 20 s zurück

Am 2026-08-30 im ioBroker-Log gesehen, beide Läufe:

Zeit | Meldung
:--- | :---
16:52:22 | `HydrStatus`: Relais AUS, WP2 meldet aber 2-stufig-fähig — der Hydraulikschritt hat geschaltet
**16:52:44** | `WegeVentilRelais`: „Hydraulik steht auf AUS/1-stufig, Soll ist EIN/2-stufig (WP2-Modus 4) — wird nachgestellt"
16:55:59 | derselbe Ablauf beim Lauf an Stufe 1
**16:57:44** | wieder nachgestellt

**Nach rund 20 s war die Hydraulik wieder auf 2-stufig** — mitten im laufenden
Notbetrieb. Das ist **kein Fehler, sondern die Testbedingung**: Die Steuerung
lebte, ihre Frische-Bedingung war erfüllt, und sie tat genau das, wofür sie
gebaut ist. Im echten Notbetriebsfall ist sie weg, dann bleibt es bei
1-stufig.

Für die Bewertung eines Testlaufs heißt das: **Der Hydraulikschritt lässt sich
mit lebender Steuerung nicht auf Dauerwirkung prüfen**, nur auf Ausführung.
Dass er geschaltet hat, steht in der ersten Zeile; dass es nicht hielt, ist die
Antwort der Gegenseite und in Abschnitt „Wer zurück auf 2-stufig schaltet"
beschrieben.

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

# 1b. Der Heizstabschritt

Er steht seit 3.18.0 in **beiden** Schrittfolgen an Position 2 — direkt hinter
der Hydraulik und vor jedem anderen Kommando an die Wärmepumpe.

## Warum er sein muss

Seit dem 2026-08-30 benutzt die Kaskadensteuerung `SET39 ForceHeater` im
**Regelbetrieb**. Drei Heizmodi (App-Menü 9/10/11) ersetzen den Verdichter durch
den Backup-Heizstab der Wärmepumpen: 3 kW an einer Stufe, 6 kW mit beiden.

App-Modus | Hauptmodus | `SET39` an | `SET1 Heatpump`
:--- | ---: | :--- | :---
9 · Heizstab 3 kW | 5 | H1 | **0** an beiden
10 · Heizstab 3 kW HH-Boost | 5 | H1 | **0** an beiden
11 · Heizstab 6 kW | 6 | H1 + H2 | **0** an beiden

**`Heatpump = 0` ist dabei Voraussetzung, nicht Nebenwirkung**: Die Wärmepumpe
nimmt `SET39` nur bei ausgeschalteter Einheit an (Messung 2026-08-28,
[`MQTT-Topics.md`](MQTT-Topics.md); am Bedienpanel wird die Anforderung bei
laufender Einheit abgelehnt).

Genau das ist die Lage, in der der Notbetriebsknopf gefährlich wurde. Bis 3.17.0
kam `SET39` in keiner der beiden Schrittfolgen vor, und beide enden mit
`Heatpump = 1`.

**Und dieser letzte Schritt hätte gar nicht gewirkt** — am 2026-08-30 gemessen,
siehe [unten](#die-wärmepumpe-schaltet-mit-stehendem-heizstab-nicht-ein). Die
Wärmepumpe nimmt kein Einschaltkommando an, solange der Heizstab-Auftrag steht.
Der Notbetrieb aus einem Heizstab-Modus heraus wäre also nicht etwa „gelaufen,
aber mit stehendem Stab", sondern hätte **nach 20 s in ROT geendet, ohne die
Anlage einzuschalten** — ausgerechnet in der Lage, in der jemand auf ihn
angewiesen ist.

Zwei Fälle, die sich unterscheiden:

* **Die Steuerung lebt.** Der Notbetrieb wird ohnehin binnen ≤ 5 min vom
  Re-Assert überschrieben — bekanntes Verhalten, es betrifft alle Kanäle. Neu
  ist nur, dass dabei auch `SET39 = 1` wieder gesetzt wird.
* **Die Steuerung ist tot** — der eigentliche Notbetriebsfall. Dann nimmt
  niemand `SET39` zurück. **`SET39` ist ein Zustand, den niemand automatisch
  räumt**, und die Firmware hat keinen Rückschaltpfad (Entscheidung 7).

## Die Wärmepumpe schaltet mit stehendem Heizstab nicht ein

**Gemessen am 2026-08-30, 17:32 bis 17:35 an Stufe 2**, Anlage aus, Betriebsart
`DHW`, zwei Versuche unter sonst gleichen Bedingungen. Die Kommandos gingen
einzeln über MQTT, nicht über das Bedienpanel:

Versuch | Ausgangslage | Kommando | Ergebnis
:--- | :--- | :--- | :---
1 | TOP68 = 1 (`Active`) | `Heatpump = 1` um 17:32:56 | **TOP0 bleibt 0 über 48 s.** TOP68 fällt dabei um 17:33:06 **von selbst auf 0**
2 | TOP68 = 0 | `Heatpump = 1` um 17:34:48 | **TOP0 geht nach 10 s auf 1**

Die Wärmepumpe **verweigert das Einschalten, solange der Heizstab-Auftrag
steht** — und räumt den Auftrag dabei ab, ohne selbst anzulaufen. Der
Einschaltwunsch ist damit verbraucht; er müsste erneut gesendet werden.

Das ergänzt, was bisher nur für die Gegenrichtung bekannt war: Das Bedienpanel
lehnt eine Heizstab-Anforderung bei laufender Einheit mit einem Hinweis ab
(„die Anlage muss ausgeschaltet sein", Owner am Panel geprüft). **Die Sperre
gilt in beide Richtungen** — nur meldet sie sich auf dem Protokollweg nicht,
sondern äußert sich als Kommando, das nichts bewirkt.

**Für die Schrittfolge ist das der eigentliche Grund, warum der Schritt weit
vorn steht.** Stünde er hinten oder gar nicht, träfe `Heatpump = 1` auf eine
Wärmepumpe, die es nicht annimmt — der Schritt käme nicht zurück, und der Lauf
endete nach 20 s in ROT. An Position 2 ist der Auftrag acht Schritte vorher
geräumt, und das Einschalten trifft auf eine Einheit, die es annehmen kann.
Genau so lief es in den beiden Läufen eine Dreiviertelstunde zuvor.

**Auch der Fall „beide Kommandos in einem Telegramm" ist inzwischen gemessen** —
noch am selben Abend, an der Steuerungsseite. Beim Moduswechsel aus dem
Heizstab-Modus gingen `Heatpump = 1` und `ForceHeater = 0` gemeinsam raus:

```
h1  18:46:36  SET1 Heatpump: 1 / SET39 ForceHeater: 0   (ein Telegramm)
    18:46:43  TOP0 Heatpump_State: 1     ← geht an
    18:46:49  TOP0 Heatpump_State: 0     ← und 6 s später wieder aus
```

An beiden Stufen, auf die Sekunde parallel. **Die Wärmepumpe nimmt das
Einschalten also auch dann nicht an, wenn der Heizstab im selben Telegramm
beendet wird** — sie schaltet kurz ein und fällt zurück. Ein „`heater` steht
vorn in der Warteschlange" genügt nicht; es braucht zeitlichen Abstand, weil das
Sammelfenster der Bridge alles binnen 2 s zu einem Telegramm zusammenfasst.

**Für den Notbetrieb ist das ohne Belang** — zwischen Schritt 2 und dem
Einschalten liegen acht Schritte, also 64 s. Die Kaskadensteuerung hat daraus
`Hauptmodus-Verteiler V6.8` gemacht: Sie hält `heatpump = ON` zurück, bis der
Heizstab-Ausstieg 10 s her ist (`nodered-flows/HEIZSTAB-MODI.md` §4).

## Die Umwälzpumpe ist der eigentliche Schaden

**Sie hängt am Kommando, nicht am Heizstab** (gemessen 2026-08-28): Sie startet
mit `SET39`, läuft weiter, nachdem der Stab thermisch abgeschaltet hat, und
stoppt erst mit `ForceHeater = 0`. Ein vergessenes Kommando lässt sie also
dauerhaft laufen — dieselbe Art von Befund wie beim Umpumpbetrieb, der den
Schritt `WaterPump = 0` ans Ende der Folge gebracht hat.

## Was er tut

Ein gewöhnlicher Set-Schritt: `set/ForceHeater` = 0, zurückgelesen an **TOP68**
`Force_Heater_State`, Mindestwartezeit 8 s, Timeout 20 s. Nichts an ihm ist
Sonderfall — das ist Absicht.

Lage beim Drücken | Was passiert | Kosten
:--- | :--- | ---:
TOP68 steht auf 0 (kein Heizstab-Modus lief) | der Schritt ist nach der Mindestwarte bestätigt | 8 s
TOP68 steht auf 1 | das Kommando geht raus, TOP68 fällt auf 0 | 8 s
TOP68 bleibt auf 1 | **ROT**, kein weiteres Kommando an die WP | 20 s

**Der Regelfall kostet also 8 s und sonst nichts.** Wer nie einen Heizstab-Modus
fährt, merkt von diesem Schritt nur die acht Sekunden längere Laufzeit.

## Warum das Timeout bei 20 s bleibt

`SET39` ist unser langsamstes Kommando — aber nur in einer Richtung. **Beim
Einschalten** lag die Übernahme in einem Lauf bei einer knappen halben Minute
(`MQTT-Topics.md`, 2026-08-28): Die Wärmepumpe prüft erst ihre eigenen
Bedingungen und übernimmt den Wert dann. **Die Rücknahme**, um die es hier allein
geht, lag im Erstlauf der Steuerungsseite bei **7 s an beiden Stufen**
(`nodered-flows/HEIZSTAB-MODI.md` §5, 2026-08-30) — innerhalb der Mindestwarte
von 8 s, die der Schritt ohnehin absitzt.

Kommt sie wider Erwarten nicht zurück, ist **ROT die richtige Antwort**: An der
Wärmepumpe ist in diesem Moment nichts verstellt, die Anlage steht so da wie
vorher, und der Mensch drückt erneut — dieselbe Logik wie beim Hydraulikschritt.
Ein Wiederholungsversuch der Firmware wäre der schlechtere Weg.

## An der Anlage belegt — 2026-08-30, beide Rollen

Zwei Läufe unmittelbar nach dem Rollout von 3.18.0, **mit tatsächlich laufendem
Heizstab als Ausgangslage**: App-Modus 11 (6 kW), TOP68 und TOP60 an beiden
Stufen auf 1, beide Verdichter aus, Umwälzpumpen auf `Fix`, KNX auf Heizen.

**Lauf 1 — Stufe 2, Rolle Warmwasser** (Klick 16:52:21, abgelesen an der
Switch-Meldung im ioBroker-Log um 16:52:22,976)

Zeit | Was zurückkam | Schritt
:--- | :--- | :---
16:52:39 | **TOP68 1 → 0, TOP60 1 → 0** — der Stab geht aus | **2**
16:52:48 | TOP4 4 → 3 (DHW only) | 3
16:53:05 | TOP104 → 0 (Auto) | 5
16:53:10 | TOP0 0 → 1 — die Anlage geht an | 6
**16:53:13** | Status `2;7;6;0;0` — **GRÜN nach 52 s** (Regelzeit 48 s, dazu bis zu 3 s Abfragetakt) | —

**Lauf 2 — Stufe 1, Rolle Heizen** (Klick 16:55:58, ebenso abgelesen)

Zeit | Was zurückkam | Schritt
:--- | :--- | :---
16:56:15 | **TOP68 1 → 0, TOP60 1 → 0** | **2**
16:56:21 bis 17:57:16 | Schrittzähler 3 → 10, jeder Schritt drei Abfragen à 3 s | 3–10
**16:57:19** | Status `2;11;10;0;0` — **GRÜN nach 81 s** bei 80 s Regelzeit | —

Endzustand an H1: TOP0 `On`, TOP4 `Heat`, TOP76 `Comp. Curve`, Kurve
**34 / 26 / 15 / −10** aus dem RAM, TOP68 und TOP60 `Inactive`, TOP104 `Auto`,
Tasmota `{"POWER":"OFF"}`.

**Drei Befunde:**

* **Die Rücknahme kam nach 6 bis 8 s zurück** (Abtastung 4 s) — also innerhalb
  der Mindestwartezeit von 8 s, die der Schritt ohnehin absitzt. Die 7 s der
  Steuerungsseite sind damit unabhängig bestätigt, und die 20 s Timeout haben
  reichlich Luft.
* **Der Stab war aus, bevor die Anlage anlief** — 34 s Vorsprung an Stufe 2,
  64 s an Stufe 1. Genau das ist der Zweck des Schritts.
* **Die andere Stufe bleibt unberührt.** Während des Laufs an H2 stand H1
  durchgehend auf TOP68 = 1 und TOP60 = 1, der Stab dort heizte weiter. Das ist
  keine Beobachtungslücke, sondern der Befund aus dem Abschnitt unten — hier
  einmal an der Anlage gesehen.

**Der Re-Assert um 16:55:29 hat den Kreislauf mitbelegt:** Er stellte Modus 11
wieder her, TOP68 an H2 ging zurück auf 1 und der Verdichter wieder aus — die
Steuerung holt den Kanal zurück wie jeden anderen auch.

## Was er nicht kann: die andere Stufe

**Jede Bridge spricht nur mit ihrer eigenen Wärmepumpe.** Lief die Anlage im
6-kW-Modus (App-Modus 11), steht `SET39` auch an der anderen Stufe — dieser Lauf
erreicht sie nicht.

> **Lief die Anlage mit Heizstab, gehört der Knopf an BEIDEN Bridges gedrückt.**
> Sonst bleibt an der anderen Stufe der Heizstab-Auftrag stehen und ihre
> Umwälzpumpe läuft weiter.

Das steht bewusst **nur hier und im README**, nicht im grünen Panel: Dessen
Wortlaut kommt vom Familienrat (2026-08-29) und soll knapp bleiben — ein Satz,
der bei jedem Lauf erscheint, aber nur im Heizstabfall gilt, verwässert ihn
(Owner-Entscheid 2026-08-30).

**Der 1-stufige Hydraulikschritt entkoppelt die zweite Stufe zusätzlich** —
Modus 11 fährt 2-stufig, der Notbetrieb stellt in Schritt 1 auf 1-stufig. Deren
Heizstab arbeitete danach in einen abgekoppelten Kreis.

## Was der Notbetrieb ebenfalls nicht zurückholt

**Modus 10 (HH-Boost) schließt den Mischer und schaltet die VH-Pumpe ab.** Beides
hängt an der Kaskadensteuerung und nicht an der Firmware — der Notbetrieb kann es
so wenig zurücknehmen wie irgendetwas anderes außerhalb der Wärmepumpe. Wer aus
Modus 10 heraus in den Notbetrieb geht, bekommt Wärme, aber weiterhin vorrangig
ins Hinterhaus. Der Hydraulikschritt ist die einzige Ausnahme, und er ist es aus
einem Grund: Ohne ihn schiebt der Warmwasserbetrieb bis zu 57 °C in die
Fußbodenheizung.

## Wer `SET39` wieder setzt

**Die Kaskadensteuerung, wie bei allen anderen Werten auch.** Der Kanal liegt im
Hauptmodus-Verteiler und damit im 5-Minuten-Re-Assert; in allen Modi außer 9/10/11
sendet sie aktiv `SET39 = 0`.

Steht beim Re-Assert weiterhin ein Heizstab-Modus, setzt sie `SET39 = 1` zurück —
und das trifft eine Anlage, die der Notbetrieb gerade eingeschaltet hat. Es löst
sich von selbst auf: Die Modi 5/6 senden `heatpump` **vor** `heater`, die Einheit
geht also zuerst wieder aus, und dann wird `SET39` angenommen
(`HEIZSTAB-MODI.md` §4). Der Rest ist der übliche Kreislauf — Notbetrieb setzt,
Re-Assert holt zurück.

---

# 2. Notbetrieb einschalten — Stufe 2, Rolle Warmwasser

Derselbe Automat, dieselben Regelzeiten, nur eine kürzere Folge: Warmwasser
braucht keine Kurve.

Nr | t ab Klick | Kommando | TOP | Anmerkung
---: | ---: | :--- | ---: | :---
**1** | **0 s** | **Hydraulik auf AUS (1-stufig)** | — | derselbe Schritt wie an Stufe 1 — und hier der wichtigere, siehe Abschnitt 1a
**2** | **+8 s** | **`ForceHeater` = 0 (Heizstab aus)** | **68** | derselbe Schritt wie an Stufe 1: Der 6-kW-Modus setzt `SET39` an **beiden** Stufen, siehe Abschnitt 1b
3 | +16 s | `OperationMode` = 3 (DHW only) | 4 | trägt auch im Kühlbetrieb — am 2026-08-20 an H2 gemessen (M3)
4 | +24 s | `DHWTemp` | 9 | der einzige gehaltene Wert dieser Rolle
5 | +32 s | `WaterPump` = 0 (auto) | 104 | wie an Stufe 1 — die Pumpe zurück auf bedarfsgeregelt
6 | +40 s | `Heatpump` = 1 | 0 |
— | **+48 s** | **GRÜN** | | vor dem Heizstabschritt an H2 gemessen: GRÜN nach 43 s bei 40 s Regelzeit (2026-08-27)

**Der Unterschied, der zählt:** TOP101 ist für diese Rolle **keine**
Freigabebedingung. Der Knopf an Stufe 2 funktioniert also auch im Sommer, wenn
die Anlage auf Kühlen steht — und genau dafür ist er gedacht. Der Gesamtdeckel
liegt hier bei 120 s statt 200 s, weil er sich aus der Schrittzahl ableitet.

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
aus dem RAM — daran haben weder 3.15.0 noch 3.18.0 etwas geändert. Die
Gesamtzeiten liegen seither um drei Schritte höher (80 s statt 56 s, 48 s statt
24 s) — Hydraulik am Anfang, Heizstab dahinter, Umwälzpumpe vor dem Einschalten.

Lauf | Datum | Was er belegt | Ergebnis
:--- | :--- | :--- | :---
Etappe 5, Lauf A | 2026-08-21 | Die Sperre wirkt serverseitig | POST abgewiesen, TOP4/TOP101/TOP0 30 s später unverändert
Etappe 5, Lauf B | 2026-08-21 | Die Schrittfolge an H1 | **GRÜN nach 57 s**, jeder Schritt genau 8 s
Etappe 5, Rückweg | 2026-08-21 | Die Rückkehr durch den Re-Assert | 9 s nach dem Takt von 01:06:24 war die Anlage von allein zurück
Etappe 6 | 2026-08-21 | Der Knopf **ohne Broker** | **GRÜN nach 58 s**, Kompressor 26 → 33 Hz
Etappe 6, Rückweg | 2026-08-21 | Reconnect nach 8 min Ausfall | 52 s Backoff, Re-Assert 39 s später, Sollwerte nach 6:17 min
Warmwasser an H2 | 2026-08-21 | Die kurze Folge im Kühlbetrieb | **GRÜN nach 24 s**
**Heizstab an H2** | **2026-08-30** | **Die Rücknahme von SET39, Rolle Warmwasser** | **GRÜN nach 48 s**, TOP68 1 → 0 in Schritt 2
**Heizstab an H1** | **2026-08-30** | **Dieselbe Rücknahme, Rolle Heizen** | **GRÜN nach 80 s**, Stab 64 s vor dem Einschalten aus

Die beiden Läufe vom 2026-08-30 stehen mit allen Zeiten in
[Abschnitt 1b](#an-der-anlage-belegt--2026-08-30-beide-rollen).

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

---

# 6. Memo: Was die Steuerungsseite vom Notbetrieb mitbekommt

**Stand: 2026-08-29.** Dieser Abschnitt beschreibt keine Firmware. Er hält fest,
wie der Notbetrieb von der anderen Seite aussieht — seit dort ein Wächter auf
denselben Topics sitzt, die dieses Dokument beschreibt. Aufgeschrieben, weil
zwei der Topics, mit denen der Ablauf oben arbeitet, dort neuerdings ausgewertet
werden und im Notbetriebsfall zwangsläufig anschlagen.

## Wer mitliest

`script.js.common.kaskade.WP_Befehls_Waechter` (V1.2.0, ioBroker `javascript.0`,
60-s-Takt) vergleicht je Stufe sieben abgesetzte Kommandos mit ihrer
Rückmeldung. Rein beobachtend: kein Nachsenden, kein Eingriff, er schreibt nur
vier eigene Datenpunkte. **Zwei der sieben Kanäle gibt es erst, seit 3.10.0
TOP103 und TOP104 dekodiert** — vorher waren sie über Hilfsgrößen behelfsmäßig
angenähert.

Kanal | Soll | Ist | seit
:--- | :--- | :--- | :---
Heatpump | `set/Heatpump` | TOP0 | V1.0.0
OpMode | `set/OperationMode` | TOP4 | V1.0.0
HeatTarget | `set/Z1HeatRequestTemperature` | TOP27 | V1.0.0
CoolTarget | `set/Z1CoolRequestTemperature` | TOP28 | V1.0.0
QuietMode | `set/QuietMode` | TOP18 | V1.0.0
**WaterPump** | `set/WaterPump` | **TOP104** | **V1.2.0 (2026-08-29)**
**WaterPumpSpeed** | `set/WaterPumpSpeed` | **TOP103** | **V1.2.0 (2026-08-29)**
**ForceHeater** | `set/ForceHeater` | **TOP68** | **2026-08-30**

Der achte Kanal kam mit den Heizstab-Modi der Steuerung dazu und vergleicht
**Auftrag gegen Auftrag**: TOP68 meldet den von der Wärmepumpe übernommenen
Auftrag und bleibt stehen, solange `SET39` steht — auch wenn der Stab thermisch
abgeschaltet hat. Ob er tatsächlich heizt, sagt TOP60 (`HEIZSTAB-MODI.md` §6a);
in einer Befehlsquittierung wäre dort jedes normale Takten eine Abweichung.

Jeder Kanal hat 5 min Karenz, bevor eine Abweichung gemeldet wird. Auf die
Ist-Seite wirkt zusätzlich ein Stale-Guard von 20 min, und die LWT der Bridge
ist vorgeschaltet: Ist sie „Offline", gibt es eine Bridge-Meldung statt sieben
Kanal-Meldungen.

## Gemessen 2026-08-29: TOP103 und TOP104 antworten nach 7,1 s

Kaskadenmodus 2 (Umpumpen, Pumpe auf Fix) → 4 → 2, beide Flanken an **beiden**
Stufen mitgeschnitten:

Flanke | Kommando | Rückmeldung folgt nach
:--- | :--- | ---:
Fix → Auto | `WaterPump` 1→0, `WaterPumpSpeed` 100→125 | **7,1 s**
Auto → Fix | `WaterPump` 0→1, `WaterPumpSpeed` 125→100 | **7,1 s**

Die 7,1 s waren über alle acht Messungen identisch — das ist der Publish-Takt
der Bridge, nicht die Antwortzeit der Wärmepumpe. Das ergänzt den Abschnitt
[„Die Umwälzpumpe gehört mit zurück"](#die-umwälzpumpe-gehört-mit-zurück): Dort
steht die Flanke, hier steht ihre Zeit. Für Schritt 8 der Folge heißt das, dass
die Bestätigung lange vor dem nächsten Schrittabstand von 8 s vorliegt.

**Nebenbefund für die Messpraxis: `WaterPump = 0` heißt bedarfsgeregelt, nicht
aus.** Im selben Lauf modulierte TOP92 `Pump_Duty` über 40 s von 100 auf 80
herunter (TOP65 2350 → 1500 1/min, Fluss 16,2 → 10,2 l/min, Anlage in Betrieb),
während TOP103 `Pump_Duty_Max` unverändert auf der Vorgabe 125 stand. Bei `Fix`
lag der Ist-Duty dagegen exakt auf der Grenze. **Wer prüfen will, ob ein
Pumpenkommando angekommen ist, liest deshalb TOP103/TOP104 — nicht TOP92.** Der
Ist-Duty ist im Auto-Betrieb eine Regelgröße und sagt über das Kommando nichts.

## Warum der Notbetrieb Meldungen auslöst — abgeleitet, nicht gemessen

Die Firmware **abonniert** die `set/`-Topics, sie publiziert nicht auf ihnen: Die
Notbetriebsschritte gehen per UART an die Wärmepumpe, zurück kommen nur die
TOP-States. Der letzte Sollwert der Kaskadensteuerung bleibt derweil in
`mqtt.0…set/*` stehen — die Steuerung ist im Notbetriebsfall ja gerade weg.
Soll- und Ist-Seite laufen damit auseinander, und der Wächter meldet das nach
seiner Karenz:

Auslöser im Ablauf | betroffener Kanal | Abweichung
:--- | :--- | :---
**Schritt 2 `ForceHeater` = 0** | **ForceHeater** | **neu in 3.18.0 — nur wenn ein Heizstab-Modus lief; sonst sind Soll und Ist ohnehin beide 0**
Schritt 3 `OperationMode` = 0 | OpMode | solange die Steuerung zuletzt etwas anderes wollte
Schritt 4, Werks-Reset TOP27 → 35 | HeatTarget | bis der Re-Assert die Sollwerte zurückholt
Schritt 4, Werks-Reset TOP28 → 10 | CoolTarget | dito
**Schritt 9 `WaterPump` = 0** | **WaterPump** | **neu — bis V1.1.0 blieb dieser Fall unsichtbar**
Schritt 10 `Heatpump` = 1 | Heatpump | falls die Steuerung zuletzt 0 gesendet hatte

**Das ist kein Fehlalarm.** Die Meldung sagt zutreffend: „Der Sollwert der
Steuerung ist nicht mehr wirksam" — im Notbetrieb ist genau das der Zweck. Sie
quittiert sich beim Re-Assert von allein, im selben Zug, in dem
[Abschnitt 3](#3-rückkehr-der-übergeordneten-steuerung) die Anlage zurückholt.
Wer nach einem Notbetriebslauf ins ioBroker-Log sieht, findet dort also
`WP*.WaterPump NICHT ausgeführt` und einige Geschwister — **Wartungssignatur,
kein Befund.**

## Am 2026-08-30 geprüft — zwei Kanäle auf den Punkt, der Rest schweigt zu Recht

Die Tabelle war bis dahin aus dem Code beider Seiten abgeleitet. Das ioBroker-Log
der beiden Läufe hat sie eingelöst:

```
17:02:14  WP1.HeatTarget NICHT ausgeführt – soll 20, ist 35 (seit 5 min, Karenz 5 min)
17:02:14  WP1.CoolTarget NICHT ausgeführt – soll 20, ist 10 (seit 5 min, Karenz 5 min)
17:06:14  WP1.HeatTarget wieder quittiert (Abweichung bestand 9 min)
17:06:14  WP1.CoolTarget wieder quittiert (Abweichung bestand 9 min)
```

**Die Ist-Werte sind exakt die vorhergesagten: 35 und 10** — die
Panasonic-Werksvorgaben, die der Moduswechsel in Schritt 4 setzt. Die Tabelle
hatte sie ohne Messung genannt; hier stehen sie im Klartext. Und die Dauer
passt zu [Abschnitt 3](#3-rückkehr-der-übergeordneten-steuerung): Die Meldung
quittierte sich selbst, als der übernächste Re-Assert die Sollwerte zurückholte.

**Die übrigen Kanäle blieben stumm — und auch das ist die Vorhersage**, nur von
der anderen Seite: Ihre Abweichungen endeten, bevor die Karenz von 5 Minuten
ablief.

Kanal | Abweichung ab | endete durch | Dauer
:--- | :--- | :--- | ---:
ForceHeater | 16:56:15 (Schritt 2 an H1) | Moduswechsel auf 0 um 16:58:19 | **2 min**
Heatpump | 16:57:19 (Schritt 10) | Re-Assert um 17:00:29 | **3 min**
OpMode | — | keine: Soll und Ist waren beide `Heat` | —
WaterPump | — | keine: der Moduswechsel setzte den Soll auf denselben Wert | —

**Was daraus für den Ernstfall folgt:** Die kurzen Abweichungen blieben hier nur
deshalb unter der Karenz, weil die Steuerung lebte und den Modus zurückstellte.
Im echten Notbetriebsfall — die Steuerung ist weg — passiert das nicht; dann
schlagen nach fünf Minuten alle betroffenen Kanäle an. Die Tabelle oben gilt
also weiter, nur ist ihr Auslöser der Ausfall selbst und nicht der Knopf.

⚠️ Noch offen bleibt der **ForceHeater-Kanal in voller Länge**: Dass er nach
fünf Minuten tatsächlich meldet, ist wegen des Moduswechsels nach zwei Minuten
nicht gesehen worden.

## Und der Fall, in dem der Wächter schweigt

Fällt der **Broker** aus (nicht nur Node-RED), frieren alle `mqtt.0.*`-States
auf ihrem letzten Wert ein — Soll- **und** Ist-Seite gleichermaßen. Der Wächter
findet sie damit deckungsgleich und meldet „alles quittiert", obwohl er nichts
mehr sieht. Erst nach 20 min greift der Stale-Guard und stuft die Kanäle auf
„nicht bewertbar" zurück. Die LWT hilft in diesem Fenster nicht: Sie steht als
eingefrorener State ebenfalls noch auf „Online".

Das ist dieselbe Falle, die Etappe 6 schon für die Messung festgehalten hat —
**im Ausfallfall ist der ioBroker keine gültige Quelle**, auch nicht über den
Umweg eines Wächters, der auf ihm sitzt. Was gilt, gibt die Bridge über
`/tablerefresh` heraus.
