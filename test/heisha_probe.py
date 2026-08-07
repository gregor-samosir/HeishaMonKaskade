#!/usr/bin/env python3
"""Gemeinsame Helfer fuer die HeishaMon-Diagnosewerkzeuge in diesem Ordner.

Telnet-Konsole ansprechen, Hexlog mitschneiden und daraus das gesendete
Kommandotelegramm herausloesen.
"""
import re
import select
import socket
import time

QUERYSIZE = 110      # Laenge eines Kommandotelegramms ohne Pruefsumme
COMMAND_HEADER = "F1"  # Kommando; Abfragen beginnen mit 71


def telnet_connect(host, port=23, timeout=10):
    sock = socket.create_connection((host, port), timeout=timeout)
    sock.settimeout(2)
    return sock


def drain(sock, seconds, sink, stop_marker=None, tail=1.5):
    """Liest bis zu 'seconds' lang mit und haengt alles an 'sink' an.

    Mit 'stop_marker' wird abgebrochen, sobald der Text auftaucht - danach
    wird noch 'tail' Sekunden nachgelesen, damit ein Hexlog vollstaendig wird.
    Rueckgabe: True, wenn der Marker gefunden wurde.
    """
    end = time.time() + seconds
    while time.time() < end:
        rest = max(0.0, end - time.time())
        r, _, _ = select.select([sock], [], [], min(1.0, rest))
        if not r:
            continue
        try:
            data = sock.recv(4096)
        except (socket.timeout, BlockingIOError):
            continue
        if not data:
            return False
        sink.append(data.decode("utf-8", "replace"))
        if stop_marker and stop_marker in "".join(sink):
            nachlauf = time.time() + tail
            while time.time() < nachlauf:
                r, _, _ = select.select([sock], [], [], 0.3)
                if not r:
                    continue
                try:
                    more = sock.recv(8192)
                except (socket.timeout, BlockingIOError, OSError):
                    break
                if not more:
                    break
                sink.append(more.decode("utf-8", "replace"))
            return True
    return False


def parse_command_telegram(text):
    """Zieht das zuletzt GESENDETE Kommandotelegramm aus einem Hexlog-Mitschnitt.

    Wichtig: Der Hexlog gibt auch die EMPFANGENEN 203 Bytes der Waermepumpe
    aus (HeishaMon.cpp, readSerial). Ein naiver Parser klebt Kommando und
    Antwort zu 313 Bytes zusammen. Deshalb wird ab dem F1-Header hart nach
    QUERYSIZE Bytes abgeschnitten.

    Rueckgabe: Liste von 110 Hex-Strings oder None.
    """
    blocks = re.findall(r"data:\s+((?:[0-9A-F]{2}\s+)+)", text)
    telegramme = []
    aktuell = None
    for block in blocks:
        werte = block.split()
        if werte and werte[0].upper() == COMMAND_HEADER:
            if aktuell:
                telegramme.append(aktuell)
            aktuell = list(werte)
        elif aktuell is not None and len(aktuell) < QUERYSIZE:
            aktuell += werte
        if aktuell and len(aktuell) >= QUERYSIZE:
            telegramme.append(aktuell[:QUERYSIZE])
            aktuell = None
    if aktuell and len(aktuell) >= QUERYSIZE:
        telegramme.append(aktuell[:QUERYSIZE])
    return telegramme[-1] if telegramme else None


def parse_all_command_telegrams(text):
    """Wie parse_command_telegram, liefert aber alle gefundenen Telegramme."""
    blocks = re.findall(r"data:\s+((?:[0-9A-F]{2}\s+)+)", text)
    telegramme, aktuell = [], None
    for block in blocks:
        werte = block.split()
        if werte and werte[0].upper() == COMMAND_HEADER:
            aktuell = list(werte)
        elif aktuell is not None and len(aktuell) < QUERYSIZE:
            aktuell += werte
        if aktuell and len(aktuell) >= QUERYSIZE:
            telegramme.append(aktuell[:QUERYSIZE])
            aktuell = None
    return telegramme


def zeige_byte4(wert):
    """Byte 4 in seine drei Bitfelder zerlegen."""
    return (f"0x{wert:02X}  ->  Heatpump {wert & 0x03} | "
            f"WaterPump {(wert & 0x30) >> 4} | ForceDHW {(wert & 0xC0) >> 6}")
