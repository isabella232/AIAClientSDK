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
 * @file aia_dispatcher.h
 * @brief User-facing functions for the message parsing and dispatching
 * of AIA.
 */

#ifndef AIA_DISPATCHER_H_
#define AIA_DISPATCHER_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <aiaconnectionmanager/aia_connection_manager.h>
#include <aiacore/aia_directive.h>
#include <aiacore/capabilities_sender/aia_capabilities_sender.h>
#include <aiacore/private/aia_capabilities_sender.h>
#include <aiadispatcher/private/aia_dispatcher.h>
#include <aiaspeakermanager/private/aia_speaker_manager.h>

/**
 * This class parses messages received on topic subscription callbacks and
 * distributes them to the appropriate handlers.
 */
typedef struct AiaDispatcher AiaDispatcher_t;

/**
 * Allocates and initializes a new @c AiaDispatcher_t.  An @c
 * AiaDispatcher_t created by this function should later be released by a
 * call to @c AiaDispatcher_Destroy().
 *
 * @param taskPool Taskpool used to schedule jobs.
 * @param capabilitiesSender @c AiaCapabilitiesSender_t used by this dispatcher.
 * @param regulator @c AiaRegulator_t used by this dispatcher.
 * @param secretManager @c AiaSecretManager_t used by this dispatcher.
 * @return the new @c AiaDispatcher_t if successful, else @c NULL.
 */
AiaDispatcher_t* AiaDispatcher_Create(
    AiaTaskPool_t aiaTaskPool, AiaCapabilitiesSender_t* capabilitiesSender,
    AiaRegulator_t* regulator, AiaSecretManager_t* secretManager );

/**
 * Releases a @c AiaDispatcher_t previously allocated by @c
 * AiaDispatcher_Create().
 *
 * @param dispatcher The @c AiaDispatcher_t instance to act on.
 */
void AiaDispatcher_Destroy( AiaDispatcher_t* dispatcher );

/**
 * Fills the @c AiaConnectionManager_t member of an @c AiaDispatcher_t.
 *
 * @param dispatcher The @c AiaDispatcher_t instance to act on.
 * @param connectionManager The @c AiaConnectionManager_t instance to add.
 */
void AiaDispatcher_AddConnectionManager(
    AiaDispatcher_t* dispatcher,
    struct AiaConnectionManager* connectionManager );

#ifdef AIA_ENABLE_SPEAKER
/**
 * Fills the @c AiaSpeakerManager_t member of an @c AiaDispatcher_t.
 *
 * @param dispatcher The @c AiaDispatcher_t instance to act on.
 * @param speakerManager The @c AiaSpeakerManager_t instance to add.
 */
void AiaDispatcher_AddSpeakerManager( AiaDispatcher_t* dispatcher,
                                      AiaSpeakerManager_t* speakerManager );
#endif

/**
 * Adds a directive handler to the @c AiaDispatcher_t instance.
 *
 * @param dispatcher The @c AiaDispatcher_t instance to act on.
 * @param handler Pointer to the directive handler being added.
 * @param directive Pointer to the directive type handled by the @c handler.
 * @param userData User data associated with @c handler.
 * @return @c true if the @c handler was succesfully added, else @c false.
 */
bool AiaDispatcher_AddHandler( AiaDispatcher_t* dispatcher,
                               AiaDirectiveHandler_t handler,
                               AiaDirective_t directive, void* userData );

/**
 * Callback function for messages received from the subscription.
 *
 * @param callbackArg The context of the callback function.
 * @param callbackParam The parameter of the callback function.
 *
 */
void messageReceivedCallback( void* callbackArg,
                              AiaMqttCallbackParam_t* callbackParam );

#endif /* ifndef AIA_DISPATCHER_H_ */
