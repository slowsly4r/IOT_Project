#include "task_check_info.h"

void Load_info_File(SystemData_t *pData)
{
  File file = LittleFS.open("/info.dat", "r");
  if (!file)
  {
    Serial.println("No config file found. Using defaults.");
    return;
  }
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, file);
  if (error)
  {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
  }
  else
  {
    pData->wifi_ssid = doc["WIFI_SSID"].as<String>();
    pData->wifi_pass = doc["WIFI_PASS"].as<String>();
    pData->core_iot_token = doc["CORE_IOT_TOKEN"].as<String>();
    pData->core_iot_server = doc["CORE_IOT_SERVER"].as<String>();
    pData->core_iot_port = doc["CORE_IOT_PORT"].as<String>();
    Serial.println("Configuration loaded from Flash.");
    }
    file.close();
}

void Delete_info_File()
{
  if (LittleFS.exists("/info.dat"))
  {
    LittleFS.remove("/info.dat");
    Serial.println("Config file deleted.");
  }
  ESP.restart();
}

void Save_info_File(SystemData_t *pData)
{
  Serial.println(pData->wifi_ssid);
  Serial.println(pData->wifi_pass);

  StaticJsonDocument<512> doc;
  doc["WIFI_SSID"] = pData->wifi_ssid;
  doc["WIFI_PASS"] = pData->wifi_pass;
  doc["CORE_IOT_TOKEN"] = pData->core_iot_token;
  doc["CORE_IOT_SERVER"] = pData->core_iot_server;
  doc["CORE_IOT_PORT"] = pData->core_iot_port;

  File configFile = LittleFS.open("/info.dat", "w");
  if (configFile)
  {
    serializeJson(doc, configFile);
    configFile.close();
    Serial.println("Configuration saved successfully.");
  }
  else
  {
    Serial.println("Unable to save the configuration.");
  }
  delay(500);
  ESP.restart();
};

bool check_info_File(SystemData_t *pData, bool check)
{
  if (!check)
  {
    if (!LittleFS.begin(true))
    {
      Serial.println("❌ Lỗi khởi động LittleFS!");
      return false;
    }
    Load_info_File(pData);
  }
  
  if (pData->wifi_ssid.isEmpty())
  {
      Serial.println("WiFi credentials missing!");
      if (!check)
      {
          startAP();
      }
      return false;
  }
  return true;
}