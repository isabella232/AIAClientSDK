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
 * @brief User-facing functions of the @c AiaMessage_t type.
 */

#ifndef AIA_MESSAGE_H_
#define AIA_MESSAGE_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <stddef.h>

/**
 * This type is used to hold an unencrypted abstract Aia message.
 *
 * @note Functions in this header may not be thread-safe.  Users are required to
 * provide external synchronization.
 */
typedef struct AiaMessage AiaMessage_t;

/**
 * Returns the size of this message.
 *
 * @param message The message to get the size of.
 * @return The size (in bytes) of this message.
 */
size_t AiaMessage_GetSize( const AiaMessage_t* message );

#endif /* ifndef AIA_MESSAGE_H_ */
