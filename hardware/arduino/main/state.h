#ifndef STATE_H
#define STATE_H

// Language definitions | 语言定义
#define LANG_EN 0
#define LANG_ZH 1

#define FINGER_THRESHOLD 50000                                     // Finger detection IR threshold | 手指检测 IR 阈值
#define FINGER_TIMEOUT_MS 10000UL                                  // Finger wait timeout: 10 seconds |  等待手指超时：10 秒
#define COLLECT_TIME_MS 10000UL                                    // Collection duration: 10 seconds | 采集时长：10 秒
#define DISPLAY_UPDATE_MS 100                                      // Screen refresh interval (ms) | 屏幕刷新间隔（毫秒）
#define RECORD_TIME_MS 60000UL                                     // Recording duration: 60 seconds | 录音时长：60 秒
#define SAMPLE_RATE 16000                                          // Audio sample rate | 音频采样率
#define RECORD_BUF_SIZE (SAMPLE_RATE * 2 * RECORD_TIME_MS / 1000)  // Recording buffer size (16-bit samples × duration) | 录音缓冲区大小（16 位采样 × 时长）

/**
 * Page definitions
 * 页面定义
 */
enum Page : uint8_t {
  PAGE_HOME = 0,
  PAGE_DETECT_START,
  PAGE_DETECTING,
  PAGE_DETECT_END,
  PAGE_RECORDING,
  PAGE_RECORD_END,
};

/**
 * Single button state (debounced)
 * 单个按钮状态（已消抖）
 */
struct ButtonState {
  bool pressed;
  bool justPressed;              /// Rising edge this frame | 本帧刚按下的边沿
  bool justReleased;             /// Falling edge this frame / 本帧刚松开的边沿
  uint8_t lastValue;             /// Last raw reading (for debounce) | 上一次原始读数（消抖用）
  unsigned long lastDebounceMs;  /// Last debounce timestamp | 上一次消抖时间戳
};

/**
 * Global application state
 * 全局应用状态
 */
struct AppState {
  Page page;          // Current page | 当前页面
  uint8_t lang;       // Language | 语言
  bool wifiInited;    // WiFi initialized | WiFi 已初始化
  bool timeSynced;    // NTP time synced | NTP 时间已同步
  ButtonState btnK1;  // Button K1 state | 按键 K1 状态
  ButtonState btnK2;  // Button K2 state | 按键 K2 状态
  ButtonState btnK3;  // Button K3 state | 按键 K3 状态
  ButtonState btnK4;  // Button K4 state | 按键 K4 状态
};

/**
 * Global state instance
 * 全局状态实例
 */
extern AppState g_state;

#endif
