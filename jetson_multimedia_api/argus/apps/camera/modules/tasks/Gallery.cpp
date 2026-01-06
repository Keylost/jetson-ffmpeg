/*
 * Copyright (c) 2016-2017, NVIDIA CORPORATION. All rights reserved.
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

/**
 * @file
 * Gallery implementation file.
 * The gallery task creates a thread handling scanning for items, loading of images and displaying
 * them, using a playback video pipeline for displaying videos.
 * The task communicates with the thread through commands.
 * Image gallery items share one EGL stream, image data is written to that stream. Video gallery
 * items each have an EGL stream.
 * EGL streams are enabled for the current visible item only. The composer displays them on the
 * screen.
 */

#define GL_GLEXT_PROTOTYPES

#include <GLES3/gl31.h>
#include <GLES2/gl2ext.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <assert.h>

#include <list>

#include "Gallery.h"
#include "Composer.h"
#include "Dispatcher.h"
#include "Error.h"
#include "Ordered.h"
#include "Mutex.h"
#include "ConditionVariable.h"
#include "UniquePointer.h"
#include "Thread.h"
#include "GLContext.h"
#include "VideoPipeline.h"

extern "C" {
#include "jpeglib.h"
}

namespace ArgusSamples
{

/**
 * Represents an item in the gallery.
 */
class GalleryItem
{
public:
    GalleryItem(const char *fileName, time_t modTime)
        : m_fileName(fileName)
        , m_modTime(modTime)
    {
    }

    virtual ~GalleryItem()
    {
    }

    /**
     * item types
     */
    enum Type
    {
        TYPE_IMAGE,
        TYPE_VIDEO,
        TYPE_INVALID
    };

    /**
     * Compare function for sort(). Returns true if the first argument goes before the second
     * argument, and false otherwise.
     */
    friend bool operator<(const GalleryItem &l, const GalleryItem &r)
    {
        return (difftime(l.m_modTime, r.m_modTime) > 0);
    }

    /**
     * Initialize
     */
    virtual bool initialize() = 0;

    /**
     * shutdown
     */
    virtual bool shutdown() = 0;

    /**
     * Start the display
     */
    virtual bool startDisplay() { return true; }

    /**
     * Pause the display
     */
    virtual bool pauseDisplay() { return true; }

    /**
     * Toggle playback
     */
    virtual bool togglePlayBack() { return true; }

    /**
     * Rewind
     */
    virtual bool rewind() { return true; }

    /**
     * Get the item type
     */
    virtual Type getType() const = 0;

    /**
     * Get the file name
     */
    const std::string& getFileName() const
    {
        return m_fileName;
    }

protected:
    std::string m_fileName;
    time_t m_modTime;

    GalleryItem();
};

/**
 * Compare function for sort(). Returns true if the first argument goes before the second
 * argument, and false otherwise.
 */
static bool galleryItemCompare(const GalleryItem* const &l, const GalleryItem* const &r)
{
    return (*l < *r);
}

/**
 * A gallery image. Can load JPEG images. Holds image data in CPU memory.
 */
class GalleryItemImage : public GalleryItem
{
public:
    GalleryItemImage(const char *fileName, time_t modTime)
        : GalleryItem(fileName, modTime)
        , m_width(0)
        , m_height(0)
    {
    }

    virtual ~GalleryItemImage()
    {
        PROPAGATE_ERROR_CONTINUE(shutdown());
    }

    /** @name GalleryItem methods */
    /**@{*/
    virtual Type getType() const
    {
        return TYPE_IMAGE;
    }
    virtual bool initialize();
    virtual bool shutdown();
    /**@}*/

    size_t getWidth() const
    {
        return m_width;
    }

    size_t getHeight() const
    {
        return m_height;
    }

    const uint8_t* getData() const
    {
        return m_data.data();
    }

private:
    size_t m_width;
    size_t m_height;
    std::vector<uint8_t> m_data;
};

bool GalleryItemImage::initialize()
{
    // already loaded?
    if (!m_data.empty())
        return true;

    struct jpeg_decompress_struct info;
    struct jpeg_error_mgr err;
    std::vector<JSAMPLE*> rowPointers;
    JDIMENSION read;
    bool success = false;

    // Open file.
    FILE *file = fopen(m_fileName.c_str(), "rb");
    if (!file)
        ORIGINATE_ERROR("Could not open file '%s'.", m_fileName.c_str());

    // Prepare for jpeg decompression.
    memset(&info, 0, sizeof(info));
    info.err = jpeg_std_error(&err);
    jpeg_create_decompress(&info);
#ifdef TEGRA_ACCELERATE
    // Tegra JPEG acceleration seems to be broken, image is all black. We need to disable
    // hardware acceleration.
    jpeg_set_hardware_acceleration_parameters_dec(&info, false, 0, 0, 0, 0, false);
#endif
    jpeg_stdio_src(&info, file);
    if (jpeg_read_header(&info, TRUE) != JPEG_HEADER_OK)
        ORIGINATE_ERROR_FAIL("Invalid JPEG image file '%s'.", m_fileName.c_str());
    if (jpeg_start_decompress(&info) != TRUE)
        ORIGINATE_ERROR_FAIL("Invalid JPEG image file '%s'.", m_fileName.c_str());

    // Determine image format.
    if (info.output_components != 3)
        ORIGINATE_ERROR_FAIL("Only RGB JPEGs supported.");

    // Read the image size.
    m_width = info.output_width;
    m_height = info.output_height;

    // Resize vector for the output.
    m_data.resize(m_width * m_height * info.output_components);

    // Allocate and set row pointers.
    rowPointers.resize(m_height);
    for (size_t row = 0; row < m_height; ++row)
        rowPointers[row] = m_data.data() + row * m_width * info.output_components;

    // Read the image data.
    read = 0;
    while (read < m_height)
        read += jpeg_read_scanlines(&info, &rowPointers[read], m_height - read);

    success = true;

    // Fallthrough
fail:
    if (jpeg_finish_decompress(&info) != TRUE)
        REPORT_ERROR("jpeg_finish_decompress() failed.");
    jpeg_destroy_decompress(&info);
    if (fclose(file) != 0)
        REPORT_ERROR("fclose() failed.");

    return success;
}

bool GalleryItemImage::shutdown()
{
    m_data.clear();
    m_width = 0;
    m_height = 0;
    return true;
}

/**
 * A gallery video. Outputs to an EGL stream.
 */
class GalleryItemVideo : public GalleryItem
{
public:
    GalleryItemVideo(const char *fileName, time_t modTime)
        : GalleryItem(fileName, modTime)
        , m_pipeline(NULL)
        , m_eglStream(EGL_NO_STREAM_KHR)
    {
    }

    virtual ~GalleryItemVideo()
    {
        PROPAGATE_ERROR_CONTINUE(shutdown());
    }

    /** @name GalleryItem methods */
    /**@{*/
    virtual Type getType() const
    {
        return TYPE_VIDEO;
    }
    virtual bool initialize();
    virtual bool shutdown();
    virtual bool startDisplay();
    virtual bool pauseDisplay();
    virtual bool togglePlayBack();
    virtual bool rewind();
    /**@}*/

    EGLStreamKHR getEGLStream() const
    {
        return m_eglStream;
    }

private:
    VideoPipeline *m_pipeline;  ///! playback pipeline
    EGLStreamKHR m_eglStream;
};

bool GalleryItemVideo::initialize()
{
    if (m_pipeline)
        return true;

    m_pipeline = new VideoPipeline;
    if (!m_pipeline)
        ORIGINATE_ERROR("Failed to allocate video pipeline");

    PROPAGATE_ERROR(m_pipeline->setupForPlayback(&m_eglStream, m_fileName.c_str()));
    PROPAGATE_ERROR(Composer::getInstance().bindStream(m_eglStream));

    // set to pause
    PROPAGATE_ERROR(m_pipeline->pause());

    // query size
    float aspectRatio = 1.0f;
    PROPAGATE_ERROR(m_pipeline->getAspectRatio(&aspectRatio));
    PROPAGATE_ERROR(Composer::getInstance().setStreamAspectRatio(m_eglStream, aspectRatio));

    return true;
}

bool GalleryItemVideo::shutdown()
{
    if (m_pipeline)
    {
        PROPAGATE_ERROR_CONTINUE(Composer::getInstance().unbindStream(m_eglStream));
        delete m_pipeline;
        m_pipeline = NULL;
    }
    return true;
}

bool GalleryItemVideo::startDisplay()
{
    if (!m_pipeline)
        ORIGINATE_ERROR("Not initialized");

    // start in paused state
    PROPAGATE_ERROR(m_pipeline->pause());
    return true;
}

bool GalleryItemVideo::pauseDisplay()
{
    if (!m_pipeline)
        ORIGINATE_ERROR("Not initialized");

    PROPAGATE_ERROR(m_pipeline->pause());
    return true;
}

bool GalleryItemVideo::togglePlayBack()
{
    if (!m_pipeline)
        ORIGINATE_ERROR("Not initialized");

    PROPAGATE_ERROR(m_pipeline->toggle());
    return true;
}

bool GalleryItemVideo::rewind()
{
    if (!m_pipeline)
        ORIGINATE_ERROR("Not initialized");

    PROPAGATE_ERROR(m_pipeline->rewind());
    return true;
}

/**
 * This class handles creation of a thread scanning for supported images/videos, loading them and
 * writing the content to an EGLStream.
 */
class GalleryThread : public Thread
{
public:
    GalleryThread();
    ~GalleryThread();

    bool initialize();
    bool shutdown();

    enum Command
    {
        COMMAND_NONE,
        COMMAND_SHUTDOWN,           //!< shutdown
        COMMAND_NEXT,               //!< next item
        COMMAND_PREV,               //!< previous item
        COMMAND_START,              //!< start replay
        COMMAND_TOGGLE_PLAY_BACK,   //!< toggle playback
        COMMAND_REWIND,             //!< rewind
        COMMAND_STOP,               //!< stop replay
    };

    typedef std::list<GalleryItem*> GalleryItemList;

    /**
     * Execute a command
     */
    bool execute(Command command);

private:
    ConditionVariable m_cmdCond;        //! command condition variable
    Mutex m_cmdMutex;                   //! command mutex
    std::list<Ordered<Command> > m_cmdList; //! command list, written by parent thread

    GLContext m_context;

    // resources used to display images
    EGLSurface m_eglOutputSurface;
    EGLStreamHolder m_eglImageOutputStream;
    GLuint m_textureID;
    GLuint m_copyProgram;
    GLuint m_vbo;

    GalleryItemList m_itemList;
    GalleryItemList::iterator m_curItem;

    virtual bool threadInitialize();
    virtual bool threadExecute();
    virtual bool threadShutdown();

    bool buildItemList();

    bool start();
    bool stop();

    bool startDisplay();
    bool pauseDisplay();
    bool togglePlayBack();
    bool rewind();

    /**
     * Get the current output stream
     */
    EGLStreamKHR getOutputStream() const
    {
        if (m_curItem != m_itemList.end())
        {
            switch ((*m_curItem)->getType())
            {
            case GalleryItem::TYPE_IMAGE:
                // images share one output stream
                return m_eglImageOutputStream.get();
            case GalleryItem::TYPE_VIDEO:
                // each video has its own stream
                return static_cast<GalleryItemVideo*>(*m_curItem)->getEGLStream();
            default:
                break;
            }
        }
        return EGL_NO_STREAM_KHR;
    }
};

GalleryThread::GalleryThread()
    : m_eglOutputSurface(EGL_NO_SURFACE)
    , m_textureID(0)
    , m_copyProgram(0)
    , m_vbo(0)
    , m_curItem(m_itemList.end())
{
}

GalleryThread::~GalleryThread()
{
    shutdown();
}

bool GalleryThread::initialize()
{
    PROPAGATE_ERROR(m_cmdMutex.initialize());
    PROPAGATE_ERROR(m_cmdCond.initialize());

    PROPAGATE_ERROR(Thread::initialize());
    return true;
}

bool GalleryThread::shutdown()
{
    PROPAGATE_ERROR_CONTINUE(Thread::shutdown());
    PROPAGATE_ERROR_CONTINUE(m_cmdMutex.shutdown());
    PROPAGATE_ERROR_CONTINUE(m_cmdCond.shutdown());
    return true;
}

bool GalleryThread::execute(Command command)
{
    // this function is *not* to be executed in the thread itself but in the parent thread
    ScopedMutex sm(m_cmdMutex);
    PROPAGATE_ERROR(sm.expectLocked());

    m_cmdList.push_back(command);

    PROPAGATE_ERROR(m_cmdCond.signal());

    return true;
}

/**
 * Builds a list of gallery items by scanning the output path for image and video files. The
 * list is sorted with the newest files first.
 */
bool GalleryThread::buildItemList()
{
    bool success = true;
    struct extTypePair
    {
        const char *ext;
        GalleryItem::Type type;
    };
    const extTypePair extensions[] =
    {
        { "jpg",
            GalleryItem::TYPE_IMAGE },
        { VideoPipeline::getFileExtension(VideoPipeline::VIDEO_FILE_TYPE_MP4),
            GalleryItem::TYPE_VIDEO },
        { VideoPipeline::getFileExtension(VideoPipeline::VIDEO_FILE_TYPE_3GP),
            GalleryItem::TYPE_VIDEO },
        { VideoPipeline::getFileExtension(VideoPipeline::VIDEO_FILE_TYPE_AVI),
            GalleryItem::TYPE_VIDEO },
        { VideoPipeline::getFileExtension(VideoPipeline::VIDEO_FILE_TYPE_H265),
            GalleryItem::TYPE_VIDEO },
    };

    // open output directory
    const std::string path(Dispatcher::getInstance().m_outputPath.get());
    DIR *directory = opendir(path.c_str());
    if (directory != NULL)
    {
        // scan all files
        struct dirent *entry;
        while ((entry = readdir(directory)))
        {
            // we are looking for files only
            if (entry->d_type != DT_REG)
                continue;

            // check for supported extensions
            GalleryItem::Type type = GalleryItem::TYPE_INVALID;
            const size_t fileNameLen = strlen(entry->d_name);
            for (size_t index = 0; index < sizeof(extensions) / sizeof(extensions[0]); ++index)
            {
                const extTypePair *ext = &extensions[index];

                // filename should be longer than '.ext', ext has no '.' therefore +1
                const size_t extLen = strlen(ext->ext);
                if (fileNameLen >= extLen + 1)
                {
                    if ((entry->d_name[fileNameLen - extLen - 1] == '.') &&
                        (strcasecmp(&entry->d_name[fileNameLen - extLen], ext->ext) == 0))
                    {
                        type = ext->type;
                        break;
                    }
                }
            }
            if (type == GalleryItem::TYPE_INVALID)
                continue;

            std::string fullName;

            fullName = path;
            fullName += "/";
            fullName += entry->d_name;

            // get status on the file
            struct stat fileStat;
            if (stat(fullName.c_str(), &fileStat) != 0)
                ORIGINATE_ERROR_FAIL("Failed to query file status on '%s'", fullName.c_str());

            UniquePointer<GalleryItem> item;
            if (type == GalleryItem::TYPE_VIDEO)
            {
                item.reset(new GalleryItemVideo(fullName.c_str(), fileStat.st_mtime));
            }
            else
            {
                assert(type == GalleryItem::TYPE_IMAGE);
                item.reset(new GalleryItemImage(fullName.c_str(), fileStat.st_mtime));
            }
            if (!item)
                ORIGINATE_ERROR("Failed to create gallery item");
            m_itemList.push_back(item.release());
        }
    }

    m_itemList.sort(galleryItemCompare);

    goto pass;

fail:
    success = false;

pass:
    if (directory != NULL)
        closedir(directory);

    return success;
}

bool GalleryThread::threadInitialize()
{
    Composer &composer = Composer::getInstance();
    //! @todo Using 1920x1080 for now. Should use the image size, but this would require creating
    //        one stream and one surface for each image because surfaces can't be resized
    const uint32_t streamWidth = 1920;
    const uint32_t streamHeight = 1080;

    // create the EGL output stream
    PROPAGATE_ERROR(m_eglImageOutputStream.create(composer.getEGLDisplay()));
    CHECK_STREAM_STATE(m_eglImageOutputStream, CREATED);

    // bind the output stream to the composer
    PROPAGATE_ERROR(composer.bindStream(m_eglImageOutputStream.get()));
    PROPAGATE_ERROR(composer.setStreamAspectRatio(m_eglImageOutputStream.get(),
        (float)streamWidth / (float)streamHeight));
    CHECK_STREAM_STATE(m_eglImageOutputStream, CONNECTING);

    // create a EGL context and make it current to the output surface
    PROPAGATE_ERROR(m_context.initialize(composer.getEGLDisplay()));

    // create the EGL output surface and connect the EGL output stream to it
    PROPAGATE_ERROR(m_context.createEGLStreamProducerSurface(&m_eglOutputSurface,
        m_eglImageOutputStream.get(), streamWidth, streamHeight));
    CHECK_STREAM_STATE(m_eglImageOutputStream, EMPTY);

    PROPAGATE_ERROR(m_context.makeCurrent(m_eglOutputSurface));

    // create a texture used for images
    glGenTextures(1, &m_textureID);
    if (m_textureID == 0)
        ORIGINATE_ERROR("Failed to create GL texture");

    // Create the shader programs
    static const char vtxSrc[] =
        "#version 300 es\n"
        "#extension GL_ARB_explicit_uniform_location : require\n"
        "in layout(location = 0) vec2 vertex;\n"
        "out vec2 vTexCoord;\n"
        "layout(location = 0) uniform vec2 offset;\n"
        "layout(location = 1) uniform vec2 scale;\n"
        "void main() {\n"
        "  gl_Position = vec4((offset + vertex * scale) * 2.0 - 1.0, 0.0, 1.0);\n"
        "  vTexCoord = vec2(vertex.x, vertex.y);\n"
        "}\n";

    static const char copyFrgSrc[] =
        "#version 300 es\n"
        "precision highp float;\n"
        "uniform sampler2D texSampler;\n"
        "in vec2 vTexCoord;\n"
        "out vec4 fragColor;\n"
        "void main() {\n"
        "  fragColor = texture(texSampler, vTexCoord);\n"
        "}\n";
    PROPAGATE_ERROR(m_context.createProgram(vtxSrc, copyFrgSrc, &m_copyProgram));

    // Setup vertex state.
    static const GLfloat vertices[] =
    {
         0.0f, 0.0f,
         0.0f, 1.0f,
         1.0f, 0.0f,
         1.0f, 1.0f,
    };
    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glEnableVertexAttribArray(0);

    return true;
}

/**
 * Start. Build the item list.
 */
bool GalleryThread::start()
{
    if (!m_itemList.empty())
        ORIGINATE_ERROR("Item list should be empty");

    // build a list of all items
    PROPAGATE_ERROR(buildItemList());
    if (m_itemList.empty())
        m_curItem = m_itemList.end();
    else
        m_curItem = m_itemList.begin();

     return true;
}

/**
 * Stop. Pause display and free the item list.
 */
bool GalleryThread::stop()
{
    PROPAGATE_ERROR(pauseDisplay());

    for (GalleryItemList::iterator it = m_itemList.begin(); it != m_itemList.end(); ++it)
        delete (*it);
    m_itemList.clear();
    m_curItem = m_itemList.end();

    return true;
}

bool GalleryThread::startDisplay()
{
    if (m_curItem == m_itemList.end())
        return true;

    std::ostringstream message;

    message << "Displaying '" << (*m_curItem)->getFileName() << "'" << std::endl;
    Dispatcher::getInstance().message(message.str().c_str());

    PROPAGATE_ERROR((*m_curItem)->initialize());

    switch ((*m_curItem)->getType())
    {
    case GalleryItem::TYPE_IMAGE:
    {
        GalleryItemImage *image = static_cast<GalleryItemImage*>(*m_curItem);

        // draw it to the surface
        glClear(GL_COLOR_BUFFER_BIT);

        glBindTexture(GL_TEXTURE_2D, m_textureID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        // load the item into the texture
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, image->getWidth(), image->getHeight(), 0,
            GL_RGB, GL_UNSIGNED_BYTE, reinterpret_cast<const void*>(image->getData()));

        // copy from the input to the output
        glUseProgram(m_copyProgram);
        glUniform2f(0, 0.0f, 0.0f); // offset
        glUniform2f(1, 1.0f, 1.0f); // scale
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // the swap will put the image into the output EGL stream
        PROPAGATE_ERROR(m_context.swapBuffers(m_eglOutputSurface));
    }
        break;
    case GalleryItem::TYPE_VIDEO:
        PROPAGATE_ERROR(static_cast<GalleryItemVideo*>(*m_curItem)->startDisplay());
        break;
    default:
        ORIGINATE_ERROR("Unhandled gallery item type");
    }

    PROPAGATE_ERROR(Composer::getInstance().setStreamActive(getOutputStream(), true));

    return true;
}

bool GalleryThread::pauseDisplay()
{
    if (m_curItem == m_itemList.end())
        return true;

    PROPAGATE_ERROR(Composer::getInstance().setStreamActive(getOutputStream(), false));
    PROPAGATE_ERROR((*m_curItem)->pauseDisplay());

    return true;
}

bool GalleryThread::togglePlayBack()
{
    if (m_curItem == m_itemList.end())
        return true;

    PROPAGATE_ERROR((*m_curItem)->togglePlayBack());

    return true;
}

bool GalleryThread::rewind()
{
    if (m_curItem == m_itemList.end())
        return true;

    PROPAGATE_ERROR((*m_curItem)->rewind());

    return true;
}

bool GalleryThread::threadExecute()
{
    Command cmd = COMMAND_NONE;

    {
        ScopedMutex sm(m_cmdMutex);
        PROPAGATE_ERROR(sm.expectLocked());

        // if the list is empty wait for new commands
        if (m_cmdList.empty())
            PROPAGATE_ERROR(m_cmdCond.wait(m_cmdMutex));

        // get the command from the list
        cmd = m_cmdList.front();
        m_cmdList.pop_front();
    }

    switch(cmd)
    {
    case COMMAND_NONE:
        break;
    case COMMAND_SHUTDOWN:
        PROPAGATE_ERROR(requestShutdown());
        break;
    case COMMAND_START:
        PROPAGATE_ERROR(start());
        PROPAGATE_ERROR(startDisplay());
        break;
    case COMMAND_TOGGLE_PLAY_BACK:
        PROPAGATE_ERROR(togglePlayBack());
        break;
    case COMMAND_REWIND:
        PROPAGATE_ERROR(rewind());
        break;
    case COMMAND_STOP:
        PROPAGATE_ERROR(stop());
        break;
    case COMMAND_PREV:
        if ((m_curItem != m_itemList.end()) && (m_curItem != m_itemList.begin()))
        {
            PROPAGATE_ERROR(pauseDisplay());
            --m_curItem;
            PROPAGATE_ERROR(startDisplay());
        }
        break;
    case COMMAND_NEXT:
        if ((m_curItem != m_itemList.end()) && (&(*m_curItem) != &m_itemList.back()))
        {
            PROPAGATE_ERROR(pauseDisplay());
            ++m_curItem;
            PROPAGATE_ERROR(startDisplay());
        }
        break;
    default:
        ORIGINATE_ERROR("Invalid command %d", cmd);
    }

    return true;
}

bool GalleryThread::threadShutdown()
{
    Composer &composer = Composer::getInstance();

    PROPAGATE_ERROR_CONTINUE(stop());

    if (m_eglOutputSurface != EGL_NO_SURFACE)
    {
        eglDestroySurface(composer.getEGLDisplay(), m_eglOutputSurface);
        m_eglOutputSurface = EGL_NO_SURFACE;
    }

    // unbind the EGL output stream from the composer
    PROPAGATE_ERROR_CONTINUE(composer.unbindStream(m_eglImageOutputStream.get()));
    CHECK_STREAM_STATE(m_eglImageOutputStream, DISCONNECTED);
    // and destroy the EGL output stream
    PROPAGATE_ERROR_CONTINUE(m_eglImageOutputStream.destroy());

    // free GL resources
    glDeleteTextures(1, &m_textureID);
    m_textureID = 0;
    glDeleteProgram(m_copyProgram);
    m_copyProgram = 0;
    glDeleteBuffers(1, &m_vbo);
    m_vbo = 0;

    PROPAGATE_ERROR_CONTINUE(m_context.cleanup());

    return true;
}

TaskGallery::TaskGallery()
    : m_initialized(false)
    , m_running(false)
    , m_thread(NULL)
{
}

TaskGallery::~TaskGallery()
{
    shutdown();
}

bool TaskGallery::initialize()
{
    if (m_initialized)
        return true;

    m_initialized = true;

    return true;
}

bool TaskGallery::shutdown()
{
    if (!m_initialized)
        return true;

    // stop the module
    PROPAGATE_ERROR_CONTINUE(stop());

    m_initialized = false;

    return true;
}

bool TaskGallery::start()
{
    if (!m_initialized)
        ORIGINATE_ERROR("Not initialized");
    if (m_running)
        return true;

    // create the gallery thread, it will load and display the items
    UniquePointer<GalleryThread> galleryThread(new GalleryThread());
    if (!galleryThread)
        ORIGINATE_ERROR("Out of memory");

    PROPAGATE_ERROR(galleryThread->initialize());
    m_thread = galleryThread.release();

    PROPAGATE_ERROR(m_thread->waitRunning());

    PROPAGATE_ERROR(m_thread->execute(GalleryThread::COMMAND_START));

    m_running = true;

    return true;
}

bool TaskGallery::stop()
{
    if (!m_initialized)
        ORIGINATE_ERROR("Not initialized");
    if (!m_running)
        return true;

    PROPAGATE_ERROR(m_thread->execute(GalleryThread::COMMAND_STOP));

    // send the shutdown command
    PROPAGATE_ERROR(m_thread->execute(GalleryThread::COMMAND_SHUTDOWN));
    // destroy the thread
    PROPAGATE_ERROR(m_thread->shutdown());

    delete m_thread;
    m_thread = NULL;

    m_running = false;

    return true;
}

bool TaskGallery::togglePlayBack()
{
    if (!m_running)
        ORIGINATE_ERROR("Not running");

    PROPAGATE_ERROR(m_thread->execute(GalleryThread::COMMAND_TOGGLE_PLAY_BACK));
    return true;
}

bool TaskGallery::rewind()
{
    if (!m_running)
        ORIGINATE_ERROR("Not running");

    PROPAGATE_ERROR(m_thread->execute(GalleryThread::COMMAND_REWIND));
    return true;
}

bool TaskGallery::prevItem()
{
    if (!m_running)
        ORIGINATE_ERROR("Not running");

    PROPAGATE_ERROR(m_thread->execute(GalleryThread::COMMAND_PREV));
    return true;
}

bool TaskGallery::nextItem()
{
    if (!m_running)
        ORIGINATE_ERROR("Not running");

    PROPAGATE_ERROR(m_thread->execute(GalleryThread::COMMAND_NEXT));
    return true;
}

}; // namespace ArgusSamples
