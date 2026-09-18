#!/usr/bin/env python3
"""Prueft Byte-Zuordnung.md gegen den Code - und ruft die Pruefung von
SET-TOP-Zuordnung.md mit auf.

Warum es diesen Test gibt
-------------------------
Byte-Zuordnung.md fuehrt jedes Set-Kommando und jedes State-Topic an seiner
Byte-Position. Kommt ein Topic dazu, faellt eines weg oder aendert sich ein
Dekodierer, stimmt die Datei nicht mehr - und niemand merkt es, weil eine
veraltete Tabelle genauso aussieht wie eine richtige. Anlass war TOP66
(2026-09-18): Die Frage, welcher Faktor fuer Byte 164 gilt, haette die
Kodierungsangabe in der Bemerkung still veralten lassen, sobald jemand den
Dekodierer tauscht.

SET-TOP-Zuordnung.md hatte bis hierher eine Pruefung, die nur lief, wenn jemand
sie aufrief (set_top_zuordnung.py --pruefen). Hier laeuft sie mit, damit beide
Dateien in den Hosttests stehen - lokal vor dem Merge und in der CI.

Was geprueft wird
-----------------
Die Spalten SET, Kommando, TOP und Status sowie Einheit und Kodierung in der
Bemerkung - alles, was sich aus dem Code ableiten laesst:

  1. Die Tabelle fuehrt Byte 1 bis 202 lueckenlos und aufsteigend.
  2. Die Bitgruppen jedes Bytes ueberlappen sich nicht und ergeben alle 8 Bit.
  3. Jedes Set-Kommando steht auf seinem Byte, und seine Zeilen decken genau
     die Bits ab, ueber die es zurueckgelesen wird (ohne Rueckmeldung: die es
     tatsaechlich schreibt). Kein SET ab Byte 110. Kommando-Name passt.
  4. Jedes State-Topic steht auf genau den Bytes und Bits, die sein Dekodierer
     liest - bei Mehrbyte-Topics auf allen. Status-Name passt.
  5. Bei ganzen Bytes nennt die Bemerkung Einheit und Kodierung des
     Dekodierers, z. B. "(°C, Rohwert − 128)".
  6. Die Stand-Zeile nennt die richtige Anzahl Kommandos und Topics.
  7. SET-TOP-Zuordnung.md besteht set_top_zuordnung.py --pruefen: die Paare
     SET -> TOP, die Topic-Listen in Abschnitt 3 und die Zahlen im Text.

Nicht pruefbar sind die Bedeutungstexte ("Referenz:", "Original:", Befunde) -
die bleiben Handarbeit.

  ./doku_zuordnung_test.py                  # das Repo pruefen
  ./doku_zuordnung_test.py <kopie.md>       # eine andere Fassung pruefen (Gegenprobe)

Nur Standardbibliothek, kein Geraet noetig. Neu am 2026-09-18.
"""
import re
import subprocess
import sys
from pathlib import Path

HIER = Path(__file__).resolve().parent
REPO = HIER.parent
sys.path.insert(0, str(HIER))
import set_top_zuordnung as stz  # noqa: E402 - Parser fuer setCommands[]/stateTopics[]

KOPF = "| SET | Kommando | Byte | Bits | TOP | Status | Bemerkung |"
ERSTES_BYTE, LETZTES_BYTE = 1, 202  # Byte 0 (Telegrammkennung) fuehrt die Tabelle bewusst nicht

# Mehrbyte-Dekodierer aus decode.cpp: Funktionsname -> {Byte: Maske}. Bei den
# beiden Nachkomma-Dekodierern kommt das eigene Byte der Tabellenzeile dazu.
# Kommt ein Mehrbyte-Dekodierer hinzu oder aendert sich einer, gehoert er hier
# nachgetragen - der Test meldet beides, statt es zu raten.
MEHRBYTE = {
    "getPumpFlow": {169: 0xFF, 170: 0xFF},
    "getOperationHour": {182: 0xFF, 183: 0xFF},
    "getOperationCount": {179: 0xFF, 180: 0xFF},
    "getRoomHeaterHour": {185: 0xFF, 186: 0xFF},
    "getDHWHeaterHour": {188: 0xFF, 189: 0xFF},
    "getErrorInfo": {113: 0xFF, 114: 0xFF},
    "getInletTempWithFraction": {118: 0x07},
    "getOutletTempWithFraction": {118: 0x38},
}

# Kodierung der 1-Byte-Dekodierer fuer ganze Bytes, wie sie in der Bemerkung
# steht. Bitfeld-Dekodierer (DECODER_MASK in set_top_zuordnung.py) haben keine.
KODIERUNG = {
    "getIntMinus128": "Rohwert − 128",
    "getIntMinus1": "Rohwert − 1",
    "getIntMinus1Times10": "(Rohwert − 1) × 10",
    "getIntMinus1Times30": "(Rohwert − 1) × 30",
    "getIntMinus1Times50": "(Rohwert − 1) × 50",
    "getIntMinus1Times200": "(Rohwert − 1) × 200",
    "getIntMinus1Div5": "(Rohwert − 1) / 5",
}

# Beschreibungsliste in stateTopics[] -> Einheit in der Bemerkung
EINHEIT = {
    "Celsius": "°C", "Kelvin": "K", "Watt": "W", "Hertz": "Hz",
    "Pressure": "kgf/cm²", "Ampere": "A", "RotationsPerMin": "1/min",
    "Minutes": "min", "Hours": "h", "Percent": "%", "Duty": "Duty",
    "LitersPerMin": "l/min", "Counter": "Anzahl",
}


class Ergebnis:
    """Sammelt Befunde je Pruefung, damit ein Lauf ALLE Fehler zeigt."""

    def __init__(self):
        self.fehler = 0

    def pruefung(self, titel, befunde, ok_text):
        if befunde:
            self.fehler += len(befunde)
            print(f"  [FEHLER] {titel}:")
            for b in befunde:
                print(f"           {b}")
        else:
            print(f"  [ok ] {ok_text}")


def bitmaske(bits):
    """'ganz' | '1+2' | '3–5' | '6' -> Maske, Bit 1 ist das hoechstwertige.
    None bei unlesbarer Angabe."""
    if bits == "ganz":
        return 0xFF
    maske = 0
    for teil in bits.split("+"):
        bereich = re.fullmatch(r"([1-8])(?:[–-]([1-8]))?", teil)
        if not bereich:
            return None
        von = int(bereich.group(1))
        bis = int(bereich.group(2) or von)
        if bis < von:
            return None
        for bit in range(von, bis + 1):
            maske |= 1 << (8 - bit)
    return maske


def tabelle_lesen(pfad):
    """Die Zeilen der Byte-Tabelle als Liste von 7er-Listen, dazu der Text."""
    try:
        text = pfad.read_text()
    except OSError as fehler:
        sys.exit(f"Fehler: {pfad} nicht lesbar - {fehler}")
    zeilen = text.splitlines()
    kopf = [i for i, z in enumerate(zeilen) if z.strip() == KOPF]
    if len(kopf) != 1:
        sys.exit(f"Fehler: Tabellenkopf in {pfad.name} {len(kopf)}-mal gefunden statt einmal "
                 f"- Aufbau geaendert?\n  erwartet: {KOPF}")
    tabelle = []
    # Kopf und Trennzeile ueberspringen, dann bis zur ersten Nicht-Tabellenzeile
    for zeile in zeilen[kopf[0] + 2:]:
        if not zeile.startswith("|"):
            break
        tabelle.append([z.strip() for z in zeile.strip().strip("|").split("|")])
    return tabelle, text


def mehrbyte_im_code():
    """Je Mehrbyte-Dekodierer die serial_data[]-Zugriffe mit Maske aus decode.cpp."""
    text = (REPO / "src" / "decode.cpp").read_text()
    gefunden = {}
    for name in MEHRBYTE:
        rumpf = re.search(r"void " + name + r"\(const StateTopic[^)]*\)\s*\{(.*?)\n\}", text, re.S)
        if not rumpf:
            gefunden[name] = None
            continue
        felder = {}
        for zugriff in re.finditer(r"serial_data\[(\d+)\]", rumpf.group(1)):
            byte = int(zugriff.group(1))
            danach = rumpf.group(1)[zugriff.end():]
            # (serial_data[n] >> s) & 0bxxx  bzw.  serial_data[n] & 0bxxx  bzw. ganzes Byte
            verschoben = re.match(r"\s*>>\s*(\d+)\)\s*&\s*0b([01]+)", danach)
            maskiert = re.match(r"\s*&\s*0b([01]+)", danach)
            if verschoben:
                maske = int(verschoben.group(2), 2) << int(verschoben.group(1))
            elif maskiert:
                maske = int(maskiert.group(1), 2)
            else:
                maske = 0xFF
            felder[byte] = maske
        gefunden[name] = felder
    return gefunden


def main():
    pfad = Path(sys.argv[1]) if len(sys.argv) > 1 else REPO / "Byte-Zuordnung.md"
    erg = Ergebnis()
    tabelle, text = tabelle_lesen(pfad)

    # --- Code-Seite -------------------------------------------------------
    sets = stz.set_kommandos()
    tops = stz.state_topics()
    rueck = {k["nr"]: h for k, h, _ in stz.zuordnen(sets, tops)}
    beschr = {int(t.group(1)): t.group(6) for t in stz.tabelle_lesen(
        "decode.cpp",
        r"\{\s*(\d+),\s*(\d+),\s*\"([\w/]+)\",\s*(\w+),\s*(\w+),\s*(\w+)\}",
        r"const StateTopic stateTopics\[NUMBEROFTOPICS\] = \{(.*?)\n\};")}

    # Mehrbyte-Tabelle oben gegen decode.cpp halten; unbekannte Dekodierer
    # melden, statt sie stillschweigend als ganzes Byte zu werten
    befunde = []
    for name, felder in mehrbyte_im_code().items():
        if felder is None:
            befunde.append(f"{name} nicht in decode.cpp gefunden - MEHRBYTE pflegen")
        elif felder != MEHRBYTE[name]:
            befunde.append(f"{name} liest {sorted(felder.items())}, MEHRBYTE sagt "
                           f"{sorted(MEHRBYTE[name].items())}")
    for t in tops:
        if t["wide"] != "nullptr" and t["wide"] not in MEHRBYTE:
            befunde.append(f"TOP{t['nr']}: Mehrbyte-Dekodierer {t['wide']} fehlt in MEHRBYTE")
        if (t["decode"] != "nullptr" and t["decode"] not in stz.DECODER_MASK
                and t["decode"] not in KODIERUNG):
            befunde.append(f"TOP{t['nr']}: Dekodierer {t['decode']} unbekannt - in DECODER_MASK "
                           f"(set_top_zuordnung.py) oder KODIERUNG eintragen")
    erg.pruefung("Dekodierer", befunde, "alle Dekodierer bekannt, Mehrbyte-Zugriffe wie im Code")

    # --- 1. Bytes lueckenlos und aufsteigend -------------------------------
    befunde = []
    zeilen = []
    for nr, zellen in enumerate(tabelle, 1):
        if len(zellen) != 7:
            befunde.append(f"Tabellenzeile {nr}: {len(zellen)} Spalten statt 7")
            continue
        if not zellen[2].isdigit():
            befunde.append(f"Tabellenzeile {nr}: Byte '{zellen[2]}' ist keine Zahl")
            continue
        zeilen.append(dict(set=zellen[0], kommando=zellen[1], byte=int(zellen[2]),
                           bits=zellen[3], top=zellen[4], status=zellen[5], bem=zellen[6]))
    folge = [z["byte"] for z in zeilen]
    if folge != sorted(folge):
        befunde.append("Bytes nicht aufsteigend")
    fehlend = sorted(set(range(ERSTES_BYTE, LETZTES_BYTE + 1)) - set(folge))
    zuviel = sorted(set(folge) - set(range(ERSTES_BYTE, LETZTES_BYTE + 1)))
    if fehlend:
        befunde.append(f"Bytes fehlen: {fehlend}")
    if zuviel:
        befunde.append(f"Bytes ausserhalb {ERSTES_BYTE}-{LETZTES_BYTE}: {zuviel}")
    erg.pruefung("Byte-Folge", befunde,
                 f"Byte {ERSTES_BYTE}-{LETZTES_BYTE} lueckenlos und aufsteigend ({len(zeilen)} Zeilen)")

    # --- 2. Bitgruppen je Byte ---------------------------------------------
    befunde = []
    je_byte = {}
    for z in zeilen:
        z["maske"] = bitmaske(z["bits"])
        if z["maske"] is None:
            befunde.append(f"Byte {z['byte']}: Bitangabe '{z['bits']}' unlesbar")
            continue
        belegt = je_byte.get(z["byte"], 0)
        if belegt & z["maske"]:
            befunde.append(f"Byte {z['byte']}: Bits {z['bits']} ueberlappen")
        je_byte[z["byte"]] = belegt | z["maske"]
    for byte, belegt in sorted(je_byte.items()):
        if belegt != 0xFF:
            befunde.append(f"Byte {byte}: Bitgruppen ergeben 0x{belegt:02X} statt 0xFF")
    zeilen = [z for z in zeilen if z["maske"] is not None]
    erg.pruefung("Bitgruppen", befunde, "Bitgruppen je Byte ueberlappungsfrei und vollstaendig")

    # --- 3. Set-Kommandos ----------------------------------------------------
    befunde = []
    code_sets = {k["nr"]: k for k in sets}
    for z in zeilen:
        if bool(z["set"]) != bool(z["kommando"]):
            befunde.append(f"Byte {z['byte']} Bits {z['bits']}: SET und Kommando nur halb ausgefuellt")
        if z["set"]:
            treffer = re.fullmatch(r"SET(\d+)", z["set"])
            if not treffer or int(treffer.group(1)) not in code_sets:
                befunde.append(f"Byte {z['byte']} Bits {z['bits']}: '{z['set']}' gibt es im Code nicht")
            if z["byte"] >= stz.QUERYSIZE:
                befunde.append(f"Byte {z['byte']}: {z['set']} ab Byte {stz.QUERYSIZE} - "
                               f"dort existiert kein Kommando")
    for nr, k in sorted(code_sets.items()):
        eigene = [z for z in zeilen if z["set"] == f"SET{nr}"]
        if not eigene:
            befunde.append(f"SET{nr} `{k['name']}` fehlt (Byte {k['pos']})")
            continue
        haupt = rueck.get(nr)
        soll = haupt["mask"] if haupt else k["eff"]
        ist = 0
        for z in eigene:
            ist |= z["maske"]
            if z["byte"] != k["pos"]:
                befunde.append(f"SET{nr} steht auf Byte {z['byte']}, im Code auf Byte {k['pos']}")
            if z["kommando"] != f"`{k['name']}`":
                befunde.append(f"SET{nr}: Kommando {z['kommando']}, im Code `{k['name']}`")
        if ist != soll:
            befunde.append(f"SET{nr}: Zeilen decken Bits 0x{ist:02X}, erwartet 0x{soll:02X} "
                           f"({'Ruecklesung ueber TOP' + str(haupt['nr']) if haupt else 'ohne Ruecklesung'})")
    erg.pruefung("Set-Kommandos", befunde,
                 f"alle {len(code_sets)} Set-Kommandos auf Byte und Bits, Namen stimmen")

    # --- 4. State-Topics -----------------------------------------------------
    befunde = []
    code_tops = {t["nr"]: t for t in tops}
    for z in zeilen:
        if bool(z["top"]) != bool(z["status"]):
            befunde.append(f"Byte {z['byte']} Bits {z['bits']}: TOP und Status nur halb ausgefuellt")
        if z["top"]:
            treffer = re.fullmatch(r"TOP(\d+)", z["top"])
            if not treffer or int(treffer.group(1)) not in code_tops:
                befunde.append(f"Byte {z['byte']} Bits {z['bits']}: '{z['top']}' gibt es im Code nicht")
    for nr, t in sorted(code_tops.items()):
        # Felder, die der Dekodierer liest: eigenes Byte (1-Byte-Dekodierer),
        # dazu die Bytes des Mehrbyte-Dekodierers
        soll = {}
        if t["decode"] != "nullptr":
            soll[t["pos"]] = t["mask"]
        soll.update(MEHRBYTE.get(t["wide"], {}))
        eigene = [z for z in zeilen if z["top"] == f"TOP{nr}"]
        if not eigene:
            befunde.append(f"TOP{nr} `{t['name']}` fehlt (Byte {sorted(soll)})")
            continue
        ist = {}
        for z in eigene:
            ist[z["byte"]] = ist.get(z["byte"], 0) | z["maske"]
            if z["status"] != f"`{t['name']}`":
                befunde.append(f"TOP{nr}: Status {z['status']}, im Code `{t['name']}`")
        if ist != soll:
            befunde.append(f"TOP{nr}: Tabelle {_felder(ist)}, Dekodierer liest {_felder(soll)}")
    erg.pruefung("State-Topics", befunde,
                 f"alle {len(code_tops)} State-Topics auf Byte und Bits, Namen stimmen")

    # --- 5. Einheit und Kodierung in der Bemerkung ---------------------------
    befunde = []
    geprueft = 0
    for z in zeilen:
        treffer = re.fullmatch(r"TOP(\d+)", z["top"])
        if not treffer or z["bits"] != "ganz":
            continue
        t = code_tops.get(int(treffer.group(1)))
        if not t or t["decode"] not in KODIERUNG:
            continue
        einheit = EINHEIT.get(beschr.get(t["nr"], ""))
        if einheit is None:
            befunde.append(f"TOP{t['nr']}: Einheit fuer '{beschr.get(t['nr'])}' fehlt in EINHEIT")
            continue
        erwartet = f"({einheit}, {KODIERUNG[t['decode']]})"
        geprueft += 1
        if erwartet not in z["bem"]:
            befunde.append(f"Byte {z['byte']} TOP{t['nr']}: Bemerkung ohne \"{erwartet}\"")
    erg.pruefung("Einheit und Kodierung", befunde,
                 f"Einheit und Kodierung in {geprueft} Bemerkungen wie im Dekodierer")

    # --- 6. Stand-Zeile ------------------------------------------------------
    befunde = []
    stand = re.search(r"alle (\d+) Set-Kommandos und alle\s+(\d+) State-Topics", text)
    if not stand:
        befunde.append("Stand-Zeile ('alle N Set-Kommandos und alle M State-Topics') nicht gefunden")
    elif (int(stand.group(1)), int(stand.group(2))) != (len(sets), len(tops)):
        befunde.append(f"Stand-Zeile nennt {stand.group(1)} Kommandos und {stand.group(2)} Topics, "
                       f"der Code hat {len(sets)} und {len(tops)}")
    erg.pruefung("Stand-Zeile", befunde,
                 f"Stand-Zeile nennt {len(sets)} Kommandos und {len(tops)} Topics")

    # --- 7. SET-TOP-Zuordnung.md ---------------------------------------------
    befunde = []
    lauf = subprocess.run([sys.executable, str(HIER / "set_top_zuordnung.py"), "--pruefen"],
                          capture_output=True, text=True, check=False)
    if lauf.returncode != 0:
        # je Abweichung eine Zeile; die Kopfzeile von --pruefen ist kein Befund.
        # Bricht das Skript mit einer Meldung ab (Aufbau geaendert), zaehlt die.
        ausgabe = [z.strip() for z in (lauf.stdout + lauf.stderr).splitlines() if z.strip()]
        befunde = [z for z in ausgabe if not z.endswith("weicht vom Code ab:")] or ausgabe
    erg.pruefung("SET-TOP-Zuordnung.md", befunde,
                 "SET-TOP-Zuordnung.md deckt sich mit dem Code (set_top_zuordnung.py --pruefen)")

    print()
    if erg.fehler:
        print(f"FEHLGESCHLAGEN ({erg.fehler} Fehler)")
        return 1
    print("ALLE PRUEFUNGEN BESTANDEN (0 Fehler)")
    return 0


def _felder(felder):
    """{Byte: Maske} lesbar: 'Byte 118 0x38, Byte 144 0xFF'."""
    return ", ".join(f"Byte {b} 0x{m:02X}" for b, m in sorted(felder.items()))


if __name__ == "__main__":
    print(f"\n== {Path(sys.argv[1]).name if len(sys.argv) > 1 else 'Byte-Zuordnung.md'} gegen den Code ==")
    sys.exit(main())
