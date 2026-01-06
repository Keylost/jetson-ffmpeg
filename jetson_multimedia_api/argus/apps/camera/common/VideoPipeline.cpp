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

#include <stdio.h>

#include <string>

#include "Error.h"
#include "VideoPipeline.h"
#include "Composer.h"
#include "Util.h"

namespace ArgusSamples
{

VideoPipeline::VideoPipeline()
#ifdef GST_SUPPORTED
    : m_state(GST_STATE_NULL)
    , m_pipeline(NULL)
#endif
{
}

VideoPipeline::~VideoPipeline()
{
    destroy();
}

///! give the video eoncoder a name so we can find it at stop()
static const char *s_videoEncoderName = "video encoder";

/**
 * RAII helper class for calling gst_object_unref on exit from a block or function.
 */
template <typename T> class GstUnrefer
{
public:
    explicit GstUnrefer(T * p)
        : m_p(p)
    {
    }
    GstUnrefer()
        : m_p(NULL)
    {
    }
    ~GstUnrefer()
    {
        release();
    }

    /// Cancel the unref.
    void cancel()
    {
        m_p = NULL;
    }

    /// Unref the object now.
    void release()
    {
        if (m_p)
            gst_object_unref(m_p);
        m_p = NULL;
    }

    /// Set the object to be unrefed.
    void set(T* p)
    {
        release();
        m_p = p;
    }

    /// Get the object.
    T * get() const
    {
        return m_p;
    }

private:
    T *m_p;

    /// Not implemented -- use default constructor
    GstUnrefer(GstUnrefer& other);
    /// Not implemented
    GstUnrefer& operator=(GstUnrefer&);
};

bool VideoPipeline::setupForRecording(EGLStreamKHR videoStream, uint32_t width, uint32_t height,
    float frameRate, const char *fileName, VideoFormat videoFormat,
    VideoFileType videoFileType, uint32_t bitRate)
{
#ifdef GST_SUPPORTED
    // set the filename
    std::string videoFileName(fileName);
    if (videoFileName != "/dev/null")
    {
        videoFileName += ".";
        videoFileName += getFileExtension(videoFileType);
        PROPAGATE_ERROR(validateOutputPath(videoFileName.c_str()));
    }

    // Init gstreamer
    gst_init(NULL, NULL);

    // create the pipeline
    m_pipeline = gst_pipeline_new("video_pipeline");
    if (!m_pipeline)
        ORIGINATE_ERROR("Failed to create video pipeline");

    // Create the capture source element
    GstElement *videoSource = gst_element_factory_make("nveglstreamsrc", NULL);
    if (!videoSource)
        ORIGINATE_ERROR("Failed to create capture source element");
    GstUnrefer<GstElement> unrefer(videoSource);
    if (!gst_bin_add(GST_BIN(m_pipeline), videoSource))
        ORIGINATE_ERROR("Failed to add video source to pipeline");
    unrefer.cancel();

    g_object_set(G_OBJECT(videoSource), "display", Composer::getInstance().getEGLDisplay(), NULL);
    g_object_set(G_OBJECT(videoSource), "eglstream", videoStream, NULL);

    // Create queue
    GstElement *queue = gst_element_factory_make("queue", NULL);
    if (!queue)
        ORIGINATE_ERROR("Failed to create queue");
    unrefer.set(queue);
    if (!gst_bin_add(GST_BIN(m_pipeline), queue))
        ORIGINATE_ERROR("Failed to add queue to pipeline");
    unrefer.cancel();

    // create the encoder
    GstElement *videoEncoder = NULL;
    switch (videoFormat)
    {
    case VIDEO_FORMAT_H264:
        videoEncoder = gst_element_factory_make("omxh264enc", s_videoEncoderName);
        break;
    case VIDEO_FORMAT_H265:
        videoEncoder = gst_element_factory_make("omxh265enc", s_videoEncoderName);
        break;
    case VIDEO_FORMAT_VP8:
        videoEncoder = gst_element_factory_make("omxvp8enc", s_videoEncoderName);
        break;
    case VIDEO_FORMAT_VP9:
        videoEncoder = gst_element_factory_make("omxvp9enc", s_videoEncoderName);
        break;
    default:
        ORIGINATE_ERROR("Unhandled video format");
    }
    if (!videoEncoder)
        ORIGINATE_ERROR("Failed to create video encoder");
    unrefer.set(videoEncoder);
    if (!gst_bin_add(GST_BIN(m_pipeline), videoEncoder))
        ORIGINATE_ERROR("Failed to add video encoder to pipeline");
    unrefer.cancel();

    // if no bitrate is given select from reasonable presets
    if (bitRate == 0)
    {
        if (height < 720)
            bitRate = VIDEO_BITRATE_4M;
        else if (height < 1080)
            bitRate = VIDEO_BITRATE_8M;
        else if (height <= 2160)
            bitRate = VIDEO_BITRATE_14M;
        else
            bitRate = VIDEO_BITRATE_20M;
    }

    g_object_set(G_OBJECT(videoEncoder), "bitrate", bitRate, NULL);

    /*
     * Currently, of all the supported videoEncoders above: H264, H265, VP8 and VP9, Only H265
     * supports resolution > 4k.
     */
    const uint32_t WIDTH_4K = 3840;
    if (width > WIDTH_4K && videoFormat != VIDEO_FORMAT_H265)
    {
        ORIGINATE_ERROR("\n Resolution > 4k requires videoformat H265 \n");
    }
    // set video encoding profile for h.264 to high to get optmized video quality
    if (videoFormat == VIDEO_FORMAT_H264)
    {
        g_object_set(G_OBJECT(videoEncoder), "profile", VIDEO_AVC_PROFILE_HIGH, NULL);
    }

    // create the muxer
    if (videoFormat == VIDEO_FORMAT_VP9)
    {
        printf("\nThe VP9 video format is not supported on Jetson-tx1.\n");
    }

    if (((videoFormat == VIDEO_FORMAT_H265) || (videoFormat == VIDEO_FORMAT_VP9)) &&
        (videoFileType != VIDEO_FILE_TYPE_MKV))
    {
        printf("\nThe video format H265/VP9 is only supported with MKV in current GST version. "
               "Selecting MKV as container.\n");
        videoFileType = VIDEO_FILE_TYPE_MKV;
    }

    GstElement *videoMuxer = NULL;
    switch (videoFileType)
    {
    case VIDEO_FILE_TYPE_MP4:
        videoMuxer = gst_element_factory_make("qtmux", NULL);
        break;
    case VIDEO_FILE_TYPE_3GP:
        videoMuxer = gst_element_factory_make("3gppmux", NULL);
        break;
    case VIDEO_FILE_TYPE_AVI:
        videoMuxer = gst_element_factory_make("avimux", NULL);
        break;
    case VIDEO_FILE_TYPE_MKV:
        videoMuxer = gst_element_factory_make("matroskamux", NULL);
        break;
    case VIDEO_FILE_TYPE_H265:
        videoMuxer = gst_element_factory_make("identity", NULL);
        break;
    default:
        ORIGINATE_ERROR("Unhandled video file type");
    }
    if (!videoMuxer)
        ORIGINATE_ERROR("Failed to create video muxer");
    unrefer.set(videoMuxer);
    if (!gst_bin_add(GST_BIN(m_pipeline), videoMuxer))
        ORIGINATE_ERROR("Failed to add video muxer to pipeline");
    unrefer.cancel();

    // create the sink
    GstElement *videoSink = gst_element_factory_make("filesink", NULL);
    if (!videoSink)
        ORIGINATE_ERROR("Failed to create video sink");
    unrefer.set(videoSink);
    if (!gst_bin_add(GST_BIN(m_pipeline), videoSink))
        ORIGINATE_ERROR("Failed to add video sink to pipeline");
    unrefer.cancel();

    g_object_set(G_OBJECT(videoSink), "location", videoFileName.c_str(), NULL);

    // @todo 'Floating point exception' and error 'Framerate set to : 0 at
    // NvxVideoEncoderSetParameter' when no setting the framerate. '0' should be VFR, use 30
    // instead
    if (frameRate == 0.0f)
        frameRate = 30.0f;

    // create a caps filter
    GstCaps *caps = gst_caps_new_simple("video/x-raw",
        "format", G_TYPE_STRING, "NV12",
        "width", G_TYPE_INT, width,
        "height", G_TYPE_INT, height,
        "framerate", GST_TYPE_FRACTION, static_cast<gint>(frameRate * 100.f), 100,
        NULL);
    if (!caps)
        ORIGINATE_ERROR("Failed to create caps");

    GstCapsFeatures *feature = gst_caps_features_new("memory:NVMM", NULL);
    if (!feature)
    {
        gst_caps_unref(caps);
        ORIGINATE_ERROR("Failed to create caps feature");
    }

    gst_caps_set_features(caps, 0, feature);

    // link the source to the queue via the capture filter
    if (!gst_element_link_filtered(videoSource, queue, caps))
    {
        gst_caps_unref(caps);
        ORIGINATE_ERROR("Failed to link source to queue");
    }
    gst_caps_unref(caps);

    // link the queue to the encoder
    if (!gst_element_link(queue, videoEncoder))
        ORIGINATE_ERROR("Failed to link queue to encoder");

    // link the encoder pad to the muxer
    if (videoFileType == VIDEO_FILE_TYPE_H265)
    {
        // H265 has a identity muxer, need to link directly
        if (!gst_element_link(videoEncoder, videoMuxer))
            ORIGINATE_ERROR("Failed to link encoder to muxer");
    }
    else
    {
        if (!gst_element_link_pads(videoEncoder, "src", videoMuxer, "video_%u"))
            ORIGINATE_ERROR("Failed to link encoder to muxer pad");
    }

    // link the muxer to the sink
    if (!gst_element_link(videoMuxer, videoSink))
        ORIGINATE_ERROR("Failed to link muxer to sink");

    return true;
#else // GST_SUPPORTED
    ORIGINATE_ERROR("Not supported");
#endif // GST_SUPPORTED
}

#ifdef GST_SUPPORTED
/**
 * Modify object flag values by name.
 */
static bool objectModifyFlags(GObject *obj, const char *flagName, const char *valueName, bool set)
{
    guint count;
    GParamSpec **spec = g_object_class_list_properties(G_OBJECT_GET_CLASS(obj), &count);

    for (guint index = 0; index < count; ++index)
    {
        GParamSpec *param = spec[index];
        if (strcmp(param->name, flagName) == 0)
        {
            if (!G_IS_PARAM_SPEC_FLAGS(param))
                ORIGINATE_ERROR("Param '%s' is not a flag", flagName);

            GParamSpecFlags *pflags = G_PARAM_SPEC_FLAGS(param);
            GFlagsValue *value = g_flags_get_value_by_nick(pflags->flags_class, valueName);
            if (!value)
                ORIGINATE_ERROR("Value '%s' of flag '%s' not found", valueName, flagName);

            gint flags;
            g_object_get(obj, flagName, &flags, NULL);
            if (set)
                flags |= value->value;
            else
                flags &= ~value->value;
            g_object_set(obj, flagName, flags, NULL);

            return true;
        }
    }

    ORIGINATE_ERROR("Param '%s' not found", flagName);
}
#endif // GST_SUPPORTED

bool VideoPipeline::setupForPlayback(EGLStreamKHR *videoStream, const char *fileName)
{
#ifdef GST_SUPPORTED
    // Init gstreamer
    gst_init(NULL, NULL);

    // Create the source element
    m_pipeline = gst_element_factory_make("playbin", "play");
    if (!m_pipeline)
        ORIGINATE_ERROR("Failed to create playback pipeline");

    // set the uri
    char *uri = gst_filename_to_uri(fileName, NULL);
    g_object_set(G_OBJECT(m_pipeline), "uri", uri, NULL);
    g_free(uri);
    uri = NULL;

    PROPAGATE_ERROR(objectModifyFlags(G_OBJECT(m_pipeline), "flags", "text", false));
    PROPAGATE_ERROR(objectModifyFlags(G_OBJECT(m_pipeline), "flags", "native-video", true));

    // create the audio sink
    GstElement *audioSink = gst_element_factory_make("autoaudiosink", "audio_sink");
    if (!audioSink)
        ORIGINATE_ERROR("Failed to create audio sink");

    // set the audio sink of the pipeline
    g_object_set(G_OBJECT(m_pipeline), "audio-sink", audioSink, NULL);

    // Create the sink bin, this will hold the video converter and the video sink
    GstElement *videoSinkBin = gst_bin_new("video_sink_bin");
    if (!videoSinkBin)
        ORIGINATE_ERROR("Failed to create video sink bin");

    // set the video sink of the pipeline
    g_object_set(G_OBJECT(m_pipeline), "video-sink", videoSinkBin, NULL);

    // Create the video converted
    GstElement *videoConvert = gst_element_factory_make("nvvidconv", "video converter");
    if (!videoConvert)
        ORIGINATE_ERROR("Failed to create video converter");
    GstUnrefer<GstElement> unrefer(videoConvert);
    if (!gst_bin_add(GST_BIN(videoSinkBin), videoConvert))
        ORIGINATE_ERROR("Failed to add video convert to video sink bin");
    unrefer.cancel();

    // Create the video sink
    GstElement *videoSink = gst_element_factory_make("nvvideosink", "video sink");
    if (!videoSink)
        ORIGINATE_ERROR("Failed to create video sink");
    unrefer.set(videoSink);
    if (!gst_bin_add(GST_BIN(videoSinkBin), videoSink))
        ORIGINATE_ERROR("Failed to add video sink to video sink bin");
    unrefer.cancel();

    // configure video sink
    g_object_set(G_OBJECT(videoSink), "display", Composer::getInstance().getEGLDisplay(), NULL);
    // get the EGL stream
    *videoStream = EGL_NO_STREAM_KHR;
    g_object_get(G_OBJECT(videoSink), "stream", videoStream, NULL);
    if (*videoStream == EGL_NO_STREAM_KHR)
        ORIGINATE_ERROR("Failed to get EGL stream from video sink");

    if (!gst_element_link(videoConvert, videoSink))
        ORIGINATE_ERROR("Failed to link video convert to video sink");

    // create a ghost pad so that the pipeline can connect to the bin as a sink
    GstPad *pad = gst_element_get_static_pad(videoConvert, "sink");
    if (!pad)
        ORIGINATE_ERROR("Failed to get sink pad of video convert");
    GstUnrefer<GstPad> padUnrefer(pad);
    GstPad *ghostPad = gst_ghost_pad_new("sink", pad);
    if (!ghostPad)
        ORIGINATE_ERROR("Failed to create the ghost pad");
    GstUnrefer<GstPad> ghostPadUnrefer(ghostPad);
    if (!gst_pad_set_active(ghostPad, TRUE))
        ORIGINATE_ERROR("Failed to set pad active");
    if (!gst_element_add_pad(videoSinkBin, ghostPad))
        ORIGINATE_ERROR("Failed to add pad");
    ghostPadUnrefer.cancel();
    padUnrefer.release();

    return true;
#else // GST_SUPPORTED
    ORIGINATE_ERROR("Not supported");
#endif // GST_SUPPORTED
}

bool VideoPipeline::start()
{
#ifdef GST_SUPPORTED
    if (!m_pipeline)
        ORIGINATE_ERROR("Video pipeline is not set up");

    if (m_state != GST_STATE_PLAYING)
    {
        // set to playing state
        if (gst_element_set_state(m_pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
            ORIGINATE_ERROR("Failed to set playing state");

        m_state = GST_STATE_PLAYING;

        /* Dump Capture - Playing Pipeline into the dot file
         * Set environment variable "export GST_DEBUG_DUMP_DOT_DIR=/tmp"
         * Run argus_camera and 0.00.00.*-argus_camera.dot
         * file will be generated.
         * Run "dot -Tpng 0.00.00.*-argus_camera.dot > image.png"
         * image.png will display the running capture pipeline.
         * */
        GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(m_pipeline),
            GST_DEBUG_GRAPH_SHOW_ALL, "argus_camera");
    }

    return true;
#else // GST_SUPPORTED
    ORIGINATE_ERROR("Not supported");
#endif // GST_SUPPORTED
}

bool VideoPipeline::pause()
{
#ifdef GST_SUPPORTED
    if (!m_pipeline)
        ORIGINATE_ERROR("Video pipeline is not set up");

    if (m_state != GST_STATE_PAUSED)
    {
        if (gst_element_set_state(m_pipeline, GST_STATE_PAUSED) == GST_STATE_CHANGE_FAILURE)
            ORIGINATE_ERROR("Failed to set pause state");
        m_state = GST_STATE_PAUSED;
    }

    return true;
#else // GST_SUPPORTED
    ORIGINATE_ERROR("Not supported");
#endif // GST_SUPPORTED
}


bool VideoPipeline::toggle()
{
#ifdef GST_SUPPORTED
    if (!m_pipeline)
        ORIGINATE_ERROR("Video pipeline is not set up");

    GstState newState = GST_STATE_NULL;
    if (m_state == GST_STATE_PLAYING)
        newState = GST_STATE_PAUSED;
    else if (m_state == GST_STATE_PAUSED)
        newState = GST_STATE_PLAYING;
    else
        ORIGINATE_ERROR("Invalid state");

    if (gst_element_set_state(m_pipeline, newState) == GST_STATE_CHANGE_FAILURE)
        ORIGINATE_ERROR("Failed to set pause state");

    m_state = newState;

    return true;
#else // GST_SUPPORTED
    ORIGINATE_ERROR("Not supported");
#endif // GST_SUPPORTED
}

bool VideoPipeline::rewind()
{
#ifdef GST_SUPPORTED
    if (!m_pipeline)
        ORIGINATE_ERROR("Video pipeline is not set up");

    if (!gst_element_seek_simple(m_pipeline, GST_FORMAT_TIME,
        static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT), 0))
    {
        ORIGINATE_ERROR("Failed to rewind");
    }

    return true;
#else // GST_SUPPORTED
    ORIGINATE_ERROR("Not supported");
#endif // GST_SUPPORTED
}

bool VideoPipeline::stop()
{
#ifdef GST_SUPPORTED
    if (!m_pipeline)
        ORIGINATE_ERROR("Video pipeline is not set up");

    if ((m_state == GST_STATE_PLAYING) || (m_state == GST_STATE_PAUSED))
    {
        // check if there is a video encoder
        GstElement *videoEncoder = gst_bin_get_by_name(GST_BIN(m_pipeline), s_videoEncoderName);
        if (videoEncoder)
        {
            // send the end of stream event
            GstPad *pad = gst_element_get_static_pad(videoEncoder, "sink");
            if (!pad)
                ORIGINATE_ERROR("Failed to get 'sink' pad");
            GstUnrefer<GstPad> padUnrefer(pad);
            if (!gst_pad_send_event(pad, gst_event_new_eos()))
                ORIGINATE_ERROR("Failed to send end of stream event encoder");
            padUnrefer.release();

            // wait for the event to go through
            GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(m_pipeline));
            if (!bus)
                ORIGINATE_ERROR("Failed to get bus");
            GstUnrefer<GstBus> busUnrefer(bus);
            if (!gst_bus_poll(bus, GST_MESSAGE_EOS, GST_CLOCK_TIME_NONE))
                ORIGINATE_ERROR("Failed to wait for the eof event");
            busUnrefer.release();
        }

        // stop the pipeline
        if (gst_element_set_state(m_pipeline, GST_STATE_NULL) != GST_STATE_CHANGE_SUCCESS)
            ORIGINATE_ERROR("Failed to stop pipeline");

        m_state = GST_STATE_NULL;
    }

    return true;
#else // GST_SUPPORTED
    ORIGINATE_ERROR("Not supported");
#endif // GST_SUPPORTED
}

bool VideoPipeline::destroy()
{
#ifdef GST_SUPPORTED
    if (m_pipeline)
    {
        PROPAGATE_ERROR(stop());

        // delete pipeline
        gst_object_unref(GST_OBJECT(m_pipeline));

        m_pipeline = NULL;
    }

    return true;
#else // GST_SUPPORTED
    ORIGINATE_ERROR("Not supported");
#endif // GST_SUPPORTED
}

/*static*/ const char* VideoPipeline::getFileExtension(VideoFileType fileType)
{
    switch (fileType)
    {
    case VIDEO_FILE_TYPE_MP4:
        return "mp4";
    case VIDEO_FILE_TYPE_3GP:
        return "3gp";
    case VIDEO_FILE_TYPE_AVI:
        return "avi";
    case VIDEO_FILE_TYPE_MKV:
        return "mkv";
    case VIDEO_FILE_TYPE_H265:
        return "h265";
    default:
        break;
    }

    return "Unhandled video file type";
}

bool VideoPipeline::getAspectRatio(float *aspectRatio) const
{
    if (aspectRatio == NULL)
        ORIGINATE_ERROR("'aspectRatio' is NULL");
#ifdef GST_SUPPORTED
    if ((m_state != GST_STATE_PLAYING) && (m_state != GST_STATE_PAUSED))
        ORIGINATE_ERROR("Must be in paused or playing state.");

    GstState state = GST_STATE_NULL;
    while ((state != GST_STATE_PLAYING) && (state != GST_STATE_PAUSED))
    {
        if (gst_element_get_state(m_pipeline, &state, NULL, GST_CLOCK_TIME_NONE) ==
            GST_STATE_CHANGE_FAILURE)
        {
            ORIGINATE_ERROR("gst_element_get_state failed");
        }
    }

    // Retrieve the Caps at the entrance of the video sink
    GstElement *videoSink;
    g_object_get(m_pipeline, "video-sink", &videoSink, NULL);
    if (!videoSink)
        ORIGINATE_ERROR("Failed to get video-sink");
    GstUnrefer<GstElement> videoSinkUnrefer(videoSink);

    GstPad *videoSinkPad = gst_element_get_static_pad(videoSink, "sink");
    if (!videoSinkPad)
        ORIGINATE_ERROR("Failed to get video-sink pad");

    GstCaps *caps = gst_pad_get_current_caps(videoSinkPad);
    if (!caps)
        ORIGINATE_ERROR("Failed to get video-sink pad caps");

    *aspectRatio = 1.0f;

    GstStructure *structure = gst_caps_get_structure(caps, 0);
    if (!structure)
    {
        gst_caps_unref(caps);
        ORIGINATE_ERROR("Failed to get caps structure");
    }

    gint width, height;
    gint pixelAspectRatioNumerator, pixelAspectRatioDenominator;

    if (!gst_structure_get_int(structure, "width", &width) ||
        !gst_structure_get_int(structure, "height", &height) ||
        !gst_structure_get_fraction(structure, "pixel-aspect-ratio",
            &pixelAspectRatioNumerator, &pixelAspectRatioDenominator))
    {
        gst_caps_unref(caps);
        ORIGINATE_ERROR("Failed to get structure values");
    }

    *aspectRatio = (float)width / (float)height;
    *aspectRatio *= (float)pixelAspectRatioNumerator / (float)pixelAspectRatioDenominator;

    gst_caps_unref(caps);

    return true;
#else // GST_SUPPORTED
    ORIGINATE_ERROR("Not supported");
#endif // GST_SUPPORTED
}

bool VideoPipeline::isSupported()
{
#ifdef GST_SUPPORTED
    return true;
#else // GST_SUPPORTED
    return false;
#endif // GST_SUPPORTED
}

}; // namespace ArgusSamples
