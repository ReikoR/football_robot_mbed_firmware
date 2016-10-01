#include "mbed.h"
#include "EthernetInterface.h"
#include "neopixel.h"
#include "coilgun.h"
#include "MCP3021.h"
#include "MotorDriverManagerRS485.h"

#define SERVER_PORT   8042

// This must be an SPI MOSI pin.
#define DATA_PIN P0_9

MotorDriverManagerRS485 motors(P2_0, P2_1, 150000);

Serial erfRef(P0_10, P0_11);

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

int erfReceiveCounter = 0;
char erfReceiveBuffer[16];

void executeCommand(char *buffer);

char sendBuffer[64];

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

void updateTick() {
    if (ledCounter++ > ledCount) {
        ledCounter = 0;
        //led1 = !led1;

        updateLeds = 1;
    }

    update = 1;
}

void erfRx() {
    while (erfRef.readable()) {
        char c = LPC_UART2->RBR;

        if (erfReceiveCounter < 12) {
            switch (erfReceiveCounter) {
                case 0:
                    if (c == 'a') {
                        erfReceiveBuffer[erfReceiveCounter] = c;
                        erfReceiveCounter++;
                    } else {
                        erfReceiveCounter = 0;
                    }
                    break;
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                case 10:
                    erfReceiveBuffer[erfReceiveCounter] = c;
                    erfReceiveCounter++;
                    break;
                case 11:
                    erfReceiveBuffer[erfReceiveCounter] = c;
                    erfReceiveCounter++;
                    break;
                default:
                    erfReceiveCounter = 0;
            }
        }
    }
}

void led1UpdateTick() {
    led1Update = 1;
}

void led2UpdateTick() {
    led2Update = 1;
}

void handleSpeedsSent() {
    if (returnSpeeds) {
        int *currentSpeeds = motors.getSpeeds();

        int charCount = sprintf(ethSendBuffer, "<speeds:%d:%d:%d:%d:%d>",
                                currentSpeeds[1], currentSpeeds[2], currentSpeeds[0], currentSpeeds[3],
                                currentSpeeds[4]);

        server.sendTo(client, ethSendBuffer, charCount);
    }
}

int main() {
    erfRef.baud(115200);

    erfRef.attach(&erfRx);

    motors.attach(&handleSpeedsSent);

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
        motors.update();

        if (erfReceiveCounter == 12) {
            erfReceiveBuffer[12] = '\0';

            int charCount = sprintf(ethSendBuffer, "<ref:%s>", erfReceiveBuffer);
            server.sendTo(client, ethSendBuffer, charCount);

            erfReceiveCounter = 0;
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
        motors.setSpeeds(speed1, speed2, speed3, speed4, speed5);
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
        unsigned int kickLength = (unsigned int) atoi(strtok(NULL, ":"));
        unsigned int kickDelay = (unsigned int) atoi(strtok(NULL, ":"));
        unsigned int chipLength = (unsigned int) atoi(strtok(NULL, ":"));
        unsigned int chipDelay = (unsigned int) atoi(strtok(NULL, ":"));
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
        int *speeds = motors.getSpeeds();

        int charCount = sprintf(sendBuffer, "<speeds:%d:%d:%d:%d:%d>",
                                speeds[1], speeds[2], speeds[0], speeds[3], speeds[4]);
        server.sendTo(client, sendBuffer, charCount);
    } /*else if (strncmp(cmd, "reset", 5) == 0) {
        motors.setSpeeds(0, 0, 0, 0, 0);
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
