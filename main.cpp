#include "mbed.h"
#include "EthernetInterface.h"
#include "neopixel.h"
#include "coilgun.h"
#include "MCP3021.h"

#define SERVER_PORT   8042

// This must be an SPI MOSI pin.
#define DATA_PIN P0_9

Serial device(P2_0, P2_1);  // tx, rx
//Serial device(P0_15, P0_16);  // tx, rx

//DigitalOut led1(P0_25);
//DigitalOut led2(P0_26);

PwmOut servo1(P2_5);
PwmOut servo2(P2_4);

MCP3021 coilADC(P0_27, P0_28, 5.0);
//Coilgun coilgun(P0_30, P0_29, P3_25, P3_26);
Coilgun coilgun(P0_30, P0_29, P3_25, NC);

DigitalIn ball(P1_29);
DigitalIn goalButton(P0_26);
DigitalIn startButton(P0_25);

Ticker sensorUpdate;
volatile int update = 0;
volatile int updateLeds = 0;
bool blinkState = false;
int ledState = 0;

extern "C" void mbed_mac_address(char *s) {
    char mac[6];
    mac[0] = 0x00;
    mac[1] = 0x02;
    mac[2] = 0xf7;
    mac[3] = 0xf0;
    mac[4] = 0x45;
    mac[5] = 0xbe;
    // Write your own mac address here
    memcpy(s, mac, 6);
}

Ticker led1Ticker;
volatile int led1Update = 0;

Ticker led2Ticker;
volatile int led2Update = 0;

char ethBuffer[64];
char ethSendBuffer[64];

void executeCommand(char *buffer);
void deviceWrite(char *sendData, int length);

int charCounter = 0;
char serialBuffer[64];
char sendBuffer[64];

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

bool returnSpeeds = true;

int ledCount = 1000;
int ledCounter = 0;
int ballState = 0;
int goalButtonState = goalButton;
int startButtonState = startButton;

int goalButtonDebounceCount = 50;
int startButtonDebounceCount = 50;
int goalButtonDebounceCounter = goalButtonDebounceCount + 1;
int startButtonDebounceCounter = goalButtonDebounceCount + 1;

int currentGoal = 2;

UDPSocket server;
Endpoint client;

EthernetInterface eth;

void setSpeeds(int speed1, int speed2, int speed3, int speed4, int speed5) {
    speeds[1] = speed1;
    speeds[2] = speed2;
    speeds[0] = speed3;
    speeds[3] = speed4;
    speeds[4] = speed5;

    //t.start();
    isSettingSpeeds = true;
    txSend = 1;
    //led3 = 1;
}

void updateTick() {
    if (ledCounter++ > ledCount) {
        ledCounter = 0;
        //led1 = !led1;

        updateLeds = 1;
    }

    update = 1;
}

void deviceRx() {
    // Interrupt does not work with RTOS when using standard functions (getc, putc)
    // https://developer.mbed.org/forum/bugs-suggestions/topic/4217/

    while (device.readable()) {
        //char c = device.getc();
        char c = LPC_UART1->RBR;

        //LPC_UART0->RBR = c;

        //pc.putc('-');
        //pc.putc(c);

        //receiveBuffer[receiveCounter] = c;
        //receiveCounter++;

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

    //led4 = !led4;
}

void led1UpdateTick() {
    led1Update = 1;
}

void led2UpdateTick() {
    led2Update = 1;
}

void generate(neopixel::Pixel * out, uint32_t index, uintptr_t extra) {
    uint32_t brightness = (index + extra) >> 3;
    out->red   = ((index + extra) & 0x1) ? brightness : 0;
    out->green = ((index + extra) & 0x2) ? brightness : 0;
    out->blue  = ((index + extra) & 0x4) ? brightness : 0;
}

int main() {
    device.baud(150000);

    device.attach(&deviceRx);

    sensorUpdate.attach(&updateTick, 0.001);

    led1Ticker.attach(&led1UpdateTick, 3.0);
    led2Ticker.attach(&led2UpdateTick, 1.0);

    // Create a temporary DigitalIn so we can configure the pull-down resistor.
    // (The mbed API doesn't provide any other way to do this.)
    // An alternative is to connect an external pull-down resistor.
    DigitalIn(DATA_PIN, PullDown);

    // The pixel array control class.
    neopixel::PixelArray array(DATA_PIN);

    eth.init("192.168.4.1", "255.255.255.0", "192.168.4.8");

    eth.connect(10000);

    server.bind(SERVER_PORT);

    server.set_blocking(false, 1);

    neopixel::Pixel pixels[] = {
            {0, 0, 40}, {40, 40, 0}
    };
    neopixel::Pixel pixels2[] = {
            {0, 40, 0}, {0, 30, 30}
    };

    servo1.period_us(20000);

    servo1.pulsewidth_us(1500);
    servo2.pulsewidth_us(1500);

    uint32_t offset = 0;

    int isFirst = true;

    array.update(pixels, 2);

    while(1) {

        if (receiveCounter == 8) {

            //pc.putc('\n');

            if (receiveBuffer[2] == 'd') {
                int value = ((int)receiveBuffer[3]) | ((int)receiveBuffer[4] << 8) | ((int)receiveBuffer[5] << 16) | ((int)receiveBuffer[6] << 24);
                value = ((value >> 8) * 1000) >> 16;

                if (receiveBuffer[1] == deviceIds[activeSpeedIndex]) {
                    actualSpeeds[activeSpeedIndex] = value;
                    //sendNextSpeed = true;
                    //txDelayActive = 1;
                    if (activeSpeedIndex == 4) {
                        isSettingSpeeds = false;
                        //led3 = 0;
                    } else {
                        txSend = 1;
                    }
                }

                activeSpeedIndex++;

                if (activeSpeedIndex == 5) {
                    activeSpeedIndex = 0;

                    //t.stop();

                    //pc.printf("s:%d:%d:%d:%d:%d\n", actualSpeeds[0], actualSpeeds[1], actualSpeeds[2], actualSpeeds[3], actualSpeeds[4]);
                    //pc.printf("t: %d", t.read_us());

                    if (returnSpeeds) {
                        int charCount = sprintf(ethSendBuffer, "<speeds:%d:%d:%d:%d:%d>",
                                                actualSpeeds[1], actualSpeeds[2], actualSpeeds[0], actualSpeeds[3], actualSpeeds[4]);
                        server.sendTo(client, ethSendBuffer, charCount);
                    }

                    //t.reset();
                }
            } else {
                //sendNextSpeed = true;
                txDelayActive = 1;
                txSend = 1;
            }

            receiveCounter = 0;
            //}
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

        if (led1Update) {
            led1Update = 0;
            //led1 = !led1;
        }

        if (led2Update) {
            led2Update = 0;
            //led2 = !led2;

            if (isFirst) {
                array.update(pixels, 2);
            } else {
                array.update(pixels2, 2);
            }

            isFirst = !isFirst;

            /*int qSpeed = ((0 << 16) / 1000) << 8;

            sendBuffer[0] = '<';
            sendBuffer[1] = deviceIds[activeSpeedIndex];
            sendBuffer[2] = 's';

            int * intlocation = (int*)(&sendBuffer[3]);
            *intlocation = qSpeed;

            sendBuffer[7] = '>';

            deviceWrite(sendBuffer, 8);*/
        }

        int n = server.receiveFrom(client, ethBuffer, sizeof(ethBuffer));

        if (n > 0) {
            //pc.printf("Received packet from: %s\n", client.get_address());
            //pc.printf("n: %d\n", n);
            ethBuffer[n] = '\0';
            //server.sendTo(client, ethBuffer, n);
            //led2 = !led2;
            executeCommand(ethBuffer);
        }

        if (update) {
            update = 0;

            if (goalButtonDebounceCounter < goalButtonDebounceCount) {
                goalButtonDebounceCounter++;
            } else if (goalButtonDebounceCounter == goalButtonDebounceCount) {
                goalButtonDebounceCounter++;
                if (!goalButtonState) {
                    int charCount = sprintf(sendBuffer, "<toggle-side>");
                    server.sendTo(client, sendBuffer, charCount);
                }
            }

            if (startButtonDebounceCounter < startButtonDebounceCount) {
                startButtonDebounceCounter++;
            } else if (startButtonDebounceCounter == startButtonDebounceCount) {
                startButtonDebounceCounter++;
                if (startButtonState) {
                    int charCount = sprintf(sendBuffer, "<toggle-go>");
                    server.sendTo(client, sendBuffer, charCount);
                }
            }

            if (updateLeds) {
                updateLeds = 0;

                /*if (currentGoal == 2) {
                    rgbLed1.toggle();
                } else if (currentGoal == 0) {
                    if (blinkState) {
                        rgbLed1.setColor(RgbLed::BLUE);
                    } else {
                        rgbLed1.setColor(RgbLed::OFF);
                    }
                } else if (currentGoal == 1) {
                    if (blinkState) {
                        rgbLed1.setColor(RgbLed::YELLOW);
                    } else {
                        rgbLed1.setColor(RgbLed::OFF);
                    }
                }

                blinkState = !blinkState;*/
            }
        }

        int newBallState = ball;
        if (ballState != newBallState) {

            /*if (newBallState) {
                rgbLed2.setColor(RgbLed::MAGENTA);
            } else {
                rgbLed2.setColor(RgbLed::OFF);
            }*/

            int charCount = sprintf(sendBuffer, "<ball:%d>", newBallState);
            server.sendTo(client, sendBuffer, charCount);
            //pc.printf("<ball:%d>\n", newBallState);
            ballState = newBallState;
        }

        int newGoalButtonState = goalButton;
        if (goalButtonState != newGoalButtonState) {
            goalButtonDebounceCounter = 0;

            goalButtonState = newGoalButtonState;
        }

        int newStartButtonState = startButton;
        if (startButtonState != newStartButtonState) {
            startButtonDebounceCounter = 0;

            startButtonState = newStartButtonState;
        }
    }
}

void executeCommand(char *buffer) {
    //failSafeCountMotors = 0;
    //failSafeCountCoilgun = 0;

    char *cmd;
    cmd = strtok(buffer, ":");

    //pc.printf("%s\n", cmd);

    if (strncmp(cmd, "speeds", 6) == 0) {
        int speed1 = atoi(strtok(NULL, ":"));
        int speed2 = atoi(strtok(NULL, ":"));
        int speed3 = atoi(strtok(NULL, ":"));
        int speed4 = atoi(strtok(NULL, ":"));
        int speed5 = atoi(strtok(NULL, ":"));

        returnSpeeds = true;
        setSpeeds(speed1, speed2, speed3, speed4, speed5);
    } else if (strncmp(cmd, "servos", 6) == 0) {
        int servo1Duty = atoi(strtok(NULL, ":"));
        int servo2Duty = atoi(strtok(NULL, ":"));

        //servo1.pulsewidth_us(servo1Duty);
        //servo2.pulsewidth_us(servo2Duty);

        if (servo1Duty + servo2Duty > 3180) {
            //pc.printf("<err:servo1Duty + servo2Duty must be smaller than 3180>\n");
        } else {
            if (servo1Duty < 800 || servo1Duty > 2200) {
                //pc.printf("<err:servo1Duty must be between 800 and 2200>\n");
            } else {
                servo1.pulsewidth_us(servo1Duty);
            }

            if (servo2Duty < 800 || servo2Duty > 2200) {
                //pc.printf("<err:servo2Duty must be between 800 and 2200>\n");
            } else {
                servo2.pulsewidth_us(servo2Duty);
            }
        }
    } /*else if (strncmp(cmd, "kick", 4) == 0) {
        unsigned int kickLength = atoi(strtok(NULL, ":"));
        coilgun.kick(kickLength, 0, 0, 0);
    }*/ else if (strncmp(cmd, "dkick", 5) == 0) {
        unsigned int kickLength = atoi(strtok(NULL, ":"));
        unsigned int kickDelay = atoi(strtok(NULL, ":"));
        unsigned int chipLength = atoi(strtok(NULL, ":"));
        unsigned int chipDelay = atoi(strtok(NULL, ":"));
        //pc.printf("kick:%d:%d:%d:%d\n", kickLength, kickDelay, chipLength, chipDelay);
        coilgun.kick(kickLength, kickDelay, chipLength, chipDelay);
    } /*else if (strncmp(cmd, "bdkick", 6) == 0) {
        currentKickLength = atoi(strtok(NULL, ":"));
        currentKickDelay = atoi(strtok(NULL, ":"));
        currentChipLength = atoi(strtok(NULL, ":"));
        currentChipDelay = atoi(strtok(NULL, ":"));
        //pc.printf("kick:%d:%d:%d:%d\n", kickLength, kickDelay, chipLength, chipDelay);
        if (ballState) {
            coilgun.kick(currentKickLength, currentKickDelay, currentChipLength, currentChipDelay);
            kickWhenBall = false;
        } else {
            kickWhenBall = true;
        }
    } else if (strncmp(cmd, "nokick", 6) == 0) {
        kickWhenBall = false;
    }*/ else if (strncmp(cmd, "charge", 6) == 0) {
        coilgun.charge();
    } else if (strncmp(cmd, "discharge", 9) == 0) {
        //pc.printf("discharge\n");
        coilgun.discharge();
    } else if (strncmp(cmd, "gs", 2) == 0) {
        int charCount = sprintf(sendBuffer, "<speeds:%d:%d:%d:%d:%d>",
                                speeds[1], speeds[2], speeds[0], speeds[3], speeds[4]);
        server.sendTo(client, sendBuffer, charCount);
    } /*else if (strncmp(cmd, "reset", 5) == 0) {
        setSpeeds(0, 0, 0, 0, 0);
    } else if (strncmp(cmd, "fs", 2) == 0) {
        failSafeEnabled = (bool)atoi(strtok(NULL, ":"));
    } else if (strncmp(cmd, "target", 6) == 0) {
        int target = atoi(strtok(NULL, ":"));
        currentGoal = target;
        if (target == 0) {
            rgbLed1.setColor(RgbLed::BLUE);
        } else if (target == 1) {
            rgbLed1.setColor(RgbLed::YELLOW);
        } else if (target == 2) {
            rgbLed1.setColor(RgbLed::OFF);
        }
    } else if (strncmp(cmd, "error", 5) == 0) {
        rgbLed2.setRed((bool)atoi(strtok(NULL, ":")));
    } else if (strncmp(cmd, "go", 2) == 0) {
        rgbLed2.setGreen((bool)atoi(strtok(NULL, ":")));
    }*/ else if (strncmp(cmd, "adc", 3) == 0) {
        int charCount = sprintf(sendBuffer, "<adc:%.1f>", coilADC.read() * 80);
        server.sendTo(client, sendBuffer, charCount);
    } /*else if (strncmp(cmd, "k", 1) == 0) {
        unsigned int kickLength = atoi(strtok(NULL, ":"));
        coilgun.kick(kickLength, 0, 0, 0);
    } else if (strncmp(cmd, "g", 1) == 0) {
        unsigned int kickLength = atoi(strtok(NULL, ":"));
        coilgun.kick(0, 0, kickLength, 0);
    } else if (strncmp(cmd, "d", 1) == 0) {
        coilgun.discharge();
    } else if (strncmp(cmd, "c", 1) == 0) {
        if (atoi(strtok(NULL, ":")) == 1) {
            coilgun.charge();
        } else {
            coilgun.chargeEnd();
        }
    }*/
}

void deviceWrite(char *sendData, int length) {
    int i = 0;

    //pc.putc(sendData[1]);

    while (i < length) {
        if (device.writeable()) {
            device.putc(sendData[i]);
            //pc.putc(sendData[i]);
        }
        i++;
    }

    //pc.putc('\n');
}
