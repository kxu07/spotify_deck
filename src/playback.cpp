#include <Arduino.h>
#include "config.h"
#include "playback.h"

extern void displayTime();

void updatePlayButton() {
  int btnW = 40;
  int btnH = 35;
  int gap = 20;
  int startX = 80; 
  int startY = 170;
  int iconY = startY + (btnH - 14) / 2; // Y = 200
  int x = startX + (btnW + gap);
  tft.fillRect(x, startY, btnW, btnH, backgroundBlue);
  tft.drawRect(x, startY, btnW, btnH, backgroundBlue);
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
    titleSprite.drawString(trackName, textWidth + 15, 0);
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