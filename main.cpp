#include "mbed.h"
#include "EthernetInterface.h"
#include "neopixel.h"
#include "coilgun.h"
#include "MCP3021.h"
#include "MotorDriverManagerRS485.h"
#include "CisecoManager.h"
#include "LedManager.h"

#define SERVER_PORT   8042

MotorDriverManagerRS485 motors(P2_0, P2_1);
CisecoManager ciseco(P0_10, P0_11);
LedManager leds(P0_9);

PwmOut servo1(P2_5);
PwmOut servo2(P2_4);

MCP3021 coilADC(P0_27, P0_28, 5.0);
//Coilgun coilgun(P0_29, P3_25, P3_26, P0_30);
Coilgun coilgun(P0_29, P3_25, P3_26, NC);

//https://developer.mbed.org/questions/897/P029-P030-as-GPIO/
//DigitalOut test1(P0_29);
DigitalOut test2(P0_30);
//DigitalOut test3(P3_25);

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

char ethBuffer[64];
char ethSendBuffer[64];

void executeCommand(char *buffer);

char sendBuffer[64];

char *messageDischarged = (char *)"<discharged>";
char *messageKicked = (char *)"<kicked>";
char *messageToggleSide = (char *)"<toggle-side>";
char *messageToggleGo = (char *)"<toggle-go>";
char *messageHasBall = (char *)"<ball:1>";
char *messageNoBall = (char *)"<ball:0>";

bool returnSpeeds = true;

bool failSafeEnabled = true;
int failSafeCountMotors = 0;
int failSafeCountCoilgun = 0;
int failSafeLimitMotors = 500;
int failSafeLimitCoilgun = 5000;

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

unsigned int currentKickLength = 0;
unsigned int currentKickDelay = 0;
unsigned int currentChipLength = 0;
unsigned int currentChipDelay = 0;
bool kickWhenBall = false;
bool sendKicked = false;
unsigned int noBallKickLength = 500;
unsigned int noBallChipLength = 500;

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

void handleSpeedsSent() {
    if (returnSpeeds) {
        int *currentSpeeds = motors.getSpeeds();

        int charCount = sprintf(ethSendBuffer, "<speeds:%d:%d:%d:%d:%d>",
                                currentSpeeds[1], currentSpeeds[2], currentSpeeds[0], currentSpeeds[3],
                                currentSpeeds[4]);

        server.sendTo(client, ethSendBuffer, charCount);
    }
}

void handleCisecoMessage() {
    int charCount = sprintf(ethSendBuffer, "<ref:%s>", ciseco.read());
    server.sendTo(client, ethSendBuffer, charCount);
}

int main() {
    ciseco.baud(115200);
    ciseco.attach(&handleCisecoMessage);

    motors.baud(150000);
    motors.attach(&handleSpeedsSent);

    sensorUpdate.attach(&updateTick, 0.001);

    eth.init("192.168.4.1", "255.255.255.0", "192.168.4.8");

    eth.connect(10000);

    server.bind(SERVER_PORT);

    server.set_blocking(false, 1);

    servo1.period_us(20000);

    servo1.pulsewidth_us(1500);
    servo2.pulsewidth_us(1500);

    int isFirst = true;

    while(1) {
        motors.update();

        ciseco.update();

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

            failSafeCountMotors++;
            failSafeCountCoilgun++;

            if (failSafeCountMotors == failSafeLimitMotors) {
                failSafeCountMotors = 0;

                if (failSafeEnabled) {
                    returnSpeeds = false;
                    motors.setSpeeds(0, 0, 0, 0, 0);
                }
            }

            if (failSafeCountCoilgun == failSafeLimitCoilgun) {
                failSafeCountCoilgun = 0;

                if (failSafeEnabled) {
                    coilgun.discharge();

                    if (!coilgun.isCharged) {
                        //int charCount = sprintf(sendBuffer, "<discharged>");
                        //server.sendTo(client, sendBuffer, charCount);
                        server.sendTo(client, messageDischarged, 12);
                    }
                }
            }

            if (goalButtonDebounceCounter < goalButtonDebounceCount) {
                goalButtonDebounceCounter++;
            } else if (goalButtonDebounceCounter == goalButtonDebounceCount) {
                goalButtonDebounceCounter++;
                if (!goalButtonState) {
                    /*int charCount = sprintf(sendBuffer, "<toggle-side>");
                    server.sendTo(client, sendBuffer, charCount);*/
                    server.sendTo(client, messageToggleSide, 13);
                }
            }

            if (startButtonDebounceCounter < startButtonDebounceCount) {
                startButtonDebounceCounter++;
            } else if (startButtonDebounceCounter == startButtonDebounceCount) {
                startButtonDebounceCounter++;
                if (startButtonState) {
                    /*int charCount = sprintf(sendBuffer, "<toggle-go>");
                    server.sendTo(client, sendBuffer, charCount);*/
                    server.sendTo(client, messageToggleGo, 11);
                }
            }

            if (updateLeds) {
                updateLeds = 0;

                if (currentGoal == 2) {
                    if (blinkState) {
                        leds.setLedColor(0, LedManager::BLUE);
                    } else {
                        leds.setLedColor(0, LedManager::YELLOW);
                    }
                } else if (currentGoal == 0) {
                    if (blinkState) {
                        leds.setLedColor(0, LedManager::BLUE);
                    } else {
                        leds.setLedColor(0, LedManager::OFF);
                    }
                } else if (currentGoal == 1) {
                    if (blinkState) {
                        leds.setLedColor(0, LedManager::YELLOW);
                    } else {
                        leds.setLedColor(0, LedManager::OFF);
                    }
                }

                blinkState = !blinkState;

                leds.update();
            }
        }

        int newBallState = ball;
        if (ballState != newBallState) {

            if (newBallState) {
                leds.setLedColor(1, LedManager::MAGENTA);
            } else {
                leds.setLedColor(1, LedManager::OFF);
            }

            /*int charCount = sprintf(sendBuffer, "<ball:%d>", newBallState);
            server.sendTo(client, sendBuffer, charCount);*/
            if (newBallState) {
                server.sendTo(client, messageHasBall, 8);
            } else {
                server.sendTo(client, messageNoBall, 8);
            }
            //pc.printf("<ball:%d>\n", newBallState);
            ballState = newBallState;

            if (kickWhenBall && ballState) {
                kickWhenBall = false;
                coilgun.kick(currentKickLength, currentKickDelay, currentChipLength, currentChipDelay);
                sendKicked = true;
            }

            if (!ballState && sendKicked) {
                sendKicked = false;
                //pc.printf("<kicked>\n");
                //int charCount = sprintf(sendBuffer, "<kicked>");
                //server.sendTo(client, sendBuffer, charCount);
                server.sendTo(client, messageKicked, 8);
            }
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
    failSafeCountMotors = 0;
    failSafeCountCoilgun = 0;

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
    } else if (strncmp(cmd, "kick", 4) == 0) {
        unsigned int kickLength = (unsigned int) atoi(strtok(NULL, ":"));
        coilgun.kick(kickLength, 0, 0, 0);
    } else if (strncmp(cmd, "dkick", 5) == 0) {
        unsigned int kickLength = (unsigned int) atoi(strtok(NULL, ":"));
        unsigned int kickDelay = (unsigned int) atoi(strtok(NULL, ":"));
        unsigned int chipLength = (unsigned int) atoi(strtok(NULL, ":"));
        unsigned int chipDelay = (unsigned int) atoi(strtok(NULL, ":"));
        //pc.printf("kick:%d:%d:%d:%d\n", kickLength, kickDelay, chipLength, chipDelay);
        coilgun.kick(kickLength, kickDelay, chipLength, chipDelay);
    } else if (strncmp(cmd, "bdkick", 6) == 0) {
        currentKickLength = (unsigned int) atoi(strtok(NULL, ":"));
        currentKickDelay = (unsigned int) atoi(strtok(NULL, ":"));
        currentChipLength = (unsigned int) atoi(strtok(NULL, ":"));
        currentChipDelay = (unsigned int) atoi(strtok(NULL, ":"));
        //pc.printf("kick:%d:%d:%d:%d\n", kickLength, kickDelay, chipLength, chipDelay);
        if (ballState) {
            coilgun.kick(currentKickLength, currentKickDelay, currentChipLength, currentChipDelay);
            kickWhenBall = false;
        } else {
            kickWhenBall = true;
        }
    } else if (strncmp(cmd, "nokick", 6) == 0) {
        kickWhenBall = false;
    } else if (strncmp(cmd, "charge", 6) == 0) {
        coilgun.charge();
    } else if (strncmp(cmd, "discharge", 9) == 0) {
        //pc.printf("discharge\n");
        coilgun.discharge();
    } else if (strncmp(cmd, "gs", 2) == 0) {
        int *speeds = motors.getSpeeds();

        int charCount = sprintf(sendBuffer, "<speeds:%d:%d:%d:%d:%d>",
                                speeds[1], speeds[2], speeds[0], speeds[3], speeds[4]);
        server.sendTo(client, sendBuffer, charCount);
    } else if (strncmp(cmd, "rf", 2) == 0) {
        //ciseco.send(strtok(NULL, ":"), strlen(buffer) - 3);

        //char *message = strtok(NULL, ":");

        //int charCount = sprintf(sendBuffer, "<rf:%d:%s>", strlen(message), message);
        //server.sendTo(client, sendBuffer, charCount);

        //char test[] = "test";
        ciseco.send(buffer + 3);
    } else if (strncmp(cmd, "reset", 5) == 0) {
        motors.setSpeeds(0, 0, 0, 0, 0);
        currentGoal = 2;
        leds.setLedColor(1, LedManager::OFF);
    } else if (strncmp(cmd, "fs", 2) == 0) {
        failSafeEnabled = (bool)atoi(strtok(NULL, ":"));
    } else if (strncmp(cmd, "target", 6) == 0) {
        int target = atoi(strtok(NULL, ":"));
        currentGoal = target;
        if (target == 0) {
            leds.setLedColor(0, LedManager::BLUE);
        } else if (target == 1) {
            leds.setLedColor(0, LedManager::YELLOW);
        } else if (target == 2) {
            leds.setLedColor(0, LedManager::OFF);
        }
    } else if (strncmp(cmd, "error", 5) == 0) {
        leds.setLedColor(1, LedManager::RED);
    } else if (strncmp(cmd, "go", 2) == 0) {
        leds.setLedColor(1, LedManager::GREEN);
    } else if (strncmp(cmd, "adc", 3) == 0) {
        int charCount = sprintf(sendBuffer, "<adc:%.1f>", coilADC.read() * 80);
        server.sendTo(client, sendBuffer, charCount);
    } else if (strncmp(cmd, "refshort", 8) == 0) {
        ciseco.setShortCommandMode((bool)atoi(strtok(NULL, ":")));
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
