#ifndef MOTORDRIVERMANAGERRS485_H
#define MOTORDRIVERMANAGERRS485_H

#include "mbed.h"

class MotorDriverManagerRS485 {
public:
    MotorDriverManagerRS485(PinName txPinName, PinName rxPinName, int baudrate);

    void setSpeeds(int speed1, int speed2, int speed3, int speed4, int speed5);

private:
    Serial serial;

    void rxHandler();

    int receiveCounter = 0;
    char receiveBuffer[64];

    int speeds[5] = {0, 0, 0, 0, 0};
    int actualSpeeds[5] = {0, 0, 0, 0, 0};
    char deviceIds[5] = {'1', '2', '3', '4', '5'};
    int activeSpeedIndex = 0;
    bool isSettingSpeeds = false;
    bool sendNextSpeed = false;

    int txDelayCount = 1;
    int txDelayCounter = 0;
    int txDelayActive = 0;
    int txSend = 0;
};

#endif //MOTORDRIVERMANAGERRS485_H
