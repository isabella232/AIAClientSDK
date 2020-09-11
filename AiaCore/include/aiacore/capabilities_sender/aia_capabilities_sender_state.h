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
 * @file aia_capabilities_sender_state.h
 * @brief Declaration and utility functions of the @c
 * AiaCapabilitiesSenderState_t type.
 */

#ifndef AIA_CAPABILITIES_SENDER_STATE_H_
#define AIA_CAPABILITIES_SENDER_STATE_H_

/* The config header is always included first. */
#include <aia_config.h>

/** Enumeration of the states that a @c AiaCapabilitiesSender_t can be in. */
typedef enum AiaCapabilitiesSenderState
{
    /** A default state, nothing has occurred. */
    AIA_CAPABILITIES_STATE_NONE,

    /** Capabilities have been published but an acknowledgement has not yet been
       received. */
    AIA_CAPABILITIES_STATE_PUBLISHED,

    /** Published capabilities have been accepted. */
    AIA_CAPABILITIES_STATE_ACCEPTED,

    /** Published capabilities have been rejected. */
    AIA_CAPABILITIES_STATE_REJECTED
} AiaCapabilitiesSenderState_t;

/**
 * Utility function used to convert a given @AiaCapabilitiesSenderState_t to a
 * string for logging purposes.
 *
 * @param state The state to convert.
 * @return The state as a string or @c NULL on failure.
 */
static inline const char* AiaCapabilitiesSenderState_ToString(
    AiaCapabilitiesSenderState_t state )
{
    switch( state )
    {
        case AIA_CAPABILITIES_STATE_NONE:
            return "AIA_CAPABILITIES_STATE_NONE";
        case AIA_CAPABILITIES_STATE_PUBLISHED:
            return "AIA_CAPABILITIES_STATE_PUBLISHED";
        case AIA_CAPABILITIES_STATE_ACCEPTED:
            return "AIA_CAPABILITIES_STATE_ACCEPTED";
        case AIA_CAPABILITIES_STATE_REJECTED:
            return "AIA_CAPABILITIES_STATE_REJECTED";
    }
    return NULL;
}

#endif /* ifndef AIA_CAPABILITIES_SENDER_STATE_H_ */
