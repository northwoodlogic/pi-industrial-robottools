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

#include "Arduino.h"
#include "IBT2Motor.h"

IBT2Motor::IBT2Motor(
        uint8_t enl, uint8_t enr,
        uint8_t pwml, uint8_t pwmr,
        uint8_t invert)
{
    _enl = enl;
    _enr = enr;
    _pwml = pwml;
    _pwmr = pwmr;
    _invert = invert;

    _error = 0;
    _setpoint = 0;
    _clampl = 0;
    _clamph = 0;

    pinMode(_pwml, OUTPUT);
    pinMode(_pwmr, OUTPUT);
    pinMode(_enl,  OUTPUT);
    pinMode(_enr,  OUTPUT);
    disable();
}


void
IBT2Motor::enable()
{
    digitalWrite(_enl, HIGH);
    digitalWrite(_enr, HIGH);
}

void
IBT2Motor::disable()
{
    digitalWrite(_enl, LOW);
    digitalWrite(_enr, LOW);
    //_setpoint = 0;
}

void
IBT2Motor::clamp(int16_t clampl, int16_t clamph)
{
    if (clampl < -512)
        clampl = -512;

    if (clampl > 0)
        clampl = 0;

    if (clamph > 511)
        clamph = 511;

    if (clamph < 0)
        clamph = 0;

    _clampl = clampl;
    _clamph = clamph;
}

void
IBT2Motor::update(int16_t setpoint)
{
    if (setpoint < -512)
        setpoint = -512;

    if (setpoint > 511)
        setpoint = 511;

    _setpoint = setpoint;
}


void
IBT2Motor::tick()
{
    _error = _setpoint;
    if (_invert)
        _error *= -1;

    if (_error < _clampl)
        _error = _clampl;

    if (_error > _clamph)
        _error = _clamph;

    drive(_error / 2);
}

void
IBT2Motor::drive(int16_t error)
{
    int16_t pwm = 0;
  
    if (error < 0) { // CCW
        pwm = -(error);

        if (pwm > 255) {
            pwm = 255;
        }
    
        analogWrite(_pwmr, 0);
        analogWrite(_pwml, pwm);

    } else { // CW
        pwm =  (error);

        if (pwm > 255) {
            pwm = 255;
        }
    
        analogWrite(_pwml, 0);
        analogWrite(_pwmr, pwm);
    }

}

