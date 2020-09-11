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
 * @file aia_message.h
 * @brief Internal header for the AiaMessage_t type. This header should not be
 * directly included in typical application code.
 */

#ifndef PRIVATE_AIA_MESSAGE_H_
#define PRIVATE_AIA_MESSAGE_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <stdbool.h>
#include <stddef.h>

/**
 * Base type for all messages.
 *
 * @note Functions in this header may not be thread-safe.  Users are required to
 * provide external synchronization.
 */
struct AiaMessage
{
    /** Size (in bytes) this message will occupy when assembled. */
    size_t size;
};

/**
 * Initializes a @c Message_t.
 *
 * @param message Pointer to a caller-owned @c AiaMessage_t to initialize.
 * @param size The size (in bytes) that this message will occupy when assembled.
 * @return @c true if @c message was initialized successfuly, else @c false.
 */
bool _AiaMessage_Initialize( struct AiaMessage* message, size_t size );

/**
 * Uninitializes an @c AiaMessage_t.  This will free up any internally allocated
 * resources so that @c message can be safely deallocated and discarded by the
 * caller without leaking memory.
 *
 * @param message Pointer to a caller-owned @c Message_t to uninitialize.
 */
void _AiaMessage_Uninitialize( struct AiaMessage* message );

#endif /* ifndef PRIVATE_AIA_MESSAGE_H_ */
