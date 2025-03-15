/*
   March 13, 2025 
   M5Stack-Core2-WebRadio-v0.3.1 by Amellinium
   Based on m5StreamTest Version 2020.12b by tommyho510@gmail.com
   and M5Stack Core2 Web Media Player modified from other sources 
   by bwbguard
   Board: M5StackCore2 (esp32)
   Required Libraries: ESP8266Audio, Preferences
*/

// --- Libraries and Includes ---
#include <M5Core2.h>                // Core2 hardware control
#include <driver/i2s.h>             // I2S audio driver
#include <WiFi.h>                   // WiFi connectivity
#include <AudioFileSourceICYStream.h> // ICY stream source
#include <AudioFileSourceBuffer.h>   // Buffering for audio stream
#include <AudioGeneratorMP3.h>       // MP3 decoding
#include <AudioOutputI2S.h>          // I2S audio output
#include <Preferences.h>             // Persistent storage for WiFi credentials
#include "Free_Fonts.h"              // Free fonts for display

// --- Constants and Global Variables ---
// Increased buffer size to 256KB (from 128KB) for smoother streaming
const int bufferSize = 256 * 1024;  
Preferences preferences;            // Object for storing WiFi credentials

float audioGain = 5.0;              // Audio volume (0.0 to 10.0)
float gainfactor = 0.05;            // Gain scaling factor
int currentStationNumber = 0;       // Current station index
unsigned long disUpdate = millis(); // Last display update time
unsigned long audioCheck = millis(); // Last audio status check
unsigned long lastStationChange = 0; // Last station change time
bool isScrollingStations = false;    // Flag for station scrolling
float lastAudioGain = -1.0;          // Last volume level for comparison
float lastBatteryLevel = -1.0;       // Last battery level for comparison
unsigned long lastMeterUpdate = 0;   // Last meter update time
int lastDisplayedStation = -1;       // Last displayed station number
unsigned long lastLedToggle = 0;     // Last LED toggle time
bool ledState = false;               // LED on/off state

// Audio objects
AudioGeneratorMP3 *mp3;             // MP3 decoder
AudioFileSourceICYStream *filemp3;  // Stream source
AudioFileSourceBuffer *buffmp3;     // Stream buffer
AudioOutputI2S *outmp3;             // I2S output

#define SOFT_BLUE 0x3DAF            // Soft blue color for UI elements

// Station list (name, URL pairs)
char *stationList[][2] = {
  {"SomaFM Groove Salad", "http://ice2.somafm.com/groovesalad-128-mp3"},
  {"SomaFM Drone Zone", "http://ice1.somafm.com/dronezone-128-mp3"},
  {"Classic FM", "http://media-ice.musicradio.com:80/ClassicFMMP3"},
  {"Lite Favorites", "http://naxos.cdnstream.com:80/1255_128"},
  {"Radio Swiss Jazz", "http://stream.srg-ssr.ch/m/rsj/mp3_128"},
  {"KEXP Seattle", "http://live-mp3-128.kexp.org/kexp128.mp3"},
  {"Radio Paradise", "http://stream.radioparadise.com/mp3-128"},
  {"Heart London", "http://media-ice.musicradio.com/HeartLondonMP3"},
  {"Capital FM", "http://media-ice.musicradio.com/CapitalMP3"},
  {"Radio X", "http://media-ice.musicradio.com/RadioXUKMP3"},
  {"Triple J", "http://live-radio01.mediahubaustralia.com/2TJW/mp3/"},
  {"Hit Radio FFH", "http://mp3.ffh.de/radioffh/hqlivestream.mp3"},
  {"Los 40", "http://playerservices.streamtheworld.com/api/livestream-redirect/LOS40.mp3"},
  {"Radio Swiss Classic", "http://stream.srg-ssr.ch/m/rsc_de/mp3_128"},
  {"Radio Swiss Pop", "http://stream.srg-ssr.ch/m/rsp/mp3_128"},
  {"Smooth Radio", "http://media-ice.musicradio.com/SmoothLondonMP3"},
};

String selectedSSID = "";           // Selected WiFi SSID
String selectedPassword = "";       // Selected WiFi password
String streamBitrate = "128 kbps";  // Static bitrate display
String streamSampleRate = "44.1 kHz"; // Static sample rate display

// --- WiFi Functions ---
// Initialize WiFi by loading saved credentials or prompting setup
void initWiFi() {
  preferences.begin("wifi-config", false);
  if (preferences.isKey("ssid") && preferences.isKey("password")) {
    selectedSSID = preferences.getString("ssid", "");
    selectedPassword = preferences.getString("password", "");
    connectToWiFi(selectedSSID.c_str(), selectedPassword.c_str());
  } else {
    wifiSetupMenu();
  }
}

// Connect to WiFi with given SSID and password
void connectToWiFi(const char *ssid, const char *password) {
  M5.Lcd.clear();
  M5.Lcd.setTextColor(SOFT_BLUE, TFT_BLACK);
  M5.Lcd.setFreeFont(FSS9);
  M5.Lcd.setTextDatum(TC_DATUM);
  M5.Lcd.drawString("Connecting to " + String(ssid), 160, 120, GFXFF);

  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 15) {
    delay(1000);
    attempts++;
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    preferences.putString("ssid", ssid);
    preferences.putString("password", password);
  } else {
    Serial.println("\nConnection Failed!");
    wifiSetupMenu();
  }
}

// Display WiFi setup menu and scan for networks
void wifiSetupMenu() {
  M5.Lcd.clear();
  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Lcd.setFreeFont(FSS9);
  M5.Lcd.setTextDatum(TC_DATUM);
  M5.Lcd.drawString("WiFi Setup", 160, 20, GFXFF);

  int n = WiFi.scanNetworks();
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
    M5.update();
    if (M5.Touch.ispressed()) {
      TouchPoint_t pos = M5.Touch.getPressPoint();
      int selected = (pos.y - 50) / 20;
      if (selected >= 0 && selected < min(n, 5)) {
        selectedSSID = WiFi.SSID(selected);
        showKeyboard();
        break;
      }
    }
    delay(100);
  }
}

// Display on-screen keyboard for password entry
void showKeyboard() {
  M5.Lcd.clear();
  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Lcd.setFreeFont(FSS9);
  M5.Lcd.setTextDatum(TC_DATUM);
  M5.Lcd.drawString("Password for " + selectedSSID, 160, 10, GFXFF);

  String password = "";
  M5.Lcd.fillRect(20, 30, 280, 25, TFT_DARKGREY);
  M5.Lcd.drawRect(20, 30, 280, 25, TFT_WHITE);
  M5.Lcd.setTextColor(TFT_WHITE, TFT_DARKGREY);
  M5.Lcd.drawString(password, 160, 34, GFXFF);

  int y = 80;
  int keyWidth = 32;
  int keyHeight = 32;
  int keySpacing = 2;
  bool shift = false;
  const char *keyboardLower[4] = {"1234567890", "qwertyuiop", "asdfghjkl ", "zxcvbnm_<-"};
  const char *keyboardUpper[4] = {"1234567890", "QWERTYUIOP", "ASDFGHJKL ", "ZXCVBNM_<-"};
  const char **currentKeyboard = keyboardLower;

  for (int row = 0; row < 4; row++) {
    int len = strlen(currentKeyboard[row]);
    int xStart = 0;
    for (int col = 0; col < len; col++) {
      int x = xStart + (col * keyWidth);
      M5.Lcd.fillRect(x, y, keyWidth - 2, keyHeight - 2, TFT_LIGHTGREY);
      M5.Lcd.drawRect(x, y, keyWidth - 2, keyHeight - 2, TFT_WHITE);
      M5.Lcd.setTextColor(TFT_BLACK, TFT_LIGHTGREY);
      M5.Lcd.drawChar(currentKeyboard[row][col], x + (keyWidth / 2) - 5, y + (keyHeight / 2) - 1, GFXFF);
    }
    y += keyHeight + keySpacing;
  }

  int bottomY = 215;
  int btnWidth = 106;
  int btnHeight = 25;
  int stripWidth = 2;

  M5.Lcd.fillRect(0, bottomY, 320, btnHeight, TFT_BLACK);
  M5.Lcd.fillRect(0, bottomY, btnWidth, btnHeight, SOFT_BLUE);
  M5.Lcd.setTextColor(TFT_WHITE, SOFT_BLUE);
  M5.Lcd.setFreeFont(FSSB9);
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

      if (pos.y >= 80 && pos.y < (80 + 4 * (keyHeight + keySpacing))) {
        int row = (pos.y - 80) / (keyHeight + keySpacing);
        int len = strlen(currentKeyboard[row]);
        int xStart = 0;
        int col = (pos.x - xStart) / keyWidth;
        if (col >= 0 && col < len && pos.x >= xStart && pos.x < (xStart + len * keyWidth)) {
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
            int xS = 0;
            for (int c = 0; c < len; c++) {
              int x = xS + (c * keyWidth);
              M5.Lcd.fillRect(x, y, keyWidth - 2, keyHeight - 2, TFT_LIGHTGREY);
              M5.Lcd.drawRect(x, y, keyWidth - 2, keyHeight - 2, TFT_WHITE);
              M5.Lcd.setTextColor(TFT_BLACK, TFT_LIGHTGREY);
              M5.Lcd.drawChar(currentKeyboard[r][c], x + (keyWidth / 2) - 5, y + (keyHeight / 2) - 1, GFXFF);
            }
            y += keyHeight + keySpacing;
          }
          M5.Lcd.fillRect(btnWidth + stripWidth, bottomY, btnWidth, btnHeight, SOFT_BLUE);
          M5.Lcd.drawString(shift ? "shift" : "Shift", btnWidth + stripWidth + btnWidth / 2, bottomY + 5, GFXFF);
        } else if (pos.x >= (btnWidth * 2 + stripWidth * 2)) {
          selectedPassword = password;
          connectToWiFi(selectedSSID.c_str(), selectedPassword.c_str());
          break;
        }
      }
      delay(300);
    }
  }
}

// --- Menu Functions ---
// Display WiFi menu for viewing or changing connection
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

// --- Display Functions ---
// Placeholder for WiFi info display (currently empty)
void displayWiFiInformation() {
}

// Placeholder for WiFi signal update (currently empty)
void updateWiFiSignal() {
}

// Update battery, volume, and WiFi meters if significant change detected
void displayBattery() {
  float battLevel = map(M5.Axp.GetBatVoltage() * 100, 320, 410, 0, 10000) / 100.0;
  if ((abs(battLevel - lastBatteryLevel) >= 1.0 || abs(audioGain - lastAudioGain) >= 0.5) && 
      (millis() - lastMeterUpdate > 200)) {
    drawMeters();
    lastBatteryLevel = battLevel;
    lastMeterUpdate = millis();
  }
  displayStationInfo(); // Always update station info
}

// Display station number in "Station x of y" format
void displayStationInfo() {
  if (currentStationNumber != lastDisplayedStation) {
    M5.Lcd.setFreeFont(FSS9);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.setTextDatum(TC_DATUM);
    int stationCount = sizeof(stationList) / sizeof(stationList[0]);
    String stationInfo = "Station " + String(currentStationNumber + 1) + " of " + String(stationCount);
    M5.Lcd.fillRect(0, 180, 320, 20, TFT_BLACK);  // Clear area at y=180-200
    M5.Lcd.drawString(stationInfo, 160, 190, GFXFF); // Display at y=190
    lastDisplayedStation = currentStationNumber;
  }
}

// Draw top and bottom buttons (Station +/-, BT, Vol +/-, WiFi)
void drawButtons() {
  int btnWidth = 106;
  int btnHeight = 25;
  int stripWidth = 2;

  M5.Lcd.fillRect(0, 0, 320, btnHeight, TFT_BLACK);
  M5.Lcd.fillRect(0, 0, btnWidth, btnHeight, SOFT_BLUE);
  M5.Lcd.setTextColor(TFT_WHITE, SOFT_BLUE);
  M5.Lcd.setFreeFont(FSSB9);
  M5.Lcd.setTextDatum(TC_DATUM);
  M5.Lcd.drawString("Station -", btnWidth/2, 5, GFXFF);
  M5.Lcd.fillRect(btnWidth, 0, stripWidth, btnHeight, TFT_BLACK);
  M5.Lcd.fillRect(btnWidth + stripWidth, 0, btnWidth, btnHeight, SOFT_BLUE);
  M5.Lcd.drawString("BT", btnWidth + stripWidth + btnWidth/2, 5, GFXFF);
  M5.Lcd.fillRect(btnWidth*2 + stripWidth, 0, stripWidth, btnHeight, TFT_BLACK);
  M5.Lcd.fillRect(btnWidth*2 + stripWidth*2, 0, btnWidth, btnHeight, SOFT_BLUE);
  M5.Lcd.drawString("Station +", btnWidth*2 + stripWidth*2 + btnWidth/2, 5, GFXFF);

  M5.Lcd.fillRect(0, 215, 320, btnHeight, TFT_BLACK);
  M5.Lcd.fillRect(0, 215, btnWidth, btnHeight, SOFT_BLUE);
  M5.Lcd.drawString("Vol -", btnWidth/2, 220, GFXFF);
  M5.Lcd.fillRect(btnWidth, 215, stripWidth, btnHeight, TFT_BLACK);
  M5.Lcd.fillRect(btnWidth + stripWidth, 215, btnWidth, btnHeight, SOFT_BLUE);
  M5.Lcd.drawString("WiFi", btnWidth + stripWidth + btnWidth/2, 220, GFXFF);
  M5.Lcd.fillRect(btnWidth*2 + stripWidth, 215, stripWidth, btnHeight, TFT_BLACK);
  M5.Lcd.fillRect(btnWidth*2 + stripWidth*2, 215, btnWidth, btnHeight, SOFT_BLUE);
  M5.Lcd.drawString("Vol +", btnWidth*2 + stripWidth*2 + btnWidth/2, 220, GFXFF);
}

// Clear the track display area
void clearTrack() {
  M5.Lcd.fillRect(0, 60, 320, 75, TFT_DARKGREY);
  M5.Lcd.drawRect(0, 60, 320, 75, SOFT_BLUE);
}

// Draw volume, WiFi, and battery meters
void drawMeters() {
  int barWidth = 103;
  int barHeight = 10;
  int gap = 5;

  M5.Lcd.fillRect(0, 145, 320, 35, TFT_BLACK);  // Clear from 145 to 180

  M5.Lcd.fillRect(0, 145, barWidth, barHeight, TFT_BLACK); // Volume bar at y=145
  int volumeLevel = map(audioGain, 0, 10, 0, 100);
  int volumeWidth = map(volumeLevel, 0, 100, 0, barWidth);
  for (int i = 0; i < volumeWidth; i++) {
    M5.Lcd.drawFastVLine(i, 145, barHeight, SOFT_BLUE);
  }
  M5.Lcd.drawFastVLine(volumeWidth, 145, barHeight, TFT_WHITE);
  M5.Lcd.drawRect(0, 145, barWidth, barHeight, TFT_WHITE);

  M5.Lcd.fillRect(barWidth + gap, 145, barWidth, barHeight, TFT_BLACK); // WiFi bar at y=145
  int wifiLevel = map(WiFi.RSSI(), -100, -30, 0, 100);
  wifiLevel = constrain(wifiLevel, 0, 100);
  int wifiWidth = map(wifiLevel, 0, 100, 0, barWidth);
  for (int i = 0; i < wifiWidth; i++) {
    M5.Lcd.drawFastVLine(barWidth + gap + i, 145, barHeight, SOFT_BLUE);
  }
  M5.Lcd.drawFastVLine(barWidth + gap + wifiWidth, 145, barHeight, TFT_WHITE);
  M5.Lcd.drawRect(barWidth + gap, 145, barWidth, barHeight, TFT_WHITE);

  int battStart = barWidth * 2 + gap * 2;
  int battWidthFull = 320 - battStart;
  M5.Lcd.fillRect(battStart, 145, battWidthFull, barHeight, TFT_BLACK); // Battery bar at y=145
  int battLevel = map(M5.Axp.GetBatVoltage() * 100, 320, 410, 0, 10000) / 100.0;
  battLevel = constrain(battLevel, 0, 100);
  int battWidth = map(battLevel, 0, 100, 0, battWidthFull);
  for (int i = 0; i < battWidth; i++) {
    M5.Lcd.drawFastVLine(battStart + i, 145, barHeight, SOFT_BLUE);
  }
  M5.Lcd.drawFastVLine(battStart + battWidth, 145, barHeight, TFT_WHITE);
  M5.Lcd.drawRect(battStart, 145, battWidthFull, barHeight, TFT_WHITE);

  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Lcd.setFreeFont(FSS9);
  M5.Lcd.setTextDatum(TC_DATUM);
  M5.Lcd.drawString("Volume", barWidth/2, 164, GFXFF); // Descriptions at y=164
  M5.Lcd.drawString("WiFi", barWidth + gap + barWidth/2, 164, GFXFF);
  M5.Lcd.drawString("Battery", battStart + battWidthFull/2, 164, GFXFF);
}

// --- Audio Functions ---
// Handle metadata callbacks (artist, track, static bitrate/sample rate)
void MDCallback(void *cbData, const char *type, bool isUnicode, const char *string) {
  const char *ptr = reinterpret_cast<const char *>(cbData);
  char s1[32], s2[64];
  strncpy_P(s1, type, sizeof(s1));
  s1[sizeof(s1) - 1] = 0;
  strncpy_P(s2, string, sizeof(s2));
  s2[sizeof(s2) - 1] = 0;

  String band = getValue(s2, '-', 0);
  band.trim();
  String track = getValue(s2, '-', 1);
  track.trim();

  if (band.length() > 30) band = band.substring(0, 30);
  if (track.length() > 30) track = track.substring(0, 30);

  Serial.printf("METADATA(%s) '%s' = '%s'\n", ptr, s1, s2);
  M5.Lcd.setTextColor(TFT_BLACK, TFT_DARKGREY);
  clearTrack();
  M5.Lcd.setTextDatum(TC_DATUM);
  M5.Lcd.setFreeFont(FSSB9);
  M5.Lcd.drawString(band, 160, 67, GFXFF);               // Line 1: Artist/Band
  M5.Lcd.drawString(track, 160, 89, GFXFF);              // Line 2: Track
  M5.Lcd.drawString(streamBitrate + ", " + streamSampleRate, 160, 112, GFXFF); // Line 3: Static bitrate/sample rate
  Serial.flush();
  displayBattery();
}

// Handle status callbacks for audio playback
void StatusCallback(void *cbData, int code, const char *string) {
  const char *ptr = reinterpret_cast<const char *>(cbData);
  char s1[64];
  strncpy_P(s1, string, sizeof(s1));
  s1[sizeof(s1) - 1] = 0;
  Serial.printf("STATUS(%s) '%d' = '%s'\n", ptr, code, s1);
  Serial.flush();
}

// Stop audio playback and clean up resources
void stopPlaying() {
  Serial.printf("Stopping MP3...\n");
  if (mp3) {
    mp3->stop();
    delete mp3;
    mp3 = NULL;
  }
  if (buffmp3) {
    buffmp3->close();
    delete buffmp3;
    buffmp3 = NULL;
  }
  if (filemp3) {
    filemp3->close();
    delete filemp3;
    filemp3 = NULL;
  }
  if (outmp3) {
    delete outmp3;
    outmp3 = NULL;
  }
  Serial.printf("STATUS(Stopped)\n");
  Serial.flush();
}

// Update station name display above track info
void updateStation(String message) {
  M5.Lcd.fillRect(0, 25, 320, 35, BLACK);
  M5.Lcd.setTextColor(SOFT_BLUE, TFT_BLACK);
  M5.Lcd.setFreeFont(FSSB9);
  M5.Lcd.setTextDatum(TC_DATUM);
  M5.Lcd.drawString(message, 160, 35, GFXFF);
}

// Scroll through station list
void scrollStation(int direction) {
  isScrollingStations = true;
  lastStationChange = millis();
  int stationCount = sizeof(stationList) / sizeof(stationList[0]);
  currentStationNumber += direction;
  if (currentStationNumber >= stationCount) currentStationNumber = 0;
  if (currentStationNumber < 0) currentStationNumber = stationCount - 1;
  updateStation(String(stationList[currentStationNumber][0]));
  Serial.printf("Scrolling to station: %d - %s (Total stations: %d)\n", currentStationNumber, stationList[currentStationNumber][0], stationCount);
  displayStationInfo();
}

// Adjust audio volume
void changeVolume(float adjustment) {
  audioGain += adjustment;
  audioGain = constrain(audioGain, 0.0, 10.0);
  if (outmp3) {
    outmp3->SetGain(audioGain * gainfactor);
  }
  drawMeters();
  lastAudioGain = audioGain;
}

// Initialize and start MP3 playback
void playMP3() {
  Serial.println("Starting playMP3...");
  stopPlaying();

  outmp3 = new AudioOutputI2S(0, 0);
  outmp3->SetPinout(12, 0, 2);
  outmp3->SetOutputModeMono(true);
  outmp3->SetGain(audioGain * gainfactor);
  Serial.println("Audio output initialized");

  filemp3 = new AudioFileSourceICYStream(stationList[currentStationNumber][1]);
  filemp3->RegisterMetadataCB(MDCallback, (void*)"ICY");
  Serial.println("Stream source created");

  buffmp3 = new AudioFileSourceBuffer(filemp3, bufferSize);
  buffmp3->RegisterStatusCB(StatusCallback, (void*)"buffer");
  Serial.printf("Buffer allocated, free heap: %d bytes\n", ESP.getFreeHeap());

  mp3 = new AudioGeneratorMP3();
  mp3->RegisterStatusCB(StatusCallback, (void*)"mp3");
  Serial.println("MP3 generator ready");

  if (mp3->begin(buffmp3, outmp3)) {
    Serial.printf("Playing: %s - %s\n", stationList[currentStationNumber][0], stationList[currentStationNumber][1]);
  } else {
    Serial.println("Failed to start MP3 playback");
  }
  updateStation(String(stationList[currentStationNumber][0]));
  displayStationInfo();
  Serial.flush();
}

// Main audio playback loop
void loopMP3() {
  if (mp3 != NULL) {
    if (mp3->isRunning()) {
      if (!mp3->loop()) {
        Serial.println("MP3 loop failed, stopping...");
        mp3->stop();
      }
      if ((audioCheck + 1000) < millis()) {
        audioCheck = millis();
        Serial.printf("Audio running, buffer fill: %d bytes\n", buffmp3->getFillLevel());
      }
    } else {
      Serial.printf("Status(Stream) Stopped \n");
      clearTrack();
      scrollStation(1);
      playMP3();
    }
  }
}

// --- Utility Functions ---
// Extract value from string by separator
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

// --- Setup and Main Loop ---
// Initial setup
void setup() {
  Serial.begin(115200);
  M5.begin();
  M5.Axp.SetSpkEnable(true);      // Enable internal speaker
  M5.Axp.SetLed(false);           // Start with LED off
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextWrap(false);

  Serial.printf("Free heap at setup: %d bytes\n", ESP.getFreeHeap());

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

// Main loop handling audio, UI, and LED blinking
void loop() {
  loopMP3();
  M5.update();

  // Physical button controls
  if (M5.BtnA.wasPressed()) {
    changeVolume(-0.5);
  }
  if (M5.BtnB.wasPressed()) {
    wifiMenu();
  }
  if (M5.BtnC.wasPressed()) {
    changeVolume(0.5);
  }

  // Touchscreen controls
  if (M5.Touch.ispressed()) {
    TouchPoint_t pos = M5.Touch.getPressPoint();

    if (pos.y >= 0 && pos.y <= 25) { // Top buttons
      if (pos.x < 106) {
        scrollStation(-1);
      } else if (pos.x >= 108 && pos.x < 214) {
        // BT placeholder
      } else if (pos.x >= 216) {
        scrollStation(1);
      }
      delay(300);
    }

    if (pos.y >= 215 && pos.y <= 239) { // Bottom buttons
      if (pos.x < 106) {
        changeVolume(-0.5);
      } else if (pos.x >= 108 && pos.x < 214) {
        wifiMenu();
      } else if (pos.x >= 216) {
        changeVolume(0.5);
      }
      delay(300);
    }
  }

  // Handle station change after scrolling
  if (isScrollingStations && (millis() - lastStationChange > 1000)) {
    clearTrack();
    stopPlaying();
    playMP3();
    isScrollingStations = false;
  }

  // Update display every 50ms
  if ((disUpdate + 50) < millis()) {
    disUpdate = millis();
    displayBattery();
  }

  // Blink power LED every 1000ms
  if ((millis() - lastLedToggle) > 1000) {
    ledState = !ledState;
    M5.Axp.SetLed(ledState); // Toggle green power LED
    lastLedToggle = millis();
  }
}
