#!/usr/bin/env python3
"""
frame_diff.py – Vergleicht HeishaMon-Rohtelegramme (0x71, 203 Byte) ueber einen Mitschnitt

Version 1.1.0 (2026-08-15)
Changelog:
  1.1.0 – Zwei Fehler aus dem ersten realen Einsatz behoben:
          (1) Bloecke werden jetzt an den Marker-Zeilen des Logs getrennt statt
              stur bis 203 Byte gezaehlt. Ein Kommandotelegramm (110 Byte, nach
              "Send command") verschob vorher den Raster, wodurch ALLE folgenden
              Frames um N Byte verrutschten und die Doku-Spalte nicht mehr zu den
              Werten passte. Fremde Blocklaengen werden jetzt gemeldet und vom
              Vergleich ausgenommen.
          (2) Ausgabe als Verlauf statt als Spalte je Frame - bei 70 Frames war
              die Tabelle 482 Zeichen breit und damit unlesbar.
  1.0.0 – Erstfassung: Frame-Extraktion, Laengen- und Pruefsummenpruefung,
          Diff aller Frames, Anreicherung aus ProtocolByteDecrypt.md

Aufruf:
    ./frame_diff.py <logdatei> [--doku PFAD] [--alle] [--spalten]

    --doku     Pfad zu ProtocolByteDecrypt.md (Default: eine Ebene ueber diesem Skript)
    --alle     auch unveraenderte Bytes ausgeben (Default: nur Unterschiede)
    --spalten  alte Spaltenansicht (ein Block je Frame) - nur bei wenigen Frames sinnvoll

Eingabeformat: Debug-/Telnet-Mitschnitt. Aufeinanderfolgende "data:"-Zeilen
bilden einen Block; jede andere Zeile (Valid data, Send command, Decode topics,
Callback from mqtt ...) beendet ihn.
"""

import sys
import os
import re
import argparse

# --- Protokollkonstanten (Belege: src/telegram.h, src/HeishaMon.h) ----------
FRAME_LEN = 203          # Antworttelegramm inkl. Pruefsumme
TYPE_BYTE = 0x71         # Typbyte des Antworttelegramms
LEN_BYTE = 0xC8          # Datenlaengenbyte des Antworttelegramms (200 + 3 = 203)
COMMAND_LEN = 110        # QUERYSIZE - Abfrage- und Kommandotelegramme
TEMP_OFFSET = 128        # die meisten Temperaturbytes dekodieren als Wert-128

# Doku liegt eine Ebene ueber test/ - relativ zum Skript, damit das Werkzeug
# auch in einem anders ausgecheckten Repo findet, was es braucht
DOKU_DEFAULT = os.path.join(os.path.dirname(os.path.abspath(__file__)), os.pardir, "ProtocolByteDecrypt.md")

# Der Log kann doppelt praefixiert sein:
#   [15.08.2026, 3:01:10,776 PM] [2026-08-15 15:01:10] <DBG> data: 71 C8 ...
# Gesucht wird der ISO-Zeitstempel; fehlt er, dient die Zeilennummer als Label.
RE_ZEIT = re.compile(r"\[(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})\]")
RE_HEX = re.compile(r"data:\s*((?:[0-9A-Fa-f]{2}[\s]*)+)")
# Tabellenzeile der Doku: |  TOP14 | 142 | 7c |  Convert to DEC-128 | Beschreibung |
# Gruppen: 1=Topic, 2=Byte-Nr, 3=Dekodiervorschrift, 4=Beschreibung
RE_DOKU = re.compile(r"^\|\s*(TOP[^|]*?)\s*\|\s*(\d+)\s*\|[^|]*\|\s*([^|]*?)\s*\|\s*(.*?)\s*\|?\s*$")


def parse_bloecke(text):
    """Zerlegt den Logtext in zusammenhaengende data-Bloecke.

    Trenner ist JEDE Zeile ohne "data:" - im Log stehen zwischen zwei
    Telegrammen immer Marker wie "Valid data" oder "Decode topics". Dadurch
    behaelt jeder Block seine echte Laenge, statt in ein festes Raster
    gepresst zu werden.

    Zu jedem Block werden die Zeilenmarken (Byte-Offset -> Zeitstempel)
    mitgefuehrt, damit auch beim Aufteilen eines markerlosen Mitschnitts
    jedes Telegramm seinen eigenen Zeitstempel behaelt.

    Rueckgabe: Liste von (marken, [bytes]).
    """
    bloecke = []
    current = []
    marken = []

    for nr, zeile in enumerate(text.splitlines(), 1):
        treffer = RE_HEX.search(zeile)
        if treffer:
            zeit = RE_ZEIT.search(zeile)
            marken.append((len(current), zeit.group(1) if zeit else f"Zeile {nr}"))
            current.extend(int(b, 16) for b in treffer.group(1).split())
        elif current:                                         # Nicht-data-Zeile beendet den Block
            bloecke.append((marken, current))
            current, marken = [], []

    if current:                                               # Datei endet mitten im Block
        bloecke.append((marken, current))
    return bloecke


def label_fuer(marken, offset):
    """Zeitstempel der data-Zeile, in der das Byte an <offset> stand."""
    treffer = "?"
    for start, label in marken:
        if start <= offset:
            treffer = label
        else:
            break
    return treffer


def teile_block(marken, daten):
    """Schneidet aneinanderhaengende Antworttelegramme aus einem Block.

    Deckt zwei Logformen ab: Steht zwischen den Telegrammen ein Marker, ist der
    Block genau ein Telegramm und es wird nichts geteilt. Enthaelt der Mitschnitt
    dagegen nur data-Zeilen (z. B. von Hand zusammenkopiert), haengen mehrere
    Telegramme aneinander und werden hier an ihrer Signatur getrennt.

    Rueckgabe: (Liste von (label, frame), Restlaenge oder 0)
    """
    frames = []
    i = 0
    # Signatur des Antworttelegramms: 71 C8 (Typ + Datenlaenge 200 -> 200+3=203)
    while i + FRAME_LEN <= len(daten) and daten[i] == TYPE_BYTE and daten[i + 1] == LEN_BYTE:
        frames.append((label_fuer(marken, i), daten[i:i + FRAME_LEN]))
        i += FRAME_LEN
    return frames, len(daten) - i


def pruefe_frame(daten):
    """Validiert Typbyte und Pruefsumme eines Antworttelegramms.

    Rueckgabe: Liste von Fehlertexten (leer = in Ordnung).
    """
    fehler = []
    if daten[0] != TYPE_BYTE:
        fehler.append(f"Typbyte {daten[0]:02X} statt {TYPE_BYTE:02X}")
    # Belegt in telegram.h: Summe ALLER Bytes inkl. Pruefsumme muss 0 ergeben
    if sum(daten) & 0xFF != 0:
        fehler.append("Pruefsumme falsch")
    return fehler


def lade_doku(pfad):
    """Liest die Byte->Bedeutung-Zuordnung aus ProtocolByteDecrypt.md.

    Fehlt oder klemmt die Datei, laeuft der Vergleich ohne Bedeutungen weiter -
    die Zahlen sind auch so brauchbar.
    """
    zuordnung = {}
    if not os.path.isfile(pfad):
        print(f"Hinweis: {pfad} nicht gefunden - Ausgabe ohne Bedeutungen\n", file=sys.stderr)
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
                    beschreibung = re.sub(r"<br\s*/?>", " ", beschreibung).strip()
                    # "TOP" ohne Nummer heisst in der Doku: kein eigenes Topic
                    label = f"{top} {beschreibung}" if top != "TOP" else beschreibung
                    zuordnung[nr] = (label.strip(), dekodierung.strip())
    except OSError as exc:
        print(f"Hinweis: {pfad} nicht lesbar ({exc}) - Ausgabe ohne Bedeutungen\n", file=sys.stderr)
    return zuordnung


def laeufe(werte):
    """Fasst aufeinanderfolgende gleiche Werte zu Laeufen zusammen.

    Aus [94,94,94,95,95,97] wird [(0,2,94),(3,4,95),(5,5,97)] - das macht aus
    70 Spalten eine kurze Kette von Wechselpunkten.
    """
    ergebnis = []
    start = 0
    for i in range(1, len(werte) + 1):
        if i == len(werte) or werte[i] != werte[start]:
            ergebnis.append((start, i - 1, werte[start]))
            start = i
    return ergebnis


def formatiere_verlauf(werte, labels, ist_temperatur):
    """Baut die Verlaufszeile: 'F0-F12 (15:00:58): 94 = 20 C  ->  F13-F35 ...'

    Bleibt ein Byte ueber den ganzen Mitschnitt gleich, waeren Spanne und
    Zeitstempel nur Ballast - dann steht dort schlicht "konstant". Das betrifft
    bei --alle die grosse Mehrheit der 203 Bytes.
    """
    abschnitte = laeufe(werte)
    if len(abschnitte) == 1:
        wert = abschnitte[0][2]
        text = f"konstant ueber alle {len(werte)}: {wert:02X}"
        return text + (f" = {wert - TEMP_OFFSET} C" if ist_temperatur else "")

    teile = []
    for start, ende, wert in abschnitte:
        spanne = f"F{start}" if start == ende else f"F{start}-F{ende}"
        text = f"{spanne} ({labels[start]}): {wert:02X}"
        if ist_temperatur:
            text += f" = {wert - TEMP_OFFSET} C"
        teile.append(text)
    return "  ->  ".join(teile)


def main():
    parser = argparse.ArgumentParser(description="Diff von HeishaMon-Rohtelegrammen")
    parser.add_argument("logdatei", help="Datei mit dem Debug-/Telnet-Mitschnitt")
    parser.add_argument("--doku", default=DOKU_DEFAULT)
    parser.add_argument("--alle", action="store_true", help="auch unveraenderte Bytes zeigen")
    parser.add_argument("--spalten", action="store_true", help="alte Spaltenansicht je Frame")
    args = parser.parse_args()

    # --- Eingabe lesen und validieren ---
    if not os.path.isfile(args.logdatei):
        sys.exit(f"FEHLER: {args.logdatei} existiert nicht")
    try:
        with open(args.logdatei, encoding="utf-8", errors="replace") as fh:
            text = fh.read()
    except OSError as exc:
        sys.exit(f"FEHLER: {args.logdatei} nicht lesbar ({exc})")

    bloecke = parse_bloecke(text)
    if not bloecke:
        sys.exit("FEHLER: keine data-Zeilen gefunden - passt das Logformat?")

    # --- Bloecke klassifizieren: nur echte Antworttelegramme werden verglichen ---
    frames, fremde = [], {}
    for marken, daten in bloecke:
        gefunden, rest = teile_block(marken, daten)
        frames.extend(gefunden)
        if rest:                                              # Kommandotelegramm o. Fragment
            fremde.setdefault(rest, []).append(label_fuer(marken, len(daten) - rest))

    print(f"{len(bloecke)} Bloecke gelesen: {len(frames)} Antworttelegramme ({FRAME_LEN} Byte)")
    for laenge, labels in sorted(fremde.items()):
        art = "Kommando-/Abfragetelegramm" if laenge == COMMAND_LEN else "unbekannte Laenge"
        print(f"  {len(labels)}x {laenge} Byte - {art}, nicht verglichen (z. B. {labels[0]})")

    if len(frames) < 2:
        sys.exit(f"FEHLER: mindestens 2 Antworttelegramme noetig, gefunden: {len(frames)}")

    # --- Frames einzeln pruefen; nur Auffaelliges wird einzeln benannt ---
    defekt = [(i, label, pruefe_frame(daten)) for i, (label, daten) in enumerate(frames)
              if pruefe_frame(daten)]
    if defekt:
        print(f"\n  {len(defekt)} Telegramm(e) mit Befund:")
        for i, label, fehler in defekt:
            print(f"    Frame {i} ({label}): {'; '.join(fehler)}")
    else:
        print(f"  alle {len(frames)} auf Typbyte und Pruefsumme geprueft, keine Beanstandung")

    doku = lade_doku(args.doku)
    labels = [f[0].split()[-1] if " " in f[0] else f[0] for f in frames]   # nur Uhrzeit als Label

    # --- Vergleich ---
    print()
    geaendert = 0
    for pos in range(FRAME_LEN):
        werte = [f[1][pos] for f in frames]
        if not args.alle and len(set(werte)) == 1:
            continue
        geaendert += 1

        eintrag = doku.get(pos)
        bedeutung, dekodierung = eintrag if eintrag else ("", "")
        if pos == FRAME_LEN - 1:
            bedeutung = "PRUEFSUMME (folgt jeder Aenderung, kein Fund)"
        elif not bedeutung:
            bedeutung = "(in der Doku nicht aufgefuehrt)"

        if args.spalten:
            spalten = " ".join(f"{v:02X}" for v in werte)
            print(f"{pos:>5} {bedeutung[:52]:<52} {spalten}")
        else:
            print(f"Byte {pos:>3}  {bedeutung}")
            print(f"         {formatiere_verlauf(werte, labels, '-128' in dekodierung)}")

    if geaendert == 0:
        print("  keine Unterschiede zwischen den Telegrammen")
    else:
        print(f"\n{geaendert} Byte-Position(en){'' if args.alle else ' mit Unterschieden'}")


if __name__ == "__main__":
    main()
