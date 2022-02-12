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
#include "ksvf.h"

#include <dev/ppbus/ppbconf.h>
#include "ppbus_if.h"
#include <dev/ppbus/ppbio.h>

static d_ioctl_t ksvf_ioctl;

static struct cdevsw ksvf_cdevsw = {
    .d_version  = D_VERSION,
    .d_ioctl    = ksvf_ioctl,
    .d_name     = "lpksvf",
};

struct lpksvf_ctx {
    uint8_t dval;

    int wr_iodup;
    int rd_iodup;

    int tms_dbit;
    int tck_dbit;
    int tdi_dbit;
    int oe_dbit;
    int ie_dbit;

    int tdo_sbit;
    int vtgt_sbit;

    struct mtx mtx;
    device_t ppbus;
    device_t ksvf_dev;
    struct cdev *ctl_dev;
};

static void
lpksvf_identify(driver_t *driver, device_t parent)
{
    device_t dev;
    dev = device_find_child(parent, "lpksvf", -1);
    if (!dev)
        BUS_ADD_CHILD(parent, 0, "lpksvf", -1);
}

static int
lpksvf_probe(device_t dev)
{
    device_set_desc(dev, "Parallel KSVF JTAG bit-banging interface");
    return (0);
}

static int
lpksvf_attach(device_t dev)
{
    struct lpksvf_ctx *sc;
    struct sysctl_ctx_list *sysctlctx;
    sc = device_get_softc(dev);

    sc->wr_iodup = 1;
    sc->rd_iodup = 1;

    // data register output bits
    sc->tms_dbit = 2;
    sc->tck_dbit = 1;
    sc->tdi_dbit = 0;
    sc->oe_dbit  = 5; // output enable, drive "0" 
    sc->ie_dbit  = 4; // input enable,  drive "1"

    // status register input bits
    sc->tdo_sbit  = 4;
    sc->vtgt_sbit = 3; // Target VCC sense

    sysctlctx = device_get_sysctl_ctx(dev);
    SYSCTL_ADD_INT(sysctlctx,
        SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
        "wr_iodup", CTLFLAG_RW, &sc->wr_iodup, 0,
        "Write I/O dup");

    SYSCTL_ADD_INT(sysctlctx,
        SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
        "rd_iodup", CTLFLAG_RW, &sc->rd_iodup, 0,
        "Read I/O dup");

    SYSCTL_ADD_INT(sysctlctx,
        SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
        "tck_dbit", CTLFLAG_RW, &sc->tck_dbit, 0,
        "JTAG TCK data reg bit");

    SYSCTL_ADD_INT(sysctlctx,
        SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
        "tms_dbit", CTLFLAG_RW, &sc->tms_dbit, 0,
        "JTAG TMS data reg bit");

    SYSCTL_ADD_INT(sysctlctx,
        SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
        "tdi_dbit", CTLFLAG_RW, &sc->tdi_dbit, 0,
        "JTAG TDI data reg bit");

    SYSCTL_ADD_INT(sysctlctx,
        SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
        "tdo_sbit", CTLFLAG_RW, &sc->tdo_sbit, 0,
        "JTAG TDI status reg bit");

    mtx_init(&sc->mtx, "lpksvf", NULL, MTX_DEF);
    sc->ksvf_dev = dev;
    sc->ppbus = device_get_parent(dev);
    sc->ctl_dev = make_dev(&ksvf_cdevsw, 0,
                    UID_ROOT, GID_WHEEL, 0600, "lpksvf%d",
                    device_get_unit(sc->ppbus));

    sc->ctl_dev->si_drv1 = sc;
#if 0
    if(!ctl_dev) {
        e = ENXIO;
    }
#endif
    return bus_generic_attach(dev);
}

static int
lpksvf_detach(device_t dev)
{
    struct lpksvf_ctx *sc;
    sc = device_get_softc(dev);

    if(sc->ctl_dev) {
        destroy_dev(sc->ctl_dev);
        sc->ctl_dev = NULL;
    }

    bus_generic_detach(dev);
    mtx_destroy(&sc->mtx);
    return (0);
}

// Bit-Banging the I/O pins can actually go too fast for some
// devices to program reliably. Duplicate every I/O operation
// IO_DUP times to slow down if needed.

// The "pin" value is a bit position in the data register
static int
pin_set(struct lpksvf_ctx *sc, int pin, int value)
{
    int i;
    if(value)
        sc->dval |=  (1 << pin);
    else
        sc->dval &= ~(1 << pin);

    for(i = 0; i < sc->wr_iodup; i++) {
        ppb_wdtr(sc->ppbus, (u_char)sc->dval);
    }
    return 0;
}

static int
pin_get(struct lpksvf_ctx *sc, int pin, int *value)
{
    int i;
    for(i = 0; i < sc->rd_iodup; i++) {
        *value = (ppb_rstr(sc->ppbus) & (1 << pin)) ? 1 : 0;
    }
    return 0;
}

static int 
ksvf_ioctl(struct cdev *cdev, u_long cmd, caddr_t arg, int fflag, 
    struct thread *td)
{
    int n;
    int res = ENOTTY;
    uint32_t line_tdo;
    struct ksvf_req req;
    struct lpksvf_ctx *sc = cdev->si_drv1;
    ppb_lock(sc->ppbus);

    if(ppb_request_bus(sc->ppbus, sc->ksvf_dev, PPB_DONTWAIT)) {
		device_printf(sc->ksvf_dev, "can't allocate ppbus\n");
        ppb_unlock(sc->ppbus);
        return EAGAIN;
    }

    switch (cmd) {
        case KSVF_INIT:

            bcopy(arg, &req, sizeof(req));
            sc->dval = 0;
            sc->dval |=  (1 << sc->ie_dbit);
            sc->dval &= ~(1 << sc->oe_dbit);
            pin_set(sc, sc->tck_dbit, 1);
            res = 0;
            break;

        case KSVF_FINI:

            bcopy(arg, &req, sizeof(req));
            res = 0;
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

            res = pin_set(sc, sc->tms_dbit, req.tms_val);
            if(res) {
                req.tck_cnt = 0;
                bcopy(&req, arg, sizeof(req));
                break;
            }

            for(n = 0; n < req.tck_cnt; n++) {
                res = pin_set(sc, sc->tck_dbit, 0);
                if(res)
                    break;

                res = pin_set(sc, sc->tck_dbit, 1);
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
                res = pin_set(sc, sc->tms_dbit, req.tms_val);
                if(res)
                    break;

                if(req.tdi_val >= 0) {
                    res = pin_set(sc, sc->tdi_dbit, req.tdi_val);
                    if(res)
                        break;
                }

                res = pin_set(sc, sc->tck_dbit, 0);
                if(res)
                    break;

                res = pin_set(sc, sc->tck_dbit, 1);
                if(res)
                    break;

                res = pin_get(sc, sc->tdo_sbit, &line_tdo);
                if(res)
                    break;

                req.tdo_val = line_tdo;

            } while(0);
            bcopy(&req, arg, sizeof(req));
            break;
        default:
            break;
    }

	ppb_release_bus(sc->ppbus, sc->ksvf_dev);
    ppb_unlock(sc->ppbus);
    return res;
}

static devclass_t lpksvf_devclass;
static device_method_t lpksvf_methods[] = {
    /* device interface */
    DEVMETHOD(device_identify,  lpksvf_identify),
    DEVMETHOD(device_probe,     lpksvf_probe),
    DEVMETHOD(device_attach,    lpksvf_attach),
    DEVMETHOD(device_detach,    lpksvf_detach),
    DEVMETHOD_END
};

static driver_t lpksvf_driver = {
    "lpksvf",
    lpksvf_methods,
    sizeof(struct lpksvf_ctx),
};

DRIVER_MODULE(lpksvf, ppbus, lpksvf_driver, lpksvf_devclass, 0, 0);
MODULE_DEPEND(lpksvf, ppbus, 1, 1, 1);
MODULE_VERSION(lpksvf, 1);

