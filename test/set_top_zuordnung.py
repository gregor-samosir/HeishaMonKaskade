#!/usr/bin/env python3
"""Ordnet jedem Set-Kommando das State-Topic zu, das es zurueckliest.

Zweck: die Tabellen in SET-TOP-Zuordnung.md erzeugen, statt sie von Hand zu
pflegen. Aendert sich eine Zeile in commands.cpp oder decode.cpp, faellt der
Unterschied hier auf - die Doku laesst sich gegen die Ausgabe diffen.

  ./set_top_zuordnung.py                  # alle Tabellen nach stdout
  ./set_top_zuordnung.py --pruefen        # gegen SET-TOP-Zuordnung.md abgleichen

Der Abgleich vergleicht die PAARE (welches SET liest ueber welches TOP zurueck),
nicht den Text: die Doku traegt zusaetzlich Fussnoten und Bewertungsspalten, ein
woertlicher diff waere deshalb dauerhaft rot. Rueckgabewert 1 bei Abweichung -
so faellt eine geaenderte Code-Zeile auf, bevor die Doku still veraltet.

Zugeordnet wird ueber Byte-Position UND Bitmaske, nie ueber Namen: Namen
koennen passen, wo die Bytes es nicht tun, und umgekehrt. Die tatsaechlich
beschriebene Maske eines Kommandos entsteht aus seinem Wertebereich - fuer
jeden erlaubten Wert das Protokollbyte bilden, alle gesetzten Bits verodern.
Nur so werden die Faelle sichtbar, in denen die Tabellenmaske 0xFF lautet, das
Kommando aber nur eine Bitgruppe belegt (QuietMode und PowerfulMode auf Byte 7
teilen sich das Byte, siehe Kommentarblock in commands.cpp).

Nur Standardbibliothek, kein Geraeteeingriff - es wird ausschliesslich der
Quelltext im Repository gelesen.
"""
import re
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parent.parent / "src"

# Bitmasken der 1-Byte-Dekodierer aus decode.cpp. Aendert sich dort ein
# Dekodierer oder kommt einer dazu, gehoert er hier ergaenzt - sonst gilt
# fuer seine Zeilen die Vorgabe 0xFF (ganzes Byte).
DECODER_MASK = {
    "getBit1and2": 0xC0,
    "getBit3and4": 0x30,
    "getBit5and6": 0x0C,
    "getBit7and8": 0x03,
    "getBit3and4and5": 0x38,
    "getRight3bits": 0x07,
    "getOpMode": 0x3F,
}

# Protokollbytes fuer OperationMode 0-6, Spiegel von opModeBytes[] in commands.cpp
OPMODE_BYTES = [18, 19, 24, 33, 34, 35, 40]

# Laenge des Kommandotelegramms (QUERYSIZE in HeishaMon.h): Indizes 0..109.
# Ein State-Topic ab Byte 110 kann grundsaetzlich kein Set-Kommando haben,
# weil die Adresse im Kommando nicht existiert.
QUERYSIZE = 110


def bitgruppe(mask):
    """Maske -> Bitgruppe in Projektzaehlung (Bit 1 ist das hoechstwertige)."""
    if mask == 0xFF:
        return "ganz"
    gesetzt = [i for i in range(1, 9) if mask & (1 << (8 - i))]
    if not gesetzt:
        return "-"
    # zusammenhaengende Gruppe kurz schreiben: 3+4 bzw. 3-5
    if len(gesetzt) > 1 and gesetzt == list(range(gesetzt[0], gesetzt[-1] + 1)):
        return f"{gesetzt[0]}+{gesetzt[1]}" if len(gesetzt) == 2 else f"{gesetzt[0]}-{gesetzt[-1]}"
    return "+".join(str(i) for i in gesetzt)


def tabelle_lesen(datei, muster, klammer):
    """Den Inhalt einer C-Tabelle aus einer Quelldatei holen."""
    try:
        text = (SRC / datei).read_text()
    except OSError as fehler:
        sys.exit(f"Fehler: {datei} nicht lesbar - {fehler}")
    treffer = re.search(klammer, text, re.S)
    if not treffer:
        sys.exit(f"Fehler: Tabelle in {datei} nicht gefunden - Aufbau geaendert?")
    return list(re.finditer(muster, treffer.group(1)))


def set_kommandos():
    """setCommands[] aus commands.cpp, je Zeile mit effektiv belegter Maske."""
    zeilen = tabelle_lesen(
        "commands.cpp",
        r"\{\s*(\d+),\s*(\d+),\s*(0x[0-9A-Fa-f]+),\s*(CONV_\w+),\s*\"(\w+)\","
        r"\s*(-?\d+),\s*(-?\d+),\s*(-?\d+)\s*\}",
        r"static const SetCommand setCommands\[\] = \{(.*?)\n\};",
    )
    ergebnis = []
    for t in zeilen:
        nr, pos, mask, conv, name, lo, hi, param = t.groups()
        eintrag = dict(nr=int(nr), pos=int(pos), mask=int(mask, 16), conv=conv,
                       name=name, lo=int(lo), hi=int(hi), param=int(param))
        # effektive Maske ueber den ganzen erlaubten Wertebereich
        eff = 0
        for wert in range(eintrag["lo"], eintrag["hi"] + 1):
            if conv == "CONV_ADD":
                byte = wert + eintrag["param"]
            elif conv == "CONV_MUL":
                byte = wert * eintrag["param"]
            elif conv == "CONV_MUL_INC":
                byte = (wert + 1) * eintrag["param"]
            elif conv == "CONV_OPMODE":
                byte = OPMODE_BYTES[wert]
            else:
                sys.exit(f"Fehler: unbekannte Umrechnung {conv} bei SET{nr}")
            eff |= (byte & 0xFF) & eintrag["mask"]
        eintrag["eff"] = eff
        ergebnis.append(eintrag)
    return sorted(ergebnis, key=lambda e: e["nr"])


def state_topics():
    """stateTopics[] aus decode.cpp, je Zeile mit der Maske des Dekodierers."""
    zeilen = tabelle_lesen(
        "decode.cpp",
        r"\{\s*(\d+),\s*(\d+),\s*\"([\w/]+)\",\s*(\w+),\s*(\w+),\s*(\w+)\}",
        r"const StateTopic stateTopics\[NUMBEROFTOPICS\] = \{(.*?)\n\};",
    )
    return [dict(nr=int(t.group(1)), pos=int(t.group(2)), name=t.group(3),
                 decode=t.group(4), wide=t.group(5),
                 mask=DECODER_MASK.get(t.group(4), 0xFF))
            for t in zeilen]


def zuordnen(sets, tops):
    """Je Set-Kommando das zurueckmeldende Topic suchen. Liefert Tripel aus
    Kommando, Haupt-Topic (oder None) und den nebenbei getroffenen Topics."""
    paare = []
    for kommando in sets:
        # Kandidat ist jedes Topic am selben Byte, das mindestens ein Bit liest,
        # das dieses Kommando auch schreibt
        kandidaten = [t for t in tops if t["wide"] == "nullptr"
                      and t["pos"] == kommando["pos"]
                      and (t["mask"] & kommando["eff"])]
        # exakte Deckung zuerst, danach die groesste Ueberlappung
        kandidaten.sort(key=lambda t: (t["mask"] == kommando["eff"],
                                       bin(t["mask"] & kommando["eff"]).count("1")),
                        reverse=True)
        paare.append((kommando, kandidaten[0] if kandidaten else None, kandidaten[1:]))
    return paare


def doku_paare(pfad):
    """Die Paare SET -> TOP aus SET-TOP-Zuordnung.md ziehen.

    Gelesen werden die Tabellenzeilen der Abschnitte 1 und 2; ein Gedankenstrich
    in der TOP-Spalte heisst 'keine Rueckmeldung'. Fussnotenzeichen und
    Zusatzspalten stoeren nicht, weil nur die beiden Nummern zaehlen.
    """
    try:
        text = pfad.read_text()
    except OSError as fehler:
        sys.exit(f"Fehler: {pfad.name} nicht lesbar - {fehler}")
    paare = {}
    for zeile in text.splitlines():
        if not zeile.startswith("SET"):
            continue
        spalten = [s.strip() for s in zeile.split("|")]
        treffer = re.match(r"SET(\d+)$", spalten[0])
        if not treffer:
            continue
        top = next((int(m.group(1)) for s in spalten[1:]
                    if (m := re.match(r"TOP(\d+)$", s))), None)
        paare[int(treffer.group(1))] = top
    return paare


def main():
    nur_pruefen = "--pruefen" in sys.argv
    sets, tops = set_kommandos(), state_topics()
    paare = zuordnen(sets, tops)
    getroffen = {haupt["nr"] for _, haupt, _ in paare if haupt}

    if nur_pruefen:
        # berechnete Zuordnung gegen die Doku halten
        soll = {k["nr"]: (h["nr"] if h else None) for k, h, _ in paare}
        ist = doku_paare(Path(__file__).resolve().parent.parent / "SET-TOP-Zuordnung.md")
        abweichungen = []
        for nummer in sorted(set(soll) | set(ist)):
            if soll.get(nummer, "fehlt") != ist.get(nummer, "fehlt"):
                abweichungen.append(
                    f"  SET{nummer}: Code sagt {soll.get(nummer, 'kein Kommando')}, "
                    f"Doku sagt {ist.get(nummer, 'nicht aufgefuehrt')}")
        if abweichungen:
            print("SET-TOP-Zuordnung.md weicht vom Code ab:", file=sys.stderr)
            print("\n".join(abweichungen), file=sys.stderr)
            return 1
        print(f"SET-TOP-Zuordnung.md deckt sich mit dem Code "
              f"({len(soll)} Set-Kommandos geprueft).", file=sys.stderr)

    if not nur_pruefen:
        print("SET | Kommando | Byte | Bits | TOP | State-Topic | Art")
        print(":--- | :--- | ---: | :--- | :--- | :--- | :---")
        for kommando, haupt, neben in paare:
            if not haupt:
                print(f"SET{kommando['nr']} | `{kommando['name']}` | {kommando['pos']} | "
                      f"{bitgruppe(kommando['mask'])} | — | — | **kein Rücklesen**")
                continue
            # voll = das Topic liest genau die Bits, die das Kommando schreibt
            art = "voll" if haupt["mask"] in (kommando["mask"], kommando["eff"]) else "teilweise"
            if neben:
                art += " (+ " + ", ".join(f"TOP{t['nr']}" for t in neben) + ")"
            print(f"SET{kommando['nr']} | `{kommando['name']}` | {kommando['pos']} | "
                  f"{bitgruppe(kommando['mask'])} | TOP{haupt['nr']} | `{haupt['name']}` | {art}")

    # Topics ohne Kommando in drei Gruppen: erreichbare Einstellwerte sind die
    # eigentlichen Luecken, alles ab Byte 110 kann keines haben
    luecken, zustaende, messwerte = [], [], []
    for topic in tops:
        if topic["nr"] in getroffen:
            continue
        if topic["wide"] != "nullptr" or topic["pos"] >= QUERYSIZE:
            (zustaende if topic["mask"] != 0xFF else messwerte).append(topic)
        else:
            luecken.append(topic)

    if not nur_pruefen:
        for titel, gruppe in (("Einstellwerte im Kommandobereich (Byte < 110) - die Luecken", luecken),
                              ("Ist-Zustaende ab Byte 110 - kein Kommando moeglich", zustaende),
                              ("Messwerte und Zaehler - kein Kommando sinnvoll", messwerte)):
            print(f"\n## {titel} - {len(gruppe)}\n")
            print("TOP | State-Topic | Byte | Bits")
            print(":--- | :--- | ---: | :---")
            for topic in gruppe:
                byte = topic["pos"] if topic["wide"] == "nullptr" else "mehrere"
                print(f"TOP{topic['nr']} | `{topic['name']}` | {byte} | {bitgruppe(topic['mask'])}")

    mit_topic = len(getroffen)
    ohne_topic = len(sets) - len([1 for _, h, _ in paare if h])
    print(f"\n{len(sets)} Set-Kommandos, davon {len(sets) - ohne_topic} mit Rueckmeldung "
          f"und {ohne_topic} ohne.", file=sys.stderr)
    print(f"{len(tops)} State-Topics, davon {mit_topic} als Rueckmeldung genutzt, "
          f"{len(luecken)} erreichbare Einstellwerte ohne Kommando, "
          f"{len(zustaende)} Ist-Zustaende, {len(messwerte)} Messwerte.", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
