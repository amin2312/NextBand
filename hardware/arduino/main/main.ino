#include <ESP.h>
#include <SPI.h>
#include <WiFi.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include "DevLab_MAX30102.h"
#include <driver/i2s.h>
#include "heartRate.h"
#include "test.h"
#include "ui.h"
#include "task.h"

// Pin definitions | 引脚定义
#define TFT_SCLK 12
#define TFT_MOSI 11
#define TFT_RST 4
#define TFT_DC 5
#define TFT_CS 6

#define BUTTON_K4 7
#define BUTTON_K3 15
#define BUTTON_K2 16
#define BUTTON_K1 17

#define MAX30102_SDA 18
#define MAX30102_SCL 8

#define INMP441_SCK 9
#define INMP441_WS 10
#define INMP441_SD 46

// Component instances | 元件实例
// Adafruit_ST7735 tft = Adafruit_ST7735(TFT_DC, TFT_RST, TFT_MOSI, TFT_SCLK); // Software SPI | 软件 SPI
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);  // Hardware SPI | 硬件 SPI
U8G2_FOR_ADAFRUIT_GFX u8g2;
DevLab_MAX30102 particleSensor;  // Heart rate / SpO2 sensor | 心率/血氧传感器

AppState g_state = { PAGE_HOME, LANG_ZH };

struct HeartRateData {
  static constexpr byte RATE_SIZE = 4;  // Sliding window size for BPM averaging | BPM 滑动平均窗口大小
  byte rates[4] = { 0 };                // Ring buffer of recent beat-to-beat BPM values | 最近逐拍 BPM 值的环形缓冲区
  byte rateSpot = 0;                    // Current write position in rates ring buffer | rates 环形缓冲区当前写入位置
  long lastBeat = 0;                    // Timestamp of the last detected beat (ms) | 上一次检测到心跳的时间戳（毫秒）
  int beatCount = 0;                    // Total number of beats detected | 已检测到的心跳总数
  float bpm = 0;                        // Current averaged BPM (beats per minute) | 当前滑动平均 BPM（每分钟心跳数）
  long rateSum = 0;                     // Running sum of valid BPM values in the window | 滑动窗口内有效 BPM 值的运行总和
  byte validRateCount = 0;              // Number of valid BPM entries in the window | 窗口内有效 BPM 条目数
  unsigned long startTime = 0;          // Detection start timestamp (ms) | 检测开始时间戳（毫秒）
  unsigned long updatedTime = 0;        // Last UI refresh timestamp (ms) | 上次 UI 刷新时间戳（毫秒）
};
HeartRateData g_hrData;

struct AudioRecordData {
  uint8_t *buffer;            // Complete WAV buffer (44-byte header + PCM data) | WAV 完整缓冲区（44 字节头 + PCM 数据）
  size_t bufferSize;          // Allocated size = RECORD_BUF_SIZE + 44 | 已分配大小 = RECORD_BUF_SIZE + 44
  size_t dataOffset;          // Current write offset (starts from 44) | 当前写入偏移（从 44 开始）
  unsigned long startTime;    // Recording start timestamp (ms) | 录音开始时间戳（毫秒）
  unsigned long updatedTime;  // Last time the time counter was updated | 时间计数上次刷新时间（毫秒）
};
AudioRecordData g_arData;

/**
 * Read and debounce a single button, update the corresponding ButtonState
 * 消抖读取单个按钮，更新对应的 ButtonState
 */
void readButton(uint8_t pin, ButtonState &btn) {
  unsigned long now = millis();
  if (now - btn.lastDebounceMs < 30) {  // Within 30ms debounce window, only clear edges | 30ms 消抖窗口内只清除边沿
    btn.justPressed = 0;
    btn.justReleased = 0;
    return;
  }
  btn.lastDebounceMs = now;
  bool v = (digitalRead(pin) == LOW);
  if (v && !btn.lastValue) {  // Just pressed | 刚按下
    btn.pressed = true;
    btn.justPressed = true;
    btn.justReleased = false;
  } else if (!v && btn.lastValue) {  // Just released | 刚松开
    btn.pressed = false;
    btn.justPressed = false;
    btn.justReleased = true;
  } else {
    btn.justPressed = false;
    btn.justReleased = false;
  }
  btn.lastValue = v;
}

/**
 * Read all button states (K1~K4)
 * 统一读取所有按钮状态（K1~K4）
 */
static void readButtons() {
  readButton(BUTTON_K1, g_state.btnK1);
  readButton(BUTTON_K2, g_state.btnK2);
  readButton(BUTTON_K3, g_state.btnK3);
  readButton(BUTTON_K4, g_state.btnK4);
}

/**
 * Wait for finger placement on sensor (polling mode)
 * 等待手指放置在传感器上（轮询方式）
 */
bool waitForFinger(unsigned long timeoutMs) {
  unsigned long startTime = millis();
  while (millis() - startTime < timeoutMs) {
    // Check MAX30102 FIFO | 检查 MAX30102 FIFO
    particleSensor.check();
    // Iterate all available samples | 遍历所有可用采样
    while (particleSensor.available()) {
      if (millis() - startTime > timeoutMs)
        return false;
      long irValue = particleSensor.getIR();  // Get IR data | 获取 IR 数据
      if (irValue > FINGER_THRESHOLD) {
        return true;
      }
      particleSensor.nextSample();
    }
    delay(10);
  }
  return false;
}

/**
 * Initialize heart rate detection state
 * 初始化心率检测状态
 */
void startHeartRateDetection() {
  g_hrData = HeartRateData{};
  g_hrData.startTime = millis();
  particleSensor.clearFIFO();
}

/**
 * Process MAX30102 heart rate sample data (polling mode)
 * 处理 MAX30102 心率采样数据（轮询方式）
 */
void processHeartRateSamples() {
  // Check MAX30102 FIFO | 检查 MAX30102 FIFO
  particleSensor.check();
  // Iterate all available samples | 遍历所有可用采样
  while (particleSensor.available()) {
    // Check if collection time is up | 检查采集时间是否已到
    unsigned long elapsed = millis() - g_hrData.startTime;
    if (elapsed >= COLLECT_TIME_MS) {
      endHeartRateDetection();
      showDetectEndPage(g_hrData.bpm, g_hrData.beatCount);
      return;
    }
    // Periodic progress bar refresh | 定期刷新进度条
    if (elapsed - g_hrData.updatedTime >= DISPLAY_UPDATE_MS) {
      g_hrData.updatedTime = elapsed;
      updateDetectBar(elapsed);
    }
    long irValue = particleSensor.getIR();  // Get IR data | 获取 IR 数据
    if (irValue < FINGER_THRESHOLD) {
      g_hrData.bpm = 0;
      particleSensor.nextSample();
      continue;
    }
    // Check if this is a heartbeat | 检测是不是心跳
    if (checkForBeat(irValue)) {
      g_hrData.beatCount++;
      unsigned long now = millis();
      if (g_hrData.lastBeat != 0) {
        long delta = now - g_hrData.lastBeat;
        if (delta > 0) {
          float bpm = 60000.0f / (float)delta;
          if (bpm > 20 && bpm < 255) {
            byte newRate = (byte)bpm;
            // Sliding window: running sum incremental update | 滑动窗口：运行总和增量更新
            g_hrData.rateSum += newRate - g_hrData.rates[g_hrData.rateSpot];
            g_hrData.rates[g_hrData.rateSpot] = newRate;
            g_hrData.rateSpot = (g_hrData.rateSpot + 1) % HeartRateData::RATE_SIZE;
            if (g_hrData.validRateCount < HeartRateData::RATE_SIZE)
              g_hrData.validRateCount++;
            g_hrData.bpm = g_hrData.rateSum / g_hrData.validRateCount;
          }
          Serial.printf("Beat #%d | IR=%ld | BPM=%.1f | Avg=%.1f\n", g_hrData.beatCount, irValue, bpm, g_hrData.bpm);
        }
      }
      g_hrData.lastBeat = now;
    }
    particleSensor.nextSample();
  }
}

/**
 * End heart rate detection and trigger async upload
 * 结束心率检测并触发异步上传
 */
void endHeartRateDetection() {
  Serial.printf("Detection done: %d beats, %.1f BPM\n", g_hrData.beatCount, g_hrData.bpm);

  HeartRateUploadParams *params = new HeartRateUploadParams;
  params->bpm = g_hrData.bpm;
  params->beatCount = g_hrData.beatCount;
  xTaskCreate(uploadHeartRateTask, "uploadHR", 8192, params, 1, NULL);
}

/**
 * Initialize INMP441 I2S microphone
 * 初始化 INMP441 I2S 麦克风
 */
bool initINMP441() {
  // I2S driver configuration | I2S 驱动配置
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,  // 采样率
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0,
  };
  // Install I2S driver | 安装 I2S 驱动
  esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("I2S driver install failed: %d\n", err);
    return false;
  }
  // I2S pin mapping | I2S 引脚映射
  i2s_pin_config_t pin_config = {
    .bck_io_num = INMP441_SCK,
    .ws_io_num = INMP441_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = INMP441_SD,
  };
  i2s_set_pin(I2S_NUM_0, &pin_config);
  Serial.println("I2S microphone (INMP441) initialized");
  return true;
}

/**
 * Start audio recording
 * 开始录音
 */
bool startRecording() {
  size_t allocSize = RECORD_BUF_SIZE + 44;
  g_arData.buffer = (uint8_t *)ps_malloc(allocSize);
  if (!g_arData.buffer) {
    Serial.println("Record buffer allocation failed (OOM)");
    i2s_stop(I2S_NUM_0);
    // Reset state to prevent subsequent misuse | 重置状态防止后续误用
    g_arData.bufferSize = 0;
    g_arData.dataOffset = 0;
    return false;
  }
  g_arData.bufferSize = allocSize;
  g_arData.dataOffset = 44;  // Start writing PCM after header | 从 header 之后开始写 PCM
  g_arData.startTime = millis();
  // Clear stale DMA data and restart I2S (needed after stopRecording) | 清除 DMA 残留并重启 I2S（stopRecording 后需要）
  i2s_zero_dma_buffer(I2S_NUM_0);
  i2s_start(I2S_NUM_0);  // start microphone
  Serial.printf("Recording started, buffer: %u bytes\n", allocSize);
  return true;
}

/**
 * Stop recording and return upload parameters
 * 停止录音并返回上传参数
 */
AudioUploadParams *stopRecording() {
  i2s_stop(I2S_NUM_0);  // stop microphone

  size_t pcmSize = g_arData.dataOffset - 44;
  writeWavHeader(g_arData.buffer, pcmSize);
  Serial.printf("Recording stopped: %u bytes PCM\n", pcmSize);

  AudioUploadParams *params = new AudioUploadParams;
  params->buffer = g_arData.buffer;
  params->size = g_arData.dataOffset;
  g_arData.buffer = nullptr;  // Ownership transfer | 所有权转移
  return params;
}

/**
 * Arduino setup
 * 初始化入口
 */
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Start");
  Serial.printf("Free heap: %u\n", ESP.getFreeHeap());
  Serial.printf("Min free heap: %u\n", ESP.getMinFreeHeap());
  Serial.printf("Free PSRAM: %u\n", ESP.getFreePsram());
  Serial.printf("PSRAM size: %u\n", ESP.getPsramSize());
  // Screen initialization | 屏幕初始化
  tft.initR(INITR_144GREENTAB);
  u8g2.begin(tft);
  u8g2.setBackgroundColor(ST77XX_BLUE);
  u8g2.setForegroundColor(ST77XX_WHITE);
  u8g2.setFont(u8g2_font_wqy12_t_gb2312);
  // MAX30102 initialization | MAX30102 初始化
  Wire.begin(MAX30102_SDA, MAX30102_SCL);
  if (particleSensor.begin(Wire, I2C_SPEED_FAST) == false) {
    u8g2.setCursor(6, 18);
    u8g2.print("MAX30102 was not found.");
    Serial.println("MAX30102 was not found.");
    while (1) delay(1000);
  }
  particleSensor.setup();
  // INMP441 initialization | INMP441 初始化
  if (initINMP441() == false) {
    u8g2.setCursor(6, 18);
    u8g2.print("INMP441 was not found.");
    Serial.println("INMP441 was not found.");
    while (1) delay(1000);
  }
  // WiFi initialization | WIFI 初始化
  xTaskCreate(
    wifiInitTask,  // Task function | 任务函数
    "wifiInit",    // Task name | 任务名称
    4096,          // Stack size | 栈大小
    NULL,          // Parameters | 参数
    1,             // Priority | 优先级
    NULL           // Task handle | 任务句柄
  );
  // Button initialization / 按钮初始化
  pinMode(BUTTON_K1, INPUT_PULLUP);
  pinMode(BUTTON_K2, INPUT_PULLUP);
  pinMode(BUTTON_K3, INPUT_PULLUP);
  pinMode(BUTTON_K4, INPUT_PULLUP);

  showHomePage();

  Serial.println("setup: OK");
}

/**
 * Arduino main loop
 * 主循环
 */
void loop() {
  readButtons();
  // --- WiFi → NTP trigger | WiFi → NTP 触发 ---
  {
    static bool ntpTaskCreated = false;
    if (!ntpTaskCreated && g_state.wifiInited && WiFi.status() == WL_CONNECTED) {
      xTaskCreate(ntpTimeSyncTask, "ntpSync", 4096, NULL, 1, NULL);
      ntpTaskCreated = true;
    }
  }
  // --- Datetime refresh | 时间刷新 ---
  if (g_state.timeSynced && g_state.page == PAGE_HOME) {
    static unsigned long lastDatetimeUpdate = 0;
    unsigned long now = millis();
    if (now - lastDatetimeUpdate >= 1000) {
      lastDatetimeUpdate = now;
      showDatetime();
    }
  }
  // --- WiFi status change refresh | WiFi 状态变化刷新 ---
  {
    static bool lastWifiInited = false;
    if (!lastWifiInited && g_state.wifiInited) {
      showWiFi();
    }
    lastWifiInited = g_state.wifiInited;
  }
  // --- Page state machine | 页面状态机 ---
  switch (g_state.page) {
    case PAGE_HOME:
      if (g_state.btnK1.justPressed) {
        Serial.println("K1 pressed - start recording");
        if (startRecording()) {
          showRecordingPage();
        } else {
          Serial.println("Recording aborted: OOM");
        }
      }
      if (g_state.btnK2.justPressed) {
        Serial.println("K2 pressed - start detecting");
        showDetectStartPage();  // → PAGE_DETECT_START | 跳转到检测开始页
      }
      if (g_state.btnK3.justPressed) {
        Serial.println("K3 pressed - switch language");
        g_state.lang = (g_state.lang == LANG_ZH) ? LANG_EN : LANG_ZH;
        refreshCurrentPage();
      }
      break;
    case PAGE_DETECT_START:
      if (waitForFinger(FINGER_TIMEOUT_MS)) {
        showDetectingPage();  // → PAGE_DETECTING | 跳转到采集中页
        startHeartRateDetection();
      } else {
        Serial.println("Finger timeout or cancelled");
        showHomePage();  // → PAGE_HOME | 跳转到主页
      }
      break;
    case PAGE_DETECTING:
      processHeartRateSamples();
      break;
    case PAGE_DETECT_END:
      if (g_state.btnK2.justPressed) {
        Serial.println("K2 pressed - back to home");
        showHomePage();  // → PAGE_HOME | 跳转到主页
      }
      break;
    case PAGE_RECORDING:
      {
        // Write audio data | 写入音频数据
        size_t bytesRead = 0;
        size_t remaining = g_arData.bufferSize - g_arData.dataOffset;
        if (remaining >= 512) {
          esp_err_t err = i2s_read(I2S_NUM_0, g_arData.buffer + g_arData.dataOffset, 512, &bytesRead, 0);
          if (err == ESP_OK && bytesRead > 0) {
            g_arData.dataOffset += bytesRead;
          }
        }
        // Update time counter | 更新时间计数
        unsigned long elapsed = millis() - g_arData.startTime;
        if (elapsed - g_arData.updatedTime >= DISPLAY_UPDATE_MS) {
          g_arData.updatedTime = elapsed;
          updateRecordingCounter(elapsed);
        }
        // K1 pressed or time reached → stop and upload | K1 按下或时间到 → 停止并上传
        bool stopNow = g_state.btnK1.justPressed || (elapsed >= RECORD_TIME_MS);
        if (stopNow) {
          AudioUploadParams *params = stopRecording();
          xTaskCreate(uploadWantsAudioTask, "uploadAudio", 8192, params, 1, NULL);
          showRecordEndPage(true);
        }
      }
      break;
    case PAGE_RECORD_END:
      if (g_state.btnK1.justPressed) {
        Serial.println("K1 pressed - back to home");
        showHomePage();  // → PAGE_HOME
      }
      break;
  }
}
