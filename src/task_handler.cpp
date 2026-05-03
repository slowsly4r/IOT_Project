#include <task_handler.h>
#include "fan_control.h"
#include "task_lcd_display.h"

bool applyLcdState(SystemData_t *pData, bool state)
{
    if (pData != NULL && pData->xDataMutex != NULL) {
        if (xSemaphoreTake(pData->xDataMutex, portMAX_DELAY)) {
            pData->lcd_status = state;
            xSemaphoreGive(pData->xDataMutex);
        }
    }

    LcdSetPower(state);
    Serial.printf("⚙️ LCD -> %s\n", state ? "ON" : "OFF");
    return true;
}

bool applyDeviceState(SystemData_t *pData, int gpio, bool state)
{
    if (gpio == LED_GPIO) {
        Serial.printf("⚠️ GPIO %d is auto-controlled, ignoring web command\n", gpio);
        return false;
    }

    if (gpio != FAN_GPIO) {
        pinMode(gpio, OUTPUT);
    }

    if (pData != NULL && pData->xDataMutex != NULL) {
        if (xSemaphoreTake(pData->xDataMutex, portMAX_DELAY)) {
            if (gpio == FAN_GPIO) pData->fan_status = state;
            xSemaphoreGive(pData->xDataMutex);
        }
    }

    if (gpio == FAN_GPIO) {
        if (state) {
            FanON();
        } else {
            FanOFF();
        }
    } else {
        digitalWrite(gpio, state ? HIGH : LOW);
    }
    if (gpio == FAN_GPIO) {
        Serial.printf("⚙️ Fan PWM -> state=%s speed=%u\n", FanGetState() ? "ON" : "OFF", FanGetSpeed());
    } else {
        Serial.printf("⚙️ Device GPIO %d -> %s\n", gpio, state ? "ON" : "OFF");
        Serial.printf("GPIO %d level now = %d\n", gpio, digitalRead(gpio));
    }
    return true;
}

bool applyFanSpeed(SystemData_t *pData, uint8_t speed)
{
    if (pData != NULL && pData->xDataMutex != NULL) {
        if (xSemaphoreTake(pData->xDataMutex, portMAX_DELAY)) {
            pData->fan_speed = speed;
            xSemaphoreGive(pData->xDataMutex);
        }
    }

    FanSetSpeed(speed);
    Serial.printf("⚙️ Fan speed -> %u\n", speed);
    return true;
}

void handleWebSocketMessage(String message, void *pvParameters)
{
    SystemData_t *pData = (SystemData_t *)pvParameters;
    if (pData == NULL) return;

    Serial.println(message);
    StaticJsonDocument<512> doc;

    DeserializationError error = deserializeJson(doc, message);
    if (error)
    {
        Serial.println("❌ Lỗi parse JSON!");
        return;
    }
    JsonObject value = doc["value"];
    if (doc["page"] == "device")
    {
        if (value.containsKey("device") && value["device"].as<String>().equalsIgnoreCase("lcd")) {
            if (!value.containsKey("status")) {
                Serial.println("⚠️ JSON thiếu thông tin status cho LCD");
                return;
            }

            bool state = value["status"].as<String>().equalsIgnoreCase("ON");
            applyLcdState(pData, state);
            return;
        }

        if (!value.containsKey("gpio") || !value.containsKey("status"))
        {
            Serial.println("⚠️ JSON thiếu thông tin gpio hoặc status");
            return;
        }

        int gpio = value["gpio"];
        String status = value["status"].as<String>();

        if (gpio == FAN_GPIO && status.equalsIgnoreCase("SPEED")) {
            if (!value.containsKey("value")) {
                Serial.println("⚠️ JSON thiếu thông tin value cho SPEED");
                return;
            }
            int speed = value["value"].as<int>();
            speed = constrain(speed, 0, 255);
            applyFanSpeed(pData, (uint8_t)speed);
            return;
        }

        bool state = status.equalsIgnoreCase("ON");
        applyDeviceState(pData, gpio, state);
    }
    else if (doc["page"] == "setting")
    {
        if (xSemaphoreTake(pData->xDataMutex, portMAX_DELAY)) {
            pData->wifi_ssid = value["ssid"].as<String>();
            pData->wifi_pass = value["password"].as<String>();
            pData->core_iot_token = value["token"].as<String>();
            pData->core_iot_server = value["server"].as<String>();
            pData->core_iot_port = value["port"].as<String>();
            xSemaphoreGive(pData->xDataMutex);
        }

        Serial.println("📥 Nhận cấu hình từ WebSocket:");
        Serial.println("SSID: " + pData->wifi_ssid);
        Serial.println("PASS: " + pData->wifi_pass);
        Serial.println("TOKEN: " + pData->core_iot_token);
        Serial.println("SERVER: " + pData->core_iot_server);
        Serial.println("PORT: " + pData->core_iot_port);

        // 👉 Gọi hàm lưu cấu hình
        Save_info_File(pData);

        // Phản hồi lại client (tùy chọn)
        String msg = "{\"status\":\"ok\",\"page\":\"setting_saved\"}";
        Webserver_sendata(msg);
    }
}
