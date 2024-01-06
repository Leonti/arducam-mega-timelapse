#ifndef PHOTO_TAKER_H
#define PHOTO_TAKER_H

#include "debug.h"
#include <ArduinoHttpClient.h>
#include "Arducam_Mega.h"

#define  BUFFER_SIZE  1024

struct CameraConfig {
  uint8_t autoExposure;
  uint8_t autoFocus; // 0-4
  uint8_t autoISOSensitive;
  uint8_t autoWhiteBalance;
  uint8_t autoWhiteBalanceMode; // 0-4 auto,sunny,office,cloudy,home
  uint32_t absoluteExposure; // exposure in ms? uint32_t
  uint8_t brightness; // 0-8
  uint8_t contrast; // 0-6
  uint8_t EV; // 0-6
  uint8_t ISOSensitivity; // manual gain int
  uint8_t saturation; // 0-6
  uint8_t sharpness; // 0-8     
};

struct ServerResponse {
  bool takePicture;
  char uploadUrl[3000];
  CameraConfig config;
};

class PhotoTaker {
public:
  PhotoTaker(const char* cameraId, Arducam_Mega& arducam, HttpClient& configApiClient, HttpClient& uploadClient, Debug& debug);
  bool begin();
  void loop();

private:
  char cameraId[20];
  uint8_t imageBuff[BUFFER_SIZE] = {0};
  unsigned long lastConfigCheck = 0;
  Debug* debug;
  HttpClient* configApiClient;
  HttpClient* uploadClient;
  Arducam_Mega* arducam;
  ServerResponse serverResponse;
  bool getPhotoConfig();
  void configureCamera();
  bool uploadPhoto();
};
#endif