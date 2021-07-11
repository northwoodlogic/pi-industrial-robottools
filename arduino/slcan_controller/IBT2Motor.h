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

#ifndef IBT2_MOTOR_H
#define IBT2_MOTOR_H

/*
 * This is a simple motor driver implementation using IBT2 H-Bridge having no speed feedback
 */

class IBT2Motor {
    public:
        IBT2Motor(uint8_t enl, uint8_t enr, uint8_t pwml, uint8_t pwmr, uint8_t invert);

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
         * disabling the primary switching elements. 
         */
        void disable();

        /*
         * Update the command setpoint.This is the desired motor drive level.
         * or [-512, 511] inclusive.
         * position magnitude values:
         *      < 0 : reverse
         *      = 0 : off
         *      > 0 : forward
         */
        void update(int16_t setpoint);

        /*
        */
        void clamp(int16_t clampl, int16_t clamph);

       /*
        * Reporting / telemetry feedback functions, they read live data or
        * return settings
        */
        int16_t getError() { return _error; } // value sent to PWM 
        int16_t getSetpoint() { return _setpoint; } // setpoint after clamping

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
         
         uint8_t _invert; /* Invert ADC conversion sign */
         int16_t _setpoint;
         int16_t _error;

         int16_t _clampl; /* valid range [-512,   0]; */
         int16_t _clamph; /* valid range [   0, 511]; */

       /*
        * Drive the PWM units based on the calculated error value
        */
        void drive(int16_t error);
};


#endif

