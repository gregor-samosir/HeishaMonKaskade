#!/usr/bin/env python3
"""Meldet vorzeichenbehaftete und direkte millis()-Vergleiche in src/.

Warum es diesen Test gibt
-------------------------
Das Karenzfenster nach dem Verbinden pruefte bis 3.23.0 so:

    if ((long)(millis() - setCommandsIgnoredUntil) < 0)

Das gilt allgemein als ueberlaufsicher und ist es nur fuer Fristen, die nach
Ablauf nicht weiter verglichen werden. Diese wurde weiter verglichen: Nach
2^31 ms = 24,86 Tagen ohne Reconnect kippt die Differenz wieder ins Negative,
die Regel meldet "offen", und jedes Set-Kommando wird still verworfen. Zwei
Codedurchsichten haben die Stelle als "ueberlaufsicher geprueft" abgehakt,
weil das Idiom bekannt aussieht (Befund S1 der Durchsicht vom 2026-10-15).

Was geprueft wird
-----------------
In src/*.cpp und src/*.h (Code, nicht Kommentare) darf nicht stehen:

  * ein Cast auf long / int / int32_t vor einem Ausdruck mit millis()
  * ein direkter Vergleich millis() < x, millis() > x, x < millis(), x > millis()
    (auch <= und >=) - der laeuft beim Ueberlauf nach 49,7 Tagen falsch

Ausnahme: Eine Zeile mit der Marke `kurzlebige Frist` in einem Zeilenkommentar.
Sie sagt: Die Frist lebt nur kurz (hoechstens Sekunden) und wird nach dem
Ende der Schleife nie wieder geprueft - die Naht ist nicht erreichbar. Wer
die Marke setzt, schreibt darueber, warum (Beispiel: src/notbetrieb.cpp).

Alles, was Zeit ueber lange Spannen vergleicht, gehoert in einen arduino-freien
Header mit uint32_t und eigenem Hosttest (src/karenz.h, src/sendwindow.h).

Kein Geraet noetig, nur Standardbibliothek.

Stand 3.23.1 (neu).
"""
import re
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parent.parent / "src"
MARKE = "kurzlebige Frist"

# Cast auf eine vorzeichenbehaftete Breite, danach irgendwo in derselben Zeile
# ein millis(). Deckt (long)(millis() - x) ebenso ab wie (int32_t)(x - millis()).
CAST = re.compile(r"\(\s*(?:long|int|int32_t|signed)\s*\)\s*\(.*millis\s*\(\s*\)")

# Direkter Vergleich. Die Rueckschau haelt "->", "<<" und ">>" heraus, die
# Vorschau haelt "<<" und ">>" heraus (Schiebeoperator ist kein Vergleich).
VERGLEICH_RECHTS = re.compile(r"millis\s*\(\s*\)\s*(?<![-<>])[<>]=?(?![<>])")
VERGLEICH_LINKS = re.compile(r"(?<![-<>])[<>]=?(?![<>])\s*millis\s*\(\s*\)")


def ohne_blockkommentare(text: str) -> str:
    """/* ... */ entfernen, Zeilenumbrueche erhalten (Zeilennummern stimmen)."""
    return re.sub(r"/\*.*?\*/",
                  lambda m: "\n" * m.group(0).count("\n"),
                  text, flags=re.S)


def code_und_kommentar(zeile: str):
    """Zeile am ersten // trennen. String-Literale mit // gibt es in src/ an
    den betroffenen Stellen nicht; im Zweifel meldet der Test lieber zu viel."""
    i = zeile.find("//")
    if i < 0:
        return zeile, ""
    return zeile[:i], zeile[i:]


def befunde_in(text: str):
    """Liefert (zeilennummer, zeile, grund) fuer jede unerlaubte Zeile."""
    ergebnis = []
    for nr, zeile in enumerate(ohne_blockkommentare(text).splitlines(), start=1):
        code, kommentar = code_und_kommentar(zeile)
        if "millis" not in code:
            continue
        grund = None
        if CAST.search(code):
            grund = "vorzeichenbehafteter Cast auf millis()-Differenz"
        elif VERGLEICH_RECHTS.search(code) or VERGLEICH_LINKS.search(code):
            grund = "direkter Vergleich mit millis()"
        if grund and MARKE not in kommentar:
            ergebnis.append((nr, zeile.strip(), grund))
    return ergebnis


def selbsttest() -> int:
    """Die Muster gegen Beispielzeilen - ohne das waere ein stummer Regex
    (trifft nichts) von einem sauberen Quelltext nicht zu unterscheiden."""
    muss_melden = [
        "if ((long)(millis() - frist) < 0)",
        "if ((int32_t)(frist - millis()) > 0)",
        "while ((int)(millis() - t0) < 100)",
        "if (millis() < naechster)",
        "if (millis() >= frist)",
        "if (frist > millis())",
        "if (frist <= millis())",
        "if ((long)(millis() - x) < 0) // irgendein Kommentar",
    ]
    darf_nicht_melden = [
        "if ((uint32_t)(millis() - start) >= dauer)",
        "if ((millis() - nextInfoTime) > UPDATEALLTIME)",
        "const uint32_t frist = millis() + 5000;",
        "x = (long)(wert * 2);",
        "// if ((long)(millis() - frist) < 0) steht hier nur als Prosa",
        "if ((long)(millis() - frist) < 0) // kurzlebige Frist: endet mit der Schleife",
        "p->t = millis(); // < kein Vergleich",
        "v = millis() >> 3;",
    ]
    fehler = 0
    for z in muss_melden:
        if not befunde_in(z):
            print(f"  SELBSTTEST FEHLER: haette melden muessen: {z}")
            fehler += 1
    for z in darf_nicht_melden:
        if befunde_in(z):
            print(f"  SELBSTTEST FEHLER: haette schweigen muessen: {z}")
            fehler += 1
    # Blockkommentar ueber mehrere Zeilen: Zeilennummer bleibt, Inhalt zaehlt nicht
    mehrzeilig = "/* (long)(millis() - x) < 0\n   noch Prosa */\nint a;\nif (millis() < b)\n"
    nr = [n for n, _, _ in befunde_in(mehrzeilig)]
    if nr != [4]:
        print(f"  SELBSTTEST FEHLER: Blockkommentar/Zeilennummer, erwartet [4], ist {nr}")
        fehler += 1
    return fehler


def main() -> int:
    print("Zeitvergleiche in src/ (vorzeichenbehaftet oder direkt)")
    fehler = selbsttest()
    if fehler:
        print(f"SELBSTTEST: {fehler} Fehler")
        return 1
    print("  Selbsttest der Muster: ok")

    dateien = sorted(list(SRC.glob("*.cpp")) + list(SRC.glob("*.h")))
    if not dateien:
        print("FEHLER: keine Quelldateien unter src/ gefunden")
        return 1

    gefunden = 0
    markiert = 0
    for datei in dateien:
        text = datei.read_text(encoding="utf-8", errors="replace")
        for nr, zeile, grund in befunde_in(text):
            print(f"  FEHLER {datei.relative_to(SRC.parent)}:{nr}: {grund}\n"
                  f"         {zeile}")
            gefunden += 1
        markiert += sum(1 for z in text.splitlines()
                        if MARKE in z and "millis" in z)

    print(f"  {len(dateien)} Dateien geprueft, {markiert} Zeile(n) mit Marke "
          f"'{MARKE}'")
    if gefunden:
        print(f"ROT: {gefunden} Fundstelle(n). Entweder in einen arduino-freien "
              "Header mit uint32_t und Hosttest ziehen (src/karenz.h) oder, "
              "wenn die Frist nur kurz lebt, die Marke samt Begruendung setzen.")
        return 1
    print("alle Zusicherungen bestanden")
    return 0


if __name__ == "__main__":
    sys.exit(main())
