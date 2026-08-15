#!/bin/sh
# Hosttests gegen den echten Dekodierpfad uebersetzen und ausfuehren.
#
# Warum ein Skript und kein einzelner c++-Aufruf: decode.cpp beginnt mit
# #include "HeishaMon.h", und ein Include in Anfuehrungszeichen sucht IMMER
# zuerst im Verzeichnis der einbindenden Datei. Aus src/ heraus gewinnt also
# der echte Header - und der zieht LittleFS, WiFi, Webserver und den Rest der
# Arduino-Welt nach. Deshalb wird die Uebersetzungseinheit in ein
# Bauverzeichnis KOPIERT, in dem die Ersatzheader aus test/stubs/ daneben
# liegen. Uebersetzt wird der unveraenderte Quelltext, kein Nachbau.
#
#   ./test/decode_hosttest.sh                    # byte110_test.cpp
#   ./test/decode_hosttest.sh test/anderer.cpp   # ein anderer Test
#
# Rueckgabewert != 0 = Uebersetzung oder Zusicherung fehlgeschlagen.
set -eu

TEST=${1:-test/byte110_test.cpp}
REPO=$(cd "$(dirname "$0")/.." && pwd)
BAU=${TMPDIR:-/tmp}/heisha_decode_hosttest

# Fehlt der Test, bricht der Lauf hier ab statt mit einer Compilermeldung
[ -f "$REPO/$TEST" ] || { echo "FEHLER: $TEST nicht gefunden" >&2; exit 2; }
command -v c++ >/dev/null || { echo "FEHLER: c++ nicht gefunden (Xcode Command Line Tools)" >&2; exit 2; }

# Bauverzeichnis frisch anlegen - Reste eines aelteren Standes koennten sonst
# stillschweigend mituebersetzt werden
rm -rf "$BAU"
mkdir -p "$BAU"
cp "$REPO"/src/decode.cpp "$REPO"/src/decode.h "$REPO"/src/Topics.cpp "$REPO"/src/Topics.h "$BAU/"
cp "$REPO"/test/stubs/*.h "$BAU/"

NAME=$(basename "$TEST" .cpp)
c++ -std=c++17 -O2 -Wall -I "$BAU" -o "$BAU/$NAME" \
    "$REPO/$TEST" "$BAU/decode.cpp" "$BAU/Topics.cpp"
"$BAU/$NAME"
