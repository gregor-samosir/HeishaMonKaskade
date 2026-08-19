# Vorhaben: Betriebsart Kurve ↔ Direkt als Set-Kommando (Byte 28)

Übergabe für eine eigene Session. Ziel sind zwei neue Set-Kommandos, mit denen
sich die Wärmepumpe zwischen **Kompensationskurve** und **Direktvorgabe**
umschalten lässt — heute geht das nur am Bedienterminal.

**Stand dieser Datei:** 2026-08-19, Firmware 3.10.0 auf beiden Stufen.
Vorarbeit und Begründung stehen in
[`SET-TOP-Zuordnung.md`](SET-TOP-Zuordnung.md), Abschnitt 4.

---

## 1. Warum das der lohnendste offene Punkt ist

Der Notbetrieb der Anlage ist heute **halb automatisiert**: Fällt die
Node-RED-Kaskadensteuerung aus, soll die Wärmepumpe auf ihrer eigenen Heizkurve
weiterlaufen. Die Kurvenwerte werden dafür laufend aus dem ioBroker gespiegelt
(SET27–SET34, `test/kurven_sync.py`) — aber **die Umschaltung selbst muss ein
Mensch am Bedienterminal machen**. Genau dieser eine Handgriff bliebe liegen,
wenn niemand im Haus ist.

Mit einem Set-Kommando auf Byte 28 wäre der Notbetrieb vollständig
fernschaltbar. Das Rücklesen existiert bereits (TOP76 `Heating_Mode`, TOP81
`Cooling_Mode`), der Nachweis kostet also nichts extra.

## 2. Was belegt ist

**Byte 28 trägt beide Betriebsarten in zwei Bitfeldern.** Das ist nicht nur aus
der Referenz übernommen, sondern rechnerisch geprüft: Alle vier in
`ProtocolByteDecrypt.md` genannten Rohwerte ergeben mit den Dekodierern aus
`decode.cpp` genau die dort beschriebene Bedeutung.

Rohwert | Bits 5+6 → TOP81 `Cooling_Mode` | Bits 7+8 → TOP76 `Heating_Mode` | Referenztext
:--- | :--- | :--- | :---
`0x05` | `01` → 0 Kurve | `01` → 0 Kurve | „both compensation curves"
`0x09` | `10` → 1 Direkt | `01` → 0 Kurve | „compensation curve heat and direct cool"
`0x06` | `01` → 0 Kurve | `10` → 1 Direkt | „heat direct, cool compensation curve"
`0x0A` | `10` → 1 Direkt | `10` → 1 Direkt | „direct heat and direct cool"

Vier von vier stimmen. Die Leseseite gilt damit als sicher.

## 3. Was offen ist

**Nimmt die Wärmepumpe Byte 28 im Kommandotelegramm überhaupt an?** Das ist die
einzige echte Unbekannte. Byte 28 liegt innerhalb der 110 Bytes des Kommandos,
die Adresse existiert also — aber ob die Wärmepumpe dort schreiben lässt, ist
unbelegt. Das Original-HeishaMon-Projekt hat kein Kommando dafür, es gibt also
auch keine Fremderfahrung.

## 4. Vorschlag für die Umsetzung

Zwei Zeilen in `setCommands[]` ([`src/commands.cpp`](src/commands.cpp)). Die
Umrechnung ist aus den vorhandenen Dekodierern zurückgerechnet, nicht geraten:

```c
{35, 28, 0x03, CONV_MUL_INC, "HeatingMode",  0, 1, 1},   // 0=Kurve, 1=Direkt
{36, 28, 0x0C, CONV_MUL_INC, "CoolingMode",  0, 1, 4},   // 0=Kurve, 1=Direkt
```

Gegenprobe der Kodierung:

* `HeatingMode` (Maske `0x03`, `getBit7and8` liest `(b & 0b11) - 1`):
  Wert 0 → `(0+1)*1` = `0b01` → gelesen 0. Wert 1 → `0b10` → gelesen 1. ✓
* `CoolingMode` (Maske `0x0C`, `getBit5and6` liest `((b >> 2) & 0b11) - 1`):
  Wert 0 → `(0+1)*4` = `0x04` → gelesen 0. Wert 1 → `0x08` → gelesen 1. ✓

**Die Masken sind hier nicht optional.** Byte 28 trägt zwei Felder; ohne
bitgenaue Maske schaltet ein Kühl-Kommando die Heizung mit um. Genau diese
Fehlerklasse hat 3.1.0 beseitigt — die Maskenspalte existiert dafür.

SET35 und SET36 sind die nächsten freien Nummern (höchste vergebene: SET34).

## 5. ⚠️ Die wichtigste Warnung: Der Test ist NICHT folgenlos umkehrbar

**Ein Wechsel von Direkt auf Kurve setzt alle vier Kurvenpunkte je Kurve auf die
Panasonic-Werksvorgaben zurück** — und das Zurückschalten stellt sie *nicht*
wieder her. Am 2026-08-11 an WP2 am Bedienterminal beobachtet, dokumentiert in
[`test/README.md`](test/README.md).

Werksvorgaben, die die Wärmepumpe dabei einträgt:

```text
Heizkurve:  55 °C bei -5 °C   und  35 °C bei +15 °C
Kühlkurve:  15 °C bei 20 °C   und  10 °C bei 30 °C
```

Zwei weitere Nebeneffekte aus derselben Beobachtung:

* Im Kurvenbetrieb melden **TOP27/TOP28 den Wert 0** (die Anforderungstemperatur
  ist dort ohne Bedeutung).
* Der **Direktsollwert geht beim Roundtrip verloren**: Er übernimmt beim
  Zurückschalten den unteren Kurvenpunkt (gemessen damals: vorher 20, danach 35
  beim Heizen bzw. 10 beim Kühlen).

**Folge für den Testplan:** Ein *erfolgreicher* Umschalttest zerstört die
gespiegelte Kurvenkonfiguration und verstellt den Sollwert. Beides muss danach
aktiv wiederhergestellt werden — `test/kurven_sync.py` für die Kurve, und der
5-min-Re-Assert von Node-RED zieht den Sollwert nach. Das ist einzuplanen, nicht
zu hoffen.

## 6. Ausgangszustand (2026-08-19, beide Stufen)

Vor dem Test gegenprüfen, ob das noch stimmt.

Wert | Stufe 1 | Stufe 2
:--- | ---: | ---:
TOP76 `Heating_Mode` | 1 (Direkt) | 1 (Direkt)
TOP81 `Cooling_Mode` | 1 (Direkt) | 1 (Direkt)
TOP27 `Z1_Heat_Request_Temp` | 20 | 20
TOP28 `Z1_Cool_Request_Temp` | 20 | 20
TOP29 `Z1_Heat_Curve_Target_High_Temp` | 20 | 20
TOP30 `Z1_Heat_Curve_Target_Low_Temp` | 34 | 34
TOP31 `Z1_Heat_Curve_Outside_High_Temp` | 15 | 15
TOP32 `Z1_Heat_Curve_Outside_Low_Temp` | -10 | -10
TOP72 `Z1_Cool_Curve_Target_High_Temp` | 20 | 20
TOP73 `Z1_Cool_Curve_Target_Low_Temp` | 20 | 20
TOP74 `Z1_Cool_Curve_Outside_High_Temp` | 30 | 30
TOP75 `Z1_Cool_Curve_Outside_Low_Temp` | 20 | 20

Byte 28 müsste demnach `0x0A` sein (beide Felder auf Direkt) — **das ist im
ersten Schritt zu bestätigen**, siehe unten.

**Achtung bei `Target_High`:** TOP29 zeigt hier 20 und damit *nicht* den
konfigurierten oberen Kurvenpunkt, sondern die Vorlauf-Solltemperatur — beide
teilen sich in der Wärmepumpe dieselbe Speicherstelle (SET27 ≡ SET5, belegt
2026-08-10). Die maßgebliche Kurvenkonfiguration steht im ioBroker unter
`0_userdata.0.kaskade.Konfiguration.*`, von dort spiegelt `kurven_sync.py`.
**Für die Wiederherstellung ist der ioBroker die Quelle, nicht diese Tabelle.**

## 7. Messplan

In dieser Reihenfolge, jeder Schritt einzeln ausgewertet. Vorgehen wie bei den
Pumpen-Bytes am 2026-08-19 (dort hat es sich bewährt):

**Schritt 0 — Vorbedingungen.**
Anlage muss **stehen** (`Heatpump_State` 0, `Compressor_Freq` 0). Rettungsanker
setzen und auf Branch arbeiten. Kurvenkonfiguration aus dem ioBroker sichern.

**Schritt 1 — Ist-Zustand von Byte 28 lesen, ohne etwas zu ändern.**
```bash
./test/byte_monitor.py 192.168.2.120 28 --dauer 20
```
Erwartung: `0x0A`, Bits 5+6 = `10`, Bits 7+8 = `10`. Stimmt das nicht, ist die
Bitzuordnung aus Abschnitt 2 falsch und alles Weitere hinfällig.

**Schritt 2 — Schreibversuch, zunächst nur die Kühlkurve.**
Kühlen ist der risikoärmere von beiden: Die Anlage heizt im Zweifel weiter.
Nach dem Einbau von SET36 `set/CoolingMode 0` senden und **im selben Mitschnitt**
beobachten, ob Byte 28 von `0x0A` auf `0x06` wandert.

Passiert nichts, ist die Frage beantwortet — die Wärmepumpe nimmt Byte 28 nicht
an, und das Vorhaben endet hier ohne Schaden.

**Schritt 3 — Rücklesen prüfen.**
TOP81 muss auf 0 (`Comp. Curve`) gehen, TOP76 auf 1 (`Direkt`) **stehen
bleiben**. Bleibt TOP76 nicht stehen, greift die Maske nicht bitgenau — dann
Abbruch und Fehlersuche, nicht weitermachen.

**Schritt 4 — Kurvenwerte kontrollieren.**
TOP72–TOP75 nachsehen: stehen dort jetzt die Werksvorgaben? Damit ist die
Beobachtung von 2026-08-11 zum ersten Mal auch über den Kommandopfad belegt.

**Schritt 5 — zurückschalten und aufräumen.**
`set/CoolingMode 1`, danach `kurven_sync.py` laufen lassen und die Sollwerte
kontrollieren. Erst wenn Stufe 1 vollständig wiederhergestellt ist, überhaupt
über Heizen oder über Stufe 2 nachdenken.

**Schritt 6 — Heizseite.**
Nur, wenn die Schritte 2–5 sauber durchliefen, und nur bei stehender Anlage in
einer Jahreszeit, in der ein verstellter Heizsollwert nichts anrichtet.

## 8. Werkzeuge

Werkzeug | wofür
:--- | :---
[`test/byte_monitor.py`](test/byte_monitor.py) | Byte 28 im Hexlog beobachten, Flanke im selben Mitschnitt fangen
[`test/mqtt_pub.py`](test/mqtt_pub.py) | Set-Kommando senden
[`test/kurven_sync.py`](test/kurven_sync.py) | Kurve aus dem ioBroker wiederherstellen (`--dry-run` zuerst)
[`test/set_top_zuordnung.py`](test/set_top_zuordnung.py) | nach dem Einbau: Doku gegen den Code abgleichen
[`test/tablesnap.py`](test/tablesnap.py) | Abnahme nach dem Flashen
[`test/decode_vergleich.py`](test/decode_vergleich.py) | belegen, dass der Dekodierpfad unverändert bleibt

## 9. Nach der Umsetzung nachzuziehen

* `MQTT-Topics.md` — zwei Zeilen in der Kommandotabelle
* `SET-TOP-Zuordnung.md` — Abschnitt 4 Punkt 1 wird erledigt, Abschnitt 3a
  verliert TOP76 und TOP81; danach `./test/set_top_zuordnung.py --pruefen`
* `src/version.h` — Changelog mit Problem, Nachweis und Größenänderung
* `README.md` — Kommandozahl (heute 32)
* Der Notbetrieb ist damit fernschaltbar: Das gehört in `NOTBETRIEB.md` im
  Node-RED-Projekt, und die Kaskadensteuerung kann den Handgriff übernehmen.

## 10. Wenn es nicht klappt

Nimmt die Wärmepumpe Byte 28 nicht an, ist das ein **verwertbares Ergebnis** und
gehört dokumentiert — in `SET-TOP-Zuordnung.md` bei TOP76/TOP81 als „Schreiben
versucht, wird nicht angenommen (Datum)". Damit sucht niemand ein zweites Mal.
Der Notbetrieb bleibt dann bei der heutigen Lösung: Kurvenwerte spiegeln, den
letzten Handgriff macht ein Mensch.
