# Spotify Cyberdeck: An ESP32 CYD display for music-lovers


Here is a project that I made for making a neat, little cyberdeck that is can display the details of what you're currently listening to on Spotify and that can control playback. I'm using a ESP32 CYD with a 2.8 inch display for hardware, and I'm using PlatformIO in VSCode to write software. I'm using the Spotify API and the ESP32's WiFi capabilities to pull data via API call. Currently, I have implemented the display functions and playback buttons, and I have the UI and API calls on different cores so that everything runs a little smoother. 
