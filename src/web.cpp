/**
 * @file web.cpp
 * @brief Webserver handler
 *
 * @copyright Copyright (c) 2021 Karl Thorén
 *
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include <strings_en.h>
#include <uptime_formatter.h>
#include "common.h"

WebServer webServer(80);

String getHTTPHead(String title)
{
    String page;
    page += FPSTR(HTTP_HEAD_START);
    page.replace(FPSTR(T_v), title);
    page += FPSTR(HTTP_SCRIPT);
    page += FPSTR(HTTP_STYLE);

    {
        String p = FPSTR(HTTP_HEAD_END);
        p.replace(FPSTR(T_c), "invert"); // add class str
        page += p;
    }

    return page;
}

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

    String page = getHTTPHead("Minidisplay status");
    String str = FPSTR(HTTP_ROOT_MAIN);
    str.replace(FPSTR(T_t), "Minidisplay status");
    str.replace(FPSTR(T_v), (String)WiFi.getHostname() + " - " + WiFi.localIP().toString());
    page += str;
    page += F("<dl>");
    page += F("<dt>Current time</dt><dd>");
    page += buf;
    page += F("</dd>");
    page += F("<dt>Uptime</dt><dd>");
    page += uptime_formatter::getUptime();
    page += F("</dd>");
    page += F("<dt>ADC value</dt><dd>");
    page += ldrAdVal;
    page += F("</dd>");
    page += F("<dt>Chip cores</dt><dd>");
    page += ESP.getChipCores();
    page += F("</dd>");
    page += F("<dt>Chip model</dt><dd>");
    page += ESP.getChipModel();
    page += F("</dd>");
    page += F("<dt>Chip revision</dt><dd>");
    page += ESP.getChipRevision();
    page += F("</dd>");
    page += F("<dt>Flash Size</dt><dd>");
    page += ESP.getFlashChipSize();
    page += F(" bytes</dd>");
    page += F("<dt>Sketch size</dt><dd>");
    page += ESP.getSketchSize();
    page += F("</dd>");
    page += F("<dt>Free sketch space</dt><dd>");
    page += ESP.getFreeSketchSpace();
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
    page += FPSTR(HTTP_END);

    webServer.send(200, FPSTR(HTTP_HEAD_CT), page);
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

void setupOta()
{
    ArduinoOTA.setHostname("wallclock");

    // No authentication by default
    //ArduinoOTA.setPassword((const char *)"xxxxx");
    ArduinoOTA.onStart([]() {
        Serial.println(F("OTA Start"));
        ticker.detach();
        matrix.setBrightness(14u);
    });

    ArduinoOTA.onEnd([]() {
        Serial.println(F("OTA End"));
        Serial.println(F("Rebooting..."));
        matrix.printError();
        matrix.writeDisplay();
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.println(String(F("OTA: ")) + String(progress / (total / 100)) + String(F("%")));
        matrix.print(progress / (total / 100));
        matrix.writeDisplay();
    });

    ArduinoOTA.onError([](ota_error_t error) {
        Serial.println(F("OTA Error"));
        Serial.println(F("Error["));
        Serial.print(error);
        Serial.println(F("]"));
        String otaMsgL3;
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
        Serial.println(otaMsgL3);
        matrix.clear();
        matrix.writeDigitNum(0u, 0xE, false);
        matrix.writeDigitNum(4u, error, false);
        matrix.writeDisplay();
    });
    ArduinoOTA.begin();
}
