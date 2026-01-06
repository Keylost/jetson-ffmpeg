/*
 * Copyright (c) 2019-2023, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer.
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

/**
 * Specifies the decoder device node.
 */
#define DECODER_DEV "/dev/nvhost-nvdec"
#define MAX_BUFFERS 32
#define CHUNK_SIZE 4000000
/**
 * Specifies the maximum number of planes a buffer can contain.
 */
#define MAX_PLANES 3
#define MIN(a,b) (((a) < (b)) ? (a) : (b))

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
        uint32_t width;             /**< Holds the width of the plane in pixels. */
        uint32_t height;            /**< Holds the height of the plane in pixels. */

        uint32_t bytesperpixel;     /**< Holds the bytes used to represent one
                                      pixel in the plane. */
        uint32_t stride;            /**< Holds the stride of the plane in bytes. */
        uint32_t sizeimage;         /**< Holds the size of the plane in bytes. */
    } BufferPlaneFormat;

    /**
     * Holds the buffer plane parameters.
     */
    typedef struct
    {
        BufferPlaneFormat fmt;      /**< Holds the format of the plane. */

        unsigned char *data;        /**< Holds a pointer to the plane memory. */
        uint32_t bytesused;         /**< Holds the number of valid bytes in the plane. */

        int fd;                     /**< Holds the file descriptor (FD) of the plane of the
                                      exported buffer, in the case of V4L2 MMAP buffers. */
        uint32_t mem_offset;        /**< Holds the offset of the first valid byte
                                      from the data pointer. */
        uint32_t length;            /**< Holds the size of the buffer in bytes. */
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

    enum v4l2_buf_type buf_type;    /**< Type of the buffer. */
    enum v4l2_memory memory_type;   /**< Type of memory associated
                                        with the buffer. */

    uint32_t index;                 /**< Holds the buffer index. */

    uint32_t n_planes;              /**< Holds the number of planes in the buffer. */
    BufferPlane planes[MAX_PLANES]; /**< Holds the data pointer, plane file
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

/**
 * @brief Struct defining the decoder context.
 * The video decoder device node is `/dev/nvhost-nvdec`. The category name
 * for the decoder is \c "NVDEC".
 *
 * The context stores the information for decoding.
 * Refer to [V4L2 Video Decoder](group__V4L2Dec.html) for more information on the decoder.
 */
typedef struct
{
    uint32_t decode_pixfmt;
    uint32_t out_pixfmt;
    uint32_t display_width;
    uint32_t display_height;
    enum v4l2_memory op_mem_type;
    enum v4l2_memory cp_mem_type;
    enum v4l2_buf_type op_buf_type;
    enum v4l2_buf_type cp_buf_type;
    Buffer::BufferPlaneFormat op_planefmts[MAX_PLANES];
    Buffer::BufferPlaneFormat cp_planefmts[MAX_PLANES];
    uint32_t cp_num_planes;
    uint32_t op_num_planes;
    uint32_t cp_num_buffers;
    uint32_t op_num_buffers;

    uint32_t num_queued_op_buffers;

    Buffer **op_buffers;
    Buffer **cp_buffers;

    ifstream *in_file;

    ofstream *out_file;

    pthread_mutex_t queue_lock;
    pthread_cond_t queue_cond;
    pthread_t dec_capture_thread;

    bool in_error;
    bool eos;
    bool got_eos;
    bool op_streamon;
    bool cp_streamon;
    int fd;
    int dst_dma_fd;
    int dmabuff_fd[MAX_BUFFERS];
} context_t;

/**
 * @brief Reads the encoded data from a file to the buffer structure.
 *
 * Helper function to read the filestream to mmaped buffer
 *
 * @param[in] stream Input stream
 * @param[in] buffer Buffer class pointer
 */
static void read_input_chunk(ifstream * stream, Buffer * buffer);

/**
 * @brief Writes a plane data of the buffer to a file.
 *
 * This function writes data into the file from a plane of the DMA buffer.
 *
 * @param[in] dmabuf_fd DMABUF FD of buffer.
 * @param[in] plane video frame plane.
 * @param[in] stream A pointer to the output file stream.
 * @return 0 for success, -1 otherwise.
 */
static int dump_raw_dmabuf(int dmabuf_fd, unsigned int plane, ofstream * stream);

/**
 * @brief Sets the format on the decoder capture plane.
 *
 * Calls the \c VIDIOC_S_FMT IOCTL internally on the capture plane.
 *
 * @param[in] ctx Pointer to the decoder context struct created.
 * @param[in] pixfmt One of the coded V4L2 pixel formats.
 * @param[in] width Width of the frame
 * @param[in] height Height of the frame
 * @return 0 for success, -1 otherwise.
 */
static int set_capture_plane_format(context_t * ctx, uint32_t pixfmt,
    uint32_t width, uint32_t height);

/**
 * @brief Query and Set Capture plane.
 *
 * On successful dqevent, this method sets capture plane properties,
 * buffers and streaming status.
 *
 * @param[in] ctx Pointer to the decoder context struct created.
 */
static void query_set_capture(context_t * ctx);

/**
 * @brief Callback function on capture thread.
 *
 * This is a callback function of the capture loop thread created.
 * the function runs infinitely until signaled to stop, or error
 * is encountered.
 *
 * Setting the stream to off automatically stops the thread.
 *
 * @param[in] arg A pointer to the application data.
 */
static void * capture_thread(void *arg);

/**
 * @brief Decode processing function for blocking mode.
 *
 * Function loop to DQ and EnQ buffers on output plane
 * till eos is signalled
 *
 * @param[in] ctx Reference to the decoder context struct created.
 * @return If the application implementing this call returns TRUE,
 *         EOS is detected by the decoder and all the buffers are dequeued;
 *         else the decode process continues running.
 */
static bool decode_process(context_t& ctx);

/**
 * @brief Dequeues an event.
 *
 * Calls \c VIDIOC_DQEVENT IOCTL internally. This is a blocking call.
 * The call returns when an event is successfully dequeued or timeout is reached.
 *
 * @param[in] ctx Pointer to the decoder context struct created.
 * @param[in,out] event A reference to the \c v4l2_event structure to fill.
 * @param[in] max_wait_ms Specifies the max wait time for dequeuing an event,
 *                        in milliseconds.
 * @return 0 for success, -1 otherwise.
 */
static int dq_event(context_t * ctx, struct v4l2_event &event, uint32_t max_wait_ms);

/**
 * @brief Dequeues a buffer from the plane.
 *
 * This method calls \c VIDIOC_DQBUF IOCTL internally.
 * This is a blocking call. This call returns when a buffer is successfully
 * dequeued or timeout is reached. If the buffer is not NULL, returns the
 * Buffer object at the index returned by VIDIOC_DQBUF IOCTL
 *
 * @param[in] ctx Pointer to the decoder context struct created.
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
static int dq_buffer(context_t * ctx, struct v4l2_buffer &v4l2_buf, Buffer ** buffer,
    enum v4l2_buf_type buf_type, enum v4l2_memory memory_type, uint32_t num_retries);

/**
 * @brief Queues a buffer on the plane.
 *
 * This method calls \c VIDIOC_QBUF IOCTL internally.
 *
 * @param[in] ctx Pointer to the decoder context struct created.
 * @param[in] v4l2_buf A reference to the \c v4l2_buffer structure to use for queueing.
 * @param[in] buffer A pointer to the \c %Buffer object.
 * @param[in] buf_type Type of buffer, one of the enum v4l2_buf_type.
 * @param[in] mem_type Memory type of the plane, one of the
 *                     enum v4l2_memory mem_type
 * @param[in] num_planes Number of planes in the buffer.
 * @return 0 for success, -1 otherwise.
 */
static int q_buffer(context_t * ctx, struct v4l2_buffer &v4l2_buf, Buffer * buffer,
    enum v4l2_buf_type buf_type, enum v4l2_memory memory_type, int num_planes);

/**
 * @brief Requests for buffers on the decoder capture plane.
 *
 * Calls the \c VIDIOC_REQBUFS IOCTL internally on the capture plane.
 *
 * @param[in] ctx Pointer to the decoder context struct created.
 * @param[in] buf_type Type of buffer, one of the enum v4l2_buf_type.
 * @param[in] mem_type Memory type of the plane, one of the
 *                     enum v4l2_memory mem_type, here V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE
 * @param[in] num_buffers Number of buffers to be requested.
 * @return 0 for success, -1 otherwise.
 */
static int req_buffers_on_capture_plane(context_t * ctx, enum v4l2_buf_type buf_type,
    enum v4l2_memory mem_type, int num_buffers);

/**
 * @brief Requests for buffers on the decoder output plane.
 *
 * Calls the \c VIDIOC_REQBUFS IOCTL internally on the output plane.
 *
 * @param[in] ctx Pointer to the decoder context struct created.
 * @param[in] buf_type Type of buffer, one of the enum v4l2_buf_type.
 * @param[in] mem_type Memory type of the plane, one of the
 *                     enum v4l2_memory mem_type, here V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE
 * @param[in] num_buffers Number of buffers to be requested.
 * @return 0 for success, -1 otherwise.
 */
static int req_buffers_on_output_plane(context_t * ctx, enum v4l2_buf_type buf_type,
    enum v4l2_memory mem_type, int num_buffers);

/**
 * @brief Sets the format on the decoder output plane.
 *
 * Calls the \c VIDIOC_S_FMT IOCTL internally on the output plane.
 *
 * @param[in] ctx Reference to the decoder context struct created.
 * @param[in] pixfmt One of the coded V4L2 pixel formats.
 * @param[in] sizeimage Maximum size of the buffers on the output plane.
                        containing encoded data in bytes.
 * @return 0 for success, -1 otherwise.
 */
static int set_output_plane_format(context_t& ctx, uint32_t pixfmt, uint32_t sizeimage);

/**
 * @brief Sets the value of controls
 *
 * Calls the \c VIDIOC_S_EXT_CTRLS IOCTL internally with Control ID
 * as an input.
 *
 * @param[in] fd context FD
 * @param[in] id control id
 * @param[in] value control value
 * @return 0 for success, -1 otherwise.
 */
static int set_ext_controls(int fd, uint32_t id, uint32_t value);

/**
 * @brief Subscribes to an V4L2 event
 *
 * Calls the \c VIDIOC_SUBSCRIBE_EVENT IOCTL internally
 *
 * @param[in] fd context FD
 * @param[in] type Type of the event
 * @param[in] id ID of the event source
 * @param[in] flags Event flags
 * @return 0 for success, -1 otherwise.
 */
static int subscribe_event(int fd, uint32_t type, uint32_t id, uint32_t flags);
