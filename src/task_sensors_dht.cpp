#include "task_sensors_dht.h"

namespace {
// LCD state signals - binary semaphores indicate which state is active
enum LcdStateSignal {
    LCD_STATE_NORMAL,
    LCD_STATE_WARNING,
    LCD_STATE_CRITICAL
};
}

// SENSOR MONITOR TASK (Producer)
// Reads DHT20 temperature & humidity sensor every 5 seconds
// Signals consumer tasks via binary semaphores
void temp_humi_monitor(void *pvParameters){
    SystemData_t *pData = (SystemData_t *) pvParameters;
    DHT20 dht20;

    Wire.begin(11, 12);
    Serial.begin(115200);
    dht20.begin();

    while (1){
        dht20.read();
        float temperature = dht20.getTemperature();
        float humidity = dht20.getHumidity();

        // Sensor error handling - signal LCD to show error state
        if (isnan(temperature) || isnan(humidity)) {
            Serial.println("Error: Failed to read from DHT sensor!");
            xSemaphoreTake(pData->xLcdNormal, 0);
            xSemaphoreTake(pData->xLcdWarning, 0);
            xSemaphoreTake(pData->xLcdCritical, 0);
            xSemaphoreGive(pData->xLcdSensorError);
        }
        else {
            // CRITICAL SECTION: Update shared sensor data
            // Mutex ensures exclusive access to prevent data race
            if (xSemaphoreTake(pData->xDataMutex, portMAX_DELAY) == pdTRUE) {
                pData->temperature = temperature;
                pData->humidity = humidity;
                xSemaphoreGive(pData->xDataMutex);
            }

            // Signal all consumer tasks that new data is available
            xSemaphoreGive(pData->xLedTempReady);  // Task 1: LED control
            xSemaphoreGive(pData->xNeoReady);      // Task 2: NeoPixel control
            xSemaphoreGive(pData->xLcdReady);      // Task 3: LCD display
            xSemaphoreGive(pData->xTinyMLReady);   // Task 5: TinyML inference

            // DETERMINE LCD STATE based on thresholds
            // Normal: temp <= 30 AND humi <= 70
            // Warning: temp 30-35 OR humi 70-80
            // Critical: temp > 35 OR humi > 80
            LcdStateSignal state = LCD_STATE_NORMAL;
            if (temperature > 35.0f || humidity > 80.0f) {
                state = LCD_STATE_CRITICAL;
            } else if (temperature > 30.0f || humidity > 70.0f) {
                state = LCD_STATE_WARNING;
            }

            // Clear all LCD state semaphores first (prevent stale states)
            xSemaphoreTake(pData->xLcdSensorError, 0);
            xSemaphoreTake(pData->xLcdNormal, 0);
            xSemaphoreTake(pData->xLcdWarning, 0);
            xSemaphoreTake(pData->xLcdCritical, 0);

            // Signal the current LCD state
            switch (state) {
                case LCD_STATE_CRITICAL:
                    xSemaphoreGive(pData->xLcdCritical);
                    break;
                case LCD_STATE_WARNING:
                    xSemaphoreGive(pData->xLcdWarning);
                    break;
                default:
                    xSemaphoreGive(pData->xLcdNormal);
                    break;
            }

            Serial.printf("Producer Task -> Temperature: %.2f C, Humidity: %.2f %%\n", temperature, humidity);
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
