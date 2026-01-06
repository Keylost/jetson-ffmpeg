/*
 * Copyright (c) 2016-2022, NVIDIA CORPORATION. All rights reserved.
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
 * <b>Libargus API: Request API</b>
 *
 * @b Description: Defines the Request object and core IRequest interface.
 */

#ifndef _ARGUS_REQUEST_H
#define _ARGUS_REQUEST_H

namespace Argus
{

/**
 * Container for all settings used by a single capture request.
 *
 * @defgroup ArgusRequest Request
 * @ingroup ArgusObjects
 */
class Request : public InterfaceProvider, public Destructable
{
protected:
    ~Request() {}
};

/**
 * @class IRequest
 *
 * Interface to the core Request settings.
 *
 * @ingroup ArgusRequest
 *
 * @defgroup ArgusAutoControlSettings AutoControlSettings
 * Child auto control settings, returned by IRequest::getAutoControlSettings
 * @ingroup ArgusRequest
 *
 * @defgroup ArgusStreamSettings StreamSettings
 * Child per-stream settings, returned by IRequest::getStreamSettings
 * @ingroup ArgusRequest
 *
 * @defgroup ArgusSourceSettings SourceSettings
 * Child source settings, returned by IRequest::getSourceSettings
 * @ingroup ArgusRequest
 */
DEFINE_UUID(InterfaceID, IID_REQUEST, eb9b3750,fc8d,455f,8e0f,91,b3,3b,d9,4e,c5);
class IRequest : public Interface
{
public:
    static const InterfaceID& id() { return IID_REQUEST; }

    /**
     * Enables the specified output stream.
     * Captures made with this Request will produce output on that stream.
     */
    virtual Status enableOutputStream(OutputStream* stream) = 0;

    /**
     * Disables the specified output stream.
     */
    virtual Status disableOutputStream(OutputStream* stream) = 0;

    /**
     * Disables all output streams.
     */
    virtual Status clearOutputStreams() = 0;

    /**
     * Returns all enabled output streams.
     * @param[out] streams A vector that will be populated with the enabled streams.
     *
     * @returns success/status of the call.
     */
    virtual Status getOutputStreams(std::vector<OutputStream*>* streams) const = 0;

    /**
     * Returns the Stream settings for a particular stream in the request.
     * The returned object will have the same lifespan as this object,
     * and expose the IStreamSettings interface.
     * @param[in] stream The stream for which the settings are requested.
     */
    virtual InterfaceProvider* getStreamSettings(const OutputStream* stream) = 0;

    /**
     * Returns the capture control settings for a given AC.
     * The returned object will have the same lifespan as this object,
     * and expose the IAutoControlSettings interface.
     * @param[in] acId The id of the AC component for which the settings are requested.
     * <b>(Currently unused)</b>
     */
    virtual InterfaceProvider* getAutoControlSettings(const AutoControlId acId = 0) = 0;

    /**
     * Returns the source settings for the request.
     * The returned object will have the same lifespan as this object,
     * and expose the ISourceSettings interface.
     */
    virtual InterfaceProvider* getSourceSettings() = 0;

    /**
     * Sets the client data for the request. This value is passed through to and queryable
     * from the CaptureMetadata generated for any captures completed using this Request.
     * Default value is 0.
     * @param[in] data The client data.
     */
    virtual Status setClientData(uint32_t data) = 0;

    /**
     * Gets the client data for the request.
     */
    virtual uint32_t getClientData() const = 0;

    /**
     * Set this to false if o/p buffer is Bayer and ISP stage needs to be skipped
     */
    virtual Status setEnableIspStage(bool enableIspStage) = 0;

    /**
     * Check if ISP stage is enabled/disabled.
     */
    virtual bool getEnableIspStage() const = 0;

protected:
    ~IRequest() {}
};

} // namespace Argus

#endif // _ARGUS_REQUEST_H
