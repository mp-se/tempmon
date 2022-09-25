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
#ifndef SRC_TEMPLATING_HPP_
#define SRC_TEMPLATING_HPP_

#include <Arduino.h>

#include <tempconfig.hpp>
#include <utils.hpp>

#define TPL_MDNS "${mdns}"
#define TPL_ID "${id}"
#define TPL_TOKEN "${token}"
#define TPL_SLEEP_INTERVAL "${sleep-interval}"
#define TPL_TEMP "${temp}"
#define TPL_TEMP_C "${temp-c}"
#define TPL_TEMP_F "${temp-f}"
#define TPL_TEMP_UNITS "${temp-unit}"
#define TPL_BATTERY "${battery}"
#define TPL_RSSI "${rssi}"
#define TPL_RUN_TIME "${run-time}"
#define TPL_APP_VER "${app-ver}"
#define TPL_APP_BUILD "${app-build}"

extern const char iSpindleFormat[] PROGMEM;
extern const char iHttpGetFormat[] PROGMEM;
extern const char influxDbFormat[] PROGMEM;
extern const char mqttFormat[] PROGMEM;

// Classes
class TemplatingEngine {
 private:
  struct KeyVal {
    String key;
    String val;
  };

  KeyVal _items[13] = {
      {TPL_MDNS, ""},       {TPL_ID, ""},      {TPL_SLEEP_INTERVAL, ""},
      {TPL_TEMP, ""},       {TPL_TEMP_C, ""},  {TPL_TEMP_F, ""},
      {TPL_TEMP_UNITS, ""}, {TPL_BATTERY, ""}, {TPL_RSSI, ""},
      {TPL_RUN_TIME, ""},   {TPL_TOKEN, ""},   {TPL_APP_VER, ""},
      {TPL_APP_BUILD, ""}};

  char _buffer[20] = "";
  String _baseTemplate;
  char *_output = 0;

  void setVal(String key, float val, int dec = 2) {
    String s = convertFloatToString(val, &_buffer[0], dec);
    s.trim();
    setVal(key, s);
  }
  void setVal(String key, int val) { setVal(key, String(val)); }
  void setVal(String key, char val) { setVal(key, String(val)); }
  void setVal(String key, String val) {
    int max = sizeof(_items) / sizeof(KeyVal);
    for (int i = 0; i < max; i++) {
      if (_items[i].key.equals(key)) {
        _items[i].val = val;
        return;
      }
    }

    Log.warning(F("TPL : Key not found %s." CR), key.c_str());
  }

  void transform() {
    const char *format = _baseTemplate.c_str();
    int len = _baseTemplate.length();
    int size = len;

    // Lets check how much memory will be needed to transform the template
    for (int j = 0; j < len - 2; j++) {
      if (*(format + j) == '$' && *(format + j + 1) == '{') {
        // Start of format tag found
        int max = sizeof(_items) / sizeof(KeyVal);
        for (int i = 0; i < max; i++) {
          if (strncmp(format + j, _items[i].key.c_str(),
                      _items[i].key.length()) == 0) {
            // Found key
            size = size - _items[i].key.length() + _items[i].val.length();
          }
        }
      }
    }

    freeMemory();  // In case this is reused
    _output = static_cast<char *>(malloc(size + 20));

    if (!_output) {
      Log.error(F("TPL : Unable to allocate memory for transforming template, "
                  "needed %d." CR),
                size);
      return;
    }

    memset(_output, 0, size + 20);

    // Lets do the transformation
    int k = 0;
    for (int j = 0; j < len - 2; j++) {
      if (*(format + j) == '$' && *(format + j + 1) == '{') {
        // Start of format tag found
        int max = sizeof(_items) / sizeof(KeyVal);
        for (int i = 0; i < max; i++) {
          if (strncmp(format + j, _items[i].key.c_str(),
                      _items[i].key.length()) == 0) {
            // Found key
            strncat(_output, format + k, j - k);
            strncat(_output, _items[i].val.c_str(), _items[i].val.length());
            k = j + _items[i].key.length();
          }
        }
      }
    }
    strncat(_output, format + k, size - k);
    Log.notice(F("TPL : Transformed template %d chars to %d chars" CR),
               strlen(format), strlen(_output));

#if LOG_LEVEL == 6
    printHeap("TPL ");
    Log.verboseln(format);
    Log.verboseln(_output);
#endif
  }

  void dumpAll() {
    int max = sizeof(_items) / sizeof(KeyVal);
    for (int i = 0; i < max; i++) {
      Serial.print("Key=\'");
      Serial.print(_items[i].key.c_str());
      Serial.print("\', Val=\'");
      Serial.print(_items[i].val.c_str());
      Serial.println("\'");
    }
  }

  TempConfig *_config = 0;

 public:
  enum Templates {
    TEMPLATE_HTTP1 = 0,
    TEMPLATE_HTTP3 = 2,
    TEMPLATE_INFLUX = 3,
    TEMPLATE_MQTT = 4
  };

  explicit TemplatingEngine(TempConfig *config) { _config = config; }
  ~TemplatingEngine() { freeMemory(); }

  void freeMemory() {
    if (_output) free(_output);

    _output = 0;
    _baseTemplate.clear();
  }
  void initialize(float tempC, float battery, float runTime);
  const char *create(TemplatingEngine::Templates idx);
};

#endif  // SRC_TEMPLATING_HPP_

// EOF
