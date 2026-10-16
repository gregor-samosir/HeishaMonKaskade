#include "telnetausgabe.h"

#include <errno.h>
#include <lwip/sockets.h>

/*****************************************************************************/
/* Abstand der Annahmepruefung. WiFiServer::accept() ist nicht blockierend,  */
/* kostet aber einen Systemaufruf; ein neuer Client darf ruhig 100 ms warten. */
/*****************************************************************************/
static const uint32_t ANNAHME_ABSTAND_MS = 100u;

TelnetAusgabe TelnetStream(23);

TelnetAusgabe::TelnetAusgabe(uint16_t port)
    : server_(port), zeitFn_(nullptr), letztePruefung_(0)
{
  telnet_neuer_client(&zustand_);
}

void TelnetAusgabe::begin(void (*zeitFn)(char *, size_t))
{
  zeitFn_ = zeitFn;
  server_.begin();
}

void TelnetAusgabe::trennen_()
{
  client_.stop();
  telnet_neuer_client(&zustand_);
}

void TelnetAusgabe::stop()
{
  trennen_();
}

/*****************************************************************************/
/* Neuen Client annehmen, den alten verdraengen                              */
/*                                                                           */
/* Die Annahmepruefung laeuft aus dem Lese- und dem Schreibpfad, also in     */
/* jedem loop()-Durchlauf; die Differenz ist unsigned und damit ueber den    */
/* millis()-Ueberlauf hinweg richtig (Abstand 100 ms gegen 49,7 Tage).       */
/*****************************************************************************/
void TelnetAusgabe::annehmen_()
{
  const uint32_t jetzt = (uint32_t)millis();
  if ((uint32_t)(jetzt - letztePruefung_) < ANNAHME_ABSTAND_MS)
    return;
  letztePruefung_ = jetzt;

  // Gegenseite hat geschlossen und nichts mehr zu lesen: Platz freigeben
  // (WiFiClient::operator bool fragt connected() ab - deshalb ueber fd())
  if (client_.fd() >= 0 && !client_.connected() && client_.available() == 0)
    trennen_();

  WiFiClient neu = server_.accept();
  if (neu)
  {
    client_.stop(); // E2: der Neue verdraengt den Alten (stop() ohne Client ist ein No-op)
    client_ = neu;
    telnet_neuer_client(&zustand_);
  }
}

/*****************************************************************************/
/* Lesen: Tasten des Telnet-Clients. Nie blockierend - available() zaehlt nur */
/* das, was schon im Empfangspuffer liegt.                                    */
/*****************************************************************************/
int TelnetAusgabe::available()
{
  annehmen_();
  return client_ ? client_.available() : 0;
}

int TelnetAusgabe::read()
{
  return (client_ && client_.available() > 0) ? client_.read() : -1;
}

int TelnetAusgabe::peek()
{
  return (client_ && client_.available() > 0) ? client_.peek() : -1;
}

void TelnetAusgabe::flush()
{
  // Kein Sendepuffer auf unserer Seite: was send() annahm, liegt im Stack.
}

/*****************************************************************************/
/* Senden ohne Warten                                                        */
/*                                                                           */
/* send() mit MSG_DONTWAIT kehrt immer sofort zurueck: alles, ein Teil oder  */
/* EAGAIN. Es gibt hier kein select() und keine Wiederholung - genau das war */
/* in NetworkClient::write() (10 x 1 s) der Stillstand aus S2.               */
/*                                                                           */
/* Rueckgabe true nur, wenn alles im Sendepuffer gelandet ist. Teilschreiben  */
/* gilt als verworfen: der Rest der Zeile geht verloren (E7).                */
/* Ein harter Fehler (Gegenseite weg) trennt sofort.                         */
/*****************************************************************************/
bool TelnetAusgabe::senden_(const uint8_t *buf, size_t size)
{
  const int fd = client_.fd();
  if (fd < 0)
  {
    trennen_();
    return false;
  }
  const ssize_t n = ::send(fd, buf, size, MSG_DONTWAIT);
  if (n >= 0 && (size_t)n == size)
    return true;

  if (n >= 0 || errno == EAGAIN || errno == EWOULDBLOCK || errno == ENOMEM)
  {
    // Puffer voll oder nur teilweise geschrieben: nicht warten, zaehlen
    telnet_verworfen(&zustand_, (uint32_t)millis());
    return false;
  }

  // ECONNRESET, EPIPE, ENOTCONN, ...: die Gegenseite ist weg
  trennen_();
  return false;
}

void TelnetAusgabe::verworfeneMelden_(uint32_t anzahl)
{
  // Best effort: geht auch diese Zeile nicht durch, ist die Auskunft verloren -
  // sie ist nur eine Hilfe beim Lesen eines Mitschnitts, kein Messwert.
  char zeit[24] = "";
  if (zeitFn_)
    zeitFn_(zeit, sizeof(zeit));
  char zeile[96];
  const int laenge = snprintf(zeile, sizeof(zeile), "[%s] <INF> Telnet: %lu Zeilen verworfen (Client las nicht schnell genug)\n",
                              zeit, (unsigned long)anzahl);
  if (laenge > 0 && (size_t)laenge < sizeof(zeile))
    (void)::send(client_.fd(), zeile, (size_t)laenge, MSG_DONTWAIT);
}

size_t TelnetAusgabe::write(const uint8_t *buf, size_t size)
{
  if (!buf || size == 0)
    return 0;

  annehmen_();
  // Kein Client: nichts puffern, die Zeile ist weg (wie bisher - Memory
  // "Messfallen am Pruefling"). "size" melden, damit Print nicht wiederholt.
  if (!client_ || !client_.connected())
    return size;

  if (senden_(buf, size))
  {
    const uint32_t fehlten = telnet_gelungen(&zustand_);
    if (fehlten > 0)
      verworfeneMelden_(fehlten);
  }
  else if (client_ && telnet_tot(&zustand_, (uint32_t)millis()))
  {
    // 15 s lang nichts mehr gelungen: der Client liest nicht mehr
    trennen_();
  }
  return size;
}

size_t TelnetAusgabe::write(uint8_t b)
{
  return write(&b, 1);
}
