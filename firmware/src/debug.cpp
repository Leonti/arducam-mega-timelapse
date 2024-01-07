#include "debug.h"

Debug::Debug(PubSubClient& pubsub) {
  this->pubsub = &pubsub; 
}

bool Debug::begin(const char* debugTopic) {
  strcpy(this->debugTopic, debugTopic);
  isReady = true;
  return true;
}

void Debug::debug(const char* message) {
  if (isReady) {
    pubsub->publish(debugTopic, message);
  }
  Serial.println(message);
}