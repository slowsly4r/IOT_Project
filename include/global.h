#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// HARDWARE PIN DEFINITIONS
#define LED_GPIO       41   // LED auto-blink (Task 1 - temperature indicator)
#define LED_CTRL_GPIO  48   // LED manual control (Web/Cloud)
#define FAN_GPIO       42   // Fan manual control (Web/Cloud)

// SHARED DATA STRUCTURE
typedef struct {
    // Sensor readings (updated by sensor task, read by consumer tasks)
    float temperature;
    float humidity;

    // Device states (controlled via web/cloud, read by actuator tasks)
    bool led_status;
    bool fan_status;

    // TinyML inference result (written by TinyML task, read by cloud task)
    int ai_prediction;

    // WiFi and CoreIOT credentials (stored in flash, loaded at boot)
    String wifi_ssid;
    String wifi_pass;
    String core_iot_token;
    String core_iot_server;
    String core_iot_port;

    bool isWifiConnected;

    // SEMAPHORES FOR TASK SYNCHRONIZATION
    // Mutex: protects shared data during read/write (prevents data race)
    SemaphoreHandle_t xDataMutex;

    // Binary semaphores: signal that new sensor data is available
    SemaphoreHandle_t xLedTempReady;   // Task 1: new temp data ready
    SemaphoreHandle_t xNeoReady;       // Task 2: new humidity data ready
    SemaphoreHandle_t xLcdReady;       // Task 3: new data for LCD
    SemaphoreHandle_t xTinyMLReady;    // Task 5: new data for ML inference

    // Binary semaphores: signal LCD display state
    SemaphoreHandle_t xLcdNormal;      // Display normal state
    SemaphoreHandle_t xLcdWarning;    // Display warning state
    SemaphoreHandle_t xLcdCritical;    // Display critical state
    SemaphoreHandle_t xLcdSensorError; // Display sensor error

    // Binary semaphore: WiFi connected and ready for cloud
    SemaphoreHandle_t xInternetReady;  // Task 6: WiFi ready
} SystemData_t;
#endif
