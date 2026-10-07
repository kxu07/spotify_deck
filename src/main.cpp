#include <Arduino.h>
#include <WiFi.h>
#include "config.h"

// TODO: playback widgets + function, offloading to diff processors, refactor

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
  const int maxRetries = 8; // 8 attempts * 300ms 

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
    char dateBuffer[20];
    strftime(dateBuffer, sizeof(dateBuffer), "%a, %b %d", &timeinfo);
    tft.drawString(dateBuffer, 310, 20);
    tft.setTextDatum(TL_DATUM);
  }
}

void updatePlayButton() {
  int btnW = 40;
  int btnH = 35;
  int gap = 20;
  int startX = 80; 
  int startY = 170;
  int iconY = startY + (btnH - 14) / 2; // Y = 200
  int x = startX + (btnW + gap);
  if (isPlayingState == false) {
    int iconX = x + (btnW - 12) / 2;
    tft.fillTriangle(iconX, iconY, iconX, iconY + 14, iconX + 12, iconY + 7, TFT_BLACK);
  } else {
    int barW = 4;
    int barGap = 4;
    int iconX = x + (btnW - (barW*2+barGap)) / 2;
    tft.fillRect(iconX, iconY, barW, 14, TFT_BLACK);
    tft.fillRect(iconX+barW+barGap, iconY, barW, 14, TFT_BLACK);
  }
}

void drawMediaControls() {
  int btnW = 40;
  int btnH = 35;
  int gap = 20;
  
  // Center horizontally on full 320px screen: (320 - (40*3 + 15*2)) / 2 = 85
  int startX = 80; 
  int startY = 170; // Placed at bottom margin

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
      updatePlayButton();
    } 
    else if (i == 2) {
      // NEXT ICON (>>|) - Total width: 15px
      int iconX = x + (btnW - 15) / 2;
      tft.fillTriangle(iconX, iconY, iconX, iconY + 14, iconX + 11, iconY + 7, TFT_BLACK); // Triangle
      tft.fillRect(iconX + 12, iconY, 3, 14, TFT_BLACK); // Right bar
    }
  }
}

void drawProgressBar() {
  if (durationMs <= 0) return;

  // Calculate local elapsed progress since last API fetch
  long currentProgress = progressMs;
  if (isPlayingState) {
    currentProgress += (millis() - lastProgressUpdate);
  }

  // Cap currentProgress at durationMs
  if (currentProgress > durationMs) {
    currentProgress = durationMs;
  }

  // UI Dimensions for Progress Bar
  int barX = 20;
  int barY = 213;
  int barWidth = 280;
  int barHeight = 4;

  // Calculate filled pixel width based on percentage ratio
  int fillWidth = map(currentProgress, 0, durationMs, 0, barWidth);

  // 1. Draw Background Track (Gray)
  tft.fillRect(barX, barY, barWidth, barHeight, TFT_DARKGREY);

  // 2. Draw Active Fill Line (White or Spotify Green)
  if (fillWidth > 0) {
    tft.fillRect(barX, barY, fillWidth, barHeight, TFT_WHITE);
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

      if (y >= 170 && y <= 205) {
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

void checkOvernightSleep() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return ;
  int currentHour = timeinfo.tm_hour;
  const int SLEEP_START = 23;
  const int SLEEP_END = 8;
  bool isOvernight = (currentHour >= SLEEP_START || currentHour < SLEEP_END);
  if (isOvernight) {
    int targetHour = SLEEP_END;
    if (currentHour >= SLEEP_START) {
      targetHour += 24;
    }
    int hoursToSleep   = targetHour - currentHour - 1;
    int minutesToSleep = 59 - timeinfo.tm_min;
    int secondsToSleep = 60 - timeinfo.tm_sec;

    uint64_t totalSleepSeconds = (hoursToSleep * 3600) + (minutesToSleep * 60) + secondsToSleep;

    Serial.printf("Sleeping for %llu seconds (%llu hours)...\n", 
                  totalSleepSeconds, totalSleepSeconds / 3600);

    // 2. Shut down CYD display & backlight
    digitalWrite(CYD_BACKLIGHT_PIN, HIGH); // Turn off backlight (Active LOW)
    tft.writecommand(0x10);                // SPI display sleep mode command (ST7789/ILI9341)

    // 3. Configure ESP32 timer wakeup and enter deep sleep
    esp_sleep_enable_timer_wakeup(totalSleepSeconds * 1000000ULL);
    esp_deep_sleep_start();
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
  updatePlayButton();
  if (currentlyPlaying.isPlaying) {
    String currentArt = currentlyPlaying.albumImages[1].url;
    progressMs = currentlyPlaying.progressMs;
    durationMs = currentlyPlaying.durationMs;
    lastProgressUpdate = millis();
    if (currentArt != lastAlbumArtUrl || !wasPlaying) {
      lastAlbumArtUrl = currentArt;
      wasPlaying = true;
      tft.fillRect(0, 5, 162, 163, backgroundBlue);
      drawAlbumArt(currentArt, 11, 11);
    }
    String currentSong = currentlyPlaying.trackName;
    if (currentSong != lastSong || !wasPlaying) {
      updatePlayButton();
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
      drawProgressBar();
    }
  } else  {
    isPlayingState = false;
    updatePlayButton();
    if (false) {
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
  tft.fillScreen(TFT_BLUE);
  setupTouch();
  pinMode(CYD_BACKLIGHT_PIN, OUTPUT);
  digitalWrite(CYD_BACKLIGHT_PIN, LOW);
  tft.init();
  tft.setRotation(1);
  setupTime();
  tft.fillScreen(TFT_WHITE);
  tft.setTextColor(TFT_BLACK, TFT_BLACK);
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
  drawMediaControls();
}


void loop() {
  checkTouch();
  if (millis() - lastCheckTime > checkInterval) {
    lastCheckTime = millis();
    Serial.println("Retrieving currentlyplaying information");
    int status = spotify.getCurrentlyPlaying(printCurrentlyPlaying);
    if (status == 401 || status < 0) {
      Serial.println("Token expired");
      if (spotify.refreshAccessToken()) {
        Serial.println("Token refreshed");
      }
    }
    if (status != 200 && status != 204) {
      Serial.print("HTTP Error Code: ");
      Serial.println(status);
    }
  }
  if (isPlayingState && (millis() - lastProgressUpdate >= 1500)) {
    lastProgressUpdate = millis();
    drawProgressBar();
  }
  if (millis() - lastClockUpdate >= 10000) {
    lastClockUpdate = millis();
    displayTime();
    checkOvernightSleep();
  }
  updateScrollText();

}