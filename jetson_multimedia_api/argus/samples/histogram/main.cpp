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
#include "Window.h"
#include "Thread.h"

#include <Argus/Argus.h>
#include <EGLStream/EGLStream.h>

#include <unistd.h>
#include <stdlib.h>
#include <iomanip>

using namespace Argus;

namespace ArgusSamples
{

// Constants.
static const uint32_t    DEFAULT_CAPTURE_TIME  = 10; // In seconds.
static const uint32_t    DEFAULT_CAMERA_INDEX = 0;
static const Rectangle<uint32_t> DEFAULT_WINDOW_RECT(0, 0, 1024, 768);
static const Size2D<uint32_t>    STREAM_SIZE(640, 480);

// Due to clipping and differences in color correction algorithms, the
// upper-most bins may negatively affect the graph scaling. This constant
// specifies how many of the upper-most bins are excluded from the graphs.
static const uint32_t    CLIP_BINS = 10;

// Globals.
UniqueObj<CameraProvider> g_cameraProvider;
EGLDisplayHolder g_display;

// Debug print macros.
#define PRODUCER_PRINT(...) printf("PRODUCER: " __VA_ARGS__)
#define CONSUMER_PRINT(...) printf("CONSUMER: " __VA_ARGS__)

/*******************************************************************************
 * Extended options class to add additional options specific to this sample.
 ******************************************************************************/
class HistogramSampleOptions : public CommonOptions
{
public:
    HistogramSampleOptions(const char *programName)
        : CommonOptions(programName,
                        ArgusSamples::CommonOptions::Option_D_CameraDevice |
                        ArgusSamples::CommonOptions::Option_M_SensorMode |
                        ArgusSamples::CommonOptions::Option_R_WindowRect |
                        ArgusSamples::CommonOptions::Option_T_CaptureTime)
        , m_useBayer(false)
    {
        addOption(createValueOption
            ("usebayer", 'b', "0 or 1", "Use Bayer histogram (instead of RGB).", m_useBayer, "1"));
    }

    bool useBayer() const { return m_useBayer.get(); }

protected:
    Value<bool> m_useBayer;
};

/*******************************************************************************
 * Histogram Consumer thread:
 *   Opens an on-screen GL window, and renders a live camera preview with
 *   histogram overlays.
 ******************************************************************************/
class ConsumerThread : public Thread
{
public:
    explicit ConsumerThread(const HistogramSampleOptions& options, EGLStreamKHR stream)
        : m_options(options)
        , m_stream(stream)
        , m_streamTexture(0)
        , m_textureProgram(0)
        , m_histogramProgram(0)
        , m_histogramTexture(0)
        , m_colorMaskLoc(0)
        , m_binMaxLoc(0)
        , m_combinedLoc(0)
    {
        uint32_t yDiv = HISTOGRAM_COUNT + 1;
        m_histogramSpacing = options.windowRect().height() / (yDiv * yDiv);
        m_histogramSize = Size2D<uint32_t>(
            options.windowRect().width() / 2 - (m_histogramSpacing * 2),
            options.windowRect().height() / yDiv);

    }
    ~ConsumerThread()
    {
    }

    enum HistogramType
    {
        HISTOGRAM_R,
        HISTOGRAM_G,
        HISTOGRAM_B,
        HISTOGRAM_RGB,
        HISTOGRAM_COMBINED,

        HISTOGRAM_COUNT
    };

private:
    /** @name Thread methods */
    /**@{*/
    virtual bool threadInitialize();
    virtual bool threadExecute();
    virtual bool threadShutdown();
    /**@}*/

    /**
     * Renders a single histogram to the display.
     * @param[in] type The HistogramType being rendered.
     * @param[in] label The label/title for the histogram.
     * @param[in] max The maximum value containined in the histogram.
     * @param[in] colorMask The color mask to use for the shader.
     */
    void renderHistogram(HistogramType type, const char* label,
                         uint32_t max, RGBTuple<float> colorMask);

    const HistogramSampleOptions& m_options;
    EGLStreamKHR m_stream;
    GLContext m_context;
    GLuint m_streamTexture;
    GLuint m_textureProgram;
    GLuint m_histogramProgram;
    GLuint m_histogramTexture;
    GLuint m_colorMaskLoc;
    GLuint m_binMaxLoc;
    GLuint m_combinedLoc;
    Size2D<uint32_t> m_histogramSize;
    uint32_t m_histogramSpacing;
};

bool ConsumerThread::threadInitialize()
{
    Window &window = Window::getInstance();

    // Create the context and make it current.
    CONSUMER_PRINT("Creating context.\n");
    PROPAGATE_ERROR(m_context.initialize(&window));
    PROPAGATE_ERROR(m_context.makeCurrent());

    // Create the shader program to render the texture.
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
            "precision highp float;\n"
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

    // Create the shader program to render the histograms.
    {
        static const char vtxSrc[] =
            "#version 300 es\n"
            "in layout(location = 0) vec2 coord;\n"
            "out vec2 binCoord;\n"
            "void main() {\n"
            "  gl_Position = vec4((coord * 2.0) - 1.0, 0.0, 1.0);\n"
            "  binCoord = coord;\n"
            "}\n";
        static const char frgSrc[] =
            "#version 300 es\n"
            "precision highp float;\n"
            "uniform usampler2D histogramSampler;\n"
            "uniform float binMax;\n"
            "uniform vec4 colorMask;\n"
            "uniform bool combined;\n"
            "in vec2 binCoord;\n"
            "out vec4 fragColor;\n"
            "void main() {\n"
            "  if (combined) {\n"
            "    uint totalCount = texture2D(histogramSampler, binCoord).a;\n"
            "    float totalValue = float(totalCount) / binMax;\n"
            "    fragColor = vec4(vec3(step(binCoord.y, totalValue)), colorMask.a);\n"
            "  } else {\n"
            "    uvec3 rgbCounts = texture2D(histogramSampler, binCoord).rgb;\n"
            "    vec3 binValues = vec3(rgbCounts) / binMax;\n"
            "    fragColor = colorMask * vec4(step(binCoord.y, binValues), 1.0);\n"
            "  }\n"
            "}\n";
        PROPAGATE_ERROR(m_context.createProgram(vtxSrc, frgSrc, &m_histogramProgram));
        glUseProgram(m_textureProgram);
        glUniform1i(glGetUniformLocation(m_histogramProgram, "histogramSampler"), 0);
        m_binMaxLoc = glGetUniformLocation(m_histogramProgram, "binMax");
        m_colorMaskLoc = glGetUniformLocation(m_histogramProgram, "colorMask");
        m_combinedLoc = glGetUniformLocation(m_histogramProgram, "combined");
    }

    // Initialize the vertex attrib state (used for both shader programs).
    static const GLfloat quadCoords[] = {1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, quadCoords);
    glEnableVertexAttribArray(0);

    // Enable alpha blending.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Set text parameters.
    m_context.setTextSize(16.0f / m_histogramSize.height(),
                          (float)m_histogramSize.height() / (float)m_histogramSize.width());
    m_context.setTextColor(1.0f, 1.0f, 1.0f);
    m_context.setTextBackground(0.0f, 0.0f, 0.0f, 0.5f);

    // Create a 1D integer texture to store the histogram values.
    glGenTextures(1, &m_histogramTexture);
    glBindTexture(GL_TEXTURE_2D, m_histogramTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32UI, 1, 1, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Create an external texture and connect it to the stream as the consumer.
    CONSUMER_PRINT("Connecting to stream.\n");
    glGenTextures(1, &m_streamTexture);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_streamTexture);
    if (!eglStreamConsumerGLTextureExternalKHR(g_display.get(), m_stream))
        ORIGINATE_ERROR("Unable to connect GL as consumer");
    CONSUMER_PRINT("Connected to stream.\n");

    // Set the acquire timeout to infinite.
    eglStreamAttribKHR(g_display.get(), m_stream, EGL_CONSUMER_ACQUIRE_TIMEOUT_USEC_KHR, -1);

    return true;
}

bool ConsumerThread::threadExecute()
{
    // Wait until the Argus producer is connected.
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

        // Render the image.
        glViewport(0, 0, m_options.windowRect().width(), m_options.windowRect().height());
        glUseProgram(m_textureProgram);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Get the metadata from the current EGLStream frame.
        // Note: This will likely fail for the last frame since the producer has
        //       already disconected from the EGLStream, so we need to handle
        //       failure gracefully.
        UniqueObj<EGLStream::MetadataContainer> metadataContainer(
            EGLStream::MetadataContainer::create(g_display.get(), m_stream));
        EGLStream::IArgusCaptureMetadata *iArgusCaptureMetadata =
            interface_cast<EGLStream::IArgusCaptureMetadata>(metadataContainer);
        if (iArgusCaptureMetadata)
        {
            CaptureMetadata *metadata = iArgusCaptureMetadata->getMetadata();
            const ICaptureMetadata* iMetadata = interface_cast<const ICaptureMetadata>(metadata);
            if (!iMetadata)
                ORIGINATE_ERROR("Failed to get Argus metadata\n");

            std::vector< Tuple<4, uint32_t> > rgbaData;
            if (m_options.useBayer())
            {
                // Read the Bayer histogram.
                std::vector< BayerTuple<uint32_t> > histogram;
                const IBayerHistogram* bayerHistogram =
                    interface_cast<const IBayerHistogram>(iMetadata->getBayerHistogram());
                if (!bayerHistogram || bayerHistogram->getHistogram(&histogram) != STATUS_OK)
                    ORIGINATE_ERROR("Failed to get histogram data\n");

                // Generate RGBA data by combining gEven/Odd channels and putting
                // combined totals into alpha channel.
                rgbaData.resize(histogram.size() - CLIP_BINS);
                for (uint32_t i = 0; i < histogram.size() - CLIP_BINS; i++)
                {
                    rgbaData[i][0] = histogram[i].r();
                    rgbaData[i][1] = (histogram[i].gEven() + histogram[i].gOdd()) / 2;
                    rgbaData[i][2] = histogram[i].b();
                    rgbaData[i][3] = rgbaData[i][0] + rgbaData[i][1] + rgbaData[i][2];
                }
            }
            else
            {
                // Read the RGB histogram.
                std::vector< RGBTuple<uint32_t> > histogram;
                const IRGBHistogram* rgbHistogram =
                    interface_cast<const IRGBHistogram>(iMetadata->getRGBHistogram());
                if (!rgbHistogram || rgbHistogram->getHistogram(&histogram) != STATUS_OK)
                    ORIGINATE_ERROR("Failed to get histogram data\n");

                // Generate RGBA data by appending totals into alpha channel.
                rgbaData.resize(histogram.size() - CLIP_BINS);
                for (uint32_t i = 0; i < histogram.size() - CLIP_BINS; i++)
                {
                    rgbaData[i][0] = histogram[i].r();
                    rgbaData[i][1] = histogram[i].g();
                    rgbaData[i][2] = histogram[i].b();
                    rgbaData[i][3] = rgbaData[i][0] + rgbaData[i][1] + rgbaData[i][2];
                }
            }

            // Push histogram data into the texture.
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32UI, rgbaData.size(), 1, 0,
                         GL_RGBA_INTEGER, GL_UNSIGNED_INT, &rgbaData[0]);

            // Determine the maximum values, used for graph scaling.
            Tuple<4, uint32_t> maxValues(0);
            for (uint32_t i = 0; i < rgbaData.size(); i++)
            {
                maxValues[0] = std::max(rgbaData[i][0], maxValues[0]);
                maxValues[1] = std::max(rgbaData[i][1], maxValues[1]);
                maxValues[2] = std::max(rgbaData[i][2], maxValues[2]);
                maxValues[3] = std::max(rgbaData[i][3], maxValues[3]);
            }
            uint rgbMax = std::max(maxValues[0], std::max(maxValues[1], maxValues[2]));

            // Render histograms.
            renderHistogram(HISTOGRAM_R, "Red", maxValues[0], RGBTuple<float>(1.0f, 0.0f, 0.0f));
            renderHistogram(HISTOGRAM_G, "Green", maxValues[1], RGBTuple<float>(0.0f, 1.0f, 0.0f));
            renderHistogram(HISTOGRAM_B, "Blue", maxValues[2], RGBTuple<float>(0.0f, 0.0f, 1.0f));
            renderHistogram(HISTOGRAM_RGB, "RGB", rgbMax, RGBTuple<float>(1.0f, 1.0f, 1.0f));
            renderHistogram(HISTOGRAM_COMBINED, "Overall", maxValues[3],
                            RGBTuple<float>(1.0f, 1.0f, 1.0f));
        }

        PROPAGATE_ERROR(m_context.swapBuffers());
    }
    CONSUMER_PRINT("No more frames. Cleaning up.\n");

    PROPAGATE_ERROR(requestShutdown());

    return true;
}

void ConsumerThread::renderHistogram(HistogramType type, const char* label,
                                     uint32_t max, RGBTuple<float> colorMask)
{
    const Rectangle<uint32_t> &window = m_options.windowRect();
    const Point2D<uint32_t> origin(
            window.width() / 2 + m_histogramSpacing,
            window.height() - (m_histogramSpacing + m_histogramSize.height()) * (type + 1));

    // Render histogram
    glUseProgram(m_histogramProgram);
    glUniform1i(m_combinedLoc, type == HISTOGRAM_COMBINED);
    glUniform1f(m_binMaxLoc, max);
    glUniform4f(m_colorMaskLoc, colorMask.r(), colorMask.g(), colorMask.b(), 0.8f);
    glViewport(origin.x(), origin.y(), m_histogramSize.width(), m_histogramSize.height());
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // Render text.
    std::ostringstream stream;
    stream << "Max:" << std::setw(7) << max;
    m_context.setTextPosition(0.02f, 0.98f);
    m_context.renderText(label);
    m_context.setTextPosition(0.75f, 0.96f);
    m_context.renderText(stream.str().c_str());
}

bool ConsumerThread::threadShutdown()
{
    glDeleteProgram(m_textureProgram);
    glDeleteProgram(m_histogramProgram);
    glDeleteTextures(1, &m_streamTexture);
    glDeleteTextures(1, &m_histogramTexture);
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
static bool execute(const HistogramSampleOptions& options)
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

    // Create the OutputStream.
    PRODUCER_PRINT("Creating output stream\n");
    UniqueObj<OutputStreamSettings> streamSettings(
        iCaptureSession->createOutputStreamSettings(STREAM_TYPE_EGL));
    IEGLOutputStreamSettings *iEGLStreamSettings =
        interface_cast<IEGLOutputStreamSettings>(streamSettings);
    if (iEGLStreamSettings)
    {
        iEGLStreamSettings->setPixelFormat(PIXEL_FMT_YCbCr_420_888);
        iEGLStreamSettings->setResolution(Size2D<uint32_t>(options.windowRect().width(),
                                                           options.windowRect().height()));
        iEGLStreamSettings->setEGLDisplay(g_display.get());
        iEGLStreamSettings->setMetadataEnable(true);
    }
    UniqueObj<OutputStream> outputStream(iCaptureSession->createOutputStream(streamSettings.get()));
    IEGLOutputStream *iEGLOutputStream = interface_cast<IEGLOutputStream>(outputStream);
    if (!iEGLOutputStream)
        ORIGINATE_ERROR("Failed to create EGLOutputStream");

    // Launch the consumer thread to consume frames from the OutputStream's EGLStream.
    PRODUCER_PRINT("Launching consumer thread\n");
    ConsumerThread consumerThread(options, iEGLOutputStream->getEGLStream());
    PROPAGATE_ERROR(consumerThread.initialize());

    // Wait until the consumer is connected to the stream.
    PROPAGATE_ERROR(consumerThread.waitRunning());

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
    PROPAGATE_ERROR(consumerThread.shutdown());

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

int main(int argc, char** argv)
{
    ArgusSamples::HistogramSampleOptions options(basename(argv[0]));
    if (!options.parse(argc, argv))
        return EXIT_FAILURE;
    if (options.requestedExit())
        return EXIT_SUCCESS;

    if (!ArgusSamples::execute(options))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
