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
#include <LittleFS.h>

#include <battery.hpp>
#include <espframework.hpp>
#include <tempsensor.hpp>
#include <tempweb.hpp>

#define PARAM_TEMP_C "temp-c"
#define PARAM_TEMP_F "temp-f"
#define PARAM_BATTERY "battery"
#define PARAM_RSSI "rssi"
#define PARAM_APP_VER "app-ver"
#define PARAM_APP_BUILD "app-build"

TempWebHandler::TempWebHandler(TempConfig* config) : BaseWebHandler(config) {
  _config = config;
}

void TempWebHandler::setupWebHandlers() {
  Log.notice(F("WEB : Setting up web handlers." CR));
  BaseWebHandler::setupWebHandlers();

  _server->on("/api/status", HTTP_GET,
              std::bind(&TempWebHandler::webHandleStatus, this));
}

void TempWebHandler::webHandleStatus() {
  Log.notice(F("WEB : webServer callback for /api/status(get)." CR));

  DynamicJsonDocument doc(500);
  TempSensor ts;
  BatteryVoltage bv;

  double tempC = ts.getInstance().getTempC();

  doc[PARAM_ID] = _config->getID();
  doc[PARAM_TEMP_C] = reduceFloatPrecision(tempC, 1);
  doc[PARAM_TEMP_F] = reduceFloatPrecision(convertCtoF(tempC), 1);
  doc[PARAM_BATTERY] = reduceFloatPrecision(bv.getInstance().getVoltage(), 2);
  doc[PARAM_TEMP_FORMAT] = String(_config->getTempFormat());
  doc[PARAM_RSSI] = WiFi.RSSI();
  doc[PARAM_SLEEP_INTERVAL] = _config->getSleepInterval();
  doc[PARAM_TOKEN] = _config->getToken();
  doc[PARAM_APP_VER] = CFG_APPVER;
  doc[PARAM_APP_BUILD] = CFG_GITREV;
  doc[PARAM_MDNS] = _config->getMDNS();
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
}

// EOF
