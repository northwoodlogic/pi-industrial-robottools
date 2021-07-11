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

#ifndef LIMIT_SWITCH_H
#define LIMIT_SWITCH_H

class LimitSwitch {
    public:
        /*
         * Assign digital input pin to the limit switch. For safety reasons,
         * limit switches are normally closed devices and pull the input pin
         * to low logic level when the limit is not not engaged. The input pin
         * MUST support an internal pull-up to support a fail safe condition
         * if the cabling were to come unhooked.
         */
        LimitSwitch(uint8_t pin);

        /*
         * Check if limit switch has engaged Returns: non-zero if limit switch
         * has engaged
         */
        bool engaged();

        /*
         * Edge detect on engaged. This does not perform bounce filtering. The
         * expected use case is to poll this function on a timer based
         * interval suitable for any bounce / glitch filtering. Edge events
         * should NOT be used in safety critical code paths. Limit switches
         * are usually installed to prevent over-driving a mechanism. As long
         * as the switch is engaged drive should be disabled.
         *
         * Returns: non-zero on an activated edge event.
         */
        bool triggered();

    private:
         uint8_t _pin;
         uint8_t _last_state;
};

#endif

