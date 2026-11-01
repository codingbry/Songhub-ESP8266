#pragma once

// Temporary access point used to provision Wi-Fi and SongHub host settings.
#define SETUP_AP_SSID "SongHub-Setup"
#define SETUP_AP_PASSWORD "songhub123"

// Used before the user enters a SongHub port in the setup page.
#define DEFAULT_SONGHUB_PORT 48085

// 4x5 matrix based on the ESP32-C3 wired remote layout.
// I2C display uses D2 (SDA) and D1 (SCL), leaving nine pins for the matrix.
// UART pins are used by the keypad, so this sketch does not use Serial logging.
// Do not hold a keypad button while powering on: D3/D4/D8 are boot-sensitive.
static const uint8_t ROW_PINS[4] = {D0, D5, D6, D8};
static const uint8_t COL_PINS[5] = {D7, D3, D4, RX, TX};

#define CANCEL_HOLD_MS 800
#define WIFI_CONNECT_TIMEOUT_MS 15000
#define WIFI_RETRY_MS 10000
#define SETTINGS_EEPROM_SIZE 128

