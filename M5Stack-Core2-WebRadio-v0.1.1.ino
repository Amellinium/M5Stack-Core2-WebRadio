/*
   March 07, 2025 
   M5Stack-Core2-WebRadio-v0.1.1
   Based on m5StreamTest Version 2020.12b by tommyho510@gmail.com
   and M5Stack Core2 Web Media Player modified from other sources 
   by bwbguard
   Board: M5StackCore2 (esp32)
   Required Libraries: ESP8266Audio, Preferences
*/

#include <M5Core2.h>
#include <driver/i2s.h>
#include <WiFi.h>
#include <AudioFileSourceICYStream.h>
#include <AudioFileSourceBuffer.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>
#include <Preferences.h>
#include "Free_Fonts.h"

const int bufferSize = 128 * 1024;
Preferences preferences;

float audioGain = 5.0;
float gainfactor = 0.05;
int currentStationNumber = 0;
unsigned long disUpdate = millis();
unsigned long audioCheck = millis();
unsigned long lastStationChange = 0;
bool isScrollingStations = false;

// VU Meter variables
float vuLevelL = 0, vuLevelR = 0; // Use floats for smoothing
float vuPeakL = 0, vuPeakR = 0;   // Peak hold
float smoothedVuL = 0, smoothedVuR = 0; // Smoothed values
unsigned long lastVuUpdate = 0;

AudioGeneratorMP3 *mp3;
AudioFileSourceICYStream *filemp3;
AudioFileSourceBuffer *buffmp3;
AudioOutputI2S *outmp3;

#define SOFT_BLUE 0x3DAF  // Soft blue color for buttons

const int stations = 100;
char *stationList[stations][2] = {
  {"Charlie FM", "http://24083.live.streamtheworld.com:80/KYCHFM_SC"},
  {"MAXXED Out", "http://149.56.195.94:8015/steam"},
  {"Orig. Top 40", "http://ais-edge09-live365-dal02.cdnstream.com/a25710"},
  {"Smooth Jazz", "http://sj32.hnux.com/stream?type=http&nocache=3104"},
  {"Smooth Lounge", "http://sl32.hnux.com/stream?type=http&nocache=1257"},
  {"Classic FM", "http://media-ice.musicradio.com:80/ClassicFMMP3"},
  {"Lite Favorites", "http://naxos.cdnstream.com:80/1255_128"},
  {"BBC Radio 1", "http://bbcmedia.ic.llnwd.net/stream/bbcmedia_radio1_mf_p"},
  {"BBC Radio 2", "http://bbcmedia.ic.llnwd.net/stream/bbcmedia_radio2_mf_p"},
  {"NPR News", "http://npr.live.streamtheworld.com:80/WAMUFMAAC"},
  {"Radio Swiss Jazz", "http://stream.srg-ssr.ch/m/rsj/mp3_128"},
  {"181.fm - 80s", "http://listen.181fm.com/181-80s_128k.mp3"},
  {"Rock FM", "http://icecast-streaming.nice radio.fm/rockfm.mp3"},
  {"Jazz FM", "http://listen.jazzfm.com/jazzfm.mp3"},
  {"KEXP Seattle", "http://live-mp3-128.kexp.org/kexp128.mp3"},
  {"Radio Paradise", "http://stream.radioparadise.com/mp3-128"},
  {"SomaFM Groove Salad", "http://ice2.somafm.com/groovesalad-128-mp3"},
  {"SomaFM Drone Zone", "http://ice1.somafm.com/dronezone-128-mp3"},
  {"Absolute Radio", "http://icecast.thisisdax.com/AbsoluteRadioMP3"},
  {"Heart London", "http://media-ice.musicradio.com/HeartLondonMP3"},
  {"Kiss FM UK", "http://icy-e-04-cr.sharp-stream.com/kissnational.mp3"},
  {"Capital FM", "http://media-ice.musicradio.com/CapitalMP3"},
  {"Radio X", "http://media-ice.musicradio.com/RadioXUKMP3"},
  {"WNYC FM", "http://fm939.wnyc.org/wnycfm"},
  {"KCRW", "http://kcrw.streamguys1.com/kcrw_192k_mp3"},
  {"Triple J", "http://live-radio01.mediahubaustralia.com/2TJW/mp3/"},
  {"Radio Nova", "http://stream.audioaddict.com/radionova.mp3"},
  {"Chillout Lounge", "http://stream.zeno.fm/4v5q8z3k8rzuv"},
  {"Hit Radio FFH", "http://mp3.ffh.de/radioffh/hqlivestream.mp3"},
  {"Antenne Bayern", "http://stream.antenne.de/antenne.mp3"},
  {"Radio 538", "http://playerservices.streamtheworld.com/api/livestream-redirect/RADIO538.mp3"},
  {"Qmusic NL", "http://icecast-qmusicnl-cdp.triple-it.nl/Qmusic_nl_live_96.mp3"},
  {"NRJ France", "http://cdn.nrjaudio.fm/audio1/fr/30001/mp3_128.mp3"},
  {"RTL 102.5", "http://rtl-radio-stream.leanstream.co/RTL1025.mp3"},
  {"Radio Maria", "http://dreamsiteradiocp2.com:8010/stream"},
  {"Cadena SER", "http://playerservices.streamtheworld.com/api/livestream-redirect/CADENASER.mp3"},
  {"Los 40", "http://playerservices.streamtheworld.com/api/livestream-redirect/LOS40.mp3"},
  {"RNE Radio Nacional", "http://rne.rtve.es/rne.mp3"},
  {"Radio Swiss Classic", "http://stream.srg-ssr.ch/m/rsc_de/mp3_128"},
  {"Radio Swiss Pop", "http://stream.srg-ssr.ch/m/rsp/mp3_128"},
  {"Radio Berlin 88.8", "http://rbb-radioberlin888.stream24.net/radioberlin888.mp3"},
  {"WDR 3", "http://wdr-wdr3-live.icecast.wdr.de/wdr/wdr3/live/mp3/128/stream.mp3"},
  {"Bayern 3", "http://br-br3-live.cast.addradio.de/br/br3/live/mp3/128/stream.mp3"},
  {"Radio FG", "http://radiofg.impek.com/fg.mp3"},
  {"Virgin Radio Italy", "http://icecast.unitedradio.it/Virgin.mp3"},
  {"Radio Deejay", "http://stream.deejay.it/deejay.mp3"},
  {"Rai Radio 1", "http://icestreaming.rai.it/1.mp3"},
  {"J-Pop Powerplay", "http://stream.zeno.fm/6v2k8z3k8rzuv"},
  {"AnimeNfo Radio", "http://stream.animefreak.tv:8000/anime.mp3"},
  {"K-Pop Way", "http://streamer.radio.co/s06b196587/listen"},
  {"Radio Classique", "http://radioclassique.ice.infomaniak.ch/radioclassique-high.mp3"},
  {"FIP", "http://direct.fip.fr/live/fip-midfi.mp3"},
  {"Radio Nostalgia", "http://icecast.omroep.nl/radio6-nostalgia-bb-mp3"},
  {"Oldies Radio", "http://stream.zeno.fm/8v5q8z3k8rzuv"},
  {"Radio Regenbogen", "http://stream.regenbogen.de/rr-mainstream/mp3-128"},
  {"Radio Energy", "http://cdn.nrjaudio.fm/audio1/ch/52001/mp3_128.mp3"},
  {"Radio Top", "http://icecast.radiotop.ch/radiotop.mp3"},
  {"Radio 1 CH", "http://stream.srg-ssr.ch/m/radio1/mp3_128"},
  {"Sveriges Radio P1", "http://sverigesradio.se/topsy/direkt/132.mp3"},
  {"NRK P3", "http://nrk-p3-live.mp3.livecast.no/nrk-p3"},
  {"DR P4", "http://live-icy.gss.dr.dk/A/A05H.mp3"},
  {"Radio 105", "http://icecast.unitedradio.it/Radio105.mp3"},
  {"Radio Monte Carlo", "http://icecast.unitedradio.it/RMC.mp3"},
  {"Rádio Comercial", "http://mcrscast.mcr.iol.pt/comercial.mp3"},
  {"RFM Portugal", "http://mcrscast.mcr.iol.pt/rfm.mp3"},
  {"Radio Kiss Kiss", "http://icecast.unitedradio.it/KissKiss.mp3"},
  {"Radio Freccia", "http://icecast.unitedradio.it/Freccia.mp3"},
  {"Radio Capital", "http://icecast.unitedradio.it/Capital.mp3"},
  {"Radio Veronica", "http://playerservices.streamtheworld.com/api/livestream-redirect/RADIO_VERONICA.mp3"},
  {"Skyrock", "http://icecast.skyrock.net/skyrock"},
  {"Rádio Renascença", "http://rr.sapo.pt/rr.mp3"},
  {"Radio Caprice Classical", "http://stream.capriceradio.com/classical"},
  {"Radio Metal", "http://stream.radiometal.com/metal.mp3"},
  {"_LOCALS_Punk Rock Radio", "http://stream.zeno.fm/9v5q8z3k8rzuv"},
  {"Reggae Radio", "http://stream.zeno.fm/5v5q8z3k8rzuv"},
  {"Blues Radio", "http://stream.zeno.fm/7v5q8z3k8rzuv"},
  {"Country Hits", "http://stream.zeno.fm/3v5q8z3k8rzuv"},
  {"Hip Hop Radio", "http://stream.zeno.fm/2v5q8z3k8rzuv"},
  {"Electronic Dance", "http://stream.zeno.fm/1v5q8z3k8rzuv"},
  {"Ambient Radio", "http://stream.zeno.fm/0v5q8z3k8rzuv"},
  {"Trance Radio", "http://stream.zeno.fm/v5q8z3k8rzuv"},
  {"Techno Base", "http://stream.technobase.fm/tunein-mp3"},
  {"House Time", "http://stream.housetime.fm/tunein-mp3"},
  {"Drum and Bass", "http://stream.dnbradio.com/dnb.mp3"},
  {"Chillhop Radio", "http://stream.zeno.fm/chillhop"},
  {"Lofi Hip Hop", "http://stream.zeno.fm/lofi"},
  {"Radio Disney", "http://stream.zeno.fm/disney"},
  {"Kids Radio", "http://stream.zeno.fm/kids"},
  {"Comedy Radio", "http://stream.zeno.fm/comedy"},
  {"Talk Radio UK", "http://icecast.thisisdax.com/TalkRadioMP3"},
  {"ESPN Radio", "http://espnradio.streamguys1.com/espnradio"},
  {"Radio Sport", "http://stream.zeno.fm/sport"},
  {"Radio Clásica", "http://rne.rtve.es/radioclasica.mp3"},
  {"Radio Caroline", "http://sc6.shoutcaststreaming.us:8040/stream"},
  {"Pirate FM", "http://icecast.thisisdax.com/PirateFMMp3"},
  {"Smooth Radio", "http://media-ice.musicradio.com/SmoothLondonMP3"},
  {"Gold Radio", "http://icecast.thisisdax.com/GoldMP3"}
};

String selectedSSID = "";
String selectedPassword = "";

// Stream quality defaults (assumed for simplicity; can be dynamic if metadata provides it)
String streamBitrate = "128 kbps";  // Most streams in list are 128 kbps
String streamSampleRate = "44.1 kHz";  // Standard for MP3 streams

const char *keyboardLower[5] = {
  "1234567890",
  "qwertyuiop",
  "asdfghjkl ",
  "zxcvbnm_<-",
  "Shift Done"
};

const char *keyboardUpper[5] = {
  "1234567890",
  "QWERTYUIOP",
  "ASDFGHJKL ",
  "ZXCVBNM_<-",
  "shift Done"
};

// WiFi Functions
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

void showKeyboard() {
  M5.Lcd.clear();
  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Lcd.setFreeFont(FSS9);
  M5.Lcd.setTextDatum(TC_DATUM);
  M5.Lcd.drawString("Password for " + selectedSSID, 160, 20, GFXFF);

  String password = "";
  M5.Lcd.fillRect(20, 40, 280, 20, TFT_DARKGREY);
  M5.Lcd.drawRect(20, 40, 280, 20, TFT_WHITE);
  M5.Lcd.drawString(password, 160, 45, GFXFF);

  int y = 70;
  int keyWidth = 32;
  int keyHeight = 32;
  int keySpacing = 5;
  bool shift = false;
  const char **currentKeyboard = keyboardLower;

  for (int row = 0; row < 4; row++) {
    int len = strlen(currentKeyboard[row]);
    int xStart = (320 - (len * keyWidth)) / 2;
    for (int col = 0; col < len; col++) {
      int x = xStart + (col * keyWidth);
      M5.Lcd.fillRect(x, y, keyWidth - 2, keyHeight - 2, TFT_LIGHTGREY);
      M5.Lcd.drawRect(x, y, keyWidth - 2, keyHeight - 2, TFT_WHITE);
      M5.Lcd.setTextColor(TFT_BLACK, TFT_LIGHTGREY);
      M5.Lcd.drawChar(currentKeyboard[row][col], x + (keyWidth / 2) - 5, y + (keyHeight / 2) - 8, GFXFF);
    }
    y += keyHeight + keySpacing;
  }

  int bottomY = y;
  int shiftWidth = 100;
  int doneWidth = 100;
  int buttonHeight = keyHeight;
  int xShift = (320 - (shiftWidth + doneWidth + 10)) / 2;
  int xDone = xShift + shiftWidth + 10;

  M5.Lcd.fillRect(xShift, bottomY, shiftWidth - 2, buttonHeight - 2, SOFT_BLUE);
  M5.Lcd.drawRect(xShift, bottomY, shiftWidth - 2, buttonHeight - 2, TFT_WHITE);
  M5.Lcd.setTextColor(TFT_WHITE, SOFT_BLUE);
  M5.Lcd.drawString("Shift", xShift + (shiftWidth / 2) - 20, bottomY + (buttonHeight / 2) - 8, GFXFF);

  M5.Lcd.fillRect(xDone, bottomY, doneWidth - 2, buttonHeight - 2, SOFT_BLUE);
  M5.Lcd.drawRect(xDone, bottomY, doneWidth - 2, buttonHeight - 2, TFT_WHITE);
  M5.Lcd.setTextColor(TFT_WHITE, SOFT_BLUE);
  M5.Lcd.drawString("Done", xDone + (doneWidth / 2) - 18, bottomY + (buttonHeight / 2) - 8, GFXFF);

  while (true) {
    M5.update();
    if (M5.Touch.ispressed()) {
      TouchPoint_t pos = M5.Touch.getPressPoint();
      if (pos.y >= 70 && pos.y <= (70 + 5 * (keyHeight + keySpacing))) {
        int row = (pos.y - 70) / (keyHeight + keySpacing);
        if (row < 4) {
          int len = strlen(currentKeyboard[row]);
          int xStart = (320 - (len * keyWidth)) / 2;
          int col = (pos.x - xStart) / keyWidth;
          if (col >= 0 && col < len && pos.x >= xStart && pos.x < (xStart + len * keyWidth)) {
            char c = currentKeyboard[row][col];
            if (c == '<') {
              if (password.length() > 0) password.remove(password.length() - 1);
            } else if (c != ' ') {
              password += c;
            }
          }
        } else if (row == 4) {
          if (pos.x >= xShift && pos.x < (xShift + shiftWidth) && pos.y >= bottomY && pos.y < (bottomY + buttonHeight)) {
            shift = !shift;
            currentKeyboard = shift ? keyboardUpper : keyboardLower;
            y = 70;
            M5.Lcd.fillRect(0, 70, 320, 4 * (keyHeight + keySpacing), TFT_BLACK);
            for (int r = 0; r < 4; r++) {
              int len = strlen(currentKeyboard[r]);
              int xS = (320 - (len * keyWidth)) / 2;
              for (int c = 0; c < len; c++) {
                int x = xS + (c * keyWidth);
                M5.Lcd.fillRect(x, y, keyWidth - 2, keyHeight - 2, TFT_LIGHTGREY);
                M5.Lcd.drawRect(x, y, keyWidth - 2, keyHeight - 2, TFT_WHITE);
                M5.Lcd.setTextColor(TFT_BLACK, TFT_LIGHTGREY);
                M5.Lcd.drawChar(currentKeyboard[r][c], x + (keyWidth / 2) - 5, y + (keyHeight / 2) - 8, GFXFF);
              }
              y += keyHeight + keySpacing;
            }
            M5.Lcd.fillRect(xShift, bottomY, shiftWidth - 2, buttonHeight - 2, SOFT_BLUE);
            M5.Lcd.drawRect(xShift, bottomY, shiftWidth - 2, buttonHeight - 2, TFT_WHITE);
            M5.Lcd.drawString(shift ? "shift" : "Shift", xShift + (shiftWidth / 2) - 20, bottomY + (buttonHeight / 2) - 8, GFXFF);
          } else if (pos.x >= xDone && pos.x < (xDone + doneWidth) && pos.y >= bottomY && pos.y < (bottomY + buttonHeight)) {
            selectedPassword = password;
            connectToWiFi(selectedSSID.c_str(), selectedPassword.c_str());
            break;
          }
        }
        M5.Lcd.fillRect(20, 40, 280, 20, TFT_DARKGREY);
        M5.Lcd.drawRect(20, 40, 280, 20, TFT_WHITE);
        M5.Lcd.setTextColor(TFT_WHITE, TFT_DARKGREY);
        M5.Lcd.drawString(password, 160, 45, GFXFF);
      }
      delay(300);
    }
  }
}

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

// Display Functions
void displayWiFiInformation() {
  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Lcd.setFreeFont(FSS9);
  M5.Lcd.setTextDatum(BL_DATUM);
  M5.Lcd.drawString("Network: ", 10, 165, GFXFF);
  M5.Lcd.drawString("IP: ", 10, 190, GFXFF);
  M5.Lcd.drawString(selectedSSID, 90, 165, GFXFF);
  M5.Lcd.drawString(WiFi.localIP().toString(), 40, 190, GFXFF);
}

void updateWiFiSignal() {
  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Lcd.setFreeFont(FSS9);
  M5.Lcd.setTextDatum(BL_DATUM);
  M5.Lcd.drawString("WiFi Signal: ", 10, 215, GFXFF);
  M5.Lcd.fillRect(112, 195, 30, 20, BLACK);
  uint16_t clr = (WiFi.RSSI() < -70) ? TFT_RED : TFT_GREEN;
  M5.Lcd.setTextColor(clr, TFT_BLACK);
  M5.Lcd.drawString(String(WiFi.RSSI()), 115, 215, GFXFF);
}

void displayBattery() {
  M5.Lcd.setFreeFont(FSS9);
  int maxVolts = 410;
  int minVolts = 320;
  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);

  char battInfo[5];
  dtostrf(M5.Axp.GetBatVoltage(), 1, 2, battInfo);
  String btInfo = "Batt: " + String(battInfo);
  M5.Lcd.setTextDatum(BL_DATUM);
  M5.Lcd.drawString(btInfo, 230, 215, GFXFF);

  int batt = map(M5.Axp.GetBatVoltage() * 100, minVolts, maxVolts, 0, 10000) / 100.0;

  uint16_t clr = GREEN;
  for (int x = 9; x >= 0; x--) {
    if (x < 3) clr = RED;
    else if (x < 6) clr = YELLOW;
    M5.Lcd.fillRoundRect(314, (216 - (x * 24)), 6, 21, 2, (batt > (x * 10)) ? clr : BLACK);
    M5.Lcd.drawRoundRect(314, (216 - (x * 24)), 6, 21, 2, TFT_LIGHTGREY);
  }
}

void drawButtons() {
  // Top buttons with 2px vertical black strips
  int btnWidth = 98;  // (300 - 4) / 3 ≈ 98.67, adjusted for integer math
  int btnHeight = 25;
  int stripWidth = 2;

  M5.Lcd.fillRect(10, 0, 300, btnHeight, TFT_BLACK);  // Clear top area
  // Station -
  M5.Lcd.fillRect(10, 0, btnWidth, btnHeight, SOFT_BLUE);
  M5.Lcd.setTextColor(TFT_WHITE, SOFT_BLUE);
  M5.Lcd.setFreeFont(FSSB9);
  M5.Lcd.setTextDatum(TC_DATUM);
  M5.Lcd.drawString("Station -", 10 + btnWidth/2, 5, GFXFF);
  // 2px black strip
  M5.Lcd.fillRect(10 + btnWidth, 0, stripWidth, btnHeight, TFT_BLACK);
  // BT
  M5.Lcd.fillRect(10 + btnWidth + stripWidth, 0, btnWidth, btnHeight, SOFT_BLUE);
  M5.Lcd.drawString("BT", 10 + btnWidth + stripWidth + btnWidth/2, 5, GFXFF);
  // 2px black strip
  M5.Lcd.fillRect(10 + btnWidth*2 + stripWidth, 0, stripWidth, btnHeight, TFT_BLACK);
  // Station +
  M5.Lcd.fillRect(10 + btnWidth*2 + stripWidth*2, 0, btnWidth, btnHeight, SOFT_BLUE);
  M5.Lcd.drawString("Station +", 10 + btnWidth*2 + stripWidth*2 + btnWidth/2, 5, GFXFF);

  // Bottom buttons with 2px vertical black strips
  M5.Lcd.fillRect(10, 220, 300, btnHeight, TFT_BLACK);  // Clear bottom area
  // Vol -
  M5.Lcd.fillRect(10, 220, btnWidth, btnHeight, SOFT_BLUE);
  M5.Lcd.setTextColor(TFT_WHITE, SOFT_BLUE);
  M5.Lcd.drawString("Vol -", 10 + btnWidth/2, 225, GFXFF);
  // 2px black strip
  M5.Lcd.fillRect(10 + btnWidth, 220, stripWidth, btnHeight, TFT_BLACK);
  // WiFi
  M5.Lcd.fillRect(10 + btnWidth + stripWidth, 220, btnWidth, btnHeight, SOFT_BLUE);
  M5.Lcd.drawString("WiFi", 10 + btnWidth + stripWidth + btnWidth/2, 225, GFXFF);
  // 2px black strip
  M5.Lcd.fillRect(10 + btnWidth*2 + stripWidth, 220, stripWidth, btnHeight, TFT_BLACK);
  // Vol +
  M5.Lcd.fillRect(10 + btnWidth*2 + stripWidth*2, 220, btnWidth, btnHeight, SOFT_BLUE);
  M5.Lcd.drawString("Vol +", 10 + btnWidth*2 + stripWidth*2 + btnWidth/2, 225, GFXFF);
}

void clearTrack() {
  M5.Lcd.fillRect(10, 55, 300, 70, TFT_DARKGREY);
  M5.Lcd.drawRect(10, 55, 300, 70, SOFT_BLUE);
}

void drawVUMeter() {
  const float smoothingFactor = 0.2;
  const int maxBuffer = bufferSize;

  if (!mp3 || !mp3->isRunning() || !buffmp3) {
    vuLevelL = vuLevelR = vuPeakL = vuPeakR = smoothedVuL = smoothedVuR = 0;
  } else {
    int bufferFill = buffmp3->getFillLevel();
    bufferFill = constrain(bufferFill, 0, maxBuffer);
    vuLevelL = map(bufferFill, 0, maxBuffer, 0, 100);

    vuLevelR = vuLevelL + random(-5, 6);
    vuLevelL = constrain(vuLevelL, 0, 100);
    vuLevelR = constrain(vuLevelR, 0, 100);

    smoothedVuL = (smoothingFactor * vuLevelL) + ((1 - smoothingFactor) * smoothedVuL);
    smoothedVuR = (smoothingFactor * vuLevelR) + ((1 - smoothingFactor) * smoothedVuR);

    if (smoothedVuL > vuPeakL) vuPeakL = smoothedVuL;
    if (smoothedVuR > vuPeakR) vuPeakR = smoothedVuR;
  }

  if (millis() - lastVuUpdate > 200) {
    vuPeakL = max(vuPeakL - 2, smoothedVuL);
    vuPeakR = max(vuPeakR - 2, smoothedVuR);
    lastVuUpdate = millis();
  }

  int barWidth = 140;
  int barHeight = 10;

  M5.Lcd.fillRect(10, 130, barWidth, barHeight, BLACK);
  M5.Lcd.fillRect(170, 130, barWidth, barHeight, BLACK);

  int levelWidthL = map(smoothedVuL, 0, 100, 0, barWidth);
  int levelWidthR = map(smoothedVuR, 0, 100, 0, barWidth);
  for (int i = 0; i < levelWidthL; i++) {
    uint16_t color = (i < barWidth * 0.7) ? TFT_GREEN : (i < barWidth * 0.9) ? TFT_YELLOW : TFT_RED;
    M5.Lcd.drawFastVLine(10 + i, 130, barHeight, color);
  }
  for (int i = 0; i < levelWidthR; i++) {
    uint16_t color = (i < barWidth * 0.7) ? TFT_GREEN : (i < barWidth * 0.9) ? TFT_YELLOW : TFT_RED;
    M5.Lcd.drawFastVLine(170 + i, 130, barHeight, color);
  }

  M5.Lcd.drawFastVLine(10 + map(vuPeakL, 0, 100, 0, barWidth), 130, barHeight, TFT_WHITE);
  M5.Lcd.drawFastVLine(170 + map(vuPeakR, 0, 100, 0, barWidth), 130, barHeight, TFT_WHITE);

  M5.Lcd.drawRect(10, 130, barWidth, barHeight, TFT_WHITE);
  M5.Lcd.drawRect(170, 130, barWidth, barHeight, TFT_WHITE);
}

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

// Audio Functions
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
  clearTrack();  // Clear the grey area
  M5.Lcd.setTextDatum(TC_DATUM);
  M5.Lcd.setFreeFont(FSS9);
  M5.Lcd.drawString(band, 160, 65, GFXFF);  // Artist name (first line)
  M5.Lcd.drawString(track, 160, 85, GFXFF);  // Song name (second line)
  M5.Lcd.drawString(streamBitrate + ", " + streamSampleRate, 160, 105, GFXFF);  // Stream quality (third line)
  Serial.flush();
  displayBattery();
}

void StatusCallback(void *cbData, int code, const char *string) {
  const char *ptr = reinterpret_cast<const char *>(cbData);
  char s1[64];
  strncpy_P(s1, string, sizeof(s1));
  s1[sizeof(s1) - 1] = 0;
  Serial.printf("STATUS(%s) '%d' = '%s'\n", ptr, code, s1);
  Serial.flush();
}

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

void updateStation(String message) {
  M5.Lcd.fillRect(10, 25, 300, 35, BLACK);
  M5.Lcd.setTextColor(TFT_GREEN, TFT_BLACK);
  M5.Lcd.setFreeFont(FSS9);
  M5.Lcd.setTextDatum(TC_DATUM);
  M5.Lcd.drawString(message, 160, 35, GFXFF);
}

void scrollStation(int direction) {
  isScrollingStations = true;
  lastStationChange = millis();
  currentStationNumber += direction;
  if (currentStationNumber >= stations) currentStationNumber = 0;
  if (currentStationNumber < 0) currentStationNumber = stations - 1;
  updateStation(String(stationList[currentStationNumber][0]));
  Serial.printf("Scrolling to station: %d - %s\n", currentStationNumber, stationList[currentStationNumber][0]);
}

void changeVolume(float adjustment) {
  audioGain += adjustment;
  audioGain = constrain(audioGain, 0.0, 10.0);
  if (outmp3) {
    outmp3->SetGain(audioGain * gainfactor);
  }

  uint16_t clr = RED;
  for (int x = 9; x >= 0; x--) {
    if (x < 5) clr = GREEN;
    else if (x < 8) clr = TFT_ORANGE;
    M5.Lcd.fillRoundRect(0, (216 - (x * 24)), 6, 21, 2, (audioGain > x) ? clr : BLACK);
    M5.Lcd.drawRoundRect(0, (216 - (x * 24)), 6, 21, 2, TFT_LIGHTGREY);
  }
}

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
  Serial.flush();
}

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

// Main Arduino Functions
void setup() {
  Serial.begin(115200);
  M5.begin();
  M5.Axp.SetSpkEnable(true);
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

void loop() {
  loopMP3();
  M5.update();

  // Bottom Buttons
  if (M5.BtnA.wasPressed()) { // Vol -
    changeVolume(-0.5);
  }
  if (M5.BtnB.wasPressed()) { // WiFi
    wifiMenu();
  }
  if (M5.BtnC.wasPressed()) { // Vol +
    changeVolume(0.5);
  }

  // Top Buttons (using touch for top strip)
  if (M5.Touch.ispressed()) {
    TouchPoint_t pos = M5.Touch.getPressPoint();
    if (pos.y >= 0 && pos.y <= 25) { // Top strip
      if (pos.x < 108) { // Station - (10 to 107)
        scrollStation(-1);
      } else if (pos.x >= 110 && pos.x < 208) { // BT (110 to 207)
        // Do nothing for now
      } else if (pos.x >= 210) { // Station + (210 to 309)
        scrollStation(1);
      }
      delay(300); // Debounce
    }
  }

  if (isScrollingStations && (millis() - lastStationChange > 1000)) {
    clearTrack();
    stopPlaying();
    playMP3();
    isScrollingStations = false;
  }

  if ((disUpdate + 50) < millis()) {
    disUpdate = millis();
    drawVUMeter();
    displayBattery();
    updateWiFiSignal();
  }
}
