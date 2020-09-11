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
 * @brief User-facing functions of the @c AiaUXManager_t type.
 */

#ifndef AIA_UX_MANAGER_H_
#define AIA_UX_MANAGER_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <aia_application_config.h>
#include <aia_capabilities_config.h>
#include <aiaregulator/aia_regulator.h>
#include <aiauxmanager/aia_ux_state.h>
#include <aiauxmanager/private/aia_ux_manager.h>

#ifdef AIA_ENABLE_SPEAKER
#include <aiaspeakermanager/aia_speaker_manager.h>
#endif

/**
 * This type is used to manage the device UX state. Methods of this object are
 * thread-safe.
 */
typedef struct AiaUXManager AiaUXManager_t;

/**
 * Allocates and initializes a @c AiaUXManager_t object from the heap.
 * The returned pointer should be destroyed using @c
 * AiaUXManager_Destroy().
 *
 * @param eventRegulator Used to publish outbound events.
 * @param stateObserver An observer that can be used to observe
 * UX state changes.
 * @param stateObserverUserData Optional context to be passed to @c
 * stateObserver.
 * @param speakerManager The speaker manager used for offset management.
 * @return The newly created @c AiaUXManager_t if successful, or NULL
 * otherwise.
 */
AiaUXManager_t* AiaUXManager_Create( AiaRegulator_t* eventRegulator,
                                     AiaUXStateObserverCb_t stateObserver,
                                     void* stateObserverUserData
#ifdef AIA_ENABLE_SPEAKER
                                     ,
                                     AiaSpeakerManager_t* speakerManager
#endif
);

/**
 * Returns the current UX state.
 *
 * @param uxManager The @c AiaUXManager_t to act on.
 * @return Return the current UX state.
 */
AiaUXState_t AiaUXManager_GetUXState( AiaUXManager_t* uxManager );

/**
 * Updates the current UX server attention state.
 *
 * @param uxManager The @c AiaUXManager_t to act on.
 * @param newAttentionState The new server attention state for the @c
 * AiaUXManager_t.
 */
void AiaUXManager_UpdateServerAttentionState(
    AiaUXManager_t* uxManager, AiaServerAttentionState_t newAttentionState );

/**
 * Uninitializes and deallocates an @c AiaUXManager_t previously created
 * by a call to @c AiaUXManager_Create().
 *
 * @param uxManager The @c AiaUXManager_t to destroy.
 */
void AiaUXManager_Destroy( AiaUXManager_t* uxManager );

#endif /* ifndef AIA_UX_MANAGER_H_ */
