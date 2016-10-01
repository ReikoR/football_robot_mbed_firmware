#ifndef CISECOMANAGER_H
#define CISECOMANAGER_H

#include "mbed.h"

class CisecoManager {
protected:
    FunctionPointer _callback;

public:
    CisecoManager(PinName txPinName, PinName rxPinName);

    void baud(int baudrate);

    char *read();

    void send(char *sendData, int length);

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

    int serialId;

    void rxHandler(void);

    void serialWrite(char *sendData, int length);
    char serialReadChar();

    int receiveCounter;
    char receiveBuffer[16];

    char receivedMessage[13];
};


#endif //CISECOMANAGER_H
