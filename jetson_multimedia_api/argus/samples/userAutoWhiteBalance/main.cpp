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

#include "ArgusHelpers.h"
#include "Error.h"
#include <stdio.h>
#include <stdlib.h>
#include <Argus/Argus.h>
#include <Argus/Ext/BayerAverageMap.h>
#include <Argus/Ext/NonLinearHistogram.h>
#include <EGLStream/EGLStream.h>
#include "PreviewConsumer.h"
#include "CommonOptions.h"
#include <algorithm>

#define EXIT_IF_TRUE(val,msg)   \
        {if ((val)) {printf("%s\n",msg); return false;}}
#define EXIT_IF_NULL(val,msg)   \
        {if (!val) {printf("%s\n",msg); return false;}}
#define EXIT_IF_NOT_OK(val,msg) \
        {if (val!=Argus::STATUS_OK) {printf("%s\n",msg); return false;}}

using namespace Argus;

namespace ArgusSamples
{

// Globals.
EGLDisplayHolder g_display;

// Constants
const float BAYER_CLIP_COUNT_MAX = 0.25f;
const float BAYER_MISSING_SAMPLE_TOLERANCE = 0.10f;
const float BAYER_HISTOGRAM_CLIP_PERCENTILE = 0.10f;

/*******************************************************************************
 * Extended options class to add additional options specific to this sample.
 ******************************************************************************/
class UserAutoWhiteBalanceSampleOptions : public CommonOptions
{
public:
    UserAutoWhiteBalanceSampleOptions(const char *programName)
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
class UserAutoWhiteBalanceTeardown
{
public:
    CameraProvider* m_cameraProvider;
    PreviewConsumerThread* m_previewConsumerThread;
    OutputStream* m_stream;

    UserAutoWhiteBalanceTeardown()
    {
        m_cameraProvider = NULL;
        m_previewConsumerThread = NULL;
        m_stream = NULL;
    }

    ~UserAutoWhiteBalanceTeardown()
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
 * Program: userAutoWhiteBalance
 * Function: To display 101 preview images to the device display to illustrate a
 *           Grey World White Balance technique for adjusting the White Balance in real time
 *           through the use of setWbGains() and setAwbMode(AWB_MODE_MANUAL) calls.
 */

static bool execute(const UserAutoWhiteBalanceSampleOptions& options)
{
    UserAutoWhiteBalanceTeardown appTearDown;
    BayerTuple<float> precedingAverages(1.0f, 1.5f, 1.5f, 1.5f);

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
    printf("Argus Version: %s\n", iCameraProvider->getVersion().c_str());

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
    UniqueObj<CaptureSession> captureSession(iCameraProvider->createCaptureSession(cameraDevice));
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

    UniqueObj<Request> request(iSession->createRequest(CAPTURE_INTENT_PREVIEW));
    IRequest *iRequest = interface_cast<IRequest>(request);
    EXIT_IF_NULL(iRequest, "Failed to get capture request interface");

    // Set the sensor mode in the request.
    ISourceSettings *iSourceSettings =
        interface_cast<ISourceSettings>(iRequest->getSourceSettings());
    EXIT_IF_NULL(iSourceSettings, "Failed to get source settings interface");
    iSourceSettings->setSensorMode(sensorMode);

    EXIT_IF_NOT_OK(iRequest->enableOutputStream(appTearDown.m_stream),
        "Failed to enable stream in capture request");

    IAutoControlSettings* iAutoControlSettings =
        interface_cast<IAutoControlSettings>(iRequest->getAutoControlSettings());
    EXIT_IF_NULL(iAutoControlSettings, "Failed to get AutoControlSettings interface");

    if (options.useAverageMap())
    {
        // Enable BayerAverageMap generation in the request.
        Ext::IBayerAverageMapSettings *iBayerAverageMapSettings =
            interface_cast<Ext::IBayerAverageMapSettings>(request);
        EXIT_IF_NULL(iBayerAverageMapSettings,
            "Failed to get BayerAverageMapSettings interface");
        iBayerAverageMapSettings->setBayerAverageMapEnable(true);
    }

    EXIT_IF_NOT_OK(iSession->repeat(request.get()), "Unable to submit repeat() request");

    /*
     * Using the image capture event metadata, acquire the bayer histogram and then compute
     * a weighted average for each channel.  Use these weighted averages to create a White
     * Balance Channel Gain array to use for setting the manual white balance of the next
     * capture request.
     */
    uint32_t frameCaptureLoop = 0;
    while (frameCaptureLoop < options.frameCount())
    {
        // Keep PREVIEW display window serviced
        Window::getInstance().pollEvents();

        const uint64_t FIVE_SECONDS = 5000000000;
        iEventProvider->waitForEvents(queue.get(), FIVE_SECONDS);
        EXIT_IF_TRUE(iQueue->getSize() == 0, "No events in queue");

        frameCaptureLoop += iQueue->getSize();

        const Event* event = iQueue->getEvent(iQueue->getSize() - 1);
        const IEvent* iEvent = interface_cast<const IEvent>(event);
        if (!iEvent)
            printf("Error : Failed to get IEvent interface\n");
        else {
            if (iEvent->getEventType() == EVENT_TYPE_CAPTURE_COMPLETE) {
                const IEventCaptureComplete* iEventCaptureComplete =
                    interface_cast<const IEventCaptureComplete>(event);
                EXIT_IF_NULL(iEventCaptureComplete, "Failed to get EventCaptureComplete Interface");

                const CaptureMetadata* metaData = iEventCaptureComplete->getMetadata();
                const ICaptureMetadata* iMetadata = interface_cast<const ICaptureMetadata>(metaData);

                EXIT_IF_NULL(iMetadata, "Failed to get CaptureMetadata Interface");

                BayerTuple<float> bayerTotals(0.0f);
                BayerTuple<float> bayerAverages(0.0f);
                if (!options.useAverageMap())
                {
                    const IBayerHistogram* bayerHistogram =
                        interface_cast<const IBayerHistogram>(iMetadata->getBayerHistogram());
                    const Ext::INonLinearHistogram* iNonLinearHistogram =
                        interface_cast<const Ext::INonLinearHistogram>(iMetadata->getBayerHistogram());

                    std::vector< BayerTuple<uint32_t> > histogram;
                    std::vector< BayerTuple<float> > binValues;
                    EXIT_IF_NOT_OK(bayerHistogram->getHistogram(&histogram), "Failed to get histogram");

                    if (iNonLinearHistogram)
                    {
                        EXIT_IF_NOT_OK(iNonLinearHistogram->getHistogramBinValues(
                                                        &binValues), "Failed to get linear bin values");
                    }
                    uint32_t histogramClippedCount = histogram.size() *
                                                        (1.0f - BAYER_HISTOGRAM_CLIP_PERCENTILE);
                    for (int channel = 0; channel < BAYER_CHANNEL_COUNT; channel++)
                    {
                        for (uint32_t bin = 0; bin < histogramClippedCount; bin++)
                        {
                            float bayerValue = (iNonLinearHistogram) ?
                                                (binValues[bin][channel] * histogram.size()) : bin;

                            bayerTotals[channel] += histogram[bin][channel];
                            bayerAverages[channel] += histogram[bin][channel] * bayerValue;
                        }
                        if (bayerTotals[channel])
                            bayerAverages[channel] /= bayerTotals[channel];
                    }

                    printf("Histogram ");
                }
                else
                {
                    const Ext::IBayerAverageMap* iBayerAverageMap =
                        interface_cast<const Ext::IBayerAverageMap>(metaData);
                    EXIT_IF_NULL(iBayerAverageMap, "Failed to get IBayerAverageMap interface");
                    Array2D< BayerTuple<float> > averages;
                    EXIT_IF_NOT_OK(iBayerAverageMap->getAverages(&averages),
                        "Failed to get averages");
                    Array2D< BayerTuple<uint32_t> > clipCounts;
                    EXIT_IF_NOT_OK(iBayerAverageMap->getClipCounts(&clipCounts),
                        "Failed to get clip counts");
                    uint32_t pixelsPerBinPerChannel =
                        iBayerAverageMap->getBinSize().width() *
                        iBayerAverageMap->getBinSize().height() /
                        BAYER_CHANNEL_COUNT;
                    EXIT_IF_NULL(pixelsPerBinPerChannel, "Zero pixels per bin channel");

                    // Using the BayerAverageMap, get the average instensity across the checked area
                    uint32_t usedBins = 0;
                    for (uint32_t x = 0; x < averages.size().width(); x++)
                    {
                        for (uint32_t y = 0; y < averages.size().height(); y++)
                        {
                            /*
                             * Bins with excessively high clip counts many contain
                             * misleading averages, since clipped pixels are ignored
                             * in the averages, and so these bins are ignored.
                             *
                             */
                            if ((float)clipCounts(x, y).r() / pixelsPerBinPerChannel
                                    <= BAYER_CLIP_COUNT_MAX &&
                                (float)clipCounts(x, y).gEven() / pixelsPerBinPerChannel
                                    <= BAYER_CLIP_COUNT_MAX &&
                                (float)clipCounts(x, y).gOdd() / pixelsPerBinPerChannel
                                    <= BAYER_CLIP_COUNT_MAX &&
                                (float)clipCounts(x, y).b() / pixelsPerBinPerChannel
                                    <= BAYER_CLIP_COUNT_MAX)
                            {
                                bayerTotals += averages(x, y);
                                usedBins++;
                            }
                        }
                    }

                    EXIT_IF_NULL(usedBins, "No used bins");

                    /*
                     * This check is to determine if enough BayerAverageMap samples
                     * have been collected to give a valid enough evaluation of the
                     * white balance adjustment.  If not enough samples, then the
                     * most previous valid results are used.  If valid, then the valid
                     * set is stored for future use.
                     */
                    if ((float)usedBins / (averages.size().width() * averages.size().height()) <
                        (1.0f - BAYER_MISSING_SAMPLE_TOLERANCE))
                    {
                        printf("Clipped sensor sample percentage of %0.1f%%"
                            " exceed tolerance of %0.1f%%\n"
                            "Using last valid BayerAverageMap intensity values\n",
                            (1.0f - (float)usedBins /
                            (averages.size().width() * averages.size().height())) * 100,
                            BAYER_MISSING_SAMPLE_TOLERANCE * 100);
                        bayerAverages = precedingAverages;
                    }
                    else
                    {
                        for (int channel = 0; channel < BAYER_CHANNEL_COUNT; channel++)
                        {
                            bayerAverages[channel] = bayerTotals[channel] / usedBins;
                            precedingAverages[channel] = bayerAverages[channel];
                        }
                    }
                    printf("BayerAverageMap ");
                }

                float maxGain = 0;
                for (int channel = 0; channel < BAYER_CHANNEL_COUNT; channel++)
                {
                    maxGain = std::max(maxGain, bayerAverages[channel]);
                }
                float whiteBalanceGains[BAYER_CHANNEL_COUNT];
                for (int channel = 0; channel < BAYER_CHANNEL_COUNT; channel++)
                {
                    whiteBalanceGains[channel] = maxGain / bayerAverages[channel];
                }

                printf("Intensity: r:%0.3f gO:%0.3f gE:%0.3f b:%0.3f   "
                    "WBGains: r:%0.3f gO:%0.3f gE:%0.3f b:%0.3f\n",
                    bayerAverages[0],
                    bayerAverages[1],
                    bayerAverages[2],
                    bayerAverages[3],
                    whiteBalanceGains[0],
                    whiteBalanceGains[1],
                    whiteBalanceGains[2],
                    whiteBalanceGains[3]);

                BayerTuple<float> bayerGains(whiteBalanceGains[0], whiteBalanceGains[1],
                    whiteBalanceGains[2], whiteBalanceGains[3]);

                iAutoControlSettings->setWbGains(bayerGains);
                iAutoControlSettings->setAwbMode(AWB_MODE_MANUAL);

                EXIT_IF_NOT_OK(iSession->repeat(request.get()), "Unable to submit repeat() request");
            } else if (iEvent->getEventType() == EVENT_TYPE_CAPTURE_STARTED) {
                /* ToDo: Remove the empty after the bug is fixed */
                continue;
            } else if (iEvent->getEventType() == EVENT_TYPE_ERROR) {
                const IEventError* iEventError =
                    interface_cast<const IEventError>(event);
                EXIT_IF_NOT_OK(iEventError->getStatus(), "ERROR event");
            } else {
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
    ArgusSamples::UserAutoWhiteBalanceSampleOptions options(basename(argv[0]));
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
