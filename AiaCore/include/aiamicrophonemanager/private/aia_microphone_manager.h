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
 * @brief Private functions of the @c AiaMicrophoneManager_t type.
 */

#ifndef PRIVATE_AIA_MICROPHONE_MANAGER_H_
#define PRIVATE_AIA_MICROPHONE_MANAGER_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <aiamicrophonemanager/aia_microphone_manager.h>

#include <aiacore/aia_message_constants.h>

/**
 * This function may be used to notify the @c microphoneManager of sequenced @c
 * OpenMicrophone directive messages.
 *
 * #param manager The manager instance to act on.
 * @param payload Pointer to the unencrypted "payload" body value of the
 * directive. This must remain valid for the duration of this call.
 * @param size The size of the payload.
 * @param sequenceNumber The sequence number of the message.
 * @param index The index of this directive within the Aia message.
 */
void AiaMicrophoneManager_OnOpenMicrophoneDirectiveReceived(
    void* manager, const void* payload, size_t size,
    AiaSequenceNumber_t sequenceNumber, size_t index );

/**
 * This function may be used to notify the @c microphoneManager of sequenced @c
 * CloseMicrophone directive messages.
 *
 * @param manager The manager instance to act on.
 * @param payload Pointer to the unencrypted "payload" body value of the
 * directive. This must remain valid for the duration of this call.
 * @param size The size of the payload.
 * @param sequenceNumber The sequence number of the message.
 * @param index The index of this directive within the Aia message.
 */
void AiaMicrophoneManager_OnCloseMicrophoneDirectiveReceived(
    void* manager, const void* payload, size_t size,
    AiaSequenceNumber_t sequenceNumber, size_t index );

#endif /* ifndef PRIVATE_AIA_MICROPHONE_MANAGER_H_ */
