#include "task_led_temp.h"

// TASK 1: LED BLINK CONTROL BASED ON TEMPERATURE (Consumer)
// Responds to temperature conditions with 3+ different blinking behaviors
// Uses xLedTempReady semaphore to synchronize with sensor task
void vTaskLedTempControl(void *pvParameters){
  SystemData_t *pData = (SystemData_t *)pvParameters;
  // Temperature thresholds for state transitions
  constexpr float TEMP_THRESHOLD_LOW = 30.0f;
  constexpr float TEMP_THRESHOLD_HIGH = 35.0f;
  pinMode(LED_GPIO, OUTPUT);

  // LED states based on temperature
  enum LedState {
    NORMAL,      // temp < 30°C: slow blink (1s on/off)
    WARNING,      // temp 30-35°C: fast blink (0.25s on/off)
    CRITICAL,     // temp > 35°C: SOS pattern (50ms rapid blink)
    SENSOR_ERROR  // sensor timeout: triple blink
  };
  LedState currentState = NORMAL;

  while(1) {
        // Wait for signal from sensor task that new data is available
        // Timeout after 5s to prevent deadlock
        if (xSemaphoreTake(pData->xLedTempReady, pdMS_TO_TICKS(5000)) == pdTRUE) {

            float currentTemp = 0;
            // CRITICAL SECTION: Read shared temperature data
            // Mutex ensures data consistency with sensor task
            if (xSemaphoreTake(pData->xDataMutex, portMAX_DELAY)) {
                currentTemp = pData->temperature;
                xSemaphoreGive(pData->xDataMutex);
            }

            // Determine LED state based on temperature thresholds
            if (currentTemp >= TEMP_THRESHOLD_HIGH) {
                currentState = CRITICAL;
            } else if (currentTemp >= TEMP_THRESHOLD_LOW) {
                currentState = WARNING;
            } else {
                currentState = NORMAL;
            }
        } else {
            currentState = SENSOR_ERROR; // Sensor timeout
        }

        // Execute blinking pattern for current state
        switch (currentState) {
            case NORMAL:
                // Slow blink: 1 second on, 1 second off
                digitalWrite(LED_GPIO, HIGH);
                vTaskDelay(pdMS_TO_TICKS(1000));
                digitalWrite(LED_GPIO, LOW);
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;

            case WARNING:
                // Fast blink: 0.25 second on/off
                digitalWrite(LED_GPIO, HIGH);
                vTaskDelay(pdMS_TO_TICKS(250));
                digitalWrite(LED_GPIO, LOW);
                vTaskDelay(pdMS_TO_TICKS(250));
                break;

            case CRITICAL:
                // SOS pattern: rapid double blink followed by pause
                digitalWrite(LED_GPIO, HIGH); vTaskDelay(pdMS_TO_TICKS(50));
                digitalWrite(LED_GPIO, LOW);  vTaskDelay(pdMS_TO_TICKS(50));
                digitalWrite(LED_GPIO, HIGH); vTaskDelay(pdMS_TO_TICKS(50));
                digitalWrite(LED_GPIO, LOW);  vTaskDelay(pdMS_TO_TICKS(400));
                break;

            case SENSOR_ERROR:
                // Triple blink to indicate sensor error
                for(int i=0; i<3; i++) {
                    digitalWrite(LED_GPIO, HIGH); vTaskDelay(pdMS_TO_TICKS(100));
                    digitalWrite(LED_GPIO, LOW);  vTaskDelay(pdMS_TO_TICKS(100));
                }
                vTaskDelay(pdMS_TO_TICKS(500));
                break;
        }
    }
}
