/*-
 * Copyright (c) 2015,2019 Dave Rush <northwoodogic@gmail.com>
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
__FBSDID("$FreeBSD$");

#include "opt_platform.h"

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/bus.h>
#include <sys/gpio.h>
#include <sys/conf.h>
#include <sys/ioccom.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>

#include <dev/gpio/gpiobusvar.h>

#include "ksvf.h"
#include "gpiobus_if.h"

struct ksvfgpio_softc {
	device_t	dev;
	struct cdev*	ctl;

	gpio_pin_t	tdo;
	gpio_pin_t	tdi;
	gpio_pin_t	tms;
	gpio_pin_t	tck;
};

static d_ioctl_t ksvf_ioctl;

static int ksvfgpio_probe(device_t);
static int ksvfgpio_attach(device_t);
static int ksvfgpio_detach(device_t);

static int PIN_SET_IO_DUP = 2;
static int PIN_GET_IO_DUP = 2;

static struct cdevsw ksvf_cdevsw = {
    .d_version  = D_VERSION,
    .d_ioctl    = ksvf_ioctl,
    .d_name     = "ksvf",
};

#ifndef FDT
#error This module requires FDT support
#endif

static int
ksvfgpio_probe(device_t dev)
{
	if (!ofw_bus_is_compatible(dev, "ksvfgpio"))
		return (ENXIO);

	device_set_desc(dev, "GPIO KSVF");

	return BUS_PROBE_DEFAULT;
}

static int
ksvfgpio_attach(device_t dev)
{
	int err;
	phandle_t node;

	struct ksvfgpio_softc *sc = device_get_softc(dev);
	sc->dev = dev;

	node = ofw_bus_get_node(sc->dev);

	if ((err = gpio_pin_get_by_ofw_property(sc->dev, node, "tdo-gpios", &sc->tdo)) != 0) {
		device_printf(sc->dev, "missing tdo-gpios property\n");
		return err;
	}

	if ((err = gpio_pin_get_by_ofw_property(sc->dev, node, "tdi-gpios", &sc->tdi)) != 0) {
		device_printf(sc->dev, "missing tdi-gpios property\n");
		return err;
	}

	if ((err = gpio_pin_get_by_ofw_property(sc->dev, node, "tms-gpios", &sc->tms)) != 0) {
		device_printf(sc->dev, "missing tms-gpios property\n");
		return err;
	}

	if ((err = gpio_pin_get_by_ofw_property(sc->dev, node, "tck-gpios", &sc->tck)) != 0) {
		device_printf(sc->dev, "missing tck-gpios property\n");
		return err;
	}

	/*
	 * TODO: grab a string from the FDT and use that in the device name instead of arbitrary device
	 * unit number. This would make boards having > 1 SVF player attached easier to find the right
	 * device node
	 */
	sc->ctl = make_dev(&ksvf_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600, "ksvf%d", device_get_unit(sc->dev));
	if (!sc->ctl) {
		device_printf(sc->dev, "cannot create device node\n");
		return ENXIO;
	}

	/* need this during ioctl calls */
	sc->ctl->si_drv1 = sc;

	device_printf(dev, "    TDO: %s:%d flags:%d\n",
		device_get_nameunit(GPIO_GET_BUS(sc->tdo->dev)), sc->tdo->pin, sc->tdo->flags);
	device_printf(dev, "    TDI: %s:%d flags:%d\n",
		device_get_nameunit(GPIO_GET_BUS(sc->tdi->dev)), sc->tdi->pin, sc->tdi->flags);
	device_printf(dev, "    TMS: %s:%d flags:%d\n",
		device_get_nameunit(GPIO_GET_BUS(sc->tms->dev)), sc->tms->pin, sc->tms->flags);
	device_printf(dev, "    TCK: %s:%d flags:%d\n",
		device_get_nameunit(GPIO_GET_BUS(sc->tck->dev)), sc->tck->pin, sc->tck->flags);

	return (0);
}

static int
ksvfgpio_detach(device_t dev)
{
	struct ksvfgpio_softc *sc = device_get_softc(dev);

	/* remove device and release pins so module can be reloaded */
	destroy_dev(sc->ctl);

	if (sc->tdo != NULL)
		gpio_pin_release(sc->tdo);
	if (sc->tdi != NULL)
		gpio_pin_release(sc->tdi);
	if (sc->tms != NULL)
		gpio_pin_release(sc->tms);
	if (sc->tck != NULL)
		gpio_pin_release(sc->tck);

	return (0);
}

// Bit-Banging the I/O pins can actually go too fast for some
// devices to program reliably. Duplicate every I/O operation
// IO_DUP times to slow down if needed.

static int
pin_set(gpio_pin_t pin, int value)
{
    int i, rc;
    bool v = value ? 1 : 0;
    for(i = 0; i < PIN_SET_IO_DUP; i++) {
        rc = gpio_pin_set_active(pin, v);
        if(rc) {
            break;
        }
    }
    return rc;
}

static int
pin_get(gpio_pin_t pin, int *value)
{
    int i, rc;
    bool v = 0;
    for(i = 0; i < PIN_GET_IO_DUP; i++) {
        rc = gpio_pin_is_active(pin, &v);
        if(rc) {
            break;
        }
    }
    *value = v ? 1 : 0;
    return rc;
}

static int 
ksvf_ioctl(struct cdev *cdev, u_long cmd, caddr_t arg, int fflag, 
    struct thread *td)
{
    int n;
    int res = ENOTTY;
    uint32_t line_tdo;
    struct ksvfgpio_softc *sc = (struct ksvfgpio_softc*)cdev->si_drv1;
    struct ksvf_req req;
    switch (cmd) {
        case KSVF_INIT:

            bcopy(arg, &req, sizeof(req));
            // Make TDI, TMS, TCK outputs, TDO input
            res = gpio_pin_setflags(sc->tck, GPIO_PIN_OUTPUT);
            if(res)
                break;

            res = gpio_pin_setflags(sc->tms, GPIO_PIN_OUTPUT);
            if(res)
                break;

            res = gpio_pin_setflags(sc->tdi, GPIO_PIN_OUTPUT);
            if(res)
                break;

            res = gpio_pin_setflags(sc->tdo, GPIO_PIN_INPUT);
            if(res)
                break;

            res = gpio_pin_set_active(sc->tck, 1);
            if(res)
                break;

            break;

        case KSVF_FINI:

            bcopy(arg, &req, sizeof(req));
            // Make TDI, TMS, TCK, TDI inputs
            res = gpio_pin_setflags(sc->tck, GPIO_PIN_INPUT);
            if(res)
                break;

            res = gpio_pin_setflags(sc->tms, GPIO_PIN_INPUT);
            if(res)
                break;

            res = gpio_pin_setflags(sc->tdi, GPIO_PIN_INPUT);
            if(res)
                break;

            res = gpio_pin_setflags(sc->tdo, GPIO_PIN_INPUT);
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

            res = pin_set(sc->tms, req.tms_val);
            if(res) {
                req.tck_cnt = 0;
                bcopy(&req, arg, sizeof(req));
                break;
            }

            for(n = 0; n < req.tck_cnt; n++) {
                res = pin_set(sc->tck, 0);
                if(res)
                    break;

                res = pin_set(sc->tck, 1);
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
                res = pin_set(sc->tms, req.tms_val);
                if(res)
                    break;

                if(req.tdi_val >= 0) {
                    res = pin_set(sc->tdi, req.tdi_val);
                    if(res)
                        break;
                }

                res = pin_set(sc->tck, 0);
                if(res)
                    break;

                res = pin_set(sc->tck, 1);
                if(res)
                    break;

                res = pin_get(sc->tdo, &line_tdo);
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


static devclass_t ksvfgpio_devclass;

static device_method_t ksvfgpio_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ksvfgpio_probe),
	DEVMETHOD(device_attach,	ksvfgpio_attach),
	DEVMETHOD(device_detach,	ksvfgpio_detach),

	DEVMETHOD_END
};

static driver_t ksvfgpio_driver = {
	"ksvfgpio",
	ksvfgpio_methods,
	sizeof(struct ksvfgpio_softc),
};

DRIVER_MODULE(ksvfgpio, ofwbus, ksvfgpio_driver, ksvfgpio_devclass, 0, 0);
DRIVER_MODULE(ksvfgpio, simplebus, ksvfgpio_driver, ksvfgpio_devclass, 0, 0);
MODULE_DEPEND(ksvfgpio, gpiobus, 1, 1, 1);
