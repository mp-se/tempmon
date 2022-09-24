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
#include <main.hpp>
#include <wifi.hpp>

Config myConfig;
AdvancedConfig myAdvancedConfig;

Config::Config() {
  // Assiging default values
  char buf[30];
  snprintf(&buf[0], sizeof(buf), "%06x", (unsigned int)ESP.getChipId());
  _id = String(&buf[0]);
  snprintf(&buf[0], sizeof(buf), "" WIFI_MDNS "%s", getID());
  _mDNS = String(&buf[0]);

#if LOG_LEVEL == 6
  Log.verbose(F("CFG : Created config for %s (%s)." CR), _id.c_str(),
              _mDNS.c_str());
#endif
}

void Config::createJson(DynamicJsonDocument& doc) {
  doc[PARAM_MDNS] = getMDNS();
  doc[PARAM_ID] = getID();
  doc[PARAM_SSID] = getWifiSSID(0);
  doc[PARAM_PASS] = getWifiPass(0);
  doc[PARAM_SSID2] = getWifiSSID(1);
  doc[PARAM_PASS2] = getWifiPass(1);
  doc[PARAM_TEMPFORMAT] = String(getTempFormat());
  doc[PARAM_TOKEN] = getToken();
  doc[PARAM_PUSH_HTTP] = getHttpUrl();
  doc[PARAM_PUSH_HTTP_H1] = getHttpHeader(0);
  doc[PARAM_PUSH_HTTP_H2] = getHttpHeader(1);
  doc[PARAM_PUSH_HTTP3] = getHttp3Url();
  doc[PARAM_PUSH_INFLUXDB2] = getInfluxDb2PushUrl();
  doc[PARAM_PUSH_INFLUXDB2_ORG] = getInfluxDb2PushOrg();
  doc[PARAM_PUSH_INFLUXDB2_BUCKET] = getInfluxDb2PushBucket();
  doc[PARAM_PUSH_INFLUXDB2_AUTH] = getInfluxDb2PushToken();
  doc[PARAM_PUSH_MQTT] = getMqttUrl();
  doc[PARAM_PUSH_MQTT_PORT] = getMqttPort();
  doc[PARAM_PUSH_MQTT_USER] = getMqttUser();
  doc[PARAM_PUSH_MQTT_PASS] = getMqttPass();
  doc[PARAM_SLEEP_INTERVAL] = getSleepInterval();
  doc[PARAM_VOLTAGE_FACTOR] = getVoltageFactor();
  doc[PARAM_VOLTAGE_CONFIG] = getVoltageConfig();
  doc[PARAM_TEMP_ADJ] = getTempSensorAdjC();
}

bool Config::saveFile() {
  if (!_saveNeeded) {
#if LOG_LEVEL == 6
    Log.verbose(F("CFG : Skipping save, not needed." CR));
#endif
    return true;
  }

#if LOG_LEVEL == 6
  Log.verbose(F("CFG : Saving configuration to file." CR));
#endif

  File configFile = LittleFS.open(CFG_FILENAME, "w");

  if (!configFile) {
    Log.error(F("CFG : Failed to save configuration." CR));
    return false;
  }

  DynamicJsonDocument doc(3000);
  createJson(doc);

#if LOG_LEVEL == 6 && !defined(DISABLE_LOGGING)
  serializeJson(doc, Serial);
  Serial.print(CR);
#endif

  serializeJson(doc, configFile);
  configFile.flush();
  configFile.close();

  _saveNeeded = false;
  Log.notice(F("CFG : Configuration saved to " CFG_FILENAME "." CR));
  return true;
}

bool Config::loadFile() {
#if LOG_LEVEL == 6
  Log.verbose(F("CFG : Loading configuration from file." CR));
#endif

  if (!LittleFS.exists(CFG_FILENAME)) {
    Log.error(F("CFG : Configuration file does not exist." CR));
    return false;
  }

  File configFile = LittleFS.open(CFG_FILENAME, "r");

  if (!configFile) {
    Log.error(F("CFG : Failed to load configuration." CR));
    return false;
  }

  Log.notice(F("CFG : Size of configuration file=%d bytes." CR),
             configFile.size());

  DynamicJsonDocument doc(3000);
  DeserializationError err = deserializeJson(doc, configFile);
#if LOG_LEVEL == 6
  serializeJson(doc, Serial);
  Serial.print(CR);
#endif
  configFile.close();

  if (err) {
    Log.error(F("CFG : Failed to parse configuration (json)" CR));
    return false;
  }

#if LOG_LEVEL == 6
  Log.verbose(F("CFG : Parsed configuration file." CR));
#endif
  if (!doc[PARAM_MDNS].isNull()) setMDNS(doc[PARAM_MDNS]);
  if (!doc[PARAM_SSID].isNull()) setWifiSSID(doc[PARAM_SSID], 0);
  if (!doc[PARAM_PASS].isNull()) setWifiPass(doc[PARAM_PASS], 0);
  if (!doc[PARAM_SSID2].isNull()) setWifiSSID(doc[PARAM_SSID2], 1);
  if (!doc[PARAM_PASS2].isNull()) setWifiPass(doc[PARAM_PASS2], 1);

  if (!doc[PARAM_TEMPFORMAT].isNull()) {
    String s = doc[PARAM_TEMPFORMAT];
    setTempFormat(s.charAt(0));
  }

  if (!doc[PARAM_TOKEN].isNull()) setToken(doc[PARAM_TOKEN]);
  if (!doc[PARAM_PUSH_HTTP].isNull()) setHttpUrl(doc[PARAM_PUSH_HTTP]);
  if (!doc[PARAM_PUSH_HTTP_H1].isNull())
    setHttpHeader(doc[PARAM_PUSH_HTTP_H1], 0);
  if (!doc[PARAM_PUSH_HTTP_H2].isNull())
    setHttpHeader(doc[PARAM_PUSH_HTTP_H2], 1);
  if (!doc[PARAM_PUSH_HTTP3].isNull()) setHttp3Url(doc[PARAM_PUSH_HTTP3]);

  if (!doc[PARAM_PUSH_INFLUXDB2].isNull())
    setInfluxDb2PushUrl(doc[PARAM_PUSH_INFLUXDB2]);
  if (!doc[PARAM_PUSH_INFLUXDB2_ORG].isNull())
    setInfluxDb2PushOrg(doc[PARAM_PUSH_INFLUXDB2_ORG]);
  if (!doc[PARAM_PUSH_INFLUXDB2_BUCKET].isNull())
    setInfluxDb2PushBucket(doc[PARAM_PUSH_INFLUXDB2_BUCKET]);
  if (!doc[PARAM_PUSH_INFLUXDB2_AUTH].isNull())
    setInfluxDb2PushToken(doc[PARAM_PUSH_INFLUXDB2_AUTH]);

  if (!doc[PARAM_PUSH_MQTT].isNull()) setMqttUrl(doc[PARAM_PUSH_MQTT]);
  if (!doc[PARAM_PUSH_MQTT_PORT].isNull())
    setMqttPort(doc[PARAM_PUSH_MQTT_PORT].as<int>());
  if (!doc[PARAM_PUSH_MQTT_USER].isNull())
    setMqttUser(doc[PARAM_PUSH_MQTT_USER]);
  if (!doc[PARAM_PUSH_MQTT_PASS].isNull())
    setMqttPass(doc[PARAM_PUSH_MQTT_PASS]);

  if (!doc[PARAM_SLEEP_INTERVAL].isNull())
    setSleepInterval(doc[PARAM_SLEEP_INTERVAL].as<int>());
  if (!doc[PARAM_VOLTAGE_FACTOR].isNull())
    setVoltageFactor(doc[PARAM_VOLTAGE_FACTOR].as<float>());
  if (!doc[PARAM_VOLTAGE_CONFIG].isNull())
    setVoltageConfig(doc[PARAM_VOLTAGE_CONFIG].as<float>());
  if (!doc[PARAM_TEMP_ADJ].isNull())
    setTempSensorAdjC(doc[PARAM_TEMP_ADJ].as<float>());

  _saveNeeded = false;
  Log.notice(F("CFG : Configuration file " CFG_FILENAME " loaded." CR));
  return true;
}

void Config::formatFileSystem() {
  Log.notice(F("CFG : Formating filesystem." CR));
  LittleFS.format();
}

void Config::checkFileSystem() {
#if LOG_LEVEL == 6 && !defined(DISABLE_LOGGING)
  Log.verbose(F("CFG : Checking if filesystem is valid." CR));
#endif

  if (LittleFS.begin()) {
    Log.notice(F("CFG : Filesystem mounted." CR));
  } else {
    Log.error(F("CFG : Unable to mount file system, formatting..." CR));
    LittleFS.format();
  }
}

bool AdvancedConfig::saveFile() {
#if LOG_LEVEL == 6 && !defined(DISABLE_LOGGING)
  Log.verbose(F("CFG : Saving hardware configuration to file." CR));
#endif

  File configFile = LittleFS.open(CFG_HW_FILENAME, "w");

  if (!configFile) {
    Log.error(F("CFG : Failed to write hardware configuration " CR));
    return false;
  }

  DynamicJsonDocument doc(512);

  doc[PARAM_HW_WIFI_PORTAL_TIMEOUT] = this->getWifiPortalTimeout();
  doc[PARAM_HW_WIFI_CONNECT_TIMEOUT] = this->getWifiConnectTimeout();
  doc[PARAM_HW_TEMPSENSOR_RESOLUTION] = this->getTempSensorResolution();

#if LOG_LEVEL == 6
  serializeJson(doc, Serial);
  Serial.print(CR);
#endif

  serializeJson(doc, configFile);
  configFile.flush();
  configFile.close();

  Log.notice(F("CFG : Configuration saved to " CFG_HW_FILENAME "." CR));
  return true;
}

bool AdvancedConfig::loadFile() {
#if LOG_LEVEL == 6
  Log.verbose(F("CFG : Loading hardware configuration from file." CR));
#endif

  if (!LittleFS.exists(CFG_HW_FILENAME)) {
    Log.warning(
        F("CFG : Configuration file does not exist " CFG_HW_FILENAME "." CR));
    return false;
  }

  File configFile = LittleFS.open(CFG_HW_FILENAME, "r");

  if (!configFile) {
    Log.error(F("CFG : Failed to read hardware configuration" CR));
    return false;
  }

  Log.notice(F("CFG : Size of configuration file=%d bytes." CR),
             configFile.size());

  DynamicJsonDocument doc(512);
  DeserializationError err = deserializeJson(doc, configFile);
#if LOG_LEVEL == 6
  serializeJson(doc, Serial);
  Serial.print(CR);
#endif
  configFile.close();

  if (err) {
    Log.error(F("CFG : Failed to parse hardware configuration (json)" CR));
    return false;
  }

#if LOG_LEVEL == 6
  Log.verbose(F("CFG : Parsed hardware configuration file." CR));
#endif

  if (!doc[PARAM_HW_WIFI_PORTAL_TIMEOUT].isNull())
    this->setWifiPortalTimeout(doc[PARAM_HW_WIFI_PORTAL_TIMEOUT].as<int>());
  if (!doc[PARAM_HW_WIFI_CONNECT_TIMEOUT].isNull())
    this->setWifiConnectTimeout(doc[PARAM_HW_WIFI_CONNECT_TIMEOUT].as<int>());
  if (!doc[PARAM_HW_PUSH_TIMEOUT].isNull())
    this->setPushTimeout(doc[PARAM_HW_PUSH_TIMEOUT].as<int>());
  if (!doc[PARAM_HW_TEMPSENSOR_RESOLUTION].isNull())
    this->setTempSensorResolution(
        doc[PARAM_HW_TEMPSENSOR_RESOLUTION].as<int>());

  Log.notice(F("CFG : Configuration file " CFG_HW_FILENAME " loaded." CR));
  return true;
}

// EOF
