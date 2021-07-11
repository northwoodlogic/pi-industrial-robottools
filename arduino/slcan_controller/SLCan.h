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

#ifndef SLCAN_H
#define SLCAN_H

#include "Arduino.h"

#define SLC_MTU (sizeof("T1111222281122334455667788EA5F\r")+1)

#define ARRAY_LEN(_x) (sizeof(_x) / sizeof(_x[0]))
typedef void (*CanRxHandler) (uint8_t *data, uint8_t len);

struct CanRxTab {
  char CanID[4]; // ASCII String representation + NULL
  CanRxHandler cb;
};


class SLCan {
    public:
        SLCan();
        void begin(HardwareSerial *s, const struct CanRxTab *tab, uint8_t len);
        void input();
        /*
         * Send out a CAN frame.
         * id: 3 char string indicating the message ID
         * buf: pointer to send buffer, or NULL
         * len: buffer length, valid values are 0-8
         * Return: non-zero on error
         *         1 --> not enough serial buffer space
         *         1 --> invalid parameters.
         */
        bool send(const char *id, uint8_t *buf, uint8_t len);
        bool send8(const char *id, uint8_t val);
        bool send16(const char *id, uint16_t val);
        bool send32(const char *id, uint32_t val);

    private:
        void handle();
        HardwareSerial *_s;
        uint8_t _rptr;
        char _rxbuf[SLC_MTU];
        const CanRxTab *_tab;
        uint8_t _tab_len;

};

#endif
