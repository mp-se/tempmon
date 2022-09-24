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
#ifndef SRC_WEBSERVER_HPP_
#define SRC_WEBSERVER_HPP_

#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#define MAX_SKETCH_SPACE 1044464
#include <incbin.h>

INCBIN_EXTERN(IndexHtm);
INCBIN_EXTERN(ConfigHtm);
INCBIN_EXTERN(AboutHtm);
INCBIN_EXTERN(FirmwareHtm);

class WebServerHandler {
 private:
  ESP8266WebServer* _server = 0;
  File _uploadFile;
  int _uploadReturn = 200;

  void webHandleConfig();
  void webHandleConfigAdvancedRead();
  void webHandleConfigAdvancedWrite();
  void webHandleConfigHardware();
  void webHandleConfigPush();
  void webHandleConfigDevice();
  void webHandleClearWIFI();
  void webHandleStatus();
  void webHandleUploadFile();
  void webHandlePageNotFound();

  String getRequestArguments();

  // Inline functions.
  void webReturnOK() { _server->send(_uploadReturn); }
  void webReturnIndexHtm() {
    _server->send_P(200, "text/html", (const char*)gIndexHtmData,
                    gIndexHtmSize);
  }
  void webReturnConfigHtm() {
    _server->send_P(200, "text/html", (const char*)gConfigHtmData,
                    gConfigHtmSize);
  }
  void webReturnAboutHtm() {
    _server->send_P(200, "text/html", (const char*)gAboutHtmData,
                    gAboutHtmSize);
  }
  void webReturnFirmwareHtm() {
    _server->send_P(200, "text/html", (const char*)gFirmwareHtmData,
                    gFirmwareHtmSize);
  }

 public:
  enum HtmlFile { HTML_INDEX = 0, HTML_CONFIG = 1, HTML_ABOUT = 2 };

  bool setupWebServer();
  void loop();
};

// Global instance created
extern WebServerHandler myWebServerHandler;

#endif  // SRC_WEBSERVER_HPP_

// EOF
