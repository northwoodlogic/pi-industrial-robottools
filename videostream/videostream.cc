/*
 *  V4L2 video capture example
 *
 *  This program can be used and distributed without restrictions.
 *
Recommended resolution & aspect ratios

With this camera, it looks like 360p is the lowest resolution it supports

For the default 16:9 aspect ratio, encode at these resolutions:

    2160p: 3840x2160
    1440p: 2560x1440
    1080p: 1920x1080
    720p: 1280x720
    480p: 854x480
    360p: 640x360
    240p: 426x240
 *
 *
 *
 *      This program is provided with the V4L2 API
 * see http://linuxtv.org/docs.php for more information
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

#include <getopt.h>             /* getopt_long() */

#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <linux/videodev2.h>

/* The libuvgrtp source is not prefixed with 'uvgrtp' */
#include <lib.hh>
//#include <uvgrtp/lib.hh>

static void
usage(FILE *fp, int argc, char **argv)
{
        fprintf(fp,
                 "Usage: %s [options]\n\n"
                 "Options:\n"
                 "-d | --video          Video device name [/dev/video0]\n"
                 "-b | --canbus         CAN Bus Interface\n"
                 "-r | --remote         remote system address\n"
                 "-p | --port           UDP port number\n"
                 "",
                 argv[0]);
}

static const char short_options[] = "d:b:r:p:h";

static const struct option
long_options[] = {
        { "video",  required_argument, NULL, 'd' },
        { "canbus", required_argument, NULL, 'b' },
        { "remote", required_argument, NULL, 'r' },
        { "port",   required_argument, NULL, 'p' },
        { "help",   no_argument,       NULL, 'h' },
        { 0, 0, 0, 0 }
};

int camera_open(const char *dev);
int camera_capture(uint8_t **data, int *len);

int main(int argc, char **argv)
{
    int port = 8888;
    const char *remote = NULL;
    const char *dev_name = "/dev/video0";
    const char *can_name = "can0";

    while(1) {
        int idx;
        int c;

        c = getopt_long(argc, argv,
                        short_options, long_options, &idx);

        if (-1 == c)
                break;

        switch (c) {
        case 0: /* getopt_long() flag */
                break;

        case 'd':
                dev_name = optarg;
                break;

        case 'b':
                can_name = optarg;
                break;

        case 'h':
                usage(stdout, argc, argv);
                exit(EXIT_SUCCESS);

        case 'r':
            remote = optarg;
            break;

        case 'p':
            port = atoi(optarg);
            break;

        default:
                usage(stderr, argc, argv);
                exit(EXIT_FAILURE);
        }
    }

    if (!remote) {
        printf("I need a remote IP target\n");
        exit(1);
    }

    uvg_rtp::context ctx;
    uvg_rtp::session *sess = ctx.create_session(remote);
    uvg_rtp::media_stream *strm = sess->create_stream(port, port, RTP_FORMAT_GENERIC, RCE_FRAGMENT_GENERIC);
    strm->configure_ctx(RCC_PKT_MAX_DELAY, 200);

    camera_open(dev_name);
    while (1) {
        // capture frame...
        uint8_t *data = NULL;
        int data_len = 0;
        camera_capture(&data, &data_len);
        strm->push_frame(data, data_len, RTP_NO_FLAGS);
    }
    
        return 0;
}
