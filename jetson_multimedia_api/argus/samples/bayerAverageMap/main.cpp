/*
 * Copyright (c) 2016-2018, NVIDIA CORPORATION. All rights reserved.
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
#include "Window.h"
#include "Thread.h"

#include <Argus/Argus.h>
#include <Argus/Ext/BayerAverageMap.h>
#include <EGLStream/EGLStream.h>

#include <unistd.h>
#include <stdlib.h>

using namespace Argus;

namespace ArgusSamples
{

// Constants.
static const float    TEXT_SIZE    = 32.0f;

// By default the average values are rendered to areas that include the
// bin spacing in order to make them more visible. Setting this to true will
// render the values such that only the bin area is filled.
static const bool     RENDER_BINS_ACTUAL_SIZE = false;

// The average values are based on raw Bayer data and may be a little dark.
// This multiplier can be used to tweak the rendered values to make them more visible.
static const float    AVERAGE_GAIN = 3.0f;

// Similar to the average gain, this can be used to brighten the clip severity.
static const float    CLIP_GAIN    = 3.0f;

// Globals.
UniqueObj<CameraProvider> g_cameraProvider;
EGLDisplayHolder g_display;

// BayerAverageMap coordinates are relative to the sensor mode resolution, so
// this must be available to the consumer in order to scale to window coordinates.
static Size2D<uint32_t> g_sensorModeResolution;

// Debug print macros.
#define PRODUCER_PRINT(...) printf("PRODUCER: " __VA_ARGS__)
#define CONSUMER_PRINT(...) printf("CONSUMER: " __VA_ARGS__)

/*******************************************************************************
 * GL Consumer thread:
 *   Opens an on-screen window and renders the camera preview to the top third,
 *   Bayer average values to the middle, and clip severity in the bottom third.
 ******************************************************************************/
class ConsumerThread : public Thread
{
public:
    explicit ConsumerThread(const ArgusSamples::CommonOptions& options,
                            IEGLOutputStream* stream)
        : m_options(options)
        , m_stream(stream)
        , m_eglStream(stream->getEGLStream())
        , m_streamTexture(0)
        , m_textureProgram(0)
        , m_binProgram(0)
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

    const ArgusSamples::CommonOptions& m_options;
    IEGLOutputStream* m_stream;
    EGLStreamKHR m_eglStream;
    GLContext m_context;
    GLuint m_streamTexture;
    GLuint m_textureProgram;
    GLuint m_binProgram;
};

bool ConsumerThread::threadInitialize()
{
    Window &window = Window::getInstance();

    // Create the context and make it current.
    CONSUMER_PRINT("Creating context.\n");
    PROPAGATE_ERROR(m_context.initialize(&window));
    PROPAGATE_ERROR(m_context.makeCurrent());
    m_context.setTextBackground(0.2f, 0.2f, 0.2f);

    // Initialize the shader for rendering bin values (both averages and clip counts).
    // Using a single shader program and viewport prevents excessive OpenGL state changes
    // which require a large amount of overhead if changed per-bin.
    // When the 'bayerQuad' uniform is true, the 'color' uniform will be rendered as a single
    // color value to the top half of the viewport. When 'bayerQuad' is false, the 'color' uniform
    // will be used to render the color as a Bayer quad to the bottom half of the viewport,
    // where color.g is the gEven component and color.a is the gOdd component.
    {
        static const char vtxSrc[] =
            "#version 300 es\n"
            "in layout(location = 0) vec2 binCoord;\n"
            "in layout(location = 1) vec2 vertexCoord;\n"
            "uniform bool bayerQuad;\n"
            "out vec2 vBinCoord;\n"
            "void main() {\n"
            "  float x = vertexCoord.x * 2.0 - 1.0;\n"
            "  float y = 1.0 - vertexCoord.y;\n"
            "  if (bayerQuad)\n"
            "    y = y - 1.0;\n"
            "  gl_Position = vec4(x, y, 0.0, 1.0);\n"
            "  vBinCoord = binCoord;\n"
            "}\n";
        static const char frgSrc[] =
            "#version 300 es\n"
            "precision lowp float;\n"
            "uniform vec4 color;\n"
            "uniform bool bayerQuad;\n"
            "in vec2 vBinCoord;\n"
            "out vec4 fragColor;\n"
            "void main() {\n"
            "  if (bayerQuad) {\n"
            "    if (vBinCoord.y > 0.5) {\n"
            "      if (vBinCoord.x < 0.5)\n"
            "        fragColor = vec4(color.r, 0.0, 0.0, 1.0);\n"
            "      else\n"
            "        fragColor = vec4(0.0, color.g, 0.0, 1.0);\n"
            "    } else {\n"
            "      if (vBinCoord.x < 0.5)\n"
            "        fragColor = vec4(0.0, color.a, 0.0, 1.0);\n"
            "      else\n"
            "        fragColor = vec4(0.0, 0.0, color.b, 1.0);\n"
            "    }\n"
            "  } else {\n"
            "    fragColor = color;\n"
            "  }\n"
            "}\n";
        PROPAGATE_ERROR(m_context.createProgram(vtxSrc, frgSrc, &m_binProgram));
    }

    // Initialize the shader for rendering the stream texture.
    {
        static const char vtxSrc[] =
            "#version 300 es\n"
            "in layout(location = 0) vec2 coord;\n"
            "out vec2 texCoord;\n"
            "void main() {\n"
            "  gl_Position = vec4((coord * 2.0) - 1.0, 0.0, 1.0);\n"
            "  texCoord = vec2(coord.x, 1.0 - coord.y);\n"
            "}\n";
        static const char frgSrc[] =
            "#version 300 es\n"
            "#extension GL_OES_EGL_image_external : require\n"
            "precision lowp float;\n"
            "uniform samplerExternalOES texSampler;\n"
            "in vec2 texCoord;\n"
            "out vec4 fragColor;\n"
            "void main() {\n"
            "  fragColor = texture2D(texSampler, texCoord);\n"
            "}\n";
        PROPAGATE_ERROR(m_context.createProgram(vtxSrc, frgSrc, &m_textureProgram));
        glUseProgram(m_textureProgram);
        glUniform1i(glGetUniformLocation(m_textureProgram, "texSampler"), 0);
    }

    // Initialize the shared vertex attrib state.
    static const GLfloat quadCoords[] = {1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, quadCoords);

    // Create an external texture and connect it to the stream as a the consumer.
    CONSUMER_PRINT("Connecting to stream.\n");
    glGenTextures(1, &m_streamTexture);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_streamTexture);
    if (!eglStreamConsumerGLTextureExternalKHR(g_display.get(), m_eglStream))
        ORIGINATE_ERROR("Unable to connect GL as consumer");
    CONSUMER_PRINT("Connected to stream.\n");

    // Set the acquire timeout to infinite.
    eglStreamAttribKHR(g_display.get(), m_eglStream, EGL_CONSUMER_ACQUIRE_TIMEOUT_USEC_KHR, -1);

    return true;
}

bool ConsumerThread::threadExecute()
{
    const Argus::Rectangle<uint32_t> windowRect = m_options.windowRect();

    // Wait until the Argus producer is connected.
    CONSUMER_PRINT("Waiting until producer is connected...\n");
    if (m_stream->waitUntilConnected() != STATUS_OK)
        ORIGINATE_ERROR("Stream failed to connect.");
    CONSUMER_PRINT("Producer is connected; continuing.\n");

    // Initialize the vertex attrib array state for bin rendering (values updated per bin, below).
    static float rectCoords[] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, rectCoords);

    // Render until there are no more frames (the producer has disconnected).
    uint32_t frame = 0;
    while (eglStreamConsumerAcquireKHR(g_display.get(), m_eglStream))
    {
        frame++;


        // Render the image to the left half of the window.
        glViewport(0, windowRect.height() / 4, windowRect.width() / 2, windowRect.height() / 2);
        glUseProgram(m_textureProgram);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        m_context.setTextSize(TEXT_SIZE / windowRect.height());
        m_context.setTextPosition(0.02f, 0.98f);
        char str[128];
        snprintf(str, sizeof(str), " Stream Output \n  Frame %4u   ", frame);
        m_context.renderText(str);

        // Get the metadata from the current EGLStream frame.
        // Note: This will likely fail for the last frame since the producer has
        //       already disconected from the EGLStream, so we need to handle
        //       failure gracefully.
        UniqueObj<EGLStream::MetadataContainer> metadataContainer(
            EGLStream::MetadataContainer::create(g_display.get(), m_eglStream));
        EGLStream::IArgusCaptureMetadata *iArgusCaptureMetadata =
            interface_cast<EGLStream::IArgusCaptureMetadata>(metadataContainer);
        if (iArgusCaptureMetadata)
        {
            // Get the BayerAverageMap metadata.
            CaptureMetadata *argusCaptureMetadata = iArgusCaptureMetadata->getMetadata();
            const Ext::IBayerAverageMap* iBayerAverageMap =
                interface_cast<const Ext::IBayerAverageMap>(argusCaptureMetadata);
            if (!iBayerAverageMap)
                ORIGINATE_ERROR("Failed to get IBayerAverageMap interface");
            Array2D< BayerTuple<float> > averages;
            if (iBayerAverageMap->getAverages(&averages) != STATUS_OK)
                ORIGINATE_ERROR("Failed to get averages");
            Array2D< BayerTuple<uint32_t> > clipCounts;
            if (iBayerAverageMap->getClipCounts(&clipCounts) != STATUS_OK)
                ORIGINATE_ERROR("Failed to get clip counts");
            const Point2D<uint32_t> binStart = iBayerAverageMap->getBinStart();
            const Size2D<uint32_t> binSize = iBayerAverageMap->getBinSize();
            const Size2D<uint32_t> binCount = iBayerAverageMap->getBinCount();
            const Size2D<uint32_t> binInterval = iBayerAverageMap->getBinInterval();

            float startX = 0.0f;
            float startY = 0.0f;
            float binWidth = 1.0f / binCount.width();
            float binHeight = 1.0f / binCount.height();
            float intervalX = binWidth;
            float intervalY = binHeight;
            if (RENDER_BINS_ACTUAL_SIZE)
            {
                startX = (float)binStart.x() / g_sensorModeResolution.width();
                startY = (float)binStart.y() / g_sensorModeResolution.width();
                binWidth = (float)binSize.width() / g_sensorModeResolution.width();
                binHeight = (float)binSize.height() / g_sensorModeResolution.height();
                intervalX = (float)binInterval.width() / g_sensorModeResolution.width();
                intervalY = (float)binInterval.width() / g_sensorModeResolution.width();
            }

            // Render the average map bins to the top right and the clip
            // count severity to the bottom right.
            glViewport(windowRect.width() / 2, 0, windowRect.width() / 2, windowRect.height());
            glUseProgram(m_binProgram);
            GLint colorUniform = glGetUniformLocation(m_binProgram, "color");
            GLint bayerUniform = glGetUniformLocation(m_binProgram, "bayerQuad");
            glEnableVertexAttribArray(1);
            for (uint32_t x = 0; x < binCount.width(); x++)
            {
                for (uint32_t y = 0; y < binCount.height(); y++)
                {
                    // Set the bin coordinates.
                    rectCoords[0] = startX + intervalX * x + binWidth;
                    rectCoords[1] = startY + intervalY * y;
                    rectCoords[2] = startX + intervalX * x + binWidth;
                    rectCoords[3] = startY + intervalY * y + binHeight;
                    rectCoords[4] = startX + intervalX * x;
                    rectCoords[5] = startY + intervalY * y;
                    rectCoords[6] = startX + intervalX * x;
                    rectCoords[7] = startY + intervalY * y + binHeight;

                    // Render the averages (clamped to [0, 1]).
                    float r  = std::max(0.0f, std::min(1.0f, averages(x, y).r()));
                    float gE = std::max(0.0f, std::min(1.0f, averages(x, y).gEven()));
                    float gO = std::max(0.0f, std::min(1.0f, averages(x, y).gOdd()));
                    float g = (gE + gO) / 2.0f;
                    float b  = std::max(0.0f, std::min(1.0f, averages(x, y).b()));
                    glUniform4f(colorUniform,
                                r * AVERAGE_GAIN, g * AVERAGE_GAIN, b * AVERAGE_GAIN, 1.0);
                    glUniform1i(bayerUniform, GL_FALSE);
                    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

                    // Render the clip severities.
                    float pixelsPerBin = binSize.width() * binSize.height();
                    r  = clipCounts(x, y).r() / pixelsPerBin;
                    gE = clipCounts(x, y).gEven() / pixelsPerBin;
                    gO = clipCounts(x, y).gOdd() / pixelsPerBin;
                    b  = clipCounts(x, y).b() / pixelsPerBin;
                    glUniform4f(colorUniform,
                                r * CLIP_GAIN, gE * CLIP_GAIN, b * CLIP_GAIN, gO * CLIP_GAIN);
                    glUniform1i(bayerUniform, GL_TRUE);
                    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                }
            }
            glDisableVertexAttribArray(1);

            // Add average/clip viewport labels.
            m_context.setTextSize(TEXT_SIZE / windowRect.height() / 2.0, 2.0);
            m_context.setTextPosition(0.02f, 0.98);
            m_context.renderText(" Bayer Averages ");
            m_context.setTextPosition(0.02f, 0.48);
            m_context.renderText(" Clip Severity ");
        }

        PROPAGATE_ERROR(m_context.swapBuffers());
    }
    CONSUMER_PRINT("No more frames. Cleaning up.\n");

    PROPAGATE_ERROR(requestShutdown());

    return true;
}

bool ConsumerThread::threadShutdown()
{
    glDeleteProgram(m_textureProgram);
    glDeleteTextures(1, &m_streamTexture);
    m_context.cleanup();

    CONSUMER_PRINT("Done.\n");

    return true;
}

/*******************************************************************************
 * Argus Producer thread:
 *   Opens the Argus camera driver, creates an OutputStream to be consumed by the
 *   GL consumer, then performs repeating capture requests for a given number of
 *   seconds before closing the producer and Argus driver.
 ******************************************************************************/
static bool execute(const ArgusSamples::CommonOptions& options)
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

    // Ensure BayerAverageMap extension is supported.
    if (!iCameraProvider->supportsExtension(EXT_BAYER_AVERAGE_MAP))
        ORIGINATE_ERROR("Face detection not supported.");

    // Get the selected CameraDevice and SensorMode.
    CameraDevice *cameraDevice = ArgusHelpers::getCameraDevice(
            g_cameraProvider.get(), options.cameraDeviceIndex());
    ICameraProperties *iCameraProperties = interface_cast<ICameraProperties>(cameraDevice);
    if (!iCameraProperties)
        ORIGINATE_ERROR("Failed to get ICameraProperties interface");
    SensorMode* sensorMode = ArgusHelpers::getSensorMode(cameraDevice, options.sensorModeIndex());
    ISensorMode *iSensorMode = interface_cast<ISensorMode>(sensorMode);
    if (!iSensorMode)
        ORIGINATE_ERROR("Failed to get ISensorMode interface");

    // Create CaptureSession.
    UniqueObj<CaptureSession> captureSession(iCameraProvider->createCaptureSession(cameraDevice));
    ICaptureSession *iSession = interface_cast<ICaptureSession>(captureSession);
    if (!iSession)
        ORIGINATE_ERROR("Failed to create CaptureSession");

    // Create the OutputStream, which only needs to fill 1/4 of the window.
    PRODUCER_PRINT("Creating output stream\n");
    UniqueObj<OutputStreamSettings> streamSettings(
        iSession->createOutputStreamSettings(STREAM_TYPE_EGL));
    IEGLOutputStreamSettings *iStreamSettings =
        interface_cast<IEGLOutputStreamSettings>(streamSettings);
    if (!iStreamSettings)
        ORIGINATE_ERROR("Failed to create OutputStreamSettings");
    iStreamSettings->setPixelFormat(PIXEL_FMT_YCbCr_420_888);
    iStreamSettings->setResolution(Size2D<uint32_t>(options.windowRect().width() / 2,
                                                    options.windowRect().height() / 2));
    iStreamSettings->setEGLDisplay(g_display.get());
    iStreamSettings->setMetadataEnable(true);
    UniqueObj<OutputStream> outputStream(iSession->createOutputStream(streamSettings.get()));
    IEGLOutputStream *iStream = interface_cast<IEGLOutputStream>(outputStream);
    if (!iStream)
        ORIGINATE_ERROR("Failed to create OutputStream");

    // Launch the GL consumer thread to consume frames from the OutputStream's EGLStream.
    PRODUCER_PRINT("Launching consumer thread\n");
    ConsumerThread glConsumerThread(options, iStream);
    PROPAGATE_ERROR(glConsumerThread.initialize());

    // Wait until the consumer is connected to the stream.
    PROPAGATE_ERROR(glConsumerThread.waitRunning());

    // Create capture request and enable output stream and set sensor mode.
    UniqueObj<Request> request(iSession->createRequest());
    IRequest *iRequest = interface_cast<IRequest>(request);
    if (!iRequest)
        ORIGINATE_ERROR("Failed to create Request");
    iRequest->enableOutputStream(outputStream.get());
    ISourceSettings* iSourceSettings = interface_cast<ISourceSettings>(request);
    if (!iSourceSettings)
        ORIGINATE_ERROR("Failed to get SourceSettings interface");
    iSourceSettings->setSensorMode(sensorMode);

    // Enable BayerAverageMap generation in the request.
    Ext::IBayerAverageMapSettings *iBayerAverageMapSettings =
        interface_cast<Ext::IBayerAverageMapSettings>(request);
    if (!iBayerAverageMapSettings)
        ORIGINATE_ERROR("Failed to get BayerAverageMapSettings interface");
    iBayerAverageMapSettings->setBayerAverageMapEnable(true);

    // Submit capture requests.
    PRODUCER_PRINT("Starting repeat capture requests.\n");
    if (iSession->repeat(request.get()) != STATUS_OK)
        ORIGINATE_ERROR("Failed to start repeat capture request");

    // Wait for the specified number of seconds.
    PROPAGATE_ERROR(window.pollingSleep(options.captureTime()));

    // Stop the repeating request and wait for idle.
    iSession->stopRepeat();
    iSession->waitForIdle();

    // Destroy the output stream. This destroys the EGLStream which causes
    // the GL consumer acquire to fail and the consumer thread to end.
    outputStream.reset();

    // Wait for the consumer thread to complete.
    PROPAGATE_ERROR(glConsumerThread.shutdown());

    // Shut down Argus.
    g_cameraProvider.reset();

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
