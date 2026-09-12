# Arbeitsplan: KNX-Schritt „Vorderhaus“ in der Firmware

> **Übergabe für eine neue Session — Stand 2026-09-12 abends.**
> Recherche, Vorabtest, Entwurf und Rückleseregel sind abgeschlossen und
> vom Owner entschieden. Die Firmware steht unverändert auf 3.20.0. Offen ist
> die Umsetzung; dieser Plan führt vom ersten Befehl bis zum Rollout.
> **Ergebnis: offen.**

## Worum es geht

Fällt die übergeordnete Steuerung aus, stellt der Notbetrieb die
Wärmepumpen um — das Vorderhaus bleibt dabei bisher außen vor, weil sein
Mischer und seine Pumpe am KNX-Bus hängen und auf dem letzten Wert stehen
bleiben. Der neue Schritt schickt per KNXnet/IP-Tunneling vier Telegramme an
die Schnittstelle 192.168.2.127: Zwangsstellungen zurück, Mischer auf 50 %,
Pumpe ein — und liest am Aktor zurück.

## Lies zuerst — in dieser Reihenfolge

1. `CLAUDE.md` des Repos, besonders die harten Regeln (Rettungsanker,
   `doku-intern/`, keine Binaries, kein Gerätezugriff ohne Auftrag).
2. [`Analyse-KNX-Vorderhaus.md`](Analyse-KNX-Vorderhaus.md), Abschnitt 8 ganz
   (Gruppenadressen, Stellmotor, „nur kurze Verbindungen“, Mischerlauf,
   „Bestätigung über die Bewegungsmeldung“, Rückleseregel A) und Abschnitt 7
   für die Rohbytes.
3. [`test/knx_tunnel.py`](test/knx_tunnel.py) 1.6.0 — **die Referenz.**
   `befehl_mischer()` mit `_bestaetigung()` ist Regel A in ausführbarer
   Form; die Rahmenfunktionen (`baue_*`, `zerlege_*`) sind die Vorlage für
   den Header; `selbsttest()` enthält die Sollwerte.
4. `src/notbetrieb.h` (Schrittfolge, Automat, Abbruchgründe) und
   `src/notbetrieb.cpp` ab `hydraulik_kommando()` — der Hydraulikschritt ist
   das Muster, dem der neue Schritt folgt.
5. [`Ablauf-Notbetrieb.md`](Ablauf-Notbetrieb.md), Abschnitt 1a, besonders
   „Warum der Request `loop()` blockieren darf“.

## Entschieden — nicht neu aufmachen

| Entscheid | Quelle |
| --- | --- |
| Eigener minimaler Tunnel-Client, keine Bibliothek, kein Routing | Analyse §2–4 |
| Weg A: **ein** Schritt am Ende der Heizen-Folge, nach `Heatpump = 1`; nur Rolle Heizen | Analyse §8, Owner 2026-09-12 |
| ROT-Wortlaut: „Die Wärmepumpen laufen im Notbetrieb, nur das Vorderhaus ließ sich nicht umstellen“ | Owner 2026-09-12 |
| Fehlende KNX-Einstellung = ROT, kein stilles Entfallen | Owner 2026-09-12 |
| Quelladresse 1.1.250 im Telegramm | Owner 2026-09-12, Analyse §8 |
| Nur kurze Verbindungen — kein Tunnel bleibt über Sekunden offen | Analyse §8 |
| Die Busbestätigung (L_Data.con) entscheidet nichts; mit Quelle 1.1.250 kommt keine | Analyse §7/§8 |
| Nur Antworten der Aktoradressen zählen (1.1.39 Mischer, 1.1.60 Pumpe) | Analyse §8 |
| **Rückleseregel A** für den Mischer samt der vier Festlegungen beim Einarbeiten | Analyse §8, Owner 2026-09-12 abends |

## Die Parameter

| Was | Wert |
| --- | --- |
| Schnittstelle | 192.168.2.127:3671, reine Schnittstelle, 5 Tunnel, kein KNX IP Secure |
| Quelle im Telegramm | 1.1.250 |
| Zwangsstellung AUF / ZU | 6/4/17 / 6/4/16, DPT 1, schreiben 0 |
| Position Eingang | 6/4/13, DPT 5, schreiben 128, **zurücklesen** |
| Position Status | 6/4/12, DPT 5, lesen |
| Position Bewegung | 6/4/14, DPT 1, lesen / spontan |
| Pumpe schalten / Status | 6/4/20 / 6/4/21, DPT 1 |
| Aktoren | 1.1.39 Mischer, 1.1.60 Pumpe; 1.1.245 ist openknx und zählt nie |
| Toleranz Status | 128 ± 2 |
| Rückfall-Frist (Mischer fuhr schon) | 220 s |

Gemessene Zeiten als Maßstab für die Fristen: CONNECT_RESPONSE 1–4 ms,
TUNNELING_ACK 2 ms, Antwort eines Aktors auf Lesen 55–82 ms, Bewegung 1
93 ms nach dem Positionsbefehl, Pumpenstatus 55–120 ms nach dem Befehl.

## Regel A als Ablauf des Schritts

Eine kurze Verbindung beim Absetzen, wie der Hydraulikschritt:

1. CONNECT. Kanal und Datenendpunkt **aus der Antwort übernehmen** — die
   Schnittstelle vergibt beide selbst.
2. 6/4/14 lesen. `schnell` = der Mischeraktor antwortet mit 0. Keine Antwort
   zählt wie 1.
3. 6/4/17 = 0, 6/4/16 = 0, 6/4/13 = 128 schreiben. Nicht auf eine con warten.
4. 6/4/13 lesen. Keine Antwort oder ≠ 128: Schritt 3 einmal wiederholen,
   danach ROT.
5. `schnell`: kurz auf Bewegung 1 vom Aktor warten, sonst 6/4/14 lesen; 1
   ist GRÜN. Sonst 6/4/12 lesen; 128 ± 2 ist GRÜN (Mischer stand schon
   dort). Sonst Schritt 3 einmal wiederholen, danach ROT.
   Nicht `schnell`: der Mischer bleibt **ausstehend** → Rückfall (E2).
6. 6/4/20 = 1, Rücklesung 6/4/21 vom Pumpenaktor (spontan, sonst lesen).
   Abweichung: einmal wiederholen, danach ROT.
7. DISCONNECT — immer, auch im Fehlerfall.

Während jeder Wartezeit jedes eingehende TUNNELING_REQUEST des eigenen
Kanals binnen 1 s quittieren; fremde Kanäle verwerfen, ohne zu quittieren.
Die Sequenzregel steht in `Tunnel._tunnel_empfangen()`.

## Vor dem Code zu entscheiden — einzeln mit dem Owner

**E1 — Wo stehen Adressen und Gruppenadressen?** Die Analyse nennt
Einstellungen „wie `hydraulik_switch`“; das wären zwölf neue Felder in
WiFiManager, `config.json` und Einstellungsseite (`webfunctions.cpp`).

- **a (empfohlen):** ein Feld `knx_schnittstelle` (IP, optional `:Port`);
  Quelle, Gruppenadressen und Aktoradressen als Konstanten im Header.
  Wenige Felder, die Konstanten sind im Hosttest prüfbar, und die
  Gruppenadressen ändern sich nur mit ETS-Arbeit, die ohnehin einen Fachmann
  braucht. Leer = ROT.
- b: alle zwölf Werte als Felder. Änderbar ohne Build, aber viel Oberfläche
  für wenige Werte, die sich nie ändern.

**E2 — Der Rückfall braucht bis zu 220 s, der Automat kennt 20 s je
Schritt.** `NOTBETRIEB_SCHRITT_TIMEOUT_MS` gilt heute für jeden Schritt, und
der Gesamtdeckel ist `Schrittzahl × 20 s` (`notbetrieb.h`, Zeitregeln).

- **a (empfohlen):** Timeout je Schritttyp (Vorderhaus 240 s), Gesamtdeckel
  als Summe der Schritt-Timeouts — bleibt abgeleitet. Im Rückfall fragt der
  Tick alle 10 s in einer eigenen kurzen Verbindung 6/4/12 ab, bis 128 ± 2.
  Der Regelfall ist davon nicht betroffen.
- b: Rückfall = ROT mit eigenem Text. Einfach, widerspricht aber Regel A.

**E3 — Was steht unter dem ROT-Wortlaut?** Der Owner-Satz sagt, was los
ist, aber nicht, was zu tun ist. Beim Hydraulikschritt steht „Danach diesen
Knopf noch einmal drücken“ darunter.

- **a (empfohlen):** derselbe Zusatz. Ein zweiter Lauf setzt dieselben
  Werte noch einmal; mit laufender Wärmepumpe ist das allerdings nicht
  gemessen.
- b: nur der Owner-Satz.

**E4 — Wie wird am Prüfling getestet, ohne den echten Mischer zu bewegen?**

- **a (empfohlen):** `knx_tunnel.py` bekommt einen Befehl `simulator`, der
  den vorhandenen Simulator auf der LAN-Adresse des Macs anbietet. Der
  Prüfling bekommt dessen Adresse als Schnittstelle — alle Zweige von Regel A
  lassen sich dann ohne Bus durchspielen. Danach ein einzelner Lauf gegen die
  echte Schnittstelle mit Owner-Freigabe.
- b: nur gegen die echte Schnittstelle, bei ausgeschalteter Kaskade.

## Arbeitsschritte

Jeder Schritt endet mit seinem Prüfkriterium. Commits je Schritt auf dem
Branch.

0. **Rettungsanker und Branch.** Tag `rettungsanker-JJJJ-MM-TT` auf den
   aktuellen `main`, Branch `knx-vorderhaus`. E1–E4 vorher entscheiden.
1. **`src/knxtunnel.h`, arduino-frei.** Rahmen bauen und zerlegen (KNXnet/IP-
   Kopf, HPAI, CONNECT, TUNNELING_REQUEST/ACK, DISCONNECT, cEMI), GA- und
   PA-Kodierung, `bestaetigt_ok` nur über Bit 0 von ctrl1, und das Urteil von
   Regel A als reine Funktion über die gelesenen Werte. Hosttest
   `test/knx_test.cpp` gegen die Sollwerte: die xknx-Vektoren und die
   Anlagen-Rohbytes aus `selbsttest()` Teil 1–3 sowie die Tabellen in
   Analyse §7/§8; dazu die Zweige von Regel A wie in den Mischerfällen des
   Selbsttests. In die Hosttest-Liste in `.github/workflows/main.yml`,
   mit Begründungskommentar wie die anderen.
   *Prüfkriterium:* Hosttest grün, jeder Rahmen byteweise gleich den
   Sollwerten.
2. **Netzteil**, eigene Datei (Vorschlag `src/vorderhaus.cpp`), aufgerufen aus
   `notbetrieb_schritt_absetzen()`. `WiFiUDP`, Ablauf wie oben, jede Frist
   begrenzt (CONNECT 1 s, ACK 1 s mit einer Wiederholung, Lesen 1 s — endet
   an der Antwort des Aktors, nicht am Fenster —, Bewegung spontan 1 s).
   Beim ersten fehlenden ACK abbrechen, DISCONNECT, ROT. Logzeilen wie beim
   Hydraulikschritt: was geantwortet hat und was nicht.
   *Prüfkriterium:* Die größte Blockade von `loop()` ist ausgerechnet und
   im Kommentar begründet, wie in Ablauf-Notbetrieb §1a.
3. **Automat.** `NB_SCHRITT_VORDERHAUS` in `NotbetriebSchrittTyp`, als letzter
   Eintrag in `NOTBETRIEB_SCHRITTE_HEIZEN`; `NOTBETRIEB_GRUND_VORDERHAUS` und
   `notbetrieb_grund_fuer_schritt()`; E2 umsetzen. In `notbetrieb.cpp` eine
   Bestätigung wie `hydraulikBestaetigt`, beim Start jedes Laufs gelöscht;
   ROT sofort, wenn das Absetzen scheitert. Die Mindestwarte
   (`NOTBETRIEB_SCHRITT_MINDESTWARTE_MS`, 8 s) gilt auch hier — der Schritt
   kostet im Regelfall also rund 8 s, nicht die Sekunde des Austauschs.
   *Prüfkriterium:* `notbetrieb_test` deckt den neuen Schritt, den Grund und
   den Deckel ab, samt `millis()`-Überlauf im Rückfall.
4. **Einstellung** nach E1, mit der Start-Warnung wie beim
   Hydraulik-Switch (`notbetrieb.cpp`, „keine Adresse fuer den
   Hydraulik-Switch“). `jsonDoc.overflowed()` bleibt die Absicherung.
5. **Seite.** `NB_TXT_VORDERHAUS` nach `NB_TXT_HYDRAULIK` in
   `webfunctions.cpp`, der ROT-Zweig wählt nach dem Grund; Dauertext und
   Laufzeiten prüfen. `css_klassen_test.py` muss grün bleiben.
6. **Hosttests anpassen,** die an der Schrittfolge hängen
   (`test/notbetrieb_test.cpp`): „Heizen hat zehn Schritte“, „Schritt 10
   schaltet die Anlage ein“, die Schleife „letzter Schritt jeder Rolle ist
   `Heatpump`“ (gilt dann nur noch für Wasser; für Heizen: `Heatpump` direkt
   vor dem Vorderhausschritt) und die Deckelwerte.
7. **Doku.** `src/version.h`: 3.21.0 mit Problem, Nachweis, RAM/Flash-Delta.
   `Ablauf-Notbetrieb.md`: Phase 2 und „Die zehn Schritte“, neuer Abschnitt
   „1c. Der Vorderhausschritt“, Abschnitt 3 (die Rückkehr der Steuerung holt
   den Mischer erst mit dem KNX-Re-Assert zurück). `README.md` → „Aufbau“:
   neue Dateien. Die Liste der arduino-freien Header in `CLAUDE.md`. Im Kopf
   der Analyse das Ergebnis.
8. **Bauen und testen:** alle Envs, alle Hosttests (Befehle in `CLAUDE.md`,
   Liste in `main.yml`), `knx_tunnel.py selbsttest`.
9. **Prüfling.** Nach E4. Der Lauf gegen die echte Schnittstelle nur mit
   ausdrücklicher Freigabe, Kaskade aus, Busmonitor und `knx_tunnel.py
   mischer … --mithoeren` oder `lesen --alle` als Gegenprobe. Achtung: Der
   Notbetriebslauf schaltet dabei auch den echten Hydraulik-Switch (Ablauf
   §1a, „Im Test stellt die lebende Steuerung binnen 20 s zurück“).
   Zurückstellen wie am 2026-09-12, jeder Eingriff einzeln: AUF bzw. ZU wie
   vorgefunden, Eingang auf den Vorwert, Pumpe wie vorgefunden.
10. **Merge und Rollout** erst nach Schritt 8 vollständig. Abnahme mit
    `test/tablesnap.py` gegen den Stand davor.

## Fallen, die schon einmal zugeschnappt sind

- **openknx antwortet schneller als der Aktor** — auf 6/4/12 und 6/4/14 aus
  dem Zwischenspeicher, auf 6/4/14 dreifach. Wer die erste Antwort nimmt,
  meldet im Test GRÜN, und im Ernstfall ist openknx gar nicht da.
- **Mit Quelle 1.1.250 kommt keine L_Data.con.** Wer darauf wartet, verliert
  je Telegramm die ganze Frist; wer sie verlangt, meldet immer ROT.
- **ctrl1 der con kommt als `9c`, gesendet wurde `bc`.** Positiv/negativ
  steht allein in Bit 0.
- **Der Aktor speichert die Position auch unter Zwangsstellung** und fährt
  dann nicht. Der zurückgelesene Eingang allein belegt deshalb nichts.
- **In die Endlage läuft die Bewegung immer 144 s**, der Status meldet schon
  nach der proportionalen Fahrzeit. Zur Mitte gibt es keinen Nachlauf.
- **Ein ETS-Download setzt den Aktor zurück** (Eingang 0, Position ungültig);
  die nächste Fahrt läuft die volle Zeit samt Nachlauf.
- **MQTT-Reconnect blockiert `loop()` bis 2 s**, im Notbetriebsfall
  wiederholt. Deshalb nie einen Tunnel über mehrere `loop()`-Durchläufe
  offen halten.
- **Die InfluxDB schreibt openknx-Werte jede Minute neu.** Ein Eintrag ist
  kein Telegramm; ob etwas auf dem Bus war, zeigen `ts`/`lc` am Datenpunkt.

## Nicht in diesem Plan

- **Re-Assert der KNX-Befehle in `nodered-flows`** (Pumpe, Zwangsstellung).
  Ohne ihn holt die zurückkehrende Steuerung Mischer und Pumpe nicht von
  selbst zurück — Voraussetzung dafür, dass der Normalzustand wiederkommt.
- **Notfallanleitung** (`nodered-flows`, `FEUERUEBUNG.md`): Fällt nur
  Node-RED aus und ioBroker läuft, regelt `HKMregelung.js` die Position beim
  nächsten Regelschritt wieder weg (Analyse §6).

## Stand der Commits bei der Übergabe

Alle auf `main`, **noch nicht gepusht** — ob und wann, entscheidet der Owner.

| Commit | Inhalt |
| --- | --- |
| `b8cc042` | `knx_tunnel.py` 1.5.0: Bestätigung über die Bewegungsmeldung |
| `278e42e` | Analyse: Bewegungsmeldung an der Anlage erprobt |
| `0ce86c0` | `knx_tunnel.py` 1.6.0: Rückleseregel A als Referenz |
| (dieser) | Analyse: Entscheid A; dieser Arbeitsplan |
