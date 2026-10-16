#pragma once
#include <Arduino.h>
#include <WiFi.h>

#include "telnetlog.h" // Regeln: wann gilt ein Client als tot (arduino-frei, Hosttest)

/*****************************************************************************/
/* Telnet-Ausgabe, die nie wartet (S2, 3.24.0)                               */
/*                                                                           */
/* Ersetzt die Bibliothek TelnetStream (jandrassy). Dieselben Aufrufe        */
/* (printf, println, available, read, peek, flush, stop, begin) unter        */
/* demselben Objektnamen, damit HeishaMon.cpp unveraendert bleibt. Warum die */
/* Bibliothek nicht blieb, steht in telnetlog.h.                             */
/*                                                                           */
/* Eine Sitzung (Owner-Entscheid 2026-10-16): Ein neuer Client verdraengt    */
/* den alten. So blockiert eine tote oder nicht lesende Sitzung nie den      */
/* Platz, und ein neuer Mitschnitt kommt immer durch.                        */
/*                                                                           */
/* Der gesamte Zugriff laeuft aus loop() - es gibt keinen zweiten Task und   */
/* damit keine Nebenlaeufigkeit auf Client und Zaehler.                      */
/*****************************************************************************/
class TelnetAusgabe : public Stream
{
public:
  explicit TelnetAusgabe(uint16_t port);

  // zeitFn liefert den Zeitstempel fuer die <INF>-Zeile "N Zeilen verworfen";
  // nullptr = Zeile ohne Zeitstempel.
  void begin(void (*zeitFn)(char *, size_t) = nullptr);
  void stop(); // Client trennen (Taste C)

  // Stream
  int read() override;
  int available() override;
  int peek() override;

  // Print - schreibt nie blockierend, siehe telnetlog.h
  size_t write(uint8_t b) override;
  size_t write(const uint8_t *buf, size_t size) override;
  using Print::write;
  void flush() override; // kein Sendepuffer auf unserer Seite - bleibt leer

private:
  WiFiServer server_;
  WiFiClient client_;
  TelnetClientZustand zustand_;
  void (*zeitFn_)(char *, size_t);
  uint32_t letztePruefung_; // millis() der letzten Annahmepruefung

  void annehmen_();             // neuen Client annehmen, alten verdraengen (hoechstens alle 100 ms)
  void trennen_();              // Client schliessen, Zustand zuruecksetzen
  bool senden_(const uint8_t *buf, size_t size); // true = vollstaendig im Sendepuffer
  void verworfeneMelden_(uint32_t anzahl);
};

extern TelnetAusgabe TelnetStream;
