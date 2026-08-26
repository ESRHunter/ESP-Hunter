#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ======================================================================
// АППАРАТНЫЕ ПИНЫ ESP32-S3
// ======================================================================
#define TFT_SCLK       12
#define TFT_MOSI       11
#define SD_MISO        13
#define TFT_CS         10
#define TFT_DC          9
#define TFT_RST         8
#define SD_CS          15
#define PN532_SDA       1
#define PN532_SCL       2
#define BTN_UP          4
#define BTN_DOWN        5
#define BTN_OK          3
#define BTN_ESC         6
#define IR_TX_PIN       7
#define IR_RX_PIN      18
#define BACKLIGHT_PIN  21   // Управление подсветкой (PWM)
#define BUZZER_PIN     14  


// ======================================================================
// ГЛОБАЛЬНЫЕ КОНСТАНТЫ И МАКРОСЫ
// ======================================================================
#define SCREEN_WIDTH        128
#define SCREEN_HEIGHT       128
#define MAX_VISIBLE_ITEMS   4
#define RFID_SLOTS_MAX      10
#define MAX_CAPTURED_LINES  50
#define MAX_SAVED_IR        30

const byte DNS_PORT = 53;

// Цветовые константы (ST77XX)
#define COLOR_BLACK     0x0000
#define COLOR_WHITE     0xFFFF
#define COLOR_RED       0xF800
#define COLOR_GREEN     0x07E0
#define COLOR_BLUE      0x001F
#define COLOR_YELLOW    0xFFE0
#define COLOR_CYAN      0x07FF
#define COLOR_MAGENTA   0xF81F
#define COLOR_DARKGREY  0x39E7

#endif // CONFIG_H
