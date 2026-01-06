/*
 * Copyright (c) 2016-2020, NVIDIA CORPORATION. All rights reserved.
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
 * <b>Libargus API: Camera Device API</b>
 *
 * @b Description: This file defines objects representing a single camera device.
 */

#ifndef _ARGUS_CAMERA_DEVICE_H
#define _ARGUS_CAMERA_DEVICE_H

namespace Argus
{

/**
 * Object representing a single camera device.
 *
 * CameraDevices are provided by a CameraProvider and are used to
 * access the camera devices available within the system.
 * Each device is based on a single sensor or a set of synchronized sensors.
 *
 * @see ICameraProvider::getCameraDevices
 *
 * @defgroup ArgusCameraDevice CameraDevice
 * @ingroup ArgusObjects
 */
class CameraDevice : public InterfaceProvider
{
protected:
    ~CameraDevice() {}
};

/**
 * @class ICameraProperties
 *
 * Interface to the core CameraDevice properties.
 *
 * @ingroup ArgusCameraDevice
 */
DEFINE_UUID(InterfaceID, IID_CAMERA_PROPERTIES, 436d2a73,c85b,4a29,bce5,15,60,6e,35,86,91);
class ICameraProperties : public Interface
{
public:
    static const InterfaceID& id() { return IID_CAMERA_PROPERTIES; }

    /**
     * Returns the camera UUID.
     * In UUID, clock_seq contains low 16 bits of GUID,
     * node[6] contains high 48 bits of GUID.
     */
    virtual UUID getUUID() const = 0;

    /**
     * Returns the camera sensor placement position on the module.
     */
    virtual SensorPlacement getSensorPlacement() const = 0;

    /**
     * Returns the maximum number of regions of interest supported by AE.
     * A value of 0 means that the entire image is the only supported region of interest.
     *
     * @see IAutoControlSettings::setAeRegions()
     */
    virtual uint32_t getMaxAeRegions() const = 0;

    /**
     * Returns the minimum size of resultant region required by AE.
     *
     * @see IAutoControlSettings::setAeRegions()
     */
    virtual Size2D<uint32_t> getMinAeRegionSize() const = 0;

    /**
     * Returns the maximum number of regions of interest supported by AWB.
     * A value of 0 means that the entire image is the only supported region of interest.
     *
     * @see IAutoControlSettings::setAwbRegions()
     */
    virtual uint32_t getMaxAwbRegions() const = 0;

    /**
     * Returns the maximum number of regions of interest supported by AF.
     * A value of 0 means that the entire image is the only supported region of interest.
     *
     * @see IAutoControlSettings::setAfRegions()
     */
    virtual uint32_t getMaxAfRegions() const = 0;

    /**
     * Returns only the basic available sensor modes that do not have an associated
     * extension. Basic mode types include Depth, RGB, YUV and Bayer types.
     *
     * @param[out] modes A vector that will be populated with the sensor modes.
     *
     * @returns success/status of the call.
     */
    virtual Status getBasicSensorModes(std::vector<SensorMode*>* modes) const = 0;

    /**
     * Returns all the available sensor modes including the ones that have extensions.
     * The extended modes support some form of Wide Dynamic Range (WDR) technology.
     *
     * All the returned modes will support the basic sensor mode interface.
     * @see ISensorMode
     *
     * @param[out] modes A vector that will be populated with the sensor modes.
     *
     * @returns success/status of the call.
     */
    virtual Status getAllSensorModes(std::vector<SensorMode*>* modes) const = 0;

    /**
     * Returns all the recommended aperture positions.
     *
     * @param[out] positions a vector that will be populated with the aperture positions.
     *
     * @returns success/status of the call.
     */
    virtual Status getAperturePositions(std::vector<int32_t>* positions) const = 0;

    /**
     * Returns all the available aperture f-numbers.
     *
     * @param[out] fnumbers  A pointer to a vector to be populated with the
     *                       aperture f-numbers.
     *
     * @returns success/status of the call.
     */
    virtual Status getAvailableApertureFNumbers(std::vector<float>* fnumbers) const = 0;

    /**
     * Returns the valid range of focuser positions.
     * The units are focuser steps.
     */
    virtual Range<int32_t> getFocusPositionRange() const = 0;

    /**
     * Returns the valid range of aperture positions.
     * The units are aperture positions.
     */
    virtual Range<int32_t> getAperturePositionRange() const = 0;

    /**
     * Returns the valid range of aperture step positions per second.
     * The units are aperture motor steps/second.
     */
    virtual Range<float> getApertureMotorSpeedRange() const = 0;

    /**
     * Returns the supported range of ISP digital gain.
     */
    virtual Range<float> getIspDigitalGainRange() const = 0;

    /**
     * Returns the supported range of Exposure Compensation.
     */
    virtual Range<float> getExposureCompensationRange() const = 0;

    /**
     * Returns the model name of the device.
     */
    virtual const std::string& getModelName() const = 0;

    /**
     * Returns the module string for the device.
     * Contains the device's name, position, and partial model number.
     * This string is unique for each device on the system.
     */
    virtual const std::string& getModuleString() const = 0;

protected:
    ~ICameraProperties() {}
};

/**
 * Child object representing a CameraDevice's sensor mode, returned by
 * ICameraProperties::getAllSensorModes
 *
 * @defgroup ArgusSensorMode SensorMode
 * @ingroup ArgusCameraDevice
 */
class SensorMode : public InterfaceProvider
{
protected:
    ~SensorMode() {}
};

/**
 * @class ISensorMode
 *
 * Interface to the core properties of a SensorMode.
 *
 * @ingroup ArgusSensorMode
 */
DEFINE_UUID(InterfaceID, IID_SENSOR_MODE, e69015e0,db2a,11e5,a837,18,00,20,0c,9a,66);
class ISensorMode : public Interface
{
public:
    static const InterfaceID& id() { return IID_SENSOR_MODE; }

    /**
     * Returns the image resolution, in pixels.
     */
    virtual Size2D<uint32_t> getResolution() const = 0;

    /**
     * Returns the sensor crop region
     */
    virtual Rectangle<uint32_t> getCrop() const = 0;

    /**
     * Returns the binning factor
     */
    virtual float getBinningFactor() const = 0;

    /**
     * Returns the supported exposure time range (in nanoseconds).
     */
    virtual Range<uint64_t> getExposureTimeRange() const = 0;

    /**
     * Returns the hdr ratio range, it is the ratio of long exposure vs short exposure.
     * When min and/or max is greater than 1, long exposure range is given by getExposureTimeRange()
     * and short exposure range is computed as following:
     * short exposure min = getExposureTimeRange().min() / getHdrRatioRange().max()
     * short exposure max = getExposureTimeRange().max() / getHdrRatioRange().min().
     */
    virtual Range<float> getHdrRatioRange() const = 0;

    /**
     * Returns the supported frame duration range (in nanoseconds).
     */
    virtual Range<uint64_t> getFrameDurationRange() const = 0;

    /**
     * Returns the supported analog gain range.
     */
    virtual Range<float> getAnalogGainRange() const = 0;

    /**
     * Returns the bit depth of the image captured by the image sensor in the
     * current mode.  For example, a wide dynamic range image sensor capturing
     * 16 bits per pixel would have an input bit depth of 16.
     */
    virtual uint32_t getInputBitDepth() const = 0;

    /**
     * Returns the bit depth of the image returned from the image sensor in the
     * current mode. For example, a wide dynamic range image sensor capturing
     * 16 bits per pixel might be connected through a Camera Serial Interface
     * (CSI-3) which is limited to 12 bits per pixel. The sensor would have to
     * compress the image internally and would have an output bit depth not
     * exceeding 12.
     */
    virtual uint32_t getOutputBitDepth() const = 0;

    /**
     * Describes the type of the sensor (Bayer, Yuv, etc.) and key modes of
     * operation which are enabled in the sensor mode (Wide-dynamic range,
     * Piecewise Linear Compressed output, etc.)
     */
    virtual SensorModeType getSensorModeType() const = 0;

    /**
     * Describes the bayer phase of Bayer mode
     */
    virtual BayerPhase getBayerPhase() const = 0;

    /**
     * Checks if the buffer provided is supported by the camera device.
     */
     virtual bool isBufferFormatSupported(Buffer* buffer) const = 0;
protected:
    ~ISensorMode() {}
};

} // namespace Argus

#endif // _ARGUS_CAMERA_DEVICE_H
