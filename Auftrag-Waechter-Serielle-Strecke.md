# Auftrag: Wächter auf die serielle Strecke und auf abgelehnte Set-Befehle

> **Ergebnis: offen (2026-10-17) — erst nach dem Rollout von 3.25.0 zu
> übergeben**, vorher gibt es die Topics nicht. Gegenstück im Firmware-Repo:
> [`Arbeitsplan-Serielle-Strecke-3.25.0.md`](Arbeitsplan-Serielle-Strecke-3.25.0.md),
> Befunde S4 und S6 im
> [`Massnahmenplan-Codedurchsicht-2026-10-15.md`](Massnahmenplan-Codedurchsicht-2026-10-15.md).

*Zum Kopieren in eine Session im Projekt `nodered-flows` gedacht; setzt dort
keinen Vorkontext voraus.*

## Worum es geht

Die Firmware (HeishaMon, ab 3.25.0) meldet unter `<präfix>/info/` sechs neue
Werte. Der `WP_Bridge_Waechter` (iobroker-js, `common/kaskade/WP_Bridge_Waechter.js`)
soll sie auswerten. Die Firmware selbst wertet nichts aus — sie zählt und meldet.

| Topic | Inhalt | Takt |
| --- | --- | --- |
| `info/telegram_ok` | gültige Antworttelegramme der WP seit dem Start | 5 min, beim Verbinden, bei jedem Wechsel stumm ↔ lebendig |
| `info/telegram_err` | Abfragen, auf die Bytes kamen, aber kein gültiges Telegramm (Prüfsumme, Typ, abgeschnitten) | wie oben |
| `info/telegram_timeout` | Abfragen, auf die gar kein Byte kam (Leitung stumm) | wie oben |
| `info/telegram_age` | Sekunden seit dem letzten gültigen Telegramm. Vor dem ersten: seit dem Start des Abfragezyklus | wie oben |
| `info/set_abgelehnt` | abgelehnte Set-Befehle seit dem Start | bei Änderung (höchstens alle 2 s), 5 min, beim Verbinden |
| `info/set_abgelehnt_letzter` | letzter abgelehnter Befehl als `Name=Wert`, auf 64 Zeichen gekürzt | wie oben |

Alle retained, alle Zahlen als Zahlen. Die drei Zähler und `set_abgelehnt`
gelten **seit dem Start des Geräts** — ein Rückgang heißt Neustart (dann
`info/boot_count` und `info/boot_reason` ansehen).

## Was der Wächter tun soll

1. **`telegram_age`: Stale-Guard wie bei den anderen Kanälen** und ein Alarm ab
   einem Schwellwert. Die Firmware sperrt den Notbetriebsknopf bei 30 s; ein
   Alarm bei **60 s** meldet einen echten Abriss und nicht einen einzelnen
   Aussetzer (der Abfragezyklus liegt bei rund 6 s). Der Wert ist ein Vorschlag —
   die Schwelle entscheidet, wer die Meldung bekommt.
2. **`telegram_err` und `telegram_timeout`: Anstieg melden**, wie beim
   `boot_count`. Im Betrieb bleiben beide nahe 0; ein gelegentlicher Ausreißer
   (eine Abfrage in vielen Stunden) ist normal. Eine Schwelle „mehr als N
   innerhalb einer Stunde“ ist sinnvoller als „jeder Anstieg“ — **die
   Normalrate ist nicht gemessen**; nach dem Rollout einige Tage `telegram_err`
   und `telegram_timeout` beobachten, bevor die Schwelle festgelegt wird.
3. **`set_abgelehnt`: Anstieg melden** (jeder Anstieg ist ein Befund) und
   `set_abgelehnt_letzter` in die Meldung schreiben. Hintergrund: Der
   Notbetriebssender schickte vom 2026-10-10 bis 2026-10-15 Dezimalwerte, die
   die Firmware komplett verwarf, ohne dass es jemand bemerkte
   ([`Auftrag-Notbetriebssender-Ganzzahl.md`](Auftrag-Notbetriebssender-Ganzzahl.md)).
   Der Befehls-Wächter deckt solche Fälle wegen der 0,5-K-Toleranz nicht ab.

Ein Rückgang eines Zählers (Neustart) ist **kein** Alarm, sondern setzt die
Bezugsbasis zurück.

## Was dieser Auftrag nicht entscheidet

* Wer die Meldung bekommt und wie sie formuliert ist — Sache der Wächter-Konvention.
* Ob die Firmware Dezimalwerte selbst rundet — nein, siehe S6 im Maßnahmenplan
  („ablehnen und melden“ ist bei Befehlen an die Wärmepumpe sicherer als „still
  anders ausführen“).
