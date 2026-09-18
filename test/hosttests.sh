#!/bin/sh
# Alle Hosttests uebersetzen und ausfuehren - lokal UND in der CI.
#
# Warum ein Skript: Bis 2026-09-17 stand die Liste in main.yml, und wer lokal
# pruefen wollte, hat sie abgeschrieben. Davon gab es drei Kopien (main.yml,
# test/README.md, Memory), und sie waren auseinandergelaufen - in der README
# fehlten zwei Tests. Die CI ruft jetzt genau dieses Skript auf; es gibt nur
# noch diese eine Liste. Vor dem Merge (CLAUDE.md, Regel 6):
#
#   ./test/hosttests.sh            # aus dem Repo-Wurzelverzeichnis oder test/
#
# Laeuft ALLE Tests durch, auch wenn einer scheitert, und nennt am Ende jeden
# gescheiterten. Rueckgabewert != 0, sobald einer scheitert oder ein Test
# nicht in der Liste steht (Abschnitt "Vollstaendigkeit" unten).
#
# Parallel nicht zweimal starten: Die Bauverzeichnisse (hier und in
# decode_hosttest.sh) sind feste Pfade, zwei gleichzeitige Laeufe
# ueberschreiben sich gegenseitig die Programme.
set -u

# --- Umgebung -------------------------------------------------------------
# Ins Repo-Wurzelverzeichnis wechseln, damit alle Pfade unten repo-relativ
# gelten, egal von wo das Skript gerufen wird
REPO=$(cd "$(dirname "$0")/.." && pwd)
cd "$REPO" || exit 2
BAU=${TMPDIR:-/tmp}/heisha_hosttests

# Fehlende Werkzeuge frueh und verstaendlich melden statt als Folgefehler
command -v c++ >/dev/null || { echo "FEHLER: c++ nicht gefunden (Xcode Command Line Tools)" >&2; exit 2; }
command -v python3 >/dev/null || { echo "FEHLER: python3 nicht gefunden" >&2; exit 2; }

# Bauverzeichnis frisch anlegen - ein altes Programm darf nie an Stelle eines
# nicht uebersetzbaren neuen laufen
rm -rf "$BAU"
mkdir -p "$BAU" || exit 2

GESCHEITERT=""   # Namen der gescheiterten Tests, fuer die Zusammenfassung
GELAUFEN=""      # alle gerufenen .cpp, fuer die Vollstaendigkeitspruefung
ANZAHL=0

# --- Helfer ---------------------------------------------------------------
# Jeder Helfer zaehlt mit, fuehrt aus und merkt sich ein Scheitern. Ein
# Uebersetzungsfehler zaehlt wie eine gebrochene Zusicherung.
kopf() {
    ANZAHL=$((ANZAHL + 1))
    echo ""
    echo "=== $1"
}

# Eigenstaendiger Test: bindet nur arduino-freie Header aus src/ ein
cpp_test() {
    kopf "$1"
    GELAUFEN="$GELAUFEN test/$1.cpp"
    if c++ -std=c++17 -O2 -Wall -o "$BAU/$1" "test/$1.cpp" && "$BAU/$1"; then
        :
    else
        GESCHEITERT="$GESCHEITERT $1"
    fi
}

# Test gegen den echten Dekodierpfad - braucht den Baurahmen mit den
# Ersatzheadern (Begruendung im Kopf von decode_hosttest.sh)
decode_test() {
    kopf "$1"
    GELAUFEN="$GELAUFEN test/$1.cpp"
    if ./test/decode_hosttest.sh "test/$1.cpp"; then
        :
    else
        GESCHEITERT="$GESCHEITERT $1"
    fi
}

py_test() {
    kopf "$1"
    if python3 "test/$1.py"; then
        :
    else
        GESCHEITERT="$GESCHEITERT $1"
    fi
}

# --- Die Liste ------------------------------------------------------------
# Alle pruefen ihre Ergebnisse seit 3.6.0 selbst und geben bei gebrochener
# Zusicherung != 0 zurueck - gefangen werden also nicht nur Uebersetzungs-
# fehler und Abstuerze. Zu jedem Test steht, WARUM er hier steht.

# Merge-Logik der Set-Kommandos (Masken, Umrechnung)
cpp_test merge_test

# Typ-, Laengen- und Pruefsummenregel fuer das Antworttelegramm - bindet
# src/telegram.h direkt ein, prueft also den Code, der auf dem Geraet laeuft
cpp_test telegramm_test

# Zeitregeln des Kommando-Sammelfensters aus 3.8.0, bindet src/sendwindow.h
# direkt ein. Deckt den millis()-Ueberlauf nach 49,7 Tagen ab - an der Anlage
# waere der nicht abzuwarten.
cpp_test sendwindow_test

# Kodierung von SET35/SET36 auf Byte 28 - legt die Merge-Logik aus
# commands.cpp und die Dekodierer aus decode.cpp nebeneinander. Byte 28 traegt
# zwei Bitfelder; ohne bitgenaue Maske schaltet ein Kuehl-Kommando die Heizung
# mit um, und genau das prueft die Gegenprobe.
cpp_test byte28_test

# Regeln des Notbetriebs aus 3.12.0 - Vollstaendigkeit der gehaltenen Werte,
# Bereichsgrenzen (verwerfen statt klemmen), die Karenzzeit-Ausnahme fuer den
# notbetrieb-Zweig und der Zustandsautomat samt millis()-Ueberlauf. Die
# Karenz-Ausnahme ist der Grund: wird sie vergessen, funktioniert der Knopf im
# Labor und nach jedem Neustart nicht mehr - auffallen wuerde das erst im
# Ernstfall.
cpp_test notbetrieb_test

# Zeitregeln der Verbindungswacht aus 3.13.0 - Karenz von 5 Minuten, der
# Sonderfall "seit dem Neustart nie verbunden" und der millis()-Ueberlauf. Der
# Ueberlauf ist der Grund: Eine bei jeder Abfrage gerechnete Dauer faellt an
# der Naht unter die Karenz zurueck, und die Stoermeldung verschwaende
# ausgerechnet nach einem sehr langen Ausfall.
cpp_test verbindung_test

# Gueltigkeitsregel des RTC-Spiegels aus 3.20.0 - Magic samt Layoutnummer,
# Rolle, Maskenbreite, Pruefsumme und die Saettigung des Bootzaehlers. Der
# Spiegel liegt in einem Speicher, den die Firmware NIE initialisiert
# (RTC_NOINIT_ATTR); was dort nach einem Reset steht, entscheidet allein diese
# Regel. Zu grosszuegig: der Notbetrieb uebernimmt Bitmuster als Kurvenwerte.
# Zu streng: die Werte sind nach jedem Neustart weg, ohne dass es auffaellt,
# solange der Broker sie nachliefert. Beides ist am Geraet nicht zu sehen.
cpp_test rtcspiegel_test

# KNX-Tunnel des Notbetriebsschritts "Vorderhaus" aus 3.21.0 - bindet
# src/knxtunnel.h direkt ein. Jeder Rahmen byteweise gegen die Rohbytes aus
# xknx und aus den Mitschnitten an der Anlage (2026-09-12), dazu die Zweige der
# Rueckleseregel A. Der Schritt laeuft nur im Ernstfall; ein falsches Byte
# fiele erst dann auf - oder nie, wenn ein Filter die Antwort von openknx
# mitzaehlt (der meldet im Test GRUEN aus seinem Zwischenspeicher, und im
# Ernstfall ist openknx nicht da).
cpp_test knx_test

# Bitzuordnung der vier Ist-Zustands-Topics (TOP99-102) aus Byte 110
decode_test byte110_test

# Kodierung von SET37-SET39 (Heizstab, 3.17.0) gegen den echten Dekodierpfad.
# Byte 9 traegt beide Heizstab-Freigaben, Byte 5 den ForceHeater neben
# HolidayMode und dem Zeitprogramm; ohne bitgenaue Maske legt ein Kommando das
# Nachbarfeld mit um. Genau das prueft die Gegenprobe, vor dem Flashen.
decode_test byte9_test

# Die sieben Installer-Topics (TOP105-111) aus Byte 25 und Byte 23 gegen die am
# 2026-08-31 gemessenen Rohbytes. Byte 25 traegt drei Topics, Byte 23 vier -
# ein Dekodierer zwei Bit daneben liefert Werte, die plausibel aussehen und
# trotzdem falsch sind.
decode_test byte23_25_test

# Jede benutzte w3-Klasse muss im eingebetteten CSS definiert sein und jede
# Farbklasse hinter .w3-button stehen. Ein Compiler prueft davon nichts. Am
# 2026-08-20 war der Notbetriebsknopf deshalb grauer Text statt rotem Knopf;
# am 2026-08-23 fiel eine fehlende Klasse erst in der CI auf, als 3.14.0 schon
# auf beiden Stufen lief, weil lokal nur ein Teil der Tests gelaufen war.
py_test css_klassen_test

# Byte-Zuordnung.md und SET-TOP-Zuordnung.md gegen setCommands[] und
# stateTopics[]: jedes SET und TOP auf seinem Byte und seinen Bits, Namen,
# Einheit und Kodierung. Eine veraltete Tabelle sieht aus wie eine richtige -
# ohne diesen Test fiele ein neues, gestrichenes oder umkodiertes Topic dort
# erst auf, wenn jemand nach dem Byte sucht und das Falsche findet.
py_test doku_zuordnung_test

# --- Vollstaendigkeit -----------------------------------------------------
# Jede test/*_test.cpp ist ein Hosttest (die Hardware-Tests sind .py). Steht
# eine nicht in der Liste oben, laeuft sie weder lokal noch in der CI - und
# niemand merkt es, weil alles gruen ist. Deshalb zaehlt das als Scheitern.
FEHLT=""
for datei in test/*_test.cpp; do
    case " $GELAUFEN " in
        *" $datei "*) ;;
        *) FEHLT="$FEHLT $datei" ;;
    esac
done

# --- Zusammenfassung ------------------------------------------------------
echo ""
echo "================================================================"
if [ -n "$FEHLT" ]; then
    echo "NICHT IN DER LISTE von test/hosttests.sh:$FEHLT"
fi
if [ -n "$GESCHEITERT" ]; then
    echo "GESCHEITERT:$GESCHEITERT"
fi
if [ -n "$FEHLT" ] || [ -n "$GESCHEITERT" ]; then
    echo "HOSTTESTS ROT ($ANZAHL gelaufen)"
    exit 1
fi
echo "HOSTTESTS GRUEN: alle $ANZAHL bestanden"
exit 0
