#include <WiFi.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include "ui.h"

#define LINE_H 14
#define TOP_Y 14
#define STATUS_Y 110
#define MARGIN_X 2

extern Adafruit_ST7735 tft;
extern U8G2_FOR_ADAFRUIT_GFX u8g2;

float lastDetectBPM = 0;
int lastDetectBeats = 0;

/**
 * Draw multi-line text, supports \n newline
 * 绘制多行文字，支持 \n 换行符
 */
void u8g2_printf_line(int x, int y, const char* txt) {
  const char* lineStart = txt;
  while (*lineStart) {
    const char* lineEnd = strchr(lineStart, '\n');
    if (lineEnd) {
      const size_t len = lineEnd - lineStart;
      char buf[128];
      memcpy(buf, lineStart, len);
      buf[len] = '\0';
      u8g2.drawUTF8(x, y, buf);
      lineStart = lineEnd + 1;
    } else {
      u8g2.drawUTF8(x, y, lineStart);
      break;
    }
    y += LINE_H;
  }
}

/**
 * Display current date and time at the top of the screen
 * 在屏幕顶部显示当前日期时间
 */
void showDatetime() {
  tft.fillRect(0, 0, tft.width(), LINE_H, ST77XX_BLUE);

  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d  %02d:%02d:%02d",
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    u8g2_printf_line(MARGIN_X, TOP_Y, buf);
  }
}

/**
 * Display WiFi connection status at the bottom of the screen
 * 在屏幕底部区域显示 WiFi 连接状态
 */
void showWiFi() {
  tft.fillRect(0, STATUS_Y, tft.width(), LINE_H, ST77XX_BLUE);

  if (!g_state.wifiInited) {
    if (g_state.lang == LANG_ZH)
      u8g2_printf_line(MARGIN_X, STATUS_Y + LINE_H, "WiFi: 初始化中...");
    else
      u8g2_printf_line(MARGIN_X, STATUS_Y + LINE_H, "WiFi: Init...");

  } else if (WiFi.status() == WL_CONNECTED) {
    if (g_state.lang == LANG_ZH)
      u8g2_printf_line(MARGIN_X, STATUS_Y + LINE_H, "WiFi: 已连接");
    else
      u8g2_printf_line(MARGIN_X, STATUS_Y + LINE_H, "WiFi: Connected");

  } else {
    if (g_state.lang == LANG_ZH)
      u8g2_printf_line(MARGIN_X, STATUS_Y + LINE_H, "WiFi: 未连接");
    else
      u8g2_printf_line(MARGIN_X, STATUS_Y + LINE_H, "WiFi: Disconnected");
  }
}

/**
 * Display main page (HOME)
 * 显示主界面（HOME 页）
 */
void showHomePage() {
  g_state.page = PAGE_HOME;
  tft.fillScreen(ST77XX_BLUE);

  showDatetime();

  if (g_state.lang == LANG_ZH) {
    u8g2_printf_line(MARGIN_X, 28, "欢迎使用 NextBand");
    u8g2_printf_line(MARGIN_X, 42, "按钮1：语音输入你的\n意图");
    u8g2_printf_line(MARGIN_X, 70, "按钮2：心率检测");
    u8g2_printf_line(MARGIN_X, 84, "按钮3：English/中文");
  } else {
    u8g2_printf_line(MARGIN_X, 28, "Welcome to NextBand");
    u8g2_printf_line(MARGIN_X, 42, "Btn 1: Voice input \nyour intents");
    u8g2_printf_line(MARGIN_X, 70, "Btn 2: Heart rate \ndetection");
    u8g2_printf_line(MARGIN_X, 98, "Btn 3: English/中文");
  }

  showWiFi();
}

/**
 * Display detection start prompt page
 * 显示检测开始提示界面
 */
void showDetectStartPage() {
  g_state.page = PAGE_DETECT_START;
  tft.fillScreen(ST77XX_BLUE);
  if (g_state.lang == LANG_ZH) {
    u8g2_printf_line(MARGIN_X, TOP_Y, "请放置你的手指在\n传感器上");
  } else {
    u8g2_printf_line(MARGIN_X, TOP_Y, "Please place your finger\non the sensor.");
  }
  showWiFi();
}

/**
 * Draw heart rate collection progress bar
 * 绘制心率采集进度条
 */
void updateDetectBar(unsigned long elapsed) {
  int x = 4;
  int y = 56;
  int w = 120;
  int h = 12;
  tft.drawRect(x, y, w, h, ST77XX_WHITE);
  int fillW = map(elapsed, 0, COLLECT_TIME_MS, 0, w);
  if (fillW > w) fillW = w;
  tft.fillRect(x, y, fillW, h, ST77XX_WHITE);
}

/**
 * Display collecting page
 * 显示采集中界面
 */
void showDetectingPage() {
  g_state.page = PAGE_DETECTING;
  tft.fillScreen(ST77XX_BLUE);
  if (g_state.lang == LANG_ZH) {
    u8g2_printf_line(MARGIN_X, TOP_Y, "采集中...");
  } else {
    u8g2_printf_line(MARGIN_X, TOP_Y, "Collecting...");
  }
  showWiFi();
}

/**
 * Display detection result page
 * 显示检测结果界面
 */
void showDetectEndPage(float bpm, int beats) {
  g_state.page = PAGE_DETECT_END;
  lastDetectBPM = bpm;
  lastDetectBeats = beats;
  tft.fillScreen(ST77XX_BLUE);

  if (g_state.lang == LANG_ZH) {
    u8g2_printf_line(MARGIN_X, TOP_Y, "检测完成");

    char buf[64];
    if (bpm > 0) {
      snprintf(buf, sizeof(buf), "心率: %d BPM", (int)bpm);
    } else {
      snprintf(buf, sizeof(buf), "心率: 无有效数据");
    }
    u8g2_printf_line(MARGIN_X, 28, buf);

    snprintf(buf, sizeof(buf), "检测到心跳: %d 次", beats);
    u8g2_printf_line(MARGIN_X, 42, buf);

    u8g2_printf_line(MARGIN_X, 70, "按钮2返回");
  } else {
    u8g2_printf_line(MARGIN_X, TOP_Y, "Detection Complete");

    char buf[64];
    if (bpm > 0) {
      snprintf(buf, sizeof(buf), "Heart Rate: %d BPM", (int)bpm);
    } else {
      snprintf(buf, sizeof(buf), "Heart Rate: No valid data");
    }
    u8g2_printf_line(MARGIN_X, 28, buf);

    snprintf(buf, sizeof(buf), "Beats detected: %d", beats);
    u8g2_printf_line(MARGIN_X, 42, buf);

    u8g2_printf_line(MARGIN_X, 70, "Press btn 2 to return");
  }

  showWiFi();
}

/**
 * Display recording page
 * 显示录音界面
 */
void showRecordingPage() {
  g_state.page = PAGE_RECORDING;
  tft.fillScreen(ST77XX_BLUE);
  if (g_state.lang == LANG_ZH) {
    u8g2_printf_line(MARGIN_X, TOP_Y, "录音中...");
    u8g2_printf_line(MARGIN_X, 70, "按钮1: 结束录音");
  } else {
    u8g2_printf_line(MARGIN_X, TOP_Y, "Recording...");
    u8g2_printf_line(MARGIN_X, 70, "Press btn 1 to stop");
  }
  showWiFi();
}

/**
 * Draw recording time counter
 * 绘制录音时间计数
 */
void updateRecordingCounter(unsigned long elapsed) {
  int x = MARGIN_X;
  int y = 42;
  // Clear previous counter area | 清除上一次计数区域
  tft.fillRect(x, y - 2, tft.width() - x, LINE_H + 4, ST77XX_BLUE);

  int sec = (int)(elapsed / 1000) + 1;  // 1-based second counter | 从 1 开始的秒计数
  if (sec > 60) sec = 60;

  char buf[32];
  if (g_state.lang == LANG_ZH) {
    snprintf(buf, sizeof(buf), "%d/60 秒", sec);
  } else {
    snprintf(buf, sizeof(buf), "%d/60 s", sec);
  }
  u8g2_printf_line(x, y, buf);
}

/**
 * Display recording result page
 * 显示录音结果界面
 */
void showRecordEndPage(bool success) {
  g_state.page = PAGE_RECORD_END;
  tft.fillScreen(ST77XX_BLUE);

  if (g_state.lang == LANG_ZH) {
    u8g2_printf_line(MARGIN_X, TOP_Y, "录音结束");
    u8g2_printf_line(MARGIN_X, 70, "再次按下按钮1返回");
  } else {
    u8g2_printf_line(MARGIN_X, TOP_Y, "Recording ended");
    u8g2_printf_line(MARGIN_X, 70, "Press btn 1 again \nto return");
  }
  showWiFi();
}

/**
 * Redraw the interface based on current page state
 * 根据当前页面状态重新绘制界面
 */
void refreshCurrentPage() {
  switch (g_state.page) {
    case PAGE_HOME: showHomePage(); break;
    case PAGE_DETECT_START: showDetectStartPage(); break;
    case PAGE_DETECTING: showDetectingPage(); break;
    case PAGE_DETECT_END: showDetectEndPage(lastDetectBPM, lastDetectBeats); break;
    case PAGE_RECORDING: showRecordingPage(); break;
    default: showHomePage(); break;
  }
}
