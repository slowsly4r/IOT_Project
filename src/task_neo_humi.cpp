#include "task_neo_humi.h"

// TASK 2: NEOPIXEL RGB LED CONTROL BASED ON HUMIDITY (Consumer)
// Changes color based on humidity level with 3+ distinct color states
// Uses xNeoReady semaphore to synchronize with sensor task
void vTaskNeoHumiControl(void *pvParameters){

    SystemData_t *pData = (SystemData_t *)pvParameters;
    // Humidity thresholds for color changes
    constexpr float HUMI_THRESHOLD_LOW = 40.0f;
    constexpr float HUMI_THRESHOLD_HIGH = 70.0f;

    Adafruit_NeoPixel strip(LED_COUNT, NEO_PIN, NEO_GRB + NEO_KHZ800);
    strip.begin();
    // Initialize all pixels to off
    strip.clear();
    strip.show();

    uint32_t targetColor = strip.Color(0, 255, 0); // Default: green
    int stepDelay = 8;

    while(1) {
        // Wait for signal from sensor task that new data is available
        if (xSemaphoreTake(pData->xNeoReady, pdMS_TO_TICKS(5000)) == pdTRUE) {

            float currentHumi = 0;
            // CRITICAL SECTION: Read shared humidity data
            // Mutex ensures data consistency with sensor task
            if (xSemaphoreTake(pData->xDataMutex, portMAX_DELAY)) {
                currentHumi = pData->humidity;
                xSemaphoreGive(pData->xDataMutex);
            }

            // DETERMINE COLOR based on humidity thresholds
            // Blue: humi < 40% (dry)
            // Green: humi 40-70% (comfortable)
            // Red: humi > 70% (humid/warning)
            // White: sensor error
            if (currentHumi >= HUMI_THRESHOLD_HIGH) {
                // High Humidity (>70%): Red (Warning)
                targetColor = strip.Color(255, 0, 0);
                stepDelay = 2;  // Fast pulse
            } else if (currentHumi >= HUMI_THRESHOLD_LOW) {
                // Moderate Humidity (40-70%): Green (Comfortable)
                targetColor = strip.Color(0, 255, 0);
                stepDelay = 8;  // Slow pulse
            } else {
                // Low Humidity (<40%): Blue (Dry)
                targetColor = strip.Color(0, 0, 255);
                stepDelay = 5;  // Medium pulse
            }
        } else {
            // Sensor timeout: White flashing
            targetColor = strip.Color(255, 255, 255);
            stepDelay = 1;
        }

        // Breathing light animation (fade in/out)
        for (int i = 0; i < 180; i++) {
            strip.setPixelColor(0, targetColor);
            strip.setBrightness(i);
            strip.show();
            vTaskDelay(pdMS_TO_TICKS(stepDelay));
        }

        for (int i = 180; i > 0; i--) {
            strip.setPixelColor(0, targetColor);
            strip.setBrightness(i);
            strip.show();
            vTaskDelay(pdMS_TO_TICKS(stepDelay));
        }
    }
}
