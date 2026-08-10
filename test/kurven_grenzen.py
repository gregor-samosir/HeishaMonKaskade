#!/usr/bin/env python3
"""Ermittelt, welche Kurvenwerte die Waermepumpe tatsaechlich annimmt.

Hintergrund: Bei Z1CoolCurveOutsideHighTemp kam heraus, dass die WP alles
ueber 30 still auf 30 klemmt - ohne Fehlermeldung. Die in Umlauf befindlichen
Wertebereiche stimmen fuer diese Geraete also nicht durchgaengig. Dieses
Werkzeug prueft je Parameter die beiden Raender des in der Firmware
hinterlegten Bereichs: senden, warten, ueber das state-Topic zurueckvergleichen.

NUR laufen lassen, wenn die Anlage NICHT im Kurvenbetrieb faehrt (im
Direktmodus sind diese Werte wirkungslos). Am Ende werden die Ausgangswerte
wiederhergestellt.

ACHTUNG - Z1HeatCurveTargetHighTemp und Z1CoolCurveTargetHighTemp teilen sich
in der WP eine Speicherstelle mit Z1HeatRequestTemperature bzw.
Z1CoolRequestTemperature, also mit der aktiven Vorlauf-Solltemperatur. Dieser
Test verstellt beim Pruefen ihrer Raender damit den ECHTEN Sollwert der Anlage
(im Heizbetrieb bis 55 Grad!). Nur bei abgeschaltetem Kompressor laufen lassen.

ACHTUNG - danach IMMER den kompletten Kurvensatz kontrollieren, nicht nur die
getesteten Parameter: Am 2026-08-10 standen nach dem Lauf zwei Werte auf
anderen Zahlen als vor dem vorangegangenen kurven_sync.py, obwohl sie dort
bestaetigt worden waren und dazwischen niemand sie gesetzt hat
(Heat_Curve_Target_High 26 -> 20, Cool_Curve_Target_High 19 -> 20, auf beiden
Geraeten gleich). Ursache ungeklaert; ein Nachtest zeigte, dass die WP frisch
gesetzte Werte ueber 100 s NICHT von sich aus zurueckdreht. Denkbar ist, dass
die absurden Zwischenkombinationen dieses Tests (z.B. Target_High 55 bei
Target_Low 20) eine interne Korrektur ausloesen. Also: nach dem Lauf
`kurven_sync.py --dry-run` und einen Rueckvergleich fahren.

  ./kurven_grenzen.py --esp 192.168.2.120 --prefix panasonic_heat_pump
"""
import argparse
import json
import os
import socket
import sys
import time
import urllib.parse
import urllib.request
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from mqtt_pub import build_connect, build_publish  # noqa: E402

# Set-Topic, state-Topic, min, max (Bereiche aus commands.cpp)
PARAMETER = [
    ("Z1HeatCurveTargetHighTemp",  "Z1_Heat_Curve_Target_High_Temp",  20, 55),
    ("Z1HeatCurveTargetLowTemp",   "Z1_Heat_Curve_Target_Low_Temp",   20, 55),
    ("Z1HeatCurveOutsideLowTemp",  "Z1_Heat_Curve_Outside_Low_Temp",  -15, 15),
    ("Z1HeatCurveOutsideHighTemp", "Z1_Heat_Curve_Outside_High_Temp", -15, 15),
    ("Z1CoolCurveTargetHighTemp",  "Z1_Cool_Curve_Target_High_Temp",  5, 20),
    ("Z1CoolCurveTargetLowTemp",   "Z1_Cool_Curve_Target_Low_Temp",   5, 20),
    ("Z1CoolCurveOutsideLowTemp",  "Z1_Cool_Curve_Outside_Low_Temp",  20, 30),
    ("Z1CoolCurveOutsideHighTemp", "Z1_Cool_Curve_Outside_High_Temp", 15, 30),
]


def lies(iobroker, prefix, state_topic):
    oid = f"mqtt.0.{prefix}.state.{state_topic}"
    url = f"http://{iobroker}:8087/get/{urllib.parse.quote(oid)}"
    with urllib.request.urlopen(url, timeout=10) as r:
        return json.load(r).get("val")


def sende(broker, prefix, paare):
    """Eine oder mehrere Nachrichten ueber EINE Verbindung schicken.

    Bewusst eine Verbindung fuer alle Paare: Ein zweiter MQTT-Client mit
    derselben Client-ID trennt laut Spezifikation den ersten. Wurde je Wert
    neu verbunden, gingen bei schnell aufeinanderfolgenden Reconnects
    Nachrichten verloren - genau daran scheiterte am 2026-08-10 die
    Wiederherstellung eines Kurvenwerts (blieb auf dem Testwert 55 stehen).
    Die Client-ID traegt zusaetzlich die Prozess-ID, damit parallel laufende
    Werkzeuge sich nicht gegenseitig abmelden.
    """
    mq = socket.create_connection((broker, 1883), timeout=5)
    with mq:
        mq.sendall(build_connect(f"heisha-grenztest-{os.getpid()}"))
        if mq.recv(4)[3] != 0:
            raise RuntimeError("MQTT abgelehnt")
        for set_topic, wert in paare:
            mq.sendall(build_publish(f"{prefix}/set/{set_topic}", str(wert)))
            time.sleep(0.05)
        time.sleep(0.3)
        mq.sendall(bytes([0xE0, 0x00]))


def probe(args, set_topic, state_topic, wert):
    """Einen Wert setzen und zurueckvergleichen."""
    sende(args.broker, args.prefix, [(set_topic, wert)])
    time.sleep(args.wartezeit)
    return lies(args.iobroker, args.prefix, state_topic)


def stelle_wieder_her(args, original, versuche=3):
    """Ausgangswerte zurueckschreiben und das Ergebnis pruefen.

    Wird so lange wiederholt, bis alle Werte stimmen - eine Wiederherstellung,
    die man nur einmal absendet und danach bloss meldet, ist keine.
    """
    for versuch in range(1, versuche + 1):
        offen = []
        for set_topic, state_topic, *_ in PARAMETER:
            ist = lies(args.iobroker, args.prefix, state_topic)
            if str(ist) != str(original[set_topic]):
                offen.append((set_topic, original[set_topic]))
        if not offen:
            return []
        print(f"  Versuch {versuch}: {len(offen)} Wert(e) zurueckschreiben")
        sende(args.broker, args.prefix, offen)
        time.sleep(args.wartezeit)
    # letzte Kontrolle
    return [(st, original[st], lies(args.iobroker, args.prefix, stt))
            for st, stt, *_ in PARAMETER
            if str(lies(args.iobroker, args.prefix, stt)) != str(original[st])]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--esp", default="192.168.2.120")
    ap.add_argument("--prefix", default="panasonic_heat_pump")
    ap.add_argument("--broker", default="192.168.2.147")
    ap.add_argument("--iobroker", default="192.168.2.147")
    ap.add_argument("--wartezeit", type=int, default=14,
                    help="Sekunden bis zum Rueckvergleich")
    args = ap.parse_args()

    print(f"== Ausgangswerte sichern ({args.prefix}) ==")
    original = {}
    for set_topic, state_topic, *_ in PARAMETER:
        original[set_topic] = lies(args.iobroker, args.prefix, state_topic)
        print(f"  {state_topic:<34}{str(original[set_topic]):>6}")

    print(f"\n== Raender pruefen (je {args.wartezeit} s Wartezeit) ==")
    print(f"{'Parameter':<30}{'Rand':>6}{'gesendet':>10}{'gemeldet':>10}   Ergebnis")
    print("-" * 78)
    befunde = []
    for set_topic, state_topic, lo, hi in PARAMETER:
        for name, wert in (("min", lo), ("max", hi)):
            ist = probe(args, set_topic, state_topic, wert)
            ok = str(ist) == str(wert)
            if not ok:
                befunde.append((set_topic, name, wert, ist))
            print(f"{set_topic:<30}{name:>6}{wert:>10}{str(ist):>10}   "
                  f"{'ok' if ok else 'GEKLEMMT'}")

    print("\n== Ausgangswerte wiederherstellen ==")
    rest = stelle_wieder_her(args, original)
    for set_topic, state_topic, *_ in PARAMETER:
        ist = lies(args.iobroker, args.prefix, state_topic)
        ok = str(ist) == str(original[set_topic])
        print(f"  {state_topic:<34}{str(ist):>6}   "
              f"{'wiederhergestellt' if ok else 'ABWEICHUNG, erwartet ' + str(original[set_topic])}")
    fehler = len(rest)

    print("\n" + "=" * 78)
    if befunde:
        print("Die WP nimmt folgende Randwerte NICHT an:")
        for set_topic, rand, wert, ist in befunde:
            print(f"  {set_topic}: {rand}={wert} -> gemeldet {ist}")
        print("\nBereiche in commands.cpp entsprechend anpassen.")
    else:
        print("Alle Randwerte wurden uebernommen - die Bereiche sind haltbar.")
    if fehler:
        print(f"\nACHTUNG: {fehler} Ausgangswert(e) nicht wiederhergestellt!")
    print("=" * 78)
    return 1 if (befunde or fehler) else 0


if __name__ == "__main__":
    sys.exit(main())
