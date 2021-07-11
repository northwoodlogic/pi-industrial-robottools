/*
 * Copyright 2021 Dave Rush
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "SLCan.h"
#include "MTimer.h"
#include "IBT2Servo.h"
#include "IBT2Motor.h"
#include "LimitSwitch.h"
#include <Servo.h>

// PIN 5 & 6, R/L PWM signals respectively
#define SERVO_ENR      7
#define SERVO_ENL      8
#define SERVO_PWML     5
#define SERVO_PWMR     6
#define SERVO_POSITION 3

#define MOTOR_ENR       2
#define MOTOR_ENL       4
#define MOTOR_PWML      9
#define MOTOR_PWMR      10

/*
 * Controller Area Network (CAN) bus receiver/transmitter. This implementation
 * speaks the Can over Serial ASCII protocol, compatible with the Linux SLCan
 * bus interface driver.
 *
 * Received messages are handled via lookup table / callback mechanism.
 * Transmitted messages are enqueued directly to the serial port transmit
 * buffer.
 *
 * RX / TX operation is non-blocking, assuming outgoing CAN frames are at a
 * rate which does not cause the TX buffer to fill to capacity.
 */
SLCan  can;

/*
 * On a 1 second interval, send telemetry onto the CAN bus
 */
MTimer tsend(1000);

/*
 * Run the steering servo loop at 100Hz, calling the update functions every
 * 10mS
 */
MTimer servo(10);

/*
 * Disable both motor drive H-Bridges if a control setpoint is not received
 * within 200mS. This is an extremely important function, as it prevents
 * uncontrolled motion in the event the controlling base station goes offline
 * or communication is severed.
 */
MTimer drivesOff(200); 

/*
 * Pin numbers could be the same on all implementations of this project.
 * Signal inversion is dependent how motor and position sensor feedback leads
 * are wired up.
 */
IBT2Servo ibt2s(SERVO_ENL, SERVO_ENR, SERVO_PWML, SERVO_PWMR, SERVO_POSITION, 1);
IBT2Motor ibt2m(MOTOR_ENL, MOTOR_ENR, MOTOR_PWML, MOTOR_PWMR, 1);

/* Update throttle and steering position */
void UpdateServoSetpoint(uint8_t *data, uint8_t len) {
    if (len != 4)
        return;

    int16_t throttle = (int16_t)(((uint16_t)(data[0] << 8)) | (uint16_t)data[1]);
    int16_t steering = (int16_t)(((uint16_t)(data[2] << 8)) | (uint16_t)data[3]);
    ibt2m.update(throttle);
    ibt2s.update(steering);
    ibt2s.enable();
    ibt2m.enable();

    drivesOff.reset();
    digitalWrite(LED_BUILTIN, LOW);
}

/*
 * Can message ID callback table. Currently there is only a single message
 * that is handled, 0x014. This message contains control information for
 * steering position and speed/direction.
 */
const struct CanRxTab CanRxTab[] = {
  { .CanID = {'0', '1', '4'}, .cb = UpdateServoSetpoint },
};

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);

    /*
     * These clamping values were determined with the actuator pin pulled and
     * reading the position value off of the sensor with the wheels turned to
     * the mechanical limits minus some small margin.
     */
    ibt2s.clamp((int16_t)0xffc6, (int16_t)0x00a6);

    /*
     * This was also determined with the actuator pin pulled and steering
     * mechanism manually centered.
     */
    ibt2s.trim(0x0032);

    /*
     * Allow the rear wheel motor drive full range in both directions.
     */
    ibt2m.clamp(-511, 511);

    Serial.begin(115200);
    can.begin(&Serial, &(CanRxTab[0]), ARRAY_LEN(CanRxTab));
}


void loop() {

    /* Poll serial RX buffer for available data. This is non-blocking */
    can.input();

    /* Turn off motor drives, deadline timer expired */
    if (drivesOff.timedout()) {
        ibt2m.disable();
        ibt2s.disable();
        digitalWrite(LED_BUILTIN, HIGH);
    }

    if (servo.timedout()) {
        ibt2s.tick();
        ibt2m.tick();
    }

    if (tsend.timedout()) {

        uint8_t data[8];
        int16_t val;
        val = ibt2m.getSetpoint();
        data[0] = (uint8_t)(val >> 8); 
        data[1] = (uint8_t)(val); 

        val = ibt2m.getError();
        data[2] = (uint8_t)(val >> 8); 
        data[3] = (uint8_t)(val); 

        val = ibt2s.getComputedSetpoint();
        data[4] = (uint8_t)(val >> 8); 
        data[5] = (uint8_t)(val); 

        val = ibt2s.getPosition();
        data[6] = (uint8_t)(val >> 8); 
        data[7] = (uint8_t)(val); 

        can.send("008", data, 8);
    }
}
