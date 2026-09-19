# Auftrag: SET40/TOP112 in den `WP_Befehls_Wächter` aufnehmen

> **Ergebnis: offen.**

*Zum Kopieren in eine Session im Projekt `nodered-flows` gedacht; setzt dort
keinen Vorkontext voraus. Gegenstück im Firmware-Repo: `HeishaMonKaskade`,
[`Arbeitsplan-AltExternalSensor.md`](Arbeitsplan-AltExternalSensor.md)
(Entscheid E2, Schritt 28) und [`MQTT-Topics.md`](MQTT-Topics.md) (Abschnitt
„Alternative outdoor sensor", bei den Command Topics).*

## Worum es geht

Firmware 3.23.0 (ausgerollt 2026-09-19, beide Stufen) hat ein neues
Set-Kommando: `SET40 AltExternalSensor`, Rücklesung `TOP112
Alt_External_Sensor`. Es schaltet die Installer-Einstellung für den
alternativen Außenfühler um — `0` = Off (Gehäusefühler), `1` = On (externer
Fühler auf dem Flachdach). Beide Wärmepumpen haben einen eigenen externen
Fühler und stehen auf `On`.

**Owner-Entscheid E2 (Arbeitsplan, 2026-09-19): Umgeschaltet wird nur noch
über den ioBroker-Datenpunkt, nicht mehr am Bedienteil.** Grund: Der
ioBroker-Adapter spielt beim Verbinden den gespeicherten Wert jedes
Set-Topics wieder ein (`SUBSCRIBE_GRACE`, Firmware 3.6.1) — kommt dabei ein
altes `AltExternalSensor` durch, während der Datenpunkt längst auf einem
anderen Wert steht, weiß niemand, welcher Zustand gerade an der Wärmepumpe
gilt. Genau die Klasse Problem, für die der Befehls-Wächter da ist.

## Was zu tun ist

`SET40 AltExternalSensor` / `TOP112 Alt_External_Sensor` als neuntes
Kanalpaar je Stufe in `WP_Befehls_Waechter.js`
(`iobroker-js/common/kaskade/`) aufnehmen, nach demselben Muster wie die
bestehenden acht: 5-Minuten-Karenz, LWT-Vorbedingung, Stale-Guard über `ts`,
„unlesbar ⇒ unprüfbar statt abweichend". (Stand von acht Kanälen aus einer
18 Tage alten Notiz — bitte im aktuellen Skript gegenprüfen, nicht blind
übernehmen.)

**Bewusst NICHT im `WP_Installer_Waechter.js`.** Der prüft TOP105–111 gegen
einen im Code hinterlegten, festen Sollstand — dafür ist er gebaut: eine
falsche Installer-Einstellung soll aus Versehen auffallen. `TOP112` wechselt
aber **gewollt zweimal im Jahr** (Sommer/Winter), ein fester Sollstand wäre
hier falsch angewendet.

## Abnahme

1. `set.AltExternalSensor` beider Stufen auf den aktuellen Wert setzen (`1`)
   und einen vollen Wächter-Takt abwarten: keine Meldung.
2. Testweise `TOP112` state-seitig von der Firmware abweichen lassen (z. B.
   kurz `set/AltExternalSensor` auf den anderen Wert senden und die
   Rückschaltung verzögern) — der Wächter muss die Abweichung wie bei den
   bestehenden acht Kanälen melden.
3. Broker/LWT kurz kappen: Kanal muss als „unprüfbar", nicht als
   „abweichend" gelten.

## Was hier NICHT zu tun ist

* Keine Änderung an `WP_Installer_Waechter.js` — TOP112 gehört dort nicht
  hinein, siehe oben.
* Keine Automatik, die `AltExternalSensor` selbst nach Datum oder
  Außentemperatur umschaltet — das ist nach Owner-Entscheid E2/E4 im
  Arbeitsplan bewusst nicht Teil dieses Auftrags und bräuchte einen eigenen,
  separaten Auftrag.
