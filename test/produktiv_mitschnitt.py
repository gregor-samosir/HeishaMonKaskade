#!/usr/bin/env python3
"""Passiver Mitschnitt am laufenden HeishaMon - sendet selbst NICHTS.

Schaltet nur den Hexlog ein (bewusst kein 'L', damit die MQTT-Logs
weiterlaufen), hoert eine Weile mit und zerlegt jedes gesendete
Kommandotelegramm in seine Felder. Danach Hexlog wieder aus.

WICHTIG zur Deutung: Der Node-RED-Verteiler arbeitet idempotent - er sendet
nur Kanaele, deren Wert sich GEAENDERT hat (lastSent-Filter in syncOutputs).
Ein Telegramm mit nur einem gesetzten Feld ist deshalb voellig normal und
kein Hinweis auf ein Problem. Alle Kanaele auf einmal kommen erst beim
zyklischen Re-Assert (Standard: alle 5 Minuten, dort wird lastSent geleert).
Deshalb laeuft dieses Werkzeug per Vorgabe lange genug, um einen Re-Assert
mitzunehmen, und bricht nicht beim ersten Kommando ab.

  ./produktiv_mitschnitt.py --esp 192.168.2.120
  ./produktiv_mitschnitt.py --esp 192.168.2.193 --warten 400
"""
import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from heisha_probe import (drain, parse_all_command_telegrams,  # noqa: E402
                          telnet_connect, zeige_byte4)

# Byte-Position -> (Bezeichnung, Maske, Umrechnung Rohbyte -> Klartext)
FELDER = [
    (4,  "Heatpump",   0x03, lambda v: {0: "-", 1: "aus", 2: "an"}.get(v, f"?{v}")),
    (4,  "WaterPump",  0x30, lambda v: {0: "-", 1: "auto", 2: "an", 3: "Luft"}.get(v >> 4, f"?{v}")),
    (4,  "ForceDHW",   0xC0, lambda v: {0: "-", 1: "aus", 2: "an"}.get(v >> 6, f"?{v}")),
    (5,  "HolidayMode", 0x30, lambda v: {0: "-", 1: "aus", 2: "an"}.get(v >> 4, f"?{v}")),
    (6,  "OperationMode", 0xFF, lambda v: "-" if v == 0 else f"roh {v}"),
    (7,  "Quiet/Powerful", 0xFF, lambda v: "-" if v == 0 else f"roh {v}"),
    (38, "Z1 Heat",    0xFF, lambda v: "-" if v == 0 else f"{v - 128} C"),
    (39, "Z1 Cool",    0xFF, lambda v: "-" if v == 0 else f"{v - 128} C"),
    (42, "DHW Temp",   0xFF, lambda v: "-" if v == 0 else f"{v - 128} C"),
    (45, "PumpSpeed",  0xFF, lambda v: "-" if v == 0 else f"{v - 1}"),
    # Heiz-/Kuehlkurve Zone 1 (SET27-SET34, seit 3.2.0)
    (75, "HeatCurve TgtHigh",  0xFF, lambda v: "-" if v == 0 else f"{v - 128} C"),
    (76, "HeatCurve TgtLow",   0xFF, lambda v: "-" if v == 0 else f"{v - 128} C"),
    (77, "HeatCurve OutLow",   0xFF, lambda v: "-" if v == 0 else f"{v - 128} C"),
    (78, "HeatCurve OutHigh",  0xFF, lambda v: "-" if v == 0 else f"{v - 128} C"),
    (86, "CoolCurve TgtHigh",  0xFF, lambda v: "-" if v == 0 else f"{v - 128} C"),
    (87, "CoolCurve TgtLow",   0xFF, lambda v: "-" if v == 0 else f"{v - 128} C"),
    (88, "CoolCurve OutLow",   0xFF, lambda v: "-" if v == 0 else f"{v - 128} C"),
    (89, "CoolCurve OutHigh",  0xFF, lambda v: "-" if v == 0 else f"{v - 128} C"),
]


def felder_von(tel):
    """Liefert die im Telegramm gesetzten Felder als Liste (Name, Klartext)."""
    gesetzt = []
    for pos, name, maske, fmt in FELDER:
        roh = int(tel[pos], 16) & maske
        if roh:
            gesetzt.append((name, fmt(roh)))
    return gesetzt


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--esp", default="192.168.2.120", help="IP des Geraets")
    ap.add_argument("--warten", type=int, default=360,
                    help="Mithoerdauer in s (Vorgabe 360 = laenger als der "
                         "5-min-Re-Assert-Takt)")
    ap.add_argument("--stop-beim-ersten", action="store_true",
                    help="schon nach dem ersten Kommando aufhoeren (schnell, "
                         "sieht aber den Re-Assert meist nicht)")
    args = ap.parse_args()

    captured = []
    print(f"== Telnet {args.esp}:23 (passiv, es wird nichts gesendet) ==")
    sock = telnet_connect(args.esp)
    with sock:
        drain(sock, 2.0, captured)
        print("== Hexlog EIN (nur 'H', MQTT-Logs bleiben unveraendert) ==")
        sock.sendall(b"H")
        drain(sock, 1.5, captured)

        marker = "Send command" if args.stop_beim_ersten else None
        print(f"== Hoere {args.warten} s mit"
              f"{' (Abbruch beim ersten Kommando)' if marker else ''} ==")
        drain(sock, args.warten, captured, stop_marker=marker)

        print("== Hexlog AUS ==")
        sock.sendall(b"H")
        drain(sock, 1.0, captured)

    text = "".join(captured)
    telegramme = parse_all_command_telegrams(text)
    if not telegramme:
        print(f"\nIn {args.warten} s kam kein Kommando - nur Abfragen. "
              "Der Verteiler hatte nichts zu senden (alle Kanaele unveraendert).")
        return 2

    print("\n" + "=" * 70)
    print(f"{len(telegramme)} Kommandotelegramm(e) mitgeschnitten")
    print("=" * 70)
    for i, tel in enumerate(telegramme, 1):
        b4 = int(tel[4], 16)
        gesetzt = felder_von(tel)
        print(f"\n--- Telegramm {i} ---  {' '.join(tel[:6])} ...")
        print(f"Byte 4: {zeige_byte4(b4)}")
        if gesetzt:
            print("Gesetzte Felder:")
            for name, wert in gesetzt:
                print(f"    {name:<16} {wert}")
        else:
            print("    (keine Felder gesetzt - unerwartet)")

    # Auswertung: wie viele der drei Felder von Byte 4 sind belegt?
    def felder_in_byte4(roh):
        return sum(1 for maske in (0x03, 0x30, 0xC0) if roh & maske)

    mehrfach_byte4 = [t for t in telegramme if felder_in_byte4(int(t[4], 16)) >= 2]
    voll = [t for t in telegramme if len(felder_von(t)) >= 4]

    print("\n" + "-" * 70)
    if mehrfach_byte4:
        print("MERGE BELEGT: mindestens ein Telegramm traegt zwei Felder in "
              "Byte 4 gleichzeitig.")
        print("Mit 3.0.1 waere hier eines der beiden stillschweigend "
              "verschwunden.")
    elif voll:
        print("Ein Telegramm mit vier oder mehr Feldern gesehen (vermutlich "
              "der Re-Assert),")
        print("aber Byte 4 trug nie zwei Felder gleichzeitig - dann standen "
              "Heatpump und")
        print("WaterPump in diesem Zyklus eben nicht beide zur Aenderung an.")
    else:
        print("Nur Telegramme mit wenigen Feldern - normal, der Verteiler "
              "sendet idempotent")
        print("(nur geaenderte Kanaele). Fuer den Merge-Nachweis den "
              "Re-Assert abwarten:")
        print("laenger mithoeren (--warten 400) oder einen Moduswechsel "
              "ausloesen.")
    if "Field conflict" in text:
        print("\nACHTUNG - Konflikt-Warnung im Log:")
        for line in text.splitlines():
            if "Field conflict" in line:
                print("  " + line.strip())
    print("-" * 70)
    return 0


if __name__ == "__main__":
    sys.exit(main())
