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
#ifndef SRC_TEMPCONFIG_HPP_
#define SRC_TEMPCONFIG_HPP_

#include <baseconfig.hpp>
#include <main.hpp>
#include <utils.hpp>

#define CFG_APPNAME "TempMon"
#define CFG_FILENAME "/tempmon.json"

#define PARAM_TOKEN "token"
#define PARAM_SLEEP_INTERVAL "sleep-interval"
#define PARAM_VOLTAGE_FACTOR "voltage-factor"
#define PARAM_VOLTAGE_CONFIG "voltage-config"
#define PARAM_TEMP_ADJ "temp-adjustment-value"
#define PARAM_TEMP_SENSOR_RESOLUTION "temp-sensor-resolution"

class TempConfig : public BaseConfig {
 private:
  bool _saveNeeded = false;

  // Device configuration
  float _voltageFactor = 1.59;
  float _voltageConfig = 4.15;
  float _tempSensorAdjC = 0;
  int _sleepInterval = 900;

  // Push target settings
  String _token = "";

  int _tempSensorResolution = 12;  // bits

 public:
  TempConfig(String baseMDNS, String fileName);

  void createJson(DynamicJsonDocument& doc, bool skipSecrets = true);
  void parseJson(DynamicJsonDocument& doc);

  const char* getToken() { return _token.c_str(); }
  void setToken(String s) {
    _token = s;
    _saveNeeded = true;
  }

  int getSleepInterval() { return _sleepInterval; }
  void setSleepInterval(int v) {
    _sleepInterval = v;
    _saveNeeded = true;
  }
  void setSleepInterval(String s) {
    _sleepInterval = s.toInt();
    _saveNeeded = true;
  }

  float getVoltageFactor() { return _voltageFactor; }
  void setVoltageFactor(float f) {
    _voltageFactor = f;
    _saveNeeded = true;
  }
  void setVoltageFactor(String s) {
    _voltageFactor = s.toFloat();
    _saveNeeded = true;
  }

  float getVoltageConfig() { return _voltageConfig; }
  void setVoltageConfig(float f) {
    _voltageConfig = f;
    _saveNeeded = true;
  }
  void setVoltageConfig(String s) {
    _voltageConfig = s.toFloat();
    _saveNeeded = true;
  }

  float getTempSensorAdjC() { return _tempSensorAdjC; }
  void setTempSensorAdjC(float f) {
    _tempSensorAdjC = f;
    _saveNeeded = true;
  }
  void setTempSensorAdjC(String s, float adjustC = 0) {
    _tempSensorAdjC = s.toFloat() + adjustC;
    _saveNeeded = true;
  }
  void setTempSensorAdjF(String s, float adjustF = 0) {
    _tempSensorAdjC = convertFtoC(s.toFloat() + adjustF);
    _saveNeeded = true;
  }

  int getTempSensorResolution() { return _tempSensorResolution; }
  void setTempSensorResolution(int t) {
    if (t >= 9 && t <= 12) _tempSensorResolution = t;
  }
};

#endif  // SRC_TEMPCONFIG_HPP_

// EOF
