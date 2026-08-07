#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include "test.h"

extern Adafruit_ST7735 tft;
extern U8G2_FOR_ADAFRUIT_GFX u8g2;

void testShow() {
  tft.fillScreen(ST7735_RED);
  delay(500);
  tft.fillScreen(ST7735_GREEN);
  delay(500);
  tft.fillScreen(ST7735_BLUE);
  delay(500);
  tft.fillScreen(ST7735_BLACK);
  delay(500);
  tft.fillScreen(ST7735_WHITE);
  delay(500);

  tft.fillScreen(ST7735_BLUE);
  u8g2.setCursor(6, 18);
  u8g2.print("Screen Test | 屏幕测试");
  u8g2.setCursor(6, 36);
  u8g2.print("Red Green Blue Black White");
  u8g2.setCursor(6, 54);
  u8g2.print("红 绿 蓝 黑 白");
  u8g2.setCursor(6, 72);
  u8g2.print("Display OK!");
  u8g2.setCursor(6, 108);
  u8g2.print("屏幕显示正常");
}

void testNetwork() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
  const char* url = "http://www.bing.com/";
  Serial.printf("testNetwork: GET %s …\n", url);

  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();
  if (httpCode > 0) {
    Serial.printf("testNetwork: internet OK (HTTP %d)\n", httpCode);
    String payload = http.getString();
    if (payload.length() > 0) {
      Serial.println("--- first line of response ---");
      int nl = payload.indexOf('\n');
      if (nl > 0) {
        payload = payload.substring(0, nl);
      }
      Serial.println(payload.c_str());
      Serial.println("--- end ---");
    }
  } else {
    Serial.printf("testNetwork: request failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();
}
