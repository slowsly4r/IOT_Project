#include "fan_control.h"
#include "global.h"

namespace {
volatile bool fanCurrentState = false;
volatile uint8_t fanSpeed = 255;
}

void FanInit() {
    ledcSetup(FAN_PWM_CHANNEL, FAN_PWM_FREQ, FAN_PWM_RESOLUTION);
    ledcAttachPin(FAN_GPIO, FAN_PWM_CHANNEL);
    ledcWrite(FAN_PWM_CHANNEL, FAN_PWM_STOP);
    fanCurrentState = false;
    fanSpeed = 255;
}

void FanON() {
    ledcWrite(FAN_PWM_CHANNEL, fanSpeed);
    fanCurrentState = true;
}

void FanOFF() {
    ledcWrite(FAN_PWM_CHANNEL, FAN_PWM_STOP);
    fanCurrentState = false;
}

void FanSetSpeed(uint8_t speed) {
    fanSpeed = speed;
    if (fanCurrentState) {
        ledcWrite(FAN_PWM_CHANNEL, fanSpeed);
    }
}

uint8_t FanGetSpeed() {
    return fanSpeed;
}

bool FanGetState() {
    return fanCurrentState;
}