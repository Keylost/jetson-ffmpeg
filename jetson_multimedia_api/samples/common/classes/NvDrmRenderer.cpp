/*
 * Copyright (c) 2016-2021, NVIDIA CORPORATION.  All rights reserved.
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "NvDrmRenderer.h"
#include "NvLogging.h"
#include "nvbuf_utils.h"

#include <sys/time.h>
#include <sys/poll.h>
#include <unistd.h>
#include <string.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>
#include "tegra_drm.h"
#ifndef DOWNSTREAM_TEGRA_DRM
#include "tegra_drm_nvdc.h"
#endif

using namespace std;

#define CAT_NAME "DrmRenderer"
#define DRM_DEVICE_NAME "drm-nvdc"
#define ZERO_FD 0x0

struct NvBufDrmParams
{
  uint32_t num_planes;
  uint32_t pitch[4];
  uint32_t offset[4];
  uint32_t pixel_format;
};

struct NvBOFormat {
  uint32_t drm_format;
  int num_buffers;
  struct {
    int w;  // width divisor from overall fb_width (luma size)
    int h;  // height divisor from overall fb_height (luma size)
    int bpp;
  } buffers[3];
};

const NvBOFormat NvBOFormats[] = {
    // drm fourcc type     #buffers  w1 h1 bpp1   w2 h2 bpp2  w3 h3 bpp3
    {DRM_FORMAT_RGB332,    1,      {{1, 1, 8},   {0, 0, 0},  {0, 0, 0}}},
    {DRM_FORMAT_BGR233,    1,      {{1, 1, 8},   {0, 0, 0},  {0, 0, 0}}},

    {DRM_FORMAT_XRGB4444,  1,      {{1, 1, 16},  {0, 0, 0},  {0, 0, 0}}},
    {DRM_FORMAT_ARGB4444,  1,      {{1, 1, 16},  {0, 0, 0},  {0, 0, 0}}},
    {DRM_FORMAT_XBGR4444,  1,      {{1, 1, 16},  {0, 0, 0},  {0, 0, 0}}},
    {DRM_FORMAT_ABGR4444,  1,      {{1, 1, 16},  {0, 0, 0},  {0, 0, 0}}},
    {DRM_FORMAT_RGBX4444,  1,      {{1, 1, 16},  {0, 0, 0},  {0, 0, 0}}},
    {DRM_FORMAT_RGBA4444,  1,      {{1, 1, 16},  {0, 0, 0},  {0, 0, 0}}},
    {DRM_FORMAT_BGRX4444,  1,      {{1, 1, 16},  {0, 0, 0},  {0, 0, 0}}},
    {DRM_FORMAT_BGRA4444,  1,      {{1, 1, 16},  {0, 0, 0},  {0, 0, 0}}},
    {DRM_FORMAT_XRGB1555,  1,      {{1, 1, 16},  {0, 0, 0},  {0, 0, 0}}},
    {DRM_FORMAT_ARGB1555,  1,      {{1, 1, 16},  {0, 0, 0},  {0, 0, 0}}},
    {DRM_FORMAT_XBGR1555,  1,      {{1, 1, 16},  {0, 0, 0},  {0, 0, 0}}},
    {DRM_FORMAT_ABGR1555,  1,      {{1, 1, 16},  {0, 0, 0},  {0, 0, 0}}},
    {DRM_FORMAT_RGBX5551,  1,      {{1, 1, 16},  {0, 0, 0},  {0, 0, 0}}},
    {DRM_FORMAT_RGBA5551,  1,      {{1, 1, 16},  {0, 0, 0},  {0, 0, 0}}},
    {DRM_FORMAT_BGRX5551,  1,      {{1, 1, 16},  {0, 0, 0},  {0, 0, 0}}},
    {DRM_FORMAT_BGRA5551,  1,      {{1, 1, 16},  {0, 0, 0},  {0, 0, 0}}},
    {DRM_FORMAT_RGB565,    1,      {{1, 1, 16},  {0, 0, 0},  {0, 0, 0}}},
    {DRM_FORMAT_BGR565,    1,      {{1, 1, 16},  {0, 0, 0},  {0, 0, 0}}},

    {DRM_FORMAT_RGB888,    1,      {{1, 1, 24},  {0, 0, 0},  {0, 0, 0}}},
    {DRM_FORMAT_BGR888,    1,      {{1, 1, 24},  {0, 0, 0},  {0, 0, 0}}},

    {DRM_FORMAT_XRGB8888,  1,      {{1, 1, 32},  {0, 0, 0},  {0, 0, 0}}},
    {DRM_FORMAT_ARGB8888,  1,      {{1, 1, 32},  {0, 0, 0},  {0, 0, 0}}},
    {DRM_FORMAT_XBGR8888,  1,      {{1, 1, 32},  {0, 0, 0},  {0, 0, 0}}},
    {DRM_FORMAT_ABGR8888,  1,      {{1, 1, 32},  {0, 0, 0},  {0, 0, 0}}},
    {DRM_FORMAT_RGBX8888,  1,      {{1, 1, 32},  {0, 0, 0},  {0, 0, 0}}},
    {DRM_FORMAT_RGBA8888,  1,      {{1, 1, 32},  {0, 0, 0},  {0, 0, 0}}},
    {DRM_FORMAT_BGRX8888,  1,      {{1, 1, 32},  {0, 0, 0},  {0, 0, 0}}},
    {DRM_FORMAT_BGRA8888,  1,      {{1, 1, 32},  {0, 0, 0},  {0, 0, 0}}},

    {DRM_FORMAT_ARGB2101010,1,     {{1, 1, 32},  {0, 0, 0},  {0, 0, 0}}},
    {DRM_FORMAT_ABGR2101010,1,     {{1, 1, 32},  {0, 0, 0},  {0, 0, 0}}},

    {DRM_FORMAT_YUYV,      1,      {{1, 1, 16},  {0, 0, 0},  {0, 0, 0}}},
    {DRM_FORMAT_YVYU,      1,      {{1, 1, 16},  {0, 0, 0},  {0, 0, 0}}},
    {DRM_FORMAT_UYVY,      1,      {{1, 1, 16},  {0, 0, 0},  {0, 0, 0}}},
    {DRM_FORMAT_VYUY,      1,      {{1, 1, 16},  {0, 0, 0},  {0, 0, 0}}},

    {DRM_FORMAT_NV12,      2,      {{1, 1, 8},   {2, 2, 16}, {0, 0, 0}}},
    {DRM_FORMAT_NV21,      2,      {{1, 1, 8},   {2, 2, 16}, {0, 0, 0}}},
    {DRM_FORMAT_NV16,      2,      {{1, 1, 8},   {2, 1, 16}, {0, 0, 0}}},
    {DRM_FORMAT_NV61,      2,      {{1, 1, 8},   {2, 1, 16}, {0, 0, 0}}},
    {DRM_FORMAT_YUV410,    3,      {{1, 1, 8},   {4, 4, 8},  {4, 4, 8}}},
    {DRM_FORMAT_YVU410,    3,      {{1, 1, 8},   {4, 4, 8},  {4, 4, 8}}},
    {DRM_FORMAT_YUV411,    3,      {{1, 1, 8},   {4, 1, 8},  {4, 1, 8}}},
    {DRM_FORMAT_YVU411,    3,      {{1, 1, 8},   {4, 1, 8},  {4, 1, 8}}},
    {DRM_FORMAT_YUV420,    3,      {{1, 1, 8},   {2, 2, 8},  {2, 2, 8}}},
    {DRM_FORMAT_YVU420,    3,      {{1, 1, 8},   {2, 2, 8},  {2, 2, 8}}},
    {DRM_FORMAT_YUV422,    3,      {{1, 1, 8},   {2, 1, 8},  {2, 1, 8}}},
    {DRM_FORMAT_YVU422,    3,      {{1, 1, 8},   {2, 1, 8},  {2, 1, 8}}},
    {DRM_FORMAT_YUV444,    3,      {{1, 1, 8},   {1, 1, 8},  {1, 1, 8}}},
    {DRM_FORMAT_YVU444,    3,      {{1, 1, 8},   {1, 1, 8},  {1, 1, 8}}},
};

static int NvBufGetDrmParams(NvBufferParams *params, NvBufDrmParams *dParams)
{
  unsigned int i;

  if (params == NULL || dParams == NULL)
    goto error;

  memset(dParams, 0 , sizeof(NvBufDrmParams));

  dParams->num_planes = params->num_planes;
  for (i = 0; i < params->num_planes; i++) {
    dParams->pitch[i] = params->pitch[i];
    dParams->offset[i] = params->offset[i];
  }

  switch (params->pixel_format) {
    case NvBufferColorFormat_YUV420:
      dParams->pixel_format = DRM_FORMAT_YUV420;
      break;
    case NvBufferColorFormat_YVU420:
      dParams->pixel_format = DRM_FORMAT_YVU420;
      break;
    case NvBufferColorFormat_NV12:
      dParams->pixel_format = DRM_FORMAT_NV12;
      break;
    case NvBufferColorFormat_NV21:
      dParams->pixel_format = DRM_FORMAT_NV21;
      break;
    case NvBufferColorFormat_UYVY:
      dParams->pixel_format = DRM_FORMAT_UYVY;
      break;
    case NvBufferColorFormat_NV12_10LE_2020:
      dParams->pixel_format = DRM_FORMAT_TEGRA_P010_2020;
      break;
    case NvBufferColorFormat_NV12_10LE_709:
      dParams->pixel_format = DRM_FORMAT_TEGRA_P010_709;
      break;
    case NvBufferColorFormat_NV12_10LE:
      dParams->pixel_format = DRM_FORMAT_P010;
      break;
    case NvBufferColorFormat_Invalid:
    default:
      goto error;
  }
  return 0;

  error:
  ERROR_MSG("Error in transforming buffer information ");
  return -1;
}

static int get_format_info(uint32_t drm_format, NvBOFormat *bo)
{
  unsigned int i;
  for (i = 0; i < sizeof(NvBOFormats) / sizeof(NvBOFormats[0]); i++) {
    if (NvBOFormats[i].drm_format == drm_format) {
      *bo = NvBOFormats[i];
      return 1;
    }
  }
  return 0;
}

NvDrmRenderer::NvDrmRenderer(const char *name, uint32_t w, uint32_t h,
                    uint32_t w_x, uint32_t w_y, uint32_t aconn, uint32_t acrtc,
                    struct drm_tegra_hdr_metadata_smpte_2086 metadata,
                    bool streamHDR)
        :NvElement(name, valid_fields)
{
  drmModeRes* drm_res_info = NULL;
  drmModeConnector* drm_conn_info = NULL;
  drmModeEncoder* drm_enc_info = NULL;
  drmModeCrtc* drm_crtc_info = NULL;
  uint32_t crtc_mask;
  int i;
  conn = aconn;
  crtc = acrtc;
  width = w;
  height = h;
  stop_thread = false;
  flipPending = false;
  renderingStarted = false;
  planeIsSet = false;
  activeFd = flippedFd = -1;
  last_fb = 0;
  int ret =0;
  log_level = LOG_LEVEL_ERROR;
  last_render_time.tv_sec = 0;

  drm_fd = drmOpen(DRM_DEVICE_NAME, NULL);
  if (drm_fd == -1) {
    COMP_ERROR_MSG("Couldn't open device: " << DRM_DEVICE_NAME);
    goto error;
  }
  // Obtain DRM-KMS resources
  drm_res_info = drmModeGetResources(drm_fd);
  if (!drm_res_info) {
    COMP_ERROR_MSG("Couldn't obtain DRM-KMS resources ");
    goto error;
  }
  COMP_DEBUG_MSG("Obtained device information ");

  // If a specific crtc was requested, make sure it exists
  if (crtc >= drm_res_info->count_crtcs) {
    COMP_ERROR_MSG("Requested crtc index " << crtc << " exceeds count " << drm_res_info->count_crtcs);
    goto error;
  }
  crtc_mask = (crtc >= 0) ? (1<<crtc) : ((1<<drm_res_info->count_crtcs)-1);

  // Query info for requested connector
  if (conn >= drm_res_info->count_connectors) {
    COMP_ERROR_MSG("Requested connector index " << conn << " exceeds count " << drm_res_info->count_connectors);
    goto error;
  }

  drm_conn_id = drm_res_info->connectors[conn];
  drm_conn_info = drmModeGetConnector(drm_fd, drm_conn_id);
  if (!drm_conn_info) {
    COMP_ERROR_MSG("Unable to obtain info for connector " << drm_conn_id);
    goto error;
  } else if (drm_conn_info->connection != DRM_MODE_CONNECTED) {
    COMP_ERROR_MSG("Requested connnector is not connected ");
    goto error;
  } else if (drm_conn_info->count_modes <= 0) {
    COMP_ERROR_MSG("Requested connnector has no available modes ");
    goto error;
  }
  COMP_DEBUG_MSG("Obtained connector information\n");

  // If there is already an encoder attached to the connector, choose
  //   it unless not compatible with crtc/plane
  drm_enc_id = drm_conn_info->encoder_id;
  drm_enc_info = drmModeGetEncoder(drm_fd, drm_enc_id);
  if (drm_enc_info) {
    if (!(drm_enc_info->possible_crtcs & crtc_mask)) {
      drmModeFreeEncoder(drm_enc_info);
      drm_enc_info = NULL;
    }
  }

  // If we didn't have a suitable encoder, find one
  if (!drm_enc_info) {
    for (i=0; i<drm_conn_info->count_encoders; ++i) {
      drm_enc_id = drm_conn_info->encoders[i];
      drm_enc_info = drmModeGetEncoder(drm_fd, drm_enc_id);
      if (drm_enc_info) {
        if (crtc_mask & drm_enc_info->possible_crtcs) {
          crtc_mask &= drm_enc_info->possible_crtcs;
          break;
        }
        drmModeFreeEncoder(drm_enc_info);
        drm_enc_info = NULL;
      }
    }
    if (i == drm_conn_info->count_encoders) {
      COMP_ERROR_MSG("Unable to find suitable encoder ");
      goto error;
    }
  }
  COMP_DEBUG_MSG("Obtained encoder information ");

  // Select a suitable crtc. Give preference to one that's already
  //   attached to the encoder.
  for (i=0; i<drm_res_info->count_crtcs; ++i) {
    if (crtc_mask & (1 << i)) {
      drm_crtc_id = drm_res_info->crtcs[i];
      if (drm_enc_info && drm_res_info->crtcs[i] == drm_enc_info->crtc_id) {
        break;
      }
    }
  }

  if (hdrSupported()) {
    ret = setHDRMetadataSmpte2086(metadata);
    if(ret!=0)
      COMP_DEBUG_MSG("Error while getting HDR mastering display data\n");
  }
  else {
    COMP_DEBUG_MSG("APP_INFO : HDR not supported \n");
  }
  // Query info for crtc
  drm_crtc_info = drmModeGetCrtc(drm_fd, drm_crtc_id);
  if (!drm_crtc_info) {
    COMP_ERROR_MSG("Unable to obtain info for crtc " << drm_crtc_id);
    goto error;
  }

  COMP_DEBUG_MSG("Obtained crtc information\n");

#if 0
  if ((drm_conn_info->encoder_id != drm_enc_id) ||
      (drm_enc_info->crtc_id != drm_crtc_id) ||
      !drm_crtc_info->mode_valid) {

    drmModeSetCrtc(drm_fd, drm_crtc_id, -1, 0, 0, &drm_conn_id, 1, NULL);
  }
#endif

  drmModeSetCrtc(drm_fd, drm_crtc_id, -1, w_x, w_y, &drm_conn_id, 1, drm_conn_info->modes);

  pthread_mutex_init(&enqueue_lock, NULL);
  pthread_cond_init(&enqueue_cond, NULL);
  pthread_mutex_init(&dequeue_lock, NULL);
  pthread_mutex_init(&render_lock, NULL);
  pthread_cond_init(&render_cond, NULL);
  pthread_cond_init(&dequeue_cond, NULL);

  setFPS(30);

  pthread_create(&render_thread, NULL, renderThread, this);
  pthread_setname_np(render_thread, "DrmRenderer");


error_crtc:
  drmModeFreeCrtc(drm_crtc_info);

error_enc:
  drmModeFreeEncoder(drm_enc_info);

error_conn:
  drmModeFreeConnector(drm_conn_info);

error_res:
  drmModeFreeResources(drm_res_info);
  return;

error:
  is_in_error = 1;

  if (drm_fd != -1)
      drmClose(drm_fd);

  if (drm_crtc_info)
    goto error_crtc;

  if (drm_enc_info)
    goto error_enc;

  if (drm_conn_info)
    goto error_conn;

  if (drm_res_info)
    goto error_res;

  return;
}

int
NvDrmRenderer::drmUtilCloseGemBo (int fd, uint32_t bo_handle)
{
  struct drm_gem_close gemCloseArgs;

  memset (&gemCloseArgs, 0, sizeof (gemCloseArgs));
  gemCloseArgs.handle = bo_handle;
  int ret = drmIoctl (fd, DRM_IOCTL_GEM_CLOSE, &gemCloseArgs);
  if (ret < 0) {
    cout << "Failed to close gem buffer\n" << endl;
    return 0;
  }
  return 1;
}

void NvDrmRenderer::page_flip_handler(int drm_fd, unsigned int frame,
                                      unsigned int sec, unsigned int usec, void *data)
{
  NvDrmRenderer *renderer = (NvDrmRenderer *) data;
  int fd;
  int ret;

  pthread_mutex_lock(&renderer->dequeue_lock);
  if (renderer->activeFd != -1) {
    renderer->freeBuffers.push(renderer->activeFd);
    pthread_cond_signal(&renderer->dequeue_cond);
  }
  renderer->activeFd = renderer->flippedFd;
  pthread_mutex_unlock(&renderer->dequeue_lock);

  pthread_mutex_lock(&renderer->enqueue_lock);
  if (renderer->pendingBuffers.empty()) {
    renderer->flipPending = false;
    pthread_mutex_unlock(&renderer->enqueue_lock);
    return;
  } else {
    fd = (int)renderer->pendingBuffers.front();
    renderer->pendingBuffers.pop();

    if (fd == -1) {
      // drmModeSetCrtc with a ZERO FD will walk through the path that
      // disable the windows.
      // Note: drmModePageFlip doesn't support this trick.
      ret = drmModeSetCrtc(drm_fd, renderer->drm_crtc_id,
              ZERO_FD, 0, 0, &renderer->drm_conn_id, 1, NULL);
      if (ret) {
        std::cout << "Failed to disable windows before exiting" << std::endl;
        pthread_mutex_unlock(&renderer->enqueue_lock);
        return;
      }

      // EOS buffer. Release last buffer held.
      renderer->stop_thread = true;
      pthread_mutex_lock(&renderer->dequeue_lock);
      renderer->freeBuffers.push(renderer->activeFd);
      pthread_cond_signal(&renderer->dequeue_cond);
      pthread_mutex_unlock(&renderer->dequeue_lock);

      renderer->flipPending = false;
      pthread_mutex_unlock(&renderer->enqueue_lock);
      return;
    }
    pthread_mutex_unlock(&renderer->enqueue_lock);
    renderer->renderInternal(fd);
  }
}

void *
NvDrmRenderer::renderThread(void *arg)
{
  NvDrmRenderer *renderer = (NvDrmRenderer *) arg;
  drmEventContext evctx;
  struct pollfd fds;
  int ret;
  int timeout = 500; // 500ms

  memset(&fds, 0, sizeof(fds));
  fds.fd = renderer->drm_fd;
  fds.events = POLLIN;

  pthread_mutex_lock(&renderer->enqueue_lock);
  while (renderer->pendingBuffers.empty()) {
    if (renderer->stop_thread) {
      pthread_mutex_unlock(&renderer->enqueue_lock);
      return NULL;
    }
    pthread_cond_wait(&renderer->enqueue_cond, &renderer->enqueue_lock);
  }

  int fd = (int)renderer->pendingBuffers.front();
  renderer->pendingBuffers.pop();
  pthread_mutex_unlock(&renderer->enqueue_lock);

  ret = renderer->renderInternal(fd);
  if (ret < 0) {
    renderer->is_in_error = 1;
    return NULL;
  }
  renderer->renderingStarted = true;

  while (!renderer->isInError() || !renderer->stop_thread) {
    ret = poll(&fds, 1, timeout);
    if (ret > 0) {
      if (fds.revents & POLLIN) {
        memset(&evctx, 0, sizeof evctx);
        evctx.version = DRM_EVENT_CONTEXT_VERSION;
        evctx.page_flip_handler = page_flip_handler;
        drmHandleEvent(renderer->drm_fd, &evctx);
      }
    } else if (ret < 0) {
      renderer->is_in_error = 1;
      return NULL;
    } else {
      // Timeout
      return NULL;
    }
  }
  return NULL;
}

bool NvDrmRenderer::hdrSupported()
{
    uint32_t i;
    bool hdr_supported = 0;
    drmModeObjectProperties *props;
    drmModePropertyRes **props_info;

    props = drmModeObjectGetProperties(drm_fd, drm_crtc_id, DRM_MODE_OBJECT_CRTC);
    props_info = (drmModePropertyRes **) calloc(props->count_props, sizeof(props_info));
    for (i = 0; i < props->count_props; i++) {
        props_info[i] = drmModeGetProperty(drm_fd, props->props[i]);
    }

    for (i = 0; i < props->count_props; i++) {
        if (strcmp(props_info[i]->name, "HDR_SUPPORTED") == 0) {
            hdr_supported = props_info[i]->values[0];
            break;
        }
    }

    drmModeFreeObjectProperties(props);
    drmModeFreeProperty(*props_info);

    return hdr_supported;
}

int NvDrmRenderer::setHDRMetadataSmpte2086(struct drm_tegra_hdr_metadata_smpte_2086 metadata)
{
    int prop_id = -1;
    uint32_t i;
    drmModeObjectProperties *props;
    drmModePropertyRes **props_info;

    if (hdrBlobCreated) {
        drmModeDestroyPropertyBlob(drm_fd, hdrBlobId);
        hdrBlobCreated = 0;
    }

    if (drmModeCreatePropertyBlob(drm_fd, &metadata, sizeof(metadata), &hdrBlobId) != 0) {
        return -1;
    }

    hdrBlobCreated = 1;

    props = drmModeObjectGetProperties(drm_fd, drm_crtc_id, DRM_MODE_OBJECT_CRTC);
    props_info = (drmModePropertyRes **) calloc(props->count_props, sizeof(props_info));
    for (i = 0; i < props->count_props; i++) {
        props_info[i] = drmModeGetProperty(drm_fd, props->props[i]);
    }

    for (i = 0; i < props->count_props; i++) {
        if (strcmp(props_info[i]->name, "HDR_METADATA_SMPTE_2086_ID") == 0) {
            prop_id = props_info[i]->prop_id;
            break;
        }
    }

    if (prop_id < 0) {
        return -1;
    }

    return drmModeObjectSetProperty(drm_fd, drm_crtc_id, DRM_MODE_OBJECT_CRTC, prop_id, hdrBlobId);
}

NvDrmRenderer::~NvDrmRenderer()
{
  uint32_t fb;

  stop_thread = true;
  pthread_mutex_lock(&enqueue_lock);
  pthread_cond_broadcast(&enqueue_cond);
  pthread_mutex_unlock(&enqueue_lock);
  pthread_join(render_thread, NULL);
  pthread_mutex_destroy(&enqueue_lock);
  pthread_cond_destroy(&enqueue_cond);

  pthread_mutex_lock(&dequeue_lock);
  pthread_cond_broadcast(&dequeue_cond);
  pthread_mutex_unlock(&dequeue_lock);
  pthread_mutex_destroy(&dequeue_lock);
  pthread_cond_destroy(&dequeue_cond);
  pthread_mutex_destroy(&render_lock);
  pthread_cond_destroy(&render_cond);

  for (auto map_entry = map_list.begin();
      map_entry != map_list.end(); ++map_entry) {
    fb = (uint32_t) map_entry->second;
    drmModeRmFB(drm_fd, fb);
  }

  if(last_fb)
    drmModeRmFB(drm_fd, last_fb);

  if (hdrBlobCreated) {
      drmModeDestroyPropertyBlob(drm_fd, hdrBlobId);
      hdrBlobCreated = 0;
  }

  if (drm_fd != -1)
    drmClose(drm_fd);
}

int
NvDrmRenderer::dequeBuffer()
{
  int fd = -1;

//  if (stop_thread)
//    return fd;

//  usleep(15000);

  pthread_mutex_lock(&dequeue_lock);
  while (freeBuffers.empty()) {
    if (stop_thread) {
      pthread_mutex_unlock(&dequeue_lock);
      return fd;
    }
    pthread_cond_wait (&dequeue_cond, &dequeue_lock);
  }

  fd = (int) freeBuffers.front();
  freeBuffers.pop();
  pthread_mutex_unlock(&dequeue_lock);

  return fd;
}

int
NvDrmRenderer::enqueBuffer(int fd)
{
  int ret = -1;
  int tmpFd;

  if (is_in_error)
    return ret;

  pthread_mutex_lock(&enqueue_lock);
  pendingBuffers.push(fd);

  if (renderingStarted && !flipPending) {
    tmpFd = (int) pendingBuffers.front();
    pendingBuffers.pop();

    if (tmpFd == -1) {
      // drmModeSetCrtc with a ZERO FD will walk through the path that
      // disable the windows.
      // Note: drmModePageFlip doesn't support this trick.
      ret = drmModeSetCrtc(drm_fd, drm_crtc_id,
              ZERO_FD, 0, 0, &drm_conn_id, 1, NULL);
      if (ret) {
        COMP_ERROR_MSG("Failed to disable windows before exiting ");
        pthread_mutex_unlock(&enqueue_lock);
        return ret;
      }

      // This is EOS and it is assumed to be last buffer.
      // No buffer will be processed after this.
      // Release last buffer held.
      stop_thread = true;
      pthread_mutex_lock(&dequeue_lock);
      if (activeFd != -1)
        freeBuffers.push(activeFd);
      pthread_cond_signal(&dequeue_cond);
      pthread_mutex_unlock(&dequeue_lock);

      pthread_mutex_unlock(&enqueue_lock);
      return 0;
    }
    pthread_mutex_unlock(&enqueue_lock);
    ret = renderInternal(tmpFd);
  } else {
    ret = 0;
    pthread_cond_signal(&enqueue_cond);
    pthread_mutex_unlock(&enqueue_lock);
  }
  return ret;
}

int
NvDrmRenderer::renderInternal(int fd)
{
  int ret;
  uint32_t i;
  uint32_t handle;
  uint32_t fb;
  uint32_t bo_handles[4];
  uint32_t flags = 0;
  bool frame_is_late = false;

  NvBufferParams params;
  NvBufDrmParams dParams;
  struct drm_tegra_gem_set_tiling args;
  auto map_entry = map_list.find (fd);
  if (map_entry != map_list.end()) {
    fb = (uint32_t) map_entry->second;
  } else {
    // Create a new FB.
    ret = NvBufferGetParams(fd, &params);
    if (ret < 0) {
      COMP_ERROR_MSG("Failed to get buffer information ");
      goto error;
    }

    ret = NvBufGetDrmParams(&params, &dParams);
    if (ret < 0) {
      COMP_ERROR_MSG("Failed to convert to DRM params ");
      goto error;
    }

   for (i = 0; i < dParams.num_planes; i++) {
     ret = drmPrimeFDToHandle(drm_fd, fd, &handle);
     if (ret)
     {
       COMP_ERROR_MSG("Failed to import buffer object. ");
       goto error;
     }

     memset(&args, 0, sizeof(args));
     args.handle = handle;
     args.mode = DRM_TEGRA_GEM_TILING_MODE_PITCH;
     args.value = 1;

     ret = drmIoctl(drm_fd, DRM_IOCTL_TEGRA_GEM_SET_TILING, &args);
     if (ret < 0)
     {
       COMP_ERROR_MSG("Failed to set tiling parameters ");
       goto error;
     }

     bo_handles[i] = handle;
   }

    ret = drmModeAddFB2(drm_fd, width, height, dParams.pixel_format, bo_handles,
                        dParams.pitch, dParams.offset, &fb, flags);

    if (ret)
    {
      COMP_ERROR_MSG("Failed to create fb ");
      goto error;
    }

    if (planeIsSet == false)
    {
      ret = setPlane(0, fb, 0, 0, width, height, 0, 0, width, height);
      if(ret) {
        COMP_ERROR_MSG("FAILED TO SET PLANE ");
        goto error;
      }
      planeIsSet = true;
    }

    /* TODO:
     * We get new FDs from camera consumer. Don't do mapping until
     * we can resolve that.
     */
//    map_list.insert(std::make_pair(fd, fb));
  }

  if (last_render_time.tv_sec != 0)
  {
    pthread_mutex_lock(&render_lock);
    last_render_time.tv_sec += render_time_sec;
    last_render_time.tv_nsec += render_time_nsec;
    last_render_time.tv_sec += last_render_time.tv_nsec / 1000000000UL;
    last_render_time.tv_nsec %= 1000000000UL;

    if (isProfilingEnabled())
    {
        struct timeval cur_time;
        gettimeofday(&cur_time, NULL);
        if ((cur_time.tv_sec * 1000000.0 + cur_time.tv_usec) >
                (last_render_time.tv_sec * 1000000.0 +
                 last_render_time.tv_nsec / 1000.0))
        {
            frame_is_late = true;
        }
    }

    pthread_cond_timedwait(&render_cond, &render_lock,
                           &last_render_time);
    pthread_mutex_unlock(&render_lock);
  }
  else
  {
    struct timeval now;

    gettimeofday(&now, NULL);
    last_render_time.tv_sec = now.tv_sec;
    last_render_time.tv_nsec = now.tv_usec * 1000L;
  }

  flippedFd = fd;
  flipPending = true;
  ret = drmModePageFlip(drm_fd, drm_crtc_id, fb,
                        DRM_MODE_PAGE_FLIP_EVENT,
                        this);
  if (ret)
  {
    COMP_ERROR_MSG("Failed to flip");
    flipPending = false;
    goto error;
  }

  /* TODO:
   * Don't create/remove fb for each frame but maintain mapping.
   * We will do that once new FD for each frame from consumer is resolved.
   */

  for (i = 0; i < dParams.num_planes; i++)
  {
    if (!drmUtilCloseGemBo (fd,bo_handles[i]))
    {
      COMP_ERROR_MSG("Failed to close bo \n");
      goto error;
    }
  }

   if(last_fb)
    drmModeRmFB(drm_fd, last_fb);

  last_fb = fb;

  profiler.finishProcessing(0, frame_is_late);
  return 0;

error:
  COMP_ERROR_MSG("Error in rendering frame ");
  return -1;
}

int
NvDrmRenderer::createDumbBO(int width, int height, int bpp, NvDrmBO *bo)
{
  struct drm_mode_create_dumb creq;
  struct drm_mode_destroy_dumb dreq;
  struct drm_mode_map_dumb mreq;
  int ret;
  uint8_t* map = NULL;

  /* create dumb buffer */
  memset(&creq, 0, sizeof(creq));
  creq.width = width;
  creq.height = height;
  creq.bpp = bpp;
  ret = drmIoctl(drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq);
  if (ret < 0) {
    COMP_ERROR_MSG("cannot create dumb buffer\n");
    return 0;
  }

  /* prepare buffer for memory mapping */
  memset(&mreq, 0, sizeof(mreq));
  mreq.handle = creq.handle;
  ret = drmIoctl(drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq);
  if (ret) {
    COMP_ERROR_MSG("cannot map dumb buffer\n");
    ret = -errno;
    goto err_destroy;
  }

  map = (uint8_t*)(mreq.offset);

  /* clear the buffer object */
  memset(map, 0x00, creq.size);

  bo->bo_handle = creq.handle;
  bo->width = width;
  bo->height = height;
  bo->pitch = creq.pitch;
  bo->data = map;

  return 1;

err_destroy:
  memset(&dreq, 0, sizeof(dreq));
  dreq.handle = creq.handle;
  drmIoctl(drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
  return 0;
}

int
NvDrmRenderer::setFPS(float fps)
{
  uint64_t render_time_usec;

  if (fps == 0)
  {
    COMP_WARN_MSG("Fps 0 is not allowed. Not changing fps");
    return -1;
  }
  pthread_mutex_lock(&render_lock);
  this->fps = fps;

  render_time_usec = 1000000L / fps;
  render_time_sec = render_time_usec / 1000000;
  render_time_nsec = (render_time_usec % 1000000) * 1000L;
  pthread_mutex_unlock(&render_lock);
  return 0;
}

bool NvDrmRenderer::enableUniversalPlanes (int enable)
{
  return !drmSetClientCap(drm_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, enable);
}

uint32_t
NvDrmRenderer::createDumbFB(uint32_t width, uint32_t height,
                            uint32_t drm_format, NvDrmFB *fb)
{
  int buf_count;
  int i = 0;
  struct drm_mode_destroy_dumb dreq;
  int ret;

  struct NvBOFormat boFormat = {0};
  if (!get_format_info(drm_format, &boFormat)) {
    COMP_ERROR_MSG("Can't make a FB of type " << drm_format);
    return 0;
  }
  buf_count = boFormat.num_buffers;

  uint32_t buf_id;
  uint32_t bo_handles[4] = {0};
  uint32_t pitches[4] = {0};
  uint32_t offsets[4] = {0};

  /* create dumb buffers */
  for (i = 0; i < buf_count; i++) {
    NvDrmBO *bo = &(fb->bo[i]);
    ret = createDumbBO(width / boFormat.buffers[i].w,
                       height / boFormat.buffers[i].h,
                       boFormat.buffers[i].bpp, bo);
    if (ret < 0) {
      COMP_ERROR_MSG("cannot create dumb buffer ");
      return 0;
    }
    bo_handles[i] = fb->bo[i].bo_handle;
    pitches[i] = fb->bo[i].pitch;
    offsets[i] = 0;
  }

  /* create framebuffer object for the dumb-buffer */
  ret = drmModeAddFB2(drm_fd, width, height, drm_format, bo_handles,
                      pitches, offsets, &buf_id, 0);
  if (ret) {
    COMP_ERROR_MSG("cannot create framebuffer ");
    goto err_destroy;
  }

  fb->fb_id = buf_id;
  fb->width = width;
  fb->height = height;
  fb->format = drm_format;

  return 1;

err_destroy:
  for (i = 0; i < buf_count; i++) {
    memset(&dreq, 0, sizeof(dreq));
    dreq.handle = fb->bo[i].bo_handle;
    drmIoctl(drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
  }

  return 0;
}

int NvDrmRenderer::removeFB(uint32_t fb_id)
{
  return drmModeRmFB(drm_fd, fb_id);
}

int NvDrmRenderer::setPlane(uint32_t pl_index,
                            uint32_t fb_id,
                            uint32_t crtc_x,
                            uint32_t crtc_y,
                            uint32_t crtc_w,
                            uint32_t crtc_h,
                            uint32_t src_x,
                            uint32_t src_y,
                            uint32_t src_w,
                            uint32_t src_h)
{
  int ret = -1;
  drmModePlaneResPtr pl = NULL;
  drmModePlanePtr plane = NULL;
  pl = drmModeGetPlaneResources(drm_fd);
  if (pl) {
    if (pl_index < pl->count_planes) {
      plane = drmModeGetPlane(drm_fd, pl->planes[pl_index]);

      if (plane) {
       ret = drmModeSetPlane(drm_fd, plane->plane_id, drm_crtc_id,
                        fb_id, 0, crtc_x, crtc_y, crtc_w,
                        crtc_h, src_x, src_y,
                        src_w, src_h);

        drmModeFreePlane(plane);
      }
    } else {
      ret = -EINVAL;
    }
    drmModeFreePlaneResources(pl);
    return ret;
  }

  COMP_ERROR_MSG("No plane resource available ");
  return ret;
}

int NvDrmRenderer::getPlaneCount()
{
  drmModePlaneResPtr pl = NULL;
  int count = 0;
  pl = drmModeGetPlaneResources(drm_fd);
  if (pl) {
    count = pl->count_planes;
    drmModeFreePlaneResources(pl);
  }
  return count;
}

int32_t NvDrmRenderer::getPlaneIndex(uint32_t crtc_index,
                                     int32_t* plane_index)
{
  drmModePlaneResPtr pl = NULL;
  uint32_t count = 0;

  if (!plane_index)
    return 0;

  pl = drmModeGetPlaneResources(drm_fd);
  if (pl) {
    for (uint32_t i = 0; i < pl->count_planes; i++) {
      drmModePlanePtr plane;
      plane = drmModeGetPlane(drm_fd, pl->planes[i]);
      plane_index[i] = -1;

      if (plane) {
        //Find the plane is with the given crtc
        if (plane->possible_crtcs & (1 << crtc_index)) {
          plane_index[count] = i;
          count++;
        }
        drmModeFreePlane(plane);
      }
    }
    drmModeFreePlaneResources(pl);
  }

  return count;
}

int NvDrmRenderer::getCrtcCount()
{
  drmModeResPtr resPtr = NULL;
  int count = 0;
  resPtr = drmModeGetResources(drm_fd);
  if (resPtr) {
    count = resPtr->count_crtcs;
    drmModeFreeResources(resPtr);
  }
  return count;
}

int NvDrmRenderer::getEncoderCount()
{
  drmModeResPtr resPtr = NULL;
  int count = 0;
  resPtr = drmModeGetResources(drm_fd);
  if (resPtr) {
    count = resPtr->count_encoders;
    drmModeFreeResources(resPtr);
  }
  return count;
}

NvDrmRenderer *
NvDrmRenderer::createDrmRenderer(const char *name, uint32_t width,
                               uint32_t height, uint32_t w_x, uint32_t w_y,
                               uint32_t connector, uint32_t crtc,
                               struct drm_tegra_hdr_metadata_smpte_2086 metadata,
                               bool streamHDR)
{
  if (!width || ! height) {
    width = 640;
    height = 480;
  }

  NvDrmRenderer* renderer = new NvDrmRenderer(name, width, height, w_x, w_y, connector, crtc, metadata, streamHDR);
  if (renderer && renderer->isInError())
  {
    delete renderer;
    return NULL;
  }
  return renderer;
}
