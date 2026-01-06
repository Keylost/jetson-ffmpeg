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

#include "ArgusHelpers.h"
#include <Argus/Ext/DolWdrSensorMode.h>
#include <Argus/Ext/PwlWdrSensorMode.h>
#include <stdio.h>

namespace ArgusSamples
{

Argus::CameraDevice* ArgusHelpers::getCameraDevice(Argus::CameraProvider* cameraProvider,
                                                   uint32_t cameraDeviceIndex)
{
    Argus::ICameraProvider *iCameraProvider =
        Argus::interface_cast<Argus::ICameraProvider>(cameraProvider);
    if (iCameraProvider == NULL)
    {
        printf("Failed to get ICameraProvider interface.\n");
        return NULL;
    }

    std::vector<Argus::CameraDevice*> cameraDevices;
    Argus::Status status = iCameraProvider->getCameraDevices(&cameraDevices);
    if (status != Argus::STATUS_OK)
    {
        printf("Failed to get camera devices from provider.\n");
        return NULL;
    }
    if (cameraDevices.size() == 0)
    {
        printf("No camera devices are available.\n");
        return NULL;
    }
    if (cameraDevices.size() <= cameraDeviceIndex)
    {
        printf("Camera device %u was requested but only %lu %s available.\n",
               cameraDeviceIndex, static_cast<unsigned long>(cameraDevices.size()),
               cameraDevices.size() == 1 ? "is" : "are");
        printf("Available CameraDevices:\n");
        for (uint32_t i = 0; i < cameraDevices.size(); i++)
        {
            printf("  ==== CameraDevice %u: =========================================\n", i);
            printCameraDeviceInfo(cameraDevices[i], "    ");
        }
        return NULL;
    }

    return cameraDevices[cameraDeviceIndex];
}

Argus::SensorMode* ArgusHelpers::getSensorMode(Argus::CameraDevice* cameraDevice,
                                               uint32_t sensorModeIndex)
{
    Argus::ICameraProperties *iCameraProperties =
        Argus::interface_cast<Argus::ICameraProperties>(cameraDevice);
    if (!iCameraProperties)
    {
        printf("Failed to get ICameraProperties interface.\n");
        return NULL;
    }

    std::vector<Argus::SensorMode*> sensorModes;
    Argus::Status status = iCameraProperties->getAllSensorModes(&sensorModes);
    if (status != Argus::STATUS_OK)
    {
        printf("Failed to get sensor modes from device.\n");
        return NULL;
    }
    if (sensorModes.size() == 0)
    {
        printf("No sensor modes are available.\n");
        return NULL;
    }
    if (sensorModes.size() <= sensorModeIndex)
    {
        printf("Sensor mode %d was requested but only %lu %s available.\n",
               sensorModeIndex, static_cast<unsigned long>(sensorModes.size()),
               sensorModes.size() == 1 ? "is" : "are");
        printf("Available sensor modes:\n");
        for (uint32_t i = 0; i < sensorModes.size(); i++)
        {
            printf("  SensorMode %d:\n", i);
            printSensorModeInfo(sensorModes[i], "    ");
        }
        return NULL;
    }
    return sensorModes[sensorModeIndex];
}

Argus::SensorMode* ArgusHelpers::getWdrSensorMode(Argus::CameraDevice* cameraDevice)
{
    Argus::ICameraProperties *iCameraProperties =
        Argus::interface_cast<Argus::ICameraProperties>(cameraDevice);
    if (!iCameraProperties)
    {
        printf("Failed to get ICameraProperties interface.\n");
        return NULL;
    }

    std::vector<Argus::SensorMode*> sensorModes;
    Argus::Status status = iCameraProperties->getAllSensorModes(&sensorModes);
    if (status != Argus::STATUS_OK)
    {
        printf("Failed to get sensor modes from device.\n");
        return NULL;
    }

    for (uint32_t i = 0; i < sensorModes.size(); i++)
    {
        if (Argus::interface_cast<Argus::Ext::IPwlWdrSensorMode>(sensorModes[i]) ||
            Argus::interface_cast<Argus::Ext::IDolWdrSensorMode>(sensorModes[i]))
        {
            printf("Using WDR SensorMode (index = %u)\n", i);
            return sensorModes[i];
        }
    }

    printf("WDR mode requested but none available. Available sensor modes:\n");
    for (uint32_t i = 0; i < sensorModes.size(); i++)
    {
        printf("  SensorMode %d:\n", i);
        printSensorModeInfo(sensorModes[i], "    ");
    }

    return NULL;
}

void ArgusHelpers::printCameraDeviceInfo(Argus::CameraDevice* cameraDevice, const char* indent)
{
    Argus::ICameraProperties *iCameraProperties =
        Argus::interface_cast<Argus::ICameraProperties>(cameraDevice);
    if (iCameraProperties)
    {
        std::vector<Argus::SensorMode*> sensorModes;
        iCameraProperties->getAllSensorModes(&sensorModes);

        printf("%sUUID:                      ", indent);
        printUUID(iCameraProperties->getUUID());
        printf("%sMaxAeRegions:              %u\n", indent, iCameraProperties->getMaxAeRegions());
        printf("%sMaxAwbRegions:             %u\n", indent, iCameraProperties->getMaxAwbRegions());
        Argus::Range<int32_t> i32Range = iCameraProperties->getFocusPositionRange();
        printf("%sFocusPositionRange:        [%d, %d]\n", indent, i32Range.min(), i32Range.max());

        printf("%sLensApertureRange:\n", indent);
        std::vector<float> availableFnums;
        iCameraProperties->getAvailableApertureFNumbers(&availableFnums);
        for (std::vector<float>::iterator it = availableFnums.begin();
             it != availableFnums.end(); ++it)
        {
            printf("%s %f\n", indent, *it);
        }

        Argus::Range<float> fRange = iCameraProperties->getIspDigitalGainRange();
        printf("%sIspDigitalGainRange:       [%f, %f]\n", indent, fRange.min(), fRange.max());
        fRange = iCameraProperties->getExposureCompensationRange();
        printf("%sExposureCompensationRange: [%f, %f]\n", indent, fRange.min(), fRange.max());
        printf("%sNumSensorModes:            %lu\n", indent,
                static_cast<unsigned long>(sensorModes.size()));
        for (uint32_t i = 0; i < sensorModes.size(); i++)
        {
            printf("%sSensorMode %d:\n", indent, i);
            char modeIndent[32];
            snprintf(modeIndent, sizeof(modeIndent), "%s    ", indent);
            printSensorModeInfo(sensorModes[i], modeIndent);
        }
    }
}

void ArgusHelpers::printSensorModeInfo(Argus::SensorMode* sensorMode, const char* indent)
{
    Argus::ISensorMode *iSensorMode =
        Argus::interface_cast<Argus::ISensorMode>(sensorMode);
    if (iSensorMode)
    {
        Argus::Size2D<uint32_t> resolution = iSensorMode->getResolution();
        printf("%sResolution:         %ux%u\n", indent, resolution.width(), resolution.height());

        Argus::Range<float> hdrRatioRange = iSensorMode->getHdrRatioRange();
        printf("%sHdrRatioRange:  [%f, %f]\n", indent,
                static_cast<float>(hdrRatioRange.min()),
                static_cast<float>(hdrRatioRange.max()));

        if (hdrRatioRange.max() > 1.f)
        {
            Argus::Range<uint64_t> u64Range = iSensorMode->getExposureTimeRange();
            printf("%sExposureTimeRange for long exposure::  [%llu, %llu]\n", indent,
                static_cast<unsigned long long>(u64Range.min()),
                static_cast<unsigned long long>(u64Range.max()));

            printf("%sExposureTimeRange for short exposure: [%llu, %llu]\n", indent,
                static_cast<unsigned long long>(u64Range.min() / hdrRatioRange.max()),
                static_cast<unsigned long long>(u64Range.max() / hdrRatioRange.min()));
        }
        else
        {
            Argus::Range<uint64_t> u64Range = iSensorMode->getExposureTimeRange();
            printf("%sExposureTimeRange:  [%llu, %llu]\n", indent,
                static_cast<unsigned long long>(u64Range.min()),
                static_cast<unsigned long long>(u64Range.max()));
        }

        Argus::Range<uint64_t> u64Range = iSensorMode->getFrameDurationRange();
        printf("%sFrameDurationRange: [%llu, %llu]\n", indent,
                static_cast<unsigned long long>(u64Range.min()),
                static_cast<unsigned long long>(u64Range.max()));
        printf("%s                    (%.2f to %.2f fps)\n", indent,
               (1000000000.0 / u64Range.max()),
               (1000000000.0 / u64Range.min()));
        Argus::Range<float> fRange = iSensorMode->getAnalogGainRange();
        printf("%sAnalogGainRange:    [%f, %f]\n", indent, fRange.min(), fRange.max());
        printf("%sInputBitDepth:      %u\n", indent, iSensorMode->getInputBitDepth());
        printf("%sOutputBitDepth:     %u\n", indent, iSensorMode->getOutputBitDepth());
        printf("%sSensorModeType:     %s\n", indent, iSensorMode->getSensorModeType().getName());

        Argus::Ext::IDolWdrSensorMode* dolMode =
                Argus::interface_cast<Argus::Ext::IDolWdrSensorMode>(sensorMode);
        Argus::Ext::IPwlWdrSensorMode* pwlMode =
                Argus::interface_cast<Argus::Ext::IPwlWdrSensorMode>(sensorMode);
        if (dolMode)
        {
            printf("%sDOL WDR Mode Properties:\n", indent);
            printf("%s  ExposureCount:        %u\n", indent, dolMode->getExposureCount());
            printf("%s  OpticalBlackRowCount: %u\n", indent, dolMode->getOpticalBlackRowCount());
            std::vector<uint32_t> vbpRowCounts;
            dolMode->getVerticalBlankPeriodRowCount(&vbpRowCounts);
            printf("%s  VBPRowCounts:         [%u", indent, vbpRowCounts[0]);
            for (uint32_t i = 1; i < vbpRowCounts.size(); i++)
            {
                printf(", %u", vbpRowCounts[i]);
            }
            printf("]\n");
            printf("%s  LineInfoMarkerWidth:  %u\n", indent, dolMode->getLineInfoMarkerWidth());
            printf("%s  LeftMarginWidth:      %u\n", indent, dolMode->getLeftMarginWidth());
            printf("%s  RightMarginWidth:     %u\n", indent, dolMode->getRightMarginWidth());
            resolution = dolMode->getPhysicalResolution();
            printf("%s  PhysicalResolution:   %ux%u\n", indent,
                   resolution.width(), resolution.height());
        }
        else if (pwlMode)
        {
            printf("%sPWL WDR Mode Properties:\n", indent);
            printf("%s  ControlPointCount:    %u\n", indent, pwlMode->getControlPointCount());
            std::vector< Argus::Point2D<float> > controlPoints;
            pwlMode->getControlPoints(&controlPoints);
            printf("%s  ControlPoints:        {(%f, %f)", indent,
                   controlPoints[0].x(), controlPoints[0].y());
            for (uint32_t i = 1; i < controlPoints.size(); i++)
            {
                printf(", (%f, %f)", controlPoints[i].x(), controlPoints[i].y());
            }
            printf("}\n");
        }
        else
        {
            printf("%sIS WDR Mode: No\n", indent);
        }
    }
}

void ArgusHelpers::printUUID(const Argus::UUID& uuid)
{
    printf("%08x,%04x,%04x,%04x,%02x,%02x,%02x,%02x,%02x,%02x\n",
           uuid.time_low, uuid.time_mid, uuid.time_hi_and_version, uuid.clock_seq,
           uuid.node[0], uuid.node[1], uuid.node[2],
           uuid.node[3], uuid.node[4], uuid.node[5]);
}

}; // namespace ArgusSamples
