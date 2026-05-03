#include "task_core_iot.h"
#include <PubSubClient.h>

namespace {
// RAW MQTT CLIENT (Bypass ThingsBoard SDK to use custom topic esp/telemetry)
struct CoreIotRuntimeContext {
    WiFiClient wifiClient;
    PubSubClient mqttClient;
    SystemData_t *pLocalData;

    CoreIotRuntimeContext()
        : wifiClient(), mqttClient(wifiClient), pLocalData(nullptr) {}
};

CoreIotRuntimeContext& getCoreCtx() {
    static CoreIotRuntimeContext ctx;
    return ctx;
}
}

// Send telemetry/attribute data to CoreIOT cloud
void CORE_IOT_sendata(String mode, String feed, String data)
{
    CoreIotRuntimeContext &ctx = getCoreCtx();
    if (!ctx.mqttClient.connected()) return;
    
    String payload = "{\"" + feed + "\":" + data + "}";
    if (mode == "attribute") {
        ctx.mqttClient.publish("esp/attributes", payload.c_str());
    } else {
        ctx.mqttClient.publish("esp/telemetry", payload.c_str());
    }
}

// RECONNECT TO CORE IOT
bool CORE_IOT_reconnect(SystemData_t *pData) {
    CoreIotRuntimeContext &ctx = getCoreCtx();

    if (ctx.mqttClient.connected()) return true;
    if (WiFi.status() != WL_CONNECTED) return false;
    
    if (pData->core_iot_server.length() == 0 || pData->core_iot_token.length() == 0) {
        Serial.println("Core IOT: Server or Token is EMPTY!");
        return false;
    }

    Serial.println("Connecting to Core IoT (Raw MQTT)...");
    Serial.printf("  Server: %s:%s\n", pData->core_iot_server.c_str(), pData->core_iot_port.c_str());

    ctx.mqttClient.setServer(pData->core_iot_server.c_str(), pData->core_iot_port.toInt());

    // Token goes into username field, no password needed
    String clientId = "ESP32_T1_" + String(random(0xffff), HEX);
    if (!ctx.mqttClient.connect(clientId.c_str(), pData->core_iot_token.c_str(), NULL)) {
        Serial.printf("Core IoT: MQTT connect FAILED, state=%d\n", ctx.mqttClient.state());
        return false;
    }

    Serial.println("✅ Core IoT Connected (Custom Topic ESP)!");

    // Send device metadata directly to attributes topic
    String attrPayload = "{\"macAddress\":\"" + WiFi.macAddress() + "\",\"localIp\":\"" + WiFi.localIP().toString() + "\"}";
    ctx.mqttClient.publish("esp/attributes", attrPayload.c_str());

    return true;
}

// TASK 6: CORE IOT CLOUD PUBLISHING (Consumer)
// Publishes temperature and humidity to CoreIOT dashboard
// Uses MQTT over WiFi (STA mode) to ThingsBoard server
void vTaskCoreIOT(void *pvParameters) {
    CoreIotRuntimeContext &ctx = getCoreCtx();
    ctx.pLocalData = (SystemData_t *)pvParameters;

    while (1) {
        // Check WiFi STA connection first
        if (WiFi.status() != WL_CONNECTED) {
            Serial.printf("Core IOT: WiFi NOT connected (status=%d), waiting...\n", WiFi.status());
            vTaskDelay(pdMS_TO_TICKS(3000));
            continue;
        }

        // Attempt MQTT connection
        if (!CORE_IOT_reconnect(ctx.pLocalData)) {
            Serial.println("Core IOT: Connect/reconnect failed, retry in 5s");
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        // Maintain MQTT connection (Keep-alive)
        ctx.mqttClient.loop();

        // Read shared sensor data (mutex protected)
        float t = 0.0f, h = 0.0f;

        if (xSemaphoreTake(ctx.pLocalData->xDataMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
            t = ctx.pLocalData->temperature;
            h = ctx.pLocalData->humidity;
            xSemaphoreGive(ctx.pLocalData->xDataMutex);
        } else {
            Serial.println("Core IOT: Mutex timeout, skip this cycle");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        // FORMAT AND SEND RAW JSON TO CUSTOM TOPIC 'esp/telemetry'
        String payload = "{\"temperature\":" + String(t, 1) + ",\"humidity\":" + String(h, 1) + "}";
        
        bool ok = ctx.mqttClient.publish("esp/telemetry", payload.c_str());
        
        // Flush internal buffers
        ctx.mqttClient.loop();

        if (ok) {
            Serial.printf("✅ Telemetry synced -> esp/telemetry: %s\n", payload.c_str());
        } else {
            Serial.printf("❌ Telemetry FAILED, MQTT state: %d\n", ctx.mqttClient.state());
        }

        // Publish every 10 seconds
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
