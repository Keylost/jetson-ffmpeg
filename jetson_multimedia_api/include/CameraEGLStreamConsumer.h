/*
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef CAMERA_EGL_STREAM_CONSUMER_H
#define CAMERA_EGL_STREAM_CONSUMER_H

#include <EGL/egl.h>
#include <EGL/eglext.h>

enum CONSUMER_STATUS
{
  /// Function succeeded.
  CONSUMER_STATUS_OK,

  /// The set of parameters passed was invalid.
  CONSUMER_STATUS_INVALID_PARAMS,

  /// The requested settings are invalid.
  CONSUMER_STATUS_INVALID_SETTINGS,

  /// The requested device is unavailable.
  CONSUMER_STATUS_UNAVAILABLE,

  /// An operation failed because of insufficient available memory.
  CONSUMER_STATUS_OUT_OF_MEMORY,

  /// This method has not been implemented.
  CONSUMER_STATUS_UNIMPLEMENTED,

  /// An operation timed out.
  CONSUMER_STATUS_TIMEOUT,

  /// The capture was aborted. @see ICaptureSession::cancelRequests()
  CONSUMER_STATUS_CANCELLED,

  /// The stream or other resource has been disconnected.
  CONSUMER_STATUS_DISCONNECTED,

  // Number of elements in this enum.
  CONSUMER_STATUS_COUNT
};

/**
 * A Consumer object maintains a consumer connection to an EGLStream and is
 * used to acquire and release dmabuf Fd from the stream.
 *
 * Destroying a Consumer will implicitly disconnect the stream and release any
 * pending or acquired frames, invalidating any currently acquired dmabuf Fd.
 */
class CameraEGLStreamConsumer
{
public:
  /**
   * Creates a new Consumer object. The returned Consumer will have the default state
   * which can then be reconfigured using the various interfaces and settings methods
   * before it is explicitly connected to the EGLStream using connect().
   *
   * @param[out] status An optional pointer to return an error status code.
   *
   * @returns A new Consumer object, or NULL on error.
   */
  static CameraEGLStreamConsumer* create(CONSUMER_STATUS* status = NULL);

  /**
   * Sets the maximum number of frames that can be simultaneously acquired by the
   * consumer at any point in time. The default is 1.
   *
   * @param[in] maxFrames The maximum number of frames that can be acquired.
   *
   * @return Success/error code of the call.
   */
  virtual CONSUMER_STATUS setMaxAcquiredFrames(uint32_t maxFrames) = 0;

  /** @} */ // End of PreConnect methods.

  /**
   * \defgroup ConnectionState EGLStream connection state methods.
   * @{
   */

  /**
   * Connects the Consumer to an EGLStream.
   *
   * @param[in] eglDisplay The EGLDisplay the stream belongs to.
   * @param[in] eglStream The EGLStream to connect the consumer to.
   *
   * @return Success/error code of the call.
   */
  virtual CONSUMER_STATUS connect(EGLDisplay eglDisplay, EGLStreamKHR eglStream) = 0;

  /**
   * Disconnects the consumer from the EGLStream. This will notify the
   * producer endpoint of the disconnect and will prevent new frames from
   * being presented to the stream by the producer. It will also prevent new
   * frames from being acquired, but any currently acquired frames will still
   * remain valid until released or until the consumer is destroyed.
   */
  virtual void disconnect() = 0;

  /**
   *  Destroy the Consumer object. Destroying a Consumer will implicitly disconnect
   *  the stream and release any pending or acquired frames, invalidating any
   *  currently acquired dmabuf Fd.
   */

  virtual void destroy() = 0;

  /** @} */ // End of ConnectionState methods.

  /**
   * \defgroup Connected Methods available while the stream is connected.
   *
   * These methods can only be called once both the Consumer and Producer
   * have successfully connected to the EGLStream and it is in the
   * CONNECTED state. Calling any of these function when the stream is not
   * in the CONNECTED state will return an INVALID_STATE status.
   * @{
   */

  /**
   * Acquires a new dmabuf Fd. If the maximum number of fds are currently acquired,
   * an error will be returned immediately. If -1 is returned and the status is
   * DISCONNECTED, the producer has disconnected from the stream and no more fds
   * can be acquired.
   * @param[in] timeout The timeout to wait for a frame if one isn't available.
   * @param[out] status An optional pointer to return an error status code.
   *
   * @returns dmabuf Fd of a frame acquired from the stream, or -1 on error.
   *          This dmabuf object is owned by the Consumer, and is valid until it is
   *          released by releaseFd() or is implicitly released by destroy().
   */
  virtual int acquireFd(uint64_t timeout = 0xFFFFFFFFFFFFFFFF,
                        CONSUMER_STATUS* status = NULL) = 0;

  /**
   * Releases an acquired dmabuf .
   * @param[in] fd The dmabuf fd to release.
   *
   * @return Success/error code of the call.
   */
  virtual CONSUMER_STATUS releaseFd(int fd) = 0;

protected:
  ~CameraEGLStreamConsumer() {}
};

#endif // CAMERA_EGL_STREAM_CONSUMER_H
