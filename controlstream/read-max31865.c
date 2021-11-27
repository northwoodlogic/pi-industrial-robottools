#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

/*
 * This is a test program, to be used for figuring out how to use the
 * MAX31865 RTD part.
 *
 * The test board is a knock off of an Adafruit model with a very bad
 * reference resistor. It appears as if this board in question uses a 5%
 * tolerance resistor value. That's far enough off that it's not usable unless
 * the probe is calibrated in an ice bath and accounted for.
 *
 * See datasheet for details:
 * https://datasheets.maximintegrated.com/en/ds/MAX31865.pdf
 */
#define REF_VAL 430.0

/*
 * https://github.com/drhaney/pt100rtd
 * http://www.mosaic-industries.com/embedded-systems/microcontroller-projects/temperature-measurement/platinum-rtd-sensors/resistance-calibration-table
 *
 * Rational polynomial fraction approximation taken from Mosaic Industries.com
 * page on "RTD calibration." Accurate, probably beyond the ITS-90 spec
 */
float celsius_rationalpolynomial (double R_ohms)
{
    double num, denom, T ;

    double c0= -245.19 ;
    double c1 = 2.5293 ;
    double c2 = -0.066046 ;
    double c3 = 4.0422E-3 ;
    double c4 = -2.0697E-6 ;
    double c5 = -0.025422 ;
    double c6 = 1.6883E-3 ;
    double c7 = -1.3601E-6 ;

    num = R_ohms * (c1 + R_ohms * (c2 + R_ohms * (c3 + R_ohms * c4))) ;
    denom = 1.0 + R_ohms * (c5 + R_ohms * (c6 + R_ohms * c7)) ;
    T = c0 + (num / denom) ;

    return(T );
}

int
spi_read(int fd, uint8_t cmd, uint8_t *rsp, int len)
{

    struct spi_ioc_transfer tr[] = {
	    {
            .tx_buf = (uintptr_t)&cmd,
            .rx_buf = (uintptr_t)NULL,
            .len = 1,
        },
	    {
            .tx_buf = (uintptr_t)NULL,
            .rx_buf = (uintptr_t)rsp,
            .len = len,
        },
    };

    // Do the transfer here
    return ioctl(fd, SPI_IOC_MESSAGE(2), tr) < 1 ? -1 : 0;
}

int
spi_write(int fd, uint8_t cmd, uint8_t *data, int len)
{
    struct spi_ioc_transfer tr[] = {
	    {
            .tx_buf = (uintptr_t)&cmd,
            .rx_buf = (uintptr_t)NULL,
            .len = 1,
        },
	    {
            .tx_buf = (uintptr_t)data,
            .rx_buf = (uintptr_t)NULL,
            .len = len,
        },
    };

    // Do the transfer here
    return ioctl(fd, SPI_IOC_MESSAGE(2), tr) < 1 ? -1 : 0;

}

int spi_xfer(int fd, uint8_t *tx, uint8_t *rx, int len)
{
    struct spi_ioc_transfer tr = {
            .tx_buf = (uintptr_t)tx,
            .rx_buf = (uintptr_t)rx,
            .len = len,
        };

    return ioctl(fd, SPI_IOC_MESSAGE(1), &tr) < 1 ? -1 : 0;

}

int
spi_open(const char *device)
{
    int ret = 0;
    uint32_t mode = 3;
    uint8_t bits = 8;
    uint32_t speed = 100000;
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
    fprintf(stderr, "Opened SPI device: fd=%d\n", fd);
    fprintf(stderr, "spi mode: 0x%x\n", mode);
    fprintf(stderr, "bits per word: %d\n", bits);
    fprintf(stderr, "max speed: %d Hz (%d KHz)\n", speed, speed/1000);
    return fd;
}


int
main(int argc, char *argv[])
{
	int fd = spi_open("/dev/spidev0.0");
	if (fd < 0)
		return 1;

	uint8_t cfg = 0;
	spi_read(fd, 0x00, &cfg, sizeof(cfg));
	printf("Config bits: 0x%02X\n", (uint32_t)cfg);
	cfg = 0xC0;
	spi_write(fd, 0x80, &cfg, sizeof(cfg));
	printf("wrote configuration: 0x%02X\n", (uint32_t)cfg);
	spi_read(fd, 0x00, &cfg, sizeof(cfg));
	printf("Config bits: 0x%02X\n", (uint32_t)cfg);

	spi_read(fd, 0x03, &cfg, sizeof(cfg));
	printf("HFT bits: 0x%02X\n", (uint32_t)cfg);

	spi_read(fd, 0x04, &cfg, sizeof(cfg));
	printf("HFT bits: 0x%02X\n", (uint32_t)cfg);

	while (1) {
		sleep(1);
		uint8_t rx[2] = { 0 };
		spi_read(fd, 0x01, rx, sizeof(rx));
		uint32_t val = ((uint32_t)rx[0]) << 8 | (uint32_t)rx[1];

		if (val & 1) {
			printf("Conversion fault!\n");
			continue;
		}

		// that 6.5 is trying to account for error in the reference
		// resistance.

		val = val >> 1; // the lowest bit is a fault bit
		double rrtd = ((double)val * (REF_VAL + 6.5)) / 32768.0;
		double degc = celsius_rationalpolynomial(rrtd);
		double degf = (degc * (9.0 / 5.0)) + 32.0;
		printf("Conversion Result: %u Rrtd=%f, degC=%f, degF=%f\n",
			val, rrtd, degc, degf);
	}
	return 0;
}

