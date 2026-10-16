#!/usr/bin/env python3
"""Telnet-Client, der nicht mehr liest - Nachweis fuer S2 (Arbeitsplan 3.24.0).

Verbindet sich auf Port 23 mit winzigem Empfangspuffer, liest danach NIE wieder
und fragt parallel die Startseite des Geraets ab. Haelt das Geraet `loop()`
an, sobald der Sendepuffer voll ist, wird die Antwortzeit der Startseite lang
oder bleibt ganz aus.

WARUM EIN EIGENES WERKZEUG (und nicht `nc` mit Ctrl-Z): Ein angehaltenes `nc`
bestaetigt weiter, der Kernel des Macs nimmt den Strom in seinen
Empfangspuffer (grob 128 KB und mehr). Bei rund 60 Byte/s Debug-Log dauert es
dann weit ueber 20 Minuten, bis das Geraet blockiert - nach 10 Minuten stuende
"laeuft weiter", ohne dass der Fall je eingetreten waere (Memory "Messfallen am
Prüfling"). Hier ist der Empfangspuffer VOR dem connect() auf ~1 KB gesetzt, das
Fenster schliesst nach wenigen KB.

GUELTIG ist eine Messung nur, wenn der Fall wirklich eingetreten ist:
  * gegen die Vorversion (blockierend): die Startseite haengt -> STILLSTAND.
  * gegen die neue Version: die Startseite bleibt schnell UND das Geraet hat den
    Client getrennt ("Gerät hat den Client getrennt: ja"). Bleibt beides aus, war
    der Puffer nie voll - dann ist "laeuft" KEIN Nachweis.

Um den Puffer schnell zu fuellen, sendet das Werkzeug zu Beginn einen Schwall
'M' (Speicherauskunft, je Taste eine Zeile). Das ist derselbe Schreibweg wie das
Debug-Log (TelnetStream), aber unabhaengig von der Lograte - die liegt ohne
Waermepumpe bei nur ~40 Byte/s, und der Hexlog (Taste 'H') hilft nicht, weil er
nur gesendete Kommandos loggt. 'M' aendert keinen Zustand am Geraet.
Nur gegen den Pruefling richten, nie gegen eine produktive Stufe.

  python3 -u test/telnet_haenger.py 192.168.2.194 --erwarte stillstand
  python3 -u test/telnet_haenger.py 192.168.2.194 --erwarte laeuft
  python3 -u test/telnet_haenger.py 192.168.2.194 --nur-lesen      # Gegenprobe
"""
import argparse
import http.client
import socket
import sys
import time

# Ab dieser Antwortzeit gilt die Startseite als "steht" - der Normalfall liegt
# im Bereich von Millisekunden bis wenigen hundert Millisekunden.
STILLSTAND_AB_S = 10.0
LAEUFT_BIS_S = 2.0
HTTP_TIMEOUT_S = 20.0


def lese_argumente():
    """Argumente lesen und auf sinnvolle Grenzen pruefen."""
    p = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    p.add_argument("ziel", help="IP oder Hostname des Prueflings")
    p.add_argument("--dauer", type=float, default=240.0,
                   help="Messdauer in Sekunden (Vorgabe 240)")
    p.add_argument("--intervall", type=float, default=2.0,
                   help="Abstand der Startseiten-Abfragen in s (Vorgabe 2)")
    p.add_argument("--rcvbuf", type=int, default=1024,
                   help="Empfangspuffer in Byte (Vorgabe 1024)")
    p.add_argument("--nur-lesen", action="store_true",
                   help="Gegenprobe: Client liest normal weiter")
    p.add_argument("--lesen-nach", type=float, default=0.0, metavar="SEK",
                   help="nach SEK Sekunden wieder lesen (Vorgabe 0 = nie); weist die "
                        "<INF>-Zeile 'Zeilen verworfen' nach")
    p.add_argument("--schwall", type=int, default=300,
                   help="Anzahl 'M'-Tasten zu Beginn (Vorgabe 300, 0 = nur Log)")
    p.add_argument("--erwarte", choices=("stillstand", "laeuft"),
                   help="Erwartung; setzt den Exit-Code (0 = Erwartung erfuellt)")
    a = p.parse_args()
    if not (10 <= a.dauer <= 3600):
        sys.exit(f"Dauer muss zwischen 10 und 3600 s liegen, nicht {a.dauer:g}")
    if not (0.5 <= a.intervall <= 60):
        sys.exit(f"Intervall muss zwischen 0,5 und 60 s liegen, nicht {a.intervall:g}")
    if not (0 <= a.lesen_nach <= a.dauer):
        sys.exit(f"lesen-nach muss zwischen 0 und der Dauer liegen, nicht {a.lesen_nach:g}")
    if not (0 <= a.schwall <= 5000):
        sys.exit(f"Schwall muss zwischen 0 und 5000 liegen, nicht {a.schwall}")
    if not (256 <= a.rcvbuf <= 65536):
        sys.exit(f"rcvbuf muss zwischen 256 und 65536 Byte liegen, nicht {a.rcvbuf}")
    return a


def verbinde(ziel, rcvbuf):
    """Telnet-Verbindung mit kleinem Empfangspuffer.

    SO_RCVBUF muss VOR connect() stehen, sonst ist das Fenster schon
    ausgehandelt."""
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, rcvbuf)
    s.settimeout(5.0)
    s.connect((ziel, 23))
    return s


def probe(ziel):
    """Startseite einmal abfragen. Rueckgabe: Antwortzeit in s oder None."""
    t0 = time.monotonic()
    try:
        c = http.client.HTTPConnection(ziel, 80, timeout=HTTP_TIMEOUT_S)
        c.request("GET", "/")
        c.getresponse().read(256)
        c.close()
    except (OSError, http.client.HTTPException):
        return None
    return time.monotonic() - t0


def main():
    a = lese_argumente()
    print(f"Ziel {a.ziel}, {a.dauer:g} s, rcvbuf {a.rcvbuf} B, "
          f"{'liest normal' if a.nur_lesen else 'liest NIE'}")
    try:
        s = verbinde(a.ziel, a.rcvbuf)
    except OSError as fehler:
        sys.exit(f"Keine Telnet-Verbindung zu {a.ziel}:23 - {fehler}")

    # Schwall 'M': je Taste schreibt das Geraet eine Zeile zurueck. Der
    # Zustand am Geraet aendert sich dadurch nicht.
    if a.schwall and not a.nur_lesen:
        s.sendall(b"M" * a.schwall)
    s.settimeout(0.2)

    ende = time.monotonic() + a.dauer
    start = time.monotonic()
    zeiten = []
    ausfaelle = 0
    gelesen = 0
    gesehen = b""
    while time.monotonic() < ende:
        if a.nur_lesen or (a.lesen_nach and time.monotonic() - start >= a.lesen_nach):
            try:
                d = s.recv(65536)
                gelesen += len(d)
                gesehen += d
                gesehen = gesehen[-65536:]  # nur das Ende behalten
            except (socket.timeout, ConnectionResetError):
                pass
        dt = probe(a.ziel)
        rel = time.monotonic() - start
        if dt is None:
            ausfaelle += 1
            print(f"{rel:7.1f} s  Startseite: KEINE ANTWORT ({HTTP_TIMEOUT_S:g} s)")
        else:
            zeiten.append(dt)
            marke = "  <-- STEHT" if dt >= STILLSTAND_AB_S else ""
            print(f"{rel:7.1f} s  Startseite: {dt * 1000:7.0f} ms{marke}")
        time.sleep(max(0.0, a.intervall - (dt or 0)))

    # Was hat das Geraet mit dem haengenden Client gemacht? Jetzt erst lesen:
    # gibt recv() b"" oder einen Reset zurueck, hat das Geraet ihn getrennt.
    getrennt = False
    puffer_bytes = 0
    s.settimeout(3.0)
    try:
        while True:
            d = s.recv(65536)
            if not d:
                getrennt = True
                break
            puffer_bytes += len(d)
            gesehen = (gesehen + d)[-65536:]
            if puffer_bytes > 4_000_000:
                break
    except (ConnectionResetError, BrokenPipeError):
        getrennt = True
    except socket.timeout:
        pass
    finally:
        s.close()

    groesste = max(zeiten) if zeiten else float("inf")
    steht = ausfaelle > 0 or groesste >= STILLSTAND_AB_S
    laeuft = ausfaelle == 0 and groesste < LAEUFT_BIS_S
    print()
    print(f"Abfragen: {len(zeiten) + ausfaelle}, ohne Antwort: {ausfaelle}, "
          f"groesste Antwortzeit: {groesste:.2f} s")
    print(f"Im Puffer gelesen: {puffer_bytes} Byte")
    print(f"Gerät hat den Client getrennt: {'ja' if getrennt else 'nein'}")
    if a.lesen_nach:
        print(f"<INF>-Zeile 'Zeilen verworfen' gesehen: "
              f"{'ja' if b'Zeilen verworfen' in gesehen else 'nein'}")
    urteil = "STILLSTAND" if steht else ("LAEUFT" if laeuft else "UNKLAR")
    print(f"Urteil: {urteil}")

    if a.erwarte is None:
        return 0
    if a.erwarte == "stillstand":
        return 0 if steht else 1
    # Erwartung "laeuft": nur gueltig, wenn der Fall wirklich eintrat.
    if laeuft and not a.nur_lesen and not getrennt:
        print("UNGUELTIG: Startseite schnell, aber das Geraet hat den Client "
              "nicht getrennt - der Puffer war vermutlich nie voll.")
        return 2
    return 0 if laeuft else 1


if __name__ == "__main__":
    sys.exit(main())
