#include "MotorDriverManagerRS485.h"

MotorDriverManagerRS485::MotorDriverManagerRS485(PinName txPinName, PinName rxPinName, int baudrate):
 serial(txPinName, rxPinName) {

    serial.baud(baudrate);

    serial.attach(&rxHandler);
}

void MotorDriverManagerRS485::rxHandler() {
    // Interrupt does not work with RTOS when using standard functions (getc, putc)
    // https://developer.mbed.org/forum/bugs-suggestions/topic/4217/

    while (serial.readable()) {
        //char c = device.getc();
        char c = LPC_UART1->RBR;

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
