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
 * @file aia_message_constants.h
 * @brief Constants related to Aia messages.
 */

#ifndef AIA_MESSAGE_CONSTANTS_H_
#define AIA_MESSAGE_CONSTANTS_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <stdint.h>

/** Type used to represent a Aia sequence number. */
typedef uint32_t AiaSequenceNumber_t;

/** The number of bytes needed for encryption initialization vector ( IV ). */
#define AIA_COMMON_HEADER_IV_SIZE ( (size_t)12 )

/** The number of bytes needed for encryption message authentication code ( MAC
 * ). */
#define AIA_COMMON_HEADER_MAC_SIZE ( (size_t)16 )

/** Offset of the encrypted sequence number in the common header. */
#define AIA_COMMON_HEADER_ENCRYPTED_SEQUENCE_OFFSET                           \
    ( ( size_t )( sizeof( AiaSequenceNumber_t ) + AIA_COMMON_HEADER_IV_SIZE + \
                  AIA_COMMON_HEADER_MAC_SIZE ) )

/** The total size of the common binary header */
static const size_t AIA_SIZE_OF_COMMON_HEADER =
    AIA_COMMON_HEADER_ENCRYPTED_SEQUENCE_OFFSET + sizeof( AiaSequenceNumber_t );

#endif /* ifndef AIA_MESSAGE_CONSTANTS_H_ */
