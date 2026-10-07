#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "playback.h"

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

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    // Stop rendering if pixels fall off the bottom of the screen
    if (y >= tft.height()) return 0;
    
    // Draw the decoded block of pixels onto screen
    tft.pushImage(x, y, w, h, bitmap);
    return 1;
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
  tft.setTextColor(TFT_BLACK);
  tft.drawCentreString("syncing time", 160, 115, 3);
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
  if (isPlayingState && (millis() - lastProgressUpdate >= 1000)) {
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