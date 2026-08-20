# Auftrag: Die Wartung muss den Re-Assert wirklich stilllegen — und von außen sichtbar sein

*Zum Kopieren in eine Session im Projekt `nodered-flows` gedacht; setzt dort
keinen Vorkontext voraus. Gegenstück im Firmware-Repo: `HeishaMonKaskade`,
[`Vorhaben-Notbetrieb-Weboberflaeche.md`](Vorhaben-Notbetrieb-Weboberflaeche.md)
Abschnitt 7.*

## Worum es geht

Zwei Panasonic-Wärmepumpen laufen als Kaskade, gesteuert über zwei
HeishaMon-Bridges. Der *Hauptmodus-Verteiler* schickt alle fünf Minuten einen
Re-Assert an beide Stufen — er setzt die Betriebsart, die Sollwerte und die
Pumpen erneut, damit ein Kommando, das die Wärmepumpe verschluckt hat, spätestens
nach fünf Minuten wieder ankommt. Seit dem 2026-08-20 gehört dazu auch der neue
Ausgang 15 mit `set/HeatingMode` = 1, der die Anlage aus dem Notbetrieb
zurückholt.

Im Firmware-Repo steht der Notbetriebsknopf kurz vor der Abnahme an der
laufenden Anlage. Dieser Testlauf braucht **ein paar Minuten Ruhe auf dem
`set`-Kanal**: Der Knopf-Automat läuft 56 Sekunden, danach ist ein Foto vom
Bedienpanel fällig. Fällt in dieses Fenster ein Re-Assert, holt er die
Wärmepumpe mitten im Lauf in den Direktbetrieb zurück — der Testlauf wäre
wertlos, und im schlechtesten Fall meldet der Knopf GRÜN, obwohl die Anlage
schon wieder zurückgeschaltet wurde.

Dafür gibt es den Wartungsschalter. **Er hat heute nicht gewirkt.**

## Der Befund (gemessen am 2026-08-20 über die ioBroker-simple-api)

Gemessen wurde der Zeitstempel `ts` der Objekte unter
`mqtt.0.panasonic_heat_pump.set.*` — er wandert bei jedem eingehenden Publish
weiter, auch wenn der Wert gleich bleibt.

Zeit | Beobachtung
:--- | :---
20:16:24 | Re-Assert, wie erwartet
~20:18 | Wartung wird gesetzt (Meldung des Owners)
20:18:24 | `QuietMode` — läuft offenbar in einem eigenen Takt
20:21:24 | **Re-Assert erneut, vollständig**

Der Zyklus um 20:21:24 umfasste sieben Topics an `panasonic_heat_pump/set/`,
alle in derselben Sekunde:

```
WaterPump=0   WaterPumpSpeed=100   OperationMode=1   HeatingMode=1
Heatpump=0    Z1CoolRequestTemperature=20   Z1HeatRequestTemperature=20
```

Die Sperre hat also weder den neuen Ausgang 15 (`HeatingMode`) noch die übrigen
Kanäle erreicht.

**Von außen ist nicht feststellbar, ob der Schalter überhaupt stand.** Im
ioBroker gibt es kein Objekt, das den Wartungszustand abbildet — alle 254
Zustände unter `0_userdata.0` wurden durchsucht, gefunden wurden nur zwei
Service-*Text*-Felder (`familie.service_text`, `APP_ServiceText`). Der Zustand
lebt damit ausschließlich im Flow-Kontext.

## Zu klären

1. **Was soll die Wartung sperren?** Alle Kanäle des Verteilers, oder nur
   einen Teil? Falls nur einen Teil: Welche Kanäle laufen bewusst weiter, und
   warum?
2. **Liest der Re-Assert-Zweig den Schalter überhaupt?** Der Verdacht liegt
   nahe, dass die Sperre in der Berechnungsstrecke sitzt und der zyklische
   Re-Assert an ihr vorbeiläuft — er soll ja gerade auch dann senden, wenn sich
   nichts geändert hat.
3. **War der Stand deployt?** Ein nicht deployter Flow erklärt den Befund
   ebenso.
4. **Gibt es einen zweiten Absender?** Gegenprobe, die das sofort klärt:
   Wartung setzen und mitschneiden, wer noch schreibt —

   ```bash
   mosquitto_sub -h 192.168.2.147 -t 'panasonic_heat_pump/set/#' -v
   ```

   Kommt bei gesetzter Wartung weiter etwas herein, sitzt der Absender
   außerhalb des Verteilers (Legacy-Flow, Skript, Adapter).

## Zu bauen

**1. Die Wartung sperrt den Re-Assert vollständig** — einschließlich Ausgang 15.
Solange ein Kanal weiterläuft, ist die Sperre für einen Testlauf an der Anlage
wertlos, denn jeder einzelne der sieben Werte kann den Notbetrieb stören.

**2. Der Zustand wird in ein ioBroker-Objekt gespiegelt.** Das ist der
eigentliche Mangel hinter dem Befund: Ein Betriebszustand, der nur im
Flow-Kontext lebt, lässt sich im Störfall von niemandem prüfen — weder aus dem
Firmware-Repo heraus, noch von jemandem aus der Familie, der später einmal
nachsehen muss, warum die Kaskade schweigt.

Vorschlag: `0_userdata.0.kaskade.Betrieb.KK_Wartung`, boolean, vom Flow mit
`ack = true` gesetzt, sobald der Schalter umgelegt wird — und beim Flow-Start
einmal mitgeschrieben, damit der Zustand nach einem Neustart des Containers
nicht als „unbekannt" dasteht. Der Ort ist ein Vorschlag; wichtig ist nur, dass
das Objekt existiert, den Zustand führt und über `lc` sagt, seit wann er gilt.

**3. Falls die Wartung bewusst nicht alles sperrt:** dann eine zweite, klar
benannte Sperre, die es tut — gedacht für Testläufe an der Anlage, mit demselben
Spiegelobjekt-Prinzip. Lieber zwei ehrliche Schalter als einer, der je nach
Kanal etwas anderes bedeutet.

## Abnahme

1. Wartung **aus**: Der Zeitstempel von
   `mqtt.0.panasonic_heat_pump.set.HeatingMode` wandert alle fünf Minuten
   weiter.
2. Wartung **an**, dann einen vollen Zyklus (mehr als fünf Minuten) verstreichen
   lassen: Der Zeitstempel **bleibt stehen**. Genau das ist am 2026-08-20
   fehlgeschlagen und ist der Kern dieses Auftrags.
3. Dasselbe für `panasonic_heat_pump2` — die zweite Stufe darf ebenso wenig
   weitersenden.
4. Das Spiegelobjekt zeigt in beiden Richtungen den richtigen Zustand.
5. Wartung **aus**: Der nächste Zyklus sendet wieder, unverändert im Umfang.

Prüfbefehl für die Punkte 1, 2 und 5 (die Zeitstempel sind Millisekunden):

```bash
curl -s "http://192.168.2.147:8087/getBulk/\
mqtt.0.panasonic_heat_pump.set.HeatingMode,\
mqtt.0.panasonic_heat_pump.set.OperationMode,\
mqtt.0.panasonic_heat_pump.set.Heatpump,\
mqtt.0.panasonic_heat_pump2.set.OperationMode"
```

## Was hier NICHT zu tun ist

* **Nichts an der Firmware.** Der Notbetriebsknopf ist fertig und läuft seit dem
  2026-08-20 auf Stufe 1 (192.168.2.120).
* **Den Re-Assert nicht ändern.** Er ist richtig so, einschließlich der neuen
  Rückkehr-Zeile. Es geht ausschließlich darum, ihn für ein paar Minuten
  stillegen zu können — und das nachweisen zu können.
* Keine Automatik, die die Wartung nach einer Zeit von selbst aufhebt. Wer sie
  setzt, nimmt sie auch wieder weg; ein Schalter, der von allein zurückspringt,
  ist im Störfall genau der falsche.

## Wenn es länger dauert

Der Testlauf an der Anlage lässt sich notfalls ohne Sperre fahren: Der Takt
liegt sauber auf `:01:24`, `:06:24`, `:11:24` und so fort — direkt nach einem
Zyklus bleiben nach den 56 Sekunden Laufzeit noch gut drei Minuten. Das ist
aber ein Behelf für einen einzelnen Abend, kein Ersatz für den Schalter: Für
Etappe 6 (Lauf mit abgeschaltetem Broker) schweigt Node-RED ohnehin von selbst,
für jede spätere Messung an der laufenden Anlage nicht.
