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
 * @brief User-facing functions of the @c AiaJsonMessage_t type.
 */

#ifndef AIA_JSON_MESSAGE_H_
#define AIA_JSON_MESSAGE_H_

/* The config header is always included first. */
#include <aia_config.h>

#include "aia_message.h"

#include <stdbool.h>
#include <stddef.h>

/**
 * This type is used to hold an unencrypted JSON Aia message.
 *
 * @note Functions in this header may not be thread-safe.  Users are required to
 * provide external synchronization.
 */
typedef struct AiaJsonMessage AiaJsonMessage_t;

/**
 * @name Casting functions
 *
 * The functions in this group can be used to perform static casts back and
 * forth between the AiaMessage_t and AiaJsonMessage_t types.
 *
 * @note These casting functions do not change ownership of the original object,
 * and the caller is responsible for ensuring that the object is released with a
 * call to @c AiaJsonMessage_Destroy().
 */
/** @{ */
inline AiaMessage_t* AiaJsonMessage_ToMessage( AiaJsonMessage_t* jsonMessage )
{
    return (AiaMessage_t*)jsonMessage;
}

inline const AiaMessage_t* AiaJsonMessage_ToConstMessage(
    const AiaJsonMessage_t* jsonMessage )
{
    return (const AiaMessage_t*)jsonMessage;
}

inline AiaJsonMessage_t* AiaJsonMessage_FromMessage( AiaMessage_t* message )
{
    return (AiaJsonMessage_t*)message;
}

inline const AiaJsonMessage_t* ConstAiaJsonMessage_FromMessage(
    const AiaMessage_t* message )
{
    return (const AiaJsonMessage_t*)message;
}
/** @} */

/**
 * Allocates and initializes a new JSON Message from the heap.  Messages created
 * with this function should be released using @c AiaJsonMessage_Destroy().
 *
 * @param name The "name" value of the JSON header subsection.
 * @param messageId The "messageId" value of the JSON header subsection.
 * @param payload The value of the "payload" subsection of the JSON message.  In
 * the case that no "payload" is given in the message, a NULL should be passed
 * in.
 * @return The newly created JSON message if successful, else NULL.
 */
AiaJsonMessage_t* AiaJsonMessage_Create( const char* name,
                                         const char* messageId,
                                         const char* payload );

/**
 * Uninitializes and deallocates a JSON message previously created by a call to
 * @c AiaJsonMessage_Create().
 *
 * @param jsonMessage The JSON message to destroy.
 */
void AiaJsonMessage_Destroy( AiaJsonMessage_t* jsonMessage );

/**
 * Returns the "name" value of the "header" subsection of the JSON message.
 *
 * @param jsonMessage The JSON message to act on.
 * @return The "name" value of the "header" subsection of the JSON message.
 */
const char* AiaJsonMessage_GetName( const AiaJsonMessage_t* jsonMessage );

/**
 * Returns the "messageId" value of the "header" subsection of the JSON message.
 *
 * @param jsonMessage The JSON message to act on.
 * @return The "messageId" value of the "header" subsection of the JSON message.
 */
const char* AiaJsonMessage_GetMessageId( const AiaJsonMessage_t* jsonMessage );

/**
 * Returns the "payload" subsection of the JSON message. Note that in the case
 * that no payload is a part of this message, NULL will be returned.
 *
 * @param jsonMessage The JSON message to act on.
 * @return The "payload" subsection of the JSON message, or NULL if there is no
 * payload.
 */
const char* AiaJsonMessage_GetJsonPayload(
    const AiaJsonMessage_t* jsonMessage );

/**
 * Assembles the unencrypted @c AiaMessage payload in the user-provided
 * buffer.  @c AiaMessage_GetSize() can be used to determine the minimum size
 * required for @c messageBuffer.
 *
 * @note This function will not null-terminate @c messageBuffer if it is exactly
 *     @c AiaMessage_GetSize() bytes.  If at least `AiaMessage_GetSize() + 1`
 *     bytes are provided in @c messageBuffer, this function will
 *     null-terminate the string.
 *
 * @param jsonMessage The JSON message to act on.
 * @param [out] messageBuffer A user-provided buffer of at least @c
 * AiaMessage_GetSize() bytes to write the message into.
 * @param messageBufferSize The size (in bytes) of @c messageBuffer.
 * @return @c true if the message was built successfully, else @c false.
 */
bool AiaJsonMessage_BuildMessage( const AiaJsonMessage_t* jsonMessage,
                                  char* messageBuffer,
                                  size_t messageBufferSize );

#endif /* ifndef AIA_JSON_MESSAGE_H_ */
