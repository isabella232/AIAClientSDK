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
 * @file aia_ux_manager.h
 * @brief Private functions of the @c AiaUXManager_t type.
 */

#ifndef PRIVATE_AIA_UX_MANAGER_H_
#define PRIVATE_AIA_UX_MANAGER_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <aia_capabilities_config.h>

#include <aiauxmanager/aia_ux_manager.h>

#include <aiacore/aia_message_constants.h>

#ifdef AIA_ENABLE_MICROPHONE
#include <aiamicrophonemanager/aia_microphone_state.h>
#endif

/** Enumeration of the UX attention states that an AIA client can be in. */
typedef enum AiaServerAttentionState
{
    /** No active interaction. */
    AIA_ATTENTION_STATE_IDLE,

    /** The user has completed a request, the microphone is closed, and a
       response is pending. No additional voice input is accepted in this state.
     */
    AIA_ATTENTION_STATE_THINKING,

#ifdef AIA_ENABLE_SPEAKER
    /** TTS is being played through the speaker topic. This does not apply to
       long-running content, such as audiobooks or Flash Briefing. */
    AIA_ATTENTION_STATE_SPEAKING,
#endif

#ifdef AIA_ENABLE_ALERTS
    /** An alert is being played, either through the speaker topic or rendered
       locally in offline mode. */
    AIA_ATTENTION_STATE_ALERTING,
#endif

    /** A Notification is available to be played to the user. */
    AIA_ATTENTION_STATE_NOTIFICATION_AVAILABLE,

    /** The user has enabled Do Not Disturb mode. */
    AIA_ATTENTION_STATE_DO_NOT_DISTURB
} AiaServerAttentionState_t;

#ifdef AIA_ENABLE_MICROPHONE
/**
 * Used to observe microphone state changes from the @c AiaMicrophoneManager_t.
 *
 * @param state The new microphone state.
 * @param userData Context associated with this callback.
 */
void AiaUXManager_OnMicrophoneStateChange( AiaMicrophoneState_t state,
                                           void* userData );
#endif

/**
 * This function may be used to notify the @c AiaUXManager_t of sequenced @c
 * SetAttentionState directive messages.
 *
 * @param uxManager The @c AiaUXManager_t to act on.
 * @param payload Pointer to the unencrypted "payload" body value of the
 * directive. This must remain valid for the duration of this call.
 * @param size The size of the payload.
 * @param sequenceNumber The sequence number of the message.
 * @param index The index of this directive within the Aia message.
 */
void AiaUXManager_OnSetAttentionStateDirectiveReceived(
    void* uxManager, const void* payload, size_t size,
    AiaSequenceNumber_t sequenceNumber, size_t index );

#endif /* ifndef PRIVATE_AIA_UX_MANAGER_H_ */
