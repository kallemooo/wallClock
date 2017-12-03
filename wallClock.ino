#include <FS.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>
#include <Ticker.h>
#include <time.h>
#include <simpleDSTadjust.h>
#include <Wire.h> // Enable this line if using Arduino Uno, Mega, etc.
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"

Adafruit_7segment matrix = Adafruit_7segment();
ESP8266WebServer server(80);

static const int led_blue = 13;
static const int led_red = 15;
static const int led_green = 12;

void handleNotFound(){
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

char ntp_server1[40] = "ntp1.sptime.se";
char ntp_server2[40] = "ntp2.sptime.se";
char ntp_server3[40] = "pool.ntp.org";

// -------------- Configuration options -----------------

// Update time from NTP server every 5 hours
#define NTP_UPDATE_INTERVAL_SEC 5*3600

// Daylight Saving Time (DST) rule configuration
// Rules work for most contries that observe DST - see https://en.wikipedia.org/wiki/Daylight_saving_time_by_country for details and exceptions
// See http://www.timeanddate.com/time/zones/ for standard abbreviations and additional information
// Caution: DST rules may change in the future

//Central European Time zone
#define timezone +1 // Central European Time zone
struct dstRule StartRule = {"CEST", Last, Sun, Mar, 2, 3600};    // Daylight time = UTC/GMT +2 hours
struct dstRule EndRule = {"CET", Last, Sun, Oct, 2, 0};       // Standard time = UTC/GMT +1 hour

// --------- End of configuration section ---------------

Ticker ticker;
int32_t tick;

// flag changed in the ticker function to start NTP Update
bool readyForNtpUpdate = false;

// Setup simpleDSTadjust Library rules
simpleDSTadjust dstAdjusted(StartRule, EndRule);

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback ()
{
  shouldSaveConfig = true;
}

void ledTick()
{
  //toggle state
  int state = digitalRead(led_red);  // get the current state of GPIO1 pin
  digitalWrite(led_red, !state);     // set pin to the opposite state
}

//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager)
{
  // Force to AP mode only as AP + STA do not work.
  WiFi.mode(WIFI_AP);
  Serial.println(F("Entered config mode"));
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  //entered config mode, make led toggle faster
  ticker.attach(0.2, ledTick);
}

void setup()
{
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  Serial.println(F("Start"));

  Serial.print(F("Default hostname: "));
  Serial.println(WiFi.hostname());
  WiFi.hostname("wallClock");
  Serial.print(F("New hostname: "));
  Serial.println(WiFi.hostname());

  matrix.begin(0x70);
  
  //set led pin as output
  pinMode(led_red,  OUTPUT);
  pinMode(led_green, OUTPUT);
  pinMode(led_blue, OUTPUT);
  // start ticker with 0.6 because we start in AP mode and try to connect
  ticker.attach(0.6, ledTick);

  //clean FS, for testing
  //SPIFFS.format();

  //read configuration from FS json
  Serial.println(F("mounting FS..."));

  if (SPIFFS.begin())
  {
    Serial.println(F("mounted file system"));
    if (SPIFFS.exists(F("/config.json")))
    {
      //file exists, reading and loading
      Serial.println(F("reading config file"));
      File configFile = SPIFFS.open(F("/config.json"), "r");
      if (configFile)
      {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success())
        {
          Serial.println(F("\nparsed json"));

          strcpy(ntp_server1, json["ntp_server1"]);
          strcpy(ntp_server2, json["ntp_server2"]);
          strcpy(ntp_server3, json["ntp_server3"]);
        }
        else
        {
          Serial.println(F("failed to load json config"));
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
      shouldSaveConfig = true;
    }
  }
  else
  {
    Serial.println(("failed to mount FS"));
  }
  
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  
  //reset settings - for testing
  //wifiManager.resetSettings();
  
  wifiManager.setConfigPortalTimeout(180);
  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);

  WiFiManagerParameter custom_ntp_server1("NTP1", "server1", ntp_server1, 40, " NTP server 1");
  WiFiManagerParameter custom_ntp_server2("NTP2", "server2", ntp_server2, 40, " NTP server 2");
  WiFiManagerParameter custom_ntp_server3("NTP3", "server3", ntp_server3, 40, " NTP server 3");

  wifiManager.addParameter(&custom_ntp_server1);
  wifiManager.addParameter(&custom_ntp_server2);
  wifiManager.addParameter(&custom_ntp_server3);
  
  //tries to connect to last known settings
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP" with password "password"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("wallClockAp", "password")) {
    Serial.println(F("failed to connect, we should reset as see if it connects"));
    delay(500);
    ESP.reset();
    delay(1000);
  }

  //if you get here you have connected to the WiFi
  Serial.println(F("connected!"));

  //read updated parameters
  strcpy(ntp_server1, custom_ntp_server1.getValue());
  strcpy(ntp_server2, custom_ntp_server2.getValue());
  strcpy(ntp_server3, custom_ntp_server3.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig)
  {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["ntp_server1"] = ntp_server1;
    json["ntp_server2"] = ntp_server2;
    json["ntp_server3"] = ntp_server3;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile)
    {
      Serial.println(F("failed to open config file for writing"));
    }

    json.printTo(Serial);
    Serial.println(""); 
    json.printTo(configFile);
    configFile.close();
    //end save
  }
    
  tick = NTP_UPDATE_INTERVAL_SEC; // Init the NTP update countdown ticker

  updateNTP(); // Init the NTP time
  printTime(0); // print initial time time now.

  ticker.detach();
  ticker.attach(1, secTicker); // Run a 1 second interval Ticker

  if (MDNS.begin("wallClock")) {
    Serial.println(F("MDNS responder started"));
  }

  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println(F("HTTP server started"));

  digitalWrite(led_red, LOW);
  digitalWrite(led_green, HIGH);
  Serial.println("Done");
}

void loop()
{
  if(readyForNtpUpdate)
  {
    printTime(0);
    updateNTP();
    readyForNtpUpdate = false;
  }

  server.handleClient();
}

//----------------------- Functions -------------------------------

// NTP timer update ticker
void secTicker()
{
  tick--;
  if(tick<=0)
  {
    tick = NTP_UPDATE_INTERVAL_SEC; // Re-arm
    readyForNtpUpdate = true;
  }
  if (!readyForNtpUpdate)
  {
    updateDisplay();
  }
}

void updateNTP()
{
  bool updateTimeOk = false;
  int cnt = 10;

  Serial.print(F("\nTrying to update time from NTP Server "));
  configTime(timezone * 3600, 0, ntp_server1, ntp_server2, ntp_server3);
  delay(500);

  while (!updateTimeOk && (cnt > 0))
  {
    updateTimeOk = time(nullptr);
    if (!updateTimeOk)
    {
      Serial.print("#");
      delay(1000);      
      cnt--;
    }
  }

  if (updateTimeOk)
  {
    Serial.print(F("\nUpdated time from NTP Server: "));
    printTime(0);
  }
  else
  {
    Serial.print(F("\nFailed to update time from NTP Server"));
    tick = 60; // Re-arm to test again within a minute.
  }
  Serial.print(F("\nNext NTP Update: "));
  printTime(tick);
}

void handleRoot()
{
  String msg = "DST Rules";
  char buf[30];
  char *dstAbbrev;
  time_t t = dstAdjusted.time(&dstAbbrev);
  struct tm *timeinfo = localtime (&t);
  
  sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d %s\n", timeinfo->tm_year+1900, timeinfo->tm_mon+1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, dstAbbrev);


  String page = FPSTR(HTTP_HEAD);
  page.replace("{v}", "Wall clock status");
  page += FPSTR(HTTP_STYLE);
  page += FPSTR(HTTP_HEAD_END);
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
  page += FPSTR(HTTP_END);

  server.send(200, "text/html", page);
}

void printTime(time_t offset)
{
  char buf[30];
  char *dstAbbrev;
  time_t t = dstAdjusted.time(&dstAbbrev)+offset;
  struct tm *timeinfo = localtime (&t);
  
  sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d %s\n", timeinfo->tm_year+1900, timeinfo->tm_mon+1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, dstAbbrev);
  Serial.print(buf);
}

void updateDisplay()
{
  char buf[30];
  char *dstAbbrev;
  time_t t = dstAdjusted.time(&dstAbbrev);
  struct tm *timeinfo = localtime (&t);
  
  float tmpTime = timeinfo->tm_hour + (float)timeinfo->tm_min / 100;

  //char str_temp[6];
  //dtostrf(tmpTime, 2, 2, str_temp);
  //sprintf(buf, "Time: '%s'", str_temp);
  //Serial.println(buf);
  matrix.print(tmpTime);
  matrix.drawColon(true);
  matrix.writeDisplay();
}

