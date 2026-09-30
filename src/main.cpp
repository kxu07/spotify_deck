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

const char* ssid = "Device-Northwestern";
const char* password = "";
const char* ntpServer = "pool.ntp.org";
const char* timeZone = "CST6CDT,M3.2.0,M11.1.0";
const char* clientId = "15305a1ef8a8454a9575d0ed2f05e2ca";
const char* clientSecret = "7d637dfb0ba341ff8d8be0342c0dbbc1";
const char* refreshToken = "AQBj92TmuMIZ3VHt9MJkAkUs9EesUR3mA99xAxAORm6JNTVJ7zWGx-4Ktk5tkEmo97k_ZAGFkE43VGWlzoWTcLAc-vweP8vbpaqvqDwmYGVBgepCiXL7mRHg4Y_lCCtoZfE";

WiFiClientSecure client;
SpotifyArduino spotify(client, clientId, clientSecret, refreshToken);
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite titleSprite = TFT_eSprite(&tft);
int scrollPos = 0;
unsigned long lastScrollTime = 0;
const int scrollSpeed = 30;
int singleLoopWidth = 0;
bool isPlayingState = false;
unsigned long lastClockUpdate = 0;

unsigned long lastCheckTime = 0;
const unsigned long checkInterval = 5000;

uint16_t backgroundBlue = tft.color565(179,232,252);
uint16_t albumFrame = tft.color565(8,62,82);

SPIClass touchSpi = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

void setupTouch() {
  touchSpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchSpi);
  touchscreen.setRotation(1); // Match screen rotation
}

void setupTime() {
  configTzTime(timeZone, ntpServer);
  Serial.print("Synchronizing time");
  
  struct tm timeinfo;
  int retryCount = 0;
  const int maxRetries = 10; // 10 attempts * 300ms = 3 second max timeout

  while (!getLocalTime(&timeinfo) && retryCount < maxRetries) {
    Serial.print(".");
    delay(300);
    retryCount++;
  }

  if (retryCount < maxRetries) {
    Serial.println("\nTime synchronized successfully!");
  } else {
    Serial.println("\nNTP sync timed out — clock will sync in background.");
  }
}

void printCurrentTime() {
  struct tm timeinfo;

  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  char timeString[20];
  strftime(timeString, sizeof(timeString), "%I:%M:%S %p", &timeinfo);
  char dateString[20];
  strftime(dateString, sizeof(dateString), "%b %d, %y", &timeinfo);
}

void displayTime() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char timeBuffer[10]; 
    strftime(timeBuffer, sizeof(timeBuffer), "%I:%M %p", &timeinfo);
    tft.setTextSize(1);
    tft.setTextColor(TFT_BLACK, backgroundBlue);
    tft.setTextDatum(TR_DATUM); // Set alignment to Top-Right
    tft.drawString(timeBuffer, 310, 10); // Draw directly relative to X=310
    tft.setTextDatum(TL_DATUM);
  }
}

void drawMediaControls() {
  int btnW = 40;
  int btnH = 35;
  int gap = 20;
  
  // Center horizontally on full 320px screen: (320 - (40*3 + 15*2)) / 2 = 85
  int startX = 80; 
  int startY = 180; // Placed at bottom margin

  for (int i = 0; i < 3; i++) {
    int x = startX + i * (btnW + gap);
    
    // 1. Draw Button Box
    tft.fillRect(x, startY, btnW, btnH, backgroundBlue);
    tft.drawRect(x, startY, btnW, btnH, backgroundBlue);

    // Vertical center offset for 14px icons in a 35px box
    int iconY = startY + (btnH - 14) / 2; // Y = 200

    // 2. Draw Centered Icons
    if (i == 0) {
      // PREVIOUS ICON (|<<) - Total width: 15px
      int iconX = x + (btnW - 15) / 2;
      tft.fillRect(iconX, iconY, 3, 14, TFT_BLACK); // Left bar
      tft.fillTriangle(iconX + 15, iconY, iconX + 15, iconY + 14, iconX + 4, iconY + 7, TFT_BLACK); // Triangle
    } 
    else if (i == 1) {
      // PLAY ICON (>) - Total width: 12px
      int iconX = x + (btnW - 12) / 2;
      tft.fillTriangle(iconX, iconY, iconX, iconY + 14, iconX + 12, iconY + 7, TFT_BLACK);
    } 
    else if (i == 2) {
      // NEXT ICON (>>|) - Total width: 15px
      int iconX = x + (btnW - 15) / 2;
      tft.fillTriangle(iconX, iconY, iconX, iconY + 14, iconX + 11, iconY + 7, TFT_BLACK); // Triangle
      tft.fillRect(iconX + 12, iconY, 3, 14, TFT_BLACK); // Right bar
    }
  }
}

void checkTouch() {
  // Direct pressure check instead of relying on hardware IRQ pin
  if (touchscreen.touched()) {
    TS_Point p = touchscreen.getPoint();

    // Ignore ghost/light touches (minimum pressure threshold)
    if (p.z > 400) { 
      int x = map(p.x, 200, 3700, 1, 320);
      int y = map(p.y, 240, 3800, 1, 240);

      Serial.printf("RAW: X=%d Y=%d Z=%d | MAPPED: X=%d Y=%d\n", p.x, p.y, p.z, x, y);

      if (y >= 190 && y <= 225) {
        if (x >= 85 && x <= 125) {
          Serial.println("Spotify: Skip Previous");
          spotify.previousTrack();
          lastCheckTime = 0;
          delay(300);
        } else if (x >= 140 && x <= 180) {
          if (isPlayingState) {
            Serial.println("Spotify: Pausing...");
            spotify.pause();
            isPlayingState = false; // Optimistically update state
          } else {
            Serial.println("Spotify: Playing...");
            spotify.play();
            isPlayingState = true;  // Optimistically update state
          }
          lastCheckTime = 0;
          delay(300);
        } else if (x >= 195 && x <= 235) {
          Serial.println("Spotify: Skip Next");
          spotify.nextTrack();
          lastCheckTime = 0;
          delay(300);
        }
      }
    }
  }
}

void setupTitleSprite(String trackName) {
  tft.setTextSize(2);
  int textWidth = tft.textWidth(trackName);
  titleSprite.deleteSprite();
  if (textWidth > 140) {
    // need to setup scroll
    singleLoopWidth = textWidth + 10;
    int totalSpriteWidth = (textWidth + 10) * 2;
    titleSprite.createSprite(totalSpriteWidth, 30);
    titleSprite.fillSprite(backgroundBlue);
    titleSprite.setTextColor(TFT_BLACK, backgroundBlue);
    // Draw First Copy
    titleSprite.setTextSize(2);
    titleSprite.drawString(trackName, 0, 0);
    // Draw Second Copy right after the gap
    titleSprite.drawString(trackName, textWidth + 10, 0);
  } else {
    tft.setTextSize(2);
    tft.setTextColor(TFT_BLACK, backgroundBlue);
    tft.setCursor(170,130);
    tft.print(trackName);
  }
  scrollPos = 0;
}

void updateScrollText() {
  if (!titleSprite.created()) return;
  if (millis() - lastScrollTime >= scrollSpeed) {
    lastScrollTime = millis();
    int spriteWidth = titleSprite.width();
    if (spriteWidth > 130) {
      titleSprite.pushSprite(170, 130, scrollPos, 0, 140, 20);
      scrollPos++;
      if (scrollPos >= singleLoopWidth) {
        scrollPos = 0;
      }
      // Match 'gap' value above

    // Reset back to 0 right when the second copy lines up with the start position
    } 
  }
}

void drawAlbumArt(String url, int x, int y) {
  HTTPClient http;
  http.begin(url);

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    WiFiClient *stream = http.getStreamPtr();
    int len = http.getSize();
    if (len > 0) {
      uint8_t* buff = (uint8_t*)malloc(len);
      if (buff) {
        tft.fillRect(10, 10, 152, 152, albumFrame);
        stream->readBytes(buff, len);
        TJpgDec.drawJpg(x, y, buff, len);
        free(buff);
      }
    } else {
      Serial.printf("Failed to download image");
    }
  http.end();
  }
}

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    // Stop rendering if pixels fall off the bottom of the screen
    if (y >= tft.height()) return 0;
    
    // Draw the decoded block of pixels onto screen
    tft.pushImage(x, y, w, h, bitmap);
    return 1;
}

void printCurrentlyPlaying(CurrentlyPlaying currentlyPlaying) {
  static String lastAlbumArtUrl = "";
  static String lastSong = "";
  static bool wasPlaying=false;
  isPlayingState = currentlyPlaying.isPlaying;
  if (currentlyPlaying.isPlaying) {
    String currentArt = currentlyPlaying.albumImages[1].url;
    if (currentArt != lastAlbumArtUrl || !wasPlaying) {
      lastAlbumArtUrl = currentArt;
      wasPlaying = true;
      tft.fillRect(5, 5, 157, 157, backgroundBlue);
      drawAlbumArt(currentArt, 11, 11);
    }
    String currentSong = currentlyPlaying.trackName;
    if (currentSong != lastSong || !wasPlaying) {
      drawMediaControls();
      lastSong = currentSong;
      Serial.println("\n--------------------------------");
      Serial.print("Track:  ");
      Serial.println(currentlyPlaying.trackName);
      Serial.print("Artist:  ");
      Serial.println(currentlyPlaying.artists[0].artistName);
      Serial.print("Album:  ");
      Serial.println(currentlyPlaying.albumName);

      // reset 
      tft.fillRect(165, 120, 155, 45, backgroundBlue);

      tft.setTextSize(2);
      tft.setTextColor(TFT_BLACK, backgroundBlue);
      tft.setCursor(170,130);
      setupTitleSprite(currentlyPlaying.trackName);
      tft.setTextSize(1);
      tft.setCursor(170, 150);
      tft.print(currentlyPlaying.artists[0].artistName);
      displayTime();
    }
  } else if (false) {
    if (wasPlaying) {
      wasPlaying = false;
      lastAlbumArtUrl = "";
      lastSong = "";
    }
    tft.fillScreen(backgroundBlue);
    tft.setTextColor(TFT_BLACK, backgroundBlue);
    titleSprite.deleteSprite();
    Serial.println("Spotify is idle or paused");
    tft.drawCentreString("Spotify not playing", 160, 115, 4);
  }
}

void turnOffBackLight() {
  digitalWrite(CYD_BACKLIGHT_PIN, HIGH);
}

void turnOnBackLight() {
  digitalWrite(CYD_BACKLIGHT_PIN, LOW);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  setupTouch();
  setupTime();
  pinMode(CYD_BACKLIGHT_PIN, OUTPUT);
  digitalWrite(CYD_BACKLIGHT_PIN, LOW);
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_WHITE);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);

  tft.drawCentreString("Connecting to WiFi", 160, 115, 3);

  Serial.println("Start wifi connection");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected!");

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.print("MAC address: ");
  Serial.println(WiFi.macAddress());
  client.setInsecure();
  tft.fillScreen(TFT_WHITE);
  tft.drawCentreString("Connecting to Spotify", 160, 115, 3);

  Serial.println("Fetching Spotify Access Token...");
  if (spotify.refreshAccessToken()) {
    Serial.println("Successfully authenticated with Spotify!");
  } else {
    Serial.println("Failed to get Access Token. Check your credentials.");
  }
  tft.fillScreen(backgroundBlue);
  tft.drawCentreString("Everything connected, booting ...", 160, 115, 3);
  TJpgDec.setJpgScale(2);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tft_output);
}


void loop() {
  checkTouch();
  if (millis() - lastCheckTime > checkInterval) {
    lastCheckTime = millis();
    Serial.println("Retrieving currentlyplaying information");
    int status = spotify.getCurrentlyPlaying(printCurrentlyPlaying);
  
    if (status != 200 && status != 204) {
      Serial.print("HTTP Error Code: ");
      Serial.println(status);
    }
  }
  if (millis() - lastClockUpdate >= 10000) {
    lastClockUpdate = millis();
    displayTime();
  }
  updateScrollText();

}