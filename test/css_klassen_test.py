#!/usr/bin/env python3
"""Prueft, dass jede benutzte w3-Klasse im eingebetteten CSS auch definiert ist.

Warum es diesen Test gibt
-------------------------
Seit dem Ausbau der CDN-Einbindung bringt die Firmware ihr CSS selbst mit
(`webHeader` in src/webfunctions.cpp). Darin steht aber nur, was die damaligen
Seiten brauchten. Wer eine neue Seite baut und eine W3.CSS-Klasse benutzt, die
es dort nicht gibt, bekommt keinen Fehler - die Seite laedt, sie sieht nur
anders aus als gedacht.

Genau das ist am 2026-08-20 passiert: Der Notbetriebsknopf war als
`w3-button w3-red w3-xlarge w3-padding-large` gebaut und erschien am Geraet als
unauffaelliger grauer Text. `w3-red`, `w3-xlarge`, `w3-padding-large` und
`w3-panel` fehlten im CSS; die Statusmeldungen ROT und "Laeuft..." waren
dadurch farblos, GRUEN als einzige gruen - fuer eine Notfallseite die
schlechteste denkbare Asymmetrie.

Zweite Regel: REIHENFOLGE
-------------------------
`.w3-button` setzt `background:inherit`. Steht eine Farbklasse davor, gewinnt
bei gleicher Spezifitaet die spaetere Regel - der Knopf bleibt grau, obwohl
beide Klassen definiert sind. Genau daran war auch der "Save and reboot"-Knopf
der Settings-Seite grau statt gruen, lange bevor es den Notbetrieb gab.

Kein Geraet noetig, nur Standardbibliothek.

Stand 3.12.0 (neu in dieser Fassung).
"""
import re
import sys
from pathlib import Path

QUELLE = Path(__file__).resolve().parent.parent / "src" / "webfunctions.cpp"

# Bewusst nicht definiert: reine Einblendanimation der Sidebar. Ohne sie
# erscheint das Menue sofort statt zu gleiten - die Bedienung ist vollstaendig.
AUSNAHMEN = {"w3-animate-left"}

# Klassen, die eine Hintergrundfarbe setzen und deshalb NACH .w3-button stehen
# muessen (siehe Kopfkommentar).
FARBKLASSEN = {"w3-blue", "w3-green", "w3-red", "w3-orange", "w3-yellow", "w3-theme"}

KLASSE = re.compile(r"w3-[a-z0-9-]+")


def css_block(text: str) -> str:
    """Den eingebetteten <style>-Block herausschneiden."""
    anfang = text.find("<style>")
    ende = text.find("</style>", anfang)
    if anfang < 0 or ende < 0:
        sys.exit("FEHLER: kein <style>-Block in webfunctions.cpp gefunden")
    return text[anfang:ende]


def code_ohne_kommentare(text: str) -> str:
    """Zeilenkommentare entfernen - dort stehen Klassennamen als Prosa."""
    zeilen = []
    for zeile in text.splitlines():
        gestutzt = zeile.strip()
        if gestutzt.startswith("//"):
            continue
        zeilen.append(zeile)
    return "\n".join(zeilen)


def main() -> int:
    text = QUELLE.read_text(encoding="utf-8")
    stil = css_block(text)

    # Definiert ist, was im CSS als Selektor auftaucht (".w3-xyz")
    definiert = {t.lstrip(".") for t in re.findall(r"\.w3-[a-z0-9-]+", stil)}

    # Benutzt ist alles ausserhalb des CSS-Blocks, ohne Kommentarzeilen
    rest = code_ohne_kommentare(text.replace(stil, ""))
    benutzt = set(KLASSE.findall(rest))

    fehler = 0

    fehlend = sorted(benutzt - definiert - AUSNAHMEN)
    if fehlend:
        fehler += len(fehlend)
        print("  [FEHLER] benutzt, aber im CSS nicht definiert:")
        for k in fehlend:
            print(f"           {k}")
    else:
        print(f"  [ok ] alle {len(benutzt)} benutzten Klassen sind definiert")

    # Reihenfolge: jede definierte Farbklasse muss hinter .w3-button stehen
    pos_button = stil.find(".w3-button")
    if pos_button < 0:
        print("  [FEHLER] .w3-button ist gar nicht definiert")
        fehler += 1
    else:
        zu_frueh = []
        for k in sorted(FARBKLASSEN & definiert):
            if stil.find("." + k) < pos_button:
                zu_frueh.append(k)
        if zu_frueh:
            fehler += len(zu_frueh)
            print("  [FEHLER] Farbklasse steht VOR .w3-button und wird von")
            print("           dessen background:inherit ueberschrieben:")
            for k in zu_frueh:
                print(f"           {k}")
        else:
            print(f"  [ok ] alle {len(FARBKLASSEN & definiert)} Farbklassen stehen hinter .w3-button")

    # Gegenprobe: die Ausnahmeliste darf nicht stillschweigend veralten
    unnoetig = sorted(AUSNAHMEN & definiert)
    if unnoetig:
        fehler += len(unnoetig)
        print("  [FEHLER] steht in AUSNAHMEN, ist aber definiert - Liste pflegen:")
        for k in unnoetig:
            print(f"           {k}")
    else:
        print(f"  [ok ] Ausnahmeliste ({len(AUSNAHMEN)}) ist aktuell")

    print()
    if fehler:
        print(f"FEHLGESCHLAGEN ({fehler} Fehler)")
        return 1
    print("ALLE PRUEFUNGEN BESTANDEN (0 Fehler)")
    return 0


if __name__ == "__main__":
    print("\n== CSS-Klassen der Weboberflaeche ==")
    sys.exit(main())
