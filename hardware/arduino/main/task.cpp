#include <WiFi.h>
#include <HTTPClient.h>
#include "task.h"
#include "state.h"

#define IOT_HOST "https://mnt.min2k.com/3006"
// #define IOT_HOST "http://192.168.1.12:3006"  // local testing

#define NTP_SERVER "time.windows.com"
#define GMT_OFFSET_SEC (8 * 3600)  // UTC+8
#define DAYLIGHT_OFFSET_SEC 0

#define TZ_OFFSET_H (GMT_OFFSET_SEC / 3600)
#define TZ_OFFSET_M ((GMT_OFFSET_SEC % 3600) / 60)

const char *WIFI_SSID = "GaoSheng-1412-2.4G";
const char *WIFI_PASS = "gskj1412xyz666";

const char *SET_HEART_RATE_URL = IOT_HOST "/set-heart-rate";
const char *SET_WANTS_AUDIO_URL = IOT_HOST "/set-wants-audio";

/**
 * Convert negative HTTPClient error codes to readable names
 * 将 HTTPClient 负值错误码转换为可读名称
 */
static const char *httpErrorName(int code) {
  switch (code) {
    case -1: return "HTTPC_ERROR_CONNECTION_REFUSED";
    case -2: return "HTTPC_ERROR_SEND_HEADER_FAILED";
    case -3: return "HTTPC_ERROR_SEND_PAYLOAD_FAILED";
    case -4: return "HTTPC_ERROR_NOT_CONNECTED";
    case -5: return "HTTPC_ERROR_CONNECTION_LOST";
    case -6: return "HTTPC_ERROR_NO_STREAM";
    case -7: return "HTTPC_ERROR_NO_HTTP_SERVER";
    case -8: return "HTTPC_ERROR_TOO_LESS_RAM";
    case -9: return "HTTPC_ERROR_ENCODING";
    case -10: return "HTTPC_ERROR_STREAM_WRITE";
    case -11: return "HTTPC_ERROR_READ_TIMEOUT";
    default: return "unknown error";
  }
}

/**
 * Print HTTP response status
 * 打印 HTTP 响应状态
 */
static void printHttpResponse(const char *label, int httpCode, HTTPClient &http) {
  Serial.printf("%s, HTTP %d", label, httpCode);
  if (httpCode >= 200 && httpCode < 300) {
    Serial.println(" OK");
  } else if (httpCode >= 300 && httpCode < 400) {
    Serial.println(" (redirect)");
  } else if (httpCode >= 400) {
    Serial.println(" (client/server error)");
  } else {
    Serial.println();
  }
  String responseBody = http.getString();
  Serial.printf("Response: %s\n", responseBody.c_str());
}

/**
 * WiFi initialization
 * WiFi 初始化
 */
void wifiInitTask(void *pvParameters) {
  Serial.print("Connecting WiFi");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long startTime = millis();
  static const int MAX_RETRIES = 6;
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < MAX_RETRIES) {
    delay(500);
    Serial.print(".");
    retry++;
  }
  unsigned long elapsed = millis() - startTime;
  Serial.println();

  int wifiStatus = WiFi.status();
  if (wifiStatus == WL_CONNECTED) {
    Serial.printf("WiFi connected (%lu ms)\n", elapsed);
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal Strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.printf("WiFi connection failed (%lu ms), status: %d\n", elapsed, wifiStatus);
    switch (wifiStatus) {
      case WL_NO_SHIELD: Serial.println("Reason: WL_NO_SHIELD (no WiFi module)"); break;
      case WL_IDLE_STATUS: Serial.println("Reason: WL_IDLE_STATUS (idle)"); break;
      case WL_NO_SSID_AVAIL: Serial.println("Reason: WL_NO_SSID_AVAIL (SSID not found)"); break;
      case WL_CONNECT_FAILED: Serial.println("Reason: WL_CONNECT_FAILED (connection failed, check password)"); break;
      case WL_CONNECTION_LOST: Serial.println("Reason: WL_CONNECTION_LOST (connection lost)"); break;
      case WL_DISCONNECTED: Serial.println("Reason: WL_DISCONNECTED (disconnected)"); break;
      default: Serial.println("Reason: unknown error"); break;
    }
    Serial.print("Target SSID: ");
    Serial.println(WIFI_SSID);
  }
  g_state.wifiInited = true;  // (wifiStatus == WL_CONNECTED);
  vTaskDelete(NULL);
}

/**
 * NTP time synchronization
 * NTP 时间同步
 */
void ntpTimeSyncTask(void *pvParameters) {
  Serial.printf("NTP: syncing time from %s ...\n", NTP_SERVER);

  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);

  struct tm timeinfo;
  bool synced = false;
  static const int MAX_RETRIES = 3;
  int retry = 0;
  while (!(synced = getLocalTime(&timeinfo)) && retry < MAX_RETRIES) {
    delay(500);
    retry++;
    Serial.print(".");
  }
  Serial.println();

  if (synced) {
    Serial.printf("NTP: time synced - %04d-%02d-%02d %02d:%02d:%02d\n", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    g_state.timeSynced = true;
  } else {
    Serial.println("NTP: time sync failed");
  }

  vTaskDelete(NULL);
}

/**
 * Upload heart rate data to server
 * 上传心率数据到服务器
 */
void uploadHeartRateTask(void *pvParameters) {
  HeartRateUploadParams *params = (HeartRateUploadParams *)pvParameters;

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Upload failed: WiFi not connected");
    delete params;
    vTaskDelete(NULL);
    return;
  }
  {
    struct tm timeinfo;
    getLocalTime(&timeinfo);

    char url[256];
    snprintf(url, sizeof(url), "%s?mac=%s&iso8601=%04d-%02d-%02dT%02d%%3A%02d%%3A%02d%%2B%02d%%3A%02d&bpm=%.1f&beats=%d",
             SET_HEART_RATE_URL, WiFi.macAddress().c_str(),
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
             TZ_OFFSET_H, TZ_OFFSET_M,
             params->bpm, params->beatCount);

    Serial.printf("Upload: %s\n", url);

    HTTPClient http;
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode > 0) {
      printHttpResponse("Upload HR success", httpCode, http);
    } else {
      Serial.printf("Upload failed, error code: %d (%s)\n", httpCode, httpErrorName(httpCode));
    }
    http.end();
  }

  delete params;
  vTaskDelete(NULL);
}

/**
 * Write 44-byte WAV file header to buffer
 * 写入 44 字节 WAV 文件头到缓冲区
 */
void writeWavHeader(uint8_t *buffer, size_t dataSize) {
  uint32_t sampleRate = SAMPLE_RATE;
  uint16_t bitsPerSample = 16;
  uint16_t numChannels = 1;
  uint32_t byteRate = sampleRate * numChannels * bitsPerSample / 8;
  uint16_t blockAlign = numChannels * bitsPerSample / 8;
  uint32_t chunkSize = 36 + dataSize;
  uint32_t fileSize = chunkSize + 8;

  // RIFF header
  buffer[0] = 'R';
  buffer[1] = 'I';
  buffer[2] = 'F';
  buffer[3] = 'F';
  buffer[4] = (uint8_t)(fileSize);
  buffer[5] = (uint8_t)(fileSize >> 8);
  buffer[6] = (uint8_t)(fileSize >> 16);
  buffer[7] = (uint8_t)(fileSize >> 24);
  // WAVE
  buffer[8] = 'W';
  buffer[9] = 'A';
  buffer[10] = 'V';
  buffer[11] = 'E';
  // fmt chunk
  buffer[12] = 'f';
  buffer[13] = 'm';
  buffer[14] = 't';
  buffer[15] = ' ';
  buffer[16] = 16;
  buffer[17] = 0;
  buffer[18] = 0;
  buffer[19] = 0;  // chunk size = 16
  buffer[20] = 1;
  buffer[21] = 0;  // PCM = 1
  buffer[22] = (uint8_t)(numChannels);
  buffer[23] = (uint8_t)(numChannels >> 8);
  buffer[24] = (uint8_t)(sampleRate);
  buffer[25] = (uint8_t)(sampleRate >> 8);
  buffer[26] = (uint8_t)(sampleRate >> 16);
  buffer[27] = (uint8_t)(sampleRate >> 24);
  buffer[28] = (uint8_t)(byteRate);
  buffer[29] = (uint8_t)(byteRate >> 8);
  buffer[30] = (uint8_t)(byteRate >> 16);
  buffer[31] = (uint8_t)(byteRate >> 24);
  buffer[32] = (uint8_t)(blockAlign);
  buffer[33] = (uint8_t)(blockAlign >> 8);
  buffer[34] = (uint8_t)(bitsPerSample);
  buffer[35] = (uint8_t)(bitsPerSample >> 8);
  // data chunk
  buffer[36] = 'd';
  buffer[37] = 'a';
  buffer[38] = 't';
  buffer[39] = 'a';
  buffer[40] = (uint8_t)(dataSize);
  buffer[41] = (uint8_t)(dataSize >> 8);
  buffer[42] = (uint8_t)(dataSize >> 16);
  buffer[43] = (uint8_t)(dataSize >> 24);
}

/**
 * Upload audio recording data to server
 * 上传录音数据到服务器
 */
void uploadWantsAudioTask(void *pvParameters) {
  AudioUploadParams *params = (AudioUploadParams *)pvParameters;

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Upload failed: WiFi not connected");
    free(params->buffer);
    delete params;
    vTaskDelete(NULL);
    return;
  }
  {
    char url[256];
    snprintf(url, sizeof(url), "%s?mac=%s", SET_WANTS_AUDIO_URL, WiFi.macAddress().c_str());

    Serial.printf("Upload: %s\n", url);

    HTTPClient http;
    http.begin(url);
    http.addHeader("Content-Type", "audio/wav");
    int httpCode = http.POST(params->buffer, params->size);

    if (httpCode > 0) {
      printHttpResponse("Upload audio success", httpCode, http);
    } else {
      Serial.printf("Upload failed, error code: %d (%s)\n", httpCode, httpErrorName(httpCode));
    }
    http.end();
  }

  free(params->buffer);
  delete params;
  vTaskDelete(NULL);
}
