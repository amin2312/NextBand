#ifndef UI_H
#define UI_H

#include "state.h"

/**
 * Display home page
 * 显示主界面
 */
void showHomePage();

/**
 * Display WiFi connection status area
 * 显示 WiFi 连接状态区域
 */
void showWiFi();

/**
 * Display current date and time
 * 显示当前日期时间
 */
void showDatetime();

/**
 * Display detection start prompt page
 * 显示检测开始提示界面
 */
void showDetectStartPage();

/**
 * Display collecting page
 * 显示采集中界面
 */
void showDetectingPage();

/**
 * Display detection result page
 * 显示检测结果界面
 */
void showDetectEndPage(float bpm, int beats);

/**
 * Redraw the interface based on current page state
 * 根据当前页面重新绘制界面
 */
void refreshCurrentPage();

/**
 * Update heart rate collection progress bar
 * 更新心率采集进度条
 */
void updateDetectBar(unsigned long elapsed);

/**
 * Display recording page
 * 显示录音界面
 */
void showRecordingPage();

/**
 * Update recording time counter (e.g. 1/60s)
 * 更新录音时间计数（如 1/60秒）
 */
void updateRecordingCounter(unsigned long elapsed);

/**
 * Display recording result page
 * 显示录音结果界面
 */
void showRecordEndPage(bool success);

#endif
