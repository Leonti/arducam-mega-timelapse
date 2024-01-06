#include "Arduino.h"
#include <ArduinoHttpClient.h>
#include <SPI.h>

#include "LittleFS.h"
#include <WiFi.h>
#include <PubSubClient.h>
// #include <ArduinoHA.h>
#include "Arducam_Mega.h"

#include "debug.h"
#include "photo_taker.h"

#define CSN_PIN 17
#define MISO_PIN 16
#define MOSI_PIN 19
#define SCK_PIN 18

const char* cameraId = "indoor-plants";
const char* COMMAND_TOPIC = "camera-indoor-plants/command";
const char* CONFIG_TOPIC = "camera-indoor-plants/config";
const char* DEBUG_TOPIC = "camera-indoor-plants/debug";

WiFiClientSecure wifiClientSecure;
WiFiClient wifiClient;
PubSubClient mqttClient("192.168.1.100", 1883, wifiClient);

// Arducam_Mega needs to be modified to init SPI in `begin`
Arducam_Mega arducam(CSN_PIN);

HttpClient httpClient = HttpClient(wifiClientSecure, "cameras-s3-s3uploadbucket-f6yi0ntjqcfx.s3.ap-southeast-2.amazonaws.com", 443);
HttpClient configApiClient = HttpClient(wifiClientSecure, "8d74vufyr9.execute-api.ap-southeast-2.amazonaws.com", 443);

Debug debug = Debug(mqttClient, DEBUG_TOPIC);

PhotoTaker photoTaker = PhotoTaker(cameraId, arducam, configApiClient, httpClient, debug);

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
  //WiFi.begin("LeontiAndroid", "0abca18a4c50");
  Serial.print("Connecting to WiFi ..");
  WiFi.persistent(true);
}

unsigned int i = 0;
uint8_t imageData = 0;
uint8_t imageDataNext = 0;
uint8_t headFlag = 0;

uint8_t buffer[255];

String clientId = "Pi Pico Camera-" + String(random(0xffff), HEX);

bool mqttReconnect() {
  Serial.println("Attempting MQTT connection...");
  // Attempt to connect
  if (mqttClient.connect(clientId.c_str(), "mqtt", "mqtt")) {
    debug.debug("MQTT Connected");
    mqttClient.subscribe(COMMAND_TOPIC);
    mqttClient.subscribe(CONFIG_TOPIC);
  }

  return mqttClient.connected();
}

void setup() {

  Serial.begin(9600);
  // while (!Serial) {
  // }

  if (!LittleFS.begin()) {
    Serial.println("Failed to set up filesystem!\n");
  }

  wifiClientSecure.setNoDelay(true);
  wifiClientSecure.setSync(true);
  wifiClientSecure.setInsecure();

  initWiFi();

  //client.setSocketTimeout(30000);
 // reconnect();
 // Serial.println("Connected to MQTT");

  SPI.setRX(MISO_PIN);
  SPI.setTX(MOSI_PIN);
  SPI.setSCK(SCK_PIN);
  SPI.setCS(CSN_PIN);
  SPI.begin();

  Serial.println("Setting up camera");
  
  //arducam = &Arducam_Mega(CSN_PIN);

  //photoTaker.begin();

  if (arducam.begin() != CAM_ERR_SUCCESS) {
    Serial.println("Failed to initialise Camera");
  } else {
    Serial.println("Initialised Camera");
//    arducam.setAutoExposure(0);
//    arducam.setAutoISOSensitive(0);
    //arducam.setAutoWhiteBalance(0);
//    arducam.setAutoFocus(0x02); // 0x01 is ok
    //arducam.lowPowerOn();
  }

  Serial.println(arducam.getCameraInstance()->myCameraInfo.cameraId);

  Serial.println("Camera is set up");
}

bool isWifiConnected = false;
unsigned long lastReconnectAttempt = 0;
unsigned long lastWiFiStatusCheck = 0;
unsigned long lastMqttStatusCheck = 0;
unsigned long lastWifiDisconnectStart = 0;

void loop() {
  long now = millis();

  if (now - lastWiFiStatusCheck > 5000) {
    lastWiFiStatusCheck = now;
    if (WiFi.status() != WL_CONNECTED) {
      isWifiConnected = false;
      Serial.println("WiFi is not connected!");
      if (lastWifiDisconnectStart == 0) {
        lastWifiDisconnectStart = millis();
        Serial.println("WiFi disconnect detecting, waiting for 2 minutes before reconnecting");
      } else if (now - lastWifiDisconnectStart > 60 * 1000 * 2) {
        lastWifiDisconnectStart = 0;
        Serial.println("WiFi has not reconnected by itself in 2 minutes, reconnecting manually");
        WiFi.disconnect();
        WiFi.end();
        initWiFi();
        long reconnectTime = millis() - now;
        Serial.printf("Reconnect init took %dms\n", reconnectTime);
      }

    } else if (WiFi.status() == WL_CONNECTED && !isWifiConnected) {
      isWifiConnected = true;
      lastWifiDisconnectStart = -1;
      Serial.println("Connected to WiFi!");
      Serial.println(WiFi.localIP());
    }
  }

  if (now - lastMqttStatusCheck > 5000) {
    lastMqttStatusCheck = now;
    if (!mqttClient.connected()) {
      digitalWrite(LED_BUILTIN, LOW);
      // Attempt to reconnect
      mqttReconnect();
    } else {
      digitalWrite(LED_BUILTIN, HIGH);
    }
  }

  mqttClient.loop();


  photoTaker.loop();
}