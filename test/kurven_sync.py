#!/usr/bin/env python3
"""Heiz- und Kuehlkurve aus dem ioBroker-Konfigurationsbaum in die WPs spiegeln.

Zweck ist der Notbetrieb: Faellt die Node-RED-Kaskadensteuerung aus, wird am
Bedienterminal von Direkt- auf Kurvenbetrieb umgeschaltet - dann muessen in der
Waermepumpe dieselben Kurvenwerte stehen, nach denen Node-RED sonst rechnet.

Quelle: 0_userdata.0.kaskade.Konfiguration.KK_Heizkurve / KK_Kuehlkurve
Ziel:   <prefix>/set/Z1HeatCurve* und Z1CoolCurve* (SET27-SET34)

Zuordnung: 'at' = Aussentemperatur-Stuetzpunkt, 'vl' = Vorlauf an diesem Punkt.
Hi/Lo beziehen sich bei beiden Kurven auf die AUSSENtemperatur, genau wie bei
Panasonics Outside_High/Outside_Low - deshalb geht atHi -> OutsideHigh und
vlHi -> TargetHigh.

Nicht uebertragen werden KK_HK_vlMin/vlMax/Para: Das sind Begrenzungen der
Node-RED-Berechnung, die Panasonic-Kurve kennt keine Entsprechung dazu.

  ./kurven_sync.py --dry-run          # nur anzeigen, nichts schreiben
  ./kurven_sync.py                    # an beide Stufen senden
  ./kurven_sync.py --prefix panasonic_heat_pump
"""
import argparse
import json
import socket
import sys
import time
import urllib.parse
import urllib.request
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from mqtt_pub import build_connect, build_publish  # noqa: E402

KONFIG = "0_userdata.0.kaskade.Konfiguration"

# (Konfig-Gruppe, Konfig-Schluessel) -> (Set-Topic, state-Topic, min, max)
MAPPING = [
    ("KK_Heizkurve", "KK_HK_vlHi", "Z1HeatCurveTargetHighTemp",
     "Z1_Heat_Curve_Target_High_Temp", 20, 55),
    ("KK_Heizkurve", "KK_HK_vlLo", "Z1HeatCurveTargetLowTemp",
     "Z1_Heat_Curve_Target_Low_Temp", 20, 55),
    ("KK_Heizkurve", "KK_HK_atLo", "Z1HeatCurveOutsideLowTemp",
     "Z1_Heat_Curve_Outside_Low_Temp", -15, 15),
    ("KK_Heizkurve", "KK_HK_atHi", "Z1HeatCurveOutsideHighTemp",
     "Z1_Heat_Curve_Outside_High_Temp", 15, 35),
    ("KK_Kühlkurve", "KK_HK_vlHi", "Z1CoolCurveTargetHighTemp",
     "Z1_Cool_Curve_Target_High_Temp", 5, 20),
    ("KK_Kühlkurve", "KK_HK_vlLo", "Z1CoolCurveTargetLowTemp",
     "Z1_Cool_Curve_Target_Low_Temp", 5, 20),
    ("KK_Kühlkurve", "KK_HK_atLo", "Z1CoolCurveOutsideLowTemp",
     "Z1_Cool_Curve_Outside_Low_Temp", 20, 30),
    ("KK_Kühlkurve", "KK_HK_atHi", "Z1CoolCurveOutsideHighTemp",
     "Z1_Cool_Curve_Outside_High_Temp", 30, 40),
]


def hole(iobroker, ids):
    # Jede ID einzeln kodieren, die Kommas dazwischen roh lassen: quote() ueber
    # die fertige Kette wuerde auch die Kommas ersetzen, und die API liest das
    # Ganze dann als EINE Objekt-ID (Umlaut in KK_Kuehlkurve braucht die
    # Kodierung, sonst scheitert schon urllib an Nicht-ASCII).
    kette = ",".join(urllib.parse.quote(i) for i in ids)
    with urllib.request.urlopen(f"http://{iobroker}:8087/getBulk/{kette}",
                                timeout=10) as r:
        return {e.get("id"): e.get("val") for e in json.load(r)}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--iobroker", default="192.168.2.147")
    ap.add_argument("--broker", default="192.168.2.147")
    ap.add_argument("--prefix", action="append",
                    help="MQTT-Prefix, mehrfach moeglich "
                         "(Vorgabe: beide Stufen)")
    ap.add_argument("--dry-run", action="store_true",
                    help="nur anzeigen, nichts senden")
    args = ap.parse_args()
    prefixe = args.prefix or ["panasonic_heat_pump", "panasonic_heat_pump2"]

    # Konfiguration lesen
    ids = [f"{KONFIG}.{g}.{k}" for g, k, *_ in MAPPING]
    konf = hole(args.iobroker, ids)

    plan = []
    fehler = False
    print(f"{'Konfiguration':<32}{'Wert':>6}   {'-> Set-Topic':<32}{'Bereich':>12}")
    print("-" * 86)
    for gruppe, schluessel, set_topic, _state, lo, hi in MAPPING:
        roh = konf.get(f"{KONFIG}.{gruppe}.{schluessel}")
        if roh is None:
            print(f"{gruppe+'.'+schluessel:<32}{'fehlt':>6}   FEHLER: nicht gefunden")
            fehler = True
            continue
        try:
            wert = int(roh)
        except (TypeError, ValueError):
            print(f"{gruppe+'.'+schluessel:<32}{str(roh):>6}   FEHLER: keine ganze Zahl")
            fehler = True
            continue
        ok = lo <= wert <= hi
        marker = "" if ok else "   AUSSERHALB!"
        if not ok:
            fehler = True
        print(f"{gruppe+'.'+schluessel:<32}{wert:>6}   {set_topic:<32}"
              f"{str(lo)+'..'+str(hi):>12}{marker}")
        plan.append((set_topic, wert))

    if fehler:
        print("\nAbbruch: mindestens ein Wert fehlt oder liegt ausserhalb des "
              "erlaubten Bereichs.")
        return 1
    if args.dry_run:
        print("\n--dry-run: nichts gesendet.")
        return 0

    for prefix in prefixe:
        print(f"\n== Sende an {prefix} ==")
        mq = socket.create_connection((args.broker, 1883), timeout=5)
        with mq:
            mq.sendall(build_connect(f"heisha-kurvensync-{prefix[-1]}"))
            if mq.recv(4)[3] != 0:
                print("  FEHLER: MQTT abgelehnt")
                return 1
            for set_topic, wert in plan:
                mq.sendall(build_publish(f"{prefix}/set/{set_topic}", str(wert)))
                print(f"  -> {set_topic} = {wert}")
                time.sleep(0.02)
            time.sleep(0.3)
            mq.sendall(bytes([0xE0, 0x00]))

    # Rueckvergleich ueber die state-Topics
    print("\n== Warte 15 s auf die Rueckmeldung der Waermepumpen ==")
    time.sleep(15)
    gesamt_ok = True
    for prefix in prefixe:
        print(f"\n{prefix}:")
        ids = [f"mqtt.0.{prefix}.state.{st}" for *_, st, _lo, _hi
               in [(g, k, s, st, lo, hi) for g, k, s, st, lo, hi in MAPPING]]
        ist = hole(args.iobroker, ids)
        for (_g, _k, set_topic, state_topic, _lo, _hi), (_st, soll) in zip(MAPPING, plan):
            wert = ist.get(f"mqtt.0.{prefix}.state.{state_topic}")
            ok = str(wert) == str(soll)
            gesamt_ok &= ok
            print(f"  {state_topic:<34}{str(wert):>6}   "
                  f"{'ok' if ok else 'erwartet ' + str(soll)}")
    print("\n" + "-" * 60)
    print("Alle Kurvenwerte uebernommen." if gesamt_ok
          else "ACHTUNG: mindestens ein Wert weicht ab - Log der WP pruefen.")
    return 0 if gesamt_ok else 1


if __name__ == "__main__":
    sys.exit(main())
