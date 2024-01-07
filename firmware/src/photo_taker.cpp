#include "photo_taker.h"
//#include "debug.h"
#include <ArduinoJson.h>
#include <WiFi.h>

PhotoTaker::PhotoTaker(Arducam_Mega& arducam, HttpClient& configApiClient, HttpClient& uploadClient, Debug& debug) {
  this->arducam = &arducam;
  this->debug = &debug;
  this->configApiClient = &configApiClient;
  this->uploadClient = &uploadClient;
}

bool PhotoTaker::begin(const char* cameraId, const char* apiKey) {
  strcpy(this->cameraId, cameraId);
  strcpy(this->apiKey, apiKey);
  return true;
}

char* PhotoTaker::getCameraId() {
  return this->cameraId;
}

bool PhotoTaker::getPhotoConfig() {
  configApiClient->beginRequest();

  char url[60];
  sprintf(url, "/prod/uploads/%s", this->cameraId);
  configApiClient->get(url);
  configApiClient->sendHeader("Authorization", this->apiKey);
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


    state = PHOTO_CONFIG_UPDATED;
    lastStateChange = millis();

    return true;
  }

  return false;
}

void PhotoTaker::resetCamera() {
  arducam->reset();
  arducam->lowPowerOff();
  state = CAMERA_RESET;
  lastStateChange = millis();
}

void PhotoTaker::configureCamera() {
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

    state = CAMERA_CONFIGURED;
    lastStateChange = millis();
}

bool PhotoTaker::takePicture() {
  arducam->stopPreview();
  arducam->takePicture(CAM_IMAGE_MODE_WQXGA2, CAM_IMAGE_PIX_FMT_JPG);
  CamStatus status = arducam->takePicture(CAM_IMAGE_MODE_WQXGA2, CAM_IMAGE_PIX_FMT_JPG);
  arducam->lowPowerOn();
  debug->debug("Picture taken");
  state = PICTURE_TAKEN;
  lastStateChange = millis();
  return status == CAM_ERR_SUCCESS; 
}

bool PhotoTaker::startUpload() {
    pictureLength = arducam->getTotalLength();
    bytesUploaded = 0;
    bufferPosition = 0;

    sprintf(debug->debugBuffer, "Picture length is: %d\n", pictureLength);
    debug->debug(debug->debugBuffer);

    uploadClient->beginRequest();
    uploadClient->put(serverResponse.uploadUrl);
    uploadClient->sendHeader("Content-Type", "image/jpeg");
    uploadClient->sendHeader("Content-Length", pictureLength);
    uploadClient->beginBody();

    state = UPLOAD_STARTED;
    lastStateChange = millis();

    return true;
}

bool PhotoTaker::continueUpload() {

    if (arducam->getReceivedLength()) {
      imageBuff[bufferPosition++] = arducam->readByte();
      if (bufferPosition >= BUFFER_SIZE) {
          //outFile.write(imageBuff, i);
          uploadClient->write(imageBuff, bufferPosition);
          bytesUploaded += bufferPosition;
          bufferPosition = 0;

          if (millis() - lastUploadUpdate > 5000) {
            lastUploadUpdate = millis();
            sprintf(debug->debugBuffer, "Written %d bytes out of %d\n", bytesUploaded, pictureLength);
            debug->debug(debug->debugBuffer);            
          }
      }
    } else {
      // send the rest
      uploadClient->write(imageBuff, bufferPosition);
      bytesUploaded += bufferPosition;

      sprintf(debug->debugBuffer, "Written %d bytes\n", bytesUploaded);
      debug->debug(debug->debugBuffer);

      uploadClient->endRequest();

      int statusCode = uploadClient->responseStatusCode();
      String response = uploadClient->responseBody();

      sprintf(debug->debugBuffer, "Status code after upload %d\n", statusCode);
      debug->debug(debug->debugBuffer);

      sprintf(debug->debugBuffer, "Upload response '%s'\n", response.c_str());
      debug->debug(debug->debugBuffer);

      state = IDLE;
      lastStateChange = millis();
    }

    return true;
}

void PhotoTaker::loop() {
  long now = millis();

  if (state == IDLE && now - lastStateChange > 30 * 1000) {
    if (WiFi.status() != WL_CONNECTED) {
      debug->debug("WiFi is not connected, skipping api call");
      return;
    }    
    bool result = getPhotoConfig();
    if (!result) {
      debug->debug("Failed to get photo config");
    }
  } else if (state == PHOTO_CONFIG_UPDATED && serverResponse.takePicture) {
    debug->debug("Resetting camera");
    resetCamera();
  } else if (state == CAMERA_RESET && now - lastStateChange > 2 * 1000) {
    debug->debug("Configuring camera");
    configureCamera();    
  } else if (state == CAMERA_CONFIGURED && now - lastStateChange > 2 * 1000) {
    takePicture();  
  } else if (state == PICTURE_TAKEN) {
    startUpload();
  } else if (state == UPLOAD_STARTED) {
    continueUpload();
  }
}