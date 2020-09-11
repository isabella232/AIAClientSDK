/*
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file aia_button_command.h
 * @brief Constants related to button commands.
 */

#ifndef AIA_BUTTON_COMMAND_H
#define AIA_BUTTON_COMMAND_H

/* The config header is always included first. */
#include <aia_config.h>

#include <aia_capabilities_config.h>

/**
 * Possible button commands. @see
 * https://developer.amazon.com/en-US/docs/alexa/alexa-voice-service/avs-for-aws-iot-speaker.html#buttoncommandissued.
 */
typedef enum AiaButtonCommand
{
    AIA_BUTTON_PLAY,
    AIA_BUTTON_NEXT,
    AIA_BUTTON_PREVIOUS,
    AIA_BUTTON_STOP,
    AIA_BUTTON_PAUSE
} AiaButtonCommand_t;

/**
 * Utility function used to convert a given @c AiaButtonCommand_t to a
 * string for logging and message sending purposes.
 *
 * @param button The button to convert.
 * @return The button as a C-string.
 */
static inline const char* AiaButtonCommand_ToString( AiaButtonCommand_t button )
{
    switch( button )
    {
        case AIA_BUTTON_PLAY:
            return "PLAY";
        case AIA_BUTTON_NEXT:
            return "NEXT";
        case AIA_BUTTON_PREVIOUS:
            return "PREVIOUS";
        case AIA_BUTTON_STOP:
            return "STOP";
        case AIA_BUTTON_PAUSE:
            return "PAUSE";
    }
    AiaLogError( "Unknown button %d.", button );
    AiaAssert( false );
    return "";
}

#endif /* ifndef AIA_BUTTON_COMMAND_H */
