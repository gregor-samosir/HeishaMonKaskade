#!/usr/bin/env python3
"""Passiver Telnet-Mitschnitt eines HeishaMon-Geraets.

Sendet NICHTS. Die Firmware verhandelt keine Telnet-Optionen, ein roher Socket
auf Port 23 genuegt - telnetlib ist ab Python 3.13 aus der Standardbibliothek
entfernt. Es geht nur EINE Telnet-Sitzung gleichzeitig: Wer sich verbindet,
waehrend jemand anders drauf ist, kappt dessen Sitzung.

Zweck ist die Antwortquote der seriellen Strecke und alles, was als <DBG> im
Telnetstrom steht - produktiv_mitschnitt.py wertet nur Kommandotelegramme aus
und zeigt diese Zeilen nicht. Auswertung: siehe test/README.md,
"Antwortquote messen".

  ./telnet_mitschnitt.py 192.168.2.120 420 > mitschnitt.txt
"""
import socket
import sys
import time

# ---------------------------------------------------------------------------
# Argumente pruefen - ohne gueltige Werte gar nicht erst verbinden
# ---------------------------------------------------------------------------
if len(sys.argv) != 3:
    sys.exit(f"Aufruf: {sys.argv[0]} <ip-oder-hostname> <sekunden>")

ziel = sys.argv[1]
try:
    dauer = float(sys.argv[2])
except ValueError:
    sys.exit(f"Dauer ist keine Zahl: {sys.argv[2]!r}")
if not (0 < dauer <= 3600):
    sys.exit(f"Dauer muss zwischen 0 und 3600 s liegen, nicht {dauer:g}")

# ---------------------------------------------------------------------------
# Verbinden - eine unerreichbare Adresse ist der haeufigste Fehler, deshalb
# eine eigene Meldung statt eines Tracebacks
# ---------------------------------------------------------------------------
try:
    verbindung = socket.create_connection((ziel, 23), timeout=5)
except OSError as fehler:
    sys.exit(f"Keine Telnet-Verbindung zu {ziel}:23 - {fehler}")

# Kurzer Lesetimeout, damit die Schleife die Restdauer regelmaessig prueft und
# nicht bis zum naechsten Byte blockiert (das Geraet schweigt zwischen den
# Zyklen bis zu 6 s).
verbindung.settimeout(1.0)
ende = time.monotonic() + dauer
rest = b""

try:
    while time.monotonic() < ende:
        try:
            block = verbindung.recv(4096)
        except socket.timeout:
            continue  # nichts gekommen, Restdauer erneut pruefen
        except OSError as fehler:
            print(f"# Verbindung gestoert: {fehler}", file=sys.stderr)
            break
        if not block:
            print("# Gegenstelle hat aufgelegt", file=sys.stderr)
            break

        # Zeilenweise ausgeben und sofort spuelen: Ein abgebrochener Lauf soll
        # das bis dahin Gelesene behalten, nicht im Puffer verlieren.
        rest += block
        *zeilen, rest = rest.split(b"\n")
        for zeile in zeilen:
            sys.stdout.write(zeile.decode("utf-8", "replace").rstrip("\r") + "\n")
            sys.stdout.flush()
finally:
    verbindung.close()
    if rest:  # angefangene letzte Zeile nicht unterschlagen
        sys.stdout.write(rest.decode("utf-8", "replace") + "\n")
        sys.stdout.flush()
