/**
 * @file web.cpp
 * @brief Webserver handler
 *
 * @copyright Copyright (c) 2020
 *
 */
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>
#include "common.h"

const char WALLCLOCK_HTTP_HEADER[] PROGMEM = "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/><title>{v}</title>";
const char WALLCLOCK_HTTP_STYLE[] PROGMEM = "<style>.c{text-align: center;} div,input{padding:5px;font-size:1em;} input{width:95%;} body{text-align: center;font-family:verdana;} button{border:0;border-radius:0.3rem;background-color:#1fa3ec;color:#fff;line-height:2.4rem;font-size:1.2rem;width:100%;} .q{float: right;width: 64px;text-align: right;} .l{background: url(\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAMAAABEpIrGAAAALVBMVEX///8EBwfBwsLw8PAzNjaCg4NTVVUjJiZDRUUUFxdiZGSho6OSk5Pg4eFydHTCjaf3AAAAZElEQVQ4je2NSw7AIAhEBamKn97/uMXEGBvozkWb9C2Zx4xzWykBhFAeYp9gkLyZE0zIMno9n4g19hmdY39scwqVkOXaxph0ZCXQcqxSpgQpONa59wkRDOL93eAXvimwlbPbwwVAegLS1HGfZAAAAABJRU5ErkJggg==\") no-repeat left center;background-size: 1em;}</style>";
const char WALLCLOCK_HTTP_HEADER_END[] PROGMEM = "</head><body><div style='text-align:left;display:inline-block;min-width:260px;'>";
const char WALLCLOCK_HTTP_END[] PROGMEM = "</div></body></html>";

ESP8266WebServer webServer(80);

/**
 * @brief Root page of the webserver.
 *
 */
void handleRoot()
{
    char buf[30];
    time_t t;
    time(&t);
    struct tm *timeinfo = localtime(&t);

    sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

    String page = FPSTR(WALLCLOCK_HTTP_HEADER);
    page.replace("{v}", "Wall clock status");
    page += FPSTR(WALLCLOCK_HTTP_STYLE);
    page += FPSTR(WALLCLOCK_HTTP_HEADER_END);
    page += F("<h1>Wall clock status</h1>");
    page += F("<dl>");
    page += F("<dt>Current time</dt><dd>");
    page += buf;
    page += F("</dd>");
    page += F("<dt>ADC value</dt><dd>");
    page += analogRead(A0);
    page += F("</dd>");
    page += F("<dt>Chip ID</dt><dd>");
    page += ESP.getChipId();
    page += F("</dd>");
    page += F("<dt>Flash Chip ID</dt><dd>");
    page += ESP.getFlashChipId();
    page += F("</dd>");
    page += F("<dt>IDE Flash Size</dt><dd>");
    page += ESP.getFlashChipSize();
    page += F(" bytes</dd>");
    page += F("<dt>Real Flash Size</dt><dd>");
    page += ESP.getFlashChipRealSize();
    page += F(" bytes</dd>");
    page += F("<dt>Sketch size</dt><dd>");
    page += ESP.getSketchSize();
    page += F("</dd>");
    page += F("<dt>Free sketch space</dt><dd>");
    page += ESP.getFreeSketchSpace();
    page += F("</dd>");
    page += F("<dt>Core version</dt><dd>");
    page += ESP.getCoreVersion();
    page += F("</dd>");
    page += F("<dt>Sdk version</dt><dd>");
    page += ESP.getSdkVersion();
    page += F("</dd>");
    page += F("<dt>Station MAC</dt><dd>");
    page += WiFi.macAddress();
    page += F("</dd>");
    page += F("<dt>Sketch MD5</dt><dd>");
    page += ESP.getSketchMD5();
    page += F("</dd>");
    page += F("<dt>Free heap</dt><dd>");
    page += ESP.getFreeHeap();
    page += F("</dd>");
    page += F("</dl>");
    page += FPSTR(WALLCLOCK_HTTP_END);

    webServer.send(200, "text/html", page);
}

void handleNotFound()
{
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += webServer.uri();
    message += "\nMethod: ";
    message += (webServer.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += webServer.args();
    message += "\n";
    for (uint8_t i = 0; i < webServer.args(); i++)
    {
        message += " " + webServer.argName(i) + ": " + webServer.arg(i) + "\n";
    }
    webServer.send(404, "text/plain", message);
}

void setupWebServer()
{
    webServer.on("/", handleRoot);
    webServer.onNotFound(handleNotFound);
}

String otaMsgL1;
String otaMsgL2;
String otaMsgL3;

void setupOta()
{
    ArduinoOTA.setHostname("wallclock");

    // No authentication by default
    //ArduinoOTA.setPassword((const char *)"xxxxx");
    ArduinoOTA.onStart([]() {
        otaMsgL1 = F("OTA Start");
        Serial.println(otaMsgL1);
    });

    ArduinoOTA.onEnd([]() {
        otaMsgL1 = F("OTA End");
        otaMsgL2 = F("Rebooting...");
        Serial.println(otaMsgL1);
        Serial.println(otaMsgL2);
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        otaMsgL1 = String(F("OTA: ")) + String(progress / (total / 100)) + String(F("%"));
        otaMsgL2 = F("Download...");
        Serial.println(otaMsgL1);
    });

    ArduinoOTA.onError([](ota_error_t error) {
        otaMsgL1 = F("OTA Error");
        otaMsgL2 = String(F("Error[")) + String(error) + String(F("]"));
        if (error == OTA_AUTH_ERROR)
        {
            otaMsgL3 = F("Auth Failed");
        }
        else if (error == OTA_BEGIN_ERROR)
        {
            otaMsgL3 = F("Begin Failed");
        }
        else if (error == OTA_CONNECT_ERROR)
        {
            otaMsgL3 = F("Connect Failed");
        }
        else if (error == OTA_RECEIVE_ERROR)
        {
            otaMsgL3 = F("Receive Failed");
        }
        else if (error == OTA_END_ERROR)
        {
            otaMsgL3 = F("End Failed");
        }
        Serial.println(otaMsgL1);
        Serial.println(otaMsgL2);
        Serial.println(otaMsgL3);
    });
}
