#include "debug.h"

Debug::Debug(PubSubClient& pubsub, const char* debugTopic) {
  this->pubsub = &pubsub;
  this->debugTopic = debugTopic;
}

void Debug::debug(const char* message) {
  pubsub->publish(debugTopic, message);
  Serial.println(message);
}