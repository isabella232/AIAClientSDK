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
 * @file aia_ux_state.h
 * @brief Declaration and utility functions of the @c
 * AiaUXState_t type.
 */

#ifndef AIA_UX_STATE_H_
#define AIA_UX_STATE_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <aia_capabilities_config.h>

/** Enumeration of the UX states that an Aia client can be in. */
typedef enum AiaUXState
{
    /** No active interaction. */
    AIA_UX_IDLE,

#ifdef AIA_ENABLE_MICROPHONE
    /** Microphone data is being streaming to the Aia service. */
    AIA_UX_LISTENING,
#endif

    /** The user has completed a request, the microphone is closed, and a
       response is pending. No additional voice input is accepted in this state.
     */
    AIA_UX_THINKING,

#ifdef AIA_ENABLE_SPEAKER
    /** TTS is being played through the speaker topic. This does not apply to
       long-running content, such as audiobooks or Flash Briefing. */
    AIA_UX_SPEAKING,
#endif

#ifdef AIA_ENABLE_ALERTS
    /** An alert is being played, either through the speaker topic or rendered
       locally in offline mode. */
    AIA_UX_ALERTING,
#endif

    /** A Notification is available to be played to the user. */
    AIA_UX_NOTIFICATION_AVAILABLE,

    /** The user has enabled Do Not Disturb mode. */
    AIA_UX_DO_NOT_DISTURB
} AiaUXState_t;

/**
 * Utility function used to convert a given @AiaUXState_t to a
 * string for logging purposes.
 *
 * @param state The state to convert.
 * @return The state as a string.
 */
static inline const char* AiaUXState_ToString( AiaUXState_t state )
{
    switch( state )
    {
        case AIA_UX_IDLE:
            return "IDLE";
#ifdef AIA_ENABLE_MICROPHONE
        case AIA_UX_LISTENING:
            return "LISTENING";
#endif
        case AIA_UX_THINKING:
            return "THINKING";
#ifdef AIA_ENABLE_SPEAKER
        case AIA_UX_SPEAKING:
            return "SPEAKING";
#endif
#ifdef AIA_ENABLE_ALERTS
        case AIA_UX_ALERTING:
            return "ALERTING";
#endif
        case AIA_UX_NOTIFICATION_AVAILABLE:
            return "NOTIFICATION_AVAILABLE";
        case AIA_UX_DO_NOT_DISTURB:
            return "DO_NOT_DISTURB";
    }
    return "UNKNOWN_UX_STATE";
}

#endif /* ifndef AIA_UX_STATE_H_ */
