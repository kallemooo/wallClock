#include <Arduino.h>
#include <LittleFS.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <Ticker.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <Wire.h>
#include <TZ.h>
#include <sntp.h>
#include "common.h"

Adafruit_7segment matrix = Adafruit_7segment();

static const uint timeout = 120u; // seconds to run config portal for.
static const uint BUTTON_PIN = 0u; // Define pin the button is connected to

Ticker ticker;
static ulong connectedTimeStamp = 0u;
static ulong connectedTimeStampReconnect = 0u;

static String ntpServer1(FPSTR("ntp1.sptime.se"));
static String ntpServer2(FPSTR("ntp2.sptime.se"));
static String ntpServer3(FPSTR("pool.ntp.org"));

const char settingsFile[] = "/config.json";

// To make Arduino software autodetect OTA device
WiFiServer TelnetServer(8266);

void setup()
{
    Serial.begin(115200);
    Serial.println(F("Booting..."));
    matrix.begin(0x70);

    //set LED pin as output
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    //To make Arduino software autodetect OTA device
    TelnetServer.begin();

    WiFi.mode(WIFI_STA);
    WiFi.hostname(F("wallclock"));

    setupOta();
    setupWebServer();

    pinMode(BUTTON_PIN, INPUT);

    if (LittleFS.begin())
    {
        Serial.println(F("Mounted file system"));
        if (LittleFS.exists(settingsFile))
        {
            //file exists, reading and loading
            Serial.println(F("reading config file"));
            File configFile = LittleFS.open(settingsFile, "r");
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

    ticker.detach();
    ticker.attach_scheduled(1, secTicker); // Run a 1 second interval Ticker
    connectedTimeStamp = millis();
    connectedTimeStampReconnect = connectedTimeStamp;
}

static bool setupDone = false;
static const ulong CONNECT_FAILED_TIMEOUT_RESTART = 10u * 60u * 1000u;
static const ulong CONNECT_FAILED_TIMEOUT_RECONNECT = 30u * 1000u;

WiFiManagerParameter *ntpServer1Param;
WiFiManagerParameter *ntpServer2Param;
WiFiManagerParameter *ntpServer3Param;

//callback notifying us of the need to save config
void saveConfigCallback()
{
    // Fetch new values.
    ntpServer1 = ntpServer1Param->getValue();
    ntpServer2 = ntpServer2Param->getValue();
    ntpServer3 = ntpServer3Param->getValue();

    Serial.println("saving config");
    StaticJsonDocument<500> jDoc;
    jDoc[F("ntp_server1")] = ntpServer1;
    jDoc[F("ntp_server2")] = ntpServer2;
    jDoc[F("ntp_server3")] = ntpServer3;

    File configFile = LittleFS.open(settingsFile, "w");
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
    // is configuration portal requested?
    if (digitalRead(BUTTON_PIN) == LOW)
    {
        // Stop the web server as the config portal needs port 80.
        webServer.stop();
        WiFiManager wm;

        ntpServer1Param = new WiFiManagerParameter("ntpServer1", "NTP server 1", ntpServer1.c_str(), 100u);
        ntpServer2Param = new WiFiManagerParameter("ntpServer2", "NTP server 2", ntpServer2.c_str(), 100u);
        ntpServer3Param = new WiFiManagerParameter("ntpServer3", "NTP server 3", ntpServer3.c_str(), 100u);

        wm.addParameter(ntpServer1Param);
        wm.addParameter(ntpServer2Param);
        wm.addParameter(ntpServer3Param);
        wm.setSaveParamsCallback(saveConfigCallback);

        // set configportal timeout
        wm.setConfigPortalTimeout(timeout);

        if (!wm.startConfigPortal("WallClockAP"))
        {
            Serial.println(F("failed to connect and hit timeout"));
            delay(3000);
            ESP.restart();
            delay(5000);
        }

        delete ntpServer1Param;
        delete ntpServer2Param;
        delete ntpServer3Param;

        //if you get here you have connected to the WiFi
        Serial.println(F("Connected to WIFI!"));
        connectedTimeStamp = millis();
        connectedTimeStampReconnect = connectedTimeStamp;
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
        }

        webServer.handleClient();
        ArduinoOTA.handle();
    }
    else
    {
        setupDone = false;
        if (millis() > (CONNECT_FAILED_TIMEOUT_RESTART + connectedTimeStamp))
        {
            Serial.println(F("failed to connect and hit timeout, restart to try again."));
            delay(3000);
            ESP.restart();
            delay(5000);
        }
    }
}

// define directions for LED fade
#define UP 0
#define DOWN 1

// constants for min and max PWM
const int minPWM = 100;

// State Variable for Fade Direction
byte fadeDirection = UP;

// Global Fade Value
// but be bigger than byte and signed, for rollover
int fadeValue = 0;

// How smooth to fade?
const byte fadeIncrement = 20;

// How fast to increment?
const unsigned int fadeInterval = 50;

void doTheFade(uint8_t pin)
{
    if (fadeDirection == UP)
    {
        fadeValue = fadeValue + fadeIncrement;
        if (fadeValue >= PWMRANGE)
        {
            // At max, limit and change direction
            fadeValue = PWMRANGE;
            fadeDirection = DOWN;
        }
    }
    else
    {
        //if we aren't going up, we're going down
        fadeValue = fadeValue - fadeIncrement;
        if (fadeValue <= minPWM)
        {
            // At min, limit and change direction
            fadeValue = minPWM;
            fadeDirection = UP;
        }
    }
    // Only need to update when it changes
    analogWrite(pin, fadeValue);
}

void secTicker()
{
    if (!setupDone)
    {
        doTheFade(LED_BUILTIN);
    }
    else
    {
        time_t t;
        time(&t);
        struct tm *timeinfo = localtime(&t);
        Serial.printf("%04d-%02d-%02d %02d:%02d:%02d\n", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        digitalWrite(LED_BUILTIN, HIGH);

        float tmpTime = timeinfo->tm_hour + (float)timeinfo->tm_min / 100;
        matrix.print(tmpTime);
        matrix.drawColon(true);
        matrix.writeDisplay();
    }
}
