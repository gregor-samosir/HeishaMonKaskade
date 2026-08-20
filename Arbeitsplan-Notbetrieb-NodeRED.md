# Arbeitsplan: Node-RED-Seite des Notbetriebs

*Dieser Plan ist zum Kopieren in eine Session im Projekt `nodered-flows` gedacht
und setzt dort keinen Vorkontext voraus. Er kann unabhängig von der
Firmware-Arbeit erledigt werden — die Firmware ignoriert die neuen Topics,
solange sie sie nicht kennt.*

Gegenstück im Firmware-Repo: `Vorhaben-Notbetrieb-Weboberflaeche.md`
(HeishaMonKaskade), Abschnitte 3 und 5.

## Worum es geht

Die Kaskade wird von zwei HeishaMon-Bridges an zwei Panasonic-Wärmepumpen
gesteuert. Fällt der ioBroker aus, fehlt nicht nur der Absender der Kommandos,
sondern auch der MQTT-Broker selbst — der Broker **ist** der
ioBroker-MQTT-Adapter. Die Firmware bekommt ab dann nichts mehr.

Deshalb bekommt jede Bridge einen Knopf auf ihrer eigenen Weboberfläche, der
die Wärmepumpe in einen Notbetrieb schaltet: Stufe 1 heizt nach ihrer eigenen
Kurve, Stufe 2 macht Warmwasser. Dafür muss die Firmware die Kurvenwerte
**vorher schon kennen** — sie hält sie im RAM, gespeist aus einem eigenen
MQTT-Zweig, den Node-RED beschickt.

**Das ist die Aufgabe hier: diesen Zweig zu beschicken.**

## Teil 1 — Die Notbetriebswerte senden

### Topics

Ein neuer Zweig `<prefix>/notbetrieb/<Name>` neben dem bestehenden
`<prefix>/set/<Name>`. Die Namen sind identisch mit den Set-Kommandos, damit
die Firmware dieselbe Bereichsprüfung anwenden kann.

**Stufe 1 — `panasonic_heat_pump` (192.168.2.120), Rolle Heizen:**

| Topic | Quelle (ioBroker) | Wert heute | Bereich |
| --- | --- | ---: | --- |
| `panasonic_heat_pump/notbetrieb/Z1HeatCurveTargetHighTemp` | `0_userdata.0.kaskade.Konfiguration.KK_Heizkurve.KK_HK_vlLo` | 34 | 20..55 |
| `panasonic_heat_pump/notbetrieb/Z1HeatCurveTargetLowTemp` | `…KK_Heizkurve.KK_HK_vlHi` | 26 | 20..55 |
| `panasonic_heat_pump/notbetrieb/Z1HeatCurveOutsideLowTemp` | `…KK_Heizkurve.KK_HK_atLo` | −10 | −15..15 |
| `panasonic_heat_pump/notbetrieb/Z1HeatCurveOutsideHighTemp` | `…KK_Heizkurve.KK_HK_atHi` | 15 | −15..15 |

**Stufe 2 — `panasonic_heat_pump2` (192.168.2.122), Rolle Warmwasser:**

| Topic | Quelle (ioBroker) | Wert heute | Bereich |
| --- | --- | ---: | --- |
| `panasonic_heat_pump2/notbetrieb/DHWTemp` | `0_userdata.0.kaskade.Konfiguration.KK_Warmwasser.DHW_Target_Temp` | 48 | 40..75 |

Mehr nicht. Der Betriebsmodus von Stufe 2 (`OperationMode` = 3, DHW only) ist
in der Firmware fest verdrahtet und braucht keinen Konfigurationswert.

### Die Kreuzzuordnung ist der Punkt, an dem es schiefgehen kann

**`vlLo` geht nach `TargetHigh`, `vlHi` nach `TargetLow`.** Das sieht nach einem
Tippfehler aus, ist aber richtig:

* Im ioBroker-Konfigurationsbaum beziehen sich `Hi`/`Lo` **immer** auf die
  Außentemperatur — `vlLo` ist der Vorlauf am kalten Stützpunkt `atLo`.
* Bei Panasonic benennen nur die `Outside_*`-Felder Außentemperaturen; die
  `Target_*`-Felder benennen **Vorlauf**temperaturen. `Target_High` ist die
  höhere Vorlauftemperatur — und die gehört zum kalten Ende.

Am 2026-08-20 an WP1 gemessen und in `MQTT-Topics.md` des Firmware-Repos
dokumentiert. `test/kurven_sync.py` hatte dieselbe Verwechslung und ist seither
korrigiert. Wer hier `vlLo` nach `TargetLow` legt, baut eine Kurve, die bei
Kälte zu kalt und bei Wärme zu heiß fährt.

### Wann gesendet wird

Bei **Änderung** des jeweiligen Konfigurationswertes, dazu einmal beim Start des
Flows. Kein zyklisches Nachsenden nötig: Der ioBroker-MQTT-Adapter speichert den
letzten Wert jedes Topics und spielt ihn jedem neuen Abonnenten wieder ein —
genau davon lebt der Notbetrieb. Startet eine Bridge neu, sind ihre Werte binnen
Sekunden von allein wieder da.

### Was NICHT passieren darf

* **Diese Topics gehen nie an die Wärmepumpe.** Die Firmware hält sie im RAM und
  schickt sie erst los, wenn jemand den Knopf drückt. Ein `notbetrieb`-Topic
  darf niemals versehentlich als `set`-Topic gesendet werden — insbesondere
  `Z1HeatCurveTargetHighTemp` nicht: Das ist im laufenden Direktbetrieb
  dieselbe Speicherstelle wie die aktive Vorlauf-Solltemperatur und würde den
  Betrieb verstellen. Genau deshalb überträgt `kurven_sync.py` diesen einen
  Wert nicht.
* **Keine Werte erfinden.** Fehlt ein Konfigurationswert oder ist er keine
  gültige Zahl: gar nicht senden und eine Warnung loggen. Die Firmware sperrt
  den Knopf, wenn ihr ein Wert fehlt — das ist gewollt und besser als ein Knopf,
  der auf die Panasonic-Werkskurve schaltet (55 °C bei −5 °C).

## Teil 2 — Die Rückkehr aus dem Notbetrieb

Aus dem Notbetrieb holt **Node-RED** die Anlage zurück, nicht die Firmware. Eine
Firmware-Lösung ist verworfen: Ihr Notbetriebs-Zustand läge im RAM, nach einem
Neustart wüsste sie nichts mehr davon und würde nie zurückschalten.

**Zu bauen:** Der zyklische Re-Assert des Verteilers sendet zusätzlich
`panasonic_heat_pump/set/HeatingMode` = 1. Steht die Anlage im Notbetrieb
(Kurvenbetrieb), holt das sie von allein in den Direktbetrieb zurück, den die
Kaskadenregelung dann wieder bedient.

**Die Bedingung ist wichtiger als die Zeile:** Gesendet werden darf das nur,
wenn die Kaskadenregelung **tatsächlich rechnet** — ein Herzschlag, nicht bloß
„der Container läuft". Sonst holt es die Wärmepumpe aus einer funktionierenden
Kurve in einen Direktbetrieb, den niemand nachführt, und das Haus kühlt aus,
während alles „läuft".

Belegt am 2026-08-20 an WP1: Wiederholtes `set/HeatingMode 1` im bereits
laufenden Direktbetrieb ist folgenlos (31 s beobachtet, keine Änderung an
TOP7/27/28/29/30/76). Die Zeile darf also bedenkenlos in jedem Zyklus mitgehen.

## Teil 3 — `NOTBETRIEB.md` im Node-RED-Projekt

Kurz halten, für den Fall gedacht, dass jemand anderes danach sucht:

* welche Topics der Flow beschickt und woraus
* die Kreuzzuordnung mit einem Satz Begründung
* die Rückkehr-Zeile samt Herzschlag-Bedingung
* der Hinweis, dass die Bedienung im Notbetrieb am Bedienpanel der Wärmepumpe
  stattfindet, nicht in Node-RED

## Abnahme

1. `mosquitto_sub -h 192.168.2.147 -t 'panasonic_heat_pump/notbetrieb/#' -v`
   und einmal einen Konfigurationswert ändern — die Nachricht muss kommen.
2. Denselben Abonnenten neu verbinden: Der Adapter muss alle fünf Werte
   wiedereinspielen. Das ist der Mechanismus, auf dem der Notbetrieb ruht.
3. Gegenprobe, dass an `<prefix>/set/Z1HeatCurveTargetHighTemp` **nichts**
   gesendet wurde — dort darf im Direktbetrieb nichts ankommen.
4. Nach Teil 2: `HeatingMode` im Re-Assert mitsenden und prüfen, dass die
   Sollwerte der laufenden Regelung unverändert bleiben.

## Was hier NICHT zu tun ist

Der Knopf, die Weboberfläche, die Schrittfolge und das Rücklesen liegen
vollständig in der Firmware (Repo HeishaMonKaskade, Version 3.12.0 in Arbeit).
Node-RED liefert nur die Werte und holt die Anlage später zurück.
