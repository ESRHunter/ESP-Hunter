#ifndef RADAR_MODE_H
#define RADAR_MODE_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <WiFi.h>
#include "Config.h"

struct RadarTarget {
  char ssid[18];
  int32_t rssi;
  float angle;
  uint16_t distance;
  bool active;
};
//updateTargets
#define MAX_RADAR_TARGETS 12

class WifiRadar {
private:
  RadarTarget targets[MAX_RADAR_TARGETS];
  int targetCount = 0;
  float scanAngle = 0.0;
  unsigned long lastScanTime = 0;
  const unsigned long SCAN_INTERVAL = 5000;

  uint16_t rssiToDistance(int32_t rssi) {
    if (rssi >= -30) return 6;
    if (rssi <= -95) return 52;
    return map(rssi, -30, -95, 6, 52);
  }

  float calculateTargetAngle(const String& identifier, int index) {
    uint32_t hash = 0;
    for (size_t i = 0; i < identifier.length(); i++) {
      hash = hash * 31 + identifier[i];
    }
    float baseAngle = (hash % 360) * (PI / 180.0f);
    return baseAngle;
  }

public:
  void init() {
    targetCount = 0;
    scanAngle = 0.0;
    lastScanTime = 0;
  }

  // ИСПРАВЛЕННЫЙ МЕТОД – синхронное сканирование
  void updateTargets() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(50);

    // Синхронное сканирование с таймаутом 3 секунды
    int count = WiFi.scanNetworks(false, false, false, 3000);

    if (count > 0) {
      targetCount = min(count, MAX_RADAR_TARGETS);
      for (int i = 0; i < targetCount; i++) {
        String ssid = WiFi.SSID(i);
        if (ssid.length() == 0) ssid = "[Hidden]";
        
        snprintf(targets[i].ssid, sizeof(targets[i].ssid), "%s", ssid.c_str());
        targets[i].rssi = WiFi.RSSI(i);
        targets[i].distance = rssiToDistance(targets[i].rssi);
        targets[i].angle = calculateTargetAngle(WiFi.BSSIDstr(i), i);
        targets[i].active = true;
      }
      WiFi.scanDelete();
    } else {
      targetCount = 0;
    }
  }

  void draw(GFXcanvas16 &canvas, uint16_t accentColor) {
    const int centerX = 64;
    const int centerY = 64;
    const int maxRadius = 54;

    canvas.fillScreen(COLOR_BLACK);

    canvas.drawCircle(centerX, centerY, 15, COLOR_DARKGREY);
    canvas.drawCircle(centerX, centerY, 32, COLOR_DARKGREY);
    canvas.drawCircle(centerX, centerY, maxRadius, accentColor);

    canvas.drawFastHLine(centerX - maxRadius, centerY, maxRadius * 2, COLOR_DARKGREY);
    canvas.drawFastVLine(centerX, centerY - maxRadius, maxRadius * 2, COLOR_DARKGREY);

    scanAngle += 0.15f;
    if (scanAngle >= TWO_PI) scanAngle -= TWO_PI;

    int sweepX = centerX + cos(scanAngle) * maxRadius;
    int sweepY = centerY + sin(scanAngle) * maxRadius;
    canvas.drawLine(centerX, centerY, sweepX, sweepY, accentColor);

    for (int i = 0; i < targetCount; i++) {
      if (!targets[i].active) continue;

      int targetX = centerX + cos(targets[i].angle) * targets[i].distance;
      int targetY = centerY + sin(targets[i].angle) * targets[i].distance;

      float angleDiff = fabs(scanAngle - targets[i].angle);
      if (angleDiff > PI) angleDiff = TWO_PI - angleDiff;

      if (angleDiff < 0.3f) {
        canvas.fillCircle(targetX, targetY, 3, COLOR_WHITE);
        canvas.drawCircle(targetX, targetY, 5, accentColor);
      } else {
        canvas.fillRect(targetX - 1, targetY - 1, 3, 3, COLOR_GREEN);
      }
    }

    canvas.setTextColor(accentColor);
    canvas.setTextSize(1);
    canvas.setCursor(2, 2);
    canvas.print("RADAR");

    canvas.setTextColor(COLOR_WHITE);
    canvas.setCursor(85, 2);
    canvas.printf("DEV:%d", targetCount);

    canvas.setCursor(2, 118);
    canvas.print("ESC:Exit");

    if (millis() - lastScanTime > SCAN_INTERVAL) {
      lastScanTime = millis();
      updateTargets();
    }
  }
};

#endif // RADAR_MODE_H