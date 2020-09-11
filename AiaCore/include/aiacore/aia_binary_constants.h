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
 * @file aia_binary_constants.h
 * @brief Constants related to Aia binary messages.
 */

#ifndef AIA_BINARY_CONSTANTS_H_
#define AIA_BINARY_CONSTANTS_H_

/* The config header is always included first. */
#include <aia_config.h>

/** Type used to represent the length field of a Aia binary stream. */
typedef uint32_t AiaBinaryMessageLength_t;

/** Type used to represent the type field of a Aia binary stream. */
typedef uint8_t AiaBinaryMessageType_t;

/** Type used to represent the count field of a Aia binary stream. */
typedef uint8_t AiaBinaryMessageCount_t;

/** The number of reserved bytes in a Aia binary stream header. */
#define AIA_BINARY_MESSAGE_NUM_RESERVED_BYTES ( (size_t)2 )

/** The total size of a Aia binary stream header. */
static const size_t AIA_SIZE_OF_BINARY_STREAM_HEADER =
    sizeof( AiaBinaryMessageLength_t ) + sizeof( AiaBinaryMessageType_t ) +
    sizeof( AiaBinaryMessageCount_t ) + AIA_BINARY_MESSAGE_NUM_RESERVED_BYTES;

/** An enumeration of all types of Aia binary streams. */
typedef enum AiaBinaryStreamSpeakerMessageType
{
    /** A type indicating a Aia binary speaker content message. */
    AIA_BINARY_STREAM_SPEAKER_CONTENT_TYPE = 0,

    /** A type indicating a Aia binary speaker marker message. */
    AIA_BINARY_STREAM_SPEAKER_MARKER_TYPE = 1,
} AiaBinaryStreamSpeakerMessageType_t;

/** Type used to represent the offset field of a Aia binary audio stream. */
typedef uint64_t AiaBinaryAudioStreamOffset_t;

/** Type used to represent markers embedded within a Aia binary stream. */
typedef uint32_t AiaSpeakerBinaryMarker_t;

/** An enumeration of all types of Aia microphone binary streams. */
typedef enum AiaBinaryStreamMicrophoneMessageType
{
    /** A type indicating a Aia binary microphone content message. */
    AIA_BINARY_STREAM_MICROPHONE_CONTENT_TYPE = 0,

    /** A type indicating a Aia binary microphone wakeword metadata type. */
    AIA_BINARY_STREAM_MICROPHONE_WAKEWORD_METADATA_TYPE = 1
} AiaBinaryStreamMicrophoneMessageType_t;

#endif /* ifndef AIA_BINARY_CONSTANTS_H_ */
