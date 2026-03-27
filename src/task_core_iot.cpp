#include "task_core_iot.h"
#include <array>

namespace {
constexpr uint32_t MAX_MESSAGE_SIZE = 1024U;

// ============================================================================
// CORE IOT RUNTIME CONTEXT
// Manages ThingsBoard MQTT client connection and telemetry publishing
// ============================================================================
struct CoreIotRuntimeContext {
    WiFiClient wifiClient;
    Arduino_MQTT_Client mqttClient;
    ThingsBoard tb;
    SystemData_t *pLocalData;

    CoreIotRuntimeContext()
        : wifiClient(), mqttClient(wifiClient), tb(mqttClient, MAX_MESSAGE_SIZE), pLocalData(nullptr) {}
};

CoreIotRuntimeContext& getCoreCtx() {
    static CoreIotRuntimeContext ctx;
    return ctx;
}

}

// ============================================================================
// RPC CALLBACK: Handle LED switch commands from cloud dashboard
// Called when user toggles LED from CoreIOT dashboard
// ============================================================================
RPC_Response setLedSwitchValue(const RPC_Data &data)
{
    CoreIotRuntimeContext &ctx = getCoreCtx();
    Serial.println("Received Switch state");
    bool newState = data;
    if (ctx.pLocalData != nullptr) {
        // Update LED state in shared data (mutex protected)
        if (xSemaphoreTake(ctx.pLocalData->xDataMutex, portMAX_DELAY)) {
            ctx.pLocalData->led1_status = newState;
            xSemaphoreGive(ctx.pLocalData->xDataMutex);
        }
        // Control hardware GPIO 41
        digitalWrite(41, newState ? HIGH : LOW);
    }
    Serial.print("Switch state change: ");
    Serial.println(newState);
    return RPC_Response("setLedSwitchValue", newState);
}

// ============================================================================
// Send telemetry/attribute data to CoreIOT cloud
// mode: "telemetry" for time-series data, "attribute" for static data
// ============================================================================
void CORE_IOT_sendata(String mode, String feed, String data)
{
    CoreIotRuntimeContext &ctx = getCoreCtx();

    if (mode == "attribute")
    {
        ctx.tb.sendAttributeData(feed.c_str(), data);
    }
    else if (mode == "telemetry")
    {
        float value = data.toFloat();
        ctx.tb.sendTelemetryData(feed.c_str(), value);
    }
}

// ============================================================================
// RECONNECT TO CORE IOT
// Establishes MQTT connection to ThingsBoard server
// Returns true if connected successfully
// ============================================================================
bool CORE_IOT_reconnect(SystemData_t *pData) {
    CoreIotRuntimeContext &ctx = getCoreCtx();

    if (ctx.tb.connected()) return true;
    if (WiFi.status() != WL_CONNECTED) return false;
    if (pData->core_iot_server.length() == 0 || pData->core_iot_token.length() == 0) return false;
    if (pData->core_iot_port.toInt() <= 0) return false;

    Serial.println("Connecting to Core IoT (ThingsBoard)...");

    const char* server = pData->core_iot_server.c_str();
    const char* token = pData->core_iot_token.c_str();
    int port = pData->core_iot_port.toInt();

    if (!ctx.tb.connect(server, token, port)) {
        return false;
    }

    Serial.println("Core IoT Connected!");
    // Subscribe to RPC callback for cloud-to-device commands
    static const std::array<RPC_Callback, 1U> callbacks = {
        RPC_Callback{"setLedSwitchValue", setLedSwitchValue}};
    ctx.tb.RPC_Subscribe(callbacks.cbegin(), callbacks.cend());

    // Send device metadata as attributes
    ctx.tb.sendAttributeData("macAddress", WiFi.macAddress().c_str());
    ctx.tb.sendAttributeData("localIp", WiFi.localIP().toString().c_str());

    return true;
}

// ============================================================================
// TASK 6: CORE IOT CLOUD PUBLISHING (Consumer)
// Publishes temperature, humidity, and AI prediction to CoreIOT dashboard
// Uses MQTT over WiFi (STA mode) to ThingsBoard server
// ============================================================================
void vTaskCoreIOT(void *pvParameters) {
    CoreIotRuntimeContext &ctx = getCoreCtx();
    ctx.pLocalData = (SystemData_t *)pvParameters;

    while (1) {
        // Wait for WiFi to be ready before attempting cloud connection
        if (xSemaphoreTake(ctx.pLocalData->xInternetReady, pdMS_TO_TICKS(5000)) == pdTRUE) {
            xSemaphoreGive(ctx.pLocalData->xInternetReady);  // Give back for next iteration

            if (CORE_IOT_reconnect(ctx.pLocalData)) {
                float t = 0.0f, h = 0.0f;
                int ai_res = 0;

                // Read shared sensor data (mutex protected)
                if (xSemaphoreTake(ctx.pLocalData->xDataMutex, portMAX_DELAY)) {
                    t = ctx.pLocalData->temperature;
                    h = ctx.pLocalData->humidity;
                    ai_res = ctx.pLocalData->ai_prediction;
                    xSemaphoreGive(ctx.pLocalData->xDataMutex);
                }

                // Publish telemetry data to cloud dashboard
                ctx.tb.sendTelemetryData("temperature", t);
                ctx.tb.sendTelemetryData("humidity", h);
                ctx.tb.sendTelemetryData("ai_warning", ai_res);

                Serial.println("Data synced to Cloud Dashboard");
            }
        } else {
            Serial.println("Core IOT: Waiting for WiFi connection...");
        }

        // Process MQTT callbacks (keep connection alive)
        ctx.tb.loop();
        vTaskDelay(pdMS_TO_TICKS(10000));  // Publish every 10 seconds
    }
}
