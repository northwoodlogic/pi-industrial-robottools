#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/timerfd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

/*
 * The gpio numbering looks a little weird, but the physical ribbin cable I/O
 * connector on the RPI adapter board is flipped over and mounted on the
 * bottom side of the board. The ribbon cable was flipped on one end so the
 * two ground pins on each of the ribbon cable are connected. This has a side
 * effect of swizzling I/O points 0-1, 2-3, 4-5, & 6-7 such that I/O point 0
 * is pin 1, I/O point 1 is pin 0, I/O point 2 is pin 3, I/O point 3 is pin 2,
 * etc...
 *
 * I have no idea why Linux enumerates the gpio expander chip starting @ 504.
 */

/* Valves are labeled 1-6, physically wired to I/O points 1-6 too. */
#define VALVE_1 "/sys/class/gpio/gpio504/value"
#define VALVE_2 "/sys/class/gpio/gpio507/value"
#define VALVE_3 "/sys/class/gpio/gpio506/value"
#define VALVE_4 "/sys/class/gpio/gpio509/value"
#define VALVE_5 "/sys/class/gpio/gpio508/value"
#define VALVE_6 "/sys/class/gpio/gpio511/value"

/* Opto22 rack I/O points 0,7 are unused */
#define INPUT_0 "/sys/class/gpio/gpio505/value"
#define INPUT_7 "/sys/class/gpio/gpio510/value"

const char *iop[] = {
    INPUT_0, VALVE_1, VALVE_2, VALVE_3, VALVE_4, VALVE_5, VALVE_6, INPUT_7
};

const char *ion[] = {
    "504", "505", "506", "507", "508", "509", "510", "511"
};

void
export_gpio()
{
    int i;
    struct stat sb;
    char path[255];
    for (i = 0; i < (sizeof(ion) / sizeof(ion[0])); i++) {
        FILE *fd = fopen("/sys/class/gpio/export", "w+");
        if (!fd) {
            fprintf(stderr, "unable to open sysfsgpio control file\n");
            continue;
        }
        fputs(ion[i], fd);
        fclose(fd);

        snprintf(path, sizeof(path), "/sys/class/gpio/gpio%s", ion[i]);
        if (stat(path, &sb)) {
            fprintf(stderr, "unable to stat gpio dir: %s\n", path);
        }
    }
}

int
read_input(int io)
{
    char val[2] = { '1' };
    int fd, n;
    if (io < 0 || io > 7)
        return -1;

    fd = open(iop[io], O_RDONLY);
    if (fd == -1)
        return -1;

    n = read(fd, val, sizeof(val));
    close(fd);

    if (n < 1)
        return -1;

    /* Inputs are active low, return '1' if input is asserted */
    return val[0] == '0' ? 1 : 0;
}

int
main(int argc, char *argv[])
{
    int n;
    int first = 1;
    uint64_t count;
    char timestr[128];
    uint8_t io_past = 0;
    int tfd = timerfd_create(CLOCK_MONOTONIC, 0);
    struct timespec   tp;
    struct itimerspec ts = {
        .it_interval = { .tv_sec = 1, .tv_nsec = 0 },
        .it_value    = { .tv_sec = 1, .tv_nsec = 0 }
    };

    FILE *logfile = fopen("/var/log/valvelog.csv", "a+");
    if (!logfile) {
        fprintf(stderr, "Unable to open log file, exiting\n");
        return 1;
    }

    export_gpio();

    timerfd_settime(tfd, 0, &ts, NULL);
    while (1) {
        uint8_t io_prst = 0;
        struct tm tm;

        n = read(tfd, &count, sizeof(count));
        if (n != sizeof(count))
            continue;

        n = clock_gettime(CLOCK_REALTIME, &tp);
        if (n != 0)
            continue;

        /* localtime is only for mental reference, the time_t is printed in UTC */
        localtime_r(&tp.tv_sec, &tm);
        asctime_r(&tm, timestr);
        timestr[strlen(timestr) - 1] = '\0'; // zap the newline

        for (n = 1; n <= 6; n++) {
            int val = read_input(n);
            if (val < 0) {
                fprintf(stderr, "error reading input: %d\n", n);
                continue;
            }

            if (val)
                io_prst |= (1 << n);

            /* skip edge detection on first loop iteration */
            if (first)
                continue;

            /* Falling edge, valve turned off */
            if (((io_past & (1 << n)) != 0) && ((io_prst & (1 << n)) == 0)) {
                fprintf(logfile, "%s,%u,valve%d,off\n", timestr, (unsigned int)tp.tv_sec, n);
                fflush(logfile);
            }

            /* Rising edge, valve turned on */
            if (((io_past & (1 << n)) == 0) && ((io_prst & (1 << n)) != 0)) {
                fprintf(logfile, "%s,%u,valve%d,on\n", timestr, (unsigned int)tp.tv_sec, n);
                fflush(logfile);
            }
        }

        /* System level on / off detection. Skip first loop iteration */
        if (first) {
            io_past = io_prst;
            first = 0;
            continue;
        }

        /* all valves turned off */
        if ((io_past != 0) && (io_prst == 0)) {
            fprintf(logfile, "%s,%u,system,off\n", timestr, (unsigned int)tp.tv_sec);
            fflush(logfile);
        }

        /* one or more valves turned on */
        if ((io_past == 0) && (io_prst != 0)) {
            fprintf(logfile, "%s,%u,system,on\n", timestr, (unsigned int)tp.tv_sec);
            fflush(logfile);
        }

        io_past = io_prst;
    }

    return 0;
}
