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
 * @file aia_microphone_constants.h
 * @brief Constants related to Aia binary messages.
 */

#ifndef AIA_MICROPHONE_CONSTANTS_H_
#define AIA_MICROPHONE_CONSTANTS_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <aiacore/aia_utils.h>

/** The Automatic Speech Recognition (ASR) profile associated with the device.
 * These three distinct ASR profiles are optimized
 * for user speech from varying distances. */
typedef enum AiaMicrophoneProfile
{
    /** 0-2.5 ft. */
    AIA_MICROPHONE_PROFILE_CLOSE_TALK,

    /** 0-5 ft. */
    AIA_MICROPHONE_PROFILE_NEAR_FIELD,

    /** 0-20+ ft. */
    AIA_MICROPHONE_PROFILE_FAR_FIELD
} AiaMicrophoneProfile_t;

/**
 * Utility function used to convert a given @AiaMicrophoneProfile_t to a
 * string for logging and message sending purposes.
 *
 * @param error The profile to convert.
 * @return The state as a string or @c NULL on failure.
 */
static inline const char* AiaMicrophoneProfile_ToString(
    AiaMicrophoneProfile_t profile )
{
    switch( profile )
    {
        case AIA_MICROPHONE_PROFILE_CLOSE_TALK:
            return "CLOSE_TALK";
        case AIA_MICROPHONE_PROFILE_NEAR_FIELD:
            return "NEAR_FIELD";
        case AIA_MICROPHONE_PROFILE_FAR_FIELD:
            return "FAR_FIELD";
    }
    return NULL;
}

/** Represents the action taken by the end user to start an AIS microphone-based
 * interaction. */
typedef enum AiaMicrophoneInitiatorType
{
    /** Audio stream initiated by pressing a button (physical or GUI) and
       terminated by releasing it. Note that the only supported profile for this
       mode is @c AIA_MICROPHONE_PROFILE_CLOSE_TALK. */
    AIA_MICROPHONE_INITIATOR_TYPE_HOLD,

    /** Audio stream initiated by the tap and release of a button (physical or
       GUI) and terminated when a CloseMicrophone directive is received. Note
       that the only supported profiles for this mode are @c
       AIA_MICROPHONE_PROFILE_NEAR_FIELD and @c
       AIA_MICROPHONE_PROFILE_FAR_FIELD. */
    AIA_MICROPHONE_INITIATOR_TYPE_TAP,

    /** Audio stream initiated by the use of a wake word and terminated when a
       CloseMicrophone directive is received. Note that the only supported
       profiles for this mode are @c AIA_MICROPHONE_PROFILE_NEAR_FIELD and @c
       AIA_MICROPHONE_PROFILE_FAR_FIELD. */
    AIA_MICROPHONE_INITIATOR_TYPE_WAKEWORD
} AiaMicrophoneInitiatorType_t;

/**
 * Utility function used to convert a given @AiaMicrophoneInitiatorType_t to a
 * string for logging and message sending purposes.
 *
 * @param initiatorType The initiator type to convert.
 * @return The state as a string or @c NULL on failure.
 */
static inline const char* AiaMicrophoneInitiatorType_ToString(
    AiaMicrophoneInitiatorType_t initiatorType )
{
    switch( initiatorType )
    {
        case AIA_MICROPHONE_INITIATOR_TYPE_HOLD:
            return "PRESS_AND_HOLD";
        case AIA_MICROPHONE_INITIATOR_TYPE_TAP:
            return "TAP";
        case AIA_MICROPHONE_INITIATOR_TYPE_WAKEWORD:
            return "WAKEWORD";
    }
    return NULL;
}

/** The only wake word accpeted by the Aia cloud */
#define AIA_ALEXA_WAKE_WORD "ALEXA"

/** 16000 samples per second. */
#define AIA_MICROPHONE_SAMPLE_RATE_HZ ( (size_t)16000 )

/** 16 bits per sample. */
#define AIA_MICROPHONE_BITS_PER_SAMPLE ( (size_t)16 )

/** Bytes per sample (word size of microphone buffer). */
#define AIA_MICROPHONE_BUFFER_WORD_SIZE \
    ( ( size_t )( AIA_MICROPHONE_BITS_PER_SAMPLE / 8 ) )

/** Amount of pre-roll to send for wake-word interactions. */
#define AIA_MICROPHONE_WAKE_WORD_PREROLL ( (AiaDurationMs_t)500 )

/** @c PREROLL in samples. */
static const size_t AIA_MICROPHONE_WAKE_WORD_PREROLL_IN_SAMPLES =
    AIA_MICROPHONE_WAKE_WORD_PREROLL * AIA_MICROPHONE_SAMPLE_RATE_HZ /
    AIA_MS_PER_SECOND;

#endif /* ifndef AIA_MICROPHONE_CONSTANTS_H_ */
