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
 * @file aia_json_message.h
 * @brief Internal header for the AiaJsonMessage_t type. This header should not
 * be directly included in typical application code.
 */

#ifndef PRIVATE_AIA_JSON_MESSAGE_H_
#define PRIVATE_AIA_JSON_MESSAGE_H_

/* The config header is always included first. */
#include <aia_config.h>

#include "aia_message.h"

#include <stdbool.h>

/**
 * Base type for all JSON messages.
 *
 * @note Functions in this header may not be thread-safe.  Users are required to
 * provide external synchronization.
 */
struct AiaJsonMessage
{
    /** The underlying abstract @c AiaMessage type. */
    struct AiaMessage message;

    /** The "name" value of the "header" subsection of the JSON message. */
    const char* name;

    /** The "messageId" value of the "header" subsection of the JSON message. */
    const char* messageId;

    /**
     * The "payload" subsection of the JSON message.  This should be a complete
     * JSON object.
     */
    const char* payload;
};

/**
 * Initializes a @c AiaJsonMessage_t.
 *
 * @param jsonMessage Pointer to a caller-owned @c AiaJsonMessage_t to
 *     initialize.
 * @param name The "name" value of the JSON header subsection.
 * @param messageId The "messageId" value of the JSON header subsection.  If
 *     @c messageId is @c NULL, this function will automatically generate a
 *     random message ID.
 * @param payload The value of the "payload" subsection of the JSON message.  In
 *     the case that no "payload" is given in the message, a NULL should be
 *     passed in.
 * @return @c true if @c message was initialized successfully, else @c false.
 */
bool _AiaJsonMessage_Initialize( struct AiaJsonMessage* jsonMessage,
                                 const char* name, const char* messageId,
                                 const char* payload );

/**
 * Uninitializes a @c AiaJsonMessage_t.  This will free up any internally
 * allocated resources so that @c jsonMessage can be safely deallocated and
 * discarded by the caller without leaking memory.
 *
 * @param message Pointer to a caller-owned @c AiaJsonMessage_t to uninitialize.
 */
void _AiaJsonMessage_Uninitialize( struct AiaJsonMessage* jsonMessage );

#endif /* ifndef PRIVATE_AIA_JSON_MESSAGE_H_ */
