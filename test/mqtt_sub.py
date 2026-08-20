#!/usr/bin/env python3
"""Minimaler MQTT-3.1.1-Subscriber ohne externe Abhaengigkeiten.

Gegenstueck zu mqtt_pub.py, mit einem bestimmten Zweck: nachweisen, was der
Broker einem NEUEN Abonnenten von sich aus einspielt. Genau davon lebt der
Notbetrieb - die Firmware bekommt ihre Kurvenwerte nach jedem Neustart nur
deshalb zurueck, weil der ioBroker-MQTT-Adapter jedem frischen Abonnement die
gespeicherten Werte hinterherschickt.

  ./mqtt_sub.py --host 192.168.2.147 'panasonic_heat_pump_test/notbetrieb/#'
  ./mqtt_sub.py --host 192.168.2.147 --dauer 10 'panasonic_heat_pump/state/#'

Der Ablauf entspricht dem der Firmware: verbinden, abonnieren, zuhoeren. Was in
den ersten Sekunden hereinkommt, ohne dass jemand publiziert, ist die
Wiedereinspielung.

Sendet selbst NICHTS ausser CONNECT/SUBSCRIBE - es werden keine Datenpunkte
angelegt und keine Werte veraendert.

Nur Standardbibliothek.

Stand 3.12.0 (neu in dieser Fassung).
"""
import argparse
import socket
import struct
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from mqtt_pub import build_connect, mqtt_string, remaining_length, read_connack, CONNACK_MEANING  # noqa: E402


def build_subscribe(paket_id, filter_liste):
    """SUBSCRIBE-Paket fuer mehrere Topic-Filter, alle QoS 0."""
    body = struct.pack("!H", paket_id)
    for f in filter_liste:
        body += mqtt_string(f) + bytes([0])
    return bytes([0x82]) + remaining_length(len(body)) + body


def lies_restlaenge(sock):
    """Variable-length-Feld lesen. None, wenn die Verbindung endet."""
    multiplier = 1
    wert = 0
    for _ in range(4):  # max. 4 Bytes laut Protokoll
        b = sock.recv(1)
        if not b:
            return None
        wert += (b[0] & 0x7F) * multiplier
        if not (b[0] & 0x80):
            return wert
        multiplier *= 128
    return None


def lies_genau(sock, n):
    """n Bytes vollstaendig lesen - recv() liefert sonst auch weniger."""
    daten = b""
    while len(daten) < n:
        teil = sock.recv(n - len(daten))
        if not teil:
            return None
        daten += teil
    return daten


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", required=True)
    ap.add_argument("--port", type=int, default=1883)
    ap.add_argument("--user")
    ap.add_argument("--password")
    ap.add_argument("--client-id", default="heisha-sub")
    ap.add_argument("--dauer", type=float, default=8.0,
                    help="Zuhoerdauer in Sekunden (Default 8)")
    ap.add_argument("filter", nargs="+", metavar="topic-filter")
    args = ap.parse_args()

    try:
        sock = socket.create_connection((args.host, args.port), timeout=5)
    except OSError as e:
        print(f"FEHLER: keine Verbindung zu {args.host}:{args.port} - {e}")
        return 1

    with sock:
        sock.sendall(build_connect(args.client_id, args.user, args.password))
        code = read_connack(sock)
        if code < 0:
            print("FEHLER: kein CONNACK erhalten")
            return 1
        print(f"CONNACK {code}: {CONNACK_MEANING.get(code, 'unbekannter Code')}")
        if code != 0:
            return 1

        sock.sendall(build_subscribe(1, args.filter))
        print(f"Abonniert: {', '.join(args.filter)}")
        print(f"Hoere {args.dauer:.0f} s zu - alles ab hier kommt vom Broker von sich aus.\n")

        ende = time.monotonic() + args.dauer
        anzahl = 0
        sock.settimeout(1.0)
        while time.monotonic() < ende:
            try:
                kopf = sock.recv(1)
            except socket.timeout:
                continue
            except OSError as e:
                print(f"FEHLER beim Lesen: {e}")
                return 1
            if not kopf:
                print("Verbindung vom Broker beendet")
                break

            typ = kopf[0] & 0xF0
            laenge = lies_restlaenge(sock)
            if laenge is None:
                break
            rest = lies_genau(sock, laenge) if laenge else b""
            if rest is None:
                break

            if typ == 0x90:  # SUBACK
                print("SUBACK erhalten")
                continue
            if typ != 0x30:  # nur PUBLISH interessiert hier
                continue

            # PUBLISH: Topic-Laenge, Topic, Rest = Nutzlast (QoS 0, keine ID)
            if len(rest) < 2:
                continue
            topic_len = struct.unpack("!H", rest[0:2])[0]
            topic = rest[2:2 + topic_len].decode("utf-8", "replace")
            nutzlast = rest[2 + topic_len:].decode("utf-8", "replace")
            retain = "retained" if (kopf[0] & 0x01) else "         "
            anzahl += 1
            print(f"  [{retain}] {topic} = {nutzlast}")

        sock.sendall(bytes([0xE0, 0x00]))  # DISCONNECT
        print(f"\n{anzahl} Nachrichten empfangen.")
        return 0 if anzahl else 2  # 2 = nichts gekommen, fuer Skripte unterscheidbar


if __name__ == "__main__":
    sys.exit(main())
