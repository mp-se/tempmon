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
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>

#include <config.hpp>
#include <helper.hpp>
#include <main.hpp>
#include <tempsensor.hpp>
#include <wifi.hpp>

SerialDebug mySerial;
BatteryVoltage myBatteryVoltage;

// tcp cleanup, to avoid memory crash.
struct tcp_pcb;
extern struct tcp_pcb* tcp_tw_pcbs;
extern "C" void tcp_abort(struct tcp_pcb* pcb);
void tcp_cleanup() {
  while (tcp_tw_pcbs) tcp_abort(tcp_tw_pcbs);
}

void checkResetReason() {
  rst_info* _rinfo;
  _rinfo = ESP.getResetInfoPtr();

  Log.notice(F("HELP: Last reset cause %d" CR), _rinfo->exccause);

  if (_rinfo->exccause > 0) {
    char s[120];
    snprintf(&s[0], sizeof(s),
             "HELP: Exception (%d) reason=%d epc1=0x%08x epc2=0x%08x "
             "epc3=0x%08x execvaddr=0x%08x depc=0x%08x\n",
             _rinfo->exccause, _rinfo->reason, _rinfo->epc1, _rinfo->epc2,
             _rinfo->epc3, _rinfo->excvaddr, _rinfo->depc);
    Log.error(&s[0]);
  }
}

float convertCtoF(float c) { return (c * 1.8) + 32.0; }

float convertFtoC(float f) { return (f - 32.0) / 1.8; }

void printHeap(String prefix) {
  Log.notice(
      F("%s: Free-heap %d kb, Heap-rag %d %%, Max-block %d kb Stack=%d b." CR),
      prefix.c_str(), ESP.getFreeHeap() / 1024, ESP.getHeapFragmentation(),
      ESP.getMaxFreeBlockSize() / 1024, ESP.getFreeContStack());
  // Log.notice(F("%s: Heap %d kb, HeapFrag %d %%, FreeSketch %d kb." CR),
  //            prefix.c_str(), ESP.getFreeHeap() / 1024,
  //            ESP.getHeapFragmentation(), ESP.getFreeSketchSpace() / 1024);
}

void deepSleep(int t) {
#if LOG_LEVEL == 6 && !defined(HELPER_DISABLE_LOGGING)
  Log.verbose(F("HELP: Entering sleep mode for %ds." CR), t);
#endif
  uint32_t wake = t * 1000000;
  ESP.deepSleep(wake);
}

void printBuildOptions() {
  Log.notice(F("Build options: %s (%s) LOGLEVEL %d "
#ifdef COLLECT_PERFDATA
               "PERFDATA "
#endif
               CR),
             CFG_APPVER, CFG_GITREV, LOG_LEVEL);
}

SerialDebug::SerialDebug(const uint32_t serialSpeed) {
  // Start serial with auto-detected rate (default to defined BAUD)
  Serial.flush();
  Serial.begin(serialSpeed);

  getLog()->begin(LOG_LEVEL, &Serial, true);
  getLog()->setPrefix(printTimestamp);
  getLog()->notice(F("SDBG: Serial logging started at %u." CR), serialSpeed);
}

void printTimestamp(Print* _logOutput, int _logLevel) {
  char c[12];
  snprintf(c, sizeof(c), "%10lu ", millis());
  _logOutput->print(c);
}

void BatteryVoltage::read() {
  // The analog pin can only handle 3.3V maximum voltage so we need to reduce
  // the voltage (from max 5V)
  float factor = myConfig.getVoltageFactor();  // Default value is 1.63
  int v = analogRead(PIN_A0);

  // An ESP8266 has a ADC range of 0-1023 and a maximum voltage of 3.3V
  // An ESP32 has an ADC range of 0-4095 and a maximum voltage of 3.3V

  _batteryLevel = ((3.3 / 1023) * v) * factor;
#if LOG_LEVEL == 6
  Log.verbose(
      F("BATT: Reading voltage level. Factor=%F Value=%d, Voltage=%F." CR),
      factor, v, _batteryLevel);
#endif
}

#if defined(COLLECT_PERFDATA)

PerfLogging myPerfLogging;

void PerfLogging::clear() {
  // Clear the measurements
  if (first == 0) return;

  PerfEntry* pe = first;

  do {
    pe->max = 0;
    pe->start = 0;
    pe->end = 0;
    pe->mA = 0;
    pe->V = 0;
    pe = pe->next;
  } while (pe != 0);
}

void PerfLogging::start(const char* key) {
  PerfEntry* pe = add(key);
  pe->start = millis();
}

void PerfLogging::stop(const char* key) {
  PerfEntry* pe = find(key);

  if (pe != 0) {
    pe->end = millis();

    uint32_t t = pe->end - pe->start;

    if (t > pe->max) pe->max = t;
  }
}

void PerfLogging::print() {
  PerfEntry* pe = first;

  while (pe != 0) {
    Log.notice(F("PERF: %s %ums" CR), pe->key, pe->max);
    pe = pe->next;
  }
}

void PerfLogging::pushInflux() {
  if (!myConfig.isInfluxDb2Active()) return;

  if (myConfig.isInfluxSSL()) {
    Log.warning(
        F("PERF: InfluxDB2 with SSL is not supported when pushing performance "
          "data, skipping" CR));
    return;
  }

  WiFiClient wifi;
  HTTPClient http;
  String serverPath =
      String(myConfig.getInfluxDb2PushUrl()) +
      "/api/v2/write?org=" + String(myConfig.getInfluxDb2PushOrg()) +
      "&bucket=" + String(myConfig.getInfluxDb2PushBucket());

  http.begin(wifi, serverPath);

  // Create body for influxdb2, format used
  // key,host=mdns value=0.0
  String body;
  body.reserve(500);

  // Create the payload with performance data.
  // ------------------------------------------------------------------------------------------
  PerfEntry* pe = first;
  char buf[150];
  snprintf(&buf[0], sizeof(buf), "perf,host=%s,device=%s ", myConfig.getMDNS(),
           myConfig.getID());
  body += &buf[0];

  while (pe != 0) {
    if (pe->max) {
      if (pe->next)
        snprintf(&buf[0], sizeof(buf), "%s=%u,", pe->key, pe->max);
      else
        snprintf(&buf[0], sizeof(buf), "%s=%u", pe->key, pe->max);

      body += &buf[0];
    }
    pe = pe->next;
  }

  // Create the payload with debug data for validating sensor stability
  // ------------------------------------------------------------------------------------------
  snprintf(&buf[0], sizeof(buf), "\ndebug,host=%s,device=%s ",
           myConfig.getMDNS(), myConfig.getID());
  body += &buf[0];
#if defined(ESP8266)
  snprintf(&buf[0], sizeof(buf),
           "ds-temp=%.2f,heap=%d,heap-frag=%d,heap-max=%d,stack=%d",
           myTempSensor.getTempC(), ESP.getFreeHeap(),
           ESP.getHeapFragmentation(), ESP.getMaxFreeBlockSize(),
           ESP.getFreeContStack());
#else  // defined (ESP32)
  snprintf(
      &buf[0], sizeof(buf), "ds-temp=%.2f,heap=%d,heap-frag=%d,heap-max=%d",
      myTempSensor.getTempC(), ESP.getFreeHeap(), 0, ESP.getMaxAllocHeap());
#endif

  body += &buf[0];

#if LOG_LEVEL == 6 && !defined(HELPER_DISABLE_LOGGING)
  Log.verbose(F("PERF: url %s." CR), serverPath.c_str());
  Log.verbose(F("PERF: data %s." CR), body.c_str());
#endif

  // Send HTTP POST request
  String auth = "Token " + String(myConfig.getInfluxDb2PushToken());
  http.addHeader(F("Authorization"), auth.c_str());
  http.setTimeout(myAdvancedConfig.getPushTimeout());
  int httpResponseCode = http.POST(body);

  if (httpResponseCode == 204) {
#if !defined(HELPER_DISABLE_LOGGING)
    Log.notice(
        F("PERF: InfluxDB2 push performance data successful, response=%d" CR),
        httpResponseCode);
#endif
  } else {
    Log.error(F("PERF: InfluxDB2 push performance data failed, response=%d" CR),
              httpResponseCode);
  }

  http.end();
  wifi.stop();
  tcp_cleanup();
}

#endif  // COLLECT_PERFDATA

char* convertFloatToString(float f, char* buffer, int dec) {
  dtostrf(f, 6, dec, buffer);
  return buffer;
}

float reduceFloatPrecision(float f, int dec) {
  char buffer[5];
  dtostrf(f, 6, dec, &buffer[0]);
  return atof(&buffer[0]);
}

// EOF
