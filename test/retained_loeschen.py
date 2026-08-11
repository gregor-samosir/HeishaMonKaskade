#!/usr/bin/env python3
"""Retained Messages von state-Topics loeschen, die es in der Firmware nicht mehr gibt.

Hintergrund: Die Firmware publiziert alle state-Topics mit Retain-Flag. Wird ein
Topic aus der Tabelle entfernt, hoert die Firmware zwar auf zu senden - der
Broker liefert den zuletzt gesendeten Wert aber weiter an jeden neuen Abonnenten
aus. Das Topic verschwindet also nicht, es friert auf seinem letzten Wert ein.
Geloescht wird eine Retained Message, indem man eine LEERE Nutzlast mit
Retain-Flag auf dasselbe Topic schickt.

Welche Topics entfallen sind, wird nicht von Hand gepflegt, sondern aus dem Code
ermittelt: Namen der stateTopics-Tabelle im Basisstand gegen die im
Arbeitsstand. Damit kann die Liste nicht veralten.

  ./retained_loeschen.py --basis v3.3.0              # nur anzeigen
  ./retained_loeschen.py --basis v3.3.0 --loeschen   # wirklich loeschen

REIHENFOLGE: erst die neue Firmware auf BEIDE Stufen flashen, dann loeschen.
Andersherum publiziert die noch laufende alte Firmware die Werte sofort wieder.

ACHTUNG - in DIESER Installation reicht das nicht:
Der Broker auf 192.168.2.147 ist der ioBroker-MQTT-Adapter im Server-Modus, kein
eigenstaendiger Broker (am 2026-08-11 nachgemessen). Er bedient neue Abonnenten
aus seiner Objektdatenbank und setzt dabei retain=0. Ein Loeschbefehl von hier
setzt den ioBroker-State also nur auf null - angekuendigt wird das Topic weiter,
solange sein Objekt unter mqtt.0.* existiert. Das eigentliche Aufraeumen ist
dort das Loeschen der Objekte in der Admin-Oberflaeche; eine Loeschschnittstelle
bietet die simple-api auf Port 8087 nicht. Einzelheiten in test/README.md.

Gegen einen echten Broker (mosquitto o. ae.) wirkt das Skript wie beschrieben.
"""
import argparse
import os
import re
import socket
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from mqtt_pub import build_connect, build_publish, read_connack  # noqa: E402

REPO = Path(__file__).resolve().parent.parent

# Beide Stufen: jede hat ihren eigenen Topic-Praefix auf demselben Broker
PREFIXE = ["panasonic_heat_pump", "panasonic_heat_pump2"]


def namen_aus_tabelle(quelltext):
    """Topic-Namen aus der stateTopics-Tabelle in decode.cpp herausziehen."""
    start = quelltext.find("stateTopics[NUMBEROFTOPICS] = {")
    if start < 0:
        return None  # Stand ohne Tabelle (vor 3.3.0)
    ende = quelltext.index("};", start)
    return re.findall(r'"([^"]+)"', quelltext[start:ende])


def namen_aus_parallel(quelltext_topics):
    """Fallback fuer Staende vor 3.3.0: Namen aus den States::TOPn in Topics.cpp."""
    return re.findall(r'States::TOP\d+\s*=\s*"([^"]+)"', quelltext_topics)


def basis_namen(revision):
    """Topic-Namen eines Git-Standes, egal ob mit oder ohne Tabelle."""
    def zeige(datei):
        r = subprocess.run(["git", "show", f"{revision}:src/{datei}"],
                           cwd=REPO, capture_output=True, text=True)
        if r.returncode != 0:
            sys.exit(f"FEHLER: {datei} in Revision '{revision}' nicht gefunden")
        return r.stdout

    namen = namen_aus_tabelle(zeige("decode.cpp"))
    if namen is None:
        namen = namen_aus_parallel(zeige("Topics.cpp"))
    return namen


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--basis", required=True,
                    help="Git-Revision VOR dem Entfernen (z. B. v3.3.0)")
    ap.add_argument("--broker", default="192.168.2.147")
    ap.add_argument("--port", type=int, default=1883)
    ap.add_argument("--prefix", action="append", default=None,
                    help=f"Topic-Praefix (mehrfach; Vorgabe: {', '.join(PREFIXE)})")
    ap.add_argument("--loeschen", action="store_true",
                    help="wirklich loeschen (ohne dieses Flag wird nur angezeigt)")
    args = ap.parse_args()
    prefixe = args.prefix or PREFIXE

    alt = basis_namen(args.basis)
    neu = namen_aus_tabelle((REPO / "src" / "decode.cpp").read_text())
    if neu is None:
        sys.exit("FEHLER: im Arbeitsstand ist keine stateTopics-Tabelle zu finden")

    entfallen = [n for n in alt if n not in neu]
    if not entfallen:
        print(f"Keine Topics entfallen zwischen '{args.basis}' und dem Arbeitsstand.")
        return 0

    print(f"== {len(entfallen)} Topics sind gegenueber '{args.basis}' entfallen ==")
    for n in entfallen:
        print(f"   {n}")
    print(f"\n== Betroffene Retained Messages: "
          f"{len(entfallen) * len(prefixe)} ({len(prefixe)} Praefixe) ==")

    if not args.loeschen:
        print("\nNur angezeigt. Zum Loeschen mit --loeschen erneut aufrufen.")
        print("VORHER die neue Firmware auf beide Stufen flashen, sonst")
        print("publiziert die alte Firmware die Werte sofort wieder.")
        return 0

    # EINE Verbindung fuer alle Publishes, Client-ID mit Prozess-ID: zwei Clients
    # mit derselben ID trennen sich gegenseitig, das hat hier schon einmal
    # Nachrichten gekostet (siehe test/README.md)
    client_id = f"heisha-retain-{os.getpid()}"
    try:
        sock = socket.create_connection((args.broker, args.port), timeout=10)
    except OSError as e:
        sys.exit(f"FEHLER: keine Verbindung zu {args.broker}:{args.port} - {e}")

    try:
        sock.settimeout(10)
        sock.sendall(build_connect(client_id))
        code = read_connack(sock)
        if code != 0:
            sys.exit(f"FEHLER: Broker hat die Verbindung abgelehnt (CONNACK {code})")

        gesendet = 0
        for prefix in prefixe:
            for name in entfallen:
                topic = f"{prefix}/state/{name}"
                # leere Nutzlast MIT Retain-Flag = Retained Message loeschen
                sock.sendall(build_publish(topic, "", retain=True))
                gesendet += 1
                time.sleep(0.02)  # Broker nicht in einem Rutsch zuschuetten
        # kurz warten, damit alles raus ist, bevor die Verbindung faellt
        time.sleep(0.5)
        print(f"\n{gesendet} Loeschbefehle gesendet.")
    finally:
        sock.close()

    print("\nNachkontrolle: mit einem frischen MQTT-Client abonnieren (z. B.")
    print(f"  mosquitto_sub -h {args.broker} -v -t '{prefixe[0]}/state/#')")
    print("- die entfallenen Topics duerfen dabei nicht mehr auftauchen.")
    print("Die Objekte unter mqtt.0.* im ioBroker bleiben davon unberuehrt und")
    print("muessen dort von Hand geloescht werden, wenn sie weg sollen.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
