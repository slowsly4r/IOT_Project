#ifndef __TASK_CHECK_INFO_H__
#define __TASK_CHECK_INFO_H__

#include <ArduinoJson.h>
#include "LittleFS.h"
#include "global.h"
#include "task_wifi.h"


void Load_info_File(SystemData_t *pData);
void Save_info_File(SystemData_t *pData);
bool check_info_File(SystemData_t *pData, bool check);
void Delete_info_File();

#endif