#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include "esp_mac.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <SpotifyArduino.h>
#include <TFT_eSPI.h>
#include <HTTPClient.h>
#include <TJpg_Decoder.h>
#include <lvgl.h>
#include <XPT2046_Touchscreen.h>
#include <time.h>

// TODO: playback widgets + function, offloading to diff processors, refactor

#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK  25
#define XPT2046_CS   33
#define CYD_BACKLIGHT_PIN 21

extern const char* ssid;
extern const char* password;
extern const char* ntpServer;
extern const char* timeZone;

extern const char* clientId;
extern const char* clientSecret;
extern const char* refreshToken;

extern WiFiClientSecure client;
extern SpotifyArduino spotify;
extern TFT_eSPI tft;
extern TFT_eSprite titleSprite;

extern int scrollPos;
extern unsigned long lastScrollTime;
extern const int scrollSpeed;
extern int singleLoopWidth;
extern bool isPlayingState;
extern unsigned long lastClockUpdate;
extern unsigned long lastCheckTime;
extern const unsigned long checkInterval;

extern uint16_t backgroundBlue;
extern uint16_t albumFrame;

extern SPIClass touchSpi;
extern XPT2046_Touchscreen touchscreen;

#endif