#!/usr/bin/env python3
"""Dekodierpfad zweier Codestaende gegeneinander laufen lassen - auf dem Mac.

Zweck: belegen, dass ein Umbau an decode.cpp die AUSGABE nicht veraendert.
Beide Staende werden mit Arduino-Ersatzheadern auf dem Host uebersetzt und mit
denselben Telegrammen gefuettert; verglichen werden TOP-Nummer, Topic-Name,
dekodierter Wert und Einheit jedes Topics.

Die Telegramme decken in Phase 1 jeden moeglichen Bytewert (0..255 auf allen
Positionen gleichzeitig) ab - damit trifft der Lauf jeden Zweig jedes
Dekodierers, auch die seltenen (Fehlercodes F/H, Nachkommabits). Phase 2 haengt
Pseudozufall an, um Kombinationen ueber mehrere Bytes zu erwischen.

  ./decode_vergleich.py                      # Arbeitsstand gegen HEAD
  ./decode_vergleich.py --basis v3.2.2       # gegen ein Tag
  ./decode_vergleich.py --entfallen Z2_      # Topics, die neu fehlen DUERFEN

Voraussetzung: g++ (Xcode Command Line Tools). Kein Geraet noetig.
"""
import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
QUELLEN = ["decode.cpp", "decode.h", "Topics.h", "Topics.cpp"]

# Arduino-Ersatz: nur so viel, wie der Dekodierpfad wirklich anfasst
STUBS = {
    "Arduino.h": """#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
typedef uint8_t byte;
static inline unsigned int word(byte h, byte l) { return ((unsigned int)h << 8) | l; }
static inline char *dtostrf(double val, signed char width, unsigned char prec, char *out)
{
  snprintf(out, 32, "%*.*f", (int)width, (int)prec, val);
  return out;
}
static inline unsigned long millis() { return 0; }
""",
    "PubSubClient.h": """#pragma once
class PubSubClient { public: bool publish(const char *, const char *, bool) { return true; } };
""",
    "HeishaMon.h": """#pragma once
#include "Arduino.h"
#include "PubSubClient.h"
#define UPDATEALLTIME 300000
#define MQTT_RETAIN_VALUES true
void write_telnet_log(char *);
void write_mqtt_log(char *);
""",
}

# Testtreiber. Der Zugriff auf Nummer und Name unterscheidet sich zwischen den
# Staenden, deshalb zwei Varianten - der Rest ist identisch.
TREIBER_KOPF = """#include "decode.h"
#include <cstdio>
#include <cstring>
void write_telnet_log(char *) {}
void write_mqtt_log(char *) {}

static void dump(uint8_t *t, int marke)
{
  char out[MAXVALUELEN];
%s
}

int main()
{
  uint8_t t[256];
  // Phase 1: jeder Bytewert einmal auf allen Positionen
  for (int v = 0; v < 256; v++)
  {
    memset(t, v, sizeof(t));
    dump(t, v);
  }
  // Phase 2: Pseudozufall fuer Kombinationen ueber mehrere Bytes
  unsigned int seed = 12345;
  for (int runde = 0; runde < 500; runde++)
  {
    for (int i = 0; i < 256; i++) { seed = seed * 1103515245 + 12345; t[i] = (uint8_t)(seed >> 16); }
    dump(t, 1000 + runde);
  }
  return 0;
}
"""

# Stand MIT Tabelle (struct StateTopic)
SCHLEIFE_TABELLE = """  for (unsigned int i = 0; i < NUMBEROFTOPICS; i++)
  {
    const StateTopic &s = stateTopics[i];
    getTopicPayload(i, t, out);
    printf("%d TOP%u %s %s %s\\n", marke, s.number, s.name, out, s.desc[1]);
  }"""

# Stand mit den alten Parallel-Tabellen (Index == TOP-Nummer)
SCHLEIFE_PARALLEL = """  for (unsigned int n = 0; n < NUMBEROFTOPICS; n++)
  {
    getTopicPayload(n, t, out);
    printf("%d TOP%u %s %s %s\\n", marke, n, topicNames[n], out, topicDescription[n][1]);
  }"""


def hat_tabelle(verzeichnis):
    """Erkennt, welche der beiden Bauformen ein Codestand benutzt."""
    return "struct StateTopic" in (verzeichnis / "decode.h").read_text()


def stand_bauen(ziel, quelle_lesen, name):
    """Einen Codestand samt Stubs und Treiber uebersetzen und ausfuehren."""
    ziel.mkdir(parents=True, exist_ok=True)
    for datei in QUELLEN:
        inhalt = quelle_lesen(datei)
        if inhalt is None:
            sys.exit(f"FEHLER: {datei} im Stand '{name}' nicht gefunden")
        (ziel / datei).write_text(inhalt)
    for datei, inhalt in STUBS.items():
        (ziel / datei).write_text(inhalt)

    schleife = SCHLEIFE_TABELLE if hat_tabelle(ziel) else SCHLEIFE_PARALLEL
    (ziel / "main.cpp").write_text(TREIBER_KOPF % schleife)

    binaer = ziel / "lauf"
    bau = subprocess.run(
        ["g++", "-std=gnu++17", "-I.", "-o", str(binaer),
         "main.cpp", "decode.cpp", "Topics.cpp"],
        cwd=ziel, capture_output=True, text=True)
    if bau.returncode != 0:
        sys.exit(f"FEHLER: Uebersetzen von '{name}' fehlgeschlagen:\n{bau.stderr}")

    lauf = subprocess.run([str(binaer)], capture_output=True, text=True)
    if lauf.returncode != 0:
        sys.exit(f"FEHLER: Lauf von '{name}' fehlgeschlagen (Exit {lauf.returncode})")
    return lauf.stdout.splitlines()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--basis", default="HEAD",
                    help="Git-Revision als Vergleichsstand (Vorgabe: HEAD)")
    ap.add_argument("--entfallen", action="append", default=[],
                    help="Namensteil von Topics, die im neuen Stand fehlen duerfen "
                         "(mehrfach angebbar, z. B. --entfallen Z2_)")
    args = ap.parse_args()

    if shutil.which("g++") is None:
        sys.exit("FEHLER: g++ nicht gefunden - Xcode Command Line Tools installieren")

    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)

        # Basisstand aus git holen
        def aus_git(datei):
            r = subprocess.run(["git", "show", f"{args.basis}:src/{datei}"],
                               cwd=REPO, capture_output=True, text=True)
            return r.stdout if r.returncode == 0 else None

        # Arbeitsstand aus dem Arbeitsverzeichnis
        def aus_arbeitsstand(datei):
            p = REPO / "src" / datei
            return p.read_text() if p.exists() else None

        print(f"== Basis '{args.basis}' uebersetzen und laufen lassen ==")
        alt = stand_bauen(tmp / "basis", aus_git, args.basis)
        print(f"== Arbeitsstand uebersetzen und laufen lassen ==")
        neu = stand_bauen(tmp / "arbeit", aus_arbeitsstand, "Arbeitsstand")

    # Zeilen, deren Topic-Name einen der --entfallen-Teile enthaelt, aus dem
    # Basislauf herausnehmen: sie duerfen im neuen Stand fehlen
    if args.entfallen:
        vorher = len(alt)
        alt = [z for z in alt
               if not any(teil in z.split(" ")[2] for teil in args.entfallen)]
        print(f"   {vorher - len(alt)} Zeilen als erwartet entfallen ausgeblendet "
              f"({', '.join(args.entfallen)})")

    print(f"\n== Vergleich: {len(alt)} gegen {len(neu)} Zeilen ==")
    if alt == neu:
        anzahl = len({z.split(" ")[1] for z in neu})
        print(f"IDENTISCH - {anzahl} Topics, Nummer, Name, Wert und Einheit gleich")
        return 0

    # Erste Abweichungen zeigen, damit der Befund einordenbar ist
    print("ABWEICHUNGEN:")
    gezeigt = 0
    for i, (a, n) in enumerate(zip(alt, neu)):
        if a != n:
            print(f"  Zeile {i + 1}:\n    Basis  : {a}\n    Arbeit : {n}")
            gezeigt += 1
            if gezeigt >= 10:
                break
    if len(alt) != len(neu):
        print(f"  Zeilenzahl unterschiedlich: {len(alt)} gegen {len(neu)}")
    return 1


if __name__ == "__main__":
    sys.exit(main())
