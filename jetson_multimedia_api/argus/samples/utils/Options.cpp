/*
 * Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
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

#include <getopt.h>
#include <stdint.h>
#include <string.h>

#include <sstream>

#include "Error.h"
#include "Options.h"

namespace ArgusSamples
{

/* static */ bool Options::printHelp(void *userPtr, const char *optArg)
{
    Options *options = reinterpret_cast<Options*>(userPtr);

    PROPAGATE_ERROR(options->usage());
    PROPAGATE_ERROR(options->requestExit());

    return true;
}

/* static */ bool Options::exit(void *userPtr, const char *optArg)
{
    Options *options = reinterpret_cast<Options*>(userPtr);

    PROPAGATE_ERROR(options->requestExit());

    return true;
}

void Options::displayOption(bool displayType, std::vector<Option>::iterator &it, const char *optArg)
{
    if (displayType)
    {
        printf("%s: ", it->m_type == Option::TYPE_ACTION ? "Action" : "Option");
    }
    if (it->m_shortName)
    {
        printf("-%c, ", (char)it->m_shortName);
    }
    else
    {
        printf("    ");
    }
    printf("--%s", it->m_name.c_str());
    if (it->m_toStrFunc || optArg)
    {
        printf(",  value: %s", optArg ? optArg : it->m_toStrFunc(it->m_userPtr).c_str());
    }
    printf("\n");
}

/* static */ bool Options::displayOptionsAndValues(void *userPtr, const char *optArg)
{
    Options *options = reinterpret_cast<Options*>(userPtr);

    printf("================= Options ==================\n");
    for (std::vector<Option>::iterator it = options->m_options.begin();
         it < options->m_options.end(); ++it)
    {
        if (it->m_type == Option::TYPE_OPTION)
        {
            displayOption(false, it, NULL);
        }
    }
    printf("============================================\n");
    return true;
}

Options::Options(const char *programName)
    : m_initialized(false)
    , m_requestExit(false)
    , m_programName(programName)
    , m_displayOptionChanges(false)
{
}

Options::~Options()
{
}

bool Options::initialize()
{
    if (m_initialized)
        return true;

    m_initialized = true;

    PROPAGATE_ERROR(addOption(
        Option("help", 'h', "", Option::TYPE_ACTION,
        "display this help and exit", printHelp, this)));
    PROPAGATE_ERROR(addOption(
        Option("exit", 'x', "", Option::TYPE_ACTION,
        "exit from the program", exit, this)));
    PROPAGATE_ERROR(addOption(
        Option("displayoptionvalues", 'O', "",
            Option::TYPE_ACTION,
            "display all options and values", displayOptionsAndValues, this)));
    PROPAGATE_ERROR(addOption(
        createValueOption("displayoptionchanges", 'o', "0 or 1",
            "display changed options and actions", m_displayOptionChanges, "1")));

    return true;
}

bool Options::addOption(const Option &option, void *userPtr)
{
    PROPAGATE_ERROR(initialize());

    if (userPtr)
    {
        Option optionCopy = option;
        optionCopy.m_userPtr = userPtr;

        m_options.push_back(optionCopy);
    }
    else
    {
        m_options.push_back(option);
    }

    return true;
}

bool Options::addOptions(size_t count, const Option *options, void *userPtr)
{
    for (size_t i = 0; i < count; ++i)
    {
        PROPAGATE_ERROR(addOption(options[i], userPtr));
    }
    return true;
}

bool Options::addDescription(const char *description)
{
    PROPAGATE_ERROR(initialize());

    m_description += description;
    return true;
}

bool Options::requestExit()
{
    m_requestExit = true;
    return true;
}

bool Options::requestedExit() const
{
    return m_requestExit;
}

bool Options::parse(const int argc, char * const *argv)
{
    std::string optString;
    std::vector<option> options;
    std::vector<Option>::iterator it;

    PROPAGATE_ERROR(initialize());

    // build the long option struct and the short option string
    for (it = m_options.begin(); it < m_options.end(); ++it)
    {
        option getoptOption;

        memset(&getoptOption, 0, sizeof(getoptOption));

        getoptOption.name = it->m_name.c_str();
        if (it->m_flags == Option::FLAG_NO_ARGUMENT)
            getoptOption.has_arg = no_argument;
        else if (it->m_flags == Option::FLAG_OPTIONAL_ARGUMENT)
            getoptOption.has_arg = optional_argument;
        else if (it->m_flags == Option::FLAG_REQUIRED_ARGUMENT)
            getoptOption.has_arg = required_argument;
        else
            ORIGINATE_ERROR("Unhandled flag");
        getoptOption.flag = NULL;
        getoptOption.val = it->m_shortName;

        options.push_back(getoptOption);

        if (it->m_shortName)
        {
            optString += it->m_shortName;
            if (it->m_flags == Option::FLAG_REQUIRED_ARGUMENT)
                optString += ":";
            else if (it->m_flags == Option::FLAG_OPTIONAL_ARGUMENT)
                optString += "::";
        }
    }

    // The last element of the array has to be filled with zeros
    option lastElement;
    memset(&lastElement, 0, sizeof(lastElement));
    options.push_back(lastElement);

    while (!m_requestExit)
    {
        int optionIndex = 0;
        int c = getopt_long(argc, argv, optString.c_str(), options.data(), &optionIndex);

        if (c == -1)
            break;

        for (it = m_options.begin(); it < m_options.end(); ++it)
        {
            if (((c == 0) && (it->m_name == options[optionIndex].name)) ||
                ((c != 0) && (it->m_shortName == c)))
            {
                if (m_displayOptionChanges.get())
                {
                    displayOption(true, it, optarg);
                }
                if (it->m_function)
                {
                    PROPAGATE_ERROR(
                        it->m_function(it->m_userPtr, optarg ? optarg : it->m_defaultArgument));
                }
                break;
            }
        }

        if (it == m_options.end())
        {
            PROPAGATE_ERROR(usage());
            ORIGINATE_ERROR("Error parsing command line");
        }
    }

    return true;
}

/**
 * Print the usage message
 */
bool Options::usage()
{
    const Option::Type types[] =
    {
        Option::TYPE_OPTION,
        Option::TYPE_ACTION
    };
    const size_t lineLength = 80;
    const size_t optionLength = 24;
    const size_t helpTextLength = lineLength - optionLength;
    std::ostringstream usage;

    usage << "Usage: " << m_programName << " [OPTION]... [ACTION]... \n";
    usage << m_description;
    usage << "Options are set and actions are executed in the order they occur. Multiple\n";
    usage << "actions can be executed.\n";
    usage << "Mandatory arguments to long options are mandatory for short options too.\n";

    for (uint32_t typeIndex = 0; typeIndex < sizeof(types) / sizeof(types[0]); ++typeIndex)
    {
        const Option::Type type = types[typeIndex];

        if (type == Option::TYPE_OPTION)
            usage << "\nOptions:\n";
        else if (type == Option::TYPE_ACTION)
            usage << "\nActions:\n";
        else
            ORIGINATE_ERROR("Internal error, unhandled type\n");

        for (std::vector<Option>::iterator it = m_options.begin(); it < m_options.end(); ++it)
        {
            if (it->m_type == type)
            {
                std::ostringstream optionUsage;

                // print the short option
                if (it->m_shortName)
                    optionUsage << "  -" << (char)it->m_shortName << ", ";
                else
                    optionUsage << "      ";

                // print the long option
                optionUsage << "--" << it->m_name;

                if ((it->m_flags != Option::FLAG_NO_ARGUMENT) && it->m_argument.empty())
                    ORIGINATE_ERROR("Internal error, argument string required\n");

                if (it->m_flags == Option::FLAG_REQUIRED_ARGUMENT)
                    optionUsage << "=" << it->m_argument << "";
                else if (it->m_flags == Option::FLAG_OPTIONAL_ARGUMENT)
                    optionUsage << "[=" << it->m_argument << "]";

                // if the option is too long insert a new-line
                if (optionUsage.str().length() >= optionLength)
                {
                    optionUsage << std::endl;
                    optionUsage << std::string(optionLength, ' ');
                }
                else
                {
                    // insert spaces until option length limit
                    optionUsage << std::string(optionLength - optionUsage.str().length(), ' ');
                }
                usage << optionUsage.str();

                // Build the help text
                std::string helpText(it->m_usage);

                // split help message if needed
                while (helpText.length() > helpTextLength)
                {
                    // find the last space
                    size_t lastSpace = helpText.find_last_of(' ', helpTextLength);

                    // add until space
                    usage << helpText.substr(0, lastSpace) << std::endl <<
                        std::string(optionLength, ' ');
                    // continue with rest
                    helpText = helpText.substr(lastSpace + 1, std::string::npos);
                }

                usage << helpText << std::endl;
            }
        }
    }

    printf("%s", usage.str().c_str());

    return true;
}

}; // namespace ArgusSamples
