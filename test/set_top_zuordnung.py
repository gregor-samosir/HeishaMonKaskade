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

Seit 2026-09-18 vergleicht er ausserdem die Topic-LISTEN in Abschnitt 3a-3c
und die Zahlen in den Ueberschriften und im Einleitungssatz. Anlass: Die sieben
Installer-Topics TOP105-111 aus 3.19.0 fehlten dort drei Wochen lang, und der
Paarvergleich blieb gruen - ein Topic ohne Set-Kommando bildet kein Paar.
Laeuft in den Hosttests ueber doku_zuordnung_test.py.

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


def gruppieren(tops, getroffen):
    """Topics ohne Kommando in drei Gruppen: erreichbare Einstellwerte sind die
    eigentlichen Luecken, alles ab Byte 110 (oder aus mehreren Bytes) kann
    keines haben - Bitfelder sind Ist-Zustaende, ganze Bytes Messwerte."""
    luecken, zustaende, messwerte = [], [], []
    for topic in tops:
        if topic["nr"] in getroffen:
            continue
        if topic["wide"] != "nullptr" or topic["pos"] >= QUERYSIZE:
            (zustaende if topic["mask"] != 0xFF else messwerte).append(topic)
        else:
            luecken.append(topic)
    return luecken, zustaende, messwerte


def doku_abschnitte(pfad):
    """Topic-Listen der Abschnitte 3a-3c und die Zahlen im Text lesen.

    3a und 3b: Tabellenzeilen, die mit 'TOPn |' beginnen - TOP-Nummern im
    Fliesstext (etwa die Heizstab-Topics, die frueher in 3a standen) zaehlen
    nicht. 3c: der Absatz, der mit 'TOPn,' beginnt. Zahlen: die Klammer am Ende
    der Ueberschriften 1, 2, 3, 3a-3c und der Einleitungssatz. Fehlt etwas
    davon, hat sich der Aufbau geaendert - dann lieber abbrechen als gruen.
    """
    try:
        text = pfad.read_text()
    except OSError as fehler:
        sys.exit(f"Fehler: {pfad.name} nicht lesbar - {fehler}")
    listen, zahlen = {}, {}
    for kennung, naechste in (("3a", "\n### 3b."), ("3b", "\n### 3c."), ("3c", "\n## 4.")):
        anfang = text.find(f"\n### {kennung}.")
        ende = text.find(naechste, anfang + 1)
        if anfang < 0 or ende < 0:
            sys.exit(f"Fehler: Abschnitt {kennung} oder die Ueberschrift danach "
                     f"({naechste.strip()}) in {pfad.name} nicht gefunden - Aufbau geaendert?")
        teil = text[anfang + 1:ende]
        if kennung == "3c":
            absatz = re.search(r"^TOP\d+,.*?(?=\n\n)", teil, re.S | re.M)
            listen[kennung] = [int(n) for n in re.findall(r"TOP(\d+)", absatz.group(0))] if absatz else []
        else:
            listen[kennung] = [int(n) for n in re.findall(r"^TOP(\d+) \|", teil, re.M)]
        zahl = re.match(r"### [^\n]*\((\d+)\)\s*\n", teil)
        zahlen[kennung] = int(zahl.group(1)) if zahl else None
    for kennung, muster in (("1", r"^## 1\. [^\n]*\((\d+) von (\d+)\)$"),
                            ("2", r"^## 2\. [^\n]*\((\d+)\)$"),
                            ("3", r"^## 3\. [^\n]*\((\d+)\)$"),
                            ("Einleitung", r"alle (\d+) Set-Kommandos und alle\s+(\d+) State-Topics"),
                            ("English", r"all (\d+)\s+set commands and all\s+(\d+)\s+state topics")):
        treffer = re.search(muster, text, re.M)
        zahlen[kennung] = tuple(int(g) for g in treffer.groups()) if treffer else None
    return listen, zahlen


def abschnitte_pruefen(pfad, sets, tops, paare, gruppen):
    """Listen und Zahlen der Doku gegen den Code halten; liefert Abweichungen."""
    listen, zahlen = doku_abschnitte(pfad)
    abweichungen = []
    for kennung, gruppe in zip(("3a", "3b", "3c"), gruppen):
        soll = sorted(t["nr"] for t in gruppe)
        ist = sorted(listen[kennung])
        doppelt = sorted({n for n in ist if ist.count(n) > 1})
        fehlt, zuviel = sorted(set(soll) - set(ist)), sorted(set(ist) - set(soll))
        if fehlt:
            abweichungen.append(f"  Abschnitt {kennung}: es fehlen " + ", ".join(f"TOP{n}" for n in fehlt))
        if zuviel:
            abweichungen.append(f"  Abschnitt {kennung}: gehoert nicht hierher " + ", ".join(f"TOP{n}" for n in zuviel))
        if doppelt:
            abweichungen.append(f"  Abschnitt {kennung}: doppelt " + ", ".join(f"TOP{n}" for n in doppelt))
        if zahlen[kennung] != len(soll):
            abweichungen.append(f"  Ueberschrift {kennung}: nennt {zahlen[kennung]}, der Code hat {len(soll)}")
    mit = len([1 for _, h, _ in paare if h])
    ohne_kommando = sum(len(g) for g in gruppen)
    for kennung, soll in (("1", (mit, len(sets))), ("2", (len(sets) - mit,)), ("3", (ohne_kommando,)),
                          ("Einleitung", (len(sets), len(tops))), ("English", (len(sets), len(tops)))):
        if zahlen[kennung] != soll:
            abweichungen.append(f"  {'Ueberschrift ' if kennung.isdigit() else ''}{kennung}: "
                                f"nennt {_zahl(zahlen[kennung])}, der Code ergibt {_zahl(soll)}")
    return abweichungen


def _zahl(werte):
    """(35, 37) -> '35/37', None -> 'nichts' - fuer lesbare Abweichungen."""
    return "nichts" if werte is None else "/".join(str(w) for w in werte)


def main():
    nur_pruefen = "--pruefen" in sys.argv
    sets, tops = set_kommandos(), state_topics()
    paare = zuordnen(sets, tops)
    getroffen = {haupt["nr"] for _, haupt, _ in paare if haupt}
    luecken, zustaende, messwerte = gruppieren(tops, getroffen)

    if nur_pruefen:
        # berechnete Zuordnung gegen die Doku halten: erst die Paare, dann die
        # Topic-Listen in Abschnitt 3 und die Zahlen im Text
        doku = Path(__file__).resolve().parent.parent / "SET-TOP-Zuordnung.md"
        soll = {k["nr"]: (h["nr"] if h else None) for k, h, _ in paare}
        ist = doku_paare(doku)
        abweichungen = []
        for nummer in sorted(set(soll) | set(ist)):
            if soll.get(nummer, "fehlt") != ist.get(nummer, "fehlt"):
                abweichungen.append(
                    f"  SET{nummer}: Code sagt {soll.get(nummer, 'kein Kommando')}, "
                    f"Doku sagt {ist.get(nummer, 'nicht aufgefuehrt')}")
        abweichungen += abschnitte_pruefen(doku, sets, tops, paare, (luecken, zustaende, messwerte))
        if abweichungen:
            print("SET-TOP-Zuordnung.md weicht vom Code ab:", file=sys.stderr)
            print("\n".join(abweichungen), file=sys.stderr)
            return 1
        print(f"SET-TOP-Zuordnung.md deckt sich mit dem Code "
              f"({len(soll)} Set-Kommandos, {len(tops)} State-Topics geprueft).", file=sys.stderr)

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
