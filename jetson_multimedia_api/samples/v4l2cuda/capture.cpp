/*
 * Copyright (c) 2015, NVIDIA CORPORATION. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/*
 *  V4L2 video capture example
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <getopt.h>             /* getopt_long() */

#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <asm/types.h>          /* for videodev2.h */

#include <linux/videodev2.h>

#include <cuda_runtime.h>
#include "yuv2rgb.cuh"

#define CLEAR(x) memset (&(x), 0, sizeof (x))
#define ARRAY_SIZE(a)   (sizeof(a)/sizeof((a)[0]))

typedef enum {
    IO_METHOD_READ,
    IO_METHOD_MMAP,
    IO_METHOD_USERPTR,
} io_method;

struct buffer {
    void *                  start;
    size_t                  length;
};

static const char *     dev_name        = "/dev/video0";
static io_method        io              = IO_METHOD_MMAP;
static int              fd              = -1;
struct buffer *         buffers         = NULL;
static unsigned int     n_buffers       = 0;
static unsigned int     width           = 640;
static unsigned int     height          = 480;
static unsigned int     count           = 100;
static unsigned char *  cuda_out_buffer = NULL;
static bool             cuda_zero_copy = false;
static const char *     file_name       = "out.ppm";
static unsigned int     pixel_format    = V4L2_PIX_FMT_UYVY;
static unsigned int     field           = V4L2_FIELD_INTERLACED;

static void
errno_exit                      (const char *           s)
{
    fprintf (stderr, "%s error %d, %s\n",
            s, errno, strerror (errno));

    exit (EXIT_FAILURE);
}

static int
xioctl                          (int                    fd,
                                 int                    request,
                                 void *                 arg)
{
    int r;

    do r = ioctl (fd, request, arg);
    while (-1 == r && EINTR == errno);

    return r;
}

static void
process_image                   (void *           p)
{
    printf ("CUDA format conversion on frame %p\n", p);
    gpuConvertYUYVtoRGB ((unsigned char *) p, cuda_out_buffer, width, height);

    /* Save image. */
    if (count == 0) {
        FILE *fp = fopen (file_name, "wb");
        fprintf (fp, "P6\n%u %u\n255\n", width, height);
        fwrite (cuda_out_buffer, 1, width * height * 3, fp);
        fclose (fp);
    }
}

static int
read_frame                      (void)
{
    struct v4l2_buffer buf;
    unsigned int i;

    switch (io) {
        case IO_METHOD_READ:
            if (-1 == read (fd, buffers[0].start, buffers[0].length)) {
                switch (errno) {
                    case EAGAIN:
                        return 0;

                    case EIO:
                        /* Could ignore EIO, see spec. */

                        /* fall through */

                    default:
                        errno_exit ("read");
                }
            }

            process_image (buffers[0].start);

            break;

        case IO_METHOD_MMAP:
            CLEAR (buf);

            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;

            if (-1 == xioctl (fd, VIDIOC_DQBUF, &buf)) {
                switch (errno) {
                    case EAGAIN:
                        return 0;

                    case EIO:
                        /* Could ignore EIO, see spec. */

                        /* fall through */

                    default:
                        errno_exit ("VIDIOC_DQBUF");
                }
            }

            assert (buf.index < n_buffers);

            process_image (buffers[buf.index].start);

            if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
                errno_exit ("VIDIOC_QBUF");

            break;

        case IO_METHOD_USERPTR:
            CLEAR (buf);

            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_USERPTR;

            if (-1 == xioctl (fd, VIDIOC_DQBUF, &buf)) {
                switch (errno) {
                    case EAGAIN:
                        return 0;

                    case EIO:
                        /* Could ignore EIO, see spec. */

                        /* fall through */

                    default:
                        errno_exit ("VIDIOC_DQBUF");
                }
            }

            for (i = 0; i < n_buffers; ++i)
                if (buf.m.userptr == (unsigned long) buffers[i].start
                        && buf.length == buffers[i].length)
                    break;

            assert (i < n_buffers);

            process_image ((void *) buf.m.userptr);

            if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
                errno_exit ("VIDIOC_QBUF");

            break;
    }

    return 1;
}

static void
mainloop                        (void)
{
    while (count-- > 0) {
        for (;;) {
            fd_set fds;
            struct timeval tv;
            int r;

            FD_ZERO (&fds);
            FD_SET (fd, &fds);

            /* Timeout. */
            tv.tv_sec = 2;
            tv.tv_usec = 0;

            r = select (fd + 1, &fds, NULL, NULL, &tv);

            if (-1 == r) {
                if (EINTR == errno)
                    continue;

                errno_exit ("select");
            }

            if (0 == r) {
                fprintf (stderr, "select timeout\n");
                exit (EXIT_FAILURE);
            }

            if (read_frame ())
                break;

            /* EAGAIN - continue select loop. */
        }
    }
}

static void
stop_capturing                  (void)
{
    enum v4l2_buf_type type;

    switch (io) {
        case IO_METHOD_READ:
            /* Nothing to do. */
            break;

        case IO_METHOD_MMAP:
        case IO_METHOD_USERPTR:
            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

            if (-1 == xioctl (fd, VIDIOC_STREAMOFF, &type))
                errno_exit ("VIDIOC_STREAMOFF");

            break;
    }
}

static void
start_capturing                 (void)
{
    unsigned int i;
    enum v4l2_buf_type type;

    switch (io) {
        case IO_METHOD_READ:
            /* Nothing to do. */
            break;

        case IO_METHOD_MMAP:
            for (i = 0; i < n_buffers; ++i) {
                struct v4l2_buffer buf;

                CLEAR (buf);

                buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory      = V4L2_MEMORY_MMAP;
                buf.index       = i;

                if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
                    errno_exit ("VIDIOC_QBUF");
            }

            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

            if (-1 == xioctl (fd, VIDIOC_STREAMON, &type))
                errno_exit ("VIDIOC_STREAMON");

            break;

        case IO_METHOD_USERPTR:
            for (i = 0; i < n_buffers; ++i) {
                struct v4l2_buffer buf;

                CLEAR (buf);

                buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory      = V4L2_MEMORY_USERPTR;
                buf.index       = i;
                buf.m.userptr   = (unsigned long) buffers[i].start;
                buf.length      = buffers[i].length;

                if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
                    errno_exit ("VIDIOC_QBUF");
            }

            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

            if (-1 == xioctl (fd, VIDIOC_STREAMON, &type))
                errno_exit ("VIDIOC_STREAMON");

            break;
    }
}

static void
uninit_device                   (void)
{
    unsigned int i;

    switch (io) {
        case IO_METHOD_READ:
            free (buffers[0].start);
            break;

        case IO_METHOD_MMAP:
            for (i = 0; i < n_buffers; ++i)
                if (-1 == munmap (buffers[i].start, buffers[i].length))
                    errno_exit ("munmap");
            break;

        case IO_METHOD_USERPTR:
            for (i = 0; i < n_buffers; ++i) {
                if (cuda_zero_copy) {
                    cudaFree (buffers[i].start);
                } else {
                    free (buffers[i].start);
                }
            }
            break;
    }

    free (buffers);

    if (cuda_zero_copy) {
        cudaFree (cuda_out_buffer);
    }
}

static void
init_read                       (unsigned int           buffer_size)
{
    buffers = (struct buffer *) calloc (1, sizeof (*buffers));

    if (!buffers) {
        fprintf (stderr, "Out of memory\n");
        exit (EXIT_FAILURE);
    }

    buffers[0].length = buffer_size;
    buffers[0].start = malloc (buffer_size);

    if (!buffers[0].start) {
        fprintf (stderr, "Out of memory\n");
        exit (EXIT_FAILURE);
    }
}

static void
init_mmap                       (void)
{
    struct v4l2_requestbuffers req;

    CLEAR (req);

    req.count               = 4;
    req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory              = V4L2_MEMORY_MMAP;

    if (-1 == xioctl (fd, VIDIOC_REQBUFS, &req)) {
        if (EINVAL == errno) {
            fprintf (stderr, "%s does not support "
                    "memory mapping\n", dev_name);
            exit (EXIT_FAILURE);
        } else {
            errno_exit ("VIDIOC_REQBUFS");
        }
    }

    if (req.count < 2) {
        fprintf (stderr, "Insufficient buffer memory on %s\n",
                dev_name);
        exit (EXIT_FAILURE);
    }

    buffers = (struct buffer *) calloc (req.count, sizeof (*buffers));

    if (!buffers) {
        fprintf (stderr, "Out of memory\n");
        exit (EXIT_FAILURE);
    }

    for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
        struct v4l2_buffer buf;

        CLEAR (buf);

        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = n_buffers;

        if (-1 == xioctl (fd, VIDIOC_QUERYBUF, &buf))
            errno_exit ("VIDIOC_QUERYBUF");

        buffers[n_buffers].length = buf.length;
        buffers[n_buffers].start =
            mmap (NULL /* start anywhere */,
                    buf.length,
                    PROT_READ | PROT_WRITE /* required */,
                    MAP_SHARED /* recommended */,
                    fd, buf.m.offset);

        if (MAP_FAILED == buffers[n_buffers].start)
            errno_exit ("mmap");
    }
}

static void
init_userp                      (unsigned int           buffer_size)
{
    struct v4l2_requestbuffers req;
    unsigned int page_size;

    page_size = getpagesize ();
    buffer_size = (buffer_size + page_size - 1) & ~(page_size - 1);

    CLEAR (req);

    req.count               = 4;
    req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory              = V4L2_MEMORY_USERPTR;

    if (-1 == xioctl (fd, VIDIOC_REQBUFS, &req)) {
        if (EINVAL == errno) {
            fprintf (stderr, "%s does not support "
                    "user pointer i/o\n", dev_name);
            exit (EXIT_FAILURE);
        } else {
            errno_exit ("VIDIOC_REQBUFS");
        }
    }

    buffers = (struct buffer *) calloc (4, sizeof (*buffers));

    if (!buffers) {
        fprintf (stderr, "Out of memory\n");
        exit (EXIT_FAILURE);
    }

    for (n_buffers = 0; n_buffers < 4; ++n_buffers) {
        buffers[n_buffers].length = buffer_size;
        if (cuda_zero_copy) {
            cudaMallocManaged (&buffers[n_buffers].start, buffer_size, cudaMemAttachGlobal);
        } else {
            buffers[n_buffers].start = memalign (/* boundary */ page_size,
                    buffer_size);
        }

        if (!buffers[n_buffers].start) {
            fprintf (stderr, "Out of memory\n");
            exit (EXIT_FAILURE);
        }
    }
}

static void
init_device                     (void)
{
    struct v4l2_capability cap;
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;
    struct v4l2_format fmt;
    unsigned int min;

    if (-1 == xioctl (fd, VIDIOC_QUERYCAP, &cap)) {
        if (EINVAL == errno) {
            fprintf (stderr, "%s is no V4L2 device\n",
                    dev_name);
            exit (EXIT_FAILURE);
        } else {
            errno_exit ("VIDIOC_QUERYCAP");
        }
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        fprintf (stderr, "%s is no video capture device\n",
                dev_name);
        exit (EXIT_FAILURE);
    }

    switch (io) {
        case IO_METHOD_READ:
            if (!(cap.capabilities & V4L2_CAP_READWRITE)) {
                fprintf (stderr, "%s does not support read i/o\n",
                        dev_name);
                exit (EXIT_FAILURE);
            }

            break;

        case IO_METHOD_MMAP:
        case IO_METHOD_USERPTR:
            if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
                fprintf (stderr, "%s does not support streaming i/o\n",
                        dev_name);
                exit (EXIT_FAILURE);
            }

            break;
    }


    /* Select video input, video standard and tune here. */


    CLEAR (cropcap);

    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (0 == xioctl (fd, VIDIOC_CROPCAP, &cropcap)) {
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap.defrect; /* reset to default */

        if (-1 == xioctl (fd, VIDIOC_S_CROP, &crop)) {
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


    CLEAR (fmt);

    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = width;
    fmt.fmt.pix.height      = height;
    fmt.fmt.pix.pixelformat = pixel_format;
    fmt.fmt.pix.field       = field;

    if (-1 == xioctl (fd, VIDIOC_S_FMT, &fmt))
        errno_exit ("VIDIOC_S_FMT");

    /* Note VIDIOC_S_FMT may change width and height. */

    /* Buggy driver paranoia. */
    min = fmt.fmt.pix.width * 2;
    if (fmt.fmt.pix.bytesperline < min)
        fmt.fmt.pix.bytesperline = min;
    min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
    if (fmt.fmt.pix.sizeimage < min)
        fmt.fmt.pix.sizeimage = min;

    switch (io) {
        case IO_METHOD_READ:
            init_read (fmt.fmt.pix.sizeimage);
            break;

        case IO_METHOD_MMAP:
            init_mmap ();
            break;

        case IO_METHOD_USERPTR:
            init_userp (fmt.fmt.pix.sizeimage);
            break;
    }
}

static void
close_device                    (void)
{
    if (-1 == close (fd))
        errno_exit ("close");

    fd = -1;
}

static void
open_device                     (void)
{
    struct stat st;

    if (-1 == stat (dev_name, &st)) {
        fprintf (stderr, "Cannot identify '%s': %d, %s\n",
                dev_name, errno, strerror (errno));
        exit (EXIT_FAILURE);
    }

    if (!S_ISCHR (st.st_mode)) {
        fprintf (stderr, "%s is no device\n", dev_name);
        exit (EXIT_FAILURE);
    }

    fd = open (dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);

    if (-1 == fd) {
        fprintf (stderr, "Cannot open '%s': %d, %s\n",
                dev_name, errno, strerror (errno));
        exit (EXIT_FAILURE);
    }
}

static void
init_cuda                       (void)
{
    /* Check unified memory support. */
    if (cuda_zero_copy) {
        cudaDeviceProp devProp;
        cudaGetDeviceProperties (&devProp, 0);
        if (!devProp.managedMemory) {
            printf ("CUDA device does not support managed memory.\n");
            cuda_zero_copy = false;
        }
    }

    /* Allocate output buffer. */
    size_t size = width * height * 3;
    if (cuda_zero_copy) {
        cudaMallocManaged (&cuda_out_buffer, size, cudaMemAttachGlobal);
    } else {
        cuda_out_buffer = (unsigned char *) malloc (size);
    }

    cudaDeviceSynchronize ();
}

static void
usage                           (FILE *                 fp,
                                 int                    argc,
                                 char **                argv)
{
    fprintf (fp,
            "Usage: %s [options]\n\n"
            "Options:\n"
            "-c | --count N       Frame count (default: %u)\n"
            "-d | --device name   Video device name (default: %s)\n"
            "-f | --format        Capture input pixel format (default: UYVY)\n"
            "-h | --help          Print this message\n"
            "-m | --mmap          Use memory mapped buffers\n"
            "-o | --output        Output file name (default: %s)\n"
            "-s | --size WxH      Frame size (default: %ux%u)\n"
            "-u | --userp         Use application allocated buffers\n"
            "-z | --zcopy         Use zero copy CUDA memory\n"
            "Experimental options:\n"
            "-r | --read          Use read() calls\n"
            "-F | --field         Capture field (default: INTERLACED)\n"
            "",
            argv[0], count, dev_name, file_name, width, height);
}

static const char short_options [] = "c:d:f:F:hmo:rs:uz";

static const struct option
long_options [] = {
    { "count",      required_argument,      NULL,           'c' },
    { "device",     required_argument,      NULL,           'd' },
    { "format",     required_argument,      NULL,           'f' },
    { "field",      required_argument,      NULL,           'F' },
    { "help",       no_argument,            NULL,           'h' },
    { "mmap",       no_argument,            NULL,           'm' },
    { "output",     required_argument,      NULL,           'o' },
    { "read",       no_argument,            NULL,           'r' },
    { "size",       required_argument,      NULL,           's' },
    { "userp",      no_argument,            NULL,           'u' },
    { "zcopy",      no_argument,            NULL,           'z' },
    { 0, 0, 0, 0 }
};

static struct {
    const char *name;
    unsigned int fourcc;
} pixel_formats[] = {
    { "RGB332", V4L2_PIX_FMT_RGB332 },
    { "RGB555", V4L2_PIX_FMT_RGB555 },
    { "RGB565", V4L2_PIX_FMT_RGB565 },
    { "RGB555X", V4L2_PIX_FMT_RGB555X },
    { "RGB565X", V4L2_PIX_FMT_RGB565X },
    { "BGR24", V4L2_PIX_FMT_BGR24 },
    { "RGB24", V4L2_PIX_FMT_RGB24 },
    { "BGR32", V4L2_PIX_FMT_BGR32 },
    { "RGB32", V4L2_PIX_FMT_RGB32 },
    { "Y8", V4L2_PIX_FMT_GREY },
    { "Y10", V4L2_PIX_FMT_Y10 },
    { "Y12", V4L2_PIX_FMT_Y12 },
    { "Y16", V4L2_PIX_FMT_Y16 },
    { "UYVY", V4L2_PIX_FMT_UYVY },
    { "VYUY", V4L2_PIX_FMT_VYUY },
    { "YUYV", V4L2_PIX_FMT_YUYV },
    { "YVYU", V4L2_PIX_FMT_YVYU },
    { "NV12", V4L2_PIX_FMT_NV12 },
    { "NV21", V4L2_PIX_FMT_NV21 },
    { "NV16", V4L2_PIX_FMT_NV16 },
    { "NV61", V4L2_PIX_FMT_NV61 },
    { "NV24", V4L2_PIX_FMT_NV24 },
    { "NV42", V4L2_PIX_FMT_NV42 },
    { "SBGGR8", V4L2_PIX_FMT_SBGGR8 },
    { "SGBRG8", V4L2_PIX_FMT_SGBRG8 },
    { "SGRBG8", V4L2_PIX_FMT_SGRBG8 },
    { "SRGGB8", V4L2_PIX_FMT_SRGGB8 },
    { "SBGGR10_DPCM8", V4L2_PIX_FMT_SBGGR10DPCM8 },
    { "SGBRG10_DPCM8", V4L2_PIX_FMT_SGBRG10DPCM8 },
    { "SGRBG10_DPCM8", V4L2_PIX_FMT_SGRBG10DPCM8 },
    { "SRGGB10_DPCM8", V4L2_PIX_FMT_SRGGB10DPCM8 },
    { "SBGGR10", V4L2_PIX_FMT_SBGGR10 },
    { "SGBRG10", V4L2_PIX_FMT_SGBRG10 },
    { "SGRBG10", V4L2_PIX_FMT_SGRBG10 },
    { "SRGGB10", V4L2_PIX_FMT_SRGGB10 },
    { "SBGGR12", V4L2_PIX_FMT_SBGGR12 },
    { "SGBRG12", V4L2_PIX_FMT_SGBRG12 },
    { "SGRBG12", V4L2_PIX_FMT_SGRBG12 },
    { "SRGGB12", V4L2_PIX_FMT_SRGGB12 },
    { "DV", V4L2_PIX_FMT_DV },
    { "MJPEG", V4L2_PIX_FMT_MJPEG },
    { "MPEG", V4L2_PIX_FMT_MPEG },
};

static unsigned int v4l2_format_code(const char *name)
{
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(pixel_formats); ++i) {
        if (strcasecmp(pixel_formats[i].name, name) == 0)
            return pixel_formats[i].fourcc;
    }

    return 0;
}

static struct {
    const char *name;
    unsigned int field;
} fields[] = {
    { "ANY", V4L2_FIELD_ANY },
    { "NONE", V4L2_FIELD_NONE },
    { "TOP", V4L2_FIELD_TOP },
    { "BOTTOM", V4L2_FIELD_BOTTOM },
    { "INTERLACED", V4L2_FIELD_INTERLACED },
    { "SEQ_TB", V4L2_FIELD_SEQ_TB },
    { "SEQ_BT", V4L2_FIELD_SEQ_BT },
    { "ALTERNATE", V4L2_FIELD_ALTERNATE },
    { "INTERLACED_TB", V4L2_FIELD_INTERLACED_TB },
    { "INTERLACED_BT", V4L2_FIELD_INTERLACED_BT },
};

static unsigned int v4l2_field_code(const char *name)
{
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(fields); ++i) {
        if (strcasecmp(fields[i].name, name) == 0)
            return fields[i].field;
    }

    return -1;
}

int
main                            (int                    argc,
                                 char **                argv)
{
    for (;;) {
        int index;
        int c;

        c = getopt_long (argc, argv,
                short_options, long_options,
                &index);

        if (-1 == c)
            break;

        switch (c) {
            case 0: /* getopt_long() flag */
                break;

            case 'c':
                count = atoi (optarg);
                break;

            case 'd':
                dev_name = optarg;
                break;

            case 'f':
                pixel_format = v4l2_format_code(optarg);
                if (pixel_format == 0) {
                    printf("Unsupported video format '%s'\n", optarg);
                    pixel_format = V4L2_PIX_FMT_UYVY;
                }
                break;

            case 'F':
                field = v4l2_field_code(optarg);
                if ((int)field < 0) {
                    printf("Unsupported field '%s'\n", optarg);
                    field = V4L2_FIELD_INTERLACED;
                }
                break;

            case 'h':
                usage (stdout, argc, argv);
                exit (EXIT_SUCCESS);

            case 'm':
                io = IO_METHOD_MMAP;
                break;

            case 'o':
                file_name = optarg;
                break;

            case 'r':
                io = IO_METHOD_READ;
                break;

            case 's':
                width = atoi (strtok (optarg, "x"));
                height = atoi (strtok (NULL, "x"));
                break;

            case 'u':
                io = IO_METHOD_USERPTR;
                break;

            case 'z':
                cuda_zero_copy = true;
                break;

            default:
                usage (stderr, argc, argv);
                exit (EXIT_FAILURE);
        }
    }

    open_device ();

    init_device ();

    init_cuda ();

    start_capturing ();

    mainloop ();

    stop_capturing ();

    uninit_device ();

    close_device ();

    exit (EXIT_SUCCESS);

    return 0;
}
