#include <Arduino.h>
#include "esp_mac.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <SpotifyArduino.h>
#include <TFT_eSPI.h>
#include <HTTPClient.h>
#include <TJpg_Decoder.h>

const char* ssid = "Device-Northwestern";
const char* password = "";
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

unsigned long lastCheckTime = 0;
const unsigned long checkInterval = 5000;

uint16_t backgroundBlue = tft.color565(179,232,252);

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
        tft.fillRect(10, 10, 152, 152, TFT_BLACK);
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
  if (currentlyPlaying.isPlaying) {
    String currentArt = currentlyPlaying.albumImages[1].url;
    if (currentArt != lastAlbumArtUrl || !wasPlaying) {
      lastAlbumArtUrl = currentArt;
      wasPlaying = true;
      tft.fillScreen(backgroundBlue);
      drawAlbumArt(currentArt, 11, 11);
    }
    String currentSong = currentlyPlaying.trackName;
    if (currentSong != lastSong || !wasPlaying) {
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
      tft.print(currentlyPlaying.trackName);
      tft.setTextSize(1);
      tft.setCursor(170, 150);
      tft.print(currentlyPlaying.artists[0].artistName);
    }
  } else {
    if (wasPlaying) {
      wasPlaying = false;
      lastAlbumArtUrl = "";
    }
    tft.fillScreen(backgroundBlue);
    tft.setTextColor(TFT_BLACK, backgroundBlue);
    Serial.println("Spotify is idle or paused");
    tft.drawCentreString("Spotify not playing", 160, 115, 4);
  }
}


void setup() {
  Serial.begin(115200);
  delay(1000);

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
  tft.fillScreen(TFT_WHITE);
  tft.drawCentreString("Everything connected, booting ...", 160, 115, 3);
  TJpgDec.setJpgScale(2);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tft_output);
}

void loop() {
  if (millis() - lastCheckTime > checkInterval) {
    lastCheckTime = millis();
    Serial.println("Retrieving currentlyplaying information");
    int status = spotify.getCurrentlyPlaying(printCurrentlyPlaying);

    if (status != 200 && status != 204) {
      Serial.print("HTTP Error Code: ");
      Serial.println(status);
    }
  }
}