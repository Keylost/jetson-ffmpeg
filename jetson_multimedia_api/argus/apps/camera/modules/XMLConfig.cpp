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

#include <string>
#include <fstream>
#include <stdio.h>

#include <expat.h>

#include "XMLConfig.h"
#include "Dispatcher.h"
#include "UniquePointer.h"

namespace ArgusSamples
{

// XML version
static const char *VERSION = "1.0";

// element names
static const char *ELEMENT_DEVICE_INDEX = "deviceIndex";
static const char *ELEMENT_VERBOSE = "verbose";
static const char *ELEMENT_KPI = "kpi";
static const char *ELEMENT_EXPOSURE_TIME_RANGE = "exposureTimeRange";
static const char *ELEMENT_GAIN_RANGE = "gainRange";
static const char *ELEMENT_SENSOR_MODE_INDEX = "sensorModeIndex";
static const char *ELEMENT_FRAME_RATE = "frameRate";
static const char *ELEMENT_FOCUS_POSITION = "focusPosition";
static const char *ELEMENT_APERTURE_POSITION = "aperturePosition";
static const char *ELEMENT_APERTURE_MOTOR_SPEED = "apertureMotorSpeed";
static const char *ELEMENT_CAPTURE_YUV_FORMAT = "captureYuvFormat";
static const char *ELEMENT_AE_ANTIBANDING_MODE = "aeAntibandingMode";
static const char *ELEMENT_AE_LOCK = "aeLock";
static const char *ELEMENT_AWB_LOCK = "awbLock";
static const char *ELEMENT_AWB_MODE = "awbMode";
static const char *ELEMENT_EXPOSURE_COMPENSATION = "exposureCompensation";
static const char *ELEMENT_ISP_DIGITAL_GAIN_RANGE = "ispDigitalGainRange";
static const char *ELEMENT_DENOISE_MODE = "denoiseMode";
static const char *ELEMENT_STILL_FILE_TYPE = "stillFileType";
static const char *ELEMENT_VIDEO_FORMAT = "videoFormat";
static const char *ELEMENT_VIDEO_FILE_TYPE = "videoFileType";
static const char *ELEMENT_VIDEO_BIT_RATE = "videoBitRate";
static const char *ELEMENT_OUTPUT_SIZE = "outputSize";
static const char *ELEMENT_OUTPUT_PATH = "outputPath";
static const char *ELEMENT_DE_FOG_ENABLE = "deFogEnable";
static const char *ELEMENT_DE_FOG_AMOUNT = "deFogAmount";
static const char *ELEMENT_DE_FOG_QUALITY = "deFogQaulity";

static void XMLCALL xmlHandleData(void *parser, const char *s, int len)
{
    XML_Parser p = (XML_Parser)parser;
    std::string *data = reinterpret_cast<std::string*>(XML_GetUserData(p));

    data->append(s, len);
}

static void XMLCALL xmlStartElement(void *parser, const char *name, const char **atts)
{
    XML_Parser p = (XML_Parser)parser;

    if (strcmp(name, "argusconfig") == 0)
    {
        const char **curAtt = atts;

        while (*curAtt != NULL)
        {
            const char *attribute = curAtt[0];
            const char *value = curAtt[1];

            if (strcmp(attribute, "version") == 0)
            {
                if (strcmp(value, VERSION) != 0)
                {
                    ORIGINATE_ERROR_FAIL("Unsupported version '%s' expected version '%s'",
                        value, VERSION);
                }
            }
            else
                ORIGINATE_ERROR_FAIL("Found unexpected attribute '%s'", attribute);
            curAtt += 2;
        }
    }

    XML_SetCharacterDataHandler(p, xmlHandleData);

    return;

fail:
    XML_StopParser(p, XML_FALSE);
}

/**
 * Check if an element matches the value name, if this is the case set the value to 'dataStr'
 * @param [in] elementName      current element
 * @param [in] dataStr          data for that element
 * @param [in] valueName        value name
 * @param [in] value            value to update with dataStr if there is a match
 * @param [out] match           set if there was a match
 */
template<typename T> static bool checkValue(const char *elementName, const char *dataStr,
    const char *valueName, Value<T> &value, bool *match)
{
    if (strcmp(elementName, valueName) == 0)
    {
        PROPAGATE_ERROR(value.setFromString(dataStr));
        *match = true;
    }

    return true;
}

static void XMLCALL xmlEndElement(void *parser, const char *name)
{
    Dispatcher &dispatcher = Dispatcher::getInstance();
    XML_Parser p = (XML_Parser)parser;
    std::string *data = reinterpret_cast<std::string*>(XML_GetUserData(p));

    if (strcmp(name, ELEMENT_DEVICE_INDEX) == 0)
    {
        PROPAGATE_ERROR_FAIL(dispatcher.m_deviceIndex.setFromString(data->c_str()));
    }
    else if (strcmp(name, ELEMENT_VERBOSE) == 0)
    {
        PROPAGATE_ERROR_FAIL(dispatcher.m_verbose.setFromString(data->c_str()));
    }
    else if (strcmp(name, ELEMENT_KPI) == 0)
    {
        PROPAGATE_ERROR_FAIL(dispatcher.m_kpi.setFromString(data->c_str()));
    }
    else if (strcmp(name, ELEMENT_EXPOSURE_TIME_RANGE) == 0)
    {
        PROPAGATE_ERROR_FAIL(dispatcher.m_exposureTimeRange.setFromString(data->c_str()));
    }
    else if (strcmp(name, ELEMENT_GAIN_RANGE) == 0)
    {
        PROPAGATE_ERROR_FAIL(dispatcher.m_gainRange.setFromString(data->c_str()));
    }
    else if (strcmp(name, ELEMENT_SENSOR_MODE_INDEX) == 0)
    {
        PROPAGATE_ERROR_FAIL(dispatcher.m_sensorModeIndex.setFromString(data->c_str()));
    }
    else if (strcmp(name, ELEMENT_FRAME_RATE) == 0)
    {
        PROPAGATE_ERROR_FAIL(dispatcher.m_frameRate.setFromString(data->c_str()));
    }
    else if (strcmp(name, ELEMENT_FOCUS_POSITION) == 0)
    {
        PROPAGATE_ERROR_FAIL(dispatcher.m_focusPosition.setFromString(data->c_str()));
    }
    else if (strcmp(name, ELEMENT_APERTURE_POSITION) == 0)
    {
        PROPAGATE_ERROR_FAIL(dispatcher.m_aperturePosition.setFromString(data->c_str()));
    }
    else if (strcmp(name, ELEMENT_APERTURE_MOTOR_SPEED) == 0)
    {
        PROPAGATE_ERROR_FAIL(dispatcher.m_apertureMotorSpeed.setFromString(data->c_str()));
    }
    else if (strcmp(name, ELEMENT_CAPTURE_YUV_FORMAT) == 0)
    {
        PROPAGATE_ERROR_FAIL(dispatcher.m_captureYuvFormat.setFromString(data->c_str()));
    }
    else if (strcmp(name, ELEMENT_DENOISE_MODE) == 0)
    {
        PROPAGATE_ERROR_FAIL(dispatcher.m_denoiseMode.setFromString(data->c_str()));
    }
    else if (strcmp(name, ELEMENT_AE_ANTIBANDING_MODE) == 0)
    {
        PROPAGATE_ERROR_FAIL(dispatcher.m_aeAntibandingMode.setFromString(data->c_str()));
    }
    else if (strcmp(name, ELEMENT_AE_LOCK) == 0)
    {
        PROPAGATE_ERROR_FAIL(dispatcher.m_aeLock.setFromString(data->c_str()));
    }
    else if (strcmp(name, ELEMENT_AWB_LOCK) == 0)
    {
        PROPAGATE_ERROR_FAIL(dispatcher.m_awbLock.setFromString(data->c_str()));
    }
    else if (strcmp(name, ELEMENT_AWB_MODE) == 0)
    {
        PROPAGATE_ERROR_FAIL(dispatcher.m_awbMode.setFromString(data->c_str()));
    }
    else if (strcmp(name, ELEMENT_EXPOSURE_COMPENSATION) == 0)
    {
        PROPAGATE_ERROR_FAIL(dispatcher.m_exposureCompensation.setFromString(data->c_str()));
    }
    else if (strcmp(name, ELEMENT_ISP_DIGITAL_GAIN_RANGE) == 0)
    {
        PROPAGATE_ERROR_FAIL(dispatcher.m_ispDigitalGainRange.setFromString(data->c_str()));
    }
    else if (strcmp(name, ELEMENT_STILL_FILE_TYPE) == 0)
    {
        PROPAGATE_ERROR_FAIL(dispatcher.m_stillFileType.setFromString(data->c_str()));
    }
    else if (strcmp(name, ELEMENT_VIDEO_FORMAT) == 0)
    {
        PROPAGATE_ERROR_FAIL(dispatcher.m_videoFormat.setFromString(data->c_str()));
    }
    else if (strcmp(name, ELEMENT_VIDEO_FILE_TYPE) == 0)
    {
        PROPAGATE_ERROR_FAIL(dispatcher.m_videoFileType.setFromString(data->c_str()));
    }
    else if (strcmp(name, ELEMENT_VIDEO_BIT_RATE) == 0)
    {
        PROPAGATE_ERROR_FAIL(dispatcher.m_videoBitRate.setFromString(data->c_str()));
    }
    else if (strcmp(name, ELEMENT_OUTPUT_SIZE) == 0)
    {
        PROPAGATE_ERROR_FAIL(dispatcher.m_outputSize.setFromString(data->c_str()));
    }
    else if (strcmp(name, ELEMENT_OUTPUT_PATH) == 0)
    {
        PROPAGATE_ERROR_FAIL(dispatcher.m_outputPath.set(*data));
    }
    else if (strcmp(name, ELEMENT_DE_FOG_ENABLE) == 0)
    {
        PROPAGATE_ERROR_FAIL(dispatcher.m_deFogEnable.setFromString(data->c_str()));
    }
    else if (strcmp(name, ELEMENT_DE_FOG_AMOUNT) == 0)
    {
        PROPAGATE_ERROR_FAIL(dispatcher.m_deFogAmount.setFromString(data->c_str()));
    }
    else if (strcmp(name, ELEMENT_DE_FOG_QUALITY) == 0)
    {
        PROPAGATE_ERROR_FAIL(dispatcher.m_deFogQuality.setFromString(data->c_str()));
    }
    else if (strcmp(name, "argusconfig") == 0)
    {
    }
    else
    {
        ORIGINATE_ERROR_FAIL("Unhandled element '%s'.", name);
    }

    XML_SetCharacterDataHandler(p, NULL);
    data->clear();

    return;

fail:
    XML_StopParser(p, XML_FALSE);
}

bool loadConfig(const char *configFile)
{
    if (configFile == NULL)
        ORIGINATE_ERROR("'configFile' is NULL");

    FILE *xmlFile;
    bool success = true;
    long ftellResult;
    size_t fileSize;
    UniquePointer<char> fileData;
    XML_Parser parser = NULL;
    std::string data;

    // open the file
    xmlFile = fopen(configFile, "rb");
    if (xmlFile == NULL)
        ORIGINATE_ERROR_FAIL("Failed to open file %s", configFile);

    // get file size
    if (fseek(xmlFile, 0, SEEK_END) != 0)
        ORIGINATE_ERROR_FAIL("Failed to read buffer file %s", configFile);

    ftellResult = ftell(xmlFile);
    if (ftellResult == -1)
        ORIGINATE_ERROR_FAIL("Failed to read buffer file %s", configFile);
    if (ftellResult == 0)
        ORIGINATE_ERROR_FAIL("Empty file %s", configFile);

    fileSize = ftellResult;

    if (fseek(xmlFile, 0, SEEK_SET) != 0)
        ORIGINATE_ERROR_FAIL("Failed to read buffer file %s", configFile);

    // allocate buffer
    fileData.reset(new char[fileSize + 1]);
    if (!fileData)
        ORIGINATE_ERROR_FAIL("Out of memory");

    // read from file to buffer
    if (fread(fileData.get(), fileSize, 1, xmlFile) != 1)
        ORIGINATE_ERROR_FAIL("Failed to read buffer file %s", configFile);
    // terminate string
    fileData.get()[fileSize] = 0;

    // create XML parser
    parser = XML_ParserCreate(NULL);
    if (parser == NULL)
        ORIGINATE_ERROR_FAIL("Failed to create parser");

    XML_UseParserAsHandlerArg(parser);
    // the user data is a string, the XML data handler appens to this, the end element handler
    // then uses it to set the values
    XML_SetUserData(parser, &data);
    XML_SetElementHandler(parser, xmlStartElement, xmlEndElement);

    // start parsing
    if (XML_Parse(parser, fileData.get(), (int)fileSize, 1) == XML_STATUS_ERROR)
    {
        // on failure print the line and column number and the line in which the error occured
        const XML_Size lineNumber = XML_GetCurrentLineNumber(parser);
        const XML_Size columnNumber = XML_GetCurrentColumnNumber(parser);
        const XML_Index byteIndex = XML_GetCurrentByteIndex(parser);

        std::string line;

        if ((byteIndex >= 0) && (static_cast<size_t>(byteIndex) < fileSize))
        {
            // find line start
            size_t lineStart = static_cast<size_t>(byteIndex);
            while ((lineStart > 0) && (fileData.get()[lineStart] != '\n'))
                --lineStart;
            // point after new line
            if (fileData.get()[lineStart] == '\n')
                ++lineStart;

            // find line end
            size_t lineEnd = static_cast<size_t>(lineStart);
            while ((lineEnd < fileSize) && (fileData.get()[lineEnd] != '\n'))
                ++lineEnd;

            line.append(&fileData.get()[lineStart], lineEnd - lineStart);
        }
        else
        {
            line += "-";
        }

        ORIGINATE_ERROR_FAIL("%s at line %lu:%lu:\n%s",
            XML_ErrorString(XML_GetErrorCode(parser)),
            lineNumber, columnNumber, line.c_str());
    }

    goto succeeded;

fail:
    success = false;

succeeded:
    if (parser != 0)
        XML_ParserFree(parser);
    if (xmlFile != NULL)
        fclose(xmlFile);

    return success;
}

// write an string to XML
static void writeValue(std::ofstream &stream, const char *name, const std::string& string)
{
    stream << "    <" << name << ">" << string << "</" << name << ">" << std::endl;
}

// write an value to XML
template<typename T> static void writeValue(std::ofstream &stream, const char *name,
    const Value<T> &value)
{
    writeValue(stream, name, value.toString());
}

bool saveConfig(const char *configFile)
{
    if (configFile == NULL)
        ORIGINATE_ERROR("'configFile' is NULL");

    Dispatcher &dispatcher = Dispatcher::getInstance();

    // open the stream
    std::ofstream stream(configFile, std::ofstream::out);
    if (!stream.is_open())
        ORIGINATE_ERROR("Failed to open file '%s' for saving.", configFile);

    // header
    stream << "<?xml version='1.0' encoding='utf-8'?>" << std::endl;
    stream << "<argusconfig version='" << VERSION << "'>" << std::endl;

    // write the value
    writeValue(stream, ELEMENT_DEVICE_INDEX, dispatcher.m_deviceIndex);
    writeValue(stream, ELEMENT_VERBOSE, dispatcher.m_verbose);
    writeValue(stream, ELEMENT_KPI, dispatcher.m_kpi);
    writeValue(stream, ELEMENT_EXPOSURE_TIME_RANGE, dispatcher.m_exposureTimeRange);
    writeValue(stream, ELEMENT_GAIN_RANGE, dispatcher.m_gainRange);
    writeValue(stream, ELEMENT_SENSOR_MODE_INDEX, dispatcher.m_sensorModeIndex);
    writeValue(stream, ELEMENT_FRAME_RATE, dispatcher.m_frameRate);
    writeValue(stream, ELEMENT_FOCUS_POSITION, dispatcher.m_focusPosition);
    writeValue(stream, ELEMENT_APERTURE_POSITION, dispatcher.m_aperturePosition);
    writeValue(stream, ELEMENT_APERTURE_MOTOR_SPEED, dispatcher.m_apertureMotorSpeed);
    writeValue(stream, ELEMENT_CAPTURE_YUV_FORMAT, dispatcher.m_captureYuvFormat);
    writeValue(stream, ELEMENT_DENOISE_MODE, dispatcher.m_denoiseMode);
    writeValue(stream, ELEMENT_AE_ANTIBANDING_MODE, dispatcher.m_aeAntibandingMode);
    writeValue(stream, ELEMENT_AE_LOCK, dispatcher.m_aeLock);
    writeValue(stream, ELEMENT_AWB_LOCK, dispatcher.m_awbLock);
    writeValue(stream, ELEMENT_AWB_MODE, dispatcher.m_awbMode);
    writeValue(stream, ELEMENT_EXPOSURE_COMPENSATION, dispatcher.m_exposureCompensation);
    writeValue(stream, ELEMENT_ISP_DIGITAL_GAIN_RANGE, dispatcher.m_ispDigitalGainRange);
    writeValue(stream, ELEMENT_STILL_FILE_TYPE, dispatcher.m_stillFileType);
    writeValue(stream, ELEMENT_VIDEO_FORMAT, dispatcher.m_videoFormat);
    writeValue(stream, ELEMENT_VIDEO_FILE_TYPE, dispatcher.m_videoFileType);
    writeValue(stream, ELEMENT_VIDEO_BIT_RATE, dispatcher.m_videoBitRate);
    writeValue(stream, ELEMENT_OUTPUT_SIZE, dispatcher.m_outputSize);
    writeValue(stream, ELEMENT_OUTPUT_PATH, dispatcher.m_outputPath.get());
    writeValue(stream, ELEMENT_DE_FOG_ENABLE, dispatcher.m_deFogEnable);
    writeValue(stream, ELEMENT_DE_FOG_AMOUNT, dispatcher.m_deFogAmount);
    writeValue(stream, ELEMENT_DE_FOG_QUALITY, dispatcher.m_deFogQuality);

    // footer
    stream << "</argusconfig>" << std::endl;

    stream.close();

    return true;
}

}; // namespace ArgusSamples
