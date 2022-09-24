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
#include <tempsensor.hpp>
#include <webserver.hpp>
#include <wifi.hpp>

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
  LOG_PERF_START("run-time");
  LOG_PERF_START("main-setup");
  runtimeMillis = millis();

  Log.notice(F("Main: Started setup for %s." CR),
             String(ESP.getChipId(), HEX).c_str());
  printBuildOptions();

  LOG_PERF_START("main-config-load");
  myConfig.checkFileSystem();
  checkResetReason();
  myConfig.loadFile();
  myWifi.init();
  myAdvancedConfig.loadFile();
  LOG_PERF_STOP("main-config-load");

  // Setup watchdog
#if defined(ESP8266)
  ESP.wdtDisable();
  ESP.wdtEnable(5000);  // 5 seconds
#else                   // defined (ESP32)
#endif

  // No stored config, move to portal
  if (!myWifi.hasConfig()) {
    Log.notice(
        F("Main: No wifi configuration detected, entering wifi setup." CR));
    runMode = RunMode::wifiSetupMode;
  }

  // Double reset, go to portal.
  if (myWifi.isDoubleResetDetected()) {
    Log.notice(F("Main: Double reset detected, entering wifi setup." CR));
    runMode = RunMode::wifiSetupMode;
  }

  bool needWifi = true;  // Under ESP32 we dont need wifi if only BLE is active
                         // in gravityMode

  // Do this setup for all modes exect wifi setup
  switch (runMode) {
    case RunMode::wifiSetupMode:
      myWifi.startPortal();
      break;

    default:
      myBatteryVoltage.read();
      checkSleepMode(myBatteryVoltage.getVoltage());

#if defined(ESP32)
      if (!myConfig.isWifiPushActive() && runMode == RunMode::gravityMode) {
        Log.notice(
            F("Main: Wifi is not needed in gravity mode, skipping "
              "connection." CR));
        needWifi = false;
      }
#endif

      if (needWifi) {
        LOG_PERF_START("main-wifi-connect");
        myWifi.connect();
        LOG_PERF_STOP("main-wifi-connect");
      }

      LOG_PERF_START("main-temp-setup");
      myTempSensor.setup();
      LOG_PERF_STOP("main-temp-setup");
      break;
  }

  // Do this setup for configuration mode
  switch (runMode) {
    case RunMode::configurationMode:
      if (myWifi.isConnected()) {
        Log.notice(F("Main: Activating web server." CR));
        myWebServerHandler
            .setupWebServer();  // Takes less than 4ms, so skip this measurement
      }

      interval = 1000;  // Change interval from 200ms to 1s
      break;

    default:
      break;
  }

  LOG_PERF_STOP("main-setup");
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
    LOG_PERF_START("loop-push");

    if (myWifi
            .isConnected()) {  // no need to try if there is no wifi connection.
      LOG_PERF_START("loop-temp-read");
      float tempC = myTempSensor.getTempC();
      LOG_PERF_STOP("loop-temp-read");

      PushTarget push;
      push.sendAll(tempC, (millis() - runtimeMillis) / 1000);
    }

    LOG_PERF_STOP("loop-push");

    // Send stats to influx after each push run.
    if (runMode == RunMode::configurationMode) {
      LOG_PERF_PUSH();
    }
  }
  return true;
}

void loopTempOnInterval() {
  if (abs((int32_t)(millis() - loopMillis)) > interval) {
    loopMillis = millis();
    myBatteryVoltage.read();
    checkSleepMode(myBatteryVoltage.getVoltage());
  }
}

bool skipRunTimeLog = false;

void goToSleep(int sleepInterval) {
  float volt = myBatteryVoltage.getVoltage();
  float runtime = (millis() - runtimeMillis);

  Log.notice(F("MAIN: Entering deep sleep for %ds, run time %Fs, "
               "battery=%FV." CR),
             sleepInterval, reduceFloatPrecision(runtime / 1000, 2), volt);
  LittleFS.end();
  LOG_PERF_STOP("run-time");
  LOG_PERF_PUSH();
  delay(100);
  deepSleep(sleepInterval);
}

void loop() {
  switch (runMode) {
    case RunMode::configurationMode:
      if (myWifi.isConnected()) myWebServerHandler.loop();

      myWifi.loop();
      loopTempOnInterval();

      // If we switched mode, dont include this in the log.
      if (runMode != RunMode::configurationMode) skipRunTimeLog = true;
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
