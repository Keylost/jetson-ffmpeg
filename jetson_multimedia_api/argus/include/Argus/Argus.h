/*
 * Copyright (c) 2016-2017, NVIDIA CORPORATION. All rights reserved.
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
 * @file
 * <b>Libargus API: Main Include File</b>
 *
 * @b Description: This is the main include file for libargus applications.
 */

#ifndef _ARGUS_H
#define _ARGUS_H

/**
 * @defgroup LibargusAPI Libargus Camera API
 *
 * Libargus is an API for acquiring images and associated metadata from cameras.
 * The fundamental libargus operation is a capture:
 * acquiring an image from a sensor and processing it into a final output image.
 *
 * Currently, libargus is supported on Android and all Jetson Linux platforms.
 *
 * Libargus is designed to address a number of fundamental requirements:
 *
 * - Support for a wide variety of use cases (traditional photography, computational photography,
 * video, computer vision, and other application areas.)
 * To this end, libargus is a frame-based API; every capture is triggered by an explicit request that
 * specifies exactly how the capture is to be performed.
 *
 * - Support for multiple platforms, including L4T and Android.
 *
 * - Efficient and simple integration into applications and larger frameworks.  In support of this,
 * libargus delivers images with EGLStreams, which are directly supported by other system components
 * such as OpenGL and Cuda, and which require no buffer copies during delivery to the consumer.
 *
 * - Expansive metadata along with each output image.
 *
 * - Support for multiple sensors, including both separate control over independent sensors and
 * access to synchronized multi-sensor configurations.  (The latter are unsupported in the current
 * release. When support is added, it will be available on only some NVIDIA platforms.)
 *
 * - Version stability and extensibility, which are provided by unchanging virtual interfaces and
 * the ability for vendors to add specialized extension interfaces.
 *
 * Libargus provides functionality in a number of different areas:
 *
 * - Captures with a wide variety of settings.
 *
 * - Optional autocontrol (such as auto-exposure and auto-white-balance.)
 *
 * - Libraries that consume the EGLStream outputs in different ways; for example, jpeg encoding or
 * direct application access to the images.
 *
 * - Metadata delivery via both libargus events and EGLStream metadata.
 *
 * - Image post-processing such as noise reduction and edge sharpening.
 *
 * - Notification of errors, image acquisition start, and other events via synchronous event queues.
 *
 * Functionality not provided by libargus:
 *
 * - Auto-focus. (Planned to be added in a later release.)
 *
 * - Reprocessing of YUV images (such as that required by Android’s Zero Shutter Lag feature.)
 *
 * - Reprocessing of Bayer (raw) images.  (Planned to be added in a later release.)
 *
 * @defgroup ArgusObjects Objects and Interfaces
 * This module provides a list of libargus objects and the interfaces that they support.
 * Children of objects in this list describe either child InterfaceProviders provided by
 * the object or specialized types of the object (ie. Event types).
 * @ingroup LibargusAPI
 *
 * @defgroup ArgusExtensions Extensions
 * This module provides a list of extensions currently available for libargus.
 * @ingroup LibargusAPI
 */

/**
 * @ifnot SWDOCS_L4T <!-- Hides open-source doxy entries in L4T build.-->
 * @mainpage
 * Proceed to the \ref LibargusAPI module for libargus documentation.
 * @endif
 */

#include <stddef.h>

#include "Argus/UUID.h"
#include "Argus/Types.h"
#include "Argus/Stream.h"

#include "Argus/BufferStream.h"
#include "Argus/CameraDevice.h"
#include "Argus/CameraProvider.h"
#include "Argus/CaptureMetadata.h"
#include "Argus/CaptureSession.h"
#include "Argus/EGLImage.h"
#include "Argus/EGLStream.h"
#include "Argus/EGLSync.h"
#include "Argus/Event.h"
#include "Argus/EventProvider.h"
#include "Argus/EventQueue.h"
#include "Argus/Request.h"
#include "Argus/Settings.h"

#endif
