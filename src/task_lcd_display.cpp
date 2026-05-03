#include "task_lcd_display.h"

namespace {
LiquidCrystal_I2C lcd(0x21, 16, 2);
bool g_lcdInitialized = false;
bool g_lcdPowerEnabled = true;

void applyLcdPowerState()
{
    if (!g_lcdInitialized) {
        return;
    }

    if (g_lcdPowerEnabled) {
        lcd.display();
        lcd.backlight();
    } else {
        lcd.noBacklight();
        lcd.noDisplay();
    }
}
}

void LcdInit()
{
    if (g_lcdInitialized) {
        applyLcdPowerState();
        return;
    }

    byte thermometer[8] = {0x4, 0xA, 0xA, 0xE, 0xE, 0x1F, 0x1F, 0xE};
    byte droplet[8] = {0x4, 0x4, 0xA, 0xA, 0x11, 0x11, 0x11, 0xE};

    lcd.begin();
    lcd.createChar(0, thermometer);
    lcd.createChar(1, droplet);
    g_lcdInitialized = true;
    applyLcdPowerState();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("System Starting");
}

void LcdSetPower(bool enabled)
{
    g_lcdPowerEnabled = enabled;
    applyLcdPowerState();
}

bool LcdGetPower()
{
    return g_lcdPowerEnabled;
}

// TASK 3: LCD DISPLAY - Shows sensor readings and system states (Consumer)
// Displays temperature/humidity and 3 states: NORMAL, WARNING, CRITICAL
// Uses binary semaphores to receive state signals from sensor task
void vTaskLcdDisplay(void *pvParameters) {
    SystemData_t *pData = (SystemData_t *)pvParameters;
    LcdInit();

    char buffer[17];

    while (1) {
        // Wait for signal from sensor task that new data is available
        if (xSemaphoreTake(pData->xLcdReady, pdMS_TO_TICKS(5000)) == pdTRUE) {
            float temp = 0, hum = 0;

            // CRITICAL SECTION: Read shared sensor data
            // Mutex ensures data consistency with sensor task
            if (xSemaphoreTake(pData->xDataMutex, portMAX_DELAY)) {
                temp = pData->temperature;
                hum = pData->humidity;
                xSemaphoreGive(pData->xDataMutex);
            }

            applyLcdPowerState();

            // Display temperature and humidity with custom icons
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.write(0);  // Thermometer icon
            sprintf(buffer, ":%.1fC ", temp);
            lcd.print(buffer);
            lcd.write(1);  // Droplet icon
            sprintf(buffer, ":%.1f%%", hum);
            lcd.print(buffer);

            // DISPLAY SYSTEM STATE based on binary semaphore signals
            // Critical state: flashing backlight for attention
            // Warning state: static display
            // Normal state: static display
            lcd.setCursor(0, 1);
            if (xSemaphoreTake(pData->xLcdCritical, 0) == pdTRUE) {
                lcd.print("STATE: CRITICAL ");
                if (LcdGetPower()) {
                    // Flashing backlight for critical alert
                    for (int i = 0; i < 2; i++) {
                        lcd.noBacklight();
                        vTaskDelay(pdMS_TO_TICKS(50));
                        lcd.backlight();
                        vTaskDelay(pdMS_TO_TICKS(50));
                    }
                }
            } else if (xSemaphoreTake(pData->xLcdWarning, 0) == pdTRUE) {
                lcd.print("STATE: WARNING  ");
            } else {
                // xLcdNormal is cleared by sensor task before signaling new state
                xSemaphoreTake(pData->xLcdNormal, 0);
                lcd.print("STATE: NORMAL   ");
            }
        }

        // Sensor error state - completely separate from normal display
        if (xSemaphoreTake(pData->xLcdSensorError, 0) == pdTRUE) {
            applyLcdPowerState();
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("SENSOR ERROR!");
            lcd.setCursor(0, 1);
            lcd.print("Check hardware");
        }
    }
}