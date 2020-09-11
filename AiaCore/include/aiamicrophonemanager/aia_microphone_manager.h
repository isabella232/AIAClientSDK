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
 * @file aia_microphone_manager.h
 * @brief User-facing functions of the @c AiaMicrophoneManager_t type.
 */

#ifndef AIA_MICROPHONE_MANAGER_H_
#define AIA_MICROPHONE_MANAGER_H_

/* The config header is always included first. */
#include <aia_config.h>

#include "aia_microphone_constants.h"
#include "aia_microphone_state.h"

#include <aiacore/aia_binary_constants.h>
#include <aiacore/aia_message_constants.h>
#include <aiacore/data_stream_buffer/aia_data_stream_buffer_reader.h>
#include <aiaregulator/aia_regulator.h>

/**
 * This type is used to manage the microphone data flowing through the system.
 * It provides an interface for applications to interact with to initiate user
 * interactions. It will stream microphone data chunks to the Aia cloud
 * until stopped via a local command or via a cloud directive.
 */
typedef struct AiaMicrophoneManager AiaMicrophoneManager_t;

/**
 * This function will be called to notify of @c AiaMicrophoneState_t changes.
 * Implementations are expected to be non-blocking.
 *
 * @param state The new state.
 * @param userData Context to be passed with this callback.
 * @note Calling back into the @c AiaMicrophoneManager_t from within the same
 * execution context of this callback will result in a deadlock.
 */
typedef void ( *AiaMicrophoneStateObserver_t )( AiaMicrophoneState_t state,
                                                void* userData );

/**
 * Allocates and initializes a @c AiaMicrophoneManager_t object from the heap.
 * The returned pointer should be destroyed using @c
 * AiaMicrophoneManager_Destroy().
 *
 * @param eventRegulator Used to publish outbound events.
 * @param microphoneRegulator Used to publish microphone binary messages.
 * @param microphoneBufferReader Reader used to stream microphone data to the
 * Aia cloud on user interactions. Data contained in the underlying buffer
 * must be in 16-bit linear PCM, 16-kHz sample rate, single channel,
 * little-endian byte order format.
 * @param stateObserver An optional observer that can be used to observe
 * microphone state changes.
 * @Param stateObserverUserData Context to be passed to @c stateObserver.
 * @return The newly created @c AiaMicrophoneManager_t if successful, or NULL
 * otherwise.
 */
AiaMicrophoneManager_t* AiaMicrophoneManager_Create(
    AiaRegulator_t* eventRegulator, AiaRegulator_t* microphoneRegulator,
    AiaDataStreamReader_t* microphoneBufferReader,
    AiaMicrophoneStateObserver_t stateObserver, void* stateObserverUserData );

/**
 * Uninitializes and deallocates an @c AiaMicrophoneManager_t previously created
 * by a call to @c AiaMicrophoneManager_Create().
 *
 * @param microphoneManager The @c AiaMicrophoneManager_t to destroy.
 */
void AiaMicrophonerManager_Destroy( AiaMicrophoneManager_t* microphoneManager );

/**
 * Begins a hold to talk initiated Alexa interaction.
 *
 * @param microphoneManager The @c AiaMicrophoneManager_t to act on.
 * @param index The sample index that @c microphoneBufferReader should
 * begin streaming from.
 */
bool AiaMicrophoneManager_HoldToTalkStart(
    AiaMicrophoneManager_t* microphoneManager,
    AiaBinaryAudioStreamOffset_t index );

/**
 * Ends a hold to talk interaction by stopping the streaming of audio data sent
 * to the Aia cloud.
 *
 * @param microphoneManager The @c AiaMicrophoneManager_t to act on.
 */
void AiaMicrophoneManager_CloseMicrophone(
    AiaMicrophoneManager_t* microphoneManager );

/**
 * Begins a tap to talk initiated Alexa interaction.
 *
 * @param microphoneManager The @c AiaMicrophoneManager_t to act on.
 * @param index The sample index that @c microphoneBufferReader should
 * begin streaming from.
 * @param profile The ASR profile associated with this interaction. Only
 * @c AIA_MICROPHONE_PROFILE_NEAR_FIELD and @c AIA_MICROPHONE_PROFILE_FAR_FIELD
 * are supported for this type of interaction.
 *
 * @see
 * https://developer.amazon.com/docs/alexa-voice-service/speechrecognizer.html
 * @see
 * https://developer.amazon.com/docs/alexa-voice-service/audio-hardware-configurations.html#asr
 */
bool AiaMicrophoneManager_TapToTalkStart(
    AiaMicrophoneManager_t* microphoneManager,
    AiaBinaryAudioStreamOffset_t index, AiaMicrophoneProfile_t profile );

/* TODO: ADSER-1628 Add Metadata support */
/**
 * Begins a wake word initiated Alexa interaction.
 *
 * @param microphoneManager The @c AiaMicrophoneManager_t to act on.
 * @param beginIndex The sample index corresponding with the start of the
 * detected wake word.
 * @param endIndex The sample index corresponding with the end of the detected
 * wake word.
 * @param profile The ASR profile associated with this interaction. Only
 * @c AIA_MICROPHONE_PROFILE_NEAR_FIELD and @c AIA_MICROPHONE_PROFILE_FAR_FIELD
 * are supported for this type of interaction.
 * @param wakeWord The wake word that was detected.
 *
 * @see
 * https://developer.amazon.com/docs/alexa-voice-service/speechrecognizer.html
 * @see
 * https://developer.amazon.com/docs/alexa-voice-service/audio-hardware-configurations.html#asr
 */
bool AiaMicrophoneManager_WakeWordStart(
    AiaMicrophoneManager_t* microphoneManager,
    AiaBinaryAudioStreamOffset_t beginIndex,
    AiaBinaryAudioStreamOffset_t endIndex, AiaMicrophoneProfile_t profile,
    const char* wakeWord );

#endif /* ifndef AIA_MICROPHONE_MANAGER_H_ */
