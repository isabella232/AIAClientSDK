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
 * @file aia_clock_manager.h
 * @brief User-facing functions of the @c AiaClockManager_t type.
 */

#ifndef AIA_CLOCK_MANAGER_H_
#define AIA_CLOCK_MANAGER_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <aiacore/aia_message_constants.h>
#include <aiacore/aia_topic.h>
#include <aiaregulator/aia_regulator.h>

/**
 * This class manages synchronization of the device clock with AIA to prevent
 * clock drift.
 *
 * @note Functions in this header which act on an @c AiaClockManager_t are
 *     thread-safe.
 */
typedef struct AiaClockManager AiaClockManager_t;

/**
 * This function will be called to notify observers after device clock is
 * synchronized.
 * @note Implementations are expected to be thread-safe. Blocking can result in
 * delays in system processing.
 *
 * @param userData Context to be passed with this callback.
 * @param currentTime Current time in seconds.
 */
typedef void ( *AiaClockSynchronizedCallback_t )(
    void* userData, AiaTimepointSeconds_t currentTime );

/**
 * Allocates and initializes a new @c AiaClockManager_t.  An @c
 * AiaClockManager_t created by this function should later be released by a
 * call to @c AiaClockManager_Destroy().
 *
 * @param eventRegulator Used to publish events.
 * @param notifyObserverCb An optional callback pointer used to notify
 * observers after device clock is synchronized.
 * @param notifyObserverCbUserData User data associated with @c
 * notifyObserverCb.
 * @return The newly-constructed @c AiaClockManager_t when successful, else @c
 *     NULL.
 */
AiaClockManager_t* AiaClockManager_Create(
    AiaRegulator_t* eventRegulator,
    AiaClockSynchronizedCallback_t notifyObserverCb,
    void* notifyObserverCbUserData );

/**
 * Releases a @c AiaClockManager_t previously allocated by @c
 * AiaClockManager_Create().
 *
 * @param clockManager The @c AiaClockManager_t instance to act on.
 */
void AiaClockManager_Destroy( AiaClockManager_t* clockManager );

/**
 * Attempts to synchronize the device clock with the AIA server. Responses from
 * the service will be sent to @c AiaClock_SetTimeMsSinceNTPEpoch.
 *
 * @param clockManager The @c AiaClockManager_t instance to act on.
 * @return @c true if an attempt was successfully made or @c false otherwise.
 */
bool AiaClockManager_SynchronizeClock( AiaClockManager_t* clockManager );

#endif /* ifndef AIA_CLOCK_MANAGER_H_ */
