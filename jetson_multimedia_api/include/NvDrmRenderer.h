/*
 * Copyright (c) 2016-2021, NVIDIA CORPORATION.  All rights reserved.
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

/**
 * @file
 *
 * <b>NVIDIA Multimedia API: DRM Renderer API</b>
 *
 * @b Description: Helper class for rendering using LibDRM.
 */
#ifndef __NV_DRM_RENDERER_H__
#define __NV_DRM_RENDERER_H__

#include "NvElement.h"
#include <stdint.h>
#include <pthread.h>
#include <queue>
#include <unordered_map>

/**
 *
 * @defgroup l4t_mm_nvdrmrenderer_group DRM Renderer API
 *
 * @ingroup aa_framework_api_group
 * @{
 */

/** Holds a buffer object handle. */
typedef struct _NvDrmBO {
    uint32_t bo_handle;  /**< Holds DRM buffer index. */
    int width;           /**< Holds width of the DRM buffer in pixels. */
    int height;          /**< Holds height of the DRM buffer in pixels. */
    int pitch;           /**< Holds stride/pitch of the DRM buffer. */
    uint8_t* data;       /**< Holds mapped CPU accessible address. */
} NvDrmBO;

/** Holds information about the frame. */
typedef struct _NvDrmFB {
    uint32_t fb_id;  /**< Holds the frame ID. */
    int width;       /**< Holds width of the frame in pixels. */
    int height;      /**< Holds height of the frame in pixels. */
    int format;      /**< Holds frame format, such as @c DRM_FORMAT_RGB332.
                          This class supports a subset of the formats defined
                          in @c drm_fourcc.h, the standard DRM header. */
    NvDrmBO bo[4];   /**< Holds DRM buffer handles. */
    int num_buffers; /**< Holds the number of DRM buffers, which depends on
                          the buffer format. */
} NvDrmFB;


/**
 * @brief Helper class for rendering using LibDRM.
 *
 * The renderer requires the file descriptor of a buffer as input. The caller
 * must set the rendering rate in frames per second (FPS).
 *
 * The caller specifies the width, height, connector, and CRTC index.
 * Based on the connector and CRTC index, the renderer finds a suitable encoder
 * and configures the CRTC mode.
 */
class NvDrmRenderer:public NvElement
{
public:
    /**
     * Creates a new DRM based renderer named @a name.
     *
     * @param[in] name Unique name to identity the element instance.
     * @param[in] width Width of the window in pixels.
     * @param[in] height Height of the window in pixels.
     * @param[in] w_x x offset of window location.
     * @param[in] w_y y offset of window location.
     * @param[in] connector Index of connector to use.
     * @param[in] crtc Index of CRTC to use.
     * @param[in] metadata Contains HDR metadata.
     * @param[in] streamHDR Flag indicating that current stream has HDR
     *            metadata, and hence @a metadata is set.
     * @returns Reference to the newly created renderer object if successful,
     *          or NULL if initialization failed.
     */
    static NvDrmRenderer *createDrmRenderer(const char *name, uint32_t width,
                                          uint32_t height, uint32_t w_x, uint32_t w_y,
                                          uint32_t connector, uint32_t crtc,
                                          struct drm_tegra_hdr_metadata_smpte_2086 metadata,
                                          bool streamHDR);
     ~NvDrmRenderer();

    /**
     * Enqueues a buffer file descriptor for rendering.
     *
     * This is a non-blocking call. The function waits for the estimated
     * rendering time of the next buffer. The estimated rendering time is
     * calculated based on the rendering time of the last buffer and the
     * rendering rate.
     *
     * @param[in] fd  File descriptor of the exported buffer to render.
     * @returns 0 if successful, or -1 otherwise.
     */
    int enqueBuffer(int fd);

    /**
     *  Dequeues a previously rendered buffer.
     *
     *  This is blocking function that waits until a free buffer is available.
     *  The renderer retains one buffer, which must not be overwritten
     *  by any other component. This buffer can be used when the renderer
     *  is closed or after sending an EOS to the component.
     *
     *  @returns  File descriptor of the previously rendered buffer.
     */
    int dequeBuffer();

    /**
     * Sets the rendering rate.
     *
     * \warning @a fps may not be set to zero.
     *
     * @param[in] fps Rendering rate in frames per second.
     * @returns 0 if successful, or -1 otherwise.
     */
    int setFPS(float fps);

    /**
     * Enables/disables DRM universal planes client caps,
     * such as @c DRM_CLIENT_CAP_UNIVERSAL_PLANES.
     *
     * @param[in] enable  1 to enable the caps, or 0 to disable.
     * @returns true if successful, or false otherwise.
     */
    bool enableUniversalPlanes(int enable);

    /**
     * Allocates a framebuffer of size (w, h).
     *
     * @post  If the call is successful, the application must remove (free) the
     * framebuffer by calling removeFB().
     *
     * @param[in]  width         Framebuffer width in pixels.
     * @param[in]  height        Framebuffer height in pixels.
     * @param[in]  drm_format    DRM format of @a _NvDrmBO::bo_handle in @a *fb.
     * @param[out] fb            A pointer to an \ref NvDrmFB structure that
     *                           contains the fb_id and the buffer mapping.
     *
     * @return 1 if successful, or 0 otherwise.
     */
    uint32_t createDumbFB(uint32_t width, uint32_t height, uint32_t drm_format, NvDrmFB *fb);

    /**
     * Destroys (frees) a framebuffer previously allocated by createDumbFB().
     *
     * @param fb_id  ID of the framebuffer to be destroyed.
     * @return 0 if the framebuffer is successfully destroyed, or @c -ENOENT
     *         if the framebuffer is not found.
     */
    int removeFB(uint32_t fb_id);


    /**
     * Close GEM (Graphics Execution Manager) handles.
     *
     * @param fd FD of the buffer.
     * @param bo_handle the gem-handle to be closed.
     *
     * @return 1 for success, 0 for failure.
     */
    int drmUtilCloseGemBo(int fd, uint32_t bo_handle);

    /**
     * Changes a plane's framebuffer and position.
     *
     * @note  The @a crtc_... and @a src_... parameters accept the special input
     * value -1, which indicates that the hardware offset value is not to be
     * changed. (Kernel-based DRM drivers return the error code @c -ERANGE when
     * given this value.)
     *
     * @note  All @c %setPlane() operations are synced to vblank and are
     * blocking.
     *
     * @param pl_index Plane index of the plane to be changed.
     * @param fb_id    Framebuffer ID of the framebuffer to display on the
     *                  plane, or -1 to leave the framebuffer unchanged.
     * @param crtc_x   Offset from left of active display region to show plane.
     * @param crtc_y   Offset from top of active display region to show plane.
     * @param crtc_w   Width of output rectangle on display.
     * @param crtc_h   Height of output rectangle on display.
     * @param src_x    Clip offset from left of source framebuffer
     *                  (Q16.16 fixed point).
     * @param src_y    Clip offset from top of source framebuffer
     *                  (Q16.16 fixed point).
     * @param src_w    Width of source rectangle (Q16.16 fixed point).
     * @param src_h    Height of source rectangle (Q16.16 fixed point).
     * @retval 0 if successful.
     * @retval -EINVAL if @a pl_index is invalid.
     * @retval -errno otherwise.
     */
    int setPlane(uint32_t pl_index,
                 uint32_t fb_id,
                 uint32_t crtc_x,
                 uint32_t crtc_y,
                 uint32_t crtc_w,
                 uint32_t crtc_h,
                 uint32_t src_x,
                 uint32_t src_y,
                 uint32_t src_w,
                 uint32_t src_h);

    /**
     * Gets total number of planes available.
     *
     * By default, the count returned includes only "Overlay" type (regular)
     * planes&mdash;not "Primary" and "Cursor" planes. If
     * @c DRM_CLIENT_CAP_UNIVERSAL_PLANES has been enabled with
     * enableUniversalPlanes(), the count returned includes "Primary" and
     * "Cursor" planes as well.
     *
     * @return  Count of total planes available.
     */
    int getPlaneCount();

    /**
     * Gets the plane indexes supported by the given
     * crtc index.
     *
     * @param[in] crtc_index Index of crtc for which the
     * plane indexes to be found.
     * @param[in,out] plane_index Pointer to an array which
     * contains plane indexes. This array should be allocated
     * by the caller for the size of plane count returned
     * by getPlaneCount() API.
     *
     * @return Count of the indexes written in the given array.
     * */
    int getPlaneIndex(uint32_t crtc_index,
                      int32_t* plane_index);

    /**
     * Gets count of available CRTCs.
     *
     * @return  Count of available CRTCs.
     */
    int getCrtcCount();

    /**
     * Gets count of available encoders.
     *
     * @return  Count of available encoders.
     */
    int getEncoderCount();

    /**
     * Checks whether HDR mode is supported on the DRM renderer.
     *
     * @return  True if HDR mode is supported, or false otherwise.
     */
    bool hdrSupported();

    /**
     * Sets the HDR metadata retrieved from the decoder.
     *
     * @return  0 if successful, or -1 otherwise.
     */
    int setHDRMetadataSmpte2086(struct drm_tegra_hdr_metadata_smpte_2086);

private:

    struct timespec last_render_time; /**< Rendering time of the last buffer. */

    int drm_fd;                   /**< File descriptor of opened DRM device. */
    int conn, crtc;
    uint32_t width, height;
    uint32_t drm_conn_id;         /** DRM connector ID. */
    uint32_t drm_enc_id;          /** DRM encoder ID. */
    uint32_t drm_crtc_id;         /** DRM CRTC ID. */
    uint32_t last_fb;
    int activeFd;
    int flippedFd;
    bool flipPending;
    bool renderingStarted;
    bool planeIsSet;

    uint32_t hdrBlobId;
    bool hdrBlobCreated;

    std::queue<int> freeBuffers;
    std::queue<int> pendingBuffers;
    std::unordered_map <int, int> map_list;

    bool stop_thread;   /**< Boolean variable used to signal rendering thread
                             to stop. */
    pthread_t render_thread;        /**< pthread ID of the rendering thread. */

    pthread_mutex_t render_lock;     /**< Used for synchronization. */
    pthread_cond_t render_cond;      /**< Used for synchronization. */
    pthread_mutex_t enqueue_lock;    /**< Used for synchronization. */
    pthread_cond_t enqueue_cond;     /**< Used for synchronization. */
    pthread_mutex_t dequeue_lock;    /**< Used for synchronization. */
    pthread_cond_t dequeue_cond;     /**< Used for synchronization. */

    float fps;                      /**< Rendering rate in frames per second. */
    uint64_t render_time_sec;       /**< Seconds part of the time for which
                                         a frame should be displayed. */
    uint64_t render_time_nsec;      /**< Nanoseconds part of the time for which
                                         a frame should be displayed. */

    /**
     * Constructor called by the wrapper createDrmRenderer().
     *
     * \param[in] name      A pointer to a unique name that identifies
     *                      the element instance.
     * \param[in] width     Width of the window in pixels.
     * \param[in] height    Height of the window in pixels.
     * \param[in] w_x       X coordinate of the window's upper left corner.
     * \param[in] w_y       Y coordinate of the window's upper left corner.
     * \param[in] connector Index of the connector to use.
     * \param[in] crtc      Index of the CRTC to use.
     * \param[in] metadata  A pointer to HDR metadata.
     * \param[in] streamHDR True if the current stream has HDR metadata,
     *                      or false otherwise. If true,
     *                      @a metadata must be set.
     */
    NvDrmRenderer(const char *name, uint32_t width, uint32_t height,
                  uint32_t w_x, uint32_t w_y, uint32_t connector, uint32_t crtc,
                  struct drm_tegra_hdr_metadata_smpte_2086 metadata, bool streamHDR);

    /**
     * Function executed by the renderThread.
     *
     * This function is executed repeatedly until signalled to stop
     * by the @c stop_thread variable. The function contains a while loop
     * which calls renderInternal().
     *
     * \param[in] arg   A pointer to an NvDrmRenderer object.
     */
    static void * renderThread(void *arg);

    /**
     * Callback function registered with libdrm through drmHandleEvent.
     */

    static void page_flip_handler(int fd, unsigned int frame,
                                  unsigned int sec, unsigned int usec, void *data);

    /**
     * Implements the logic of rendering a buffer
     * and waiting until the buffer render time.
     *
     * \param[in] fd    Identifier for the buffer to be rendered.
     */
    int renderInternal(int fd);

    /*
     *  Returns a DRM buffer_object handle.
     *
     * \param[in] w     Width of the window in pixels.
     * \param[in] h     Height of the window in pixels.
     * \param[in] bpp   Bits per pixel in the window.
     * \param[in] bo    A pointer to a buffer object handle.
     * \return  A DRM buffer_object handle allocated of size (w,h).
     */
    int createDumbBO(int w, int h, int bpp, NvDrmBO *bo);

    static const NvElementProfiler::ProfilerField valid_fields =
            NvElementProfiler::PROFILER_FIELD_TOTAL_UNITS |
            NvElementProfiler::PROFILER_FIELD_FPS |
            NvElementProfiler::PROFILER_FIELD_LATE_UNITS;
};

/** @} */
#endif
