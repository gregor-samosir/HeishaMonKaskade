# Auftrag: Der Notbetrieb muss `SET39 ForceHeater` zurücknehmen

> **Beantwortet am 2026-08-30 — angenommen, gebaut und ausgeliefert in 3.18.0.**
> Der Befund stimmt in allen Punkten: `SET39` kam in keiner der beiden
> Schrittfolgen vor, und niemand hätte den Zustand zurückgenommen. Beide Folgen
> haben jetzt an **Position 2** den Schritt `ForceHeater = 0` — genau dort, wo
> ihr ihn vorgeschlagen habt: hinter der Hydraulik, vor `OperationMode`.
> Heizen dauert damit 80 s (zehn Schritte), Warmwasser 48 s (sechs).
>
> **Zu euren drei offenen Fragen aus §4:**
>
> 1. *Wie die Wärmepumpe reagiert, wenn `Heatpump = 1` gesetzt wird, während
>    `SET39` noch steht* — die Frage stellt sich nicht mehr. Der Schritt steht
>    acht Schritte vor dem Einschalten; wird die Rücknahme nicht bestätigt,
>    endet der Lauf dort in ROT und es geht **kein** weiteres Kommando an die
>    Wärmepumpe.
> 2. *Ob `SET39 = 0` bei laufender Einheit angenommen wird* — im Notbetriebsfall
>    irrelevant, und zwar aus eurem eigenen §3: Die Modi 5/6 setzen
>    `Heatpump = 0`, die Einheit ist beim Drücken also aus. Ungemessen bleibt der
>    Randfall „Einheit läuft **und** `SET39` steht", der nur durch einen
>    Handeingriff entstehen kann. Er kostet dort ein ROT und einen zweiten
>    Druck — die Anlage bleibt dabei unangetastet.
> 3. *Modus 11, 2-stufig gegen 1-stufig* — relevant, aber nicht firmwareseitig
>    lösbar: **Jede Bridge spricht nur mit ihrer eigenen Wärmepumpe.** Lief die
>    Anlage mit 6 kW, muss der Knopf an **beiden** Bridges gedrückt werden, sonst
>    bleibt an der anderen Stufe der Auftrag stehen und **ihre Umwälzpumpe läuft
>    weiter** — die hängt am Kommando, nicht am Stab (unsere Messung 2026-08-28).
>    Das steht jetzt in `Ablauf-Notbetrieb.md` §1b und im README; ins grüne Panel
>    kommt es bewusst nicht (Owner-Entscheid 2026-08-30).
>
> **Zwei Dinge, die euer Auftrag nicht wissen konnte:**
>
> * **Das Schritt-Timeout liegt bei 20 s** und bleibt dort. `MQTT-Topics.md`
>   hält für `SET39` eine Übernahme von bis zu einer halben Minute fest — das
>   galt dem **Einschalten**. Für die **Rücknahme** nennt euer Erstlauf (§5) 7 s
>   an beiden Stufen, und das liegt innerhalb der Mindestwartezeit von 8 s, die
>   der Schritt ohnehin absitzt.
> * **Modus 10 bleibt außen vor.** Mischer zu und VH-Pumpe aus hängen an eurer
>   Seite; der Notbetrieb kann sie nicht zurückholen. Wer aus Modus 10 heraus
>   drückt, bekommt Wärme — weiterhin vorrangig ins Hinterhaus. Nur dokumentiert.
>
> **Zu §5 (umgekehrte Richtung): nein, kein Sperren.** Eure Begründung trägt —
> der Knopf muss gerade dann funktionieren, wenn die Steuerung nicht mehr
> antwortet. Eine Notbetriebs-Sperre, die von einer lebenden Steuerung abhängt,
> wäre an dieser Stelle das Gegenteil einer Absicherung. Zurücknehmen statt
> sperren, wie vorgeschlagen.
>
> **Was ihr auf eurer Seite nicht ändern müsst:** nichts. Der Re-Assert darf
> `SET39` nach einem Notbetriebslauf ruhig wieder setzen; weil die Modi 5/6
> `heatpump` vor `heater` senden, geht die Einheit dabei zuerst wieder aus und
> das Kommando wird angenommen. Der achte Wächter-Kanal wird im Notbetrieb
> anschlagen — das ist Wartungssignatur, kein Befund, und steht jetzt so in
> `Ablauf-Notbetrieb.md` §6.

> **Absender:** Repo `nodered-flows`, 2026-08-30. **Zu prüfen:** ob der
> Notbetriebsablauf einen zusätzlichen Schritt braucht — und wo er hingehört.
> Owner-Einschätzung beim Übergeben: ja, vor dem Notbetrieb ausschalten.

## 1. Was sich in der Steuerung geändert hat

Seit dem 2026-08-30 benutzt Node-RED `SET39 ForceHeater` im Regelbetrieb. Drei
neue Heizmodi (App-Menü 9/10/11) ersetzen den Verdichter durch den Backup-Heizstab
der Wärmepumpen — 3 kW an Stufe 1, 6 kW mit beiden.

| App-Modus | Hauptmodus | `SET39` an | `SET1 Heatpump` |
|---|---|---|---|
| 9 · Heizstab 3 kW | 5 | H1 | **0** an beiden |
| 10 · Heizstab 3 kW HH-Boost | 5 | H1 | **0** an beiden |
| 11 · Heizstab 6 kW | 6 | H1 + H2 | **0** an beiden |

`Heatpump = 0` ist dabei Voraussetzung, nicht Nebenwirkung: Die WP nimmt `SET39`
nur bei ausgeschalteter Einheit an (eure Messung 2026-08-28, `MQTT-Topics.md`).

**Wichtig für euch:** `SET39` kommt aus dem Hauptmodus-Verteiler und liegt damit
im **5-Minuten-Re-Assert** wie die übrigen 13 WP-Kanäle. In allen anderen Modi
sendet die Steuerung aktiv `SET39 = 0`. Der Kanal wird seit heute auch vom
WP-Befehls-Wächter quittiert (`set/ForceHeater` gegen TOP68).

## 2. Der Befund

`SET39` kommt in **keiner** der beiden Schrittfolgen aus
[`Ablauf-Notbetrieb.md`](Ablauf-Notbetrieb.md) vor — weder in den neun Schritten
der Rolle Heizen noch in den fünf der Rolle Warmwasser. Beide Folgen enden mit
`Heatpump = 1`.

Läuft die Anlage in Modus 9/10/11 und wird der Notbetriebsknopf gedrückt, steht
`ForceHeater` in der Wärmepumpe also **weiter auf 1, während die Firmware sie
einschaltet**. `SET39` ist ein Zustand, den niemand automatisch zurücknimmt.

Zwei Fälle, die sich unterscheiden:

* **Steuerung lebt.** Der Notbetrieb wird ohnehin binnen ≤5 min vom Re-Assert
  überschrieben (bekanntes Verhalten, betrifft alle Kanäle). Neu ist nur, dass
  dabei auch `SET39 = 1` wieder gesetzt wird — konsistent, aber ein
  firmwareseitiges `SET39 = 0` hält in diesem Fall nicht.
* **Steuerung tot** — der eigentliche Notbetriebsfall. Niemand nimmt `SET39`
  zurück. Die Anlage läuft mit `Heatpump = 1` nach eigener Kurve, während der
  Heizstab-Befehl steht.

## 3. Vorschlag

Ein Schritt `SET39 ForceHeater = 0`, **vor** `Heatpump = 1` in beiden Folgen.
Nach der Logik eurer bestehenden Reihenfolge („Vor allem anderen an der WP",
Schritt 2) gehört er weit nach vorn — Vorschlag: **direkt nach dem
Hydraulikschritt, vor `OperationMode`**. Bricht der Lauf danach ab, ist die
Anlage in einem Zustand, in dem sie nichts mehr tut; das ist die sichere Seite.

An Stufe 2 (Warmwasser) gilt dasselbe: Modus 11 setzt `SET39` auch an H2.

## 4. Was wir nicht beurteilen können

* Wie die Wärmepumpe reagiert, wenn `Heatpump = 1` gesetzt wird, während `SET39`
  noch steht. Eure Doku sagt nur, dass das Bedienpanel die *Anforderung* bei
  laufender Einheit ablehnt — über einen bereits stehenden Zustand steht dort
  nichts.
* Ob `SET39 = 0` bei laufender Einheit überhaupt angenommen wird. Falls nicht,
  müsste der Schritt zwingend vor `Heatpump = 1` liegen — was der Vorschlag
  oben ohnehin tut.
* Modus 11 fährt 2-stufig, der Notbetrieb stellt in Schritt 1 auf 1-stufig.
  H2 hätte dann `SET39` stehen und wäre hydraulisch abgekoppelt. Ob das relevant
  ist, könnt ihr besser einschätzen als wir.

## 5. Umgekehrte Richtung

Falls ihr `SET39` in die Notbetriebs-Sperre aufnehmen wollt (analog zur
TOP101-Prüfung), sagt Bescheid — dann kann die Steuerung den Kanal vor einem
geplanten Notbetrieb selbst räumen. Aus unserer Sicht ist Zurücknehmen aber
besser als Sperren: Der Knopf soll gerade dann funktionieren, wenn die Steuerung
nicht mehr antwortet.

**Referenz auf unserer Seite:** `nodered-flows/HEIZSTAB-MODI.md` (§3 zur
`Heatpump = 0`-Bedingung, §4 zur Befehlsausgabe, §6a zur Bedeutung von
TOP68 gegenüber TOP60).
