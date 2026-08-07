# NextBand

[English](./README.md) | [中文](./README_zh.md) | [日本語](./README_ja.md)

---

NextBand is an open-source project for a next-generation smart wearable (band or watch).  
It combines health sensing with cloud-based AI agent that can monitor your heart rate, understand your intent, and take actions on your behalf.

[bilibili demo video](https://www.bilibili.com/video/BV19qgP6fE4u/)

## ✨ Features

- **Health Monitoring** — Heart rate monitoring via MAX30102
- **Voice Commands** — Speak your intent and let the AI agent act on it

## 🧱 Architecture

![NextBand Architecture](./resources/architecture.svg)

## 🧰 Hardware

| Component  | Model          | Notes             |
| ---------- | -------------- | ----------------- |
| MCU        | ESP32-S3 N16R8 | 16 MB Flash, 8 MB PSRAM |
| Display    | ST7735 LCD     | 128×128 TFT, within 4 buttons |
| Heart Rate | MAX30102       | HR + SpO₂ sensor  |
| Mic        | INMP441        | I²S digital MEMS  |
| Breadboard | MB-102         | Prototyping       |

### 📌 Pin Mapping

#### ST7735 (LCD within 4 buttons)

| ESP32-S3 | ST7735 | Function     |
| -------- | ------ | ------------ |
| GPIO-12  | SCL    | SPI Clock    |
| GPIO-11  | SDA    | SPI Data     |
| GPIO-4   | RST    | Reset        |
| GPIO-5   | DC     | Data/Command |
| GPIO-6   | CS     | Chip Select  |
| GPIO-7   | K4     | Button 4     |
| GPIO-15  | K3     | Button 3     |
| GPIO-16  | K2     | Button 2     |
| GPIO-17  | K1     | Button 1     |

#### MAX30102 (Heart Rate & SpO₂)

| ESP32-S3 | MAX30102 | Function     |
| -------- | -------- | ------------ |
| GPIO-8   | SCL      | Serial Clock |
| GPIO-18  | SDA      | Serial Data  |

#### INMP441 (Microphone)

| ESP32-S3 | INMP441 | Function     |
| -------- | ------- | ------------ |
| GPIO-9   | SCK     | Serial Clock |
| GPIO-46  | SD      | Serial Data  |
| GPIO-10  | WS      | Word Select  |

### 🔌 Flashing Firmware

The firmware is located at `hardware/arduino/main/` and compiled & flashed via the Arduino IDE.

#### 1. Install Arduino IDE

Download and install the latest Arduino IDE 2.3.x version from [arduino.cc](https://www.arduino.cc).

#### 2. Install prerequisites

- Install ESP32 board support: **Tools → Board → Boards Manager**, search for `esp32` (by Espressif Systems) and install it.
- Install the required libraries via **Tools → Manage Libraries**:

  - `Adafruit GFX Library`
  - `Adafruit ST7735 and ST7789 Library`
  - `U8g2`
  - `U8g2_for_Adafruit_GFX`
  - `DevLab_MAX30102`

#### 3. Open the firmware

Open `hardware/arduino/main/main.ino` in the Arduino IDE.

#### 4. Select the board

| Setting           | Value                       |
| ----------------- | --------------------------- |
| Board             | ESP32S3 Dev Module          |
| Partition Scheme  | 16M Flash (3MB APP/9.9MB FATFS) |
| Flash Size        | 16MB (128Mb)                |
| PSRAM             | OPI PSRAM                   |

#### 5. Configure

Edit `hardware/arduino/main/task.cpp`:

```cpp
#define IOT_HOST "https://your-server"
...
const char *WIFI_SSID = "your-wifi-ssid";
const char *WIFI_PASS = "your-wifi-password";
```

#### 6. Compile & upload

1. Connect the ESP32-S3 via USB and select its port.
2. Click **Upload**.
3. Open the Serial Monitor at 115200 baud - you should see the logs.

## 💻 Software

The software supports Windows and macOS for local testing.  
For production deployment, please use Linux.

### 🧩 Server Technology

| Layer      | Technology     |
| ---------- | -------------- |
| Ubuntu     | 22.04 LTS 64bit |
| Python     | 3.10 & venv    |
| Web Server | Tornado 6.5.4  |
| Docker     | 29.1           |
| AI Agent   | pi-mono 0.79.1 |

### 🚀 Server Deployment

> **Target OS:** Ubuntu Server 22.04 LTS (64-bit)  
> **Prerequisites:** Docker 29.1, Python 3.10, python3.10-venv

#### 1. Upload project files

Create `/next-band` on the server, then upload the project's `server/` and `workspace/` directories to it.

#### 2. Enable Docker REST API

```bash
sudo systemctl edit docker.service
```

Add the following configuration:

```ini
[Service]
ExecStart=
ExecStart=/usr/bin/dockerd -H unix:///var/run/docker.sock -H tcp://127.0.0.1:2375
```

**Note:** For local testing, use `tcp://0.0.0.0:2375` instead.

Then apply the changes:

```bash
sudo systemctl daemon-reload
sudo systemctl restart docker
sudo systemctl status docker
```

#### 3. Build the Docker image

```bash
cd /next-band/server/pi-image

# Standard build
docker build -t pi_bpm .

# For users in China (Tencent Cloud mirror)
docker build --build-arg MIRROR=TC -t pi_bpm .
```

#### 4. Configure the server

Edit `/next-band/server/my-agent/conf.prod.json` and fill in the API keys or other required parameters.  
The server automatically loads `conf.prod.json` when running on Linux.

#### 5. Start the server

```bash
cd /next-band/server/my-agent

python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
python main.py
```

The server will start on the configured port (default `3006`).

### 📡 API Endpoints

| Method | Path               | Description                               |
| ------ | ------------------ | ----------------------------------------- |
| `GET`  | `/set-heart-rate`  | Upload heart-rate data from the band      |
| `POST` | `/set-wants-audio` | Upload voice recording for intent parsing |

## 📂 Project Structure

```text
NextBand/
├── hardware/
│   ├── arduino/main/      # ESP32-S3 firmware source
│   └── EDA/               # Fritzing wiring diagram & pin reference
├── server/
│   ├── my-agent/          # Tornado API server
│   ├── pi-image/          # Docker image for AI agent runtime
│   └── pi-mono/           # AI Agent configuration
├── workspace/             # Per-user workspace data
```

---

## 🤝 Contact me

Email: [huangmipi@gmail.com](mailto:huangmipi@gmail.com)  
Wechat: hmp750
