#include <TARGET_LPC1768/cmsis.h>
#include "MotorDriverManagerRS485.h"

MotorDriverManagerRS485::MotorDriverManagerRS485(PinName txPinName, PinName rxPinName, int baudrate):
 serial(txPinName, rxPinName) {

    if (rxPinName == P2_1) {
        RBR = LPC_UART1->RBR;
    } else  if (rxPinName == P0_11) {
        RBR = LPC_UART2->RBR;
    }

    serial.baud(baudrate);

    serial.attach(&rxHandler);
}

void MotorDriverManagerRS485::rxHandler() {
    // Interrupt does not work with RTOS when using standard functions (getc, putc)
    // https://developer.mbed.org/forum/bugs-suggestions/topic/4217/

    while (serial.readable()) {
        //char c = device.getc();
        char c = RBR;

        if (receiveCounter < 8) {
            switch (receiveCounter) {
                case 0:
                    if (c == '<') {
                        receiveBuffer[receiveCounter] = c;
                        receiveCounter++;
                    } else {
                        receiveCounter = 0;
                    }
                    break;
                case 1:
                    if (c == '1' || c == '2' || c == '3' || c == '4' || c == '5') {
                        receiveBuffer[receiveCounter] = c;
                        receiveCounter++;
                    } else {
                        receiveCounter = 0;
                    }
                    break;
                case 2:
                    if (c == 'd') {
                        receiveBuffer[receiveCounter] = c;
                        receiveCounter++;
                    } else {
                        receiveCounter = 0;
                    }
                    break;
                case 3:
                case 4:
                case 5:
                case 6:
                    receiveBuffer[receiveCounter] = c;
                    receiveCounter++;
                    break;
                case 7:
                    if (c == '>') {
                        receiveBuffer[receiveCounter] = c;
                        receiveCounter++;
                    } else {
                        receiveCounter = 0;
                    }
                    break;
                default:
                    receiveCounter = 0;
            }
        }
    }
}

void MotorDriverManagerRS485::setSpeeds(int speed1, int speed2, int speed3, int speed4, int speed5) {
    speeds[1] = speed1;
    speeds[2] = speed2;
    speeds[0] = speed3;
    speeds[3] = speed4;
    speeds[4] = speed5;

    isSettingSpeeds = true;
    txSend = 1;
}

void MotorDriverManagerRS485::update() {
    if (receiveCounter == 8) {
        if (receiveBuffer[2] == 'd') {
            int value = ((int)receiveBuffer[3]) | ((int)receiveBuffer[4] << 8) | ((int)receiveBuffer[5] << 16) | ((int)receiveBuffer[6] << 24);
            value = ((value >> 8) * 1000) >> 16;

            if (receiveBuffer[1] == deviceIds[activeSpeedIndex]) {
                actualSpeeds[activeSpeedIndex] = value;
                //sendNextSpeed = true;
                //txDelayActive = 1;
                if (activeSpeedIndex == 4) {
                    isSettingSpeeds = false;
                } else {
                    txSend = 1;
                }
            }

            activeSpeedIndex++;

            if (activeSpeedIndex == 5) {
                activeSpeedIndex = 0;

                _callback.call();
            }
        } else {
            txDelayActive = 1;
            txSend = 1;
        }

        receiveCounter = 0;
    }

    if (txSend) {
        txSend = 0;

        if (isSettingSpeeds) {
            //sendNextSpeed = false;
            int qSpeed = ((speeds[activeSpeedIndex] << 16) / 1000) << 8;

            sendBuffer[0] = '<';
            sendBuffer[1] = deviceIds[activeSpeedIndex];
            sendBuffer[2] = 's';

            int * intlocation = (int*)(&sendBuffer[3]);
            *intlocation = qSpeed;

            sendBuffer[7] = '>';

            deviceWrite(sendBuffer, 8);
        }
    }
}

void MotorDriverManagerRS485::deviceWrite(char *sendData, int length) {
    int i = 0;

    while (i < length) {
        if (serial.writeable()) {
            serial.putc(sendData[i]);
        }
        i++;
    }
}

int *MotorDriverManagerRS485::getSpeeds() {
    return speeds;
}
