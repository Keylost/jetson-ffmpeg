/*
 * Copyright (c) 2016 - 2019, NVIDIA CORPORATION. All rights reserved.
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

#include <Argus/Argus.h>
#include <gst/gst.h>
#include <stdlib.h>
#include <unistd.h>
#include "ArgusHelpers.h"
#include "CommonOptions.h"
#include "Error.h"
#include "PreviewConsumer.h"
#include <string>
#include <queue>

namespace ArgusSamples
{

// Globals.
EGLDisplayHolder g_display;
volatile sig_atomic_t shutdownSignalReceived = 0;

void ctrl_c_sig_handler(int signo);
void ctrl_c_sig_handler(int signo)
{
    shutdownSignalReceived = 1;
}

/*******************************************************************************
 * Extended options class to add additional options specific to this sample.
 ******************************************************************************/
class GstVideoEncodeSampleOptions : public CommonOptions
{
public:
    enum NoiseModeArg
    {
        NOISE_MODE_ARG_OFF = 1,
        NOISE_MODE_ARG_FAST = 2,
        NOISE_MODE_ARG_HIGH_QUALITY = 3
    };

    GstVideoEncodeSampleOptions(const char *programName)
        : CommonOptions(programName,
                        ArgusSamples::CommonOptions::Option_D_CameraDevice |
                        ArgusSamples::CommonOptions::Option_M_SensorMode |
                        ArgusSamples::CommonOptions::Option_R_WindowRect |
                        ArgusSamples::CommonOptions::Option_T_CaptureTime)
        , m_enablePreview(true)
        , m_framerate(0)
        , m_bitrate(14000000)
        , m_useP016(false)
        , m_nrMode(NOISE_MODE_ARG_FAST)
        , m_useH264(false)
        , m_useWdr(false)
        , m_path("./")
        , m_filename("argus_gstvideoencode_out")
        , m_remoteAddress("")
        , m_remoteUsername("")
        , m_remotePath("~/")
        , m_keepLocalFiles(false)
        , m_partSize(0)
    {
        addOption(createValueOption
            ("preview", 'p', "0 or 1", "Enable preview window.", m_enablePreview, "1"));
        addOption(createValueOption
            ("framerate", 0, "RATE", "Video recording framerate. 0 indicates sensor mode maximum.",
             m_framerate));
        addOption(createValueOption
            ("bitrate", 0, "RATE", "Video recording bitrate.", m_bitrate));
        addOption(createValueOption
            ("usep016", 0, "FLAG", "Use P016 deep-color format (instead of NV12).", m_useP016));
        addOption(createValueOption
            ("nrmode", 'n', "MODE", "Noise reduction: 1=off, 2=fast, 3=best", m_nrMode));
        addOption(createValueOption
            ("useh264", 0, "FLAG", "Use H.264 video format (else H.265)", m_useH264));
        addOption(createValueOption
            ("usewdr", 'w', "0 or 1", "Force the use of a WDR SensorMode. This will use the first "
             "available WDR (DOL or PWL) mode, and overrides any explicit --mode setting.",
             m_useWdr, "1"));
        addOption(createValueOption
            ("path", 0, "PATH", "Output path (directory) for the output file(s). This path must be "
             "writable by the application, and files will be written to this location regardless"
             "of whether or not files are being sent to a remote server. See --keeplocal.",
             m_path));
        addOption(createValueOption
            ("file", 0, "FILE", "Filename for the output file(s). This will be prefixed to "
             "the part number and file type suffix (i.e. '.mkv') that will be appended by the app.",
             m_filename));
        addOption(createValueOption
            ("partsize", 0, "MBYTES", "Maximum size, in MBytes, of the output video files. When "
             "non-zero, the video output will be divided into multiple parts of this maximum size, "
             "identified by a part number appended to the filename. These parts are written as "
             "raw HEVC data (i.e. it is not written to an mp4 or mkv container).",
             m_partSize, "0"));
        addOption(createValueOption
            ("remoteaddress", 0, "ADDRESS", "Address (host name or IP address) for remote file writes.",
             m_remoteAddress));
        addOption(createValueOption
            ("remoteusername", 0, "USER", "Username for remote file writes. Note that no 'password' "
             "option is available; the expectation is that SSH passwordless login is configured "
             "for this user on the remote device.",
             m_remoteUsername));
        addOption(createValueOption
            ("remotepath", 0, "PATH", "Path on the remote server where the files will be copied.",
             m_remotePath));
        addOption(createValueOption
            ("keeplocal", 0, "FLAG", "When remote copies are enabled, this flag will specify "
             "whether or not the local copy should be deleted after being copied.",
             m_keepLocalFiles));

        addDescription("\nThis sample demonstrates the use of a GStreamer consumer for encoding\n"
                       "video streams from an Argus OutputStream.\n\n"
                       "In order to support 24x7 use cases such as security camera recording,\n"
                       "this sample can record indefinitely (specified with a capture time of 0),\n"
                       "and can optionally write the output stream to multiple file parts using\n"
                       "a maximum file size. These parts can then be optionally copied to a\n"
                       "remote server, and then kept or deleted from the local device.\n"
                       "For example, the following will indefinitely record video in 100MB chunks\n"
                       "that will be written to the local path '/argusLocal/' before being copied\n"
                       "to 192.168.1.1:/argusRemote/ using the 'argus' username. The local files\n"
                       "will also be preserved using --keeplocal\n\n"
                       "  argus_gstvideoencode -t0 \\\n"
                       "                       --partsize=100 \\\n"
                       "                       --path=/argusLocal/ \\\n"
                       "                       --remoteaddress=192.168.1.1 \\\n"
                       "                       --remoteusername=argus \\\n"
                       "                       --remotepath=/argusRemote/ \\\n"
                       "                       --keeplocal\n\n");
    }

    bool enablePreview() const { return m_enablePreview.get(); }
    uint32_t framerate() const { return m_framerate.get(); }
    uint32_t bitrate() const { return m_bitrate.get(); }
    bool useP016() const { return m_useP016.get(); }
    NoiseModeArg nrMode() const { return static_cast<NoiseModeArg>(m_nrMode.get()); }
    bool useH264() const { return m_useH264.get(); }
    bool useWdr() const { return m_useWdr.get(); }
    const std::string& path() const { return m_path.get(); }
    const std::string& filename() const { return m_filename.get(); }
    const std::string& address() const { return m_remoteAddress.get(); }
    const std::string& username() const { return m_remoteUsername.get(); }
    const std::string& remotePath() const { return m_remotePath.get(); }
    bool keepLocalFiles() const { return m_keepLocalFiles.get(); }
    uint64_t partSize() const { return m_partSize.get() * 1024 * 1024; }
    bool isRemoteDestination() const { return m_remoteAddress.get() != ""; }

protected:
    Value<bool> m_enablePreview;
    Value<uint32_t> m_framerate;
    Value<uint32_t> m_bitrate;
    Value<bool> m_useP016;
    Value<uint32_t> m_nrMode;
    Value<bool> m_useH264;
    Value<bool> m_useWdr;
    Value<std::string> m_path;
    Value<std::string> m_filename;
    Value<std::string> m_remoteAddress;
    Value<std::string> m_remoteUsername;
    Value<std::string> m_remotePath;
    Value<bool> m_keepLocalFiles;
    Value<uint32_t> m_partSize;
};

/**
 * Utility thread to copy files to a remote server using scp.
 * This class is only used when the --remoteaddress option is provided for remote file writes.
 */
class FileTransferThread : public Thread
{
public:
    FileTransferThread(const GstVideoEncodeSampleOptions& options)
        : m_options(options)
    {
    }

    /**
     * Queues a file to be transfered to the remote destination.
     * If the keepLocalFiles option is false, the source file is deleted after the copy.
     */
    void sendFile(const std::string& fileName)
    {
        m_queue.push(fileName);
    }

private:
    virtual bool threadInitialize() { return true; }
    virtual bool threadShutdown() { return true; }

    virtual bool threadExecute()
    {
        while (!m_doShutdown && m_queue.size() == 0)
            sleep(1);

        if (m_queue.size() > 0)
        {
            // Copy file to server.
            const char* filename = m_queue.front().c_str();
            std::string cmd = std::string("scp ") + filename + " " + m_options.username() +
                              "@" + m_options.address() + ":" + m_options.remotePath() +
                              " > /dev/null";
            if (system(cmd.c_str()))
            {
                printf("Failed to copy %s to %s\n", filename, m_options.address().c_str());
                return false;
            }
            else
            {
                printf("Copied %s to %s:%s  (source file is being %s)\n",
                       filename, m_options.address().c_str(),
                       m_options.remotePath().c_str(),
                       (m_options.keepLocalFiles() ? "kept" : "deleted"));
            }

            if (!m_options.keepLocalFiles())
            {
                // Delete file.
                cmd = std::string("rm ") + filename;
                if (system(cmd.c_str()))
                {
                    printf("Failed to delete %s\n", filename);
                }
            }

            m_queue.pop();
        }

        return true;
    }

    const GstVideoEncodeSampleOptions& m_options;
    std::queue< std::string > m_queue;
};


/**
 * Class to initialize and control GStreamer video encoding from an EGLStream.
 */
class GstVideoEncoder
{
public:
    GstVideoEncoder(const GstVideoEncodeSampleOptions& options)
        : m_options(options)
        , m_state(GST_STATE_NULL)
        , m_pipeline(NULL)
        , m_videoEncoder(NULL)
        , m_fileTransferThread(NULL)
        , m_currentPartNumber(0)
        , m_currentPartFile(NULL)
        , m_totalBytesWritten(0)
    {
    }

    ~GstVideoEncoder()
    {
        shutdown();
    }

    /**
     * Initialize the GStreamer video encoder pipeline.
     * @param[in] eglStream The EGLStream to consume frames from.
     * @param[in] resolution The resolution of the video.
     * @param[in] framerate The framerate of the video (in frames per second).
     */
    bool initialize(EGLStreamKHR eglStream, Argus::Size2D<uint32_t> resolution, int32_t framerate)
    {
        // Only H265 supports resolutions > 4k.
        const uint32_t WIDTH_4K = 3840;
        if (resolution.width() > WIDTH_4K && m_options.useH264())
        {
            ORIGINATE_ERROR("Resolution > 4k requires encoder to be H265\n");
        }

        // If we're outputting to a remote destination, allocate/start the FileTransferThread.
        if (m_options.isRemoteDestination())
        {
            m_fileTransferThread = new FileTransferThread(m_options);
            if (!m_fileTransferThread)
                ORIGINATE_ERROR("Failed to create FileTransferThread");
            PROPAGATE_ERROR(m_fileTransferThread->initialize());
        }

        // Initialize GStreamer.
        gst_init(NULL, NULL);

        // Create pipeline.
        m_pipeline = gst_pipeline_new("video_pipeline");
        if (!m_pipeline)
            ORIGINATE_ERROR("Failed to create video pipeline");

        // Create EGLStream video source.
        GstElement *videoSource = gst_element_factory_make("nveglstreamsrc", NULL);
        if (!videoSource)
            ORIGINATE_ERROR("Failed to create EGLStream video source");
        if (!gst_bin_add(GST_BIN(m_pipeline), videoSource))
        {
            gst_object_unref(videoSource);
            ORIGINATE_ERROR("Failed to add video source to pipeline");
        }
        g_object_set(G_OBJECT(videoSource), "display", g_display.get(), NULL);
        g_object_set(G_OBJECT(videoSource), "eglstream", eglStream, NULL);

        // Create queue.
        GstElement *queue = gst_element_factory_make("queue", NULL);
        if (!queue)
            ORIGINATE_ERROR("Failed to create queue");
        if (!gst_bin_add(GST_BIN(m_pipeline), queue))
        {
            gst_object_unref(queue);
            ORIGINATE_ERROR("Failed to add queue to pipeline");
        }

        // Create encoder.
        const char* encoder = m_options.useH264() ? "omxh264enc" : "omxh265enc";
        m_videoEncoder = gst_element_factory_make(encoder, NULL);
        if (!m_videoEncoder)
            ORIGINATE_ERROR("Failed to create video encoder");
        if (!gst_bin_add(GST_BIN(m_pipeline), m_videoEncoder))
        {
            gst_object_unref(m_videoEncoder);
            ORIGINATE_ERROR("Failed to add video encoder to pipeline");
        }
        g_object_set(G_OBJECT(m_videoEncoder), "bitrate", m_options.bitrate(), NULL);

        // If the video stream is going to be divided into parts then it will be output as a raw
        // video stream via the writeFileParts() callback, otherwise it is muxed to a container
        // format for file output (i.e. MKV or MP4).
        GstElement *encoderQueue = NULL;
        GstElement *fakeSink = NULL;
        GstElement *fileSink = NULL;
        GstElement *videoMuxer = NULL;
        if (m_options.partSize() > 0)
        {
            // Create encoder queue.
            encoderQueue = gst_element_factory_make("queue", NULL);
            if (!encoderQueue)
                ORIGINATE_ERROR("Failed to create encoder queue");
            if (!gst_bin_add(GST_BIN(m_pipeline), encoderQueue))
            {
                gst_object_unref(encoderQueue);
                ORIGINATE_ERROR("Failed to add encoder queue to pipeline");
            }

            // Create fake sink to connect the file writing thread as the output destination.
            fakeSink = gst_element_factory_make("fakesink", NULL);
            if (!fakeSink)
                ORIGINATE_ERROR("Failed to create fake sink");
            if (!gst_bin_add(GST_BIN(m_pipeline), fakeSink))
            {
                gst_object_unref(fakeSink);
                ORIGINATE_ERROR("Failed to add fake sink to pipeline");
            }
        }
        else
        {
            // Create muxer.
            const char* muxer = (m_options.useH264() ? "qtmux" : "matroskamux");
            const char* suffix = (m_options.useH264() ? ".mp4" : ".mkv");
            videoMuxer = gst_element_factory_make(muxer, NULL);
            if (!videoMuxer)
                ORIGINATE_ERROR("Failed to create video muxer");
            if (!gst_bin_add(GST_BIN(m_pipeline), videoMuxer))
            {
                gst_object_unref(videoMuxer);
                ORIGINATE_ERROR("Failed to add video muxer to pipeline");
            }

            // Create file sink as the final destination for the output file.
            fileSink = gst_element_factory_make("filesink", NULL);
            if (!fileSink)
                ORIGINATE_ERROR("Failed to create file sink");
            if (!gst_bin_add(GST_BIN(m_pipeline), fileSink))
            {
                gst_object_unref(fileSink);
                ORIGINATE_ERROR("Failed to add file sink to pipeline");
            }
            m_currentPartName = m_options.path() + m_options.filename() + suffix;
            printf("Writing output to %s\n", m_currentPartName.c_str());
            g_object_set(G_OBJECT(fileSink), "location", m_currentPartName.c_str(), NULL);
        }

        // Create caps filter to describe EGLStream image format.
        GstCaps *caps = gst_caps_new_simple("video/x-raw",
                                            "format", G_TYPE_STRING, "NV12",
                                            "width", G_TYPE_INT, resolution.width(),
                                            "height", G_TYPE_INT, resolution.height(),
                                            "framerate", GST_TYPE_FRACTION, framerate, 1,
                                            NULL);
        if (!caps)
            ORIGINATE_ERROR("Failed to create caps");
        GstCapsFeatures *features = gst_caps_features_new("memory:NVMM", NULL);
        if (!features)
        {
            gst_caps_unref(caps);
            ORIGINATE_ERROR("Failed to create caps feature");
        }
        gst_caps_set_features(caps, 0, features);

        // Link EGLStream source to queue via caps filter.
        if (!gst_element_link_filtered(videoSource, queue, caps))
        {
            gst_caps_unref(caps);
            ORIGINATE_ERROR("Failed to link EGLStream source to queue");
        }
        gst_caps_unref(caps);

        // Link queue to encoder
        if (!gst_element_link(queue, m_videoEncoder))
            ORIGINATE_ERROR("Failed to link queue to encoder");

        if (m_options.partSize() > 0)
        {
            printf("Writing parts with a maximum size of %lu bytes.\n", m_options.partSize());

            // Link EGLStream encoder queue to h264 encoder queue via new encoder caps filter.
            const char* format = m_options.useH264() ? "video/x-h264" : "video/x-h265";
            GstCaps *encoderCaps = gst_caps_new_simple(
                format, "stream-format", G_TYPE_STRING, "byte-stream", NULL);
            if (!gst_element_link_filtered(m_videoEncoder, encoderQueue, encoderCaps))
            {
                gst_caps_unref(encoderCaps);
                ORIGINATE_ERROR("Failed to link m_videoEncoder & encoder caps source to queue");
            }
            gst_caps_unref(encoderCaps);

            // Link encoder queue to fake sink.
            if (!gst_element_link(encoderQueue, fakeSink))
                ORIGINATE_ERROR("Failed to link encoder queue to fake sink");

            // Set encoder buffer probe to connect the writeFileParts callback.
            GstPad *src_pad = gst_element_get_static_pad(m_videoEncoder, "src");
            gst_pad_add_probe(src_pad, GST_PAD_PROBE_TYPE_BUFFER,
                              (GstPadProbeCallback)writeFilePartsStatic, this, NULL);
            gst_object_unref(src_pad);
        }
        else
        {
            // Link encoder to muxer pad.
            if (!gst_element_link_pads(m_videoEncoder, "src", videoMuxer, "video_%u"))
                ORIGINATE_ERROR("Failed to link encoder to muxer pad");

            // Link muxer to sink.
            if (!gst_element_link(videoMuxer, fileSink))
                ORIGINATE_ERROR("Failed to link muxer to sink");
        }

        return true;
    }

    /**
     * Shutdown the GStreamer pipeline.
     */
    void shutdown()
    {
        if (m_state == GST_STATE_PLAYING)
            stopRecording();

        if (m_pipeline)
            gst_object_unref(GST_OBJECT(m_pipeline));
        m_pipeline = NULL;

        if (m_fileTransferThread)
        {
            m_fileTransferThread->sendFile(m_currentPartName);
            m_fileTransferThread->shutdown();
            delete m_fileTransferThread;
            m_fileTransferThread = NULL;
        }
    }

    /**
     * Start recording video.
     */
    bool startRecording()
    {
        if (!m_pipeline || !m_videoEncoder)
            ORIGINATE_ERROR("Video encoder not initialized");

        if (m_state != GST_STATE_NULL)
            ORIGINATE_ERROR("Video encoder already recording");

        // Start the pipeline.
        if (gst_element_set_state(m_pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
            ORIGINATE_ERROR("Failed to start recording.");

        time(&m_startTime);
        m_lastPrintTime = 0;
        m_state = GST_STATE_PLAYING;
        return true;
    }

    /**
     * Stop recording video.
     */
    bool stopRecording()
    {
        if (!m_pipeline || !m_videoEncoder)
            ORIGINATE_ERROR("Video encoder not initialized");

        if (m_state != GST_STATE_PLAYING)
            ORIGINATE_ERROR("Video encoder not recording");

        // Send the end-of-stream event.
        GstPad *pad = gst_element_get_static_pad(m_videoEncoder, "sink");
        if (!pad)
            ORIGINATE_ERROR("Failed to get 'sink' pad");
        bool result = gst_pad_send_event(pad, gst_event_new_eos());
        gst_object_unref(pad);
        if (!result)
            ORIGINATE_ERROR("Failed to send end of stream event to encoder");

        // Wait for the event to complete.
        GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(m_pipeline));
        if (!bus)
            ORIGINATE_ERROR("Failed to get bus");
        result = gst_bus_poll(bus, GST_MESSAGE_EOS, GST_CLOCK_TIME_NONE);
        gst_object_unref(bus);
        if (!result)
            ORIGINATE_ERROR("Failed to wait for the EOF event");

        // Stop the pipeline.
        if (gst_element_set_state(m_pipeline, GST_STATE_NULL) == GST_STATE_CHANGE_FAILURE)
            ORIGINATE_ERROR("Failed to stop recording.");

        m_state = GST_STATE_NULL;
        return true;
    }

    static GstPadProbeReturn writeFilePartsStatic(
            GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
    {
        GstVideoEncoder *thiz = static_cast<GstVideoEncoder*>(user_data);
        return thiz->writeFileParts(pad, info);
    }

    GstPadProbeReturn writeFileParts(GstPad *pad, GstPadProbeInfo *info)
    {
        GstMapInfo map;
        GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);
        buffer = gst_buffer_make_writable(buffer);
        gst_buffer_map(buffer, &map, GST_MAP_WRITE);

        // If the write will exceed the file size maximum, start a new part.
        int64_t writeSize = gst_buffer_get_size(buffer);
        if (m_currentPartFile)
        {
            int64_t partBytesWritten = ftell(m_currentPartFile);
            if ((uint64_t)(partBytesWritten + writeSize) > m_options.partSize())
            {
                fclose(m_currentPartFile);
                m_currentPartFile = NULL;

                printf("Finished part %s (%ld bytes)\n",
                       m_currentPartName.c_str(), partBytesWritten);

                // If we have a file transfer thread, send the file to the remote destination.
                if (m_fileTransferThread)
                {
                    m_fileTransferThread->sendFile(m_currentPartName);
                }
            }
        }

        // Create new file part if needed.
        if (!m_currentPartFile)
        {
            std::string partString = std::to_string(m_currentPartNumber++);
            m_currentPartName = m_options.path() + m_options.filename() +
                                std::string(6 - partString.size(), '0') + partString +
                                (m_options.useH264() ? ".h264" : ".h265");
            m_currentPartFile = fopen(m_currentPartName.c_str(), "w");
            if (!m_currentPartFile)
            {
                printf("File open failed\n");
                return (GstPadProbeReturn)(0);
            }
        }

        // Write to file.
        if (fwrite(map.data, writeSize, 1, m_currentPartFile) != 1)
        {
            fclose(m_currentPartFile);
            m_currentPartFile = NULL;
            printf("Write to file %s failed!\n", m_currentPartName.c_str());
            return (GstPadProbeReturn)(0);
        }
        else
        {
            m_totalBytesWritten += writeSize;

            time_t now = time(0);
            if (difftime(now, m_lastPrintTime) >= 1)
            {
                uint64_t runtime = difftime(now, m_startTime);
                const uint32_t days = runtime / (60*60*24);
                runtime -= days * (60*60*24);
                const uint32_t hours = runtime / (60*60);
                runtime -= hours * (60*60);
                const uint32_t minutes = runtime / 60;
                runtime -= minutes * 60;
                const uint32_t seconds = runtime;
                printf("Recording: %u days, %02u:%02u:%02u, %ld MB written to %s, %lu MB total.\n",
                       days, hours, minutes, seconds,
                       (ftell(m_currentPartFile) / 1024 / 1024),
                       m_currentPartName.c_str(),
                       (m_totalBytesWritten / 1024 / 1024));
                m_lastPrintTime = now;
            }
        }

        gst_buffer_unmap(buffer, &map);
        GST_PAD_PROBE_INFO_DATA(info) = buffer;

        return GST_PAD_PROBE_OK;
    }

protected:
    const GstVideoEncodeSampleOptions& m_options;

    GstState m_state;
    GstElement *m_pipeline;
    GstElement *m_videoEncoder;

    FileTransferThread *m_fileTransferThread;
    uint32_t m_currentPartNumber;
    std::string m_currentPartName;
    FILE *m_currentPartFile;
    uint64_t m_totalBytesWritten;
    time_t m_startTime;
    time_t m_lastPrintTime;
};

static bool execute(const GstVideoEncodeSampleOptions& options)
{
    using namespace Argus;

    // Initialize the preview window and EGL display.
    Window &window = Window::getInstance();
    if (options.enablePreview())
    {
        window.setWindowRect(options.windowRect());
        PROPAGATE_ERROR(g_display.initialize(window.getEGLNativeDisplay()));
    }
    else
    {
        PROPAGATE_ERROR(g_display.initialize(EGL_DEFAULT_DISPLAY));
    }

    // Create CameraProvider.
    UniqueObj<CameraProvider> cameraProvider(CameraProvider::create());
    ICameraProvider *iCameraProvider = interface_cast<ICameraProvider>(cameraProvider);
    if (!iCameraProvider)
        ORIGINATE_ERROR("Failed to open CameraProvider");
    printf("Argus Version: %s\n", iCameraProvider->getVersion().c_str());

    // Get the selected camera device and sensor mode.
    CameraDevice* cameraDevice = ArgusHelpers::getCameraDevice(
            cameraProvider.get(), options.cameraDeviceIndex());
    if (!cameraDevice)
        ORIGINATE_ERROR("Selected camera device is not available");
    SensorMode* sensorMode = options.useWdr()
                           ? ArgusHelpers::getWdrSensorMode(cameraDevice)
                           : ArgusHelpers::getSensorMode(cameraDevice, options.sensorModeIndex());
    ISensorMode *iSensorMode = interface_cast<ISensorMode>(sensorMode);
    if (!iSensorMode)
        ORIGINATE_ERROR("Selected sensor mode not available");

    // Create CaptureSession.
    UniqueObj<CaptureSession> captureSession(iCameraProvider->createCaptureSession(cameraDevice));
    ICaptureSession *iSession = interface_cast<ICaptureSession>(captureSession);
    if (!iSession)
        ORIGINATE_ERROR("Failed to create CaptureSession");

    // Set common output stream settings.
    UniqueObj<OutputStreamSettings> streamSettings(
            iSession->createOutputStreamSettings(STREAM_TYPE_EGL));
    IEGLOutputStreamSettings *iEGLStreamSettings =
        interface_cast<IEGLOutputStreamSettings>(streamSettings);
    if (!iEGLStreamSettings)
        ORIGINATE_ERROR("Failed to create OutputStreamSettings");
    if (options.useP016())
        iEGLStreamSettings->setPixelFormat(PIXEL_FMT_P016);
    else
        iEGLStreamSettings->setPixelFormat(PIXEL_FMT_YCbCr_420_888);
    iEGLStreamSettings->setEGLDisplay(g_display.get());

    // Create video encoder stream using the sensor mode resolution.
    iEGLStreamSettings->setResolution(iSensorMode->getResolution());
    UniqueObj<OutputStream> videoStream(iSession->createOutputStream(streamSettings.get()));
    IEGLOutputStream *iVideoStream = interface_cast<IEGLOutputStream>(videoStream);
    if (!iVideoStream)
        ORIGINATE_ERROR("Failed to create video stream");

    // Create preview stream.
    UniqueObj<OutputStream> previewStream;
    IEGLOutputStream *iPreviewStream = NULL;
    if (options.enablePreview())
    {
        iEGLStreamSettings->setResolution(Size2D<uint32_t>(options.windowRect().width(),
                                                           options.windowRect().height()));
        previewStream.reset(iSession->createOutputStream(streamSettings.get()));
        iPreviewStream = interface_cast<IEGLOutputStream>(previewStream);
        if (!iPreviewStream)
            ORIGINATE_ERROR("Failed to create preview stream");
    }

    // Create capture Request and enable the streams in the Request.
    UniqueObj<Request> request(iSession->createRequest(CAPTURE_INTENT_VIDEO_RECORD));
    IRequest *iRequest = interface_cast<IRequest>(request);
    if (!iRequest)
        ORIGINATE_ERROR("Failed to create Request");
    if (iRequest->enableOutputStream(videoStream.get()) != STATUS_OK)
        ORIGINATE_ERROR("Failed to enable video stream in Request");
    if (options.enablePreview())
    {
        if (iRequest->enableOutputStream(previewStream.get()) != STATUS_OK)
            ORIGINATE_ERROR("Failed to enable preview stream in Request");
    }

    // Determine the framerate (0 indicates max framerate for sensor mode).
    uint32_t maxFramerate = (1000000000 / (iSensorMode->getFrameDurationRange().min() - 1));
    uint32_t minFramerate = (1000000000 / iSensorMode->getFrameDurationRange().max()) + 1;
    uint32_t framerate = (options.framerate() == 0) ? maxFramerate : options.framerate();
    if (framerate < minFramerate || framerate > maxFramerate)
    {
        ORIGINATE_ERROR("Requested framerate (%d) exceeds limits [%d, %d].\n",
                        framerate, minFramerate, maxFramerate);
    }

    // Set the sensor mode in the request.
    ISourceSettings *iSourceSettings = interface_cast<ISourceSettings>(request);
    if (!iSourceSettings)
        ORIGINATE_ERROR("Failed to get source settings request interface");
    iSourceSettings->setSensorMode(sensorMode);
    iSourceSettings->setFrameDurationRange(1000000000 / framerate);

    // Set the denoise settings.
    IDenoiseSettings *denoiseSettings = interface_cast<IDenoiseSettings>(request);
    if (!denoiseSettings)
        ORIGINATE_ERROR("Failed to get DenoiseSettings interface");
    switch (options.nrMode())
    {
        case GstVideoEncodeSampleOptions::NOISE_MODE_ARG_OFF:
            denoiseSettings->setDenoiseMode(DENOISE_MODE_OFF);
            break;

        default:
        case GstVideoEncodeSampleOptions::NOISE_MODE_ARG_FAST:
            denoiseSettings->setDenoiseMode(DENOISE_MODE_FAST);
            break;

        case GstVideoEncodeSampleOptions::NOISE_MODE_ARG_HIGH_QUALITY:
            denoiseSettings->setDenoiseMode(DENOISE_MODE_HIGH_QUALITY);
            break;
    }
    denoiseSettings->setDenoiseStrength(1.0f);

    // Initialize the GStreamer video encoder consumer.
    GstVideoEncoder gstVideoEncoder(options);
    if (!gstVideoEncoder.initialize(iVideoStream->getEGLStream(),
                                    iSensorMode->getResolution(),
                                    framerate))
    {
        ORIGINATE_ERROR("Failed to initialize GstVideoEncoder EGLStream consumer");
    }
    if (!gstVideoEncoder.startRecording())
    {
        ORIGINATE_ERROR("Failed to start video recording");
    }

    // Initialize the preview consumer.
    PreviewConsumerThread previewConsumer(iPreviewStream ? iPreviewStream->getEGLDisplay() : NULL,
                                          iPreviewStream ? iPreviewStream->getEGLStream() : NULL);
    if (options.enablePreview())
    {
        PROPAGATE_ERROR(previewConsumer.initialize());
        PROPAGATE_ERROR(previewConsumer.waitRunning());
    }

    // If the capture time is 0, register the signal handler to terminate the infinite captures.
    if (options.captureTime() == 0 && signal(SIGINT, ArgusSamples::ctrl_c_sig_handler) == SIG_ERR)
        ORIGINATE_ERROR("Failed to register signal handler");

    // Start repeat capture requests.
    if (iSession->repeat(request.get()) != STATUS_OK)
        ORIGINATE_ERROR("Failed to start repeat capture requests");

    // Wait for the requested amount of time while the repeat captures continue.
    // If the capture time is 0, we capture infinitely until a shutdown signal is received.
    uint32_t sleepTime = std::max(1u, options.captureTime());
    do
    {
        if (options.enablePreview())
        {
            PROPAGATE_ERROR(Window::getInstance().pollingSleep(sleepTime));
        }
        else
        {
            sleep(sleepTime);
        }
    } while (options.captureTime() == 0 && !shutdownSignalReceived);

    // Stop the repeating captures.
    iSession->stopRepeat();

    // Wait until all frames have completed before stopping recording.
    /// @todo: Not doing this may cause a deadlock.
    iSession->waitForIdle();

    // Stop video recording.
    if (!gstVideoEncoder.stopRecording())
        ORIGINATE_ERROR("Failed to stop video recording");
    gstVideoEncoder.shutdown();
    videoStream.reset();

    // Stop preview.
    if (options.enablePreview())
    {
        previewStream.reset();
        PROPAGATE_ERROR(previewConsumer.shutdown());
    }

    // Shut down Argus.
    cameraProvider.reset();

    // Shut down the window (destroys window's EGLSurface).
    window.shutdown();

    // Cleanup the EGL display
    PROPAGATE_ERROR(g_display.cleanup());
    return true;
}

}; // namespace ArgusSamples

int main(int argc, char** argv)
{
    ArgusSamples::GstVideoEncodeSampleOptions options(basename(argv[0]));
    if (!options.parse(argc, argv))
        return EXIT_FAILURE;
    if (options.requestedExit())
        return EXIT_SUCCESS;

    if (!ArgusSamples::execute(options))
        return EXIT_FAILURE;

    printf("Done.\n");

    return EXIT_SUCCESS;
}
