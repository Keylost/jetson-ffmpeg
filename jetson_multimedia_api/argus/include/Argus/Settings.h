/*
 * Copyright (c) 2016-2023, NVIDIA CORPORATION. All rights reserved.
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
 * <b>Libargus API: Settings API</b>
 *
 * @b Description: This file defines the settings that control the sensor module.
 */

#ifndef _ARGUS_SETTINGS_H
#define _ARGUS_SETTINGS_H

namespace Argus
{

/**
 * @class ISourceSettings
 *
 * Interface to the source settings (provided by IRequest::getSourceSettings()).
 *
 * @ingroup ArgusSourceSettings
 */
DEFINE_UUID(InterfaceID, IID_SOURCE_SETTINGS, eb7ae38c,3c62,4161,a92a,a6,4f,ba,c6,38,83);
class ISourceSettings : public Interface
{
public:
    static const InterfaceID& id() { return IID_SOURCE_SETTINGS; }

    /**
     * Sets the exposure time range of the source, in nanoseconds.
     * If the exposure range is outside of the available range, the capture's exposure time
     * will be as close as possible to the exposure range specified.
     * @param[in] exposureTimeRange Exposure time range, in nanoseconds.
     * @see ISensorMode::getExposureTimeRange()
     * @todo Document implications of quantization.
     *
     * @returns success/status of the call.
     */
    virtual Status setExposureTimeRange(const Range<uint64_t>& exposureTimeRange) = 0;

    /**
     * Returns the exposure time range of the source, in nanoseconds.
     */
    virtual Range<uint64_t> getExposureTimeRange() const = 0;

    /**
     * Sets the focus position, in focuser units. If the position
     * is set outside of the focuser limits, the position will be clamped.
     * @param[in] position The new focus position, in focuser units.
     * @see ICameraProperties::getFocusPositionRange()
     *
     * @returns success/status of the call.
     */
    virtual Status setFocusPosition(int32_t position) = 0;

    /**
     * Returns the focus position, in focuser units.
     */
    virtual int32_t getFocusPosition() const = 0;

    /**
     * Sets the aperture position. If the position is not valid,
     * error will be returned.
     * @param[in] position The new aperture position.
     * @see ICameraProperties::getAperturePositionRange()
     *
     * @returns success/status of the call.
     */
    virtual Status setAperturePosition(int32_t position) = 0;

    /**
     * Returns the aperture position.
     */
    virtual int32_t getAperturePosition() const = 0;

    /**
     * Sets the aperture motor speed in motor steps/second. If the speed
     * is set outside of the speed limits, the speed will be clamped.
     * @param[in] speed The new speed.
     * @see ICameraProperties::getApertureMotorSpeedRange()
     *
     * @returns success/status of the call.
     */
    virtual Status setApertureMotorSpeed(float speed) = 0;

    /**
     * Returns the aperture motor speed in motor steps/second.
     */
    virtual float getApertureMotorSpeed() const = 0;

    /**
     * Sets the aperture f-number. If the f-number is not valid,
     * error will be returned.
     * @param[in] fnumber The new f-number.
     *
     * @returns success/status of the call.
     */
    virtual Status setApertureFNumber(float fnumber) = 0;

    /**
     * Returns the aperture f-number.
     */
    virtual float getApertureFNumber() const = 0;

    /**
     * Sets the frame duration range, in nanoseconds.
     * If frame range is out of bounds of the current sensor mode,
     * the capture's frame duration will be as close as possible to the range specified.
     * @param[in] frameDurationRange Frame duration range, in nanoseconds
     * @see ISensorMode::getFrameDurationRange()
     *
     * @returns success/status of the call.
     */
    virtual Status setFrameDurationRange(const Range<uint64_t>& frameDurationRange) = 0;

    /**
     * Returns the frame duration range, in nanoseconds.
     */
    virtual Range<uint64_t> getFrameDurationRange() const = 0;

    /**
     * Sets the gain range for the sensor.
     * The range has to be within the max and min reported in the CameraProperties
     * Otherwise the range will be clipped.
     * @param[in] gainRange scalar gain range
     * @see ISensorMode::getAnalogGainRange()
     *
     * @returns success/status of the call.
     */
    virtual Status setGainRange(const Range<float>& gainRange) = 0;

    /**
     * Returns the gain range.
     */
    virtual Range<float> getGainRange() const = 0;

    /**
     * Sets the sensor mode.
     * Note that changing sensor mode from one capture to the next may result in
     * multiple sensor frames being dropped between the two captures.
     * @param[in] mode Desired sensor mode for the capture.
     * @see ICameraProperties::getAllSensorModes()
     *
     * @returns success/status of the call.
     */
    virtual Status setSensorMode(SensorMode* mode) = 0;

    /**
     * Returns the sensor mode.
     */
    virtual SensorMode* getSensorMode() const = 0;

    /**
     * Sets the user-specified optical black levels.
     * These values will be ignored unless <tt>getOpticalBlackEnable() == true</tt>
     * Values are floating point in the range [0,1) normalized based on sensor bit depth.
     * @param[in] opticalBlackLevels opticalBlack levels in range [0,1) per bayer phase
     *
     * @returns success/status of the call.
     */
    virtual Status setOpticalBlack(const BayerTuple<float>& opticalBlackLevels) = 0;

    /**
     * Returns user-specified opticalBlack level per bayer phase.
     *
     * @returns opticalBlackLevels
     */
    virtual BayerTuple<float> getOpticalBlack() const = 0;

    /**
     * Sets whether or not user-provided optical black levels are used.
     * @param[in] enable If @c true, Argus will use the user-specified optical black levels.
     * @see setOpticalBlack()
     * If @c false, the Argus implementation will choose the optical black values.
     *
     * @returns success/status of the call.
     */
    virtual Status setOpticalBlackEnable(bool enable) = 0;

    /**
     * Returns whether user-specified optical black levels are enabled.
     * If false, the Argus implementation will choose the optical black values.
     * @see setOpticalBlackEnable()
     *
     * @returns enable
     */
    virtual bool getOpticalBlackEnable() const = 0;


protected:
    ~ISourceSettings() {}
};

/**
 * @class IAutoControlSettings
 *
 * Interface to the auto control settings (provided by IRequest::getAutoControlSettings()).
 *
 * @ingroup ArgusAutoControlSettings
 */
DEFINE_UUID(InterfaceID, IID_AUTO_CONTROL_SETTINGS, 1f2ad1c6,cb13,440b,bc95,3f,fd,0d,19,91,db);
class IAutoControlSettings : public Interface
{
public:
    static const InterfaceID& id() { return IID_AUTO_CONTROL_SETTINGS; }

    /**
     * Sets the AE antibanding mode.
     * @param[in] mode The requested antibanding mode.
     *
     * @returns success/status of the call.
     */
    virtual Status setAeAntibandingMode(const AeAntibandingMode& mode) = 0;

    /**
     * Returns the AE antibanding mode.
     */
    virtual AeAntibandingMode getAeAntibandingMode() const = 0;

    /**
     * Sets the AE mode.
     * @param[in] mode The new AE mode.
     *
     * @returns success/status of the call.
     */
    virtual Status setAeMode(const AeMode& mode) = 0;

    /**
     * Returns the AE mode.
     */
    virtual AeMode getAeMode() const = 0;

    /**
     * Sets the AE lock.  When locked, AE will maintain constant exposure.
     * @param[in] lock If @c true, locks AE at its current exposure.
     *
     * @returns success/status of the call.
     */
    virtual Status setAeLock(bool lock) = 0;

    /**
     * Returns the AE lock.
     */
    virtual bool getAeLock() const = 0;

    /**
     * Sets the AE regions of interest.
     * If no regions are specified, the region of interest will be determined by device
     * and obtain by CameraMetadata::getAeRegions.
     * @param[in] regions The AE regions of interest.
     * The maximum number of regions is returned by @c ICameraProperties::getMaxAeRegions().
     * The minimum supported size of resultatnt region is returned by
     * @c ICameraProperties::getMinAeRegionSize().
     *
     * @returns success/status of the call.
     */
    virtual Status setAeRegions(const std::vector<AcRegion>& regions) = 0;

    /**
     * Returns the AE regions of interest.
     * @param[out] regions A vector that will be populated with the AE regions of interest.
     *
     * @returns success/status of the call.
     */
    virtual Status getAeRegions(std::vector<AcRegion>* regions) const = 0;

    /**
     * Sets the bayer histogram region of interest.
     * If no region is specified, the entire image is the region of interest.
     * @param[in] region The bayer histogram region of interest.
     *
     * @returns success/status of the call.
     */
    virtual Status setBayerHistogramRegion(const Rectangle<uint32_t>& region) = 0;

    /**
     * Returns the rectangle of the bayer histogram region of interest.
     */
    virtual Rectangle<uint32_t> getBayerHistogramRegion() const = 0;

    /**
     * Sets the AWB lock.
     * @param[in] lock If @c true, locks AWB at its current state.
     *
     * @returns success/status of the call.
     */
    virtual Status setAwbLock(bool lock) = 0;

    /**
     * Returns the AWB lock.
     */
    virtual bool getAwbLock() const = 0;

    /**
     * Sets the AWB mode.
     * @param[in] mode The new AWB mode.
     *
     * @returns success/status of the call.
     */
    virtual Status setAwbMode(const AwbMode& mode) = 0;

    /**
     * Returns the AWB mode.
     */
    virtual AwbMode getAwbMode() const = 0;

    /**
     * Sets the AWB regions of interest.
     * If no regions are specified, the region of interest will be determined by device
     * and obtain by CameraMetadata::getAwbRegions.
     * @param[in] regions The AWB regions of interest.
     * The maximum number of regions is returned by @c ICameraProperties::getMaxAwbRegions().
     *
     * @returns success/status of the call.
     */
    virtual Status setAwbRegions(const std::vector<AcRegion>& regions) = 0;

    /**
     * Returns the AWB regions of interest.
     * @param[out] regions A vector that will be populated with the AWB regions of interest.
     *
     * @returns success/status of the call.
     */
    virtual Status getAwbRegions(std::vector<AcRegion>* regions) const = 0;

    /**
     * Sets the AF mode.
     * @param[in] mode The new AF mode.
     *
     * @returns success/status of the call.
     */
    virtual Status setAfMode(const AfMode& mode) = 0;

    /**
     * Returns the AF mode.
     */
    virtual AfMode getAfMode() const = 0;

    /**
     * Sets the AF regions of interest.
     * If no regions are specified, the region of interest will be determined by device
     * and obtain by CameraMetadata::getAfRegions.
     * @param[in] regions The AF regions of interest.
     * The maximum number of regions is returned by @c ICameraProperties::getMaxAfRegions().
     *
     * @returns success/status of the call.
     */
    virtual Status setAfRegions(const std::vector<AcRegion>& regions) = 0;

    /**
     * Returns the AF regions of interest.
     * @param[out] regions A vector that will be populated with the AF regions of interest.
     *
     * @returns success/status of the call.
     */
    virtual Status getAfRegions(std::vector<AcRegion>* regions) const = 0;

    /**
     * Sets the Manual White Balance gains.
     * @param[in] gains The Manual White Balance Gains
     *
     * @returns success/status of the call.
     */
    virtual Status setWbGains(const BayerTuple<float>& gains) = 0;

    /**
     * Returns the Manual White Balance gains.
     *
     * @returns Manual White Balance Gains structure
     */
    virtual BayerTuple<float> getWbGains() const = 0;

    /**
     * Returns the size of the color correction matrix.
     */
    virtual Size2D<uint32_t> getColorCorrectionMatrixSize() const = 0;

    /**
     * Sets the user-specified color correction matrix.
     * This matrix will be ignored unless <tt>getColorCorrectionMatrixEnable() == true</tt>.
     * The active color correction matrix used for image processing may be internally modified
     * to account for the active color saturation value (either user-specified or automatically
     * generated, after biasing, @see setColorSaturation and @see setColorSaturationBias).
     * @param[in] matrix A color correction matrix that maps sensor RGB to linear sRGB. This matrix
     *                   is given in row-major order and must have the size w*h, where w and h are
     *                   the width and height of the Size returned by getColorCorrectionMatrixSize()
     *
     * @returns success/status of the call.
     */
    virtual Status setColorCorrectionMatrix(const std::vector<float>& matrix) = 0;

    /**
     * Returns the user-specified color correction matrix.
     * @param[out] matrix A matrix that will be populated with the CCM.
     *
     * @returns success/status of the call.
     */
    virtual Status getColorCorrectionMatrix(std::vector<float>* matrix) const = 0;

    /**
     * Enables the user-specified color correction matrix.
     * @param[in] enable If @c true, libargus uses the user-specified matrix.
     * @see setColorCorrectionMatrix()
     *
     * @returns success/status of the call.
     */
    virtual Status setColorCorrectionMatrixEnable(bool enable) = 0;

    /**
     * Returns the enable for the user-specified color correction matrix.
     */
    virtual bool getColorCorrectionMatrixEnable() const = 0;

    /**
     * Sets the user-specified absolute color saturation. This must be enabled via
     * @see setColorSaturationEnable, otherwise saturation will be determined automatically.
     * This saturation value may be used to modify the color correction matrix used
     * for processing (@see setColorCorrectionMatrix), and these changes will be reflected
     * in the color correction matrix output to the capture metadata.
     * @param[in] saturation The absolute color saturation. Acceptable values are in
     *                       [0.0, 2.0], and the default value is 1.0.

     * @returns success/status of the call.
     */
    virtual Status setColorSaturation(float saturation) = 0;

    /**
     * Returns the user-specified absolute color saturation (@see setColorSaturation).
     */
    virtual float getColorSaturation() const = 0;

    /**
     * Enables the user-specified absolute color saturation.
     * @param[in] enable If @c true, libargus uses the user-specified color saturation.
     * @see setColorSaturation()
     *
     * @returns success/status of the call.
     */
    virtual Status setColorSaturationEnable(bool enable) = 0;

    /**
     * Returns the enable for the user-specified color saturation.
     */
    virtual bool getColorSaturationEnable() const = 0;

    /**
     * Sets the color saturation bias. This bias is used to multiply the active saturation
     * value, either the user-specified or the automatically generated value depending on the state
     * of @see getColorSaturationEnable, and produces the final saturation value to use for
     * capture processing. This is used primarily to tweak automatically generated saturation
     * values when the application prefers more or less saturation than what the implementation
     * or hardware generates by default. The final saturation value (after biasing) may affect the
     * color correction matrix used for processing (@see setColorCorrectionMatrix).
     * @param[in] bias The color saturation bias. Acceptable values are in [0.0, 2.0], where
     *            1.0 does not modify the saturation (default), 0.0 is fully desaturated
     *            (greyscale), and 2.0 is highly saturated.
     *
     * @returns success/status of the call.
     */
    virtual Status setColorSaturationBias(float bias) = 0;

    /**
     * Returns the color saturation bias.
     */
    virtual float getColorSaturationBias() const = 0;

    /**
     * Sets the exposure compensation.
     * Exposure compensation is applied after AE is solved.
     * @param[in] ev The exposure adjustment step in stops.
     *
     * @returns success/status of the call.
     */
    virtual Status setExposureCompensation(float ev) = 0;

    /**
     * Returns the exposure compensation.
     */
    virtual float getExposureCompensation() const = 0;

    /**
     * Returns the number of elements required for the tone map curve.
     * @param[in] channel The color channel the curve size corresponds to.
     */
    virtual uint32_t getToneMapCurveSize(RGBChannel channel) const = 0;

    /**
     * Sets the user-specified tone map curve for a channel on the stream.
     * The user-specified tone map will be ignored unless <tt>getToneMapCurveEnable() == true</tt>.
     * @param[in] channel The color the curve corresponds to.
     * @param[in] curve A float vector that describes the LUT.
     * The number of elements must match the number of elements
     * returned from getToneMapCurve() of the same channel.
     *
     * @returns success/status of the call.
     */
    virtual Status setToneMapCurve(RGBChannel channel, const std::vector<float>& curve) = 0;

    /**
     * Returns the user-specified tone map curve for a channel on the stream.
     * @param[in] channel The color the curve corresponds to.
     * @param[out] curve A vector that will be populated by the tone map curve for the specified
     *             color channel.
     *
     * @returns success/status of the call.
     */
    virtual Status getToneMapCurve(RGBChannel channel, std::vector<float>* curve) const = 0;

    /**
     * Enables the user-specified tone map.
     * @param[in] enable If @c true, libargus uses the user-specified tone map.
     *
     * @returns success/status of the call.
     */
    virtual Status setToneMapCurveEnable(bool enable) = 0;

    /**
     * Returns the enable for the user-specified tone map.
     */
    virtual bool getToneMapCurveEnable() const = 0;

     /**
     * Sets the user-specified Isp Digital gain range.
     * @param[in] gain The user-specified Isp Digital gain.
     *
     * @returns success/status of the call.
     */
    virtual Status setIspDigitalGainRange(const Range<float>& gain) = 0;

    /**
     * Returns the user-specified Isp Digital gain range.
     *
     * @returns Isp Digital gain
     */
    virtual Range<float> getIspDigitalGainRange() const = 0;

protected:
    ~IAutoControlSettings() {}
};

/**
 * @class IStreamSettings
 *
 * Interface to per-stream settings (provided by IRequest::getStreamSettings()).
 *
 * @ingroup ArgusStreamSettings
 */
DEFINE_UUID(InterfaceID, IID_STREAM_SETTINGS, c477aeaf,9cc8,4467,a834,c7,07,d7,b6,9f,a4);
class IStreamSettings : public Interface
{
public:
    static const InterfaceID& id() { return IID_STREAM_SETTINGS; }

    /**
     * Sets the clip rectangle for the stream.
     * A clip rectangle is a normalized rectangle
     * with valid coordinates contained in the [0.0,1.0] range.
     * @param[in] clipRect The clip rectangle.
     *
     * @returns success/status of the call.
     */
    virtual Status setSourceClipRect(const Rectangle<float>& clipRect) = 0;

    /**
     * Returns the clip rectangle for the stream.
     */
    virtual Rectangle<float> getSourceClipRect() const = 0;

    /**
     * Sets whether or not post-processing is enabled for this stream.
     * Post-processing features are controlled on a per-Request basis and all streams share the
     * same post-processing control values, but this enable allows certain streams to be excluded
     * from all post-processing. The current controls defined to be a part of "post-processing"
     * includes (but may not be limited to):
     *   - Denoise
     * Default value is true.
     */
    virtual void setPostProcessingEnable(bool enable) = 0;

    /**
     * Returns the post-processing enable for the stream.
     */
    virtual bool getPostProcessingEnable() const = 0;

protected:
    ~IStreamSettings() {}
};

/**
 * @class IDenoiseSettings
 *
 * Interface to denoise settings.
 *
 * @ingroup ArgusRequest
 */
DEFINE_UUID(InterfaceID, IID_DENOISE_SETTINGS, 7A461D20,6AE1,11E6,BDF4,08,00,20,0C,9A,66);
class IDenoiseSettings : public Interface
{
public:
    static const InterfaceID& id() { return IID_DENOISE_SETTINGS; }

    /**
     * Sets the denoise (noise reduction) mode for the request.
     * @param[in] mode The denoise mode:
     *              OFF: Denoise algorithms are disabled.
     *              FAST: Noise reduction will be enabled, but it will not slow down
     *                    the capture rate.
     *              HIGH_QUALITY: Maximum noise reduction will be enabled to achieve
     *                            the highest quality, but may slow down the capture rate.
     * @returns success/status of the call.
     */
    virtual Status setDenoiseMode(const DenoiseMode& mode) = 0;

    /**
     * Returns the denoise mode for the request.
     */
    virtual DenoiseMode getDenoiseMode() const = 0;

    /**
     * Sets the strength for the denoise operation.
     * @param[in] strength The denoise strength. This must be within the range [0.0, 1.0], where
     *            0.0 is the least and 1.0 is the most amount of noise reduction that can be
     *            applied. This denoise strength is relative to the current noise reduction mode;
     *            using a FAST denoise mode with a full strength of 1.0 may not perform as well
     *            as using a HIGH_QUALITY mode with a lower relative strength.
     * @returns success/status of the call.
     */
    virtual Status setDenoiseStrength(float strength) = 0;

    /**
     * Returns the denoise strength.
     */
    virtual float getDenoiseStrength() const = 0;

protected:
    ~IDenoiseSettings() {}
};

/**
 * @class IEdgeEnhanceSettings
 *
 * Interface to edge enhancement settings.
 *
 * @ingroup ArgusRequest
 */
DEFINE_UUID(InterfaceID, IID_EDGE_ENHANCE_SETTINGS, 7A461D21,6AE1,11E6,BDF4,08,00,20,0C,9A,66);
class IEdgeEnhanceSettings : public Interface
{
public:
    static const InterfaceID& id() { return IID_EDGE_ENHANCE_SETTINGS; }

    /**
     * Sets the edge enhancement mode for the request.
     * @param[in] mode The edge enhancement mode:
     *              OFF: Edge enhancement algorithms are disabled.
     *              FAST: Edge enhancement will be enabled, but it will not slow down
     *                    the capture rate.
     *              HIGH_QUALITY: Maximum edge enhancement will be enabled to achieve
     *                            the highest quality, but may slow down the capture rate.
     * @returns success/status of the call.
     */
    virtual Status setEdgeEnhanceMode(const EdgeEnhanceMode& mode) = 0;

    /**
     * Returns the edge enhancement mode for the request.
     */
    virtual EdgeEnhanceMode getEdgeEnhanceMode() const = 0;

    /**
     * Sets the strength for the edge enhancement operation.
     * @param[in] strength The edge enhancement strength. This must be within the range [0.0, 1.0],
     *            where 0.0 is the least and 1.0 is the most amount of edge enhancement that can be
     *            applied. This strength is relative to the current edge enhancement mode; using
     *            a FAST edge enhancement mode with a full strength of 1.0 may not perform as well
     *            as using a HIGH_QUALITY mode with a lower relative strength.
     * @returns success/status of the call.
     */
    virtual Status setEdgeEnhanceStrength(float strength) = 0;

    /**
     * Returns the edge enhancement strength.
     */
    virtual float getEdgeEnhanceStrength() const = 0;

protected:
    ~IEdgeEnhanceSettings() {}
};

} // namespace Argus

#endif // _ARGUS_SETTINGS_H
