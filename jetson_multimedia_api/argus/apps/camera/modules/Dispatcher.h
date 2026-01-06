/*
 * Copyright (c) 2016-2019, NVIDIA CORPORATION. All rights reserved.
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

#ifndef DISPATCHER_H
#define DISPATCHER_H

#include <stdint.h>
#include <string>
#include <list>

#include <Argus/Argus.h>

#include "Value.h"
#include "Error.h"
#include "VideoPipeline.h"
#include "IObserver.h"
#include "Util.h"
#include "TrackedUniqueObject.h"
#include "UniquePointer.h"

namespace Argus { namespace Ext { class IDeFogSettings; } }

namespace ArgusSamples
{

class IObserverForInterface;

/**
 * The dispatcher is called by clients like the command line interface.
 * It provides functions to set properties like the camera device index or the sensor mode. It
 * also implements services to open devices, sessions and create requests.
 * It also maintains an internal capture session which is used when only a single session is
 * required.
 */
class Dispatcher
    : public IObserver
    , public Tracker<Argus::CaptureSession>
    , public Tracker<Argus::Request>
{
public:
    /**
     * Get the dispatcher instance.
     */
    static Dispatcher &getInstance();

    /**
     * Shutdown, free all resources
     */
    bool shutdown();

    /**
     * Returns whether or not an extension is supported.
     */
    bool supportsExtension(const Argus::ExtensionName& extension) const;

    /**
     * Get an information string
     */
    bool getInfo(std::string &info) const;

    /**
     * Get the sensor mode for a given index
     *
     * @param sensorModeIndex [in]
     * @param sensorMode [out]
     */
    bool getSensorMode(uint32_t sensorModeIndex, Argus::SensorMode **sensorMode) const;

    /**
     * Returns the range of focuser positions of the current device.
     * The units are focuser steps.
     */
    Argus::Range<int32_t> getDeviceFocusPositionRange() const;

    /**
     * Returns the range of aperture positions of the current device.
     * The units are aperture positions.
     */
    Argus::Range<int32_t> getDeviceAperturePositionRange() const;

    /**
     * Returns the range of aperture motor positions per second of the current device.
     * The units are aperture motor steps/second.
     */
    Argus::Range<float> getDeviceApertureMotorSpeedRange() const;

    /**
     * Get the output size
     */
    bool getOutputSize(Argus::Size2D<uint32_t> *size) const;

    /**
     * Get the amount of available camera devices
     */
    uint32_t getDeviceCount() const;

    /**
     * Create a capture session using the device index
     */
    bool createSession(TrackedUniqueObj<Argus::CaptureSession> &session, uint32_t deviceIndex);

    /**
     * Wait for events from the specific session.
     *
     * @param eventQueue [in] event queue to transfer events to
     * @param timeout [in] maximum time (in nanoseconds) to wait for new events.
     * @param session [in] capture session (optional, if NULL the internal session is used)
     */
    bool waitForEvents(Argus::EventQueue *eventQueue, TimeValue timeout = TimeValue::infinite(),
        Argus::CaptureSession *session = NULL);

    /**
     * Dump runtime information for a session.
     */
    bool dumpSessionInfo() const;

    /**
     * Create a request for a session
     *
     * @param request [out] created request
     * @param captureIntent [in] capture intent
     * @param session [in] capture session (optional, if NULL the internal session is used)
     */
    bool createRequest(TrackedUniqueObj<Argus::Request> &request,
        Argus::CaptureIntent captureIntent, Argus::CaptureSession *session = NULL);

    /**
     * create a event queue
     *
     * @param eventTypes [in]
     * @param eventQueue [out] created event queue
     * @param session [in] capture session (optional, if NULL the internal session is used)
     */
    bool createEventQueue(const std::vector<Argus::EventType> &eventTypes,
        Argus::UniqueObj<Argus::EventQueue> &eventQueue, Argus::CaptureSession *session = NULL);


    /**
     * Submits a single capture request for a session
     *
     * @param request [in] capture request
     * @param session [in] capture session (optional, if NULL the internal session is used)
     */
    bool capture(Argus::Request *request, Argus::CaptureSession *session = NULL);

    /**
     * Start a repeating request for a session
     *
     * @param request [in] request
     * @param session [in] capture session (optional, if NULL the internal session is used)
     */
    bool startRepeat(Argus::Request *request, Argus::CaptureSession *session = NULL);

    /**
     * Start a repeating burst request for a session
     *
     * @param requestList [in] burst request
     * @param session [in] capture session (optional, if NULL the internal session is used)
     */
    bool startRepeatBurst(const std::vector<const Argus::Request*>& requestList,
        Argus::CaptureSession *session = NULL);

    /**
     * Clears the repeating request for a session
     *
     * @param session [in] capture session (optional, if NULL the internal session is used)
     */
    bool stopRepeat(Argus::CaptureSession *session = NULL);

    /**
     * Restart the currently active requests to pick up changed settings.
     */
    bool restartActiveRequests();

    /**
     * Returns the maximum number of capture requests for a session that can be included in a
     * burst capture.
     *
     * @param session [in] capture session (optional, if NULL the internal session is used)
     */
    uint32_t maxBurstRequests(Argus::CaptureSession *session = NULL);

    /**
     * Wait until all pending captures for a session are complete
     *
     * @param session [in] capture session (optional, if NULL the internal session is used)
     */
    bool waitForIdle(Argus::CaptureSession *session = NULL);

    /**
     * Create an output stream of an request of a session.
     *
     * @param request [in] request
     * @param enableMetadata [in] whether or not to enable meatdata for the stream
     * @param stream [out] the created Argus output stream
     * @param session [in] capture session (optional, if NULL the internal session is used)
     */
    bool createOutputStream(Argus::Request *request, bool enableMetadata,
        Argus::UniqueObj<Argus::OutputStream> &stream,
        Argus::CaptureSession *session = NULL);

    /**
     * Enable an output stream of an request
     */
    bool enableOutputStream(Argus::Request *request, Argus::OutputStream *stream);

    /**
     * Disable an output stream of an request
     */
    bool disableOutputStream(Argus::Request *request, Argus::OutputStream *stream);

    /**
     * Output a message if verbose mode is enabled
     */
    bool message(const char *msg, ...);

private:
    // the current properties need to be initialized first, some Value<> members below use them
    // for the validator

    // current device properties
    std::vector<Argus::SensorMode*> m_sensorModes;      ///< sensor modes
    Value<Argus::Range<int32_t> > m_deviceFocusPositionRange;   ///< device focus position range
    Value<Argus::Range<int32_t> > m_deviceAperturePositionRange;   ///< device aperture position range
    std::vector<float> m_deviceApertureFnums;   ///< device aperture fnum available values
    Value<Argus::Range<float> > m_deviceApertureMotorSpeedRange;   ///< device aperture motor speed range
    Value<Argus::Range<float> > m_deviceExposureCompensationRange; ///< exposure compensation range
    Value<Argus::Range<Argus::Range<float> > >
        m_deviceIspDigitalGainRange;       ///< device isp digital gain range

    // current sensor mode properties
    Value<Argus::Range<Argus::Range<uint64_t> > >
        m_sensorExposureTimeRange;      ///< exposure time range
    Value<Argus::Range<Argus::Range<float> > >
        m_sensorAnalogGainRange;        ///< analog gain range
    Value<Argus::Range<float> > m_sensorFrameRateRange; ///< frame rate range

public:
    Value<uint32_t> m_deviceIndex;      ///< the device index
    Value<bool> m_deviceOpen;           ///< if set then the device is open
    Value<bool> m_sensorModeValid;

    Value<bool> m_verbose;              ///< if set verbose mode is enabled and messages are printed
    Value<bool> m_kpi;                  ///< if set kpi mode is enabled and kpi number are printed

    // source settings
    Value<Argus::Range<uint64_t> > m_exposureTimeRange; ///< exposure time range
    Value<Argus::Range<float> > m_gainRange; ///< gain range
    Value<uint32_t> m_sensorModeIndex;  ///< the sensor mode index
    Value<float> m_frameRate;           ///< in frames per second
    Value<int32_t> m_focusPosition;     ///< focus position
    Value<int32_t> m_aperturePosition;     ///< aperture position
    Value<float> m_apertureFnum;     ///< aperture position
    Value<float> m_apertureMotorSpeed;     ///< aperture motor steps/second
    Value<Argus::PixelFormat> m_captureYuvFormat;  ///< NV12 vs. P016 YUV color depth

    // denoise settings
    Value<Argus::DenoiseMode> m_denoiseMode; ///< denoise mode
    Value<float> m_denoiseStrength;          ///< denoise strength

    // edge enhance settings
    Value<Argus::EdgeEnhanceMode> m_edgeEnhanceMode; ///< edge enhancement mode
    Value<float> m_edgeEnhanceStrength;              ///< edge enhancement strength

    // auto control settings
    Value<Argus::AeAntibandingMode> m_aeAntibandingMode; ///< auto exposure antibanding mode
    Value<bool> m_aeLock;               ///< auto exposure lock
    Value<bool> m_awbLock;              ///< auto white balance lock
    Value<Argus::AwbMode> m_awbMode;    ///< auto white balance mode
    Value<float> m_exposureCompensation;///< exposure compensation
    Value<Argus::Range<float> > m_ispDigitalGainRange;///< ISP digital Gain Range
    Value<Argus::Range<uint32_t> > m_acRegionHorizontal; ///< auto control region horizontal
    Value<Argus::Range<uint32_t> > m_acRegionVertical; ///< auto control region vertical

    // still settings
    Value<StillFileType> m_stillFileType;    ///< the still image file format

    // video settings
    Value<VideoPipeline::VideoFormat> m_videoFormat;    ///< the video format
    Value<VideoPipeline::VideoFileType> m_videoFileType; ///< the video file type
    Value<uint32_t> m_videoBitRate;                     ///< the video bit rate

    // output settings
    Value<Argus::Size2D<uint32_t> > m_outputSize;    ///< output size
    Value<std::string> m_outputPath;                 ///< output path

    // DeFog extension
    Value<bool> m_deFogEnable;      ///< enable
    Value<float> m_deFogAmount;     ///< amount of fog to be removed. Range 0.0 - 1.0 (none - all)
    Value<float> m_deFogQuality;    ///< quality of the effect. Range 0.0 - 1.0 (low - high)

private:
    Dispatcher();
    ~Dispatcher();

    // this is a singleton, hide copy constructor etc.
    Dispatcher(const Dispatcher&);
    Dispatcher& operator=(const Dispatcher&);

    /**
     * Initialize the dispatcher
     */
    bool initialize();

    /**
     * Create the internal session
     */
    bool createSession();

    /**
     * Close the internal session
     */
    bool closeSession();

    /**
     * Start tracking a session
     */
    bool track(Argus::CaptureSession *session);

    /**
     * No longer track a session
     */
    bool untrack(Argus::CaptureSession *session);

    /**
     * Start tracking a request
     */
    bool track(Argus::Request *request);

    /**
     * No longer track a request
     */
    bool untrack(Argus::Request *request);

    /**
     * Register an IDenoiseSettings observer
     */
    bool registerObserver(Argus::IDenoiseSettings *iDenoiseSettings);

    /**
     * Register an IEdgeEnhanceSettings observer
     */
    bool registerObserver(Argus::IEdgeEnhanceSettings *iEdgeEnhanceSettings);

    /**
     * Register an ISourceSettings observer
     */
    bool registerObserver(Argus::ISourceSettings *iSourceSettings);

    /**
     * Register an IAutoControlSettings observer
     */
    bool registerObserver(Argus::IAutoControlSettings *iAutoControlSettings);

    /**
     * Register an IDeFogSettings observer
     */
    bool registerObserver(Argus::Ext::IDeFogSettings *iDeFogSettings);

    /**
     * Unregister an interface which had been registered as an observer.
     */
    bool unregisterObserver(Argus::Interface *interface);

    /**
     * Callback when the device index changes.
     */
    bool onDeviceIndexChanged(const Observed &source);

    /**
     * Callback when the sensor mode index changes.
     */
    bool onSensorModeIndexChanged(const Observed &source);

    bool m_initialized;             ///< if set the dispatcher is initialized

    std::list<IObserverForInterface*> m_observers;

    Argus::UniqueObj<Argus::CameraProvider> m_cameraProvider; ///< camera provider
    Argus::ICameraProvider *m_iCameraProvider; ///< camera provider interface

    std::vector<Argus::CameraDevice*> m_cameraDevices; ///< a list of available devices

    TrackedUniqueObj<Argus::CaptureSession> m_captureSession; ///< internal capture session

    class ActiveSession
    {
    public:
        ActiveSession(Argus::CaptureSession *session)
            : m_session(session)
        {
        }

        Argus::CaptureSession *m_session;
        std::vector<const Argus::Request*> m_requests;
    };
    typedef std::list<ActiveSession> ActiveSessionList;
    ActiveSessionList m_activeSessions;
};

}; // namespace ArgusSamples

#endif // DISPATCHER_H
