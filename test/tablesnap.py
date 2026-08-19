#!/usr/bin/env python3
"""Momentaufnahme der Topic-Tabelle eines HeishaMon-Geraets ueber /tablerefresh.

Zweck: die Abnahme nach dem Flashen zu einem Zeilenvergleich machen. Vor dem OTA
einmal laufen lassen, nach dem OTA ein zweites Mal, dann 'diff' der beiden
Dateien - was uebrig bleibt, sind entweder laufende Messwerte oder ein Befund.

  ./tablesnap.py 192.168.2.120 > vorher.txt
  ./tablesnap.py 192.168.2.120 > nachher.txt
  diff vorher.txt nachher.txt

Ausgabe je Zeile:  TOP<n>|<Name>|<Wert>|<Klartext bzw. Einheit>
Nur Standardbibliothek, kein Geraeteeingriff (reines GET).
"""
import html
import re
import sys
import urllib.request

ZELLEN = re.compile(r"<td>(.*?)</td>", re.S)


def schnappschuss(ip: str, timeout: float = 10.0) -> list[str]:
    with urllib.request.urlopen(f"http://{ip}/tablerefresh", timeout=timeout) as antwort:
        seite = antwort.read().decode("utf-8", "replace")
    zeilen = []
    for roh in seite.split("<tr>"):
        felder = [html.unescape(z).strip() for z in ZELLEN.findall(roh)]
        if len(felder) >= 4:
            zeilen.append("|".join(felder[:4]))
    return zeilen


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__, file=sys.stderr)
        return 2
    try:
        zeilen = schnappschuss(sys.argv[1])
    except OSError as fehler:
        print(f"FEHLER: {sys.argv[1]} nicht erreichbar ({fehler})", file=sys.stderr)
        return 1
    if not zeilen:
        print("FEHLER: keine Tabellenzeilen gefunden", file=sys.stderr)
        return 1
    print("\n".join(zeilen))
    print(f"# {len(zeilen)} Zeilen von {sys.argv[1]}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
