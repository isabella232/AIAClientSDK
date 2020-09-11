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
 * @file aia_capabilities_sender.h
 * @brief Private functions of the @c AiaCapabilitiesSender_t type.
 */

#ifndef PRIVATE_AIA_CAPABILITIES_SENDER_H_
#define PRIVATE_AIA_CAPABILITIES_SENDER_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <aiacore/capabilities_sender/aia_capabilities_sender.h>

/**
 * This function may be used to notify the @c capabilitiesSender of sequenced @c
 * Capabilities acknowledge messages.
 *
 * @param capabilitiesSender The @c AiaCapabilitiesSender_t to act on.
 * @param payload Pointer to the unencrypted "payload" body value of the
 * message. This must remain valid for the duration of this call.
 * @param size The size of the payload.
 *
 * @note The @c payload is expected to be in this format: "{"publishMessageId":
 * X...}".
 */
void AiaCapabilitiesSender_OnCapabilitiesAcknowledgeMessageReceived(
    AiaCapabilitiesSender_t* capabilitiesSender, const char* payload,
    size_t size );

#endif /* ifndef PRIVATE_AIA_CAPABILITIES_SENDER_H_ */
