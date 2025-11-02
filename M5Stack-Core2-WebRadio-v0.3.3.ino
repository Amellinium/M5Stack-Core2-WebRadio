/*
   November 02, 2025 
   M5Stack-Core2-WebRadio-v0.3.3 by Amellinium
   Based on m5StreamTest Version 2020.12b by tommyho510@gmail.com
   and M5Stack Core2 Web Media Player modified from other sources 
   by bwbguard
   Board: M5StackCore2 (esp32)

   REQUIRED LIBRARIES:
   - M5Core2
   - ESP8266Audio (for MP3 decoding & streaming)
   - WiFi
   - Preferences (for saving WiFi credentials)
   - SD
   - Free_Fonts.h (included in M5Core2 examples)
*/

#include <M5Core2.h>                  // Core2 library: touchscreen, buttons, AXP192 power
#include <driver/i2s.h>               // I2S driver for audio output (uses internal DAC)
#include <WiFi.h>                     // WiFi connectivity in station mode
#include <AudioFileSourceICYStream.h> // Reads internet radio stream + ICY metadata
#include <AudioFileSourceBuffer.h>    // Buffers stream to prevent dropouts
#include <AudioGeneratorMP3.h>        // Decodes MP3 audio
#include <AudioOutputI2S.h>           // Sends decoded audio to I2S (speaker)
#include <Preferences.h>              // Saves WiFi SSID/password in flash
#include <SD.h>                       // Reads station list from SD card
#include "Free_Fonts.h"               // Custom fonts: FSS9 (small), FSSB9 (bold)

// ==================================================================
// =========================== CONSTANTS =============================
// ==================================================================

const int bufferSize = 256 * 1024;    // 256 KB buffer → smooth streaming even on weak WiFi
Preferences preferences;              // Object to store WiFi credentials in NVS (flash)

// ==================================================================
// =========================== GLOBAL VARIABLES =====================
// ==================================================================

float audioGain = 5.0;                // Current volume level (0.0 = silent, 10.0 = max)
float gainfactor = 0.05;              // Converts audioGain → I2S gain: 5.0 * 0.05 = 0.25 (25% volume)
int currentStationNumber = 0;         // Index of currently playing station (0 to stationCount-1)
unsigned long disUpdate = millis();   // Timestamp: when display was last refreshed
unsigned long audioCheck = millis();  // Timestamp: when audio buffer was last checked
unsigned long lastStationChange = 0;  // Timestamp: when user last changed station
bool isScrollingStations = false;     // True = user is switching stations (debounce)
float lastAudioGain = -1.0;           // Last volume used for meter update
float lastBatteryLevel = -1.0;        // Last battery % used for meter update
unsigned long lastMeterUpdate = 0;    // Timestamp: when meters were last redrawn
int lastDisplayedStation = -1;        // Last station number shown (avoids redraw)
unsigned long lastLedToggle = 0;      // Timestamp: when power LED last blinked
bool ledState = false;                // Power LED on/off state

// MUTE SYSTEM
bool muted = false;                   // Is audio currently muted?
float savedGain = audioGain;          // Stores volume before mute (so we can restore)

// AUDIO PIPELINE OBJECTS (dynamically created/destroyed)
AudioGeneratorMP3 *mp3 = nullptr;           // MP3 decoder object
AudioFileSourceICYStream *filemp3 = nullptr;// Stream source (URL)
AudioFileSourceBuffer *buffmp3 = nullptr;   // Buffer for stream
AudioOutputI2S *outmp3 = nullptr;           // I2S output to speaker

#define SOFT_BLUE 0x3DAF              // Custom UI color: soft blue for buttons and bars

// STATION LIST: up to 50 entries stored as [Name, URL]
String stationList[50][2];            // stationList[i][0] = name, [i][1] = URL
int stationCount = 0;                 // How many stations were loaded from SD

// WIFI SETUP VARIABLES
String selectedSSID = "";             // SSID user selected
String selectedPassword = "";         // Password entered
String streamBitrate = "128 kbps";    // Static text (not real-time)
String streamSampleRate = "44.1 kHz"; // Static text

// ==================================================================
// =========================== SD CARD FUNCTIONS =====================
// ==================================================================

/*
  loadStationsFromSD()
  --------------------
  Opens /stations_list.txt on SD card
  Format: Station Name,http://stream.url:port/path
  Example:
    BBC Radio 1,http://bbc.co.uk/radio1.mp3
    Jazz FM,http://stream.jazzfm.com/live
  Loads up to 50 stations into stationList[][]
*/
void loadStationsFromSD() {
  File file = SD.open("/stations_list.txt", FILE_READ);  // Try to open file
  if (!file) {
    Serial.println("ERROR: Failed to open /stations_list.txt on SD card");
    stationCount = 0;  // No stations available
    return;
  }

  stationCount = 0;  // Reset count
  while (file.available() && stationCount < 50) {
    String line = file.readStringUntil('\n');  // Read one line
    line.trim();  // Remove whitespace
    if (line.length() == 0) continue;  // Skip empty lines

    int commaIndex = line.indexOf(',');  // Find separator
    if (commaIndex == -1) continue;  // Invalid line

    // Split into name and URL
    stationList[stationCount][0] = line.substring(0, commaIndex);      // Name
    stationList[stationCount][1] = line.substring(commaIndex + 1);     // URL
    stationList[stationCount][0].trim();  // Clean whitespace
    stationList[stationCount][1].trim();
    stationCount++;  // Valid station → increment
  }
  file.close();  // Always close file
  Serial.printf("SUCCESS: Loaded %d radio stations from SD card\n", stationCount);
}

// ==================================================================
// =========================== WIFI FUNCTIONS =========================
// ==================================================================

/*
  initWiFi()
  ----------
  Called at startup. Tries to use saved WiFi credentials.
  If none exist → shows setup menu.
*/
void initWiFi() {
  preferences.begin("wifi-config", false);  // Open NVS storage
  if (preferences.isKey("ssid") && preferences.isKey("password")) {
    selectedSSID = preferences.getString("ssid", "");      // Load saved SSID
    selectedPassword = preferences.getString("password", ""); // Load saved password
    connectToWiFi(selectedSSID.c_str(), selectedPassword.c_str());  // Try auto-connect
  } else {
    wifiSetupMenu();  // First-time setup
  }
}

/*
  connectToWiFi()
  ---------------
  Attempts to connect using given SSID and password.
  Shows "Connecting..." on screen.
  Saves credentials on success.
*/
void connectToWiFi(const char *ssid, const char *password) {
  M5.Lcd.clear();  // Clear screen
  M5.Lcd.setTextColor(SOFT_BLUE, TFT_BLACK);
  M5.Lcd.setFreeFont(FSS9);
  M5.Lcd.setTextDatum(TC_DATUM);  // Center text
  M5.Lcd.drawString("Connecting to " + String(ssid), 160, 120, GFXFF);  // Show message

  WiFi.disconnect();  // Ensure clean start
  WiFi.mode(WIFI_STA);  // Station mode
  WiFi.begin(ssid, password);  // Start connection

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 15) {
    delay(1000);  // Wait 1 second
    attempts++;
    Serial.print(".");  // Progress in Serial
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    preferences.putString("ssid", ssid);      // Save for next boot
    preferences.putString("password", password);
  } else {
    Serial.println("\nWiFi Connection Failed!");
    wifiSetupMenu();  // Retry setup
  }
}

/*
  wifiSetupMenu()
  ---------------
  Scans for WiFi networks and shows first 5.
  User touches one → goes to password entry.
*/
void wifiSetupMenu() {
  M5.Lcd.clear();
  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Lcd.setFreeFont(FSS9);
  M5.Lcd.setTextDatum(TC_DATUM);
  M5.Lcd.drawString("WiFi Setup", 160, 20, GFXFF);

  int n = WiFi.scanNetworks();  // Scan
  if (n == 0) {
    M5.Lcd.drawString("No networks found", 160, 120, GFXFF);
    delay(2000);
    return;
  }

  int y = 50;
  for (int i = 0; i < min(n, 5); i++) {
    M5.Lcd.drawString(String(i + 1) + ": " + WiFi.SSID(i), 160, y, GFXFF);
    y += 20;
  }
  M5.Lcd.drawString("Touch to select", 160, y + 10, GFXFF);

  while (true) {
    M5.update();  // Update touch
    if (M5.Touch.ispressed()) {
      TouchPoint_t pos = M5.Touch.getPressPoint();
      int selected = (pos.y - 50) / 20;  // Which line touched?
      if (selected >= 0 && selected < min(n, 5)) {
        selectedSSID = WiFi.SSID(selected);  // Store selection
        showKeyboard();  // Go to password entry
        break;
      }
    }
    delay(100);
  }
}

/*
  showKeyboard()
  --------------
  Full on-screen keyboard for password entry.
  Includes Shift, Backspace, Exit, Done.
*/
void showKeyboard() {
  M5.Lcd.clear();
  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Lcd.setFreeFont(FSS9);
  M5.Lcd.setTextDatum(TC_DATUM);
  M5.Lcd.drawString("Password for " + selectedSSID, 160, 10, GFXFF);

  String password = "";
  M5.Lcd.fillRect(20, 30, 280, 25, TFT_DARKGREY);  // Password field
  M5.Lcd.drawRect(20, 30, 280, 25, TFT_WHITE);
  M5.Lcd.setTextColor(TFT_WHITE, TFT_DARKGREY);
  M5.Lcd.drawString(password, 160, 34, GFXFF);

  int y = 80;
  int keyWidth = 32, keyHeight = 32, keySpacing = 2;
  bool shift = false;
  const char *keyboardLower[4] = {"1234567890", "qwertyuiop", "asdfghjkl ", "zxcvbnm_<-"};
  const char *keyboardUpper[4] = {"1234567890", "QWERTYUIOP", "ASDFGHJKL ", "ZXCVBNM_<-"};
  const char **currentKeyboard = keyboardLower;

  // Draw all keys
  for (int row = 0; row < 4; row++) {
    int len = strlen(currentKeyboard[row]);
    for (int col = 0; col < len; col++) {
      int x = col * keyWidth;
      M5.Lcd.fillRect(x, y, keyWidth - 2, keyHeight - 2, TFT_LIGHTGREY);
      M5.Lcd.drawRect(x, y, keyWidth - 2, keyHeight - 2, TFT_WHITE);
      M5.Lcd.setTextColor(TFT_BLACK, TFT_LIGHTGREY);
      M5.Lcd.setTextDatum(TC_DATUM);
      M5.Lcd.drawChar(currentKeyboard[row][col], x + (keyWidth / 2), y + (keyHeight / 2) - 1, GFXFF);
    }
    y += keyHeight + keySpacing;
  }

  // Bottom buttons: Exit, Shift, Done
  int bottomY = 215, btnWidth = 106, btnHeight = 25, stripWidth = 2;
  M5.Lcd.fillRect(0, bottomY, 320, btnHeight, TFT_BLACK);
  M5.Lcd.fillRect(0, bottomY, btnWidth, btnHeight, SOFT_BLUE);
  M5.Lcd.setTextColor(TFT_WHITE, SOFT_BLUE);
  M5.Lcd.setFreeFont(FSSB9);
  M5.Lcd.setTextDatum(TC_DATUM);
  M5.Lcd.drawString("Exit", btnWidth / 2, bottomY + 5, GFXFF);
  M5.Lcd.fillRect(btnWidth, bottomY, stripWidth, btnHeight, TFT_BLACK);
  M5.Lcd.fillRect(btnWidth + stripWidth, bottomY, btnWidth, btnHeight, SOFT_BLUE);
  M5.Lcd.drawString("Shift", btnWidth + stripWidth + btnWidth / 2, bottomY + 5, GFXFF);
  M5.Lcd.fillRect(btnWidth * 2 + stripWidth, bottomY, stripWidth, btnHeight, TFT_BLACK);
  M5.Lcd.fillRect(btnWidth * 2 + stripWidth * 2, bottomY, btnWidth, btnHeight, SOFT_BLUE);
  M5.Lcd.drawString("Done", btnWidth * 2 + stripWidth * 2 + btnWidth / 2, bottomY + 5, GFXFF);

  while (true) {
    M5.update();
    if (M5.Touch.ispressed()) {
      TouchPoint_t pos = M5.Touch.getPressPoint();

      // Key press
      if (pos.y >= 80 && pos.y < (80 + 4 * (keyHeight + keySpacing))) {
        int row = (pos.y - 80) / (keyHeight + keySpacing);
        int len = strlen(currentKeyboard[row]);
        int col = pos.x / keyWidth;
        if (col >= 0 && col < len) {
          char c = currentKeyboard[row][col];
          if (c == '<') {
            if (password.length() > 0) password.remove(password.length() - 1);
          } else if (c != ' ') {
            password += c;
          }
          M5.Lcd.fillRect(20, 30, 280, 25, TFT_DARKGREY);
          M5.Lcd.drawRect(20, 30, 280, 25, TFT_WHITE);
          M5.Lcd.setTextColor(TFT_WHITE, TFT_DARKGREY);
          M5.Lcd.drawString(password, 160, 37, GFXFF);
        }
      }

      // Bottom buttons
      if (pos.y >= bottomY && pos.y <= (bottomY + btnHeight)) {
        if (pos.x < btnWidth) {
          M5.Lcd.clear();
          drawButtons();
          displayWiFiInformation();
          return;
        } else if (pos.x >= (btnWidth + stripWidth) && pos.x < (btnWidth * 2 + stripWidth)) {
          shift = !shift;
          currentKeyboard = shift ? keyboardUpper : keyboardLower;
          y = 80;
          M5.Lcd.fillRect(0, 80, 320, 4 * (keyHeight + keySpacing), TFT_BLACK);
          for (int r = 0; r < 4; r++) {
            int len = strlen(currentKeyboard[r]);
            for (int c = 0; c < len; c++) {
              int x = c * keyWidth;
              M5.Lcd.fillRect(x, y, keyWidth - 2, keyHeight - 2, TFT_LIGHTGREY);
              M5.Lcd.drawRect(x, y, keyWidth - 2, keyHeight - 2, TFT_WHITE);
              M5.Lcd.setTextColor(TFT_BLACK, TFT_LIGHTGREY);
              M5.Lcd.setTextDatum(TC_DATUM);
              M5.Lcd.drawChar(currentKeyboard[r][c], x + (keyWidth / 2), y + (keyHeight / 2) - 1, GFXFF);
            }
            y += keyHeight + keySpacing;
          }
        } else if (pos.x >= (btnWidth * 2 + stripWidth * 2)) {
          selectedPassword = password;
          connectToWiFi(selectedSSID.c_str(), selectedPassword.c_str());
          break;
        }
      }
      delay(300);  // Debounce
    }
  }
}

/*
  wifiMenu()
  ----------
  Shows current WiFi or lets user change it.
*/
void wifiMenu() {
  M5.Lcd.clear();
  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Lcd.setFreeFont(FSS9);
  M5.Lcd.setTextDatum(TC_DATUM);
  M5.Lcd.drawString("WiFi Menu", 160, 20, GFXFF);
  M5.Lcd.drawString("1. Current: " + selectedSSID, 160, 60, GFXFF);
  M5.Lcd.drawString("2. Change WiFi", 160, 80, GFXFF);
  M5.Lcd.drawString("Touch to select", 160, 100, GFXFF);

  while (true) {
    M5.update();
    if (M5.Touch.ispressed()) {
      TouchPoint_t pos = M5.Touch.getPressPoint();
      if (pos.y > 70 && pos.y < 90) {
        wifiSetupMenu();
        break;
      } else if (pos.y > 50 && pos.y < 70) {
        break;
      }
    }
    delay(100);
  }
  M5.Lcd.clear();
  drawButtons();
  displayWiFiInformation();
}

void displayWiFiInformation() {}  // Placeholder
void updateWiFiSignal() {}        // Placeholder

// ==================================================================
// =========================== DISPLAY FUNCTIONS =====================
// ==================================================================

/*
  displayBattery()
  ----------------
  Updates battery, volume, WiFi meters only if changed.
  Calls displayStationInfo() every time.
*/
void displayBattery() {
  float battLevel = map(M5.Axp.GetBatVoltage() * 100, 320, 410, 0, 10000) / 100.0;
  if ((abs(battLevel - lastBatteryLevel) >= 1.0 || abs(audioGain - lastAudioGain) >= 0.5) && 
      (millis() - lastMeterUpdate > 200)) {
    drawMeters();
    lastBatteryLevel = battLevel;
    lastMeterUpdate = millis();
  }
  displayStationInfo();
}

/*
  displayStationInfo()
  --------------------
  Shows "Station X of Y" or error message.
  Only updates if station changed.
*/
void displayStationInfo() {
  M5.Lcd.setFreeFont(FSS9);
  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Lcd.setTextDatum(TC_DATUM);
  String stationInfo;

  if (stationCount == 0) {
    stationInfo = "no stations_list.txt on SD";
  } else if (currentStationNumber != lastDisplayedStation) {
    stationInfo = "Station " + String(currentStationNumber + 1) + " of " + String(stationCount);
  } else {
    return;  // No change
  }

  M5.Lcd.fillRect(0, 180, 320, 20, TFT_BLACK);
  M5.Lcd.drawString(stationInfo, 160, 190, GFXFF);
  lastDisplayedStation = currentStationNumber;
}

// ==================================================================
// =========================== BUTTONS & METERS =======================
// ==================================================================

/*
  drawButtons()
  -------------
  Draws two button bars:
  TOP:    [Station -] [ WiFi ] [Station +]
  BOTTOM: [  Vol -  ] [ Mute ] [  Vol +  ]
  Mute button turns RED when muted.
*/
void drawButtons() {
  int btnWidth = 106, btnHeight = 25, stripWidth = 2;

  // TOP ROW
  M5.Lcd.fillRect(0, 0, 320, btnHeight, TFT_BLACK);
  M5.Lcd.fillRect(0, 0, btnWidth, btnHeight, SOFT_BLUE);
  M5.Lcd.setTextColor(TFT_WHITE, SOFT_BLUE);
  M5.Lcd.setFreeFont(FSSB9);
  M5.Lcd.setTextDatum(TC_DATUM);
  M5.Lcd.drawString("Station -", btnWidth/2, 5, GFXFF);
  M5.Lcd.fillRect(btnWidth, 0, stripWidth, btnHeight, TFT_BLACK);
  M5.Lcd.fillRect(btnWidth + stripWidth, 0, btnWidth, btnHeight, SOFT_BLUE);
  M5.Lcd.drawString("WiFi", btnWidth + stripWidth + btnWidth/2, 5, GFXFF);  // WiFi on top
  M5.Lcd.fillRect(btnWidth*2 + stripWidth, 0, stripWidth, btnHeight, TFT_BLACK);
  M5.Lcd.fillRect(btnWidth*2 + stripWidth*2, 0, btnWidth, btnHeight, SOFT_BLUE);
  M5.Lcd.drawString("Station +", btnWidth*2 + stripWidth*2 + btnWidth/2, 5, GFXFF);

  // BOTTOM ROW
  M5.Lcd.fillRect(0, 215, 320, btnHeight, TFT_BLACK);
  M5.Lcd.fillRect(0, 215, btnWidth, btnHeight, SOFT_BLUE);
  M5.Lcd.drawString("Vol -", btnWidth/2, 220, GFXFF);
  M5.Lcd.fillRect(btnWidth, 215, stripWidth, btnHeight, TFT_BLACK);
  M5.Lcd.fillRect(btnWidth + stripWidth, 215, btnWidth, btnHeight, muted ? TFT_RED : SOFT_BLUE);
  M5.Lcd.setTextColor(TFT_WHITE, muted ? TFT_RED : SOFT_BLUE);
  M5.Lcd.drawString("Mute", btnWidth + stripWidth + btnWidth/2, 220, GFXFF);  // Mute on bottom
  M5.Lcd.fillRect(btnWidth*2 + stripWidth, 215, stripWidth, btnHeight, TFT_BLACK);
  M5.Lcd.fillRect(btnWidth*2 + stripWidth*2, 215, btnWidth, btnHeight, SOFT_BLUE);
  M5.Lcd.setTextColor(TFT_WHITE, SOFT_BLUE);
  M5.Lcd.drawString("Vol +", btnWidth*2 + stripWidth*2 + btnWidth/2, 220, GFXFF);
}

/*
  clearTrack()
  ------------
  Clears the area where Artist - Title is shown.
*/
void clearTrack() {
  M5.Lcd.fillRect(0, 60, 320, 75, TFT_DARKGREY);
  M5.Lcd.drawRect(0, 60, 320, 75, SOFT_BLUE);
}

/*
  drawMeters()
  -----------
  Draws Volume, WiFi, Battery bars at y=145.
*/
void drawMeters() {
  int barWidth = 103, barHeight = 10, gap = 5;
  M5.Lcd.fillRect(0, 145, 320, 35, TFT_BLACK);

  // Volume bar
  M5.Lcd.fillRect(0, 145, barWidth, barHeight, TFT_BLACK);
  int volumeLevel = map(audioGain, 0, 10, 0, 100);
  int volumeWidth = map(volumeLevel, 0, 100, 0, barWidth);
  for (int i = 0; i < volumeWidth; i++) {
    M5.Lcd.drawFastVLine(i, 145, barHeight, SOFT_BLUE);
  }
  M5.Lcd.drawFastVLine(volumeWidth, 145, barHeight, TFT_WHITE);
  M5.Lcd.drawRect(0, 145, barWidth, barHeight, TFT_WHITE);

  // WiFi bar
  M5.Lcd.fillRect(barWidth + gap, 145, barWidth, barHeight, TFT_BLACK);
  int wifiLevel = map(WiFi.RSSI(), -100, -30, 0, 100);
  wifiLevel = constrain(wifiLevel, 0, 100);
  int wifiWidth = map(wifiLevel, 0, 100, 0, barWidth);
  for (int i = 0; i < wifiWidth; i++) {
    M5.Lcd.drawFastVLine(barWidth + gap + i, 145, barHeight, SOFT_BLUE);
  }
  M5.Lcd.drawFastVLine(barWidth + gap + wifiWidth, 145, barHeight, TFT_WHITE);
  M5.Lcd.drawRect(barWidth + gap, 145, barWidth, barHeight, TFT_WHITE);

  // Battery bar
  int battStart = barWidth * 2 + gap * 2;
  int battWidthFull = 320 - battStart;
  M5.Lcd.fillRect(battStart, 145, battWidthFull, barHeight, TFT_BLACK);
  int battLevel = map(M5.Axp.GetBatVoltage() * 100, 320, 410, 0, 10000) / 100.0;
  battLevel = constrain(battLevel, 0, 100);
  int battWidth = map(battLevel, 0, 100, 0, battWidthFull);
  for (int i = 0; i < battWidth; i++) {
    M5.Lcd.drawFastVLine(battStart + i, 145, barHeight, SOFT_BLUE);
  }
  M5.Lcd.drawFastVLine(battStart + battWidth, 145, barHeight, TFT_WHITE);
  M5.Lcd.drawRect(battStart, 145, battWidthFull, barHeight, TFT_WHITE);

  // Labels
  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Lcd.setFreeFont(FSS9);
  M5.Lcd.setTextDatum(TC_DATUM);
  M5.Lcd.drawString("Volume", barWidth/2, 164, GFXFF);
  M5.Lcd.drawString("WiFi", barWidth + gap + barWidth/2, 164, GFXFF);
  M5.Lcd.drawString("Battery", battStart + battWidthFull/2, 164, GFXFF);
}

// ==================================================================
// =========================== AUDIO CALLBACKS =======================
// ==================================================================

/*
  MDCallback()
  ------------
  Called when stream sends metadata (e.g., "Artist - Title")
*/
void MDCallback(void *cbData, const char *type, bool isUnicode, const char *string) {
  char s1[32], s2[64];
  strncpy_P(s1, type, sizeof(s1)); s1[sizeof(s1)-1] = 0;
  strncpy_P(s2, string, sizeof(s2)); s2[sizeof(s2)-1] = 0;

  String band = getValue(s2, '-', 0); band.trim();
  String track = getValue(s2, '-', 1); track.trim();
  if (band.length() > 30) band = band.substring(0, 30);
  if (track.length() > 30) track = track.substring(0, 30);

  Serial.printf("METADATA: %s = %s\n", s1, s2);
  M5.Lcd.setTextColor(TFT_BLACK, TFT_DARKGREY);
  clearTrack();
  M5.Lcd.setTextDatum(TC_DATUM);
  M5.Lcd.setFreeFont(FSSB9);
  M5.Lcd.drawString(band, 160, 67, GFXFF);
  M5.Lcd.drawString(track, 160, 89, GFXFF);
  M5.Lcd.drawString(streamBitrate + ", " + streamSampleRate, 160, 112, GFXFF);
  displayBattery();
}

void StatusCallback(void *cbData, int code, const char *string) {
  char s1[64];
  strncpy_P(s1, string, sizeof(s1)); s1[sizeof(s1)-1] = 0;
  Serial.printf("STATUS: %d = %s\n", code, s1);
}

// ==================================================================
// =========================== AUDIO CONTROL =========================
// ==================================================================

/*
  stopPlaying()
  -------------
  Safely stops and deletes all audio objects.
*/
void stopPlaying() {
  if (mp3) { mp3->stop(); delete mp3; mp3 = nullptr; }
  if (buffmp3) { buffmp3->close(); delete buffmp3; buffmp3 = nullptr; }
  if (filemp3) { filemp3->close(); delete filemp3; filemp3 = nullptr; }
  if (outmp3) { delete outmp3; outmp3 = nullptr; }
}

/*
  updateStation()
  ---------------
  Updates station name at top of screen.
*/
void updateStation(String message) {
  M5.Lcd.fillRect(0, 25, 320, 30, TFT_BLACK);
  M5.Lcd.setTextColor(SOFT_BLUE, TFT_BLACK);
  M5.Lcd.setFreeFont(FSSB9);
  M5.Lcd.setTextDatum(TC_DATUM);
  M5.Lcd.drawString(message, 160, 35, GFXFF);
}

/*
  scrollStation()
  ---------------
  Changes station up/down. Wraps around.
*/
void scrollStation(int direction) {
  if (stationCount == 0) return;
  isScrollingStations = true;
  lastStationChange = millis();
  currentStationNumber = (currentStationNumber + direction + stationCount) % stationCount;
  updateStation(stationList[currentStationNumber][0]);
  displayStationInfo();
}

/*
  changeVolume()
  --------------
  Adjusts volume only if not muted.
*/
void changeVolume(float adjustment) {
  if (!muted) {
    audioGain += adjustment;
    audioGain = constrain(audioGain, 0.0, 10.0);
  }
  if (outmp3) {
    outmp3->SetGain(muted ? 0.0 : audioGain * gainfactor);
  }
  drawMeters();
  lastAudioGain = audioGain;
}

/*
  toggleMute()
  -----------
  Toggles mute on/off. Saves volume. Shows red button.
*/
void toggleMute() {
  muted = !muted;
  if (muted) {
    savedGain = audioGain;
    if (outmp3) outmp3->SetGain(0.0);
  } else {
    if (outmp3) outmp3->SetGain(audioGain * gainfactor);
  }
  drawButtons();  // Redraw with red mute button
}

/*
  playMP3()
  ---------
  Starts playing current station.
*/
void playMP3() {
  if (stationCount == 0) return;
  stopPlaying();

  outmp3 = new AudioOutputI2S(0, 0);
  outmp3->SetPinout(12, 0, 2);
  outmp3->SetOutputModeMono(true);
  outmp3->SetGain(muted ? 0.0 : audioGain * gainfactor);

  filemp3 = new AudioFileSourceICYStream(stationList[currentStationNumber][1].c_str());
  filemp3->RegisterMetadataCB(MDCallback, (void*)"ICY");

  buffmp3 = new AudioFileSourceBuffer(filemp3, bufferSize);
  buffmp3->RegisterStatusCB(StatusCallback, (void*)"buffer");

  mp3 = new AudioGeneratorMP3();
  mp3->RegisterStatusCB(StatusCallback, (void*)"mp3");

  if (mp3->begin(buffmp3, outmp3)) {
    Serial.printf("Playing: %s\n", stationList[currentStationNumber][0].c_str());
  } else {
    Serial.println("Failed to start playback");
  }
  updateStation(stationList[currentStationNumber][0]);
  displayStationInfo();
}

/*
  loopMP3() - FIXED TYPO: mpp → mp3
  ---------------------------------
  Main audio loop. Runs every cycle.
*/
void loopMP3() {
  if (stationCount == 0 || !mp3) return;
  if (mp3->isRunning()) {
    if (!mp3->loop()) {  // FIXED: was 'mpp'
      Serial.println("MP3 loop failed, stopping...");
      mp3->stop();
    }
    if ((audioCheck + 1000) < millis()) {
      audioCheck = millis();
      Serial.printf("Audio buffer: %d bytes\n", buffmp3->getFillLevel());
    }
  } else {
    clearTrack();
    scrollStation(1);
    playMP3();
  }
}

/*
  getValue()
  ----------
  Splits string by separator and returns part.
*/
String getValue(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

// ==================================================================
// =========================== SETUP & LOOP =========================
// ==================================================================

void setup() {
  Serial.begin(115200);  // Start serial monitor
  M5.begin();            // Initialize M5Core2
  M5.Axp.SetSpkEnable(true);  // Enable speaker
  M5.Axp.SetLed(false);       // LED off at start
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setTextWrap(false);

  // Initialize SD card
  if (!SD.begin()) {
    Serial.println("SD Card initialization failed!");
  } else {
    Serial.println("SD Card initialized.");
    loadStationsFromSD();
  }

  // Boot splash
  M5.Lcd.setTextColor(TFT_GREEN, TFT_BLACK);
  M5.Lcd.setFreeFont(FSS9);
  M5.Lcd.setTextDatum(TC_DATUM);
  M5.Lcd.drawString("Core2 Web Radio", 160, 35, GFXFF);

  preferences.begin("wifi-config", false);
  initWiFi();
  delay(500);
  M5.Lcd.clear();
  drawButtons();
  playMP3();
  changeVolume(0);
  displayWiFiInformation();
}

void loop() {
  loopMP3();  // Keep audio running
  M5.update();  // Update touch and buttons

  // Physical buttons
  if (M5.BtnA.wasPressed()) changeVolume(-0.5);
  if (M5.BtnB.wasPressed()) wifiMenu();
  if (M5.BtnC.wasPressed()) changeVolume(0.5);

  // Touchscreen
  if (M5.Touch.ispressed()) {
    TouchPoint_t pos = M5.Touch.getPressPoint();

    if (pos.y >= 0 && pos.y <= 25) {  // Top row
      if (pos.x < 106) scrollStation(-1);
      else if (pos.x >= 108 && pos.x < 214) wifiMenu();
      else if (pos.x >= 216) scrollStation(1);
      delay(300);
    }

    if (pos.y >= 215 && pos.y <= 239) {  // Bottom row
      if (pos.x < 106) changeVolume(-0.5);
      else if (pos.x >= 108 && pos.x < 214) toggleMute();
      else if (pos.x >= 216) changeVolume(0.5);
      delay(300);
    }
  }

  // Auto-play after station change
  if (isScrollingStations && (millis() - lastStationChange > 1000)) {
    clearTrack();
    stopPlaying();
    playMP3();
    isScrollingStations = false;
  }

  // Refresh display
  if ((disUpdate + 50) < millis()) {
    disUpdate = millis();
    displayBattery();
  }

  // Blink LED
  if ((millis() - lastLedToggle) > 1000) {
    ledState = !ledState;
    M5.Axp.SetLed(ledState);
    lastLedToggle = millis();
  }
}
