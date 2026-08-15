#pragma once
// MQTT-Ersatz fuer die Hosttests: publish() nimmt alles an und tut nichts.
class PubSubClient
{
public:
  bool publish(const char *, const char *, bool) { return true; }
};
