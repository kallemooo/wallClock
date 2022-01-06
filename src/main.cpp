/**
 * @file main.cpp
 * @brief Main class.
 *
 * @copyright Copyright (c) 2021 Karl Thorén
 *
 */

#include <FS.h>          //this needs to be first, or it all crashes and burns...
#include <Arduino.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <SPIFFS.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <Wire.h>
#include <sntp.h>
#include <uptime.h>
#include "common.h"

Adafruit_7segment matrix = Adafruit_7segment();

const char TZ_Europe_Stockholm[]                  PROGMEM = "CET-1CEST,M3.5.0,M10.5.0/3";

// setting PWM properties
const int freq = 5000;
const int ledChannel = 0;
const int resolution = 8;

static const uint timeout = 120u; // seconds to run config portal for.

static const uint8_t BUTTON_PIN = 0;

Ticker ticker;
static ulong connectedTimeStamp = 0u;

static String ntpServer1(FPSTR("ntp1.sptime.se"));
static String ntpServer2(FPSTR("ntp2.sptime.se"));
static String ntpServer3(FPSTR("pool.ntp.org"));

const char settingsFile[] = "/config.json";

WiFiManager wm; // global wm instance
WiFiManagerParameter ntpServer1Param;
WiFiManagerParameter ntpServer2Param;
WiFiManagerParameter ntpServer3Param;

//callback notifying us of the need to save config
void saveConfigCallback();

void setup()
{
    Serial.begin(115200);
    Serial.println(F("Booting..."));
    matrix.begin(0x70);
    matrix.setBrightness(0u);
    matrix.writeDigitNum(0u, 0x8, true);
    matrix.writeDigitNum(1u, 0x8, true);
    matrix.writeDigitNum(3u, 0x8, true);
    matrix.writeDigitNum(4u, 0x8, true);
    matrix.drawColon(true);
    matrix.writeDisplay();

    //set LED pin as output
    pinMode(LED_BUILTIN, OUTPUT);
    // configure LED PWM functionalities
    ledcSetup(ledChannel, freq, resolution);

    // attach the channel to the GPIO to be controlled
    ledcAttachPin(LED_BUILTIN, ledChannel);
    ledcWrite(ledChannel, 255u);

    WiFi.mode(WIFI_STA);
    WiFi.hostname(F("wallclock"));

    setupOta();
    setupWebServer();

    pinMode(BUTTON_PIN, INPUT);

    if (SPIFFS.begin())
    {
        Serial.println(F("Mounted file system"));
        if (SPIFFS.exists(settingsFile))
        {
            //file exists, reading and loading
            Serial.println(F("reading config file"));
            File configFile = SPIFFS.open(settingsFile, "r");
            if (configFile)
            {
                Serial.println(F("opened config file"));
                StaticJsonDocument<500> jDoc;
                // Deserialize the JSON document
                DeserializationError error = deserializeJson(jDoc, configFile);

                // Test if parsing succeeds.
                if (error)
                {
                    Serial.print(F("Failed to load json config: "));
                    Serial.println(error.c_str());
                }
                else
                {
                    Serial.println(F("parsed json config"));
                    const char* ntp_server1 = jDoc[F("ntp_server1")];
                    ntpServer1 = ntp_server1;
                    const char* ntp_server2 = jDoc[F("ntp_server2")];
                    ntpServer2 = ntp_server2;
                    const char* ntp_server3 = jDoc[F("ntp_server3")];
                    ntpServer3 = ntp_server3;
                }
            }
            else
            {
                Serial.println(F("failed to open config file."));
            }
        }
        else
        {
            Serial.println(F("Config file is not present."));
        }
    }
    else
    {
        Serial.println(F("failed to mount FS"));
    }

    new (&ntpServer1Param) WiFiManagerParameter("ntpServer1", "NTP server 1", ntpServer1.c_str(), 500u);
    new (&ntpServer2Param) WiFiManagerParameter("ntpServer2", "NTP server 2", ntpServer2.c_str(), 500u);
    new (&ntpServer3Param) WiFiManagerParameter("ntpServer3", "NTP server 3", ntpServer3.c_str(), 500u);

    wm.addParameter(&ntpServer1Param);
    wm.addParameter(&ntpServer2Param);
    wm.addParameter(&ntpServer3Param);
    wm.setSaveParamsCallback(saveConfigCallback);
    wm.setDarkMode(true);

    // set configportal timeout
    wm.setConfigPortalTimeout(timeout);
    bool res;
    res = wm.autoConnect("WallClockAP"); // anonymous ap

    if (!res)
    {
        Serial.println(F("Failed to connect or hit timeout"));
        // ESP.restart();
    }
    else
    {
        //if you get here you have connected to the WiFi
        Serial.println(F("Connected to WIFI!"));
    }

    adcAttachPin(A0);
    analogSetClockDiv(5);

    ticker.attach(1, secTicker); // Run a 1 second interval Ticker
    connectedTimeStamp = millis();
}

static bool setupDone = false;
static const ulong CONNECT_FAILED_TIMEOUT_RESTART = 10u * 60u * 1000u;
static const ulong CONNECT_FAILED_TIMEOUT_RECONNECT = 30u * 1000u;

//callback notifying us of the need to save config
void saveConfigCallback()
{
    // Fetch new values.
    ntpServer1 = ntpServer1Param.getValue();
    ntpServer2 = ntpServer2Param.getValue();
    ntpServer3 = ntpServer3Param.getValue();

    Serial.println(F("saving config"));
    StaticJsonDocument<500> jDoc;
    jDoc[F("ntp_server1")] = ntpServer1;
    jDoc[F("ntp_server2")] = ntpServer2;
    jDoc[F("ntp_server3")] = ntpServer3;

    File configFile = SPIFFS.open(settingsFile, "w");
    if (!configFile)
    {
        Serial.println(F("failed to open config file for writing"));
    }
    else
    {
        serializeJson(jDoc, configFile);
        configFile.close();
    }
}

String scrollString;

void loop()
{
    uptime::calculateUptime();

    // is configuration portal requested?
    if (digitalRead(BUTTON_PIN) == LOW)
    {
        ledcWrite(ledChannel, 255u);

        // Stop the web server as the config portal needs port 80.
        webServer.stop();

        matrix.writeDigitNum(0u, 0xF, false);
        matrix.writeDigitNum(1u, 0xF, false);
        matrix.writeDigitNum(3u, 0xF, false);
        matrix.writeDigitNum(4u, 0xF, false);
        matrix.writeDisplay();

        if (!wm.startConfigPortal("WallClockAP"))
        {
            matrix.printError();
            matrix.writeDisplay();
            Serial.println(F("failed to connect and hit timeout"));
            delay(3000);
            ESP.restart();
            delay(5000);
        }

        //if you get here you have connected to the WiFi
        Serial.println(F("Connected to WIFI!"));
        connectedTimeStamp = millis();
        setupDone = false;
    }

    if (WiFi.isConnected())
    {
        if (!setupDone)
        {
            ArduinoOTA.begin();
            webServer.begin();
            Serial.println(F("Ready"));
            Serial.print(F("IP address: "));
            Serial.println(WiFi.localIP());
            configTzTime(TZ_Europe_Stockholm, ntpServer1.c_str(), ntpServer2.c_str(), ntpServer3.c_str());
            setupDone = true;
            ledcWrite(ledChannel, 1u);
        }

        webServer.handleClient();
        ArduinoOTA.handle();
    }
    else
    {
        ledcWrite(ledChannel, 255u);
        setupDone = false;
        if (millis() > (CONNECT_FAILED_TIMEOUT_RESTART + connectedTimeStamp))
        {
            matrix.printError();
            matrix.writeDisplay();
            Serial.println(F("failed to connect and hit timeout, restart to try again."));
            delay(3000);
            ESP.restart();
            delay(5000);
        }
    }
}

#define FILTER_LEN  5

static uint16_t AN_LDR_Buffer[FILTER_LEN] = {0};
static uint32_t AN_LDR_i = 0;

uint16_t readADC_Avg(uint16_t ADC_Raw)
{
    uint32_t i = 0;
    uint32_t Sum = 0;

    AN_LDR_Buffer[AN_LDR_i++] = ADC_Raw;
    if (AN_LDR_i == FILTER_LEN)
    {
        AN_LDR_i = 0;
    }

    for (i=0; i < FILTER_LEN; i++)
    {
        Sum += AN_LDR_Buffer[i];
    }

    return (Sum/FILTER_LEN);
}

static uint8_t tick = 0u;
uint16_t ldrAdVal = 0;

void secTicker()
{
    if (!WiFi.isConnected())
    {
        matrix.print(tick, DEC);
        matrix.writeDisplay();
        Serial.println(F("No WiFi or waiting for Wifi"));
        Serial.print(F("Millis: "));
        Serial.print(millis());
        Serial.print(F(" reset at: "));
        Serial.println(CONNECT_FAILED_TIMEOUT_RESTART + connectedTimeStamp);
    }
    else
    {
        time_t t;
        time(&t);
        struct tm *timeinfo = localtime(&t);
        Serial.printf("T: %04d-%02d-%02d %02d:%02d:%02d\n", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

        if (timeinfo->tm_year > 100)
        {
            if (0u == timeinfo->tm_hour && 0u == timeinfo->tm_min)
            {
                matrix.clear();
                matrix.writeDigitNum(1u, 0u, false);
                matrix.writeDigitNum(3u, 0u, false);
                matrix.writeDigitNum(4u, 0u, false);
            }
            else
            {
                float tmpTime = timeinfo->tm_hour + (float)timeinfo->tm_min / 100;
                matrix.print(tmpTime);
            }
        }
        else
        {
            matrix.print(tick, DEC);
        }
        matrix.drawColon(true);
        matrix.writeDisplay();
    }

    {
        uint8_t brightness = 0u;
        uint16_t adValRaw = analogRead(A0);
        ldrAdVal = readADC_Avg(adValRaw);

        if (ldrAdVal < 600u)
        {
            brightness = 0u;
        }
        else
        {
            brightness = (ldrAdVal - 500u)/100u;
            if (brightness > 15u)
            {
                brightness = 15u;
            }
        }

        matrix.setBrightness(brightness);
        Serial.printf("A0: %04d Avg: %02d B; %02d\n", adValRaw, ldrAdVal, brightness);
    }
    tick++;
}
