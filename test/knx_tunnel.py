#!/usr/bin/env python3
"""Minimaler KNXnet/IP-Tunnel-Client - Vorabtest fuer den KNX-Schritt im Notbetrieb.

Version 1.6.0 (2026-09-12)

Zweck: bevor der Notbetrieb das Vorderhaus per KNX versorgt (Mischer auf 50 %,
Mischerpumpe ein), belegen, dass ein KURZLEBIGER Tunnel ohne Heartbeat an der
KNX-IP-Schnittstelle traegt - mit genau dem Protokollumfang, den spaeter der
Firmwareschritt braucht. Die hier gebauten Bytefolgen sind zugleich die
Sollwerte fuer den Hosttest des spaeteren Firmware-Headers. Hintergrund,
Entscheidungen und Testplan: Analyse-KNX-Vorderhaus.md.

  ./knx_tunnel.py selbsttest                              # ohne Netz, gegen Simulator
  ./knx_tunnel.py verbinden 192.168.2.127                 # Stufe 0: kein Bustelegramm
  ./knx_tunnel.py lesen     192.168.2.127 6/4/21          # Stufe 1: GroupValueRead
  ./knx_tunnel.py schalten  192.168.2.127 6/4/20 6/4/21   # Stufe 2: umschalten, zuruecklesen, zurueckstellen
  ./knx_tunnel.py schreiben 192.168.2.127 6/4/20 1 --status 6/4/21   # manuell zurueckstellen
  ./knx_tunnel.py lesen     192.168.2.127 6/4/21 --quelle 1.1.250 --gegenprobe
  ./knx_tunnel.py lesen     192.168.2.127 6/4/12 --alle     # jede Antwort samt Quelle
  ./knx_tunnel.py mischer   192.168.2.127 --mithoeren       # Referenz des Firmwareschritts "Vorderhaus"
  ./knx_tunnel.py mischer   192.168.2.127 --bewegung --ohne-pumpe --mithoeren   # Rueckleseregel A (6/4/14)

Optionen: --port (Standard 3671), --roh (jedes Datagramm als Hex),
--quelle B.L.G (Quelladresse im Telegramm statt der Tunneladresse),
--gegenprobe (zweiter Tunnel hoert mit und meldet, mit welcher Quelle die
eigenen Telegramme auf dem Bus stehen), lesen: --alle (das ganze Lesefenster
abwarten und jede Antwort mit Quelle zeigen), schreiben: --dpt 1|5 und --status GA
(Ruecklesung, nur DPT 1), schalten: --halten SEK (0-30, Standard 5),
mischer: --bewegung [GA] (Rueckleseregel A mit der Bewegungsmeldung,
Standard 6/4/14), --ohne-pumpe, --ohne-zwang (Negativprobe), --mithoeren.

Rueckleseregel (aus Stufe 2, 2026-09-12): Die L_Data.con entscheidet nichts.
Eine negative con hiess an der Anlage nicht "verloren" - das Telegramm lag
auf dem Bus, der Aktor schaltete. Und mit vorgegebener Quelle liefert die
Schnittstelle gar keine con. Das Urteil faellt deshalb die Antwort bzw. die
Ruecklesung am Aktor; passt sie nicht, wird genau einmal wiederholt. Dieselbe
Regel soll der Firmwareschritt bekommen.

Was auf den Bus geht: 'verbinden' nichts; 'lesen' nur Lesetelegramme;
'schreiben' und 'schalten' veraendern einen Aktor. '--gegenprobe' belegt
einen zusaetzlichen Tunnel und sendet selbst nichts.

Exit-Codes: 0 gruen, 1 Kommunikations- oder Protokollfehler (auch: 'schreiben'
ohne --status, wenn die con negativ ist oder fehlt), 2 Aufruffehler, 3 fremder
Schreibzugriff waehrend 'schalten' (bewusst NICHT zurueckgestellt),
4 Ruecklesung passt nicht (bei 'schalten' auch nach der Wiederholung).

Nur Standardbibliothek. Kein KNX IP Secure (an dieser Anlage nicht aktiv).

Changelog:
  1.6.0 (2026-09-12) Owner-Entscheid A: 'mischer --bewegung' setzt die
                     Rueckleseregel A um und ist damit die Referenz des
                     Firmwareschritts. Neu: Bewegung VOR den Befehlen lesen -
                     faehrt der Mischer schon oder schweigt der Aktor, gilt die
                     Endstellung mit Frist 220 s. Der Eingang ist Pflicht (keine
                     Antwort = Abweichung). Bei ROT genau eine Wiederholung der
                     Mischertelegramme. Simulator nach den Laeufen vom
                     2026-09-12: Das Zuruecknehmen der Zwangsstellung allein
                     faehrt nicht mehr (Option 'freigabe_faehrt' fuer den
                     Gegenfall).
  1.5.0 (2026-09-12) Owner-Vorschlag: den Mischer ueber 'MischerMotor_Position_
                     Bewegung' (6/4/14, K/L/Ue, 1 = faehrt bis zum Ziel)
                     bestaetigen, statt bis zu 60 s auf die Endstellung zu
                     warten. Neu 'mischer --bewegung': in der Befehlsverbindung
                     kurz auf die spontane Bewegungsmeldung warten (Laufzeit ab
                     dem vorausgehenden eigenen Telegramm), dann Eingang,
                     Bewegung und Status vom Aktor lesen und urteilen. Die
                     Abfrage bis zur Endstellung bleibt als Gegenkontrolle und
                     meldet ein falsches GRUEN. Neu '--ohne-pumpe' und
                     '--ohne-zwang' (Negativprobe). Es zaehlen nur noch
                     Antworten von 1.1.39 (bisher: alles ausser openknx).
                     Fahrzeit und Frist zaehlen ab dem Positionsbefehl, nicht
                     mehr ab dem Ende der Befehlsverbindung. Simulator:
                     Bewegungs- und Eingangsobjekt, verlorener Positionsbefehl,
                     waehlbare Ausgangsstellung.
  1.4.0 (2026-09-12) Neu 'mischer': der Firmwareschritt "Vorderhaus" nach
                     Weg A als Referenz - eine kurze Verbindung fuer
                     Zwangsstellungen, Position, Pumpe und Pumpenstatus, dann
                     der Mischerstatus in eigenen kurzen Verbindungen, bis er
                     die Position meldet. Antworten von openknx zaehlen nicht.
                     '--mithoeren': passiver Tunnel mit Heartbeat fuer den
                     ganzen Lauf. Simulator mit Mischermodell (proportionale
                     Fahrt, Meldung erst am Ziel, Zwangsstellung vorrangig).
  1.3.0 (2026-09-12) Das Lesen von 6/4/12 beantwortete openknx (1.1.245) aus
                     seinem Zwischenspeicher, nicht der Aktor - und das
                     Werkzeug nahm die erste Antwort. Neu 'lesen --alle': das
                     ganze Lesefenster abwarten, jede Antwort mit Quelle.
                     Bekannte Teilnehmer werden in der Ausgabe benannt.
  1.2.0 (2026-09-12) Nach dem Lauf mit --quelle 1.1.250: Die Schnittstelle
                     liefert bei vorgegebener Quelle keine L_Data.con. Eine
                     FEHLENDE con ist deshalb wie eine negative kein
                     Abbruchgrund mehr (Wartezeit 3 s -> 1 s). Neu
                     '--gegenprobe': zweiter, mithoerender Tunnel in einem
                     eigenen Thread. Simulator bedient zwei Tunnel.
  1.1.0 (2026-09-12) Nach Stufe 2: Eine negative L_Data.con bricht nicht mehr
                     ab. 'schalten' urteilt nach der Ruecklesung und
                     wiederholt bei Abweichung genau einmal; 'schreiben
                     --status' liest zurueck, die Rueckstellzeile nutzt das.
                     Neu '--quelle' fuer eine vorgegebene Quelladresse, wie
                     openknx sie mit 'eibadr' setzt. Simulator: Tunneladresse
                     und ctrl1 der con wie an der Anlage beobachtet.
  1.0.0 (2026-09-12) Erstfassung: verbinden, lesen, schreiben, schalten,
                     selbsttest (Sollwerte aus xknx plus Simulator).
"""
import argparse
import contextlib
import io
import ipaddress
import socket
import sys
import threading
import time
from dataclasses import dataclass

# --- KNXnet/IP: Dienstkennungen im Kopf ------------------------------------
CONNECT_REQUEST = 0x0205
CONNECT_RESPONSE = 0x0206
CONNECTIONSTATE_REQUEST = 0x0207
CONNECTIONSTATE_RESPONSE = 0x0208
DISCONNECT_REQUEST = 0x0209
DISCONNECT_RESPONSE = 0x020A
TUNNELING_REQUEST = 0x0420
TUNNELING_ACK = 0x0421

# --- cEMI: Nachrichtencodes und APCI der Gruppendienste ---------------------
L_DATA_REQ = 0x11
L_DATA_CON = 0x2E
L_DATA_IND = 0x29
APCI_READ = 0x000
APCI_RESPONSE = 0x040
APCI_WRITE = 0x080
APCI_NAME = {APCI_READ: "read", APCI_RESPONSE: "response", APCI_WRITE: "write"}

# --- Bekannte Teilnehmer dieser Anlage (nur fuer die Ausgabe) ---------------
# Gebaut wird fuer genau diese Anlage; die Namen machen Mitschnitte lesbar.
# 1.1.245 ist openknx: Es beantwortet Lesetelegramme aus seinem
# Zwischenspeicher (2026-09-12) - eine Antwort von dort ist NICHT der Aktor,
# und im Notbetriebsfall ist sie gar nicht da.
OPENKNX = 0x11F5  # 1.1.245
MISCHER_AKTOR = 0x1127  # 1.1.39 - beantwortete 6/4/12 selbst (2026-09-12)
BEKANNTE_QUELLEN = {OPENKNX: "openknx/ioBroker", 0x113C: "Aktor Pumpe VH",
                    MISCHER_AKTOR: "Aktor Mischer VH"}


def quelle_text(ia: int) -> str:
    name = BEKANNTE_QUELLEN.get(ia)
    return f"{ia_text(ia)} = {name}" if name else ia_text(ia)

# --- Statuscodes der Schnittstelle (CONNECT/CONNECTIONSTATE/ACK) -----------
STATUS_NAME = {
    0x00: "E_NO_ERROR",
    0x01: "E_HOST_PROTOCOL_TYPE",
    0x02: "E_VERSION_NOT_SUPPORTED",
    0x04: "E_SEQUENCE_NUMBER",
    0x21: "E_CONNECTION_ID",
    0x22: "E_CONNECTION_TYPE",
    0x23: "E_CONNECTION_OPTION",
    0x24: "E_NO_MORE_CONNECTIONS (alle Tunnel belegt)",
    0x25: "E_NO_MORE_UNIQUE_CONNECTIONS",
    0x26: "E_DATA_CONNECTION",
    0x27: "E_KNX_CONNECTION",
    0x29: "E_TUNNELLING_LAYER",
}


def status_text(code: int) -> str:
    return f"0x{code:02X} {STATUS_NAME.get(code, 'unbekannt')}"


@dataclass
class Zeiten:
    """Wartezeiten in Sekunden. Der Selbsttest verkuerzt sie."""
    antwort: float = 10.0  # CONNECT-/CONNECTIONSTATE-Antwort (Spezifikation: 10 s)
    trennen: float = 2.0   # DISCONNECT_RESPONSE; danach wird der Socket trotzdem geschlossen
    ack: float = 1.0       # TUNNELING_ACK (Spezifikation: 1 s, genau eine Wiederholung)
    con: float = 1.0       # L_Data.con - kam an der Anlage nach 22 ms; fehlt sie, entscheidet die Ruecklesung
    lesen: float = 3.0     # GroupValueResponse des Aktors
    status: float = 3.0    # spontane Statusmeldung nach dem Schalten, sonst aktiv lesen
    bewegung: float = 2.0  # spontane Bewegungsmeldung des Mischers - am Bus nach rund 0,1 s (2026-09-12)
    endlauf: float = 220.0  # Regel A, Rueckfall: Endlagenlauf bis 144 s + halber Hub 60 s + Reserve
    zuordnung: float = 0.5  # Mindestabstand zweier eigener Telegramme, um ihr die Meldung zuzuordnen
    nachlauf: float = 0.3  # Gegenprobe: so lange nach dem letzten Telegramm weiter mithoeren


class KnxFehler(Exception):
    """Kommunikations- oder Protokollfehler - fuehrt zu Exit-Code 1."""


# --- Ausgabe ----------------------------------------------------------------
# flush: die Ausgabe landet sonst bei Umleitung in eine Datei erst am
# Prozessende dort (Python puffert) - ein laufender Test saehe leer aus.
# Die Sperre haelt jede Zeile am Stueck: Gegenprobe und Mithoerer schreiben
# aus eigenen Threads, und print() setzt Text und Zeilenende getrennt ab -
# am 2026-09-12 liefen so zwei Zeilen ineinander.
_START = time.monotonic()
_AUSGABE = threading.Lock()


def melde(text: str) -> None:
    zeile = f"[{(time.monotonic() - _START) * 1000:7.0f} ms] {text}\n"
    with _AUSGABE:
        sys.stdout.write(zeile)
        sys.stdout.flush()


# --- Adressen ---------------------------------------------------------------
def ga_aus_text(text: str) -> int:
    """Dreistufige Gruppenadresse 'H/M/U' -> 16 Bit. Lehnt alles andere ab."""
    teile = text.strip().split("/")
    if len(teile) != 3:
        raise ValueError(f"Gruppenadresse '{text}' ist nicht dreistufig (Haupt/Mitte/Unter)")
    try:
        haupt, mitte, unter = (int(t, 10) for t in teile)
    except ValueError:
        raise ValueError(f"Gruppenadresse '{text}' enthaelt keine ganze Zahl") from None
    if not (0 <= haupt <= 31 and 0 <= mitte <= 7 and 0 <= unter <= 255):
        raise ValueError(f"Gruppenadresse '{text}' ausserhalb 0-31/0-7/0-255")
    ga = haupt << 11 | mitte << 8 | unter
    # 0/0/0 ist die Broadcast-Adresse - ein Schreibtelegramm dorthin traefe
    # jeden Teilnehmer, der darauf hoert.
    if ga == 0:
        raise ValueError("Gruppenadresse 0/0/0 ist die Broadcast-Adresse")
    return ga


def ia_aus_text(text: str) -> int:
    """Physikalische Adresse 'B.L.G' (Bereich, Linie, Geraet) -> 16 Bit."""
    teile = text.strip().split(".")
    if len(teile) != 3:
        raise ValueError(f"Physikalische Adresse '{text}' ist nicht dreiteilig (Bereich.Linie.Geraet)")
    try:
        bereich, linie, geraet = (int(t, 10) for t in teile)
    except ValueError:
        raise ValueError(f"Physikalische Adresse '{text}' enthaelt keine ganze Zahl") from None
    if not (0 <= bereich <= 15 and 0 <= linie <= 15 and 0 <= geraet <= 255):
        raise ValueError(f"Physikalische Adresse '{text}' ausserhalb 0-15.0-15.0-255")
    # Geraeteadresse 0 gehoert dem Linien- bzw. Bereichskoppler
    if geraet == 0:
        raise ValueError(f"Physikalische Adresse '{text}': Geraeteadresse 0 ist den Kopplern vorbehalten")
    return bereich << 12 | linie << 8 | geraet


def ga_text(ga: int) -> str:
    return f"{ga >> 11 & 0x1F}/{ga >> 8 & 0x07}/{ga & 0xFF}"


def ia_text(ia: int) -> str:
    return f"{ia >> 12 & 0x0F}.{ia >> 8 & 0x0F}.{ia & 0xFF}"


# --- Rahmen bauen (reine Funktionen, vom Selbsttest gegen xknx geprueft) ----
def kopf(dienst: int, nutzlaenge: int) -> bytes:
    gesamt = 6 + nutzlaenge
    return bytes((0x06, 0x10, dienst >> 8, dienst & 0xFF, gesamt >> 8, gesamt & 0xFF))


def hpai(endpunkt: tuple[str, int]) -> bytes:
    ip, port = endpunkt
    return bytes((0x08, 0x01)) + socket.inet_aton(ip) + port.to_bytes(2, "big")


def baue_connect_request(steuer: tuple[str, int], daten: tuple[str, int]) -> bytes:
    # CRI: Laenge 4, TUNNEL_CONNECTION (0x04), TUNNEL_LINKLAYER (0x02), reserviert
    rumpf = hpai(steuer) + hpai(daten) + bytes((0x04, 0x04, 0x02, 0x00))
    return kopf(CONNECT_REQUEST, len(rumpf)) + rumpf


def baue_connectionstate_request(kanal: int, steuer: tuple[str, int]) -> bytes:
    rumpf = bytes((kanal, 0x00)) + hpai(steuer)
    return kopf(CONNECTIONSTATE_REQUEST, len(rumpf)) + rumpf


def baue_disconnect_request(kanal: int, steuer: tuple[str, int]) -> bytes:
    rumpf = bytes((kanal, 0x00)) + hpai(steuer)
    return kopf(DISCONNECT_REQUEST, len(rumpf)) + rumpf


def baue_disconnect_response(kanal: int, status: int = 0) -> bytes:
    rumpf = bytes((kanal, status))
    return kopf(DISCONNECT_RESPONSE, len(rumpf)) + rumpf


def baue_tunneling_request(kanal: int, seq: int, cemi: bytes) -> bytes:
    rumpf = bytes((0x04, kanal, seq & 0xFF, 0x00)) + cemi
    return kopf(TUNNELING_REQUEST, len(rumpf)) + rumpf


def baue_tunneling_ack(kanal: int, seq: int, status: int = 0) -> bytes:
    rumpf = bytes((0x04, kanal, seq & 0xFF, status))
    return kopf(TUNNELING_ACK, len(rumpf)) + rumpf


def baue_cemi(code: int, quelle: int, ga: int, apci: int, klein: int = 0,
              daten: bytes = b"", ctrl1: int = 0xBC) -> bytes:
    """cEMI-L_Data fuer einen Gruppendienst.

    ctrl1 0xBC: Standardrahmen, nicht wiederholen, Broadcast, Prioritaet low.
    ctrl2 0xE0: Zieladresse ist eine Gruppe, Routingzaehler 6.
    Quelle 0.0.0 im Request - die Schnittstelle setzt ihre Tunneladresse ein;
    bei einer vorgegebenen Quelle (--quelle) liefert sie keine L_Data.con.
    Werte bis 6 Bit (DPT 1) stecken im APCI-Byte, laengere folgen dahinter.
    """
    if daten:
        npdu = bytes((0x00 | apci >> 8 & 0x03, apci & 0xC0)) + daten
        laenge = 1 + len(daten)
    else:
        npdu = bytes((0x00 | apci >> 8 & 0x03, (apci & 0xC0) | (klein & 0x3F)))
        laenge = 1
    return bytes((code, 0x00, ctrl1, 0xE0, quelle >> 8, quelle & 0xFF,
                  ga >> 8, ga & 0xFF, laenge)) + npdu


# --- Rahmen zerlegen ----------------------------------------------------------
def zerlege_knxip(dg: bytes) -> tuple[int, bytes]:
    if len(dg) < 6 or dg[0] != 0x06 or dg[1] != 0x10:
        raise KnxFehler("kein KNXnet/IP-Kopf")
    dienst = dg[2] << 8 | dg[3]
    gesamt = dg[4] << 8 | dg[5]
    if gesamt < 6 or gesamt > len(dg):
        raise KnxFehler(f"Laengenfeld {gesamt} passt nicht zu {len(dg)} Byte")
    return dienst, dg[6:gesamt]


def zerlege_hpai(b: bytes) -> tuple[str, int]:
    if len(b) < 8 or b[0] != 0x08 or b[1] != 0x01:
        raise KnxFehler("HPAI ungueltig")
    return socket.inet_ntoa(b[2:6]), b[6] << 8 | b[7]


@dataclass
class ConnectAntwort:
    kanal: int
    status: int
    daten_ep: tuple[str, int] | None
    adresse: int | None


def zerlege_connect_response(rumpf: bytes) -> ConnectAntwort:
    if len(rumpf) < 2:
        raise KnxFehler("CONNECT_RESPONSE zu kurz")
    kanal, status = rumpf[0], rumpf[1]
    if status != 0:
        return ConnectAntwort(kanal, status, None, None)
    if len(rumpf) < 14:
        raise KnxFehler("CONNECT_RESPONSE ohne Datenendpunkt und CRD")
    ep = zerlege_hpai(rumpf[2:10])
    crd = rumpf[10:14]
    if crd[0] != 0x04 or crd[1] != 0x04:
        raise KnxFehler("CRD beschreibt keinen Tunnel")
    return ConnectAntwort(kanal, 0, ep, crd[2] << 8 | crd[3])


@dataclass
class Cemi:
    code: int
    ctrl1: int
    ctrl2: int
    quelle: int
    ziel: int
    apci: int
    laenge: int   # NPDU-Laengenfeld: 1 = Wert im APCI-Byte, >1 = Datenbytes folgen
    klein: int    # Wert bis 6 Bit (nur bei laenge == 1)
    daten: bytes  # Datenbytes (nur bei laenge > 1)
    zeit: float = 0.0  # Empfangszeit (monotone Uhr), vom Tunnel gesetzt - fuer Laufzeiten im Mischerlauf

    @property
    def gruppe(self) -> bool:
        return bool(self.ctrl2 & 0x80)

    @property
    def bestaetigt_ok(self) -> bool:
        # Nur bei L_Data.con aussagekraeftig: Bit 0 von ctrl1 gesetzt = Fehler.
        # Nie das ganze Byte vergleichen - die Schnittstelle aendert Bit 5
        # (gesendet bc, zurueck 9c).
        return not self.ctrl1 & 0x01


def zerlege_cemi(c: bytes) -> Cemi:
    if len(c) < 2:
        raise KnxFehler("cEMI zu kurz")
    i = 2 + c[1]  # Zusatzinformation ueberspringen
    if len(c) < i + 8:
        raise KnxFehler("cEMI-L_Data zu kurz")
    laenge = c[i + 6]
    apdu = c[i + 7:]
    if laenge < 1 or len(apdu) < laenge + 1:
        raise KnxFehler(f"NPDU-Laenge {laenge} passt nicht zu {len(apdu)} Byte")
    apci = ((apdu[0] & 0x03) << 8 | apdu[1]) & 0x3C0
    klein = apdu[1] & 0x3F if laenge == 1 else 0
    daten = bytes(apdu[2:laenge + 1]) if laenge > 1 else b""
    return Cemi(c[0], c[i], c[i + 1], c[i + 2] << 8 | c[i + 3],
                c[i + 4] << 8 | c[i + 5], apci, laenge, klein, daten)


def wert_text(r: Cemi) -> str:
    if r.laenge == 1:
        return str(r.klein)
    if r.laenge == 2:
        return f"{r.daten[0]} (als DPT 5.001: {r.daten[0] * 100 / 255:.1f} %)"
    return "0x" + r.daten.hex()


# --- Die Tunnelverbindung -----------------------------------------------------
class Tunnel:
    """Eine kurzlebige Tunnelverbindung, nur mit 'with' zu benutzen - so geht
    DISCONNECT auch im Fehlerfall und bei Strg-C raus.

    Kein Heartbeat: Die Verbindung lebt Sekunden, der erste CONNECTIONSTATE
    waere erst nach 60 s faellig. Bleibt ein Tunnel doch haengen (Absturz
    ohne DISCONNECT), raeumt die Schnittstelle ihn nach 120 s selbst ab.

    Ein einziger Socket dient als Steuer- und Datenendpunkt. Alles Eingehende
    laeuft durch _empfange_einmal(), auch waehrend auf etwas anderes gewartet
    wird - die Schnittstelle schickt jedes Bustelegramm an jeden Tunnel und
    wiederholt es, wenn das ACK ausbleibt; nach der Wiederholung trennt sie.

    Eine Instanz gehoert immer genau einem Thread (die Gegenprobe hat ihre
    eigene).
    """

    def __init__(self, ip: str, port: int, zeiten: Zeiten | None = None,
                 roh: bool = False, beobachtet: tuple[int, ...] = (), quelle: int = 0,
                 praefix: str = ""):
        self.ip = ip
        self.port = port
        self.zeiten = zeiten or Zeiten()
        self.roh = roh
        self.beobachtet = set(beobachtet)
        self.quelle = quelle                   # 0 = die Schnittstelle setzt ihre Tunneladresse
        self.praefix = praefix                 # kennzeichnet die Ausgabe der Gegenprobe
        self.sock: socket.socket | None = None
        self.lokal: tuple[str, int] | None = None
        self.kanal: int | None = None
        self.adresse: int | None = None       # Tunneladresse, vergibt die Schnittstelle
        self.daten_ep: tuple[str, int] | None = None
        self.tx_seq = 0
        self.rx_seq = 0
        self.antworten: dict[int, bytes] = {}  # letzte Antwort je Dienst
        self.acks: list[tuple[int, int]] = []  # (Sequenz, Status)
        self.cons: list[Cemi] = []
        self.inds: list[Cemi] = []
        self.gesendet: list[tuple[int, int]] = []  # (Ziel-GA, APCI) jedes eigenen Telegramms
        self.sonstige = 0                      # mitgelesene fremde Bustelegramme
        self.gegenstelle_getrennt = False
        self._quelle_gemeldet = False

    def __enter__(self) -> "Tunnel":
        self.verbinden()
        return self

    def __exit__(self, *_) -> bool:
        self.trennen()
        return False

    def _melde(self, text: str) -> None:
        melde(self.praefix + text)

    # --- Senden und Empfangen ---------------------------------------------
    def _sende(self, dg: bytes, ziel: tuple[str, int]) -> None:
        if self.roh:
            self._melde(">> " + dg.hex(" "))
        self.sock.sendto(dg, ziel)

    def _warte(self, bedingung, frist: float, trennung_pruefen: bool = True):
        """Empfaengt, bis bedingung() etwas anderes als None liefert oder die
        Frist ablaeuft (dann None). Monotone Uhr: unempfindlich gegen
        Zeitspruenge der Systemuhr."""
        ende = time.monotonic() + max(0.0, frist)
        while True:
            ergebnis = bedingung()
            if ergebnis is not None:
                return ergebnis
            if trennung_pruefen and self.gegenstelle_getrennt:
                raise KnxFehler("die Schnittstelle hat die Verbindung von sich aus beendet")
            rest = ende - time.monotonic()
            if rest <= 0:
                return None
            self._empfange_einmal(rest)

    def _empfange_einmal(self, rest: float) -> None:
        self.sock.settimeout(max(0.01, min(rest, 0.5)))
        try:
            dg, absender = self.sock.recvfrom(1024)
        except socket.timeout:
            return
        if absender[0] != self.ip:
            self._melde(f"Datagramm von fremder Adresse {absender[0]} verworfen")
            return
        if self.roh:
            self._melde("<< " + dg.hex(" "))
        try:
            dienst, rumpf = zerlege_knxip(dg)
        except KnxFehler as fehler:
            self._melde(f"unlesbares Datagramm verworfen: {fehler}")
            return
        if dienst in (CONNECT_RESPONSE, CONNECTIONSTATE_RESPONSE, DISCONNECT_RESPONSE):
            self.antworten[dienst] = rumpf
        elif dienst == TUNNELING_ACK:
            if len(rumpf) >= 4 and rumpf[1] == self.kanal:
                self.acks.append((rumpf[2], rumpf[3]))
        elif dienst == TUNNELING_REQUEST:
            self._tunnel_empfangen(rumpf, absender)
        elif dienst == DISCONNECT_REQUEST:
            if rumpf and rumpf[0] == self.kanal:
                self._sende(baue_disconnect_response(self.kanal), absender)
                self.gegenstelle_getrennt = True
        else:
            self._melde(f"Dienst 0x{dienst:04X} ignoriert")

    def _tunnel_empfangen(self, rumpf: bytes, absender: tuple[str, int]) -> None:
        if len(rumpf) < 4 or rumpf[0] != 0x04 or rumpf[1] != self.kanal:
            return  # fremder Kanal oder kaputter Verbindungskopf - nicht quittieren
        seq = rumpf[2]
        # Sequenzregel der Spezifikation: die erwartete Nummer quittieren und
        # verarbeiten; die vorige nur quittieren (die Schnittstelle wiederholt,
        # weil unser ACK verloren ging); alles andere verwerfen, ohne ACK.
        if seq == self.rx_seq:
            self._sende(baue_tunneling_ack(self.kanal, seq), absender)
            self.rx_seq = (seq + 1) & 0xFF
        elif seq == (self.rx_seq - 1) & 0xFF:
            self._sende(baue_tunneling_ack(self.kanal, seq), absender)
            return
        else:
            self._melde(f"Sequenz {seq} statt {self.rx_seq} verworfen")
            return
        try:
            r = zerlege_cemi(rumpf[4:])
        except KnxFehler as fehler:
            self._melde(f"cEMI unlesbar: {fehler}")
            return
        r.zeit = time.monotonic()
        if r.code == L_DATA_CON:
            self.cons.append(r)
        elif r.code == L_DATA_IND:
            self.inds.append(r)
            if r.gruppe and r.ziel in self.beobachtet:
                self._melde(f"Bus: {ia_text(r.quelle)} -> {ga_text(r.ziel)} "
                            f"{APCI_NAME.get(r.apci, hex(r.apci))} {wert_text(r)}")
            else:
                self.sonstige += 1

    # --- Verbindung ---------------------------------------------------------
    def verbinden(self) -> None:
        try:
            # connect() auf einem UDP-Socket sendet nichts, legt aber die Route
            # fest - so steht im HPAI die Adresse, unter der uns die
            # Schnittstelle erreicht, und nicht 0.0.0.0.
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
                probe.connect((self.ip, self.port))
                lokale_ip = probe.getsockname()[0]
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.sock.bind((lokale_ip, 0))
            self.lokal = self.sock.getsockname()
            self._sende(baue_connect_request(self.lokal, self.lokal), (self.ip, self.port))
            rumpf = self._warte(lambda: self.antworten.pop(CONNECT_RESPONSE, None),
                                self.zeiten.antwort)
            if rumpf is None:
                raise KnxFehler(f"keine CONNECT_RESPONSE von {self.ip}:{self.port} "
                                f"binnen {self.zeiten.antwort:g} s")
            a = zerlege_connect_response(rumpf)
            if a.status != 0:
                raise KnxFehler(f"Verbindung abgelehnt: {status_text(a.status)}")
            self.kanal, self.adresse = a.kanal, a.adresse
            # Datenendpunkt 0.0.0.0:0 heisst "derselbe wie der Steuerendpunkt"
            if a.daten_ep[0] == "0.0.0.0" or a.daten_ep[1] == 0:
                self.daten_ep = (self.ip, self.port)
            else:
                self.daten_ep = a.daten_ep
            self._melde(f"verbunden: Kanal {self.kanal}, Tunneladresse {ia_text(self.adresse)}, "
                        f"Datenendpunkt {self.daten_ep[0]}:{self.daten_ep[1]}, "
                        f"lokal {self.lokal[0]}:{self.lokal[1]}")
        except BaseException:
            if self.sock is not None:
                self.sock.close()
                self.sock = None
            raise

    def zustand_pruefen(self) -> None:
        self._sende(baue_connectionstate_request(self.kanal, self.lokal), (self.ip, self.port))
        rumpf = self._warte(lambda: self.antworten.pop(CONNECTIONSTATE_RESPONSE, None),
                            self.zeiten.antwort)
        if rumpf is None:
            raise KnxFehler("keine CONNECTIONSTATE_RESPONSE")
        if len(rumpf) < 2 or rumpf[0] != self.kanal:
            raise KnxFehler("CONNECTIONSTATE_RESPONSE fuer einen fremden Kanal")
        if rumpf[1] != 0:
            raise KnxFehler(f"Verbindungszustand: {status_text(rumpf[1])}")
        self._melde("Verbindungszustand ok")

    def trennen(self) -> None:
        if self.sock is None:
            return
        try:
            if self.kanal is not None and not self.gegenstelle_getrennt:
                self._sende(baue_disconnect_request(self.kanal, self.lokal), (self.ip, self.port))
                rumpf = self._warte(lambda: self.antworten.pop(DISCONNECT_RESPONSE, None),
                                    self.zeiten.trennen, trennung_pruefen=False)
                if rumpf is None:
                    self._melde("keine DISCONNECT_RESPONSE - die Schnittstelle raeumt den Tunnel "
                                "nach 120 s selbst ab")
                else:
                    self._melde("getrennt, Tunnel freigegeben")
            if self.sonstige:
                self._melde(f"{self.sonstige} weitere Bustelegramme mitgelesen und quittiert")
        except (OSError, KnxFehler) as fehler:
            self._melde(f"Trennen gestoert: {fehler}")
        finally:
            self.sock.close()
            self.sock = None

    # --- Gruppendienste -----------------------------------------------------
    def _sende_cemi(self, cemi: bytes, ga: int) -> Cemi | None:
        """TUNNELING_REQUEST mit ACK (eine Wiederholung), dann L_Data.con.

        Die Reihenfolge von ACK und con ist nicht festgelegt - beide werden
        unabhaengig voneinander eingesammelt. Fehlt das ACK, ist die
        Verbindung gestoert (KnxFehler). Die con dagegen entscheidet nichts:
        Eine negative lag an der Anlage trotzdem auf dem Bus (Stufe 2), und
        bei vorgegebener Quelle kommt gar keine. Rueckgabe: die con oder None.
        """
        self.gesendet.append((ga, zerlege_cemi(cemi).apci))
        marke = len(self.cons)
        self.acks.clear()
        ack = None
        for versuch in (1, 2):
            self._sende(baue_tunneling_request(self.kanal, self.tx_seq, cemi), self.daten_ep)
            ack = self._warte(lambda: next((a for a in self.acks if a[0] == self.tx_seq), None),
                              self.zeiten.ack)
            if ack is not None:
                break
            self._melde(f"kein TUNNELING_ACK auf Sequenz {self.tx_seq} (Versuch {versuch})")
        if ack is None:
            raise KnxFehler("kein TUNNELING_ACK nach der Wiederholung")
        if ack[1] != 0:
            raise KnxFehler(f"TUNNELING_ACK meldet {status_text(ack[1])}")
        self.tx_seq = (self.tx_seq + 1) & 0xFF
        con = self._warte(lambda: next((r for r in self.cons[marke:] if r.ziel == ga), None),
                          self.zeiten.con)
        if con is None:
            self._melde(f"keine L_Data.con fuer {ga_text(ga)} binnen {self.zeiten.con:g} s"
                        + (" - bei vorgegebener Quelle liefert die Schnittstelle keine"
                           if self.quelle else "")
                        + "; die Ruecklesung entscheidet")
            return None
        self._quelle_melden(con)
        if not con.bestaetigt_ok:
            self._melde(f"Busbestaetigung fuer {ga_text(ga)} NEGATIV (L_Data.con) - "
                        "Zustellung unbekannt, die Ruecklesung entscheidet")
        return con

    def _quelle_melden(self, con: Cemi) -> None:
        # Einmal je Verbindung: Mit welcher Quelladresse ging das Telegramm auf
        # den Bus? Die con ist die Kopie des gesendeten Rahmens und traegt sie.
        if self._quelle_gemeldet:
            return
        self._quelle_gemeldet = True
        if self.quelle == 0:
            self._melde(f"Quelladresse auf dem Bus: {ia_text(con.quelle)} (von der Schnittstelle eingesetzt)")
        elif con.quelle == self.quelle:
            self._melde(f"Quelladresse auf dem Bus: {ia_text(con.quelle)} - "
                        "die Schnittstelle uebernimmt die vorgegebene Adresse")
        else:
            self._melde(f"Quelladresse auf dem Bus: {ia_text(con.quelle)} - "
                        f"die vorgegebene {ia_text(self.quelle)} wurde ERSETZT")

    def lesen(self, ga: int) -> Cemi:
        # Marke VOR dem Senden: Die Antwort des Aktors darf auch vor der
        # L_Data.con eintreffen und wird trotzdem gefunden. Ob eine con kam,
        # ist fuer das Lesen gleich - es zaehlt die Antwort.
        marke = len(self.inds)
        self._sende_cemi(baue_cemi(L_DATA_REQ, self.quelle, ga, APCI_READ), ga)
        antwort = self._warte(lambda: next(
            (r for r in self.inds[marke:]
             if r.gruppe and r.ziel == ga and r.apci == APCI_RESPONSE), None), self.zeiten.lesen)
        if antwort is None:
            raise KnxFehler(f"keine Antwort auf das Lesen von {ga_text(ga)} binnen "
                            f"{self.zeiten.lesen:g} s - hat das Objekt in der ETS das "
                            "L-Flag (Lesen)?")
        return antwort

    def lesen_alle(self, ga: int, leer_erlaubt: bool = False) -> list[Cemi]:
        """Wie lesen(), wartet aber das ganze Lesefenster ab und liefert jede
        Antwort. Mehrere sind normal, wenn ausser dem Aktor noch jemand das
        Objekt mit L-Flag fuehrt - hier openknx (1.1.245). Wer nur die erste
        nimmt, haelt dessen Zwischenspeicher fuer den Aktor.
        leer_erlaubt: keine Antwort ist ein Messergebnis, kein Fehler (eine
        gestoerte Verbindung bleibt trotzdem ein KnxFehler)."""
        marke = len(self.inds)
        self._sende_cemi(baue_cemi(L_DATA_REQ, self.quelle, ga, APCI_READ), ga)
        self._warte(lambda: None, self.zeiten.lesen)  # quittiert weiter, sammelt alles
        antworten = [r for r in self.inds[marke:]
                     if r.gruppe and r.ziel == ga and r.apci == APCI_RESPONSE]
        if not antworten and not leer_erlaubt:
            raise KnxFehler(f"keine Antwort auf das Lesen von {ga_text(ga)} binnen "
                            f"{self.zeiten.lesen:g} s - hat das Objekt in der ETS das "
                            "L-Flag (Lesen)?")
        return antworten

    def schreiben(self, ga: int, klein: int = 0, daten: bytes = b"") -> Cemi | None:
        con = self._sende_cemi(baue_cemi(L_DATA_REQ, self.quelle, ga, APCI_WRITE, klein, daten), ga)
        zustand = "fehlt" if con is None else ("positiv" if con.bestaetigt_ok else "negativ")
        self._melde(f"geschrieben: {ga_text(ga)} = {daten[0] if daten else klein}, L_Data.con {zustand}")
        return con

    def warte_auf_status(self, ga: int, erwartet: int, marke: int) -> tuple[int, str]:
        """Rueckmeldung eines 1-Bit-Statusobjekts: erst auf die spontane
        Meldung des Aktors warten, bleibt sie aus, aktiv lesen."""
        def passend():
            for r in reversed(self.inds[marke:]):
                if r.gruppe and r.ziel == ga and r.apci in (APCI_WRITE, APCI_RESPONSE) \
                        and r.laenge == 1:
                    return r if r.klein == erwartet else None
            return None
        r = self._warte(passend, self.zeiten.status)
        if r is not None:
            return r.klein, "spontan gemeldet"
        r = self.lesen(ga)
        if r.laenge != 1:
            raise KnxFehler(f"{ga_text(ga)} liefert keinen 1-Bit-Wert")
        return r.klein, "aktiv gelesen"

    def halten(self, sekunden: float) -> None:
        # Kein sleep(): Auch waehrend des Haltens muss jedes Bustelegramm
        # quittiert werden, sonst trennt die Schnittstelle.
        self._warte(lambda: None, sekunden)

    def fremde_schreiber(self, ga: int) -> list[Cemi]:
        # Eigene Telegramme kommen nicht als L_Data.ind zurueck; die eigenen
        # Adressen sind trotzdem ausgenommen, falls eine Schnittstelle das anders haelt.
        eigene = {self.adresse, self.quelle}
        return [r for r in self.inds if r.gruppe and r.ziel == ga
                and r.apci == APCI_WRITE and r.quelle not in eigene]


# --- Gegenprobe: ein zweiter Tunnel hoert mit -----------------------------------
class Gegenprobe:
    """Zweiter Tunnel, der mithoert und meldet, mit welcher Quelladresse die
    Telegramme des ersten auf dem Bus stehen.

    Noetig, weil die Schnittstelle bei vorgegebener Quelle keine L_Data.con
    liefert (2026-09-12) - der erste Tunnel sieht dann gar nicht, was er
    gesendet hat. Die Schnittstelle reicht Telegramme eines Tunnels an die
    anderen weiter; so hat openknx in Stufe 2 die Schaltbefehle gesehen.

    Eigener Thread, weil auch dieser Tunnel jedes Bustelegramm binnen 1 s
    quittieren muss - waehrend der erste wartet oder haelt, liefe sonst die
    Wiederholung der Schnittstelle ins Leere, und sie trennte ihn. Die beiden
    Threads teilen nichts; die Liste der mitgelesenen Telegramme liest der
    Hauptthread erst, nachdem der Gegenprobe-Thread beendet ist.

    Verbunden wird VOR dem ersten Telegramm. Ist kein Tunnel mehr frei,
    endet der Lauf dort, ohne dass etwas gesendet wurde.
    """

    def __init__(self, a, erster: Tunnel, gas: tuple[int, ...]):
        self.erster = erster
        self.gas = set(gas)
        self.zeiten = a.zeiten
        # roh bleibt aus: Die Hex-Zeilen beider Tunnel liefen sonst ineinander.
        self.t = Tunnel(a.ip, a.port, a.zeiten, False, (), 0, praefix="Gegenprobe: ")
        self._stopp = threading.Event()
        self._thread: threading.Thread | None = None
        self._fehler: Exception | None = None

    def __enter__(self) -> "Gegenprobe":
        self.t.verbinden()
        self._thread = threading.Thread(target=self._lauf, daemon=True)
        self._thread.start()
        return self

    def _lauf(self) -> None:
        try:
            # 60 s Obergrenze: laenger lebt kein Lauf (--halten max. 30 s), und
            # ohne Heartbeat raeumt die Schnittstelle nach 120 s ab.
            self.t._warte(lambda: True if self._stopp.is_set() else None, 60.0)
        except (KnxFehler, OSError) as fehler:
            self._fehler = fehler

    def __exit__(self, *_) -> bool:
        # Nachlauf: Das letzte Telegramm des ersten Tunnels muss hier noch
        # ankommen, bevor der Thread endet.
        time.sleep(self.zeiten.nachlauf)
        self._stopp.set()
        self._thread.join(2)
        self.t.trennen()
        self._bericht()
        return False

    def _bericht(self) -> None:
        if self._fehler is not None:
            melde(f"Gegenprobe: gestoert - {self._fehler}")
        gesehen = [r for r in self.t.inds if r.gruppe and r.ziel in self.gas]
        for r in gesehen:
            melde(f"Gegenprobe sah: {ia_text(r.quelle)} -> {ga_text(r.ziel)} "
                  f"{APCI_NAME.get(r.apci, hex(r.apci))} {wert_text(r)}")
        # Welche davon hat der erste Tunnel gesendet? Erkennbar an Ziel und
        # Dienst (Lesen/Schreiben); die Antworten kommen vom Aktor. Ein fremdes
        # Telegramm mit gleichem Ziel und Dienst zaehlte mit - deshalb nennt
        # das Urteil alle Quellen, statt eine zu unterstellen.
        eigene = set(self.erster.gesendet)
        quellen = sorted({r.quelle for r in gesehen if (r.ziel, r.apci) in eigene})
        soll = self.erster.quelle or self.erster.adresse
        art = "wie vorgegeben" if self.erster.quelle else "die Tunneladresse"
        if not quellen:
            melde("Gegenprobe: die eigenen Telegramme hat der zweite Tunnel NICHT gesehen")
        elif quellen == [soll]:
            melde(f"Gegenprobe: eigene Telegramme stehen mit {ia_text(soll)} auf dem Bus - {art}")
        else:
            melde(f"Gegenprobe: eigene Telegramme stehen mit {', '.join(ia_text(q) for q in quellen)} "
                  f"auf dem Bus - erwartet war {ia_text(soll)} ({art})")


def _gegenprobe(a, erster: Tunnel, gas: tuple[int, ...]):
    return Gegenprobe(a, erster, gas) if a.gegenprobe else contextlib.nullcontext()


class Mithoerer:
    """Passiver Tunnel fuer die Dauer eines ganzen Laufs: meldet jedes
    Telegramm auf den beobachteten GAs sofort, mit Zeit und Quelle - auch die
    spontane Statusmeldung des Mischers am Ziel, die keine der kurzen
    Abfrageverbindungen sieht.

    Mit Heartbeat alle 30 s: Ein Mischerlauf dauert laenger als die 60 s,
    nach denen der erste faellig ist. Eigener Thread aus demselben Grund wie
    bei der Gegenprobe (Quittierfrist 1 s). Das darf das Werkzeug auf dem
    Mac; die Firmware darf es nicht (Analyse, Abschnitt 8)."""

    def __init__(self, a, gas: tuple[int, ...]):
        self.zeiten = a.zeiten
        self.t = Tunnel(a.ip, a.port, a.zeiten, False, gas, 0, praefix="Mithoerer: ")
        self._stopp = threading.Event()
        self._thread: threading.Thread | None = None

    def __enter__(self) -> "Mithoerer":
        self.t.verbinden()
        self._thread = threading.Thread(target=self._lauf, daemon=True)
        self._thread.start()
        return self

    def _lauf(self) -> None:
        try:
            while not self._stopp.is_set():
                self.t._warte(lambda: True if self._stopp.is_set() else None, 30.0)
                if not self._stopp.is_set():
                    self.t.zustand_pruefen()  # Heartbeat
        except (KnxFehler, OSError) as fehler:
            melde(f"Mithoerer: gestoert - {fehler}")

    def __exit__(self, *_) -> bool:
        time.sleep(self.zeiten.nachlauf)
        self._stopp.set()
        # Laenger als eine Antwortfrist warten: Der Thread kann gerade im
        # Heartbeat stecken; trennen() darf erst danach laufen.
        self._thread.join(self.zeiten.antwort + 1)
        self.t.trennen()
        return False


# --- Befehle ------------------------------------------------------------------
def _tunnel(a, beobachtet: tuple[int, ...] = ()) -> Tunnel:
    return Tunnel(a.ip, a.port, a.zeiten, a.roh, beobachtet, a.quelle)


def befehl_verbinden(a) -> int:
    # Stufe 0: kein einziges Telegramm auf den Bus
    with _tunnel(a) as t:
        t.zustand_pruefen()
    melde("GRUEN: Tunnel auf- und abgebaut, kein Bustelegramm gesendet")
    return 0


def befehl_lesen(a) -> int:
    # Stufe 1: nur Lesetelegramme, kein Zustandswechsel. Mit --alle wird das
    # ganze Lesefenster abgewartet - am 2026-09-12 beantwortete openknx das
    # Lesen von 6/4/12 schneller als jeder Aktor, aus seinem Zwischenspeicher.
    with _tunnel(a, tuple(a.gas)) as t, _gegenprobe(a, t, tuple(a.gas)):
        for ga in a.gas:
            antworten = t.lesen_alle(ga) if a.alle else [t.lesen(ga)]
            for r in antworten:
                melde(f"{ga_text(ga)} = {wert_text(r)}  (Antwort von {quelle_text(r.quelle)})")
            if a.alle:
                quellen = sorted({r.quelle for r in antworten})
                melde(f"{ga_text(ga)}: {len(antworten)} Antwort(en) von "
                      f"{', '.join(quelle_text(q) for q in quellen)}")
    melde("GRUEN: alle Objekte haben geantwortet")
    return 0


def befehl_schreiben(a) -> int:
    # Einzelwert, vor allem fuer das manuelle Zurueckstellen. Mit --status
    # liest es zurueck - ohne kann es eine negative oder fehlende
    # Busbestaetigung nicht aufloesen und meldet dann "unbekannt".
    daten = bytes((a.wert,)) if a.dpt == 5 else b""
    gas = (a.ga,) if a.status is None else (a.ga, a.status)
    with _tunnel(a, gas) as t, _gegenprobe(a, t, gas):
        marke = len(t.inds)
        con = t.schreiben(a.ga, klein=a.wert if a.dpt == 1 else 0, daten=daten)
        if a.status is None:
            wert = None
        else:
            wert, quelle = t.warte_auf_status(a.status, a.wert, marke)
    if a.status is None:
        if con is not None and con.bestaetigt_ok:
            melde("GRUEN: Schreibtelegramm vom Bus bestaetigt")
            return 0
        melde("ROT: Busbestaetigung negativ oder fehlend und keine Status-GA angegeben - "
              "Zustellung unbekannt. Zustand pruefen, z. B. mit --status")
        return 1
    if wert == a.wert:
        melde(f"GRUEN: Rueckmeldung {ga_text(a.status)} = {wert} ({quelle})")
        return 0
    melde(f"ROT: Rueckmeldung {ga_text(a.status)} = {wert} ({quelle}), erwartet {a.wert}")
    return 4


def _schalte(t: Tunnel, schalt: int, status: int, ziel: int) -> str:
    """Ein Schaltvorgang nach der Rueckleseregel, die auch der Firmwareschritt
    bekommen soll: schreiben, zuruecklesen, bei Abweichung genau einmal
    wiederholen. Die L_Data.con entscheidet nichts (Stufe 2, 2026-09-12).
    Ergebnis: 'ok', 'abweichung' oder 'fremd'."""
    for versuch in (1, 2):
        marke = len(t.inds)
        t.schreiben(schalt, klein=ziel)
        wert, quelle = t.warte_auf_status(status, ziel, marke)
        if wert == ziel:
            melde(f"Rueckmeldung {ga_text(status)} = {wert} ({quelle}) - passt")
            return "ok"
        melde(f"Rueckmeldung {ga_text(status)} = {wert} ({quelle}) - PASST NICHT, erwartet {ziel}")
        # Vor einer Wiederholung: Hat inzwischen jemand anderes geschaltet,
        # gilt dessen Befehl - eine Wiederholung wuerde ihn ueberschreiben.
        if t.fremde_schreiber(schalt):
            return "fremd"
        if versuch == 1:
            melde("einmal wiederholen")
    return "abweichung"


def _melde_fremd(t: Tunnel, a) -> None:
    for f in t.fremde_schreiber(a.schalt):
        melde(f"FREMDER Schreibzugriff: {ia_text(f.quelle)} -> {ga_text(f.ziel)} = {wert_text(f)}")
    melde("NICHT zurueckgestellt - der fremde Befehl gilt. Zustand pruefen!")


def befehl_schalten(a) -> int:
    # Stufe 2: 1-Bit-Aktor umschalten, zuruecklesen, zurueckstellen, zuruecklesen.
    offen = None  # Wert, auf den noch zurueckgestellt werden muss
    rueckstell = None
    gas = (a.schalt, a.status)
    try:
        with _tunnel(a, gas) as t, _gegenprobe(a, t, gas):
            r = t.lesen(a.status)
            if r.laenge != 1 or r.klein not in (0, 1):
                raise KnxFehler(f"{ga_text(a.status)} liefert keinen 1-Bit-Wert: {wert_text(r)}")
            ausgang, ziel = r.klein, 1 - r.klein
            rueckstell = (f"./knx_tunnel.py schreiben {a.ip} {ga_text(a.schalt)} {ausgang} "
                          f"--status {ga_text(a.status)}")
            melde(f"Ausgangszustand {ga_text(a.status)} = {ausgang}")
            melde(f"Falls der Lauf abbricht, zurueckstellen mit:  {rueckstell}")

            # Hin: umschalten; gehalten wird nur, wenn der Aktor wirklich umsteht
            offen = ausgang
            hin = _schalte(t, a.schalt, a.status, ziel)
            if hin == "ok":
                t.halten(a.halten)

            # Fremder Schreibzugriff seit dem Verbinden? Die Kaskaden Logik
            # schreibt ereignisgesteuert und holt nichts zurueck - ein blindes
            # Zurueckstellen wuerde ihren neuen Befehl ueberschreiben, und der
            # bliebe bis zum naechsten Ereignis falsch.
            if hin == "fremd" or t.fremde_schreiber(a.schalt):
                _melde_fremd(t, a)
                offen = None
                return 3
            # Restfenster: Ein fremder Befehl zwischen dieser Pruefung und dem
            # Zurueckstellen (Millisekunden) waere nicht mehr zu erkennen.

            # Zurueck: auch nach einer Abweichung auf dem Hinweg, denn gesendet
            # wurde - der Aktor kann umstehen, ohne dass die Ruecklesung es zeigte.
            zurueck = _schalte(t, a.schalt, a.status, ausgang)
            if zurueck == "fremd":
                _melde_fremd(t, a)
                offen = None
                return 3
            if zurueck == "ok":
                offen = None
        if hin == "ok" and zurueck == "ok":
            melde("GRUEN: umgeschaltet, zurueckgemeldet, zurueckgestellt, zurueckgemeldet")
            return 0
        wege = [w for w, e in (("Hinweg", hin), ("Rueckweg", zurueck)) if e != "ok"]
        melde(f"ROT: Ruecklesung passt auch nach der Wiederholung nicht ({', '.join(wege)})")
        return 4
    finally:
        if offen is not None:
            melde("ACHTUNG: Der Aktor steht moeglicherweise noch im Testzustand. "
                  f"Zurueckstellen mit:  {rueckstell}")


def _vom_aktor(r: Cemi) -> bool:
    # Nur der Mischeraktor zaehlt. openknx antwortet aus seinem Zwischenspeicher
    # (auf 6/4/14 am 2026-09-12 sogar dreimal) und ist im Notbetriebsfall nicht da.
    return r.quelle == MISCHER_AKTOR


def _aktorwert(t: Tunnel, ga: int) -> int | None:
    """Ein Lesetelegramm, das ganze Fenster abwarten, jede Antwort zeigen.
    Zurueck kommt nur der Wert des Mischeraktors - None, wenn er schwieg."""
    wert = None
    for r in t.lesen_alle(ga, leer_erlaubt=True):
        zaehlt = _vom_aktor(r) and r.laenge in (1, 2)
        melde(f"  Antwort {ga_text(ga)} = {wert_text(r)} von {quelle_text(r.quelle)}"
              + ("" if zaehlt else " - zaehlt nicht"))
        if zaehlt:
            wert = r.daten[0] if r.laenge == 2 else r.klein
    if wert is None:
        melde(f"  {ga_text(ga)}: keine Antwort vom Mischeraktor")
    return wert


def _mischerstatus(a) -> list[Cemi]:
    """Eine kurze Verbindung, ein Lesetelegramm, alle Antworten VOM AKTOR.
    openknx zaehlt nicht: Es antwortet aus seinem Zwischenspeicher und ist im
    Notbetriebsfall nicht da (2026-09-12)."""
    with _tunnel(a, (a.pos_status,)) as t:
        antworten = t.lesen_alle(a.pos_status)
    for r in antworten:
        melde(f"  Antwort {ga_text(a.pos_status)} = {wert_text(r)} von {quelle_text(r.quelle)}"
              + ("" if _vom_aktor(r) else " - zaehlt nicht"))
    return [r for r in antworten if _vom_aktor(r) and r.laenge == 2]


def _bestaetigung(t: Tunnel, a, marke: int, gesendet: list[tuple[float, str]],
                  schnell: bool) -> tuple[str, str]:
    """Rueckleseregel A des Mischers (Owner-Entscheid 2026-09-12, Analyse
    Abschnitt 8) - der Teil in der Befehlsverbindung. Belege nur vom
    Mischeraktor:
    - Eingang (6/4/13) = Position: Der Befehl steht im Aktor. Allein belegt
      das nichts - der Aktor speichert die Position auch unter Zwangsstellung
      (Lauf 4). Keine Antwort zaehlt wie eine Abweichung.
    - Bewegung (6/4/14) = 1 seit den Befehlen, oder der Status (6/4/12) am
      Ziel, wenn der Mischer schon dort steht (Lauf 2): Der Befehl wirkt.
    Die schnelle Regel gilt nur, wenn die Bewegung VOR den Befehlen auf 0
    stand ('schnell'). Faehrt der Mischer schon - Endlagenlauf nach einem
    Zwangsstellungswechsel, bis 144 s -, stammt eine 1 nicht sicher von
    diesen Befehlen; dann entscheidet die Endstellung.
    Ergebnis: ('gruen' | 'rot' | 'endstellung', Begruendung)."""
    # 1. Eingang - in beiden Faellen Pflicht
    melde("Eingang lesen:")
    eingang = _aktorwert(t, a.pos)
    if eingang != a.position:
        ist = "keine Antwort" if eingang is None else str(eingang)
        return "rot", f"Eingang meldet {ist} statt {a.position} - der Positionsbefehl steht nicht im Aktor"
    if not schnell:
        return "endstellung", "Bewegung stand vor den Befehlen nicht auf 0 - die schnelle Regel gilt nicht"

    def bewegung_eins():
        return next((r for r in t.inds[marke:]
                     if r.gruppe and r.ziel == a.bewegung and _vom_aktor(r)
                     and r.apci == APCI_WRITE and r.laenge == 1 and r.klein == 1), None)

    # 2. Kurz auf die spontane Meldung warten. Sie kann schon vor dem
    #    Positionsbefehl gekommen sein - deshalb zaehlt alles seit 'marke'.
    spontan = t._warte(bewegung_eins, t.zeiten.bewegung)
    if spontan is None:
        melde(f"keine spontane Bewegungsmeldung vom Aktor binnen {t.zeiten.bewegung:g} s")
    else:
        # Welches eigene Telegramm ging voraus? Das zeigt, ob schon das
        # Zuruecknehmen der Zwangsstellung den Mischer losfahren liess.
        # Gemessen wird die EMPFANGSzeit: Liegen zwei eigene Telegramme dichter
        # als die Laufzeit der Meldung (am Bus rund 0,1 s), kommt die Meldung auf das
        # erste erst nach dem zweiten an. Mit --quelle 1.1.250 trennt sie die
        # vergebliche Wartezeit auf die con (1 s); ohne kommt die con nach 22 ms.
        vorher = [(z, text) for z, text in gesendet if z <= spontan.zeit]
        bezug = (f", {(spontan.zeit - vorher[-1][0]) * 1000:.0f} ms nach dem Senden von {vorher[-1][1]}"
                 if vorher else "")
        if len(vorher) >= 2 and vorher[-1][0] - vorher[-2][0] < t.zeiten.zuordnung:
            bezug += (f" - ACHTUNG, nur {(vorher[-1][0] - vorher[-2][0]) * 1000:.0f} ms nach "
                      f"{vorher[-2][1]} gesendet, Zuordnung unsicher")
        melde(f"Bewegung 1 spontan vom Aktor{bezug}")

    # 3. Ohne spontane Meldung aktiv nachlesen - es zaehlt nur der Aktor
    if spontan is None:
        melde("Bewegung lesen:")
        bewegt = _aktorwert(t, a.bewegung) == 1
    else:
        bewegt = True
    if bewegt:
        return "gruen", "Mischer faehrt, Eingang passt"

    # 4. Keine Bewegung: Steht der Mischer schon am Ziel, faehrt er nicht
    #    und meldet auch nichts (Lauf 2) - dann belegt der Status
    melde("Status lesen:")
    status = _aktorwert(t, a.pos_status)
    if status is not None and abs(status - a.position) <= a.toleranz:
        return "gruen", f"Mischer steht schon am Ziel ({status}), Eingang passt"
    return "rot", ("keine Bewegung und nicht am Ziel - Zwangsstellung noch aktiv "
                   "oder Positionsbefehl nicht angekommen")


def befehl_mischer(a) -> int:
    """Referenz fuer den Firmwareschritt "Vorderhaus", Weg A (Analyse,
    Abschnitt 8): eine kurze Verbindung fuer beide Zwangsstellungen, die
    Position, die Pumpe und deren Ruecklesung; dann der Mischerstatus in
    eigenen kurzen Verbindungen, bis er die Position meldet.

    Mit --bewegung gilt die entschiedene Rueckleseregel A (_bestaetigung):
    Bewegung vor den Befehlen lesen, danach Eingang plus Bewegung oder
    Status, bei ROT genau eine Wiederholung. Stand die Bewegung vorher nicht
    auf 0, entscheidet die Endstellung mit verlaengerter Frist. Sonst laeuft
    die Abfrage bis zur Endstellung als Gegenkontrolle weiter und deckt ein
    falsches GRUEN auf.

    Kein Tunnel bleibt offen - in der Firmware blockiert die MQTT-
    Wiederverbindung loop() bis zu 2 s, ein offener Tunnel verpasste die
    1-s-Quittierfrist. Zwischen den Abfragen ist hier deshalb keiner offen
    (ausser dem optionalen Mithoerer, der nur fuer den Test da ist).
    Zurueckgestellt wird NICHTS: Das ist der Notbetrieb selbst."""
    pumpe_gas = () if a.ohne_pumpe else (a.pumpe, a.pumpe_status)
    gas = (a.zw_auf, a.zw_zu, a.pos, a.pos_status) + pumpe_gas \
        + (() if a.bewegung is None else (a.bewegung,))
    # Der Mithoerer sieht zusaetzlich 'Position ungueltig': Nach einem
    # ETS-Download faehrt der Aktor erst die volle Zeit samt Nachlauf (2026-09-12).
    with (Mithoerer(a, gas + (a.ungueltig,)) if a.mithoeren else contextlib.nullcontext()):
        # Ausgangsstellung - nur fuer das Protokoll und die erwartete Fahrzeit
        melde("Ausgangsstellung:")
        vorher = _mischerstatus(a)

        # 1. Eine kurze Verbindung fuer alle Befehle. Die Zwangsstellungen
        #    zuerst: Sie gehen vor, ein Positionsbefehl allein bliebe wirkungslos.
        #    Die Sendezeiten braucht die Bestaetigung fuer ihre Laufzeiten.
        gesendet: list[tuple[float, str]] = []
        bestaetigt = None      # ('gruen'|'rot'|'endstellung', Begruendung) - nur mit --bewegung
        pumpe = "ausgelassen"  # bleibt so mit --ohne-pumpe
        with _tunnel(a, gas) as t:
            # Regel A, zuerst: Faehrt der Mischer schon, gilt die schnelle
            # Regel nicht. Keine Antwort vom Aktor zaehlt wie "faehrt" - dann
            # entscheidet die Endstellung, die sicherere Seite.
            schnell = False
            if a.bewegung is not None:
                melde("Bewegung vor den Befehlen lesen:")
                schnell = _aktorwert(t, a.bewegung) == 0

            def sende(ga: int, klein: int = 0, daten: bytes = b"") -> None:
                gesendet.append((time.monotonic(), f"{ga_text(ga)} = {daten[0] if daten else klein}"))
                t.schreiben(ga, klein=klein, daten=daten)

            # Regel A: bei ROT genau eine Wiederholung der Mischertelegramme -
            # dieselben Werte noch einmal zu schreiben ist unschaedlich
            for versuch in (1, 2):
                marke = len(t.inds)
                if a.ohne_zwang:
                    melde("Negativprobe: Zwangsstellungen bleiben stehen, der Mischer darf nicht fahren")
                else:
                    sende(a.zw_auf)
                    sende(a.zw_zu)
                sende(a.pos, daten=bytes((a.position,)))
                start = gesendet[-1][0]  # Fahrzeit und Frist zaehlen ab dem letzten Positionsbefehl
                if a.bewegung is None:
                    break
                bestaetigt = _bestaetigung(t, a, marke, gesendet, schnell)
                if bestaetigt[0] != "rot":
                    break
                if versuch == 1:
                    melde(f"Bestaetigung ROT ({bestaetigt[1]}) - einmal wiederholen")
            if not a.ohne_pumpe:
                pumpe = _schalte(t, a.pumpe, a.pumpe_status, 1)
        if vorher and not a.ohne_zwang:
            fahrt = abs(vorher[0].daten[0] - a.position) / 255 * 120
            melde(f"Mischer faehrt von {vorher[0].daten[0]} auf {a.position}: "
                  f"erwartet rund {fahrt:.0f} s (120 s je vollem Hub)")

        # 2. Mischerstatus: alle --takt Sekunden eine eigene kurze Verbindung.
        #    Der Aktor meldet erst in der Zielstellung; bis dahin liefert das
        #    Lesen die alte Stellung.
        # Regel A, Rueckfall: Fuhr der Mischer schon, ist die Endstellung das
        # Urteil - mit einer Frist, die den laufenden Endlagenlauf (bis 144 s)
        # und danach den Weg zur Position abdeckt.
        frist = a.frist
        if bestaetigt is not None and bestaetigt[0] == "endstellung":
            frist = max(a.frist, a.zeiten.endlauf)
            melde(f"Regel A: {bestaetigt[1]} - Urteil ueber die Endstellung, Frist {frist:g} s")
        erreicht = None
        while erreicht is None and time.monotonic() - start < frist:
            time.sleep(a.takt)  # hier ist kein Tunnel offen
            vergangen = time.monotonic() - start
            melde(f"Abfrage nach {vergangen:.0f} s:")
            try:
                aktor = _mischerstatus(a)
            except KnxFehler as fehler:
                melde(f"  gescheitert: {fehler}")
                continue
            if not aktor:
                melde("  nur openknx hat geantwortet - kein Beleg vom Aktor")
                continue
            if any(abs(r.daten[0] - a.position) <= a.toleranz for r in aktor):
                erreicht = vergangen

    # Ergebnis. Mit --bewegung wird die Bestaetigung gegen die Endstellung
    # gehalten: Nur wenn beide dasselbe sagen, taugt die schnelle Regel.
    if pumpe == "fremd":
        melde("ROT: fremder Schreibzugriff auf die Pumpe - deren Befehl gilt, Zustand pruefen")
        return 3
    teile = []
    if pumpe not in ("ok", "ausgelassen"):
        teile.append("Pumpe meldet nicht ein")
    if bestaetigt is not None and bestaetigt[0] != "endstellung":
        gruen, grund = bestaetigt[0] == "gruen", bestaetigt[1]
        melde(f"Bestaetigung ueber {ga_text(a.bewegung)}: {'GRUEN' if gruen else 'ROT'} - {grund}")
        if gruen and erreicht is None:
            teile.append("Bestaetigung war GRUEN, der Mischer kam aber nicht an - FALSCHES GRUEN")
        elif not gruen and erreicht is not None:
            teile.append("Bestaetigung war ROT, der Mischer kam trotzdem an - Regel zu streng")
        elif not gruen:
            teile.append("Bestaetigung ROT")
    if erreicht is None:
        teile.append(f"Mischer meldet {a.position} nicht binnen {frist:g} s")
    if not teile:
        melde(f"GRUEN: {'Pumpe ein, ' if pumpe == 'ok' else ''}Mischer auf {a.position} "
              f"(gemeldet bei der Abfrage nach {erreicht:.0f} s)")
        return 0
    melde("ROT: " + "; ".join(teile))
    return 4


def fuehre_aus(befehl, a) -> int:
    try:
        return befehl(a)
    except KnxFehler as fehler:
        melde(f"FEHLER: {fehler}")
        return 1
    except OSError as fehler:
        melde(f"FEHLER im Netzwerk: {fehler}")
        return 1
    except KeyboardInterrupt:
        melde("abgebrochen")
        return 1


# --- Selbsttest -----------------------------------------------------------------
class Simulator(threading.Thread):
    """Nachgebildete Schnittstelle samt Pumpenaktor, nur fuer den Selbsttest.

    Bildet ab, was dieser Client benutzt - nicht mehr. Ein gruener
    Selbsttest belegt die Logik des Werkzeugs, nicht das Verhalten der echten
    Schnittstelle. Der Anlage nachgebildet (2026-09-12): Tunneladressen ab
    1.1.148, ctrl1 der con 9c/9d, keine con bei vorgegebener Quelle, und
    Telegramme eines Tunnels erscheinen bei den anderen als L_Data.ind.
    Welche Quelle bei vorgegebener Adresse auf dem Bus steht, ist an der
    Anlage noch offen - hier waehlbar ueber quelle_ersetzen.
    """
    KANAELE = (7, 8)
    ADRESSEN = (0x1194, 0x1195)  # 1.1.148, 1.1.149
    AKTOR = 0x113C               # 1.1.60 - Pumpenaktor
    FREMD = 0x11F5               # 1.1.245 - openknx
    MISCHER_AKTOR = 0x1127       # 1.1.39 - beantwortet 6/4/12 (2026-09-12)
    ZW_AUF, ZW_ZU, POS, POS_STATUS = 0x3411, 0x3410, 0x340D, 0x340C  # 6/4/17, 6/4/16, 6/4/13, 6/4/12
    BEWEGUNG = 0x340E            # 6/4/14 - 1 von Losfahren bis Ziel (Owner, 2026-09-12)
    HUB_S = 1.2                  # voller Hub im Simulator (an der Anlage 120 s)

    def __init__(self, schalt: int, status: int, mischer: int, kanaele: int = 2, stumm=False,
                 stumm_nach_schreiben=0, ack_verlieren=False, fremd_schreiben=False,
                 con_negativ=False, schreib_verluste=0, negativ_zugestellt=False,
                 quelle_ersetzen=False, con_bei_fremder_quelle=False,
                 lesen_antwortet=True, status_spontan=True, openknx_antwortet=False,
                 zwang_klemmt=False, mischer_lesen_antwortet=True, pos_verloren=False,
                 freigabe_faehrt=False, mischer_start: dict | None = None):
        super().__init__(daemon=True)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(("127.0.0.1", 0))
        self.sock.settimeout(0.05)
        self.port = self.sock.getsockname()[1]
        self.schalt, self.status = schalt, status
        self.werte = {schalt: (0, b""), status: (0, b""), mischer: (0, bytes((128,)))}
        self.kanaele = self.KANAELE[:max(1, min(kanaele, len(self.KANAELE)))]
        # Fehlerbilder
        self.stumm = stumm                            # antwortet auf gar nichts
        self.stumm_nach_schreiben = stumm_nach_schreiben  # verstummt nach n Schreibvorgaengen
        self.ack_verlieren = ack_verlieren            # erste Anfrage kommt nicht an
        self.fremd_schreiben = fremd_schreiben        # openknx schaltet nach dem ersten Schreiben mit
        self.con_negativ = con_negativ                # jede con negativ, nichts zugestellt
        self.schreib_verluste = schreib_verluste      # erste n Schreibtelegramme: con negativ, nicht zugestellt
        self.negativ_zugestellt = negativ_zugestellt  # Schreiben: con negativ, trotzdem zugestellt (Stufe 2)
        self.quelle_ersetzen = quelle_ersetzen        # vorgegebene Quelle auf dem Bus durch Tunneladresse ersetzen
        self.con_bei_fremder_quelle = con_bei_fremder_quelle  # abweichend von der Anlage doch eine con
        self.lesen_antwortet = lesen_antwortet
        self.status_spontan = status_spontan
        self.openknx_antwortet = openknx_antwortet    # openknx beantwortet Lesen aus dem Speicher (2026-09-12)
        self.zwang_klemmt = zwang_klemmt              # Zwangsstellung ZU laesst sich nicht zuruecknehmen
        self.mischer_lesen_antwortet = mischer_lesen_antwortet
        self.pos_verloren = pos_verloren              # nur der Positionsbefehl erreicht den Aktor nicht
        self.freigabe_faehrt = freigabe_faehrt        # Gegenfall: Freigabe faehrt auf den Eingang (Anlage: nein)
        # Mischermodell, Stand wie am 2026-09-12: Zwangsstellung ZU aktiv, Stellung 0
        self.m = {"auf": 0, "zu": 1, "eingang": 46, "status": 0, "ziel": None, "ankunft": 0.0}
        self.m.update(mischer_start or {})
        # Zustand und Protokoll fuer die Pruefungen
        self.verbindungen: dict[int, dict] = {}       # Kanal -> Endpunkt, Adresse, Zaehler
        self.gesendet: list[tuple[int, int]] = []     # (Kanal, Sequenz)
        self.quittiert: set[tuple[int, int]] = set()
        self.schreibvorgaenge: list[tuple[int, int]] = []
        self.quellen: list[int] = []
        self.getrennt = 0
        self._verloren = False
        self._fremd_gesendet = False
        self._stopp = threading.Event()

    def stoppen(self) -> None:
        self._stopp.set()
        self.join(2)
        self.sock.close()

    def _an(self, kanal: int, cemi: bytes) -> None:
        v = self.verbindungen[kanal]
        self.gesendet.append((kanal, v["tx"]))
        self.sock.sendto(baue_tunneling_request(kanal, v["tx"], cemi), v["ep"])
        v["tx"] = (v["tx"] + 1) & 0xFF

    def _an_alle(self, cemi: bytes, ausser: int | None = None) -> None:
        for kanal in list(self.verbindungen):
            if kanal != ausser:
                self._an(kanal, cemi)

    def _verstummt(self) -> bool:
        return self.stumm or (self.stumm_nach_schreiben > 0
                              and len(self.schreibvorgaenge) >= self.stumm_nach_schreiben)

    def _mischer_neu(self, ga: int) -> None:
        # Zwangsstellung geht vor; sonst gilt der Positionseingang - aber erst
        # mit einem Positionsbefehl: Das Zuruecknehmen der Zwangsstellung
        # allein liess den Aktor an der Anlage stehen (2026-09-12, Lauf 1).
        # Was er mitten in einem Endlagenlauf tut, ist nicht gemessen; hier
        # faehrt er sein altes Ziel weiter.
        m = self.m
        if m["zu"] or m["auf"]:
            ziel = 0 if m["zu"] else 255
        elif ga == self.POS or self.freigabe_faehrt:
            ziel = m["eingang"]
        else:
            return
        if ziel == m["status"] and m["ziel"] is None:
            # steht schon dort: sofort melden (an der Anlage nach 0,4 s)
            self._an_alle(baue_cemi(L_DATA_IND, self.MISCHER_AKTOR, self.POS_STATUS, APCI_WRITE,
                                    daten=bytes((ziel,))))
            return
        # Bewegung meldet der Aktor beim Losfahren (an der Anlage 230-330 ms
        # nach dem Befehl), nicht bei einem neuen Ziel mitten in der Fahrt
        if m["ziel"] is None:
            self._an_alle(baue_cemi(L_DATA_IND, self.MISCHER_AKTOR, self.BEWEGUNG, APCI_WRITE, 1))
        # Fahrt proportional zum Weg, vereinfacht ab der zuletzt gemeldeten Stellung
        m["ziel"] = ziel
        m["ankunft"] = time.monotonic() + abs(ziel - m["status"]) / 255 * self.HUB_S

    def _mischer_tick(self) -> None:
        # Am Ziel angekommen: Status uebernehmen und EINMAL spontan melden,
        # danach Bewegung 0 - wer gerade nicht verbunden ist, verpasst beides
        # (wie am Bus). Den Nachlauf in die Endlagen bildet der Simulator nicht ab.
        m = self.m
        if m["ziel"] is not None and time.monotonic() >= m["ankunft"]:
            m["status"], m["ziel"] = m["ziel"], None
            self._an_alle(baue_cemi(L_DATA_IND, self.MISCHER_AKTOR, self.POS_STATUS, APCI_WRITE,
                                    daten=bytes((m["status"],))))
            self._an_alle(baue_cemi(L_DATA_IND, self.MISCHER_AKTOR, self.BEWEGUNG, APCI_WRITE, 0))

    def run(self) -> None:
        while not self._stopp.is_set():
            self._mischer_tick()
            try:
                dg, absender = self.sock.recvfrom(1024)
            except socket.timeout:
                continue
            except OSError:
                break
            if self._verstummt():
                continue
            dienst, rumpf = zerlege_knxip(dg)
            if dienst == CONNECT_REQUEST:
                frei = [k for k in self.kanaele if k not in self.verbindungen]
                if not frei:
                    self.sock.sendto(kopf(CONNECT_RESPONSE, 2) + bytes((0, 0x24)), absender)
                    continue
                kanal = frei[0]
                adresse = self.ADRESSEN[self.KANAELE.index(kanal)]
                self.verbindungen[kanal] = {"ep": zerlege_hpai(rumpf[8:16]), "adresse": adresse,
                                            "rx": 0, "tx": 0}
                antwort = (bytes((kanal, 0)) + hpai(("127.0.0.1", self.port))
                           + bytes((0x04, 0x04, adresse >> 8, adresse & 0xFF)))
                self.sock.sendto(kopf(CONNECT_RESPONSE, len(antwort)) + antwort, absender)
            elif dienst == CONNECTIONSTATE_REQUEST:
                status = 0 if rumpf[0] in self.verbindungen else 0x21
                self.sock.sendto(kopf(CONNECTIONSTATE_RESPONSE, 2) + bytes((rumpf[0], status)), absender)
            elif dienst == DISCONNECT_REQUEST:
                self.verbindungen.pop(rumpf[0], None)
                self.sock.sendto(baue_disconnect_response(rumpf[0]), absender)
                self.getrennt += 1
            elif dienst == TUNNELING_ACK:
                self.quittiert.add((rumpf[1], rumpf[2]))
            elif dienst == TUNNELING_REQUEST:
                self._tunnel(rumpf)

    def _tunnel(self, rumpf: bytes) -> None:
        kanal, seq = rumpf[1], rumpf[2]
        v = self.verbindungen.get(kanal)
        if v is None:
            return
        if self.ack_verlieren and not self._verloren:
            self._verloren = True  # erste Anfrage geht "verloren" - der Client muss wiederholen
            return
        if seq == v["rx"]:
            v["rx"] = (seq + 1) & 0xFF
            neu = True
        elif seq == (v["rx"] - 1) & 0xFF:
            neu = False  # Wiederholung: quittieren, nicht noch einmal ausfuehren
        else:
            return
        self.sock.sendto(baue_tunneling_ack(kanal, seq), v["ep"])
        if not neu:
            return
        r = zerlege_cemi(rumpf[4:])
        self.quellen.append(r.quelle)
        verloren = r.apci == APCI_WRITE and self.schreib_verluste > 0
        if verloren:
            self.schreib_verluste -= 1
        # Nur der Positionsbefehl geht verloren: die Falle, die das Zuruecklesen
        # des Eingangs schliessen soll (Bewegung kommt trotzdem, auf den alten Wert)
        verloren = verloren or (self.pos_verloren and r.apci == APCI_WRITE and r.ziel == self.POS)
        nicht_zugestellt = self.con_negativ or verloren
        negativ = nicht_zugestellt or (self.negativ_zugestellt and r.apci == APCI_WRITE)
        # Quelle auf dem Bus: 0.0.0 wird zur Tunneladresse; eine vorgegebene
        # uebernimmt der Simulator, ausser quelle_ersetzen ist gesetzt.
        fremde_quelle = r.quelle not in (0, v["adresse"])
        quelle = r.quelle if fremde_quelle and not self.quelle_ersetzen else v["adresse"]
        # Bestaetigung: ctrl1 9c/9d wie an der Anlage - und bei vorgegebener
        # Quelle gar keine (beobachtet 2026-09-12)
        if not fremde_quelle or self.con_bei_fremder_quelle:
            ctrl1 = 0x9C | (0x01 if negativ else 0x00)
            self._an(kanal, baue_cemi(L_DATA_CON, quelle, r.ziel, r.apci, r.klein, r.daten, ctrl1))
        if nicht_zugestellt:
            return
        # Das Telegramm steht auf dem Bus: Die anderen Tunnel sehen es.
        self._an_alle(baue_cemi(L_DATA_IND, quelle, r.ziel, r.apci, r.klein, r.daten), ausser=kanal)
        if r.apci == APCI_READ and r.ziel == self.POS_STATUS:
            # Mischerstatus: waehrend der Fahrt die ALTE Stellung
            stellung = bytes((self.m["status"],))
            if self.openknx_antwortet:
                self._an_alle(baue_cemi(L_DATA_IND, self.FREMD, r.ziel, APCI_RESPONSE, daten=stellung))
            if self.mischer_lesen_antwortet:
                self._an_alle(baue_cemi(L_DATA_IND, self.MISCHER_AKTOR, r.ziel, APCI_RESPONSE,
                                        daten=stellung))
        if r.apci == APCI_READ and r.ziel in (self.POS, self.BEWEGUNG):
            # Eingang: der zuletzt empfangene Positionswert; Bewegung: 1 waehrend der Fahrt.
            # openknx beantwortete 6/4/14 an der Anlage aus dem Speicher - hier stets veraltet 0.
            if self.openknx_antwortet and r.ziel == self.BEWEGUNG:
                self._an_alle(baue_cemi(L_DATA_IND, self.FREMD, r.ziel, APCI_RESPONSE, 0))
            if self.mischer_lesen_antwortet:
                antwort = ({"daten": bytes((self.m["eingang"],))} if r.ziel == self.POS
                           else {"klein": int(self.m["ziel"] is not None)})
                self._an_alle(baue_cemi(L_DATA_IND, self.MISCHER_AKTOR, r.ziel, APCI_RESPONSE, **antwort))
        if r.apci == APCI_WRITE and r.ziel in (self.ZW_AUF, self.ZW_ZU, self.POS):
            if r.ziel == self.ZW_AUF:
                self.m["auf"] = r.klein & 0x01
            elif r.ziel == self.ZW_ZU and not (self.zwang_klemmt and not r.klein & 0x01):
                self.m["zu"] = r.klein & 0x01
            elif r.ziel == self.POS and r.daten:
                self.m["eingang"] = r.daten[0]
            self._mischer_neu(r.ziel)
        if r.apci == APCI_READ and self.openknx_antwortet and r.ziel in self.werte:
            # openknx ist schneller als der Aktor - so an der Anlage gesehen
            klein, daten = self.werte[r.ziel]
            self._an_alle(baue_cemi(L_DATA_IND, self.FREMD, r.ziel, APCI_RESPONSE, klein, daten))
        if r.apci == APCI_READ and self.lesen_antwortet and r.ziel in self.werte:
            klein, daten = self.werte[r.ziel]
            self._an_alle(baue_cemi(L_DATA_IND, self.AKTOR, r.ziel, APCI_RESPONSE, klein, daten))
        elif r.apci == APCI_WRITE:
            self.schreibvorgaenge.append((r.ziel, r.klein))
            self._schalte(r.ziel, r.klein)
            if self.fremd_schreiben and not self._fremd_gesendet:
                self._fremd_gesendet = True
                self._an_alle(baue_cemi(L_DATA_IND, self.FREMD, r.ziel, APCI_WRITE, r.klein))
                self._schalte(r.ziel, r.klein)

    def _schalte(self, ga: int, klein: int) -> None:
        # Nur die Objekte des Pumpenaktors; die Mischerobjekte fuehrt das
        # Mischermodell - sonst beantwortete 1.1.60 das Lesen von 6/4/13.
        if ga not in self.werte:
            return
        self.werte[ga] = (klein, b"")
        if ga == self.schalt:
            self.werte[self.status] = (klein, b"")
            if self.status_spontan:
                self._an_alle(baue_cemi(L_DATA_IND, self.AKTOR, self.status, APCI_WRITE, klein))


def selbsttest() -> int:
    fehler = 0

    def pruefe(name: str, ok: bool, ausgabe: str = "") -> None:
        nonlocal fehler
        print(f"{'OK  ' if ok else 'FAIL'} {name}", flush=True)
        if not ok:
            fehler += 1
            if ausgabe:
                print("     " + ausgabe.replace("\n", "\n     "), flush=True)

    # 1. Rahmen gegen die Rohbytes aus den Tests von xknx - eine unabhaengige
    #    Referenz, damit hier nicht der eigene Code gegen das eigene
    #    Verstaendnis geprueft wird (github.com/XKNX/xknx, test/knxip_tests/,
    #    test/cemi_tests/cemi_frame_test.py).
    pruefe("CONNECT_REQUEST wie xknx",
           baue_connect_request(("192.168.42.1", 33941), ("192.168.42.1", 52393))
           == bytes.fromhex("06 10 02 05 00 1A 08 01 C0 A8 2A 01 84 95 08 01"
                            "C0 A8 2A 01 CC A9 04 04 02 00"))
    a = zerlege_connect_response(zerlege_knxip(bytes.fromhex(
        "06 10 02 06 00 14 01 00 08 01 C0 A8 2A 0A 0E 57 04 04 11 FF"))[1])
    pruefe("CONNECT_RESPONSE wie xknx",
           (a.kanal, a.status, a.daten_ep, a.adresse) == (1, 0, ("192.168.42.10", 3671), 0x11FF))
    pruefe("TUNNELING_REQUEST GroupValueWrite 9/0/8 = 1 wie xknx",
           baue_tunneling_request(1, 23, baue_cemi(L_DATA_REQ, 0, ga_aus_text("9/0/8"), APCI_WRITE, 1))
           == bytes.fromhex("06 10 04 20 00 15 04 01 17 00 11 00 BC E0 00 00 48 08 01 00 81"))
    pruefe("TUNNELING_ACK wie xknx",
           baue_tunneling_ack(0x2A, 0x17) == bytes((0x06, 0x10, 0x04, 0x21, 0x00, 0x0A, 0x04, 0x2A, 0x17, 0x00)))
    pruefe("CONNECTIONSTATE_REQUEST wie xknx",
           baue_connectionstate_request(0x15, ("192.168.200.12", 50100))
           == bytes.fromhex("06 10 02 07 00 10 15 00 08 01 C0 A8 C8 0C C3 B4"))
    pruefe("DISCONNECT_REQUEST wie xknx",
           baue_disconnect_request(0x15, ("192.168.200.12", 50100))
           == bytes.fromhex("06 10 02 09 00 10 15 00 08 01 C0 A8 C8 0C C3 B4"))
    c = zerlege_cemi(bytes.fromhex("2e00bcf011fd094e0103f1"))
    pruefe("L_Data.con aus dem xknx-Mitschnitt (1.1.253 -> 1/1/78, positiv)",
           c.code == L_DATA_CON and c.quelle == 0x11FD and c.ziel == ga_aus_text("1/1/78")
           and c.bestaetigt_ok)

    # 2. Rohbytes von der Anlage (Stufen 1 und 2, 2026-09-12): Die
    #    Schnittstelle schickt ctrl1 9c statt bc zurueck - positiv heisst Bit 0.
    c = zerlege_cemi(bytes.fromhex("2e 00 9c e0 11 94 34 14 01 00 80"))
    pruefe("L_Data.con der Anlage, ctrl1 9c: positiv", c.bestaetigt_ok and c.quelle == 0x1194)
    c = zerlege_cemi(bytes.fromhex("2e 00 9d e0 11 94 34 14 01 00 81"))
    pruefe("L_Data.con der Anlage, ctrl1 9d: negativ", not c.bestaetigt_ok and c.klein == 1)
    c = zerlege_cemi(bytes.fromhex("29 00 bc e0 11 3c 34 15 01 00 41"))
    pruefe("L_Data.ind der Anlage: GroupValueResponse 6/4/21 = 1 von 1.1.60",
           c.apci == APCI_RESPONSE and c.klein == 1 and c.quelle == 0x113C and c.ziel == 0x3415)

    # 3. Nach Spezifikation, ohne Vorlage: 1-Byte-Wert (DPT 5), vorgegebene
    #    Quelle und die Adressen dieser Anlage
    pruefe("GroupValueWrite 1 Byte (DPT 5, 128 = 50 %)",
           baue_cemi(L_DATA_REQ, 0, 0x3414, APCI_WRITE, daten=bytes((128,)))
           == bytes.fromhex("11 00 BC E0 00 00 34 14 02 00 80 80"))
    pruefe("GroupValueRead",
           baue_cemi(L_DATA_REQ, 0, 0x3415, APCI_READ) == bytes.fromhex("11 00 BC E0 00 00 34 15 01 00 00"))
    pruefe("GroupValueRead mit Quelle 1.1.250 (Rohbytes wie an der Anlage gesendet)",
           baue_cemi(L_DATA_REQ, 0x11FA, 0x3415, APCI_READ) == bytes.fromhex("11 00 BC E0 11 FA 34 15 01 00 00"))
    pruefe("GA 6/4/20 <-> 0x3414", ga_aus_text("6/4/20") == 0x3414 and ga_text(0x3414) == "6/4/20")
    pruefe("PA 1.1.250 <-> 0x11FA", ia_aus_text("1.1.250") == 0x11FA and ia_text(0x11FA) == "1.1.250")
    for falsch in ("6/4", "32/0/0", "6/8/0", "6/4/256", "a/b/c", "0/0/0", "6/-1/0"):
        try:
            ga_aus_text(falsch)
            abgelehnt = False
        except ValueError:
            abgelehnt = True
        pruefe(f"GA '{falsch}' abgelehnt", abgelehnt)
    for falsch in ("1.1", "16.0.1", "1.16.1", "1.1.256", "1.1.0", "a.b.c"):
        try:
            ia_aus_text(falsch)
            abgelehnt = False
        except ValueError:
            abgelehnt = True
        pruefe(f"PA '{falsch}' abgelehnt", abgelehnt)

    # 4. Ablaeufe gegen den Simulator - Zeiten verkuerzt, sonst dauern die
    #    Fehlerfall-Laeufe so lange wie an der Anlage.
    schalt, status, mischer = ga_aus_text("6/4/20"), ga_aus_text("6/4/21"), ga_aus_text("6/4/30")
    schnell = Zeiten(antwort=1.0, trennen=0.5, ack=0.2, con=0.2, lesen=0.5, status=0.3, bewegung=0.3,
                     zuordnung=0.1, endlauf=3.0, nachlauf=0.1)

    def lauf(sim_optionen: dict, befehl, **argumente):
        argumente.setdefault("quelle", 0)
        argumente.setdefault("gegenprobe", False)
        if befehl is befehl_schreiben:
            argumente.setdefault("status", None)
        if befehl is befehl_lesen:
            argumente.setdefault("alle", False)
        sim = Simulator(schalt, status, mischer, **sim_optionen)
        sim.start()
        ns = argparse.Namespace(ip="127.0.0.1", port=sim.port, zeiten=schnell, roh=False, **argumente)
        puffer = io.StringIO()
        with contextlib.redirect_stdout(puffer):
            code = fuehre_aus(befehl, ns)
        time.sleep(0.1)  # letzte ACKs beim Simulator ankommen lassen
        sim.stoppen()
        return code, puffer.getvalue(), sim

    def alles_quittiert(sim: Simulator) -> bool:
        return set(sim.gesendet) <= sim.quittiert

    hin_und_zurueck = [(schalt, 1), (schalt, 0)]
    s2 = {"schalt": schalt, "status": status, "halten": 0.2}
    q250 = 0x11FA

    code, aus, sim = lauf({}, befehl_verbinden)
    pruefe("verbinden: gruen, getrennt", code == 0 and sim.getrennt == 1, aus)

    code, aus, sim = lauf({}, befehl_lesen, gas=[status, mischer])
    pruefe("lesen: 1 Bit und 1 Byte, alles quittiert, Quelle von der Schnittstelle",
           code == 0 and "6/4/21 = 0 " in aus and "6/4/30 = 128 " in aus and alles_quittiert(sim)
           and "1.1.148 (von der Schnittstelle eingesetzt)" in aus and sim.quellen == [0, 0], aus)

    # Quelladresse: an der Anlage kam bei vorgegebener Quelle keine con
    code, aus, sim = lauf({}, befehl_lesen, gas=[status], quelle=q250)
    pruefe("lesen --quelle 1.1.250: keine con, die Antwort entscheidet -> gruen",
           code == 0 and "bei vorgegebener Quelle liefert die Schnittstelle keine" in aus
           and sim.quellen == [q250], aus)

    code, aus, sim = lauf({}, befehl_lesen, gas=[status], quelle=q250, gegenprobe=True)
    pruefe("lesen --quelle --gegenprobe: Schnittstelle uebernimmt -> 1.1.250 gesehen",
           code == 0 and "Gegenprobe: verbunden: Kanal 8, Tunneladresse 1.1.149" in aus
           and "Gegenprobe sah: 1.1.250 -> 6/4/21 read" in aus
           and "stehen mit 1.1.250 auf dem Bus - wie vorgegeben" in aus
           and sim.getrennt == 2 and alles_quittiert(sim), aus)

    code, aus, sim = lauf({"quelle_ersetzen": True}, befehl_lesen, gas=[status], quelle=q250, gegenprobe=True)
    pruefe("lesen --quelle --gegenprobe: Schnittstelle ersetzt -> 1.1.148 gesehen",
           code == 0 and "stehen mit 1.1.148 auf dem Bus - erwartet war 1.1.250" in aus, aus)

    code, aus, sim = lauf({}, befehl_lesen, gas=[status], gegenprobe=True)
    pruefe("lesen --gegenprobe ohne Quelle: Tunneladresse gesehen",
           code == 0 and "stehen mit 1.1.148 auf dem Bus - die Tunneladresse" in aus, aus)

    code, aus, sim = lauf({"con_bei_fremder_quelle": True}, befehl_lesen, gas=[status], quelle=q250)
    pruefe("lesen --quelle, falls doch eine con kaeme: 'uebernimmt'", code == 0 and "uebernimmt" in aus, aus)

    code, aus, sim = lauf({"con_bei_fremder_quelle": True, "quelle_ersetzen": True},
                          befehl_lesen, gas=[status], quelle=q250)
    pruefe("lesen --quelle, falls doch eine con kaeme: 'ERSETZT'", code == 0 and "ERSETZT" in aus, aus)

    # Wer antwortet? openknx aus dem Speicher, der Aktor selbst - oder nur einer
    code, aus, sim = lauf({"openknx_antwortet": True}, befehl_lesen, gas=[status], alle=True)
    pruefe("lesen --alle: openknx UND Aktor antworten, beide benannt",
           code == 0 and "2 Antwort(en) von 1.1.60 = Aktor Pumpe VH, 1.1.245 = openknx/ioBroker" in aus, aus)

    code, aus, sim = lauf({"openknx_antwortet": True, "lesen_antwortet": False}, befehl_lesen,
                          gas=[status], alle=True)
    pruefe("lesen --alle: nur openknx antwortet -> sichtbar, dass der Aktor schweigt",
           code == 0 and "1 Antwort(en) von 1.1.245 = openknx/ioBroker" in aus, aus)

    code, aus, sim = lauf({"kanaele": 1}, befehl_lesen, gas=[status], gegenprobe=True)
    pruefe("--gegenprobe ohne freien Tunnel -> Exit 1, nichts gesendet",
           code == 1 and "alle Tunnel belegt" in aus and sim.quellen == [], aus)

    # Schalten
    code, aus, sim = lauf({}, befehl_schalten, **s2)
    pruefe("schalten: hin und zurueck, Aktor wieder auf 0",
           code == 0 and sim.schreibvorgaenge == hin_und_zurueck
           and sim.werte[schalt] == (0, b"") and alles_quittiert(sim) and sim.getrennt == 1, aus)

    code, aus, sim = lauf({}, befehl_schalten, quelle=q250, gegenprobe=True, **s2)
    pruefe("schalten --quelle --gegenprobe: ohne con gruen, Schreibtelegramme mit 1.1.250 gesehen",
           code == 0 and sim.schreibvorgaenge == hin_und_zurueck and "L_Data.con fehlt" in aus
           and "Gegenprobe sah: 1.1.250 -> 6/4/20 write 1" in aus
           and "stehen mit 1.1.250 auf dem Bus - wie vorgegeben" in aus and alles_quittiert(sim), aus)

    code, aus, sim = lauf({"negativ_zugestellt": True}, befehl_schalten, **s2)
    pruefe("schalten: negative con, trotzdem zugestellt (Fall Stufe 2) -> gruen, keine Wiederholung",
           code == 0 and "NEGATIV" in aus and sim.schreibvorgaenge == hin_und_zurueck
           and "wiederholen" not in aus and "ACHTUNG" not in aus, aus)

    code, aus, sim = lauf({"schreib_verluste": 1}, befehl_schalten, **s2)
    pruefe("schalten: erstes Schreiben verloren -> eine Wiederholung, gruen",
           code == 0 and aus.count("einmal wiederholen") == 1 and sim.schreibvorgaenge == hin_und_zurueck, aus)

    code, aus, sim = lauf({"schreib_verluste": 99}, befehl_schalten, **s2)
    pruefe("schalten: Schreiben kommt nie an -> Exit 4 nach Wiederholung, Aktor unveraendert",
           code == 4 and "Hinweg" in aus and sim.schreibvorgaenge == [] and "ACHTUNG" not in aus, aus)

    code, aus, sim = lauf({"stumm_nach_schreiben": 1}, befehl_schalten, **s2)
    pruefe("schalten: Verbindung reisst nach dem Umschalten ab -> Exit 1 und Rueckstellzeile mit --status",
           code == 1 and "ACHTUNG" in aus and "schreiben 127.0.0.1 6/4/20 0 --status 6/4/21" in aus, aus)

    code, aus, sim = lauf({"ack_verlieren": True}, befehl_schalten, **s2)
    pruefe("schalten: verlorene Anfrage wird wiederholt",
           code == 0 and "Versuch 1" in aus and sim.schreibvorgaenge == hin_und_zurueck, aus)

    code, aus, sim = lauf({"status_spontan": False}, befehl_schalten, **s2)
    pruefe("schalten: ohne spontane Meldung wird aktiv gelesen",
           code == 0 and "aktiv gelesen" in aus, aus)

    code, aus, sim = lauf({"fremd_schreiben": True}, befehl_schalten, **s2)
    pruefe("schalten: fremder Schreibzugriff -> Exit 3, NICHT zurueckgestellt",
           code == 3 and sim.schreibvorgaenge == [(schalt, 1)] and "1.1.245" in aus
           and "ACHTUNG" not in aus, aus)

    code, aus, sim = lauf({"con_negativ": True}, befehl_schalten, **s2)
    pruefe("schalten: Fehler schon beim Lesen -> Exit 1, keine Rueckstellzeile (nichts geschrieben)",
           code == 1 and "ACHTUNG" not in aus and sim.schreibvorgaenge == [], aus)

    # Lesen und Schreiben, Fehlerfaelle
    code, aus, sim = lauf({"lesen_antwortet": False}, befehl_lesen, gas=[status])
    pruefe("lesen: keine Antwort -> Exit 1 mit Hinweis auf das L-Flag", code == 1 and "L-Flag" in aus, aus)

    code, aus, sim = lauf({"con_negativ": True}, befehl_schreiben, ga=schalt, wert=1, dpt=1)
    pruefe("schreiben ohne --status: negative con -> Exit 1, Zustellung unbekannt",
           code == 1 and "Zustellung unbekannt" in aus, aus)

    code, aus, sim = lauf({}, befehl_schreiben, ga=schalt, wert=1, dpt=1, quelle=q250)
    pruefe("schreiben ohne --status: fehlende con (Quelle 1.1.250) -> Exit 1, Zustellung unbekannt",
           code == 1 and "Zustellung unbekannt" in aus and sim.schreibvorgaenge == [(schalt, 1)], aus)

    code, aus, sim = lauf({"negativ_zugestellt": True}, befehl_schreiben, ga=schalt, wert=1, dpt=1, status=status)
    pruefe("schreiben --status: negative con, Ruecklesung passt -> gruen",
           code == 0 and "NEGATIV" in aus and sim.werte[schalt] == (1, b""), aus)

    code, aus, sim = lauf({"schreib_verluste": 1}, befehl_schreiben, ga=schalt, wert=1, dpt=1, status=status)
    pruefe("schreiben --status: nicht zugestellt -> Exit 4", code == 4 and sim.schreibvorgaenge == [], aus)

    code, aus, sim = lauf({"stumm": True}, befehl_verbinden)
    pruefe("verbinden: stumme Gegenstelle -> Exit 1", code == 1 and "keine CONNECT_RESPONSE" in aus, aus)

    # Mischer (Weg A) - Simulator faehrt den vollen Hub in 1,2 s statt 120 s
    mi = {"position": 128, "toleranz": 2, "takt": 0.3, "frist": 4.0, "mithoeren": False,
          "zw_auf": Simulator.ZW_AUF, "zw_zu": Simulator.ZW_ZU, "pos": Simulator.POS,
          "pos_status": Simulator.POS_STATUS, "pumpe": schalt, "pumpe_status": status,
          "bewegung": None, "ungueltig": ga_aus_text("6/4/18"), "ohne_pumpe": False, "ohne_zwang": False}
    code, aus, sim = lauf({"openknx_antwortet": True}, befehl_mischer, **mi)
    pruefe("mischer: Zwangsstellung ZU zurueck, 128 gemeldet, Pumpe ein; openknx zaehlt nicht",
           code == 0 and sim.m["zu"] == 0 and sim.m["status"] == 128 and sim.werte[schalt] == (1, b"")
           and "zaehlt nicht" in aus and "erwartet rund 60 s" in aus and alles_quittiert(sim), aus)

    code, aus, sim = lauf({}, befehl_mischer, **dict(mi, mithoeren=True))
    pruefe("mischer --mithoeren: spontane Meldung am Ziel vom Mischeraktor gesehen",
           code == 0 and "Mithoerer: Bus: 1.1.39 -> 6/4/12 write 128" in aus, aus)

    code, aus, sim = lauf({"zwang_klemmt": True}, befehl_mischer, **dict(mi, frist=1.5))
    pruefe("mischer: Zwangsstellung bleibt aktiv -> 128 nie gemeldet, Exit 4",
           code == 4 and "Mischer meldet 128 nicht" in aus and sim.m["status"] == 0, aus)

    code, aus, sim = lauf({"openknx_antwortet": True, "mischer_lesen_antwortet": False},
                          befehl_mischer, **dict(mi, frist=1.5))
    pruefe("mischer: nur openknx antwortet -> kein Beleg vom Aktor, Exit 4",
           code == 4 and "kein Beleg vom Aktor" in aus, aus)

    # Bestaetigung ueber die Bewegungsmeldung (1.5.0)
    mb = dict(mi, bewegung=Simulator.BEWEGUNG)
    geschrieben = lambda sim: {g for g, _ in sim.schreibvorgaenge}  # noqa: E731
    # Mit Quelle 1.1.250 wie an der Anlage: keine con, die Wartezeit darauf
    # trennt die Telegramme - nur so ist die Meldung ihrem Ausloeser zuzuordnen
    code, aus, sim = lauf({"openknx_antwortet": True}, befehl_mischer, **dict(mb, quelle=q250))
    pruefe("mischer --bewegung --quelle 1.1.250: Regel A schnell - Bewegung vorher 0, faehrt erst auf "
           "den Positionsbefehl, Eingang passt, openknx-0 auf 6/4/14 zaehlt nicht",
           code == 0 and "Bestaetigung ueber 6/4/14: GRUEN - Mischer faehrt, Eingang passt" in aus
           and "nach dem Senden von 6/4/13 = 128" in aus and "Zuordnung unsicher" not in aus
           and "6/4/14 = 0 von 1.1.245" in aus and "wiederholen" not in aus
           and sim.m["status"] == 128 and alles_quittiert(sim), aus)

    code, aus, sim = lauf({}, befehl_mischer, **mb)
    pruefe("mischer --bewegung ohne Quelle: Telegramme zu dicht -> Zuordnung als unsicher gemeldet",
           code == 0 and "Zuordnung unsicher" in aus, aus)

    code, aus, sim = lauf({"mischer_start": {"zu": 0, "status": 128, "eingang": 128}}, befehl_mischer,
                          **dict(mb, ohne_pumpe=True))
    pruefe("mischer --bewegung --ohne-pumpe: steht schon am Ziel -> GRUEN ueber den Status, Pumpe unberuehrt",
           code == 0 and "GRUEN - Mischer steht schon am Ziel (128)" in aus
           and "Bewegung 1 spontan" not in aus and schalt not in geschrieben(sim), aus)

    code, aus, sim = lauf({}, befehl_mischer, **dict(mb, ohne_zwang=True, frist=1.5))
    pruefe("mischer --bewegung --ohne-zwang: Negativprobe -> keine Bewegung, ROT, Zwangsstellung unberuehrt",
           code == 4 and "Bestaetigung ueber 6/4/14: ROT - keine Bewegung" in aus
           and not {Simulator.ZW_AUF, Simulator.ZW_ZU} & geschrieben(sim) and sim.m["zu"] == 1, aus)

    code, aus, sim = lauf({"pos_verloren": True}, befehl_mischer, **dict(mb, frist=1.5))
    pruefe("mischer --bewegung: Positionsbefehl kommt nie an -> Eingang deckt es auf, "
           "eine Wiederholung, ROT",
           code == 4 and "Eingang meldet 46 statt 128" in aus and aus.count("einmal wiederholen") == 1
           and "FALSCHES GRUEN" not in aus and sim.m["status"] == 0, aus)

    code, aus, sim = lauf({"pos_verloren": True, "freigabe_faehrt": True}, befehl_mischer,
                          **dict(mb, frist=1.5))
    pruefe("mischer --bewegung: Gegenfall - Freigabe faehrt auf den alten Eingang, Positionsbefehl "
           "verloren -> Bewegung 1, aber der Eingang deckt es auf",
           code == 4 and "Eingang meldet 46 statt 128" in aus and sim.m["status"] == 46, aus)

    code, aus, sim = lauf({"schreib_verluste": 3}, befehl_mischer, **dict(mb, quelle=q250, ohne_pumpe=True))
    pruefe("mischer --bewegung: erster Satz Mischertelegramme verloren -> die eine Wiederholung "
           "traegt, GRUEN",
           code == 0 and aus.count("einmal wiederholen") == 1
           and "GRUEN - Mischer faehrt, Eingang passt" in aus and sim.m["status"] == 128, aus)

    # Regel A, Rueckfall: ankunft weit genug hinten, dass die Vorab-Lesung
    # den Endlagenlauf noch sieht
    fahrend = {"zu": 1, "status": 255, "ziel": 0, "ankunft": time.monotonic() + 3.0}
    code, aus, sim = lauf({"mischer_start": fahrend}, befehl_mischer, **dict(mb, ohne_pumpe=True))
    pruefe("mischer --bewegung: Mischer faehrt beim Start schon (Endlagenlauf) -> schnelle Regel "
           "gilt nicht, Urteil ueber die Endstellung",
           code == 0 and "die schnelle Regel gilt nicht" in aus and "Bestaetigung ueber" not in aus
           and "Frist 4 s" in aus and sim.m["status"] == 128, aus)

    print(f"\n{'GRUEN' if fehler == 0 else 'ROT'}: {fehler} Fehler", flush=True)
    return 0 if fehler == 0 else 1


# --- Aufruf ---------------------------------------------------------------------
def arg_ip(text: str) -> str:
    try:
        return str(ipaddress.IPv4Address(text))
    except ValueError:
        raise argparse.ArgumentTypeError(f"'{text}' ist keine IPv4-Adresse") from None


def arg_port(text: str) -> int:
    try:
        port = int(text, 10)
    except ValueError:
        raise argparse.ArgumentTypeError(f"Port '{text}' ist keine Zahl") from None
    if not 1 <= port <= 65535:
        raise argparse.ArgumentTypeError(f"Port {port} ausserhalb 1-65535")
    return port


def arg_ga(text: str) -> int:
    try:
        return ga_aus_text(text)
    except ValueError as fehler:
        raise argparse.ArgumentTypeError(str(fehler)) from None


def arg_ia(text: str) -> int:
    try:
        return ia_aus_text(text)
    except ValueError as fehler:
        raise argparse.ArgumentTypeError(str(fehler)) from None


def arg_halten(text: str) -> float:
    try:
        sek = float(text)
    except ValueError:
        raise argparse.ArgumentTypeError(f"'{text}' ist keine Zahl") from None
    # Obergrenze 30 s: Die Verbindung laeuft ohne Heartbeat und muss deutlich
    # unter den 60 s bleiben, nach denen der erste faellig waere.
    if not 0 <= sek <= 30:
        raise argparse.ArgumentTypeError("--halten muss zwischen 0 und 30 s liegen")
    return sek


def main() -> int:
    p = argparse.ArgumentParser(description="Minimaler KNXnet/IP-Tunnel-Client (Vorabtest KNX-Notbetrieb)")
    sub = p.add_subparsers(dest="befehl", required=True)

    def mit_ziel(name: str, hilfe: str, telegramme: bool = True, gegenprobe: bool = True):
        s = sub.add_parser(name, help=hilfe)
        s.add_argument("ip", type=arg_ip, help="KNX-IP-Schnittstelle")
        s.add_argument("--port", type=arg_port, default=3671)
        s.add_argument("--roh", action="store_true", help="jedes Datagramm als Hex ausgeben")
        if telegramme:
            s.add_argument("--quelle", type=arg_ia, default=0, metavar="B.L.G",
                           help="Quelladresse im Telegramm (Standard: Tunneladresse der Schnittstelle)")
        else:
            s.set_defaults(quelle=0)
        if telegramme and gegenprobe:
            s.add_argument("--gegenprobe", action="store_true",
                           help="zweiter Tunnel hoert mit und meldet die Quelle auf dem Bus")
        else:
            s.set_defaults(gegenprobe=False)
        return s

    def arg_bereich(unten: float, oben: float, typ=float):
        def pruefe(text: str):
            try:
                wert = typ(text)
            except ValueError:
                raise argparse.ArgumentTypeError(f"'{text}' ist keine Zahl") from None
            if not unten <= wert <= oben:
                raise argparse.ArgumentTypeError(f"{wert} ausserhalb {unten:g}-{oben:g}")
            return wert
        return pruefe

    mit_ziel("verbinden", "Stufe 0: Tunnel auf- und abbauen, kein Bustelegramm",
             telegramme=False).set_defaults(fn=befehl_verbinden)
    s = mit_ziel("lesen", "Stufe 1: GroupValueRead auf eine oder mehrere GAs")
    s.add_argument("gas", type=arg_ga, nargs="+", metavar="GA")
    s.add_argument("--alle", action="store_true",
                   help="ganzes Lesefenster abwarten, jede Antwort mit Quelle zeigen")
    s.set_defaults(fn=befehl_lesen)
    s = mit_ziel("schreiben", "Einzelwert schreiben (z. B. manuelles Zurueckstellen)")
    s.add_argument("ga", type=arg_ga)
    s.add_argument("wert", type=int)
    s.add_argument("--dpt", type=int, choices=(1, 5), default=1, help="1 = 1 Bit, 5 = 1 Byte (0-255)")
    s.add_argument("--status", type=arg_ga, default=None, metavar="GA",
                   help="Status-GA zum Zuruecklesen (nur DPT 1)")
    s.set_defaults(fn=befehl_schreiben)
    s = mit_ziel("schalten", "Stufe 2: 1-Bit-Aktor umschalten, zuruecklesen, zurueckstellen")
    s.add_argument("schalt", type=arg_ga, metavar="SCHALT_GA")
    s.add_argument("status", type=arg_ga, metavar="STATUS_GA")
    s.add_argument("--halten", type=arg_halten, default=5.0, help="Sekunden im Testzustand (0-30)")
    s.set_defaults(fn=befehl_schalten)
    s = mit_ziel("mischer", "Referenz des Firmwareschritts Vorderhaus: Zwangsstellungen zurueck, "
                 "Position, Pumpe ein, Mischerstatus abfragen", gegenprobe=False)
    # Voreinstellungen: die Gruppenadressen dieser Anlage (Owner, 2026-09-12)
    s.add_argument("--position", type=arg_bereich(0, 255, int), default=128, help="Rohwert 0-255, 128 = 50 %%")
    s.add_argument("--toleranz", type=arg_bereich(0, 10, int), default=2)
    s.add_argument("--takt", type=arg_bereich(2, 30), default=10.0, help="Sekunden zwischen den Abfragen")
    # Obergrenze 110 s: Der Mithoerer haelt seinen Tunnel mit Heartbeat, die
    # Abfragen sind kurz - aber ein halber Hub dauert 60 s, mehr braucht es nicht.
    s.add_argument("--frist", type=arg_bereich(10, 110), default=90.0, help="Sekunden bis ROT")
    s.add_argument("--mithoeren", action="store_true", help="passiver Tunnel fuer den ganzen Lauf")
    s.add_argument("--bewegung", nargs="?", type=arg_ga, const=ga_aus_text("6/4/14"), default=None,
                   metavar="GA", help="Rueckleseregel A ueber die Bewegungsmeldung (ohne GA: 6/4/14)")
    s.add_argument("--ohne-pumpe", dest="ohne_pumpe", action="store_true", help="Pumpe nicht schalten")
    s.add_argument("--ohne-zwang", dest="ohne_zwang", action="store_true",
                   help="Zwangsstellungen NICHT zuruecknehmen - Negativprobe, der Mischer darf nicht fahren")
    s.add_argument("--ungueltig", type=arg_ga, default=ga_aus_text("6/4/18"),
                   help="Position ungueltig - nur fuer den Mithoerer")
    s.add_argument("--zw-auf", dest="zw_auf", type=arg_ga, default=ga_aus_text("6/4/17"))
    s.add_argument("--zw-zu", dest="zw_zu", type=arg_ga, default=ga_aus_text("6/4/16"))
    s.add_argument("--pos", type=arg_ga, default=ga_aus_text("6/4/13"))
    s.add_argument("--pos-status", dest="pos_status", type=arg_ga, default=ga_aus_text("6/4/12"))
    s.add_argument("--pumpe", type=arg_ga, default=ga_aus_text("6/4/20"))
    s.add_argument("--pumpe-status", dest="pumpe_status", type=arg_ga, default=ga_aus_text("6/4/21"))
    s.set_defaults(fn=befehl_mischer)
    sub.add_parser("selbsttest", help="ohne Netz: Rahmen gegen xknx, Ablaeufe gegen Simulator")

    a = p.parse_args()
    if a.befehl == "selbsttest":
        return selbsttest()
    # Wertebereich je Datentyp - erst hier pruefbar, weil er von --dpt abhaengt
    if a.befehl == "schreiben":
        if not 0 <= a.wert <= (1 if a.dpt == 1 else 255):
            p.error(f"Wert {a.wert} ausserhalb des Bereichs fuer DPT {a.dpt}")
        if a.status is not None and a.dpt != 1:
            p.error("--status geht nur mit DPT 1")
    a.zeiten = Zeiten()
    return fuehre_aus(a.fn, a)


if __name__ == "__main__":
    sys.exit(main())
