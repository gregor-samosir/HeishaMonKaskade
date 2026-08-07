#!/usr/bin/env python3
"""Kerntest zum Bitmasken-Merge (Version 3.1.0).

Sendet Heatpump und WaterPump dicht hintereinander - beide belegen Byte 4 des
Protokolls in verschiedenen Bitgruppen. Bis 3.0.1 loeschte das zweite Kommando
das erste still aus.

  3.0.1 sendet  F1 6C 01 10 10   (Heatpump-Bits = 0, verloren)
  3.1.0 sendet  F1 6C 01 10 12   (beide Felder gesetzt)

Gedacht fuer den Pruefstand OHNE Waermepumpe: der Hexlog zeigt das Telegramm,
bevor es auf die Leitung geht - eine Gegenstelle ist dafuer nicht noetig.

  ./hexlog_test.py --esp 192.168.2.197 --broker 192.168.2.147
"""
import argparse
import socket
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from heisha_probe import (drain, parse_command_telegram,  # noqa: E402
                          telnet_connect, zeige_byte4)
from mqtt_pub import build_connect, build_publish  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--esp", default="192.168.2.197", help="IP des Pruefstands")
    ap.add_argument("--broker", default="192.168.2.147")
    ap.add_argument("--prefix", default="panasonic_heat_pump_test")
    ap.add_argument("--gap-ms", type=int, default=20)
    args = ap.parse_args()

    if "test" not in args.prefix:
        print(f"ABBRUCH: '{args.prefix}' sieht nach einem produktiven Prefix aus.")
        print("         Dieser Test SENDET Kommandos - nur gegen den Pruefstand fahren.")
        return 1

    captured = []
    print(f"== Telnet {args.esp}:23 ==")
    sock = telnet_connect(args.esp)
    with sock:
        drain(sock, 1.5, captured)
        # 'L': MQTT-Log aus (spart den Datenpunkt info.log), 'H': Hexlog ein
        print("== MQTT-Log aus (L), Hexlog ein (H) ==")
        sock.sendall(b"L")
        drain(sock, 1.0, captured)
        sock.sendall(b"H")
        drain(sock, 1.5, captured)

        print(f"== Heatpump=1 und WaterPump=0, Abstand {args.gap_ms} ms ==")
        mq = socket.create_connection((args.broker, 1883), timeout=5)
        with mq:
            mq.sendall(build_connect("heisha-pruefstand"))
            connack = mq.recv(4)
            if len(connack) < 4 or connack[3] != 0:
                print(f"FEHLER: MQTT-CONNACK {connack.hex()}")
                return 1
            mq.sendall(build_publish(f"{args.prefix}/set/Heatpump", "1"))
            time.sleep(args.gap_ms / 1000.0)
            mq.sendall(build_publish(f"{args.prefix}/set/WaterPump", "0"))
            time.sleep(0.3)
            mq.sendall(bytes([0xE0, 0x00]))

        drain(sock, 5.0, captured, stop_marker="Send command")

    tel = parse_command_telegram("".join(captured))
    if tel is None:
        print("\nFEHLER: kein Kommandotelegramm (F1...) im Hexlog gefunden.")
        print("        Kam das SET an? Ist der Hexlog aktiv? Mitschnitt:")
        print("".join(captured)[-1200:])
        return 1

    byte4 = int(tel[4], 16)
    print("\n" + "=" * 62)
    print(f"Telegramm : {' '.join(tel[:8])} ...  ({len(tel)} Bytes)")
    print(f"Byte 4    : {zeige_byte4(byte4)}")
    print("-" * 62)
    if (byte4 & 0x03) and (byte4 & 0x30):
        print("BESTANDEN: beide Felder stehen im selben Telegramm.")
        rc = 0
    else:
        fehlt = "Heatpump" if not (byte4 & 0x03) else "WaterPump"
        print(f"DURCHGEFALLEN: {fehlt} fehlt - Merge greift nicht.")
        rc = 1
    print("=" * 62)
    return rc


if __name__ == "__main__":
    sys.exit(main())
