#ifndef __eglext_nv_h_
#define __eglext_nv_h_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Copyright (c) 2008 - 2020, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef EGL_EXT_stream_acquire_mode
#define EGL_EXT_stream_acquire_mode 1
#define EGL_CONSUMER_AUTO_ACQUIRE_EXT         0x332B
#define EGL_RESOURCE_BUSY_EXT                 0x3353
typedef EGLBoolean (EGLAPIENTRYP PFNEGLSTREAMCONSUMERACQUIREATTRIBEXTPROC) (EGLDisplay dpy, EGLStreamKHR stream, const EGLAttrib *attrib_list);
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLBoolean EGLAPIENTRY eglStreamConsumerAcquireAttribEXT (EGLDisplay dpy, EGLStreamKHR stream, const EGLAttrib *attrib_list);
#endif
#endif /* EGL_EXT_stream_acquire_mode */

#ifndef EGL_EXT_stream_consumer_qnxscreen_window
#define EGL_EXT_stream_consumer_qnxscreen_window 1
#define EGL_CONSUMER_ACQUIRE_QNX_FLUSHING_EXT        0x3320
#define EGL_CONSUMER_ACQUIRE_QNX_DISPNO_EXT          0x3321
#define EGL_CONSUMER_ACQUIRE_QNX_LAYERNO_EXT         0x3322
#define EGL_CONSUMER_ACQUIRE_QNX_SURFACE_TYPE_EXT    0x3323
#define EGL_CONSUMER_ACQUIRE_QNX_DISPLAY_POS_X_EXT   0x3324
#define EGL_CONSUMER_ACQUIRE_QNX_DISPLAY_POS_Y_EXT   0x3325
#define EGL_CONSUMER_ACQUIRE_QNX_DISPLAY_WIDTH_EXT   0x3326
#define EGL_CONSUMER_ACQUIRE_QNX_DISPLAY_HEIGHT_EXT  0x3327
typedef EGLBoolean (EGLAPIENTRYP PFNEGLSTREAMCONSUMERQNXSCREENWINDOWEXTPROC) (EGLDisplay dpy, EGLStreamKHR stream, const EGLAttrib *attrib_list);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLSTREAMCONSUMERRELEASEEXTPROC) (EGLDisplay dpy, EGLStreamKHR stream);
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLBoolean EGLAPIENTRY eglStreamConsumerQNXScreenWindowEXT (EGLDisplay dpy, EGLStreamKHR stream, const EGLAttrib *attrib_list);
EGLAPI EGLBoolean EGLAPIENTRY eglStreamConsumerReleaseEXT (EGLDisplay dpy, EGLStreamKHR stream);
#endif
#endif /* EGL_EXT_stream_consumer_qnxscreen_window */

#ifndef EGL_NV_output_drm_atomic
#define EGL_NV_output_drm_atomic 1
#define EGL_DRM_ATOMIC_REQUEST_NV             0x3333
#endif /* EGL_NV_output_drm_atomic */

#ifndef EGL_NV_output_drm_flip_event
#define EGL_NV_output_drm_flip_event 1
#define EGL_DRM_FLIP_EVENT_DATA_NV            0x333E
#endif /* EGL_NV_output_drm_flip_event */

#ifndef EGL_NV_perfmon
#define EGL_NV_perfmon 1
#define EGL_PERFMONITOR_HARDWARE_COUNTERS_BIT_NV    0x00000001
#define EGL_PERFMONITOR_OPENGL_ES_API_BIT_NV        0x00000010
#define EGL_PERFMONITOR_OPENVG_API_BIT_NV           0x00000020
#define EGL_PERFMONITOR_OPENGL_ES2_API_BIT_NV       0x00000040
#define EGL_COUNTER_NAME_NV                         0x3220
#define EGL_COUNTER_DESCRIPTION_NV                  0x3221
#define EGL_IS_HARDWARE_COUNTER_NV                  0x3222
#define EGL_COUNTER_MAX_NV                          0x3223
#define EGL_COUNTER_VALUE_TYPE_NV                   0x3224
#define EGL_RAW_VALUE_NV                            0x3225
#define EGL_PERCENTAGE_VALUE_NV                     0x3226
#define EGL_BAD_CURRENT_PERFMONITOR_NV              0x3227
#define EGL_NO_PERFMONITOR_NV ((EGLPerfMonitorNV)0)
#define EGL_DEFAULT_PERFMARKER_NV ((EGLPerfMarkerNV)0)
typedef void *EGLPerfMonitorNV;
typedef void *EGLPerfCounterNV;
typedef void *EGLPerfMarkerNV;
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLPerfMonitorNV EGLAPIENTRY eglCreatePerfMonitorNV(EGLDisplay dpy, EGLint mask);
EGLAPI EGLBoolean EGLAPIENTRY eglDestroyPerfMonitorNV(EGLDisplay dpy, EGLPerfMonitorNV monitor);
EGLAPI EGLBoolean EGLAPIENTRY eglMakeCurrentPerfMonitorNV(EGLPerfMonitorNV monitor);
EGLAPI EGLPerfMonitorNV EGLAPIENTRY eglGetCurrentPerfMonitorNV(void);
EGLAPI EGLBoolean EGLAPIENTRY eglGetPerfCountersNV(EGLPerfMonitorNV monitor, EGLPerfCounterNV *counters, EGLint counter_size, EGLint *num_counter);
EGLAPI EGLBoolean EGLAPIENTRY eglGetPerfCounterAttribNV(EGLPerfMonitorNV monitor, EGLPerfCounterNV counter, EGLint pname, EGLint *value);
EGLAPI const char * EGLAPIENTRY eglQueryPerfCounterStringNV(EGLPerfMonitorNV monitor, EGLPerfCounterNV counter, EGLint pname);
EGLAPI EGLBoolean EGLAPIENTRY eglPerfMonitorAddCountersNV(EGLint n, const EGLPerfCounterNV *counters);
EGLAPI EGLBoolean EGLAPIENTRY eglPerfMonitorRemoveCountersNV(EGLint n, const EGLPerfCounterNV *counters);
EGLAPI EGLBoolean EGLAPIENTRY eglPerfMonitorRemoveAllCountersNV(void);
EGLAPI EGLBoolean EGLAPIENTRY eglPerfMonitorBeginExperimentNV(void);
EGLAPI EGLBoolean EGLAPIENTRY eglPerfMonitorEndExperimentNV(void);
EGLAPI EGLBoolean EGLAPIENTRY eglPerfMonitorBeginPassNV(EGLint n);
EGLAPI EGLBoolean EGLAPIENTRY eglPerfMonitorEndPassNV(void);
EGLAPI EGLPerfMarkerNV EGLAPIENTRY eglCreatePerfMarkerNV(void);
EGLAPI EGLBoolean EGLAPIENTRY eglDestroyPerfMarkerNV(EGLPerfMarkerNV marker);
EGLAPI EGLBoolean EGLAPIENTRY eglMakeCurrentPerfMarkerNV(EGLPerfMarkerNV marker);
EGLAPI EGLPerfMarkerNV EGLAPIENTRY eglGetCurrentPerfMarkerNV(void);
EGLAPI EGLBoolean EGLAPIENTRY eglGetPerfMarkerCounterNV(EGLPerfMarkerNV marker, EGLPerfCounterNV counter, EGLuint64NV *value, EGLuint64NV *cycles);
EGLAPI EGLBoolean EGLAPIENTRY eglValidatePerfMonitorNV(EGLint *num_passes);
#endif
typedef EGLPerfMonitorNV (EGLAPIENTRYP PFNEGLCREATEPERFMONITORNVPROC)(EGLDisplay dpy, EGLint mask);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLDESTROYPERFMONITORNVPROC)(EGLDisplay dpy, EGLPerfMonitorNV monitor);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLMAKECURRENTPERFMONITORNVPROC)(EGLPerfMonitorNV monitor);
typedef EGLPerfMonitorNV (EGLAPIENTRYP PFNEGLGETCURRENTPERFMONITORNVPROC)(void);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLGETPERFCOUNTERSNVPROC)(EGLPerfMonitorNV monitor, EGLPerfCounterNV *counters, EGLint counter_size, EGLint *num_counter);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLGETPERFCOUNTERATTRIBNVPROC)(EGLPerfMonitorNV monitor, EGLPerfCounterNV counter, EGLint pname, EGLint *value);
typedef const char * (EGLAPIENTRYP PFNEGLQUERYPERFCOUNTERSTRINGNVPROC)(EGLPerfMonitorNV monitor, EGLPerfCounterNV counter, EGLint pname);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLPERFMONITORADDCOUNTERSNVPROC)(EGLint n, const EGLPerfCounterNV *counters);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLPERFMONITORREMOVECOUNTERSNVPROC)(EGLint n, const EGLPerfCounterNV *counters);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLPERFMONITORREMOVEALLCOUNTERSNVPROC)(void);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLPERFMONITORBEGINEXPERIMENTNVPROC)(void);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLPERFMONITORENDEXPERIMENTNVPROC)(void);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLPERFMONITORBEGINPASSNVPROC)(EGLint n);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLPERFMONITORENDPASSNVPROC)(void);
typedef EGLPerfMarkerNV (EGLAPIENTRYP PFNEGLCREATEPERFMARKERNVPROC)(void);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLDESTROYPERFMARKERNVPROC)(EGLPerfMarkerNV marker);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLMAKECURRENTPERFMARKERNVPROC)(EGLPerfMarkerNV marker);
typedef EGLPerfMarkerNV (EGLAPIENTRYP PFNEGLGETCURRENTPERFMARKERNVPROC)(void);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLGETPERFMARKERCOUNTERNVPROC)(EGLPerfMarkerNV marker, EGLPerfCounterNV counter, EGLuint64NV *value, EGLuint64NV *cycles);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLVALIDATEPERFMONITORNVPROC)(EGLint *num_passes);
#endif /* EGL_NV_perfmon */

#ifndef EGL_NV_quadruple_buffer
#define EGL_NV_quadruple_buffer 1
#define EGL_QUADRUPLE_BUFFER_NV             0x3231
#endif /* EGL_NV_quadruple_buffer */

#ifndef EGL_NV_secure_context
#define EGL_NV_secure_context 1
#define EGL_SECURE_NV 0x313E
#endif /* EGL_NV_secure_context */

#ifndef EGL_NV_set_renderer
#define EGL_NV_set_renderer 1
#define EGL_RENDERER_LOWEST_POWER_NV         0x313A
#define EGL_RENDERER_HIGHEST_PERFORMANCE_NV  0x313B
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLBoolean EGLAPIENTRY eglSetRendererNV(EGLenum renderer);
#endif
typedef EGLBoolean (EGLAPIENTRYP PFNEGLSETRENDERERNVPROC)(EGLenum renderer);
#endif /* EGL_NV_set_renderer */

#ifndef EGL_NV_swap_asynchronous
#define EGL_NV_swap_asynchronous
#define EGL_ASYNCHRONOUS_SWAPS_NV 0x3232
#endif /* EGL_NV_swap_asynchrounous */

#ifndef EGL_NV_swap_hint
#define EGL_NV_swap_hint
#define EGL_SWAP_HINT_NV                0x30E4
#define EGL_FASTEST_NV                  0x30E5
#endif /* EGL_NV_swap_hint */

#ifndef EGL_NV_texture_rectangle
#define EGL_NV_texture_rectangle 1
#define EGL_GL_TEXTURE_RECTANGLE_NV_KHR           0x30BB
#define EGL_TEXTURE_RECTANGLE_NV       0x20A2
#endif /* EGL_NV_texture_rectangle */

#ifndef EGL_NV_triple_buffer
#define EGL_NV_triple_buffer 1
#define EGL_TRIPLE_BUFFER_NV                0x3230
#endif /* EGL_NV_triple_buffer */

/* Deprecated. Use EGL_KHR_stream_attrib */
#ifndef EGL_NV_stream_attrib
#define EGL_NV_stream_attrib 1
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLStreamKHR EGLAPIENTRY eglCreateStreamAttribNV(EGLDisplay dpy, const EGLAttrib *attrib_list);
EGLAPI EGLBoolean EGLAPIENTRY eglSetStreamAttribNV(EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLAttrib value);
EGLAPI EGLBoolean EGLAPIENTRY eglQueryStreamAttribNV(EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLAttrib *value);
EGLAPI EGLBoolean EGLAPIENTRY eglStreamConsumerAcquireAttribNV(EGLDisplay dpy, EGLStreamKHR stream, const EGLAttrib *attrib_list);
EGLAPI EGLBoolean EGLAPIENTRY eglStreamConsumerReleaseAttribNV(EGLDisplay dpy, EGLStreamKHR stream, const EGLAttrib *attrib_list);
#endif
typedef EGLStreamKHR (EGLAPIENTRYP PFNEGLCREATESTREAMATTRIBNVPROC) (EGLDisplay dpy, const EGLAttrib *attrib_list);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLSETSTREAMATTRIBNVPROC) (EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLAttrib value);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLQUERYSTREAMATTRIBNVPROC) (EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLAttrib *value);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLSTREAMCONSUMERACQUIREATTRIBNVPROC) (EGLDisplay dpy, EGLStreamKHR stream, const EGLAttrib *attrib_list);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLSTREAMCONSUMERRELEASEATTRIBNVPROC) (EGLDisplay dpy, EGLStreamKHR stream, const EGLAttrib *attrib_list);
#endif /* EGL_NV_stream_attrib */

#ifndef EGL_WL_bind_wayland_display
#define EGL_WL_bind_wayland_display 1
#define EGL_WAYLAND_BUFFER_WL 0x31D5
#define EGL_WAYLAND_PLANE_WL 0x31D6
#define EGL_TEXTURE_Y_U_V_WL 0x31D7
#define EGL_TEXTURE_Y_UV_WL 0x31D8
#define EGL_TEXTURE_Y_XUXV_WL 0x31D9
#define EGL_TEXTURE_EXTERNAL_WL 0x31DA
#define EGL_TEXTURE_FORMAT 0x3080
#define EGL_WAYLAND_Y_INVERTED_WL 0x31DB
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLBoolean EGLAPIENTRY eglBindWaylandDisplayWL(EGLDisplay dpy, void *display);
EGLAPI EGLBoolean EGLAPIENTRY eglUnbindWaylandDisplayWL(EGLDisplay dpy, void *display);
EGLAPI EGLBoolean EGLAPIENTRY eglQueryWaylandBufferWL(EGLDisplay dpy, void *buffer, EGLint attribute, int *value);
#endif
typedef EGLBoolean (EGLAPIENTRYP PFNEGLBINDWAYLANDDISPLAYWL) (EGLDisplay dpy, void *display);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLUNBINDWAYLANDDISPLAYWL) (EGLDisplay dpy, void *display);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLQUERYWAYLANDBUFFERWL) (EGLDisplay dpy, void *buffer, EGLint attribute, void *value);
#endif /* EGL_WL_bind_wayland_display */

#ifndef EGL_WL_wayland_eglstream
#define EGL_WL_wayland_eglstream 1
#define EGL_WAYLAND_EGLSTREAM_WL             0x334B
#endif /* EGL_WL_wayland_eglstream */

#ifndef EGL_NV_stream_consumer_eglimage
#define EGL_NV_stream_consumer_eglimage 1
#define EGL_STREAM_CONSUMER_IMAGE_NV         0x3373
#define EGL_STREAM_IMAGE_ADD_NV              0x3374
#define EGL_STREAM_IMAGE_REMOVE_NV           0x3375
#define EGL_STREAM_IMAGE_AVAILABLE_NV        0x3376
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLBoolean EGLAPIENTRY eglStreamImageConsumerConnectNV(EGLDisplay dpy, EGLStreamKHR stream, EGLint num_modifiers, EGLuint64KHR* modifiers, EGLAttrib* aux);
EGLAPI EGLBoolean EGLAPIENTRY eglQueryStreamConsumerEventNV(EGLDisplay dpy, EGLStreamKHR stream, EGLTime timeout, EGLenum *event, EGLAttrib *aux);
EGLAPI EGLBoolean EGLAPIENTRY eglStreamAcquireImageNV(EGLDisplay dpy, EGLStreamKHR stream, EGLImage *pImage, EGLSync sync);
EGLAPI EGLBoolean EGLAPIENTRY eglStreamReleaseImageNV(EGLDisplay dpy, EGLStreamKHR stream, EGLImage image, EGLSync sync);
#endif
typedef EGLBoolean (EGLAPIENTRYP PFNEGLSTREAMIMAGECONSUMERCONNECTNV) (EGLDisplay dpy, EGLStreamKHR stream, EGLint num_modifiers, EGLuint64KHR* modifiers, EGLAttrib* aux);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLQUERYSTREAMCONSUMEREVENTNV) (EGLDisplay dpy, EGLStreamKHR stream, EGLTime timeout, EGLenum *event, EGLAttrib *aux);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLSTREAMACQUIREIMAGENV) (EGLDisplay dpy, EGLStreamKHR stream, EGLImage *pImage, EGLSync sync);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLSTREAMRELEASEIMAGENV) (EGLDisplay dpy, EGLStreamKHR stream, EGLImage image, EGLSync sync);
#endif /* EGL_NV_stream_consumer_eglimage */

#ifdef __cplusplus
}
#endif

#endif
