# Analyse: die zwei Relais der HeishaMon-Platine statt des KNX-Aktors

**Stand: 2026-08-22. Reine Entscheidungsgrundlage, nichts gebaut, nichts
geändert.** Die Frage lautet: Kann der KNX-Aktor, der heute den *External
comp. SW* und den *Heat/Cool SW* der beiden Wärmepumpen bedient, durch die
zwei Relais auf der HeishaMon-ESP32-Platine ersetzt werden — und entsteht
dabei ein neuer Fallstrick?

**Kurzantwort:** Die Idee trägt, aber nicht als *ein* Vorhaben. Die beiden
Relais sind zwei völlig verschiedene Fälle mit gegenläufigem Risiko, und der
Gewinn liegt woanders, als die Ausgangsfrage vermutet.

> **⚠ Abschnitt 10 ist der aktuelle Stand.** Die Handbuchangabe zur
> Kontaktlogik des Kompressorschalters ist an dieser Anlage **falsch herum**
> (Owner-Messung 2026-08-23). Überall, wo unten „offen = Kompressor frei"
> steht, gilt das Gegenteil. Die Verdrahtungsempfehlung ändert sich dadurch
> von COM+NO auf **COM+NC**.
>
> **Abschnitt 8 war der Stand davor.** Die Owner-Antworten vom 2026-08-22
> haben drei der hier genannten Fallstricke erledigt und einen neuen
> aufgedeckt, der die Empfehlung zu Heat/Cool umkehrt. Die Abschnitte 1–7
> bleiben unverändert stehen, weil sie zeigen, wie der Befund entstanden ist.

---

## 1. Was der Code hergibt

Die Relais existieren, sind aber im Original an drei Stellen kaum mehr als
zwei `digitalWrite`. Fundstellen im lokalen Original-Repo `~/HeishaMon/`:

Stelle | Inhalt
:--- | :---
`HeishaMon/gpio.h:10-11` | `#define relayOnePin 21`, `#define relayTwoPin 47`
`HeishaMon/gpio.h:22-23` | Pins 21 und 47 stehen in `gpioPin[]`/`gpioMode[]` auf `OUTPUT`
`HeishaMon/gpio.cpp:19-25` | `mqttGPIOCallback()` — `relay/one` und `relay/two`, angenommen werden `1`, `true`, `on`, `enable`
`HeishaMon/HeishaMon.ino:495` | abonniert wird `<prefix>/gpio/#`
`HeishaMon/src/rules/functions/gpio.cpp` | dieselben Pins zusätzlich aus der Regelsprache, `gpio(21,0)`

**Der gesamte Relais-Code steht in `#ifdef ESP32`.** Auf dem ESP8266 gibt es
ihn nicht — die kleine Platine hat die Relais schlicht nicht
(`binaries/README.md:5`: die große Version ist „the one with optional ethernet
support and the two blue relays").

### Was der Code NICHT hat

Und das ist der eigentliche Befund, denn genau diese fünf Punkte wären zu
bauen:

* **Keine Persistenz.** `pinMode(OUTPUT)` ohne `digitalWrite` — nach jedem
  Reset stehen beide Pins auf LOW, unabhängig davon, was vorher galt.
* **Keine Rückmeldung.** Der Zustand wird nirgends veröffentlicht. Wer über
  MQTT schaltet, erfährt nie, ob es angekommen ist.
* **Keine Bereichsprüfung, kein Log-Wert.** Alles, was nicht als „ein"
  erkannt wird, ist „aus" — auch ein Tippfehler.
* **Keine Karenzbehandlung.** Im Original läuft der `gpio`-Zweig durch
  denselben Callback wie alles andere.
* **Kein Bedienelement.** Weder Weboberfläche noch Statusseite kennen die
  Relais; es gibt nur MQTT und die Regelsprache.

**In dieser Firmware gibt es von alldem nichts.** `src/` enthält kein
`gpio.cpp`, keine Pin-Definition für 21/47, keinen `gpio`-Zweig im Callback.
Der Port ist klein (die Pins sind frei — `src/HeishaMon.h:31-34` belegt nur
17, 18, 5 und 4), aber es ist Neubau, keine Übernahme.

---

## 2. Was die Wärmepumpe erwartet

Aus dem Panasonic-Servicehandbuch, Abschnitt zum Anschluss externer Geräte
(nachgeschlagen, sinngemäß wiedergegeben — die Unterlage selbst liegt unter
`doku-intern/` und wird nicht veröffentlicht):

Eingang | Kontaktlogik | Ist-Zustand im Telegramm?
:--- | :--- | :---
External comp. SW | offen = Kompressor EIN, geschlossen = Kompressor AUS **← an dieser Anlage falsch herum, siehe Abschnitt 10** | **nein**
Heat/Cool SW | offen = Heizen, geschlossen = Kühlen | **ja — TOP101**

Beide sind trockene Kontakte, und beide brauchen die Freigabe im
Installateurmenü.

**Diese Zeile trägt die ganze Analyse.** Der Kompressorschalter hat kein
Statusbyte — in den Mitschnitten vom 15./16.08. wurde er betätigt, ohne dass
sich in einem der 203 Bytes etwas rührte. Der Heat/Cool-Schalter dagegen ist
in Byte 110 Bits 5&6 als TOP101 `Heat_Cool_SW_State` seit 3.7.0 im Einsatz und
in beide Richtungen an der laufenden Anlage belegt.

Damit zerfällt die Idee in zwei Fälle, die nichts miteinander zu tun haben.

---

## 3. Fall A — Relais 1 auf die Kompressorfreigabe

**Der Gewinn:** Der Notbetrieb verliert seine letzte Fremdabhängigkeit.
Abschnitt 5 Entscheidung 4 des Notbetriebsvorhabens sagt heute, dass die
Kompressorfreigabe über KNX kommen muss und dass dafür ein physischer
KNX-Taster vorgesehen ist — ein Gerät, das es noch nicht gibt (Abschnitt 9
führt es als OFFEN). Ein Relais erledigt das ohne neue Hardware im Haus.

**Der Ausfall zeigt in die richtige Richtung** — sofern der Kontakt richtig
gewählt wird. Relais stromlos → Kompressor **frei**. Reset, Absturz, OTA,
Stromausfall des Boards: alle enden in „darf laufen". Für einen Notbetrieb,
dessen einziger Zweck es ist, das Auskühlen zu verhindern, ist das die
gewünschte Fehlerrichtung.

> **Korrigiert am 2026-08-23:** Hier stand „Relais stromlos → Schließer offen
> → Kompressor frei". Das gilt an dieser Anlage nicht — offen sperrt. Die
> Fehlerrichtung stimmt weiterhin, aber nur über den **NC**-Kontakt
> (Abschnitt 10).

**Der Preis:** Dieselbe Eigenschaft macht die *Sperre* unzuverlässig. Ein
Reboot hebt sie auf, und die Firmware merkt es nicht, weil es kein Statusbyte
gibt. Wie schlimm das ist, hängt daran, wozu die Kaskade die Sperre benutzt.
Heute kommt sie aus `Modus Parameter Logik V4.2`, Ausgang 1 (`wpEin`) —
also aus dem Anlagenmodus, nicht aus einem Schutz. Und sie ist doppelt
abgesichert: In den Modi, in denen `wpEin` false ist, schaltet derselbe
Verteiler die Wärmepumpen ohnehin über `set/Heatpump` aus. Der Kontakt ist
dort die zweite von zwei Bremsen, nicht die einzige.

**Blind bleibt der Schritt trotzdem.** Der Notbetriebsknopf lebt davon, jeden
Schritt zurückzulesen (Abschnitt 2). Ein Relaisschritt hätte nichts, wogegen
er prüfen könnte — er wäre der einzige Schritt der Folge, der GRÜN meldet,
ohne etwas zu wissen. Der einzige verfügbare Gegenbeweis ist indirekt:
`Compressor_Freq` > 0, obwohl gesperrt. Die Wächter Logik liest den Wert
bereits.

---

## 4. Fall B — Relais 2 auf den Heat/Cool-Schalter

**Hier liegt der größere Gewinn, und er steht nicht in der Ausgangsfrage.**
Der Notbetriebsknopf der Rolle Heizen ist heute gesperrt, sobald TOP101 nicht
sauber 0 meldet — Abschnitt 2, „Gebaut am 2026-08-21: die Sperre". Im Sommer
heißt das: Der Knopf ist tot, und in der Notfallanleitung steht, jemand müsse
erst den Schalter im Haus auf Heizen stellen. Mit einem Relais könnte die
Firmware das selbst tun, als Schritt 0 der Folge, **mit Rücklesung über
TOP101** — der Nachlauf ist mit 7,7 s gemessen (2026-08-16), das passt in das
20-s-Schritt-Timeout. Aus einer Sperre würde ein Schritt.

Von den beiden Relais ist das also das einzige, das in die vorhandene
Schritt-und-Rücklese-Systematik passt, und das einzige, das einen heute
verschlossenen Pfad öffnet.

**Der Preis ist aber auch der höhere.** Offen = Heizen. Jeder Reset des
Boards wirft die Wärmepumpe für die Dauer des Bootvorgangs auf Heizen — im
Sommer, mitten im Kühlbetrieb. Was die Wärmepumpe daraus macht
(4-Wege-Ventil, Verdichterstopp, Anlaufsperre), ist nicht gemessen. Und
Resets sind hier kein seltener Sonderfall: Jedes OTA-Update ist einer.

Drei Wege daraus, in dieser Reihenfolge zu prüfen:

1. **Zustand persistent halten** (NVS oder LittleFS) und in `setup()` vor
   allem anderen wiederherstellen — die Lücke schrumpft auf die Zeit bis zum
   ersten `digitalWrite`, wenige hundert Millisekunden. Ob die Wärmepumpe so
   kurze Unterbrechungen überhaupt auswertet, muss gemessen werden. Das
   widerspricht dem Entwurfsprinzip des Notbetriebs („nichts auf Flash"),
   aber zu Recht: Eine Kontaktstellung ist kein Sollwert, sie ist ein
   Hardwarezustand.
2. **Heat/Cool beim KNX lassen** und nur Fall A bauen. Asymmetrie ist kein
   Makel.
3. **Die Alternative ohne jede Hardware:** `External_Heat_Cool_Control` im
   Installateurmenü abschalten. Dann regiert `set/OperationMode` (SET9)
   allein, der Umweg über einen Kontakt entfällt vollständig, und die Sperre
   im Notbetrieb wird gegenstandslos. TOP101 bliebe als Anzeige nutzbar — das
   Feld folgt dem Ist-Zustand unabhängig von der Befehlsquelle (belegt am
   2026-08-15, Heat→Cool über MQTT). Das kollidiert mit dem Owner-Entscheid
   vom 2026-08-15, an der Wärmepumpe nichts umzuschalten, ist aber die
   einzige Variante, die weder Lötkolben noch neuen Code braucht. **Wenn
   dieser Weg gangbar ist, erübrigt sich Fall B.**

---

## 5. Die Verdrahtungsfrage entscheidet, ob der Notbetrieb überhaupt gewinnt

Das ist der Punkt, an dem die Idee am leisesten scheitern könnte.

Variante | Kompressor frei, wenn … | Notbetrieb ohne ioBroker gelöst?
:--- | :--- | :---
Relais **parallel** zum KNX-Kontakt | beide offen sind | **nein**
Relais **in Reihe** | einer von beiden offen ist | ja
KNX raus, **nur Relais** | das Relais offen ist | ja

**Parallel ist die naheliegende und die falsche Wahl.** Fällt der ioBroker
aus, hält der KNX-Aktor seinen letzten Stand (`~/nodered-flows/FEUERUEBUNG.md`
Zeile 60 sagt das ausdrücklich). War der letzte Stand „gesperrt", bleibt er
gesperrt, und das Relais kann nichts daran ändern. Der Notbetrieb stünde
genau da, wo er heute steht.

**Reihenschaltung löst es**, kehrt aber die Logik um: Zum Sperren müssen dann
beide Kontakte schließen. Die Kaskadenlogik hat dafür bereits genau ein
Signal (`wpEin`), das nur auf zwei Ausgänge verteilt werden müsste — machbar,
aber es sind zwei Wege, die auseinanderlaufen können.

**Nur Relais ist am saubersten**, wenn parallel dazu ein **mechanischer
Handschalter** kommt. Der ersetzt dann den in Abschnitt 9 noch offenen
KNX-Taster durch einen Kippschalter für ein paar Euro, der ohne Bus, ohne
Strom und ohne Firmware funktioniert — und der ist im Ausfallszenario mehr
wert als beides zusammen.

---

## 6. Vier Fallstricke, die zum Gesamtprojekt gehören

### 6.1 Die Karenzzeit darf hier NICHT ausgenommen werden

Der Notbetriebszweig ist bewusst von `SUBSCRIBE_GRACE` befreit
(`src/HeishaMon.cpp:383-394`), weil seine Werte „nie ungefragt an die
Wärmepumpe gehen". **Für ein Relais gilt das Gegenteil** — es wirkt sofort
auf Hardware. Ein wiedereingespieltes `relay/one 0` vom letzten Monat würde
bei jedem Reconnect den Kompressor sperren. Der Relaiszweig muss also durch
die Karenz.

Damit entsteht aber die Folgelücke: Nach jedem Reconnect ist der Zustand für
5 s taub, und danach kommt nichts nach — die Freigabe wird heute
**ereignisgesteuert** geschrieben (`Modus Parameter Logik V4.2` → Junction →
`openknx.0.Kaskade.WP1_Compressor_Freigabe`), es gibt keinen zyklischen
Re-Assert wie bei den `set`-Topics. **Vor dem Umbau gehört ein 5-min-Re-Assert
für die Relaiskanäle in den Hauptmodus-Verteiler**, sonst steht das Relais
nach jedem Reconnect bis zum nächsten Moduswechsel falsch.

### 6.2 Freigabe und Kommunikation hängen danach am selben Gerät

Heute sind es zwei unabhängige Systeme: Stirbt die Bridge, schaltet der
KNX-Aktor weiter. Danach nicht mehr. Das ist der einzige Punkt, an dem der
Umbau echte Redundanz kostet, und er ist mit dem Handschalter aus Abschnitt 5
zu bezahlen.

### 6.3 Die Rückfallebene D1 mini kann es nicht

`platformio.ini` pflegt `d1_mini_h1_ota`/`d1_mini_h2_ota` als Rückfallebene,
und in `~/HeishaMon-Rollback/` liegen aktuelle 3.13.0-Binaries dafür. Ein
ESP8266 hat die Relais nicht. Wer auf die Rückfallebene wechselt, verliert
die Freigabe — es sei denn, KNX bleibt parallel liegen oder der Handschalter
existiert.

### 6.4 Vier Dinge sind ohne Board und Messgerät nicht zu klären

Für die Platine gibt es keinen Schaltplan; im Original-Repo steht nur der
EasyEDA-Verweis für die alten kleinen Boards (`PCB_Designs.md`).

Frage | Warum sie die Entscheidung ändert | Wie zu klären
:--- | :--- | :---
**Speisung.** CN-CNT liefert 5 V/250 mA und 12 V/250 mA (`README.md:289-292`). Ein ESP32-S3 mit aktivem WLAN liegt bereits im Bereich von 250 mA aus 5 V. | Zwei anziehende Relaisspulen könnten Brownouts und damit Resets auslösen — ausgerechnet an dem Gerät, das die Anlage freigibt. | Relais schalten und dabei die Versorgung messen; oder aus dem aufgedruckten Relaistyp den Spulenstrom und die Spulenspannung (5 V oder 12 V) ableiten.
**Ruhepegel beim Boot.** GPIO21 und GPIO47 sind beim ESP32-S3 keine Strapping-Pins, hängen nach dem Reset aber hochohmig in der Luft. | Ohne Pulldown auf der Platine kann das Relais während des Bootvorgangs flattern statt sauber abzufallen. | Beim Einschalten zuhören/messen.
**Kontakttyp.** Schließer allein oder Wechsler (NO+NC)? | Entscheidet, ob die Fehlerrichtung aus Abschnitt 3/4 überhaupt frei wählbar ist. | Board ansehen.
**Mindestschaltlast.** Es sind Leistungsrelais (bis 5 A/230 V, `README.md:367`), geschaltet wird ein Kleinsignal-Trockenkontakt mit wenigen Milliampere. | Unter der Mindestlast bleibt die Oxidschicht auf den Kontakten stehen — der typische Fehler ist ein Kontakt, der nach Monaten sporadisch nicht mehr durchschaltet. Bei einem Freigabekontakt ohne Statusbyte fällt das erst auf, wenn es kalt wird. | Datenblatt des aufgedruckten Relaistyps.

---

## 7. Empfehlung

**Als Gesamtidee: ja, aber gestaffelt und nicht als ein Vorhaben.**

1. **Zuerst die vier Hardwarefragen aus 6.4 klären.** Sie kosten eine halbe
   Stunde am Board und können die ganze Idee kippen (Brownout,
   Mindestschaltlast). Alles Weitere davor ist verfrüht.
2. **Dann Fall A bauen** — Kompressorfreigabe, in Reihe oder allein, mit
   Handschalter parallel. Klarer Gewinn, Fehlerrichtung stimmt, löst einen
   Punkt, der seit dem 21.08. als offen im Vorhaben steht.
3. **Fall B erst danach und erst nach der Menü-Frage.** Wenn
   `External_Heat_Cool_Control` abschaltbar ist, ist der elegantere Weg
   kostenlos. Wenn nicht, braucht das Relais Persistenz und eine Messung, was
   ein Bootvorgang im Kühlbetrieb anrichtet.
4. **Node-RED zuerst, Firmware danach.** Der 5-min-Re-Assert für die
   Relaiskanäle (6.1) muss stehen, bevor das erste Relais scharf ist.

**Was den Umbau unabhängig davon lohnt:** Die beiden Relais machen die
Kaskade an einer Stelle unabhängig vom KNX-Bus, an der sie es heute nicht ist
— und zwar genau an der Stelle, die im Notbetrieb der letzte offene Punkt
ist. Der Fallstrick liegt nicht in der Idee, sondern in drei Details:
Verdrahtungslogik (Abschnitt 5), Karenzzeit ohne Re-Assert (6.1) und
Bootverhalten bei Heat/Cool (Abschnitt 4).

---

## 8. Nachtrag 2026-08-22 — die Owner-Antworten und was sie ändern

### 8.1 Drei Fallstricke sind erledigt

**Speisung (6.4).** Das aktuelle große Board wird laut Entwickler über die
**12 V** aus dem Kabel versorgt, nicht über die 5 V. Die Produktseite
bestätigt „the heishamon boards are powered through the cable from the
heatpump" und nennt die 12-V-Ader ausdrücklich; Relaisdaten stehen dort nicht.
Damit stehen 12 V/250 mA = 3 W zur Verfügung statt 1,25 W, und die
Brownout-Sorge aus 6.4 ist vom Tisch: Eine 12-V-Spule zieht typisch 30–40 mA,
zwei davon liegen weit innerhalb der Reserve. **Der Punkt ist damit kein
Entscheidungshindernis mehr** — die Stromaufnahme beim Anziehen im Blick zu
behalten bleibt eine Fleißaufgabe fürs Inbetriebnehmen, keine Vorbedingung.

**Reihen- oder Parallelschaltung (Abschnitt 5).** Entfällt vollständig. Der
KNX-Aktor wird für diese Kanäle abgeklemmt, und die übergeordnete Node-RED-
Steuerung schaltet im Normalbetrieb dieselben Relais. Es gibt dann nur noch
**eine** Quelle, keine zwei Kontakte, die sich widersprechen können. Damit
erledigt sich auch der mechanische Handschalter: Über den NC-Kontakt ist der
Ruhezustand geschlossen, und geschlossen heißt an dieser Anlage „Kompressor
frei" (Abschnitt 10) — fällt die Bridge komplett aus, ist die Freigabe da, und
die Wärmepumpe lässt sich am Bedienfeld bedienen. Der in Abschnitt 9 des Notbetriebsvorhabens noch offene
**physische KNX-Taster wird damit ersatzlos gegenstandslos.**

**Rückfallebene D1 mini (6.3).** Entfällt. Zwei weitere große Platinen sind
bestellt; die Rückfallebene besteht künftig aus derselben Hardware und kann
die Relais.

### 8.2 Zum Einwand „wir kennen den Schaltzustand auch ohne Rückmeldung"

Das stimmt — mit einer Einschränkung, und sie ist kleiner als sie klingt.

**Heute ist es nicht besser.** `openknx.0.Kaskade.WP1_Compressor_Freigabe` ist
im Datenmodell schreibend, nicht lesend (`~/nodered-flows/DATAMODEL.md:217`).
Auch heute kennt die Steuerung nur ihren eigenen Befehl, nicht die
Kontaktstellung. Der Umbau verschlechtert also nichts, er verlagert nur.

**Die eine echte Lücke ist der Neustart.** Nach einem Reset des Boards steht
der Pin auf LOW, und die Firmware weiß nicht, was vorher galt. Solange der
Broker da ist, holt der Re-Assert das binnen Minuten ein. Ohne Broker — also
genau im Notbetriebsfall — kommt nichts nach. **Gegenmittel: den Zustand
persistent halten** (NVS) und in `setup()` vor dem WLAN wiederherstellen. Das
ist für beide Relais billig und sollte von vornherein mit hinein; ohne
Persistenz fällt bei jedem OTA-Update ein Relais für die Dauer des Bootens
und danach bis zum nächsten Re-Assert ab — bis zu fünf Minuten.

**Was blind bleibt, ist gutmütig blind.** Ein oxidierter Kontakt (6.4) macht
sich als „Sperre wirkt nicht" bemerkbar, nicht als „Freigabe fehlt", weil der
Kontakt zum Sperren *schließt*. Beide Fehlerarten zeigen also in dieselbe
Richtung wie der Stromausfall: Wärme. Für einen Freigabekontakt ist das die
richtige Richtung, und `Compressor_Freq` bleibt der indirekte Gegenbeweis.

### 8.3 Der neue Befund: die Menü-Abschaltung ist teurer als sie aussieht

**Das kehrt die Empfehlung aus Abschnitt 4 um.** In Abschnitt 4 stand,
`External_Heat_Cool_Control` abzuschalten sei die elegante Variante ohne
Hardware. Der Blick in die Kaskadenlogik zeigt, dass sie stattdessen an der
heikelsten Stelle eingreift.

Heute läuft Heizen/Kühlen als **offene Kette** mit getrenntem Hin- und
Rückweg:

```
Befehl:       KK_Heat-Cool_Switch  ->  KNX-Aktor  ->  Kontakt an der WP
Rückmeldung:  WP  ->  TOP101  ->  Heat/Cool Number->Boolean V1.0
                                   |-> isCoolingActive (file-Context)
                                   |     -> Verriegelung §3, Modus Parameter Logik V4.2
                                   |-> HKM_HeatCoolMode_Input (Mischerregelung)
                                   |-> legacy.KK_Heat-Cool_Status
```

TOP101 ist damit die **einzige** Quelle der Verriegelung — und heute eine von
der Steuerung unabhängige: Eine andere Instanz legt den Zweig um, die Logik
folgt dem gemessenen Ist-Zustand.

Wird der Kontakteingang abgeschaltet, muss der Befehl über
`set/OperationMode`. Den schreibt aber bereits der Hauptmodus-Verteiler V6.5,
und zwar aus einem Befehlssatz, den die Verriegelung auswählt — die an TOP101
hängt, das nun seinerseits von `set/OperationMode` kommt. **Aus der offenen
Kette wird ein Kreis**, und der klemmt in der Umschaltung: Steht die Anlage
auf Kühlen, verwirft §4 den Heizmodus-Befehl des inaktiven Zweigs, der
Verteiler sendet im Re-Assert weiter `opmode: COOL`, TOP101 bleibt Cool. Es
bräuchte einen zusätzlichen Umschaltpfad, der die Verriegelung bewusst umgeht
— also genau die Rolle, die heute der externe Kontakt spielt, nur diesmal
innerhalb der Logik.

Machbar ist das. Aber es ist **kein Rückbau, sondern ein Umbau der
Verriegelung**, und die trägt die Trennung von Heiz- und Kühlzweig. Ihr
file-Context existiert eigens, um eine Start-Race nach Deploys zu vermeiden
(V3.0.4). Dazu kommt: TOP101 speist auch die Mischerregelung
(`HKM_HeatCoolMode_Input`), die dann ebenfalls am eigenen Befehl statt an
einer Messung hängt.

**Das Relais lässt diese Struktur unangetastet.** Es tauscht nur das
Stellglied — KNX-Aktor raus, Relais rein — und lässt Hin- und Rückweg
getrennt. TOP101 bleibt die unabhängige Bestätigung, dass der Kontakt
tatsächlich gewirkt hat, und wird sogar besser: Der Weg wird kürzer und
verliert das KNX-Gateway.

### 8.4 Korrigierte Empfehlung

**Beide Relais bauen, Menü-Abschaltung nicht.**

Relais | Zweck | Fehlerrichtung bei Reset | Rückmeldung
:--- | :--- | :--- | :---
1 | External comp. SW | **geschlossen** = Kompressor frei → **COM+NC** (korrigiert, Abschnitt 10) | keine, indirekt über `Compressor_Freq`
2 | Heat/Cool SW | offen = Heizen → COM+NO | **TOP101, vollwertig**

Reihenfolge:

1. **Persistenz von Anfang an mitbauen** (NVS, in `setup()` vor dem WLAN
   herstellen, nur bei Änderung schreiben). Ohne sie ist jedes OTA-Update im
   Kühlbetrieb ein kurzer Heizbefehl an die Anlage.
2. **Node-RED zuerst:** Re-Assert für beide Relaiskanäle im 5-min-Takt (6.1).
   Er muss stehen, bevor das erste Relais scharf ist, weil die Karenzzeit die
   Wiedereinspielung nach jedem Reconnect verwirft und heute nichts nachkommt
   — die Freigabe wird rein ereignisgesteuert geschrieben.
3. **Die Relais-Topics durch die Karenzzeit laufen lassen**, nicht wie den
   Notbetriebszweig davon ausnehmen (6.1). Sie wirken sofort auf Hardware.
4. **Relais 1 in Betrieb nehmen**, KNX-Kanal abklemmen. Damit ist der
   Notbetrieb zum ersten Mal ohne fremdes Gewerk lauffähig.
5. **Relais 2 danach**, mit einer Messung vorweg: Bridge im Kühlbetrieb einmal
   neu starten und zusehen, ob die Wärmepumpe die Bootlücke überhaupt
   bemerkt (TOP101 und `Compressor_Freq` mitschneiden). Fällt die Messung
   ungünstig aus, bleibt Heat/Cool beim KNX — Relais 1 steht davon unberührt.
6. **Erst danach den Notbetrieb erweitern:** Relais 2 als Schritt 0 mit
   TOP101-Rücklesung (7,7 s Nachlauf, gemessen 2026-08-16, passt ins
   20-s-Timeout), Relais 1 als blinder Schritt davor. Damit fällt die Sperre
   „Der Notbetrieb ist nur im Modus Heizen möglich" ersatzlos weg, und die
   Notfallanleitung verliert zwei Absätze.

**Vor jedem Eingriff am Board:** Relaistyp ablesen (Mindestschaltlast,
Kontakttyp Schließer oder Wechsler) und den Pegel von GPIO21/47 während des
Bootens prüfen. Beides kippt die Idee nicht mehr, beides gehört aber
protokolliert, bevor der erste Kontakt an der Wärmepumpe hängt.

**Offen und außerhalb dieser Analyse:** ob die Notbetriebsseite zusätzlich
zwei Handschalter für die Relais bekommt. Entscheidung 7 des
Notbetriebsvorhabens sagt „die Seite hat genau einen Knopf", und das ist eine
Owner-Entscheidung, keine technische.

---

## 9. Nachtrag 2026-08-22 — Wechselkontakte und der Bootpegel

### 9.1 Wechselkontakte: die Fehlerrichtung ist wählbar

Auf dem Produktfoto sind **NO, COM, NC** herausgeführt. Damit ist der Punkt
„Kontakttyp" aus 6.4 erledigt, und es entsteht ein Freiheitsgrad: Welcher
Zustand bei stromlosem Relais gilt, entscheidet die Klemmenwahl.

> **Die Tabelle unten war nach der Handbuchangabe gerechnet und ist für den
> Kompressor falsch.** Die richtige Fassung steht in Abschnitt 10.2; das
> Prinzip — die Klemmenwahl entscheidet über die Fehlerrichtung — gilt
> unverändert.

Verdrahtung | Relais stromlos | Kompressor (Relais 1) | Heat/Cool (Relais 2)
:--- | :--- | :--- | :---
**COM + NO** | Kontakt offen | ~~frei~~ **gesperrt** | Heizen
COM + NC | Kontakt geschlossen | ~~gesperrt~~ **frei** | Kühlen

**Das Auswahlkriterium bleibt:** Die Spule soll in dem Zustand stromlos sein,
der im Winter gilt. Der Dauerzustand der kalten Jahreszeit — also genau der,
in dem ein Ausfall weh tut — braucht dann kein einziges Bauteil, das
funktionieren muss. Ein Defekt an Spule, Treiber oder Versorgung fällt in
Richtung „darf heizen", und zwar unabhängig von Firmware und Persistenz.
Welche Klemme das leistet, sagt Abschnitt 10.2.

**Eine echte Kontaktrückmeldung geben die Wechsler nicht her.** Es gibt nur
ein COM je Relais, und das liegt an der Wärmepumpe; NC bleibt zwar frei, ist
aber mit demselben COM verbunden und taugt nicht als getrennter Meldekreis.
Wer die Kontaktstellung wirklich messen wollte, bräuchte ein Hilfsrelais mit
zweitem Kontaktsatz an einem der freien Eingänge (das große Board führt
GPIO33–37 als Eingänge, siehe `gpio.h:22-23`). Lohnt nicht — die indirekte
Prüfung über `Compressor_Freq` aus 8.2 kostet nichts.

### 9.2 Der Bootpegel — was Espressif dazu sagt

Belegt aus dem **ESP32-S3 Series Datasheet v2.2**, Tabelle 2-1 „Pin
Overview". Wichtig zum Nachschlagen: **GPIO47 heißt in der Tabelle
`SPICLK_P`** (Pin 37) — unter „GPIO47" ist die Zeile nicht zu finden. GPIO21
steht als Pin 27.

| | GPIO21 (Pin 27) | GPIO47 = SPICLK_P (Pin 37) |
:--- | :--- | :---
At Reset | *(leer)* | `IE` |
After Reset | *(leer)* | `IE` |
Versorgt aus | VDD3P3_RTC | VDD_SPI / VDD3P3_CPU |
Strapping-Pin | nein | nein |
Power-Up-Glitch (Tab. 2-2) | nicht gelistet | nicht gelistet |
Treiberstärke | 20 mA | 20 mA |

Die Legende (Fußnote 6) definiert: `IE – input enabled`, `WPU – internal weak
pull-up resistor enabled`, `WPD – internal weak pull-down resistor enabled`.

**Damit ist die Frage in vier Schichten zu beantworten:**

**Schicht 1 — der Chip: kein Pull, in beiden Fällen.** Bei GPIO21 steht in
beiden Spalten gar nichts, bei GPIO47 nur `IE` (Eingangspuffer aktiv). Weder
WPU noch WPD. Der interne Pull-Widerstand hätte ohnehin 45 kΩ (Tabelle
„DC Characteristics"), aber er ist hier schlicht nicht aktiv. **Beide Pins
hängen vom Reset bis zum ersten `pinMode()` hochohmig in der Luft.**

**Schicht 2 — Bootloader: unauffällig, aber ohne Zusage.** Keiner der beiden
ist Strapping-Pin (das sind GPIO0, GPIO3, GPIO45, GPIO46), keiner gehört zum
Quad-SPI-Flash, und keiner steht in Tabelle 2-2 „Power-Up Glitches on Pins" —
die listet nur GPIO1–GPIO20. Espressif verweigert trotzdem ausdrücklich die
Garantie: „Do not rely on the default configurations values in the Technical
Reference Manual, because it may be changed in the bootloader or application
startup code before app_main."

**Schicht 3 — die Platine, und hier liegt die Antwort.** Weil der Chip nicht
treibt und nicht zieht, bestimmt allein ein externer Pulldown am Eingang des
Relaistreibers, ob das Relais während des Bootens sauber abgefallen bleibt
oder undefiniert flattert. Dazu sagt keine Espressif-Doku etwas, und für die
HeishaMon-Platine gibt es keinen veröffentlichten Schaltplan.

> **Beantwortet am 2026-08-23 durch den Platinenentwickler:** „when heishamon
> is without power (or booting), this side will do what it says (normally
> means, not engaged, no power)." Das Relais bleibt beim Booten also
> abgefallen — die Erwartung ist damit belegt statt vermutet. Der
> Durchgangstest unten bleibt trotzdem die Gegenprobe von fünf Minuten; er
> kostet nichts und deckt zugleich ab, was das Datasheet offenlässt.

**Das ist aber in fünf Minuten messbar, ohne Wärmepumpe:** Board über USB
versorgen, Durchgangsprüfer an COM und NO, Reset drücken. Bleibt der
Durchgang aus (bzw. das Relais still), ist ein Pulldown vorhanden und die
Sache erledigt. Zieht es kurz an, muss ein externer Pulldown von 10 kΩ gegen
Masse an den Pin — dann ist auch dieser Punkt geschlossen.

**Schicht 4 — die Firmware, und hier steckt der eigentliche Fallstrick.**
Nicht im Chip, sondern in der Reihenfolge von `setup()`. Die Pins, die diese
Firmware heute setzt, werden in `switchSerial()` konfiguriert — und
`switchSerial()` steht als **letzter** Aufruf in `setup()`
(`src/HeishaMon.cpp:917`), nach `setupWifi()`. Ein Relais-Init an derselben
Stelle wäre nicht Hunderte Millisekunden zu spät, sondern die **gesamte
WLAN-Anlaufzeit** — und wenn der WiFiManager sein Konfigurationsportal
öffnet, unbegrenzt.

**Bauvorschrift daraus:** Das Wiederherstellen des Relaiszustands gehört als
**allererste Anweisung in `setup()`**, noch vor `setupSerial()`. Dann bleibt
als Lücke nur Bootloader plus Arduino-Init — Größenordnung einige hundert
Millisekunden, und die ist mit demselben Durchgangsprüfer nachzumessen.

### 9.3 Ein Nebenfund zu GPIO47, der nur eine Chipvariante trifft

Fußnote 4 derselben Tabelle: „For ESP32-S3R8V and ESP32-S3R16V chip, as the
VDD_SPI voltage has been set to 1.8 V, the working voltage for pins SPICLK_N
and SPICLK_P (GPIO47 and GPIO48) would also be 1.8 V, which is different from
other GPIOs."

Auf diesen beiden Varianten liefert GPIO47 also **1,8 V statt 3,3 V** — je
nach Relaistreiber zu wenig zum sauberen Durchschalten. Die hier verbauten
Boards haben 4 MB Flash und kein PSRAM (`platformio.ini`,
`board_upload.flash_size = 4MB` mit `min_spiffs.csv`), gehören also
aller Wahrscheinlichkeit nach nicht dazu. Zu belegen ist es beiläufig: Die
Chipbezeichnung steht in der Bootausgabe auf der USB-Konsole und im
`esptool`-Kopf beim USB-Flashen der beiden neu bestellten Boards. Kostet
nichts, wenn man beim Erstflash ohnehin hinschaut — und erspart die Suche
nach einem Relais, das grundlos nicht anzieht.

---

## 10. Nachtrag 2026-08-23 — die Handbuchangabe stimmt nicht

**Das ist der wichtigste Befund dieser Analyse, und er kam beim Einrichten des
KNX-Tasters heraus.** Er ändert die Verdrahtung von Relais 1.

### 10.1 Der Befund

Das Panasonic-Handbuch gibt für den External comp. SW an: offen = Kompressor
EIN, geschlossen = Kompressor AUS. **An dieser Anlage ist es genau umgekehrt.**

Owner-Messung vom 2026-08-23, dreifach abgesichert:

* Der KNX-Aktorkanal ist ein **Schließer**.
* Gesendet wird **True, wenn der Kompressor freigegeben ist** — der Kontakt
  ist bei Freigabe also **geschlossen**.
* Am Aktor selbst gegengeprüft, und die Anlage läuft seit fünf Jahren so.

Damit gilt hier:

Kontakt | Kompressor
:--- | :---
**geschlossen** | **frei**
offen | gesperrt

Woher die Abweichung kommt — Fehler im Handbuch, Modell- oder
Revisionsunterschied, oder eine Menüoption — ist offen und für die
Entscheidung ohne Belang. Fünf Jahre Betrieb und die Gegenprobe am Aktor
schlagen jede Handbuchzeile.

**Für den Heat/Cool SW gilt die Umkehrung nicht ohne Weiteres.** Es wäre
naheliegend und falsch, vom einen auf den anderen Eingang zu schließen. Ein
Hinweis aus den Livedaten spricht sogar dafür, dass das Handbuch dort recht
hat — am 2026-08-23 um 14:40 UTC gelesen:

Datenpunkt | Wert
:--- | :---
`openknx.0.Kaskade.WP1_Heat-Cool` | `false`
`openknx.0.Kaskade.WP2_Heat-Cool` | `false`
`Heat_Cool_SW_State` (TOP101), beide Stufen | `0` = Heizen

Ist auch dieser Aktorkanal ein Schließer mit derselben Konvention, war der
Kontakt offen und die Wärmepumpe meldete Heizen — also **offen = Heizen**, wie
im Handbuch. **Beweis ist das keiner**, weil die Kanalart nicht geprüft ist.
Sie ist am Aktor genauso abzulesen wie beim Kompressorkanal; bis dahin bleibt
die Zeile eine Annahme, und die Verdrahtung von Relais 2 wartet darauf.

### 10.2 Was sich dadurch ändert

**Nur die Klemme, nicht die Begründung.** Das Kriterium aus 9.1 bleibt: Die
Spule soll in dem Zustand stromlos sein, der im Winter gilt, damit der Ausfall
jedes beteiligten Bauteils in Richtung „darf heizen" fällt.

Relais | Eingang | Gewünschter Ruhezustand | **Klemme** | Spule bestromt, wenn
:--- | :--- | :--- | :--- | :---
1 | External comp. SW | Kompressor frei | **COM + NC** | gesperrt
2 | Heat/Cool SW | Heizen | **COM + NO** (Annahme, siehe 10.1) | Kühlen

Die beiden Relais werden also **unterschiedlich** angeklemmt. Das ist keine
Unsauberkeit, sondern die Folge davon, dass die Wärmepumpe die beiden Eingänge
gegenläufig auslegt — und genau deshalb gehört es beschriftet, an der Klemme
wie in der Notfallanleitung.

**Zwei Folgen, die man leicht übersieht:**

* **Der Ausfall der ganzen Bridge bleibt gutmütig** — aber nur über NC. Mit
  COM+NO an Relais 1 wäre jeder Reset, jedes OTA und jeder Stromausfall des
  Boards eine Kompressorsperre. Bei einem Gerät, dessen Zweck es ist, das
  Auskühlen zu verhindern, wäre das die Umkehrung des Vorhabens.
* **Die Kontaktqualität wird wichtiger** (6.4). Mit der richtigen Klemme ist
  der Kontakt im Freigabezustand geschlossen; ein Kontakt, der nicht mehr
  sauber schließt, heißt jetzt „keine Freigabe" statt „Sperre wirkt nicht" —
  die Fehlerrichtung des *Kontakts* dreht sich also mit. Entwarnung kommt aus
  den Verlaufsdaten weiter unten: Der Kontakt wird täglich mehrfach bewegt und
  steht nicht monatelang still.

### 10.3 Was die Verlaufsdaten über diesen Kanal sagen

Nachgesehen am 2026-08-23 über die simple-api, `openknx.0.Kaskade.WP1_Compressor_Freigabe`,
Zeitraum 2026-08-09 bis 2026-08-23 (21114 Punkte, 54 Flanken):

Größe | Wert | Bedeutung für den Umbau
:--- | :--- | :---
Flanken | 3,7 pro Tag, rund **1350 im Jahr** | Für ein Kleinrelais unkritisch. Der Kontakt bleibt in Bewegung — das entschärft die Oxidationssorge aus 6.4 spürbar.
Anteil Freigabe = true | **59 %** (207 h von 351 h) | Die Spule wäre über NC also im August zu 41 % bestromt, im Winter deutlich seltener. Innerhalb der 250 mA, siehe 9.1.
**Längste Pause ohne Flanke** | **48 h** (08-09 06:13 bis 08-11 06:11), zweitlängste 33 h | **Der harte Beleg für 6.1.**

Die letzte Zeile ist der eigentliche Ertrag dieser Abfrage. Die Freigabe wird
rein ereignisgesteuert geschrieben, und zwischen zwei Ereignissen können
**zwei Tage** liegen. Fällt ein Reconnect in so eine Pause, verwirft die
Karenzzeit die Wiedereinspielung, und ohne zyklischen Re-Assert stünde das
Relais bis zu zwei Tage falsch — im ungünstigen Fall zwei Tage lang gesperrt.
Der 5-min-Re-Assert aus 8.4 Punkt 2 ist damit keine Vorsichtsmaßnahme mehr,
sondern Voraussetzung.

### 10.4 Prüfschritt, der vor die Verdrahtung gehört

Beim Kompressoreingang hat sich die Handbuchangabe als falsch erwiesen. Daraus
folgt für die Inbetriebnahme eine Regel, die vorher nicht nötig schien:

**Jeden der beiden Kontakte einmal einzeln nachweisen, bevor die Steuerung
darauf gebaut wird** — den Heat/Cool-Kontakt gegen TOP101 (das kostet nichts,
das Feld meldet den Ist-Zustand binnen Sekunden), den Kompressorkontakt gegen
`Compressor_Freq` bei laufender Anforderung. Nicht gegen das Handbuch.

---

## 11. Nachtrag 2026-08-23 — der Retain-Vorschlag des Platinenentwicklers

Der Entwickler der großen Platine hat auf Nachfrage zwei Dinge geantwortet.
Das erste bestätigt die Analyse, das zweite passt an dieser Anlage nicht — und
zwar aus einem Grund, der nichts mit seiner Firmware zu tun hat.

### 11.1 Bestätigt: das Bootverhalten der Kontakte

> „when heishamon is without power (or booting), this side will do what it
> says (normally means, not engaged, no power). So for example if you use the
> 'normally open' the port isn't connect to the 'common' port when booting."

Deckt sich mit Abschnitt 9.2 und schließt dort die offene Schicht 3: Das
Relais bleibt beim Booten abgefallen. Zusammen mit dem Befund aus Abschnitt 10
heißt das für Relais 1 unverändert **COM + NC** — nur so ist der Kompressor
beim Booten frei.

### 11.2 Nicht übertragbar: den letzten Zustand über Retain wiederherstellen

> „if you want heishamon to switch the relais to the last known state, you
> just send a 'retained' mqtt message for the gpio message instead of a normal
> message. This means that heishamon after booting will read this message
> again and put the relais in that last retained state."

**Für die offizielle Firmware an einem gewöhnlichen Broker ist das genau
richtig** — es ist der Grund, warum die Original-Firmware ohne jede Persistenz
auskommt (Abschnitt 1). Hier trägt es nicht, aus drei voneinander unabhängigen
Gründen.

**Erstens: Der Broker ist genau das, was im Zielszenario fehlt.** Der
MQTT-Broker dieser Anlage *ist* der ioBroker-MQTT-Adapter
(`Vorhaben-Notbetrieb-Weboberflaeche.md`, Abschnitt 1). Fällt der ioBroker aus,
fällt der Broker mit. Ein Neustart der Bridge in dieser Lage findet niemanden,
der irgendetwas wiedereinspielen könnte — und das ist die einzige Lage, für
die der ganze Umbau gemacht wird. Retain löst den Normalfall und lässt den
Notfall offen. NVS löst beide.

**Zweitens: Die Karenzzeit wirft die Wiedereinspielung weg — mit gutem
Grund.** Der Adapter schickt jedem neuen Abonnenten die gespeicherten Werte,
und zwar unmittelbar nach dem SUBACK, also mitten in `SUBSCRIBE_GRACE`. Diese
Sperre ist nicht vorsorglich, sondern eine Reaktion auf gemessenen Schaden:
Am 2026-08-13 setzte ein wiedereingespielter Kurvenwert vom Vortag nach jedem
Neustart den Vorlauf-Sollwert auf 55 °C. Damit der Retain-Weg funktionierte,
müsste der Relaiszweig von der Karenz ausgenommen werden — wie der
Notbetriebszweig. Der darf das, weil seine Werte **nie ungefragt an die
Wärmepumpe gehen**. Für ein Relais gilt das Gegenteil: Es wirkt sofort auf
Hardware. Ein alter Wert würde nicht gespeichert, sondern geschaltet.

**Drittens: Das Retain-Bit trägt hier nicht die Bedeutung, die der Vorschlag
voraussetzt.** Am 2026-08-20 am Adaptercode belegt und am `notbetrieb`-Zweig
gemessen: Beim ioBroker-Adapter trägt die **Wiedereinspielung beim Subscribe
`retain=0`**, ein **Live-Publish `retain=1`** — also genau andersherum, als der
Vorschlag annimmt. Und die Wiedereinspielung passiert ohnehin, auch für Topics,
die nie retained gesendet wurden: Vier Werte an einen Zweig, den der Adapter
nie zuvor gesehen hatte, kamen beim nächsten Verbinden von allein zurück.
Etwas „retained" zu senden ändert an diesem Verhalten also nichts — der Effekt
ist hier ohnehin da, er ist nur unerwünscht. Dazu kommt: PubSubClient reicht
das Retain-Flag gar nicht an den Callback durch, die Firmware könnte die
beiden Fälle heute nicht einmal unterscheiden.

### 11.3 Der Vergleich, auf den es hinausläuft

Lage | Retain (Vorschlag) | NVS (Empfehlung)
:--- | :--- | :---
Reboot, Broker läuft | Zustand zurück, aber erst nach WLAN **und** MQTT-Connect — Sekunden bis Minuten | Zustand nach ~300 ms, noch vor dem WLAN
**Reboot, Broker weg** | **gar nicht** | Zustand da
Karenzzeit | muss für den Relaiszweig aufgegeben werden | unberührt
Board war lange aus | Broker-Wert ist der aktuellere | NVS-Wert evtl. veraltet, Re-Assert korrigiert in 5 min

Die vorletzte Zeile ist der eigentliche Preis, die letzte der einzige Punkt,
an dem Retain besser wäre — und der ist durch den 5-min-Re-Assert (8.4
Punkt 2) ohnehin abgedeckt.

Für Relais 2 kommt die Zeit aus der ersten Zeile dazu: Zwischen Boot und
MQTT-Connect stünde der Heat/Cool-Kontakt auf „Heizen", und zwar nicht
Millisekunden, sondern die volle Netzanlaufzeit — im Sommer also genau der
Moduswechsel, den Abschnitt 4 vermeiden will. Mit NVS in der ersten Zeile von
`setup()` schrumpft das auf die Bootlücke.

**Ergebnis: Es bleibt bei NVS, Retain wird nicht zusätzlich genutzt.** Ein
zweiter Weg, der denselben Zustand herstellt, bringt hier keine Redundanz —
er bringt eine zweite Stelle, an der ein alter Wert hereinkommen kann.
