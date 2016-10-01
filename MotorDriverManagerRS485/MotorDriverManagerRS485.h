#ifndef MOTORDRIVERMANAGERRS485_H
#define MOTORDRIVERMANAGERRS485_H

#include "mbed.h"

class MotorDriverManagerRS485 {
protected:
    FunctionPointer _callback;

public:
    MotorDriverManagerRS485(PinName txPinName, PinName rxPinName, int baudrate);

    void setSpeeds(int speed1, int speed2, int speed3, int speed4, int speed5);

    int* getSpeeds();

    void update();

    void attach(void (*function)(void)) {
        _callback.attach(function);
    }

    template<typename T>
    void attach(T *object, void (T::*member)(void)) {
        _callback.attach( object, member );
    }

private:
    Serial serial;

    uint8_t RBR;

    void rxHandler();

    void deviceWrite(char *sendData, int length);

    int receiveCounter = 0;
    char receiveBuffer[64];
    char sendBuffer[64];

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
