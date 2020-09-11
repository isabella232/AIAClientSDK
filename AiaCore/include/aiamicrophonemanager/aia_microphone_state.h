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
 * @file aia_microphone_state.h
 * @brief Declaration and utility functions of the @c
 * AiaMicrophoneState_t type.
 */

#ifndef AIA_MICROPHONE_STATE_H_
#define AIA_MICROPHONE_STATE_H_

/* The config header is always included first. */
#include <aia_config.h>

/**
 * Enumeration of the current state of the @c AiaMicrophoneManager_t.
 */
typedef enum AiaMicrophoneState
{
    /** Microphone data not currently streaming. */
    AIA_MICROPHONE_STATE_CLOSED,

    /** Microphone data currently streaming. */
    AIA_MICROPHONE_STATE_OPEN
} AiaMicrophoneState_t;

/**
 * Utility function used to convert a given @AiaMicrophoneState_t to a
 * string for logging purposes.
 *
 * @param state The state to convert.
 * @return The state as a string or @c NULL on failure.
 */
static inline const char* AiaMicrophoneState_ToString(
    AiaMicrophoneState_t state )
{
    switch( state )
    {
        case AIA_MICROPHONE_STATE_CLOSED:
            return "MICROPHONE_CLOSED";
        case AIA_MICROPHONE_STATE_OPEN:
            return "MICROPHONE_OPEN";
    }
    return "UNKNOWN_MICROPHONE_STATE";
}

#endif /* ifndef AIA_MICROPHONE_STATE_H_ */
