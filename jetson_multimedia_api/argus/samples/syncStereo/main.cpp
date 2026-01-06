/*
 * Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
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
#include "CommonOptions.h"
#include "EGLGlobal.h"
#include "Error.h"
#include "Thread.h"

#include <Argus/Argus.h>
#include <EGLStream/EGLStream.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include <Argus/Ext/SyncSensorCalibrationData.h>

using namespace Argus;
using namespace EGLStream;

namespace ArgusSamples
{

/*
 * This sample opens sessions based on the number of stereo/sensor modules
 * connected. Each module can have 1 sensor or multiple (2) sensors connected.
 * The processing of the images happens in the worker thread, while the main
 * app thread is used to drive the captures.
 */

// Constants.
static const Size2D<uint32_t> STREAM_SIZE(640, 480);

// Globals and derived constants.
EGLDisplayHolder g_display;

// Debug print macros.
#define PRODUCER_PRINT(...) printf("PRODUCER: " __VA_ARGS__)
#define CONSUMER_PRINT(...) printf("CONSUMER: " __VA_ARGS__)

// forward declaration
class SyncStereoConsumerThread;

#define MAX_MODULE_STRING 32
#define MAX_CAM_DEVICE 6
typedef struct
{
    char moduleName[MAX_MODULE_STRING];
    int camDevice[MAX_CAM_DEVICE];
    UniqueObj<OutputStream> stream[MAX_CAM_DEVICE];
    UniqueObj<CaptureSession> captureSession;
    UniqueObj<OutputStreamSettings> streamSettings;
    SyncStereoConsumerThread *syncStereoConsumer;
    int sensorCount;
    bool initialized;
} ModuleInfo;


/*******************************************************************************
 * Argus disparity class
 * This class will analyze frames from 2 synchronized sensors
 ******************************************************************************/
class SyncStereoConsumerThread : public Thread
{
public:
    explicit SyncStereoConsumerThread(ModuleInfo *modInfo)
    {
        m_leftStream = modInfo->stream[0].get();
        if (modInfo->sensorCount > 1)
            m_rightStream = modInfo->stream[1].get();
        else
            m_rightStream = NULL;

        strcpy(m_moduleName, modInfo->moduleName);
    }
    ~SyncStereoConsumerThread()
    {
        CONSUMER_PRINT("DESTRUCTOR  ... \n");
    }

private:
    /** @name Thread methods */
    /**@{*/
    virtual bool threadInitialize();
    virtual bool threadExecute();
    virtual bool threadShutdown();
    /**@}*/

    /* Assumption: We only have a Left-Right pair.
     * OutputStream and FrameConsumer should be created to a vector of
     * MAX_CAM_DEVICE size.
     */
    OutputStream *m_leftStream;
    OutputStream *m_rightStream;
    char m_moduleName[MAX_MODULE_STRING];
    UniqueObj<FrameConsumer> m_leftConsumer;
    UniqueObj<FrameConsumer> m_rightConsumer;
};

ICaptureSession* g_iCaptureSession[MAX_CAM_DEVICE];

bool SyncStereoConsumerThread::threadInitialize()
{
    CONSUMER_PRINT("Creating FrameConsumer for left stream\n");
    m_leftConsumer = UniqueObj<FrameConsumer>(FrameConsumer::create(m_leftStream));
    if (!m_leftConsumer)
        ORIGINATE_ERROR("Failed to create FrameConsumer for left stream");

    if (m_rightStream)
    {
        CONSUMER_PRINT("Creating FrameConsumer for right stream\n");
        m_rightConsumer = UniqueObj<FrameConsumer>(FrameConsumer::create(m_rightStream));
        if (!m_rightConsumer)
            ORIGINATE_ERROR("Failed to create FrameConsumer for right stream");
    }

    return true;
}

bool SyncStereoConsumerThread::threadExecute()
{
    IEGLOutputStream *iLeftStream = interface_cast<IEGLOutputStream>(m_leftStream);
    IFrameConsumer* iFrameConsumerLeft = interface_cast<IFrameConsumer>(m_leftConsumer);

    IFrameConsumer* iFrameConsumerRight = NULL;
    if (m_rightStream)
    {
        IEGLOutputStream *iRightStream = interface_cast<IEGLOutputStream>(m_rightStream);
        iFrameConsumerRight = interface_cast<IFrameConsumer>(m_rightConsumer);

        // Wait until the producer has connected to the stream.
        CONSUMER_PRINT("Waiting until Argus producer is connected to right stream...\n");
        if (iRightStream->waitUntilConnected() != STATUS_OK)
            ORIGINATE_ERROR("Argus producer failed to connect to right stream.");
        CONSUMER_PRINT("Argus producer for right stream has connected; continuing.\n");
    }

    // Wait until the producer has connected to the stream.
    CONSUMER_PRINT("Waiting until Argus producer is connected to left stream...\n");
    if (iLeftStream->waitUntilConnected() != STATUS_OK)
        ORIGINATE_ERROR("Argus producer failed to connect to left stream.");
    CONSUMER_PRINT("Argus producer for left stream has connected; continuing.\n");

    while (true)
    {
        UniqueObj<Frame> frameleft(iFrameConsumerLeft->acquireFrame());
        if (!frameleft)
            break;

        // Use the IFrame interface to print out the frame number/timestamp, and
        // to provide access to the Image in the Frame.
        IFrame *iFrameLeft = interface_cast<IFrame>(frameleft);
        if (!iFrameLeft)
            ORIGINATE_ERROR("Failed to get left IFrame interface.");
        CONSUMER_PRINT("[%s]: Acquired Left Frame: %llu, time %llu\n", m_moduleName,
                       static_cast<unsigned long long>(iFrameLeft->getNumber()),
                       static_cast<unsigned long long>(iFrameLeft->getTime()));

        if (iFrameConsumerRight)
        {
            UniqueObj<Frame> frameright(iFrameConsumerRight->acquireFrame());
            if (!frameright)
                break;

            // Use the IFrame interface to print out the frame number/timestamp, and
            // to provide access to the Image in the Frame.
            IFrame *iFrameRight = interface_cast<IFrame>(frameright);
            if (!iFrameRight)
                ORIGINATE_ERROR("Failed to get right IFrame interface.");
            CONSUMER_PRINT("[%s]: Acquired Right Frame: %llu, time %llu\n", m_moduleName,
                           static_cast<unsigned long long>(iFrameRight->getNumber()),
                           static_cast<unsigned long long>(iFrameRight->getTime()));
        }
    }

    CONSUMER_PRINT("No more frames. Cleaning up.\n");

    PROPAGATE_ERROR(requestShutdown());

    return true;
}

bool SyncStereoConsumerThread::threadShutdown()
{
    return true;
}

static void SyncStereoCalibrationData(
    const Ext::ISyncSensorCalibrationData *iSyncSensorCalibrationData)
{
    Size2D<uint32_t> ImageSize = iSyncSensorCalibrationData->getImageSizeInPixels();
    printf("\n Image size = %d, %d\n", ImageSize.width(), ImageSize.height());

    Point2D<float> FocalLength = iSyncSensorCalibrationData->getFocalLength();
    printf("\n Focal Length = %f, %f\n", FocalLength.x(), FocalLength.y());

    Point2D<float> PrincipalPoint = iSyncSensorCalibrationData->getPrincipalPoint();
    printf("\n Principal Point = %f, %f\n", PrincipalPoint.x(), PrincipalPoint.y());

    float Skew = iSyncSensorCalibrationData->getSkew();
    printf("\n Skew = %f\n", Skew);

    MappingType FishEyeMappingType = iSyncSensorCalibrationData->getFisheyeMappingType();
    printf("\n Fish Eye mapping type = %s\n", FishEyeMappingType.getName());

    DistortionType LensDistortionType = iSyncSensorCalibrationData->getLensDistortionType();
    printf("\n Lens Distortion type = %s\n", LensDistortionType.getName());

    uint32_t RadialCoeffsCount =
                        iSyncSensorCalibrationData->getRadialCoeffsCount(LensDistortionType);
    printf("\n Radial coeffs count = %d\n", RadialCoeffsCount);

    std::vector<float> k;
    iSyncSensorCalibrationData->getRadialCoeffs(&k, LensDistortionType);

    printf("\n Radial coefficients = ");
    for (uint32_t idx = 0; idx < k.size(); idx++)
    {
        printf("%f ", k[idx]);
    }

    uint32_t TangentialCoeffsCount =
        iSyncSensorCalibrationData->getTangentialCoeffsCount();
    printf("\n\n Tangential coeffs count = %d\n", TangentialCoeffsCount);

    std::vector<float> p;
    iSyncSensorCalibrationData->getTangentialCoeffs(&p);

    printf("\n Tangential coefficients = ");
    for (uint32_t idx = 0; idx < p.size(); idx++)
    {
        printf("%f ", p[idx]);
    }

    Point3D<float> rot3d = iSyncSensorCalibrationData->getRotationParams();
    printf("rot3d x, y, x{%f, %f, %f}\n", rot3d.x(), rot3d.y(), rot3d.z());

    Point3D<float> translation = iSyncSensorCalibrationData->getTranslationParams();
    printf("translation 3d x, y, x{%f, %f, %f}\n",
        translation.x(), translation.y(), translation.z());

    bool isImu = iSyncSensorCalibrationData->isImuSensorAvailable();
    if (isImu)
    {
        printf("\n\n For IMU sensors \n");

        Point3D<float> linearAccBias = iSyncSensorCalibrationData->getLinearAccBias();
        printf("linearAccBias 3d x, y, x{%f, %f, %f}\n", linearAccBias.x(), linearAccBias.y(), linearAccBias.z());

        Point3D<float> angularVelocityBias = iSyncSensorCalibrationData->getAngularVelocityBias();
        printf("angularVelocityBias 3d x, y, x{%f, %f, %f}\n", angularVelocityBias.x(), angularVelocityBias.y(), angularVelocityBias.z());

        Point3D<float> gravityAcc = iSyncSensorCalibrationData->getGravityAcc();
        printf("gravityAcc 3d x, y, x{%f, %f, %f}\n", gravityAcc.x(), gravityAcc.y(), gravityAcc.z());

        Point3D<float> imuRotation = iSyncSensorCalibrationData->getImuRotationParams();
        printf("ImuRotation 3d x, y, x{%f, %f, %f}\n", imuRotation.x(), imuRotation.y(), imuRotation.z());

        Point3D<float> imuTranslationParams = iSyncSensorCalibrationData->getImuTranslationParams();
        printf("imuTranslationParams 3d x, y, x{%f, %f, %f}\n", imuTranslationParams.x(), imuTranslationParams.y(), imuTranslationParams.z());
        printf("\n\n");
    }
}

static bool execute(const CommonOptions& options)
{
    // Initialize EGL.
    PROPAGATE_ERROR(g_display.initialize());

    ModuleInfo moduleInfo[MAX_CAM_DEVICE];
    int moduleCount = 0;
    memset(&moduleInfo, 0, MAX_CAM_DEVICE*sizeof(ModuleInfo));
    for (int i = 0; i < MAX_CAM_DEVICE; i++)
        moduleInfo[i].initialized = false;

    // Initialize the Argus camera provider.
    UniqueObj<CameraProvider> cameraProvider(CameraProvider::create());

    // Get the ICameraProvider interface from the global CameraProvider.
    ICameraProvider *iCameraProvider = interface_cast<ICameraProvider>(cameraProvider);
    if (!iCameraProvider)
        ORIGINATE_ERROR("Failed to get ICameraProvider interface");
    printf("Argus Version: %s\n", iCameraProvider->getVersion().c_str());

    // Get the camera devices.
    std::vector<CameraDevice*> cameraDevices;
    iCameraProvider->getCameraDevices(&cameraDevices);
    if (cameraDevices.size() < 2)
        ORIGINATE_ERROR("Must have at least 2 sensors available");

    /**
     * For multiple HAWK modules, we need to map the available sensors
     * to identify which sensor belongs to which HAWK module.
     * In case we have non-HAWK modules, the sensors would be mapped accordingly.
     * Current assumption is that each HAWK module has only 2 sensors and
     * each non-HAWK module has only a single sensor.
     *
     */

    char syncSensorId[MAX_MODULE_STRING];
    for (uint32_t i = 0; i < cameraDevices.size(); i++)
    {
        Argus::ICameraProperties *iCameraProperties =
                        Argus::interface_cast<Argus::ICameraProperties>(cameraDevices[i]);
        if (!iCameraProperties)
            ORIGINATE_ERROR("Failed to get cameraProperties interface");

        printf("getSensorPlacement for sensor i %d is %s\n",
                i, iCameraProperties->getSensorPlacement().getName());
        const Ext::ISyncSensorCalibrationData* iSyncSensorCalibrationData =
            interface_cast<const Ext::ISyncSensorCalibrationData>(cameraDevices[i]);
        if (iSyncSensorCalibrationData)
        {
            iSyncSensorCalibrationData->getSyncSensorModuleId(
                        syncSensorId, sizeof(syncSensorId));
            printf("Found : %s\n", syncSensorId);

            for (int j = 0; j <= moduleCount; j++)
            {
                if (strcmp(syncSensorId, moduleInfo[j].moduleName))
                {
                    if (moduleInfo[j].initialized == false)
                    {
                        strcpy(moduleInfo[j].moduleName, syncSensorId);
                        moduleInfo[j].initialized = true;
                        moduleInfo[j].camDevice[moduleInfo[j].sensorCount++] = i;
                    }
                    else
                    {
                        continue;
                    }

                    moduleCount++;
                    break;
                }
                else
                {
                    moduleInfo[j].camDevice[moduleInfo[j].sensorCount++] = i;
                    break;
                }
            }
        }
    }

    for (int i = 0; i < moduleCount; i++)
    {
        printf("/**************************/\n");
        printf("Identified %s module with %d sensors connected\n", moduleInfo[i].moduleName
                                                                 , moduleInfo[i].sensorCount);
        printf("/**************************/\n");
    }

    for (int i = 0; i < moduleCount; i++)
    {
        UniqueObj<Request> request;
        std::vector <CameraDevice*> lrCameras;

        // Group camera devices to identify no. of sessions to be created
        for (int j = 0; j < moduleInfo[i].sensorCount; j++)
        {
            lrCameras.push_back(cameraDevices[moduleInfo[i].camDevice[j]]);
            printf("Session[%d] : add cameraDevices[%d]\n", i, moduleInfo[i].camDevice[j]);
        }

        /**
         * Create the capture session for each set of camera devices identified above,
         * Each session will comprise of two devices (for now) in case of HAWK module.
         * AutoControl will be based on what the 1st device sees.
         * In case of non-HAWK module, there will be a single session for single camera device.
         *
         */

        moduleInfo[i].captureSession =
            UniqueObj<CaptureSession>(iCameraProvider->createCaptureSession(lrCameras));
        g_iCaptureSession[i] = interface_cast<ICaptureSession>(moduleInfo[i].captureSession);
        if (!g_iCaptureSession[i])
            ORIGINATE_ERROR("Failed to get capture session interface");

        /**
         * Create stream settings object and set settings common to both streams in case of HAWK module.
         * Else single stream will be created for non-HAWK module.
         *
         */

        moduleInfo[i].streamSettings = UniqueObj<OutputStreamSettings>(
            g_iCaptureSession[i]->createOutputStreamSettings(STREAM_TYPE_EGL));
        IOutputStreamSettings* iStreamSettings =
            interface_cast<IOutputStreamSettings>(moduleInfo[i].streamSettings);
        IEGLOutputStreamSettings* iEGLStreamSettings =
            interface_cast<IEGLOutputStreamSettings>(moduleInfo[i].streamSettings);
        if (!iStreamSettings || !iEGLStreamSettings)
            ORIGINATE_ERROR("Failed to create OutputStreamSettings");
        iEGLStreamSettings->setPixelFormat(PIXEL_FMT_YCbCr_420_888);
        iEGLStreamSettings->setResolution(STREAM_SIZE);
        iEGLStreamSettings->setEGLDisplay(g_display.get());

        // Create EGL streams based on stream settings created above for HAWK/non-HAWK modules.
        for (int a = 0; a < moduleInfo[i].sensorCount; a++)
        {
            PRODUCER_PRINT("Creating stream[%d].\n", a);
            iStreamSettings->setCameraDevice(lrCameras[a]);
            moduleInfo[i].stream[a] = UniqueObj<OutputStream>(
                g_iCaptureSession[i]->createOutputStream(moduleInfo[i].streamSettings.get()));
        }

        PRODUCER_PRINT("Launching syncsensor consumer\n");
        moduleInfo[i].syncStereoConsumer = new SyncStereoConsumerThread(&moduleInfo[i]);
        PROPAGATE_ERROR(moduleInfo[i].syncStereoConsumer->initialize());
        PROPAGATE_ERROR(moduleInfo[i].syncStereoConsumer->waitRunning());

        // Create a request
        request = UniqueObj<Request>(g_iCaptureSession[i]->createRequest());
        IRequest *iRequest = interface_cast<IRequest>(request);
        if (!iRequest)
            ORIGINATE_ERROR("Failed to create Request");

        // Enable output streams based on EGL streams created above for HAWK/non-HAWK modules.
        for (int a = 0; a < moduleInfo[i].sensorCount; a++)
        {
            PRODUCER_PRINT("Enable stream[%d].\n", a);
            iRequest->enableOutputStream(moduleInfo[i].stream[a].get());
        }

        for (uint32_t i = 0; i < lrCameras.size(); i++)
        {
            const Ext::ISyncSensorCalibrationData *iSyncSensorCalibrationData =
                              interface_cast<const Ext::ISyncSensorCalibrationData>(lrCameras[i]);
            if (iSyncSensorCalibrationData)
            {
                SyncStereoCalibrationData(iSyncSensorCalibrationData);
            }
        }

        // Submit capture for the specified time.
        PRODUCER_PRINT("Starting repeat capture requests.\n");
        if (g_iCaptureSession[i]->repeat(request.get()) != STATUS_OK)
            ORIGINATE_ERROR("Failed to start repeat capture request for preview");
    }

    // Wait for specified number of seconds.
    sleep(options.captureTime());

    for (int i = 0; i < moduleCount; i++)
    {
        // Stop the capture requests and wait until they are complete.
        g_iCaptureSession[i]->stopRepeat();
        g_iCaptureSession[i]->waitForIdle();

        // Destroy the output streams to end the consumer thread.
        PRODUCER_PRINT("Captures complete, disconnecting producer: %d\n", i);
        for (int a = 0; a < moduleInfo[i].sensorCount; a++)
        {
            moduleInfo[i].stream[a].reset();
        }

        // Wait for the consumer thread to complete.
        PRODUCER_PRINT("Wait for consumer thread to complete\n");
        PROPAGATE_ERROR(moduleInfo[i].syncStereoConsumer->shutdown());
    }

    // Shut down Argus.
    cameraProvider.reset();

    // Cleanup the EGL display
    PROPAGATE_ERROR(g_display.cleanup());

    PRODUCER_PRINT("Done -- exiting.\n");
    return true;
}

}; // namespace ArgusSamples

int main(int argc, char *argv[])
{
    ArgusSamples::CommonOptions options(basename(argv[0]),
                                        ArgusSamples::CommonOptions::Option_T_CaptureTime);
    if (!options.parse(argc, argv))
        return EXIT_FAILURE;
    if (options.requestedExit())
        return EXIT_SUCCESS;

    if (!ArgusSamples::execute(options))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

