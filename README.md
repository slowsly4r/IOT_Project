<h1 align="center">YoloUNO ESP32-S3 IoT Platform</h1>

<p align="center">
  <img src="https://img.shields.io/badge/-ESP32--S3-A455C5?style=flat-square&logo=espressif&logoColor=white" alt="ESP32-S3">
  <img src="https://img.shields.io/badge/-PlatformIO-FAB444?style=flat-square&logo=platformio&logoColor=white" alt="PlatformIO">
  <img src="https://img.shields.io/badge/-FreeRTOS-2E3A41?style=flat-square&logo=freertos&logoColor=white" alt="FreeRTOS">
  <img src="https://img.shields.io/badge/CPU-240%20MHz-0078D7?style=flat-square" alt="CPU Clock">
  <img src="https://img.shields.io/github/last-commit/slowsly4r/IOT_Project?style=flat-square" alt="Last Commit">
  <img src="https://img.shields.io/github/license/slowsly4r/IOT_Project?style=flat-square" alt="License">
</p>

<p align="center">
  <a href="https://github.com/nhanksd85/YoloUNO_PlatformIO/tree/RTOS_Project">Original Project</a>
  |
  <a href="https://docs.platformio.org">Documentation</a>
  |
  <a href="https://www.espressif.com/en/products/socs/esp32-s3">ESP32-S3 Datasheet</a>
  |
  <a href="https://www.freertos.org">FreeRTOS Docs</a>
</p>

---

## Project Overview

This project implements a real-time IoT monitoring and control system on the ESP32-S3 microcontroller using FreeRTOS. The system integrates temperature/humidity sensing, RGB LED indicators, LCD display, web-based dashboard, TinyML anomaly detection, and cloud data publishing.

**Hardware Platform:** ESP32-S3 (YoloUNO board)  
**Framework:** PlatformIO with Arduino framework  
**RTOS:** FreeRTOS

## System Architecture

### FreeRTOS Task Structure

The system consists of 11 concurrent tasks with priority-based scheduling:

| Task | Priority | Stack Size | Description |
|------|----------|------------|-------------|
| vTaskLedTempControl | 2 | 2048 | LED blink pattern based on temperature |
| vTaskNeoHumiControl | 2 | 2048 | NeoPixel color based on humidity |
| temp_humi_monitor | 2 | 2048 | DHT20 sensor data acquisition |
| vTaskLcdDisplay | 2 | 4096 | LCD status display with 3 states |
| tiny_ml_task | 2 | 8192 | TensorFlow Lite inference |
| vTaskWifi | 2 | 4096 | WiFi connection management |
| connnectWSV | 1 | 8192 | Async web server |
| vTaskWebUpdateDashboard | 1 | 4096 | WebSocket data streaming |
| vTaskCoreIOT | 1 | 8192 | Cloud MQTT publishing |
| Task_Toogle_BOOT | 2 | 2048 | Boot mode toggle |
| vTaskRS485 | 1 | 2048 | RS485 communication |

**Priority 2:** Sensor and actuator tasks (higher priority for real-time response)  
**Priority 1:** Network and cloud tasks (background operations)

### Shared Data Management

All inter-task communication uses the `SystemData_t` structure, which is passed as a parameter to each task. No global variables are used.

```cpp
typedef struct {
    float temperature;
    float humidity;
    bool led1_status;
    bool led2_status;
    int ai_prediction;
    String wifi_ssid;
    String wifi_pass;
    String core_iot_token;
    String core_iot_server;
    String core_iot_port;
    bool isWifiConnected;

    // Semaphores
    SemaphoreHandle_t xDataMutex;
    SemaphoreHandle_t xLedTempReady;
    SemaphoreHandle_t xNeoReady;
    SemaphoreHandle_t xLcdReady;
    SemaphoreHandle_t xLcdNormal;
    SemaphoreHandle_t xLcdWarning;
    SemaphoreHandle_t xLcdCritical;
    SemaphoreHandle_t xLcdSensorError;
    SemaphoreHandle_t xTinyMLReady;
    SemaphoreHandle_t xInternetReady;
} SystemData_t;
```

### Semaphore Synchronization

The system uses 10 semaphores for task synchronization:

- **xDataMutex:** Mutex for protecting shared sensor data during read/write
- **xLedTempReady:** Binary semaphore signaling new temperature data for Task 1
- **xNeoReady:** Binary semaphore signaling new humidity data for Task 2
- **xLcdReady:** Binary semaphore signaling new data for LCD display
- **xLcdNormal/Warning/Critical:** Binary semaphores indicating current system state
- **xLcdSensorError:** Binary semaphore for sensor failure indication
- **xTinyMLReady:** Binary semaphore signaling data for TinyML inference
- **xInternetReady:** Binary semaphore indicating WiFi is connected

## Implemented Tasks

### Task 1: LED Blink with Temperature Conditions

The LED (GPIO 41) changes blinking behavior based on temperature readings from the DHT20 sensor:

| Temperature Range | Behavior | Pattern |
|------------------|----------|---------|
| < 30 degrees C | NORMAL | Slow blink (1s on, 1s off) |
| 30 - 35 degrees C | WARNING | Fast blink (0.25s on, 0.25s off) |
| > 35 degrees C | CRITICAL | SOS pattern (50ms rapid blink) |
| Sensor timeout | SENSOR_ERROR | Triple blink |

**Implementation:** `src/task_led_temp.cpp`

### Task 2: NeoPixel LED Control Based on Humidity

The NeoPixel RGB LED changes color based on humidity levels with a breathing light animation:

| Humidity Range | Color | Animation Speed |
|---------------|-------|-----------------|
| < 40% | Blue (dry) | Medium pulse |
| 40 - 70% | Green (comfortable) | Slow pulse |
| > 70% | Red (humid) | Fast pulse |
| Sensor timeout | White | Rapid flash |

**Implementation:** `src/task_neo_humi.cpp`

### Task 3: Temperature and Humidity Monitoring with LCD Display

The 16x2 I2C LCD displays sensor readings and system states:

| State | Condition | LCD Behavior |
|-------|-----------|--------------|
| NORMAL | temp <= 30 AND humi <= 70 | Static display |
| WARNING | temp 30-35 OR humi 70-80 | Static display |
| CRITICAL | temp > 35 OR humi > 80 | Flashing backlight |
| SENSOR_ERROR | DHT read failure | Error message |

**Features:**
- Custom icons for temperature (thermometer) and humidity (droplet)
- Real-time data display with 5-second refresh
- State indication with backlight flashing for critical alerts

**Implementation:** `src/task_lcd_display.cpp`, `src/task_sensors_dht.cpp`

### Task 4: Web Server in Access Point Mode

The ESP32-S3 creates an Access Point with a redesigned web dashboard:

**Features:**
- Real-time temperature and humidity gauges
- Temperature trend chart (Chart.js, 10-minute history)
- Control interface for 2 devices: LED (GPIO 41) and Fan (GPIO 42)
- ON/OFF buttons for each device
- Dark/Light theme toggle with localStorage persistence
- Device management page for adding/removing dynamic relays
- Settings page for WiFi STA and CoreIOT configuration
- OTA firmware update support via ElegantOTA

**WebSocket Protocol:**
- Server sends JSON data every 2 seconds
- Format: `{"temp":float,"humi":float,"led1":0/1,"led2":0/1,"uptime":int,"heap":int,"rssi":int}`

**Implementation:** `src/task_webserver.cpp`, `data/index.html`, `data/script.js`, `data/styles.css`

### Task 5: TinyML Deployment and Accuracy Evaluation

TensorFlow Lite model running on ESP32-S3 for anomaly detection:

**Model Details:**
- Input: [temperature, humidity] (2 float values)
- Output: anomaly score (0 = normal, 1 = anomaly)
- Threshold: 0.6 (optimized for best F1-score)
- Tensor Arena: 8KB RAM

**Anomaly Detection Logic:**
- Anomaly when: temperature > 35 degrees C OR humidity > 80%

**Implementation:** `src/tinyml.cpp`, `src/dht_anomaly_model.h`

### Task 6: Data Publishing to CoreIOT Cloud Server

The system publishes telemetry data to ThingsBoard via MQTT:

**Connectivity:**
- ESP32-S3 connects to WiFi in Station (STA) mode
- Publishes telemetry to ThingsBoard server

**Published Data:**
- Temperature (degrees C)
- Humidity (%)
- AI warning flag (0/1)

**Cloud Control:**
- RPC callback for remote LED control
- Device metadata (MAC address, local IP) sent as attributes

**Publishing Interval:** 10 seconds

**Configuration:**
- Server URL, port, and device token stored in LittleFS
- Credentials configurable via web UI settings page

**Implementation:** `src/task_core_iot.cpp`

## Hardware Configuration

### GPIO Pin Mapping

| GPIO | Function |
|------|----------|
| 41 | LED output |
| 42 | Fan output |
| 11 | I2C SDA (DHT20, LCD) |
| 12 | I2C SCL (DHT20, LCD) |
| 21 | NeoPixel data |
| USB | Serial debug (115200 baud) |

### I2C Devices

| Device | Address | Purpose |
|--------|---------|---------|
| DHT20 | 0x38 | Temperature and humidity sensor |
| LCD I2C | 0x21 | 16x2 character display |

## Project Structure

```
YoloUNO_PlatformIO-RTOS_Project/
├── src/
│   ├── main.cpp                 # FreeRTOS task creation
│   ├── global.cpp               # SystemData_t initialization
│   ├── task_sensors_dht.cpp     # DHT20 sensor task
│   ├── task_led_temp.cpp        # Task 1: LED temperature control
│   ├── task_neo_humi.cpp        # Task 2: NeoPixel humidity control
│   ├── task_lcd_display.cpp     # Task 3: LCD display
│   ├── task_webserver.cpp       # Task 4: Web server
│   ├── tinyml.cpp               # Task 5: TinyML inference
│   ├── task_core_iot.cpp        # Task 6: Cloud publishing
│   ├── task_wifi.cpp            # WiFi connection management
│   ├── task_handler.cpp         # WebSocket message handler
│   ├── task_check_info.cpp      # Flash config storage
│   ├── task_toogle_boot.cpp     # Boot mode toggle
│   ├── task_rs485.cpp           # RS485 communication
│   └── dht_anomaly_model.h      # TensorFlow Lite model
├── include/
│   ├── global.h                 # SystemData_t definition
│   ├── task_*.h                 # Task header files
│   └── tinyml.h                 # TinyML declarations
├── data/
│   ├── index.html               # Web dashboard HTML
│   ├── script.js                # WebSocket client logic
│   └── styles.css               # Dashboard styling
├── platformio.ini               # PlatformIO configuration
└── boards/                      # Board definitions
```

## Dependencies

| Library | Version | Purpose |
|---------|---------|---------|
| DHT20 | 0.3.2 | Temperature/humidity sensor |
| TensorFlowLite_ESP32 | 1.0.0 | TinyML inference |
| Adafruit NeoPixel | 1.15.1 | RGB LED control |
| ESPAsyncWebServer | latest | Async web server |
| Arduino_MQTT_Client | - | ThingsBoard MQTT |
| PubSubClient | - | MQTT protocol |
| LiquidCrystal I2C | - | LCD display |

## Setup and Installation

### Hardware Requirements

- ESP32-S3 YoloUNO board
- DHT20 temperature/humidity sensor
- 16x2 I2C LCD display
- Adafruit NeoPixel RGB LED
- USB-C cable for programming

### Software Requirements

- PlatformIO IDE (VS Code extension recommended)
- Python 3.8+ (for TensorFlow Lite model training)

### Build and Upload

1. Open the project in PlatformIO (VS Code)
2. Connect ESP32-S3 via USB
3. Build: `pio run`
4. Upload: `pio run --target upload`
5. Monitor: `pio device monitor`

### Configuration

1. When powered on, ESP32-S3 starts in AP mode
2. Connect to WiFi SSID "ESP32 LOCAL" (password: 12345678)
3. Open browser to http://192.168.4.1
4. Go to Settings page to configure:
   - WiFi STA credentials (SSID and password)
   - CoreIOT server URL, port, and device token
5. Save configuration - device will restart and connect to WiFi

## Development Notes

### Semaphore Usage Guidelines

When adding new tasks or modifying existing ones:

1. **Mutex (xDataMutex):** Always acquire before reading/writing shared data, release immediately after
2. **Binary Semaphores:** Use `xSemaphoreGive()` to signal, `xSemaphoreTake()` with timeout to wait
3. **Never block in critical sections:** Keep mutex hold time minimal

### Adding New Devices

To add new actuator devices:

1. Define GPIO pin in `include/global.h`
2. Add device state to `SystemData_t` struct
3. Create new task or modify existing task
4. Add WebSocket command handling in `src/task_handler.cpp`
5. Add control buttons in `data/index.html`

## Credits

This project is based on the YoloUNO_PlatformIO-RTOS_Project by nhanksd85.

Original repository: https://github.com/nhanksd85/YoloUNO_PlatformIO/tree/RTOS_Project

## License

This project is for educational purposes as part of the IoT Systems course assignment.
