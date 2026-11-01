# SongHub Wi-Fi Remote for ESP8266

This project turns a NodeMCU ESP8266 and a 4×5 button keypad into a Wi-Fi
remote for SongHub. It uses a 128×64 SH1106 I²C OLED to show setup and
connection status. The SongHub IP address is entered on the keypad and starts
blank, so each user can set the address for their own network.

## What you need

- NodeMCU ESP8266 development board with USB connection
- 4×5 matrix keypad wired like the SongHub ESP32-C3 remote
- 128×64 SH1106 I²C OLED module
- USB data cable
- Computer with Arduino IDE 2
- SongHub host device and ESP8266 connected to the same local Wi-Fi network

## 1. Wire the display and keypad

Power off the NodeMCU before wiring.

### OLED

| OLED pin | NodeMCU pin |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SDA | D2 |
| SCL | D1 |

Use an **SH1106 128×64 I²C OLED**. The sketch reserves D1 and D2 for I²C.

### 4×5 keypad matrix

Connect the keypad's four row wires and five column wires in this order:

| Keypad wire | NodeMCU pin |
|---|---|
| Row 1 | D0 |
| Row 2 | D5 |
| Row 3 | D6 |
| Row 4 | D8 |
| Column 1 | D7 |
| Column 2 | D3 |
| Column 3 | D4 |
| Column 4 | RX |
| Column 5 | TX |

The keypad layout, left to right, is:

| Row | Buttons |
|---|---|
| 1 | BGV, 1, 2, 3, Melody |
| 2 | Volume +, 4, 5, 6, Tempo + |
| 3 | Volume −, 7, 8, 9, Tempo − |
| 4 | Next, Stop, 0, Play/Pause, Reserve |

**Important:** D3, D4, and D8 affect ESP8266 startup mode. Don’t hold a keypad
button while powering on or resetting the board. RX and TX are also used by the
keypad, so the sketch does not use Serial Monitor logging. If USB uploading
fails with the keypad connected, temporarily disconnect the keypad wires from
RX and TX (or disconnect the matrix), upload, then reconnect them.

## 2. Install Arduino IDE and ESP8266 board support

1. Install [Arduino IDE 2](https://www.arduino.cc/en/software) for your computer.
2. Open Arduino IDE settings/preferences.
3. Add this URL to **Additional Boards Manager URLs**:

   `https://arduino.esp8266.com/stable/package_esp8266com_index.json`

4. Open **Boards Manager**, search for **esp8266**, and install **esp8266 by ESP8266 Community**.
5. In the board selector, choose **NodeMCU 1.0 (ESP-12E Module)**.

These steps follow the [ESP8266 Arduino Core installation guide](https://arduino-esp8266.readthedocs.io/en/latest/installing.html).

## 3. Install the required libraries

In Arduino IDE, open **Tools → Manage Libraries** (or **Sketch → Include Library → Manage Libraries**), search for each library, and install:

- **Keypad** by Mark Stanley
- **WebSockets** by Markus Sattler / Links2004
- **U8g2** by olikraus

The ESP8266 Wi-Fi, EEPROM, I²C, and web-server libraries are included with the ESP8266 board package.

## 4. Upload the sketch to the ESP8266

1. Unzip the project package if you received it as a ZIP file.
2. Open `SongHubESP8266/SongHubESP8266.ino` in Arduino IDE. Keep `config.h` in the same folder as the `.ino` file.
3. Connect the NodeMCU to the computer with a USB **data** cable.
4. In Arduino IDE, select **NodeMCU 1.0 (ESP-12E Module)** as the board.
5. Select the serial port that appeared when you connected the board (for example, `COM4` on Windows or `/dev/ttyUSB0` on Linux).
6. Click **Upload** and wait for the IDE to report that the upload is complete.
7. If the upload fails, close Serial Monitor, check the cable/port, and try disconnecting the keypad matrix—especially its RX and TX wires—during upload.

The NodeMCU normally enters upload mode automatically. The separate Espressif Flash Download Tool and `.bin` files in the other SongHub folder are **not** used for this sketch.

## 5. Set the Wi-Fi network

On first startup, or if the saved Wi-Fi network can’t be reached, the OLED asks you to configure Wi-Fi:

1. On a phone or laptop, connect to the ESP8266 access point **SongHub-Setup**.
2. Enter its password: `songhub123`.
3. Open `http://192.168.4.1` in the phone/laptop browser.
4. Enter the Wi-Fi network name (SSID) and password, then save. The ESP8266 restarts and joins that network.

To reopen Wi-Fi setup later, hold the keypad’s **BGV** button for about 0.8 seconds.

## 6. Enter the SongHub IP address and port on the remote

The SongHub host IP is intentionally blank until entered on the ESP8266. Find the host’s current local Wi-Fi IP in SongHub or the host device’s network settings. Keep the host and keypad on the same network.

1. Hold **Reserve** for about 0.8 seconds to open the OLED address editor.
2. Enter the IP digits. Press **BGV** between octets to enter dots, for example `192`, **BGV**, `168`, **BGV**, `1`, **BGV**, `50`.
3. Press **Next** to switch to the port field. The default is `48085`; change it if SongHub shows a different WebSocket port.
4. Press **Reserve** to validate and save. Press **Stop** to cancel.

While editing, **Volume −** deletes the last character and **Play/Pause** clears the current field. The OLED reports an invalid IP or port so it can be corrected.

The browser control page and WebSocket use different ports. The browser page is commonly on `48081`; this keypad connects to the SongHub **WebSocket port** (the working setup discussed for this project used `48085`). Enter the WebSocket port, not the browser page port.

## 7. Use the remote

- Number buttons enter a song code locally, up to six digits.
- **Reserve** sends the entered song code to SongHub.
- **Play/Pause**, **Stop**, and **Next** control playback.
- **Volume ±**, **Tempo ±**, **Melody**, and **BGV** send their corresponding SongHub commands.
- The OLED shows Wi-Fi status, SongHub host and port, connection status, and the entered song code.

The 4×5 layout does not have dedicated Key ±, MPX, Play Now, or Priority buttons.

## Troubleshooting

- **OLED says SongHub IP not set:** Hold Reserve and enter the host IP and WebSocket port.
- **OLED says SongHub reconnecting:** Confirm the IP is current, the port is the WebSocket port, SongHub’s remote server is enabled, and both devices are on the same Wi-Fi network.
- **Can’t see SongHub-Setup:** The ESP8266 may have saved Wi-Fi settings. Hold BGV for about 0.8 seconds to reopen the setup access point.
- **Upload fails or times out:** Use a USB data cable, select the correct port and board, close Serial Monitor, and disconnect the keypad from RX/TX while uploading.
- **Buttons trigger the wrong actions:** Check that the keypad rows and columns follow the pin order above; the code assumes this exact 4×5 layout.

## Current hardware status

This firmware and pin layout have not yet been compiled or tested on the physical NodeMCU/keypad/OLED. Confirm the board variant, keypad wire order, OLED controller, and boot behavior before distributing a flashed unit.
