#include "Arduino.h"
#include <SPI.h>

#include "LittleFS.h"
#include <WiFi.h>
#include <PubSubClient.h>

// char commandTopic[] = "indoorPlantsWatering/command";
// char debugTopic[] = "indoorPlantsWatering/debug";
// char pumpPins[4] = {18, 19, 20, 21};

char commandTopic[] = "sprouts/command";
char debugTopic[] = "sprouts/debug";
char pumpPins[4] = {18, 19, 20, 21};

WiFiClient wifiClient;
PubSubClient client("192.168.1.100", 1883, wifiClient);

void debug(const char* message) {
  client.publish(debugTopic, message);
  Serial.println(message);
}

void initWiFi() {

  File credentials = LittleFS.open("/wifi", "r");
  if (!credentials) {
      Serial.println("file open failed");
  }
  String ssid = credentials.readStringUntil('\n');
  String password = credentials.readStringUntil('\n');

  Serial.printf("'%s'", ssid.c_str());
  Serial.println("");
  Serial.printf("'%s'", password.c_str());

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println("\nConnected to WiFi");
  Serial.println(WiFi.localIP());
  WiFi.persistent(true);
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "Pi Pico-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    File credentials = LittleFS.open("/mqtt", "r");
    if (!credentials) {
        Serial.println("file open failed");
    }
    String username = credentials.readStringUntil('\n');
    String password = credentials.readStringUntil('\n');

    if (client.connect(clientId.c_str(), username.c_str(), password.c_str())) {
      Serial.println("connected");
      client.subscribe(commandTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

char debugBuffer[255];
char buffer[128];
unsigned long pumpDurationMs = 0;
unsigned long pumpStart = 0;
int pumpId = -1;
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i=0;i<length;i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  memcpy(buffer, payload, length);
  buffer[length] = '\0';

  int pump;
  int seconds;

  int parsed = sscanf(buffer, "%d,%d", &pump, &seconds);
  sprintf(debugBuffer, "Requested watering on pump: %d, seconds: %d\n", pump, seconds);
  debug(debugBuffer);

  pumpStart = millis();
  pumpDurationMs = seconds * 1000;
  pumpId = pump;
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  for (int i = 0; i < 4; i++) {
    pinMode(pumpPins[i], OUTPUT);
  }

  Serial.begin(9600);
//  while (!Serial) {
//  }

  if (!LittleFS.begin()) {
    Serial.println("Failed to set up filesystem!\n");
  }

  initWiFi();
  client.setCallback(callback);
  reconnect();
  Serial.println("Connected to MQTT");
}

void loop() {
  if (!client.connected()) {
    digitalWrite(LED_BUILTIN, LOW);
    reconnect();
  }

  digitalWrite(LED_BUILTIN, HIGH);

  if (pumpId != -1) {
    if (millis() - pumpStart < pumpDurationMs) {
      digitalWrite(pumpPins[pumpId], HIGH);
    } else {
      digitalWrite(pumpPins[pumpId], LOW);
      pumpId = -1;
    }
  }

  client.loop();
}