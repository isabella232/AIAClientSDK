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
 * @file aia_alert_manager.h
 * @brief Private functions of the @c AiaAlertManager_t type.
 */

#ifndef PRIVATE_AIA_ALERT_MANAGER_H_
#define PRIVATE_AIA_ALERT_MANAGER_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <aiaalertmanager/aia_alert_manager.h>

/**
 * This function may be used to notify the @c alertManager of a new sequenced @c
 * SetAlert directive messages.
 *
 * @param manager The manager instance to act on.
 * @param payload Pointer to the unencrypted "payload" body value of the
 * directive. This must remain valid for the duration of this call.
 * @param size The size of the payload.
 * @param sequenceNumber The sequence number of the message.
 * @param index The index of this directive within the Aia message.
 */
void AiaAlertManager_OnSetAlertDirectiveReceived(
    void* manager, const void* payload, size_t size,
    AiaSequenceNumber_t sequenceNumber, size_t index );

/**
 * This function may be used to notify the @c alertManager of sequenced @c
 * DeleteAlert directive messages.
 *
 * @param manager The manager instance to act on.
 * @param payload Pointer to the unencrypted "payload" body value of the
 * directive. This must remain valid for the duration of this call.
 * @param size The size of the payload.
 * @param sequenceNumber The sequence number of the message.
 * @param index The index of this directive within the Aia message.
 */
void AiaAlertManager_OnDeleteAlertDirectiveReceived(
    void* manager, const void* payload, size_t size,
    AiaSequenceNumber_t sequenceNumber, size_t index );

/**
 * This function may be used to notify the @c alertManager of sequenced @c
 * SetAlertVolume directive messages.
 *
 * @param manager The manager instance to act on.
 * @param payload Pointer to the unencrypted "payload" body value of the
 * directive. This must remain valid for the duration of this call.
 * @param size The size of the payload.
 * @param sequenceNumber The sequence number of the message.
 * @param index The index of this directive within the Aia message.
 */
void AiaAlertManager_OnSetAlertVolumeDirectiveReceived(
    void* manager, const void* payload, size_t size,
    AiaSequenceNumber_t sequenceNumber, size_t index );

#endif /* ifndef PRIVATE_AIA_ALERT_MANAGER_H_ */
