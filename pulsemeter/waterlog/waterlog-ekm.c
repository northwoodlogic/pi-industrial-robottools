/* SPDX-License-Identifier: [MIT] */

#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <byteswap.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>


int
spi_xfer(int fd, uint8_t *tx, uint8_t *rx, int len)
{
    struct spi_ioc_transfer tr = {
        .tx_buf = (uintptr_t)tx,
        .rx_buf = (uintptr_t)rx,
        .len = len,
    };

    return ioctl(fd, SPI_IOC_MESSAGE(1), &tr) < 1 ? -1 : 0;
}

/* fpga test wants mode 0,
 * clock polarity = 0, phase = 0
 */
int
spi_open(const char *device, uint32_t mode, uint32_t speed)
{
    int ret = 0;
    uint8_t bits = 8;
    int fd = open(device, O_RDWR);
    const char *msg = NULL;
    do {
        if(fd == -1) {
            msg = "Error opening SPI device";
            break;
        }

        ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);
        if (ret == -1) {
            msg = "can't set spi mode";
            break;
        }

        ret = ioctl(fd, SPI_IOC_RD_MODE, &mode);
        if (ret == -1) {
            msg = "can't get spi mode";
            break;
        }

        /*
         * bits per word
         */
        ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
        if (ret == -1) {
            msg = "can't set bits per word";
            break;
        }

        ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
        if(ret == -1) {
            msg = "can't get bits per word";
            break;
        }

        /*
         * max speed hz
         */
        ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
        if(ret == -1) {
            msg = "can't set max speed hz";
            break;
        }

        ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
        if(ret == -1) {
            msg = "can't get max speed hz";
            break;
        }

    } while(0);

    if(msg != NULL) {
        if(fd != -1) {
            close(fd);
        }
        fprintf(stderr, "%s\n", msg);
        return -1;
    }

    return fd;
}

void
show_help()
{
    printf("usage:\n"
        "  -h print help\n"
        "  -d spidev interface, defaults to /dev/spidev0.0 if omitted\n"
        "  -s SPI bus speed in Hz, 100000 if omitted\n"
        "  -f appended log file, /data/waterlog.csv if omitted\n"
    );
}

int
main(int argc, char *argv[])
{
        int rc, opt;
        const char *dev = "/dev/spidev0.0";
        const char *log = "/data/waterlog.csv";
        uint32_t speed = 100000;

        int i = 0;

        while ((opt = getopt(argc, argv, "hs:d:f:")) != -1) {
            switch (opt) {
                case 'h':
                    show_help();
                    return 0;
                case 'd':
                    dev = optarg;
                    break;
                case 'f':
                    log = optarg;
                    break;
                case 's':
                    speed = (uint32_t)strtoull(optarg, NULL, 0);
                    break;
                default:
                    printf("Unknown option: %c\n", (char)opt);
                    return 1;
            }
        }

        int fd = spi_open(dev, 0, speed);
        if (fd < 0) {
            printf("unable to open device: %s\n", dev);
            return 1;
        }

        FILE* lf = fopen(log, "a");
        if (lf == NULL) {
            printf("unable to open file: %s\n", log);
            return 1;
        }


#define SLEEP_TIME 10

        uint32_t last_count = 0;
        while (1) {
            sleep(SLEEP_TIME);

            uint32_t tx = 0xdeadbeef;
            uint32_t rx = 0;

            rc = spi_xfer(fd, (uint8_t*)&tx, (uint8_t*)&rx, 4);
            if (rc) {
                fprintf(stderr, "transfer error!\n");
                continue;
            }

            /*
             * This relies on the over-shift pass through debug feature to
             * verify a working SPI bus.
             */
            if ((rx & 0xffff0000) != 0xbeef0000) {
                fprintf(stderr, "SPI chain broken!\n");
                continue;
            }

            /*
             * The accumulator count is in the lower two bytes and requires
             * byte swapping.
             */
            uint32_t count = (((rx & 0x00ff) << 8) | ((rx & 0xff00) >> 8));
            time_t now = time(NULL);
            if (count) {
                /*
                 * These floating point constants depend on the meter specific
                 * pulse per gallon scaling factor. These constants are
                 * accurate for the SPWM-075-HD-NSF meter.
                 */
                double gal = (double)count * 0.01 * 7.48052;
                double gpm = gal * (60 / SLEEP_TIME); // this
                fprintf(lf, "%s, %lu, %u, %.3f gal, %.3f gpm\n",
                    strtok(ctime(&now), "\n"),
                    (unsigned long)now, count, gal, gpm);

                fflush(lf);
            }

            /*
             * Emit a zero record any time water was flowing then it stops.
             * This removes the need for post-processing when feeding the
             * output into interpolating line plots. This is needed because
             * data is only omitted when water is flowing.
             */
            if ((count == 0) && (last_count != 0)) {
                fprintf(lf, "%s, %lu, %u, %.3f gal, %.3f gpm\n",
                    strtok(ctime(&now), "\n"),
                    (unsigned long)now, 0, 0.0, 0.0);

                fflush(lf);
            }

            last_count = count;
#if 0
            if ((i++ % 500) == 0)
                fprintf(stderr, "%d mark...\n", i);
#endif
        }
        return rc;
}

