#!/usr/bin/env python3
"""Ausgewaehlte TOP-Werte am laufenden Geraet ueber die Zeit verfolgen.

Zweck: `tablesnap.py` liefert den Schnappschuss - dieses Werkzeug den Verlauf.
Es fragt /tablerefresh im Takt ab und meldet jede Aenderung mit Zeitstempel.
Damit ist zu sehen, WANN ein Set-Kommando durchschlaegt, wie lange die
Waermepumpe dafuer braucht, ob ein Wert wieder zurueckspringt und ob ein
fremder Sender (der 5-min-Re-Assert der Kaskadensteuerung) dazwischenfunkt.

  ./top_watch.py 192.168.2.120 27 29 76 --dauer 120
  ./top_watch.py 192.168.2.120 --alle --dauer 60 --takt 10

Kein Geraeteeingriff: reines GET auf /tablerefresh, wie `tablesnap.py`.
Zwei Werkzeuge duerfen parallel laufen - der Hexlog-Mitschnitt
(`produktiv_mitschnitt.py`, Telnet) wird davon nicht beruehrt.

Nur Standardbibliothek.

Stand 3.11.0 (neu in dieser Fassung; erstmals eingesetzt fuer M1 des
Vorhabens Notbetrieb, 2026-08-20).
"""
import argparse
import re
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from tablesnap import schnappschuss  # noqa: E402

TAKT_MIN = 2        # unter 2 s hat der Abfragezyklus der WP (~6 s) nichts Neues
DAUER_MAX = 3600    # Obergrenze, damit ein vergessener Lauf nicht ewig laeuft
ZEILE = re.compile(r"^TOP(\d+)\|([^|]*)\|([^|]*)\|(.*)$")


def zerlege(zeilen):
    """Tabellenzeilen in {TOP-Nummer: (Name, Wert, Einheit)} umwandeln."""
    werte = {}
    for zeile in zeilen:
        treffer = ZEILE.match(zeile)
        if treffer:
            nummer = int(treffer.group(1))
            werte[nummer] = (treffer.group(2), treffer.group(3), treffer.group(4))
    return werte


def zeitstempel():
    return time.strftime("%H:%M:%S")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("host", help="IP des HeishaMon")
    ap.add_argument("tops", nargs="*", type=int,
                    help="TOP-Nummern, die verfolgt werden sollen")
    ap.add_argument("--alle", action="store_true",
                    help="alle TOPs verfolgen statt einer Auswahl")
    ap.add_argument("--dauer", type=int, default=120,
                    help=f"Laufzeit in Sekunden (max {DAUER_MAX})")
    ap.add_argument("--takt", type=int, default=5,
                    help=f"Abfrageabstand in Sekunden (min {TAKT_MIN})")
    args = ap.parse_args()

    # Eingaben klemmen statt abzulehnen - ein Messlauf soll nicht an einer
    # Zahl scheitern, aber auch nicht das Geraet fluten.
    if not args.tops and not args.alle:
        print("FEHLER: entweder TOP-Nummern angeben oder --alle", file=sys.stderr)
        return 2
    takt = max(TAKT_MIN, args.takt)
    dauer = max(takt, min(DAUER_MAX, args.dauer))
    gesucht = set(args.tops)

    # Ausgangslage einmal vollstaendig ausgeben - sie ist der Bezugspunkt
    # fuer alles, was danach als Aenderung gemeldet wird.
    try:
        stand = zerlege(schnappschuss(args.host))
    except OSError as fehler:
        print(f"FEHLER: {args.host} nicht erreichbar ({fehler})", file=sys.stderr)
        return 1
    if not stand:
        print("FEHLER: keine Tabellenzeilen gefunden", file=sys.stderr)
        return 1

    fehlend = sorted(gesucht - set(stand))
    if fehlend:
        print(f"# WARNUNG: unbekannte TOPs: {fehlend}", file=sys.stderr)

    beobachtet = sorted(stand) if args.alle else sorted(gesucht & set(stand))
    print(f"# {zeitstempel()} Ausgangslage {args.host} "
          f"({len(beobachtet)} Werte, Takt {takt} s, Dauer {dauer} s)")
    for nummer in beobachtet:
        name, wert, einheit = stand[nummer]
        print(f"  TOP{nummer:<3} {name:<34} {wert:>8}  {einheit}")
    print(f"# {zeitstempel()} ab hier nur noch Aenderungen")

    ende = time.monotonic() + dauer
    fehler_in_folge = 0
    aenderungen = 0
    while time.monotonic() < ende:
        time.sleep(takt)
        # Ein Netzaussetzer darf den Lauf nicht beenden - erst nach fuenf
        # Fehlversuchen in Folge wird abgebrochen, sonst nur gemeldet.
        try:
            neu = zerlege(schnappschuss(args.host))
            fehler_in_folge = 0
        except OSError as fehler:
            fehler_in_folge += 1
            print(f"! {zeitstempel()} Abruf fehlgeschlagen ({fehler})", file=sys.stderr)
            if fehler_in_folge >= 5:
                print("FEHLER: fuenf Abrufe in Folge fehlgeschlagen", file=sys.stderr)
                return 1
            continue

        for nummer in beobachtet:
            alt = stand.get(nummer)
            jetzt = neu.get(nummer)
            if jetzt is None or alt is None or alt[1] == jetzt[1]:
                continue
            print(f"  {zeitstempel()} TOP{nummer:<3} {jetzt[0]:<34} "
                  f"{alt[1]:>8} -> {jetzt[1]:>8}  {jetzt[2]}")
            aenderungen += 1
        stand = neu

    print(f"# {zeitstempel()} Ende, {aenderungen} Aenderungen")
    for nummer in beobachtet:
        name, wert, einheit = stand[nummer]
        print(f"  TOP{nummer:<3} {name:<34} {wert:>8}  {einheit}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
