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
#include <basepush.hpp>
#include <battery.hpp>
#include <log.hpp>
#include <main.hpp>
#include <ota.hpp>
#include <tempconfig.hpp>
#include <templating.hpp>
#include <tempsensor.hpp>
#include <tempweb.hpp>
#include <wificonnection.hpp>

SerialDebug mySerial(115200L);
TempConfig myConfig("tempmon", "/tempmon.json");
WifiConnection myWifi(&myConfig, "temp", "password", "tempmon", "", "");
// OtaUpdate myOta(&myConfig, "0.0.0");
BasePush myPush(&myConfig);
TempWebHandler myWebHandler(&myConfig);

int interval = 200;       // ms, time to wait between changes to output
uint32_t loopMillis = 0;  // Used for main loop to run the code every _interval_
uint32_t pushMillis = 0;  // Used to control how often we will send push data
uint32_t runtimeMillis;   // Used to calculate the total time since start/wakeup

RunMode runMode = RunMode::tempMode;

void checkSleepMode(float volt) {
  if (volt > myConfig.getVoltageConfig()) {
    runMode = RunMode::configurationMode;
  } else {
    runMode = RunMode::tempMode;
  }

  switch (runMode) {
    case RunMode::configurationMode:
      Log.notice(F("MAIN: run mode CONFIG (volt=%F)." CR), volt);
      break;
    case RunMode::wifiSetupMode:
      break;
    case RunMode::tempMode:
      Log.notice(F("MAIN: run mode TEMP (volt=%F)." CR), volt);
      break;
  }
}

void setup() {
  runtimeMillis = millis();

  Log.notice(F("Main: Started setup for %s." CR),
             String(ESP.getChipId(), HEX).c_str());

  myConfig.checkFileSystem();
  myConfig.loadFile();
  myWifi.init();

#if defined(ESP8266)
  ESP.wdtDisable();
  ESP.wdtEnable(5000);
#else
#endif

  if (!myWifi.hasConfig()) {
    Log.notice(
        F("Main: No wifi configuration detected, entering wifi setup." CR));
    runMode = RunMode::wifiSetupMode;
  }

  if (myWifi.isDoubleResetDetected()) {
    Log.notice(F("Main: Double reset detected, entering wifi setup." CR));
    runMode = RunMode::wifiSetupMode;
  }

  switch (runMode) {
    case RunMode::wifiSetupMode:
      myWifi.startPortal();
      break;

    default:
      BatteryVoltage bv;
      bv.getInstance().setConfig(&myConfig);
      bv.getInstance().read();
      checkSleepMode(bv.getInstance().getVoltage());

#if defined(ESP32)
      if (!myConfig.isWifiPushActive() && runMode == RunMode::gravityMode) {
        Log.notice(
            F("Main: Wifi is not needed in gravity mode, skipping "
              "connection." CR));
        needWifi = false;
      }
#endif
      myWifi.connect();
      TempSensor ts;
      ts.getInstance().setup(&myConfig);
      break;
  }

  switch (runMode) {
    case RunMode::configurationMode:
      if (myWifi.isConnected()) {
        Log.notice(F("Main: Activating web server." CR));
        myWebHandler.setupWebServer();
      }

      interval = 1000;
      break;

    default:
      break;
  }

  Log.notice(F("Main: Setup completed." CR));
}

bool loopReadTemp() {
#if LOG_LEVEL == 6
  Log.verbose(F("Main: Entering main loopTemp." CR));
#endif

  bool pushExpired = (abs((int32_t)(millis() - pushMillis)) >
                      (myConfig.getSleepInterval() * 1000));

  if (pushExpired || runMode == RunMode::tempMode) {
    pushMillis = millis();

    if (myWifi.isConnected()) {
      TempSensor ts;
      BatteryVoltage bv;
      BasePush push(&myConfig);
      TemplatingEngine tpl(&myConfig);

      tpl.initialize(ts.getInstance().getTempC(), bv.getInstance().getVoltage(),
                     (millis() - runtimeMillis) / 1000);

      if (myConfig.hasTargetHttpPost()) {
        String payload = tpl.create(TemplatingEngine::TEMPLATE_HTTP1);
        push.sendHttpPost(payload);
      }

      if (myConfig.hasTargetHttpGet()) {
        String payload = tpl.create(TemplatingEngine::TEMPLATE_HTTP3);
        push.sendHttpGet(payload);
      }

      if (myConfig.hasTargetInfluxDb2()) {
        String payload = tpl.create(TemplatingEngine::TEMPLATE_INFLUX);
        push.sendInfluxDb2(payload);
      }

      if (myConfig.hasTargetMqtt()) {
        String payload = tpl.create(TemplatingEngine::TEMPLATE_MQTT);
        push.sendMqtt(payload);
      }
    }
  }
  return true;
}

void loopTempOnInterval() {
  if (abs((int32_t)(millis() - loopMillis)) > interval) {
    loopMillis = millis();
    BatteryVoltage bv;
    bv.getInstance().read();
    checkSleepMode(bv.getInstance().getVoltage());
  }
}

void goToSleep(int sleepInterval) {
  float runtime = (millis() - runtimeMillis);

  Log.notice(F("MAIN: Entering deep sleep for %ds, run time %Fs." CR),
             sleepInterval, reduceFloatPrecision(runtime / 1000, 2));
  LittleFS.end();
  delay(100);
  deepSleep(sleepInterval);
}

void loop() {
  switch (runMode) {
    case RunMode::configurationMode:
      if (myWifi.isConnected()) myWebHandler.loop();

      myWifi.loop();
      loopTempOnInterval();
      break;

    case RunMode::tempMode:
      // If we didnt get a wifi connection, we enter sleep for a short time to
      // conserve battery.
      if (!myWifi.isConnected()) {  // no connection to wifi and we have
                                    // defined push targets.
        Log.notice(
            F("MAIN: No connection to wifi established, sleeping for 60s." CR));
        myWifi.stopDoubleReset();
        goToSleep(60);
      }

      if (loopReadTemp()) {
        myWifi.stopDoubleReset();
        goToSleep(myConfig.getSleepInterval());
      }

      myWifi.loop();
      break;

    case RunMode::wifiSetupMode:
      myWifi.loop();
      break;
  }
}

// EOF
