# Arbeitsplan: Serielle Strecke überwacht, abgelehnte Set-Befehle sichtbar (3.25.0)

> **Ergebnis: in Arbeit (2026-10-17) — Code, Hosttests, Builds und Prüflingsnachweis
> fertig; Rückgabe des Prüflings, Merge und Rollout stehen aus.** Paket P4 der Codedurchsicht vom
> 2026-10-15 ([`Massnahmenplan-Codedurchsicht-2026-10-15.md`](Massnahmenplan-Codedurchsicht-2026-10-15.md):
> S4, dazu S6 als Beigabe). Eine Firmware-Änderung mit Funktionswirkung im
> Notbetrieb: Der Nachweis läuft am **Prüfling** (`h1b`), nicht an der Anlage.
>
> **Übergabe:** Der Plan führt vom Rettungsanker bis zum Release und ist für den
> Einstieg in einer neuen Session geschrieben. Zuerst „Lies zuerst“, dann der
> Stand der Schritte.

## Worum es geht

* **S4, die serielle Strecke hat keine Gesundheitsanzeige.** `actual_data[]`
  wird nur bei einem gültigen Telegramm fortgeschrieben. Reißt die Strecke ab
  (Kabel, Pegelwandler, WP-Platine), bleiben die letzten Werte stehen — auf der
  Webseite, retained beim Broker und als Rücklesequelle des Notbetriebs. Ein
  Schritt, dessen Sollwert dem letzten Stand entspricht, wird dann ohne Antwort
  der Wärmepumpe abgehakt; im Warmwasserzweig im ungünstigen Fall alle fünf
  WP-Schritte (GRÜN, ohne dass ein Kommando ankam).
* **S6, abgelehnte Set-Befehle sind nirgends dauerhaft sichtbar.** Ein Payload,
  der keine Ganzzahl ist, oder ein Wert außerhalb der Tabellengrenzen verwirft
  den ganzen Befehl. Die Ablehnung steht nur im Logring, den der 5-min-Re-Assert
  binnen Minuten verdrängt. Anlass war N1 (Dezimalwerte des Notbetriebssenders,
  fünf Tage unbemerkt).

## Lies zuerst — in dieser Reihenfolge

1. [`CLAUDE.md`](CLAUDE.md) — harte Regeln 4 (Rettungsanker), 5 (kein Zugriff auf
   die produktiven Geräte ohne Auftrag) und 6 (alle Envs, alle Hosttests).
2. [`Massnahmenplan-Codedurchsicht-2026-10-15.md`](Massnahmenplan-Codedurchsicht-2026-10-15.md),
   Befunde S4 und S6, Paket P4.
3. [`src/serielle_strecke.h`](src/serielle_strecke.h) und [`src/setpayload.h`](src/setpayload.h) — die
   neuen Regeln; ihre Köpfe tragen die Begründungen.
4. [`src/notbetrieb.h`](src/notbetrieb.h): `notbetrieb_tick()` (Kommentar zur
   Bestätigungsregel) und `notbetrieb_sperrgrund()`.
5. [`test/README.md`](test/README.md), „Prüfstand aufsetzen“ — die **Reihenfolge
   beim Zurückgeben ist sicherheitskritisch** — und „Antwortquote messen“.
6. Memory „Messfallen am Prüfling“; die Skills `/pruefen-vor-merge` und `/rollout`.

## Entschieden — nicht neu aufmachen

| Nr. | Frage | Entscheid | Wann |
| --- | --- | --- | --- |
| E1 | Umfang von 3.25.0 | **S4 und S6 gemeinsam** | Owner 2026-10-16 |
| E2 | Wortlaut der Sperre | Überschrift „Keine Verbindung zur Wärmepumpe“, Absatz „Die Steuerung bekommt von der Wärmepumpe keine Antwort mehr und kann deshalb nicht umschalten. Die Wärmepumpe läuft mit den zuletzt gesetzten Werten weiter. Bitte nach der ausgedruckten Anleitung am Bedienfeld der Wärmepumpe weitermachen.“ Bewusst ohne Hinweis auf Stecker oder Ersatz-Board. | Owner 2026-10-16 |
| E3 | H1 und H2 | **gleich** — derselbe Text und dieselbe Schwelle an beiden Stufen und in beiden Rollen (STOPP-Regel damit ausdrücklich geprüft) | Owner 2026-10-16 |
| E4 | Schwelle | **30 s** ohne gültiges Telegramm | Owner 2026-10-16 |
| E5 | Topics | **vier**: `info/telegram_ok`, `telegram_err`, `telegram_timeout`, `telegram_age`. Der Plan nannte drei; stumme Leitung (Kabel, Pegelwandler) und Müll (Baudrate, Störung) haben verschiedene Ursachen | Owner 2026-10-17 |
| E6 | Reihenfolge der Sperrgründe | **Werte → Verbindung zur WP → Heizbetrieb**. Ohne Telegramme ist TOP101 nur der eingefrorene Wert | Owner 2026-10-17 |
| E7 | Bestätigungsregel | **Telegramm seit dem Absetzen** des Schritts, zusätzlich zur Mindestwarte von 8 s. *Nicht* „nach der Mindestwarte“ (Wortlaut im Maßnahmenplan): Das verlängerte jeden Schritt auf rund 12 s und den Heizen-Lauf von 88 s auf gut zwei Minuten, der Seitentext „bis zu anderthalb Minuten“ und die Zeitleiste in `Ablauf-Notbetrieb.md` stimmten nicht mehr | Owner 2026-10-17 |

### Was aus dem Code folgt — kein Entscheid, aber festgelegt

* **Kein neues Feld in der Statusroute.** Der Sperrgrund steht schon in Feld 4;
  die Verbindungssperre ist dort der Wert 3. Das „neues Feld hinten anhängen“ im
  Maßnahmenplan entfällt damit. Index 5 und 6 der Startseite bleiben unberührt;
  in `nodered-flows` liest nichts das Sperre-Feld (am 2026-10-17 geprüft).
* **Die Sperre wirkt nur beim Start.** Ein laufender Lauf wird nicht
  abgebrochen; bei stehender Strecke endet er durch die Bestätigungsregel nach
  dem Schritt-Timeout in ROT.
* **Hydraulik- und Vorderhausschritt** bestätigen nicht über Telegramme und
  bleiben von der neuen Regel unberührt.
* **`set_abgelehnt` wird nicht im MQTT-Callback veröffentlicht** — PubSubClient
  teilt einen Puffer, `topic` und `msg` zeigen hinein. Ein Flag, `loop()`
  veröffentlicht, höchstens alle 2 s.
* **Ganzzahlregel unverändert** (`strtol`, Basis 10): `" 33"` und `"+33"` bleiben
  gültig. Der Header hält das im Hosttest fest; geändert wird nichts.
* **Das Alter ist überlaufsicher** wie `karenz.h` (Lehre aus S1): Faltung je 2^30
  ms in ein 64-Bit-Vorab statt einer Frist mit Vorzeichenvergleich.

## Arbeitsschritte

Gerätezugriffe, die etwas verändern, stehen **einzeln** in einem Bash-Aufruf und
werden vorher angesagt (**HALT** = auf das Ok des Owners warten). Die
produktiven Geräte werden bis zum Rollout nicht angefasst.

### Phase A — Bau und Hosttests

- [x] Rettungsanker `rettungsanker-vor-serielle-strecke-2026-10-17`, Branch
      `serielle-strecke`.
- [x] Baseline `heishamon_esp32_h1_ota`: RAM 61384, Flash 1224317 Byte.
- [x] `src/serielle_strecke.h` und `test/serielle_strecke_test.cpp` (36 Zusicherungen).
- [x] `src/setpayload.h` und `test/setpayload_test.cpp` (52 Zusicherungen).
- [x] Einbindung in `HeishaMon.cpp` (Zählstellen in `readSerial()`,
      `send_pana_command()`, `timeout_serial()`; Veröffentlichung; Wechselmeldung),
      `commands.cpp`, `notbetrieb.h/.cpp`, `webfunctions.cpp`.
- [x] `test/notbetrieb_test.cpp`: Abschnitt 16, bisherige Fälle über den Hüllaufruf
      `tick_lebt()`. **Gegenproben:** Bestätigungsregel entschärft → 7
      Zusicherungen rot; Sperre entfernt → 3 rot.
- [x] `./test/hosttests.sh` — alle 20 grün; alle sechs Envs ohne Warnung.
- [x] Doku: `MQTT-Topics.md`, README (Aufbau, MQTT-Schnittstelle, Abschnitt zur
      Strecke), `Ablauf-Notbetrieb.md`, `test/README.md`, `CLAUDE.md`
      (Headerliste), Changelog in `src/version.h`.

### Phase B — Prüfling `h1b` (HALT vor jedem Gerätezugriff)

Ablauf der Ausleihe nach [`test/README.md`](test/README.md), „Prüfstand
aufsetzen“. Immer nur **ein** Board; vorher bedenken, ob die Stufe gerade ihren
Notanker braucht. Der Prüfling hat **keine Wärmepumpe** — das ist hier der
Messaufbau, nicht eine Lücke: Er ist die stumme Strecke.

- [x] **HALT:** Owner leiht `h1b` (usbmodem1101, IP 192.168.2.194); Prüfling-Build
      (`heishamon_esp32_usb`, Port per `--upload-port`, die ini bleibt
      unberührt) aufgespielt; `mqtt_port` für die Topics auf 1883.
- [x] **Stumme Strecke:** 60-s-Mitschnitt: 10 Abfragen, 10 Timeouts, kein `err`.
      `telegram_timeout` = 4 bei `telegram_age` = 30; `ok` und `err` bleiben 0.
      **Nicht doppelt gezählt.**
- [x] **Sperre:** Feld 4 der Statusroute wechselt nach 30 s von 2 auf 3; die Seite
      trägt Überschrift und Absatz laut E2, der Knopf ist ausgeblendet; MQTT-Log
      „Keine gueltigen Telegramme … (Strecke stumm)“.
      **Nicht ausgelöst:** `POST /notbetrieb/start` — der Prüfling trägt die
      Adresse des Hydraulik-Switch (192.168.2.180) und die des KNX-Zugangs aus
      der Backup-Konfiguration; ein Fehlschlag der Sperre hätte einen Request an
      den Switch bedeutet. Die Abweisung ist dieselbe Variable, die die Seite
      liest (`notbetrieb_starten()`), und im Hosttest belegt.
- [x] **S6:** `33.5` → `set_abgelehnt` 1, `…_letzter` = `Z1HeatRequestTemperature=33.5`;
      Wert 99 auf `DHWTemp` → 2 (`DHWTemp=99`); zwei leere Payloads → 4. Die
      Test-Datenpunkte stehen danach auf `NaN` bzw. `null` (der Adapter wandelt
      um); beim nächsten Verbinden werden sie im Karenzfenster verworfen und
      nicht als abgelehnt gezählt.
- [x] **Zwei Fehler fand erst der Prüfling,** beide behoben und neu geflasht:
      (1) Das Alter stand nach einem Verbinden *innerhalb* von `setup()` mit dem
      Wert seit `millis()` = 0 retained beim Broker (324 s) — ein Wächter hätte
      bei jedem langsamen Boot angeschlagen; jetzt wird nach dem Start der Uhr
      sofort neu veröffentlicht. (2) `set_abgelehnt_letzter` lautete `??=33.5`:
      `write_mqtt_log()` veröffentlicht im MQTT-Callback und überschrieb den
      Puffer, auf den `topic` zeigt; jetzt wird davor gebucht. Beide wären kein
      Hosttest aufgefallen.
- [ ] **Rückgabe** in der sicherheitskritischen Reihenfolge: **erst `mqtt_port`
      auf 1884, dann die Stufen-Firmware flashen.** Gegenprobe über `/settings`:
      Port 1884 und Hostname `HeishaMon32_h1b` ablesen.

**Nicht am Prüfling prüfbar:** der Positivpfad (gültige Telegramme steigen,
`err` bleibt nahe 0, der Knopf gibt sich frei, ein echter Lauf bestätigt wie
bisher in 88 s). Er wird bei der Abnahme nach dem Rollout belegt.

### Phase C — Merge, Rollout, Abnahme

- [ ] `/pruefen-vor-merge`.
- [ ] `/rollout` nach Freigabe des Owners; Abnahme an der Anlage: `telegram_ok`
      steigt, `telegram_age` einstellig, `telegram_err` und `telegram_timeout`
      im Betrieb nahe 0, Notbetrieb bereit (`Sperre` 0). Ein Lauf an der Anlage
      nur auf ausdrücklichen Auftrag und einzeln aufgerufen.
- [ ] Auftrag an `nodered-flows` anlegen und übergeben
      ([`Auftrag-Waechter-Serielle-Strecke.md`](Auftrag-Waechter-Serielle-Strecke.md)) —
      **erst nach dem Rollout**, vorher gibt es die Topics nicht.
- [ ] Maßnahmenplan: S4, S6, P4 auf erledigt setzen.

## Messwerte

| | Baseline 3.24.0 | 3.25.0 | Delta |
| --- | --- | --- | --- |
| `heishamon_esp32_h1_ota` RAM | 61384 | 61512 | +128 |
| `heishamon_esp32_h1_ota` Flash | 1224317 | 1226437 | +2120 |
| `heishamon_esp32_h2_ota` RAM | 61384 | 61512 | +128 |
| `heishamon_esp32_h2_ota` Flash | 1223653 | 1225809 | +2156 |

## Offene Punkte

* Die Auswertung gehört in den `WP_Bridge_Waechter` — Auftrag siehe oben.
* Die Zähler gelten seit dem Start; ein Rückgang heißt Neustart. Der Wächter
  behandelt sie wie `boot_count`.
