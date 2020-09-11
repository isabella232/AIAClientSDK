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
 * @file aia_registration_manager.h
 * @brief Implements registration functions for AIA.
 */

#ifndef AIA_REGISTRATION_MANAGER_H_
#ifdef __cplusplus
extern "C" {
#endif
#define AIA_REGISTRATION_MANAGER_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <aiaregistrationmanager/aia_registration_failure_code.h>

/**
 * This class handles registration for the device with AIA.
 *
 * @note Functions in this header which act on an @c AiaRegistrationManager_t
 * may not be thread-safe. Users are required to provide external
 * synchronization.
 */
typedef struct AiaRegistrationManager AiaRegistrationManager_t;

/**
 * This callback function is used after the client successfully registers with
 * the service.
 *
 * @param userData Optional user data pointer which was provided alongside the
 * callback.
 */
typedef void ( *AiaRegistrationManagerOnRegisterSuccessCallback_t )(
    void* userData );

/**
 * This callback function is used after the client fails to register with the
 * service.
 *
 * @param userData Optional user data pointer which was provided alongside the
 * callback.
 * @param code The code provided in a registration failure response.
 */
typedef void ( *AiaRegistrationManagerOnRegisterFailureCallback_t )(
    void* userData, AiaRegistrationFailureCode_t code );

/**
 * Allocates and initializes a new @c AiaRegistrationManager_t. An @c
 * AiaRegistrationManager_t created by this function should later be released by
 * a call to @c AiaRegistrationManager_Destroy().
 *
 * @param onRegisterSuccess A callback that is ran if registration is
 * successful.
 * @param onRegisterSuccessUserData User data to pass to @c onRegisterSuccess.
 * @param onRegisterFailure A callback that is ran if registration fails.
 * @param onRegisterFailureUserData User data to pass to @c onRegisterFailure
 * @return the new @c AiaRegistrationManager_t if successful, else @c NULL.
 */
AiaRegistrationManager_t* AiaRegistrationManager_Create(
    AiaRegistrationManagerOnRegisterSuccessCallback_t onRegisterSuccess,
    void* onRegisterSuccessUserData,
    AiaRegistrationManagerOnRegisterFailureCallback_t onRegisterFailure,
    void* onRegisterFailureUserData );

/**
 * Sends a registration request for AIA.
 * @note Callbacks in @c registrationManager will only be made if @c true is
 * returned.
 * @note Only one registration request is expected at a time. If another
 * registration request is in progress then @c false is returned.
 *
 * @param registrationManager The registration manager instance to act on.
 * @return @c true if the request was able to be performed successfully, @c
 * false otherwise.
 */
bool AiaRegistrationManager_Register(
    AiaRegistrationManager_t* registrationManager );

/**
 * Releases a @c AiaRegistrationManager_t previously allocated by @c
 * AiaRegistrationManager_Create().
 * @note If this function is called while a registration is in progress, this
 * function will cancel the registration and call @c onRegisterFailure()
 *
 * @param registrationManager The registration manager instance to act on.
 */
void AiaRegistrationManager_Destroy(
    AiaRegistrationManager_t* registrationManager );

#ifdef __cplusplus
}
#endif
#endif /* ifndef AIA_REGISTRATION_MANAGER_H_ */
