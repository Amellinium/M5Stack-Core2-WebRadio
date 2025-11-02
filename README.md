M5Stack-Core2-WebRadio
Web Radio for M5Stack Core2
Current Version: v0.3.3

M5Stack Core2 Web Radio Player

![1](https://github.com/user-attachments/assets/602b2a14-3139-4a75-8280-cf559b150c34)
![2](https://github.com/user-attachments/assets/44867758-ee5f-48ce-82d0-f0f5abc280a4)


Description:
This is a Web Radio Player for the M5Stack Core2 (ESP32).
It is based on m5StreamTest Version 2020.12b by tommyho510@gmail.com
and modified from other sources by bwbguard.
The project adds new features, interface improvements, and better audio handling.

Key Features:
- UI Improvements: Buttons rearranged for a modern and intuitive layout.
- Mute Button: Added a dedicated mute button on the bottom row.
- Wi-Fi Management: Scan for networks, enter password via touchscreen,
  and save credentials for automatic reconnection.
- Load Stations from SD Card: Reads up to 50 stations from stations_list.txt
  in root of SD card.
- Station Navigation: Switch stations up/down via touchscreen or physical buttons.
- Volume Control: Adjustable via buttons or touchscreen.
- Real-Time Display: Shows station info, artist/title metadata, volume,
  Wi-Fi signal, and battery levels.
- Smooth Streaming: 256 KB buffer for uninterrupted playback even on weak Wi-Fi.

SD Card Structure Example:
```
SD_ROOT/
├─ stations_list.txt       # List of stations (Station Name,URL)
```

stations_list.txt Format:
```
Station Name,Stream URL
Example:
BBC Radio 1,http://bbc.co.uk/radio1.mp3
Jazz FM,http://stream.jazzfm.com/live
```

Required Libraries:
- M5Core2 (Core2 touchscreen, buttons, AXP192 power)
- ESP8266Audio by Earle F. Philhower, III, version 2.0.0 (for MP3 decoding & streaming)
- ESP32 by Espressif Systems, version 2.0.17
- WiFi (station mode)
- Preferences (store Wi-Fi credentials)
- SD (load stations list)
- Free_Fonts.h (included with M5Core2 examples)

Original Project and Acknowledgments:
- Modified from M5Stack Core2 Web Media Player by bwbguard
- Project Source: https://github.com/bwbguard/M5Stack-Core2-MediaPlayer
- Based on: m5StreamTest Version 2020.12b by tommyho510@gmail.com
- Core functionality powered by ESP8266Audio

Enjoy!
