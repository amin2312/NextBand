# NextBand

[English](./README.md) | [中文](./README_zh.md) | [日本語](./README_ja.md)

---

NextBand 是面向下一代智能可穿戴设备的开源项目（手环或手表）。  
它将健康传感与云端的 AI 智能体相结合，可以监测你的心率，理解你的意图，并代你采取行动。

[Bilibili 演示视频](https://www.bilibili.com/video/BV19qgP6fE4u/)

## ✨ 功能特性

- **健康监测** — 通过 MAX30102 进行心率监测
- **语音指令** — 说出你的意图，让 AI Agent 替你执行

## 🧱 架构

![NextBand 架构](./resources/architecture.svg)

## 🧰 硬件

| 组件       | 型号          | 备注                       |
| ---------- | ------------- | -------------------------- |
| MCU        | ESP32-S3 N16R8 | 16 MB Flash，8 MB PSRAM    |
| 显示屏     | ST7735 LCD    | 128×128 TFT，带 4 个按键    |
| 心率传感器  | MAX30102      | 心率 + 血氧传感器          |
| 麦克风     | INMP441       | I²S 数字 MEMS 麦克风        |
| 面包板     | MB-102        | 原型搭建                   |

### 📌 引脚映射

#### ST7735（带 4 个按键的 LCD）

| ESP32-S3 | ST7735 | 功能       |
| -------- | ------ | ---------- |
| GPIO-12  | SCL    | SPI 时钟   |
| GPIO-11  | SDA    | SPI 数据   |
| GPIO-4   | RST    | 复位       |
| GPIO-5   | DC     | 数据/命令  |
| GPIO-6   | CS     | 片选       |
| GPIO-7   | K4     | 按键 4     |
| GPIO-15  | K3     | 按键 3     |
| GPIO-16  | K2     | 按键 2     |
| GPIO-17  | K1     | 按键 1     |

#### MAX30102（心率 & 血氧）

| ESP32-S3 | MAX30102 | 功能      |
| -------- | -------- | --------- |
| GPIO-8   | SCL      | 串行时钟  |
| GPIO-18  | SDA      | 串行数据  |

#### INMP441（麦克风）

| ESP32-S3 | INMP441 | 功能      |
| -------- | ------- | --------- |
| GPIO-9   | SCK     | 串行时钟  |
| GPIO-46  | SD      | 串行数据  |
| GPIO-10  | WS      | 字选择    |

### 🔌 烧录固件

固件位于 `hardware/arduino/main/`，通过 Arduino IDE 编译和烧录。

#### 1. 安装 Arduino IDE

从 [arduino.cc](https://www.arduino.cc) 下载并安装最新的 Arduino IDE 2.3.x 版本。

#### 2. 安装依赖

- 安装 ESP32 开发板支持：在 **Tools → Board → Boards Manager** 中搜索 `esp32`（由 Espressif Systems 提供）并安装。
- 通过 **Tools → Manage Libraries** 安装所需的库：

  - `Adafruit GFX Library`
  - `Adafruit ST7735 and ST7789 Library`
  - `U8g2`
  - `U8g2_for_Adafruit_GFX`
  - `DevLab_MAX30102`

#### 3. 打开固件

在 Arduino IDE 中打开 `hardware/arduino/main/main.ino`。

#### 4. 选择开发板

| 设置       | 值                          |
| ---------- | --------------------------- |
| 开发板     | ESP32S3 Dev Module          |
| 分区方案   | 16M Flash (3MB APP/9.9MB FATFS) |
| Flash 大小 | 16MB (128Mb)                |
| PSRAM      | OPI PSRAM                   |

#### 5. 配置

编辑 `hardware/arduino/main/task.cpp`：

```cpp
#define IOT_HOST "https://your-server"
...
const char *WIFI_SSID = "your-wifi-ssid";
const char *WIFI_PASS = "your-wifi-password";
```

#### 6. 编译并烧录

1. 通过 USB 连接 ESP32-S3 并选择对应端口。
2. 点击 **Upload**。
3. 以 115200 波特率打开串口监视器 - 你应该能看到日志输出。

## 💻 软件

软件支持 Windows 和 macOS 进行本地测试。  
生产环境部署请使用 Linux。

### 🧩 服务器技术

| 层级        | 技术             |
| ----------- | ---------------- |
| 操作系统    | Ubuntu 22.04 LTS 64 位 |
| Python    | Python 3.10 & venv |
| Web 服务器  | Tornado 6.5.4    |
| Docker    | Docker 29.1      |
| AI Agent    | pi-mono 0.79.1   |

### 🚀 服务端部署

> **目标系统：** Ubuntu Server 22.04 LTS（64 位）  
> **前置条件：** Docker 29.1、Python 3.10、python3.10-venv  

#### 1. 上传项目文件

在服务器上创建 `/next-band` 目录，然后将项目的 `server/` 和 `workspace/` 目录上传到其中。

#### 2. 启用 Docker REST API

```bash
sudo systemctl edit docker.service
```

添加以下配置：

```ini
[Service]
ExecStart=
ExecStart=/usr/bin/dockerd -H unix:///var/run/docker.sock -H tcp://127.0.0.1:2375
```

**注意：** 本地测试请改用 `tcp://0.0.0.0:2375`。

然后应用更改：

```bash
sudo systemctl daemon-reload
sudo systemctl restart docker
sudo systemctl status docker
```

#### 3. 构建 Docker 镜像

```bash
cd /next-band/server/pi-image

# 标准构建
docker build -t pi_bpm .

# 适用于中国用户（腾讯云镜像源）
docker build --build-arg MIRROR=TC -t pi_bpm .
```

#### 4. 配置服务器

编辑 `/next-band/server/my-agent/conf.prod.json`，填写 API 密钥或其他所需的参数。  
服务器在 Linux 上运行时会自动加载 `conf.prod.json`。

#### 5. 启动服务器

```bash
cd /next-band/server/my-agent

python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
python main.py
```

服务器将监听配置的端口（默认 `3006`）。

## 📡 API 接口

| 方法   | 路径               | 描述                             |
| ------ | ------------------ | -------------------------------- |
| `GET`  | `/set-heart-rate`  | 上传手环采集的心率数据           |
| `POST` | `/set-wants-audio` | 上传语音录音，用于意图解析       |

## 📂 项目结构

```text
NextBand/
├── hardware/
│   ├── arduino/main/      # ESP32-S3 固件源码
│   └── EDA/               # Fritzing 接线图与引脚参考
├── server/
│   ├── my-agent/          # Tornado API 服务器
│   ├── pi-image/          # AI Agent 运行时的 Docker 镜像
│   └── pi-mono/           # AI Agent 配置
├── workspace/             # 每个用户的工作区数据
```

---

## 🤝 联系我

Email: [huangmipi@gmail.com](mailto:huangmipi@gmail.com)  
微信: hmp750
