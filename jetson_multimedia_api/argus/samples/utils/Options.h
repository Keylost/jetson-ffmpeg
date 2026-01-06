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

#ifndef OPTIONS_H
#define OPTIONS_H

#include <vector>
#include <string>

#include "Value.h"

namespace ArgusSamples
{

/**
 * Handles command line options.
 */
class Options
{
public:
    explicit Options(const char *programName);
    virtual ~Options();

    bool initialize();

    /**
     * Defines a single option
     */
    class Option
    {
    public:
        /**
         * Type
         */
        typedef enum
        {
            TYPE_ACTION,    ///< triggers an action
            TYPE_OPTION     ///< sets an option
        } Type;

        /**
         * Argument flags
         */
        typedef enum
        {
            FLAG_NO_ARGUMENT,       ///< requires no argument
            FLAG_OPTIONAL_ARGUMENT, ///< optionally takes an argument
            FLAG_REQUIRED_ARGUMENT  ///< requires an argument
        } Flag;

        /**
         * Call back function.
         *
         * @param [in] usrPtr   user pointer
         * @param [in] optArg   optional argument string, NULL when there is no argument
         */
        typedef bool (*CallBackFunc)(void *usrPtr, const char *optArg);

        /**
         * ToString Call back function.
         *
         * @param [in] varPtr   optional argument string, NULL when there is no argument
         */
        typedef std::string (*ToStringCallBackFunc)(void *varPtr);

        /**
         * Construct an option
         *
         * @param name [in] long option name
         * @param shortName [in] short option name
         * @param argument [in] argument
         * @param type [in] option type
         * @param flags [in] flags
         * @param usage [in] a string describing the usage
         * @param function [in] callback function
         * @param userPtr [in] user pointer
         * @param defaultArgument [in] provides value if none provided by user
         */
        explicit Option(std::string name, char shortName, std::string argument, Type type,
                std::string usage, CallBackFunc function, void *userPtr = NULL,
                const char* defaultArgument = NULL)
            : m_name(name)
            , m_shortName(shortName)
            , m_argument(argument)
            , m_type(type)
            , m_usage(usage)
            , m_function(function)
            , m_userPtr(userPtr)
            , m_toStrFunc(NULL)
            , m_defaultArgument(defaultArgument)
        {
            if (argument.empty())
            {
                m_flags = FLAG_NO_ARGUMENT;
            }
            else
            {
                if (defaultArgument)
                {
                    m_flags = FLAG_OPTIONAL_ARGUMENT;
                }
                else
                {
                    m_flags = FLAG_REQUIRED_ARGUMENT;
                }
            }

            if (argument.empty())
            {
                if (m_type == Option::TYPE_OPTION)
                {
                    m_usage.append(" Set to '").append(std::string(defaultArgument)).append("'.");
                }
            }
            else
            {
                if (defaultArgument)
                {
                    m_usage.append(" Default argument is '").
                        append(std::string(defaultArgument)).append("'.");
                }
            }
        }

        /**
         * Construct an option from a Value. The final usage string is built from the given usage
         * and the default and valid values of the given value variable.
         *
         * @param name [in] long option name
         * @param shortName [in] short option name
         * @param argument [in] argument
         * @param value [in] value to construct the option from
         * @param usage [in] a string describing the usage
         * @param function [in] callback function
         * @param userPtr [in] user pointer
         * @param defaultArgument [in] provides value if none provided by user
         */
        template<typename T> explicit Option(std::string name, char shortName,
            std::string argument, Value<T> &value, std::string usage, CallBackFunc function,
            void *userPtr = NULL, ToStringCallBackFunc toStrFunc = NULL,
            const char* defaultArgument = NULL)
            : m_name(name)
            , m_shortName(shortName)
            , m_argument(argument)
            , m_type(TYPE_OPTION)
            , m_function(function)
            , m_userPtr(userPtr)
            , m_toStrFunc(toStrFunc)
            , m_defaultArgument(defaultArgument)
        {
            if (argument.empty())
            {
                m_flags = FLAG_NO_ARGUMENT;
            }
            else
            {
                if (defaultArgument)
                {
                    m_flags = FLAG_OPTIONAL_ARGUMENT;
                }
                else
                {
                    m_flags = FLAG_REQUIRED_ARGUMENT;
                }
            }

            m_usage = usage.append(" ").append(value.getValidator()->getValidValuesMessage());
            if (m_type == Option::TYPE_OPTION)
            {
                m_usage.append(" Default is '").append(value.toString()).append("'.");
            }

            if (argument.empty())
            {
                if (m_type == Option::TYPE_OPTION)
                {
                    m_usage.append(" Set to '").append(std::string(defaultArgument)).append("'.");
                }
            }
            else
            {
                if (defaultArgument)
                {
                    m_usage.append(" Default argument is '").
                        append(std::string(defaultArgument)).append("'.");
                }
            }
        }
        ~Option()
        {
        }

        std::string m_name;             //!< option name
        char m_shortName;               //!< option short name
        std::string m_argument;         //!< argument name
        Type m_type;                    //!< option type
        Flag m_flags;                   //!< option flags
        std::string m_usage;            //!< usage message
        CallBackFunc m_function;        //!< callback function
        void *m_userPtr;                //!< user pointer passed to callback function
        ToStringCallBackFunc m_toStrFunc;   //!< callback to display current variable value
        const char *m_defaultArgument;  //!< default value for no param or missing optional param
    };

    /**
     * Print the usage message.
     */
    bool usage();

    /**
     * Parse the command line options
     *
     * @param [in] argc     argument count
     * @param [in] argv     argument values
     */
    virtual bool parse(const int argc, char * const *argv);

    /**
     * Add a option
     *
     * @param [in] option   option to add
     * @param [in] userPtr  user pointer passed to callback function
     */
    bool addOption(const Option &option, void *userPtr = NULL);

    /**
     * Add multiple options
     *
     * @param [in] count    how many options to add
     * @param [in] options  option array
     * @param [in] userPtr  user pointer passed to callback functions
     */
    bool addOptions(size_t count, const Option *options, void *userPtr = NULL);

    /**
     * Add test to the description, will be printed with the usage message
     *
     * @param [in] description  Description
     */
    bool addDescription(const char *description);

    /**
     * Request exit after the current option, called from callback function.
     */
    bool requestExit();

    /**
     * Has the exit been requested?
     */
    bool requestedExit() const;

    /**
     * help option callback
     */
    static bool printHelp(void *userPtr, const char *optArg);

    /**
     * exit option callback
     */
    static bool exit(void *userPtr, const char *optArg);

    /**
     * Display Changed Option and associated value
     */
    static void displayOption(bool displayType, std::vector<Option>::iterator &it, const char *optArg);

    /**
     * Display Options and assocciated values
     */
    static bool displayOptionsAndValues(void *userPtr, const char *optArg);

private:
    bool m_initialized;
    bool m_requestExit;
    std::string m_programName;
    std::string m_description;
    std::vector<Option> m_options;
    Value<bool> m_displayOptionChanges;     ///< follow all options

    /**
     * Hide default constructor
     */
    Options();
};

/**
 * A callback function that sets a Value object from the argument.
 * @param[in] usrPtr A pointer to the Value object whose value should be set.
 * @param[in] optArg The string form of the value to set,
 * which will be evaluated based on the type T.
 */
template<typename T> bool valueCallback(void *usrPtr, const char *optArg)
{
    Value<T>* value = static_cast<Value<T>*>(usrPtr);
    PROPAGATE_ERROR(value->setFromString(optArg));
    return true;
}

/**
 * A callback function that gets a Value object
 * @param[in] usrPtr A pointer to the Value object whose value should be set.
 * which will be evaluated based on the type T.
 */
template<typename T> std::string valueStringCallback(void *usrPtr)
{
    return static_cast<Value<T>*>(usrPtr)->toString();
}

/**
 * A helper function to create an Option that uses valueCallback().
 * Such an option will simply write its argument value to the @result parameter.
 * @param[in] name Long option name
 * @param[in] shortName Short option name
 * @param[in] argument Argument description
 * @param[in] usage A string describing the usage
 * @param[in,out] result The output location to receive the argument value.
 * This Value object is also used to construct the Option usage text.
 * @returns The Option object.
 */
template<typename T> Options::Option createValueOption(std::string name, char shortName,
    std::string argument, std::string usage, Value<T> &result, const char* defaultArgument = NULL)
{
    return Options::Option(name, shortName, argument, result, usage,
        valueCallback<T>, &result, valueStringCallback<T>, defaultArgument);
}


}; // namespace ArgusSamples

#endif // OPTIONS_H
