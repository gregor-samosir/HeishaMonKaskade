#!/usr/bin/env python3
"""Prueft die Tabellen und Verweise, die das Repo ueber sich selbst fuehrt.

Warum es diesen Test gibt
-------------------------
Das Projekt wird vollstaendig von Claude bearbeitet, Session fuer Session.
Was nicht durch eine Pruefung erzwungen wird, haengt davon ab, dass jemand
daran denkt - und genau das ging in Kleinigkeiten immer wieder schief. Die
Durchsicht vom 2026-09-18 fand, ohne danach gesucht zu haben:

  * zehn Markdown-Dateien und zwei Header, die in der README-Tabelle "Aufbau"
    fehlten - obwohl CLAUDE.md sie als vollstaendig ausweist,
  * css_klassen_test.py fehlte in der Werkzeugtabelle von test/README.md,
  * ein Link auf ein Skript, das seit 3.16.0 geloescht ist.

Keiner dieser Fehler richtet Schaden an. Aber jeder kostet die naechste
Session Zeit, und eine Tabelle, die einmal unvollstaendig ist, verliert ihren
Zweck: Man muss dann doch wieder das Verzeichnis durchsuchen.

Was geprueft wird
-----------------
  1. README.md, "Aufbau": jede Markdown-Datei im Wurzelverzeichnis und jede
     Datei in src/ steht in der ersten Spalte (ein Header gilt ueber seine
     .cpp als eingetragen und umgekehrt), und jeder Eintrag existiert.
  2. test/README.md, "Werkzeuge": jedes Skript und jeder Test in test/ steht
     in der Tabelle, und jeder Eintrag existiert.
  3. MQTT-Topics.md gegen den Code: jedes State-Topic mit Nummer und Name,
     jedes Set-Kommando mit Nummer, Name, Byte und Wertebereich - in beide
     Richtungen.
  4. CLAUDE.md: jeder Pfad in `...` existiert. Ein blosser Dateiname ohne '/'
     gilt auch, wenn er in src/ oder test/ liegt (CLAUDE.md nennt die Header
     so: `notbetrieb.h`).
  5. Jeder relative Link in jeder Markdown-Datei zeigt auf eine vorhandene
     Datei (Anker hinter '#' werden nicht geprueft).

Dateien, die git ignoriert (platformio_user_env.ini, doku-intern/), zaehlen als
vorhanden: In der CI fehlen sie, lokal sind sie da, und beides ist richtig.

Nur Standardbibliothek und git, kein Geraet noetig. Neu am 2026-09-18.
"""
import re
import subprocess
import sys
from pathlib import Path

HIER = Path(__file__).resolve().parent
REPO = HIER.parent
sys.path.insert(0, str(HIER))
import set_top_zuordnung as stz  # noqa: E402 - Parser fuer setCommands[]/stateTopics[]

# Stehen bewusst nicht in "Aufbau" - mit Grund, damit die Liste nicht still waechst
AUFBAU_AUSNAHMEN = {
    "README.md": "ist die Datei, die die Tabelle enthaelt",
    "CLAUDE.md": "Arbeitsanweisung fuer Claude, verweist selbst auf README und Aufbau",
}

# Endungen, die in test/ als Werkzeug oder Test gelten
WERKZEUG_ENDUNGEN = {".py", ".cpp", ".sh"}


class Ergebnis:
    """Sammelt Befunde je Pruefung, damit ein Lauf ALLE Fehler zeigt."""

    def __init__(self):
        self.fehler = 0

    def pruefung(self, titel, befunde, ok_text):
        if befunde:
            self.fehler += len(befunde)
            print(f"  [FEHLER] {titel}:")
            for b in befunde:
                print(f"           {b}")
        else:
            print(f"  [ok ] {ok_text}")


def git(*argumente):
    """git im Repo aufrufen; ohne git ist der Test nicht aussagekraeftig."""
    try:
        lauf = subprocess.run(["git", *argumente], cwd=REPO, capture_output=True, text=True, check=False)
    except OSError as fehler:
        sys.exit(f"Fehler: git nicht aufrufbar - {fehler}")
    return lauf


def versioniert():
    """Alle Dateien, die git fuehrt - lokale Reste (Logs, Build) zaehlen nicht."""
    lauf = git("ls-files")
    if lauf.returncode != 0:
        sys.exit(f"Fehler: git ls-files - {lauf.stderr.strip()}")
    return lauf.stdout.split()


def vorhanden(pfad):
    """Existiert lokal oder wird von git ignoriert (fehlt dann in der CI).

    Ein Verzeichnismuster wie 'doku-intern/' in .gitignore trifft einen Pfad,
    den es nicht gibt, nur MIT Schraegstrich - git kann nicht wissen, dass es
    ein Verzeichnis waere. Deshalb beide Schreibweisen. Aufgefallen in einem
    frischen Klon am 2026-09-18: lokal existierte der Ordner, die CI waere rot
    geworden."""
    if (REPO / pfad).exists():
        return True
    pfad = str(pfad).rstrip("/")
    return any(git("check-ignore", "-q", p).returncode == 0 for p in (pfad, pfad + "/"))


def abschnitt(text, ueberschrift, datei):
    """Den Text von '## ueberschrift' bis zur naechsten '## '-Ueberschrift."""
    anfang = text.find(f"\n## {ueberschrift}")
    if anfang < 0:
        sys.exit(f"Fehler: Abschnitt '## {ueberschrift}' in {datei} nicht gefunden - Aufbau geaendert?")
    ende = text.find("\n## ", anfang + 1)
    return text[anfang:ende if ende > 0 else len(text)]


def erste_spalte(tabelle):
    """Die Eintraege der ersten Spalte: '| [`X`](...)' oder '| `X`'."""
    return set(re.findall(r"^\| \[?`([^`]+)`", tabelle, re.M))


def ohne_codebloecke(text):
    """Codebloecke entfernen - Beispielbefehle darin sind keine Verweise."""
    return re.sub(r"```.*?```", "", text, flags=re.S)


def wertebereich(text):
    """'-5 to 65' / '40 - 75' -> (-5, 65); '0=off, 1=on' / '0, 1, 2 or 3' -> (0, 1) bzw. (0, 3).
    None, wenn die Spalte keinem der beiden Muster folgt."""
    text = text.strip()
    bereich = re.fullmatch(r"(-?\d+)\s*(?:to|-)\s*(-?\d+)", text)
    if bereich:
        return int(bereich.group(1)), int(bereich.group(2))
    werte = []
    for eintrag in re.split(r",|\bor\b", text):
        zahl = re.match(r"\s*(-?\d+)\s*(?:=|$)", eintrag)
        if not zahl:
            return None
        werte.append(int(zahl.group(1)))
    return (min(werte), max(werte)) if werte else None


def main():
    erg = Ergebnis()
    dateien = versioniert()

    # --- 1. README.md, Aufbau ------------------------------------------------
    befunde = []
    aufbau = abschnitt((REPO / "README.md").read_text(), "Aufbau", "README.md")
    gelistet = erste_spalte(aufbau)
    wurzel_md = sorted(d for d in dateien if "/" not in d and d.endswith(".md"))
    for datei in wurzel_md:
        if datei not in gelistet and datei not in AUFBAU_AUSNAHMEN:
            befunde.append(f"{datei} fehlt in der Tabelle")
    for datei in sorted(d for d in dateien if d.startswith("src/") and d.count("/") == 1):
        partner = str(Path(datei).with_suffix(".h" if datei.endswith(".cpp") else ".cpp"))
        if datei not in gelistet and partner not in gelistet:
            befunde.append(f"{datei} fehlt in der Tabelle (auch nicht ueber {Path(partner).name})")
    for eintrag in sorted(gelistet):
        if not vorhanden(eintrag.rstrip("/")):
            befunde.append(f"{eintrag} steht in der Tabelle, existiert aber nicht")
    for datei in sorted(AUFBAU_AUSNAHMEN):
        if datei in gelistet:
            befunde.append(f"{datei} steht in der Tabelle UND in AUFBAU_AUSNAHMEN - Liste pflegen")
    erg.pruefung("README.md, Aufbau", befunde,
                 f"Aufbau vollstaendig ({len(wurzel_md)} Markdown-Dateien, src/), alle Eintraege vorhanden")

    # --- 2. test/README.md, Werkzeuge ----------------------------------------
    befunde = []
    werkzeuge = abschnitt((REPO / "test" / "README.md").read_text(), "Werkzeuge", "test/README.md")
    gelistet = erste_spalte(werkzeuge)
    im_test = set()
    for datei in dateien:
        teile = Path(datei).parts
        if len(teile) == 2 and teile[0] == "test" and Path(datei).suffix in WERKZEUG_ENDUNGEN:
            im_test.add(teile[1])
        elif len(teile) > 2 and teile[0] == "test":
            im_test.add(teile[1] + "/")  # Unterverzeichnis wie stubs/ als Ganzes
    for name in sorted(im_test - gelistet):
        befunde.append(f"test/{name} fehlt in der Tabelle")
    for name in sorted(gelistet):
        if not vorhanden(Path("test") / name.rstrip("/")):
            befunde.append(f"{name} steht in der Tabelle, existiert in test/ aber nicht")
    erg.pruefung("test/README.md, Werkzeuge", befunde,
                 f"Werkzeugtabelle vollstaendig ({len(im_test)} Eintraege in test/), alle vorhanden")

    # --- 3. MQTT-Topics.md gegen den Code ------------------------------------
    befunde = []
    topics = (REPO / "MQTT-Topics.md").read_text()
    code_tops = {t["nr"]: t["name"] for t in stz.state_topics()}
    doku_tops = {}
    for nr, name in re.findall(r"^TOP(\d+) \| (\w+) \|", topics, re.M):
        if int(nr) in doku_tops:
            befunde.append(f"TOP{nr} steht doppelt")
        doku_tops[int(nr)] = name
    for nr in sorted(set(code_tops) | set(doku_tops)):
        if nr not in doku_tops:
            befunde.append(f"TOP{nr} `{code_tops[nr]}` fehlt")
        elif nr not in code_tops:
            befunde.append(f"TOP{nr} `{doku_tops[nr]}` gibt es im Code nicht")
        elif doku_tops[nr] != code_tops[nr]:
            befunde.append(f"TOP{nr}: Doku `{doku_tops[nr]}`, Code `{code_tops[nr]}`")
    code_sets = {k["nr"]: k for k in stz.set_kommandos()}
    doku_sets = {}
    for nr, name, byte, bereich in re.findall(
            r"^SET(\d+)\s*\| (\w+) \| (\d+) \| [^|\n]* \| ([^|\n]+)$", topics, re.M):
        if int(nr) in doku_sets:
            befunde.append(f"SET{nr} steht doppelt")
        doku_sets[int(nr)] = (name, int(byte), bereich)
    for nr in sorted(set(code_sets) | set(doku_sets)):
        if nr not in doku_sets:
            befunde.append(f"SET{nr} `{code_sets[nr]['name']}` fehlt")
            continue
        if nr not in code_sets:
            befunde.append(f"SET{nr} `{doku_sets[nr][0]}` gibt es im Code nicht")
            continue
        name, byte, bereich = doku_sets[nr]
        k = code_sets[nr]
        if name != k["name"]:
            befunde.append(f"SET{nr}: Doku `{name}`, Code `{k['name']}`")
        if byte != k["pos"]:
            befunde.append(f"SET{nr}: Doku Byte {byte}, Code Byte {k['pos']}")
        grenzen = wertebereich(bereich)
        if grenzen is None:
            befunde.append(f"SET{nr}: Wertebereich '{bereich.strip()}' nicht lesbar "
                           f"('a to b', 'a - b' oder '0=..., 1=...')")
        elif grenzen != (k["lo"], k["hi"]):
            befunde.append(f"SET{nr}: Doku {grenzen[0]}..{grenzen[1]}, Code {k['lo']}..{k['hi']}")
    erg.pruefung("MQTT-Topics.md", befunde,
                 f"{len(code_tops)} State-Topics und {len(code_sets)} Set-Kommandos wie im Code "
                 f"(Nummer, Name, Byte, Wertebereich)")

    # --- 4. CLAUDE.md: Pfade in `...` ----------------------------------------
    befunde = []
    claude = ohne_codebloecke((REPO / "CLAUDE.md").read_text())
    geprueft = 0
    for pfad in sorted(set(re.findall(r"`([^`\s]+)`", claude))):
        # Pfadartig: enthaelt '/' oder hat eine Dateiendung; ohne Platzhalter,
        # nicht ausserhalb des Repos (~) und keine Optionen (-e ...)
        if "*" in pfad or pfad.startswith(("~", "-", "/")):
            continue
        if not ("/" in pfad or re.search(r"\.(md|py|h|cpp|sh|ini|yml|json)$", pfad)):
            continue
        geprueft += 1
        kandidaten = [pfad] if "/" in pfad else [pfad, f"src/{pfad}", f"test/{pfad}"]
        if not any(vorhanden(k.rstrip("/")) for k in kandidaten):
            befunde.append(f"`{pfad}` existiert nicht")
    erg.pruefung("CLAUDE.md", befunde, f"alle {geprueft} Pfadangaben in CLAUDE.md vorhanden")

    # --- 5. Relative Links in allen Markdown-Dateien --------------------------
    befunde = []
    links = 0
    for datei in sorted(d for d in dateien if d.endswith(".md")):
        text = ohne_codebloecke((REPO / datei).read_text())
        for ziel in re.findall(r"\]\(([^)\s]+)\)", text):
            if re.match(r"[a-z]+:", ziel) or ziel.startswith("#"):
                continue  # http(s), mailto, reine Anker
            pfad = ziel.split("#")[0]
            links += 1
            if not vorhanden(Path(datei).parent / pfad):
                befunde.append(f"{datei}: Link auf {ziel} fuehrt ins Leere")
    erg.pruefung("Links", befunde, f"alle {links} relativen Links in den Markdown-Dateien fuehren zu einer Datei")

    print()
    if erg.fehler:
        print(f"FEHLGESCHLAGEN ({erg.fehler} Fehler)")
        return 1
    print("ALLE PRUEFUNGEN BESTANDEN (0 Fehler)")
    return 0


if __name__ == "__main__":
    print("\n== Tabellen und Verweise des Repos ==")
    sys.exit(main())
