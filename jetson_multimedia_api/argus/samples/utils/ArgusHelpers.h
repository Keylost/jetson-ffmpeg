/*
 * Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
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

#ifndef ARGUS_HELPERS_H
#define ARGUS_HELPERS_H

#include <Argus/Argus.h>

namespace ArgusSamples
{

namespace ArgusHelpers
{


/**
 * Returns the CameraDevice of a given index from a CameraProvider.
 *
 * @param[in] cameraProvider The CameraProvider to get the device from.
 * @param[in] cameraDeviceIndex The index of the device to get.
 */
Argus::CameraDevice* getCameraDevice(Argus::CameraProvider* cameraProvider,
                                     uint32_t cameraDeviceIndex);

/**
 * Returns the SensorMode of a given index from a CameraProvider.
 *
 * @param[in] cameraDevice The CameraDevice to get the sensor mode from.
 * @param[in] sensorModendex The index of the sensor mode to get.
 */
Argus::SensorMode* getSensorMode(Argus::CameraDevice* cameraDevice,
                                 uint32_t sensorModeIndex);

/**
 * Return the first available WDR (DOL or PWL) sensor mode from a CameraDevice.
 *
 * @param[in] cameraDevice The CameraDevice to get the WDR sensor mode from.
 */
Argus::SensorMode* getWdrSensorMode(Argus::CameraDevice* cameraDevice);

/**
 * Prints out information about a CameraDevice.
 *
 * @param[in] cameraDevice The device to print the info for.
 * @param[in] indent the indent to prepend to each line of output.
 */
void printCameraDeviceInfo(Argus::CameraDevice* cameraDevice, const char* indent);

/**
 * Prints out information about a SensorMode.
 *
 * @param[in] sensorMode The mode to print the info for.
 * @param[in] indent the indent to prepend to each line of output.
 */
void printSensorModeInfo(Argus::SensorMode* sensorMode, const char* indent);

/**
 * Prints out a UUID.
 */
void printUUID(const Argus::UUID& uuid);


}; // namespace ArgusHelpers

}; // namespace ArgusSamples

#endif // ARGUS_HELPERS_H
