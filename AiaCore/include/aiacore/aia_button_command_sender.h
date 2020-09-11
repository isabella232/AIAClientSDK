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
 * @file aia_button_command_sender.h
 * @brief User-facing functions of the @c AiaButtonCommandSender_t type.
 */

#ifndef AIA_BUTTON_COMMAND_SENDER_H_
#define AIA_BUTTON_COMMAND_SENDER_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <aiacore/aia_button_command.h>
#include <aiaregulator/aia_regulator.h>

/**
 * @copydoc AiaSpeakerManager_StopPlayback()
 *
 * @note Implementations are expected to be thread-safe. Blocking can result in
 * delays in system processing.
 */
typedef void ( *AiaStopPlayback_t )( void* userData );

/**
 * This type is used to synchronize user initiated button presses with the
 * service on behalf of the application.
 *
 * @note Functions in this header are thread-safe.
 */
typedef struct AiaButtonCommandSender AiaButtonCommandSender_t;

/**
 * Allocates and initializes a @c AiaButtonCommandSender_t object via @c
 * AiaCalloc. The returned pointer should be destroyed using @c
 * AiaButtonCommandSender_Destroy().
 *
 * @param eventRegulator The regulator to use to publish messages on the event
 * topic.
 * @param stopPlayback Optional callback to enable faster local stoppage of
 * audio. This shall point to a callback used to stop local playback.
 * @param stopPlaybackUserData Context associated with @c stopPlayback.
 * @return The newly created @c AiaButtonCommandSender_t if successful, or NULL
 * otherwise.
 */
AiaButtonCommandSender_t* AiaButtonCommandSender_Create(
    AiaRegulator_t* eventRegulator, AiaStopPlayback_t stopPlayback,
    void* stopPlaybackUserData );

/**
 * Uninitializes and deallocates an @c AiaButtonCommandSender_t previously
 * created by a call to @c AiaButtonCommandSender_Create().
 *
 * @param buttonCommandSender The @c AiaButtonCommandSender_t to destroy.
 */
void AiaButtonCommandSender_Destroy(
    AiaButtonCommandSender_t* buttonCommandSender );

/**
 * Provides applications a way to inform AIA of a user-initiated button press.
 *
 * @param buttonCommandSender The @c AiaButtonCommandSender_t to act on.
 * @param button The button that was pressed.
 * @return @c true if an event was successfully sent downstream or @c false
 * otherwise.
 */
bool AiaButtonCommandSender_OnButtonPressed(
    AiaButtonCommandSender_t* buttonCommandSender, AiaButtonCommand_t button );

#endif /* ifndef AIA_BUTTON_COMMAND_SENDER_H_ */
