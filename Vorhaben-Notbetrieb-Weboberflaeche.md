# Vorhaben: Notbetrieb über die Weboberfläche schaltbar machen

Der Notbetrieb soll ohne ioBroker, ohne Node-RED und ohne MQTT-Broker
auslösbar sein — mit einem Browser, von einem Menschen aus der Familie.

**Anspruch:** Der Notbetrieb muss nicht alles halten, was der Normalbetrieb
kann. Er soll verhindern, dass das Haus auskühlt, und Zeit verschaffen, in Ruhe
nach einer Lösung zu suchen. Er ist **einstufig** — Stufe 1 heizt, Stufe 2
macht Warmwasser; ein Kaskadenbetrieb ohne übergeordnete Steuerung würde nicht
lange gutgehen (Owner-Entscheidung 2026-08-19).

**Stand dieser Datei:** 2026-08-19, Firmware 3.11.0 auf beiden Stufen.
Angefangen als Entwurf; Abschnitt 5 sammelt die offenen Fragen.

---

## 1. Warum das nötig ist — das Henne-Ei-Problem

Mit 3.11.0 ist die Betriebsart erstmals fernschaltbar (SET35 `HeatingMode`,
SET36 `CoolingMode`, am Gerät in vier Läufen belegt, siehe
[`SET-TOP-Zuordnung.md`](SET-TOP-Zuordnung.md) Fußnote ⁶). Das löst den
Notbetrieb aber nur halb:

**Der MQTT-Broker *ist* der ioBroker-MQTT-Adapter** auf 192.168.2.147:1883
(am 2026-08-11 nachgemessen). Fällt der ioBroker aus, fehlt nicht nur der
Absender des Kommandos, sondern der Übertragungsweg selbst — die Firmware hat
dann niemanden mehr, von dem sie ein `set/HeatingMode 0` bekommen könnte.

Ausfallszenario | heute lösbar? | womit
:--- | :--- | :---
Node-RED-Flow fehlerhaft, ioBroker läuft | ja | SET35/SET36 über MQTT
Node-RED-Container weg, Broker läuft | ja | SET35/SET36 über MQTT
**ioBroker/Synology komplett aus** | **nein** | *dieses Vorhaben*
WLAN weg | nein | AP-Modus des HeishaMon, siehe Abschnitt 5

Die Firmware hat einen eigenen Webserver, der vom Broker unabhängig läuft. Das
ist der Weg, der im letzten Fall noch übrig bleibt.

## 2. Was der Notbetrieb tatsächlich leisten muss

Nicht nur umschalten. Damit die Wärmepumpe danach sinnvoll heizt, braucht es
mehrere Schritte in der richtigen Reihenfolge:

1. **Betriebsart auf Kurve** — SET35 (nur Heizen, siehe Frage 1).
2. **Kurvenwerte wiederherstellen.** Das Umschalten setzt die Kurve auf die
   Panasonic-Werksvorgaben zurück (am 2026-08-19 gemessen: Heizkurve 55 °C bei
   −5 °C und 35 °C bei +15 °C). Das ist keine Fußbodenheizungskurve — mit
   diesen Werten liefe die Anlage falsch. SET28–SET30 für die Heizkurve; die
   Kühlkurve entfällt im Notbetrieb (Frage 1).
3. **Oberen Kurvenpunkt setzen** — SET5, weil TargetHigh und die
   Vorlauf-Solltemperatur sich in der Wärmepumpe eine Speicherstelle teilen
   (SET27 ≡ SET5, belegt 2026-08-10).
4. **Ggf. die Anlage einschalten** — siehe Frage 4 in Abschnitt 5.

Alle vier Schritte gehen über `setCommands[]` und damit über denselben
geprüften Pfad, den heute schon MQTT benutzt.

## 3. Der Knackpunkt: Die Firmware kennt die Kurve nicht

Schritt 2 ist der Grund, warum das mehr ist als ein Knopf.

Heute spiegelt [`test/kurven_sync.py`](test/kurven_sync.py) die Kurvenwerte aus
`0_userdata.0.kaskade.Konfiguration.*` **direkt in die Wärmepumpe**. Die
Firmware sieht die Werte nur als durchlaufende Set-Kommandos und behält nichts
davon. Im Totalausfall ist der ioBroker als Quelle weg — die Firmware müsste
die zuletzt gültigen Werte also **selbst persistent halten**.

Vorhandener Ort dafür: LittleFS, dort liegt schon `/config.json`
([`src/webfunctions.cpp`](src/webfunctions.cpp)). Eine getrennte Datei
(`/notbetrieb.json`) wäre vorzuziehen, damit ein Fehler dort nicht den Boot
gefährdet — genau die Falle, die der Kommentar bei `loadConfigValue()`
beschreibt: Ein fehlender Schlüssel ließ das Gerät früher in einer
Boot-Endlosschleife landen, und beide Stufen wären nach einem OTA gleichzeitig
ausgefallen.

## 4. Vorschlag für die Umsetzung

Vier Bausteine, in dieser Reihenfolge einzeln nutzbar:

**A — Kurvenwerte in der Firmware ablegen.**
`/notbetrieb.json` auf LittleFS mit den sechs Kurvenwerten plus dem
Vorlaufsollwert. Gefüttert im Normalbetrieb, gelesen nur im Notfall. Ohne
gültige Datei bleibt der Notbetriebs-Knopf gesperrt und sagt das auch — lieber
gar nicht schalten als auf Werkskurve.

**B — Ein Endpunkt, der die Schrittfolge ausführt.**
`/notbetrieb` mit demselben Auth-Muster wie `/reboot` und `/settings`
([`src/HeishaMon.cpp:196-226`](src/HeishaMon.cpp#L196-L226)):

```cpp
httpServer.on("/notbetrieb", []() {
  if (!httpServer.authenticate(update_username, ota_password))
  {
    return httpServer.requestAuthentication();
  }
  handleNotbetrieb(&httpServer);
});
```

Ein Sidebar-Eintrag mehr in `sidebar[]`, das ist ein PROGMEM-String.

**C — Die Set-Kommandos ohne MQTT auslösen.**
`build_heatpump_command()` durchläuft heute Bereichsprüfung, Maskenmerge,
Konfliktwarnung, Logging und `register_new_command()`. Diesen Pfad
wiederzuverwenden statt die Logik zu duplizieren ist der springende Punkt —
ein zweiter Merge-Pfad wäre genau die Sorte Doppelung, die in 3.1.0 zum
Maskenfehler geführt hat. Sauberste Form: die Funktion in „Topic auflösen" und
„Wert anwenden" trennen, damit der Webhandler den zweiten Teil direkt aufruft.

**D — Anzeige des Ist-Zustands.**
Die Seite muss zeigen, was gerade gilt: TOP76/TOP81 (Betriebsart), TOP29–TOP32
und TOP72–TOP75 (Kurve), TOP0 (Anlage an/aus). Sonst weiß der Bedienende nach
dem Klick nicht, ob es gewirkt hat — und genau darauf kommt es an, weil
„gesendet" nicht „steht" heißt.

### Getrennte Knöpfe

Alexanders Vorschlag, die Kurven einzeln initialisierbar zu machen, passt gut
zu Baustein A und ist auch im Normalbetrieb nützlich (z. B. nach einem
versehentlichen Umschalten am Bedienterminal). Denkbare Aufteilung:

Knopf | Rolle | Wirkung
:--- | :--- | :---
**Notbetrieb ein** | Heizen | SET35 auf Kurve + Kurvenwerte + Sollwert + einschalten
**Notbetrieb ein** | Warmwasser | SET9 auf DHW + SET11 + einschalten
**Notbetrieb aus** | beide | zurück auf Direktvorgabe bzw. den vorherigen Modus
Heizkurve setzen | Heizen | nur SET28–SET30 + SET5 aus `/notbetrieb.json`
Kühlkurve setzen | — | im Normalbetrieb nützlich, für den Notbetrieb nicht nötig

## 5. Was zu klären ist

Diese Fragen bestimmen den Umfang. Ich kann sie nicht aus dem Code beantworten.

**1. Beide Kaskadenstufen oder nur eine? — ENTSCHIEDEN am 2026-08-19.**

> **Der Notbetrieb ist einstufig: Stufe 1 heizt, Stufe 2 macht Warmwasser.**
> Ein Kaskadenbetrieb ohne übergeordnete Steuerung würde nicht lange gutgehen.
>
> **Der Notbetrieb muss nicht alles halten, was der Normalbetrieb kann.**
> Hauptziel ist, dass das Haus nicht auskühlt und in Ruhe nach einer Lösung
> gesucht werden kann. (Owner-Entscheidung)

Das vereinfacht das Vorhaben an drei Stellen erheblich:

* **Die Kühlseite entfällt.** SET36 `CoolingMode` und die Kühlkurve
  (SET32–SET34) spielen im Notbetrieb keine Rolle. Byte 28 wird nur über
  SET35 angefasst — der Fall, der am 2026-08-19 in Lauf 3 gemessen wurde.
  Damit erübrigt sich auch der kombinierte Schaltvorgang (Rohwert `0x05`).
* **Kein Taktproblem.** Die beiden Stufen konkurrieren nicht, weil sie
  verschiedene Aufgaben haben.
* **Die Rollen sind verschieden, und damit auch die Knöpfe.**

Rolle | Gerät | was der Notbetrieb dort tut
:--- | :--- | :---
**Heizen** | H1 / WP1, 192.168.2.120 | SET35 auf Kurve, Heizkurve (SET28–SET30) und Sollwert (SET5) setzen, Anlage einschalten
**Warmwasser** | H2 / WP2, 192.168.2.122 | SET9 `OperationMode` auf 3 (DHW only), SET11 `DHWTemp` setzen, Anlage einschalten

Die Warmwasserseite braucht **gar keine Kurve** — der Knopf dort ist deutlich
einfacher als auf Stufe 1.

**1a. Wie erfährt die Firmware ihre Rolle?** Beide Stufen laufen denselben
Quelltext. Die Stufen-Unterschiede stecken heute als Build-Flags in
`platformio.ini` (MQTT-Prefix, Hostname). Für den Notbetrieb wäre stattdessen
ein Feld in `/notbetrieb.json` denkbar — dann ließe sich die Rolle ohne OTA
ändern, was bei einem Gerätetausch hilft. Was ist dir lieber?

**2. Knopf oder automatischer Rückfall — oder beides?**
Wenn die Firmware die Kurve ohnehin kennt, könnte sie selbst umschalten, sobald
seit *n* Stunden kein Set-Kommando mehr kam. Dann braucht es im Ernstfall gar
keinen Menschen. Der Preis ist die Fehlauslösung: ein längerer Wartungsstopp am
ioBroker sähe für die Firmware genauso aus wie ein Totalausfall. Meine
Empfehlung wäre, mit dem Knopf anzufangen und den Automatismus als zweite Stufe
zu betrachten — aber das ist deine Entscheidung.

**3. Wie kommen die Kurvenwerte in die Firmware?**
Drei Möglichkeiten:
* `kurven_sync.py` schreibt sie zusätzlich per HTTP an die Firmware (bleibt
  automatisch aktuell, braucht einen Schreib-Endpunkt).
* Von Hand über die Weboberfläche eingetragen (kein neuer Automatismus, kann
  aber veralten).
* Fest einkompiliert (am einfachsten, aber jede Kurvenänderung braucht ein OTA).

**4. Was ist mit dem Ein-/Ausschalten der Anlage?**
Heute steht die Kaskade im Standby (`Heatpump_State` 0) und wird von Node-RED
freigegeben. Fällt die Steuerung im Standby aus, nützt die Kurve nichts —
die Anlage läuft ja gar nicht. Der Knopf muss also vermutlich auch SET1
`Heatpump` auf 1 setzen; auf der Warmwasser-Rolle zusätzlich SET9
`OperationMode` auf 3. Bestätigst du das, und gibt es Bedingungen, unter denen
NICHT eingeschaltet werden darf?

**5. Zugang, wenn das WLAN weg ist.**
Bei WLAN-Ausfall startet der Watchdog das Gerät nach 5 min neu, `autoConnect`
scheitert nach 10 s und öffnet dann für 180 s den AP (WPA2, Passwort aus
`platformio_user_env.ini`). Über diesen AP wäre die Weboberfläche erreichbar —
aber nur in diesem 180-Sekunden-Fenster, und das Konfigurationsportal zeigt
dort die echten Zugangsdaten vorbefüllt an. Soll dieser Weg Teil des Konzepts
sein? Dann gehört er in die Offline-Anleitung, samt der Warnung, dort nichts zu
speichern.

**6. Zurückschalten nach der Störung.**
Kommt die Steuerung wieder, überschreibt ihr Re-Assert die Sollwerte — aber
nicht die Betriebsart. Muss jemand den Notbetrieb von Hand beenden, oder soll
die Firmware zurückschalten, sobald wieder Kommandos eintreffen? Letzteres wäre
bequem, könnte aber mitten in einem Notbetrieb auslösen, wenn ein Teil der
Steuerung schon wieder läuft und ein anderer nicht.

## 6. Risiken und Fallen

* **Der Test muss mit abgeschaltetem Broker laufen.** Sonst prüft er nicht den
  Fall, für den er gebaut ist. Ob die Firmware ohne erreichbaren Broker sauber
  weiterläuft, ist plausibel (die Reconnect-Logik hat einen Backoff), aber
  **nicht gemessen**.
* **Kein zweiter Merge-Pfad.** Siehe Baustein C.
* **`/notbetrieb.json` darf den Boot nicht gefährden.** Fehlende Datei,
  fehlende Schlüssel und Unsinn im Inhalt müssen alle in „Knopf gesperrt"
  münden, nicht in einen Absturz.
* **Ein Knopf, der schaltet, ist ein Knopf, den jemand versehentlich drückt.**
  Der Zugangsschutz ist dasselbe Passwort wie für `/firmware` — das ist
  vertretbar, aber die Seite sollte vor dem Ausführen fragen und danach den
  Ist-Zustand zeigen.
* **Die Bedienung ist für Laien.** „Notbetrieb ein" statt „SET35 auf 0", und
  die Seite muss ohne Vorwissen verständlich sein. Die Passwörter und die
  Schritt-für-Schritt-Anleitung stehen offline bereit (Owner-Angabe
  2026-08-19).

## 7. Testplan (Entwurf)

Erst zu füllen, wenn Abschnitt 5 beantwortet ist. Grobe Reihenfolge nach dem
Muster, das sich beim Byte-28-Vorhaben bewährt hat:

0. Rettungsanker, Branch, Ausgangszustand sichern.
1. Bausteine A–D auf dem Prüfstand-ESP8266 (192.168.2.197, keine WP
   angeschlossen) — Weboberfläche, Auth, `/notbetrieb.json`, Fehlerfälle.
2. Hosttest für die Schrittfolge, so wie `byte28_test.cpp` für die Kodierung.
3. An Stufe 1 bei stehender Anlage: Knopf drücken, Byte 28 und die Kurvenwerte
   im Mitschnitt verfolgen (`byte_monitor.py`).
4. **Wiederholung mit abgeschaltetem MQTT-Broker** — der eigentliche Nachweis.
5. Aufräumen wie gehabt: `kurven_sync.py`, Sollwerte, Endkontrolle.

## 8. Nach der Umsetzung nachzuziehen

* `README.md` — Weboberfläche und der neue Endpunkt.
* `src/version.h` — Changelog mit Problem, Nachweis und Größenänderung.
* Die Offline-Anleitung der Familie — Schrittfolge, IP-Adressen, Passwort.
* `NOTBETRIEB.md` im Node-RED-Projekt.
* **Zu korrigieren:** Changelog, `SET-TOP-Zuordnung.md` und das
  GitHub-Release zu 3.11.0 sagen sinngemäß „damit ist der Notbetrieb
  vollständig fernschaltbar". Das stimmt nur, solange ein Broker erreichbar
  ist, und gehört präzisiert.
