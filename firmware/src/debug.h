#ifndef DEBUG_H
#define DEBUG_H

#include <PubSubClient.h>
#include "Arduino.h"

class Debug {
public:
  Debug(PubSubClient& pubsub, const char* debugTopic);
  void debug(const char* message);
  char debugBuffer[300];

private:
  PubSubClient* pubsub;
  const char* debugTopic;
};
#endif