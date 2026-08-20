# Vorhaben: Notbetrieb über die Weboberfläche schaltbar machen

Der Notbetrieb soll ohne ioBroker, ohne Node-RED und ohne MQTT-Broker
auslösbar sein — mit einem Browser, von einem Menschen aus der Familie.

**Anspruch:** Der Notbetrieb muss nicht alles halten, was der Normalbetrieb
kann. Er soll verhindern, dass das Haus auskühlt, und Zeit verschaffen, in Ruhe
nach einer Lösung zu suchen. Er ist **einstufig** — Stufe 1 heizt, Stufe 2
macht Warmwasser.

**Grenze:** Der Notbetrieb setzt eine funktionierende WLAN-Verbindung voraus.
Ist auch das WLAN weg, liegt ein Stromausfall vor — dafür wird hier keine
Lösung gesucht (Owner-Entscheidung 2026-08-19).

**Stand dieser Datei:** 2026-08-20 abends, Firmware 3.11.0 auf beiden Stufen.
Alle Grundsatzfragen sind entschieden (Abschnitt 5), alle drei Messungen sind
beantwortet (Abschnitt 6). Der Messlauf zu M1 hat zusätzlich einen Fehler in
der Kurvenspiegelung aufgedeckt; Doku und `kurven_sync.py` sind korrigiert
(Abschnitt 6a). **Gebaut ist beides: der Knopf in der Firmware und die
Node-RED-Seite, die ihn versorgt** (Abschnitt 10). Was fehlt, ist der Nachweis
an der Anlage — Etappe 5.

---

## 1. Warum das nötig ist — das Henne-Ei-Problem

Mit 3.11.0 ist die Betriebsart erstmals fernschaltbar (`HeatingMode` (SET35)
und `CoolingMode` (SET36), am Gerät in vier Läufen belegt, siehe
[`SET-TOP-Zuordnung.md`](SET-TOP-Zuordnung.md) Fußnote ⁶). Das löst den
Notbetrieb aber nur halb:

**Der MQTT-Broker *ist* der ioBroker-MQTT-Adapter** auf 192.168.2.147:1883
(am 2026-08-11 nachgemessen). Fällt der ioBroker aus, fehlt nicht nur der
Absender des Kommandos, sondern der Übertragungsweg selbst — die Firmware hat
dann niemanden mehr, von dem sie ein `set/HeatingMode 0` bekommen könnte.

Ausfallszenario | heute lösbar? | womit
:--- | :--- | :---
Node-RED-Flow fehlerhaft, ioBroker läuft | ja | `HeatingMode` über MQTT
Node-RED-Container weg, Broker läuft | ja | `HeatingMode` über MQTT
**ioBroker/Synology komplett aus** | **nein** | *dieses Vorhaben*
WLAN weg | — | außerhalb des Umfangs, siehe Grenze oben

Die Firmware hat einen eigenen Webserver, der vom Broker unabhängig läuft. Das
ist der Weg, der im dritten Fall noch übrig bleibt.

## 2. Was der Notbetrieb tatsächlich leisten muss

Nicht nur umschalten. Damit die Wärmepumpe danach sinnvoll arbeitet, braucht es
mehrere Schritte in der richtigen Reihenfolge — und die Reihenfolge ist keine
Kosmetik: Das Umschalten auf Kurvenbetrieb setzt die Kurvenpunkte auf die
Panasonic-Werksvorgaben zurück (am 2026-08-11 gemessen: 55 °C bei −5 °C und
35 °C bei +15 °C). Wer die Kurve *vor* dem Umschalten schreibt, schreibt sie
umsonst.

**Stufe 1 (H1 / WP1, 192.168.2.120) — Rolle Heizen**

Schritt | Kommando | Rückgelesen an
:--- | :--- | :---
1. Betriebsart auf Kurve | `HeatingMode` (SET35) = 0 | `Heating_Mode` (TOP76) = 0
2. Vorlauf bei kalt | `Z1HeatCurveTargetHighTemp` (SET27) | `Z1_Heat_Curve_Target_High_Temp` (TOP29)
3. Vorlauf bei warm | `Z1HeatCurveTargetLowTemp` (SET28) | `Z1_Heat_Curve_Target_Low_Temp` (TOP30)
4. Untere Außentemperatur | `Z1HeatCurveOutsideLowTemp` (SET29) | `Z1_Heat_Curve_Outside_Low_Temp` (TOP32)
5. Obere Außentemperatur | `Z1HeatCurveOutsideHighTemp` (SET30) | `Z1_Heat_Curve_Outside_High_Temp` (TOP31)
6. Anlage einschalten | `Heatpump` (SET1) = 1 | `Heatpump_State` (TOP0) = 1

Die Schritte 2–5 sind am 2026-08-20 im Kurvenbetrieb belegt (M1a): alle vier
Werte in einem Sammelfenster gesendet, alle vier binnen 15 s zurückgelesen. Zu
Schritt 2 und 3 siehe Abschnitt 6a — **`TargetHigh` gehört zur *unteren*
Außentemperatur**, ist also der Vorlauf bei Kälte. Bis zum 2026-08-20 stand das
in der Doku falsch herum.

**Stufe 2 (H2 / WP2, 192.168.2.122) — Rolle Warmwasser**

Schritt | Kommando | Rückgelesen an
:--- | :--- | :---
1. Betriebsmodus | `OperationMode` (SET9) = 3 (DHW only) | `Operating_Mode_State` (TOP4) = 3
2. Speichertemperatur | `DHWTemp` (SET11) | `DHW_Target_Temp` (TOP9)
3. Anlage einschalten | `Heatpump` (SET1) = 1 | `Heatpump_State` (TOP0) = 1

Schritt 1 trägt auch dann, wenn die Anlage vom KNX-Aktor auf Kühlen steht —
also im Sommerfall, für den der Notbetrieb an Stufe 2 überhaupt gedacht ist
(gemessen 2026-08-20, M3 in Abschnitt 6).

Die Warmwasserseite braucht **keine Kurve** — der Knopf dort ist deutlich
einfacher. Alle Kommandos gehen über `setCommands[]` und damit über denselben
geprüften Pfad, den heute schon MQTT benutzt.

### `Z1HeatRequestTemperature` (SET5) wird NICHT gesetzt

Das war im ersten Entwurf noch anders und ist eine echte Korrektur:

Im **Direktbetrieb** sind `Z1HeatRequestTemperature` (SET5, Byte 38) und
`Z1HeatCurveTargetHighTemp` (SET27, Byte 75) in der Wärmepumpe derselbe Wert —
am 2026-08-10 in beide Richtungen belegt. Im **Kurvenbetrieb** gilt das nicht:
Dort ist der Benutzerwert die **Parallelverschiebung der Kurve (±5 K)**, und
die vier Kurvenpunkte sind eigene Größen.

Drei Belege, die zusammenpassen:

* Am 2026-08-11 meldete `Z1_Heat_Request_Temp` (TOP27) im Kurvenbetrieb den
  Wert **0**, während `Z1_Heat_Curve_Target_High_Temp` (TOP29) gleichzeitig
  **55** zeigte. Wären es dieselbe Speicherstelle, müsste `Z1_Heat_Request_Temp`
  dort ebenfalls
  55 zeigen. Die 0 ist die neutrale Verschiebung.
* Die Herstellerunterlagen in `doku-intern/` trennen genauso: die vier
  Kurvenpunkte sind Installateurwerte, der Benutzerwert ist eine Verschiebung
  gegen die Kurve, die Zieltemperatur ergibt sich als Kurvenwert plus
  Verschiebung.
* Die Messung vom 2026-08-10, auf der die Gleichsetzung beruht, lief
  **ausschließlich im Direktbetrieb**.

**Folge für den Notbetrieb:** umschalten, dann die vier Kurvenpunkte setzen,
`Z1HeatRequestTemperature` gar nicht anfassen. Ein Wert weniger im Notfallpfad.

**Am 2026-08-20 am Gerät bestätigt (M1b).** Im Kurvenbetrieb sind die beiden
Werte getrennte Speicherstellen, und der Benutzerwert wirkt als
Parallelverschiebung — beides in einer Messung sichtbar:

Gesendet | `Z1_Heat_Request_Temp` (TOP27) | `Main_Target_Temp` (TOP7) | `Z1_Heat_Curve_Target_High_Temp` (TOP29)
:--- | ---: | ---: | ---:
— (Kurve 34/26, Außentemperatur 28 °C) | 0 | 26 | 34
`Z1HeatRequestTemperature` = 2 | 2 | **28** | 34
`Z1HeatRequestTemperature` = 4 | 4 | **30** | 34

Die Zieltemperatur folgt dem Kurvenwert plus Verschiebung, der Kurvenpunkt
bleibt stehen. Im Direktbetrieb wäre TOP29 mitgewandert.

**Folge für die Doku:** Die Gleichsetzung von `Z1HeatRequestTemperature` und
`Z1HeatCurveTargetHighTemp` ist an drei Stellen zu präzisieren (siehe
Abschnitt 9). Sie wird nicht falsch, aber sie gilt nur im Direktbetrieb.

**Das Restrisiko ist erledigt (M1c).** Die Befürchtung war, der 5-min-Re-Assert
der Kaskadensteuerung könnte mit `set/Z1HeatRequestTemperature 20` im
Kurvenbetrieb als *Verschiebung* landen — die Bereichsprüfung der Firmware
lässt −5..65 durch, klemmt also nicht. Die Wärmepumpe verwirft den Wert
stillschweigend: Er steht nachweislich im Kommandotelegramm, und TOP27 bleibt
unverändert stehen. Einzelheiten in Abschnitt 6.

## 3. Woher die Werte kommen — ein eigener MQTT-Kanal, nur im RAM

Die Firmware kennt die Kurve heute nicht. [`test/kurven_sync.py`](test/kurven_sync.py)
spiegelt die Werte aus `0_userdata.0.kaskade.Konfiguration.*` **direkt in die
Wärmepumpe**; die Firmware sieht sie nur durchlaufen und behält nichts davon.
Für den Notbetrieb muss sie sie kennen, ohne dass in dem Moment noch jemand
sendet.

**Der Weg: ein eigener Topic-Zweig `<prefix>/notbetrieb/<Name>`,** von Node-RED
bei Änderung beschickt, in der Firmware im RAM gehalten. Diese Werte gehen
**nie** an die Wärmepumpe — außer wenn der Knopf gedrückt wird.

Rolle | Werte
:--- | :---
Heizen (H1) | `Z1HeatCurveTargetHighTemp`, `Z1HeatCurveTargetLowTemp`, `Z1HeatCurveOutsideLowTemp`, `Z1HeatCurveOutsideHighTemp`
Warmwasser (H2) | `DHWTemp`

Der obere Kurvenpunkt muss über diesen Kanal kommen und **darf nicht** über das
normale `set/Z1HeatCurveTargetHighTemp` laufen: Im Direktbetrieb verstellte das
die aktive Vorlauf-Solltemperatur. Genau deshalb überträgt `kurven_sync.py` ihn
bis heute nicht.

### Warum RAM reicht und keine Datei geschrieben wird

Der ursprüngliche Entwurf sah `/notbetrieb.json` auf LittleFS vor. Das ist
gestrichen (Owner-Entscheidung 2026-08-20):

* Die Werte überleben im RAM jeden Broker-Ausfall — sie altern nur, wenn der
  ESP neu startet.
* Startet der ESP neu, **während der Broker läuft**, sind sie binnen Sekunden
  wieder da (siehe unten).
* Startet er neu, **während der Broker weg ist**, liegt ein Stromausfall vor.
  Dann braucht die Wärmepumpe selbst länger, bis sie wieder arbeitsbereit ist,
  als die Steuerung zum Hochfahren. Dieser Fall läuft heute problemlos von
  allein und braucht keinen Notbetrieb.

Damit entfallen: die Datei, der JSON-Parser im Notfallpfad, der Flash-Verschleiß
und das Boot-Risiko. Der Kommentar bei `loadConfigValue()` beschreibt die Falle,
die es damit gar nicht erst gibt.

### Der tragende Punkt: die Karenzzeit muss diesen Kanal ausnehmen

Nach jedem Verbinden verwirft [`HeishaMon.cpp:320`](src/HeishaMon.cpp#L320) für
`SUBSCRIBE_GRACE` alles, was hereinkommt. Der Grund ist gemessen (2026-08-13):
Der ioBroker-Adapter spielt einem neuen Abonnenten die gespeicherten Werte
jedes Set-Topics ein, und ein Kurvenwert vom Vortag setzte so nach jedem
Neustart den Vorlauf-Sollwert auf 55 °C.

Für den Notbetriebskanal ist dieselbe Wiedereinspielung **kein Problem, sondern
der Mechanismus**: Nach jedem Neustart und jedem Reconnect sind die Kurvenwerte
dadurch binnen Sekunden wieder da, ohne dass Node-RED etwas tun muss. Die
Gefahr, die zur Karenzzeit geführt hat, kann hier nicht auftreten — diese Werte
gehen nie ungefragt an die Wärmepumpe.

**Also: Der Zweig `<prefix>/notbetrieb/` wird vor der Karenzprüfung behandelt.**
Fünf Zeilen in `mqtt_callback()`, und sie sind der Unterschied zwischen einem
Knopf, der nach jedem Neustart funktioniert, und einem, der es nicht tut.
Umgesetzt am 2026-08-20 (Etappe 2), abgesichert durch
[`test/notbetrieb_test.cpp`](test/notbetrieb_test.cpp).

**Der Mechanismus ist am Broker nachgemessen (2026-08-20).** Belegt war die
Wiedereinspielung bisher nur für den `set`-Zweig; ob der Adapter das auch für
einen Zweig tut, den er nie zuvor gesehen hat, war offen. Er tut es: Vier Werte
an `panasonic_heat_pump_test/notbetrieb/*` gesendet, dann mit
[`test/mqtt_sub.py`](test/mqtt_sub.py) neu verbunden — alle vier kamen von
allein, ohne dass jemand publizierte. Der Subscriber macht dabei genau das, was
die Firmware nach einem Neustart tut, deshalb brauchte der Nachweis kein Gerät.
Die Wiedereinspielung kommt mit **retain=0** — der Schluss daraus, sie sei
damit nicht von einem echten Kommando zu unterscheiden, war jedoch **falsch und
ist am 2026-08-20 richtiggestellt**: Ein Live-Publish des Adapters trägt
**retain=1**, die Wiedereinspielung beim Subscribe **retain=0**. Belegt im
Adaptercode des ioBroker-MQTT-Adapters (`MQTTServer.js:162` gegen `:331`) und am
`notbetrieb`-Zweig gemessen — es hängt am **Codepfad**, nicht am Topic-Zweig.

Für den Notbetriebskanal ändert das nichts: Die Trennung läuft hier über den
eigenen Zweig und braucht das Bit nicht. Es eröffnet aber einen präziseren Weg
für die Karenzzeit im `set`-Zweig, der heute pauschal alles wegwirft — als
Folgethema in Abschnitt 9 notiert.

### Fehlen Werte, bleibt der Knopf gesperrt

Unvollständig (drei von vier Kurvenpunkten) heißt gesperrt, mit Klartext auf
der Seite. Lieber gar nicht schalten als auf die Werkskurve.

## 4. Die Umsetzung — vier Bausteine

**A — Notbetriebswerte annehmen und halten. Steht seit 2026-08-20.**
Neue Pfadwurzel `Topics::NOTBETRIEB` in [`Topics.h`](src/Topics.h)/[`Topics.cpp`](src/Topics.cpp),
eine kleine Tabelle mit Name und Bereichsgrenzen (dieselben Grenzen wie in
`setCommands[]`), Abonnement analog `subscribe_set_topics()`, Annahme vor der
Karenzprüfung. Gehalten wird in wenigen Bytes RAM plus einem
Vollständigkeits-Flag.

*Am Prüfstand belegt (192.168.2.197, D1 mini ohne Wärmepumpe):* Nach einem
Neustart standen ohne jedes Zutun binnen Sekunden wieder alle vier Werte —
`Notbetrieb einsatzbereit: alle 4 Werte liegen vor`, und im selben Moment
`34 wiedereingespielte Set-Kommandos nach dem Verbinden verworfen`. Die
Karenz-Ausnahme wirkt also genau so getrennt, wie sie soll. Einzelheiten und
die Ablehnungspfade in [`test/README.md`](test/README.md).

**B — Endpunkt und Seite. Steht seit 2026-08-20.**
`/notbetrieb` mit demselben Auth-Muster wie `/reboot` und `/settings`
([`HeishaMon.cpp:196-226`](src/HeishaMon.cpp#L196-L226)), ein Sidebar-Eintrag
mehr in `sidebar[]`.

Der Knopf ist ein **POST-Formular**, kein Link: ein Klick, keine Rückfrage —
aber auch kein versehentliches Auslösen durch Link-Vorschau oder Virenscanner.
Eine zweite Route `/notbetrieb/status` liefert den Fortschritt, die Seite fragt
sie alle zwei Sekunden ab.

**Welcher Knopf erscheint, entscheidet ein Build-Flag.** Die Stufe ist der
Firmware zur Laufzeit bekannt, weil sie einkompiliert ist (`HEISHA_STAGE_NAME`,
`HEISHA_MQTT_PREFIX` aus `stage_h1`/`stage_h2` in
[`platformio.ini`](platformio.ini)). Eine gemeinsame Firmware für beide Stufen
gibt es ohnehin nicht, solange der MQTT-Prefix ein Build-Flag ist. Sauberer als
ein Stringvergleich auf den Stufennamen ist ein eigenes Flag in denselben
Abschnitten, etwa `NOTBETRIEB_ROLLE_HEIZEN` bzw. `NOTBETRIEB_ROLLE_WASSER`.
Kein neues Konfigurationsfeld, kein Rollenfeld irgendwo auf dem Gerät.

**C — Die Schrittfolge ausführen, ohne einen zweiten Merge-Pfad zu bauen. Steht seit 2026-08-20.**

Die Kommandos gehen durch `build_heatpump_command()` — **unverändert**. Der
Handler baut nur den Topic-String:

```cpp
snprintf(topic, sizeof(topic), "%s/HeatingMode", Topics::SET.c_str());
build_heatpump_command(topic, "0");
```

Damit bleiben Bereichsprüfung, Maskenmerge, Konfliktdiagnose, Log und
`register_new_command()` genau ein Mal im Code. Die Alternative — ein
vorgefertigtes 110-Byte-Kommando unterschieben — ist verworfen: Sie umgeht alle
vier Prüfungen und überschreibt zusätzlich jedes andere Feld, das in einem
gerade offenen Sammelfenster steht.

**Die Schritte laufen einzeln, nicht in einem Sammelfenster.** Ein Webhandler,
der sechs Kommandos hintereinander absetzt, packt sie alle in ein Telegramm
(Deckel 2 s, [`sendwindow.h`](src/sendwindow.h)) — dann konkurriert das
Kurvenschreiben mit dem Werks-Reset des Moduswechsels, und welcher gewinnt, ist
unbekannt. Der Notbetrieb wird deshalb ein kleiner Zustandsautomat, der aus
`loop()` getickt wird: senden → zurücklesen → nächster Schritt. Der
HTTP-Handler stößt ihn nur an und antwortet sofort.

**D — GRÜN oder ROT, sonst nichts. Steht seit 2026-08-20.**

Keine Zustandstabelle, keine Erklärseite. In dieser Lage hilft eine
Fehlerbeschreibung niemandem: Entweder es wird GRÜN, oder es gilt Plan B — die
Bedienung am Panel nach der Offline-Anleitung.

**GRÜN heißt zurückgelesen, nicht abgesendet.** Ein GRÜN, das nur „gesendet"
bedeutet, wäre das wertlose Signal. Die Firmware hält die Rücklesewerte ohnehin
in `actual_data[]`; jeder Schritt wird gegen sein TOP geprüft (Spalte 3 in den
Tabellen in Abschnitt 2). Zeitbudget: Die Wärmepumpe übernimmt in 2–8 s
(KNX-Messung 2026-08-16), der Abfragezyklus liegt bei rund 6 s — **Schritt-Timeout
20 s**, bis dahin zeigt die Seite „läuft…". Ein vollständiger Heizen-Lauf mit
realistischen 6-s-Antworten braucht 36 s (im Hosttest gemessen).

**Der Gesamtdeckel ist abgeleitet, nicht frei gewählt** (Umsetzung 2026-08-20):
Schrittzahl × Schritt-Timeout, also 120 s für Heizen und 60 s für Warmwasser.
Der ursprüngliche Entwurf nannte pauschal 60 s — das wäre bei sechs Schritten
inkonsistent gewesen: Schon zwei Schritte im Timeout hätten den Deckel gerissen,
und ob ein Lauf ROT wird, hinge davon ab, welche der beiden Regeln zufällig
zuerst greift. So bleibt der Deckel das, was er sein soll: ein Notausgang, falls
der Automat hängt, nicht der normale Weg zu ROT.

**Eine Regel kam beim Bauen dazu: die Mindestwartezeit.** Ein Schritt gilt
frühestens 8 s nach seinem Kommando als bestätigt. Ohne das könnte ein
*veralteter* Rücklesewert einen Schritt sofort abhaken — und der gefährliche
Fall steckt in der Schrittfolge selbst: Das Umschalten setzt die Kurvenpunkte
auf die Werksvorgaben zurück. Trüge `actual_data` beim zweiten Schritt noch den
Kurvenwert von vorher, gälte er als erledigt, und der Werks-Reset überschriebe
ihn danach. GRÜN, und die Anlage führe die Werkskurve. Der Preis: Ein
Heizen-Lauf dauert rund 48 statt 36 s.

**Und eine Falle, die am Prüfstand sichtbar wurde:** `actual_data[]` wird über
den **Zeilenindex** adressiert, nicht über die TOP-Nummer — die Nummerierung hat
Lücken und reicht bis 104 bei 92 Zeilen. Dafür gibt es jetzt
`state_topic_index()` in `decode.cpp`. Dazu kommt, dass ein noch nie
empfangenes TOP als leerer Text dasteht: `atoi("")` ist 0, und der erste Schritt
der Heizen-Folge hat den Sollwert 0. Ein naiver Vergleich hätte GRÜN gemeldet,
ohne dass die Wärmepumpe je geantwortet hätte. Beides ist im Hosttest
abgesichert und am Prüfstand belegt (dort läuft der Fehlerpfad, weil keine
Wärmepumpe antwortet).

**Was GRÜN nicht heißt:** dass die Anlage heizt. Die KNX-Freigabe des
Kompressors ist im Antworttelegramm nicht sichtbar — am 2026-08-15/16 byteweise
gesucht und nicht gefunden. Wenn es nach GRÜN nicht warm wird, fehlt die
KNX-Freigabe. Dieser Satz gehört in die Offline-Anleitung, nicht auf die Seite.

## 5. Entschieden

**1. Beide Kaskadenstufen oder nur eine?** — Einstufig: **Stufe 1 heizt, Stufe 2
macht Warmwasser.** Ein Kaskadenbetrieb ohne übergeordnete Steuerung würde nicht
lange gutgehen. Der Notbetrieb muss nicht alles halten, was der Normalbetrieb
kann; er soll das Haus warm halten und Zeit verschaffen. (2026-08-19)

Damit entfällt die **Kühlseite** vollständig: `CoolingMode` (SET36) und die
Kühlkurve (`Z1CoolCurveTargetHighTemp` bis `Z1CoolCurveOutsideHighTemp`,
SET31–SET34) spielen keine Rolle, Byte 28 wird nur über
`HeatingMode` (SET35) angefasst — der Fall aus Lauf 3 vom 2026-08-19. Im Sommer
wird nur H2 auf Warmwasser geschaltet, H1 bleibt aus. Und die beiden Stufen
konkurrieren nicht, weil sie verschiedene Aufgaben haben.

**1a. Wie erfährt die Firmware ihre Rolle?** — Über ein Build-Flag, siehe
Baustein B. Kein Rollenfeld auf dem Gerät.

**2. Knopf oder automatischer Rückfall?** — **Nur der Knopf.** Kein
automatisches Umschalten in den Notbetrieb; dafür ist ein Mensch lieber. Ein
längerer Wartungsstopp am ioBroker sähe für die Firmware ohnehin genauso aus wie
ein Totalausfall.

**3. Wie kommen die Kurvenwerte in die Firmware?** — Über den eigenen
MQTT-Kanal aus Abschnitt 3, gehalten im RAM. Node-RED sendet bei Änderung.
Fest einkompiliert wird nichts.

**4. Ein-/Ausschalten der Anlage.** — `Heatpump` (SET1) = 1 gehört in die
Schrittfolge, es gibt keine Bedingung, unter der nicht eingeschaltet werden
dürfte. Zu beachten sind die **zwei Freigabestufen**: KNX gibt den Kompressor
frei, MQTT schaltet die Wärmepumpe ein wie das AN am Bedienfeld. Ist die
Kaskade beim Ausfall nicht über KNX freigegeben, muss das über KNX geschehen —
vorgesehen ist dafür ein physischer KNX-Taster. Das liegt außerhalb dieses
Repos, ist aber Voraussetzung dafür, dass der Notbetrieb wirkt.

**5. Zugang bei WLAN-Ausfall.** — Entfällt, siehe Grenze am Anfang.

**6. Zurückschalten nach der Störung.** — **Node-RED macht das, nicht die
Firmware.** Eine Zeile im 5-min-Re-Assert, die `set/HeatingMode 1` mitsendet,
holt die Anlage automatisch aus dem Notbetrieb.

Bedingung: Node-RED darf das nur senden, wenn die Kaskadenregelung
**tatsächlich rechnet** (Herzschlag), nicht schon, wenn der Container läuft —
sonst holt es die Wärmepumpe aus der Kurve in einen Direktbetrieb, den niemand
nachführt.

Eine Firmware-Lösung ist verworfen: Ihr Notbetriebs-Zustand läge im RAM, nach
einem Neustart wüsste sie nichts mehr davon und würde nie zurückschalten.

**Gebaut am 2026-08-20** (Hauptmodus-Verteiler V6.5, neuer Ausgang 15): Der
5-min-Re-Assert sendet `set/HeatingMode` = 1 mit, sobald ein Herzschlag vorliegt
(`KK_HeatTarget_long`/`KK_CoolTarget_long`, Fenster 5 min); ohne Puls unterbleibt
die Zeile. Der Wartungsschalter sperrt den Kanal wie die übrigen zwölf. Am
Produktivsystem nachgesehen (2026-08-20 19:56): `mqtt.0.panasonic_heat_pump.set.HeatingMode`
= 1, Zeitstempel keine zwei Minuten alt. **Der Rückweg ist damit scharf** — mit
Folgen für jeden Testlauf, siehe Abschnitt 7.

**7. Kein Knopf „Notbetrieb aus".** (2026-08-20) Für „zurück auf Direktvorgabe"
gibt es kein Szenario, das Node-RED nicht besser löst — und im einzigen Fall,
in dem überhaupt jemand eingreifen müsste (Steuerung bleibt tot), wäre es die
falsche Aktion: Beim Zurückschalten übernimmt der Sollwert den unteren
Kurvenpunkt (2026-08-11: `Z1_Heat_Request_Temp` sprang auf 35). Die Anlage stünde dann auf 35 °C
Vorlauf fest, ohne jeden Regler.

Läuft die Anlage im Notbetrieb, verhält sie sich wie eine ganz normale
Wärmepumpe. Die weitere Bedienung — wärmer, kälter, aus — erfolgt am
Bedienpanel, und das versteht jedes Kind. Ein zweiter Knopf wäre ein zweiter
Knopf, den jemand drückt.

**Die Seite hat damit genau einen Knopf.**

## 6. Was zu messen war — alles beantwortet

**M1 — an H1, BEANTWORTET 2026-08-20.** Ein Lauf von 20 Minuten an der
stehenden Anlage (`Heatpump_State` 0, `Compressor_Freq` 0), Kompressor über KNX
freigegeben, Betriebsrichtung Kühlen (`Heat_Cool_SW_State` TOP101 = 1),
Außentemperatur 28 °C. Werkzeuge: [`test/top_watch.py`](test/top_watch.py) für
den TOP-Verlauf, [`test/produktiv_mitschnitt.py`](test/produktiv_mitschnitt.py)
für die Kommandotelegramme.

**a. Nimmt die Wärmepumpe die Kurvenpunkte im Kurvenbetrieb an? — Ja.** Nach
`HeatingMode 0` (TOP76 auf 0 nach 15 s, Werkskurve 55/35/−5/15 wie erwartet)
wurden alle vier Punkte in *einem* Sammelfenster gesendet und binnen 15 s
zurückgelesen:

Gesendet | Zurückgelesen
:--- | :---
`Z1HeatCurveTargetHighTemp` = 34 | TOP29 = 34
`Z1HeatCurveTargetLowTemp` = 26 | TOP30 = 26
`Z1HeatCurveOutsideLowTemp` = −10 | TOP32 = −10
`Z1HeatCurveOutsideHighTemp` = 15 | TOP31 = 15

Damit trägt die Schrittfolge aus Abschnitt 2. Der obere Kurvenpunkt geht im
Kurvenbetrieb durch — die Einschränkung aus `kurven_sync.py` gilt nur im
Direktbetrieb.

**b. Wirkt `Z1HeatRequestTemperature` als Parallelverschiebung? — Ja.**
Messwerte in der Tabelle in Abschnitt 2. `Main_Target_Temp` (TOP7) folgte dem
Kurvenwert plus Verschiebung (26 → 28 → 30), der Kurvenpunkt TOP29 blieb auf 34
stehen. Getrennte Speicherstellen, Doku-Korrektur bestätigt.

**c. Was macht ein hereinschneidender Re-Assert mit 20? — Nichts.** Die
Wärmepumpe verwirft ihn stillschweigend. Dreifach belegt:

* `set/Z1HeatRequestTemperature 20` zweimal einzeln gesendet — TOP27 blieb
  beide Male auf seinem vorherigen Wert (2 bzw. 4), TOP7 und TOP29 unverändert.
* Der Hexlog-Mitschnitt zeigt das Kommando im Telegramm (`Z1 Heat 20 C`) — es
  wird also gesendet und nicht etwa von der Firmware gefiltert. Die
  Gegenprobe mit einem gültigen Wert (4) schlug im selben Mitschnitt durch.
* Im selben Fenster fing der Mitschnitt einen **echten Re-Assert** der
  Kaskadensteuerung (Telegramm mit `Heatpump aus`, `WaterPump auto`,
  `OperationMode`, `Z1 Heat 20 C`, `Z1 Cool 20 C`, `PumpSpeed 100`). Auch der
  ließ die Verschiebung unberührt.

Werte innerhalb −5..+5 werden dagegen sofort übernommen. Die Wärmepumpe prüft
den Bereich also selbst — die Firmware muss das nicht nachbilden.

**Eine Einschränkung, ehrlich notiert:** 71 s nach dem Moduswechsel trat
einmalig ein Ausschlag auf — TOP27 sprang auf 20 und TOP7 auf 55, beides fiel
5 s später von selbst auf 0 bzw. 35 zurück. Im eingeschwungenen Kurvenbetrieb
war das nicht mehr zu reproduzieren. Für den Notbetrieb ohne Rückwirkung, weil
dort ohnehin nichts nachgesendet wird.

**M2 — an H1, BEANTWORTET 2026-08-20. Ja, folgenlos.** `set/HeatingMode 1` im
laufenden Direktbetrieb, 31 s beobachtet: keine Änderung an TOP7, TOP27, TOP28,
TOP29, TOP30 oder TOP76. Der Rückkehrweg aus Entscheidung 6 trägt — Node-RED
darf die Zeile bedenkenlos in jeden Re-Assert legen.

**Der Rückweg selbst ist dabei ein zweites Mal belegt.** Beim Zurückschalten
lief derselbe Werks-Reset wie beim Hinschalten, 4 s nach dem Moduswechsel:
TOP29 und TOP30 auf 35, TOP32 auf −5, und der Sollwert TOP27 übernahm die 35.
Das ist genau die Beobachtung vom 2026-08-11 und bestätigt Entscheidung 7 (kein
Knopf „Notbetrieb aus") aus einer zweiten Messung.

**Und der Werks-Reset trifft die Kühlseite mit — auch bei aktivem KNX-Kühlen.**
In beide Richtungen sprangen `Z1_Cool_Request_Temp` (TOP28),
`Z1_Water_Target_Temp` (TOP42) und `Z1_Cool_Curve_Target_High_Temp` (TOP72) auf
10. Der Kühl-Sollwert stand so 71 s (hin) bzw. 90 s (zurück) auf 10 °C, bis er
zurückgesetzt wurde. Bei stehender Anlage folgenlos; bei laufendem Kühlbetrieb
wäre es ein realer Eingriff. **Für den Notbetrieb heißt das:** Stufe 1 schaltet
im Sommer nicht auf Kurve — genau so ist Entscheidung 1 gefasst (H1 bleibt im
Sommer aus, nur H2 macht Warmwasser). Die Einschränkung gehört trotzdem in die
Offline-Anleitung.

## 6a. Nebenbefund aus M1: die Kurvenpaarung steht falsch herum

Der Lauf hat einen Fehler aufgedeckt, der nichts mit dem Webknopf zu tun hat,
aber den Notbetrieb unbrauchbar gemacht hätte.

**Der Beleg.** Bei 28 °C Außentemperatur (TOP14), Kurve auf TargetHigh = 34 /
TargetLow = 26 und Außenpunkten −10/+15, meldete `Main_Target_Temp` (TOP7)
**26 °C**. Weit oberhalb der oberen Außentemperatur gilt also **TargetLow**.

Daraus folgt die Paarung — und sie ist die umgekehrte von der, die in
[`MQTT-Topics.md`](MQTT-Topics.md) steht:

Wert | gehört zu | gilt bei
:--- | :--- | :---
`Z1HeatCurveTargetHighTemp` (SET27, TOP29) | `OutsideLow` (SET29, TOP32) | **kaltem** Wetter
`Z1HeatCurveTargetLowTemp` (SET28, TOP30) | `OutsideHigh` (SET30, TOP31) | **warmem** Wetter

Die Gegenprobe aus der Werkskurve passt: 55 °C bei −5 °C und 35 °C bei +15 °C —
eine Heizkurve fällt mit steigender Außentemperatur. Nach der Lesart in
`MQTT-Topics.md` müsste sie steigen.

**Was das anrichtet.** [`test/kurven_sync.py`](test/kurven_sync.py) spiegelt
`KK_HK_vlLo` — den Vorlauf bei der *niedrigen* Außentemperatur, 34 °C — nach
`Z1HeatCurveTargetLowTemp`, also in das Feld, das bei *warmem* Wetter gilt. Die
gespiegelte Kurve ist damit verdreht: 34 °C Vorlauf bei +15 °C und wärmer, und
bei −10 °C der obere Punkt, den das Werkzeug bewusst gar nicht überträgt.

Heute ohne Wirkung, weil die Anlage im Direktbetrieb läuft und die Kurve nur
mitgeführt wird. **Der Notbetrieb würde sie aktivieren** — und läuft er, wie
vorgesehen, an einem kalten Tag an, fährt die Wärmepumpe genau dann nach dem
falschen Stützpunkt. Das ist der Fall, für den das ganze Vorhaben gebaut wird.

**Erledigt am 2026-08-20:**

1. Paarung in [`MQTT-Topics.md`](MQTT-Topics.md) korrigiert — Heizkurve,
   Kühlkurve, SET-Tabelle und beide Prosafassungen. Ebenso
   [`README.md`](README.md), Fußnote ² in
   [`SET-TOP-Zuordnung.md`](SET-TOP-Zuordnung.md) und die Kopfkommentare von
   `kurven_test.py` und `kurven_grenzen.py`.
2. `MAPPING` und Kopfkommentar in [`kurven_sync.py`](test/kurven_sync.py)
   gedreht: `vlHi` (Vorlauf bei hoher Außentemperatur) geht nach `TargetLow`.
   Die Kühlkurve folgt derselben Systematik; dort sind beide Werte 20, es
   ändert sich also nur der Code, nicht was gesendet wird. Belegt ist nur die
   Heizseite.
3. Der Schlusshinweis des Werkzeugs **beziffert** jetzt den fehlenden Wert
   (`KK_HK_vlLo`, derzeit 34 °C) statt nur auf ihn zu verweisen. Wer im Notfall
   vor dem Bedienterminal steht, braucht die Zahl.
4. **Der korrigierte Sync ist um 15:01 auf beiden Stufen gelaufen.** Der
   `tablesnap`-Diff vorher/nachher zeigt an H1 wie an H2 genau eine Änderung:
   `Z1_Heat_Curve_Target_Low_Temp` (TOP30) von 34 auf 26. Kein Sollwert, kein
   Modus, kein Kühlwert hat sich bewegt — wie erwartet, denn im Direktbetrieb
   liest die Wärmepumpe dieses Feld nicht.

**Warum der Sync den laufenden Betrieb nicht berührt.** Es sind zwei getrennte
Wege: Die Regelung läuft über `KKheizkurve.js` (liest den
Konfigurationsbaum, rechnet `KK_HeatTarget`) und weiter über Node-RED als
`set/Z1HeatRequestTemperature` — im Direktbetrieb der einzige Wert, den die
Wärmepumpe befolgt. `kurven_sync.py` liest denselben Konfigurationsbaum **nur**
und schreibt ausschließlich die Kurvenfelder der Wärmepumpe, die erst bei
`Heating_Mode` = 0 wirksam werden. Es schreibt keinen einzigen
ioBroker-Datenpunkt, die Rechengrundlage bleibt also unverändert. Am 2026-08-20
in beide Richtungen belegt: im Kurvenbetrieb zog eine Änderung von `TargetLow`
den `Main_Target_Temp` mit (35 → 26), im Direktbetrieb blieb er unberührt
(TargetLow 35 → 34, `Main_Target_Temp` blieb 20).

Gegengeprüft wurde außerdem der Code aller 44 ioBroker-Skripte (Vollpull über
`js-pull.sh`): keines enthält `Z1HeatCurve`, `Z1CoolCurve`, `HeatingMode` oder
`CoolingMode`, und der `WP_Befehls_Waechter` prüft sieben Kanäle, von denen
keiner ein Kurvenfeld ist.

**Die ioBroker-Seite ist geprüft und richtig** (Owner-Auskunft 2026-08-20): Die
Kurvenberechnung in `ioBroker.javascript` liefert für Heiz- wie Kühlkurve
plausible Werte. Der Fehler saß allein in der Abbildung auf die
Panasonic-Felder.

**Was daraus für den Notbetrieb folgt.** `TargetHigh` ist nicht mehr nur der
Wert, der sich im Direktbetrieb mit dem Sollwert eine Speicherstelle teilt — er
ist der **tragende** Punkt der Notbetriebskurve, der Vorlauf bei Kälte. Er muss
über den Notbetriebskanal aus Abschnitt 3 kommen; dort ist er schon vorgesehen.

**Offen bleibt der Weg über das Bedienterminal.** Solange 3.12.0 nicht läuft,
ist der Notbetrieb der manuelle: umschalten, dann `TargetHigh` von Hand auf
34 °C setzen. Ohne diesen Schritt bleibt die Werksvorgabe von 55 °C stehen —
bei −5 °C Außentemperatur wären das rund 51 °C Vorlauf statt der ausgelegten
32 °C, jenseits der Estrichgrenze. Das gehört mit dieser Zahl in die
Offline-Anleitung (Abschnitt 9).

**M3 — an H2. BEANTWORTET 2026-08-20.** Ursprüngliche Frage: Läuft Warmwasser
bei KNX-erzwungenem Kühlen mit `OperationMode` (SET9) = 3, oder braucht es die 5
(Cool+DHW)?

**Antwort: SET9 = 3 genügt.** Die 5 wird nicht gebraucht — der Knopf auf Stufe 2
sendet die 3, wie in der Schrittfolge in Abschnitt 2 vorgesehen. Der KNX-Eingang
unterdrückt den Warmwasserbetrieb nicht.

Aufbau: Die Kaskade wurde aus der Haussteuerung über den KNX-Aktor auf Kühlen
geschaltet (`WP2_Heat-Cool` = true), H2 anschließend auf reinen Warmwasserbetrieb
gestellt — genau SET9 = 3. H2 meldete dabei durchgehend `Heat_Cool_SW_State`
(TOP101) = 1, stand also wirklich im Kühlzweig. Gemessen wurde per Mitschnitt der
ioBroker-States im 2-s-Takt.

Ergebnis: `ThreeWay_Valve_State` (TOP20) drehte um 09:54:41 von Raum auf DHW, der
Kompressor lief um 09:57:50 an (`Compressor_Freq` TOP8: 18 → 59 Hz), `Pump_Flow`
(TOP1) ging von 14,45 auf 10,2 l/min in den Speicherkreis. H1 blieb durchgehend
bei 0 Hz.

`Operating_Mode_State` (TOP4) blieb dabei auf 3 und wurde von der KNX-Richtung
**nicht** mitgezogen — im selben Lauf ging TOP4 an H1, das SET9 = 1 bekam, auf 1.
Das bestätigt die Semantik aus `MQTT-Topics.md`: TOP4 zeigt den zuletzt
kommandierten Modus, die tatsächliche Richtung steht in TOP101. Die Annahme im
ursprünglichen Fragetext („der KNX-Eingang … zieht TOP4 mit") galt für die
08-16-Messung, weil die Steuerung dort ihr eigenes `set/OperationMode`
nachschickte; ohne diesen Nachschub bleibt TOP4 stehen.

Der Ladezyklus wurde mit `ForceDHW` (SET10) angestoßen, weil der Speicher mit
38 °C die Starthysterese von Soll − 12 K (48 → 36 °C) noch nicht unterschritten
hatte. Das verfälscht das Ergebnis nicht: Der Boost lief nur von 09:54:33 bis
09:55:03, der Kompressor startete **2:47 min nach seinem Ende**. Die Ladung im
Kühlzweig ist also kein per Zwang gehaltener Sonderzustand.

**Nicht abgedeckt:** Der Lauf fand mit erreichbarem Broker und laufendem
Node-RED statt, dessen 5-Minuten-Re-Assert SET9 = 3 nachhielt. Im Notbetrieb wird
der Wert einmalig gesendet — nach der belegten Semantik „letzter Wert gilt"
unkritisch, aber hier nicht getrennt gemessen.

## 7. Risiken und Fallen

* **Der Test muss mit abgeschaltetem Broker laufen.** Sonst prüft er nicht den
  Fall, für den er gebaut ist. Ob die Firmware ohne erreichbaren Broker sauber
  weiterläuft, ist plausibel (die Reconnect-Logik hat einen Backoff), aber
  **nicht gemessen**.
* **Kein zweiter Merge-Pfad.** Siehe Baustein C.
* **Die Karenzzeit-Ausnahme ist kritisch.** Wird sie vergessen, funktioniert der
  Knopf im Labor und nach jedem Neustart nicht mehr. Gehört in den Hosttest.
* **Ein Knopf, der schaltet, ist ein Knopf, den jemand versehentlich drückt.**
  Der Zugangsschutz ist dasselbe Passwort wie für `/firmware`. Keine Rückfrage —
  aber ein POST-Formular statt eines Links, damit ihn niemand aus Versehen für
  den Browser mitlädt.
* **Die Bedienung ist für Laien.** „Notbetrieb ein" statt „`HeatingMode` auf 0". Die
  Passwörter und die Schritt-für-Schritt-Anleitung stehen offline bereit.
* **Ein Neustart ohne Broker sperrt den Knopf.** Bewusst in Kauf genommen
  (Abschnitt 3), aber die Seite muss es sagen statt stumm ROT zu zeigen.
* **Der Rückweg ist seit dem 2026-08-20 scharf und trifft die Testläufe.**
  Läuft die Kaskadensteuerung, holt der 5-min-Re-Assert die Anlage mit
  `set/HeatingMode` = 1 aus dem Notbetrieb zurück — im Ernstfall gewollt, im
  Versuch aber ein Rennen: Der Knopf-Lauf braucht rund 48 s, danach bleiben
  höchstens noch gut vier Minuten für Kurvenfoto und Kontrolle. Schlimmer ist
  der Treffer *während* des Laufs: Fällt der Re-Assert zwischen Schritt 1 und
  Schritt 6, steht die Betriebsart wieder auf Direktbetrieb, während der Automat
  noch Kurvenpunkte schreibt. **Deshalb wird jeder Lauf an Stufe 1 mit gesetztem
  Wartungsschalter gefahren** — der sperrt den Kanal, und das Abschalten der
  Wartung ist danach zugleich der Nachweis für Testplan-Punkt 6.

## 8. Testplan

0. Rettungsanker (Tag), Branch, Ausgangszustand über `/tablerefresh` sichern.
1. **Die Kurvenpaarung richtigstellen** (Abschnitt 6a) — erst danach ist die
   Schrittfolge das, was sie zu sein vorgibt. Die Messungen M1–M3 sind seit dem
   2026-08-20 alle beantwortet.
2. Hosttest für die prüfbaren Regeln, arduino-frei wie
   [`sendwindow.h`](src/sendwindow.h): Vollständigkeit der Werte,
   Bereichsgrenzen, Schrittfolge und Abbruch, Karenzzeit-Ausnahme.
3. Bausteine A–D auf dem Prüfstand-ESP8266 (192.168.2.197, keine WP
   angeschlossen) — Weboberfläche, Auth, fehlende Werte, Rollen-Flag.
4. An Stufe 1 bei stehender Anlage: Knopf drücken, Byte 28 und die Kurvenwerte
   im Mitschnitt verfolgen ([`test/byte_monitor.py`](test/byte_monitor.py)).
5. **Wiederholung mit abgeschaltetem MQTT-Broker** — der eigentliche Nachweis.
6. Rückkehr prüfen: Broker wieder an, Node-RED sendet `HeatingMode 1`,
   Betriebsart und Sollwerte kommen von allein zurück.
7. Aufräumen wie gehabt: `kurven_sync.py`, Sollwerte, Endkontrolle.

## 9. Nach der Umsetzung nachzuziehen

* [`README.md`](README.md) — Weboberfläche und der neue Endpunkt.
* [`src/version.h`](src/version.h) — Changelog zu 3.12.0 mit Problem, Nachweis
  und Größenänderung.
* [`MQTT-Topics.md`](MQTT-Topics.md) — der neue Zweig `<prefix>/notbetrieb/`.
* Die Offline-Anleitung der Familie — Schrittfolge, IP-Adressen, Passwort, der
  KNX-Taster für die Kompressorfreigabe, und der Hinweis, dass im Kurvenbetrieb
  „+1 am Bedienpanel" die ganze Kurve um 1 K verschiebt. **Dazu der obere
  Kurvenpunkt mit seiner Zahl:** nach dem Umschalten auf Kurve ist
  `TargetHigh` auf 34 °C zu setzen, sonst bleibt die Werksvorgabe 55 °C stehen
  (Abschnitt 6a).
* `NOTBETRIEB.md` im Node-RED-Projekt — samt der Rückkehr-Zeile im Re-Assert und
  der Bedingung dazu (Entscheidung 6).
* **Zu korrigieren, M1 liegt seit 2026-08-20 vor:** Die Gleichsetzung von
  `Z1HeatRequestTemperature` (SET5) und `Z1HeatCurveTargetHighTemp` (SET27) gilt
  nur im Direktbetrieb. Fundstellen: [`MQTT-Topics.md:468`](MQTT-Topics.md#L468)
  und [`MQTT-Topics.md:537`](MQTT-Topics.md#L537), Fußnote ² in
  [`SET-TOP-Zuordnung.md:125`](SET-TOP-Zuordnung.md#L125), der
  TargetHigh-Abschnitt in [`test/README.md:699`](test/README.md#L699).
* **Vorrangig zu korrigieren:** die Kurvenpaarung in `MQTT-Topics.md` und
  `kurven_sync.py` — Abschnitt 6a führt die Stellen einzeln auf. Das ist keine
  Nacharbeit nach der Umsetzung, sondern eine Vorbedingung für sie.
* **Zu korrigieren:** Changelog, `SET-TOP-Zuordnung.md` und das GitHub-Release
  zu 3.11.0 sagen sinngemäß „damit ist der Notbetrieb vollständig
  fernschaltbar". Das stimmt nur, solange ein Broker erreichbar ist.

**Folgethema, nicht Teil dieses Vorhabens — die Karenzzeit genauer fassen.**
`SUBSCRIBE_GRACE` wirft heute für ein Zeitfenster *alles* weg, was nach dem
Verbinden hereinkommt, also auch echte Kommandos. Seit dem Retain-Befund
(Abschnitt 3) gibt es einen präziseren Hebel: Die Wiedereinspielung des
ioBroker-Adapters trägt `retain=0`, ein Live-Publish `retain=1`. Ein Filter auf
das Bit träfe genau die Wiedereinspielung statt eines Zeitraums.

Nicht ohne eigene Messung umbauen: Belegt ist das bislang am `notbetrieb`-Zweig;
für den `set`-Zweig ist es aus dem Adaptercode abgeleitet, aber nicht
nachgemessen. Und der Callback der Firmware müsste das Retain-Flag überhaupt
erst durchreichen — heute tut er das nicht. Ein Fehlgriff hier bringt genau den
55-°C-Vorlauf zurück, wegen dem die Karenzzeit entstanden ist.

---

## 10. Stand der Umsetzung — Stand 2026-08-20, abends

**Alles committet, Branch `notbetrieb-web`.**
Rettungsanker: Tag `rettungsanker-vor-notbetrieb-web-2026-08-20` auf `main`.

### Was steht

Etappe | Inhalt | Commit
:--- | :--- | :---
0 | Tag, Branch, Arbeitsplan für Node-RED | `e2c81d0`
1 | [`src/notbetrieb.h`](src/notbetrieb.h) + Hosttest, in der CI | `d367220`
2 | Baustein A: Werte annehmen und halten | `6eaec46`
2 | Nachweis Wiedereinspielung am Broker | `39417e9`
2 | Nachweis am Prüfstand: übersteht den Neustart | `e5f654b`
3 | Bausteine B, C und D: der Knopf | `17b7621`
4 | **Node-RED-Seite** — im Nachbarprojekt gebaut und abgenommen | dort

Die Bausteine A–D sind vollständig gebaut und am Prüfstand geprüft — dort lief
mangels Wärmepumpe der **Fehler**pfad (ROT nach 20 s), und genau das war der
Zweck. Alle zehn Envs bauen, der Hosttest steht bei 113 Zusicherungen.

**Etappe 4 ist seit dem 2026-08-20 fertig** (Einzelheiten im Nachbarprojekt
`nodered-flows`, [`Arbeitsplan-Notbetrieb-NodeRED.md`](Arbeitsplan-Notbetrieb-NodeRED.md)
trägt den Erledigt-Vermerk):

* *Notbetriebswerte-Sender V1.0* speist die vier Kurvenpunkte an H1 und den
  DHW-Soll an H2, bei Änderung und beim Flow-Start; Werte außerhalb der
  Firmware-Bereiche werden geloggt statt gesendet.
* *Hauptmodus-Verteiler V6.5*, Ausgang 15: `set/HeatingMode` = 1 im
  5-min-Re-Assert, an den Herzschlag gebunden — der Rückweg aus Entscheidung 6.
* Die Abnahme umfasst auch die Gegenprobe, dass repoweit nichts an
  `set/Z1HeatCurveTargetHighTemp` schreibt.

**Von hier aus nachgesehen (2026-08-20 19:58, über die simple-api):** Alle fünf
Werte liegen im Broker — `panasonic_heat_pump/notbetrieb/` mit 34 / 26 / −10 /
15 und `panasonic_heat_pump2/notbetrieb/DHWTemp` = 48. Die Kreuzzuordnung stimmt
(`TargetHigh` = 34 am kalten Punkt). Der Knopf an H1 wäre damit freigegeben,
sobald die Firmware dort läuft.

### Was noch fehlt

1. **Etappe 5 — Lauf an Stufe 1 bei stehender Anlage.** Zum ersten Mal wird
   GRÜN erwartet. Voraussetzung: Die Firmware dieses Branches muss erst auf H1,
   dort läuft noch 3.11.0 ohne den Endpunkt (`/notbetrieb` antwortet mit 404,
   am 2026-08-20 nachgesehen).
   **Neu zu beachten:** Der Wartungsschalter gehört vor dem Lauf gesetzt, sonst
   holt der Re-Assert die Anlage mitten im Versuch zurück (Abschnitt 7).
   Dabei entsteht das **Kurvenfoto fürs Handbuch**: Sobald die Anlage im
   Kurvenbetrieb mit 34 °C bei −10 °C und 26 °C bei +15 °C steht, ist der Moment
   für das Foto vom Bedienpanel (siehe `pictures/IMG_4887.png` als Beispiel —
   das zeigt allerdings die *Werks*kurve).
   Aufräumen heißt jetzt: Wartung aus — den Rest macht der Re-Assert von allein,
   und genau das ist der Nachweis für Testplan-Punkt 6.
2. **Etappe 6 — Wiederholung mit abgeschaltetem MQTT-Broker.** Der eigentliche
   Nachweis. Ungemessen ist bis heute, ob die Firmware ohne erreichbaren Broker
   sauber weiterläuft (die Reconnect-Logik hat einen Backoff, aber gemessen ist
   das nicht).
3. **Etappe 7 — Rückkehr prüfen, Doku, Release 3.12.0.** Die Nacharbeiten
   stehen in Abschnitt 9. Dazu gehört auch `NOTBETRIEB.md` §7 im Nachbarprojekt:
   Es beschreibt noch den Handweg am Bedienterminal statt den Knopf (dort als
   TODO 1.6 eingetragen).

### Zustand der Geräte

Gerät | Stand
:--- | :---
H1 (192.168.2.120) | 3.11.0, **unverändert**, Direktbetrieb, Kurve 34/26 bei −10/+15; `set/Heatpump` = 0, die Anlage steht |
H2 (192.168.2.122) | 3.11.0, **unverändert** |
Prüfstand (192.168.2.197) | **stromlos** (2026-08-20 abends nicht erreichbar); Firmware dieses Branches, Rolle Heizen |

Die Testwerte unter dem Prefix `panasonic_heat_pump_test` bleiben im ioBroker —
sie stören nichts und sparen beim Wiedereinstieg einen Schritt.

### Wiedereinstieg prüfen

```bash
git switch notbetrieb-web
c++ -std=c++17 -O2 -Wall -o /tmp/nb_test test/notbetrieb_test.cpp && /tmp/nb_test

# Liegen die Notbetriebswerte im Broker? (Knopf bleibt sonst gesperrt)
curl -s "http://192.168.2.147:8087/getBulk/\
mqtt.0.panasonic_heat_pump.notbetrieb.Z1HeatCurveTargetHighTemp,\
mqtt.0.panasonic_heat_pump.notbetrieb.Z1HeatCurveTargetLowTemp,\
mqtt.0.panasonic_heat_pump.notbetrieb.Z1HeatCurveOutsideLowTemp,\
mqtt.0.panasonic_heat_pump.notbetrieb.Z1HeatCurveOutsideHighTemp,\
mqtt.0.panasonic_heat_pump2.notbetrieb.DHWTemp"

curl http://192.168.2.197/notbetrieb/status   # nur wenn der Prüfstand Strom hat
```

**Die Versionsnummer steht bewusst noch auf 3.11.0.** Eine Firmware mit Knopf,
aber ohne Nachweis an der Anlage ist kein Release; 3.12.0 wird in Etappe 7
gesetzt, zusammen mit dem Changelog.

**Eine lokale Besonderheit:** Der USB-Port des Prüfstands hat sich auf
`/dev/cu.usbserial-1110` geändert. Das steht in `platformio_user_env.ini`
(gitignored) und ist auf einem anderen Rechner erneut anzupassen.
