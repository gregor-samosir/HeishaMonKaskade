# Vorhaben: Heizstab freigeben (Byte 9) und ForceHeater (Byte 5)

Ziel ist, den internen Heizstab über MQTT ansprechbar zu machen — bis 3.16.0
ging das nur am Bedienterminal.

**Stand dieser Datei:** 2026-08-28, Firmware 3.17.0.

> **In 3.17.0 gebaut und am 2026-08-28 an Stufe 1 gemessen — beides erledigt.**
> Alle **drei** Kommandos stehen in `setCommands[]`: SET37 `RoomHeaterState`,
> SET38 `DHWHeaterState`, SET39 `ForceHeater`. Owner-Entscheidung vom
> 2026-08-28, alle drei zusammen zu bauen statt sie zu staffeln — die Begründung
> dafür steht am Ende von Abschnitt 1.
>
> **Die WH-MDC05H3E5 nimmt Byte 9 an.** M0–M5 sind gelaufen (Abschnitt 8), die
> Kodierung ist zusätzlich ohne Gerät belegt
> ([`test/byte9_test.cpp`](test/byte9_test.cpp), in der CI). Offen ist nur noch
> das Winterexperiment.
>
> **Der Ausgangszustand war ein anderer als hier beschrieben** — beim Messen
> stand TOP59 an beiden Anlagen auf `Free`. Kein Widerspruch, sondern die
> Testfreigabe des Owners: Er hatte den Heizstab für seine eigenen Panel-Tests
> im Installateurmenü aktiviert und über Byte 9 freigegeben. Abschnitt 6,
> Punkt 3 hält den Stand fest.
>
> Die Topic-Namen tragen **kein** `Set`-Präfix: Bei uns heißt der Pfad
> `<prefix>/set/<Name>`, also `set/RoomHeaterState`. Der Upstream schreibt
> `commands/SetRoomHeaterState` — dort steckt das `Set` im Namen, weil sein
> Pfad es nicht trägt.

**Herkunft:** Fund beim Abgleich von `TobiasHanss/ioBroker.heishamon` und
`Egyras/HeishaMon` gegen unseren Stand am 2026-08-28. Das ioBroker-Repo brachte
nichts Neues (1:1-Portierung des Upstreams), der aktuelle Upstream dagegen drei
Set-Kommandos, die es bei uns nicht gibt.

---

## 1. Das Ziel — und warum `ForceHeater` es im laufenden Betrieb nicht erreicht

Motiv ist ein **Komfortgewinn im Heizbetrieb**, keine Notwendigkeit. Der
Heizstab ist an beiden Anlagen heute **deaktiviert**, und das aus gutem Grund:
Selbst bei −15 °C Außentemperatur hat sich gezeigt, dass die große Masse der
Fußbodenheizung ihn nicht braucht. Er hat 3 kW fix je Stufe — er kann
mithelfen, mehr nicht.

Interessant ist deshalb nicht die Dauerfreigabe, sondern das **gezielte
Zuschalten**: dann, wenn die Wassertemperaturen es laut Kaskadensteuerung
erlauben. Ob das den Komfort spürbar hebt, müssen Experimente im Winter zeigen —
das Vorhaben schafft nur die Voraussetzung dafür.

Das naheliegende Kommando dafür wäre `ForceHeater` gewesen. **Für dieses Ziel
ist es das falsche.** Das Panasonic-Servicehandbuch (Kapitel 12.9, liegt in
`doku-intern/`, nicht öffentlich) beschreibt Force Heater als Ersatzwärmequelle
**bei einer Störung der Wärmepumpe** — die Betriebsart setzt einen anliegenden
Fehler voraus und wird von der Fernbedienung im Fehlerfall auch selbst
aktiviert, wenn im Servicemenü „Force Heater: Auto" eingestellt ist. Bei
laufender Anlage wird die Anforderung abgelehnt, das Bedienteil meldet dann
sinngemäß „wegen laufendem Betrieb nicht möglich". In einer Frostphase läuft die
Anlage — genau dann greift `ForceHeater` also nicht.

**Der richtige Hebel steht in Kapitel 12.6.1.** Für den normalen Heizbetrieb
schaltet die Wärmepumpe den Heizstab selbst zu, sobald sechs Bedingungen
gleichzeitig erfüllt sind:

Bedingung | bei uns
:--- | :---
Heizstab-Schalter ist an | **fehlt — das ist dieses Vorhaben**, Byte 9 / TOP59
30 min seit Kompressor-Thermo-ON | Anlage
9 min seit Start der Umwälzpumpe | Anlage
Außentemperatur unter der Heizstab-Schwelle | TOP78 / SET20 `HeaterOnOutdoorTemp` — **haben wir**
Vorlauf mehr als 4 K unter Soll | Anlage
20 min seit dem letzten Heizstab-Aus | Anlage

Abgeschaltet wird bei Außentemperatur über Schwelle + 2 K oder Vorlauf über
Soll − 2 K, jeweils 15 s durchgehend, sowie bei Heizstab-Schalter aus oder
Kompressor thermo-off.

Damit ist die Lage klar: **Von allem, was für das Ziel nötig ist, fehlt genau
ein Schalter** — die Freigabe in Byte 9. Die Schwelle, ab der die Anlage den
Heizstab überhaupt in Betracht zieht, können wir mit SET20 längst setzen.

### Nachtrag 2026-08-28: `ForceHeater` ist trotzdem ein eigener Hebel

Am Bedienpanel gegengeprüft, und der Befund kippt die Staffelung dieses
Dokuments: **Steht die Wärmepumpe auf aus, lässt sich der Heizstab über Force
Heater einschalten, ohne dass eine Störung anliegt.** Die Handbuchaussage
bleibt richtig, sie ist nur unvollständig — sie beschreibt den Fall der
laufenden Anlage, und dort wird die Anforderung tatsächlich abgelehnt.

Für das Ziel dieses Vorhabens — Komfort im laufenden Heizbetrieb — ändert das
nichts: Dort bleibt SET37 der Hebel. Für **stehende** Anlage ist `ForceHeater`
dagegen der einzige Weg, den Heizstab überhaupt anzufordern, und das ist der
Fall, der im Notbetrieb und bei einer Störung interessiert. Deshalb ist SET39
in 3.17.0 mitgebaut worden statt zurückgestellt.

## 2. Abgrenzung — was ausdrücklich NICHT dazugehört

**Keine Anbindung an den Notbetrieb.** Entscheidung vom 2026-08-28: Das würde
den Notbetrieb nur unnötig komplex machen, und Komforteinbußen im Notbetrieb
sind akzeptiert. `NOTBETRIEB_WERTE_HEIZEN[]` in `src/notbetrieb.h` bleibt
unverändert.

> **Präzisierung vom 2026-08-30 (3.18.0):** Diese Abgrenzung gilt weiter — sie
> betrifft die **Nutzung** des Heizstabs als Notheizung. Der Notbetrieb schaltet
> ihn nach wie vor nicht ein, und die gehaltenen Werte sind unverändert.
> Umgekehrt nimmt er ihn seit 3.18.0 aber **zurück**: Beide Schrittfolgen
> beginnen an Position 2 mit `ForceHeater = 0`. Anlass ist die Steuerungsseite,
> die SET39 seit dem 2026-08-30 im Regelbetrieb fährt — ein stehender
> Heizstab-Auftrag träfe sonst auf eine Anlage, die der Notbetrieb gerade
> einschaltet, und die Umwälzpumpe hängt am Kommando. Aufräumen ist keine
> Anbindung. Siehe [`Auftrag-Heizstab-Notbetrieb.md`](Auftrag-Heizstab-Notbetrieb.md)
> und [`Ablauf-Notbetrieb.md`](Ablauf-Notbetrieb.md) Abschnitt 1b.

**`SetReset` (Byte 8, Bit 0) wird nicht übernommen.** Das Kommando quittiert
verriegelte Fehlercodes aus der Ferne (Äquivalent der Reset-Taste am
Bedienteil). Läuft die Anlage in einen Fehler, der einen Reset braucht, ist ein
Mensch am Bedienpanel die richtige Antwort — nicht ein MQTT-Topic. Byte 8 Bit 0
bleibt frei; wir belegen dort nur `0x02` (SET12 `ForceDefrost`) und `0x04`
(SET13 `ForceSterilization`).

**Der Warmwasser-Heizstab ist nicht das Ziel.** Für DHW hängt an dieser Anlage
bereits ein externer Heizstab. SET38 `DHWHeaterState` ist trotzdem gebaut, weil
Byte 9 beide Felder trägt und der Hosttest dadurch den Nachbarschutz belegen
kann — gebraucht wird es an dieser Anlage nicht.

**Kompatibilität der SET-Nummern mit HeishaMon ist kein Ziel.** Unsere
Nummerierung ist ab SET16 gegenüber dem Upstream verschoben und bleibt es. Was
zählt, ist Byte, Funktion und Name. Vergeben wurden **SET37 – SET39**; nächste
freie Nummer ist damit SET40.

## 3. Was belegt ist

Quelle ist `commands.cpp` des aktuellen Upstreams (`Egyras/HeishaMon`), im
Original nachgelesen — nicht über die TypeScript-Portierung.

### Byte 9 — Heizstab freigeben (das Vorhaben)

Kommando | Werte | Bits | Maske | Rücklesen
:--- | :--- | :--- | :--- | :---
SET37 `RoomHeaterState` | `1` = blockiert, `2` = frei | `getBit7and8` | `0x03` | TOP59 `Room_Heater_State`
SET38 `DHWHeaterState` | `4` = blockiert, `8` = frei | `getBit5and6` | `0x0C` | TOP58 `DHW_Heater_State`

Byte 9 war in `setCommands[]` bis 3.16.0 gar nicht belegt. Beide Rücklese-Topics
existierten schon und zeigen `Blocked` / `Free`.

### Byte 5 — ForceHeater (SET39)

Das Kommando schreibt Byte 5 auf `4` (aus) oder `8` (an) — die Bits, die
`getBit5and6` liest, also **Maske `0x0C`**. Rücklesen über TOP68
`Force_Heater_State`. Byte 5 war schon belegt: SET2 `HolidayMode` mit
Maske `0x30`, kein Überlapp. `ProtocolByteDecrypt.md` Zeile 10 führt für Byte 5
zusätzlich „Dry Concrete" auf Bits 7+8; auch das bleibt unberührt.

### Die Umsetzung ist je eine Tabellenzeile

`setCommands[]` in [`src/commands.cpp`](src/commands.cpp) trägt Byte, Maske,
Umrechnung, Name und Grenzen in einer Zeile; `subscribe_set_topics()` und
`set_command_range()` laufen über dieselbe Tabelle:

```c
    {37,  9, 0x03, CONV_MUL_INC, "RoomHeaterState",     0,   1,   1}, // blockiert=1 frei=2
    {38,  9, 0x0C, CONV_MUL_INC, "DHWHeaterState",      0,   1,   4}, // blockiert=4 frei=8
    {39,  5, 0x0C, CONV_MUL_INC, "ForceHeater",         0,   1,   4}, // aus=4 an=8
```

So stehen sie seit 3.17.0 im Code — mit den erklärenden Kommentarblöcken
davor, die hier in Abschnitt 1 und 4 begründet sind.

`CONV_MUL_INC` ist `(Wert + 1) * param` und trifft alle drei Wertepaare exakt —
dasselbe Muster wie SET2 `HolidayMode` (`16`/`32`). Nachgerechnet:
`RoomHeaterState 0` → `1*1 = 1`, `RoomHeaterState 1` → `2*1 = 2`.

**Mit geändert:** der Kommentarblock „Why the mask column exists" in
`commands.cpp` listet die geteilten Bytes auf. Byte 9 ist dort eine neue Zeile,
Byte 5 hat `ForceHeater 0x0C` dazubekommen. Wer die Liste nicht pflegt, nimmt
der nächsten Session die einzige Übersicht darüber, welche Kommandos sich ein
Byte teilen.

## 4. Nebenwirkungen der Freigabe

**Die Freigabe ändert das Abtauverhalten.** Laut Servicehandbuch 12.6.2 läuft
der Raumheizstab während der Abtauung mit — allerdings nur, wenn der Backup-
Heizer im Custom Setup überhaupt freigegeben ist. Zweck ist der Schutz des
Plattenwärmetauschers vor Eisbildung; ausgelöst wird es bei niedrigem Vorlauf,
tiefer Außentemperatur oder niedrigem Rücklauf während der Abtauung, und es
hängt nicht am Heizstab-Knopf der Fernbedienung.

Für dieses Vorhaben ist das kein Gegenargument — die Freigabe **soll** die
Regelgröße sein —, aber es hat eine Folge für die Auswertung: In einer
Freigabephase läuft der Heizstab nicht nur dann, wenn die Steuerung ihn haben
wollte, sondern auch bei jeder Abtauung, die in diese Phase fällt. Wer den
Nutzen der gezielten Zuschaltung beziffern will, muss den Abtau-Anteil davon
trennen. Da der Heizstab heute gesperrt ist, läuft er auch beim Abtauen nicht
mit; der Vergleich „vorher/nachher" misst also beides zusammen.

### Die Anlage entscheidet weiter mit

Die Freigabe ist Bedingung (a) von sechs. Die übrigen fünf bleiben in Kraft,
und daraus folgt für eine Steuerung, die gezielt freigibt:

* **Die Freigabe wirkt nicht sofort.** Nach ihr müssen erst 30 min
  Kompressorlauf, 9 min Pumpenlauf und 4 K Vorlaufabweichung zusammenkommen.
  Die Steuerung muss also vorausschauend freigeben, nicht reaktiv im Moment des
  Bedarfs.
* **Schnelles Ein/Aus bringt nichts.** Nach jedem Abschalten des Heizstabs
  sperrt die Anlage ihn 20 Minuten.
* **SET20 muss passen.** Die Außentemperaturschwelle
  (`HeaterOnOutdoorTemp`, TOP78) ist Bedingung (d). Steht sie so, dass die
  Anlage den Heizstab nie in Betracht zieht, bleibt die Freigabe wirkungslos.
  Bei einem seit Jahren deaktivierten Heizstab ist gut möglich, dass der Wert
  nie bewusst gesetzt wurde — **vor dem ersten Versuch prüfen**.
* **Die Bedingung „4 K unter Soll" ist vorhersagbar.** Sie lässt sich aus
  `Main_Outlet_Temp` (TOP6) und `Main_Target_Temp` (TOP7) mitrechnen. Die
  Steuerung kann damit erkennen, ob die Anlage bei freigegebenem Stab
  überhaupt zuschalten würde — und nur dann freigeben.

**Sperren unabhängig vom Schalter.** Der Heizstab läuft laut Handbuch generell
nicht, wenn Vorlauf- oder Rücklaufsensor gestört sind, der Strömungswächter
gestört ist oder die Umwälzpumpe steht. Das ist die Temperatur- und
Durchflussüberwachung, die die Anlage selbst mitführt — ein freigegebener
Heizstab ist damit kein unbeaufsichtigter 3-kW-Tauchsieder.

## 5. Risiken

**Elektrische Leistung — geklärt.** 3 kW fix je Stufe, also bis zu 6 kW
zusätzlich, wenn beide Stufen gleichzeitig zuschalten. Das ist überschaubar,
setzt die Zuschaltung aber unter dieselbe Beobachtung wie alles andere in der
Kaskade: Ob beide Stufen den Heizstab gleichzeitig freigegeben bekommen sollen,
ist eine Entscheidung, keine Selbstverständlichkeit.

**Der 5-min-Re-Assert der Kaskadensteuerung.** Läufe nur im Ruhefenster, sonst
läuft die Messung gegen die Steuerung. Jeden Eingriff einzeln aufrufen — ein
Abbruch mitten in einer Befehlskette greift an dieser Anlage nicht zuverlässig.

**Byte 9 nicht ohne Maske schreiben.** Ein Kommando, das Byte 9 als Ganzes
setzt, löscht das jeweils andere Feld — der Raumheizstab würde die DHW-Freigabe
mit umlegen. Die Maskenspalte deckt das ab, solange sie richtig gesetzt ist.
Zwei Kommandos im selben 500-ms-Sammelfenster sind damit unkritisch; Lauf 4 des
Byte-28-Vorhabens hat gezeigt, dass die Anlage zwei gleichzeitig wechselnde
Bitfelder annimmt.

**`ForceHeater` ist ein Zustand, kein Impuls** wie SET12 `ForceDefrost`.
Praktisch entschärft die Anlage das selbst — die Betriebsart endet mit dem
Fehler, mit „Betrieb aus" oder mit einem Netz-Reset —, aber ein gesetztes
Kommando, das niemand zurücknimmt, bleibt ein loses Ende. Wer SET39 benutzt,
plant das Zurücknehmen mit ein.

## 6. Was offen ist

1. ~~**Nimmt die H-Serie Byte 9 an?**~~ **Beantwortet, 2026-08-28: ja.** M1/M2
   haben Byte 9 im laufenden Mitschnitt wandern lassen (`0x56` → `0x55` →
   `0x56`), TOP59 folgte in beide Richtungen. Wie seinerzeit bei Byte 28.

2. **Was heißt „frei" wirklich?** Unser Code zeigt `Blocked`/`Free`,
   `ProtocolByteDecrypt.md` Zeile 14 schreibt für dieselben Bits „heater
   off/on". Nach Kapitel 12.6.1 ist „Freigabe" richtig: Bedingung (a) ist der
   Schalter, die übrigen fünf Bedingungen entscheidet die Anlage. M2 im
   Messplan prüft, dass das Bit ankommt — ob „frei" auch heißt, dass der Stab
   je läuft, zeigt erst das Winterexperiment über TOP60 und TOP90.

3. ~~**Was steht heute in Byte 9?**~~ **Beantwortet, 2026-08-28 gemessen — und
   der Stand hat sich seit dem Schreiben dieses Dokuments geändert:**

   Anlage | Byte 9 | TOP59 `Room_Heater_State` | TOP58 `DHW_Heater_State` | TOP90 Betriebsstunden
   :--- | :--- | :--- | :--- | ---:
   H1 | `0x56` | **Free** | Blocked | **267 h**
   H2 | — | **Free** | **Free** | **35 h**

   **Warum, ist geklärt (Owner, 2026-08-28):** Der Heizstab war bis zu diesen
   Tests im Installateurmenü **komplett aktiviert**, und der Owner hatte ihn für
   seine eigenen Panel-Versuche über Byte 9 freigegeben. Der Satz „deaktiviert,
   Byte 9 blockiert" weiter oben beschreibt den Stand davor.

   **Damit ist der Messlauf zugleich eine Bestätigung der Zuordnung:** Byte 9
   zeigte genau das, was am Bedienpanel eingestellt war — TOP59 ist der
   Heizstab-Schalter des Panels, nicht irgendein Nachbarbit.

   **Folge für den Messplan:** Er drehte sich um — M1 (`0`) wurde zur
   eigentlichen Änderung, M2 (`1`) zur Rückstellung auf den vorgefundenen Stand.

4. ~~**Die Feineinstellung fehlt uns vermutlich dauerhaft.**~~ **Bestätigt,
   2026-08-28:** Startverzögerung (Byte 104), Start-Delta (105) und Stopp-Delta
   (106) stehen an H1 alle drei auf `0x00` — die Bytes sind in
   `ProtocolByteDecrypt.md` als „J/K/L series" markiert und bei dieser Serie
   leer. **Einziger Stellhebel bleibt die Außentemperaturschwelle SET20**, und
   die ist gesetzt: TOP78 meldet **2 °C**.

## 7. Reihenfolge der Umsetzung

Die ursprüngliche Staffelung („erst SET37, den Rest nur bei Bedarf") ist mit
der Owner-Entscheidung vom 2026-08-28 hinfällig — gebaut sind alle drei.

Schritt | Stand
:--- | :---
**1.** Alle drei Kommandos in `setCommands[]`, Masken im Kommentarblock nachgezogen | **erledigt, 3.17.0**
**2.** Hosttest `test/byte9_test.cpp`, in der CI | **erledigt, 3.17.0**
**3.** `MQTT-Topics.md`, `SET-TOP-Zuordnung.md`, `README.md`, `test/README.md`, Changelog | **erledigt, 3.17.0**
**4.** Mitschnitt auswerten: SET20 / TOP78 und Byte 104–106 im Ist-Zustand (kein Eingriff) | **erledigt, 2026-08-28** — TOP78 = 2 °C, Bytes 104–106 = `0x00`
**5.** OTA auf Stufe 1, Abnahme über `/tablerefresh`, M0–M5 messen (Abschnitt 8) | **erledigt, 2026-08-28** — Abnahme zeilengleich bis auf einen Messwert
**6.** OTA auf Stufe 2 und auf beide Backup-Boards | **erledigt, 2026-08-28** — H2 zeilengleich, `h1b`/`h2b` auf 3.17.0 und weiter auf Port 1884
**7.** Winterexperiment vorbereiten: Freigabekriterium in der Steuerung festlegen, Mitschrieb einrichten | offen

Byte 9 ist ohne Messung bekannt: blockiert, an beiden Anlagen.

## 8. Messplan und Ergebnis

Muster wie beim Byte-28-Vorhaben: Ausgangszustand sichern, ein Bit ändern,
zurücklesen, zurückstellen. Alles an **Stufe 1**. **Gelaufen am 2026-08-28**,
bei ausgeschalteter Anlage im Modus Heizen — jeder Eingriff einzeln aufgerufen,
damit ein Abbruch greift.

### Ergebnis: gelaufen am 2026-08-28, Stufe 1, Anlage aus, Modus Heizen

Firmware 3.17.0 per OTA um 14:07, Abnahme über `test/tablesnap.py` gegen die
Baseline von 14:02: 92 Zeilen, **einzige Abweichung `Inside_Pipe_Temp` 25 → 24**
— ein laufender Messwert. Danach die Messreihe, jeder Eingriff einzeln, Byte im
laufenden Mitschnitt über [`test/byte_monitor.py`](test/byte_monitor.py):

Schritt | Kommando | Byte 9 | TOP59 `Room_Heater_State` | TOP58 `DHW_Heater_State`
:--- | :--- | :--- | :--- | :---
M0 | — | `0x56` | Free | Blocked
M1 | `RoomHeaterState 0` | `0x56` → **`0x55`** | Free → **Blocked** | **Blocked, unverändert**
M2 | `RoomHeaterState 1` | `0x55` → **`0x56`** | Blocked → **Free** | **Blocked, unverändert**

**Die WH-MDC05H3E5 nimmt Byte 9 an** — das war die einzige echte Unbekannte des
Vorhabens. In M1 lag die Flanke zwischen dem zweiten und dritten Telegramm des
Mitschnitts, also im üblichen Rahmen von rund zwei Abfragezyklen.

**Der Maskennachweis fiel stärker aus als geplant.** Byte 9 trägt an dieser
Anlage nicht nur die beiden Heizstab-Felder: Die Bitgruppen 1+2 und 3+4 stehen
beide auf `01`. Bei M1 und M2 wechselte **ausschließlich** die Gruppe 7+8 —
alle drei übrigen blieben stehen. Ohne Maske wäre Byte 9 auf `0x02`
zusammengefallen und hätte drei Felder gleichzeitig umgelegt.

### ForceHeater: M3–M5, dieselbe Sitzung

Die Wärmepumpe stand aus — nach dem Befund aus Abschnitt 1 die einzige Lage, in
der sie das Kommando überhaupt annimmt:

Schritt | Kommando | Byte 5 | TOP68 `Force_Heater_State` | TOP19 / TOP13
:--- | :--- | :--- | :--- | :---
M3 | — (WP aus) | `0x55` | Inactive | Off / Disabled
M4 | `ForceHeater 1` | `0x55` → **`0x59`** | Inactive → **Active** | **unverändert**
M5 | `ForceHeater 0` | `0x59` → **`0x55`** | Active → **Inactive** | **unverändert**

**Auch Byte 5 nimmt die Anlage an, und auch hier wechselte nur die eigene
Bitgruppe** — die Gruppen 1+2 (Zeitprogramm), 3+4 (HolidayMode) und 7+8 blieben
auf `01` stehen.

**ForceHeater wird langsamer übernommen als Byte 9** — die Flanke lag im
Mitschnitt erst beim zehnten von zwölf Telegrammen, grob eine halbe Minute nach
dem Kommando; ein `/tablerefresh` unmittelbar danach zeigte noch `Inactive`.
**Das ist Bauart, kein Befund** (Owner-Einordnung): Die Wärmepumpe prüft erst
ihre Randbedingungen und übernimmt den Wert dann; solche Wartezeiten sind bei
Panasonic üblich. Fürs Prüfen heißt es trotzdem: nicht sofort nach dem Senden
zurücklesen und daraus auf ein verworfenes Kommando schließen.

**TOP60 `Internal_Heater_State` blieb während M4 auf `Inactive`, TOP90 auf
267 h** — der Heizstab lief nicht an. Auch dafür gibt es die Erklärung: Die
Außentemperatur des Tages erzeugte keinen Heizbedarf. Am Bedienpanel wurde der
Heizstab mit kurz auf 40 °C angehobener Zieltemperatur sehr wohl aktiv. Der
Messlauf belegt also die Übertragung des Bits; das Anlaufen des Stabs hängt
weiter an den Bedingungen der Anlage.

**Ausgangszustand wiederhergestellt:** Byte 9 = `0x56`, Byte 5 = `0x55`, TOP59
`Free`, TOP58 `Blocked`, TOP68 `Inactive`, TOP90 unverändert 267 h.

Der Satz, der hier bis 3.17.0 stand — „der Heizstab gehört danach wieder auf
blockiert" —, war eine Folge der falschen Annahme über den Ist-Zustand. Richtig
ist: **wiederhergestellt wird, was M0 vorgefunden hat**, und das war an Stufe 1
`Free`.

### ForceHeater trägt die volle Regelung mit — und startet die Pumpe (2026-08-28, 21:43)

Zweiter Lauf desselben Abends, diesmal mit Mitschrieb, nachdem der Owner das
Verhalten zuerst von Hand gesehen hatte. **Stufe 1 ausgeschaltet**
(`Heatpump_State` = 0), Außentemperatur 17 °C, Pumpe stand, im Ruhefenster des
Re-Assert gestartet (`~/nodered-flows/testfenster.py --warte 240`).

Zeiten aus `test/top_watch.py` im 5-Sekunden-Takt, Kommandos aus dem Sendelog:

Zeit | Ereignis | Anlage
:--- | :--- | :---
21:43:11 | `set/ForceHeater 1` | —
21:43:19 | TOP68 → `Active` | **Pumpe läuft im selben Schritt an**: 0 → 2300 1/min, 11,95 l/min — noch bei Sollwert 20
21:43:27 | `set/Z1HeatRequestTemperature 30` | TOP7/TOP27 folgen 21:43:34
21:43:29–21:43:49 | — | Pumpe regelt sich ein: 2150 → 2200 → 2250 → 2300 1/min
21:45:31 | **TOP60 → `Active`, TOP16 = 3000 W** | Heizstab läuft, Vorlauf steigt
21:45:57 | `set/Z1HeatRequestTemperature 20` | TOP7/TOP27 folgen 21:46:06
21:46:16 | **TOP60 → `Inactive`, TOP16 = 0 W** | Vorlauf 25,0 °C — **Pumpe läuft unverändert weiter**
21:46:37 | `set/ForceHeater 0` | —
21:46:47 | TOP68 → `Inactive` | —
21:46:57 | — | **Pumpe steht** (2300 → 0 1/min)

Der Vorlauf stieg von 22,5 auf 25,5 °C — sein Höchstwert fiel 11 s **nach** dem
Abschalten, die Trägheit des Kreises ist im Mitschrieb also sichtbar. Rücklauf
21,0 → 23,0 °C, Durchfluss konstant rund 12 l/min. 3000 W elektrisch für 3 kW
thermisch — der Heizstab hat keinen Wirkungsgrad zu verlieren.

**Drei Befunde, die vorher nicht dokumentiert waren:**

1. **Die Umwälzpumpe hängt an SET39, nicht am Heizstab.** Sie läuft an, sobald
   TOP68 aktiv wird — noch bevor überhaupt eine Wärmeanforderung besteht —, und
   sie läuft weiter, nachdem der Heizstab abgeschaltet hat. Erst das Zurücknehmen
   von SET39 stoppt sie. **Wer SET39 setzt und vergisst, lässt die Pumpe
   dauerhaft laufen.** Das ist das lose Ende, vor dem Abschnitt 5 warnt, in
   konkreter Form.
2. **Die thermische Regelung arbeitet im Force-Modus mit.** Der Heizstab schaltete
   von selbst ab, als der Vorlauf über die Stoppschwelle stieg — **10 s**, nachdem
   der zurückgenommene Sollwert im Antworttelegramm stand. Auf die Sekunde
   nachrechnen lässt sich die 15-Sekunden-Stoppbedingung damit nicht: Die
   Wärmepumpe hatte den Sollwert schon vor seiner Sichtbarkeit im Telegramm. Der
   Punkt selbst steht — Force Heater ist kein ungeregeltes Durchheizen, sondern
   eine Ersatzwärmequelle **innerhalb** der normalen Vorlaufregelung.
3. **Der Heizstab lief 2:20 min nach dem Kommando an** (1:57 min nach der
   sichtbaren Sollwertanhebung), nicht erst nach den neun Minuten Pumpenlauf, die
   man aus der Handbuchbedingung erwarten würde. Die Bedingung wirkt hier also
   nicht als harte Sperre — warum, ist offen.

4. **Die Übernahme von SET39 schwankt.** Mittags lag die Flanke rund eine halbe
   Minute nach dem Kommando, abends **8 s** (ein) und **10 s** (aus). Die
   Verzögerung ist also keine feste Größe, mit der man rechnen kann — fürs
   Rücklesen bleibt es dabei, eine halbe Minute Geduld einzuplanen.

**Nebenbefund für die Auswertung des Winterexperiments:** TOP90
`Room_Heater_Operations_Hours` blieb über den ganzen Lauf auf 267 h stehen,
obwohl der Stab mit voller Leistung lief. Der Zähler erfasst kurze Läufe nicht.
**Für Läufe unter einer Stunde sind TOP60 und TOP16 die Zeugen, nicht TOP90.**

#### Was daraus folgt: eine Notheizung bei Kompressordefekt

Der Lauf fand bei **ausgeschalteter Wärmepumpe** statt. Damit ist belegt, was
SET39 wirklich wert ist: Fällt der Kompressor aus, lassen sich über MQTT 3 kW je
Stufe in den Heizkreis bringen — mit laufender Umwälzpumpe und unter der
Vorlaufregelung der Anlage, also ohne dass eine externe Steuerung die
Temperaturführung übernehmen müsste. Sie muss nur den Sollwert setzen und SET39
halten.

Das ist **keine Anbindung an den Notbetrieb** und soll auch keine werden — die
Abgrenzung aus Abschnitt 2 bleibt bestehen. Aber es ist der Grund, warum SET39
in 3.17.0 gebaut wurde, und er ist jetzt gemessen statt vermutet.

Genau dieser Befund — die Pumpe hängt am Kommando, nicht am Stab — ist zwei Tage
später zum Anlass für 3.18.0 geworden: Der Notbetrieb **nimmt** SET39 jetzt
zurück, weil ihn sonst niemand zurücknähme. Siehe die Präzisierung in
Abschnitt 2.

### Das Winterexperiment

Ob der Heizstab tatsächlich zuschaltet, lässt sich im Messfenster **nicht**
prüfen: dafür müssten 30 min Kompressorlauf, die Außentemperaturschwelle und
4 K Vorlaufabweichung zusammenkommen. Das ist eine Beobachtung über eine
Frostphase, keine Messung.

#### Der Plan für Winter 2026/27 (Owner, 2026-08-28)

**SET20 `HeaterOnOutdoorTemp` kommt unter den Abtaubereich**, als Grundschwelle
etwa −7 °C statt der heutigen 2 °C. Damit zieht die Wärmepumpe den Heizstab in
ihrer normalen Zuschaltlogik erst bei strengem Frost überhaupt in Betracht — im
Temperaturbereich, in dem abgetaut wird, bleibt er außen vor. Der eigentliche
Test ist dann: **bei etwa −7 °C gezielt mit einem oder zwei Heizstäben
nachhelfen**, wenn es bei Wind und mehreren Frosttagen knapp wird. Ein oder
zwei, weil jede Stufe ihre eigene Schwelle und ihre eigene Freigabe hat — 3 kW
je Stufe, getrennt schaltbar.

**Für die Abtauung braucht diese Anlage den Heizstab nicht.** Belegt durch
mehrere Winter ohne ihn, auch in den seltenen Fällen, in denen beide Stufen
gleichzeitig abtauen: Die Gebäudemasse und der hohe Durchfluss liefern dann
knapp **10 kW Abtauleistung über 5–6 Minuten**, und der Rücklauf sinkt dabei
nur um rund **2 K**. Das ist zugleich die Erklärung, warum die Freigabe hier
gefahrlos als Regelgröße taugt.

⚠️ **SET20 ist aber nicht der Hebel für das Abtau-Mitlaufen.** Die Schutzfunktion
aus Abschnitt 4 (Handbuch 12.6.2) hat **eigene** Auslösekriterien — Vorlauf-,
Rücklauf- und Außentemperatur während der Abtauung — und hängt an der Freigabe
des Backup-Heizers im Custom Setup, ausdrücklich **weder an SET20 noch am
Heizstab-Schalter** (also auch nicht an SET37). Solange der Backup-Heizer im
Installateurmenü aktiviert ist, kann der Stab beim Abtauen also mitlaufen,
selbst wenn SET20 tief steht und SET37 auf blockiert.

Wie wahrscheinlich das ist, hängt an den Rücklauftemperaturen: Die
Rücklaufschwelle der Schutzfunktion liegt in einem Bereich, den eine
Fußbodenheizung bei strengem Frost durchaus streifen kann — also ausgerechnet
im geplanten Testfenster. **Praktische Folge:** `Defrosting_State` (TOP26) ist
im Mitschrieb kein Beiwerk, sondern die Bedingung dafür, dass das Experiment
überhaupt auswertbar ist — der Abtau-Anteil wird **herausgerechnet, nicht
verhindert**.

⛔ **Am Custom Setup wird dafür nichts abgeschaltet.** Solange der Heizstab im
Nutzermenü freigegeben ist, ist die Freigabe im Installateurmenü keine
Stellschraube mehr, mit der man im laufenden Betrieb spielt — die Kombination
aus freigegebenem und zugleich im Servicemenü abgeschaltetem Heizstab ist an
dieser Anlage nicht zulässig. Wer dort etwas ändern will, nimmt vorher die
Freigabe zurück. **Reihenfolge: erst SET37 auf blockiert, dann das Menü.**

**Der zweite Hebel liegt bei der Kaskadensteuerung.** Bedingung (e) — Vorlauf
mehr als 4 K unter Soll — erfüllt eine träge Fußbodenheizung von allein selten.
Die Steuerung setzt den Vorlaufsollwert über SET5 aber selbst: Ein kurzes
Anheben erzeugt die Abweichung gezielt. Genau so ist der Heizstab am
2026-08-28 auch am Bedienpanel aktiv geworden (Zieltemperatur kurz auf 40 °C).
Freigabe und Sollwertanhebung gehören im Experiment deshalb zusammen gedacht.

Mitzuschreiben:

Topic | wofür
:--- | :---
`Room_Heater_Operations_Hours` (TOP90) | der belastbarste Zeuge — zählt nur, wenn der Stab wirklich lief
`Room_Heater_State` (TOP59) | wann die Steuerung freigegeben hat
`Heat_Power_Consumption` (TOP16) | Leistungsaufnahme, zeigt den 3-kW-Sprung
`Defrosting_State` (TOP26) | trennt den Abtau-Anteil vom geregelten Anteil (Abschnitt 4)
`Main_Outlet_Temp` / `Main_Target_Temp` (TOP6/7) | die 4-K-Bedingung, gegen die freigegeben wurde
`Outside_Temp` (TOP14) | Bezug zur Außentemperaturschwelle
`Heater_On_Outdoor_Temp` (TOP78) | die Schwelle selbst — sie wird im Experiment verstellt
`Internal_Heater_State` (TOP60) | ob der Stab in diesem Moment läuft, nicht nur freigegeben ist
`Main_Inlet_Temp` (TOP5) | Rücklauf — entscheidet mit, ob die Abtau-Schutzfunktion anspringt

Die Frage, die das Experiment beantworten soll, ist eine Komfortfrage, keine
Verbrauchsfrage: Kommt die Raumtemperatur in der Frostphase spürbar früher
nach? Der Mehrverbrauch steht ohnehin fest — 3 kW mal Laufzeit aus TOP90.

**Auswertung nicht vergessen:** Ohne TOP26 daneben ist der Abtau-Anteil in TOP90
nicht vom geregelten Anteil zu trennen, und dann misst das Experiment etwas
anderes als das, was gesteuert wurde.

**Werkzeuge:** `test/frame_diff.py` für die Bytes (Ausgabe ist **hexadezimal**),
`test/top_watch.py` für die Rückmeldungen, `test/mqtt_pub.py` zum Senden.
