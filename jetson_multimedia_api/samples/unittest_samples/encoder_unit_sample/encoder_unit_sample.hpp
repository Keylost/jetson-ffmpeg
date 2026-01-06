/*
 * Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
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

/**
 * Specifies the encoder device node.
 */
#define ENCODER_DEV "/dev/nvhost-msenc"
/**
 * Specifies the maximum number of planes a buffer can contain.
 */
#define MAX_PLANES 3

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
 * @brief Struct defining the encoder context.
 * The video encoder device node is `/dev/nvhost-msenc`. The category name
 * for the encoder is \c "NVENC".
 *
 * The context stores the information for encoding.
 * Refer to [V4L2 Video Encoder](group__V4L2Enc.html) for more information on the encoder.
 */
typedef struct
{
    uint32_t encode_pixfmt;
    uint32_t raw_pixfmt;
    uint32_t width;
    uint32_t height;
    uint32_t capplane_num_planes;
    uint32_t outplane_num_planes;
    uint32_t capplane_num_buffers;
    uint32_t outplane_num_buffers;

    uint32_t num_queued_outplane_buffers;
    uint32_t num_queued_capplane_buffers;

    enum v4l2_memory outplane_mem_type;
    enum v4l2_memory capplane_mem_type;
    enum v4l2_buf_type outplane_buf_type;
    enum v4l2_buf_type capplane_buf_type;
    Buffer::BufferPlaneFormat outplane_planefmts[MAX_PLANES];
    Buffer::BufferPlaneFormat capplane_planefmts[MAX_PLANES];

    Buffer **outplane_buffers;
    Buffer **capplane_buffers;

    string input_file_path;
    ifstream *input_file;

    string output_file_path;
    ofstream *output_file;

    pthread_mutex_t queue_lock;
    pthread_cond_t queue_cond;
    pthread_t enc_dq_thread;

    bool in_error;
    bool eos;
    bool dqthread_running;
    bool outplane_streamon;
    bool capplane_streamon;
    int fd;
} context_t;

/**
 * @brief Reads the raw data from a file to the buffer structure.
 *
 * Helper function to read a raw frame.
 * This function reads data from the file into the buffer plane-by-plane
 * while taking care of the stride of the plane.
 *
 * @param stream : Input stream
 * @param buffer : Buffer class pointer
 */
static int read_video_frame(ifstream * stream, Buffer & buffer);

/**
 * @brief Writes an elementary encoded frame from the buffer to a file.
 *
 * This function writes data into the file from the buffer plane-by-plane.
 *
 * @param[in] stream A pointer to the output file stream.
 * @param[in] buffer Buffer class pointer
 * @return 0 for success, -1 otherwise.
 */
static int write_encoded_frame(ofstream * stream, Buffer * buffer);

/**
 * @brief Sets the format on the encoder capture plane.
 *
 * Calls the \c VIDIOC_S_FMT IOCTL internally on the capture plane.
 *
 * @param[in] ctx Reference to the encoder context struct created.
 * @param[in] sizeimage Size of the encoded bitstream
 * @return 0 for success, -1 otherwise.
 */
static int set_capture_plane_format(context_t& ctx, uint32_t sizeimage);

/**
 * @brief Sets the format on the encoder output plane.
 *
 * Calls the \c VIDIOC_S_FMT IOCTL internally on the output plane.
 *
 * @param[in] ctx Reference to the encoder context struct created.
 * @return 0 for success, -1 otherwise.
 */
static int set_output_plane_format(context_t& ctx);

/**
 * @brief Requests for buffers on the encoder capture plane.
 *
 * Calls the \c VIDIOC_REQBUFS IOCTL internally on the capture plane.
 *
 * @param[in] ctx Pointer to the encoder context struct created.
 * @param[in] buf_type Type of buffer, one of the enum v4l2_buf_type.
 * @param[in] mem_type Memory type of the plane, one of the
 *                     enum v4l2_memory mem_type, here V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE
 * @param[in] num_buffers Number of buffers to be requested.
 * @return 0 for success, -1 otherwise.
 */
static int req_buffers_on_capture_plane(context_t * ctx, enum v4l2_buf_type buf_type,
		enum v4l2_memory mem_type, int num_buffers);

/**
 * @brief Requests for buffers on the encoder output plane.
 *
 * Calls the \c VIDIOC_REQBUFS IOCTL internally on the output plane.
 *
 * @param[in] ctx Pointer to the encoder context struct created.
 * @param[in] buf_type Type of buffer, one of the enum v4l2_buf_type.
 * @param[in] mem_type Memory type of the plane, one of the
 *                     enum v4l2_memory mem_type, here V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE
 * @param[in] num_buffers Number of buffers to be requested.
 * @return 0 for success, -1 otherwise.
 */
static int req_buffers_on_output_plane (context_t * ctx, enum v4l2_buf_type buf_type,
		enum v4l2_memory mem_type, int num_buffers);

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

/**
 * @brief Queues a buffer on the plane.
 *
 * This method calls \c VIDIOC_QBUF IOCTL internally.
 *
 * @param[in] ctx Pointer to the encoder context struct created.
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
 * @brief Dequeues a buffer from the plane.
 *
 * This method calls \c VIDIOC_DQBUF IOCTL internally.
 * This is a blocking call. This call returns when a buffer is successfully
 * dequeued or timeout is reached. If the buffer is not NULL, returns the
 * Buffer object at the index returned by VIDIOC_DQBUF IOCTL
 *
 * @param[in] ctx Pointer to the encoder context struct created.
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
 * @brief Callback function when enc_cap_thread is created.
 *
 * This is a callback function of the capture loop thread created.
 * The function runs until signaled to stop, or error
 * is encountered. On successful dequeue of a buffer from the plane,
 * the method calls capture_plane_callback.
 *
 * Setting the stream to off automatically stops the thread.
 *
 * @param[in] arg A pointer to the application data.
 */
static void * dq_thread(void *arg);

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
static bool
capture_plane_callback(struct v4l2_buffer *v4l2_buf, Buffer * buffer, void *arg);

/**
 * @brief Waits for the DQ Thread to stop.
 *
 * This method waits until the DQ Thread stops or timeout is reached.
 *
 * @sa dq_thread
 *
 * @param[in] ctx Reference to the encoder context struct created.
 * @param[in] max_wait_ms Maximum wait time, in milliseconds.
 * @return 0 for success, -1 otherwise.
 */
static int wait_for_dqthread(context_t& ctx, uint32_t max_wait_ms);

/**
 * @brief Encode processing function for blocking mode.
 *
 * Function loop to DQ and EnQ buffers on output plane
 * till eos is signalled
 *
 * @param[in] ctx Reference to the encoder context struct created.
 * @return If the application implementing this call returns TRUE,
 *         EOS is detected by the encoder and all the buffers are dequeued;
 *         else the encode process continues running.
 */
static int encoder_process_blocking(context_t& ctx);
