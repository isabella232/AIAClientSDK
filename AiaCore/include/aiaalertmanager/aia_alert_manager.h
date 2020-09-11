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
 * @brief User-facing functions of the @c AiaAlertManager_t type.
 */

#ifndef AIA_ALERT_MANAGER_H_
#define AIA_ALERT_MANAGER_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <aiaalertmanager/aia_alert_constants.h>
#include <aiaconnectionmanager/aia_connection_manager.h>
#include <aiacore/aia_message_constants.h>
#include <aiaregulator/aia_regulator.h>
#include <aiaspeakermanager/aia_speaker_manager.h>
#include <aiauxmanager/aia_ux_manager.h>

/**
 * This type is used to manage the alerts in the system. It provides an
 * interface to handle alert related directives or events in order to set or
 * delete alarms.
 */
typedef struct AiaAlertManager AiaAlertManager_t;

#ifdef AIA_ENABLE_SPEAKER
/**
 * Callback that returns @c true if the speaker can stream data.
 *
 * @param userData Context associated with this callback.
 * @return @c true if the speaker can stream or @c false otherwise.
 */
typedef bool ( *AiaSpeakerCanStreamCb_t )( void* userData );
#endif

/**
 * Callback that returns the UX state.
 *
 * @param userData Context associated with this callback.
 * @return the UX state.
 */
typedef AiaUXState_t ( *AiaUXStateObserver_t )( void* userData );

/**
 * Callback that updates the UX server attention state.
 *
 * @param userData Context associated with this callback.
 * @param newAttentionState The new server attention state.
 */
typedef void ( *AiaUXServerAttentionStateUpdateCb_t )(
    void* userData, AiaServerAttentionState_t newAttentionState );

#ifdef AIA_ENABLE_SPEAKER
/**
 * Callback that enables offline alert playback. This callback causes @c
 * AiaSpeakerManager_t to initiate @c AiaOfflineAlertPlayback_t at a pre-defined
 * cadence.
 *
 * @param offlineAlert The offline alert to synthesize an alert tone for.
 * @param userData User data associated with this callback.
 * @param offlineAlertVolume Speaker volume to use during offline alert
 * playback.
 *
 * @return @c true If offline alert playback has been enabled successfully or @c
 * false otherwise.
 */
typedef bool ( *AiaOfflineAlertStart_t )( const AiaAlertSlot_t* offlineAlert,
                                          void* userData,
                                          uint8_t offlineAlertVolume );
#endif

/**
 * Allocates and initializes a @c AiaAlertManager_t object from the heap.
 * The returned pointer should be destroyed using @c AiaAlertManager_Destroy().
 *
 * @param eventRegulator Used to publish outbound messages.
 * @param speakerCheckCb Callback used to query the status of the speaker
 * manager.
 * @param speakerCheckCbUserData User data to be passed along with @c
 * speakerCheckCb.
 * @param startOfflineAlertCb Callback used to enable the offline alert tone
 * playback.
 * @param startOfflineAlertCbUserData User data to be passed along with @c
 * startOfflineAlertCb.
 * @param uxStateUpdateCb Callback used to update UX server attention state
 * while playing offline alerts.
 * @param uxStateUpdateCbUserData User data to be passed along with @c
 * uxStateUpdateCb.
 * @param uxStateCheckCb Callback used to query the UX state.
 * @param uxStateCheckCbUserData User data to be passed along with @c
 * uxStateCheckCb.
 * @param disconnectCb Callback used to disconnect from service.
 * @param disconnectCbUserData User data to be passed along with @c
 * disconnectCbUserData.
 * @return The newly created @c AiaAlertManager_t if successful, or NULL
 * otherwise.
 */
AiaAlertManager_t* AiaAlertManager_Create(
    AiaRegulator_t* eventRegulator
#ifdef AIA_ENABLE_SPEAKER
    ,
    AiaSpeakerCanStreamCb_t speakerCheckCb, void* speakerCheckCbUserData,
    AiaOfflineAlertStart_t startOfflineAlertCb,
    void* startOfflineAlertCbUserData
#endif
    ,
    AiaUXServerAttentionStateUpdateCb_t uxStateUpdateCb,
    void* uxStateUpdateCbUserData, AiaUXStateObserver_t uxStateCheckCb,
    void* uxStateCheckCbUserData, AiaDisconnectHandler_t disconnectCb,
    void* disconnectUserData );

/**
 * Returns the alert tokens known by the @c AiaAlertManager_t instance as a
 * list of quoted strings; i.e. "token1", "token2", "token3"
 *
 * @param alertManager The @c AiaAlertManager_t to get the alert tokens from.
 * @param[out] alertTokens Buffer to hold the alert tokens.
 *
 * @note This function allocates the memory for the @c alertTokens buffer and
 * it is up to the callers of this function to free that memory (via @c
 * AiaFree()) when this buffer is no longer needed.
 * @note @c alertTokens will be NULL if no alerts are found or in case of a
 * failure.
 *
 * @return Size of the @c alertTokens buffer or 0 if no alerts are found or in
 * case of a failure.
 */
size_t AiaAlertManager_GetTokens( AiaAlertManager_t* alertManager,
                                  uint8_t** alertTokens );
/**
 * Updates the @c AiaAlertManager_t's @c currentTime and arms offline alert
 * related timers based on this time.
 *
 * @param alertManager The @c AiaAlertManager_t to act on.
 * @param currentTime The current NTP epoch time in seconds to pass to the @c
 * alertManager.
 * @return Returns @c true if successful, @c false otherwise.
 */
bool AiaAlertManager_UpdateAlertManagerTime(
    AiaAlertManager_t* alertManager, AiaTimepointSeconds_t currentTime );

#ifdef AIA_ENABLE_SPEAKER
/**
 * Updates @c AiaAlertManager_t's @c currentBufferState with the information
 * pushed by the @c AiaClockManager_t.
 *
 * @param alertManager The @c AiaAlertManager_t to act on.
 * @param bufferState The current speaker buffer state to pass to the @c
 * alertManager.
 */
void AiaAlertManager_UpdateSpeakerBufferState(
    AiaAlertManager_t* alertManager,
    AiaSpeakerManagerBufferState_t bufferState );
#endif

/**
 * Updates @c AiaAlertManager_t's @c currentUXState with the information
 * pushed by the @c AiaUXManager_t.
 *
 * @param alertManager The @c AiaAlertManager_t to act on.
 * @param uxState The current UX state to pass to the @c alertManager.
 */
void AiaAlertManager_UpdateUXState( AiaAlertManager_t* alertManager,
                                    AiaUXState_t uxState );

/**
 * Uninitializes and deallocates an @c AiaAlertManager_t previously created by a
 * call to @c AiaAlertManager_Create().
 *
 * @param alertManager The @c AiaAlertManager_t to destroy.
 */
void AiaAlertManager_Destroy( AiaAlertManager_t* alertManager );

/**
 * Deletes a given alert token from memory and persistent storage.
 *
 * @param alertManager The @c AiaAlertManager_t to act on.
 * @param alertToken The token of the alert to delete.
 *
 * @return @c true if successful, @c false otherwise.
 */
bool AiaAlertManager_DeleteAlert( AiaAlertManager_t* alertManager,
                                  const char* alertToken );

#endif /* ifndef AIA_ALERT_MANAGER_H_ */
