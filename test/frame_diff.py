#!/usr/bin/env python3
"""
frame_diff.py – Vergleicht HeishaMon-Rohtelegramme (0x71, 203 Byte) ueber mehrere Mitschnitte

Version 1.0.0 (2026-08-15)
Changelog:
  1.0.0 – Erstfassung: Frame-Extraktion aus Telnet-/Debug-Log, Laengen- und
          Pruefsummenpruefung, Diff aller Frames gegen den ersten, Anreicherung
          der geaenderten Bytes mit der Bedeutung aus ProtocolByteDecrypt.md

Aufruf:
    ./frame_diff.py <logdatei> [--doku PFAD] [--alle]

    --doku   Pfad zu ProtocolByteDecrypt.md (Default: eine Ebene ueber diesem Skript)
    --alle   auch unveraenderte Bytes ausgeben (Default: nur Unterschiede)

Eingabeformat: der Debug-Log wie er im Telnet erscheint. Die "data:"-Zeilen
zwischen "Valid data" und "Decode topics" werden zu je einem Frame gruppiert.
Der Zeitstempel der ersten data-Zeile dient als Frame-Label.
"""

import sys
import os
import re
import argparse

# --- Protokollkonstanten (Belege: src/telegram.h) ---------------------------
FRAME_LEN = 203          # vollstaendige Telegrammlaenge inkl. Pruefsumme
TYPE_BYTE = 0x71         # Typbyte des Antworttelegramms
TEMP_OFFSET = 128        # die meisten Temperaturbytes dekodieren als Wert-128

# Die Protokolldoku liegt eine Ebene ueber test/ – relativ zum Skript aufgeloest,
# damit das Werkzeug auch in einem anders ausgecheckten Repo findet, was es braucht
DOKU_DEFAULT = os.path.join(os.path.dirname(os.path.abspath(__file__)), os.pardir, "ProtocolByteDecrypt.md")

# Zeile im Log:  [2026-08-15 13:48:06] <DBG> data: 71 C8 01 10 ...
RE_DATA = re.compile(r"\[([\d\-: ]+)\]\s*<\w+>\s*data:\s*((?:[0-9A-Fa-f]{2}\s*)+)")
# Tabellenzeile in der Doku:  |  TOP14 | 142 | 7c |  Convert to DEC-128 | Beschreibung |
# Gruppen: 1=Topic, 2=Byte-Nr, 3=Dekodiervorschrift, 4=Beschreibung
RE_DOKU = re.compile(r"^\|\s*(TOP[^|]*?)\s*\|\s*(\d+)\s*\|[^|]*\|\s*([^|]*?)\s*\|\s*(.*?)\s*\|?\s*$")


def parse_frames(text):
    """Extrahiert alle vollstaendigen Frames aus dem Logtext.

    Rueckgabe: Liste von (label, [bytes]). Unvollstaendige oder ueberlange
    Frames werden gemeldet und uebersprungen, statt still falsche Diffs zu
    erzeugen.
    """
    frames = []
    current = []          # Bytes des gerade gelesenen Frames
    label = None          # Zeitstempel der ersten data-Zeile dieses Frames
    warnungen = []

    for zeile in text.splitlines():
        treffer = RE_DATA.search(zeile)
        if not treffer:
            continue
        if not current:
            label = treffer.group(1).strip()
        current.extend(int(b, 16) for b in treffer.group(2).split())

        # Frame gilt als komplett, sobald die Sollaenge erreicht ist
        if len(current) >= FRAME_LEN:
            if len(current) > FRAME_LEN:
                warnungen.append(f"{label}: {len(current)} Bytes statt {FRAME_LEN} – abgeschnitten")
            frames.append((label, current[:FRAME_LEN]))
            current = []
            label = None

    if current:
        warnungen.append(f"{label}: unvollstaendiger Frame ({len(current)} Bytes) – ignoriert")
    return frames, warnungen


def pruefe_frame(daten):
    """Validiert Typbyte und Pruefsumme. Rueckgabe: Liste von Fehlertexten (leer = ok)."""
    fehler = []
    if daten[0] != TYPE_BYTE:
        fehler.append(f"Typbyte {daten[0]:02X} statt {TYPE_BYTE:02X}")
    # Belegt in telegram.h: Summe ALLER Bytes inkl. Pruefsumme muss 0 ergeben
    if sum(daten) & 0xFF != 0:
        fehler.append("Pruefsumme falsch")
    return fehler


def lade_doku(pfad):
    """Liest die Byte->Bedeutung-Zuordnung aus ProtocolByteDecrypt.md.

    Fehlt die Datei, laeuft das Skript ohne Bedeutungen weiter – der Diff
    ist auch ohne sie brauchbar.
    """
    zuordnung = {}
    if not os.path.isfile(pfad):
        print(f"Hinweis: {pfad} nicht gefunden – Ausgabe ohne Bedeutungen\n", file=sys.stderr)
        return zuordnung
    try:
        with open(pfad, encoding="utf-8", errors="replace") as fh:
            for zeile in fh:
                treffer = RE_DOKU.match(zeile)
                if not treffer:
                    continue
                top, byte_nr, dekodierung, beschreibung = treffer.groups()
                nr = int(byte_nr)
                if 0 <= nr < FRAME_LEN:                       # Bounds-Check
                    top = top.strip()
                    # HTML-Umbrueche der Tabelle glaetten
                    beschreibung = re.sub(r"<br\s*/?>", " ", beschreibung).strip()
                    # "TOP" ohne Nummer heisst in der Doku: kein eigenes Topic
                    label = f"{top:<8} {beschreibung}" if top != "TOP" else f"{'--':<8} {beschreibung}"
                    # Dekodiervorschrift getrennt merken – sie steuert die Temperaturdeutung
                    zuordnung[nr] = (label, dekodierung.strip())
    except OSError as exc:
        print(f"Hinweis: {pfad} nicht lesbar ({exc}) – Ausgabe ohne Bedeutungen\n", file=sys.stderr)
    return zuordnung


def main():
    parser = argparse.ArgumentParser(description="Diff von HeishaMon-Rohtelegrammen")
    parser.add_argument("logdatei", help="Datei mit dem Debug-/Telnet-Mitschnitt")
    parser.add_argument("--doku", default=DOKU_DEFAULT)
    parser.add_argument("--alle", action="store_true", help="auch unveraenderte Bytes zeigen")
    args = parser.parse_args()

    # --- Eingabe lesen und validieren ---
    if not os.path.isfile(args.logdatei):
        sys.exit(f"FEHLER: {args.logdatei} existiert nicht")
    try:
        with open(args.logdatei, encoding="utf-8", errors="replace") as fh:
            text = fh.read()
    except OSError as exc:
        sys.exit(f"FEHLER: {args.logdatei} nicht lesbar ({exc})")

    frames, warnungen = parse_frames(text)
    for w in warnungen:
        print(f"WARNUNG: {w}", file=sys.stderr)
    if len(frames) < 2:
        sys.exit(f"FEHLER: mindestens 2 vollstaendige Frames noetig, gefunden: {len(frames)}")

    # --- Frames einzeln validieren, damit ein kaputter Frame nicht als Aenderung erscheint ---
    print(f"{len(frames)} Frames gelesen\n")
    for i, (label, daten) in enumerate(frames):
        fehler = pruefe_frame(daten)
        status = "OK" if not fehler else "; ".join(fehler)
        print(f"  Frame {i}: {label}  [{status}]")
    print()

    doku = lade_doku(args.doku)

    # --- Diff: jedes Byte ueber alle Frames vergleichen ---
    referenz = frames[0][1]
    kopf = f"{'Byte':>5} {'Doku-Bedeutung':<52} " + " ".join(f"F{i:<3}" for i in range(len(frames)))
    print(kopf)
    print("-" * len(kopf))

    geaendert = 0
    for pos in range(FRAME_LEN):
        werte = [f[1][pos] for f in frames]
        if not args.alle and len(set(werte)) == 1:
            continue
        geaendert += 1
        eintrag = doku.get(pos)
        bedeutung, dekodierung = eintrag if eintrag else ("", "")
        # Das letzte Byte ist die Pruefsumme – sie zieht jeder Datenaenderung
        # nach und ist selbst nie ein inhaltlicher Fund
        if pos == FRAME_LEN - 1:
            bedeutung = "PRUEFSUMME (folgt jeder Aenderung, kein Fund)"
        elif not bedeutung:
            bedeutung = "(in der Doku nicht aufgefuehrt)"
        spalten = " ".join(f"{v:02X}  " for v in werte)
        print(f"{pos:>5} {bedeutung[:52]:<52} {spalten}")

        # Zweitzeile mit der Temperaturdeutung, falls die Doku sie vorsieht
        if "-128" in dekodierung:
            temps = " ".join(f"{v - TEMP_OFFSET:>3} " for v in werte)
            print(f"{'':>5} {'   -> als Wert-128 [Grad C]':<52} {temps}")

    if geaendert == 0:
        print("  keine Unterschiede zwischen den Frames")
    else:
        print(f"\n{geaendert} Byte-Position(en) mit Unterschieden")


if __name__ == "__main__":
    main()
