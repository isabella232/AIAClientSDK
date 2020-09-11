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
 * @file aia_exception_manager.h
 * @brief Implements functions for the AiaExceptionManager_t type.
 */

#ifndef AIA_EXCEPTION_MANAGER_H_
#define AIA_EXCEPTION_MANAGER_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <aia_application_config.h>

#include <aiacore/aia_message_constants.h>
#include <aiaexceptionmanager/aia_exception_code.h>
#include <aiaregulator/aia_regulator.h>

typedef struct AiaExceptionManager AiaExceptionManager_t;

/**
 * Allocates and initializes a new @c AiaExceptionManager_t.  An @c
 * AiaExceptionManager_t created by this function should later be released by a
 * call to @c AiaExceptionManager_Destroy().
 *
 * @param eventRegulator Used to publish events.
 * @param onException An optional callback that is ran when an Exception
 * directive is received.
 * @param onExceptionUserData User data to pass to @c onException.
 * @return the new @c AiaExceptionManager_t if successful, else @c NULL.
 */
AiaExceptionManager_t* AiaExceptionManager_Create(
    AiaRegulator_t* eventRegulator,
    AiaExceptionManagerOnExceptionCallback_t onException,
    void* onExceptionUserData );

/**
 * This function may be used to notify the @c exceptionManager of a new
 * sequenced
 * @c Exception directive.
 *
 * @param manager The @c AiaExceptionManager_t to act on.
 * @param payload Pointer to the unencrypted "payload" body value of the
 * directive. This must remain valid for the duration of this call.
 * @param size The size of the payload.
 * @param sequenceNumber The sequence number of the message.
 * @param index The index of this directive within the Aia message.
 */
void AiaExceptionManager_OnExceptionReceived(
    void* manager, const void* payload, size_t size,
    AiaSequenceNumber_t sequenceNumber, size_t index );

/**
 * Releases a @c AiaExceptionManager_t previously allocated by @c
 * AiaExceptionManager_Create().
 *
 * @param exceptionManager The exception manager instance to act on.
 */
void AiaExceptionManager_Destroy( AiaExceptionManager_t* exceptionManager );

#endif /* ifndef AIA_EXCEPTION_MANAGER_H_ */
