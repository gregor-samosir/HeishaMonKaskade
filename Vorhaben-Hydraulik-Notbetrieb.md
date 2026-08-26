# Vorhaben: Die Hydraulik beim Notbetrieb auf 1-stufig schalten

Der Notbetrieb setzt hydraulisch **1-stufigen** Betrieb voraus. Bisher stellt
das niemand sicher — im Normalbetrieb schaltet die Kaskadensteuerung einen
Tasmota-Switch, und genau die ist im Notbetriebsfall weg. Dieses Vorhaben legt
die Umschaltung in die Firmware, als **Schritt 1** der Notbetriebsfolge.

**Stand:** 2026-08-27, **umgesetzt als 3.15.0.** Ausgangsversion war 3.14.2.
Die Reihenfolgefrage aus Abschnitt 11 ist zugunsten dieses Vorhabens entschieden
(Owner, 2026-08-27): Es kommt zuerst, der ESP8266-Pfad wird dafür noch einmal
mitgepflegt. Was am Code steht, ist gebaut und im Hosttest belegt; der Prüfplan
aus Abschnitt 10 steht noch aus.

---

## 1. Warum das sein muss

Steht die Hydraulik auf 2-stufig, während eine Stufe im Warmwasser-Notbetrieb
läuft, **schiebt der Warmwasserbetrieb bis zu 57 °C in den Heizkreis.** Die
Fußbodenheizung verträgt das nicht gut (Owner, 2026-08-26).

Das ist kein Randfall, sondern der Regelfall des Notbetriebs an Stufe 2: Der
Warmwasserknopf ist gerade der, der im Sommer trägt, und er ist der, den jemand
aus der Familie im Ernstfall drücken soll. Die Firmware darf ihn nicht
anbieten, ohne die Hydraulik zu kennen.

Der Notbetrieb hat für die andere Richtung längst eine Sperre: Steht die Anlage
auf Kühlen, geht die Rolle Heizen gar nicht erst los (TOP101, siehe
[`notbetrieb.h:90-107`](src/notbetrieb.h#L90-L107)). Die Hydraulik ist
derselbe Gedanke an einer anderen Stelle — mit dem Unterschied, dass die
Firmware sie nicht nur prüfen, sondern auch **stellen** kann.

## 2. Der Schalter

Ein Sonoff TH mit Tasmota, `192.168.2.180`, **ohne Web-Passwort**.

Zustand | Hydraulik
:--- | :---
EIN | 2-stufig
**AUS** | **1-stufig — für den Notbetrieb erforderlich**

Er schaltet **zwei motorische Stellantriebe mit je 90 s Laufzeit.** Das Relais
ist damit der Startschuss der Umschaltung, nicht ihr Ergebnis — Abschnitt 5
zeigt, warum das trotzdem keine Wartezeit erzwingt.

Die beiden Kommandos, die gebraucht werden:

```
GET http://192.168.2.180/cm?cmnd=Power         ->  {"POWER":"ON"}   Zustand lesen
GET http://192.168.2.180/cm?cmnd=Power%20Off   ->  {"POWER":"OFF"}  ausschalten
```

Beide antworten mit demselben JSON-Feld. Es genügt, die Antwort auf `"OFF"` zu
prüfen; ein JSON-Parser wird dafür nicht gebraucht.

## 3. Die Entscheidungen

Alle vom Owner am 2026-08-26 getroffen.

Frage | Entscheidung | Folge
:--- | :--- | :---
Was, wenn der Switch nicht antwortet? | **Abbruch.** Kein Notbetrieb ohne bestätigte 1-stufige Hydraulik. | Eigener Abbruchgrund und eigene Meldung, Abschnitt 8
Erst lesen oder gleich schalten? | **Erst `Power` lesen, nur bei EIN schalten.** | Zwei Requests, Abschnitt 4
Beide Stufen oder nur Stufe 1? | **Beide.** Welchen Knopf jemand zuerst drückt, weiß niemand; ein doppeltes AUS schadet nicht. | Der Schritt steht in beiden Schrittfolgen
Wer schaltet zurück auf 2-stufig? | **Die Kaskadensteuerung per Re-Assert.** Sie liest den Switch zurück, stellt bei Abweichung nach und meldet sie. | **Kein** Rückschaltpfad in der Firmware — wie bei allen anderen Werten auch (Entscheidung 7, [`Ablauf-Notbetrieb.md`](Ablauf-Notbetrieb.md) Abschnitt 3)
Kann der Re-Assert die Hydraulik mitten im Notbetrieb zurückholen? | **Nur, wenn die Kaskadensteuerung den Betriebsmodus von Stufe 2 gerade frisch sieht.** Das prüft sie vor jedem Schaltbefehl. | Frische-Bedingung in Node-RED, nicht in der Firmware — Begründung gleich unten

### Warum „selber Flow, selber Takt" als Begründung nicht reicht

Die naheliegende Beruhigung lautet: Switch-Re-Assert und Wärmepumpen-Re-Assert
liegen im selben Node-RED-Flow und im selben 5-Minuten-Takt — kommt der eine,
kommt der andere und holt die Anlage ohnehin aus dem Notbetrieb. Selber Flow und
selber Takt stimmen. **Selbes Schicksal nicht.**

Die beiden Befehle verlassen den ioBroker auf verschiedenen Wegen:

* Die Wärmepumpenkommandos gehen über den `mqtt`-Adapter, Port 1883. Der ist
  zugleich der Broker, an dem die Bridges hängen.
* Das Switch-Kommando geht über den `sonoff`-Adapter, **Port 1886** — eigener
  Adapter, eigener Broker, eigener Prozess.

Genau der Ausfall, für den der Notbetriebsknopf gebaut ist, trifft den
`mqtt`-Adapter. Der `sonoff`-Adapter läuft weiter. Ein Re-Assert, der stur alle
fünf Minuten schaltet, täte in diesem Fall Folgendes: Node-RED läuft, der zuletzt
gesehene Betriebsmodus von Stufe 2 steht eingefroren auf dem Wert von vor dem
Ausfall — etwa 4 (Heat+DHW), also Soll 2-stufig. Jemand drückt den
Warmwasser-Notbetrieb, die Firmware legt den Switch auf AUS. Fünf Minuten später
legt Node-RED ihn wieder auf EIN. Das ist der Schaden aus Abschnitt 1, nur von
der anderen Seite verursacht.

Dass das heute nicht passieren kann, ist ein Zufall der Verdrahtung und kein
Entwurf: Der 5-Minuten-Takt der Hydraulikgruppe ist gar kein eigener Timer,
sondern der Vollversand der Bridge (`UPDATEALLTIME`, 300 s in
[`HeishaMon.h`](src/HeishaMon.h)). Er stirbt mit dem Broker — und mit ihm der
Schaltbefehl. Wer den Takt eigenständig macht, nimmt diese Sicherung weg.

**Deshalb liegt der Schutz in Node-RED, als Frische-Bedingung.** Ein
Schaltbefehl an den Switch geht nur raus, wenn der Betriebsmodus von Stufe 2
nicht älter als zwölf Minuten ist — zwei Vollversand-Zyklen plus Reserve für den
Reconnect-Backoff. Ist er älter, unterbleibt der Befehl und die Steuerung meldet
es. Das ist dieselbe Bauart wie die Puls-Bedingung, mit der der Verteiler die
Rückkehr von Stufe 1 absichert ([`Ablauf-Notbetrieb.md`](Ablauf-Notbetrieb.md)
Abschnitt 3), und sie steht dort aus demselben Grund: nicht an einer Anlage
drehen, die man gerade nicht sieht.

Umgesetzt im Repo `nodered-flows`, Tab *Kaskaden Logik*, Gruppe „Hydraulik-Status
Systemweit festlegen", 2026-08-26. Der Knoten `WegeVentil-Relais Steuerung V2.1`
liest den Switch über den ioBroker zurück (Telemetrie alle 15 s), schaltet nur
bei Abweichung und schreibt jede Abweichung ins Log.

Dazu gehört ein zweiter Knoten: `Hydraulik-Status V2.0` leitet die Stufigkeit
der Anlage nicht mehr allein aus den Rückmeldungen von Stufe 2 ab, sondern lässt
den zurückgelesenen Schalterzustand **widersprechen**. Das ist für den Notbetrieb
der Rolle Heizen nötig: Dort bleibt Stufe 2 unangetastet und meldet weiter
Heizbetrieb, während hydraulisch nur noch eine Wärmepumpe auf dem Kreis liegt.
Ohne diesen Widerspruch führte die Kaskade die Anlage in diesem Fenster als
2-stufig — mit abgezogener Spreizung und gedrosselter Pumpe.

Die verworfene Möglichkeit, der Vollständigkeit halber: **weiterlaufen mit
Warnung**, falls der Switch nicht antwortet. Sie ist abgelehnt, und der Grund
steht in Abschnitt 1 — ein Notbetrieb, der die Fußbodenheizung beschädigt, ist
kein Notbetrieb. Der Mensch steht ohnehin vor dem Browser und kann den Schalter
von Hand legen.

Ebenfalls verworfen: eine **Tasmota-Rule** `ON Mqtt#Disconnected DO Power0 0
ENDON`, die ganz ohne Firmwareänderung auskäme. Sie schaltet die Hydraulik bei
jedem ioBroker-Neustart um, auch wenn gar kein Notbetrieb läuft.

## 4. Der neue Schritt im Ablauf

Der Hydraulikschritt steht **ganz vorn** — vor `OperationMode`, vor allem
anderen. Zwei Gründe:

* **Es ist noch nichts verstellt.** Bricht er ab, steht die Wärmepumpe genau so
  da wie vorher: kein Kommando abgesetzt, kein Sammelfenster offen, kein
  halber Notbetrieb, den jemand aufräumen müsste.
* **Die 90 s der Stellantriebe laufen ab dem frühestmöglichen Moment** und
  damit parallel zur restlichen Schrittfolge.

### Rolle Heizen (Stufe 1) — acht Schritte statt sieben

Nr | t ab Klick | Was | Bestätigt durch
---: | ---: | :--- | :---
**1** | **0 s** | **Hydraulik auf AUS (1-stufig)** | **`{"POWER":"OFF"}` vom Switch**
2 | +8 s | `OperationMode` = 0 (Heat only) | TOP4
3 | +16 s | `HeatingMode` = 0 (Comp. Curve) | TOP76
4 | +24 s | `Z1HeatCurveTargetHighTemp` | TOP29
5 | +32 s | `Z1HeatCurveTargetLowTemp` | TOP30
6 | +40 s | `Z1HeatCurveOutsideLowTemp` | TOP32
7 | +48 s | `Z1HeatCurveOutsideHighTemp` | TOP31
8 | +56 s | `Heatpump` = 1 | TOP0
— | **+64 s** | **GRÜN** |

### Rolle Warmwasser (Stufe 2) — vier Schritte statt drei

Nr | t ab Klick | Was | Bestätigt durch
---: | ---: | :--- | :---
**1** | **0 s** | **Hydraulik auf AUS (1-stufig)** | **`{"POWER":"OFF"}` vom Switch**
2 | +8 s | `OperationMode` = 3 (DHW only) | TOP4
3 | +16 s | `DHWTemp` | TOP9
4 | +24 s | `Heatpump` = 1 | TOP0
— | **+32 s** | **GRÜN** |

### Was der Schritt intern tut

1. `cmnd=Power` lesen.
   * Antwort `"OFF"` → **schon 1-stufig**, Schritt sofort erledigt, kein
     Schaltvorgang, Logzeile „Hydraulik stand bereits auf 1-stufig".
   * Antwort `"ON"` → weiter mit 2.
   * keine Antwort → **Abbruch** (Abschnitt 8).
2. `cmnd=Power Off` senden.
   * Antwort `"OFF"` → Schritt erledigt.
   * alles andere oder keine Antwort → **Abbruch**.

Der Zustand wird **vorher gelesen und nicht blind geschaltet**, obwohl ein
einzelnes `Power Off` beide Fälle abdecken würde: Tasmota antwortet auch dann
mit `"OFF"`, wenn der Schalter schon aus war. Der Unterschied ist trotzdem
wichtig — nur so steht im Log, ob tatsächlich umgeschaltet wurde, und nur so
ließe sich später eine Wartezeit anhängen, falls die Stellantriebe doch einmal
knapp werden sollten.

Der Schritt hält die **Mindestwartezeit von 8 s** ein wie jeder andere, obwohl
er in Millisekunden fertig ist. Das ist kein Versehen: Der Automat kennt genau
einen Rhythmus, und ein Schritt, der aus der Reihe tanzt, wäre eine Sonderregel
mehr im Zeitgefüge. Die 8 s kosten hier nichts — sie fallen in die 90 s der
Stellantriebe, die ohnehin laufen.

## 5. Warum die 90 s keine Wartezeit erzwingen

Die naheliegende Sorge: Das Relais bestätigt sich sofort, die Stellantriebe
brauchen 90 s, und `Heatpump = 1` geht an Stufe 2 schon nach 24 s raus. Läuft
die Wärmepumpe dann 66 s lang gegen eine querstehende Hydraulik?

**Nein — der Kompressor braucht länger als die Ventile.** Nach dem Einschalten
vergehen rund **3 Minuten**, bis die Wärmepumpe den Kompressor hochfährt;
zunächst läuft nur die Umwälzpumpe an (Owner, 2026-08-26). Die Stellantriebe
sind nach 90 s durch, also gut anderthalb Minuten früher.

**Der Beleg ist der Normalbetrieb selbst.** Dort sendet die Kaskadensteuerung
ihre Kommandos an die Wärmepumpen **gleichzeitig** mit dem Switch-Kommando —
dieselbe Konstellation, seit jeher, ohne Schaden. Der Notbetrieb ist dabei
sogar der günstigere Fall: Bei ihm liegen zwischen Hydraulikschritt und
`Heatpump = 1` noch 16 s (Stufe 2) beziehungsweise 48 s (Stufe 1) zusätzlicher
Vorsprung.

Beim Abnahmelauf ist trotzdem ein Blick wert, wie sich der Vorlauf verhält,
wenn der Notbetrieb eine **bereits laufende** Anlage übernimmt — dann ist der
Kompressor schon oben und die 3 Minuten Vorlaufzeit gibt es nicht. Das ändert
nichts am Entwurf; es gehört in das Protokoll von Abschnitt 10.

## 6. Warum der Request blockierend sein darf

`HTTPClient.GET()` blockiert `loop()` bis zur Antwort oder zum Timeout. In
dieser Zeit läuft nichts: kein `read_pana_data`, kein `timeout_serial`, kein
Webserver ([`HeishaMon.cpp:928-1029`](src/HeishaMon.cpp#L928-L1029)). Der
UART-RX-Puffer fasst 256 Byte, ein Telegramm hat 203 — ein zweites ginge
verloren.

Genau deshalb steht der Schritt **vorn**: In diesem Moment ist noch kein
Kommando an die Wärmepumpe unterwegs und kein Sammelfenster offen. Eine
einmalige Blockade trifft nur den Abfragezyklus, der ohnehin nur liest, und
verschiebt ihn um die Blockadedauer.

Damit die im Fehlerfall nicht ausufert:

* **Timeout 1500 ms** statt der voreingestellten 5000 ms. Der Switch hängt im
  selben Subnetz; antwortet er in 1,5 s nicht, antwortet er nicht.
* **Höchstens ein Timeout je Lauf.** Läuft schon der Lesevorgang ins Leere,
  bricht der Lauf ab — der zweite Request kommt gar nicht erst.
* **Kein Wiederholungsversuch.** Der Mensch steht vor der Seite; er drückt
  erneut, und das ist der bessere Wiederholungsversuch.

Der Schritt wird aus `loop()` abgesetzt, **nicht** im POST-Handler. Der Handler
stößt heute nur an und antwortet sofort mit 303
([`notbetrieb.cpp:413-447`](src/notbetrieb.cpp#L413-L447)); läge der Request
darin, hinge der Browser bis zu 1,5 s an einer Seite, die noch nichts anzeigen
kann. Das bleibt so.

Eine asynchrone Lösung (`AsyncTCP`, eigener FreeRTOS-Task) ist **verworfen**:
Sie bringt Nebenläufigkeit in einen Zustandsautomaten, der heute vollständig
aus `loop()` getrieben wird, und kauft dafür 1,5 s in einem Fehlerfall zurück,
der ohnehin im Abbruch endet.

## 7. Was zu ändern ist

Datei | Änderung
:--- | :---
[`notbetrieb.h`](src/notbetrieb.h) | `NotbetriebSchritt` bekommt ein Typfeld (`NB_SCHRITT_SET` / `NB_SCHRITT_HYDRAULIK`). Der Hydraulikschritt kommt an den Anfang **beider** Schrittfolgen. Neuer Abbruchgrund im Ergebnis des Ticks.
[`notbetrieb.cpp`](src/notbetrieb.cpp) | Der eigentliche Request. `notbetrieb_schritt_absetzen()` verzweigt nach Typ; die Bestätigung des Hydraulikschritts kommt aus der Antwort statt aus `actual_data[]`.
[`webfunctions.cpp`](src/webfunctions.cpp) | Die neue ROT-Meldung (Abschnitt 8). Feld 9 im Statusstring, **hinten angehängt** — aus demselben Grund wie Feld 7 bzw. 8: Die Startseite liest dieselbe Route und ihre Feldpositionen müssen stehen bleiben.
[`notbetrieb_test.cpp`](test/notbetrieb_test.cpp) | Abschnitt 9.
[`version.h`](src/version.h) | Changelog-Eintrag mit Größenvergleich.
[`Ablauf-Notbetrieb.md`](Ablauf-Notbetrieb.md) | Beide Schrittfolgen und beide Gesamtzeiten fortschreiben.

Die Trennung aus [`notbetrieb.h:7-20`](src/notbetrieb.h#L7-L20) bleibt
erhalten: Der Automat kennt nur „Schritt vom Typ Hydraulik, bestätigt ja/nein"
und bleibt damit arduino-frei und im Hosttest prüfbar. Der Netzzugriff steht
ausschließlich in der `.cpp`.

Die IP gehört **nicht** fest in den Code, sondern zu den übrigen Einstellungen —
sie steht in derselben Größenordnung wie der MQTT-Broker und wird sich
irgendwann ändern.

## 8. Die Fehlerfälle

Lage | Zustand | Was der Mensch sieht
:--- | :--- | :---
Switch stand schon auf AUS | läuft weiter | nichts Besonderes; im Log „Hydraulik stand bereits auf 1-stufig"
Switch antwortet nicht (Timeout) | **ROT** | die Meldung unten
Switch antwortet, meldet aber weiter `"ON"` | **ROT** | die Meldung unten
Switch antwortet unverständlich | **ROT** | die Meldung unten

Die Meldung auf der Seite, wörtlich (Owner, 2026-08-26):

> **Die Umschaltung der Hydraulik ist fehlgeschlagen, bitte den Switch im
> Waschraum von Hand auf AUS schalten**

Sie tritt an die Stelle des generischen „Hat nicht geklappt", das der ROT-Zweig
heute zeigt ([`webfunctions.cpp:740`](src/webfunctions.cpp#L740)) — deshalb
braucht der Statusstring den Abbruchgrund.

**Der Knopf kommt nach ROT von selbst zurück**, ohne Neuladen und ohne die 15
Minuten der Verfallszeit abzuwarten: Die Seite blendet ihn nur bei „läuft" und
GRÜN aus ([`webfunctions.cpp:745`](src/webfunctions.cpp#L745)). Wer den
Schalter im Waschraum von Hand legt und zurückkommt, drückt erneut — der
Lesevorgang meldet dann `"OFF"`, und die Folge läuft durch. Genau dieser Weg
macht die harte Abbruchentscheidung erst vertretbar; er gehört in den Prüfplan
(Abschnitt 10, P4).

## 9. Hosttest

Der Request selbst ist im Hosttest nicht nachzubilden — sein **Ergebnis**
schon. Der Automat bekommt die Bestätigung als Wahrheitswert herein, genau wie
bei den TOP-Schritten:

* Der Hydraulikschritt steht in beiden Rollen an Position 1.
* Bestätigt → der Lauf geht zum nächsten Schritt, frühestens nach 8 s.
* Nicht bestätigt → der Lauf endet mit dem **neuen** Abbruchgrund, nicht mit
  dem allgemeinen Timeout-Grund.
* Die Schrittzahlen sind 8 (Heizen) und 4 (Wasser); der abgeleitete
  Gesamtdeckel wächst mit (`notbetrieb_gesamtdeckel_ms()`).
* Der `static_assert` zur Mindestwarte trägt unverändert.

## 10. Prüfplan an der Anlage

Nur im Ruhefenster des 5-Minuten-Re-Assert. Vorher Baseline über
`/tablerefresh` ziehen.

Nr | Was | Erwartung
:--- | :--- | :---
P1 | Switch von Hand auf EIN, Notbetrieb an Stufe 2 auslösen | Switch geht auf AUS, GRÜN nach 32 s
P2 | Switch steht schon auf AUS, Notbetrieb auslösen | kein Schaltvorgang, GRÜN nach 32 s, Logzeile „stand bereits"
P3 | Switch stromlos, Notbetrieb auslösen | **ROT innerhalb von ~2 s**, die Meldung aus Abschnitt 8, **kein** Kommando an der Wärmepumpe angekommen
P4 | Nach P3: Switch von Hand auf AUS, Knopf erneut drücken | Knopf ist da, Lauf geht durch
P5 | P1 an Stufe 1 wiederholen | GRÜN nach 64 s
P6 | Nach P1: Steuerung zurückkommen lassen | Re-Assert stellt 2-stufig wieder her
P7 | Während P3 den Abfragezyklus beobachten | einmaliger Versatz, keine Telegrammverluste

**Der ioBroker ist im Ausfallfall keine gültige Messquelle** — seine
Datenpunkte stehen dann eingefroren auf dem Wert von vorher und sehen aus wie
aktuelle Werte. Es zählt, was die Bridge über `/tablerefresh` herausgibt, und
für den Switch das, was `cmnd=Power` direkt antwortet.

## 11. Version und Reihenfolge — entschieden

Parallel läuft [`Vorhaben-Nur-ESP32-Pfad.md`](Vorhaben-Nur-ESP32-Pfad.md) mit
der Zielversion 3.15.0. Die beiden Vorhaben berühren sich nicht, aber die
Reihenfolge ist zu entscheiden:

Reihenfolge | Dafür
:--- | :---
**Erst 3.15.0, dann dieses als 3.16.0** *(Vorschlag)* | 3.15.0 ändert die Firmware um kein Byte und ist über den `.elf`-Vergleich schnell abgenommen. Danach hat dieses Vorhaben nur noch sechs Envs statt zehn und einen einzigen Zielprozessor.
Dieses zuerst als 3.15.0 | Es schließt eine Lücke, an der die Fußbodenheizung hängt. Der ESP8266-Pfad wäre dann noch einmal mitzupflegen.

Das Argument gegen den Vorschlag ist ernst zu nehmen: Solange dieses Vorhaben
nicht umgesetzt ist, **darf der Warmwasser-Notbetrieb an Stufe 2 nur gedrückt
werden, wenn vorher jemand den Switch im Waschraum von Hand auf AUS gelegt
hat.** Bis dahin gehört dieser Satz an den Anfang der Notbetriebsanleitung —
unabhängig davon, welche Reihenfolge gewählt wird.

**Entschieden am 2026-08-27 (Owner): dieses Vorhaben zuerst, als 3.15.0.** Der
ESP8266-Pfad wird dafür noch einmal mitgepflegt — der Hydraulikschritt ist in
beiden Pfaden gebaut und in beiden übersetzt (`ESP8266HTTPClient` bzw.
`HTTPClient`, eine `#if defined(ESP32)`-Weiche in `notbetrieb.cpp`). Der Preis
steht im Changelog: rund 6 kB Flash auf dem D1 mini, 20 kB auf dem ESP32, fast
vollständig die HTTP-Bibliothek.

[`Vorhaben-Nur-ESP32-Pfad.md`](Vorhaben-Nur-ESP32-Pfad.md) rückt damit auf
**3.16.0**; seine Zielversionsangabe ist entsprechend fortzuschreiben, sobald es
angefasst wird.

Der Satz oben — erst den Switch von Hand legen — gilt bis zum Rollout dieser
Version und kann danach aus der Notbetriebsanleitung entfallen.
