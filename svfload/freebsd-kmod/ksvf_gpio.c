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

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/ioccom.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/gpio.h>
#include "ksvf.h"
#include "gpio_if.h"


static device_t pdev = NULL;
static struct cdev* ctl_dev = NULL;
static d_ioctl_t ksvf_ioctl;

static int PIN_SET_IO_DUP = 2;
static int PIN_GET_IO_DUP = 2;

// These are settable via sysctl. They should be set once at boot and then
// left alone!
static int tms_pin = 23;
static int tck_pin = 24;
static int tdi_pin = 22;
static int tdo_pin = 17;

static struct sysctl_ctx_list clist;
static struct sysctl_oid *poid;

static struct cdevsw ksvf_cdevsw = {
    .d_version  = D_VERSION,
    .d_ioctl    = ksvf_ioctl,
    .d_name     = "ksvf",
};

// Bit-Banging the I/O pins can actually go too fast for some
// devices to program reliably. Duplicate every I/O operation
// IO_DUP times to slow down if needed.

static int
pin_set(device_t pdev, int pin, int value)
{
    int i, rc;
    for(i = 0; i < PIN_SET_IO_DUP; i++) {
        rc = GPIO_PIN_SET(pdev, pin, value);
        if(rc) {
            break;
        }
    }
    return rc;
}

static int
pin_get(device_t pdev, int pin, int *value)
{
    int i, rc;
    for(i = 0; i < PIN_GET_IO_DUP; i++) {
        rc = GPIO_PIN_GET(pdev, pin, value);
        if(rc) {
            break;
        }
    }
    return rc;
}

static int 
ksvf_ioctl(struct cdev *cdev, u_long cmd, caddr_t arg, int fflag, 
    struct thread *td)
{
    int n;
    int res = ENOTTY;
    uint32_t line_tdo;
    struct ksvf_req req;
    switch (cmd) {
        case KSVF_INIT:

            bcopy(arg, &req, sizeof(req));
            // Make TDI, TMS, TCK outputs, TDO input
            res = GPIO_PIN_SETFLAGS(pdev, tck_pin, GPIO_PIN_OUTPUT);
            if(res)
                break;

            res = GPIO_PIN_SETFLAGS(pdev, tms_pin, GPIO_PIN_OUTPUT);
            if(res)
                break;

            res = GPIO_PIN_SETFLAGS(pdev, tdi_pin, GPIO_PIN_OUTPUT);
            if(res)
                break;

            res = GPIO_PIN_SETFLAGS(pdev, tdo_pin, GPIO_PIN_INPUT);
            if(res)
                break;

            res = GPIO_PIN_SET(pdev, tck_pin, 1);
            if(res)
                break;

            break;

        case KSVF_FINI:

            bcopy(arg, &req, sizeof(req));
            // Make TDI, TMS, TCK, TDI inputs
            res = GPIO_PIN_SETFLAGS(pdev, tck_pin, GPIO_PIN_INPUT);
            if(res)
                break;

            res = GPIO_PIN_SETFLAGS(pdev, tms_pin, GPIO_PIN_INPUT);
            if(res)
                break;

            res = GPIO_PIN_SETFLAGS(pdev, tdi_pin, GPIO_PIN_INPUT);
            if(res)
                break;

            res = GPIO_PIN_SETFLAGS(pdev, tdo_pin, GPIO_PIN_INPUT);
            break;

        case KSVF_UDELAY:

            bcopy(arg, &req, sizeof(req));
            // Set TMS if needed, then toggle TCK for the
            // specified count. The actual number of TCK cycles will
            // be written back to tck_cnt and passed back to the
            // caller.
            res = 0;
            if(req.tck_cnt <= 0) {
                req.tck_cnt = 0;
                bcopy(&req, arg, sizeof(req));
                break;
            }

            res = pin_set(pdev, tms_pin, req.tms_val);
            if(res) {
                req.tck_cnt = 0;
                bcopy(&req, arg, sizeof(req));
                break;
            }

            for(n = 0; n < req.tck_cnt; n++) {
                res = pin_set(pdev, tck_pin, 0);
                if(res)
                    break;

                res = pin_set(pdev, tck_pin, 1);
                if(res)
                    break;
            }

            req.tck_cnt = n;
            bcopy(&req, arg, sizeof(req));
            break;

        case KSVF_PULSE:
            bcopy(arg, &req, sizeof(req));
            // This is the most complicated operation
            res = 0;
            do {
                res = pin_set(pdev, tms_pin, req.tms_val);
                if(res)
                    break;

                if(req.tdi_val >= 0) {
                    res = pin_set(pdev, tdi_pin, req.tdi_val);
                    if(res)
                        break;
                }

                res = pin_set(pdev, tck_pin, 0);
                if(res)
                    break;

                res = pin_set(pdev, tck_pin, 1);
                if(res)
                    break;

                res = pin_get(pdev, tdo_pin, &line_tdo);
                if(res)
                    break;

                req.tdo_val = line_tdo;

            } while(0);
            bcopy(&req, arg, sizeof(req));
            break;
        default:
            break;
    }
    return res;
}

static int
ksvf_sysctl_init()
{
    sysctl_ctx_init(&clist);
    poid = SYSCTL_ADD_NODE(&clist,
            SYSCTL_STATIC_CHILDREN(_hw), OID_AUTO,
            "ksvf_gpio", CTLFLAG_RW, 0, "KSVF GPIO Driver Conf");

    if(poid == NULL) {
        uprintf("SYSCTL_ADD_NODE failed\n");
        return EINVAL;
    }

    poid = SYSCTL_ADD_NODE(&clist,
            SYSCTL_CHILDREN(poid), OID_AUTO,
            "0", CTLFLAG_RW, 0, "KSVF GPIO JTAG Controller 0");

    if(poid == NULL) {
        uprintf("SYSCTL_ADD_NODE failed\n");
        return sysctl_ctx_free(&clist) ? ENOTEMPTY : EINVAL;
    }

    SYSCTL_ADD_INT(&clist, SYSCTL_CHILDREN(poid), OID_AUTO,
        "pin_wr_io_dup", CTLFLAG_RW, &PIN_SET_IO_DUP, 0, "Set GPIO Duplicate");

    SYSCTL_ADD_INT(&clist, SYSCTL_CHILDREN(poid), OID_AUTO,
        "pin_rd_io_dup", CTLFLAG_RW, &PIN_GET_IO_DUP, 0, "Get GPIO Duplicate");

    SYSCTL_ADD_INT(&clist, SYSCTL_CHILDREN(poid), OID_AUTO,
        "tck_pin", CTLFLAG_RW, &tck_pin, 0, "JTAG Test Clock Pin");

    SYSCTL_ADD_INT(&clist, SYSCTL_CHILDREN(poid), OID_AUTO,
        "tms_pin", CTLFLAG_RW, &tms_pin, 0, "JTAG Test Mode Select Pin");

    SYSCTL_ADD_INT(&clist, SYSCTL_CHILDREN(poid), OID_AUTO,
        "tdi_pin", CTLFLAG_RW, &tdi_pin, 0, "JTAG Test Data Input Pin");

    SYSCTL_ADD_INT(&clist, SYSCTL_CHILDREN(poid), OID_AUTO,
        "tdo_pin", CTLFLAG_RW, &tdo_pin, 0, "JTAG Test Data Output Pin");

    return 0;
}

static int 
ksvf_modevent(struct module* module, int event, void* arg)
{
    int e = 0;

    switch(event) {
        case MOD_LOAD:
            pdev = devclass_get_device(devclass_find("gpio"), 0);
            if(!pdev) {
                printf("KSVF GPIO JTAG driver cannot locate parent device - gpio0\n");
            } else {
                e = ksvf_sysctl_init();
                if(e) {
                    break;
                }
                ctl_dev = make_dev(&ksvf_cdevsw, 0,
                    UID_ROOT, GID_WHEEL, 0600, "ksvf%d", 0);
                if(!ctl_dev) {
                    e = ENXIO;
                    if(sysctl_ctx_free(&clist)) {
                        e = ENOTEMPTY;
                    }
                } else {
                    device_printf(pdev, "KSVF GPIO JTAG driver attached - /dev/ksvf0\n");
                }
            }
            break;
        case MOD_UNLOAD:
            printf("KSVF GPIO JTAG driver unloading\n");
            if(ctl_dev) {
                destroy_dev(ctl_dev);
                ctl_dev = NULL;
                pdev = NULL;
                if(sysctl_ctx_free(&clist)) {
                    e = ENOTEMPTY;
                }
            }
            break;
        default:
            e = EOPNOTSUPP;
    }
    return e;
}

static moduledata_t ksvf_conf = {
    "ksvf",
    ksvf_modevent,
    NULL
};

DECLARE_MODULE(ksvf, ksvf_conf, SI_SUB_PSEUDO, SI_ORDER_ANY);

