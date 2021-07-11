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
#include "SLCan.h"

static uint8_t
hex_to_dec(char hex)
{
    if (hex >= '0' && hex <= '9')
        return (uint8_t)(hex - 48);

    if (hex >= 'A' && hex <= 'F')
        return (uint8_t)(hex - 55);

    if (hex >= 'a' && hex <= 'f')
        return (uint8_t)(hex - 87);

    return 0;
}

SLCan::SLCan() {

}

void SLCan::begin(HardwareSerial *s, const struct CanRxTab *tab, uint8_t len) {
    _s = s;
    _rptr = 0;
    _tab = tab;
    _tab_len = len;
}

// Minimum CAN length message for this implementation is 't1112aabb\r' <-- 10
// bytes
void
SLCan::handle()
{
    int n;
    uint8_t len;
    uint8_t data[8];

    if (_rptr < 6) {
        return;
    }

    if (_rxbuf[0] != 't') {
        return;
    }

    for (n = 0; n < _tab_len; n++) {
        const CanRxTab* item = &(_tab[n]);
        if (memcmp(&(_rxbuf[1]), item->CanID, 3) != 0)
            continue;

        len = hex_to_dec(_rxbuf[4]);

	// Corrupt packet! Length * 2 (due to hex encoding using two bytes per
	// byte) must equal the amount of data bytes received.
        if ((len << 1) != (_rptr - 6)) {
            break;
        }
        
        if (len >= 1)
            data[0] = (hex_to_dec(_rxbuf[5]) << 4) | hex_to_dec(_rxbuf[6]);

        if (len >= 2)
            data[1] = (hex_to_dec(_rxbuf[7]) << 4) | hex_to_dec(_rxbuf[8]);

        if (len >= 2)
            data[2] = (hex_to_dec(_rxbuf[9]) << 4) | hex_to_dec(_rxbuf[10]);
        
        if (len >= 3)
            data[3] = (hex_to_dec(_rxbuf[11]) << 4) | hex_to_dec(_rxbuf[12]);
        
        if (len >= 4)
            data[4] = (hex_to_dec(_rxbuf[13]) << 4) | hex_to_dec(_rxbuf[14]);
        
        if (len >= 5)
            data[5] = (hex_to_dec(_rxbuf[15]) << 4) | hex_to_dec(_rxbuf[16]);

        if (len >= 6)
            data[6] = (hex_to_dec(_rxbuf[17]) << 4) | hex_to_dec(_rxbuf[18]);
        
        if (len >= 7)
            data[7] = (hex_to_dec(_rxbuf[19]) << 4) | hex_to_dec(_rxbuf[20]);
        
        if (len >= 8)
            data[8] = (hex_to_dec(_rxbuf[21]) << 4) | hex_to_dec(_rxbuf[22]);

        // I guess this is it, call the function pointer.
        item->cb(data, len);
        break;
    }
}

bool
SLCan::send(const char *id, uint8_t *buf, uint8_t len)
{
    uint8_t n;
    int ntx;
    char hex[3];
    if (strlen(id) != 3)
        return 1;

    if (len > 8)
        return 1;

    // 'txxxn3333\r'
    ntx = 6 + (len << 1);
    if (ntx > _s->availableForWrite())
        return 2;

    _s->write('t');
    _s->write(id, 3);

    // This can be optimized to not use sprintf
    snprintf(hex, sizeof(hex), "%x", len);
    _s->write(hex, 1);

    for (n = 0; n < len; n++) {
        snprintf(hex, sizeof(hex), "%02x", buf[n]);
        _s->write(hex, 2);
    }
    _s->write('\r');
    
    return 0;
}

/* helper functions for sending integer values */
bool
SLCan::send8(const char *id, uint8_t val)
{
    return send(id, &val, (uint8_t)1);
}

/* Send big endian */
bool
SLCan::send16(const char *id, uint16_t val)
{
    uint8_t buf[2];
    buf[0] = (uint8_t)(val >> 8);
    buf[1] = (uint8_t)(val);
    return send(id, buf, (uint8_t)sizeof(buf));
}

/* Send big endian */
bool
SLCan::send32(const char *id, uint32_t val)
{
    uint8_t buf[4];
    buf[0] = (uint8_t)(val >> 24);
    buf[1] = (uint8_t)(val >> 16);
    buf[2] = (uint8_t)(val >> 8);
    buf[3] = (uint8_t)(val >> 0);
    return send(id, buf, (uint8_t)sizeof(buf));
}

void
SLCan::input()
{
    char bi;
    while(_s->available() > 0) {

        // no overflow. this message is probably junk so start over
        if (_rptr >= SLC_MTU) {
            _rptr = 0;
        }
        bi = (char)_s->read();
        _rxbuf[_rptr++] = bi;
        if (bi == '\r') {
            // got message termination, parse the message
            handle();
            _rptr = 0;
            memset(_rxbuf, 0, sizeof(_rxbuf));
        }
    }
}
