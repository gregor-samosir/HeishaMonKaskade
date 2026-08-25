#!/usr/bin/env python3
"""Heiz- und Kuehlkurve aus dem ioBroker-Konfigurationsbaum in die WPs spiegeln.

Zweck ist der Notbetrieb: Faellt die Node-RED-Kaskadensteuerung aus, wird am
Bedienterminal von Direkt- auf Kurvenbetrieb umgeschaltet - dann muessen in der
Waermepumpe dieselben Kurvenwerte stehen, nach denen Node-RED sonst rechnet.

Quelle: 0_userdata.0.kaskade.Konfiguration.KK_Heizkurve / KK_Kuehlkurve
Ziel:   <prefix>/set/Z1HeatCurve* und Z1CoolCurve* (SET27-SET34)

Zuordnung: 'at' = Aussentemperatur-Stuetzpunkt, 'vl' = Vorlauf an diesem Punkt.
Im Konfigurationsbaum beziehen sich Hi/Lo IMMER auf die Aussentemperatur, bei
Panasonic dagegen nur die Outside_*-Felder - die Target_*-Felder benennen die
VORLAUFtemperatur. Beide Paare sind deshalb ueber Kreuz zugeordnet:

  atLo -> OutsideLow     vlLo (Vorlauf bei Kaelte) -> TargetHIGH
  atHi -> OutsideHigh    vlHi (Vorlauf bei Waerme) -> TargetLOW

Weil die Namen allein diese Kreuzung nicht zeigen, fuehrt jede Zeile der
Ausgabe ein Etikett mit, das keiner der beiden Konventionen folgt: VL kalt,
VL warm, AT kalt, AT warm (Kuehlseite: VL kuehl/heiss, AT kuehl/heiss). Es ist
dasselbe Vokabular wie in MQTT-Topics.md und im Node-RED-Sender.

Bis zum 2026-08-20 stand es hier andersherum, und die Kurve wurde entsprechend
verdreht gespiegelt. Belegt an WP1: bei 26-28 C aussen, also weit oberhalb
OutsideHigh, folgte Main_Target_Temp (TOP7) dem TargetLow - auch mit
vertauschten Werten (34/26 -> 26, 26/34 -> 34). Einzelheiten in
test/README.md, Abschnitt "Kurvenbetrieb: was die WP annimmt und was sie
verwirft".

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

# ACHTUNG - TargetHigh wird bewusst NICHT uebertragen:
#
# Z1HeatCurveTargetHighTemp (SET27, Byte 75) und Z1HeatRequestTemperature
# (SET5, Byte 38) sind in der Waermepumpe DERSELBE Wert, SOLANGE DER KREIS AUF
# DIREKTVORGABE STEHT. Am 2026-08-10 an WP1 in beide Richtungen belegt:
#   RequestTemp=20 gesendet          -> CurveTargetHigh sprang auf 20
#   beide zusammen (20 und 26)       -> BEIDE standen danach auf 26
# Fuer Z1CoolCurveTargetHighTemp / Z1CoolRequestTemperature gilt dasselbe.
#
# Diese beiden Werte hier mitzuschicken haette also zwei Folgen: Sie halten
# ohnehin nicht (der naechste Re-Assert des Verteilers ueberschreibt sie),
# und schlimmer - sie verstellen im Direktbetrieb den AKTIVEN Sollwert der
# Anlage. Beim ersten Lauf am 2026-08-10 wurde so der Kuehl-Sollwert fuer
# einige Minuten von 20 auf 19 gezogen.
#
# IM KURVENBETRIEB IST ES EINE EIGENE SPEICHERSTELLE (2026-08-20 gemessen):
# dort ist der Benutzerwert die Parallelverschiebung (-5..+5), und TargetHigh
# laesst sich sauber schreiben. Genau deshalb ist die Reihenfolge des
# Notbetriebs "erst umschalten, dann Kurve schreiben" - siehe
# Vorhaben-Notbetrieb-Weboberflaeche.md, Abschnitt 2.
#
# Die Kurve besteht fuer dieses Werkzeug daher aus TargetLow und den beiden
# Aussentemperatur-Stuetzpunkten. DER FEHLENDE WERT IST DER VORLAUF BEI KAELTE
# (KK_HK_vlLo) - also ausgerechnet der, auf den es im Notbetrieb ankommt. Wer
# heute von Hand am Bedienterminal auf Kurve umschaltet, muss ihn dort
# nachtragen; sonst bleibt die Panasonic-Werksvorgabe von 55 C stehen.

# (Konfig-Gruppe, Konfig-Schluessel, Etikett) -> (Set-Topic, state-Topic, min, max)
# Das Etikett steht in jeder Ausgabezeile und sagt, bei welchem Wetter der Wert
# gilt - es ueberbrueckt die Kreuzung zwischen Konfigbaum und Panasonic-Namen.
MAPPING = [
    # vlHi = Vorlauf bei HOHER Aussentemperatur -> Panasonics TargetLOW.
    ("KK_Heizkurve", "KK_HK_vlHi", "VL warm", "Z1HeatCurveTargetLowTemp",
     "Z1_Heat_Curve_Target_Low_Temp", 20, 55),
    ("KK_Heizkurve", "KK_HK_atLo", "AT kalt", "Z1HeatCurveOutsideLowTemp",
     "Z1_Heat_Curve_Outside_Low_Temp", -15, 15),
    ("KK_Heizkurve", "KK_HK_atHi", "AT warm", "Z1HeatCurveOutsideHighTemp",
     "Z1_Heat_Curve_Outside_High_Temp", -15, 15),
    ("KK_Kühlkurve", "KK_HK_vlHi", "VL heiss", "Z1CoolCurveTargetLowTemp",
     "Z1_Cool_Curve_Target_Low_Temp", 5, 20),
    ("KK_Kühlkurve", "KK_HK_atLo", "AT kuehl", "Z1CoolCurveOutsideLowTemp",
     "Z1_Cool_Curve_Outside_Low_Temp", 15, 30),
    # Bereiche der beiden OutsideHigh-Parameter an der Anlage ausgemessen
    # (s. kurven_grenzen.py): Heat -15..15, Cool 15..30. Werte darueber oder
    # darunter klemmt die WP still. Ein Konfigurationswert ausserhalb wird
    # hier deshalb als "AUSSERHALB" gemeldet statt wirkungslos gesendet.
    ("KK_Kühlkurve", "KK_HK_atHi", "AT heiss", "Z1CoolCurveOutsideHighTemp",
     "Z1_Cool_Curve_Outside_High_Temp", 15, 30),
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


def als_zahl(roh):
    """Konfigurationswert als int, oder None wenn er fehlt/keine Zahl ist."""
    try:
        return int(roh)
    except (TypeError, ValueError):
        return None


def kurve_pruefen(konf):
    """Zeigen die Kurven in die richtige Richtung?

    Dieselbe Regel wie notbetrieb_kurve_pruefen() in src/notbetrieb.h: Eine
    Kurve faellt mit steigender Aussentemperatur, also VL kalt >= VL warm.
    Gleichheit ist erlaubt (flache Vorgabe), die Aussenpunkte muessen dagegen
    echt auseinanderliegen - zwei Stuetzpunkte auf derselben Temperatur ergeben
    keine Kurve.

    Der Sinn: Alle vier Werte koennen einzeln im erlaubten Bereich liegen und
    trotzdem eine verdrehte Kurve ergeben - genau das ist bis zum 2026-08-20
    passiert, weil vlHi/vlLo und Panasonics TargetHigh/Low ueber Kreuz liegen.
    Kein Bereichstest kann das finden.

    Rueckgabe: Liste von Klartextmeldungen, leer wenn alles plausibel ist.
    Fehlende Werte werden uebersprungen - sie sind schon in der Tabelle darueber
    als FEHLER gemeldet.
    """
    meldungen = []
    for gruppe, vl_bez, at_bez in (("KK_Heizkurve", ("VL kalt", "VL warm"),
                                    ("AT kalt", "AT warm")),
                                   ("KK_Kühlkurve", ("VL kuehl", "VL heiss"),
                                    ("AT kuehl", "AT heiss"))):
        vl_kalt = als_zahl(konf.get(f"{KONFIG}.{gruppe}.KK_HK_vlLo"))
        vl_warm = als_zahl(konf.get(f"{KONFIG}.{gruppe}.KK_HK_vlHi"))
        at_kalt = als_zahl(konf.get(f"{KONFIG}.{gruppe}.KK_HK_atLo"))
        at_warm = als_zahl(konf.get(f"{KONFIG}.{gruppe}.KK_HK_atHi"))

        if vl_kalt is not None and vl_warm is not None and vl_kalt < vl_warm:
            meldungen.append(
                f"{gruppe}: {vl_bez[0]} ({vl_kalt}) liegt unter {vl_bez[1]} "
                f"({vl_warm}) - sind KK_HK_vlLo und KK_HK_vlHi vertauscht?")
        if at_kalt is not None and at_warm is not None and at_kalt >= at_warm:
            meldungen.append(
                f"{gruppe}: {at_bez[0]} ({at_kalt}) liegt nicht unter "
                f"{at_bez[1]} ({at_warm})")
    return meldungen


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--iobroker", default="192.168.2.147")
    ap.add_argument("--broker", default="192.168.2.147")
    ap.add_argument("--prefix", action="append",
                    help="MQTT-Prefix, mehrfach moeglich "
                         "(Vorgabe: beide Stufen)")
    ap.add_argument("--dry-run", action="store_true",
                    help="nur anzeigen, nichts senden")
    ap.add_argument("--kurve-ignorieren", action="store_true",
                    help="trotz unplausibler Kurve senden (siehe kurve_pruefen)")
    args = ap.parse_args()
    prefixe = args.prefix or ["panasonic_heat_pump", "panasonic_heat_pump2"]

    # Konfiguration lesen. Die beiden KK_HK_vlLo stehen nicht im MAPPING
    # (TargetHigh wird nicht gespiegelt), werden aber mitgelesen: Der
    # Schlusshinweis beziffert damit den nachzutragenden Wert statt nur auf ihn
    # zu verweisen, und kurve_pruefen() braucht beide Kurvenpunkte, um die
    # Richtung ueberhaupt beurteilen zu koennen.
    ids = [f"{KONFIG}.{g}.{k}" for g, k, *_ in MAPPING]
    ids.append(f"{KONFIG}.KK_Heizkurve.KK_HK_vlLo")
    ids.append(f"{KONFIG}.KK_Kühlkurve.KK_HK_vlLo")
    konf = hole(args.iobroker, ids)
    vorlauf_kalt = konf.get(f"{KONFIG}.KK_Heizkurve.KK_HK_vlLo")

    plan = []
    fehler = False
    print(f"{'Konfiguration':<32}{'Wert':>6}   {'gilt bei':<10}"
          f"{'-> Set-Topic':<32}{'Bereich':>12}")
    print("-" * 96)
    for gruppe, schluessel, etikett, set_topic, _state, lo, hi in MAPPING:
        roh = konf.get(f"{KONFIG}.{gruppe}.{schluessel}")
        if roh is None:
            print(f"{gruppe+'.'+schluessel:<32}{'fehlt':>6}   {etikett:<10}"
                  f"FEHLER: nicht gefunden")
            fehler = True
            continue
        try:
            wert = int(roh)
        except (TypeError, ValueError):
            print(f"{gruppe+'.'+schluessel:<32}{str(roh):>6}   {etikett:<10}"
                  f"FEHLER: keine ganze Zahl")
            fehler = True
            continue
        ok = lo <= wert <= hi
        marker = "" if ok else "   AUSSERHALB!"
        if not ok:
            fehler = True
        print(f"{gruppe+'.'+schluessel:<32}{wert:>6}   {etikett:<10}{set_topic:<32}"
              f"{str(lo)+'..'+str(hi):>12}{marker}")
        plan.append((set_topic, wert))

    if fehler:
        print("\nAbbruch: mindestens ein Wert fehlt oder liegt ausserhalb des "
              "erlaubten Bereichs.")
        return 1

    # Bereiche sagen nur, ob ein Wert erlaubt ist - nicht, ob die vier
    # zusammen eine sinnvolle Kurve ergeben. Deshalb hier der Richtungstest.
    unplausibel = kurve_pruefen(konf)
    if unplausibel:
        print("\nDie Kurve sieht verdreht aus:")
        for zeile in unplausibel:
            print(f"  - {zeile}")
        if not args.kurve_ignorieren:
            print("\nAbbruch. Eine verdrehte Kurve gespiegelt heisst: Die "
                  "Anlage faehrt im")
            print("Notbetrieb nach der falschen Kurve. Wenn das so gewollt "
                  "ist, mit")
            print("--kurve-ignorieren erneut starten.")
            return 1
        print("\n--kurve-ignorieren: wird trotzdem gesendet.")
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
        ids = [f"mqtt.0.{prefix}.state.{state_topic}"
               for _g, _k, _e, _s, state_topic, _lo, _hi in MAPPING]
        ist = hole(args.iobroker, ids)
        for (_g, _k, etikett, _set, state_topic, _lo, _hi), (_st, soll) in zip(MAPPING, plan):
            wert = ist.get(f"mqtt.0.{prefix}.state.{state_topic}")
            ok = str(wert) == str(soll)
            gesamt_ok &= ok
            print(f"  {state_topic:<34}{str(wert):>6}   {etikett:<10}"
                  f"{'ok' if ok else 'erwartet ' + str(soll)}")
    print("\n" + "-" * 60)
    print("Alle Kurvenwerte uebernommen." if gesamt_ok
          else "ACHTUNG: mindestens ein Wert weicht ab - Log der WP pruefen.")
    # Der fehlende vierte Wert wird beziffert, nicht nur benannt - wer im
    # Notfall vor dem Bedienterminal steht, braucht die Zahl, nicht den Hinweis.
    kalt = f"{vorlauf_kalt} C" if vorlauf_kalt is not None else "KK_HK_vlLo"
    print("\nHinweis: TargetHigh - das ist 'VL kalt' - wird bewusst nicht")
    print("uebertragen. Im Direktbetrieb teilt er sich mit der Vorlauf-")
    print("Solltemperatur eine Speicherstelle und traegt daher immer den")
    print("zuletzt gesendeten Direktwert.")
    print(f"Nach dem Umschalten auf Kurvenbetrieb ist er auf {kalt} zu setzen -")
    print("am Bedienterminal oder per set/Z1HeatCurveTargetHighTemp, das geht")
    print("im Kurvenbetrieb sauber durch. Sonst bleibt die Panasonic-")
    print("Werksvorgabe von 55 C stehen, und die Anlage heizt viel zu heiss.")
    return 0 if gesamt_ok else 1


if __name__ == "__main__":
    sys.exit(main())
