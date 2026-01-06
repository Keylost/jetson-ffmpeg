/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
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
 * <b>Libargus Extension: Sync Sensor Calibration Data API</b>
 *
 * @b Description: This file defines the SyncSensorCalibrationData extension.
 */

#ifndef _ARGUS_SYNC_SENSOR_CALIBRATION_DATA_H
#define _ARGUS_SYNC_SENSOR_CALIBRATION_DATA_H

namespace Argus
{

/**
 * The DistortionType of a sync sensor defines the type of distortion model.
 */
DEFINE_NAMED_UUID_CLASS(DistortionType);
DEFINE_UUID(DistortionType, DISTORTION_TYPE_POLYNOMIAL,        23e59580,17ff,11eb,8b6f,08,00,20,0c,9a,66);
DEFINE_UUID(DistortionType, DISTORTION_TYPE_FISHEYE,           23e59581,17ff,11eb,8b6f,08,00,20,0c,9a,66);
DEFINE_UUID(DistortionType, DISTORTION_TYPE_OMINI_DIRECTIONAL, 23e5bc90,17ff,11eb,8b6f,08,00,20,0c,9a,66);

/**
 * The MappingType of a sync sensor defines the type of mapping used for fisheye distortion model.
 */
DEFINE_NAMED_UUID_CLASS(MappingType);
DEFINE_UUID(MappingType, MAPPING_TYPE_EQUIDISTANT,   9e7f3c10,17ff,11eb,8b6f,08,00,20,0c,9a,66);
DEFINE_UUID(MappingType, MAPPING_TYPE_EQUISOLID,     9e7f3c11,17ff,11eb,8b6f,08,00,20,0c,9a,66);
DEFINE_UUID(MappingType, MAPPING_TYPE_ORTHOGRAPHIC,  9e7f3c12,17ff,11eb,8b6f,08,00,20,0c,9a,66);
DEFINE_UUID(MappingType, MAPPING_TYPE_STEREOGRAPHIC, 9e7f3c13,17ff,11eb,8b6f,08,00,20,0c,9a,66);

/**
   * Adds accessors for sync sensor calibration data.
   *   - Ext::ISyncSensorCalibrationData : Accesses the sync sensor calibration data.
   *
   * @defgroup ArgusExtSyncSensorCalibrationData Ext::SyncSensorCalibrationData
   * @ingroup ArgusExtensions
   */
DEFINE_UUID(ExtensionName, EXT_SYNC_SENSOR_CALIBRATION_DATA, 10845a70,d52f,11ea,8b6e,08,00,20,0c,9a,66);
namespace Ext
{
/**
 * @class ISyncSensorCalibrationData
 *
 * Interface used to access sync sensor calibration data.
 *
 * @ingroup ArgusCameraDevice ArgusExtSyncSensorCalibrationData
 */
DEFINE_UUID(InterfaceID, IID_SYNC_SENSOR_CALIBRATION_DATA, 5925f360,d52f,11ea,8b6e,08,00,20,0c,9a,66);
class ISyncSensorCalibrationData : public Interface
{
public:
    static const InterfaceID& id() { return IID_SYNC_SENSOR_CALIBRATION_DATA; }

    /**
     * Returns the sync sensor module id in the provided memory location.
     * The maximum supported length of sync sensor id string is 32.
     * @param [in,out] syncSensorId Pointer for getting the sync sensor id string associated
     * with sensor.
     * @param [in] size The size of the syncSensorId.
     */
    virtual Status getSyncSensorModuleId(void* syncSensorId, size_t size) const = 0;

    /**
     * Returns the size of the image in pixels.
     */
    virtual Size2D<uint32_t> getImageSizeInPixels() const = 0;

    /**
     * Returns the focal length fx and fy from intrinsic parameters.
     */
    virtual Point2D<float> getFocalLength() const = 0;

    /**
     * Returns the skew from intrinsic parameters.
     */
    virtual float getSkew() const = 0;

    /**
     * Returns the principal point (optical center) x and y from intrinsic parameters.
     */
    virtual Point2D<float> getPrincipalPoint() const = 0;

    /**
     * Returns the lens distortion type as per the model being used.
     */
    virtual DistortionType getLensDistortionType() const = 0;

    /**
     * Returns the mapping type in case of fisheye distortion.
     */
    virtual MappingType getFisheyeMappingType() const = 0;

    /**
     * Returns the radial coefficients count in case of polynomial or fisheye distortion.
     *
     * @param[in] distortionType The lens distortion type.
     */
    virtual uint32_t getRadialCoeffsCount(const DistortionType& distortionType) const = 0;

    /**
     * Returns the radial coefficients vector as per distortion type and
     * size of the vector is given by getRadialCoeffsCount().
     *
     * @param[out] k The radial coefficient vector from distortion properties.
     *
     * @param[in] distortionType The lens distortion type.
     */
    virtual Status getRadialCoeffs(std::vector<float>* k,
        const DistortionType& distortionType) const = 0;

    /**
     * Returns the tangential coefficients count in case of polynomial distortion.
     */
    virtual uint32_t getTangentialCoeffsCount() const = 0;

    /**
     * Returns the tangential coefficients in case of polynomial distortion and
     *  size of the vector is given by getTangentialCoeffsCount().
     *
     * @param[out] p The tangential coefficient vector from distortion properties.
     */
    virtual Status getTangentialCoeffs(std::vector<float>* p) const = 0;

    /**
     * Returns the rotation parameter expressed in Rodrigues notation from extrinsic parameters.
     * angle = sqrt(rx^2+ry^2+rz^2).
     * unit axis = [rx,ry,rz]/angle.
     */
    virtual Point3D<float> getRotationParams() const = 0;

    /**
     * Returns the translation parameters in x, y and z co-ordinates with respect to a
     * reference point from extrinsic params.
     */
    virtual Point3D<float> getTranslationParams() const = 0;

    /**
     * Returns whether IMU sensor is present or not.
     */
    virtual bool isImuSensorAvailable() const = 0;

    /**
     * Returns the linear acceleration bias for all three axes x, y and z of the IMU device.
     */
    virtual Point3D<float> getLinearAccBias() const = 0;

    /**
     * Returns the angular velocity bias for all three axes x, y and z of the IMU device.
     */
    virtual Point3D<float> getAngularVelocityBias() const = 0;

    /**
     * Returns the gravity acceleration for all three axes x, y and z of the IMU device.
     */
    virtual Point3D<float> getGravityAcc() const = 0;

    /**
     * Returns the IMU rotation parameter expressed in Rodrigues notation from extrinsic parameters.
     * angle = sqrt(rx^2+ry^2+rz^2).
     * unit axis = [rx,ry,rz]/angle.
     */
    virtual Point3D<float> getImuRotationParams() const = 0;

    /**
     * Returns the IMU translation parameters in x, y and z co-ordinates with respect to a
     * reference point from extrinsic params.
     */
    virtual Point3D<float> getImuTranslationParams() const = 0;

protected:
  ~ISyncSensorCalibrationData() {}

};

} // namespace Ext

} // namespace Argus

#endif // _ARGUS_SYNC_SENSOR_CALIBRATION_DATA_H

