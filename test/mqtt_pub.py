#!/usr/bin/env python3
"""Minimaler MQTT-3.1.1-Publisher ohne externe Abhaengigkeiten.

Nur fuer den HeishaMon-Pruefstand: sendet mehrere Topics ueber EINE
Verbindung dicht hintereinander, damit sie im selben 500-ms-Sammelfenster
der Firmware landen.

  ./mqtt_pub.py --host 192.168.2.147 --check
  ./mqtt_pub.py --host 192.168.2.147 topic1=wert1 topic2=wert2
"""
import argparse
import socket
import struct
import sys
import time


def remaining_length(n):
    """MQTT variable-length encoding."""
    out = bytearray()
    while True:
        b = n % 128
        n //= 128
        if n > 0:
            b |= 0x80
        out.append(b)
        if n == 0:
            return bytes(out)


def mqtt_string(s):
    data = s.encode("utf-8")
    return struct.pack("!H", len(data)) + data


def build_connect(client_id, user=None, password=None, keepalive=30):
    flags = 0x02  # clean session
    payload = mqtt_string(client_id)
    if user:
        flags |= 0x80
        payload += mqtt_string(user)
        if password is not None:
            flags |= 0x40
            payload += mqtt_string(password)
    variable = mqtt_string("MQTT") + bytes([4, flags]) + struct.pack("!H", keepalive)
    body = variable + payload
    return bytes([0x10]) + remaining_length(len(body)) + body


def build_publish(topic, payload):
    # QoS 0, kein Retain: der Pruefstand soll nichts auf dem Broker hinterlassen
    body = mqtt_string(topic) + payload.encode("utf-8")
    return bytes([0x30]) + remaining_length(len(body)) + body


CONNACK_MEANING = {
    0: "Verbindung akzeptiert",
    1: "abgelehnt: Protokollversion nicht unterstuetzt",
    2: "abgelehnt: Client-ID zurueckgewiesen",
    3: "abgelehnt: Broker nicht verfuegbar",
    4: "abgelehnt: Benutzername/Passwort falsch",
    5: "abgelehnt: nicht autorisiert",
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", required=True)
    ap.add_argument("--port", type=int, default=1883)
    ap.add_argument("--user")
    ap.add_argument("--password")
    ap.add_argument("--client-id", default="heisha-pruefstand")
    ap.add_argument("--gap-ms", type=int, default=20,
                    help="Pause zwischen den Publishes (Default 20 ms)")
    ap.add_argument("--check", action="store_true",
                    help="nur verbinden und wieder trennen, nichts publizieren")
    ap.add_argument("pairs", nargs="*", metavar="topic=wert")
    args = ap.parse_args()

    if not args.check and not args.pairs:
        ap.error("entweder --check oder mindestens ein topic=wert angeben")

    try:
        sock = socket.create_connection((args.host, args.port), timeout=5)
    except OSError as e:
        print(f"FEHLER: keine Verbindung zu {args.host}:{args.port} - {e}")
        return 1

    with sock:
        sock.sendall(build_connect(args.client_id, args.user, args.password))
        connack = sock.recv(4)
        if len(connack) < 4 or connack[0] != 0x20:
            print(f"FEHLER: kein CONNACK erhalten (Antwort: {connack.hex()})")
            return 1
        code = connack[3]
        print(f"CONNACK {code}: {CONNACK_MEANING.get(code, 'unbekannter Code')}")
        if code != 0:
            return 1

        if args.check:
            print("Verbindung ok - nichts publiziert, keine Datenpunkte angelegt.")
            sock.sendall(bytes([0xE0, 0x00]))  # DISCONNECT
            return 0

        for pair in args.pairs:
            if "=" not in pair:
                print(f"FEHLER: '{pair}' ist kein topic=wert")
                return 1
            topic, value = pair.split("=", 1)
            sock.sendall(build_publish(topic, value))
            print(f"  -> {topic} = {value}")
            time.sleep(args.gap_ms / 1000.0)

        time.sleep(0.2)  # ausgehenden Puffer leeren lassen
        sock.sendall(bytes([0xE0, 0x00]))  # DISCONNECT
        print(f"{len(args.pairs)} Nachrichten gesendet (Abstand {args.gap_ms} ms).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
