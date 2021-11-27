/*
 * This code is a quick prototype used for capturing JPEG stills off of a web cam
 * and sending them frame by frame to a remote host.
 *
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

#define CLEAR(x) memset(&(x), 0, sizeof(x))

#ifndef V4L2_PIX_FMT_H264
#define V4L2_PIX_FMT_H264     v4l2_fourcc('H', '2', '6', '4') /* H264 with start codes */
#endif


struct buffer {
        void   *start;
        size_t  length;
};

static const char      *dev_name;
static int              fd = -1;
struct buffer          *buffers;
static unsigned int     n_buffers;
static int              out_buf;
static int              force_format = 1;
static int              frame_count = 50;
static int              frame_number = 0;

static void
errno_exit(const char *s)
{
    fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
    exit(EXIT_FAILURE);
}

static int
xioctl(int fh, int request, void *arg)
{
        int r;

        do {
                r = ioctl(fh, request, arg);
        } while (-1 == r && EINTR == errno);

        return r;
}

#if 0
static void
save_jpeg(const void *p, int size)
{
        frame_number++;
    if (frame_number > 100)
        frame_number = 0;

        char filename[15];


        sprintf(filename, "frame-%d.jpg", frame_number);
        FILE *fp=fopen(filename,"wb");

    int n = fwrite(p, 1, size, fp);
    if (n != size)
        printf("didn't write all data: %d\n", n);

    fclose(fp);

}
#endif

static int
read_frame(uint8_t **jpg, int *nbytes)
{
    unsigned int i;
    struct v4l2_buffer buf;

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf)) {
        switch (errno) {
        case EAGAIN:
            return 0;

        case EIO:
        default:
            errno_exit("VIDIOC_DQBUF");
        }
    }

    assert(buf.index < n_buffers);

    *jpg = (uint8_t *)(buffers[buf.index].start);
    *nbytes = buf.bytesused;

    //
    // This is what sets up for the next frame. Maybe this should be at the top of the loop, then
    // select on it, and read frame out. 
    //usleep(100000);
    usleep(50000);
    if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
        errno_exit("VIDIOC_QBUF");

    return 0;
}

int
camera_capture(uint8_t **data, int *len)
{
    unsigned int count;
    count = frame_count;

    fd_set fds;
    struct timeval tv;
    int r;

    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    /* Timeout. */
    tv.tv_sec = 10;
    tv.tv_usec = 0;

    r = select(fd + 1, &fds, NULL, NULL, &tv);

    if (-1 == r) {
//            if (EINTR == errno)
//                    continue;
        errno_exit("select");
    }

    if (0 == r) {
        fprintf(stderr, "select timeout\n");
        exit(EXIT_FAILURE);
    }

    if (read_frame(data, len)) {
        fprintf(stderr, "error reading frame\n");
    }

    return 0;
}

static void
stop_capturing(void)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == xioctl(fd, VIDIOC_STREAMOFF, &type))
            errno_exit("VIDIOC_STREAMOFF");
}

static void
start_capturing(void)
{
    unsigned int i;
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    for (i = 0; i < n_buffers; ++i) {
        struct v4l2_buffer buf;
        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
            errno_exit("VIDIOC_QBUF");
    }
    if (-1 == xioctl(fd, VIDIOC_STREAMON, &type))
        errno_exit("VIDIOC_STREAMON");

}

static void
uninit_device(void)
{
    unsigned int i;

    for (i = 0; i < n_buffers; ++i)
        if (-1 == munmap(buffers[i].start, buffers[i].length))
            errno_exit("munmap");

    free(buffers);
}

static void
init_mmap(void)
{
    struct v4l2_requestbuffers req;
    CLEAR(req);

    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) {
        if (EINVAL == errno) {
            fprintf(stderr, "%s does not support "
                     "memory mapping\n", dev_name);
            exit(EXIT_FAILURE);
        } else {
            errno_exit("VIDIOC_REQBUFS");
        }
    }

    if (req.count < 2) {
        fprintf(stderr, "Insufficient buffer memory on %s\n",
                 dev_name);
        exit(EXIT_FAILURE);
    }

    buffers = (buffer*)calloc(req.count, sizeof(*buffers));

    if (!buffers) {
        fprintf(stderr, "Out of memory\n");
        exit(EXIT_FAILURE);
    }

    for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
        struct v4l2_buffer buf;

        CLEAR(buf);

        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = n_buffers;

        if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf))
            errno_exit("VIDIOC_QUERYBUF");

        buffers[n_buffers].length = buf.length;
        buffers[n_buffers].start =
                mmap(NULL /* start anywhere */,
                      buf.length,
                      PROT_READ | PROT_WRITE /* required */,
                      MAP_SHARED /* recommended */,
                      fd, buf.m.offset);

        if (MAP_FAILED == buffers[n_buffers].start)
            errno_exit("mmap");
    }
}

static void
init_device(void)
{
    struct v4l2_capability cap;
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;
    struct v4l2_format fmt;
    unsigned int min;

    if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap)) {
            if (EINVAL == errno) {
                    fprintf(stderr, "%s is no V4L2 device\n",
                             dev_name);
                    exit(EXIT_FAILURE);
            } else {
                    errno_exit("VIDIOC_QUERYCAP");
            }
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
            fprintf(stderr, "%s is no video capture device\n",
                     dev_name);
            exit(EXIT_FAILURE);
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
            fprintf(stderr, "%s does not support streaming i/o\n",
                     dev_name);
            exit(EXIT_FAILURE);
    }


    /* Select video input, video standard and tune here. */


    CLEAR(cropcap);

    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (0 == xioctl(fd, VIDIOC_CROPCAP, &cropcap)) {
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap.defrect; /* reset to default */
        if (-1 == xioctl(fd, VIDIOC_S_CROP, &crop)) {
                switch (errno) {
                case EINVAL:
                        /* Cropping not supported. */
                        break;
                default:
                        /* Errors ignored. */
                        break;
                }
        }
    } else {
            /* Errors ignored. */
    }


    CLEAR(fmt);

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    fmt.fmt.pix.width       = 640; //replace
    fmt.fmt.pix.height      = 360; //replace
    //fmt.fmt.pix.width       = 1280; //replace
    //fmt.fmt.pix.height      = 720; //replace
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG; //replace
    fmt.fmt.pix.field       = V4L2_FIELD_ANY;

    if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt))
            errno_exit("VIDIOC_S_FMT");

    /* Note VIDIOC_S_FMT may change width and height.
     * TODO: Readback actual resolution selected */


    /* Buggy driver paranoia. */
    min = fmt.fmt.pix.width * 2;
    if (fmt.fmt.pix.bytesperline < min)
            fmt.fmt.pix.bytesperline = min;
    min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
    if (fmt.fmt.pix.sizeimage < min)
            fmt.fmt.pix.sizeimage = min;

    init_mmap();
}

int
camera_close()
{
    stop_capturing();
    uninit_device();
    if (-1 == close(fd))
        errno_exit("close");

    fd = -1;
    return 0;
}

int
camera_open(const char *dev)
{
    dev_name = dev;
    struct stat st;

    if (-1 == stat(dev_name, &st)) {
        fprintf(stderr, "Cannot identify '%s': %d, %s\n",
                 dev_name, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (!S_ISCHR(st.st_mode)) {
        fprintf(stderr, "%s is no device\n", dev_name);
        exit(EXIT_FAILURE);
    }

    fd = open(dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);

    if (-1 == fd) {
        fprintf(stderr, "Cannot open '%s': %d, %s\n",
                 dev_name, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    init_device();
    start_capturing();
    return fd;
}

#if 0
int main(int argc, char **argv)
{

        open_device();
        init_device();
        start_capturing();
        mainloop(remote, port);
        stop_capturing();
        uninit_device();
        close_device();
        fprintf(stderr, "\n");
        return 0;
}
#endif
