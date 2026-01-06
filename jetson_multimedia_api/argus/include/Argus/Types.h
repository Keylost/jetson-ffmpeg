/*
 * Copyright (c) 2016-2023, NVIDIA CORPORATION. All rights reserved.
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
 * <b>Libargus API: Types API</b>
 *
 * @b Description: Defines the basic types that are used by the API.
 */

#ifndef _ARGUS_TYPES_H
#define _ARGUS_TYPES_H

#include <stdint.h>
#include <vector>
#include <string>
#include <assert.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

// Some versions of the Xlib.h header file define 'Status' to 'int'.
// This collides with the libargus 'Status' type.
// If 'Status' is defined then undefine it and use a typedef instead.
#ifdef Status
#undef Status
typedef int Status;
#endif // Status

namespace Argus
{

/*
 * Forward declaration of standard objects
 */
class CameraDevice;
class CameraProvider;
class CaptureSession;
class CaptureMetadata;
class CaptureMetadataContainer;
class Event;
class EventQueue;
class InputStream;
class OutputStream;
class OutputStreamSettings;
class Request;
class SensorMode;

/*
 * Forward declaration of standard interfaces
 */
class ICameraProperties;
class ICameraProvider;
class ICaptureSession;
class IAutoControlSettings;
class IRequest;
class IStream;
class IStreamSettings;

/**
 * Constant used for infinite timeouts.
 */
const uint64_t TIMEOUT_INFINITE = 0xFFFFFFFFFFFFFFFF;

/**
 * Status values returned by API function calls.
 */
enum Status
{
    /// Function succeeded.
    STATUS_OK                 = 0,

    /// The set of parameters passed was invalid.
    STATUS_INVALID_PARAMS     = 1,

    /// The requested settings are invalid.
    STATUS_INVALID_SETTINGS   = 2,

    /// The requested device is unavailable.
    STATUS_UNAVAILABLE        = 3,

    /// An operation failed because of insufficient mavailable memory.
    STATUS_OUT_OF_MEMORY      = 4,

    /// This method has not been implemented.
    STATUS_UNIMPLEMENTED      = 5,

    /// An operation timed out.
    STATUS_TIMEOUT            = 6,

    /// The capture was aborted. @see ICaptureSession::cancelRequests()
    STATUS_CANCELLED          = 7,

    /// The stream or other resource has been disconnected.
    STATUS_DISCONNECTED       = 8,

    /// End of stream, used by Stream objects.
    STATUS_END_OF_STREAM      = 9,

    // Number of elements in this enum.
    STATUS_COUNT
};

/**
 * Color channel constants for Bayer data.
 */
enum BayerChannel
{
    BAYER_CHANNEL_R,
    BAYER_CHANNEL_G_EVEN,
    BAYER_CHANNEL_G_ODD,
    BAYER_CHANNEL_B,

    BAYER_CHANNEL_COUNT
};

/**
 * Coordinates used for 2D and 3D points.
 */
enum Coordinate
{
    COORDINATE_X,
    COORDINATE_Y,
    COORDINATE_Z,

    COORDINATE_2D_COUNT = 2,
    COORDINATE_3D_COUNT = 3
};

/**
 * Color channel constants for RGB data.
 */
enum RGBChannel
{
    RGB_CHANNEL_R,
    RGB_CHANNEL_G,
    RGB_CHANNEL_B,

    RGB_CHANNEL_COUNT
};

/**
 * Auto Exposure Anti-Banding Modes.
 */
DEFINE_NAMED_UUID_CLASS(AeAntibandingMode);
DEFINE_UUID(AeAntibandingMode, AE_ANTIBANDING_MODE_OFF,  AD1E5560,9C16,11E8,B568,18,00,20,0C,9A,66);
DEFINE_UUID(AeAntibandingMode, AE_ANTIBANDING_MODE_AUTO, AD1E5561,9C16,11E8,B568,18,00,20,0C,9A,66);
DEFINE_UUID(AeAntibandingMode, AE_ANTIBANDING_MODE_50HZ, AD1E5562,9C16,11E8,B568,18,00,20,0C,9A,66);
DEFINE_UUID(AeAntibandingMode, AE_ANTIBANDING_MODE_60HZ, AD1E5563,9C16,11E8,B568,18,00,20,0C,9A,66);

/**
 * Auto Exposure (AE) Modes.
 */
DEFINE_NAMED_UUID_CLASS(AeMode);
DEFINE_UUID(AeMode, AE_MODE_OFF,    FCACB1E0,C6E5,11ED,A901,08,00,20,0C,9A,66);
DEFINE_UUID(AeMode, AE_MODE_ON,     FCACB1E1,C6E5,11ED,A901,08,00,20,0C,9A,66);
DEFINE_UUID(AeMode, AE_MODE_OTHERS, FCACB1E2,C6E5,11ED,A901,08,00,20,0C,9A,66);

/**
 * Auto Exposure Flicker States.
 */
DEFINE_NAMED_UUID_CLASS(AeFlickerState);
DEFINE_UUID(AeFlickerState, AE_FLICKER_NONE,  AD1E5564,9C16,11E8,B568,18,00,20,0C,9A,66);
DEFINE_UUID(AeFlickerState, AE_FLICKER_50HZ, AD1E5565,9C16,11E8,B568,18,00,20,0C,9A,66);
DEFINE_UUID(AeFlickerState, AE_FLICKER_60HZ, AD1E5566,9C16,11E8,B568,18,00,20,0C,9A,66);

/**
 * Auto Exposure States.
 */
DEFINE_NAMED_UUID_CLASS(AeState);
DEFINE_UUID(AeState, AE_STATE_INACTIVE,       D2EBEA50,9C16,11E8,B568,18,00,20,0C,9A,66);
DEFINE_UUID(AeState, AE_STATE_SEARCHING,      D2EBEA51,9C16,11E8,B568,18,00,20,0C,9A,66);
DEFINE_UUID(AeState, AE_STATE_CONVERGED,      D2EBEA52,9C16,11E8,B568,18,00,20,0C,9A,66);
DEFINE_UUID(AeState, AE_STATE_FLASH_REQUIRED, D2EBEA53,9C16,11E8,B568,18,00,20,0C,9A,66);
DEFINE_UUID(AeState, AE_STATE_TIMEOUT,        D2EBEA54,9C16,11E8,B568,18,00,20,0C,9A,66);

/**
 * Auto White Balance (AWB) Modes.
 */
DEFINE_NAMED_UUID_CLASS(AwbMode);
DEFINE_UUID(AwbMode, AWB_MODE_OFF,              FB3F365A,CC62,11E5,9956,62,56,62,87,07,61);
DEFINE_UUID(AwbMode, AWB_MODE_AUTO,             FB3F365B,CC62,11E5,9956,62,56,62,87,07,61);
DEFINE_UUID(AwbMode, AWB_MODE_INCANDESCENT,     FB3F365C,CC62,11E5,9956,62,56,62,87,07,61);
DEFINE_UUID(AwbMode, AWB_MODE_FLUORESCENT,      FB3F365D,CC62,11E5,9956,62,56,62,87,07,61);
DEFINE_UUID(AwbMode, AWB_MODE_WARM_FLUORESCENT, FB3F365E,CC62,11E5,9956,62,56,62,87,07,61);
DEFINE_UUID(AwbMode, AWB_MODE_DAYLIGHT,         FB3F365F,CC62,11E5,9956,62,56,62,87,07,61);
DEFINE_UUID(AwbMode, AWB_MODE_CLOUDY_DAYLIGHT,  FB3F3660,CC62,11E5,9956,62,56,62,87,07,61);
DEFINE_UUID(AwbMode, AWB_MODE_TWILIGHT,         FB3F3661,CC62,11E5,9956,62,56,62,87,07,61);
DEFINE_UUID(AwbMode, AWB_MODE_SHADE,            FB3F3662,CC62,11E5,9956,62,56,62,87,07,61);
DEFINE_UUID(AwbMode, AWB_MODE_MANUAL,           20FB45DA,C49F,4293,AB02,13,3F,8C,CA,DD,69);

/**
 * Auto White-Balance States.
 */
DEFINE_NAMED_UUID_CLASS(AwbState);
DEFINE_UUID(AwbState, AWB_STATE_INACTIVE,  E33CDB30,9C16,11E8,B568,18,00,20,0C,9A,66);
DEFINE_UUID(AwbState, AWB_STATE_SEARCHING, E33CDB31,9C16,11E8,B568,18,00,20,0C,9A,66);
DEFINE_UUID(AwbState, AWB_STATE_CONVERGED, E33CDB32,9C16,11E8,B568,18,00,20,0C,9A,66);
DEFINE_UUID(AwbState, AWB_STATE_LOCKED,    E33CDB33,9C16,11E8,B568,18,00,20,0C,9A,66);

/**
 * Auto Focus (AF) Modes.
 */
DEFINE_NAMED_UUID_CLASS(AfMode);
DEFINE_UUID(AfMode, AF_MODE_OFF,    FCACB1E3,C6E5,11ED,A901,08,00,20,0C,9A,66);
DEFINE_UUID(AfMode, AF_MODE_AUTO,   FCACB1E4,C6E5,11ED,A901,08,00,20,0C,9A,66);
DEFINE_UUID(AfMode, AF_MODE_MANUAL, FCACB1E5,C6E5,11ED,A901,08,00,20,0C,9A,66);
DEFINE_UUID(AfMode, AF_MODE_OTHERS, FCACB1E6,C6E5,11ED,A901,08,00,20,0C,9A,66);

/**
 * A CaptureIntent may be provided during capture request creation to initialize the new
 * Request with default settings that are appropriate for captures of the given intent.
 * More details regarding each intent are as follows:
 * MANUAL intent disables auto white balance and auto-focus.
 * PREVIEW intent disables noise reduction related post-processing in order to
 * reduce latency and resource usage.
 * STILL_CAPTURE intent enables Noise Reduction related post-processing in order
 * to optimize still image quality.
 * VIDEO_RECORD intent enables motion sensors related post-processing to optimize
 * the video quality.
 * Apart from above processing blocks each intent also helps in optimizing the
 * processing resource usage appropriate for that intent.
 */
DEFINE_NAMED_UUID_CLASS(CaptureIntent);
DEFINE_UUID(CaptureIntent, CAPTURE_INTENT_MANUAL,         FB3F3663,CC62,11E5,9956,62,56,62,87,07,61);
DEFINE_UUID(CaptureIntent, CAPTURE_INTENT_PREVIEW,        FB3F3664,CC62,11E5,9956,62,56,62,87,07,61);
DEFINE_UUID(CaptureIntent, CAPTURE_INTENT_STILL_CAPTURE,  FB3F3665,CC62,11E5,9956,62,56,62,87,07,61);
DEFINE_UUID(CaptureIntent, CAPTURE_INTENT_VIDEO_RECORD,   FB3F3666,CC62,11E5,9956,62,56,62,87,07,61);
DEFINE_UUID(CaptureIntent, CAPTURE_INTENT_VIDEO_SNAPSHOT, FB3F3667,CC62,11E5,9956,62,56,62,87,07,61);

/**
 * Denoise (noise reduction) Modes.
 */
DEFINE_NAMED_UUID_CLASS(DenoiseMode);
DEFINE_UUID(DenoiseMode, DENOISE_MODE_OFF,          FB3F3668,CC62,11E5,9956,62,56,62,87,07,61);
DEFINE_UUID(DenoiseMode, DENOISE_MODE_FAST,         FB3F3669,CC62,11E5,9956,62,56,62,87,07,61);
DEFINE_UUID(DenoiseMode, DENOISE_MODE_HIGH_QUALITY, FB3F366A,CC62,11E5,9956,62,56,62,87,07,61);

/**
 * Edge Enhance Modes.
 */
DEFINE_NAMED_UUID_CLASS(EdgeEnhanceMode);
DEFINE_UUID(EdgeEnhanceMode, EDGE_ENHANCE_MODE_OFF,          F7100B40,6A5F,11E6,BDF4,08,00,20,0C,9A,66);
DEFINE_UUID(EdgeEnhanceMode, EDGE_ENHANCE_MODE_FAST,         F7100B41,6A5F,11E6,BDF4,08,00,20,0C,9A,66);
DEFINE_UUID(EdgeEnhanceMode, EDGE_ENHANCE_MODE_HIGH_QUALITY, F7100B42,6A5F,11E6,BDF4,08,00,20,0C,9A,66);

/**
 * Extension Names. Note that ExtensionName UUIDs are defined by their respective extension headers.
 */
DEFINE_NAMED_UUID_CLASS(ExtensionName);

/**
 * Pixel formats.
 */
DEFINE_NAMED_UUID_CLASS(PixelFormat);
DEFINE_UUID(PixelFormat, PIXEL_FMT_UNKNOWN,       00000000,93d5,11e5,0000,1c,b7,2c,ef,d4,1e);
DEFINE_UUID(PixelFormat, PIXEL_FMT_Y8,            569be14a,93d5,11e5,91bc,1c,b7,2c,ef,d4,1e);
DEFINE_UUID(PixelFormat, PIXEL_FMT_Y16,           56ddb19c,93d5,11e5,8e2c,1c,b7,2c,ef,d4,1e);
DEFINE_UUID(PixelFormat, PIXEL_FMT_YCbCr_420_888, 570c10e6,93d5,11e5,8ff3,1c,b7,2c,ef,d4,1e);
DEFINE_UUID(PixelFormat, PIXEL_FMT_YCbCr_422_888, 573a7940,93d5,11e5,99c2,1c,b7,2c,ef,d4,1e);
DEFINE_UUID(PixelFormat, PIXEL_FMT_YCbCr_444_888, 576043dc,93d5,11e5,8983,1c,b7,2c,ef,d4,1e);
DEFINE_UUID(PixelFormat, PIXEL_FMT_JPEG_BLOB,     578b08c4,93d5,11e5,9686,1c,b7,2c,ef,d4,1e);
DEFINE_UUID(PixelFormat, PIXEL_FMT_RAW16,         57b484d8,93d5,11e5,aeb6,1c,b7,2c,ef,d4,1e);
DEFINE_UUID(PixelFormat, PIXEL_FMT_P016,          57b484d9,93d5,11e5,aeb6,1c,b7,2c,ef,d4,1e);

/**
 * The SensorModeType of a sensor defines the type of image data that is output by the
 * imaging sensor before any sort of image processing (ie. pre-ISP format).
 */
DEFINE_NAMED_UUID_CLASS(SensorModeType);
DEFINE_UUID(SensorModeType, SENSOR_MODE_TYPE_DEPTH,     64483464,4b91,11e6,bbbd,40,16,7e,ab,86,92);
DEFINE_UUID(SensorModeType, SENSOR_MODE_TYPE_YUV,       6453e00c,4b91,11e6,871d,40,16,7e,ab,86,92);
DEFINE_UUID(SensorModeType, SENSOR_MODE_TYPE_RGB,       6463d4c6,4b91,11e6,88a3,40,16,7e,ab,86,92);
DEFINE_UUID(SensorModeType, SENSOR_MODE_TYPE_BAYER,     646f04ea,4b91,11e6,9c06,40,16,7e,ab,86,92);
DEFINE_UUID(SensorModeType, SENSOR_MODE_TYPE_BAYER_PWL, f6a08220,6a0f,11eb,8572,08,00,20,0c,9a,66);
DEFINE_UUID(SensorModeType, SENSOR_MODE_TYPE_BAYER_DOL, f6a08221,6a0f,11eb,8572,08,00,20,0c,9a,66);

/**
 * SensorPlacement defines the placement of the sensor on the module
 */
DEFINE_NAMED_UUID_CLASS(SensorPlacement);
DEFINE_UUID(SensorPlacement, SENSOR_PLACEMENT_REAR_OR_BOTTOM_OR_BOTTOM_LEFT, 01dba8b0,1946,11eb,8b6f,08,00,20,0c,9a,66);
DEFINE_UUID(SensorPlacement, SENSOR_PLACEMENT_FRONT_OR_TOP_OR_CENTER_LEFT,   01dba8b1,1946,11eb,8b6f,08,00,20,0c,9a,66);
DEFINE_UUID(SensorPlacement, SENSOR_PLACEMENT_CENTER_OR_CENTER_RIGHT,        01dba8b2,1946,11eb,8b6f,08,00,20,0c,9a,66);
DEFINE_UUID(SensorPlacement, SENSOR_PLACEMENT_TOP_LEFT,                      01dba8b3,1946,11eb,8b6f,08,00,20,0c,9a,66);
DEFINE_UUID(SensorPlacement, SENSOR_PLACEMENT_BOTTOM_RIGHT,                  01dba8b4,1946,11eb,8b6f,08,00,20,0c,9a,66);
DEFINE_UUID(SensorPlacement, SENSOR_PLACEMENT_TOP_RIGHT,                     01dba8b5,1946,11eb,8b6f,08,00,20,0c,9a,66);

/**
 * Bayer Phases
 */
DEFINE_NAMED_UUID_CLASS(BayerPhase);
DEFINE_UUID(BayerPhase, BAYER_PHASE_UNKNOWN,     b9d43270,6a0e,12eb,8572,08,00,20,0c,9a,66);
DEFINE_UUID(BayerPhase, BAYER_PHASE_RGGB,        b9d43271,6a0e,12eb,8572,08,00,20,0c,9a,66);
DEFINE_UUID(BayerPhase, BAYER_PHASE_BGGR,        b9d43272,6a0e,12eb,8572,08,00,20,0c,9a,66);
DEFINE_UUID(BayerPhase, BAYER_PHASE_GRBG,        b9d43273,6a0e,12eb,8572,08,00,20,0c,9a,66);
DEFINE_UUID(BayerPhase, BAYER_PHASE_GBRG,        b9d43274,6a0e,12eb,8572,08,00,20,0c,9a,66);

/**
 * Utility class for libargus interfaces.
 */
class NonCopyable
{
protected:
    NonCopyable() {}

private:
    NonCopyable(NonCopyable& other);
    NonCopyable& operator=(NonCopyable& other);
};

/**
 * The top-level interface class.
 *
 * By convention, every Interface subclass exposes a public static method called @c id(),
 * which returns the unique InterfaceID for that interface.
 * This is required for the @c interface_cast<> template to work with that interface.
 */
class Interface : NonCopyable
{
protected:
    Interface() {}
    ~Interface() {}
};

/**
 * A unique identifier for a libargus Interface.
 */
class InterfaceID : public NamedUUID
{
public:
    InterfaceID(uint32_t time_low_
              , uint16_t time_mid_
              , uint16_t time_hi_and_version_
              , uint16_t clock_seq_
              , uint8_t c0, uint8_t c1, uint8_t c2, uint8_t c3, uint8_t c4, uint8_t c5
              , const char* name)
    : NamedUUID(time_low_, time_mid_, time_hi_and_version_, clock_seq_,
                c0, c1, c2, c3, c4, c5, name)
    {}

    InterfaceID()
    : NamedUUID(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "IID_UNSPECIFIED")
    {}
};

/**
 * The base interface for a class that provides libargus Interfaces.
 */
class InterfaceProvider : NonCopyable
{
public:

    /**
     * Acquire the interface specified by @c interfaceId.
     * @returns An instance of the requested interface,
     * or NULL if that interface is not available.
     */
    virtual Interface* getInterface(const InterfaceID& interfaceId) = 0;

protected:
    ~InterfaceProvider() {}
};

/**
 * Interface-casting helper similar to dynamic_cast.
 */

template <typename TheInterface>
inline TheInterface* interface_cast(InterfaceProvider* obj)
{
    return static_cast<TheInterface*>(obj ? obj->getInterface(TheInterface::id()): 0);
}

template <typename TheInterface>
inline TheInterface* interface_cast(const InterfaceProvider* obj)
{
    return static_cast<TheInterface*>(
        obj ? const_cast<const Interface*>(
                const_cast<InterfaceProvider*>(obj)->getInterface(TheInterface::id())): 0);
}

/**
 * A top level object class for libargus objects that are created and owned by
 * the client. All Destructable objects created by the client must be explicitly destroyed.
 */
class Destructable
{
public:

    /**
     * Destroy this object.
     * After making this call, the client cannot make any more calls on this object.
     */
    virtual void destroy() = 0;

protected:
    ~Destructable() {}
};

/**
 * Template helper emulating C++11 rvalue semantics.
 * @cond
 */
template<typename T>
class rv : public T
{
    rv();
    ~rv();
    rv(const rv&);
    void operator=(const rv&);
};

template<typename T>
    rv<T>& move(T& self)
{
    return *static_cast<rv<T>*>(&self);
}
/** @endcond */

/**
 * Movable smart pointer mimicking std::unique_ptr.
 * @cond
 */
template <typename T> struct remove_const;
template <typename T> struct remove_const<const T&>{ typedef T& type; };
template <typename T> struct remove_const<const T*>{ typedef T* type; };
template <typename T> struct remove_const<const T >{ typedef T  type; };
template <typename T> struct remove_const          { typedef T  type; };
/** @endcond */

template <typename T>
class UniqueObj : NonCopyable
{
public:
    explicit UniqueObj(T* obj=NULL): m_obj(obj) {}

    void reset(T* obj=NULL)
        { if (m_obj && (m_obj != obj)) const_cast<typename remove_const<T*>::type>(m_obj)->destroy(); m_obj = obj; }
    T* release()
        { T* obj = m_obj; m_obj = NULL; return obj; }

    UniqueObj( rv<UniqueObj>& moved ): m_obj(moved.release()) {}
    UniqueObj& operator=( rv<UniqueObj>& moved ){ reset( moved.release()); return *this; }

    ~UniqueObj() { reset(); }

    T& operator*() const { return *m_obj; }
    T* get() const { return m_obj; }

    operator bool() const { return !!m_obj; }

    operator       rv<UniqueObj>&()       { return *static_cast<      rv<UniqueObj>*>(this); }
    operator const rv<UniqueObj>&() const { return *static_cast<const rv<UniqueObj>*>(this); }

private:
    T* m_obj;

    T* operator->() const; // Prevent calling destroy() directly.
                           // Note: For getInterface functionality use interface_cast.
};

template <typename TheInterface, typename TObject>
inline TheInterface* interface_cast(const UniqueObj<TObject>& obj)
{
    return interface_cast<TheInterface>( obj.get());
}

/**
 * Tuple template class. This provides a finite ordered list of N elements having type T.
 */
template <unsigned int N, typename T>
class Tuple
{
public:
    Tuple() {}

    /// Initialize every element of the tuple to a single value.
    Tuple(T init)
    {
        for (unsigned int i = 0; i < N; i++)
            m_data[i] = init;
    }

    /// Returns true when every element in the two tuples are identical.
    bool operator==(const Tuple<N,T>& rhs) const
    {
        return !memcmp(m_data, rhs.m_data, sizeof(m_data));
    }

    /// Returns true if there are any differences between the two tuples.
    bool operator!=(const Tuple<N,T>& rhs) const
    {
        return !(*this == rhs);
    }

    /// Adds every element of another tuple to the elements of this tuple.
    Tuple<N, T>& operator+=(const Tuple<N, T>& rhs)
    {
        for (unsigned int i = 0; i < N; i++)
            m_data[i] += rhs.m_data[i];
        return *this;
    }

    /// Subtracts every element of another tuple from the elements of this tuple.
    Tuple<N, T>& operator-=(const Tuple<N, T>& rhs)
    {
        for (unsigned int i = 0; i < N; i++)
            m_data[i] -= rhs.m_data[i];
        return *this;
    }

    /// Multiplies every element in the tuple by a single value.
    Tuple<N, T>& operator*=(const T& rhs)
    {
        for (unsigned int i = 0; i < N; i++)
            m_data[i] *= rhs;
        return *this;
    }

    /// Divides every element in the tuple by a single value.
    Tuple<N, T>& operator/=(const T& rhs)
    {
        for (unsigned int i = 0; i < N; i++)
            m_data[i] /= rhs;
        return *this;
    }

    /// Returns the result of adding another tuple to this tuple.
    const Tuple<N, T> operator+(const Tuple<N, T>& rhs) const
    {
        return Tuple<N, T>(*this) += rhs;
    }

    /// Returns the result of subtracting another tuple from this tuple.
    const Tuple<N, T> operator-(const Tuple<N, T>& rhs) const
    {
        return Tuple<N, T>(*this) -= rhs;
    }

    /// Returns the result of multiplying this tuple by a single value.
    const Tuple<N, T> operator*(const T& rhs) const
    {
        return Tuple<N, T>(*this) *= rhs;
    }

    /// Returns the result of dividing this tuple by a single value.
    const Tuple<N, T> operator/(const T& rhs) const
    {
        return Tuple<N, T>(*this) /= rhs;
    }

    T& operator[](unsigned int i)             { assert(i < N); return m_data[i]; }
    const T& operator[](unsigned int i) const { assert(i < N); return m_data[i]; }

    /// Returns the number of elements in the tuple.
    static unsigned int tupleSize() { return N; }

protected:
    T m_data[N];
};

/**
 * BayerTuple template class. This is a Tuple specialization containing 4 elements corresponding
 * to the Bayer color channels: R, G_EVEN, G_ODD, and B. Values can be accessed using the named
 * methods or subscript indexing using the Argus::BayerChannel enum.
 */
template <typename T>
class BayerTuple : public Tuple<BAYER_CHANNEL_COUNT, T>
{
public:
    BayerTuple() {}
    BayerTuple(const Tuple<BAYER_CHANNEL_COUNT, T>& other) : Tuple<BAYER_CHANNEL_COUNT, T>(other) {}

    BayerTuple(T init)
    {
        r() = gEven() = gOdd() = b() = init;
    }

    BayerTuple(T _r, T _gEven, T _gOdd, T _b)
    {
        r() = _r;
        gEven() = _gEven;
        gOdd() = _gOdd;
        b() = _b;
    }

    T& r()                 { return Tuple<BAYER_CHANNEL_COUNT, T>::m_data[BAYER_CHANNEL_R]; }
    const T& r() const     { return Tuple<BAYER_CHANNEL_COUNT, T>::m_data[BAYER_CHANNEL_R]; }
    T& gEven()             { return Tuple<BAYER_CHANNEL_COUNT, T>::m_data[BAYER_CHANNEL_G_EVEN]; }
    const T& gEven() const { return Tuple<BAYER_CHANNEL_COUNT, T>::m_data[BAYER_CHANNEL_G_EVEN]; }
    T& gOdd()              { return Tuple<BAYER_CHANNEL_COUNT, T>::m_data[BAYER_CHANNEL_G_ODD]; }
    const T& gOdd() const  { return Tuple<BAYER_CHANNEL_COUNT, T>::m_data[BAYER_CHANNEL_G_ODD]; }
    T& b()                 { return Tuple<BAYER_CHANNEL_COUNT, T>::m_data[BAYER_CHANNEL_B]; }
    const T& b() const     { return Tuple<BAYER_CHANNEL_COUNT, T>::m_data[BAYER_CHANNEL_B]; }
};

/**
 * RGBTuple template class. This is a Tuple specialization containing 3 elements corresponding
 * to the RGB color channels: R, G, and B. Values can be accessed using the named methods or
 * subscript indexing using the Argus::RGBChannel enum.
 */
template <typename T>
class RGBTuple : public Tuple<RGB_CHANNEL_COUNT, T>
{
public:
    RGBTuple() {}
    RGBTuple(const Tuple<RGB_CHANNEL_COUNT, T>& other) : Tuple<RGB_CHANNEL_COUNT, T>(other) {}

    RGBTuple(T init)
    {
        r() = g() = b() = init;
    }

    RGBTuple(T _r, T _g, T _b)
    {
        r() = _r;
        g() = _g;
        b() = _b;
    }

    T& r()             { return Tuple<RGB_CHANNEL_COUNT, T>::m_data[RGB_CHANNEL_R]; }
    const T& r() const { return Tuple<RGB_CHANNEL_COUNT, T>::m_data[RGB_CHANNEL_R]; }
    T& g()             { return Tuple<RGB_CHANNEL_COUNT, T>::m_data[RGB_CHANNEL_G]; }
    const T& g() const { return Tuple<RGB_CHANNEL_COUNT, T>::m_data[RGB_CHANNEL_G]; }
    T& b()             { return Tuple<RGB_CHANNEL_COUNT, T>::m_data[RGB_CHANNEL_B]; }
    const T& b() const { return Tuple<RGB_CHANNEL_COUNT, T>::m_data[RGB_CHANNEL_B]; }
};

/**
 * Point2D template class. This is a Tuple specialization containing 2 elements corresponding
 * to the x and y coordinates a 2D point. Values can be accessed using the named methods or
 * subscript indexing using the Argus::Coordinate enum.
 */
template <typename T>
class Point2D : public Tuple<COORDINATE_2D_COUNT, T>
{
public:
    Point2D() {}
    Point2D(const Tuple<COORDINATE_2D_COUNT, T>& other) : Tuple<COORDINATE_2D_COUNT, T>(other) {}

    Point2D(T init)
    {
        x() = y() = init;
    }

    Point2D(T _x, T _y)
    {
        x() = _x;
        y() = _y;
    }

    T& x()             { return Tuple<COORDINATE_2D_COUNT, T>::m_data[COORDINATE_X]; }
    const T& x() const { return Tuple<COORDINATE_2D_COUNT, T>::m_data[COORDINATE_X]; }
    T& y()             { return Tuple<COORDINATE_2D_COUNT, T>::m_data[COORDINATE_Y]; }
    const T& y() const { return Tuple<COORDINATE_2D_COUNT, T>::m_data[COORDINATE_Y]; }
};


/**
 * Point3D template class. This is a Tuple specialization containing 3 elements corresponding
 * to the x, y and z coordinates of a 3D point. Values can be accessed using the named methods or
 * subscript indexing using the Argus::Coordinate enum.
 */
template <typename T>
class Point3D : public Tuple<COORDINATE_3D_COUNT, T>
{
public:
    Point3D() {}
    Point3D(const Tuple<COORDINATE_3D_COUNT, T>& other) : Tuple<COORDINATE_3D_COUNT, T>(other) {}

    Point3D(T init)
    {
        x() = y() = z() = init;
    }

    Point3D(T _x, T _y, T _z)
    {
        x() = _x;
        y() = _y;
        z() = _z;
    }

    T& x()             { return Tuple<COORDINATE_3D_COUNT, T>::m_data[COORDINATE_X]; }
    const T& x() const { return Tuple<COORDINATE_3D_COUNT, T>::m_data[COORDINATE_X]; }
    T& y()             { return Tuple<COORDINATE_3D_COUNT, T>::m_data[COORDINATE_Y]; }
    const T& y() const { return Tuple<COORDINATE_3D_COUNT, T>::m_data[COORDINATE_Y]; }
    T& z()             { return Tuple<COORDINATE_3D_COUNT, T>::m_data[COORDINATE_Z]; }
    const T& z() const { return Tuple<COORDINATE_3D_COUNT, T>::m_data[COORDINATE_Z]; }
};

/**
 * Size2D template class. This is a Tuple specialization containing 2 elements corresponding to the
 * width and height of a 2D size, in that order. Values can be accessed using the named methods.
 */
template <typename T>
class Size2D : public Tuple<2, T>
{
public:
    Size2D() {}
    Size2D(const Tuple<2, T>& other) : Tuple<2, T>(other) {}

    Size2D(T init)
    {
        width() = height() = init;
    }

    Size2D(T _width, T _height)
    {
        width() = _width;
        height() = _height;
    }

    T& width()              { return Tuple<2, T>::m_data[0]; }
    const T& width() const  { return Tuple<2, T>::m_data[0]; }
    T& height()             { return Tuple<2, T>::m_data[1]; }
    const T& height() const { return Tuple<2, T>::m_data[1]; }

    /// Returns the area of the size (width * height).
    T area() const { return width() * height(); }
};

/**
 * Rectangle template class. This is a Tuple specialization containing 4 elements corresponding
 * to the positions of the left, top, right, and bottom edges of a rectangle, in that order.
 * Values can be accessed using the named methods.
 */
template <typename T>
class Rectangle : public Tuple<4, T>
{
public:
    Rectangle() {}
    Rectangle(const Tuple<4, T>& other) : Tuple<4, T>(other) {}

    Rectangle(T init)
    {
        left() = top() = right() = bottom() = init;
    }

    Rectangle(T _left, T _top, T _right, T _bottom)
    {
        left() = _left;
        top() = _top;
        right() = _right;
        bottom() = _bottom;
    }

    T& left()               { return Tuple<4, T>::m_data[0]; }
    const T& left() const   { return Tuple<4, T>::m_data[0]; }
    T& top()                { return Tuple<4, T>::m_data[1]; }
    const T& top() const    { return Tuple<4, T>::m_data[1]; }
    T& right()              { return Tuple<4, T>::m_data[2]; }
    const T& right() const  { return Tuple<4, T>::m_data[2]; }
    T& bottom()             { return Tuple<4, T>::m_data[3]; }
    const T& bottom() const { return Tuple<4, T>::m_data[3]; }

    /// Returns the width of the rectangle.
    T width() const  { return right() - left(); }

    /// Returns the height of the rectangle.
    T height() const { return bottom() - top(); }

    /// Returns the area of the rectangle (width * height).
    T area() const { return width() * height(); }
};

/**
 * Range template class. This is a Tuple specialization containing 2 elements corresponding to the
 * min and max values of the range, in that order. Values can be accessed using the named methods.
 */
template <typename T>
class Range : public Tuple<2, T>
{
public:
    Range() {}
    Range(const Tuple<2, T>& other) : Tuple<2, T>(other) {}

    Range(T init)
    {
        min() = max() = init;
    }

    Range(T _min, T _max)
    {
        min() = _min;
        max() = _max;
    }

    T& min()             { return Tuple<2, T>::m_data[0]; }
    const T& min() const { return Tuple<2, T>::m_data[0]; }
    T& max()             { return Tuple<2, T>::m_data[1]; }
    const T& max() const { return Tuple<2, T>::m_data[1]; }

    bool empty() const   { return max() < min(); }
};

/**
 * Defines an autocontrol region of interest (in pixel space). This region consists of a rectangle
 * (inherited from the Rectangle<uint32_t> Tuple) and a floating point weight value.
 */
class AcRegion : public Rectangle<uint32_t>
{
public:
    AcRegion()
        : Rectangle<uint32_t>(0, 0, 0, 0)
        , m_weight(1.0f)
    {}

    AcRegion(uint32_t _left, uint32_t _top, uint32_t _right, uint32_t _bottom, float _weight)
        : Rectangle<uint32_t>(_left, _top, _right, _bottom)
        , m_weight(_weight)
    {}

    float& weight()             { return m_weight; }
    const float& weight() const { return m_weight; }

protected:
    float m_weight;
};

/**
 * A template class to hold a 2-dimensional array of data.
 * Data in this array is tightly packed in a 1-dimensional vector in row-major order;
 * that is, the vector index for any value given its 2-dimensional location (Point2D) is
 *  index = location.x() + (location.y() * size.x());
 * Indexing operators using iterators, 1-dimensional, or 2-dimensional coordinates are provided.
 */
template <typename T>
class Array2D
{
public:
    // Iterator types.
    typedef T* iterator;
    typedef const T* const_iterator;

    /// Default Constructor.
    Array2D() : m_size(0, 0) {}

    /// Constructor given initial array size.
    Array2D(const Size2D<uint32_t>& size) : m_size(size)
    {
        m_data.resize(size.width() * size.height());
    }

    /// Constructor given initial array size and initial fill value.
    Array2D(const Size2D<uint32_t>& size, const T& value) : m_size(size)
    {
        m_data.resize(size.width() * size.height(), value);
    }

    /// Copy constructor.
    Array2D(const Array2D<T>& other)
    {
        m_data = other.m_data;
        m_size = other.m_size;
    }

    /// Assignment operator.
    Array2D& operator= (const Array2D<T>& other)
    {
        m_data = other.m_data;
        m_size = other.m_size;
        return *this;
    }

    /// Equality operator.
    bool operator== (const Array2D<T>& other) const
    {
        return (m_size == other.m_size && m_data == other.m_data);
    }

    /// Returns the size (dimensions) of the array.
    Size2D<uint32_t> size() const { return m_size; }

    /// Resize the array. Array contents after resize are undefined.
    /// Boolean return value enables error checking when exceptions are not available.
    bool resize(const Size2D<uint32_t>& size)
    {
        uint32_t s = size.width() * size.height();
        m_data.resize(s);
        if (m_data.size() != s)
            return false;
        m_size = size;
        return true;
    }

    /// STL style iterators.
    inline const_iterator begin() const { return m_data.data(); }
    inline const_iterator end() const { return m_data.data() + m_data.size(); }
    inline iterator begin() { return m_data.data(); }
    inline iterator end() { return m_data.data() + m_data.size(); }

    /// Array indexing using [] operator.
    T& operator[](unsigned int i) { return m_data[checkIndex(i)]; }
    const T& operator[](unsigned int i) const { return m_data[checkIndex(i)]; }

    /// Array indexing using () operator.
    inline const T& operator() (uint32_t i) const { return m_data[checkIndex(i)]; }
    inline const T& operator() (uint32_t x, uint32_t y) const { return m_data[checkIndex(x, y)]; }
    inline const T& operator() (const Point2D<uint32_t>& p) const
        { return m_data[checkIndex(p.x(), p.y())]; }
    inline T& operator() (uint32_t i) { return m_data[checkIndex(i)]; }
    inline T& operator() (uint32_t x, uint32_t y) { return m_data[checkIndex(x, y)]; }
    inline T& operator() (const Point2D<uint32_t>& p)
        { return m_data[checkIndex(p.x(), p.y())]; }

    // Get pointers to data.
    inline const T* data() const { return m_data.data(); }
    inline T* data() { return m_data.data(); }

private:
    inline uint32_t checkIndex(uint32_t i) const
    {
        assert(i < m_data.size());
        return i;
    }

    inline uint32_t checkIndex(uint32_t x, uint32_t y) const
    {
        assert(x < m_size.width());
        assert(y < m_size.height());
        return x + (y * m_size.width());
    }

    std::vector<T> m_data;
    Size2D<uint32_t> m_size;
};

typedef uint32_t AutoControlId;

} // namespace Argus

#endif // _ARGUS_TYPES_H
