/*
 * Copyright (c) 2016 - 2018, NVIDIA CORPORATION. All rights reserved.
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
#include "Error.h"
#include "EGLGlobal.h"
#include "GLContext.h"
#include "Thread.h"
#include "Window.h"
#include "MathUtils.h"

#include <Argus/Argus.h>

#include <string.h>
#include <unistd.h>
#include <stdlib.h>

using namespace Argus;

namespace ArgusSamples
{

// Globals.
EGLDisplayHolder g_display;

// Debug print macros.
#define PRODUCER_PRINT(...) printf("PRODUCER: " __VA_ARGS__)
#define CONSUMER_PRINT(...) printf("CONSUMER: " __VA_ARGS__)

/*******************************************************************************
 * GL Consumer thread:
 *   Opens an on-screen window, and renders to it a 3D rotating cube where each
 *   face is textured with the current EGLStream frame. This is done using an
 *   OpenGL ES context and an EXTERNAL_OES texture binding to the EGLStream.
 *   Frames will be acquired and rendered by this thread until the stream is
 *   disconnected by the Argus producer thread once all capture requests have
 *   been submitted.
 ******************************************************************************/
class ConsumerThread : public Thread
{
public:
    explicit ConsumerThread(EGLStreamKHR stream)
        : m_stream(stream)
        , m_texture(0)
        , m_program(0)
    {
    }
    ~ConsumerThread()
    {
    }

private:
    /** @name Thread methods */
    /**@{*/
    virtual bool threadInitialize();
    virtual bool threadExecute();
    virtual bool threadShutdown();
    /**@}*/

    EGLStreamKHR m_stream;
    GLContext m_context;
    GLuint m_texture;
    GLuint m_program;
};

bool ConsumerThread::threadInitialize()
{
    Window &window = Window::getInstance();

    // Create the context and make it current.
    CONSUMER_PRINT("Creating context.\n");
    PROPAGATE_ERROR(m_context.initialize(&window));
    PROPAGATE_ERROR(m_context.makeCurrent());

    // Create the shader program.
    static const char vtxSrc[] =
        "#version 300 es\n"
        "uniform mat4 projMat;\n"
        "uniform mat4 rotMat;\n"
        "in layout(location = 0) vec4 vertex;\n"
        "in layout(location = 1) vec2 texCoord;\n"
        "out vec2 vTexCoord;\n"
        "void main() {\n"
        "  vec4 pos = rotMat * vertex;\n"
        "  pos.z -= 5.0;\n"
        "  gl_Position = projMat * pos;\n"
        "  vTexCoord = texCoord;\n"
        "}\n";
    static const char frgSrc[] =
        "#version 300 es\n"
        "#extension GL_OES_EGL_image_external : require\n"
        "precision highp float;\n"
        "uniform samplerExternalOES texSampler;\n"
        "in vec2 vTexCoord;\n"
        "out vec4 fragColor;\n"
        "void main() {\n"
        "  if (vTexCoord.x < 0.01 || vTexCoord.x > 0.99 ||\n"
        "      vTexCoord.y < 0.01 || vTexCoord.y > 0.99) {\n"
        "    fragColor = vec4(1.0, 0.0, 0.0, 1.0);\n"
        "  } else {\n"
        // Note: Argus frames use a top-left origin and need to be inverted for GL texture use.
        "    vec2 texCoord = vec2(vTexCoord.x, 1.0 - vTexCoord.y);\n"
        "    fragColor = texture2D(texSampler, texCoord);\n"
        "  }\n"
        "}\n";
    PROPAGATE_ERROR(m_context.createProgram(vtxSrc, frgSrc, &m_program));
    glUseProgram(m_program);

    // Setup vertex state.
    static const GLfloat cubeVertices[] = {
         1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,// Front
        -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f,// Back
         1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,// Right
        -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f,// Left
         1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f,// Top
         1.0f, -1.0f, -1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f // Bottom
    };
    static const GLfloat cubeTexCoords[] = {
        1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f
    };
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, cubeVertices);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, cubeTexCoords);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    // Set static rendering state.
    glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    GLfloat projMat[16];
    MathUtils::createPerspectiveMatrix(
        45.0f, (float)window.getWidth()/(float)window.getHeight(), 1.0f, 1000.0f, projMat);
    glUniformMatrix4fv(glGetUniformLocation(m_program, "projMat"), 1, GL_FALSE, projMat);
    glUniform1i(glGetUniformLocation(m_program, "texSampler"), 0);

    // Create an external texture and connect it to the stream as a the consumer.
    CONSUMER_PRINT("Connecting to stream.\n");
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_texture);
    if (!eglStreamConsumerGLTextureExternalKHR(g_display.get(), m_stream))
        ORIGINATE_ERROR("Unable to connect GL as consumer");
    CONSUMER_PRINT("Connected to stream.\n");

    // Set the acquire timeout to infinite.
    eglStreamAttribKHR(g_display.get(), m_stream, EGL_CONSUMER_ACQUIRE_TIMEOUT_USEC_KHR, -1);

    return true;
}

bool ConsumerThread::threadExecute()
{
    CONSUMER_PRINT("Waiting until producer is connected...\n");
    while (true)
    {
        EGLint state = EGL_STREAM_STATE_CONNECTING_KHR;
        if (!eglQueryStreamKHR(g_display.get(), m_stream, EGL_STREAM_STATE_KHR, &state))
            ORIGINATE_ERROR("Failed to query stream state (possible producer failure).");
        if (state != EGL_STREAM_STATE_CONNECTING_KHR)
            break;
        usleep(1000);
    }
    CONSUMER_PRINT("Producer is connected; continuing.\n");

    // Render until there are no more frames (the producer has disconnected).
    uint32_t frame = 0;
    while (eglStreamConsumerAcquireKHR(g_display.get(), m_stream))
    {
        frame++;
        CONSUMER_PRINT("Acquired frame %d. Rendering.\n", frame);

        GLfloat rotMat[16];
        MathUtils::createRotationMatrix((float)frame, 0.3f, 0.5f, 0.2f, rotMat);
        glUniformMatrix4fv(glGetUniformLocation(m_program, "rotMat"), 1, GL_FALSE, rotMat);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        for (uint32_t i = 0; i < 6; i++)
            glDrawArrays(GL_TRIANGLE_STRIP, 4*i, 4);

        PROPAGATE_ERROR(m_context.swapBuffers());
    }
    CONSUMER_PRINT("No more frames. Cleaning up.\n");

    PROPAGATE_ERROR(requestShutdown());

    return true;
}

bool ConsumerThread::threadShutdown()
{
    glDeleteProgram(m_program);
    glDeleteTextures(1, &m_texture);
    m_context.cleanup();

    CONSUMER_PRINT("Done.\n");

    return true;
}

/*******************************************************************************
 * Argus Producer thread:
 *   Opens the Argus camera driver, creates an OutputStream to be consumed by
 *   the GL consumer, then performs repeating capture requests for CAPTURE_TIME
 *   seconds before closing the producer and Argus driver.
 ******************************************************************************/
static bool execute(const ArgusSamples::CommonOptions& options)
{
    // Initialize the window and EGL display.
    Window &window = Window::getInstance();
    window.setWindowRect(options.windowRect());
    PROPAGATE_ERROR(g_display.initialize(window.getEGLNativeDisplay()));

    // Initialize the Argus camera provider.
    UniqueObj<CameraProvider> cameraProvider(CameraProvider::create());
    ICameraProvider *iCameraProvider = interface_cast<ICameraProvider>(cameraProvider);
    if (!iCameraProvider)
        ORIGINATE_ERROR("Failed to get ICameraProvider interface");
    printf("Argus Version: %s\n", iCameraProvider->getVersion().c_str());

    // Get the selected camera device and sensor mode.
    CameraDevice* cameraDevice = ArgusHelpers::getCameraDevice(
            cameraProvider.get(), options.cameraDeviceIndex());
    if (!cameraDevice)
        ORIGINATE_ERROR("Selected camera device is not available");
    SensorMode* sensorMode = ArgusHelpers::getSensorMode(cameraDevice, options.sensorModeIndex());
    ISensorMode *iSensorMode = interface_cast<ISensorMode>(sensorMode);
    if (!iSensorMode)
        ORIGINATE_ERROR("Selected sensor mode not available");

    // Create the capture session using the specified device and get its interfaces.
    UniqueObj<CaptureSession> captureSession(iCameraProvider->createCaptureSession(cameraDevice));
    ICaptureSession *iCaptureSession = interface_cast<ICaptureSession>(captureSession);
    if (!iCaptureSession)
        ORIGINATE_ERROR("Failed to create CaptureSession");

    // Create the OutputStream.
    PRODUCER_PRINT("Creating output stream\n");
    UniqueObj<OutputStreamSettings> streamSettings(
        iCaptureSession->createOutputStreamSettings(STREAM_TYPE_EGL));
    IEGLOutputStreamSettings *iStreamSettings =
        interface_cast<IEGLOutputStreamSettings>(streamSettings);
    if (iStreamSettings)
    {
        iStreamSettings->setPixelFormat(PIXEL_FMT_YCbCr_420_888);
        iStreamSettings->setResolution(Size2D<uint32_t>(options.windowRect().width(),
                                                        options.windowRect().height()));
        iStreamSettings->setEGLDisplay(g_display.get());
    }
    UniqueObj<OutputStream> outputStream(iCaptureSession->createOutputStream(streamSettings.get()));
    IEGLOutputStream *iStream = interface_cast<IEGLOutputStream>(outputStream);
    if (!iStream)
        ORIGINATE_ERROR("Failed to create OutputStream");

    // Launch the GL consumer thread to consume frames from the OutputStream's EGLStream.
    PRODUCER_PRINT("Launching consumer thread\n");
    ConsumerThread glConsumerThread(iStream->getEGLStream());
    PROPAGATE_ERROR(glConsumerThread.initialize());

    // Wait until the consumer is connected to the stream.
    PROPAGATE_ERROR(glConsumerThread.waitRunning());

    // Create capture request and enable output stream.
    UniqueObj<Request> request(iCaptureSession->createRequest());
    IRequest *iRequest = interface_cast<IRequest>(request);
    if (!iRequest)
        ORIGINATE_ERROR("Failed to create Request");
    iRequest->enableOutputStream(outputStream.get());

    // Set the sensor mode in the request.
    ISourceSettings *iSourceSettings = interface_cast<ISourceSettings>(request);
    if (!iSourceSettings)
        ORIGINATE_ERROR("Failed to get source settings request interface");
    iSourceSettings->setSensorMode(sensorMode);

    // Submit capture requests.
    PRODUCER_PRINT("Starting repeat capture requests.\n");
    if (iCaptureSession->repeat(request.get()) != STATUS_OK)
        ORIGINATE_ERROR("Failed to start repeat capture request");

    // Wait for specified number of seconds.
    PROPAGATE_ERROR(window.pollingSleep(options.captureTime()));

    // Stop the repeating request and wait for idle.
    iCaptureSession->stopRepeat();
    iCaptureSession->waitForIdle();

    // Destroy the output stream. This destroys the EGLStream which causes
    // the GL consumer acquire to fail and the consumer thread to end.
    outputStream.reset();

    // Wait for the consumer thread to complete.
    PROPAGATE_ERROR(glConsumerThread.shutdown());

    // Shut down Argus.
    cameraProvider.reset();

    // Shut down the window (destroys window's EGLSurface).
    window.shutdown();

    // Cleanup the EGL display
    PROPAGATE_ERROR(g_display.cleanup());

    PRODUCER_PRINT("Done -- exiting.\n");
    return true;
}

}; // namespace ArgusSamples

int main(int argc, char **argv)
{
    ArgusSamples::CommonOptions options(basename(argv[0]),
                                        ArgusSamples::CommonOptions::Option_D_CameraDevice |
                                        ArgusSamples::CommonOptions::Option_M_SensorMode |
                                        ArgusSamples::CommonOptions::Option_R_WindowRect |
                                        ArgusSamples::CommonOptions::Option_T_CaptureTime);
    if (!options.parse(argc, argv))
        return EXIT_FAILURE;
    if (options.requestedExit())
        return EXIT_SUCCESS;

    if (!ArgusSamples::execute(options))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
