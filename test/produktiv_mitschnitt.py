#!/usr/bin/env python3
"""Passiver Mitschnitt am laufenden HeishaMon - sendet selbst NICHTS.

Schaltet nur den Hexlog ein (bewusst kein 'L', damit die MQTT-Logs
weiterlaufen), hoert mit, bis der Node-RED-Verteiler das naechste Kommando
schickt, und schaltet den Hexlog danach wieder aus.

Erwartungswert Byte 4, solange die zwei 5-s-delay-Nodes im Logik-Flow noch
drin sind: WaterPump und Heatpump kommen in ZWEI getrennten Telegrammen,
also 0x10 und dann 0x01/0x02. Ohne die Delays muessten beide Felder in einem
Telegramm stehen (0x11 bzw. 0x12).

  ./produktiv_mitschnitt.py --esp 192.168.2.120 --warten 330
"""
import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from heisha_probe import (drain, parse_all_command_telegrams,  # noqa: E402
                          telnet_connect, zeige_byte4)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--esp", default="192.168.2.120", help="IP des Produktivgeraets")
    ap.add_argument("--warten", type=int, default=330,
                    help="max. Wartezeit in s (Re-Assert laeuft im 5-min-Takt)")
    ap.add_argument("--nachlauf", type=int, default=8,
                    help="nach dem ersten Kommando noch so lange mithoeren, "
                         "um ein zweites Telegramm aus dem 5-s-Delay zu fangen")
    args = ap.parse_args()

    captured = []
    print(f"== Telnet {args.esp}:23 (passiv, es wird nichts gesendet) ==")
    sock = telnet_connect(args.esp)
    with sock:
        drain(sock, 2.0, captured)
        print("== Hexlog EIN (nur 'H', MQTT-Logs bleiben unveraendert) ==")
        sock.sendall(b"H")
        drain(sock, 1.5, captured)

        print(f"== Warte auf den naechsten Verteiler-Befehl (max. {args.warten} s) ==")
        gefunden = drain(sock, args.warten, captured, stop_marker="Send command")
        if gefunden:
            print(f"== Kommando gesehen, hoere noch {args.nachlauf} s nach ==")
            drain(sock, args.nachlauf, captured)

        print("== Hexlog AUS ==")
        sock.sendall(b"H")
        drain(sock, 1.0, captured)

    text = "".join(captured)
    telegramme = parse_all_command_telegrams(text)
    if not telegramme:
        print(f"\nIn {args.warten} s kam kein Kommando - nur Abfragen. "
              "Der Verteiler hatte nichts zu senden.")
        return 2

    print("\n" + "=" * 68)
    print(f"{len(telegramme)} Kommandotelegramm(e) mitgeschnitten")
    print("=" * 68)
    for i, tel in enumerate(telegramme, 1):
        b4 = int(tel[4], 16)
        print(f"\n--- Telegramm {i} ---")
        print(f"Anfang  : {' '.join(tel[:8])} ...  ({len(tel)} Bytes)")
        print(f"Byte  4 : {zeige_byte4(b4)}")
        print(f"Byte  6 : 0x{int(tel[6], 16):02X}   OperationMode")
        print(f"Byte 38 : 0x{int(tel[38], 16):02X}   Z1 Heat  = {int(tel[38], 16) - 128} C"
              if int(tel[38], 16) else "Byte 38 : 0x00   Z1 Heat  nicht gesetzt")
        print(f"Byte 39 : 0x{int(tel[39], 16):02X}   Z1 Cool  = {int(tel[39], 16) - 128} C"
              if int(tel[39], 16) else "Byte 39 : 0x00   Z1 Cool  nicht gesetzt")
        print(f"Byte 45 : 0x{int(tel[45], 16):02X}   PumpSpeed = {int(tel[45], 16) - 1}"
              if int(tel[45], 16) else "Byte 45 : 0x00   PumpSpeed nicht gesetzt")

    vereint = [t for t in telegramme
               if (int(t[4], 16) & 0x03) and (int(t[4], 16) & 0x30)]
    print("\n" + "-" * 68)
    if vereint:
        print("Heatpump und WaterPump stehen in EINEM Telegramm - der Merge "
              "wirkt und die 5-s-Delays im Flow werden nicht mehr gebraucht.")
    else:
        print("Heatpump und WaterPump kamen getrennt. Das ist erwartet, solange "
              "die 5-s-delay-Nodes im Logik-Flow aktiv sind.")
    if "Field conflict" in text:
        print("\nACHTUNG - Konflikt-Warnung im Log:")
        for line in text.splitlines():
            if "Field conflict" in line:
                print("  " + line.strip())
    print("-" * 68)
    return 0


if __name__ == "__main__":
    sys.exit(main())
