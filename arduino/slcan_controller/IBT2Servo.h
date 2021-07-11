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

#ifndef IBT2_SERVO_H
#define IBT2_SERVO_H

/*
 * This is a "Big Servo" implementation using IBT2 H-Bridge and an analog
 * input pin.
 */

class IBT2Servo {
    public:
        IBT2Servo(uint8_t enl, uint8_t enr, uint8_t pwml, uint8_t pwmr, uint8_t posi, uint8_t invert);

        /*
         * Run the servo state computation loop. Call on a 100Hz interval.
         * This class does not maintain an internal timer.
         */
        void tick();

        /*
         * Enable H-Bridge drive output. This writes a 1 to the enl/enr pins,
         * enabling the primary switching elements.
         */
        void enable();

        /*
         * Disable H-Bridge drive output. This writes a 0 to the en pins,
         * disabling the primary switching elements. This is most useful as a
         * maintenance or calibration mode where the mechanical linkage is
         * physically disconnected but position feedback must be acquired.
         */
        void disable();

        /*
         * Update the command setpoint.This is the desired servo shaft
         * position. The absolute maximum range is 1024 discrete positions,
         * centered around 1/2 of the ADC converter resolution, or [-512, 511]
         * inclusive. A setpoint of zero will command the output shaft to mid
         * position.
         *  magnitude values:
         *      < 0 : 4th quadrant position
         *      = 0 : Center position
         *      > 0 : 1st quadrant position
         */
        void update(int16_t setpoint);

        /*
         * Limit the setpoint to prevent mechanically over driving the
         * servomechanism. By default these are zero and MUST be set, or the
         * motor will not be driven.
         */
        void clamp(int16_t clampl, int16_t clamph);
        void trim(int16_t trim) { _trim = trim; };

        /*
         * Reporting / telemetry feedback functions, they read live data or
         * return settings
         */

        int16_t getTrim() { return _trim; }
        int16_t getError() { return _error; }
        int16_t getPosition() { return _position; }
        int16_t getComputedSetpoint() { return _computed_setpoint; }

    private:
         /*
          * H-Bridge L & R enable input signals. 
          */
         uint8_t _enl; /* ENL & ENR output pin numbers */
         uint8_t _enr;

         /*
          * PWM pin output signals. Pick two pins that have the same PWM
          * frequency.
          * Arduino UNO
          *     980Hz --> 5, 6
          *     490Hz --> 3, 9, 10, 11
          */
         uint8_t _pwml; /* "L" PWM output pin number */
         uint8_t _pwmr; /* "R" PWM output pin number */

         /*
          * Analog position feedback input pin number. Try to wire the
          * potentiometer up such that the raw ADC conversion value increases
          * as control motion is swept in a clockwise direction. However, if
          * that is not possible, the sign of the ADC conversion result may be
          * inverted by passing in a non-zero value to the "invert"
          * constructor parameter.
          *
          * Calibrate mechanics such that the physical mid-position results in
          * a RAW ADC conversion value as close as possible to the ADC
          * conversion range mid point. Any error resulting from physical
          * misalignment may be trimmed with a small calibration constant.
          */
         uint8_t _posi; /* Position sensor input pin number */
         uint8_t _invert; /* Invert ADC conversion sign */

         /*
          * Trim and shaft position setpoint. Both values default to zero and
          * may be set at any time.
          */
         int16_t _trim;
         int16_t _setpoint;

         /*
          * Computed error and measured position
          */
         int16_t _error;
         int16_t _position;
         int16_t _computed_setpoint; // value after trim and clamp

         /*
          * Low & High value clamp positions. Physical limitations may not
          * allow full ADC conversion range. Prevent blow out and other damage
          * by limiting to safe allowable ranges.
          *
          * These values default to zero and MUST be set by the application.
          */
         int16_t _clampl; /* valid range [-512,   0]; */
         int16_t _clamph; /* valid range [   0, 511]; */

         /*
          * Drive the PWM units based on the calculated error value
          */
         void drive(int16_t error);
};


#endif

