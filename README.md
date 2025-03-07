# M5Stack-Core2-WebRadio
Web Radio for M5Stack Core2

M5Stack Core2 Web Radio Player

![1](https://github.com/user-attachments/assets/5196a5e9-3028-4a58-8fbf-7a71b1f55000)
![2](https://github.com/user-attachments/assets/d5c7bede-785b-474e-8563-d73b638b98f7)

This is my updated version of the web radio player for the M5Stack Core2. It’s based on an existing project but has been enhanced with new features and improved functionality.

Key Changes and Enhancements

- Revamped UI: Improvements to the user interface, providing a more modern and intuitive design.
- Station Navigation: Added functionality for seamless switching between radio stations (up and down).
- Volume Control: Integrated volume control to increase or decrease the audio level.
- Wi-Fi Scanning & Password Entry: The app now allows you to scan for available Wi-Fi networks and enter your Wi-Fi password. The device stores the password for easy reconnection. This is a beta release, with ongoing improvements. The on-screen keyboard's accuracy is still being worked on.
- Stream Information: Displays stream bitrate (kbps) and sampling rate (kHz) on the screen for enhanced user information.
- Bluetooth Button: Currently inactive, but in the next release, Bluetooth functionality will be added for streaming to Bluetooth speakers.
- VU Meter: Added a basic VU meter, but due to limitations of the Core2, the functionality is not fully stable and may be removed in future updates.

Original Project and Acknowledgments

This project is based on contributions from various sources. Notably, the M5Stack-Core2-MediaPlayer project served as the foundation by bwbguard and other community members.

Modified from: M5Stack Core2 Web Media Player by bwbguard
- Project Source: [GitHub Repository](https://github.com/bwbguard/M5Stack-Core2-MediaPlayer)
- Based on: m5StreamTest Version 2020.12b by tommyho510@gmail.com

The ESP8266Audio library powers the core functionality for media streaming.

Future Plans

- Improve On-Screen Keyboard: Work on making the keyboard input more accurate.
- Bluetooth Support: Implement Bluetooth streaming.
- VU Meter Optimization: Remove the VU meter due to hardware limitations.
- Loading Stations from SD Card: Load stations from the stations_list file on the SD card.

Enjoy the updates.
