/*-
 * Copyright (c) 2015 David Rush <northwoodlogic@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#ifndef K_SVF_H
#define K_SVF_H

#include <sys/ioccom.h>

struct ksvf_req {
    // Unused, spare data members
    uint8_t  unused_0;
    uint8_t  unused_1;
    uint8_t  unused_2;
    uint8_t  unused_3;

    // 
    int8_t   tms_val;
    int8_t   tdi_val;
    int8_t   tdo_val;

    int8_t   padding;
    // TCK pulse count for test mode run
    uint32_t tck_cnt;
};

#define KSVF_INIT       _IOW('S', 0, struct ksvf_req)
#define KSVF_FINI       _IOW('S', 1, struct ksvf_req)
#define KSVF_UDELAY     _IOWR('S', 2, struct ksvf_req)
#define KSVF_PULSE      _IOWR('S', 3, struct ksvf_req)

#endif
