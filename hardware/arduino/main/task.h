#ifndef TASK_H
#define TASK_H

/**
 * Heart rate upload task parameters
 * 心率上传任务参数
 */
struct HeartRateUploadParams {
  float bpm;
  int beatCount;
};

/**
 * Audio upload task parameters
 * 音频上传任务参数
 */
struct AudioUploadParams {
  uint8_t *buffer;  // Complete WAV file data (44-byte header + PCM) | WAV 文件完整数据（44 字节头 + PCM）
  size_t size;      // Total WAV file bytes | WAV 文件总字节数
};

/**
 * WiFi initialization
 * WiFi 初始化
 */
void wifiInitTask(void *pvParameters);

/**
 * NTP time synchronization
 * NTP 时间同步
 */
void ntpTimeSyncTask(void *pvParameters);

/**
 * Upload heart rate data to server
 * 上传心率数据到服务器
 */
void uploadHeartRateTask(void *pvParameters);

/**
 * Write 44-byte WAV file header to buffer
 * 写入 44 字节 WAV 文件头到缓冲区
 */
void writeWavHeader(uint8_t *buffer, size_t dataSize);

/**
 * Upload audio recording data to server
 * 上传录音数据到服务器
 */
void uploadWantsAudioTask(void *pvParameters);

#endif
