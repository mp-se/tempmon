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
#include <DallasTemperature.h>
#include <OneWire.h>
#include <Wire.h>

#include <main.hpp>
#include <tempsensor.hpp>

OneWire myOneWire(PIN_DS18B20);
DallasTemperature mySensors(&myOneWire);

void TempSensor::setup(TempConfig* config) {
  _config = config;
#if LOG_LEVEL == 6
  Log.verbose(F("TSEN: Looking for temp sensors." CR));
#endif
  mySensors.begin();

  if (mySensors.getDS18Count()) {
    Log.notice(
        F("TSEN: Found %d temperature sensor(s). Using %d resolution" CR),
        mySensors.getDS18Count(), _config->getTempSensorResolution());
  }

  // Set the temp sensor adjustment values
  _tempSensorAdjC = _config->getTempSensorAdjC();

#if LOG_LEVEL == 6
  Log.verbose(F("TSEN: Adjustment values for temp sensor %F C." CR),
              _tempSensorAdjC);
#endif
}

float TempSensor::getValue() {
  // If we dont have sensors just return 0
  if (!mySensors.getDS18Count()) {
    Log.notice(F("TSEN: No temperature sensors found. Skipping read." CR));
    return -273;
  }

  // Read the sensors
  mySensors.setResolution(_config->getTempSensorResolution());
  mySensors.requestTemperatures();

  float c = 0;

  if (mySensors.getDS18Count() >= 1) {
    c = mySensors.getTempCByIndex(0);

#if LOG_LEVEL == 6
    Log.verbose(F("TSEN: Reciving temp value for DS18B20 sensor %F C." CR), c);
#endif
    _hasSensor = true;
  }
  return c;
}

// EOF
