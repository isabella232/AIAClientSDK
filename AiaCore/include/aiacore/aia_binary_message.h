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
 * @file aia_binary_message.h
 * @brief User-facing functions of the @c AiaBinaryMessage_t type.
 */

#ifndef AIA_BINARY_MESSAGE_H_
#define AIA_BINARY_MESSAGE_H_

/* The config header is always included first. */
#include <aia_config.h>

#include "aia_binary_constants.h"
#include "aia_message.h"

#include <stdbool.h>

/**
 * This type is used to hold an unencrypted Aia binary message.
 *
 * @note Functions in this header may not be thread-safe.  Users are required to
 * provide external synchronization.
 */
typedef struct AiaBinaryMessage AiaBinaryMessage_t;

/**
 * @name Casting functions
 *
 * The functions in this group can be used to perform static casts back and
 * forth between the AiaMessage_t and AiaBinaryMessage_t types.
 *
 * @note These casting functions do not change ownership of the original object,
 * and the caller is responsible for ensuring that the object is released with a
 * call to @c AiaBinaryMessage_Destroy().
 */
/** @{ */
inline AiaMessage_t* AiaBinaryMessage_ToMessage(
    AiaBinaryMessage_t* binaryMessage )
{
    return (AiaMessage_t*)binaryMessage;
}

inline const AiaMessage_t* AiaBinaryMessage_ToConstMessage(
    const AiaBinaryMessage_t* binaryMessage )
{
    return (const AiaMessage_t*)binaryMessage;
}

inline AiaBinaryMessage_t* AiaBinaryMessage_FromMessage( AiaMessage_t* message )
{
    return (AiaBinaryMessage_t*)message;
}

inline const AiaBinaryMessage_t* ConstAiaBinaryMessage_FromMessage(
    const AiaMessage_t* message )
{
    return (const AiaBinaryMessage_t*)message;
}
/** @} */

/**
 * Allocates and initializes a new binary Message from the heap.  Messages
 * created with this function should be released using @c
 * AiaBinaryMessage_Destroy().
 *
 * @param length The length of @c data.
 * @param type The "type" of this binary stream message.
 * @param count The number of binary stream data chunks included in this
 * message.
 * @param data Data associated with this binary stream message. This object will
 * store a pointer to this data. Note that ownership of @c data is transferred
 * upon successful allocation of a @c AiaBinaryMessage_t. This data will be
 * freed using @c AiaFree on calls to @c AiaBinaryMessage_Destroy().
 * @return The newly created binary message if successful, else NULL.
 */
AiaBinaryMessage_t* AiaBinaryMessage_Create( AiaBinaryMessageLength_t length,
                                             AiaBinaryMessageType_t type,
                                             AiaBinaryMessageCount_t count,
                                             void* data );

/**
 * Uninitializes and deallocates a binary message previously created by a call
 * to
 * @c AiaBinaryMessage_Create().
 *
 * @param binaryMessage The binary message to destroy.
 */
void AiaBinaryMessage_Destroy( AiaBinaryMessage_t* binaryMessage );

/**
 * Returns the "length" field of the binary message.
 *
 * @param binaryMessage The binary message to act on.
 * @return The "length" field of the binary message or 0 on failure.
 */
AiaBinaryMessageLength_t AiaBinaryMessage_GetLength(
    const AiaBinaryMessage_t* binaryMessage );

/**
 * Returns the "type" field of the binary message.
 *
 * @param binaryMessage The binary message to act on.
 * @return The "type" field of the binary message. Callers may deduce failures
 * from failures resulting in calling @c AiaBinaryMessage_GetLength.
 */
AiaBinaryMessageType_t AiaBinaryMessage_GetType(
    const AiaBinaryMessage_t* binaryMessage );

/**
 * Returns the "count" field of the binary message.
 *
 * @param binaryMessage The binary message to act on.
 * @return The "count" field of the binary message. Callers may deduce failures
 * from failures resulting in calling @c AiaBinaryMessage_GetLength.
 */
AiaBinaryMessageCount_t AiaBinaryMessage_GetCount(
    const AiaBinaryMessage_t* binaryMessage );

/**
 * Returns the "data" of the binary message.
 *
 * @param binaryMessage The binary message to act on.
 * @return The "data" of the binary message or @c NULL on failure.
 * @note Modifications to the data returned will impact the data that the @c
 * binaryMessage points to.
 */
const void* AiaBinaryMessage_GetData( const AiaBinaryMessage_t* binaryMessage );

/**
 * Assembles the unencrypted @c AiaMessage payload in the user-provided
 * buffer.  @c AiaMessage_GetSize() can be used to determine the minimum size
 * required for @c messageBuffer.
 *
 * @param binaryMessage The binary message to act on.
 * @param [out] messageBuffer A user-provided buffer of at least @c
 * AiaMessage_GetSize() bytes to write the message into.
 * @param messageBufferSize The size (in bytes) of @c messageBuffer.
 * @return @c true if the message was built successfully, else @c false.
 */
bool AiaBinaryMessage_BuildMessage( const AiaBinaryMessage_t* binaryMessage,
                                    uint8_t* messageBuffer,
                                    size_t messageBufferSize );

#endif /* ifndef AIA_BINARY_MESSAGE_H_ */
