#!/usr/bin/env python3
"""Nachweis der Kurven-Set-Kommandos SET27-SET34 am laufenden Geraet.

ACHTUNG: Zwei der acht Werte (Z1HeatCurveTargetHighTemp,
Z1CoolCurveTargetHighTemp) teilen sich in der WP eine Speicherstelle mit der
aktiven Vorlauf-Solltemperatur (Z1Heat/Z1CoolRequestTemperature) - solange der
jeweilige Kreis auf Direktvorgabe steht; im Kurvenbetrieb sind es getrennte
Speicherstellen (2026-08-20 gemessen). Weil dieses Werkzeug ausschliesslich die
Ist-Werte zurueckschreibt, aendert es trotzdem nichts - aber mit anderen Werten
waere es ein Eingriff in den laufenden Betrieb.

Sicherheitsprinzip: Das Werkzeug erfindet keine Werte. Es liest die aktuell in
der Waermepumpe hinterlegten Kurvenwerte aus den state-Topics und schreibt
genau diese zurueck. An der Anlage aendert sich dadurch nichts - geprueft wird
nur, ob die acht Werte korrekt codiert im Telegramm landen (und gemeinsam in
einem, dank Sammelfenster und Bitmasken-Merge).

  ./kurven_test.py --esp 192.168.2.120 --prefix panasonic_heat_pump
  ./kurven_test.py --esp 192.168.2.193 --prefix panasonic_heat_pump2
"""
import argparse
import json
import socket
import sys
import time
import urllib.request
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from heisha_probe import (drain, parse_all_command_telegrams,  # noqa: E402
                          telnet_connect)
from mqtt_pub import build_connect, build_publish  # noqa: E402

# Set-Topic -> (state-Topic zum Rueckvergleich, Byte im Telegramm)
KURVE = [
    ("Z1HeatCurveTargetHighTemp",  "Z1_Heat_Curve_Target_High_Temp",  75),
    ("Z1HeatCurveTargetLowTemp",   "Z1_Heat_Curve_Target_Low_Temp",   76),
    ("Z1HeatCurveOutsideLowTemp",  "Z1_Heat_Curve_Outside_Low_Temp",  77),
    ("Z1HeatCurveOutsideHighTemp", "Z1_Heat_Curve_Outside_High_Temp", 78),
    ("Z1CoolCurveTargetHighTemp",  "Z1_Cool_Curve_Target_High_Temp",  86),
    ("Z1CoolCurveTargetLowTemp",   "Z1_Cool_Curve_Target_Low_Temp",   87),
    ("Z1CoolCurveOutsideLowTemp",  "Z1_Cool_Curve_Outside_Low_Temp",  88),
    ("Z1CoolCurveOutsideHighTemp", "Z1_Cool_Curve_Outside_High_Temp", 89),
]


def ist_werte(iobroker, prefix):
    """Aktuelle Kurvenwerte aus den state-Topics holen."""
    ids = ",".join(f"mqtt.0.{prefix}.state.{st}" for _, st, _ in KURVE)
    with urllib.request.urlopen(f"http://{iobroker}:8087/getBulk/{ids}",
                                timeout=10) as r:
        daten = json.load(r)
    werte = {}
    for e in daten:
        werte[e.get("id", "").split(".state.")[-1]] = e.get("val")
    return werte


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--esp", default="192.168.2.120")
    ap.add_argument("--prefix", default="panasonic_heat_pump")
    ap.add_argument("--broker", default="192.168.2.147")
    ap.add_argument("--iobroker", default="192.168.2.147")
    args = ap.parse_args()

    print(f"== Ist-Werte aus mqtt.0.{args.prefix}.state.* lesen ==")
    werte = ist_werte(args.iobroker, args.prefix)
    plan = []
    for set_topic, state_topic, pos in KURVE:
        v = werte.get(state_topic)
        if v is None:
            print(f"FEHLER: {state_topic} nicht gefunden - laeuft die WP?")
            return 1
        try:
            v = int(v)
        except (TypeError, ValueError):
            print(f"FEHLER: {state_topic} ist kein ganzzahliger Wert: {v!r}")
            return 1
        plan.append((set_topic, v, pos))
        print(f"  {state_topic:<34}{v:>5}")

    captured = []
    print(f"\n== Telnet {args.esp}:23, Hexlog ein ==")
    sock = telnet_connect(args.esp)
    with sock:
        drain(sock, 2.0, captured)
        sock.sendall(b"H")
        drain(sock, 1.5, captured)

        print("== Dieselben Werte zurueckschreiben (keine Aenderung) ==")
        mq = socket.create_connection((args.broker, 1883), timeout=5)
        with mq:
            mq.sendall(build_connect("heisha-kurventest"))
            if mq.recv(4)[3] != 0:
                print("FEHLER: MQTT abgelehnt")
                return 1
            for set_topic, wert, _ in plan:
                mq.sendall(build_publish(f"{args.prefix}/set/{set_topic}",
                                         str(wert)))
                print(f"  -> {set_topic} = {wert}")
                time.sleep(0.02)
            time.sleep(0.3)
            mq.sendall(bytes([0xE0, 0x00]))

        drain(sock, 6.0, captured, stop_marker="Send command")
        print("== Hexlog aus ==")
        sock.sendall(b"H")
        drain(sock, 1.0, captured)

    text = "".join(captured)
    tels = parse_all_command_telegrams(text)
    if not tels:
        print("\nFEHLER: kein Kommandotelegramm mitgeschnitten")
        print(text[-1200:])
        return 1

    tel = tels[-1]
    print(f"\n{len(tels)} Telegramm(e) mitgeschnitten, ausgewertet wird das letzte")
    print("=" * 74)
    print(f"{'Set-Topic':<30}{'Wert':>6}{'Byte':>6}{'erwartet':>10}{'gelesen':>9}   ")
    print("-" * 74)
    alle_ok = True
    for set_topic, wert, pos in plan:
        erwartet = wert + 128  # alle acht nutzen CONV_ADD 128
        ist = int(tel[pos], 16)
        ok = ist == erwartet
        alle_ok &= ok
        print(f"{set_topic:<30}{wert:>6}{pos:>6}{erwartet:>10}{ist:>9}   "
              f"{'ok' if ok else 'FEHLT'}")
    print("=" * 74)
    if alle_ok and len(tels) == 1:
        print("BESTANDEN: alle acht Kurvenwerte korrekt codiert, in EINEM Telegramm.")
    elif alle_ok:
        print(f"Alle Werte korrekt codiert, verteilt auf {len(tels)} Telegramme "
              "(Sammelfenster war zu kurz - kein Fehler).")
    else:
        print("DURCHGEFALLEN: mindestens ein Byte fehlt oder ist falsch.")
    for line in text.splitlines():
        if "Field conflict" in line or "Error:" in line:
            print("  !! " + line.strip())
    return 0 if alle_ok else 1


if __name__ == "__main__":
    sys.exit(main())
