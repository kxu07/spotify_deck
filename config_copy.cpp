#include "config.h"

// WiFi & NTP Configuration
const char* ssid = "Device-Northwestern";
const char* password = "";
const char* ntpServer = "pool.ntp.org";
const char* timeZone = "CST6CDT,M3.2.0,M11.1.0";

// Spotify Credentials put your own here
const char* clientId = "";
const char* clientSecret = "";
const char* refreshToken = "";

// Global Objects
WiFiClientSecure client;
SpotifyArduino spotify(client, clientId, clientSecret, refreshToken);
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite titleSprite = TFT_eSprite(&tft);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

// Theme Colors
uint16_t backgroundBlue = 0xB7FC; // Pre-calculated color565(179, 232, 252)
uint16_t albumFrame     = 0x0BEA; // Pre-calculated color565(8, 62, 82)

// Global State
unsigned long lastCheckTime = 0;
const unsigned long checkInterval = 5000;
unsigned long lastClockUpdate = 0;
bool isPlayingState = false;