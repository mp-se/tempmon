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
#include <ESP8266WiFi.h>

#include <tempconfig.hpp>
#include <templating.hpp>

const char iSpindleFormat[] PROGMEM =
    "{"
    "\"name\" : \"${mdns}\", "
    "\"ID\": \"${id}\", "
    "\"token\" : \"${token}\", "
    "\"interval\": ${sleep-interval}, "
    "\"temperature\": ${temp}, "
    "\"temp_units\": \"${temp-unit}\", "
    "\"gravity\": 0 "
    "\"angle\": 0, "
    "\"battery\": ${battery}, "
    "\"RSSI\": ${rssi}, "
    "\"run-time\": ${run-time} "
    "}";

const char iHttpGetFormat[] PROGMEM =
    "?name=${mdns}"
    "&id=${id}"
    "&token=${token2}"
    "&interval=${sleep-interval}"
    "&temperature=${temp}"
    "&temp-units=${temp-unit}"
    "&battery=${battery}"
    "&rssi=${rssi}"
    "&run-time=${run-time}";

const char influxDbFormat[] PROGMEM =
    "measurement,host=${mdns},device=${id},temp-format=${temp-unit} "
    "temp=${temp},battery=${battery},"
    "rssi=${rssi}\n";

const char mqttFormat[] PROGMEM =
    "tempmon/${mdns}/temperature:${temp}|"
    "tempmon/${mdns}/temp_units:${temp-unit}|"
    "tempmon/${mdns}/battery:${battery}|"
    "tempmon/${mdns}/interval:${sleep-interval}|"
    "tempmon/${mdns}/RSSI:${rssi}|";

//
// Initialize the variables
//
void TemplatingEngine::initialize(float tempC, float battery, float runTime) {
  // Names
  setVal(TPL_MDNS, _config->getMDNS());
  setVal(TPL_ID, _config->getID());
  setVal(TPL_TOKEN, _config->getToken());

  // Temperature
  if (_config->isTempFormatC()) {
    setVal(TPL_TEMP, tempC, 1);
  } else {
    setVal(TPL_TEMP, convertCtoF(tempC), 1);
  }

  setVal(TPL_TEMP_C, tempC, 1);
  setVal(TPL_TEMP_F, convertCtoF(tempC), 1);
  setVal(TPL_TEMP_UNITS, _config->getTempFormat());

  // Battery & Timer
  setVal(TPL_BATTERY, battery);
  setVal(TPL_SLEEP_INTERVAL, _config->getSleepInterval());

  // Performance metrics
  setVal(TPL_RUN_TIME, runTime, 1);
  setVal(TPL_RSSI, WiFi.RSSI());

  setVal(TPL_APP_VER, CFG_APPVER);
  setVal(TPL_APP_BUILD, CFG_GITREV);

#if LOG_LEVEL == 6
  // dumpAll();
#endif
}

//
// Create the data using defined template.
//
const char* TemplatingEngine::create(TemplatingEngine::Templates idx) {
  String fname;
  _baseTemplate.reserve(600);

  // Load templates from memory
  switch (idx) {
    case TEMPLATE_HTTP1:
      _baseTemplate = String(iSpindleFormat);
      break;
    case TEMPLATE_HTTP3:
      _baseTemplate = String(iHttpGetFormat);
      break;
    case TEMPLATE_INFLUX:
      _baseTemplate = String(influxDbFormat);
      break;
    case TEMPLATE_MQTT:
      _baseTemplate = String(mqttFormat);
      break;
  }

#if LOG_LEVEL == 6
  Log.verbose(F("TPL : Base '%s'." CR), _baseTemplate.c_str());
#endif

  // Insert data into template.
  transform();
  _baseTemplate.clear();

#if LOG_LEVEL == 6
  // Log.verbose(F("TPL : Transformed '%s'." CR), baseTemplate.c_str());
#endif

  if (_output) return _output;

  return "";
}

// EOF
