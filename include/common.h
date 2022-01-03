/**
 * @file common.h
 * @brief Common defines.
 *
 * @copyright Copyright (c) 2021
 *
 */
#ifndef COMMON_H
#define COMMON_H

#include <Arduino.h>
#include <WebServer.h>
#include <Adafruit_LEDBackpack.h>
#include <Ticker.h>

void handleRoot();
void handleNotFound();
void secTicker();
void setupWebServer();
void setupOta();

extern WebServer webServer;
extern Adafruit_7segment matrix;
extern Ticker ticker;

#endif // COMMON_H
