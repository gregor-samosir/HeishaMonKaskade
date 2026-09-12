#!/usr/bin/env python3
"""Minimaler KNXnet/IP-Tunnel-Client - Vorabtest fuer den KNX-Schritt im Notbetrieb.

Version 1.1.0 (2026-09-12)

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

Optionen: --port (Standard 3671), --roh (jedes Datagramm als Hex),
--quelle B.L.G (Quelladresse im Telegramm statt der Tunneladresse - ob die
Schnittstelle sie uebernimmt, meldet das Werkzeug), schreiben: --dpt 1|5 und
--status GA (Ruecklesung, nur DPT 1), schalten: --halten SEK (0-30, Standard 5).

Rueckleseregel (aus Stufe 2, 2026-09-12): Eine negative L_Data.con heisst
nicht, dass das Telegramm verloren ging - an der Anlage lag es trotzdem auf
dem Bus, und der Aktor hat geschaltet. Das Urteil faellt deshalb die
Ruecklesung am Aktor; passt sie nicht, wird genau einmal wiederholt. Dieselbe
Regel soll der Firmwareschritt bekommen.

Was auf den Bus geht: 'verbinden' nichts; 'lesen' nur Lesetelegramme;
'schreiben' und 'schalten' veraendern einen Aktor.

Exit-Codes: 0 gruen, 1 Kommunikations- oder Protokollfehler (auch: negative
Busbestaetigung bei 'schreiben' ohne --status), 2 Aufruffehler, 3 fremder
Schreibzugriff waehrend 'schalten' (bewusst NICHT zurueckgestellt),
4 Ruecklesung passt nicht (bei 'schalten' auch nach der Wiederholung).

Nur Standardbibliothek. Kein KNX IP Secure (an dieser Anlage nicht aktiv).

Changelog:
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
    con: float = 3.0       # L_Data.con - Bestaetigung der Schnittstelle vom Bus
    lesen: float = 3.0     # GroupValueResponse des Aktors
    status: float = 3.0    # spontane Statusmeldung nach dem Schalten, sonst aktiv lesen


class KnxFehler(Exception):
    """Kommunikations- oder Protokollfehler - fuehrt zu Exit-Code 1."""


# --- Ausgabe ----------------------------------------------------------------
# flush=True: die Ausgabe landet sonst bei Umleitung in eine Datei erst am
# Prozessende dort (Python puffert) - ein laufender Test saehe leer aus.
_START = time.monotonic()


def melde(text: str) -> None:
    print(f"[{(time.monotonic() - _START) * 1000:7.0f} ms] {text}", flush=True)


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
    eine vorgegebene Quelle (--quelle) uebernimmt sie oder ersetzt sie.
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
    """

    def __init__(self, ip: str, port: int, zeiten: Zeiten | None = None,
                 roh: bool = False, beobachtet: tuple[int, ...] = (), quelle: int = 0):
        self.ip = ip
        self.port = port
        self.zeiten = zeiten or Zeiten()
        self.roh = roh
        self.beobachtet = set(beobachtet)
        self.quelle = quelle                   # 0 = die Schnittstelle setzt ihre Tunneladresse
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
        self.sonstige = 0                      # mitgelesene fremde Bustelegramme
        self.gegenstelle_getrennt = False
        self._quelle_gemeldet = False

    def __enter__(self) -> "Tunnel":
        self.verbinden()
        return self

    def __exit__(self, *_) -> bool:
        self.trennen()
        return False

    # --- Senden und Empfangen ---------------------------------------------
    def _sende(self, dg: bytes, ziel: tuple[str, int]) -> None:
        if self.roh:
            melde(">> " + dg.hex(" "))
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
            melde(f"Datagramm von fremder Adresse {absender[0]} verworfen")
            return
        if self.roh:
            melde("<< " + dg.hex(" "))
        try:
            dienst, rumpf = zerlege_knxip(dg)
        except KnxFehler as fehler:
            melde(f"unlesbares Datagramm verworfen: {fehler}")
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
            melde(f"Dienst 0x{dienst:04X} ignoriert")

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
            melde(f"Sequenz {seq} statt {self.rx_seq} verworfen")
            return
        try:
            r = zerlege_cemi(rumpf[4:])
        except KnxFehler as fehler:
            melde(f"cEMI unlesbar: {fehler}")
            return
        if r.code == L_DATA_CON:
            self.cons.append(r)
        elif r.code == L_DATA_IND:
            self.inds.append(r)
            if r.gruppe and r.ziel in self.beobachtet:
                melde(f"Bus: {ia_text(r.quelle)} -> {ga_text(r.ziel)} "
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
            melde(f"verbunden: Kanal {self.kanal}, Tunneladresse {ia_text(self.adresse)}, "
                  f"Datenendpunkt {self.daten_ep[0]}:{self.daten_ep[1]}, lokal {self.lokal[0]}:{self.lokal[1]}")
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
        melde("Verbindungszustand ok")

    def trennen(self) -> None:
        if self.sock is None:
            return
        try:
            if self.kanal is not None and not self.gegenstelle_getrennt:
                self._sende(baue_disconnect_request(self.kanal, self.lokal), (self.ip, self.port))
                rumpf = self._warte(lambda: self.antworten.pop(DISCONNECT_RESPONSE, None),
                                    self.zeiten.trennen, trennung_pruefen=False)
                if rumpf is None:
                    melde("keine DISCONNECT_RESPONSE - die Schnittstelle raeumt den Tunnel "
                          "nach 120 s selbst ab")
                else:
                    melde("getrennt, Tunnel freigegeben")
            if self.sonstige:
                melde(f"{self.sonstige} weitere Bustelegramme mitgelesen und quittiert")
        except (OSError, KnxFehler) as fehler:
            melde(f"Trennen gestoert: {fehler}")
        finally:
            self.sock.close()
            self.sock = None

    # --- Gruppendienste -----------------------------------------------------
    def _sende_cemi(self, cemi: bytes, ga: int) -> Cemi:
        """TUNNELING_REQUEST mit ACK (eine Wiederholung), dann L_Data.con.

        Die Reihenfolge von ACK und con ist nicht festgelegt - beide werden
        unabhaengig voneinander eingesammelt. Fehlen ACK oder con ganz, ist
        die Verbindung gestoert (KnxFehler). Eine NEGATIVE con dagegen ist kein
        Abbruchgrund: An der Anlage lag ein so bestaetigtes Telegramm trotzdem
        auf dem Bus (Stufe 2). Was sie bedeutet, entscheidet der Aufrufer.
        """
        marke = len(self.cons)
        self.acks.clear()
        ack = None
        for versuch in (1, 2):
            self._sende(baue_tunneling_request(self.kanal, self.tx_seq, cemi), self.daten_ep)
            ack = self._warte(lambda: next((a for a in self.acks if a[0] == self.tx_seq), None),
                              self.zeiten.ack)
            if ack is not None:
                break
            melde(f"kein TUNNELING_ACK auf Sequenz {self.tx_seq} (Versuch {versuch})")
        if ack is None:
            raise KnxFehler("kein TUNNELING_ACK nach der Wiederholung")
        if ack[1] != 0:
            raise KnxFehler(f"TUNNELING_ACK meldet {status_text(ack[1])}")
        self.tx_seq = (self.tx_seq + 1) & 0xFF
        con = self._warte(lambda: next((r for r in self.cons[marke:] if r.ziel == ga), None),
                          self.zeiten.con)
        if con is None:
            raise KnxFehler(f"keine L_Data.con fuer {ga_text(ga)} binnen {self.zeiten.con:g} s")
        self._quelle_melden(con)
        if not con.bestaetigt_ok:
            melde(f"Busbestaetigung fuer {ga_text(ga)} NEGATIV (L_Data.con) - "
                  "Zustellung unbekannt, die Ruecklesung entscheidet")
        return con

    def _quelle_melden(self, con: Cemi) -> None:
        # Einmal je Verbindung: Mit welcher Quelladresse ging das Telegramm auf
        # den Bus? Die con ist die Kopie des gesendeten Rahmens und traegt sie.
        if self._quelle_gemeldet:
            return
        self._quelle_gemeldet = True
        if self.quelle == 0:
            melde(f"Quelladresse auf dem Bus: {ia_text(con.quelle)} (von der Schnittstelle eingesetzt)")
        elif con.quelle == self.quelle:
            melde(f"Quelladresse auf dem Bus: {ia_text(con.quelle)} - "
                  "die Schnittstelle uebernimmt die vorgegebene Adresse")
        else:
            melde(f"Quelladresse auf dem Bus: {ia_text(con.quelle)} - "
                  f"die vorgegebene {ia_text(self.quelle)} wurde ERSETZT")

    def lesen(self, ga: int) -> Cemi:
        # Marke VOR dem Senden: Die Antwort des Aktors darf auch vor der
        # L_Data.con eintreffen und wird trotzdem gefunden. Nach einer
        # negativen con wird trotzdem auf die Antwort gewartet.
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

    def schreiben(self, ga: int, klein: int = 0, daten: bytes = b"") -> Cemi:
        con = self._sende_cemi(baue_cemi(L_DATA_REQ, self.quelle, ga, APCI_WRITE, klein, daten), ga)
        melde(f"geschrieben: {ga_text(ga)} = {daten[0] if daten else klein}, L_Data.con "
              + ("positiv" if con.bestaetigt_ok else "negativ"))
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
    # Stufe 1: nur Lesetelegramme, kein Zustandswechsel
    with _tunnel(a, tuple(a.gas)) as t:
        for ga in a.gas:
            r = t.lesen(ga)
            melde(f"{ga_text(ga)} = {wert_text(r)}  (Antwort von {ia_text(r.quelle)})")
    melde("GRUEN: alle Objekte haben geantwortet")
    return 0


def befehl_schreiben(a) -> int:
    # Einzelwert, vor allem fuer das manuelle Zurueckstellen. Mit --status
    # liest es zurueck - ohne kann es eine negative Busbestaetigung nicht
    # aufloesen und meldet dann "unbekannt" statt "gruen".
    daten = bytes((a.wert,)) if a.dpt == 5 else b""
    beobachtet = (a.ga,) if a.status is None else (a.ga, a.status)
    with _tunnel(a, beobachtet) as t:
        marke = len(t.inds)
        con = t.schreiben(a.ga, klein=a.wert if a.dpt == 1 else 0, daten=daten)
        if a.status is None:
            if con.bestaetigt_ok:
                melde("GRUEN: Schreibtelegramm vom Bus bestaetigt")
                return 0
            melde("ROT: Busbestaetigung negativ und keine Status-GA angegeben - "
                  "Zustellung unbekannt. Zustand pruefen, z. B. mit --status")
            return 1
        wert, quelle = t.warte_auf_status(a.status, a.wert, marke)
    if wert == a.wert:
        melde(f"GRUEN: Rueckmeldung {ga_text(a.status)} = {wert} ({quelle})")
        return 0
    melde(f"ROT: Rueckmeldung {ga_text(a.status)} = {wert} ({quelle}), erwartet {a.wert}")
    return 4


def _schalte(t: Tunnel, a, ziel: int) -> str:
    """Ein Schaltvorgang nach der Rueckleseregel, die auch der Firmwareschritt
    bekommen soll: schreiben, zuruecklesen, bei Abweichung genau einmal
    wiederholen. Die L_Data.con entscheidet nichts (Stufe 2, 2026-09-12).
    Ergebnis: 'ok', 'abweichung' oder 'fremd'."""
    for versuch in (1, 2):
        marke = len(t.inds)
        t.schreiben(a.schalt, klein=ziel)
        wert, quelle = t.warte_auf_status(a.status, ziel, marke)
        if wert == ziel:
            melde(f"Rueckmeldung {ga_text(a.status)} = {wert} ({quelle}) - passt")
            return "ok"
        melde(f"Rueckmeldung {ga_text(a.status)} = {wert} ({quelle}) - PASST NICHT, erwartet {ziel}")
        # Vor einer Wiederholung: Hat inzwischen jemand anderes geschaltet,
        # gilt dessen Befehl - eine Wiederholung wuerde ihn ueberschreiben.
        if t.fremde_schreiber(a.schalt):
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
    try:
        with _tunnel(a, (a.schalt, a.status)) as t:
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
            hin = _schalte(t, a, ziel)
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
            zurueck = _schalte(t, a, ausgang)
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
    Schnittstelle; das belegen erst die Stufen 0-2 an der Anlage. Adressen
    und ctrl1 der con sind der Anlage nachgebildet (Stufen 1 und 2).
    """
    KANAL = 7
    ADRESSE = 0x1194  # 1.1.148 - Tunneladresse, wie sie die echte Schnittstelle vergibt
    AKTOR = 0x113C    # 1.1.60 - Pumpenaktor
    FREMD = 0x11F5    # 1.1.245 - openknx

    def __init__(self, schalt: int, status: int, mischer: int, stumm=False,
                 stumm_nach_schreiben=0, ack_verlieren=False, fremd_schreiben=False,
                 con_negativ=False, schreib_verluste=0, negativ_zugestellt=False,
                 quelle_ersetzen=False, lesen_antwortet=True, status_spontan=True):
        super().__init__(daemon=True)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(("127.0.0.1", 0))
        self.sock.settimeout(0.05)
        self.port = self.sock.getsockname()[1]
        self.schalt, self.status = schalt, status
        self.werte = {schalt: (0, b""), status: (0, b""), mischer: (0, bytes((128,)))}
        # Fehlerbilder
        self.stumm = stumm                            # antwortet auf gar nichts
        self.stumm_nach_schreiben = stumm_nach_schreiben  # verstummt nach n Schreibvorgaengen
        self.ack_verlieren = ack_verlieren            # erste Anfrage kommt nicht an
        self.fremd_schreiben = fremd_schreiben        # openknx schaltet nach dem ersten Schreiben mit
        self.con_negativ = con_negativ                # jede con negativ, nichts zugestellt
        self.schreib_verluste = schreib_verluste      # erste n Schreibtelegramme: con negativ, nicht zugestellt
        self.negativ_zugestellt = negativ_zugestellt  # Schreiben: con negativ, trotzdem zugestellt (Stufe 2)
        self.quelle_ersetzen = quelle_ersetzen        # vorgegebene Quelle durch Tunneladresse ersetzen
        self.lesen_antwortet = lesen_antwortet
        self.status_spontan = status_spontan
        # Protokoll fuer die Pruefungen
        self.client = None
        self.rx = 0
        self.tx = 0
        self.gesendet: list[int] = []
        self.quittiert: set[int] = set()
        self.schreibvorgaenge: list[tuple[int, int]] = []
        self.quellen: list[int] = []
        self.getrennt = False
        self._verloren = False
        self._fremd_gesendet = False
        self._stopp = threading.Event()

    def stoppen(self) -> None:
        self._stopp.set()
        self.join(2)
        self.sock.close()

    def _an_client(self, cemi: bytes) -> None:
        self.gesendet.append(self.tx)
        self.sock.sendto(baue_tunneling_request(self.KANAL, self.tx, cemi), self.client)
        self.tx = (self.tx + 1) & 0xFF

    def _verstummt(self) -> bool:
        return self.stumm or (self.stumm_nach_schreiben > 0
                              and len(self.schreibvorgaenge) >= self.stumm_nach_schreiben)

    def run(self) -> None:
        while not self._stopp.is_set():
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
                self.client = zerlege_hpai(rumpf[8:16])
                antwort = (bytes((self.KANAL, 0)) + hpai(("127.0.0.1", self.port))
                           + bytes((0x04, 0x04, self.ADRESSE >> 8, self.ADRESSE & 0xFF)))
                self.sock.sendto(kopf(CONNECT_RESPONSE, len(antwort)) + antwort, absender)
            elif dienst == CONNECTIONSTATE_REQUEST:
                self.sock.sendto(kopf(CONNECTIONSTATE_RESPONSE, 2) + bytes((self.KANAL, 0)), absender)
            elif dienst == DISCONNECT_REQUEST:
                self.sock.sendto(baue_disconnect_response(self.KANAL), absender)
                self.getrennt = True
            elif dienst == TUNNELING_ACK:
                self.quittiert.add(rumpf[2])
            elif dienst == TUNNELING_REQUEST:
                self._tunnel(rumpf)

    def _tunnel(self, rumpf: bytes) -> None:
        seq = rumpf[2]
        if self.ack_verlieren and not self._verloren:
            self._verloren = True  # erste Anfrage geht "verloren" - der Client muss wiederholen
            return
        if seq == self.rx:
            self.rx = (seq + 1) & 0xFF
            neu = True
        elif seq == (self.rx - 1) & 0xFF:
            neu = False  # Wiederholung: quittieren, nicht noch einmal ausfuehren
        else:
            return
        self.sock.sendto(baue_tunneling_ack(self.KANAL, seq), self.client)
        if not neu:
            return
        r = zerlege_cemi(rumpf[4:])
        self.quellen.append(r.quelle)
        verloren = r.apci == APCI_WRITE and self.schreib_verluste > 0
        if verloren:
            self.schreib_verluste -= 1
        nicht_zugestellt = self.con_negativ or verloren
        negativ = nicht_zugestellt or (self.negativ_zugestellt and r.apci == APCI_WRITE)
        # Quelle auf dem Bus: 0.0.0 ersetzt jede Schnittstelle durch die
        # Tunneladresse; eine vorgegebene uebernimmt der Simulator, ausser
        # quelle_ersetzen ist gesetzt. ctrl1 9c/9d wie an der Anlage.
        quelle = r.quelle if r.quelle and not self.quelle_ersetzen else self.ADRESSE
        ctrl1 = 0x9C | (0x01 if negativ else 0x00)
        self._an_client(baue_cemi(L_DATA_CON, quelle, r.ziel, r.apci, r.klein, r.daten, ctrl1))
        if nicht_zugestellt:
            return
        if r.apci == APCI_READ and self.lesen_antwortet and r.ziel in self.werte:
            klein, daten = self.werte[r.ziel]
            self._an_client(baue_cemi(L_DATA_IND, self.AKTOR, r.ziel, APCI_RESPONSE, klein, daten))
        elif r.apci == APCI_WRITE:
            self.schreibvorgaenge.append((r.ziel, r.klein))
            self._schalte(r.ziel, r.klein)
            if self.fremd_schreiben and not self._fremd_gesendet:
                self._fremd_gesendet = True
                self._an_client(baue_cemi(L_DATA_IND, self.FREMD, r.ziel, APCI_WRITE, r.klein))
                self._schalte(r.ziel, r.klein)

    def _schalte(self, ga: int, klein: int) -> None:
        self.werte[ga] = (klein, b"")
        if ga == self.schalt:
            self.werte[self.status] = (klein, b"")
            if self.status_spontan:
                self._an_client(baue_cemi(L_DATA_IND, self.AKTOR, self.status, APCI_WRITE, klein))


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
    pruefe("GroupValueRead mit Quelle 1.1.250",
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
    schnell = Zeiten(antwort=1.0, trennen=0.5, ack=0.2, con=0.5, lesen=0.5, status=0.3)

    def lauf(sim_optionen: dict, befehl, **argumente):
        argumente.setdefault("quelle", 0)
        if befehl is befehl_schreiben:
            argumente.setdefault("status", None)
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

    code, aus, sim = lauf({}, befehl_verbinden)
    pruefe("verbinden: gruen, getrennt", code == 0 and sim.getrennt, aus)

    code, aus, sim = lauf({}, befehl_lesen, gas=[status, mischer])
    pruefe("lesen: 1 Bit und 1 Byte, alles quittiert, Quelle von der Schnittstelle",
           code == 0 and "6/4/21 = 0 " in aus and "6/4/30 = 128 " in aus and alles_quittiert(sim)
           and "1.1.148 (von der Schnittstelle eingesetzt)" in aus and sim.quellen == [0, 0], aus)

    code, aus, sim = lauf({}, befehl_lesen, gas=[status], quelle=0x11FA)
    pruefe("lesen --quelle 1.1.250: Schnittstelle uebernimmt",
           code == 0 and "uebernimmt" in aus and sim.quellen == [0x11FA], aus)

    code, aus, sim = lauf({"quelle_ersetzen": True}, befehl_lesen, gas=[status], quelle=0x11FA)
    pruefe("lesen --quelle 1.1.250: Schnittstelle ersetzt", code == 0 and "ERSETZT" in aus, aus)

    code, aus, sim = lauf({}, befehl_schalten, **s2)
    pruefe("schalten: hin und zurueck, Aktor wieder auf 0",
           code == 0 and sim.schreibvorgaenge == hin_und_zurueck
           and sim.werte[schalt] == (0, b"") and alles_quittiert(sim) and sim.getrennt, aus)

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

    code, aus, sim = lauf({"lesen_antwortet": False}, befehl_lesen, gas=[status])
    pruefe("lesen: keine Antwort -> Exit 1 mit Hinweis auf das L-Flag", code == 1 and "L-Flag" in aus, aus)

    code, aus, sim = lauf({"con_negativ": True}, befehl_schreiben, ga=schalt, wert=1, dpt=1)
    pruefe("schreiben ohne --status: negative con -> Exit 1, Zustellung unbekannt",
           code == 1 and "Zustellung unbekannt" in aus, aus)

    code, aus, sim = lauf({"negativ_zugestellt": True}, befehl_schreiben, ga=schalt, wert=1, dpt=1, status=status)
    pruefe("schreiben --status: negative con, Ruecklesung passt -> gruen",
           code == 0 and "NEGATIV" in aus and sim.werte[schalt] == (1, b""), aus)

    code, aus, sim = lauf({"schreib_verluste": 1}, befehl_schreiben, ga=schalt, wert=1, dpt=1, status=status)
    pruefe("schreiben --status: nicht zugestellt -> Exit 4", code == 4 and sim.schreibvorgaenge == [], aus)

    code, aus, sim = lauf({"stumm": True}, befehl_verbinden)
    pruefe("verbinden: stumme Gegenstelle -> Exit 1", code == 1 and "keine CONNECT_RESPONSE" in aus, aus)

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

    def mit_ziel(name: str, hilfe: str, telegramme: bool = True):
        s = sub.add_parser(name, help=hilfe)
        s.add_argument("ip", type=arg_ip, help="KNX-IP-Schnittstelle")
        s.add_argument("--port", type=arg_port, default=3671)
        s.add_argument("--roh", action="store_true", help="jedes Datagramm als Hex ausgeben")
        if telegramme:
            s.add_argument("--quelle", type=arg_ia, default=0, metavar="B.L.G",
                           help="Quelladresse im Telegramm (Standard: Tunneladresse der Schnittstelle)")
        else:
            s.set_defaults(quelle=0)
        return s

    mit_ziel("verbinden", "Stufe 0: Tunnel auf- und abbauen, kein Bustelegramm",
             telegramme=False).set_defaults(fn=befehl_verbinden)
    s = mit_ziel("lesen", "Stufe 1: GroupValueRead auf eine oder mehrere GAs")
    s.add_argument("gas", type=arg_ga, nargs="+", metavar="GA")
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
