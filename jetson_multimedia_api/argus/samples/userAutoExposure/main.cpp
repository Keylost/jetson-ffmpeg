/*
 * Copyright (c) 2016-2021, NVIDIA CORPORATION. All rights reserved.
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
#include "Error.h"
#include <stdio.h>
#include <stdlib.h>
#include <Argus/Argus.h>
#include <Argus/Ext/BayerAverageMap.h>
#include <EGLStream/EGLStream.h>
#include "PreviewConsumer.h"
#include "CommonOptions.h"
#include <algorithm>
#include <math.h>
#include <Argus/Ext/BlockingSessionCameraProvider.h>

#define USER_AUTO_EXPOSURE_PRINT(...) \
        (printf("USER AUTO EXPOSURE SAMPLE: " __VA_ARGS__),fflush(stdout))

#define EXIT_IF_TRUE(val,msg)   \
        {if ((val)) {USER_AUTO_EXPOSURE_PRINT("%s\n",msg); return false;}}
#define EXIT_IF_NULL(val,msg)   \
        {if (!val) {USER_AUTO_EXPOSURE_PRINT("%s\n",msg); return false;}}
#define EXIT_IF_NOT_OK(val,msg) \
        {if (val!=Argus::STATUS_OK) {USER_AUTO_EXPOSURE_PRINT("%s\n",msg); return false;}}

using namespace Argus;

namespace ArgusSamples
{

// Globals.
EGLDisplayHolder g_display;

// Constants.
const float TARGET_EXPOSURE_LEVEL = 0.18f;
const float TARGET_EXPOSURE_LEVEL_RANGE_TOLERANCE = 0.01f;
const float BAYER_CLIP_COUNT_MAX = 0.10f;
const float CENTER_WEIGHTED_DISTANCE = 10.0f;
const float CENTER_WEIGHT = 50.0f;

/*******************************************************************************
 * Extended options class to add additional options specific to this sample.
 ******************************************************************************/
class UserAutoExposureSampleOptions : public CommonOptions
{
public:
    UserAutoExposureSampleOptions(const char *programName)
        : CommonOptions(programName,
                        ArgusSamples::CommonOptions::Option_D_CameraDevice |
                        ArgusSamples::CommonOptions::Option_M_SensorMode |
                        ArgusSamples::CommonOptions::Option_R_WindowRect |
                        ArgusSamples::CommonOptions::Option_F_FrameCount)
        , m_useAverageMap(false)
    {
        addOption(createValueOption
            ("useaveragemap", 'a', "0 or 1", "Use Average Map (instead of Bayer Histogram).",
             m_useAverageMap));
    }

    bool useAverageMap() const { return m_useAverageMap.get(); }

protected:
    Value<bool> m_useAverageMap;
};

/**
 * RAII class for app teardown
 */
class UserAutoExposureTeardown
{
public:
    CameraProvider* m_cameraProvider;
    PreviewConsumerThread* m_previewConsumerThread;
    OutputStream* m_stream;

    UserAutoExposureTeardown()
    {
        m_cameraProvider = NULL;
        m_previewConsumerThread = NULL;
        m_stream = NULL;
    }

    ~UserAutoExposureTeardown()
    {
        shutdown();
    }

private:
    void shutdown()
    {
        // Destroy the output streams (stops consumer threads).
        if (m_stream != NULL)
            m_stream->destroy();

        // Wait for the consumer threads to complete.
        if (m_previewConsumerThread != NULL)
            PROPAGATE_ERROR_CONTINUE(m_previewConsumerThread->shutdown());

        // Shut down Argus.
        if (m_cameraProvider != NULL)
            m_cameraProvider->destroy();

        // Shut down the window (destroys window's EGLSurface).
        Window::getInstance().shutdown();

        // Cleanup the EGL display
        PROPAGATE_ERROR_CONTINUE(g_display.cleanup());
    }
};

/*
 * Program: userAutoExposure
 * Function: To display preview images to the device display to illustrate a Bayer
 *           Histogram based exposure time manager technique in real time
 */
static bool execute(const UserAutoExposureSampleOptions& options)
{
    UserAutoExposureTeardown appTearDown;
    // Initialize the window and EGL display.
    Window::getInstance().setWindowRect(options.windowRect());
    PROPAGATE_ERROR(g_display.initialize(Window::getInstance().getEGLNativeDisplay()));

    /*
     * Set up Argus API Framework, identify available camera devices, create
     * a capture session for the first available device, and set up the event
     * queue for completed events
     */
    appTearDown.m_cameraProvider = CameraProvider::create();
    ICameraProvider *iCameraProvider = interface_cast<ICameraProvider>(appTearDown.m_cameraProvider);
    EXIT_IF_NULL(iCameraProvider, "Cannot get core camera provider interface");
    USER_AUTO_EXPOSURE_PRINT("Argus Version: %s\n", iCameraProvider->getVersion().c_str());

    // Get the selected camera device and sensor mode.
    CameraDevice* cameraDevice = ArgusHelpers::getCameraDevice(
        appTearDown.m_cameraProvider, options.cameraDeviceIndex());
    if (!cameraDevice)
        ORIGINATE_ERROR("Selected camera device is not available");
    SensorMode* sensorMode = ArgusHelpers::getSensorMode(cameraDevice, options.sensorModeIndex());
    ISensorMode *iSensorMode = interface_cast<ISensorMode>(sensorMode);
    if (!iSensorMode)
        ORIGINATE_ERROR("Selected sensor mode not available");

    // Create the CaptureSession using the selected device.
    Ext::IBlockingSessionCameraProvider *iBlockingSessionCameraProvider =
            interface_cast<Ext::IBlockingSessionCameraProvider>(appTearDown.m_cameraProvider);
    EXIT_IF_NULL(iBlockingSessionCameraProvider,
                    "Cannot get blocking session camera provider interface");

    UniqueObj<CaptureSession> captureSession(
        iBlockingSessionCameraProvider->createBlockingCaptureSession(cameraDevice));
    ICaptureSession* iSession = interface_cast<ICaptureSession>(captureSession);
    EXIT_IF_NULL(iSession, "Cannot get Capture Session Interface");

    IEventProvider *iEventProvider = interface_cast<IEventProvider>(captureSession);
    EXIT_IF_NULL(iEventProvider, "iEventProvider is NULL");

    std::vector<EventType> eventTypes;
    eventTypes.push_back(EVENT_TYPE_CAPTURE_COMPLETE);
    eventTypes.push_back(EVENT_TYPE_ERROR);
    /* Seems there is bug in Argus, which drops EVENT_TYPE_ERROR if all
    3 events are not set. Set it for now */
    eventTypes.push_back(EVENT_TYPE_CAPTURE_STARTED);
    UniqueObj<EventQueue> queue(iEventProvider->createEventQueue(eventTypes));
    IEventQueue *iQueue = interface_cast<IEventQueue>(queue);
    EXIT_IF_NULL(iQueue, "event queue interface is NULL");

    /*
     * Creates the stream between the Argus camera image capturing
     * sub-system (producer) and the image acquisition code (consumer)
     * preview thread.  A consumer object is created from the stream
     * to be used to request the image frame.  A successfully submitted
     * capture request activates the stream's functionality to eventually
     * make a frame available for acquisition, in the preview thread,
     * and then display it on the device screen.
     */

    UniqueObj<OutputStreamSettings> streamSettings(
        iSession->createOutputStreamSettings(STREAM_TYPE_EGL));
    IEGLOutputStreamSettings *iEGLStreamSettings =
        interface_cast<IEGLOutputStreamSettings>(streamSettings);
    EXIT_IF_NULL(iEGLStreamSettings, "Cannot get IEGLOutputStreamSettings Interface");

    iEGLStreamSettings->setPixelFormat(PIXEL_FMT_YCbCr_420_888);
    iEGLStreamSettings->setResolution(Size2D<uint32_t>(options.windowRect().width(),
                                                       options.windowRect().height()));
    iEGLStreamSettings->setEGLDisplay(g_display.get());

    appTearDown.m_stream = iSession->createOutputStream(streamSettings.get());
    IEGLOutputStream *iEGLOutputStream = interface_cast<IEGLOutputStream>(appTearDown.m_stream);
    EXIT_IF_NULL(iEGLOutputStream, "Cannot get IEGLOutputStream Interface");

    appTearDown.m_previewConsumerThread = new PreviewConsumerThread(
        iEGLOutputStream->getEGLDisplay(), iEGLOutputStream->getEGLStream());
    PROPAGATE_ERROR(appTearDown.m_previewConsumerThread->initialize());
    PROPAGATE_ERROR(appTearDown.m_previewConsumerThread->waitRunning());

    UniqueObj<Request> request(iSession->createRequest(CAPTURE_INTENT_MANUAL));
    IRequest *iRequest = interface_cast<IRequest>(request);
    EXIT_IF_NULL(iRequest, "Failed to get capture request interface");

    //Fix the Isp Digital Gain to the minimum value i.e. 1.0x
    Argus::IAutoControlSettings* ac =
        Argus::interface_cast<Argus::IAutoControlSettings>(iRequest->getAutoControlSettings());
    EXIT_IF_NULL(ac, "Failed to get autocontrol settings interface");

    Range<float> ispDigitalGainRange = ac->getIspDigitalGainRange();
    EXIT_IF_NOT_OK(ac->setIspDigitalGainRange(Range<float>(ispDigitalGainRange.min())),
        "Unable to set Isp Digital Gain");

    Argus::ISourceSettings *iSourceSettings =
        Argus::interface_cast<Argus::ISourceSettings>(iRequest->getSourceSettings());
    EXIT_IF_NULL(iSourceSettings, "Failed to get source settings interface");

    Range<uint64_t> limitExposureTimeRange = iSensorMode->getExposureTimeRange();
    USER_AUTO_EXPOSURE_PRINT("Sensor Exposure Range min %ju, max %ju\n",
        limitExposureTimeRange.min(), limitExposureTimeRange.max());

    Size2D<uint32_t> sensorModeResolution = iSensorMode->getResolution();

    EXIT_IF_NOT_OK(iSourceSettings->setSensorMode(sensorMode),
        "Unable to set the SensorMode in the Request");

    EXIT_IF_NOT_OK(iRequest->enableOutputStream(appTearDown.m_stream),
        "Failed to enable stream in capture request");

    // Start off the min exposure time.
    Range<uint64_t> initialExposureTime = Range<uint64_t>(limitExposureTimeRange.min());
    EXIT_IF_NOT_OK(iSourceSettings->setExposureTimeRange(initialExposureTime),
        "Unable to set the Source Settings Exposure Time Range");

    Range<float> sensorModeAnalogGainRange = iSensorMode->getAnalogGainRange();
    USER_AUTO_EXPOSURE_PRINT("Sensor Analog Gain range min %f, max %f\n",
        sensorModeAnalogGainRange.min(), sensorModeAnalogGainRange.max());

    EXIT_IF_NOT_OK(iSourceSettings->setGainRange(Range<float>(sensorModeAnalogGainRange.min())),
        "Unable to set the Source Settings Gain Range");

    if (options.useAverageMap())
    {
        // Enable BayerAverageMap generation in the request.
        Ext::IBayerAverageMapSettings *iBayerAverageMapSettings =
            interface_cast<Ext::IBayerAverageMapSettings>(request);
        EXIT_IF_NULL(iBayerAverageMapSettings,
            "Failed to get BayerAverageMapSettings interface");
        iBayerAverageMapSettings->setBayerAverageMapEnable(true);
    }

    Argus::Status status = STATUS_OK;

    for (int index = 0; index < 100; index++)
    {
        iSession->capture(request.get(), 1000000000, &status);
    }

    // Set tolerance difference from the target before making adjustments.
    const Range<float> targetRange(TARGET_EXPOSURE_LEVEL - TARGET_EXPOSURE_LEVEL_RANGE_TOLERANCE,
                                   TARGET_EXPOSURE_LEVEL + TARGET_EXPOSURE_LEVEL_RANGE_TOLERANCE);
    USER_AUTO_EXPOSURE_PRINT("Exposure target range is from %f to %f\n",
                             targetRange.min(), targetRange.max());

    for (uint32_t frameCaptureLoop = 0; frameCaptureLoop < options.frameCount(); frameCaptureLoop++)
    {
        // Keep PREVIEW display window serviced
        Window::getInstance().pollEvents();

        const uint64_t FIVE_SECONDS = 5000000000;
        iEventProvider->waitForEvents(queue.get(), FIVE_SECONDS);
        EXIT_IF_TRUE(iQueue->getSize() == 0, "No events in queue");

        const Event* event = iQueue->getEvent(iQueue->getSize() - 1);
        const IEvent* iEvent = interface_cast<const IEvent>(event);
        if (!iEvent)
            printf("Error : Failed to get IEvent interface\n");
        else {
            if (iEvent->getEventType() == EVENT_TYPE_CAPTURE_COMPLETE) {
                const IEventCaptureComplete* iEventCaptureComplete
                    = interface_cast<const IEventCaptureComplete>(event);
                EXIT_IF_NULL(iEventCaptureComplete, "Failed to get EventCaptureComplete Interface");

                const CaptureMetadata* metaData = iEventCaptureComplete->getMetadata();
                const ICaptureMetadata* iMetadata = interface_cast<const ICaptureMetadata>(metaData);
                EXIT_IF_NULL(iMetadata, "Failed to get CaptureMetadata Interface");

                uint64_t frameExposureTime = iMetadata->getSensorExposureTime();
                float frameGain = iMetadata->getSensorAnalogGain();

                USER_AUTO_EXPOSURE_PRINT("Frame metadata ExposureTime %ju, Analog Gain %f\n",
                    frameExposureTime, frameGain);

                float curExposureLevel;

                if (!options.useAverageMap())
                {
                    /*
                     * By using the Bayer Histogram, find the exposure middle point
                     * in the histogram to be used as a guide as to whether to increase
                     * or decrease the Exposure Time or Analog Gain
                     */

                    const IBayerHistogram* bayerHistogram =
                        interface_cast<const IBayerHistogram>(iMetadata->getBayerHistogram());
                    EXIT_IF_NULL(bayerHistogram, "Unable to get Bayer Histogram from metadata");

                    std::vector< BayerTuple<uint32_t> > histogram;
                    EXIT_IF_NOT_OK(bayerHistogram->getHistogram(&histogram), "Failed to get histogram");

                    uint64_t halfPixelTotal = (uint64_t)sensorModeResolution.area() *
                                                BAYER_CHANNEL_COUNT / 2;
                    uint64_t sum = 0;
                    uint64_t currentBin;
                    for (currentBin = 0; currentBin < histogram.size(); currentBin++)
                    {
                        sum += histogram[currentBin].r()
                            + histogram[currentBin].gEven()
                            + histogram[currentBin].gOdd()
                            + histogram[currentBin].b();
                        if (sum > halfPixelTotal)
                        {
                            break;
                        }
                    }
                    curExposureLevel = (float) ++currentBin / (float)histogram.size();
                }
                else
                {
                    /*
                     * By using the Bayer Average Map, find the average point in the
                     * region to be used as a guide as to whether to increase or
                     * decrease the Exposure Time or Analog Gain
                     */

                    const Ext::IBayerAverageMap* iBayerAverageMap =
                        interface_cast<const Ext::IBayerAverageMap>(metaData);
                    EXIT_IF_NULL(iBayerAverageMap, "Failed to get IBayerAverageMap interface");
                    Array2D< BayerTuple<float> > averages;
                    EXIT_IF_NOT_OK(iBayerAverageMap->getAverages(&averages),
                        "Failed to get averages");
                    Array2D< BayerTuple<uint32_t> > clipCounts;
                    EXIT_IF_NOT_OK(iBayerAverageMap->getClipCounts(&clipCounts),
                        "Failed to get clip counts");
                    uint32_t pixelsPerBinPerChannel = iBayerAverageMap->getBinSize().width() *
                                                        iBayerAverageMap->getBinSize().height() /
                                                        BAYER_CHANNEL_COUNT;
                    float centerX = (float)(averages.size().width() - 1) / 2;
                    float centerY = (float)(averages.size().height() - 1) / 2;

                    // Using the BayerAverageMap, get the average instensity across the checked area
                    float weightedPixelCount = 0.0f;
                    float weightedAverageTotal = 0.0f;
                    for (uint32_t x = 0; x < averages.size().width(); x++)
                    {
                        for (uint32_t y = 0; y < averages.size().height(); y++)
                        {
                            /*
                             * Bin averages disregard pixels which are the result
                             * of intensity clipping.  A total of these over exposed
                             * pixels is kept and retrieved by clipCounts(x,y).  The
                             * following formula treats these pixels as a max intensity
                             * of 1.0f and adjusts the overall average upwards depending
                             * on the number of them.
                             *
                             * Also, only the GREEN EVEN channel is used to determine
                             * the exposure level, as GREEN is usually used for luminance
                             * evaluation, and the two GREEN channels tend to be equivalent
                             */

                            float clipAdjustedAverage =
                                (averages(x, y).gEven() *
                                    (pixelsPerBinPerChannel - clipCounts(x, y).gEven()) +
                                clipCounts(x, y).gEven()) / pixelsPerBinPerChannel;

                            /*
                             * This will add the flat averages across the entire image
                             */
                            weightedAverageTotal += clipAdjustedAverage;
                            weightedPixelCount += 1.0f;

                            /*
                             * This will add more 'weight' or significance to averages closer to
                             * the center of the image
                             */
                            float distance = sqrt((pow(fabs((float)x - centerX), 2)) +
                                                (pow(fabs((float)y - centerY), 2)));
                            if (distance < CENTER_WEIGHTED_DISTANCE)
                            {
                                weightedAverageTotal += clipAdjustedAverage *
                                    (1.0f - pow(distance / CENTER_WEIGHTED_DISTANCE, 2)) * CENTER_WEIGHT;
                                weightedPixelCount +=
                                    (1.0f - pow(distance / CENTER_WEIGHTED_DISTANCE, 2)) * CENTER_WEIGHT;
                            }
                        }
                    }
                    if (weightedPixelCount)
                    {
                        curExposureLevel = weightedAverageTotal / weightedPixelCount;
                    }
                    else
                    {
                        curExposureLevel = 1.0f;
                    }
                }

                /*
                 * If the acquired target is outside the target range, then
                 * calculate adjustment values for the Exposure Time and/or the Analog Gain to bring
                 * the next target into range.
                 */

                USER_AUTO_EXPOSURE_PRINT("Exposure level at %0.3f, ", curExposureLevel);
                if (curExposureLevel > targetRange.max())
                {
                    if (frameGain > sensorModeAnalogGainRange.min() &&
                        frameExposureTime <= limitExposureTimeRange.min())
                    {
                        frameGain = std::max(frameGain *
                            TARGET_EXPOSURE_LEVEL / curExposureLevel,
                            sensorModeAnalogGainRange.min());
                        printf("decreasing analog gain to %f\n", frameGain);
                        EXIT_IF_NOT_OK(iSourceSettings->setGainRange(Range<float>(frameGain)),
                            "Unable to set the Source Settings Gain Range");
                    }
                    else
                    {
                        frameExposureTime = std::max(limitExposureTimeRange.min(),
                            (uint64_t)((float)frameExposureTime *
                                TARGET_EXPOSURE_LEVEL / curExposureLevel));
                        printf("decreasing Exposure Time to %ju\n", frameExposureTime);
                        EXIT_IF_NOT_OK(iSourceSettings->setExposureTimeRange(
                            Range<uint64_t>(frameExposureTime)),
                            "Unable to set the Source Settings Exposure Time Range");
                    }
                }
                else if (curExposureLevel < targetRange.min())
                {
                    if (frameGain < sensorModeAnalogGainRange.max())
                    {
                        frameGain = std::min(frameGain *
                            TARGET_EXPOSURE_LEVEL / curExposureLevel,
                            sensorModeAnalogGainRange.max());
                        printf("increasing analog gain to %f\n", frameGain);
                        EXIT_IF_NOT_OK(iSourceSettings->setGainRange(Range<float>(frameGain)),
                            "Unable to set the Source Settings Gain Range");
                    }
                    else
                    {
                        frameExposureTime = std::min(limitExposureTimeRange.max(),
                            (uint64_t)((float)frameExposureTime *
                                TARGET_EXPOSURE_LEVEL / curExposureLevel));
                        printf("increasing Exposure Time to %ju\n", frameExposureTime);
                        EXIT_IF_NOT_OK(iSourceSettings->setExposureTimeRange(
                            Range<uint64_t>(frameExposureTime)),
                            "Unable to set the Source Settings Exposure Time Range");
                    }
                }
                else
                {
                    printf("currently within target range\n");
                    continue;
                }
                /*
                 * The modified request is re-submitted to terminate the previous repeat() with
                 * the old settings and begin captures with the new settings
                 */
                iSession->capture(request.get());
            } else if (iEvent->getEventType() == EVENT_TYPE_CAPTURE_STARTED) {
                /* ToDo: Remove the empty after the bug is fixed */
                continue;
            } else if (iEvent->getEventType() == EVENT_TYPE_ERROR) {
                const IEventError* iEventError =
                    interface_cast<const IEventError>(event);
                EXIT_IF_NOT_OK(iEventError->getStatus(), "ERROR event");
            }
            else {
                printf("WARNING: Unknown event. Continue\n");
            }
        }
    }

    //iSession->stopRepeat is cleaned with captureSession RAII
    return true;
}

}; // namespace ArgusSamples

int main(int argc, char **argv)
{
    ArgusSamples::UserAutoExposureSampleOptions options(basename(argv[0]));
    if (!options.parse(argc, argv))
        return EXIT_FAILURE;
    if (options.requestedExit())
        return EXIT_SUCCESS;

    if (!ArgusSamples::execute(options))
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
