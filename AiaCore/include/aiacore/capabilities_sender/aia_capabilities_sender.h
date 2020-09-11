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
 * @brief User-facing functions of the @c AiaCapabilitiesSender_t type.
 */

#ifndef AIA_CAPABILITIES_SENDER_H_
#define AIA_CAPABILITIES_SENDER_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <aia_application_config.h>
#include <aiacore/capabilities_sender/aia_capabilities_sender_state.h>
#include <aiaregulator/aia_regulator.h>

/**
 * This type is used to publish capabilities on behalf of the application.
 *
 * @note Functions in this header are thread-safe.
 */
typedef struct AiaCapabilitiesSender AiaCapabilitiesSender_t;

/**
 * Allocates and initializes a @c AiaCapabilitiesSender_t object from the
 * heap. The returned pointer should be destroyed using @c
 * AiaCapabilitiesSender_Destroy().
 *
 * @param capabilitiesRegulator The regulator to use to publish messages on the
 * capabilities topic.
 * @param stateObserver The observer callback to be notified of state changes.
 * @param stateObserverUserData Context associated with @c stateObserver.
 * @return The newly created @c AiaCapabilitiesSender_t if successful, or
 * NULL otherwise.
 */
AiaCapabilitiesSender_t* AiaCapabilitiesSender_Create(
    AiaRegulator_t* capabilitiesRegulator,
    AiaCapabilitiesObserver_t stateObserver, void* stateObserverUserData );

/**
 * Uninitializes and deallocates an @c AiaCapabilitiesSender_t previously
 * created by a call to @c AiaCapabilitiesSender_Create().
 *
 * @param capabilitiesSender The @c AiaCapabilitiesSender_t to destroy.
 */
void AiaCapabilitiesSender_Destroy(
    AiaCapabilitiesSender_t* capabilitiesSender );

/**
 * Publishes capabilities from @c aia_capabilities_config.h on behalf of the
 * client.
 *
 * @param capabilitiesSender The @c AiaCapabilitiesSender_t to act on.
 * @return @c true if the capabilities were published or @c false if not.
 * @note Capabilities will be automatically rejected if the @c
 * AiaCapabilitiesSender_t's state is @c AIA_CAPABILITIES_STATE_PUBLISHED.
 */
bool AiaCapabilitiesSender_PublishCapabilities(
    AiaCapabilitiesSender_t* capabilitiesSender );

#endif /* ifndef AIA_CAPABILITIES_SENDER_H_ */
