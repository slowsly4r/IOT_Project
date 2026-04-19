#include "global.h"

#include "task_led_temp.h"
#include "task_neo_humi.h"
#include "task_sensors_dht.h"
#include "task_lcd_display.h"
#include "tinyml.h"

// Task headers
#include "task_check_info.h"
#include "task_toogle_boot.h"
#include "task_wifi.h"
#include "task_webserver.h"
#include "task_core_iot.h"

void setup() {
  // Shared data structure accessible by all FreeRTOS tasks
  SystemData_t *sysData = new SystemData_t();

  Serial.begin(115200);
  delay(1000);
  Serial.println("System Starting...");

  // SEMAPHORE INITIALIZATION
  // Mutex: protects shared sensor data (temperature, humidity)
  // Binary semaphores: signal when new data is available
  sysData->xDataMutex = xSemaphoreCreateMutex();
  sysData->xLedTempReady = xSemaphoreCreateBinary();
  sysData->xNeoReady = xSemaphoreCreateBinary();
  sysData->xLcdReady = xSemaphoreCreateBinary();
  sysData->xLcdNormal = xSemaphoreCreateBinary();
  sysData->xLcdWarning = xSemaphoreCreateBinary();
  sysData->xLcdCritical = xSemaphoreCreateBinary();
  sysData->xLcdSensorError = xSemaphoreCreateBinary();
  sysData->xTinyMLReady = xSemaphoreCreateBinary();
  sysData->xInternetReady = xSemaphoreCreateBinary();

  // Hardware outputs: LED auto (GPIO 48), LED manual (GPIO 6 - D3), Fan (GPIO 8 - D5)
  pinMode(LED_GPIO, OUTPUT);
  pinMode(LED_CTRL_GPIO, OUTPUT);
  pinMode(FAN_GPIO, OUTPUT);
  digitalWrite(LED_GPIO, LOW);
  digitalWrite(LED_CTRL_GPIO, LOW);
  digitalWrite(FAN_GPIO, LOW);

  // Load WiFi config from flash; if fails, enter AP mode
  if (!check_info_File(sysData, false)) {
        Serial.println("No WiFi Config! AP Mode started.");
    }

  // FREE RTOS TASK CREATION
  // Priority 2: Sensor-related tasks (higher priority)
  // Priority 1: Network/Cloud tasks (lower priority)

  // Task 1: LED blinks based on temperature
  xTaskCreate(vTaskLedTempControl, "Task LED Temperature", 4096, sysData, 2, NULL);

  // Task 2: NeoPixel color based on humidity
  xTaskCreate(vTaskNeoHumiControl, "Task NEO Humidity", 4096, sysData, 2, NULL);

  // Task: Read DHT20 sensor and publish data
  xTaskCreate(temp_humi_monitor, "Task TEMP HUMI Monitor", 4096, sysData, 2, NULL);

  // Task 3: LCD displays status (normal/warning/critical)
  xTaskCreate(vTaskLcdDisplay, "Task LCD Display", 4096, sysData, 2, NULL);

  // Task 5: TinyML inference for anomaly detection
  xTaskCreate(tiny_ml_task, "Tiny ML Task", 8192, sysData, 2, NULL);

  // WiFi connection task
  xTaskCreate(vTaskWifi, "WiFi_Task", 4096, (void*)sysData, 2, NULL);

  // Task 4: Web server (AP mode) with dashboard
  xTaskCreate(connnectWSV, "Web_Task", 8192, (void*)sysData, 1, NULL);

  // Task 4: WebSocket updates dashboard every 2s
  xTaskCreate(vTaskWebUpdateDashboard, "Web_Update", 4096, (void*)sysData, 1, NULL);

  // Task 6: Publish telemetry to CoreIOT cloud
  xTaskCreate(vTaskCoreIOT, "Cloud_Task", 8192, (void*)sysData, 1, NULL);

  // Toggle boot mode task
  xTaskCreate(Task_Toogle_BOOT, "Task_Toogle_BOOT", 2048, sysData, 2, NULL);
}

void loop()
{
  vTaskDelete(NULL);
}
