/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION. All rights reserved.
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
#include "GLContext.h"
#include "NativeBuffer.h"
#include "Thread.h"
#include "Window.h"

#include <Argus/Argus.h>

#include <unistd.h>
#include <stdlib.h>

using namespace Argus;

namespace ArgusSamples
{

// Constants.
static const uint32_t NUM_BUFFERS = 10;

// Globals.
UniqueObj<CameraProvider> g_cameraProvider;
EGLDisplayHolder g_display;
PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;

/*******************************************************************************
 * Extended options class to add additional options specific to this sample.
 ******************************************************************************/
class EGLImageSampleOptions : public CommonOptions
{
public:
    EGLImageSampleOptions(const char *programName)
        : CommonOptions(programName,
                        ArgusSamples::CommonOptions::Option_D_CameraDevice |
                        ArgusSamples::CommonOptions::Option_M_SensorMode |
                        ArgusSamples::CommonOptions::Option_R_WindowRect |
                        ArgusSamples::CommonOptions::Option_T_CaptureTime)
        , m_useEGLSync(true)
    {
        addOption(createValueOption
            ("eglsync", 's', "0 or 1", "Enable EGLSync usage.", m_useEGLSync, "1"));
    }

    bool useEGLSync() const { return m_useEGLSync.get(); }

protected:
    Value<bool> m_useEGLSync;
};

/*******************************************************************************
 * EGLImage Rendering Thread.
 *   Acquires completed Buffers from an EGLImage OutputStream and renders them
 *   to an on-screen window.
 ******************************************************************************/
class EGLImageRenderingThread : public Thread
{
public:
    explicit EGLImageRenderingThread(OutputStream* stream, const EGLImageSampleOptions& options)
        : m_stream(stream)
        , m_options(options)
    {
    }
    ~EGLImageRenderingThread()
    {
    }

private:
    /** @name Thread methods */
    /**@{*/
    virtual bool threadInitialize();
    virtual bool threadExecute();
    virtual bool threadShutdown();
    /**@}*/

    OutputStream* m_stream;
    const EGLImageSampleOptions& m_options;

    GLContext m_context;
    GLuint m_program;
    GLuint m_texture;
};

bool EGLImageRenderingThread::threadInitialize()
{
    Window &window = Window::getInstance();

    // Create the context and make it current.
    PROPAGATE_ERROR(m_context.initialize(&window));
    PROPAGATE_ERROR(m_context.makeCurrent());

    glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)
        eglGetProcAddress("glEGLImageTargetTexture2DOES");

    // Create the shader program to render the texture.
    {
        static const char vtxSrc[] =
            "#version 300 es\n"
            "in layout(location = 0) vec2 coord;\n"
            "out vec2 texCoord;\n"
            "void main() {\n"
            "  gl_Position = vec4((coord * 2.0) - 1.0, 0.0, 1.0);\n"
            // Note: Argus frames use a top-left origin and need to be inverted for GL texture use.
            "  texCoord = vec2(coord.x, 1.0 - coord.y);\n"
            "}\n";
        static const char frgSrc[] =
            "#version 300 es\n"
            "#extension GL_OES_EGL_image_external : require\n"
            "precision highp float;\n"
            "uniform samplerExternalOES texSampler;\n"
            "in vec2 texCoord;\n"
            "out vec4 fragColor;\n"
            "void main() {\n"
            "  fragColor = texture2D(texSampler, texCoord);\n"
            "}\n";
        PROPAGATE_ERROR(m_context.createProgram(vtxSrc, frgSrc, &m_program));
        glUseProgram(m_program);
        glUniform1i(glGetUniformLocation(m_program, "texSampler"), 0);
    }

    // Set the vertex attrib state.
    static const GLfloat quadCoords[] = {1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, quadCoords);
    glEnableVertexAttribArray(0);

    // Create the external texture.
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_texture);

    return true;
}

bool EGLImageRenderingThread::threadExecute()
{
    IBufferOutputStream* stream = interface_cast<IBufferOutputStream>(m_stream);
    if (!stream)
        ORIGINATE_ERROR("Failed to get IBufferOutputStream interface");

    while (true)
    {
        Window::getInstance().pollEvents();

        // Acquire a Buffer from a completed capture request until END_OF_STREAM is received.
        Argus::Status status = STATUS_OK;
        Buffer* buffer = stream->acquireBuffer(TIMEOUT_INFINITE, &status);
        if (status == STATUS_END_OF_STREAM)
        {
            break;
        }

        IEGLImageBuffer* eglImageBuffer = interface_cast<IEGLImageBuffer>(buffer);
        if (!eglImageBuffer)
            ORIGINATE_ERROR("Failed to get IEGLImageBuffer interface");

        if (m_options.useEGLSync())
        {
            // Get (create) the EGLSync acquire sync.
            IEGLSync* eglSync = interface_cast<IEGLSync>(buffer);
            if (!eglSync)
                ORIGINATE_ERROR("Failed to get IEGLSync interface");
            EGLSyncKHR acquireSync = EGL_NO_SYNC_KHR;
            if (eglSync->getAcquireSync(g_display.get(), &acquireSync) != STATUS_OK)
                ORIGINATE_ERROR("Failed to get acquire sync");

            // getAcquireSync may return EGL_NO_SYNC_KHR, in which case no sync is needed.
            if (acquireSync != EGL_NO_SYNC_KHR)
            {
                // Push the EGLSync to the GL command stream to block all future rendering
                // operations. This ensures that GL will not read from the buffer until
                // libargus has completed all write operations.
                if (!eglWaitSyncKHR(g_display.get(), acquireSync, 0))
                    ORIGINATE_ERROR("Failed to wait on EGLSync");
                eglDestroySyncKHR(g_display.get(), acquireSync);
            }
        }

        // Render the EGLImage from the buffer.
        glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, eglImageBuffer->getEGLImage());
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        if (m_options.useEGLSync())
        {
            // Create an EGLSync object from the GL context that will correspond to the end of all
            // previous rendering operations. This is then pushed to the Buffer before it is
            // released so that libargus will not use the buffer until after all GL operations
            // using the Buffer have completed.
            IEGLSync* eglSync = interface_cast<IEGLSync>(buffer);
            if (!eglSync)
                ORIGINATE_ERROR("Failed to get IEGLSync interface");
            EGLSyncKHR releaseSync = eglCreateSyncKHR(g_display.get(), EGL_SYNC_FENCE_KHR, NULL);
            if (releaseSync == EGL_NO_SYNC_KHR)
                ORIGINATE_ERROR("Failed to issue EGLSync");
            if (eglSync->setReleaseSync(g_display.get(), releaseSync) != STATUS_OK)
                ORIGINATE_ERROR("Failed to set release sync");
        }
        else
        {
            // If sync is disabled, we must ensure all reads from the Buffer have completed
            // before we release the Buffer back to the libargus Stream.
            glFinish();
        }

        // Release the Buffer back to the stream.
        stream->releaseBuffer(buffer);

        PROPAGATE_ERROR(m_context.swapBuffers());
    }

    PROPAGATE_ERROR(requestShutdown());

    return true;
}

bool EGLImageRenderingThread::threadShutdown()
{
    glDeleteTextures(1, &m_texture);
    glDeleteProgram(m_program);
    m_context.cleanup();

    return true;
}


/*******************************************************************************
 * This sample uses an BufferOutputStream with EGLImage sibling Buffers in
 * order to output capture request results to EGLImages that are allocated
 * and managed by this application.
 ******************************************************************************/
static bool execute(const EGLImageSampleOptions& options)
{
    // Initialize the window and EGL display.
    Window &window = Window::getInstance();
    window.setWindowRect(options.windowRect());
    PROPAGATE_ERROR(g_display.initialize(window.getEGLNativeDisplay()));

    // Initialize the Argus camera provider.
    g_cameraProvider = UniqueObj<CameraProvider>(CameraProvider::create());
    ICameraProvider *iCameraProvider = interface_cast<ICameraProvider>(g_cameraProvider);
    if (!iCameraProvider)
        ORIGINATE_ERROR("Failed to get ICameraProvider interface");
    printf("Argus Version: %s\n", iCameraProvider->getVersion().c_str());

    // Get the selected camera device and sensor mode.
    CameraDevice* cameraDevice = ArgusHelpers::getCameraDevice(
            g_cameraProvider.get(), options.cameraDeviceIndex());
    if (!cameraDevice)
        ORIGINATE_ERROR("Selected camera device is not available");
    SensorMode* sensorMode = ArgusHelpers::getSensorMode(cameraDevice, options.sensorModeIndex());
    ISensorMode *iSensorMode = interface_cast<ISensorMode>(sensorMode);
    if (!iSensorMode)
        ORIGINATE_ERROR("Selected sensor mode not available");

    // Create the capture session using the specified device and get its interfaces.
    UniqueObj<CaptureSession> captureSession(iCameraProvider->createCaptureSession(cameraDevice));
    ICaptureSession *iCaptureSession = interface_cast<ICaptureSession>(captureSession);
    IEventProvider *iEventProvider = interface_cast<IEventProvider>(captureSession);
    if (!iCaptureSession || !iEventProvider)
        ORIGINATE_ERROR("Failed to create CaptureSession");

    // Create the OutputStreamSettings object for a Buffer-based OutputStream.
    UniqueObj<OutputStreamSettings> streamSettings(
        iCaptureSession->createOutputStreamSettings(STREAM_TYPE_BUFFER));
    IBufferOutputStreamSettings *iStreamSettings =
        interface_cast<IBufferOutputStreamSettings>(streamSettings);
    if (!iStreamSettings)
        ORIGINATE_ERROR("Failed to create OutputStreamSettings");

    // Configure the OutputStream to use the EGLImage BufferType.
    iStreamSettings->setBufferType(BUFFER_TYPE_EGL_IMAGE);

    // Enable EGLSync use with the stream (if requested).
    if (options.useEGLSync() && (iStreamSettings->setSyncType(SYNC_TYPE_EGL_SYNC) != STATUS_OK))
        ORIGINATE_ERROR("EGLSync not supported");

    // Create the OutputStream.
    UniqueObj<OutputStream> outputStream(iCaptureSession->createOutputStream(streamSettings.get()));
    IBufferOutputStream *iBufferOutputStream = interface_cast<IBufferOutputStream>(outputStream);
    if (!iBufferOutputStream)
        ORIGINATE_ERROR("Failed to create BufferOutputStream");

    // Allocate native buffers.
    NativeBuffer* nativeBuffers[NUM_BUFFERS];
    for (uint32_t i = 0; i < NUM_BUFFERS; i++)
    {
        nativeBuffers[i] = NativeBuffer::create(Size2D<uint32_t>(options.windowRect().width(),
                                                                 options.windowRect().height()));
        if (!nativeBuffers[i])
            ORIGINATE_ERROR("Failed to allocate NativeBuffer");
    }

    // Create EGLImages from the native buffers.
    EGLImageKHR eglImages[NUM_BUFFERS];
    for (uint32_t i = 0; i < NUM_BUFFERS; i++)
    {
        eglImages[i] = nativeBuffers[i]->createEGLImage(g_display.get());
        if (eglImages[i] == EGL_NO_IMAGE_KHR)
            ORIGINATE_ERROR("Failed to create EGLImage");
    }

    // Create the BufferSettings object to configure Buffer creation.
    UniqueObj<BufferSettings> bufferSettings(iBufferOutputStream->createBufferSettings());
    IEGLImageBufferSettings *iBufferSettings =
        interface_cast<IEGLImageBufferSettings>(bufferSettings);
    if (!iBufferSettings)
        ORIGINATE_ERROR("Failed to create BufferSettings");
    iBufferSettings->setEGLDisplay(g_display.get());

    // Create the Buffers for each EGLImage (and release to stream for initial capture use).
    UniqueObj<Buffer> buffers[NUM_BUFFERS];
    for (uint32_t i = 0; i < NUM_BUFFERS; i++)
    {
        iBufferSettings->setEGLImage(eglImages[i]);
        buffers[i].reset(iBufferOutputStream->createBuffer(bufferSettings.get()));
        if (!interface_cast<IEGLImageBuffer>(buffers[i]))
            ORIGINATE_ERROR("Failed to create Buffer");
        if (iBufferOutputStream->releaseBuffer(buffers[i].get()) != STATUS_OK)
            ORIGINATE_ERROR("Failed to release Buffer for capture use");
    }

    // Create capture request, set the sensor mode, and enable the output stream.
    UniqueObj<Request> request(iCaptureSession->createRequest());
    IRequest *iRequest = interface_cast<IRequest>(request);
    if (!iRequest)
        ORIGINATE_ERROR("Failed to create Request");
    iRequest->enableOutputStream(outputStream.get());
    ISourceSettings *iSourceSettings = interface_cast<ISourceSettings>(request);
    if (!iSourceSettings)
        ORIGINATE_ERROR("Failed to get source settings request interface");
    iSourceSettings->setSensorMode(sensorMode);

    // Start the EGLImage rendering thread.
    EGLImageRenderingThread renderingThread(outputStream.get(), options);
    PROPAGATE_ERROR(renderingThread.initialize());
    PROPAGATE_ERROR(renderingThread.waitRunning());

    // Submit capture requests.
    if (iCaptureSession->repeat(request.get()) != STATUS_OK)
        ORIGINATE_ERROR("Failed to start repeat capture request");

    // Wait for (capture) specified number of seconds.
    PROPAGATE_ERROR(window.pollingSleep(options.captureTime()));

    // Stop the repeating request and signal end of stream to stop the rendering thread.
    iCaptureSession->stopRepeat();
    iBufferOutputStream->endOfStream();

    // Wait for the rendering thread to complete.
    PROPAGATE_ERROR(renderingThread.shutdown());

    // Destroy the buffers and output stream.
    for (uint32_t i = 0; i < NUM_BUFFERS; i++)
        buffers[i].reset();
    outputStream.reset();

    // Destroy the EGLImages.
    for (uint32_t i = 0; i < NUM_BUFFERS; i++)
        eglDestroyImageKHR(g_display.get(), eglImages[i]);

    // Destroy the native buffers.
    for (uint32_t i = 0; i < NUM_BUFFERS; i++)
        delete nativeBuffers[i];

    // Shut down Argus.
    g_cameraProvider.reset();

    // Shut down the window (destroys window's EGLSurface).
    window.shutdown();

    // Cleanup the EGL display
    PROPAGATE_ERROR(g_display.cleanup());

    return true;
}

}; // namespace ArgusSamples

int main(int argc, char** argv)
{
    ArgusSamples::EGLImageSampleOptions options(basename(argv[0]));
    if (!options.parse(argc, argv))
        return EXIT_FAILURE;
    if (options.requestedExit())
        return EXIT_SUCCESS;

    printf("EGLSync enabled: %s\n", options.useEGLSync() ? "true" : "false");

    if (!ArgusSamples::execute(options))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
