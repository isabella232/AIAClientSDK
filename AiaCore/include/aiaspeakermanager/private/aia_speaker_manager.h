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

#ifndef PRIVATE_AIA_SPEAKER_MANAGER_H_
#define PRIVATE_AIA_SPEAKER_MANAGER_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <aiaspeakermanager/aia_speaker_manager.h>

/**
 * This function may be used to notify the @c speakerManager of a new sequenced
 * speaker topic message.
 *
 * @param speakerManager The @c AiaSpeakerManager_t to act on.
 * @param message Pointer to the unencrypted message body (without the common
 * header) i.e. the unencrypted Binary Stream. This must remain valid for the
 * duration of this call.
 * @param size The size of the message.
 * @param sequenceNumber The sequence number of the message.
 */
void AiaSpeakerManager_OnSpeakerTopicMessageReceived(
    AiaSpeakerManager_t* speakerManager, const uint8_t* message, size_t size,
    AiaSequenceNumber_t sequenceNumber );

/**
 * This function may be used to notify the @c speakerManager of sequenced @c
 * OpenSpeaker directive messages.
 *
 * @param manager The manager instance to act on.
 * @param payload Pointer to the unencrypted "payload" body value of the
 * directive. This must remain valid for the duration of this call.
 * @param size The size of the payload.
 * @param sequenceNumber The sequence number of the message.
 * @param index The index of this directive within the AIS message.
 *
 * @note The @c payload is expected to be in this format: "{"offset": X}".
 */
void AiaSpeakerManager_OnOpenSpeakerDirectiveReceived(
    void* manager, const void* payload, size_t size,
    AiaSequenceNumber_t sequenceNumber, size_t index );

/**
 * This function may be used to notify the @c speakerManager of sequenced @c
 * CloseSpeaker directive messages.
 *
 * @param manager The manager instance to act on.
 * @param payload Pointer to the unencrypted "payload" body value of the
 * directive. This must remain valid for the duration of this call.
 * @param size The size of the payload.
 * @param sequenceNumber The sequence number of the message.
 * @param index The index of this directive within the AIS message.
 *
 * @note The @c payload is expected to be in this format: "{"offset": X}".
 */
void AiaSpeakerManager_OnCloseSpeakerDirectiveReceived(
    void* manager, const void* payload, size_t size,
    AiaSequenceNumber_t sequenceNumber, size_t index );

/**
 * This function may be used to notify the @c speakerManager of sequenced @c
 * SetVolume directive messages.
 *
 * @param speakerManager The @c AiaSpeakerManager_t to act on.
 * @param payload Pointer to the unencrypted "payload" body value of the
 * directive. This must remain valid for the duration of this call.
 * @param size The size of the payload.
 * @param sequenceNumber The sequence number of the message.
 * @param index The index of this directive within the AIS message.
 *
 * @note The @c payload is expected to be in this format: "{"volume": X,
 * "offset": X}".
 */
void AiaSpeakerManager_OnSetVolumeDirectiveReceived(
    void* speakerManager, const void* payload, size_t size,
    AiaSequenceNumber_t sequenceNumber, size_t index );

/** Opaque handle to an @c AiaActionAtSpeakerOffset_t. */
typedef void* AiaSpeakerActionHandle_t;

/** An invalid @c AiaSpeakerActionHandle_t. */
static const AiaSpeakerActionHandle_t AIA_INVALID_ACTION_ID = NULL;

/**
 * Function to invoke when an offset is reached.
 *
 * @param actionValid @c true if this callback is being invoked due to the
 * offset actually being reached or @c false if the action was invalidated due
 * to a local action (e.g. barge-in) that invalidated the future offset.
 * @param userData User data associated with this callback.
 * @note Implementations are expected to be non-blocking.
 * @note Implementations are not required to be thread-safe.
 * @note Calling back into the @c AiaSpeakerManager_t from this callback
 * execution context can result in a deadlock.
 */
typedef void ( *AiaActionAtSpeakerOffset_t )( bool actionValid,
                                              void* userData );

/**
 * This function may be used to submit an action for invocation when the speaker
 * reaches the provided offset.
 *
 * @param speakerManager The @c AiaSpeakerManager_t to act on.
 * @param offset The offset at which to invoke @c action.
 * @param action The action to invoke.
 * @param userData Context to associate with @c action.
 * @return An handle associated with this action or @c NULL on
 * failure.
 */
AiaSpeakerActionHandle_t AiaSpeakerManager_InvokeActionAtOffset(
    AiaSpeakerManager_t* speakerManager, AiaBinaryAudioStreamOffset_t offset,
    AiaActionAtSpeakerOffset_t action, void* userData );

/**
 * This function may be used to cancel an action previously submitted for
 * invocation with @c AiaSpeakerManager_InvokeActionAtOffset.
 *
 * @param speakerManager The @c AiaSpeakerManager_t to act on.
 * @param handle The handle of the action to cancel.
 */
void AiaSpeakerManager_CancelAction( AiaSpeakerManager_t* speakerManager,
                                     AiaSpeakerActionHandle_t handle );

/**
 * This function may be used to query the current offset in the speaker stream.
 * Specifically, it reports the byte offset of the next audio frame that will be
 * emitted from @c SpeakerManager.
 *
 * @param speakerManager The @c AiaSpeakerManager_t to act on.
 * @return The byte offset within the speaker stream of the next audio frame
 * that will be emitted.
 */
AiaBinaryAudioStreamOffset_t AiaSpeakerManager_GetCurrentOffset(
    const AiaSpeakerManager_t* speakerManager );

#endif /* ifndef PRIVATE_AIA_SPEAKER_MANAGER_H_ */
