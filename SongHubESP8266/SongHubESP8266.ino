// SongHub Wi-Fi remote for a NodeMCU ESP8266, 4x5 keypad matrix, and SH1106 OLED.
// Libraries: Keypad (Mark Stanley), WebSockets (Markus Sattler/Links2004), U8g2.

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsClient.h>
#include <EEPROM.h>
#include <Wire.h>
#include <Keypad.h>
#include <U8g2lib.h>
#include "config.h"

static const char KEYS[4][5] = {
  {'B', '1', '2', '3', 'M'},
  {'V', '4', '5', '6', 'T'},
  {'v', '7', '8', '9', 't'},
  {'N', 'S', '0', 'P', 'R'},
};

Keypad keypad(makeKeymap(KEYS), (byte*)ROW_PINS, (byte*)COL_PINS, 4, 5);
WebSocketsClient webSocket;
ESP8266WebServer setupServer(80);
U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

struct SavedSettings {
  char magic[4];
  char ssid[33];
  char password[64];
  char host[16];
  uint16_t port;
};

static SavedSettings settings;
static char entry[7] = "";
static bool socketConnected = false;
static bool portalMode = false;
static bool configMode = false;
static bool editingPort = false;
static bool setupKeyWasLong = false;
static bool configError = false;
static char hostEdit[16] = "";
static char portEdit[6] = "";
static unsigned long lastWifiAttempt = 0;
static unsigned long lastDraw = 0;

static void startWebSocket();

static bool validWifiSettings() {
  return memcmp(settings.magic, "SHK1", 4) == 0 &&
         settings.ssid[0] != '\0';
}

static bool hostConfigured() {
  IPAddress parsed;
  return settings.host[0] != '\0' && parsed.fromString(settings.host) && settings.port > 0;
}

static void loadSettings() {
  EEPROM.begin(SETTINGS_EEPROM_SIZE);
  EEPROM.get(0, settings);
  if (!validWifiSettings()) {
    memset(&settings, 0, sizeof(settings));
    memcpy(settings.magic, "SHK1", 4);
    settings.port = DEFAULT_SONGHUB_PORT;
  }
}

static void saveSettings() {
  memcpy(settings.magic, "SHK1", 4);
  EEPROM.put(0, settings);
  EEPROM.commit();
}

static void sendCommand(const char* action) {
  if (!socketConnected) return;
  String message = "{\"action\":\"";
  message += action;
  message += "\",\"payload\":null}";
  webSocket.sendTXT(message);
}

static void sendNumberCommand(const char* action, int payload) {
  if (!socketConnected) return;
  String message = "{\"action\":\"";
  message += action;
  message += "\",\"payload\":";
  message += String(payload);
  message += "}";
  webSocket.sendTXT(message);
}

static void sendSongCommand(const char* action) {
  if (!socketConnected || !entry[0]) return;
  String message = "{\"action\":\"";
  message += action;
  message += "\",\"payload\":\"";
  message += entry;
  message += "\"}";
  webSocket.sendTXT(message);
  entry[0] = '\0';
}

static void beginDeviceConfig() {
  configMode = true;
  editingPort = false;
  strncpy(hostEdit, settings.host, sizeof(hostEdit) - 1);
  hostEdit[sizeof(hostEdit) - 1] = '\0';
  snprintf(portEdit, sizeof(portEdit), "%u", settings.port);
}

static void editDigit(char digit) {
  configError = false;
  char* field = editingPort ? portEdit : hostEdit;
  const size_t capacity = editingPort ? sizeof(portEdit) : sizeof(hostEdit);
  size_t n = strlen(field);
  if (n + 1 < capacity) {
    field[n] = digit;
    field[n + 1] = '\0';
  }
}

static void editBackspace() {
  configError = false;
  char* field = editingPort ? portEdit : hostEdit;
  size_t n = strlen(field);
  if (n) field[n - 1] = '\0';
}

static void saveDeviceConfig() {
  IPAddress parsedIp;
  char* end = nullptr;
  unsigned long parsedPort = strtoul(portEdit, &end, 10);
  if (!parsedIp.fromString(hostEdit) || end == portEdit || *end != '\0' ||
      parsedPort < 1 || parsedPort > 65535) {
    configError = true;
    return;
  }

  parsedIp.toString().toCharArray(settings.host, sizeof(settings.host));
  settings.port = (uint16_t)parsedPort;
  saveSettings();
  configMode = false;
  if (WiFi.status() == WL_CONNECTED) startWebSocket();
}

static void addDigit(char digit) {
  size_t n = strlen(entry);
  if (n >= 6) {
    memmove(entry, entry + 1, 5);
    n = 5;
  }
  entry[n++] = digit;
  entry[n] = '\0';
}

static void backspaceEntry() {
  size_t n = strlen(entry);
  if (n) entry[n - 1] = '\0';
}

static String setupPage() {
  String page;
  page.reserve(1100);
  page += F("<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
            "<title>SongHub Keypad Setup</title><style>body{font:16px Arial;max-width:440px;margin:30px auto;padding:0 16px;background:#111;color:#eee}"
            "input{box-sizing:border-box;width:100%;padding:12px;margin:6px 0 16px;background:#222;color:white;border:1px solid #555;border-radius:8px}"
            "button{padding:12px 18px;background:#1685ff;color:white;border:0;border-radius:8px;font-size:16px}</style></head><body>"
            "<h2>SongHub Wi-Fi Keypad</h2><form method='post' action='/save'>"
            "<label>Wi-Fi name (SSID)</label><input name='ssid' maxlength='32' required>"
            "<label>Wi-Fi password</label><input name='password' type='password' maxlength='63'>"
            "<button type='submit'>Save Wi-Fi and restart</button></form>"
            "<p>Enter the SongHub host IP and WebSocket port on the keypad OLED setup screen.</p></body></html>");
  return page;
}

static void startSetupPortal() {
  if (portalMode) return;
  portalMode = true;
  socketConnected = false;
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(SETUP_AP_SSID, SETUP_AP_PASSWORD);

  setupServer.on("/", HTTP_GET, []() {
    setupServer.send(200, "text/html", setupPage());
  });
  setupServer.on("/save", HTTP_POST, []() {
    String ssid = setupServer.arg("ssid");
    String password = setupServer.arg("password");
    if (!ssid.length() || ssid.length() > 32 || password.length() > 63) {
      setupServer.send(400, "text/plain", "Invalid settings. Check the Wi-Fi name and password.");
      return;
    }
    memset(settings.ssid, 0, sizeof(settings.ssid));
    memset(settings.password, 0, sizeof(settings.password));
    ssid.toCharArray(settings.ssid, sizeof(settings.ssid));
    password.toCharArray(settings.password, sizeof(settings.password));
    saveSettings();
    setupServer.send(200, "text/html", F("<html><body><h2>Saved</h2><p>Restarting keypad...</p></body></html>"));
    delay(500);
    ESP.restart();
  });
  setupServer.begin();
}

static void startWebSocket() {
  webSocket.disconnect();
  socketConnected = false;
  webSocket.onEvent([](WStype_t type, uint8_t* payload, size_t length) {
    (void)payload;
    (void)length;
    switch (type) {
      case WStype_CONNECTED:
        socketConnected = true;
        sendCommand("get_database");
        sendCommand("get_state");
        break;
      case WStype_DISCONNECTED:
      case WStype_ERROR:
        socketConnected = false;
        break;
      default:
        break;
    }
  });
  webSocket.begin(settings.host, settings.port, "/");
  webSocket.setReconnectInterval(3000);
}

static void onKey(char k) {
  if (configMode) {
    if (k >= '0' && k <= '9') editDigit(k);
    else if (k == 'v') editBackspace();
    else if (k == 'N') editingPort = !editingPort;
    else if (k == 'P') {
      configError = false;
      if (editingPort) portEdit[0] = '\0';
      else hostEdit[0] = '\0';
    } else if (k == 'S') {
      configMode = false;
    }
    return;
  }

  if (k >= '0' && k <= '9') {
    addDigit(k);
    return;
  }
  switch (k) {
    case 'B': sendCommand("bgv"); break;
    case 'M': sendCommand("melody"); break;
    case 'V': sendNumberCommand("volume", 10); break;
    case 'v': sendNumberCommand("volume", -10); break;
    case 'T': sendNumberCommand("tempo", 1); break;
    case 't': sendNumberCommand("tempo", -1); break;
    case 'N': sendCommand("next"); break;
    case 'S': sendCommand("stop"); break;
    case 'P': sendCommand("play_pause"); break;
    case 'R': break; // short press / long-press behavior is handled by keypadEvent
    case 'C': break; // not in this 4x5 layout
  }
}

static void keypadEvent(KeypadEvent k) {
  const KeyState state = keypad.getState();
  if (k == 'R') {
    if (state == PRESSED && !configMode) setupKeyWasLong = false;
    else if (state == HOLD && !configMode) {
      setupKeyWasLong = true;
      beginDeviceConfig();
    } else if (state == RELEASED) {
      if (setupKeyWasLong) setupKeyWasLong = false;
      else if (configMode) saveDeviceConfig();
      else sendSongCommand("reserve");
    }
    return;
  }

  if (k == 'B') {
    if (state == PRESSED) setupKeyWasLong = false;
    else if (state == HOLD && !configMode) {
      setupKeyWasLong = true;
      startSetupPortal();
    } else if (state == RELEASED) {
      if (configMode && !editingPort) {
        configError = false;
        size_t n = strlen(hostEdit);
        if (n + 1 < sizeof(hostEdit)) {
          hostEdit[n] = '.';
          hostEdit[n + 1] = '\0';
        }
      } else if (!setupKeyWasLong && !configMode) {
        sendCommand("bgv");
      }
      setupKeyWasLong = false;
    }
    return;
  }

  if (state == PRESSED) onKey(k);
}

static void drawStatus() {
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tf);
  if (configMode) {
    oled.drawStr(0, 10, editingPort ? "SET SONGHUB PORT" : "SET SONGHUB IP");
    oled.drawStr(0, 25, editingPort ? portEdit : hostEdit);
    oled.drawStr(0, 39, editingPort ? "Digits  VOL-=delete" : "Digits  BGV=dot  VOL-=del");
    oled.drawStr(0, 51, "NEXT=IP/PORT P=clear");
    oled.drawStr(0, 63, configError ? "Invalid IP or port" : "RES=save  STOP=cancel");
  } else if (portalMode) {
    oled.drawStr(0, 10, "SETUP: connect phone to");
    oled.drawStr(0, 23, SETUP_AP_SSID);
    oled.drawStr(0, 36, "Open http://192.168.4.1");
    oled.drawStr(0, 52, "Enter Wi-Fi credentials");
  } else {
    oled.drawStr(0, 10, WiFi.status() == WL_CONNECTED ? "Wi-Fi connected" : "Connecting to Wi-Fi...");
    oled.drawStr(0, 22, hostConfigured() ? settings.host : "Hold Reserve to set IP");
    char portLine[24];
    snprintf(portLine, sizeof(portLine), "WebSocket port: %u", settings.port);
    oled.drawStr(0, 34, portLine);
    const char* status = !hostConfigured() ? "SongHub IP not set" :
                         (socketConnected ? "SongHub connected" : "SongHub reconnecting");
    oled.drawStr(0, 46, status);
    char codeLine[20];
    snprintf(codeLine, sizeof(codeLine), "Code: %s", entry[0] ? entry : "------");
    oled.drawStr(0, 60, codeLine);
  }
  oled.sendBuffer();
}

void setup() {
  Wire.begin(D2, D1); // SDA, SCL on NodeMCU
  oled.begin();
  oled.setContrast(180);

  keypad.setDebounceTime(35);
  keypad.setHoldTime(CANCEL_HOLD_MS);
  keypad.addEventListener(keypadEvent);

  loadSettings();
  if (!validWifiSettings()) {
    startSetupPortal();
  } else {
    WiFi.mode(WIFI_STA);
    WiFi.begin(settings.ssid, settings.password);
    lastWifiAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - lastWifiAttempt < WIFI_CONNECT_TIMEOUT_MS) {
      delay(100);
    }
    if (WiFi.status() == WL_CONNECTED) {
      if (hostConfigured()) startWebSocket();
    } else {
      startSetupPortal();
    }
  }
  drawStatus();
}

void loop() {
  keypad.getKeys();

  if (portalMode) {
    setupServer.handleClient();
  } else if (WiFi.status() == WL_CONNECTED && hostConfigured()) {
    webSocket.loop();
  } else if (millis() - lastWifiAttempt >= WIFI_RETRY_MS) {
    WiFi.reconnect();
    lastWifiAttempt = millis();
  }

  if (millis() - lastDraw >= 250) {
    drawStatus();
    lastDraw = millis();
  }
}

