#include "photo_taker.h"
//#include "debug.h"
#include <ArduinoJson.h>
#include <WiFi.h>

PhotoTaker::PhotoTaker(const char* cameraId, Arducam_Mega& arducam, HttpClient& configApiClient, HttpClient& uploadClient, Debug& debug) {
  this->arducam = &arducam;
  this->debug = &debug;
  this->configApiClient = &configApiClient;
  this->uploadClient = &uploadClient;
  strcpy(this->cameraId, cameraId);
}

bool PhotoTaker::begin() {
  return true;
}

bool PhotoTaker::getPhotoConfig() {
  configApiClient->beginRequest();

  char url[40];
  sprintf(url, "/prod/uploads/%s", this->cameraId);
  configApiClient->get(url);
  configApiClient->sendHeader("Authorization", "allow");
  configApiClient->endRequest();

  // read the status code and body of the response
  int statusCode = configApiClient->responseStatusCode();
  String response = configApiClient->responseBody();
  Serial.print("Status code: ");
  Serial.println(statusCode);
  if (statusCode == 200) {
    
    StaticJsonDocument<3000> doc;
    DeserializationError error = deserializeJson(doc, response);

    debug->debug(response.c_str());

    // Test if parsing succeeds.
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return false;
    }

    strcpy(serverResponse.uploadUrl, doc["uploadURL"]);
    serverResponse.takePicture = doc["takePicture"];
    serverResponse.config.autoExposure = doc["config"]["autoExposure"];
    serverResponse.config.autoFocus = doc["config"]["autoFocus"];
    serverResponse.config.autoISOSensitive = doc["config"]["autoISOSensitive"];
    serverResponse.config.autoWhiteBalance = doc["config"]["autoWhiteBalance"];
    serverResponse.config.autoWhiteBalanceMode = doc["config"]["autoWhiteBalanceMode"];
    serverResponse.config.absoluteExposure = doc["config"]["absoluteExposure"];
    serverResponse.config.brightness = doc["config"]["brightness"];
    serverResponse.config.contrast = doc["config"]["contrast"];
    serverResponse.config.EV = doc["config"]["EV"];
    serverResponse.config.ISOSensitivity = doc["config"]["ISOSensitivity"];
    serverResponse.config.saturation = doc["config"]["saturation"];
    serverResponse.config.sharpness = doc["config"]["sharpness"];

    return true;
  }

  return false;
}

void PhotoTaker::configureCamera() {
    arducam->reset();
    //arducam->begin();
    delay(2000);
    arducam->setAbsoluteExposure(serverResponse.config.absoluteExposure);
    arducam->setAutoExposure(serverResponse.config.autoExposure);
    arducam->setAutoFocus(serverResponse.config.autoFocus);
    arducam->setAutoISOSensitive(serverResponse.config.autoISOSensitive);
    arducam->setAutoWhiteBalance(serverResponse.config.autoWhiteBalance);
    arducam->setAutoWhiteBalanceMode(CAM_WHITE_BALANCE(serverResponse.config.autoWhiteBalanceMode));
    arducam->setBrightness(CAM_BRIGHTNESS_LEVEL(serverResponse.config.brightness));
    arducam->setContrast(CAM_CONTRAST_LEVEL(serverResponse.config.contrast));
    arducam->setEV(CAM_EV_LEVEL(serverResponse.config.EV));
    arducam->setISOSensitivity(serverResponse.config.ISOSensitivity);
    arducam->setSaturation(CAM_STAURATION_LEVEL(serverResponse.config.saturation));
    arducam->setSharpness(CAM_SHARPNESS_LEVEL(serverResponse.config.sharpness));
    arducam->setImageQuality(HIGH_QUALITY);
}

bool PhotoTaker::uploadPhoto() {
    uint32_t length = arducam->getTotalLength();
    Serial.printf("Picture length is: %d\n", length); 

    uploadClient->beginRequest();
    uploadClient->put(serverResponse.uploadUrl);
    uploadClient->sendHeader("Content-Type", "image/jpeg");
    uploadClient->sendHeader("Content-Length", length);
    uploadClient->beginBody();

    unsigned int writtenBytes = 0;
    int i = 0;
    int bytesSent = 0;
    unsigned long start = millis();
    // give 3 minutes to upload, otherwise abort
    while (arducam->getReceivedLength() && millis() - start < 180 * 1000) {

      imageBuff[i++] = arducam->readByte();
      if (i >= BUFFER_SIZE) {
          //outFile.write(imageBuff, i);
          uploadClient->write(imageBuff, i);
          bytesSent += i;
          i = 0;
      }
    }

    // send the rest
    uploadClient->write(imageBuff, i);
    bytesSent += i;

    sprintf(debug->debugBuffer, "Written %d bytes\n", bytesSent);
    debug->debug(debug->debugBuffer);

    uploadClient->endRequest();

    int statusCode = uploadClient->responseStatusCode();
    String response = uploadClient->responseBody();

    sprintf(debug->debugBuffer, "Status code after upload %d\n", statusCode);
    debug->debug(debug->debugBuffer);

    sprintf(debug->debugBuffer, "Upload response '%s'\n", response.c_str());
    debug->debug(debug->debugBuffer);

    Serial.printf("Still left %s\n", arducam->getReceivedLength());

    return true;
}

void PhotoTaker::loop() {
  long now = millis();
  if (now - lastConfigCheck > 30 * 1000) {
    lastConfigCheck = now;

    if (WiFi.status() != WL_CONNECTED) {
      debug->debug("WiFi is not connected, skipping api call");
      return;
    }

    if (getPhotoConfig()) {
      if (serverResponse.takePicture) {
        debug->debug("Taking a picture");
        //Serial.println("UPLOAD URL:");
        //Serial.println(serverResponse.uploadUrl);
        configureCamera();
        debug->debug("Camera configured");
        delay(3000);
        arducam->stopPreview();
        arducam->takePicture(CAM_IMAGE_MODE_WQXGA2, CAM_IMAGE_PIX_FMT_JPG);
        CamStatus status = arducam->takePicture(CAM_IMAGE_MODE_WQXGA2, CAM_IMAGE_PIX_FMT_JPG);
        debug->debug("Picture taken");
        if (status == CAM_ERR_SUCCESS) {
          uint32_t start = millis();
          if (uploadPhoto()) {
            sprintf(debug->debugBuffer, "Photo uploaded in %ds\n", (millis() - start)/1000);
            debug->debug(debug->debugBuffer);
          } else {
            debug->debug("Failed to upload photo");
          }
        } else {
          debug->debug("Failed to take picture");
        }

      } else {
        debug->debug("Not taking a picture");
      }
    } else {
      debug->debug("Failed to get camera config");
    }
  }
}