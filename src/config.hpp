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
#ifndef SRC_CONFIG_HPP_
#define SRC_CONFIG_HPP_

#include <helper.hpp>
#include <resources.hpp>

#define CFG_APPNAME "TemperatureMon"      // Name of firmware
#define CFG_FILENAME "/tempmon.json"      // Name of config file
#define CFG_HW_FILENAME "/hardware.json"  // Name of config file for hw

class AdvancedConfig {
 private:
  int _wifiPortalTimeout = 120;    // seconds
  int _wifiConnectTimeout = 20;    // seconds
  int _tempSensorResolution = 12;  // bits
  int _gyroReadCount = 50;
  int _pushTimeout = 10;  // seconds

 public:
  int getWifiPortalTimeout() { return _wifiPortalTimeout; }
  void setWifiPortalTimeout(int t) { _wifiPortalTimeout = t; }

  int getWifiConnectTimeout() { return _wifiConnectTimeout; }
  void setWifiConnectTimeout(int t) { _wifiConnectTimeout = t; }

  int getTempSensorResolution() { return _tempSensorResolution; }
  void setTempSensorResolution(int t) {
    if (t >= 9 && t <= 12) _tempSensorResolution = t;
  }

  int getPushTimeout() { return _pushTimeout; }
  void setPushTimeout(int t) { _pushTimeout = t; }

  bool saveFile();
  bool loadFile();
};

class Config {
 private:
  bool _saveNeeded = false;
  int _configVersion = 2;

  // Device configuration
  String _id = "";
  String _mDNS = "";
  char _tempFormat = 'C';
  float _voltageFactor = 1.59;
  float _voltageConfig = 4.15;
  float _tempSensorAdjC = 0;
  int _sleepInterval = 900;

  // Wifi Config
  String _wifiSSID[2] = {"", ""};
  String _wifiPASS[2] = {"", ""};

  // Push target settings
  String _token = "";

  String _httpUrl = "";
  String _httpHeader[2] = {"Content-Type: application/json", ""};
  String _http3Url = "";

  String _influxDb2Url = "";
  String _influxDb2Org = "";
  String _influxDb2Bucket = "";
  String _influxDb2Token = "";

  String _mqttUrl = "";
  int _mqttPort = 1883;
  String _mqttUser = "";
  String _mqttPass = "";

  void formatFileSystem();

 public:
  Config();
  const char* getID() { return _id.c_str(); }

  const char* getMDNS() { return _mDNS.c_str(); }
  void setMDNS(String s) {
    _mDNS = s;
    _saveNeeded = true;
  }

  int getConfigVersion() { return _configVersion; }

  const char* getWifiSSID(int idx) { return _wifiSSID[idx].c_str(); }
  void setWifiSSID(String s, int idx) {
    _wifiSSID[idx] = s;
    _saveNeeded = true;
  }
  const char* getWifiPass(int idx) { return _wifiPASS[idx].c_str(); }
  void setWifiPass(String s, int idx) {
    _wifiPASS[idx] = s;
    _saveNeeded = true;
  }
  bool dualWifiConfigured() {
    return _wifiSSID[0].length() > 0 && _wifiSSID[1].length() > 0 ? true
                                                                  : false;
  }
  void swapPrimaryWifi() {
    String s = _wifiSSID[0];
    _wifiSSID[0] = _wifiSSID[1];
    _wifiSSID[1] = s;

    String p = _wifiPASS[0];
    _wifiPASS[0] = _wifiPASS[1];
    _wifiPASS[1] = p;

    _saveNeeded = true;
  }

  // Token parameter
  const char* getToken() { return _token.c_str(); }
  void setToken(String s) {
    _token = s;
    _saveNeeded = true;
  }

  // Standard HTTP
  const char* getHttpUrl() { return _httpUrl.c_str(); }
  void setHttpUrl(String s) {
    _httpUrl = s;
    _saveNeeded = true;
  }
  const char* getHttpHeader(int idx) { return _httpHeader[idx].c_str(); }
  void setHttpHeader(String s, int idx) {
    _httpHeader[idx] = s;
    _saveNeeded = true;
  }
  bool isHttpActive() { return _httpUrl.length() ? true : false; }
  bool isHttpSSL() { return _httpUrl.startsWith("https://"); }

  const char* getHttp3Url() { return _http3Url.c_str(); }
  void setHttp3Url(String s) {
    _http3Url = s;
    _saveNeeded = true;
  }
  bool isHttp3Active() { return _http3Url.length() ? true : false; }
  bool isHttp3SSL() { return _http3Url.startsWith("https://"); }

  // InfluxDB2
  const char* getInfluxDb2PushUrl() { return _influxDb2Url.c_str(); }
  void setInfluxDb2PushUrl(String s) {
    _influxDb2Url = s;
    _saveNeeded = true;
  }
  bool isInfluxDb2Active() { return _influxDb2Url.length() ? true : false; }
  bool isInfluxSSL() { return _influxDb2Url.startsWith("https://"); }
  const char* getInfluxDb2PushOrg() { return _influxDb2Org.c_str(); }
  void setInfluxDb2PushOrg(String s) {
    _influxDb2Org = s;
    _saveNeeded = true;
  }
  const char* getInfluxDb2PushBucket() { return _influxDb2Bucket.c_str(); }
  void setInfluxDb2PushBucket(String s) {
    _influxDb2Bucket = s;
    _saveNeeded = true;
  }
  const char* getInfluxDb2PushToken() { return _influxDb2Token.c_str(); }
  void setInfluxDb2PushToken(String s) {
    _influxDb2Token = s;
    _saveNeeded = true;
  }

  // MQTT
  const char* getMqttUrl() { return _mqttUrl.c_str(); }
  void setMqttUrl(String s) {
    _mqttUrl = s;
    _saveNeeded = true;
  }
  bool isMqttActive() { return _mqttUrl.length() ? true : false; }
  bool isMqttSSL() { return _mqttPort > 8000 ? true : false; }

  int getMqttPort() { return _mqttPort; }
  void setMqttPort(String s) {
    _mqttPort = s.toInt();
    _saveNeeded = true;
  }
  void setMqttPort(int i) {
    _mqttPort = i;
    _saveNeeded = true;
  }
  const char* getMqttUser() { return _mqttUser.c_str(); }
  void setMqttUser(String s) {
    _mqttUser = s;
    _saveNeeded = true;
  }
  const char* getMqttPass() { return _mqttPass.c_str(); }
  void setMqttPass(String s) {
    _mqttPass = s;
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

  char getTempFormat() { return _tempFormat; }
  void setTempFormat(char c) {
    if (c == 'C' || c == 'F') {
      _tempFormat = c;
      _saveNeeded = true;
    }
  }
  bool isTempC() { return _tempFormat == 'C'; }
  bool isTempF() { return _tempFormat == 'F'; }

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

  // IO functions
  void createJson(DynamicJsonDocument& doc);
  bool saveFile();
  bool loadFile();
  void checkFileSystem();
  bool isSaveNeeded() { return _saveNeeded; }
  void setSaveNeeded() { _saveNeeded = true; }
};

extern Config myConfig;
extern AdvancedConfig myAdvancedConfig;

#endif  // SRC_CONFIG_HPP_

// EOF
