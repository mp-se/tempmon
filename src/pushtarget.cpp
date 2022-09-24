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
#include <ESP8266mDNS.h>
#include <MQTT.h>

#include <config.hpp>
#include <helper.hpp>
#include <pushtarget.hpp>
#include <wifi.hpp>

void PushTarget::sendAll(float tempC, float runTime) {
  printHeap("PUSH");

  _http.setReuse(true);
  _httpSecure.setReuse(true);

  TemplatingEngine engine;
  engine.initialize(tempC, runTime);

  if (myConfig.isHttpActive()) {
    LOG_PERF_START("push-http");
    sendHttpPost(engine, myConfig.isHttpSSL());
    LOG_PERF_STOP("push-http");
  }

  if (myConfig.isHttp3Active()) {
    LOG_PERF_START("push-http3");
    sendHttpGet(engine, myConfig.isHttp3SSL());
    LOG_PERF_STOP("push-http3");
  }

  if (myConfig.isInfluxDb2Active()) {
    LOG_PERF_START("push-influxdb2");
    sendInfluxDb2(engine, myConfig.isInfluxSSL());
    LOG_PERF_STOP("push-influxdb2");
  }

  if (myConfig.isMqttActive()) {
    LOG_PERF_START("push-mqtt");
    sendMqtt(engine, myConfig.isMqttSSL());
    LOG_PERF_STOP("push-mqtt");
  }

  engine.freeMemory();
}

void PushTarget::probeMaxFragement(String& serverPath) {
  // Format: http:://servername:port/path
  int port = 443;
  String host = serverPath.substring(8);  // remove the prefix or the probe will
                                          // fail, it needs a pure host name.
  // Remove the path if it exist
  int idx = host.indexOf("/");
  if (idx != -1) host = host.substring(0, idx);

  // If a server port is defined, lets extract that part
  idx = host.indexOf(":");
  if (idx != -1) {
    String p = host.substring(idx + 1);
    port = p.toInt();
    host = host.substring(0, idx);
  }

  Log.notice(F("PUSH: Probing server to max fragment %s:%d" CR), host.c_str(),
             port);
  if (_wifiSecure.probeMaxFragmentLength(host, port, 512)) {
    Log.notice(F("PUSH: Server supports smaller SSL buffer." CR));
    _wifiSecure.setBufferSizes(512, 512);
  }
}

void PushTarget::sendInfluxDb2(TemplatingEngine& engine, bool isSecure) {
#if !defined(PUSH_DISABLE_LOGGING)
  Log.notice(F("PUSH: Sending values to influxdb2." CR));
#endif
  _lastCode = 0;
  _lastSuccess = false;

  String serverPath =
      String(myConfig.getInfluxDb2PushUrl()) +
      "/api/v2/write?org=" + String(myConfig.getInfluxDb2PushOrg()) +
      "&bucket=" + String(myConfig.getInfluxDb2PushBucket());
  String doc = engine.create(TemplatingEngine::TEMPLATE_INFLUX);

#if LOG_LEVEL == 6 && !defined(PUSH_DISABLE_LOGGING)
  Log.verbose(F("PUSH: url %s." CR), serverPath.c_str());
  Log.verbose(F("PUSH: data %s." CR), doc.c_str());
#endif

  String auth = "Token " + String(myConfig.getInfluxDb2PushToken());

  if (isSecure) {
    if (runMode == RunMode::configurationMode) {
      Log.notice(
          F("PUSH: Skipping InfluxDB since SSL is enabled and we are in config "
            "mode." CR));
      _lastCode = -100;
      return;
    }

    Log.notice(F("PUSH: InfluxDB, SSL enabled without validation." CR));
    _wifiSecure.setInsecure();
    probeMaxFragement(serverPath);
    _httpSecure.setTimeout(myAdvancedConfig.getPushTimeout() * 1000);
    _httpSecure.begin(_wifiSecure, serverPath);
    _httpSecure.addHeader(F("Authorization"), auth.c_str());
    _lastCode = _httpSecure.POST(doc);
  } else {
    _http.setTimeout(myAdvancedConfig.getPushTimeout() * 1000);
    _http.begin(_wifi, serverPath);
    _http.addHeader(F("Authorization"), auth.c_str());
    _lastCode = _http.POST(doc);
  }

  if (_lastCode == 204) {
    _lastSuccess = true;
    Log.notice(F("PUSH: InfluxDB2 push successful, response=%d" CR), _lastCode);
  } else {
    Log.error(F("PUSH: Influxdb push failed response=%d" CR), _lastCode);
  }

  if (isSecure) {
    _httpSecure.end();
    _wifiSecure.stop();
  } else {
    _http.end();
    _wifi.stop();
  }
  tcp_cleanup();
}

void PushTarget::addHttpHeader(HTTPClient& http, String header) {
  if (!header.length()) return;

  int i = header.indexOf(":");
  if (i) {
    String name = header.substring(0, i);
    String value = header.substring(i + 1);
    value.trim();
    Log.notice(F("PUSH: Adding header '%s': '%s'" CR), name.c_str(),
               value.c_str());
    http.addHeader(name, value);
  } else {
    Log.error(F("PUSH: Unable to set header, invalid value %s" CR),
              header.c_str());
  }
}

void PushTarget::sendHttpPost(TemplatingEngine& engine, bool isSecure) {
#if !defined(PUSH_DISABLE_LOGGING)
  Log.notice(F("PUSH: Sending values to http post" CR));
#endif
  _lastCode = 0;
  _lastSuccess = false;

  String serverPath, doc;

  serverPath = myConfig.getHttpUrl();
  doc = engine.create(TemplatingEngine::TEMPLATE_HTTP1);

#if LOG_LEVEL == 6
  Log.verbose(F("PUSH: url %s." CR), serverPath.c_str());
  Log.verbose(F("PUSH: json %s." CR), doc.c_str());
#endif

  if (isSecure) {
    if (runMode == RunMode::configurationMode) {
      Log.notice(
          F("PUSH: Skipping HTTP since SSL is enabled and we are in config "
            "mode." CR));
      _lastCode = -100;
      return;
    }

    Log.notice(F("PUSH: HTTP, SSL enabled without validation." CR));
    _wifiSecure.setInsecure();
    probeMaxFragement(serverPath);
    _httpSecure.setTimeout(myAdvancedConfig.getPushTimeout() * 1000);
    _httpSecure.begin(_wifiSecure, serverPath);
    addHttpHeader(_httpSecure, myConfig.getHttpHeader(0));
    addHttpHeader(_httpSecure, myConfig.getHttpHeader(1));
    _lastCode = _httpSecure.POST(doc);
  } else {
    _http.setTimeout(myAdvancedConfig.getPushTimeout() * 1000);
    _http.begin(_wifi, serverPath);
    addHttpHeader(_http, myConfig.getHttpHeader(0));
    addHttpHeader(_http, myConfig.getHttpHeader(1));
    _lastCode = _http.POST(doc);
  }

  if (_lastCode == 200) {
    _lastSuccess = true;
    Log.notice(F("PUSH: HTTP post successful, response=%d" CR), _lastCode);
  } else {
    Log.error(F("PUSH: HTTP post failed response=%d" CR), _lastCode);
  }

  if (isSecure) {
    _httpSecure.end();
    _wifiSecure.stop();
  } else {
    _http.end();
    _wifi.stop();
  }
  tcp_cleanup();
}

void PushTarget::sendHttpGet(TemplatingEngine& engine, bool isSecure) {
#if !defined(PUSH_DISABLE_LOGGING)
  Log.notice(F("PUSH: Sending values to http3" CR));
#endif
  _lastCode = 0;
  _lastSuccess = false;

  String serverPath;

  serverPath = myConfig.getHttp3Url();
  serverPath += engine.create(TemplatingEngine::TEMPLATE_HTTP3);

#if LOG_LEVEL == 6
  Log.verbose(F("PUSH: url %s." CR), serverPath.c_str());
#endif

  if (isSecure) {
    if (runMode == RunMode::configurationMode) {
      Log.notice(
          F("PUSH: Skipping HTTP since SSL is enabled and we are in config "
            "mode." CR));
      _lastCode = -100;
      return;
    }

    Log.notice(F("PUSH: HTTP, SSL enabled without validation." CR));
    _wifiSecure.setInsecure();
    probeMaxFragement(serverPath);
    _httpSecure.setTimeout(myAdvancedConfig.getPushTimeout() * 1000);
    _httpSecure.begin(_wifiSecure, serverPath);
    _lastCode = _httpSecure.GET();
  } else {
    _http.setTimeout(myAdvancedConfig.getPushTimeout() * 1000);
    _http.begin(_wifi, serverPath);
    _lastCode = _http.GET();
  }

  if (_lastCode == 200) {
    _lastSuccess = true;
    Log.notice(F("PUSH: HTTP get successful, response=%d" CR), _lastCode);
  } else {
    Log.error(F("PUSH: HTTP get failed response=%d" CR), _lastCode);
  }

  if (isSecure) {
    _httpSecure.end();
    _wifiSecure.stop();
  } else {
    _http.end();
    _wifi.stop();
  }
  tcp_cleanup();
}

void PushTarget::sendMqtt(TemplatingEngine& engine, bool isSecure) {
  Log.notice(F("PUSH: Sending values to mqtt." CR));
  _lastCode = 0;
  _lastSuccess = false;

  MQTTClient mqtt(512);
  String host = myConfig.getMqttUrl();
  String doc = engine.create(TemplatingEngine::TEMPLATE_MQTT);
  int port = myConfig.getMqttPort();

  if (myConfig.isMqttSSL()) {
    if (runMode == RunMode::configurationMode) {
      Log.notice(
          F("PUSH: Skipping MQTT since SSL is enabled and we are in config "
            "mode." CR));
      _lastCode = -100;
      return;
    }

    Log.notice(F("PUSH: MQTT, SSL enabled without validation." CR));
    _wifiSecure.setInsecure();

    if (_wifiSecure.probeMaxFragmentLength(host, port, 512)) {
      Log.notice(F("PUSH: MQTT server supports smaller SSL buffer." CR));
      _wifiSecure.setBufferSizes(512, 512);
    }

    mqtt.setTimeout(myAdvancedConfig.getPushTimeout() * 1000);
    mqtt.begin(host.c_str(), port, _wifiSecure);
  } else {
    mqtt.setTimeout(myAdvancedConfig.getPushTimeout() * 1000);
    mqtt.begin(host.c_str(), port, _wifi);
  }

  mqtt.connect(myConfig.getMDNS(), myConfig.getMqttUser(),
               myConfig.getMqttPass());

#if LOG_LEVEL == 6
  Log.verbose(F("PUSH: url %s." CR), myConfig.getMqttUrl());
  Log.verbose(F("PUSH: data %s." CR), doc.c_str());
#endif

  int lines = 1;
  // Find out how many lines are in the document. Each line is one
  // topic/message. | is used as new line.
  for (unsigned int i = 0; i < doc.length() - 1; i++) {
    if (doc.charAt(i) == '|') lines++;
  }

  int index = 0;
  while (lines) {
    int next = doc.indexOf('|', index);
    String line = doc.substring(index, next);

    // Each line equals one topic post, format is <topic>:<value>
    String topic = line.substring(0, line.indexOf(":"));
    String value = line.substring(line.indexOf(":") + 1);
#if LOG_LEVEL == 6
    Log.verbose(F("PUSH: topic '%s', value '%s'." CR), topic.c_str(),
                value.c_str());
#endif

    if (mqtt.publish(topic, value)) {
      _lastSuccess = true;
      Log.notice(F("PUSH: MQTT publish successful on %s" CR), topic.c_str());
      _lastCode = 0;
    } else {
      _lastSuccess = false;
      _lastCode = mqtt.lastError();
      Log.error(F("PUSH: MQTT push on %s failed error=%d" CR), topic.c_str(),
                _lastCode);
    }

    index = next + 1;
    lines--;
  }

  mqtt.disconnect();
  if (isSecure) {
    _wifiSecure.stop();
  } else {
    _wifi.stop();
  }
  tcp_cleanup();
}

// EOF
