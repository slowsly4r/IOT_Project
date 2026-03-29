#include <task_handler.h>

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
        if (!value.containsKey("gpio") || !value.containsKey("status"))
        {
            Serial.println("⚠️ JSON thiếu thông tin gpio hoặc status");
            return;
        }

        int gpio = value["gpio"];
        String status = value["status"].as<String>();
        bool state = status.equalsIgnoreCase("ON");

        if (xSemaphoreTake(pData->xDataMutex, portMAX_DELAY)) {
            if (gpio == 41) pData->led1_status = state;
            if (gpio == 42) pData->led2_status = state;
            xSemaphoreGive(pData->xDataMutex);
        }

        digitalWrite(gpio, state ? HIGH : LOW);
        Serial.printf("⚙️ Device GPIO %d -> %s\n", gpio, status.c_str());
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
