#ifndef __FAN_CONTROL_H__
#define __FAN_CONTROL_H__

#include <Arduino.h>

void FanInit();
void FanON();
void FanOFF();
void FanSetSpeed(uint8_t speed);
uint8_t FanGetSpeed();
bool FanGetState();

#endif