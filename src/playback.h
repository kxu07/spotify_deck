#ifndef PLAYBACK_UTILS_H
#define PLAYBACK_UTILS_H

void updatePlayButton();
void drawMediaControls();
void drawProgressBar();
void setupTitleSprite(String trackName);
void updateScrollText();
void drawAlbumArt(String url, int x, int y);
void printCurrentlyPlaying(CurrentlyPlaying currentlyPlaying);

#endif