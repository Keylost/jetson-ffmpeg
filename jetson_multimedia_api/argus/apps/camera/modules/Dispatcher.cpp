/*
 * Copyright (c) 2016-2019, NVIDIA CORPORATION. All rights reserved.
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
#include <unistd.h>
#include <stdarg.h>
#include <assert.h>

#include <sstream>
#include <limits>

#include "Dispatcher.h"
#include "InitOnce.h"
#include "UniquePointer.h"
#include "Error.h"
#include "Util.h"
#include "Composer.h"
#include "Validator.h"
#include <Argus/Ext/BayerSharpnessMap.h>
#include <Argus/Ext/DebugCaptureSession.h>
#include <Argus/Ext/DeFog.h>
#include <Argus/Ext/FaceDetect.h>
#include <Argus/Ext/InternalFrameCount.h>
#include <Argus/Ext/SensorPrivateMetadata.h>
#include <Argus/Ext/DebugCaptureSession.h>
#include <Argus/Ext/PwlWdrSensorMode.h>
#include <Argus/Ext/DolWdrSensorMode.h>

namespace ArgusSamples
{

/**
 * An observer for an Argus interface.
 */
class IObserverForInterface : public IObserver
{
public:
    virtual ~IObserverForInterface() { };

    /**
     * Check if this is the observer for the given interface.
     *
     * @param interface [in]
     */
    virtual bool isInterface(Argus::Interface *interface) const = 0;
};

/**
 * Denoise settings observer. Update Argus denoise settings when values change.
 */
class DenoiseSettingsObserver : public IObserverForInterface
{
public:
    DenoiseSettingsObserver(Argus::IDenoiseSettings *iDenoiseSettings)
        : m_iDenoiseSettings(iDenoiseSettings)
    {
        Dispatcher &dispatcher = Dispatcher::getInstance();

        PROPAGATE_ERROR_CONTINUE(dispatcher.m_denoiseMode.registerObserver(this,
            static_cast<IObserver::CallbackFunction>(
                &DenoiseSettingsObserver::onDenoiseModeChanged)));
        PROPAGATE_ERROR_CONTINUE(dispatcher.m_denoiseStrength.registerObserver(this,
            static_cast<IObserver::CallbackFunction>(
                &DenoiseSettingsObserver::onDenoiseStrengthChanged)));
    }

    virtual ~DenoiseSettingsObserver()
    {
        Dispatcher &dispatcher = Dispatcher::getInstance();

        PROPAGATE_ERROR_CONTINUE(dispatcher.m_denoiseStrength.unregisterObserver(this,
            static_cast<IObserver::CallbackFunction>(
                &DenoiseSettingsObserver::onDenoiseStrengthChanged)));
        PROPAGATE_ERROR_CONTINUE(dispatcher.m_denoiseMode.unregisterObserver(this,
            static_cast<IObserver::CallbackFunction>(
                &DenoiseSettingsObserver::onDenoiseModeChanged)));
    }

    virtual bool isInterface(Argus::Interface *interface) const
    {
        return (interface == m_iDenoiseSettings);
    }

private:
    bool onDenoiseModeChanged(const Observed &source)
    {
        Dispatcher &dispatcher = Dispatcher::getInstance();

        assert(&source == &dispatcher.m_denoiseMode);

        if (m_iDenoiseSettings->setDenoiseMode(dispatcher.m_denoiseMode.get()) != Argus::STATUS_OK)
            ORIGINATE_ERROR("Failed to set the denoising mode");

        PROPAGATE_ERROR(Dispatcher::getInstance().restartActiveRequests());

        return true;
    }

    bool onDenoiseStrengthChanged(const Observed &source)
    {
        Dispatcher &dispatcher = Dispatcher::getInstance();

        assert(&source == &dispatcher.m_denoiseStrength);

        if (m_iDenoiseSettings->setDenoiseStrength(dispatcher.m_denoiseStrength.get()) !=
            Argus::STATUS_OK)
        {
            ORIGINATE_ERROR("Failed to set the denoise strength");
        }

        PROPAGATE_ERROR(Dispatcher::getInstance().restartActiveRequests());

        return true;
    }

    Argus::IDenoiseSettings *m_iDenoiseSettings;
};

/**
 * Edge enhancement settings observer. Update Argus edge enhance settings when values change.
 */
class EdgeEnhanceSettingsObserver : public IObserverForInterface
{
public:
    EdgeEnhanceSettingsObserver(Argus::IEdgeEnhanceSettings *iEdgeEnhanceSettings)
        : m_iEdgeEnhanceSettings(iEdgeEnhanceSettings)
    {
        Dispatcher &dispatcher = Dispatcher::getInstance();

        PROPAGATE_ERROR_CONTINUE(dispatcher.m_edgeEnhanceMode.registerObserver(this,
            static_cast<IObserver::CallbackFunction>(
                &EdgeEnhanceSettingsObserver::onEdgeEnhanceModeChanged)));
        PROPAGATE_ERROR_CONTINUE(dispatcher.m_edgeEnhanceStrength.registerObserver(this,
            static_cast<IObserver::CallbackFunction>(
                &EdgeEnhanceSettingsObserver::onEdgeEnhanceStrengthChanged)));
    }

    virtual ~EdgeEnhanceSettingsObserver()
    {
        Dispatcher &dispatcher = Dispatcher::getInstance();

        PROPAGATE_ERROR_CONTINUE(dispatcher.m_edgeEnhanceStrength.unregisterObserver(this,
            static_cast<IObserver::CallbackFunction>(
                &EdgeEnhanceSettingsObserver::onEdgeEnhanceStrengthChanged)));
        PROPAGATE_ERROR_CONTINUE(dispatcher.m_edgeEnhanceMode.unregisterObserver(this,
            static_cast<IObserver::CallbackFunction>(
                &EdgeEnhanceSettingsObserver::onEdgeEnhanceModeChanged)));
    }

    virtual bool isInterface(Argus::Interface *interface) const
    {
        return (interface == m_iEdgeEnhanceSettings);
    }

private:
    bool onEdgeEnhanceModeChanged(const Observed &source)
    {
        Dispatcher &dispatcher = Dispatcher::getInstance();

        assert(&source == &dispatcher.m_edgeEnhanceMode);

        if (m_iEdgeEnhanceSettings->setEdgeEnhanceMode(dispatcher.m_edgeEnhanceMode.get())
            != Argus::STATUS_OK)
        {
            ORIGINATE_ERROR("Failed to set the edge enhancement mode");
        }

        PROPAGATE_ERROR(Dispatcher::getInstance().restartActiveRequests());

        return true;
    }

    bool onEdgeEnhanceStrengthChanged(const Observed &source)
    {
        Dispatcher &dispatcher = Dispatcher::getInstance();

        assert(&source == &dispatcher.m_edgeEnhanceStrength);

        if (m_iEdgeEnhanceSettings->setEdgeEnhanceStrength(dispatcher.m_edgeEnhanceStrength.get())
            != Argus::STATUS_OK)
        {
            ORIGINATE_ERROR("Failed to set the edge enhancement strength");
        }

        PROPAGATE_ERROR(Dispatcher::getInstance().restartActiveRequests());

        return true;
    }

    Argus::IEdgeEnhanceSettings *m_iEdgeEnhanceSettings;
};

/**
 * Source settings observer. Update Argus source settings if values which are set through the
 * source settings change.
 */
class SourceSettingsObserver : public IObserverForInterface
{
public:
    SourceSettingsObserver(Argus::ISourceSettings *iSourceSettings)
        : m_iSourceSettings(iSourceSettings)
    {
        Dispatcher &dispatcher = Dispatcher::getInstance();

        PROPAGATE_ERROR_CONTINUE(dispatcher.m_exposureTimeRange.registerObserver(this,
            static_cast<IObserver::CallbackFunction>(
                &SourceSettingsObserver::onExposureTimeRangeChanged)));
        PROPAGATE_ERROR_CONTINUE(dispatcher.m_gainRange.registerObserver(this,
            static_cast<IObserver::CallbackFunction>(
                &SourceSettingsObserver::onGainRangeChanged)));
        PROPAGATE_ERROR_CONTINUE(dispatcher.m_sensorModeIndex.registerObserver(this,
            static_cast<IObserver::CallbackFunction>(
                &SourceSettingsObserver::onSensorModeChanged)));
        PROPAGATE_ERROR_CONTINUE(dispatcher.m_frameRate.registerObserver(this,
            static_cast<IObserver::CallbackFunction>(
                &SourceSettingsObserver::onFrameRateChanged)));
        PROPAGATE_ERROR_CONTINUE(dispatcher.m_focusPosition.registerObserver(this,
            static_cast<IObserver::CallbackFunction>(
                &SourceSettingsObserver::onFocusPositionChanged)));
        PROPAGATE_ERROR_CONTINUE(dispatcher.m_aperturePosition.registerObserver(this,
            static_cast<IObserver::CallbackFunction>(
                &SourceSettingsObserver::onAperturePositionChanged)));
        PROPAGATE_ERROR_CONTINUE(dispatcher.m_apertureFnum.registerObserver(this,
            static_cast<IObserver::CallbackFunction>(
                &SourceSettingsObserver::onApertureFnumChanged)));
        PROPAGATE_ERROR_CONTINUE(dispatcher.m_apertureMotorSpeed.registerObserver(this,
            static_cast<IObserver::CallbackFunction>(
                &SourceSettingsObserver::onApertureMotorSpeedChanged)));
        PROPAGATE_ERROR_CONTINUE(dispatcher.m_captureYuvFormat.registerObserver(this,
            static_cast<IObserver::CallbackFunction>(
                &SourceSettingsObserver::onCaptureYuvFormatChanged)));
    }

    virtual ~SourceSettingsObserver()
    {
        Dispatcher &dispatcher = Dispatcher::getInstance();

        PROPAGATE_ERROR_CONTINUE(dispatcher.m_apertureMotorSpeed.unregisterObserver(this,
            static_cast<IObserver::CallbackFunction>(
                &SourceSettingsObserver::onApertureMotorSpeedChanged)));
        PROPAGATE_ERROR_CONTINUE(dispatcher.m_aperturePosition.unregisterObserver(this,
            static_cast<IObserver::CallbackFunction>(
                &SourceSettingsObserver::onAperturePositionChanged)));
        PROPAGATE_ERROR_CONTINUE(dispatcher.m_apertureFnum.unregisterObserver(this,
            static_cast<IObserver::CallbackFunction>(
                &SourceSettingsObserver::onApertureFnumChanged)));
        PROPAGATE_ERROR_CONTINUE(dispatcher.m_focusPosition.unregisterObserver(this,
            static_cast<IObserver::CallbackFunction>(
                &SourceSettingsObserver::onFocusPositionChanged)));
        PROPAGATE_ERROR_CONTINUE(dispatcher.m_frameRate.unregisterObserver(this,
            static_cast<IObserver::CallbackFunction>(
                &SourceSettingsObserver::onFrameRateChanged)));
        PROPAGATE_ERROR_CONTINUE(dispatcher.m_sensorModeIndex.unregisterObserver(this,
            static_cast<IObserver::CallbackFunction>(
                &SourceSettingsObserver::onSensorModeChanged)));
        PROPAGATE_ERROR_CONTINUE(dispatcher.m_gainRange.unregisterObserver(this,
            static_cast<IObserver::CallbackFunction>(
                &SourceSettingsObserver::onGainRangeChanged)));
        PROPAGATE_ERROR_CONTINUE(dispatcher.m_exposureTimeRange.unregisterObserver(this,
            static_cast<IObserver::CallbackFunction>(
                &SourceSettingsObserver::onExposureTimeRangeChanged)));
        PROPAGATE_ERROR_CONTINUE(dispatcher.m_captureYuvFormat.unregisterObserver(this,
            static_cast<IObserver::CallbackFunction>(
                &SourceSettingsObserver::onCaptureYuvFormatChanged)));
    }

    virtual bool isInterface(Argus::Interface *interface) const
    {
        return (interface == m_iSourceSettings);
    }

private:
    bool onExposureTimeRangeChanged(const Observed &source)
    {
        Dispatcher &dispatcher = Dispatcher::getInstance();

        assert(&source == &dispatcher.m_exposureTimeRange);

        if (m_iSourceSettings->setExposureTimeRange(dispatcher.m_exposureTimeRange.get()) !=
            Argus::STATUS_OK)
        {
            ORIGINATE_ERROR("Failed to set exposure time range");
        }

        PROPAGATE_ERROR(Dispatcher::getInstance().restartActiveRequests());

        return true;
    }

    bool onGainRangeChanged(const Observed &source)
    {
        Dispatcher &dispatcher = Dispatcher::getInstance();

        assert(&source == &dispatcher.m_gainRange);

        if (m_iSourceSettings->setGainRange(dispatcher.m_gainRange.get()) != Argus::STATUS_OK)
            ORIGINATE_ERROR("Failed to set gain range");

        PROPAGATE_ERROR(Dispatcher::getInstance().restartActiveRequests());

        return true;
    }

    bool onSensorModeChanged(const Observed &source)
    {
        Dispatcher &dispatcher = Dispatcher::getInstance();

        assert(&source == &dispatcher.m_sensorModeIndex);

        Argus::SensorMode *sensorMode = NULL;
        PROPAGATE_ERROR(dispatcher.getSensorMode(dispatcher.m_sensorModeIndex.get(), &sensorMode));

        if (m_iSourceSettings->setSensorMode(sensorMode) != Argus::STATUS_OK)
            ORIGINATE_ERROR("Failed to set sensor mode");

        PROPAGATE_ERROR(dispatcher.restartActiveRequests());

        return true;
    }

    bool onCaptureYuvFormatChanged(const Observed & source)
    {
        Dispatcher &dispatcher = Dispatcher::getInstance();

        assert(&source == &dispatcher.m_captureYuvFormat);

        // The Video/Still task will shut down and restart their
        // EGLStreams, causing their underlying buffer pools to be reallocated.
        // So there's not much else to do here.
        PROPAGATE_ERROR(dispatcher.restartActiveRequests());

        return true;
    }

    bool onFocusPositionChanged(const Observed &source)
    {
        Dispatcher &dispatcher = Dispatcher::getInstance();

        assert(&source == &dispatcher.m_focusPosition);

        if (m_iSourceSettings->setFocusPosition(dispatcher.m_focusPosition.get()) !=
            Argus::STATUS_OK)
        {
            ORIGINATE_ERROR("Failed to set focus position");
        }

        PROPAGATE_ERROR(Dispatcher::getInstance().restartActiveRequests());

        return true;
    }

    bool onAperturePositionChanged(const Observed &source)
    {
        Dispatcher &dispatcher = Dispatcher::getInstance();

        assert(&source == &dispatcher.m_aperturePosition);

        if (m_iSourceSettings->setAperturePosition(dispatcher.m_aperturePosition.get()) !=
            Argus::STATUS_OK)
        {
            ORIGINATE_ERROR("Failed to set aperture motor step");
        }

        PROPAGATE_ERROR(Dispatcher::getInstance().restartActiveRequests());

        return true;
    }

    bool onApertureFnumChanged(const Observed &source)
    {
        Dispatcher &dispatcher = Dispatcher::getInstance();

        assert(&source == &dispatcher.m_apertureFnum);

        if (m_iSourceSettings->setApertureFNumber(dispatcher.m_apertureFnum.get()) !=
            Argus::STATUS_OK)
        {
            ORIGINATE_ERROR("Failed to set aperture F-num");
        }

        PROPAGATE_ERROR(Dispatcher::getInstance().restartActiveRequests());

        return true;
    }

    bool onApertureMotorSpeedChanged(const Observed &source)
    {
        Dispatcher &dispatcher = Dispatcher::getInstance();

        assert(&source == &dispatcher.m_apertureMotorSpeed);

        if (m_iSourceSettings->setApertureMotorSpeed(dispatcher.m_apertureMotorSpeed.get()) !=
            Argus::STATUS_OK)
        {
            ORIGINATE_ERROR("Failed to set aperture motor speed");
        }

        PROPAGATE_ERROR(Dispatcher::getInstance().restartActiveRequests());

        return true;
    }

    bool onFrameRateChanged(const Observed &source)
    {
        Dispatcher &dispatcher = Dispatcher::getInstance();

        assert(&source == &dispatcher.m_frameRate);

        Argus::Range<uint64_t> frameDurationRangeNs(0);

        if (dispatcher.m_frameRate.get() == 0.0f)
        {
            // a frame rate of zero means VFR, get the sensor frame duration and apply it to
            // the source
            Argus::SensorMode *sensorMode = NULL;
            PROPAGATE_ERROR(dispatcher.getSensorMode(dispatcher.m_sensorModeIndex.get(),
                &sensorMode));

            Argus::ISensorMode *iSensorMode =
                Argus::interface_cast<Argus::ISensorMode>(sensorMode);

            frameDurationRangeNs = iSensorMode->getFrameDurationRange();
        }
        else
        {
            // frame rate is frames per second, frameduration is in nanoseconds
            frameDurationRangeNs =
                TimeValue::fromCycelsPerSec(dispatcher.m_frameRate.get()).toNSec();
        }

        if (m_iSourceSettings->setFrameDurationRange(frameDurationRangeNs) != Argus::STATUS_OK)
            ORIGINATE_ERROR("Failed to set frame duration range");

        PROPAGATE_ERROR(Dispatcher::getInstance().restartActiveRequests());

        return true;
    }

    Argus::ISourceSettings *m_iSourceSettings;
};

/**
 * Auto control settings observer. Update Argus auto control settings if values which are set
 * through the auto control settings change.
 */
class AutoControlSettingsObserver : public IObserverForInterface
{
public:
    AutoControlSettingsObserver(Argus::IAutoControlSettings *iAutoControlSettings)
        : m_iAutoControlSettings(iAutoControlSettings)
    {
        Dispatcher &dispatcher = Dispatcher::getInstance();

        PROPAGATE_ERROR_CONTINUE(dispatcher.m_aeAntibandingMode.registerObserver(this,
            static_cast<IObserver::CallbackFunction>(
                &AutoControlSettingsObserver::onAeAntibandingModeChanged)));
        PROPAGATE_ERROR_CONTINUE(dispatcher.m_aeLock.registerObserver(this,
            static_cast<IObserver::CallbackFunction>(
                &AutoControlSettingsObserver::onAeLockChanged)));
        PROPAGATE_ERROR_CONTINUE(dispatcher.m_awbLock.registerObserver(this,
            static_cast<IObserver::CallbackFunction>(
                &AutoControlSettingsObserver::onAwbLockChanged)));
        PROPAGATE_ERROR_CONTINUE(dispatcher.m_awbMode.registerObserver(this,
            static_cast<IObserver::CallbackFunction>(
                &AutoControlSettingsObserver::onAwbModeChanged)));
        PROPAGATE_ERROR_CONTINUE(dispatcher.m_exposureCompensation.registerObserver(this,
            static_cast<IObserver::CallbackFunction>(
                &AutoControlSettingsObserver::onExposureCompensationChanged)));
        PROPAGATE_ERROR_CONTINUE(dispatcher.m_ispDigitalGainRange.registerObserver(this,
            static_cast<IObserver::CallbackFunction>(
                &AutoControlSettingsObserver::onIspDigitalGainRangeChanged)));
        PROPAGATE_ERROR_CONTINUE(dispatcher.m_acRegionHorizontal.registerObserver(this,
            static_cast<IObserver::CallbackFunction>(
                &AutoControlSettingsObserver::onAcRegionChanged)));
        PROPAGATE_ERROR_CONTINUE(dispatcher.m_acRegionVertical.registerObserver(this,
            static_cast<IObserver::CallbackFunction>(
                &AutoControlSettingsObserver::onAcRegionChanged)));
    }

    virtual ~AutoControlSettingsObserver()
    {
        Dispatcher &dispatcher = Dispatcher::getInstance();

        PROPAGATE_ERROR_CONTINUE(dispatcher.m_ispDigitalGainRange.unregisterObserver(this,
            static_cast<IObserver::CallbackFunction>(
                &AutoControlSettingsObserver::onIspDigitalGainRangeChanged)));
        PROPAGATE_ERROR_CONTINUE(dispatcher.m_exposureCompensation.unregisterObserver(this,
            static_cast<IObserver::CallbackFunction>(
                &AutoControlSettingsObserver::onExposureCompensationChanged)));
        PROPAGATE_ERROR_CONTINUE(dispatcher.m_awbMode.unregisterObserver(this,
            static_cast<IObserver::CallbackFunction>(
                &AutoControlSettingsObserver::onAwbModeChanged)));
        PROPAGATE_ERROR_CONTINUE(dispatcher.m_awbLock.unregisterObserver(this,
            static_cast<IObserver::CallbackFunction>(
                &AutoControlSettingsObserver::onAwbLockChanged)));
        PROPAGATE_ERROR_CONTINUE(dispatcher.m_aeLock.unregisterObserver(this,
            static_cast<IObserver::CallbackFunction>(
                &AutoControlSettingsObserver::onAeLockChanged)));
        PROPAGATE_ERROR_CONTINUE(dispatcher.m_aeAntibandingMode.unregisterObserver(this,
            static_cast<IObserver::CallbackFunction>(
                &AutoControlSettingsObserver::onAeAntibandingModeChanged)));
        PROPAGATE_ERROR_CONTINUE(dispatcher.m_acRegionHorizontal.unregisterObserver(this,
            static_cast<IObserver::CallbackFunction>(
                &AutoControlSettingsObserver::onAcRegionChanged)));
        PROPAGATE_ERROR_CONTINUE(dispatcher.m_acRegionVertical.unregisterObserver(this,
            static_cast<IObserver::CallbackFunction>(
                &AutoControlSettingsObserver::onAcRegionChanged)));
    }

    virtual bool isInterface(Argus::Interface *interface) const
    {
        return (interface == m_iAutoControlSettings);
    }

private:
    bool onAeAntibandingModeChanged(const Observed &source)
    {
        Dispatcher &dispatcher = Dispatcher::getInstance();

        assert(&source == &dispatcher.m_aeAntibandingMode);

        if (m_iAutoControlSettings->setAeAntibandingMode(dispatcher.m_aeAntibandingMode.get()) !=
            Argus::STATUS_OK)
        {
            ORIGINATE_ERROR("Failed to set the AE antibanding mode");
        }

        PROPAGATE_ERROR(Dispatcher::getInstance().restartActiveRequests());

        return true;
    }

    bool onAeLockChanged(const Observed &source)
    {
        Dispatcher &dispatcher = Dispatcher::getInstance();

        assert(&source == &dispatcher.m_aeLock);

        if (m_iAutoControlSettings->setAeLock(dispatcher.m_aeLock.get()) != Argus::STATUS_OK)
            ORIGINATE_ERROR("Failed to set the AE lock");

        PROPAGATE_ERROR(Dispatcher::getInstance().restartActiveRequests());

        return true;
    }

    bool onAwbLockChanged(const Observed &source)
    {
        Dispatcher &dispatcher = Dispatcher::getInstance();

        assert(&source == &dispatcher.m_awbLock);

        if (m_iAutoControlSettings->setAwbLock(dispatcher.m_awbLock.get()) != Argus::STATUS_OK)
            ORIGINATE_ERROR("Failed to set the AWB lock");

        PROPAGATE_ERROR(Dispatcher::getInstance().restartActiveRequests());

        return true;
    }

    bool onAwbModeChanged(const Observed &source)
    {
        Dispatcher &dispatcher = Dispatcher::getInstance();

        assert(&source == &dispatcher.m_awbMode);

        if (m_iAutoControlSettings->setAwbMode(dispatcher.m_awbMode.get()) != Argus::STATUS_OK)
            ORIGINATE_ERROR("Failed to set the AWB mode");

        PROPAGATE_ERROR(Dispatcher::getInstance().restartActiveRequests());

        return true;
    }

    bool onExposureCompensationChanged(const Observed &source)
    {
        Dispatcher &dispatcher = Dispatcher::getInstance();

        assert(&source == &dispatcher.m_exposureCompensation);

        if (m_iAutoControlSettings->setExposureCompensation(
            dispatcher.m_exposureCompensation.get()) != Argus::STATUS_OK)
        {
            ORIGINATE_ERROR("Failed to set the exposure compensation");
        }

        PROPAGATE_ERROR(Dispatcher::getInstance().restartActiveRequests());

        return true;
    }

    bool onIspDigitalGainRangeChanged(const Observed &source)
    {
        Dispatcher &dispatcher = Dispatcher::getInstance();

        assert(&source == &dispatcher.m_ispDigitalGainRange);

        if (m_iAutoControlSettings->setIspDigitalGainRange(
            dispatcher.m_ispDigitalGainRange.get()) != Argus::STATUS_OK)
        {
            ORIGINATE_ERROR("Failed to set the Isp Digital Gain Range");
        }
        PROPAGATE_ERROR(Dispatcher::getInstance().restartActiveRequests());

        return true;
    }

    bool onAcRegionChanged(const Observed &source)
    {
        Dispatcher &dispatcher = Dispatcher::getInstance();

        assert((&source == &dispatcher.m_acRegionHorizontal) ||
                (&source == &dispatcher.m_acRegionVertical));

        Argus::Range<uint32_t> horizontal = dispatcher.m_acRegionHorizontal.get();
        Argus::Range<uint32_t> vertical = dispatcher.m_acRegionVertical.get();

        if ((horizontal.min() < horizontal.max()) &&
                vertical.min() < vertical.max())
        {
            // set bayerHistogram
            Argus::Rectangle<uint32_t> histRegion(horizontal.min(), vertical.min(),
                                                    horizontal.max(), vertical.max());

            if (m_iAutoControlSettings->setBayerHistogramRegion(histRegion) != Argus::STATUS_OK)
            {
                ORIGINATE_ERROR("Failed to set the bayer histogram region");
            }

            // set AF
            std::vector<Argus::AcRegion> afRegions;
            Argus::AcRegion oneRegion(horizontal.min(), vertical.min(), horizontal.max(),
                                        vertical.max(), 1.0f);
            afRegions.push_back(oneRegion);

            if (m_iAutoControlSettings->setAfRegions(afRegions) != Argus::STATUS_OK)
            {
                ORIGINATE_ERROR("Failed to set the af region");
            }

            PROPAGATE_ERROR(Dispatcher::getInstance().restartActiveRequests());
        }

        return true;
    }

    Argus::IAutoControlSettings *m_iAutoControlSettings;
};

/**
 * DeFog settings observer. Update Argus DeFog settings if values which are set through the
 * DeFog settings change.
 */
class DeFogSettingsObserver : public IObserverForInterface
{
public:
    DeFogSettingsObserver(Argus::Ext::IDeFogSettings *iDeFogSettings)
        : m_iDeFogSettings(iDeFogSettings)
    {
        Dispatcher &dispatcher = Dispatcher::getInstance();

        PROPAGATE_ERROR_CONTINUE(dispatcher.m_deFogEnable.registerObserver(this,
            static_cast<IObserver::CallbackFunction>(&DeFogSettingsObserver::onDeFogEnableChanged)));
        PROPAGATE_ERROR_CONTINUE(dispatcher.m_deFogAmount.registerObserver(this,
            static_cast<IObserver::CallbackFunction>(&DeFogSettingsObserver::onDeFogAmountChanged)));
        PROPAGATE_ERROR_CONTINUE(dispatcher.m_deFogQuality.registerObserver(this,
            static_cast<IObserver::CallbackFunction>(&DeFogSettingsObserver::onDeFogQualityChanged)));
    }

    virtual ~DeFogSettingsObserver()
    {
        Dispatcher &dispatcher = Dispatcher::getInstance();

        PROPAGATE_ERROR_CONTINUE(dispatcher.m_deFogQuality.unregisterObserver(this,
            static_cast<IObserver::CallbackFunction>(&DeFogSettingsObserver::onDeFogQualityChanged)));
        PROPAGATE_ERROR_CONTINUE(dispatcher.m_deFogAmount.unregisterObserver(this,
            static_cast<IObserver::CallbackFunction>(&DeFogSettingsObserver::onDeFogAmountChanged)));
        PROPAGATE_ERROR_CONTINUE(dispatcher.m_deFogEnable.unregisterObserver(this,
            static_cast<IObserver::CallbackFunction>(&DeFogSettingsObserver::onDeFogEnableChanged)));
    }

    virtual bool isInterface(Argus::Interface *interface) const
    {
        return (interface == m_iDeFogSettings);
    }

private:
    bool onDeFogEnableChanged(const Observed &source)
    {
        Dispatcher &dispatcher = Dispatcher::getInstance();

        assert(&source == &dispatcher.m_deFogEnable);

        m_iDeFogSettings->setDeFogEnable(dispatcher.m_deFogEnable.get());

        PROPAGATE_ERROR(Dispatcher::getInstance().restartActiveRequests());

        return true;
    }

    bool onDeFogAmountChanged(const Observed &source)
    {
        Dispatcher &dispatcher = Dispatcher::getInstance();

        assert(&source == &dispatcher.m_deFogAmount);

        if (m_iDeFogSettings->setDeFogAmount(dispatcher.m_deFogAmount.get()) != Argus::STATUS_OK)
            ORIGINATE_ERROR("Failed to set the DeFog amount");

        PROPAGATE_ERROR(Dispatcher::getInstance().restartActiveRequests());

        return true;
    }

    bool onDeFogQualityChanged(const Observed &source)
    {
        Dispatcher &dispatcher = Dispatcher::getInstance();

        assert(&source == &dispatcher.m_deFogQuality);

        if (m_iDeFogSettings->setDeFogQuality(dispatcher.m_deFogQuality.get()) != Argus::STATUS_OK)
            ORIGINATE_ERROR("Failed to set the DeFog quality");

        PROPAGATE_ERROR(Dispatcher::getInstance().restartActiveRequests());

        return true;
    }

    Argus::Ext::IDeFogSettings *m_iDeFogSettings;
};

// valid YUV pixel formats
static const ValidatorEnum<Argus::PixelFormat>::ValueStringPair s_captureYuvFormatTypes[] =
{
    { Argus::PIXEL_FMT_YCbCr_420_888, "nv12" },
    { Argus::PIXEL_FMT_P016, "p016" }
};

// valid denoise modes
static const ValidatorEnum<Argus::DenoiseMode>::ValueStringPair s_denoiseModes[] =
{
    { Argus::DENOISE_MODE_OFF, "off" },
    { Argus::DENOISE_MODE_FAST, "fast" },
    { Argus::DENOISE_MODE_HIGH_QUALITY, "highquality" }
};

// valid edge enhance modes
static const ValidatorEnum<Argus::EdgeEnhanceMode>::ValueStringPair s_edgeEnhanceModes[] =
{
    { Argus::EDGE_ENHANCE_MODE_OFF, "off" },
    { Argus::EDGE_ENHANCE_MODE_FAST, "fast" },
    { Argus::EDGE_ENHANCE_MODE_HIGH_QUALITY, "highquality" }
};

// valid AE antibanding modes
static const ValidatorEnum<Argus::AeAntibandingMode>::ValueStringPair s_aeAntibandingModes[] =
{
    { Argus::AE_ANTIBANDING_MODE_OFF, "off" },
    { Argus::AE_ANTIBANDING_MODE_AUTO, "auto" },
    { Argus::AE_ANTIBANDING_MODE_50HZ, "50hz" },
    { Argus::AE_ANTIBANDING_MODE_60HZ, "60hz" }
};

// valid AWB modes
static const ValidatorEnum<Argus::AwbMode>::ValueStringPair s_awbModes[] =
{
    { Argus::AWB_MODE_OFF, "off" },
    { Argus::AWB_MODE_AUTO, "auto" },
    { Argus::AWB_MODE_INCANDESCENT, "incandescent" },
    { Argus::AWB_MODE_FLUORESCENT, "fluorescent" },
    { Argus::AWB_MODE_WARM_FLUORESCENT, "warmfluorescent" },
    { Argus::AWB_MODE_DAYLIGHT, "daylight" },
    { Argus::AWB_MODE_CLOUDY_DAYLIGHT, "cloudydaylight" },
    { Argus::AWB_MODE_TWILIGHT, "twilight" },
    { Argus::AWB_MODE_SHADE, "shade" },
    { Argus::AWB_MODE_MANUAL, "manual" }
};

// valid still file formats
static const ValidatorEnum<ArgusSamples::StillFileType>::ValueStringPair s_stillFileTypes[] =
{
    { ArgusSamples::STILL_FILE_TYPE_JPG, "jpg" },
    { ArgusSamples::STILL_FILE_TYPE_HEADERLESS, "headerless" }
};

// valid video formats
static const ValidatorEnum<VideoPipeline::VideoFormat>::ValueStringPair s_videoFormats[] =
{
    { VideoPipeline::VIDEO_FORMAT_H264, "h264" },
    { VideoPipeline::VIDEO_FORMAT_H265, "h265" },
    { VideoPipeline::VIDEO_FORMAT_VP8, "vp8" },
    { VideoPipeline::VIDEO_FORMAT_VP9, "vp9" }
};

// valid video file types
static const ValidatorEnum<VideoPipeline::VideoFileType>::ValueStringPair s_videoFileTypes[] =
{
    { VideoPipeline::VIDEO_FILE_TYPE_MP4, "mp4" },
    { VideoPipeline::VIDEO_FILE_TYPE_3GP, "3gp" },
    { VideoPipeline::VIDEO_FILE_TYPE_AVI, "avi" },
    { VideoPipeline::VIDEO_FILE_TYPE_MKV, "mkv" },
    { VideoPipeline::VIDEO_FILE_TYPE_H265, "h265" }
};

static const Argus::Size2D<uint32_t> s_outputSizes[] =
{
    Argus::Size2D<uint32_t>(0, 0),          // if size is 0,0 take the current sensor size
    Argus::Size2D<uint32_t>(176, 144),      // QCIF
    Argus::Size2D<uint32_t>(320, 240),
    Argus::Size2D<uint32_t>(640, 480),
    Argus::Size2D<uint32_t>(1280, 720),     // 720p  HDTV
    Argus::Size2D<uint32_t>(1920, 1080),    // 1080p HDTV
    Argus::Size2D<uint32_t>(3840, 2160),    // 2160p 4K UHDTV
};

Dispatcher::Dispatcher()
    : m_deviceFocusPositionRange(0)
    , m_deviceAperturePositionRange(0)
    , m_deviceApertureMotorSpeedRange(1.0f)
    , m_deviceExposureCompensationRange(0.0f)
    , m_deviceIspDigitalGainRange(Argus::Range<float>(0.0f))
    , m_sensorExposureTimeRange(Argus::Range<uint64_t>(0))
    , m_sensorAnalogGainRange(Argus::Range<float>(0.0f))
    , m_sensorFrameRateRange(0.0f)
    , m_deviceIndex(new ValidatorStdVector<uint32_t, Argus::CameraDevice*>(&m_cameraDevices), 0)
    , m_deviceOpen(false)
    , m_sensorModeValid(false)
    , m_verbose(false)
    , m_kpi(false)
    , m_exposureTimeRange(new ValidatorRange<Argus::Range<uint64_t> >(&m_sensorExposureTimeRange),
        Argus::Range<uint64_t>(0))
    , m_gainRange(new ValidatorRange<Argus::Range<float > >(&m_sensorAnalogGainRange),
        Argus::Range<float>(0.0f))
    , m_sensorModeIndex(new ValidatorEnum<uint32_t>(), 0)
    , m_frameRate(new ValidatorRange<float>(&m_sensorFrameRateRange), 0.0f)
    , m_focusPosition(new ValidatorRange<int32_t>(&m_deviceFocusPositionRange), 0)
    , m_aperturePosition(new ValidatorRange<int32_t>(&m_deviceAperturePositionRange), 0)
    , m_apertureFnum(new ValidatorEnum<float>(), 0.0f)
    , m_apertureMotorSpeed(new ValidatorRange<float>(&m_deviceApertureMotorSpeedRange), 1.0f)
    , m_captureYuvFormat(new ValidatorEnum<Argus::PixelFormat>(
        s_captureYuvFormatTypes,
        sizeof(s_captureYuvFormatTypes) / sizeof(s_captureYuvFormatTypes[0])),
        Argus::PIXEL_FMT_YCbCr_420_888)
    , m_denoiseMode(new ValidatorEnum<Argus::DenoiseMode>(
        s_denoiseModes, sizeof(s_denoiseModes) / sizeof(s_denoiseModes[0])),
        Argus::DENOISE_MODE_FAST)
    , m_denoiseStrength(new ValidatorRange<float>(-1.0f, 1.0f), -1.0f)
    , m_edgeEnhanceMode(new ValidatorEnum<Argus::EdgeEnhanceMode>(
        s_edgeEnhanceModes, sizeof(s_edgeEnhanceModes) / sizeof(s_edgeEnhanceModes[0])),
        Argus::EDGE_ENHANCE_MODE_FAST)
    , m_edgeEnhanceStrength(new ValidatorRange<float>(-1.0f, 1.0f), -1.0f)
    , m_aeAntibandingMode(new ValidatorEnum<Argus::AeAntibandingMode>(
        s_aeAntibandingModes, sizeof(s_aeAntibandingModes) / sizeof(s_aeAntibandingModes[0])),
        Argus::AE_ANTIBANDING_MODE_AUTO)
    , m_aeLock(false)
    , m_awbLock(false)
    , m_awbMode(new ValidatorEnum<Argus::AwbMode>(
        s_awbModes, sizeof(s_awbModes) / sizeof(s_awbModes[0])),
        Argus::AWB_MODE_AUTO)
    , m_exposureCompensation(new ValidatorRange<float>(&m_deviceExposureCompensationRange), 0.0f)
    , m_ispDigitalGainRange(new ValidatorRange<Argus::Range<float> >(&m_deviceIspDigitalGainRange),
        Argus::Range<float>(1.0f))
    , m_acRegionHorizontal(Argus::Range<uint32_t>(0))
    , m_acRegionVertical(Argus::Range<uint32_t>(0))
    , m_stillFileType(new ValidatorEnum<StillFileType>(
        s_stillFileTypes, sizeof(s_stillFileTypes) / sizeof(s_stillFileTypes[0])),
        STILL_FILE_TYPE_JPG)
    , m_videoFormat(new ValidatorEnum<VideoPipeline::VideoFormat>(
        s_videoFormats, sizeof(s_videoFormats) / sizeof(s_videoFormats[0])),
        VideoPipeline::VIDEO_FORMAT_H265)
    , m_videoFileType(new ValidatorEnum<VideoPipeline::VideoFileType>(
        s_videoFileTypes, sizeof(s_videoFileTypes) / sizeof(s_videoFileTypes[0])),
        VideoPipeline::VIDEO_FILE_TYPE_MKV)
    , m_videoBitRate(new ValidatorRange<uint32_t>(0, VideoPipeline::VIDEO_BITRATE_MAX),0)
    , m_outputSize(new ValidatorSize2D<uint32_t>(s_outputSizes,
        sizeof(s_outputSizes) / sizeof(s_outputSizes[0]), true /*allowArbitrarySizes*/),
        Argus::Size2D<uint32_t>(0, 0))
    , m_outputPath(".")
    , m_deFogEnable(false)
    , m_deFogAmount(new ValidatorRange<float>(0.0f, 1.0f), 0.9f)
    , m_deFogQuality(new ValidatorRange<float>(0.0f, 1.0f), 0.14285f)
    , m_initialized(false)
    , m_iCameraProvider(NULL)
{
    PROPAGATE_ERROR_CONTINUE(initialize());
}

Dispatcher::~Dispatcher()
{
    if (!shutdown())
        REPORT_ERROR("Failed to shutdown");
}

Dispatcher &Dispatcher::getInstance()
{
    static InitOnce initOnce;
    static Dispatcher instance;

    if (initOnce.begin())
    {
        if (instance.initialize())
        {
            initOnce.complete();
        }
        else
        {
            initOnce.failed();
            REPORT_ERROR("Initalization failed");
        }
    }

    return instance;
}

bool Dispatcher::initialize()
{
    if (m_initialized)
        return true;

    // Create the CameraProvider object and obtain its interface.
    m_cameraProvider = Argus::UniqueObj<Argus::CameraProvider>(Argus::CameraProvider::create());
    m_iCameraProvider = Argus::interface_cast<Argus::ICameraProvider>(m_cameraProvider);
    if (!m_iCameraProvider)
        ORIGINATE_ERROR("Failed to create CameraProvider");
    printf("Argus Version: %s\n", m_iCameraProvider->getVersion().c_str());

    // Get the camera devices
    m_iCameraProvider->getCameraDevices(&m_cameraDevices);
    if (m_cameraDevices.size() == 0)
    {
        PROPAGATE_ERROR(shutdown());
        ORIGINATE_ERROR("No cameras available");
    }

    m_initialized = true;

    // register the device index observer after 'm_initialize' is set, the call back will be
    // called immediately and assert that 'm_initialize' is set
    PROPAGATE_ERROR_CONTINUE(m_deviceIndex.registerObserver(this,
        static_cast<IObserver::CallbackFunction>(&Dispatcher::onDeviceIndexChanged)));
    PROPAGATE_ERROR_CONTINUE(m_sensorModeIndex.registerObserver(this,
        static_cast<IObserver::CallbackFunction>(&Dispatcher::onSensorModeIndexChanged)));

    return true;
}

bool Dispatcher::shutdown()
{
    if (m_initialized)
    {
        m_initialized = false;
        // unregister the device index observer in reverse order
        PROPAGATE_ERROR_CONTINUE(m_sensorModeIndex.unregisterObserver(this,
            static_cast<IObserver::CallbackFunction>(&Dispatcher::onSensorModeIndexChanged)));
        PROPAGATE_ERROR_CONTINUE(m_deviceIndex.unregisterObserver(this,
            static_cast<IObserver::CallbackFunction>(&Dispatcher::onDeviceIndexChanged)));

        PROPAGATE_ERROR_CONTINUE(closeSession());

        m_cameraDevices.clear();
        m_cameraProvider.reset();
    }

    return true;
}

bool Dispatcher::onDeviceIndexChanged(const Observed &source)
{
    assert(static_cast<const Value<uint32_t>&>(source).get() == m_deviceIndex);
    assert(m_initialized);

    // close the currently open device
    if (m_deviceOpen)
    {
        PROPAGATE_ERROR(m_deviceOpen.set(false));

        PROPAGATE_ERROR(closeSession());

        // reset the current device properties
    }

    // open the new device
    const Argus::ICameraProperties *iCameraProperties =
        Argus::interface_cast<Argus::ICameraProperties>(m_cameraDevices[m_deviceIndex]);
    if (!iCameraProperties)
        ORIGINATE_ERROR("Failed to get ICameraProperties interface");

    // get the sensor modes
    if (iCameraProperties->getAllSensorModes(&m_sensorModes) != Argus::STATUS_OK)
        ORIGINATE_ERROR("Failed to get sensor modes");

    if (m_sensorModes.size() == 0)
        ORIGINATE_ERROR("No sensor modes found");

    // get the focus position range
    PROPAGATE_ERROR(m_deviceFocusPositionRange.set(iCameraProperties->getFocusPositionRange()));

    // get the aperture position range
    PROPAGATE_ERROR(m_deviceAperturePositionRange.set(iCameraProperties->getAperturePositionRange()));

    // get the aperture Fnum available values
    if(iCameraProperties->getAvailableApertureFNumbers(&m_deviceApertureFnums))
        ORIGINATE_ERROR("Failed to get Aperture Fnum");

    ValidatorEnum<float>* apertureFnumValidator =
        static_cast<ValidatorEnum<float>*>(m_apertureFnum.getValidator());
    apertureFnumValidator->setValidValues(m_deviceApertureFnums);
    // update the valid aperture F-num
    PROPAGATE_ERROR(m_apertureFnum.set(m_deviceApertureFnums.front(), true /*forceNotify*/));

    // get the aperture motor speed range
    PROPAGATE_ERROR(m_deviceApertureMotorSpeedRange.set(iCameraProperties->getApertureMotorSpeedRange()));

    // get new limits
    Argus::Range<float> digitalGainRange = iCameraProperties->getIspDigitalGainRange();
    Argus::Range<float> deviceExposureCompensationRange =
        iCameraProperties->getExposureCompensationRange();

    /* set ranges to unified range (to avoid errors when setting new values)
     * Eg. Say Device 1 has range [-1,0] and Device 2 has range [1,2] .If current value is -1 and
     * range is [-1,0] , setting the range to [1,2] would give error . Similarly, if current value
     * is 1 , setting the range to [-1,0] would give error. Thus we set range to [-1,2] , set value
     * to 1 and then set range to [1,2] to avoid errors.
     */
    Argus::Range<float> unifiedDigitalGainRange(0);
    unifiedDigitalGainRange.min() =
        std::min(m_deviceIspDigitalGainRange.get().min().min(), digitalGainRange.min());
    unifiedDigitalGainRange.max() =
        std::max(m_deviceIspDigitalGainRange.get().max().max(), digitalGainRange.max());

    Argus::Range<float> unifiedExposureCompensationRange(0);
    unifiedExposureCompensationRange.min() =
        std::min(m_deviceExposureCompensationRange.get().min(),
            deviceExposureCompensationRange.min());
    unifiedExposureCompensationRange.max() =
        std::max(m_deviceExposureCompensationRange.get().max(),
            deviceExposureCompensationRange.max());

    PROPAGATE_ERROR(m_deviceIspDigitalGainRange.set(
        Argus::Range<Argus::Range<float> >(unifiedDigitalGainRange)));
    PROPAGATE_ERROR(m_deviceExposureCompensationRange.set(
        Argus::Range<float> (unifiedExposureCompensationRange)));

    // update dependent values
    PROPAGATE_ERROR(m_ispDigitalGainRange.set(digitalGainRange));
    PROPAGATE_ERROR(m_exposureCompensation.set(0.0f));

    // set to final range
    PROPAGATE_ERROR(m_deviceIspDigitalGainRange.set(Argus::Range<Argus::Range<float> >(
        digitalGainRange, digitalGainRange)));
    PROPAGATE_ERROR(m_deviceExposureCompensationRange.set(Argus::Range<float> (
       deviceExposureCompensationRange)));

    // add value/string pairs for each sensor mode index
    std::vector<ValidatorEnum<uint32_t>::ValueStringPair> valueStringPairs;
    valueStringPairs.resize(m_sensorModes.size());
    for (size_t index = 0; index < m_sensorModes.size(); ++index)
    {
        Argus::ISensorMode *sensorMode =
            Argus::interface_cast<Argus::ISensorMode>(m_sensorModes[index]);

        valueStringPairs[index].value = (uint32_t)index;

        std::ostringstream stream;
        stream << index << ": "
            << sensorMode->getResolution().width() << "x" << sensorMode->getResolution().height();

        Argus::Ext::IPwlWdrSensorMode* pwlMode =
            Argus::interface_cast<Argus::Ext::IPwlWdrSensorMode>(m_sensorModes[index]);

        Argus::Ext::IDolWdrSensorMode* dolMode =
            Argus::interface_cast<Argus::Ext::IDolWdrSensorMode>(m_sensorModes[index]);
        if (pwlMode)
        {
            stream << " @" << sensorMode->getInputBitDepth() << "bpp -> " <<
                sensorMode->getOutputBitDepth() << "bpp";
        }
        else if (dolMode)
        {
            stream << " @" << sensorMode->getOutputBitDepth() << "bpp -> " <<
                    dolMode->getExposureCount() << " exposure" << " DOL WDR";
        }
        else
        {
            stream << " @" << sensorMode->getOutputBitDepth() << "bpp";
        }

        valueStringPairs[index].string = stream.str();
    }
    // update the validator with the new value/string pairs
    ValidatorEnum<uint32_t> *validator =
        static_cast<ValidatorEnum<uint32_t>*>(m_sensorModeIndex.getValidator());
    PROPAGATE_ERROR(validator->setValidValues(valueStringPairs.data(), valueStringPairs.size()));

    // set the sensor mode index (force notify observer because the sensor modes are different now
    // although the sensor mode index could be the same)
    PROPAGATE_ERROR(m_sensorModeIndex.set(0, true /*forceNotify*/));

    PROPAGATE_ERROR(m_deviceOpen.set(true));

    return true;
}

bool Dispatcher::onSensorModeIndexChanged(const Observed &source)
{
    m_sensorModeValid.set(false);
    assert(static_cast<const Value<uint32_t>&>(source).get() == m_sensorModeIndex);
    assert(m_initialized);

    Argus::ISensorMode *iSensorMode =
        Argus::interface_cast<Argus::ISensorMode>(m_sensorModes[m_sensorModeIndex.get()]);
    if (!iSensorMode)
        ORIGINATE_ERROR("Failed to get ISensorMode interface");

    // get new limits
    Argus::Range<uint64_t> sensorExposureTimeRange = iSensorMode->getExposureTimeRange();
    Argus::Range<float> sensorAnalogGainRange = iSensorMode->getAnalogGainRange();
    Argus::Range<TimeValue> sensorFrameDurationRange(
        TimeValue::fromNSec(iSensorMode->getFrameDurationRange().min()),
        TimeValue::fromNSec(iSensorMode->getFrameDurationRange().max()));
    Argus::Range<float> sensorFrameRateRange(
        sensorFrameDurationRange.max().toCyclesPerSec(),
        sensorFrameDurationRange.min().toCyclesPerSec());

    // set ranges to unified range (to avoid errors when setting new values)
    Argus::Range<uint64_t> unifiedSensorExposureTimeRange(0);
    unifiedSensorExposureTimeRange.min() =
        std::min(m_sensorExposureTimeRange.get().min().min(), sensorExposureTimeRange.min());
    unifiedSensorExposureTimeRange.max() =
        std::max(m_sensorExposureTimeRange.get().max().max(), sensorExposureTimeRange.max());
    Argus::Range<float> unifiedSensorAnalogGainRange(0);
    unifiedSensorAnalogGainRange.min() =
        std::min(m_sensorAnalogGainRange.get().min().min(), sensorAnalogGainRange.min());
    unifiedSensorAnalogGainRange.max() =
        std::max(m_sensorAnalogGainRange.get().max().max(), sensorAnalogGainRange.max());
    Argus::Range<float> unifiedSensorFrameRateRange(0.0f);
    unifiedSensorFrameRateRange.min() =
        std::min(m_sensorFrameRateRange.get().min(), sensorFrameRateRange.min());
    unifiedSensorFrameRateRange.max() =
        std::max(m_sensorFrameRateRange.get().max(), sensorFrameRateRange.max());

    PROPAGATE_ERROR(m_sensorExposureTimeRange.set(
        Argus::Range<Argus::Range<uint64_t> >(unifiedSensorExposureTimeRange)));
    PROPAGATE_ERROR(m_sensorAnalogGainRange.set(
        Argus::Range<Argus::Range<float> >(unifiedSensorAnalogGainRange)));
    PROPAGATE_ERROR(m_sensorFrameRateRange.set(unifiedSensorFrameRateRange));

    // update dependent values
    PROPAGATE_ERROR(m_exposureTimeRange.set(sensorExposureTimeRange));
    PROPAGATE_ERROR(m_gainRange.set(sensorAnalogGainRange));
    PROPAGATE_ERROR(m_frameRate.set(sensorFrameRateRange.max()));

    // set to final ranges
    PROPAGATE_ERROR(m_sensorExposureTimeRange.set(Argus::Range<Argus::Range<uint64_t> >(
        sensorExposureTimeRange, sensorExposureTimeRange)));
    PROPAGATE_ERROR(m_sensorAnalogGainRange.set(Argus::Range<Argus::Range<float> >(
        sensorAnalogGainRange, sensorAnalogGainRange)));
    PROPAGATE_ERROR(m_sensorFrameRateRange.set(sensorFrameRateRange));
    m_sensorModeValid.set(true);

    return true;
}

bool Dispatcher::supportsExtension(const Argus::ExtensionName& extension) const
{
    return m_iCameraProvider->supportsExtension(extension);
}

bool Dispatcher::getInfo(std::string &info) const
{
    std::ostringstream stream;

    assert(m_initialized);

    stream << "Argus extensions:" << std::endl;
    stream << "  BayerSharpnessMap: " <<
        (supportsExtension(Argus::EXT_BAYER_SHARPNESS_MAP) ?
            "supported" : "not supported") << std::endl;
    stream << "  DebugCaptureSession: " <<
        (supportsExtension(Argus::EXT_DEBUG_CAPTURE_SESSION) ?
            "supported" : "not supported") << std::endl;
    stream << "  DeFog: " <<
        (supportsExtension(Argus::EXT_DE_FOG) ?
            "supported" : "not supported") << std::endl;
    stream << "  FaceDetect: " <<
        (supportsExtension(Argus::EXT_FACE_DETECT) ?
             "supported" : "not supported") << std::endl;
    stream << "  InternalFrameCount: " <<
        (supportsExtension(Argus::EXT_INTERNAL_FRAME_COUNT) ?
            "supported" : "not supported") << std::endl;
    stream << "  SensorPrivateMetadata: " <<
        (supportsExtension(Argus::EXT_SENSOR_PRIVATE_METADATA) ?
            "supported" : "not supported") << std::endl;

    stream << "Number of camera devices: " << m_cameraDevices.size() << std::endl;

    for (uint32_t deviceIndex = 0; deviceIndex < m_cameraDevices.size(); ++deviceIndex)
    {
        stream << "Device: " << deviceIndex << std::endl;

        const Argus::ICameraProperties *iCameraProperties =
            Argus::interface_cast<Argus::ICameraProperties>(m_cameraDevices[deviceIndex]);
        if (!iCameraProperties)
            ORIGINATE_ERROR("Failed to get ICameraProperties interface");

        stream << "  Max AE Regions: " <<
            iCameraProperties->getMaxAeRegions() << std::endl;
        stream << "  Max AWB Regions: " <<
            iCameraProperties->getMaxAwbRegions() << std::endl;
        stream << "  Focus Position Range: " <<
            iCameraProperties->getFocusPositionRange().min() << " - " <<
            iCameraProperties->getFocusPositionRange().max() << std::endl;
        stream << "  Aperture Position Range: " <<
            iCameraProperties->getAperturePositionRange().min() << " - " <<
            iCameraProperties->getAperturePositionRange().max() << std::endl;
        stream << "  Aperture Motor Speed Range: " <<
            iCameraProperties->getApertureMotorSpeedRange().min() << " - " <<
            iCameraProperties->getApertureMotorSpeedRange().max() << std::endl;
        stream << "  Lens Aperture Available values: ";
        std::vector<float> availableFnums;
        iCameraProperties->getAvailableApertureFNumbers(&availableFnums);
        for (std::vector<float>::iterator it = availableFnums.begin();
             it != availableFnums.end(); ++it)
        {
            stream << *it << ", ";
        }
        stream << std::endl;

        stream << "  Isp Digital Gain Range: " <<
            iCameraProperties->getIspDigitalGainRange().min() << " - " <<
            iCameraProperties->getIspDigitalGainRange().max() << std::endl;

        // print the sensor modes
        std::vector<Argus::SensorMode*> sensorModes;
        iCameraProperties->getAllSensorModes(&sensorModes);
        stream << "  Number of sensor modes: " << sensorModes.size() << std::endl;
        for (uint32_t sensorModeIndex = 0; sensorModeIndex < sensorModes.size(); ++sensorModeIndex)
        {
            Argus::ISensorMode *sensorMode =
                Argus::interface_cast<Argus::ISensorMode>(sensorModes[sensorModeIndex]);
            if (!sensorMode)
                ORIGINATE_ERROR("Failed to get ISensorMode interface");

            // convert ns to fps
            float maximum_fps = TimeValue::fromNSec(
                sensorMode->getFrameDurationRange().min()).toCyclesPerSec();
            float minimum_fps = TimeValue::fromNSec(
                sensorMode->getFrameDurationRange().max()).toCyclesPerSec();

            stream << "  Sensor mode: " << sensorModeIndex << std::endl;
            stream << "    Resolution: " <<
                sensorMode->getResolution().width() << "x" <<
                sensorMode->getResolution().height() << std::endl;
            stream << "    Exposure time range: " <<
                sensorMode->getExposureTimeRange().min() << " - " <<
                sensorMode->getExposureTimeRange().max() << " ns" << std::endl;
            stream << "    Frame duration range: " <<
                sensorMode->getFrameDurationRange().min() << " - " <<
                sensorMode->getFrameDurationRange().max() << " ns" << std::endl;
            stream << "    Framerate range: " <<
                minimum_fps << " - " <<
                maximum_fps << " fps" << std::endl;
            stream << "    InputBitDepth:  " <<
                sensorMode->getInputBitDepth() << std::endl;
            stream << "    OutputBitDepth: " <<
                sensorMode->getOutputBitDepth() << std::endl;
            stream << "    Analog gain range: " <<
                sensorMode->getAnalogGainRange().min() << " - " <<
                sensorMode->getAnalogGainRange().max() << std::endl;

            stream << "    SensorModeType: " <<
                sensorMode->getSensorModeType().getName() << std::endl;

            Argus::Ext::IPwlWdrSensorMode* pwlMode =
                Argus::interface_cast<Argus::Ext::IPwlWdrSensorMode>(sensorModes[sensorModeIndex]);

            Argus::Ext::IDolWdrSensorMode* dolMode =
                Argus::interface_cast<Argus::Ext::IDolWdrSensorMode>(sensorModes[sensorModeIndex]);

            if (pwlMode)
            {
                stream << "    Piecewise Linear (PWL) WDR Extension supported with: " <<
                    pwlMode->getControlPointCount() << " control points." << std::endl;
                std::vector< Argus::Point2D<float> > points;
                Argus::Status status = pwlMode->getControlPoints(&points);
                if (status != Argus::STATUS_OK)
                    ORIGINATE_ERROR("Error obtaining control points");
                stream << "    - Control Points: " << std::endl;
                for (uint32_t j = 0; j < pwlMode->getControlPointCount(); j++)
                {
                    stream << "                    (" <<  points[j].x() << ", " <<
                        points[j].y() << ")" << std::endl;
                }
            }
            else if (dolMode)
            {
                stream << "    Digital Overlap (DOL) WDR Extension supported with: " << std::endl <<
                        "    - Number of Exposures: " <<
                        dolMode->getExposureCount() << std::endl <<
                        "    - Number of Optical Black Lines per exposure: " <<
                        dolMode->getOpticalBlackRowCount() << std::endl <<
                        "    - Number of Line Info marker pixels per row per exposure: " <<
                        dolMode->getLineInfoMarkerWidth() << std::endl <<
                        "    - Number of margin pixels on left per row per exposure: " <<
                        dolMode->getLeftMarginWidth() << std::endl <<
                        "    - Number of margin pixels on right per row per exposure: " <<
                        dolMode->getRightMarginWidth() << std::endl;

                std::vector<u_int32_t> verticalBlankPeriodRowCounts;
                Argus::Status status =
                        dolMode->getVerticalBlankPeriodRowCount(&verticalBlankPeriodRowCounts);
                if (status != Argus::STATUS_OK)
                    ORIGINATE_ERROR("Error obtaining vertical blank period offsets per exposure");
                stream << "    - Vertical blank period section row counts per exposure: "
                       << std::endl;
                for (uint32_t j = 0; j < verticalBlankPeriodRowCounts.size(); j++)
                {
                    stream << "      - VBP-section[" <<  j << "] : "
                           << verticalBlankPeriodRowCounts[j] << std::endl;
                }

                stream << "    - Physical Resolution: " <<
                        dolMode->getPhysicalResolution().width() << "x" <<
                        dolMode->getPhysicalResolution().height() << std::endl;
            }

            stream << std::endl;
        }
    }

    info = stream.str();

    return true;
}

bool Dispatcher::getSensorMode(uint32_t sensorModeIndex, Argus::SensorMode **sensorMode) const
{
    assert(m_deviceOpen);

    if (sensorModeIndex >= m_sensorModes.size())
        ORIGINATE_ERROR("Invalid sensor mode index");

    *sensorMode = m_sensorModes[sensorModeIndex];

    return true;
}

Argus::Range<int32_t> Dispatcher::getDeviceFocusPositionRange() const
{
    return m_deviceFocusPositionRange.get();
}

Argus::Range<int32_t> Dispatcher::getDeviceAperturePositionRange() const
{
    return m_deviceAperturePositionRange.get();
}

Argus::Range<float> Dispatcher::getDeviceApertureMotorSpeedRange() const
{
    return m_deviceApertureMotorSpeedRange.get();
}

uint32_t Dispatcher::getDeviceCount() const
{
    assert(m_deviceOpen);

    return m_cameraDevices.size();
}

bool Dispatcher::createSession(TrackedUniqueObj<Argus::CaptureSession> &session,
    uint32_t deviceIndex)
{
    assert(m_deviceOpen);

    if (deviceIndex > m_cameraDevices.size())
        ORIGINATE_ERROR("Invalid device index");

    // close the internal session, it might be using the device which will be used by the new
    // session
    PROPAGATE_ERROR(closeSession());

    // create the new capture session
    Argus::UniqueObj<Argus::CaptureSession> newSession(
        m_iCameraProvider->createCaptureSession(m_cameraDevices[deviceIndex]));
    if (!newSession)
        ORIGINATE_ERROR("Failed to create CaptureSession");

    PROPAGATE_ERROR(session.reset(newSession.release(), this));

    return true;
}

bool Dispatcher::dumpSessionInfo() const
{
    for (ActiveSessionList::const_iterator it = m_activeSessions.begin(); it != m_activeSessions.end();
         ++it)
    {
        Argus::Ext::IDebugCaptureSession *iDebugCaptureSession =
            Argus::interface_cast<Argus::Ext::IDebugCaptureSession>(it->m_session);
        if (!iDebugCaptureSession)
            ORIGINATE_ERROR("DebugCaptureSession extension not supported");

        const Argus::Status status = iDebugCaptureSession->dump(STDOUT_FILENO);
        if (status != Argus::STATUS_OK)
            ORIGINATE_ERROR("Failed to get dump runtime info");
    }

    return true;
}

/**
 * Create the internal session
 */
bool Dispatcher::createSession()
{
    if (m_captureSession)
        return true;

    PROPAGATE_ERROR(createSession(m_captureSession, m_deviceIndex));

    return true;
}

/**
 * Close the internal session
 */
bool Dispatcher::closeSession()
{
    m_captureSession.reset();
    return true;
}

bool Dispatcher::track(Argus::CaptureSession *session)
{
    m_activeSessions.push_back(ActiveSession(session));
    return true;
}

bool Dispatcher::untrack(Argus::CaptureSession *session)
{
    for (ActiveSessionList::iterator it = m_activeSessions.begin(); it != m_activeSessions.end();
         ++it)
    {
        if (it->m_session == session)
        {
            m_activeSessions.erase(it);
            return true;
        }
    }

    ORIGINATE_ERROR("Session not found");
}

bool Dispatcher::waitForEvents(Argus::EventQueue *eventQueue, TimeValue timeout,
    Argus::CaptureSession *session)
{
    if (!session)
    {
        // use the internal session
        PROPAGATE_ERROR(createSession());
        session = m_captureSession.get();
    }

    Argus::IEventProvider *iEventProvider = Argus::interface_cast<Argus::IEventProvider>(session);
    if (!iEventProvider)
        ORIGINATE_ERROR("Failed to get iEventProvider interface");

    // wait for the events
    const Argus::Status status = iEventProvider->waitForEvents(eventQueue, timeout.toNSec());
    if ((status != Argus::STATUS_OK) && (status != Argus::STATUS_TIMEOUT))
        ORIGINATE_ERROR("Failed to get events");

    return true;
}

bool Dispatcher::createRequest(TrackedUniqueObj<Argus::Request> &request,
    Argus::CaptureIntent captureIntent, Argus::CaptureSession *session)
{
    if (!session)
    {
        // use the internal session
        PROPAGATE_ERROR(createSession());
        session = m_captureSession.get();
    }

    Argus::ICaptureSession *iCaptureSession =
        Argus::interface_cast<Argus::ICaptureSession>(session);
    if (!iCaptureSession)
        ORIGINATE_ERROR("Failed to get ICaptureSession interface");

    // Create request
    Argus::UniqueObj<Argus::Request> newRequest =
        Argus::UniqueObj<Argus::Request>(iCaptureSession->createRequest(captureIntent));
    if (!newRequest)
        ORIGINATE_ERROR("Failed to create request");

    // Setup request
    Argus::IRequest *iRequest = Argus::interface_cast<Argus::IRequest>(newRequest);
    if (!iRequest)
        ORIGINATE_ERROR("Failed to get IRequest interface");

    // get source settings interface
    Argus::ISourceSettings *iSourceSettings =
        Argus::interface_cast<Argus::ISourceSettings>(iRequest->getSourceSettings());
    if (!iSourceSettings)
        ORIGINATE_ERROR("Failed to get ISourceSettings interface");

    // register the source settings observer
    PROPAGATE_ERROR(registerObserver(iSourceSettings));

    // get auto control settings interface
    Argus::IAutoControlSettings *iAutoControlSettings =
        Argus::interface_cast<Argus::IAutoControlSettings>(iRequest->getAutoControlSettings());
    if (!iAutoControlSettings)
        ORIGINATE_ERROR("Failed to get IAutoControlSettings interface");

    // register the auto control settings observer
    PROPAGATE_ERROR(registerObserver(iAutoControlSettings));

    // register denoise settings interface
    Argus::IDenoiseSettings *iDenoiseSettings =
        Argus::interface_cast<Argus::IDenoiseSettings>(newRequest);
    if (!iDenoiseSettings)
        ORIGINATE_ERROR("Failed to get IDenoiseSettings interface");
    PROPAGATE_ERROR(registerObserver(iDenoiseSettings));

    // register edge enhance settings interface
    Argus::IEdgeEnhanceSettings *iEdgeEnhanceSettings =
        Argus::interface_cast<Argus::IEdgeEnhanceSettings>(newRequest);
    if (!iEdgeEnhanceSettings)
        ORIGINATE_ERROR("Failed to get IEdgeEnhanceSettings interface");
    PROPAGATE_ERROR(registerObserver(iEdgeEnhanceSettings));

    if (supportsExtension(Argus::EXT_DE_FOG))
    {
        // get DeFog settings interface
        Argus::Ext::IDeFogSettings *iDeFogSettings =
            Argus::interface_cast<Argus::Ext::IDeFogSettings>(newRequest);
        if (iDeFogSettings)
        {
            // register the DeFog settings observer
            PROPAGATE_ERROR(registerObserver(iDeFogSettings));
        }
    }

    PROPAGATE_ERROR(request.reset(newRequest.release(), this));

    return true;
}

bool Dispatcher::track(Argus::Request *request)
{
    return true;
}

bool Dispatcher::untrack(Argus::Request *request)
{
    Argus::IRequest *iRequest = Argus::interface_cast<Argus::IRequest>(request);
    if (!iRequest)
        ORIGINATE_ERROR("Failed to get IRequest interface");

    // get source settings interface
    Argus::ISourceSettings *iSourceSettings =
        Argus::interface_cast<Argus::ISourceSettings>(iRequest->getSourceSettings());
    if (!iSourceSettings)
        ORIGINATE_ERROR("Failed to get ISourceSettings interface");

    // unregister the source settings observer
    PROPAGATE_ERROR(unregisterObserver(iSourceSettings));

    // get auto control settings interface
    Argus::IAutoControlSettings *iAutoControlSettings =
        Argus::interface_cast<Argus::IAutoControlSettings>(iRequest->getAutoControlSettings());
    if (!iAutoControlSettings)
        ORIGINATE_ERROR("Failed to get IAutoControlSettings interface");

    // unregister the auto control settings observer
    PROPAGATE_ERROR(unregisterObserver(iAutoControlSettings));

    // unregister denoise settings interface
    Argus::IDenoiseSettings *iDenoiseSettings =
        Argus::interface_cast<Argus::IDenoiseSettings>(request);
    if (!iDenoiseSettings)
        ORIGINATE_ERROR("Failed to get IDenoiseSettings interface");
    PROPAGATE_ERROR(unregisterObserver(iDenoiseSettings));

    // unregister edge enhance settings interface
    Argus::IEdgeEnhanceSettings *iEdgeEnhanceSettings =
        Argus::interface_cast<Argus::IEdgeEnhanceSettings>(request);
    if (!iEdgeEnhanceSettings)
        ORIGINATE_ERROR("Failed to get IEdgeEnhanceSettings interface");
    PROPAGATE_ERROR(unregisterObserver(iEdgeEnhanceSettings));

    if (supportsExtension(Argus::EXT_DE_FOG))
    {
        // get DeFog settings interface
        Argus::Ext::IDeFogSettings *iDeFogSettings =
            Argus::interface_cast<Argus::Ext::IDeFogSettings>(request);
        if (iDeFogSettings)
        {
            // unregister the DeFog settings observer
            PROPAGATE_ERROR(unregisterObserver(iDeFogSettings));
        }
    }

    return true;
}

bool Dispatcher::createEventQueue(const std::vector<Argus::EventType> &eventTypes,
    Argus::UniqueObj<Argus::EventQueue>& eventQueue, Argus::CaptureSession *session)
{
    if (!session)
    {
        // use the internal session
        PROPAGATE_ERROR(createSession());
        session = m_captureSession.get();
    }

    Argus::IEventProvider *iEventProvider =
        Argus::interface_cast<Argus::IEventProvider>(session);
    if (!iEventProvider)
        ORIGINATE_ERROR("Failed to get IEventProvider interface");

    Argus::EventQueue *newEventQueue = iEventProvider->createEventQueue(eventTypes);
    if (!newEventQueue)
        ORIGINATE_ERROR("Failed to create eventQueue");

    eventQueue.reset(newEventQueue);

    return true;
}

bool Dispatcher::capture(Argus::Request *request, Argus::CaptureSession *session)
{
    if (!session)
    {
        // use the internal session
        PROPAGATE_ERROR(createSession());
        session = m_captureSession.get();
    }

    Argus::ICaptureSession *iCaptureSession =
        Argus::interface_cast<Argus::ICaptureSession>(session);
    if (!iCaptureSession)
        ORIGINATE_ERROR("Failed to get ICaptureSession interface");

    if (iCaptureSession->capture(request, Argus::TIMEOUT_INFINITE) == 0)
        ORIGINATE_ERROR("Failed to submit the still capture request");

    return true;
}

bool Dispatcher::startRepeat(Argus::Request *request, Argus::CaptureSession *session)
{
    if (!session)
    {
        // use the internal session
        PROPAGATE_ERROR(createSession());
        session = m_captureSession.get();
    }

    Argus::ICaptureSession *iCaptureSession =
        Argus::interface_cast<Argus::ICaptureSession>(session);
    if (!iCaptureSession)
        ORIGINATE_ERROR("Failed to get ICaptureSession interface");

    if (iCaptureSession->repeat(request) != Argus::STATUS_OK)
        ORIGINATE_ERROR("Failed to submit repeating capture request");

    // add the request to the list of active requests for the session
    bool found = false;
    for (ActiveSessionList::iterator it = m_activeSessions.begin(); it != m_activeSessions.end();
         ++it)
    {
        if (it->m_session == session)
        {
            it->m_requests.push_back(request);
            found = true;
            break;
        }
    }
    if (!found)
        ORIGINATE_ERROR("Did not find the session in the list of active sessions");

    return true;
}

bool Dispatcher::startRepeatBurst(const std::vector<const Argus::Request*>& requestList,
    Argus::CaptureSession *session)
{
    if (!session)
    {
        // use the internal session
        PROPAGATE_ERROR(createSession());
        session = m_captureSession.get();
    }

    Argus::ICaptureSession *iCaptureSession =
        Argus::interface_cast<Argus::ICaptureSession>(session);
    if (!iCaptureSession)
        ORIGINATE_ERROR("Failed to get ICaptureSession interface");

    if (iCaptureSession->repeatBurst(requestList) != Argus::STATUS_OK)
        ORIGINATE_ERROR("Failed to submit repeating burst request");

    // add the requests to the list of active requests for the session
    bool found = false;
    for (ActiveSessionList::iterator it = m_activeSessions.begin(); it != m_activeSessions.end();
         ++it)
    {
        if (it->m_session == session)
        {
            it->m_requests.insert(it->m_requests.end(), requestList.begin(), requestList.end());
            found = true;
            break;
        }
    }
    if (!found)
        ORIGINATE_ERROR("Did not find the session in the list of active sessions");

    return true;
}

bool Dispatcher::stopRepeat(Argus::CaptureSession *session)
{
    if (!session)
    {
        // use the internal session
        PROPAGATE_ERROR(createSession());
        session = m_captureSession.get();
    }

    Argus::ICaptureSession *iCaptureSession =
        Argus::interface_cast<Argus::ICaptureSession>(session);
    if (!iCaptureSession)
        ORIGINATE_ERROR("Failed to get ICaptureSession interface");

    iCaptureSession->stopRepeat();

    // clear the list of active requests for the session
    bool found = false;
    for (ActiveSessionList::iterator it = m_activeSessions.begin(); it != m_activeSessions.end();
         ++it)
    {
        if (it->m_session == session)
        {
            it->m_requests.clear();
            found = true;
            break;
        }
    }
    if (!found)
        ORIGINATE_ERROR("Did not find the session in the list of active sessions");

    return true;
}

bool Dispatcher::restartActiveRequests()
{
    for (ActiveSessionList::iterator it = m_activeSessions.begin(); it != m_activeSessions.end();
        ++it)
    {
        if (!it->m_requests.empty())
        {
            Argus::ICaptureSession *iCaptureSession =
                Argus::interface_cast<Argus::ICaptureSession>(it->m_session);
            if (!iCaptureSession)
                ORIGINATE_ERROR("Failed to get ICaptureSession interface");

            iCaptureSession->stopRepeat();

            if (iCaptureSession->repeatBurst(it->m_requests) != Argus::STATUS_OK)
                ORIGINATE_ERROR("Failed to submit repeating burst request");
        }
    }

    return true;
}

uint32_t Dispatcher::maxBurstRequests(Argus::CaptureSession *session)
{
    if (!session)
    {
        // use the internal session
        PROPAGATE_ERROR(createSession());
        session = m_captureSession.get();
    }

    Argus::ICaptureSession *iCaptureSession =
        Argus::interface_cast<Argus::ICaptureSession>(session);
    if (!iCaptureSession)
    {
        REPORT_ERROR("Failed to get ICaptureSession interface");
        return 0;
    }

    return iCaptureSession->maxBurstRequests();
}

bool Dispatcher::waitForIdle(Argus::CaptureSession *session)
{
    if (!session)
    {
        // use the internal session
        PROPAGATE_ERROR(createSession());
        session = m_captureSession.get();
    }

    Argus::ICaptureSession *iCaptureSession =
        Argus::interface_cast<Argus::ICaptureSession>(session);
    if (!iCaptureSession)
        ORIGINATE_ERROR("Failed to get ICaptureSession interface");

    if (iCaptureSession->waitForIdle() != Argus::STATUS_OK)
        ORIGINATE_ERROR("Waiting for idle failed");

    return true;
}

bool Dispatcher::getOutputSize(Argus::Size2D<uint32_t> *size) const
{
    // if width or height is 0 take the sensor size
    if ((m_outputSize.get().width() == 0) || (m_outputSize.get().height() == 0))
    {
        // the device needs to be open to get sensor modes
        assert(m_deviceOpen);

        Argus::ISensorMode *sensorMode =
            Argus::interface_cast<Argus::ISensorMode>(m_sensorModes[m_sensorModeIndex.get()]);
        if (!sensorMode)
            ORIGINATE_ERROR("Failed to get ISensorMode interface");
        *size = sensorMode->getResolution();
    }
    else
    {
        *size = m_outputSize;
    }

    return true;
}

bool Dispatcher::createOutputStream(Argus::Request *request, bool enableMetadata,
    Argus::UniqueObj<Argus::OutputStream> &stream, Argus::CaptureSession *session)
{
    if (!session)
    {
        // use the internal session
        PROPAGATE_ERROR(createSession());
        session = m_captureSession.get();
    }

    Argus::IRequest *iRequest = Argus::interface_cast<Argus::IRequest>(request);
    if (!iRequest)
        ORIGINATE_ERROR("Failed to get IRequest interface");

    Argus::Size2D<uint32_t> outputSize;
    PROPAGATE_ERROR(getOutputSize(&outputSize));

    Argus::ICaptureSession *iCaptureSession =
        Argus::interface_cast<Argus::ICaptureSession>(session);
    if (!iCaptureSession)
        ORIGINATE_ERROR("Failed to get ICaptureSession interface");

    Argus::UniqueObj<Argus::OutputStreamSettings> outputStreamSettings(
        iCaptureSession->createOutputStreamSettings(Argus::STREAM_TYPE_EGL));
    Argus::IEGLOutputStreamSettings* iEGLOutputStreamSettings =
        Argus::interface_cast<Argus::IEGLOutputStreamSettings>(outputStreamSettings);
    if (!iEGLOutputStreamSettings)
        ORIGINATE_ERROR("Failed to get IEGLOutputStreamSettings interface");

    iEGLOutputStreamSettings->setPixelFormat(m_captureYuvFormat.get());
    iEGLOutputStreamSettings->setResolution(outputSize);
    iEGLOutputStreamSettings->setEGLDisplay(Composer::getInstance().getEGLDisplay());
    iEGLOutputStreamSettings->setMetadataEnable(enableMetadata);

    Argus::UniqueObj<Argus::OutputStream> outputStream(
        iCaptureSession->createOutputStream(outputStreamSettings.get()));
    if (!outputStream)
        ORIGINATE_ERROR("Failed to create OutputStream");

    stream.reset(outputStream.release());

    return true;
}

bool Dispatcher::enableOutputStream(Argus::Request *request, Argus::OutputStream *stream)
{
    Argus::IRequest *iRequest = Argus::interface_cast<Argus::IRequest>(request);
    if (!iRequest)
        ORIGINATE_ERROR("Failed to get IRequest interface");

    // Enable the stream
    if (iRequest->enableOutputStream(stream) != Argus::STATUS_OK)
        ORIGINATE_ERROR("Failed to enable the output stream");

    return true;
}

bool Dispatcher::disableOutputStream(Argus::Request *request, Argus::OutputStream *stream)
{
    Argus::IRequest *iRequest = Argus::interface_cast<Argus::IRequest>(request);
    if (!iRequest)
        ORIGINATE_ERROR("Failed to get IRequest interface");

    // Disable the stream
    if (iRequest->disableOutputStream(stream) != Argus::STATUS_OK)
        ORIGINATE_ERROR("Failed to disable the output stream");

    return true;
}

bool Dispatcher::registerObserver(Argus::IDenoiseSettings *iDenoiseSettings)
{
    UniquePointer<DenoiseSettingsObserver> denoiseSettings;

    denoiseSettings.reset(new DenoiseSettingsObserver(iDenoiseSettings));
    if (!denoiseSettings)
        ORIGINATE_ERROR("Out of memory");

    m_observers.push_front(denoiseSettings.release());
    return true;
}

bool Dispatcher::registerObserver(Argus::IEdgeEnhanceSettings *iEdgeEnhanceSettings)
{
    UniquePointer<EdgeEnhanceSettingsObserver> edgeEnhanceSettings;

    edgeEnhanceSettings.reset(new EdgeEnhanceSettingsObserver(iEdgeEnhanceSettings));
    if (!edgeEnhanceSettings)
        ORIGINATE_ERROR("Out of memory");

    m_observers.push_front(edgeEnhanceSettings.release());
    return true;
}

bool Dispatcher::registerObserver(Argus::ISourceSettings *iSourceSettings)
{
    UniquePointer<SourceSettingsObserver> sourceSettings;

    sourceSettings.reset(new SourceSettingsObserver(iSourceSettings));
    if (!sourceSettings)
        ORIGINATE_ERROR("Out of memory");

    m_observers.push_front(sourceSettings.release());
    return true;
}

bool Dispatcher::registerObserver(Argus::IAutoControlSettings *iAutoControlSettings)
{
    UniquePointer<AutoControlSettingsObserver> autoControlSettings;

    autoControlSettings.reset(new AutoControlSettingsObserver(iAutoControlSettings));
    if (!autoControlSettings)
        ORIGINATE_ERROR("Out of memory");

    m_observers.push_front(autoControlSettings.release());
    return true;
}

bool Dispatcher::registerObserver(Argus::Ext::IDeFogSettings *iDeFogSettings)
{
    UniquePointer<DeFogSettingsObserver> deFogSettings;

    deFogSettings.reset(new DeFogSettingsObserver(iDeFogSettings));
    if (!deFogSettings)
        ORIGINATE_ERROR("Out of memory");

    m_observers.push_front(deFogSettings.release());
    return true;
}

bool Dispatcher::unregisterObserver(Argus::Interface *interface)
{
    for (std::list<IObserverForInterface*>::iterator it = m_observers.begin();
         it != m_observers.end(); ++it)
    {
        if ((*it)->isInterface(interface))
        {
            delete *it;
            m_observers.erase(it);
            return true;
        }
    }

    ORIGINATE_ERROR("Observer not found");
}

bool Dispatcher::message(const char *msg, ...)
{
    if (m_verbose)
    {
        va_list list;

        va_start(list, msg);

        if (vprintf(msg, list) < 0)
        {
            va_end(list);
            ORIGINATE_ERROR("Failed to print message");
        }

        va_end(list);
    }

    return true;
}

}; // namespace ArgusSamples
