#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include "telegram.h"   // Typ-, Laengen- und Pruefsummenregel des Antworttelegramms
#include "sendwindow.h" // Deckel des Sammelfensters, Grenze fuers Verschieben
#include "notbetrieb.h" // Werte, Schrittfolge und Zeitregeln des Notbetriebs
#include "rtcspiegel.h" // Gueltigkeitsregel des Spiegels im RTC-Speicher
#include "verbindung.h" // Karenz und Ausfalldauer der Verbindung zur Hausteuerung
#include "decode.h"     // MAXVALUELEN/NUMBEROFTOPICS fuer actual_data-Parameter

// platform layer: the official HeishaMon ESP32-S3 board. Until 3.15.0 the same
// firmware also built for the D1 mini (ESP8266); that branch was dropped in
// 3.16.0, everything board specific still lives here.
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <HTTPUpdateServer.h>
#include <DNSServer.h>
// quotes on purpose: pulls sstaub/Ticker from lib_deps. <Ticker.h> would find
// the core's own Ticker with a different API and break the timing chain.
#include "Ticker.h"
#include <TelnetStream.h>

// class names kept behind typedefs since the 3.0.0 port
typedef WebServer WebServerClass;
typedef HTTPUpdateServer HTTPUpdateServerClass;

// heatpump on its own UART (pins from the official HeishaMon ESP32-S3 board),
// Serial stays available as USB console
#define heatpumpSerial Serial1
#define HEATPUMPRX 18
#define HEATPUMPTX 17
#define ENABLEPIN 5   // mosfet enable for TX line to heatpump
#define ENABLEOTPIN 4 // OpenTherm 24V booster - unused, must stay LOW

// posix TZ string (identical to TZ_Europe_Berlin)
#define TIME_ZONE "CET-1CEST,M3.5.0,M10.5.0/3"

#define MAXDATASIZE 256
#define QUERYSIZE 110

#define UPDATEALLTIME 300000 // time to resend all to mqtt
#define MQTT_RETAIN_VALUES 1

// config your timing
// Die zwei Grenzen des Sammelfensters (COMMAND_WINDOW_MAX, COMMAND_DEFER_MAX)
// stehen bewusst nicht hier, sondern in sendwindow.h - sie gehoeren zu den
// Regeln, die der Hosttest mituebersetzt.
#define COMMANDTIMER 500  // Command / timer to send commands from buffer to HP
#define QUERYTIMER 5000   // Query / timer to initiate a query
#define BUFFERTIMEOUT 500 // Serial Buffer Filltime / timer to fill the UART buffer with all 203 bytes from HP board
#define SERIALTIMEOUT 600 // Serial Timout / timer to wait on serial to read all 203 bytes from HP
#define NTPTIMEOUT 30000  // max. wait for NTP at boot before continuing without valid time

// MQTT-Wiederverbindung: PubSubClient wartet ohne setSocketTimeout 15 s je
// Versuch, und zwar mitten in loop() - der 5-s-Abfragetakt der Waermepumpe
// steht so lange still. 2 s reichen im LAN, der Rest ist Backoff.
#define MQTT_SOCKET_TIMEOUT_S 2   // Sekunden, ersetzt den Bibliotheksstandard 15
#define MQTT_RECONNECT_MIN 5000   // erster Wiederverbindungsversuch nach 5 s
#define MQTT_RECONNECT_MAX 60000  // Obergrenze, danach im Minutentakt

// Karenzzeit nach jedem erfolgreichen Abonnieren der Set-Topics. Der
// ioBroker-MQTT-Adapter beantwortet ein neues Abonnement aus seiner
// Objektdatenbank und schickt den gespeicherten Wert JEDES Set-Topics -
// teils Monate alt. Ohne diese Sperre laufen sie als frische Kommandos in die
// Waermepumpe (gemessen 2026-08-13, s. Changelog 3.6.1). Der Schwall kommt
// unmittelbar nach dem SUBACK, 5 s sind reichlich bemessen.
#define SUBSCRIBE_GRACE 5000

// WLAN-Watchdog: ohne WLAN ist das Geraet fuer die Kaskadenregelung blind.
// Stufe 1 erneuter Verbindungsversuch, Stufe 2 Neustart.
#define WIFI_RETRY_TIMEOUT 30000   // 30 s ohne WLAN -> WiFi.reconnect()
#define WIFI_REBOOT_TIMEOUT 300000 // 5 min ohne WLAN -> Neustart

void send_pana_command(void);
void send_pana_mainquery(void);
void read_pana_data(void);
void timeout_serial(void);
void flush_serial_input(void);
void check_wifi(void);
void write_mqtt_log(char *);
void write_telnet_log(char *);
void register_new_command(void);

// Global command buffer plus the bits already claimed in it (see commands.cpp)
extern byte mainCommand[QUERYSIZE];
extern byte usedMask[QUERYSIZE];

// true once at least one real SET field sits in mainCommand. Replaces the old
// byte-sum heuristic, which could not tell "empty buffer" from "sum wrapped to 0"
extern bool setDataPending;

// query timer, needs restart from mqtt_callback if a command was rejected
extern Ticker Send_Pana_Mainquery_Timer;

// Notbetrieb (notbetrieb.cpp). Die Regeln stehen arduino-frei in notbetrieb.h,
// hier nur die Anbindung ans Geraet.
//
// spiegel_gueltig kommt aus rtc_spiegel_boot() und sagt, ob im RTC-Speicher
// ein Wertesatz des vorigen Laufs steht. Der Parameter statt eines Aufrufs
// von rtc_spiegel_boot() hier drin: Der Spiegel traegt auch den Bootzaehler,
// der den Notbetrieb nichts angeht. Die Entscheidung "war da etwas" faellt
// deshalb genau einmal in setup(), und beide Nutzer bekommen sie gereicht.
void notbetrieb_init(bool spiegel_gueltig);
bool notbetrieb_subscribe(PubSubClient &);
// true, wenn das Topic in den Notbetriebszweig gehoerte - dann ist die
// Nachricht abschliessend behandelt und laeuft NICHT weiter in den Set-Pfad
bool notbetrieb_mqtt_annehmen(const char *topic, const char *msg);
void notbetrieb_loop(char actual[][MAXVALUELEN]); // Tick aus loop()
bool notbetrieb_starten(void);                    // vom Webhandler
void notbetrieb_status(char *out, size_t len);     // Zustand;Schritt;Schritte;fehlend;Sperre
                                                  // (Lage, Dauer, Kurvenwarnung und
                                                  //  Abbruchgrund haengt die Statusroute an)
// Warum der Knopf gesperrt ist. In notbetrieb_loop() je Durchlauf aus TOP101
// und den gehaltenen Werten bestimmt, weil die Webhandler kein actual_data
// haben. NOTBETRIEB_FREI heisst: der Knopf darf gedrueckt werden.
NotbetriebSperre notbetrieb_sperre(void);
// Plausibilitaet der gehaltenen Kurve (nur Rolle Heizen). WARNT, sperrt nicht -
// die Regel steht in notbetrieb.h, der Text auf der Seite in webfunctions.cpp.
NotbetriebKurvenWarnung notbetrieb_kurvenwarnung(void);

// Warum ein Lauf abgebrochen wurde. Die Seite macht daraus Klartext: Beim
// Hydraulik-Grund fuehrt der Weg zurueck ueber den Schalter im Waschraum,
// nicht ueber die Firmware.
NotbetriebAbbruchgrund notbetrieb_abbruchgrund(void);

// Adresse des Tasmota-Switch fuer die Hydraulik (config.json, Feld
// "hydraulik_switch"). Leer heisst "nicht eingerichtet" - dann bricht der
// Notbetrieb im ersten Schritt ab.
extern char hydraulik_switch[];

// Rolle dieser Stufe (Build-Flag) und der gehaltene Zustand
extern const NotbetriebRolle notbetriebRolle;
extern NotbetriebSpeicher notbetriebWerte;
extern NotbetriebLauf notbetriebLauf;

// Der Spiegel im RTC-Speicher (HeishaMon.cpp). Er liegt in HeishaMon.cpp und
// nicht in notbetrieb.cpp, weil er zwei Dinge traegt, die sich nur den
// Speicher teilen: die Notbetriebswerte (M2) und den Bootzaehler (M3). Ein
// gemeinsamer Block heisst eine Pruefsumme und eine Gueltigkeitsfrage -
// siehe rtcspiegel.h.
extern RtcSpiegel rtcSpiegel;

// Verbindungswacht (verbindung.h). Aus loop() nachgefuehrt, von den
// Webseiten gelesen: Startseite und Notbetriebsseite melden daraus, ob die
// Hausteuerung erreichbar ist. Sie haengt an mqtt_client.connected() und
// nicht am WLAN - der Ausfall, um den es geht, ist der des ioBroker, und
// der laesst das WLAN unberuehrt.
extern VerbindungsWacht hausteuerung;
