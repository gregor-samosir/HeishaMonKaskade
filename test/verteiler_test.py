#!/usr/bin/env python3
"""Abnahmetest: sechs gleichzeitige SET-Kommandos in EINEM Telegramm.

Bildet die OUTPUT_MAP des Node-RED-Verteilers (Hauptmodus-Verteiler V6.2,
~/nodered-flows) fuer eine Waermepumpe nach - genau die sechs Kanaele, die
beim 5-min-Re-Assert gleichzeitig herausgehen. Zwei davon (Heatpump und
WaterPump) teilen sich Byte 4.

Nur gegen den Pruefstand fahren, nicht gegen eine WP.

  ./verteiler_test.py --esp 192.168.2.197
"""
import argparse
import socket
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from heisha_probe import (drain, parse_command_telegram,  # noqa: E402
                          telnet_connect)
from mqtt_pub import build_connect, build_publish  # noqa: E402

# Kanal, Wert, Byte, Maske, erwartetes Rohbyte, Bedeutung
# Werte entsprechen MODES[4].heat des Verteilers (Reihenschaltung, Heizen)
KANAELE = [
    ("Z1HeatRequestTemperature", "35", 38, 0xFF, 163, "Vorlauf-Soll heizen 35 C"),
    ("Z1CoolRequestTemperature", "18", 39, 0xFF, 146, "Vorlauf-Soll kuehlen 18 C"),
    ("Heatpump",                  "1",  4, 0x03,   2, "WP an"),
    ("OperationMode",             "4",  6, 0xFF,  34, "Heat+DHW"),
    ("WaterPump",                 "0",  4, 0x30,  16, "UWP auto"),
    ("WaterPumpSpeed",          "125", 45, 0xFF, 126, "Pumpendrehzahl 125"),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--esp", default="192.168.2.197")
    ap.add_argument("--broker", default="192.168.2.147")
    ap.add_argument("--prefix", default="panasonic_heat_pump_test")
    ap.add_argument("--gap-ms", type=int, default=20)
    args = ap.parse_args()

    if "test" not in args.prefix:
        print(f"ABBRUCH: '{args.prefix}' sieht nach einem produktiven Prefix aus.")
        print("         Dieser Test SENDET Kommandos - nur gegen den Pruefstand fahren.")
        return 1

    captured = []
    sock = telnet_connect(args.esp)
    with sock:
        drain(sock, 1.5, captured)
        sock.sendall(b"L")
        drain(sock, 0.8, captured)
        sock.sendall(b"H")
        drain(sock, 1.2, captured)

        print(f"== Sende {len(KANAELE)} Kanaele, Abstand {args.gap_ms} ms ==")
        mq = socket.create_connection((args.broker, 1883), timeout=5)
        with mq:
            mq.sendall(build_connect("heisha-pruefstand"))
            connack = mq.recv(4)
            if len(connack) < 4 or connack[3] != 0:
                print(f"FEHLER: MQTT-CONNACK {connack.hex()}")
                return 1
            for topic, wert, *_ in KANAELE:
                mq.sendall(build_publish(f"{args.prefix}/set/{topic}", wert))
                print(f"  -> {topic} = {wert}")
                time.sleep(args.gap_ms / 1000.0)
            time.sleep(0.3)
            mq.sendall(bytes([0xE0, 0x00]))

        drain(sock, 5.0, captured, stop_marker="Send command")

    tel = parse_command_telegram("".join(captured))
    if tel is None:
        print("\nFEHLER: kein Kommandotelegramm im Hexlog.")
        return 1

    print(f"\nTelegramm: {len(tel)} Bytes, Anfang {' '.join(tel[:8])}")
    print("=" * 76)
    print(f"{'Kanal':<28}{'Byte':>5}{'Maske':>7}{'erwartet':>10}{'gelesen':>9}   Ergebnis")
    print("-" * 76)
    alle_ok = True
    for topic, _wert, pos, maske, erwartet, _bedeutung in KANAELE:
        ist = int(tel[pos], 16) & maske
        ok = ist == (erwartet & maske)
        alle_ok &= ok
        print(f"{topic:<28}{pos:>5}{maske:>#7x}{erwartet & maske:>10}"
              f"{ist:>9}   {'ok' if ok else 'FEHLT'}")
    print("=" * 76)
    print("BESTANDEN: alle sechs Kanaele in einem Telegramm."
          if alle_ok else "DURCHGEFALLEN: mindestens ein Kanal fehlt.")
    return 0 if alle_ok else 1


if __name__ == "__main__":
    sys.exit(main())
