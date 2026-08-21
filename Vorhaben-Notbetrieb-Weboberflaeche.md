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

**Stand dieser Datei:** 2026-08-21 nachts. **Der Knopf funktioniert an der
Anlage** — Etappe 5 ist gefahren und grün: gesperrt im Kühlbetrieb, GRÜN nach
57 s im Heizbetrieb, Rückkehr durch den Re-Assert von allein (Protokoll in
Abschnitt 10). Alle Grundsatzfragen sind entschieden (Abschnitt 5), alle drei
Messungen beantwortet (Abschnitt 6). Der Messlauf zu M1 hat zusätzlich einen
Fehler in der Kurvenspiegelung aufgedeckt; Doku und `kurven_sync.py` sind
korrigiert (Abschnitt 6a) und im Kurvenbetrieb an der Anlage bestätigt. **Auch Etappe 6 ist gefahren:** Der Knopf schaltet mit
abgeschaltetem Broker, die Firmware übersteht den Ausfall und holt sich in 52 s
von allein zurück. **Und der Warmwasserknopf an H2 ist belegt** — GRÜN nach 24 s
im Kühlbetrieb. Beide Stufen tragen die Firmware dieses Branches, Etappe 7 (Doku,
Release 3.12.0) ist erledigt.

**Fortsetzung 3.13.0, seit dem 2026-08-21:** Zwei der Folgethemen aus Abschnitt 9
sind gebaut — die Weboberfläche meldet jetzt, wenn die Hausteuerung ausgefallen
ist, und der Notbetriebsknopf ist blau statt rot. Dazu **Etappe B**: Sie
unterscheidet zwei Ausfälle, den weggefallenen Broker und die stumme Steuerung
(Broker da, aber der 5-Minuten-Re-Assert bleibt aus). Einzelheiten,
Entscheidungen und der Prüfplan stehen in **Abschnitt 11**; der Nachweis am Gerät
ist am 2026-08-21 am Prüfstand erbracht — alle sechs Lagen, ohne Eingriff an der
Anlage. **Dabei ist ein Fehler in Etappe B aufgefallen und behoben worden**
(eine Log-Zeile im MQTT-Callback zerstörte das eintreffende Kommando).
**Beide Stufen laufen seit dem 2026-08-21 nachmittags auf 3.13.0**, Abnahme
grün, und die Gegenprobe im Normalbetrieb zeigt über drei Re-Assert-Takte
hinweg keinen Fehlalarm.

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
1. Betriebsart auf Heizen | `OperationMode` (SET9) = 0 (Heat only) | `Operating_Mode_State` (TOP4) = 0
2. Betriebsart auf Kurve | `HeatingMode` (SET35) = 0 | `Heating_Mode` (TOP76) = 0
3. Vorlauf bei kalt | `Z1HeatCurveTargetHighTemp` (SET27) | `Z1_Heat_Curve_Target_High_Temp` (TOP29)
4. Vorlauf bei warm | `Z1HeatCurveTargetLowTemp` (SET28) | `Z1_Heat_Curve_Target_Low_Temp` (TOP30)
5. Untere Außentemperatur | `Z1HeatCurveOutsideLowTemp` (SET29) | `Z1_Heat_Curve_Outside_Low_Temp` (TOP32)
6. Obere Außentemperatur | `Z1HeatCurveOutsideHighTemp` (SET30) | `Z1_Heat_Curve_Outside_High_Temp` (TOP31)
7. Anlage einschalten | `Heatpump` (SET1) = 1 | `Heatpump_State` (TOP0) = 1

Die Schritte 3–6 sind am 2026-08-20 im Kurvenbetrieb belegt (M1a): alle vier
Werte in einem Sammelfenster gesendet, alle vier binnen 15 s zurückgelesen. Zu
Schritt 3 und 4 siehe Abschnitt 6a — **`TargetHigh` gehört zur *unteren*
Außentemperatur**, ist also der Vorlauf bei Kälte. Bis zum 2026-08-20 stand das
in der Doku falsch herum.

### Schritt 1 kam am 2026-08-20 dazu — sonst kühlt der Notbetrieb

Die Folge fasste die Betriebsart ursprünglich nicht an. Der Ist-Zustand von H1
an diesem Abend zeigt, warum das nicht trägt: `Operating_Mode_State` (TOP4)
stand auf **1 = Cool only**. Die Kaskadensteuerung führt die Betriebsart selbst
über `set/OperationMode` — im 5-min-Re-Assert nachgesehen — und stellt sie im
Sommer auf Kühlen. Fällt der ioBroker in so einem Moment aus, bleibt genau
dieser Zustand stehen: Der Knopf hätte alle sechs Schritte bestätigt, GRÜN
gemeldet und eine Anlage eingeschaltet, die kühlt. Für einen Knopf, dessen
einziger Zweck es ist, das Auskühlen zu verhindern, ist das der schlechteste
denkbare Fehler — und er trifft gerade die Übergangszeit, in der ein Ausfall
ebenso wahrscheinlich ist wie im Januar.

Die Warmwasserseite macht es seit jeher richtig (`OperationMode` = 3); die
Heizenseite zog nach. Der Schritt steht **vor** dem Moduswechsel: Ob ein
Wechsel der Betriebsart die Kurvenpunkte ebenfalls anfasst, ist nicht gemessen
— an erster Stelle kann er keinen bereits geschriebenen Wert mehr zerstören.

Preis: ein Schritt mehr, rund 8 s längere Laufzeit, Gesamtdeckel 140 statt
120 s.

**Am 2026-08-20 abends an der Anlage gescheitert — und das ist der bisher
wichtigste Befund des Vorhabens.** Der erste echte Lauf an H1 endete um 21:31:55
mit ROT in Schritt 1, nach dem vollen Schritt-Timeout von 20 s. Die
Gegenmessung unmittelbar danach trennt Firmware und Wärmepumpe sauber:

* **Die Firmware hat gesendet.** Ein einzelnes `set/OperationMode 0` über MQTT
  erscheint im Log der Bridge als `<SUB> SET9 OperationMode: 0` (21:33:16) — es
  ist also durch Bereichsprüfung, Maskenmerge und Telegramm gegangen.
* **Die Wärmepumpe hat es verworfen.** 40 s später, über sechs Abfragezyklen
  hinweg: `Operating_Mode_State` (TOP4) unverändert 1 = Cool, und
  `Heat_Cool_SW_State` (TOP101) — der *echte* Ist-Zustand aus Byte 110, nicht
  der zuletzt kommandierte — ebenfalls Cool. Dasselbe stillschweigende
  Verwerfen wie bei `Z1HeatRequestTemperature` im Kurvenbetrieb (M1c).

**Beantwortet noch am selben Abend durch den Owner:** Der externe Schalter
(KNX) gibt den Modus vor — **steht die Anlage auf Kühlen, nimmt sie nur
Kühlmodi an.** Damit ist Hypothese (a) unten die richtige, die Messung zu (b)
erübrigt sich, und der Notbetrieb Heizen ist im Kühlbetrieb über MQTT
grundsätzlich nicht schaltbar. Folge für den Bau: Der Knopf gehört gesperrt,
solange `Heat_Cool_SW_State` (TOP101) auf Cool steht, mit Klartext auf der
Seite und dem KNX-Taster in der Offline-Anleitung. Die Tabelle bleibt als Beleg
stehen, wie der Befund entstanden ist:

Hypothese | Prüfung | Folge, falls sie zutrifft
:--- | :--- | :---
**(a)** Der KNX-Aktor gibt Heizen/Kühlen vor; im Kühlbetrieb nimmt die Anlage kein „Heat only" per MQTT an | Heiz/Kühl-Schalter auf Heizen stellen, `SET9` wiederholen | Der Notbetrieb Heizen ist im Kühlbetrieb **grundsätzlich nicht schaltbar**. Der Knopf gehört dann gesperrt, solange TOP101 auf Cool steht — mit Klartext, statt jemanden in ein ROT ohne Erklärung laufen zu lassen. Der KNX-Taster gehört in die Offline-Anleitung.
**(b)** Die Anlage stand aus (`Heatpump_State` = 0) und nimmt im Aus-Zustand keine Betriebsartänderung an | `Heatpump` = 1 senden, dann `SET9` wiederholen | Ein Fehler in der **Schrittfolge**: `Heatpump` = 1 gehört dann nach vorn statt ans Ende. Der Knopf bliebe voll funktionsfähig.

Für (b) spricht, dass die Gegenprobe an H2 (M3, `OperationMode` = 3 aus dem
Kühlbetrieb heraus) an einer **laufenden** Stufe gemessen wurde — H2 macht
Warmwasser und ist eingeschaltet. Für (a) spricht Entscheidung 4: Die
Betriebsart ist die eine Freigabestufe, die nicht über MQTT läuft.

### Gebaut am 2026-08-21: die Sperre

Der Knopf der Rolle Heizen ist freigegeben, **solange `Heat_Cool_SW_State`
(TOP101) sich sauber als 0 liest** — und sonst nie. Owner-Entscheidung: Alles
andere gilt als „nicht Heizen", und zwar mit einer Regel für vier Fälle:

Gelesen | bedeutet | Knopf
:--- | :--- | :---
`0` | Heizen | **frei**
`1` | Kühlen | gesperrt
`2` | unknown (Rohwert b11) | gesperrt
`-1` | Feld leer geliefert | gesperrt
`` (leer) | TOP101 nie empfangen | gesperrt

Der letzte Fall trägt die Strenge: Ohne Rückmeldung der Wärmepumpe erreicht sie
auch kein Kommando. Ein Knopf, der dann zum Drücken einlädt, verspricht etwas,
das er nicht halten kann. Der Preis ist bezifferbar — **am Prüfstand ohne
Wärmepumpe ist der Knopf dauerhaft gesperrt**, der Fehlerpfad ist dort nur noch
über den Hosttest zu prüfen.

Drei Eigenschaften, die zur Sperre gehören:

* **Die Seite gibt sich von selbst frei.** Die Statusroute liefert den
  Sperrgrund als fünftes Feld mit; die Seite fragt sie ohnehin alle zwei
  Sekunden ab. Ein Neuladen von Hand wäre hier eine Falle: TOP101 folgt dem
  KNX-Schalter erst nach bis zu 7,7 s (gemessen 2026-08-16) — wer sofort neu
  lädt, sähe die Sperre ein zweites Mal und hielte den Schalter für wirkungslos.
* **Auch mitten im Lauf wird abgebrochen**, sobald die Anlage ausdrücklich `1`
  meldet. Sonst schaltete die Folge am Ende eine kühlende Anlage ein. Nur die
  klare 1 bricht ab — ein einzelner Aussetzer darf einen sauber laufenden
  Vorgang nicht zerreißen und die Anlage halb geschaltet stehen lassen.
* **Der POST-Handler prüft dieselbe Sperre.** Eine Oberfläche, die nur den Knopf
  versteckt, ist keine Sperre: `/notbetrieb/start` lässt sich auch ohne die
  Seite absetzen, und zwischen Seitenaufbau und Klick können Minuten liegen.

**Stufe 2 ist nicht betroffen.** `OperationMode` = 3 trägt im Kühlbetrieb (M3),
und genau dafür ist der Warmwasserknopf gebaut.

**Was die Sperre nicht kann:** TOP101 ist der zuletzt empfangene Wert. Reißt die
serielle Verbindung zur Wärmepumpe ab, altert er stumm, und die Sperre merkt es
nicht. Ein Lauf, der in dieser Lage gestartet wird, scheitert im ersten
Schritt — dasselbe Ergebnis wie vorher, nur ohne den Gewinn.

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
der sieben Kommandos hintereinander absetzt, packt sie alle in ein Telegramm
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
realistischen 6-s-Antworten braucht 42 s (im Hosttest gemessen; sieben Schritte,
seit die Betriebsart mitgeschaltet wird).

**Der Gesamtdeckel ist abgeleitet, nicht frei gewählt** (Umsetzung 2026-08-20):
Schrittzahl × Schritt-Timeout, also 140 s für Heizen und 60 s für Warmwasser.
Der ursprüngliche Entwurf nannte pauschal 60 s — das wäre bei sechs Schritten
inkonsistent gewesen: Schon zwei Schritte im Timeout hätten den Deckel gerissen,
und ob ein Lauf ROT wird, hinge davon ab, welche der beiden Regeln zufällig
zuerst greift. So bleibt der Deckel das, was er sein soll: ein Notausgang, falls
der Automat hängt, nicht der normale Weg zu ROT. (Die 120 s des ersten
Entwurfs wurden zu 140 s, als die Betriebsart als siebter Schritt dazukam — die
Regel blieb dieselbe, nur die Schrittzahl änderte sich.)

**Eine Regel kam beim Bauen dazu: die Mindestwartezeit.** Ein Schritt gilt
frühestens 8 s nach seinem Kommando als bestätigt. Ohne das könnte ein
*veralteter* Rücklesewert einen Schritt sofort abhaken — und der gefährliche
Fall steckt in der Schrittfolge selbst: Das Umschalten setzt die Kurvenpunkte
auf die Werksvorgaben zurück. Trüge `actual_data` beim Kurvenschritt noch den
Kurvenwert von vorher, gälte er als erledigt, und der Werks-Reset überschriebe
ihn danach. GRÜN, und die Anlage führe die Werkskurve. Der Preis: Ein
Heizen-Lauf dauert rund 56 statt 42 s.

**Und eine Falle, die am Prüfstand sichtbar wurde:** `actual_data[]` wird über
den **Zeilenindex** adressiert, nicht über die TOP-Nummer — die Nummerierung hat
Lücken und reicht bis 104 bei 92 Zeilen. Dafür gibt es jetzt
`state_topic_index()` in `decode.cpp`. Dazu kommt, dass ein noch nie
empfangenes TOP als leerer Text dasteht: `atoi("")` ist 0, und der erste Schritt
der Heizen-Folge hat den Sollwert 0. Ein naiver Vergleich hätte GRÜN gemeldet,
ohne dass die Wärmepumpe je geantwortet hätte. Beides ist im Hosttest
abgesichert und am Prüfstand belegt (dort läuft der Fehlerpfad, weil keine
Wärmepumpe antwortet).

**Ein Ergebnis verfällt nach 15 Minuten** (2026-08-21). GRÜN und ROT blieben
vorher stehen, bis jemand erneut drückte — wer die Seite am nächsten Tag
öffnete, sah das ROT von gestern und musste raten, ob gerade etwas schiefgeht.
Jetzt fällt die Anzeige nach 15 Minuten auf „bereit" zurück und der Knopf steht
wieder da; im MQTT-Log bleibt der Lauf vollständig nachlesbar. 15 Minuten sind
länger als jeder Lauf (Deckel 140 s) und kurz genug, dass niemand ein fremdes
Ergebnis für seines hält.

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
  Keine Rückfrage — aber ein POST-Formular statt eines Links, damit ihn niemand
  aus Versehen für den Browser mitlädt. **Seit dem 2026-08-21 hat er einen
  eigenen Zugang** (Benutzer `notbetrieb`), nicht mehr das Passwort von
  `/firmware`: Er gehört mit Passwort in die Notfallanleitung, und dasselbe
  Blatt darf nicht auch den Firmware-Upload öffnen.
* **Die Bedienung ist für Laien.** „Notbetrieb ein" statt „`HeatingMode` auf 0". Die
  Passwörter und die Schritt-für-Schritt-Anleitung stehen offline bereit.
* **Ein Neustart ohne Broker sperrt den Knopf.** Bewusst in Kauf genommen
  (Abschnitt 3), aber die Seite muss es sagen statt stumm ROT zu zeigen.
* **Der Rückweg ist seit dem 2026-08-20 scharf und trifft die Testläufe.**
  Läuft die Kaskadensteuerung, holt der 5-min-Re-Assert die Anlage mit
  `set/HeatingMode` = 1 aus dem Notbetrieb zurück — im Ernstfall gewollt, im
  Versuch aber ein Rennen: Der Knopf-Lauf braucht rund 56 s, danach bleiben
  höchstens noch gut vier Minuten für Kurvenfoto und Kontrolle. Schlimmer ist
  der Treffer *während* des Laufs: Fällt der Re-Assert zwischen Schritt 1 und
  Schritt 7, steht die Betriebsart wieder auf Direktbetrieb, während der Automat
  noch Kurvenpunkte schreibt.

  **Der Weg dagegen ist ein Ruhefenster, kein Schalter** (2026-08-20). Der
  naheliegende Gedanke — den Verteiler für die Dauer des Versuchs stilllegen —
  ist geprüft und verworfen: Der Wartungsmodus der Kaskade schaltet auf seiner
  AN-Flanke zuerst beide Wärmepumpen aus. Für einen Lauf, der die Anlage
  *einschalten* soll, ist das das Gegenteil dessen, was gebraucht wird.
  Einzelheiten in [`Auftrag-Wartungsschalter-NodeRED.md`](Auftrag-Wartungsschalter-NodeRED.md).

  Stattdessen `~/nodered-flows/testfenster.py`: Es liest den Takt aus den
  Zeitstempeln der set-Datenpunkte (jeder Deploy verschiebt ihn), wartet mit
  `--warte` auf den Beginn eines ausreichend langen Fensters und bewacht den
  Lauf mit `--wache`; Exit-Code 2, wenn jemand dazwischenschreibt. **Wachzeit
  nie größer als die verlangte Ruhe wählen** — `--warte 120 --wache 240` ist ein
  Widerspruch und meldet zwangsläufig GESTÖRT. Für den Heizen-Lauf samt Foto
  passt `--warte 240 --wache 200`.

  Zurück kommt die Anlage von allein: Der nächste Re-Assert holt sie in den
  Direktbetrieb, und das ist zugleich der Nachweis für Testplan-Punkt 6.

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

**Am 2026-08-21 erledigt** (Etappe 7), bis auf die beiden Punkte außerhalb
dieses Repos, die unten ausdrücklich als offen markiert sind.

* ✔ [`README.md`](README.md) — eigener Abschnitt „Der Notbetrieb ist ein Knopf
  im Browser (3.12.0)", dazu `notbetrieb.h`/`notbetrieb.cpp` in der Dateitabelle.
* ✔ [`src/version.h`](src/version.h) — Changelog zu 3.12.0 mit Problem, Regeln,
  Freigabebedingung, eigenem Zugang und den vier Läufen an der Anlage.
* ✔ [`MQTT-Topics.md`](MQTT-Topics.md) — der neue Zweig `<prefix>/notbetrieb/`,
  die Sperre über TOP101 und der eigene Zugang.
* **OFFEN:** Die Offline-Anleitung der Familie — Schrittfolge, IP-Adressen, Passwort, der
  KNX-Taster für die Kompressorfreigabe, und der Hinweis, dass im Kurvenbetrieb
  „+1 am Bedienpanel" die ganze Kurve um 1 K verschiebt. **Dazu der obere
  Kurvenpunkt mit seiner Zahl:** nach dem Umschalten auf Kurve ist
  `TargetHigh` auf 34 °C zu setzen, sonst bleibt die Werksvorgabe 55 °C stehen
  (Abschnitt 6a).

  **Und die Reihenfolge ist dabei zwingend (Owner-Befund 2026-08-21):** Das
  Kurvenmenü am Bedienpanel ist **nur bei ausgeschalteter Wärmepumpe
  erreichbar**. Der Handweg lautet also: Anlage aus → auf Kurve umschalten und
  `TargetHigh` setzen → einschalten. Wer zuerst einschaltet, kommt an die Kurve
  nicht mehr heran, ohne die Anlage wieder auszuschalten.

  Für den Knopf ist das ohne Folgen — er schaltet die Anlage als **letzten**
  Schritt ein, aus einem anderen Grund (Abschnitt 2), und trifft damit dieselbe
  Reihenfolge. Es kostet aber das **Kurvenfoto**: Nach einem GRÜN-Lauf läuft die
  Anlage, das Menü bleibt zu. Das Foto braucht deshalb einen eigenen Termin —
  Kurvenbetrieb herstellen, dann den **Wartungsmodus** einschalten (der schaltet
  beide Stufen aus und legt den Re-Assert still, hier also genau richtig),
  fotografieren, Wartungsmodus wieder aus. Für einen Lauf, der die Anlage
  einschalten soll, bleibt er das falsche Werkzeug
  ([`Auftrag-Wartungsschalter-NodeRED.md`](Auftrag-Wartungsschalter-NodeRED.md)).
* ✔ **Erledigt:** `NOTBETRIEB.md` im Node-RED-Projekt — §7a (Knopf) vor §7b
  (Handweg) steht seit dem 2026-08-21. Am selben Tag mit 3.13.0 nachgezogen
  (dort `928eb69`): §0 nennt die Bridge-Seite als **besseren Ausfalltest** als
  die Familien-App (die lädt bei einer bloß stummen Steuerung und zeigt
  eingefrorene Werte), §2 führt die Bridge-Seiten als zweite verlässliche
  Anzeige, §7a sagt **blauer** Knopf, und §9 trennt die **zwei Herzschläge**
  auseinander — den der Kaskadenregelung (Bedingung für die Rückkehr-Zeile) und
  den der Firmware (Ausfallerkennung).
* ✔ **Erledigt 2026-08-21:** Die Gleichsetzung von
  `Z1HeatRequestTemperature` (SET5) und `Z1HeatCurveTargetHighTemp` (SET27) gilt
  nur im Direktbetrieb. Fundstellen: [`MQTT-Topics.md:468`](MQTT-Topics.md#L468)
  und [`MQTT-Topics.md:537`](MQTT-Topics.md#L537), Fußnote ² in
  [`SET-TOP-Zuordnung.md:125`](SET-TOP-Zuordnung.md#L125), der
  TargetHigh-Abschnitt in [`test/README.md:699`](test/README.md#L699).
* ✔ **Erledigt am 2026-08-20:** die Kurvenpaarung in `MQTT-Topics.md` und
  `kurven_sync.py` — Abschnitt 6a führt die Stellen einzeln auf. Am 2026-08-21
  im Kurvenbetrieb an der Anlage bestätigt (`Main_Target_Temp` 26 °C bei 15 °C
  außen).
* ✔ **Erledigt 2026-08-21 im Repo:** Changelog und `SET-TOP-Zuordnung.md`
  sagten sinngemäß „damit ist der Notbetrieb vollständig fernschaltbar". Das
  stimmt nur, solange ein Broker erreichbar ist; beide Stellen sind präzisiert,
  der Changelog zu 3.12.0 greift es auf. **OFFEN bleibt der Text des
  GitHub-Release zu 3.11.0** — der lässt sich nur dort ändern.

**ERLEDIGT in 3.13.0 — Abschnitt 11 führt es aus.** Der Text unten steht
unverändert, weil er die Ausgangslage beschreibt.

**Folgethema, nicht Teil dieses Vorhabens — niemand merkt den Ausfall.**
Owner-Beobachtung 2026-08-21, mitten in Etappe 6: Während der Broker weg war,
heizte die Wärmepumpe einfach weiter, mit dem zuletzt gesetzten Sollwert. Kein
Alarm, kein Hinweis, nichts. Der Ausfall wirkt sich erst mit Verzögerung aus —
im Sommer über Tage, im Januar über Stunden, wenn der Sollwert der fallenden
Außentemperatur nicht mehr nachgeführt wird.

Damit steht der Knopf auf einem stillen Fundament: Er funktioniert, aber jemand
muss auf die Idee kommen, ihn zu suchen. Nachgesehen und bestätigt: **Die
Weboberfläche der Bridge zeigt nirgends an, dass die Verbindung zur Hausteuerung
weg ist** — Startseite, Topic-Tabelle, sonst nichts. Selbst wer gezielt
nachschaut, sieht es nicht. Die Firmware weiß es (der Reconnect läuft im Backoff
ins Leere), sie sagt es nur niemandem, außer im MQTT-Log — und das geht in genau
dieser Lage per Definition ins Leere.

Die naheliegende Form wäre klein: eine Zeile auf der Startseite und auf der
Notbetriebsseite, „Hausteuerung: seit 14 Minuten nicht erreichbar", aus dem, was
die Firmware ohnehin kennt. Mit einer Karenz von einigen Minuten, damit ein
WLAN-Wackler keinen Fehlalarm auslöst. Wer dann die Seite öffnet, weiß sofort,
ob er den Knopf braucht.

**ERLEDIGT in 3.13.0 — der Knopf ist blau.**

**Kleinigkeit, aber dieselbe Richtung — Rot heißt auf der Seite zweierlei.** Der
Knopf ist rot eingefärbt („drück mich"), das Ergebnisfeld ROT bedeutet „hat
nicht geklappt". Am 2026-08-21 hat das prompt zu einer Verwechslung geführt. Für
eine Seite, deren ganze Rückmeldung aus einer Ampel besteht, ist das die falsche
Farbsprache; der Knopf sollte blau werden. Zwei Zeichen im Quelltext, noch nicht
entschieden.

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

## 10. Stand der Umsetzung — 2026-08-21, Etappen 5 und 6 erledigt

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
5a | Erster Lauf an H1: ROT in Schritt 1, Ursache getrennt | `6deeeaf`, `07431f8`
5b | **Die Sperre über die Betriebsart** samt Anzeigeverfall | `c22c9a5`
5c | **Etappe 5 an der Anlage: Sperre belegt, GRÜN nach 57 s** | `a6fdafd`
6 | **Etappe 6: GRÜN ohne Broker, Rückkehr nach 52 s** | `a59955a`
6a | **Der Warmwasserknopf an H2: GRÜN nach 24 s** | `4676e0e`

Die Bausteine A–D sind vollständig gebaut und am Prüfstand geprüft — dort lief
mangels Wärmepumpe der **Fehler**pfad (ROT nach 20 s), und genau das war der
Zweck. Alle zehn Envs bauen, der Hosttest steht bei 159 Zusicherungen.

Mit der Sperre (Abschnitt 2, gebaut am 2026-08-21) ist der Prüfstand als
Werkzeug für den Knopf ausgeschieden: Ohne Wärmepumpe liefert er kein TOP101,
und ohne TOP101 bleibt der Knopf gesperrt. Die Regeln sind dafür vollständig im
Hosttest abgebildet — Freigabe, Abbruch im Lauf und Anzeigeverfall stehen dort
mit je einem eigenen Abschnitt.

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

1. **Etappe 5 — ERLEDIGT am 2026-08-21. Beide Läufe gefahren, beide grün.**
   Einzelheiten im Protokoll unten.
2. **Etappe 6 — ERLEDIGT am 2026-08-21.** Der Knopf schaltet ohne Broker, die
   Firmware übersteht den Ausfall, und nach GRÜN kommt Wärme. Protokoll unten.
3. **Etappe 7 — im Repo erledigt am 2026-08-21.** Version 3.12.0, Changelog,
   README, `MQTT-Topics.md`, `SET-TOP-Zuordnung.md` und die Korrekturen aus
   Abschnitt 9 stehen. Die Rückkehr ist in beiden Läufen geprüft (Etappe 5 und 6).

   **Neu dazugekommen:** Der Notbetriebsknopf hat einen **eigenen Zugang**
   (Benutzer `notbetrieb`, Passwort als Build-Flag `HEISHA_NOTBETRIEB_PASSWORD`
   aus `platformio_user_env.ini`) statt des OTA-Passworts — Owner-Entscheidung
   2026-08-21. Grund: Der Knopf steht mit Passwort in der ausgedruckten
   Notfallanleitung; dasselbe Blatt hätte sonst auch den Firmware-Upload und die
   MQTT-Zugangsdaten geöffnet. Abschnitt 7 („Der Zugangsschutz ist dasselbe
   Passwort wie für `/firmware`") ist damit überholt.

   **Was noch fehlt, liegt außerhalb dieses Repos:** die Offline-Anleitung der
   Familie und `NOTBETRIEB.md` §7 im Node-RED-Projekt (beschreibt noch den
   Handweg am Bedienterminal als einzigen Weg), dazu der Text des GitHub-Release
   zu 3.11.0.

### Etappe 5 — das Protokoll vom 2026-08-21

Zwei Läufe an H1, nacheinander, mit einer Pause dazwischen, in der der Owner den
KNX-Schalter umgelegt hat. Firmware dieses Branches per OTA auf H1 (ESP32-S3,
Env `heishamon_esp32_h1_ota`); die Abnahme gegen die Baseline von 00:52 zeigte
**keine einzige Abweichung** in 92 Zeilen.

**Lauf A — der Sperr-Nachweis, Anlage auf Kühlen.** Kein Kommando ging an die
Wärmepumpe.

* `/notbetrieb/status` = `0;1;7;0;2` — bereit, **alle vier Kurvenwerte binnen
  Sekunden nach dem Neustart wieder da** (Broker-Wiedereinspielung, `fehlend`
  = 0), gesperrt wegen der Betriebsart.
* Die Seite trägt den Knopf mit `display:none` und zeigt stattdessen den
  Klartext.
* **Der POST auf `/notbetrieb/start` wurde abgewiesen** (303, „nicht bereit"),
  Log: `Notbetrieb abgelehnt: die Anlage steht nicht auf Heizen (TOP101)`.
  TOP4, TOP101 und TOP0 standen 30 s später unverändert — die Sperre wirkt
  serverseitig, nicht nur in der Oberfläche.

**Die Pause — das Entsperren geschieht von selbst.** Sekundengenau
mitgeschrieben, in beide Richtungen:

Zeit | Status | Auslöser
:--- | :--- | :---
00:59:08 | `0;1;7;0;2` | KNX steht auf Kühlen
**00:59:10** | `0;1;7;0;0` | KNX auf Heizen — **frei ohne Neuladen**
**01:14:52** | `2;8;7;0;2` | KNX zurück auf Kühlen — **wieder gesperrt**
**01:17:39** | `0;1;7;0;2` | 15 min nach GRÜN — **Anzeige verfallen**

Die letzte Zeile ist der Nachweis für den Anzeigeverfall: GRÜN fiel um 01:02:38,
die Anzeige stand um 01:17:34 noch und war um 01:17:39 weg — bei einer Abtastung
alle 5 s ist das der berechnete Zeitpunkt 01:17:38. Der Knopf stand danach
wieder da, nur eben gesperrt, weil die Anlage inzwischen auf Kühlen steht.

**Lauf B — der GRÜN-Lauf, Anlage auf Heizen.** Im Ruhefenster (`testfenster.py
--warte 240`, Fenster 4:43 min ab 01:01:26), bewacht mit `--wache 200`.

```
01:01:41  +0s   Schritt 1 von 7      01:02:13  +32s  Schritt 5
01:01:49  +8s   Schritt 2            01:02:22  +41s  Schritt 6
01:01:57  +16s  Schritt 3            01:02:30  +49s  Schritt 7
01:02:05  +24s  Schritt 4            01:02:38  +57s  GRUEN
```

**GRÜN nach 57 s.** Jeder Schritt genau 8 s — das ist die Mindestwartezeit, nicht
die Antwortzeit der Wärmepumpe. Sie hat also jeden Schritt schon innerhalb
dieser acht Sekunden zurückgemeldet; die 56 s aus dem Entwurf sind damit auf die
Sekunde bestätigt.

TOP | vor dem Lauf | nach dem Lauf
:--- | ---: | ---:
`Heating_Mode` (76) | 1 = Direkt | **0 = Comp. Curve**
`Z1_Heat_Curve_Target_High_Temp` (29) | **20** | **34**
`Z1_Heat_Curve_Target_Low_Temp` (30) | 26 | 26
`Z1_Heat_Curve_Outside_Low_Temp` (32) | −10 | −10
`Z1_Heat_Curve_Outside_High_Temp` (31) | 15 | 15
`Heatpump_State` (0) | 0 = aus | **1 = An**
`Main_Target_Temp` (7) | 20 | **26**

**Die 34 ist der tragende Beleg.** Diesen Wert schreibt sonst niemand:
`kurven_sync.py` lässt ihn im Direktbetrieb bewusst aus, und vor dem Lauf stand
dort eine 20. Er kann nur aus dem RAM der Firmware gekommen sein — über den
Notbetriebskanal aus Abschnitt 3, der damit end-to-end belegt ist.

**Die Wache bestätigt den Lauf: SAUBER.** In 3:20 min hat niemand die
Betriebsart, eine Kurve oder Ein/Aus angefasst; die beiden einzigen
Schreibvorgänge waren `QuietMode` an beiden Stufen, vom Werkzeug selbst als
unkritisch eingestuft. Das Umschalten kam also vom Knopf, nicht von der
Kaskadensteuerung.

**Ein Zusatzbefund, der nicht gesucht war:** `Main_Target_Temp` stand im
Kurvenbetrieb auf **26 °C bei 15 °C Außentemperatur** — genau der Punkt, den die
am 2026-08-20 korrigierte Kurvenpaarung vorhersagt (TargetLow gilt am oberen
Außenpunkt). Abschnitt 6a ist damit an der laufenden Anlage im Kurvenbetrieb
bestätigt, nicht nur aus der Werkskurve abgeleitet.

**Der Rückweg trägt — Testplan-Punkt 6 ist belegt.** Neun Sekunden nach dem
Re-Assert um 01:06:24 war die Anlage von allein zurück:

```
01:06:28  Heating_Mode=0  Heatpump_State=1
01:06:33  Heating_Mode=1  Heatpump_State=0
```

**Der Werks-Reset beim Zurückschalten, ein drittes Mal reproduziert.** TOP27,
TOP29, TOP30 und TOP42 sprangen auf 35, TOP32 auf −5, TOP28 und TOP72 auf 10 —
genau das Muster aus M2. Die Sollwerte holte der nächste Re-Assert um 01:11:24
von allein zurück (01:11:35: Heat/Cool/Water wieder je 20), die Kurvenpunkte
stellte `kurven_sync.py` wieder her. **Endkontrolle:** von 92 Zeilen wichen
zuletzt nur noch die beiden Betriebsart-Zeilen (KNX stand noch auf Heizen) und
drei laufende Messwerte ab. Kein Rest.

**Was Lauf B nicht belegt:** Schritt 1 (`OperationMode` = 0) fand seinen
Sollwert bereits vor — die Kaskadensteuerung war dem KNX-Wechsel von selbst
gefolgt, TOP4 stand schon auf 0. Ob die Wärmepumpe *unser* Kommando angenommen
hat oder der Wert ohnehin stand, ist aus diesem Lauf nicht zu trennen. Die
Beweislast tragen die Schritte 2–7, die alle nachweislich anders standen.

### Etappe 6 — der Lauf ohne Broker, 2026-08-21 nachts

**Der eigentliche Nachweis, und er ist erbracht.** Aufbau: KNX auf Heizen,
Kompressor freigegeben, die Anlage lief bereits im Direktbetrieb mit 26 Hz — die
realistischere Lage als Etappe 5, denn wenn der ioBroker ausfällt, läuft die
Anlage meistens. Abgeschaltet wurde die Adapterinstanz `mqtt.0` (Variante A: der
ioBroker selbst lief weiter, damit die simple-api als Vergleichsquelle erhalten
blieb).

**Erst der Beweis, dass der Broker wirklich weg war.** Ein Porttest auf 1883
taugt dafür nicht — ein Docker-Port-Mapping nimmt die TCP-Verbindung auch dann
an, wenn dahinter niemand horcht; genau darauf bin ich zuerst hereingefallen.
Tragfähig sind drei andere Belege: `system.adapter.mqtt.0.alive` = false ab
01:39:06, die Werte im ioBroker eingefroren (Vorlauf 25,5 vom Stand 01:36:00),
und dieselben Werte am Gerät weiterlaufend (Kompressor 27 → 26, Rücklauf 23,75
statt der 23,5 im ioBroker).

Nachweis | Ergebnis
:--- | :---
Firmware läuft ohne Broker weiter | 2 min Beobachtung, durchgehend HTTP 200, Abfragezyklus zur Wärmepumpe unbeirrt
Kurvenwerte überleben im RAM | `fehlend = 0` nach vier Minuten ohne Broker
Der Knopf schaltet ohne MQTT | **GRÜN nach 58 s**, `TargetHigh` 26 → **34**
Nach GRÜN kommt Wärme | Kompressor **26 → 33 Hz**
Oberfläche unter Last bedienbar | Browser des Owners und Messskript gleichzeitig, ohne Stocken

**Die 34 ist hier der endgültige Beleg.** Der Broker war seit vier Minuten tot,
der ioBroker fror bei 01:38:48 ein — der Wert lag im RAM der Bridge, seit dem
Neustart um 00:54, und ging von dort in die Wärmepumpe. Es gab keine Leitung, aus
der er sonst hätte kommen können.

**Und der Satz „Was GRÜN nicht heißt: dass die Anlage heizt" ist eingelöst.** Bei
Etappe 5 blieb der Kompressor auf 0 Hz, weil die KNX-Freigabe fehlte; hier ging
er auf 33 Hz. Damit ist zum ersten Mal die ganze Kette gezeigt: Knopf → Kurve →
Wärme.

**Der Rückweg nach dem Wiedereinschalten:**

```
01:47:23  mqtt.0 wieder gestartet
01:48:15  H1 meldet sich von allein wieder an, LWT Online   (52 s Backoff)
01:48:54  Re-Assert holt zurück: Heating_Mode 0 -> 1        (39 s später)
01:53:40  Re-Assert korrigiert den Sollwert 35 -> 26
```

Die 52 Sekunden sind der zweite ungemessene Punkt aus Abschnitt 7: Die
Reconnect-Logik übersteht einen achtminütigen Ausfall und holt sich von allein
zurück, in der erwarteten Größenordnung des Backoff-Deckels von 60 s.

**Ein Fehler beim Messen, der hierher gehört.** Der erste Wächter las
`Heating_Mode` aus dem **ioBroker** und meldete eine Rückkehr, die nicht
stattgefunden hatte: Der Datenpunkt stand seit dem Broker-Stopp eingefroren auf
dem Wert von *vor* dem Notbetrieb und sah dabei aus wie ein aktueller Wert. Am
Gerät stand `Heating_Mode` unverändert auf 0. Daraus die Regel: **Im Ausfallfall
ist der ioBroker keine gültige Messquelle** — dann zählt nur, was die Bridge
selbst über `/tablerefresh` herausgibt. Das gilt für jede künftige Messung an
diesem Vorhaben.

**Aufgeräumt.** Sollwerte durch den Re-Assert, Kurvenpunkte durch
`kurven_sync.py`; die Endkontrolle gegen die Baseline von 01:31 zeigt **keinen
abweichenden Konfigurationswert**, nur 15 laufende Messwerte. Nachwirkung des
Laufs: `Heat_Energy_Production` stand danach bei 2400 W statt 200 W, weil der
Sollwert nach dem Werks-Reset knapp fünf Minuten auf 35 °C stand — das läuft von
allein aus. Nach dem Zurückschalten auf Kühlen (Modus Nur-DHW) steht H1 wieder
exakt auf dem Stand von 00:52, vor dem ersten Lauf des Abends.

### Der Warmwasserknopf an H2 — 2026-08-21, 02:19

Bis zu diesem Lauf war die Rolle Warmwasser **nie auf einem Gerät gelaufen**:
`NOTBETRIEB_ROLLE_WASSER` war gebaut, hosttestbar und im Broker versorgt, aber
`/notbetrieb` antwortete an H2 mit 404. Ein Release mit einem Knopf, den nie
jemand gedrückt hat, wäre kein Release gewesen.

**Vorweg ein Fund beim Aufräumen der Rückfallebene — und seine
Richtigstellung:** Im lokalen Ordner `~/HeishaMon-Rollback/` fehlte
`heishamon_esp32_h2_ota_v3.11.0.bin`. Es wurde deshalb aus dem Tag `v3.11.0` in
einem temporären Worktree nachgebaut, bevor das Produktivgerät angefasst wurde
(geprüft über MD5 gegen das H1-Binary und den Stufennamen im Abbild).

**Der Rückfall war dabei nie in Gefahr, anders als hier zunächst notiert.** Die
Binaries liegen nicht in der Git-Historie des privaten Repos — dessen
`.gitignore` schließt `*.bin` ausdrücklich aus —, sondern an dessen
**GitHub-Releases**. Und das private Release `v3.11.0` enthält alle vier
Builds, auch den für H2, nur unter der dortigen Namensform ohne „v"
(`heishamon_esp32_h2_ota_3.11.0.bin`). Gefehlt hat also nur die lokale Kopie.

**Merke für künftige Releases:** Ein `cp` in den Ordner ist keine Sicherung. Der
vorgesehene Weg steht in der `.gitignore` des Repos —
`gh release create v<version> <dateien>`.

**Ausgangszustand:** KNX auf Kühlen, Modus Nur-DHW — der Sommerfall, für den
Stufe 2 gebaut ist. H2 an, `Operating_Mode_State` = 3, Speicher 64 °C bei
Sollwert 48, Kompressor 0 Hz. Kein KNX-Eingriff, kein Ladebedarf.

**Der Status nach dem Flashen ist für sich schon ein Nachweis:** `0;1;3;0;0`

* **Drei Schritte statt sieben** — das Rollen-Flag greift, H2 fährt die
  Warmwasser-Folge.
* **`fehlend` = 0** — `DHWTemp` war binnen Sekunden nach dem Neustart wieder da.
  Die Broker-Wiedereinspielung ist damit an einem zweiten Gerät belegt.
* **Sperre = 0, obwohl `Heat_Cool_SW_State` auf Cool steht.** Das ist der
  Nachweis, den es nur hier gibt: Die Betriebsart-Sperre gilt für Heizen und
  **nicht** für Warmwasser. Am selben Abend war H1 im selben Kühlbetrieb
  gesperrt.

**Der Lauf hatte zunächst gar keinen Beweiswert** — und das ist der lehrreiche
Teil. Alle drei Schritte fanden ihren Sollwert bereits vor: `OperationMode` = 3,
`DHWTemp` = 48, `Heatpump` = 1. Der Knopf hätte GRÜN gemeldet, ohne dass sich ein
Byte ändert. Deshalb wurden im Ruhefenster vorher zwei Werte verstellt und die
Rückmeldung abgewartet:

```
02:18:43  set/DHWTemp 45 und set/Heatpump 0   (ein Telegramm, 20 ms Abstand)
02:18:52  von der Waermepumpe bestaetigt: TOP0=0, TOP9=45
02:18:52  KNOPF
02:19:16  GRUEN nach 24 s (drei Schritte a 8 s)
```

Gesendet | vor dem Knopf | nach dem Lauf
:--- | ---: | ---:
`DHW_Target_Temp` (TOP9) | **45** | **48**
`Heatpump_State` (TOP0) | **0 = aus** | **1 = An**
`Operating_Mode_State` (TOP4) | 3 | 3

Zwei von drei Schritten tragen damit den Beweis; der dritte ist über M3
inhaltlich belegt. **Endkontrolle:** von 92 Zeilen weicht genau eine ab —
`Main_Inlet_Temp` 19,75 → 19,50, ein laufender Messwert.

**Nicht abgedeckt bleibt der Heizmodus.** M3 und dieser Lauf fanden beide im
Kühlbetrieb statt. Dass `OperationMode` = 3 im Heizbetrieb durchgeht, ist
plausibel — wenn es sogar im Kühlbetrieb trägt, wo die Anlage Heizmodi ablehnt —
aber ungemessen. Ein eigener KNX-Wechsel lohnt dafür nicht; der Fall sollte
mitlaufen, wenn der Schalter ohnehin auf Heizen steht, etwa beim Termin fürs
Kurvenfoto.

### Der eigene Zugang — am Gerät geprüft (2026-08-21, 02:46)

Acht Abfragen gegen die frisch geflashten Stufen, alle wie erwartet:

Weg | Endpunkt | Ergebnis
:--- | :--- | :---
`admin` + OTA-Passwort | `/notbetrieb` | **401** — der alte Weg ist zu
`admin` + OTA-Passwort | `/notbetrieb/start` | **401**
`notbetrieb` + falsches Passwort | `/notbetrieb` | **401**
`notbetrieb` + eigenes Passwort | `/notbetrieb` an H1 | **200**
`notbetrieb` + eigenes Passwort | `/notbetrieb` an H2 | **200**
`admin` + OTA-Passwort | `/settings` | **200** — unverändert
`notbetrieb` + eigenes Passwort | `/settings` | **401** — trennt in beide Richtungen
ohne Anmeldung | `/notbetrieb/status` | **200** — bewusst offen

Die Trennung wirkt also beidseitig: Mit dem Notbetriebspasswort kommt niemand an
die Einstellungen, und mit dem Firmware-Passwort niemand an den Knopf.

### Zustand der Geräte

Gerät | Stand
:--- | :---
H1 (192.168.2.120) | **3.13.0**, per OTA am 2026-08-21 um 15:08 (Env `heishamon_esp32_h1_ota`). Abnahme grün, vier laufende Messwerte. Anlage steht, Direktbetrieb, Betriebsart Cool, Knopf gesperrt (`0;1;7;0;2;0;`) |
H2 (192.168.2.122) | **3.13.0**, per OTA am 2026-08-21 um 15:16 (Env `heishamon_esp32_h2_ota`, Rolle Warmwasser). Abnahme grün, zwei laufende Messwerte. Knopf frei (`0;1;3;0;0;0;`) |
Prüfstand (192.168.2.197) | **3.13.0**, per USB am 2026-08-21 um 14:27, Rolle Heizen, Broker wieder auf 192.168.2.147. Für den Notbetriebsknopf weiterhin untauglich (ohne Wärmepumpe kein TOP101), für die **Verbindungsanzeige** dagegen genau richtig — dort ist der ganze Nachweis zu 3.13.0 gelaufen |

Die Anlage ist nach dem Lauf zeilengleich mit dem Zustand davor; der Re-Assert
läuft normal weiter. Nichts ist aufzuräumen.

Die Testwerte unter dem Prefix `panasonic_heat_pump_test` bleiben im ioBroker —
sie stören nichts und sparen beim Wiedereinstieg einen Schritt.

### Wiedereinstieg prüfen

**Der nächste Schritt ist Etappe 5**, in den zwei Läufen oben — erst der
Sperr-Nachweis im Kühlbetrieb, dann der GRÜN-Lauf mit dem KNX-Schalter auf
Heizen. Beides setzt voraus, dass die Firmware dieses Branches auf H1 liegt.

Beim Sperr-Nachweis ist die Statusroute die Kurzfassung: Das fünfte Feld ist der
Sperrgrund (0 = frei, 1 = Werte fehlen, 2 = nicht auf Heizen).

```bash
git switch notbetrieb-web
c++ -std=c++17 -O2 -Wall -o /tmp/nb_test test/notbetrieb_test.cpp && /tmp/nb_test
python3 test/css_klassen_test.py

# Testlauf an der Anlage immer im Ruhefenster des Re-Assert:
cd ~/nodered-flows && ./testfenster.py --warte 240 --wache 200

# Liegen die Notbetriebswerte im Broker? (Knopf bleibt sonst gesperrt)
curl -s "http://192.168.2.147:8087/getBulk/\
mqtt.0.panasonic_heat_pump.notbetrieb.Z1HeatCurveTargetHighTemp,\
mqtt.0.panasonic_heat_pump.notbetrieb.Z1HeatCurveTargetLowTemp,\
mqtt.0.panasonic_heat_pump.notbetrieb.Z1HeatCurveOutsideLowTemp,\
mqtt.0.panasonic_heat_pump.notbetrieb.Z1HeatCurveOutsideHighTemp,\
mqtt.0.panasonic_heat_pump2.notbetrieb.DHWTemp"

# Steht die Anlage auf Heizen? 0 = Heizen, 1 = Kuehlen (alles andere sperrt)
curl -s "http://192.168.2.147:8087/getPlainValue/mqtt.0.panasonic_heat_pump.state.Heat_Cool_SW_State"

# Zustand;Schritt;Schritte;fehlend;Sperre - Sperre: 0 frei, 1 Werte, 2 Betriebsart
curl http://192.168.2.120/notbetrieb/status
```

**Die Versionsnummer steht seit dem 2026-08-21 auf 3.12.0**, gesetzt in
Etappe 7 zusammen mit dem Changelog — nachdem der Nachweis an der Anlage vorlag
und nicht davor. Beide Stufen laufen darauf, die Rollback-Binaries liegen in
`~/HeishaMon-Rollback/`.

**Achtung bei den Binaries:** Ab 3.12.0 stecken **zwei** Passwörter lesbar in
jedem Abbild — das des Setup-Hotspots (seit 3.8.1) und neu das des
Notbetriebsknopfs, das auf dem ausgedruckten Notfallblatt steht. Am 2026-08-21
mit `strings` nachgeprüft. Sie gehören damit erst recht nur an die Releases des
**privaten** Repos, nie an ein öffentliches.

Das private Release `v3.12.0` ist am 2026-08-21 angelegt und trägt alle vier
produktiven Builds (beide ESP32-Stufen, beide D1-mini-Rückfallebenen).

**Eine lokale Besonderheit:** Der USB-Port des Prüfstands hat sich auf
`/dev/cu.usbserial-1110` geändert. Das steht in `platformio_user_env.ini`
(gitignored) und ist auf einem anderen Rechner erneut anzupassen.

---

## 11. Nachtrag 3.13.0 — die Anzeige, die den Ausfall sichtbar macht

**Stand: 2026-08-21, im Repo fertig, der Nachweis am Gerät steht noch aus.**
Branch `verbindungsanzeige`, Rettungsanker: Tag
`rettungsanker-vor-verbindungsanzeige-2026-08-21` auf `main`.

Aufgegriffen sind die beiden Folgethemen aus Abschnitt 9 — der stille Ausfall
und die doppelte Bedeutung von Rot. Dazu kam eine dritte Kleinigkeit aus der
Bedienung: Der Text „Läuft…" während der Schrittfolge sagte nicht, *was* läuft.

### Was gebaut ist

Etappe | Inhalt | Commit
:--- | :--- | :---
0 | Tag und Branch | —
1 | [`src/verbindung.h`](src/verbindung.h) + Hosttest, in der CI | `76002ab`
2 | Die Anzeige auf beiden Seiten, Knopffarbe, Lauftext | `c38066f`
3 | Doku und Version 3.13.0 | `05ac412`
3a | Nachweis: der Re-Assert kommt bei beiden Stufen an | `67d8d8e`
4 | **Etappe B — der Herzschlag der Steuerung** | `a499fcd`
5 | Fix: Logzeile aus dem MQTT-Callback nach loop() | `ff56532`
6 | Nachweis am Prüfstand, alle sechs Lagen | `4e8d263`
7 | **Rollout auf H1 und H2, Abnahme grün** | `8c07350`

### Die Entscheidungen

**Gemessen wird die MQTT-Verbindung, nicht das WLAN** (Owner-Entscheidung
2026-08-21, Variante A). Der Ausfall, um den es geht, ist der des ioBroker — und
der Broker *ist* der ioBroker-Adapter. Ohne WLAN wäre auch die Weboberfläche
weg, die die Auskunft anzeigen soll.

Der zweite Ausfall — „Broker läuft, aber die Kaskadenregelung rechnet nicht
mehr", der Fall *Node-RED-Container weg, ioBroker läuft* aus der Tabelle in
Abschnitt 1 — wurde als **Etappe B** direkt danach gebaut, siehe unten. Für den
Menschen vor der Seite ist die Folge identisch: niemand führt den Sollwert nach.
Er bekommt deshalb dieselbe Anzeige, aber einen eigenen Text.

**Karenz: 5 Minuten.** Ein Neustart des ioBroker-Adapters oder des Containers
auf der Synology dauert regelmäßig ein bis zwei Minuten. Eine Störmeldung, die
von selbst wieder verschwindet, erzieht die Familie dazu, sie zu übersehen — und
dann wird auch die echte übersehen. Der Reconnect-Backoff (5 s bis 60 s) liegt
vollständig darunter: In fünf Minuten hat die Firmware mindestens acht
Verbindungsversuche hinter sich.

**Die Notbetriebsseite zeigt auch den Normalfall, die Startseite nicht.** Wer
die Notbetriebsseite öffnet, ist im Zweifel; ein ruhiges „Hausteuerung:
verbunden" sagt ihm, dass er den Knopf **nicht** braucht — und das ist die
häufigere und die gefährlichere Fehlentscheidung. Die Startseite ist ein
Nachschauwerkzeug; ein dauerhaftes „verbunden" über der Topic-Tabelle würde nach
kurzer Zeit übersehen, samt der Störmeldung an derselben Stelle.

**Farbe: Orange.** Rot ist ab jetzt das Laufergebnis und sonst nichts, Gelb ist
„Konfiguration Notbetrieb läuft". Dass der Sperrhinweis dieselbe Farbe trägt,
ist stimmig — beides heißt „so wie es ist, geht es nicht weiter".

**Der Knopf ist blau.** Zwei Zeichen im Quelltext, wie in Abschnitt 9 vermutet.
`w3-blue` war im eingebetteten CSS bereits definiert und stand schon an der
richtigen Stelle hinter `.w3-button`, deshalb hat
[`test/css_klassen_test.py`](test/css_klassen_test.py) den Wechsel ohne
Anpassung mitgetragen.

**Die Seite spricht Deutsch.** `webHeader` trägt jetzt ein `charset`, und die
Texte der Notbetriebsseite haben Umlaute. Bis 3.12.0 war jeder Text der
Oberfläche in `ae/oe/ue` geschrieben und die Zeile deshalb entbehrlich; diese
eine Seite liest im Ernstfall jemand aus der Familie. Die übrigen Seiten (Home,
Settings, Firmware) bleiben unberührt — sie sind Technikseiten. Im Binary
nachgeprüft, dass die Umlaute als UTF-8 ankommen; alle Quelldateien waren
vorher reines ASCII, es kann also nichts umkippen.

### Drei Regeln, die man falsch programmieren kann

Sie stehen arduino-frei in [`src/verbindung.h`](src/verbindung.h) und werden von
[`test/verbindung_test.cpp`](test/verbindung_test.cpp) mit 62 Zusicherungen
geprüft — gleiches Muster wie `sendwindow.h` und `notbetrieb.h`.

1. **Die Karenz auf die Sekunde.** Geprüft wird die Grenze selbst, nicht ein
   Punkt weit dahinter.
2. **„Seit dem Neustart" statt einer Zahl**, wenn seit dem Einschalten nie eine
   Verbindung bestand. Dort ist die wahre Ausfalldauer unbekannt — der Broker
   kann seit Tagen weg sein, das Gerät ist nur gerade neu gestartet.
3. **Der `millis()`-Überlauf nach 49,7 Tagen.** Deshalb wird die Dauer
   *fortgeschrieben* und nicht bei jeder Abfrage neu gerechnet, und deshalb gibt
   es den Deckel bei 30 Tagen.

**Die Gegenprobe ist der aufschlussreiche Teil.** Ersetzt man das Fortschreiben
durch die naive Rechnung, meldet die Seite nach 49,7 Tagen Ausfall „Hausteuerung
seit **1 Minute** nicht erreichbar" — also „alles in Ordnung", ausgerechnet nach
sieben Wochen ohne Steuerung. Der Test trifft diesen Zeitpunkt gezielt: eine
Ausfalldauer knapp über der Naht, an der die naive Differenz unter der Karenz
liegt. Nur so ist belegt, dass der Fall nicht bloß zufällig ausblieb.

Vier der ursprünglichen Zusicherungen waren **falsch aufgeschrieben**, nicht der
Header — unter anderem die Annahme, 30 Tage lägen unter der halben
`millis()`-Breite. Tun sie nicht (die liegt bei 24,85 Tagen); sie müssen es auch
nicht, weil die unsigned-Differenz bis zur vollen Naht eindeutig ist und
zwischen Deckel und Naht 19 Tage bleiben.

### Was der Hosttest nicht abdeckt

Die Anbindung in [`HeishaMon.cpp`](src/HeishaMon.cpp) — also
`mqtt_client.connected()` als Eingang der Wacht — und die Anzeige selbst. Beides
gehört in den Abnahmetest.

**Der Prüfstand ist dafür wieder das richtige Werkzeug.** Für den Notbetriebsknopf
war er seit der Sperre ausgeschieden (ohne Wärmepumpe kein TOP101); eine
Verbindungsanzeige braucht kein TOP101. Der ganze Nachweis lief damit **ohne
Eingriff an H1 oder H2 und ohne Testfenster** — und er hat sich gelohnt: Genau
in dieser nicht hosttestbaren Anbindung steckte ein Fehler (Protokoll unten).

### Prüfplan

1. Prüfstand (192.168.2.197) mit Strom versorgen und die Firmware dieses
   Branches per USB aufspielen (Env `d1_mini_test`).
2. Startseite und `/notbetrieb` öffnen: Auf der Notbetriebsseite muss
   „Hausteuerung: verbunden" stehen, auf der Startseite nichts.
3. **Den Broker unerreichbar machen** — am einfachsten über
   `/settings`: die MQTT-Serveradresse auf eine tote IP im eigenen Netz stellen
   und speichern. Das ist reversibel und rührt den ioBroker nicht an.
4. **Fünf Minuten warten.** Vorher darf nichts erscheinen — das ist die halbe
   Prüfung. Danach muss auf beiden Seiten die orange Zeile stehen, und die
   Minutenzahl muss mitlaufen.
5. Serveradresse zurückstellen. Nach der Rückkehr muss die Zeile verschwinden
   und im MQTT-Log eine Zeile „Hausteuerung war *n* s nicht erreichbar" stehen.
6. **Den Sonderfall prüfen:** mit toter Serveradresse neu starten. Dann muss
   dort „seit dem Neustart dieses Geräts" stehen, keine Minutenzahl.
7. Erst danach OTA auf H1 und H2, mit der üblichen Abnahme gegen die Baseline
   ([`test/tablesnap.py`](test/tablesnap.py)).

### Etappe B — der Herzschlag, gebaut am 2026-08-21

Der Ausfall „Broker läuft, Node-RED rechnet nicht mehr" wird an einer Zeitmarke
auf das zuletzt empfangene `set`-Kommando erkannt. Karenz **12 Minuten**, eigene
Lage (4) in der Statusroute, eigener Text auf der Seite:

> **Hausteuerung erreichbar, sendet aber seit 23 Minuten keine Vorgaben.**
> Der Server antwortet, aber die Steuerung rechnet nicht mehr.

Das ausdrückliche „erreichbar" ist der Zweck der Unterscheidung: Wer zum Server
im Keller läuft, soll wissen, ob dort überhaupt etwas zu holen ist.

**Zwei Regeln halten das zusammen, und beide sind im Hosttest belegt:**

1. **Ist der Broker weg, gilt der Broker-Ausfall.** Beides zu melden wäre
   doppelt gemoppelt — ohne Broker *kann* kein Kommando kommen. Die Uhr für die
   stumme Steuerung läuft deshalb nur bei stehender Verbindung und startet mit
   dem Verbindungsaufbau neu. Die Gegenprobe zeigt, was ohne diese Regel
   passiert: Unmittelbar nach der Rückkehr des Brokers meldet die Seite einen
   zweiten Fehler, den es nie gab.
2. **Der Wiedereinspiel-Schwall zählt nicht als Lebenszeichen** — dazu unten
   mehr, das war eine Korrektur an meiner eigenen Annahme.

**Beide Punkte, die vorher zu klären waren, sind beantwortet:**

* **Bekommt H2 in jedem Re-Assert-Takt ein `set`-Kommando? Ja.** Am 2026-08-21
  im Flow-Code nachgesehen (`Hauptmodus-Verteiler V6.5`, §6 „Idempotente
  Ausgabe"): Der Verteiler sendet zwar nur bei Änderung gegenüber `lastSent` —
  aber der 5-Minuten-Takt setzt `state.lastSent = {}` zurück, und danach gelten
  **alle dreizehn Kanäle als geändert**. Sechs davon gehen an H2 (Ausgänge 7–12:
  `Z1HeatRequestTemperature`, `Z1CoolRequestTemperature`, `Heatpump`,
  `OperationMode`, `WaterPump`, `WaterPumpSpeed`). Beide Stufen bekommen also in
  jedem Takt Verkehr; ein Herzschlag trüge an beiden.
  **Und es kommt auch wirklich an — am 2026-08-21 an H2 gemessen.** Ein
  passiver Telnet-Mitschnitt über 400 s (nichts gesendet, nur mitgelesen) fing
  zwei volle Takte:

  ```
  13:40:46  6 × "Callback from mqtt" innerhalb von 0,1 s
  13:40:57  1 × "Callback from mqtt"
  13:45:46  6 × "Callback from mqtt" innerhalb von 0,1 s
  13:45:57  1 × "Callback from mqtt"
  ```

  Taktabstand exakt 300,0 s, je Takt sieben empfangene Kommandos. Damit ist
  auch die Restfrage beantwortet: **Der ioBroker-MQTT-Adapter publiziert auch
  unveränderte Werte** — sonst wäre der zweite Takt leer geblieben, denn an den
  Sollwerten hatte sich zwischen 13:40 und 13:45 nichts geändert. Die sechs im
  Schwall sind die WP2-Kanäle des Verteilers; der siebte zehn Sekunden später
  kommt aus der Wächter-Logik mit ihrem eigenen Takt (`QuietMode`).

  Der Herzschlag ist damit an beiden Stufen tragfähig, und der Takt ist keine
  Annahme mehr, sondern gemessen.
* **Die Zeitmarke gehört NACH die Karenzprüfung — nicht davor.** Hier stand
  vorher das Gegenteil, mit der Begründung „empfangen ist empfangen". Das ist
  falsch, und zwar aus genau dem Grund, aus dem es die Karenzzeit überhaupt
  gibt: Der ioBroker-Adapter spielt jedem neuen Abonnenten die gespeicherten
  Set-Werte ein — **auch dann, wenn Node-RED längst tot ist**. Dieser Schwall
  belegt nur, dass der Broker lebt, und das beobachtet bereits die andere Uhr.
  Vor der Karenzprüfung gestempelt, verstummte die Meldung nach jedem Reconnect
  für zwölf Minuten, ohne dass sich etwas geändert hätte.

  Ein Kommando, das die Firmware danach **verwirft** (unbekanntes Topic,
  Bereichsfehler), zählt dagegen sehr wohl — die Steuerung hat gesendet, sie
  lebt. Der Aufruf steht deshalb vor `build_heatpump_command()`.

**Die Karenz liegt bei 12 Minuten** und damit deutlich über der
Verbindungskarenz. Der Takt ist gemessene 300,0 s, ein einzelner ausgefallener
Takt ist noch kein Ausfall; zwölf Minuten decken zwei verpasste Takte samt
Reserve ab. Dass sie größer ist als die Broker-Karenz, ist kein Zufall: Hier
wird auf ein *Ausbleiben* gewartet, und das ist die unsicherere Aussage.

### Ein Befund aus der Gegenprobe, der eine Lücke aufdeckte

Beide Uhren teilen sich denselben Kern (`struct Ausfall`) — die
Überlauffestigkeit ist der subtile Teil, und zweimal hingeschrieben wäre zweimal
Gelegenheit, sie falsch zu machen. Fünf Gegenproben sind gefahren; eine davon
**bestand**, und das war die aufschlussreiche:

Dreht man den Vorrang in `verbindung_lage()` um, fällt keine einzige Zusicherung
um. Der Grund: Beide Uhren laufen im Betrieb nie gleichzeitig, weil die Stumm-Uhr
beim Verbindungsverlust zurückgesetzt wird. Die Reihenfolge dort ist also eine
**zweite Sicherung**, keine tragende Regel — sie greift nur, falls jemand später
das Zurücksetzen entfernt. Sie bleibt stehen, aber der Test baut den im Betrieb
unmöglichen Zustand jetzt von Hand, damit sie geprüft ist und nicht geglaubt.

### Das Protokoll vom 2026-08-21, 14:08–15:02 — alles am Prüfstand

**Gefahren, ohne H1 oder H2 anzufassen und ohne Testfenster.** Der Prüfstand
(192.168.2.197, Env `d1_mini_test`) ist für diese Funktion wieder das richtige
Werkzeug: Eine Verbindungsanzeige braucht kein TOP101 und damit keine
Wärmepumpe.

**Ein Glücksfall im Aufbau:** Der Prüfstand läuft unter dem Prefix
`panasonic_heat_pump_test`, und dorthin sendet der Hauptmodus-Verteiler nichts.
Er wird also **von allein stumm** — der Herzschlag ließ sich nachweisen, ohne
den Node-RED-Container anzuhalten.

Für den Broker-Ausfall brauchte es einen Broker, der sich abschalten lässt, ohne
den ioBroker der Anlage anzurühren. Über `/settings` geht das nicht: Jede
Änderung dort startet das Gerät neu, und danach ist die Lage immer 3 („nie
verbunden"), nie 2. Also lief für die zweite Hälfte ein **minimaler
MQTT-Broker** auf dem Arbeitsrechner (192.168.2.145), auf den der Prüfstand
umgestellt wurde — er kann nur, was PubSubClient braucht, und zeigt die
Log-Zeilen der Firmware direkt an.

Zeit | Prüfung | Erwartet | Gemessen
:--- | :--- | :--- | :---
14:11 | Ausgangslage verbunden | Startseite leer, Notbetriebsseite „verbunden" | beides ✓
14:23:38 | **Stumm-Karenz, 1. Lauf** | 14:23:29 (Verbindung 14:11:29 + 12 min) | im 20-s-Fenster getroffen ✓
14:39:26 | **Stumm-Karenz, 2. Lauf** | 14:39:23 (letztes Kommando 14:27:23 + 12 min) | ✓
14:39:49 | Rückkehr der Vorgaben | Lage sofort 0, Log mit der Stille-Dauer | „Hausteuerung hat **745 s** keine Vorgaben gesendet" (14:27:23 → 14:39:48) ✓
14:41:05 | Broker gekappt | Lage 1 = Karenz, **keine** Störmeldung | ✓
14:46:06 | **Broker-Karenz** | 14:46:06 (Abriss ~14:41:06 + 5 min) | punktgenau ✓
14:55:07 | **Vorrang** | nach 14 min ohne Broker weiterhin Lage 2 | `0;1;7;15;1;**2**;14 Minuten` ✓
14:55:09 | **Rückkehr des Brokers** | Log mit Dauer, Lage 0, **keine** Stumm-Meldung | „Hausteuerung war **850 s** nicht erreichbar", Lage 0 ✓
15:00:58 | **Lage 3 „nie verbunden"** | Text ohne Minutenzahl | „Hausteuerung seit dem Neustart dieses Geräts nicht erreichbar", Dauertext **leer** ✓

**Die beiden Seiten sagen wirklich Verschiedenes** — im selben Störfall:

* Startseite: „… Die Wärmepumpe läuft mit dem zuletzt gesetzten Sollwert weiter.
  **Wird es zu kalt, hilft der Notbetrieb.**"
* Notbetriebsseite: derselbe Satz **ohne** den Verweis — wer dort steht, braucht
  den Hinweis auf den Notbetrieb nicht mehr.

Die Umlaute kommen über das neue `charset` korrekt an („Wärmepumpe läuft"),
der Knopf ist blau.

### Der Fehler, den dieser Lauf gefunden hat

**Das erste Kommando an den Prüfstand ging verloren.** Statt `SET3 QuietMode: 0`
meldete die Firmware:

```
14:24:14  Error: Unknown set topic 0Q
```

Aus `panasonic_heat_pump_test/set/QuietMode` war `0Q` geworden.

**Ursache:** `write_mqtt_log()` ruft `mqtt_client.publish()`, und **PubSubClient
benutzt für Senden und Empfangen denselben Puffer**. Genau in diesen Puffer
zeigen `topic` und `payload` während des Callbacks. Etappe B setzte die
Herzschlag-Meldung mitten in der Auswertung ab und überschrieb damit den
Topic-Namen. Der bestehende Code war nie betroffen: Dort steht
`write_mqtt_log()` nur an Stellen, an denen `topic` und `msg` nicht mehr
gebraucht werden.

**Was das im Betrieb bedeutet hätte:** Jedes Mal, wenn die Stumm-Meldung endet,
wäre genau das erste Kommando danach verschluckt worden — ausgerechnet das, das
die Rückkehr der Steuerung anzeigt.

**Behoben** (`ff56532`): Der Callback merkt sich nur noch die Dauer, geloggt wird
aus `loop()`, wenn der Puffer frei ist — dasselbe Muster wie `wifiOutageSeconds`
beim WLAN-Ausfall. Gegenprobe am Gerät: dasselbe Kommando meldet jetzt
`<SUB> SET3 QuietMode: 0`.

Das ist genau die Lücke, die im Changelog als „gehört in den Abnahmetest" steht —
die Anbindung in `HeishaMon.cpp`, die kein Hosttest abdecken kann. Sie ist damit
auch der Beleg, dass der Abnahmetest nötig war.

### Ein Nebenbefund, der die Platzierung des Herzschlags bestätigt

Zweimal im Log, nach jedem Verbindungsaufbau:

```
14:11:34  34 wiedereingespielte Set-Kommandos nach dem Verbinden verworfen
14:27:19  34 wiedereingespielte Set-Kommandos nach dem Verbinden verworfen
```

**34 Set-Kommandos** spielt der ioBroker-Adapter dem Prüfstand beim Verbinden
ein — obwohl unter dem Prefix `panasonic_heat_pump_test` niemand steuert, weder
Node-RED noch sonst wer. Hätte der Herzschlag wie ursprünglich geplant *vor* der
Karenzprüfung gestempelt, wären diese 34 Nachrichten als „die Steuerung lebt"
durchgegangen — und die Stumm-Meldung wäre nach jedem Reconnect zwölf Minuten
lang unterdrückt worden, ohne dass irgendetwas rechnet. Aus dem Adaptercode
abgeleitet war das vorher schon; jetzt ist es gemessen.

### Zustand nach dem Lauf

Der Prüfstand steht wieder auf `192.168.2.147:1883`, Status `0;1;7;0;2;0;` —
Notbetriebswerte vollständig, Lage 0, Sperre 2 (mangels Wärmepumpe kein TOP101,
wie erwartet). Der Minibroker ist beendet, Port 1883 auf dem Arbeitsrechner
wieder frei. An H1 und H2 wurde nichts angefasst.

### Der Rollout, 2026-08-21 — beide Stufen auf 3.13.0

**H1 um 15:08:22, H2 um 15:16:22** (Envs `heishamon_esp32_h1_ota` und
`heishamon_esp32_h2_ota`), nacheinander mit Abnahme dazwischen, damit nie beide
Stufen gleichzeitig weg sind. Baseline vorher und nachher über
[`test/tablesnap.py`](test/tablesnap.py).

 | H1 (192.168.2.120) | H2 (192.168.2.122)
:--- | :--- | :---
Version | 3.13.0 ✓ | 3.13.0 ✓
Statusroute | `0;1;7;0;2;0;` | `0;1;3;0;0;0;`
Tabellenstruktur | 92 Topics, identisch ✓ | identisch ✓
Abweichungen zur Baseline | 4 laufende Messwerte | 2 laufende Messwerte
Sollwerte über den Reboot | alle fünf unverändert ✓ | alle fünf unverändert ✓
LWT | Online ✓ | Online ✓

Die Abweichungen sind ausschließlich Werte, die sich ohnehin bewegen — an
beiden Stufen die Außentemperatur (22 → 21 °C), dazu an H1 Raumthermostat,
Außenrohr und Hochdruck, an H2 der Wärmetauscher-Auslauf. **Kein Sollwert hat
sich verstellt**, die `SUBSCRIBE_GRACE` aus 3.6.1 hat wie vorgesehen gegriffen
(`Quiet_Mode_Level` 0, `Z1_Heat_Request_Temp` 20, `Z1HeatCurveTargetHigh` 20,
`DHW_Target_Temp` 50 bzw. 48).

### Die wichtigste Gegenprobe: kein Fehlalarm im Normalbetrieb

Eine Anzeige, die grundlos Alarm schlägt, ist schlimmer als keine. Der
entscheidende Nachweis ist deshalb nicht, dass die Meldung kommt, wenn sie soll
— sondern dass sie **ausbleibt**, solange alles läuft.

**Beide Stufen 18 Minuten lang beobachtet (15:19–15:37), 37 Messpunkte, im
30-Sekunden-Takt:** durchgehend Lage 0, kein einziger Lagewechsel, **kein
einziges Vorkommen von Lage 4**.

Das ist mehr als drei Re-Assert-Takte und deutlich über der Stumm-Karenz von
zwölf Minuten. Käme der Herzschlag nicht durch — würde also der echte Re-Assert
aus irgendeinem Grund nicht als Lebenszeichen gezählt —, hätte spätestens nach
zwölf Minuten an beiden Stufen die Stumm-Meldung stehen müssen. Damit ist der
Herzschlag nicht nur im Fehlerfall belegt (Prüfstand), sondern auch im
Normalfall.

Die Anzeige selbst an H1 im Normalbetrieb: Notbetriebsseite „Hausteuerung:
verbunden" (grau, klein), Startseite leer.

### Rollback

`~/HeishaMon-Rollback/` trägt alle vier produktiven Abbilder als `v3.13.0`
(beide ESP32-Stufen, beide D1-mini-Rückfallebenen). **Weiterhin gilt seit
3.12.0:** Das Notbetriebspasswort steckt lesbar in jedem Abbild — beim Rollout
erneut mit `strings` nachgeprüft. Die Binaries gehören nur an die Releases des
**privaten** Repos.

### Prüfplan-Ergänzung für Etappe B

**Alle erledigt am 2026-08-21, Protokoll oben.** Der ursprüngliche Plan stand so
da:

8. Broker wieder erreichbar, dann **den Node-RED-Flow anhalten** (oder den
   Container stoppen) und 12 Minuten warten. Die Seite muss von „verbunden" auf
   „Hausteuerung erreichbar, sendet aber seit …" wechseln — **nicht** auf „nicht
   erreichbar".
9. Flow wieder starten: Spätestens mit dem nächsten Re-Assert (< 5 min) muss die
   Meldung verschwinden, und im MQTT-Log steht „Hausteuerung hat *n* s keine
   Vorgaben gesendet".
10. **Die Gegenprobe zur Vorrangregel:** Broker abschalten und länger als 12
    Minuten warten. Es darf **nur** „nicht erreichbar" erscheinen, nie die
    Stumm-Meldung — und direkt nach dem Wiedereinschalten des Brokers darf die
    Stumm-Meldung ebenfalls nicht aufblitzen.
