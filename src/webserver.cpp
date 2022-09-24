/*
MIT License

Copyright (c) 2021-22 Magnus

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 */
#include <config.hpp>
#include <helper.hpp>
#include <main.hpp>
#include <pushtarget.hpp>
#include <resources.hpp>
#include <templating.hpp>
#include <tempsensor.hpp>
#include <webserver.hpp>
#include <wifi.hpp>

WebServerHandler myWebServerHandler;  // My wrapper class fr webserver functions

void WebServerHandler::webHandleConfig() {
  LOG_PERF_START("webserver-api-config");
  Log.notice(F("WEB : webServer callback for /api/config(get)." CR));

  DynamicJsonDocument doc(2000);
  myConfig.createJson(doc);

  doc[PARAM_PASS] = "";  // dont show the wifi password
  doc[PARAM_PASS2] = "";

  doc[PARAM_APP_VER] = String(CFG_APPVER);
  doc[PARAM_APP_BUILD] = String(CFG_GITREV);

  // Format the adjustment so we get rid of rounding errors
  if (myConfig.isTempF())
    // We want the delta value (32F = 0C).
    doc[PARAM_TEMP_ADJ] =
        reduceFloatPrecision(convertCtoF(myConfig.getTempSensorAdjC()) - 32, 1);
  else
    doc[PARAM_TEMP_ADJ] = reduceFloatPrecision(myConfig.getTempSensorAdjC(), 1);

  doc[PARAM_BATTERY] = reduceFloatPrecision(myBatteryVoltage.getVoltage());

#if LOG_LEVEL == 6
  serializeJson(doc, Serial);
  Serial.print(CR);
#endif

  String out;
  out.reserve(2000);
  serializeJson(doc, out);
  doc.clear();
  _server->send(200, "application/json", out.c_str());
  LOG_PERF_STOP("webserver-api-config");
}

void WebServerHandler::webHandleUploadFile() {
  LOG_PERF_START("webserver-api-upload-file");
  Log.verbose(F("WEB : webServer callback for /api/upload(post)." CR));
  HTTPUpload& upload = _server->upload();
  String f = upload.filename;
  bool firmware = false;

  if (f.endsWith(".bin")) {
    firmware = true;
  }

#if LOG_LEVEL == 6
  Log.verbose(
      F("WEB : webServer callback for /api/upload, receiving file %s, %d(%d) "
        "firmware=%s." CR),
      f.c_str(), upload.currentSize, upload.totalSize, firmware ? "yes" : "no");
#endif

  if (firmware) {
    // Handle firmware update, hardcode since function return wrong value.
    // (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    uint32_t maxSketchSpace = MAX_SKETCH_SPACE;

    if (upload.status == UPLOAD_FILE_START) {
      _uploadReturn = 200;
      Log.notice(F("WEB : Start firmware upload, max sketch size %d kb." CR),
                 maxSketchSpace / 1024);

      if (!Update.begin(maxSketchSpace, U_FLASH, PIN_LED)) {
        Log.error(F("WEB : Not enough space to store for this firmware." CR));
        _uploadReturn = 500;
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      Log.notice(F("WEB : Writing firmware upload %d (%d)." CR),
                 upload.totalSize, maxSketchSpace);

      if (upload.totalSize > maxSketchSpace) {
        Log.error(F("WEB : Firmware file is to large." CR));
        _uploadReturn = 500;
      } else if (Update.write(upload.buf, upload.currentSize) !=
                 upload.currentSize) {
        Log.warning(F("WEB : Firmware write was unsuccessful." CR));
        _uploadReturn = 500;
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      Log.notice(F("WEB : Finish firmware upload." CR));
      if (Update.end(true)) {
        _server->send(200);
        delay(500);
        ESP_RESET();
      } else {
        Log.error(F("WEB : Failed to finish firmware flashing error=%d" CR),
                  Update.getError());
        _uploadReturn = 500;
      }
    } else {
      Update.end();
      Log.notice(F("WEB : Firmware flashing aborted." CR));
      _uploadReturn = 500;
    }

    delay(0);
  }

  LOG_PERF_STOP("webserver-api-upload-file");
}

void WebServerHandler::webHandleStatus() {
  LOG_PERF_START("webserver-api-status");
  Log.notice(F("WEB : webServer callback for /api/status(get)." CR));

  DynamicJsonDocument doc(500);

  double tempC = myTempSensor.getTempC();

  doc[PARAM_ID] = myConfig.getID();
  doc[PARAM_TEMP_C] = reduceFloatPrecision(tempC, 1);
  doc[PARAM_TEMP_F] = reduceFloatPrecision(convertCtoF(tempC), 1);
  doc[PARAM_BATTERY] = reduceFloatPrecision(myBatteryVoltage.getVoltage());
  doc[PARAM_TEMPFORMAT] = String(myConfig.getTempFormat());
  doc[PARAM_RSSI] = WiFi.RSSI();
  doc[PARAM_SLEEP_INTERVAL] = myConfig.getSleepInterval();
  doc[PARAM_TOKEN] = myConfig.getToken();

  doc[PARAM_APP_VER] = CFG_APPVER;
  doc[PARAM_APP_BUILD] = CFG_GITREV;
  doc[PARAM_MDNS] = myConfig.getMDNS();
  doc[PARAM_SSID] = WiFi.SSID();

#if LOG_LEVEL == 6
  serializeJson(doc, Serial);
  Serial.print(CR);
#endif

  String out;
  out.reserve(500);
  serializeJson(doc, out);
  doc.clear();
  _server->send(200, "application/json", out.c_str());
  LOG_PERF_STOP("webserver-api-status");
}

void WebServerHandler::webHandleClearWIFI() {
  String id = _server->arg(PARAM_ID);
  Log.notice(F("WEB : webServer callback for /api/clearwifi." CR));

  if (!id.compareTo(myConfig.getID())) {
    _server->send(200, "text/plain",
                  "Clearing WIFI credentials and doing reset...");
    myConfig.setWifiPass("", 0);
    myConfig.setWifiSSID("", 0);
    myConfig.setWifiPass("", 1);
    myConfig.setWifiSSID("", 1);
    myConfig.saveFile();
    delay(1000);
    WiFi.disconnect();  // Clear credentials
    ESP_RESET();
  } else {
    _server->send(400, "text/plain", "Unknown ID.");
  }
}

void WebServerHandler::webHandleConfigDevice() {
  LOG_PERF_START("webserver-api-config-device");
  String id = _server->arg(PARAM_ID);
  Log.notice(F("WEB : webServer callback for /api/config/device(post)." CR));

  if (!id.equalsIgnoreCase(myConfig.getID())) {
    Log.error(F("WEB : Wrong ID received %s, expected %s" CR), id.c_str(),
              myConfig.getID());
    _server->send(400, "text/plain", "Invalid ID.");
    LOG_PERF_STOP("webserver-api-config-device");
    return;
  }

#if LOG_LEVEL == 6
  Log.verbose(F("WEB : %s." CR), getRequestArguments().c_str());
#endif

  if (_server->hasArg(PARAM_MDNS))
    myConfig.setMDNS(_server->arg(PARAM_MDNS).c_str());
  if (_server->hasArg(PARAM_TEMPFORMAT))
    myConfig.setTempFormat(_server->arg(PARAM_TEMPFORMAT).charAt(0));
  if (_server->hasArg(PARAM_SLEEP_INTERVAL))
    myConfig.setSleepInterval(_server->arg(PARAM_SLEEP_INTERVAL).c_str());
  myConfig.saveFile();
  _server->sendHeader("Location", "/config.htm#collapseDevice", true);
  _server->send(302, "text/plain", "Device config updated");
  LOG_PERF_STOP("webserver-api-config-device");
}

void WebServerHandler::webHandleConfigPush() {
  LOG_PERF_START("webserver-api-config-push");
  String id = _server->arg(PARAM_ID);
  Log.notice(F("WEB : webServer callback for /api/config/push(post)." CR));

  if (!id.equalsIgnoreCase(myConfig.getID())) {
    Log.error(F("WEB : Wrong ID received %s, expected %s" CR), id.c_str(),
              myConfig.getID());
    _server->send(400, "text/plain", "Invalid ID.");
    LOG_PERF_STOP("webserver-api-config-push");
    return;
  }
#if LOG_LEVEL == 6
  Log.verbose(F("WEB : %s." CR), getRequestArguments().c_str());
#endif

  if (_server->hasArg(PARAM_TOKEN))
    myConfig.setToken(_server->arg(PARAM_TOKEN).c_str());
  if (_server->hasArg(PARAM_PUSH_HTTP))
    myConfig.setHttpUrl(_server->arg(PARAM_PUSH_HTTP).c_str());
  if (_server->hasArg(PARAM_PUSH_HTTP_H1))
    myConfig.setHttpHeader(_server->arg(PARAM_PUSH_HTTP_H1).c_str(), 0);
  if (_server->hasArg(PARAM_PUSH_HTTP_H2))
    myConfig.setHttpHeader(_server->arg(PARAM_PUSH_HTTP_H2).c_str(), 1);
  if (_server->hasArg(PARAM_PUSH_HTTP3))
    myConfig.setHttp3Url(_server->arg(PARAM_PUSH_HTTP3).c_str());
  if (_server->hasArg(PARAM_PUSH_INFLUXDB2))
    myConfig.setInfluxDb2PushUrl(_server->arg(PARAM_PUSH_INFLUXDB2).c_str());
  if (_server->hasArg(PARAM_PUSH_INFLUXDB2_ORG))
    myConfig.setInfluxDb2PushOrg(
        _server->arg(PARAM_PUSH_INFLUXDB2_ORG).c_str());
  if (_server->hasArg(PARAM_PUSH_INFLUXDB2_BUCKET))
    myConfig.setInfluxDb2PushBucket(
        _server->arg(PARAM_PUSH_INFLUXDB2_BUCKET).c_str());
  if (_server->hasArg(PARAM_PUSH_INFLUXDB2_AUTH))
    myConfig.setInfluxDb2PushToken(
        _server->arg(PARAM_PUSH_INFLUXDB2_AUTH).c_str());
  if (_server->hasArg(PARAM_PUSH_MQTT))
    myConfig.setMqttUrl(_server->arg(PARAM_PUSH_MQTT).c_str());
  if (_server->hasArg(PARAM_PUSH_MQTT_PORT))
    myConfig.setMqttPort(_server->arg(PARAM_PUSH_MQTT_PORT).c_str());
  if (_server->hasArg(PARAM_PUSH_MQTT_USER))
    myConfig.setMqttUser(_server->arg(PARAM_PUSH_MQTT_USER).c_str());
  if (_server->hasArg(PARAM_PUSH_MQTT_PASS))
    myConfig.setMqttPass(_server->arg(PARAM_PUSH_MQTT_PASS).c_str());
  myConfig.saveFile();
  String section("/config.htm#");
  section += _server->arg("section");
  _server->sendHeader("Location", section.c_str(), true);
  _server->send(302, "text/plain", "Push config updated");
  LOG_PERF_STOP("webserver-api-config-push");
}

//
// Get string with all received arguments. Used for debugging only.
//
String WebServerHandler::getRequestArguments() {
  String debug;

  for (int i = 0; i < _server->args(); i++) {
    if (!_server->argName(i).equals(
            "plain")) {  // this contains all the arguments, we dont need that.
      if (debug.length()) debug += ", ";

      debug += _server->argName(i);
      debug += "=";
      debug += _server->arg(i);
    }
  }
  return debug;
}

void WebServerHandler::webHandleConfigHardware() {
  LOG_PERF_START("webserver-api-config-hardware");
  String id = _server->arg(PARAM_ID);
  Log.notice(F("WEB : webServer callback for /api/config/hardware(post)." CR));

  if (!id.equalsIgnoreCase(myConfig.getID())) {
    Log.error(F("WEB : Wrong ID received %s, expected %s" CR), id.c_str(),
              myConfig.getID());
    _server->send(400, "text/plain", "Invalid ID.");
    LOG_PERF_STOP("webserver-api-config-hardware");
    return;
  }

#if LOG_LEVEL == 6
  Log.verbose(F("WEB : %s." CR), getRequestArguments().c_str());
#endif

  if (_server->hasArg(PARAM_VOLTAGE_FACTOR))
    myConfig.setVoltageFactor(_server->arg(PARAM_VOLTAGE_FACTOR).toFloat());
  if (_server->hasArg(PARAM_VOLTAGE_CONFIG))
    myConfig.setVoltageConfig(_server->arg(PARAM_VOLTAGE_CONFIG).toFloat());
  if (_server->hasArg(PARAM_TEMP_ADJ)) {
    if (myConfig.isTempC()) {
      myConfig.setTempSensorAdjC(_server->arg(PARAM_TEMP_ADJ));
    } else {
      // Data is delta so we add 32 in order to conver to C.
      myConfig.setTempSensorAdjF(_server->arg(PARAM_TEMP_ADJ), 32);
    }
  }

  myConfig.saveFile();
  _server->sendHeader("Location", "/config.htm#collapseHardware", true);
  _server->send(302, "text/plain", "Hardware config updated");
  LOG_PERF_STOP("webserver-api-config-hardware");
}

void WebServerHandler::webHandleConfigAdvancedWrite() {
  LOG_PERF_START("webserver-api-config-advanced");
  String id = _server->arg(PARAM_ID);
  Log.notice(F("WEB : webServer callback for /api/config/advaced(post)." CR));

  if (!id.equalsIgnoreCase(myConfig.getID())) {
    Log.error(F("WEB : Wrong ID received %s, expected %s" CR), id.c_str(),
              myConfig.getID());
    _server->send(400, "text/plain", "Invalid ID.");
    LOG_PERF_STOP("webserver-api-config-advanced");
    return;
  }

#if LOG_LEVEL == 6
  Log.verbose(F("WEB : %s." CR), getRequestArguments().c_str());
#endif

  if (_server->hasArg(PARAM_HW_WIFI_PORTAL_TIMEOUT))
    myAdvancedConfig.setWifiPortalTimeout(
        _server->arg(PARAM_HW_WIFI_PORTAL_TIMEOUT).toInt());
  if (_server->hasArg(PARAM_HW_WIFI_CONNECT_TIMEOUT))
    myAdvancedConfig.setWifiConnectTimeout(
        _server->arg(PARAM_HW_WIFI_CONNECT_TIMEOUT).toInt());
  if (_server->hasArg(PARAM_HW_PUSH_TIMEOUT))
    myAdvancedConfig.setPushTimeout(
        _server->arg(PARAM_HW_PUSH_TIMEOUT).toInt());
  if (_server->hasArg(PARAM_HW_TEMPSENSOR_RESOLUTION))
    myAdvancedConfig.setTempSensorResolution(
        _server->arg(PARAM_HW_TEMPSENSOR_RESOLUTION).toInt());

  myAdvancedConfig.saveFile();
  _server->sendHeader("Location", "/config.htm#collapseAdvanced", true);
  _server->send(302, "text/plain", "Advanced config updated");
  LOG_PERF_STOP("webserver-api-config-advanced");
}

void WebServerHandler::webHandleConfigAdvancedRead() {
  LOG_PERF_START("webserver-api-config-advanced");
  Log.notice(F("WEB : webServer callback for /api/config/advanced(get)." CR));

  DynamicJsonDocument doc(500);

  doc[PARAM_HW_WIFI_PORTAL_TIMEOUT] = myAdvancedConfig.getWifiPortalTimeout();
  doc[PARAM_HW_WIFI_CONNECT_TIMEOUT] = myAdvancedConfig.getWifiConnectTimeout();
  doc[PARAM_HW_PUSH_TIMEOUT] = myAdvancedConfig.getPushTimeout();
  doc[PARAM_HW_TEMPSENSOR_RESOLUTION] =
      myAdvancedConfig.getTempSensorResolution();

#if LOG_LEVEL == 6
  serializeJson(doc, Serial);
  Serial.print(CR);
#endif

  String out;
  out.reserve(500);
  serializeJson(doc, out);
  doc.clear();
  _server->send(200, "application/json", out.c_str());
  LOG_PERF_STOP("webserver-api-config-advanced");
}

void WebServerHandler::webHandlePageNotFound() {
  Log.error(F("WEB : URL not found %s received." CR), _server->uri().c_str());
  _server->send(404, "text/plain", F("URL not found"));
}

bool WebServerHandler::setupWebServer() {
  Log.notice(F("WEB : Configuring web server." CR));

  _server = new ESP8266WebServer();

  MDNS.begin(myConfig.getMDNS());
  MDNS.addService("http", "tcp", 80);

  // Show files in the filessytem at startup
  FSInfo fs;
  LittleFS.info(fs);
  Log.notice(F("WEB : File system Total=%d, Used=%d." CR), fs.totalBytes,
             fs.usedBytes);
  Dir dir = LittleFS.openDir("/");
  while (dir.next()) {
    Log.notice(F("WEB : File=%s, %d bytes" CR), dir.fileName().c_str(),
               dir.fileSize());
    if (!dir.fileSize()) {
      Log.notice(F("WEB : Empty file detected, removing file." CR));
      LittleFS.remove(dir.fileName().c_str());
    }
  }

  // Static content
  Log.notice(F("WEB : Setting up handlers for web server." CR));
  _server->on("/", std::bind(&WebServerHandler::webReturnIndexHtm, this));
  _server->on("/index.htm",
              std::bind(&WebServerHandler::webReturnIndexHtm, this));
  _server->on("/config.htm",
              std::bind(&WebServerHandler::webReturnConfigHtm, this));
  _server->on("/about.htm",
              std::bind(&WebServerHandler::webReturnAboutHtm, this));
  _server->on("/firmware.htm",
              std::bind(&WebServerHandler::webReturnFirmwareHtm, this));

  // Dynamic content
  _server->on("/api/config", HTTP_GET,
              std::bind(&WebServerHandler::webHandleConfig, this));
  _server->on("/api/status", HTTP_GET,
              std::bind(&WebServerHandler::webHandleStatus, this));
  _server->on("/api/clearwifi", HTTP_GET,
              std::bind(&WebServerHandler::webHandleClearWIFI, this));

  _server->on("/api/upload", HTTP_POST,
              std::bind(&WebServerHandler::webReturnOK, this),
              std::bind(&WebServerHandler::webHandleUploadFile,
                        this));  // File upload data
  _server->on("/api/config/device", HTTP_POST,
              std::bind(&WebServerHandler::webHandleConfigDevice,
                        this));  // Change device settings
  _server->on("/api/config/push", HTTP_POST,
              std::bind(&WebServerHandler::webHandleConfigPush,
                        this));  // Change push settings
  _server->on("/api/config/hardware", HTTP_POST,
              std::bind(&WebServerHandler::webHandleConfigHardware,
                        this));  // Change hardware settings
  _server->on("/api/config/advanced", HTTP_GET,
              std::bind(&WebServerHandler::webHandleConfigAdvancedRead,
                        this));  // Read advanced settings
  _server->on("/api/config/advanced", HTTP_POST,
              std::bind(&WebServerHandler::webHandleConfigAdvancedWrite,
                        this));  // Change advanced params

  _server->onNotFound(
      std::bind(&WebServerHandler::webHandlePageNotFound, this));
  _server->begin();
  Log.notice(F("WEB : Web server started." CR));
  return true;
}

void WebServerHandler::loop() {
  MDNS.update();
  _server->handleClient();
}

// EOF
