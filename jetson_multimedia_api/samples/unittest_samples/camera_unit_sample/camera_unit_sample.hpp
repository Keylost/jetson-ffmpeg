/*
 * Copyright (c) 2020-2021, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <string>
#include <sstream>
#include <fstream>
#include <libv4l2.h>
#include <linux/videodev2.h>
#include <linux/v4l2-controls.h>
#include "NvEglRenderer.h"

/**
 * Specifies the camera device node.
 */
#define CAMERA_DEV "/dev/video"
/**
 * Specifies the maximum number of planes a buffer can contain.
 */
#define MAX_PLANES 3

/**
 * Specifies the maximum number of capture buffers.
 */
#define MAX_CAPTURE_BUFFFERS 32

/* Defaults */
#define DEFAULT_ARGUS_SENSOR_ID                 0
#define DEFAULT_ARGUS_SENSOR_MODE               -1
#define DEFAULT_ARGUS_FPS                       30
#define DEFAULT_NUM_FRAMES                      300
#define DEFAULT_ARGUS_AWB_MODE                  V4L2_ARGUS_AWB_MODE_AUTO
#define DEFAULT_ARGUS_DENOISE_MODE              V4L2_ARGUS_DENOISE_MODE_UNKNOWN
#define DEFAULT_ARGUS_DENOISE_STRENGTH          -1.0
#define DEFAULT_ARGUS_EE_MODE                   V4L2_ARGUS_EDGE_ENHANCE_MODE_UNKNOWN
#define DEFAULT_ARGUS_EE_STRENGTH               -1.0
#define DEFAULT_ARGUS_AEANTIBANDING_MODE        V4L2_ARGUS_AE_ANTIBANDING_MODE_UNKNOWN
#define DEFAULT_ARGUS_AE_LOCK                   0
#define DEFAULT_ARGUS_AWB_LOCK                  0
#define DEFAULT_ARGUS_EXP_COMPENSATION          0.0
#define DEFAULT_ARGUS_DIGITAL_GAIN_RANGE_MIN    1
#define DEFAULT_ARGUS_DIGITAL_GAIN_RANGE_MAX    256
#define DEFAULT_ARGUS_GAIN_RANGE_MIN            1
#define DEFAULT_ARGUS_GAIN_RANGE_MAX            16
#define DEFAULT_ARGUS_EXPOSURE_TIME_MIN         34000
#define DEFAULT_ARGUS_EXPOSURE_TIME_MAX         358733000
#define DEFAULT_ARGUS_DIGITAL_SATURATION        1.0

/**
 * @brief Class representing a buffer.
 *
 * The Buffer class is modeled on the basis of the @c v4l2_buffer
 * structure. The buffer has @c buf_type @c v4l2_buf_type, @c
 * memory_type @c v4l2_memory, and an index. It contains an
 * BufferPlane array similar to the array of @c v4l2_plane
 * structures in @c v4l2_buffer.m.planes. It also contains a
 * corresponding BufferPlaneFormat array that describes the
 * format of each of the planes.
 *
 * In the case of a V4L2 MMAP, this class provides convenience methods
 * for mapping or unmapping the contents of the buffer to or from
 * memory, allocating or deallocating software memory depending on its
 * format.
 */
class Buffer
{
public:
    /**
     * Holds the buffer plane format.
     */
    typedef struct
    {
        uint32_t width;             /** Holds the width of the plane in pixels. */
        uint32_t height;            /** Holds the height of the plane in pixels. */

        uint32_t bytesperpixel;     /** Holds the bytes used to represent one
                                      pixel in the plane. */
        uint32_t stride;            /** Holds the stride of the plane in bytes. */
        uint32_t sizeimage;         /** Holds the size of the plane in bytes. */
    } BufferPlaneFormat;

    /**
     * Holds the buffer plane parameters.
     */
    typedef struct
    {
        BufferPlaneFormat fmt;      /** Holds the format of the plane. */

        unsigned char *data;        /** Holds a pointer to the plane memory. */
        uint32_t bytesused;         /** Holds the number of valid bytes in the plane. */

        int fd;                     /** Holds the file descriptor (FD) of the plane of the
                                      exported buffer, in the case of V4L2 MMAP buffers. */
        uint32_t mem_offset;        /** Holds the offset of the first valid byte
                                      from the data pointer. */
        uint32_t length;            /** Holds the size of the buffer in bytes. */
    } BufferPlane;

    Buffer(enum v4l2_buf_type buf_type, enum v4l2_memory memory_type,
        uint32_t index);

    Buffer(enum v4l2_buf_type buf_type, enum v4l2_memory memory_type,
           uint32_t n_planes, BufferPlaneFormat *fmt, uint32_t index);

     ~Buffer();

    /**
     * Maps the contents of the buffer to memory.
     *
     * This method maps the file descriptor (FD) of the planes to
     * a data pointer of @c planes. (MMAP buffers only.)
     */
    int map();
    /**
     * Unmaps the contents of the buffer from memory. (MMAP buffers only.)
     *
     */
    void unmap();

    enum v4l2_buf_type buf_type;    /** Type of the buffer. */
    enum v4l2_memory memory_type;   /** Type of memory associated
                                        with the buffer. */

    uint32_t index;                 /** Holds the buffer index. */

    uint32_t n_planes;              /** Holds the number of planes in the buffer. */
    BufferPlane planes[MAX_PLANES]; /** Holds the data pointer, plane file
                                        descriptor (FD), plane format, etc. */

    /**
     * Fills the Buffer::BufferPlaneFormat array.
     */
    static int fill_buffer_plane_format(uint32_t *num_planes,
            Buffer::BufferPlaneFormat *planefmts,
            uint32_t width, uint32_t height, uint32_t raw_pixfmt);

private:

    bool mapped;

};

typedef struct Range
{
  /* Lower limit for the float range. */
  float low;
  /* Upper limit for the float range. */
  float high;
} Range;

/* Argus controls */
typedef struct
{
    uint32_t fps_n;
    uint32_t fps_d;
    int32_t sensor_id;
    int32_t sensor_mode;
    uint8_t aelock;
    uint8_t awblock;
    enum v4l2_argus_ac_awb_mode autowhitebalance_mode;
    enum v4l2_argus_denoise_mode denoise_mode;
    enum v4l2_argus_edge_enhance_mode edge_enhacement_mode;
    enum v4l2_argus_ac_ae_antibanding_mode ae_antibanding_mode;
    float denoise_strength;
    float edge_enhacement_strength;
    float exposure_compensation;
    float color_saturation;
    Range isp_digital_gain_range;
    Range gain_range;
    Range exposure_time_range;
} argus_controls;

/** Renderer **/
typedef struct
{
    NvEglRenderer *renderer;
    uint32_t window_width;
    uint32_t window_height;
    uint32_t window_xoff;
    uint32_t window_yoff;
} display_settings;

/** Stores the capture plane data **/
typedef struct
{
    uint32_t num_planes;
    uint32_t num_buffers;
    uint32_t num_queued_buffers;

    enum v4l2_memory mem_type;
    enum v4l2_buf_type buf_type;

    Buffer::BufferPlaneFormat planefmts[MAX_PLANES];
    Buffer **buffers;

    bool streamon;
    bool format_set;
} capture_plane;

/**
 * @brief Struct defining the camera context.
 * The video camera device node is `/dev/video0`.
 *
 * The context stores the information for Argus camera.
 */
typedef struct
{
    uint32_t raw_pixfmt;
    uint32_t width;
    uint32_t height;

    char *output_file_path;
    ofstream *output_file;

    pthread_mutex_t queue_lock;
    pthread_cond_t queue_cond;
    pthread_t cam_dq_thread;

    argus_controls ctrls;
    display_settings display;
    capture_plane capplane;

    bool set_awbmode;
    bool set_denoisemode;
    bool set_denoise_strength;
    bool set_eemode;
    bool set_eestrength;
    bool set_aeantibandingmode;
    bool set_autolock;
    bool set_expcompensation;
    bool set_ispdigitalgainrange;
    bool set_colorsaturation;
    bool set_gainrange;
    bool set_exptimerange;
    bool fullscreen_mode;
    bool disable_rendering;
    bool enable_metadata;

    bool in_error;
    bool eos;
    bool dqthread_running;
    int32_t stress_test;
    int32_t num_frames;
    int32_t dq_buffer_count;
    int32_t dmabuffers_fd[MAX_CAPTURE_BUFFFERS];
    int32_t fd;
} context_t;

/**
 * @brief Writes a raw YUV frame from the buffer to a file.
 *
 * This function writes data into the file from the buffer plane-by-plane.
 *
 * @param[in] stream A pointer to the output file stream.
 * @param[in] buffer Buffer class pointer
 * @return 0 for success, -1 otherwise.
 */
int write_frame(ofstream * stream, Buffer *buffer);

/**
 * @brief Sets the format on the camera capture plane.
 *
 * Calls the \c VIDIOC_S_FMT IOCTL internally on the capture plane.
 *
 * @param[in] ctx Reference to the camera context struct created.
 * @return 0 for success, -1 otherwise.
 */
int set_plane_format(context_t& ctx);

/**
 * @brief Sets the Argus properties on the camera capture plane.
 *
 * Calls the \c VIDIOC_S_EXT_CTRLS IOCTL internally on the capture plane.
 *
 * @param[in] ctx Reference to the camera context struct created.
 * @return 0 for success, -1 otherwise.
 */
int set_controls(context_t *ctx);

/**
 * @brief Requests for MMAP buffers on the camera capture plane.
 *
 * Calls the \c VIDIOC_REQBUFS IOCTL internally on the capture plane.
 *
 * @param[in] ctx Pointer to the camera context struct created.
 * @param[in] buf_type Type of buffer, one of the enum v4l2_buf_type.
 * @param[in] mem_type Memory type of the plane, one of the
 *                     enum v4l2_memory mem_type, here V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE
 * @param[in] num_buffers Number of buffers to be requested.
 * @return 0 for success, -1 otherwise.
 */
int req_buffers_on_capture_plane(context_t * ctx, enum v4l2_buf_type buf_type,
		enum v4l2_memory mem_type, int num_buffers);

/**
 * @brief Requests and allocates DMA buffers on the camera capture plane.
 *
 * Calls the \c VIDIOC_REQBUFS IOCTL internally on the capture plane.
 *
 * @param[in] ctx Pointer to the camera context struct created.
 * @param[in] buf_type Type of buffer, one of the enum v4l2_buf_type.
 * @param[in] mem_type Memory type of the plane, one of the
 *                     enum v4l2_memory mem_type, here V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE
 * @param[in] num_buffers Number of buffers to be requested.
 * @return 0 for success, -1 otherwise.
 */
int allocate_dmabuffers(context_t * ctx, enum v4l2_buf_type buf_type,
        enum v4l2_memory mem_type, int num_buffers);

/**
 * @brief Queues a buffer on the plane.
 *
 * This method calls \c VIDIOC_QBUF IOCTL internally.
 *
 * @param[in] ctx Pointer to the camera context struct created.
 * @param[in] v4l2_buf A reference to the \c v4l2_buffer structure to use for queueing.
 * @param[in] buffer A pointer to the \c %Buffer object.
 * @param[in] buf_type Type of buffer, one of the enum v4l2_buf_type.
 * @param[in] mem_type Memory type of the plane, one of the
 *                     enum v4l2_memory mem_type
 * @param[in] num_planes Number of planes in the buffer.
 * @return 0 for success, -1 otherwise.
 */
int q_buffer(context_t * ctx, struct v4l2_buffer &v4l2_buf, Buffer * buffer,
    enum v4l2_buf_type buf_type, enum v4l2_memory memory_type, int num_planes);

/**
 * @brief Dequeues a buffer from the plane.
 *
 * This method calls \c VIDIOC_DQBUF IOCTL internally.
 * This is a blocking call. This call returns when a buffer is successfully
 * dequeued or timeout is reached. If the buffer is not NULL, returns the
 * Buffer object at the index returned by VIDIOC_DQBUF IOCTL
 *
 * @param[in] ctx Pointer to the camera context struct created.
 * @param[in] v4l2_buf A reference to the \c v4l2_buffer structure to use for dequeueing.
 * @param[in] buffer A double pointer to the \c %Buffer object associated with the dequeued
 *                   buffer. Can be NULL.
 * @param[in] buf_type Type of buffer, one of the enum v4l2_buf_type.
 * @param[in] mem_type Memory type of the plane, one of the
 *                     enum v4l2_memory mem_type
 * @param[in] num_retries Number of times to try dequeuing a buffer before
 *                        a failure is returned.
 * @return 0 for success, -1 otherwise.
 */
int dq_buffer(context_t * ctx, struct v4l2_buffer &v4l2_buf, Buffer ** buffer,
	enum v4l2_buf_type buf_type, enum v4l2_memory memory_type, uint32_t num_retries);

/**
 * @brief Callback function when cam_dq_thread is created.
 *
 * This is a callback function of the capture loop thread created.
 * The function runs infinitely until signaled to stop, or error
 * is encountered. On successful dequeue of a buffer from the plane,
 * the method calls capture_plane_callback.
 *
 * Setting the stream to off automatically stops the thread.
 *
 * @param[in] arg A pointer to the application data.
 */
void * dq_thread(void *arg);

/**
 * @brief Writes NvBuffer data to a file.
 *
 * This function writes data to the file from a buffer.
 *
 * @param[in] stream A pointer to the output file stream.
 * @param[in] dmabuf_fd DMABUF FD of buffer.
 * @return 0 for success, -1 otherwise.
 */
int dump_dmabuffers(ofstream *stream, int fd);

/**
 * @brief DQ callback function.
 *
 * This is a callback function type method that is called by the DQ Thread when
 * it successfully dequeues a buffer from the plane.
 *
 * Setting the stream to off automatically stops the thread.
 *
 * @param[in] v4l2_buf A reference to the \c v4l2_buffer structure to use for dequeueing.
 * @param[in] buffer A pointer to the \c %Buffer object associated with the dequeued
 *                   buffer. Can be NULL.
 * @param[in] arg A pointer to the application data.
 */
bool capture_plane_callback(struct v4l2_buffer *v4l2_buf, Buffer * buffer, void *arg);

/**
 * @brief Waits for the DQ Thread to stop.
 *
 * This method waits until the DQ Thread stops or timeout is reached.
 *
 * @sa dq_thread
 *
 * @param[in] ctx Reference to the camera context struct created.
 * @param[in] max_wait_ms Maximum wait time, in milliseconds.
 * @return 0 for success, -1 otherwise.
 */
int wait_for_dqthread(context_t& ctx, uint32_t max_wait_ms);

/**
 * @brief Parses command-line arguments.
 *
 * This function parses the arguments given to the sample app.
 *
 *
 * @param[in] ctx Reference to the camera context struct created.
 * @param[in] argv Arguments.
 * @return 0 for success, -1 otherwise.
 */
int parse_cmdline_args(context_t * ctx, int argc, const char *argv[]);

int capture_proc (context_t &ctx, int argc, char const *argv[]);
