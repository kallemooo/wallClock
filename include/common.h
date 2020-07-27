/**
 * @file common.h
 * @brief Common defines.
 *
 * @copyright Copyright (c) 2020
 *
 */
#ifndef COMMON_H
#define COMMON_H

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <Adafruit_LEDBackpack.h>

void handleRoot();
void handleNotFound();
void secTicker();
void setupWebServer();
void setupOta();

extern ESP8266WebServer webServer;
extern Adafruit_7segment matrix;

extern String otaMsgL1;
extern String otaMsgL2;
extern String otaMsgL3;

#endif // COMMON_H
