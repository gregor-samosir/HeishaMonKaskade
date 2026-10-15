# Auftrag: Notbetriebswerte-Sender rundet auf Ganzzahlen

> **Ergebnis: offen** — Angelegt am 2026-10-15 aus der Abnahme von 3.23.1
> an Stufe 1. Noch nichts umgesetzt. Owner-Entscheid 2026-10-15: Der Fix
> kommt in den **Sender** (Option 1), die Firmware bleibt streng.

*Zum Kopieren in eine Session im Projekt `nodered-flows` gedacht; setzt dort
keinen Vorkontext voraus. Gegenstück im Firmware-Repo: `HeishaMonKaskade`,
[`src/notbetrieb.cpp`](src/notbetrieb.cpp) (`notbetrieb_mqtt_annehmen`) und
[`Vorhaben-Notbetrieb-Weboberflaeche.md`](Vorhaben-Notbetrieb-Weboberflaeche.md), §3.*

## Worum es geht

Der Flow-Knoten **`Notbetriebswerte-Sender V1.0`** (`nb_sender`) beschickt den
MQTT-Zweig `<prefix>/notbetrieb/<Name>` mit den Kurvenwerten aus
`0_userdata.0.kaskade.Konfiguration.KK_Heizkurve.*`. Er prüft nur den
Bereich und sendet den Wert **unverändert**. Seit 2026-10-10 19:32 stehen
dort Dezimalwerte:

| Quelle (ioBroker) | Wert | Ziel (MQTT) |
| --- | --- | --- |
| `KK_HK_vlLo` | 33.5 | `panasonic_heat_pump/notbetrieb/Z1HeatCurveTargetHighTemp` |
| `KK_HK_vlHi` | 25.5 | `panasonic_heat_pump/notbetrieb/Z1HeatCurveTargetLowTemp` |

Die Firmware nimmt auf diesem Zweig **nur Ganzzahlen** an (`strtol`, jedes
Restzeichen macht die Nachricht ungültig — so streng wie im Set-Pfad, denn die
Panasonic-Software rechnet intern nur mit ganzen Grad). `33.5` wird deshalb
nicht gerundet, sondern ganz verworfen; im Log der Bridge steht bei jedem
Verbinden:

```text
Notbetrieb: Z1HeatCurveTargetHighTemp ist keine Zahl (33.5) - verworfen
Notbetrieb: Z1HeatCurveTargetLowTemp ist keine Zahl (25.5) - verworfen
```

**Folge.** Die Bridge hält für beide Kurvenpunkte den zuletzt akzeptierten
Ganzzahlwert (der RTC-Spiegel überlebt OTA und Software-Reset). Der Knopf ist
deshalb heute bereit, fährt aber mit alten Werten. Nach einem **echten
Stromausfall** ist der Spiegel leer; der Broker spielt 33.5 und 25.5 ein, beide
werden verworfen, die Werte fehlen, und der Notbetriebsknopf ist gesperrt
(Sperre 1, „Werte fehlen“) — obwohl der ioBroker da ist.

Stufe 2 ist nicht betroffen: ihr einziger Wert (`DHWTemp`, 48) ist ganzzahlig.

## Was zu tun ist

`nb_sender` auf **V1.1** bringen:

1. **Runden vor der Bereichsprüfung.** Nach der `isFinite`-Prüfung in §2:
   `const n = Math.round(roh)` und danach erst gegen `min`/`max` prüfen.
   Gesendet und gecacht wird der **gerundete** Wert. Dieselbe Regel wie
   `iobroker-js/common/kaskade/KKkuehlkurve.js` (`Math.round`), damit Notbetrieb
   und Direktbetrieb dieselbe Zahl sehen.
2. **Sichtbar machen, wenn gerundet wurde.** Eine `node.warn`-Zeile
   `VL kalt: 33.5 auf 34 gerundet (Firmware nimmt nur Ganzzahlen)` — nur wenn
   `roh !== n`. Sonst fällt nie auf, dass die Notbetriebskurve um bis zu 0,5 K
   von der Direktkurve abweicht.
3. **Die Bereichsprüfung gilt für den gerundeten Wert.** 55.4 → 55 wird
   gesendet, 55.5 → 56 liegt außerhalb und wird (wie bisher) nicht gesendet.
   Nicht klemmen — die bestehende Regel „keine Werte erfinden“ bleibt.
4. **§4 (Plausibilität) und §5 (Status)** arbeiten mit den gerundeten Werten
   aus dem Cache; dort ist nichts zu ändern, nur zu prüfen.
5. **Versionspflege nach Memory-Regel:** `LOG_TAG`, Nodename und Header-Changelog
   auf V1.1 (`V1.1 — rundet Dezimalwerte auf Ganzzahlen; Firmware verwirft
   sonst den ganzen Wert`).

**Nicht anfassen:** die Konfigurationswerte selbst. Die Berechnung der
Vorlaufwerte in Node-RED arbeitet sinnvoll mit 33.5; gerundet wird nur auf dem
Weg zur Bridge.

**Rundungsregel, ausdrücklich:** `Math.round` rundet .5 Richtung +∞ (33.5 → 34,
25.5 → 26, aber −10.5 → −10). Für die vier Kurvenpunkte unkritisch; wer eine
andere Regel will, entscheidet das hier, nicht in der Firmware.

## Abnahme

Der Sender schickt bei Flow-Start alle fünf Werte neu (`fireOnStart`, Cache
`memoryOnly`) — ein Deploy genügt, es ist kein manueller Inject nötig.

1. **Offline, wie beim Befehls-Wächter:** kleine Skriptdatei mit Mocks unter
   `tests/notbetriebssender/`. Mindestens: `33.5 → 34`, `25.5 → 26`, `33 → 33`
   (keine Warnzeile), `"abc"` und `null` → nichts gesendet, `19.6 → 20`
   (gesendet), `19.4 → 19` (außerhalb 20..55, nicht gesendet), `55.5 → 56`
   (nicht gesendet), `-10.5 → -10`. Gegenprobe von Hand: Rundung ausbauen →
   die ersten beiden Fälle werden rot.
2. **An der Anlage, lesend:** nach dem Deploy
   `mqtt.0.panasonic_heat_pump.notbetrieb.Z1HeatCurveTargetHighTemp` = **34** und
   `…TargetLowTemp` = **26**, `ts` nach dem Deploy.
3. **Passiv:** Beim nächsten Verbinden der Bridge steht im `/log`
   (Notbetriebs-Zugang) **keine** Zeile „ist keine Zahl“ mehr. Bis dahin
   nichts erzwingen — ein Neustart der Bridge nur zu diesem Zweck ist die
   Sache nicht wert.

## Was dieser Auftrag nicht entscheidet

Ob die **Firmware** künftig Dezimalwerte selbst rundet, ist offen und bleibt es:
Sie würde gegen die Quelle schützen, die sich nicht an die Regel hält, macht
den Notbetriebszweig aber lockerer als den Set-Pfad. Falls die Firmware ohnehin
angefasst wird (3.24.0, Durchsicht 2026-10-15, S2/S3), kann das mitlaufen — als
eigener Owner-Entscheid, nicht als Beiwerk.
