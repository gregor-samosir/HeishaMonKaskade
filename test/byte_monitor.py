#!/usr/bin/env python3
"""Einzelne Bytes des Antworttelegramms am laufenden Geraet beobachten.

Zweck: eine Byte-Zuordnung belegen statt sie aus Referenztabellen abzuleiten.
Das Werkzeug schaltet den Hexlog ueber Telnet ein, schneidet die 203-Byte-
Antworten der Waermepumpe mit, gibt die gewuenschten Bytes mit allen im
Protokoll ueblichen Umrechnungen aus und schaltet den Hexlog wieder ab.

  ./byte_monitor.py 192.168.2.120 4 45 95          # drei Bytes, 30 s lang
  ./byte_monitor.py 192.168.2.120 45 --dauer 60    # laenger mitschneiden

Vorgehen fuer einen Aenderungsnachweis: einmal vor und einmal nach dem
Schreiben eines Set-Kommandos laufen lassen und die Ausgaben vergleichen. Das
Byte, das mitwandert, traegt den Wert - alles andere ist Vermutung.

Kein Schreibvorgang an der Waermepumpe: Es wird nur der Hexlog des HeishaMon
umgeschaltet und mitgelesen. Der Hexlog wird am Ende auch dann wieder
ausgeschaltet, wenn das Skript mit einem Fehler abbricht.

Nur Standardbibliothek.
"""
import argparse
import re
import sys
import time

from heisha_probe import telnet_connect, drain

ANSWERSIZE = 203       # Laenge des Antworttelegramms inkl. Pruefsumme
ANSWER_HEADER = "71"   # Antwort/Abfrage; Kommandos beginnen mit F1


def parse_answer_telegrams(text):
    """Alle vollstaendigen 203-Byte-Antworttelegramme aus einem Hexlog ziehen.

    Der Hexlog gibt 32 Bytes je Zeile aus und mischt gesendete Kommandos
    (F1, 110 Bytes) unter die Antworten. Ein Header zaehlt deshalb nur als
    Telegrammanfang, wenn gerade keines offen ist - sonst wuerde ein
    Fortsetzungsblock, der zufaellig mit 71 beginnt, das Telegramm zerreissen.
    """
    bloecke = re.findall(r"data:\s+((?:[0-9A-F]{2}\s+)+)", text)
    telegramme, aktuell = [], None
    for block in bloecke:
        werte = block.split()
        if aktuell is None:
            if werte and werte[0].upper() == ANSWER_HEADER:
                aktuell = list(werte)
        else:
            aktuell += werte
        if aktuell and len(aktuell) >= ANSWERSIZE:
            telegramme.append([int(w, 16) for w in aktuell[:ANSWERSIZE]])
            aktuell = None
    return telegramme


def bitfelder(wert):
    """Byte in die vier 2-Bit-Felder der Projektzaehlung zerlegen (Bit 1 = MSB)."""
    return (f"1+2={(wert >> 6) & 3:02b} 3+4={(wert >> 4) & 3:02b} "
            f"5+6={(wert >> 2) & 3:02b} 7+8={wert & 3:02b}")


def zeige(telegramm, positionen):
    """Die gewuenschten Bytes mit den ueblichen Umrechnungen ausgeben."""
    for pos in positionen:
        wert = telegramm[pos]
        print(f"  Byte {pos:3d} = 0x{wert:02X} = {wert:3d} dez   "
              f"X-1={wert - 1:4d}   X-128={wert - 128:4d}   {bitfelder(wert)}")


def hexlog_umschalten(sock, protokoll):
    """Hexlog-Flag umschalten und die Quittung des Geraets abwarten."""
    sock.sendall(b"H")
    drain(sock, 3, protokoll, stop_marker="Toggled hexlog flag")


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("host", help="IP des HeishaMon")
    parser.add_argument("bytes", nargs="+", type=int,
                        help="Byte-Positionen im Antworttelegramm (0-202)")
    parser.add_argument("--dauer", type=int, default=30,
                        help="Mitschnittdauer in Sekunden (Vorgabe 30)")
    args = parser.parse_args()

    # Bereich pruefen, bevor eine Verbindung aufgemacht wird
    ungueltig = [p for p in args.bytes if not 0 <= p < ANSWERSIZE]
    if ungueltig:
        sys.exit(f"Fehler: Byte-Position ausserhalb 0..{ANSWERSIZE - 1}: {ungueltig}")

    try:
        sock = telnet_connect(args.host)
    except OSError as fehler:
        sys.exit(f"Fehler: keine Telnet-Verbindung zu {args.host} - {fehler}")

    protokoll = []
    try:
        hexlog_umschalten(sock, protokoll)
        print(f"Hexlog eingeschaltet, schneide {args.dauer} s mit ...", file=sys.stderr)
        drain(sock, args.dauer, protokoll)

        text = "".join(protokoll)
        telegramme = parse_answer_telegrams(text)

        # Kam nichts an, war der Hexlog vorher schon eingeschaltet und das
        # Umschalten hat ihn abgeschaltet - einmal zuruecknehmen und erneut
        if not telegramme and "data:" not in text:
            print("Keine Hexdaten - Flag war offenbar an, schalte zurueck und "
                  "messe erneut ...", file=sys.stderr)
            hexlog_umschalten(sock, protokoll)
            protokoll = []
            drain(sock, args.dauer, protokoll)
            telegramme = parse_answer_telegrams("".join(protokoll))

        if not telegramme:
            print("Kein vollstaendiges Antworttelegramm im Mitschnitt.", file=sys.stderr)
            return 1

        stempel = time.strftime("%H:%M:%S")
        print(f"{args.host}  {stempel}  {len(telegramme)} Antworttelegramme")
        for nummer, telegramm in enumerate(telegramme, 1):
            print(f"\nTelegramm {nummer}:")
            zeige(telegramm, args.bytes)

        # Wandert ein Byte waehrend des Mitschnitts, ist das fuer sich schon ein
        # Befund - deshalb ausdruecklich benennen statt es im Zahlenwald zu lassen
        if len(telegramme) > 1:
            gewandert = [p for p in args.bytes
                         if len({t[p] for t in telegramme}) > 1]
            print(f"\nWaehrend des Mitschnitts veraendert: "
                  f"{gewandert if gewandert else 'keines der beobachteten Bytes'}")
        return 0
    finally:
        # Hexlog in jedem Fall wieder abschalten, auch nach einem Fehler
        try:
            hexlog_umschalten(sock, protokoll)
            print("Hexlog wieder ausgeschaltet.", file=sys.stderr)
        except OSError:
            print("WARNUNG: Hexlog konnte nicht abgeschaltet werden - "
                  "per Telnet Taste 'H' nachholen.", file=sys.stderr)
        sock.close()


if __name__ == "__main__":
    sys.exit(main())
