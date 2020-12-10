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
 * @file aia_alerts_manager.c
 * @brief Implements functions for the AiaAlertManager_t type.
 */

/* The config header is always included first. */
#include <aia_config.h>

#include <aiaalertmanager/aia_alert_constants.h>
#include <aiaalertmanager/aia_alert_manager.h>
#include <aiaalertmanager/aia_alert_slot.h>
#include <aiacore/aia_events.h>
#include <aiacore/aia_exception_encountered_utils.h>
#include <aiacore/aia_json_message.h>
#include <aiacore/aia_json_utils.h>
#include <aiacore/aia_topic.h>
#include <aiacore/aia_volume_constants.h>
#include <aiaspeakermanager/private/aia_speaker_manager.h>

#include AiaTimer( HEADER )

#include <inttypes.h>
#include <stdio.h>

/**
 * Underlying struct that contains all the data required to present the @c
 * AiaAlertManager_t abstraction.
 */
struct AiaAlertManager
{
    /** Mutex used to guard against asynchronous calls in threaded environments.
     */
    AiaMutex_t mutex;

    /** @name Variables synchronized by mutex. */
    /** @{ */

    /** The offline alert volume. */
    uint8_t offlineAlertVolume;

    /** The linked list representing all alerts. */
    AiaListDouble_t allAlerts;

    /** Used to keep track of the number of UX state
     * changes by the @c offlineAlertPlayOrStatusCheckTimer */
    uint32_t numStateChanges;

    /** Used to keep track of the last known UX state
     * by the @c offlineAlertPlayOrStatusCheckTimer */
    AiaUXState_t lastUXState;

    /** Used to keep track of the current UX state
     * by the @c offlineAlertPlayOrStatusCheckTimer */
    AiaUXState_t currentUXState;

#ifdef AIA_ENABLE_SPEAKER
    /** Used to keep track of the number of underruns per each iteration of the
     * @c offlineAlertPlayOrStatusCheckTimer. It gets reset in between
     * invocations of the @c offlineAlertPlayOrStatusCheckTimer. */
    uint32_t numUnderruns;

    /** Used to keep track of the last known buffer state by the @c
     * offlineAlertPlayOrStatusCheckTimer */
    AiaSpeakerManagerBufferState_t lastBufferState;

    /** Used to keep track of the current buffer state by the @c
     * offlineAlertPlayOrStatusCheckTimer */
    AiaSpeakerManagerBufferState_t currentBufferState;
#endif

    /** Callback to update the UX server attention state. */
    const AiaUXServerAttentionStateUpdateCb_t uxStateUpdateCb;

    /** User data to pass to @c uxStateUpdateCb. */
    void* const uxStateUpdateCbUserData;

    /** Callback to check the UX state. */
    const AiaUXStateObserver_t uxStateCheckCb;

    /** User data to pass to @c uxStateCheckCb. */
    void* const uxStateCheckCbUserData;

#ifdef AIA_ENABLE_SPEAKER
    /** Callback to check the status of the speaker manager. */
    const AiaSpeakerCanStreamCb_t speakerCheckCb;

    /** User data to pass to @c speakerCheckCb. */
    void* const speakerCheckCbUserData;
#endif

    /** @} */

#ifdef AIA_ENABLE_SPEAKER
    /** Callback for enabling the offline alert tone playback. */
    const AiaOfflineAlertStart_t startOfflineAlertCb;

    /** User data to pass to @c startOfflineAlertCb. */
    void* const startOfflineAlertCbUserData;
#endif

    /** Callback to disconnect from the service. */
    const AiaDisconnectHandler_t disconnectCb;

    /** User data to pass to @c disconnectCb. */
    void* const disconnectCbUserData;

    /** Used for two different purposes at every @c
     * AIA_OFFLINE_ALERT_STATUS_CHECK_CADENCE_MS seconds:
     * - Check if we should start playing the offline alerts.
     * - Otherwise, check if the state of the speaker buffer (if @c
     * AIA_ENABLE_SPEAKER is defined) and the UX state to decide if the device
     * should disconnect and start playing offline alerts.
     */
    AiaTimer_t offlineAlertPlayOrStatusCheckTimer;

    /** Used to publish outbound messages. Methods of this object are
     * thread-safe. */
    AiaRegulator_t* const eventRegulator;
};

/**
 * Helper function handling @c SetAlertVolume directives. @c mutex must be
 * locked prior to calling this method.
 *
 * @param alertManager The @c AiaAlertManager_t to act on.
 * @param payload Pointer to the unencrypted "payload" body value of the
 * directive. This must remain valid for the duration of this call.
 * @param size The size of the payload.
 * @param sequenceNumber The sequence number of the message.
 * @param index The index of this directive within the Aia message.
 */
static void AiaAlertManager_OnSetAlertVolumeDirectiveReceivedLocked(
    AiaAlertManager_t* alertManager, const void* payload, size_t size,
    AiaSequenceNumber_t sequenceNumber, size_t index );

/**
 * Helper function handling @c SetAlert directives. @c mutex must be locked
 * prior to calling this method.
 *
 * @param alertManager The @c AiaAlertManager_t to act on.
 * @param payload Pointer to the unencrypted "payload" body value of the
 * directive. This must remain valid for the duration of this call.
 * @param size The size of the payload.
 * @param sequenceNumber The sequence number of the message.
 * @param index The index of this directive within the Aia message.
 */
static void AiaAlertManager_OnSetAlertDirectiveReceivedLocked(
    AiaAlertManager_t* alertManager, const void* payload, size_t size,
    AiaSequenceNumber_t sequenceNumber, size_t index );

/**
 * Helper function handling @c DeleteAlert directives. @c mutex must be locked
 * prior to calling this method.
 *
 * @param alertManager The @c AiaAlertManager_t to act on.
 * @param payload Pointer to the unencrypted "payload" body value of the
 * directive. This must remain valid for the duration of this call.
 * @param size The size of the payload.
 * @param sequenceNumber The sequence number of the message.
 * @param index The index of this directive within the Aia message.
 */
static void AiaAlertManager_OnDeleteAlertDirectiveReceivedLocked(
    AiaAlertManager_t* alertManager, const void* payload, size_t size,
    AiaSequenceNumber_t sequenceNumber, size_t index );

/**
 * Helper function that generates a @c SetAlertSucceeded event.
 *
 * @param alertToken The alert token to publish in the event, not
 * null-terminated.
 * @param alertTokenLen The length of @c alertToken, not including the
 * null-terminator.
 * @return The generated @c AiaJsonMessage_t or @c NULL on failures.
 */
static AiaJsonMessage_t* generateSetAlertSucceededEvent( const char* alertToken,
                                                         size_t alertTokenLen );

/**
 * Helper function that generates a @c SetAlertFailed event.
 *
 * @param alertToken The alert token to publish in the event, not
 * null-terminated.
 * @param alertTokenLen The length of @c alertToken, not including the
 * null-terminator.
 * @return The generated @c AiaJsonMessage_t or @c NULL on failures.
 */
static AiaJsonMessage_t* generateSetAlertFailedEvent( const char* alertToken,
                                                      size_t alertTokenLen );

/**
 * Helper function that generates a @c DeleteAlertSucceeded event.
 *
 * @param alertToken The alert token to publish in the event, not
 * null-terminated.
 * @param alertTokenLen The length of @c alertToken, not including the
 * null-terminator.
 * @return The generated @c AiaJsonMessage_t or @c NULL on failures.
 */
static AiaJsonMessage_t* generateDeleteAlertSucceededEvent(
    const char* alertToken, size_t alertTokenLen );

/**
 * Helper function that generates a @c DeleteAlertFailed event.
 *
 * @param alertToken The alert token to publish in the event, not
 * null-terminated.
 * @param alertTokenLen The length of @c alertToken, not including the
 * null-terminator.
 * @return The generated @c AiaJsonMessage_t or @c NULL on failures.
 */
static AiaJsonMessage_t* generateDeleteAlertFailedEvent( const char* alertToken,
                                                         size_t alertTokenLen );

/**
 * Helper function that generates a @c AlertVolumeChanged event.
 *
 * @param alertVolume The alert volume to publish in the event.
 * @return The generated @c AiaJsonMessage_t or @c NULL on failures.
 */
static AiaJsonMessage_t* generateAlertVolumeChangedEvent(
    const uint8_t alertVolume );

/**
 * Comparator used for @c AiaListDouble( InsertSorted ).
 *
 * @param link1 First element to compare. A null value will result in undefined
 * behavior.
 * @param link2 Second element to compare. A null value will result in undefined
 * behavior.
 * @return Returns a negative value if its first argument is less than its
 * second argument; returns zero if its first argument is equal to its second
 * argument; returns a positive value if its first argument is greater than its
 * second argument.
 */
static int32_t AiaAlertScheduledTimeComparator(
    const AiaListDouble( Link_t ) * const link1,
    const AiaListDouble( Link_t ) * const link2 )
{
    return ( (AiaAlertSlot_t*)link1 )->scheduledTime -
           ( (AiaAlertSlot_t*)link2 )->scheduledTime;
}

/**
 * This is a recurring function that occurs at @c
 * AIA_OFFLINE_ALERT_STATUS_CHECK_CADENCE_MS intervals after the projected start
 * time of an offline alert and it checks for two things:
 * - Whether we should be playing an offline alert. If so, start the offline
 * alert playback routine.
 * - Otherwise, check the speaker buffer state (if @c AIA_ENABLE_SPEAKER is
 * defined) for underruns and the UX state; and disconnect from the service if
 * necessary.
 *
 * @param context User data associated with this routine.
 */
static void AiaAlertManager_PlayOfflineAlertOrCheckStatus( void* context );

/**
 * An internal helper function to check the speaker buffer state for underruns
 * (if @c AIA_ENABLE_SPEAKER is defined) and the UX state; and to
 * disconnect from the service as needed.
 *
 * @param alertManager The @c AiaAlertManager_t to act on.
 * @note This method must be called while @c alertManager->mutex is locked.
 * @return @c true if the device should be disconnecting due to the speaker
 * buffer state or the UX state, or @c false otherwise.
 */
static bool AiaAlertManager_CheckSpeakerBufferAndUXStateLocked(
    AiaAlertManager_t* alertManager );

/**
 * An internal helper function used to update the offline alert related timers
 * of @c AiaAlertManager_t.
 *
 * @param alertManager The @c AiaAlertManager_t to act on.
 * @param currentTime The current NTP epoch time in seconds.
 * @return Returns @c true if the offline alert timers are updated successfully,
 * @c false otherwise.
 * @note This method must be called while @c alertManager->mutex is locked.
 */
static bool AiaAlertManager_UpdateOfflineAlertTimersLocked(
    AiaAlertManager_t* alertManager, AiaTimepointSeconds_t currentTime );

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
    void* disconnectCbUserData )
{
    if( !eventRegulator )
    {
        AiaLogError( "Null eventRegulator" );
        return NULL;
    }
#ifdef AIA_ENABLE_SPEAKER
    if( !speakerCheckCb )
    {
        AiaLogError( "Null speakerCheckCb" );
        return NULL;
    }
    if( !startOfflineAlertCb )
    {
        AiaLogError( "Null startOfflineAlertCb" );
        return NULL;
    }
#endif
    if( !uxStateUpdateCb )
    {
        AiaLogError( "Null uxStateUpdateCb" );
        return NULL;
    }
    if( !uxStateCheckCb )
    {
        AiaLogError( "Null uxStateCheckCb" );
        return NULL;
    }
    if( !disconnectCb )
    {
        AiaLogError( "Null disconnectCb" );
        return NULL;
    }

    AiaAlertManager_t* alertManager =
        (AiaAlertManager_t*)AiaCalloc( 1, sizeof( AiaAlertManager_t ) );
    if( !alertManager )
    {
        AiaLogError( "AiaCalloc failed, bytes=%zu.",
                     sizeof( AiaAlertManager_t ) );
        return NULL;
    }

    if( !AiaMutex( Create )( &alertManager->mutex, false ) )
    {
        AiaLogError( "AiaMutex( Create ) failed." );
        AiaAlertManager_Destroy( alertManager );
        return NULL;
    }

    AiaListDouble( Create )( &alertManager->allAlerts );

    *(AiaRegulator_t**)&alertManager->eventRegulator = eventRegulator;
#ifdef AIA_ENABLE_SPEAKER
    *(AiaSpeakerCanStreamCb_t*)&( alertManager->speakerCheckCb ) =
        speakerCheckCb;
    *(void**)&alertManager->speakerCheckCbUserData = speakerCheckCbUserData;
    *(AiaOfflineAlertStart_t*)&( alertManager->startOfflineAlertCb ) =
        startOfflineAlertCb;
    *(void**)&alertManager->startOfflineAlertCbUserData =
        startOfflineAlertCbUserData;
#endif
    *(AiaUXServerAttentionStateUpdateCb_t*)&( alertManager->uxStateUpdateCb ) =
        uxStateUpdateCb;
    *(void**)&alertManager->uxStateUpdateCbUserData = uxStateUpdateCbUserData;
    *(AiaUXStateObserver_t*)&( alertManager->uxStateCheckCb ) = uxStateCheckCb;
    *(void**)&alertManager->uxStateCheckCbUserData = uxStateCheckCbUserData;
    *(AiaDisconnectHandler_t*)&( alertManager->disconnectCb ) = disconnectCb;
    *(void**)&alertManager->disconnectCbUserData = disconnectCbUserData;

    alertManager->offlineAlertVolume = AIA_DEFAULT_OFFLINE_ALERT_VOLUME;
#ifdef AIA_ENABLE_SPEAKER
    alertManager->currentBufferState = AIA_NONE_STATE;
    alertManager->lastBufferState = AIA_NONE_STATE;
#endif
    alertManager->currentUXState = AIA_UX_IDLE;
    alertManager->lastUXState = AIA_UX_IDLE;

    /* Load alerts from persistent storage */
    size_t allAlertsBytes = AiaGetAlertsSize();
    uint8_t* allAlertsBuffer = AiaCalloc( 1, allAlertsBytes );
    if( !allAlertsBuffer )
    {
        AiaLogError( "AiaCalloc failed, bytes=%zu.", allAlertsBytes );
        AiaAlertManager_Destroy( alertManager );
        return NULL;
    }

    if( !AiaLoadAlerts( allAlertsBuffer, allAlertsBytes ) )
    {
        AiaLogError( "AiaLoadBlob failed" );
        AiaFree( allAlertsBuffer );
        AiaAlertManager_Destroy( alertManager );
        return NULL;
    }

    /* Insert the loaded alerts to the alerts list */
    size_t bytePosition = 0;
    while( bytePosition < allAlertsBytes )
    {
        if( !allAlertsBuffer[ bytePosition ] )
        {
            break;
        }
        char readAlertToken[ AIA_ALERT_TOKEN_CHARS ];
        AiaTimepointSeconds_t readScheduledTime = 0;
        AiaDurationMs_t readDuration = 0;
        AiaAlertStorageType_t readAlertType = 0;
        if( !AiaLoadAlert( readAlertToken, AIA_ALERT_TOKEN_CHARS,
                           &readScheduledTime, &readDuration, &readAlertType,
                           &allAlertsBuffer[ bytePosition ] ) )
        {
            AiaLogError( "AiaLoadAlert failed" );
            AiaFree( allAlertsBuffer );
            AiaAlertManager_Destroy( alertManager );
            return NULL;
        }

        bytePosition += AIA_SIZE_OF_ALERT_IN_BYTES;

        /* Add the read token to the alerts list */
        AiaLogDebug( "Adding the alert token: %.*s, scheduled time: %" PRIu64
                     " duration: %" PRIu32 " alert type: %s",
                     AIA_ALERT_TOKEN_CHARS, readAlertToken, readScheduledTime,
                     readDuration,
                     AiaAlertType_ToString( (AiaAlertType_t)readAlertType ) );

        AiaAlertSlot_t* alertSlot = AiaCalloc( 1, sizeof( AiaAlertSlot_t ) );
        if( !alertSlot )
        {
            AiaLogError( "AiaCalloc failed, bytes=%zu",
                         sizeof( AiaAlertSlot_t ) );
            AiaFree( allAlertsBuffer );
            AiaAlertManager_Destroy( alertManager );
            return NULL;
        }
        AiaListDouble( Link_t ) defaultLink = AiaListDouble( LINK_INITIALIZER );
        alertSlot->link = defaultLink;
        memcpy( alertSlot->alertToken, readAlertToken, AIA_ALERT_TOKEN_CHARS );
        alertSlot->scheduledTime = readScheduledTime;
        alertSlot->duration = readDuration;
        alertSlot->alertType = (AiaAlertType_t)readAlertType;
        AiaListDouble( InsertSorted )( &alertManager->allAlerts,
                                       &alertSlot->link,
                                       AiaAlertScheduledTimeComparator );
    }

    /* Free the @c allAlertsBuffer as we don't need it anymore. */
    AiaFree( allAlertsBuffer );

    if( !AiaTimer( Create )( &alertManager->offlineAlertPlayOrStatusCheckTimer,
                             AiaAlertManager_PlayOfflineAlertOrCheckStatus,
                             alertManager ) )
    {
        AiaLogError( "AiaTimer( Create ) failed" );
        AiaAlertManager_Destroy( alertManager );
        return NULL;
    }

    AiaTimepointSeconds_t now = AiaClock_GetTimeSinceNTPEpoch();
    if( !AiaAlertManager_UpdateAlertManagerTime( alertManager, now ) )
    {
        AiaLogError( "AiaAlertManager_UpdateAlertManagerTime failed" );
        AiaAlertManager_Destroy( alertManager );
        return NULL;
    }

    return alertManager;
}

void AiaAlertManager_OnSetAlertVolumeDirectiveReceived(
    void* manager, const void* payload, size_t size,
    AiaSequenceNumber_t sequenceNumber, size_t index )
{
    AiaAlertManager_t* alertManager = (AiaAlertManager_t*)manager;
    AiaAssert( alertManager );
    if( !alertManager )
    {
        AiaLogError( "Null alertManager" );
        return;
    }

    AiaAssert( payload );
    if( !payload )
    {
        AiaLogError( "Null payload, sequenceNumber=%" PRIu32 ", index=%zu",
                     sequenceNumber, index );
        return;
    }

    AiaMutex( Lock )( &alertManager->mutex );
    AiaAlertManager_OnSetAlertVolumeDirectiveReceivedLocked(
        alertManager, payload, size, sequenceNumber, index );
    AiaMutex( Unlock )( &alertManager->mutex );
}

void AiaAlertManager_OnSetAlertDirectiveReceived(
    void* manager, const void* payload, size_t size,
    AiaSequenceNumber_t sequenceNumber, size_t index )
{
    AiaAlertManager_t* alertManager = (AiaAlertManager_t*)manager;
    AiaAssert( alertManager );
    if( !alertManager )
    {
        AiaLogError( "Null alertManager" );
        return;
    }

    AiaAssert( payload );
    if( !payload )
    {
        AiaLogError( "Null payload, sequenceNumber=%" PRIu32 ", index=%zu",
                     sequenceNumber, index );
        return;
    }

    AiaMutex( Lock )( &alertManager->mutex );
    AiaAlertManager_OnSetAlertDirectiveReceivedLocked(
        alertManager, payload, size, sequenceNumber, index );
    AiaMutex( Unlock )( &alertManager->mutex );
}

void AiaAlertManager_OnDeleteAlertDirectiveReceived(
    void* manager, const void* payload, size_t size,
    AiaSequenceNumber_t sequenceNumber, size_t index )
{
    AiaAlertManager_t* alertManager = (AiaAlertManager_t*)manager;
    AiaAssert( alertManager );
    if( !alertManager )
    {
        AiaLogError( "Null alertManager" );
        return;
    }

    AiaAssert( payload );
    if( !payload )
    {
        AiaLogError( "Null payload, sequenceNumber=%" PRIu32 ", index=%zu",
                     sequenceNumber, index );
        return;
    }

    AiaMutex( Lock )( &alertManager->mutex );
    AiaAlertManager_OnDeleteAlertDirectiveReceivedLocked(
        alertManager, payload, size, sequenceNumber, index );
    AiaMutex( Unlock )( &alertManager->mutex );
}

/**
 * Matcher used for @c AiaListDouble( RemoveAllMatches ).
 *
 * @param link Element to check for match.
 * @param matchArgs Pointer to the arguments to use while checking for a match.
 * @return Returns @c true if the alert tokens of the given link and match
 * arguments match, @c false otherwise.
 */
static bool AiaAlertTokenMatcher( const AiaListDouble( Link_t ) * link,
                                  void* matchArgs )
{
    const char* inputToken = (const char*)matchArgs;

    return ( strncmp( ( (AiaAlertSlot_t*)link )->alertToken, inputToken,
                      AIA_ALERT_TOKEN_CHARS ) == 0 );
}

static void AiaAlertManager_OnSetAlertVolumeDirectiveReceivedLocked(
    AiaAlertManager_t* alertManager, const void* payload, size_t size,
    AiaSequenceNumber_t sequenceNumber, size_t index )
{
    AiaJsonLongType alertVolume;
    if( !AiaJsonUtils_ExtractLong(
            payload, size, AIA_SET_ALERT_VOLUME_VOLUME_KEY,
            sizeof( AIA_SET_ALERT_VOLUME_VOLUME_KEY ) - 1, &alertVolume ) )
    {
        AiaLogError( "Failed to get " AIA_SET_ALERT_VOLUME_VOLUME_KEY );
        AiaJsonMessage_t* malformedMessageEvent =
            generateMalformedMessageExceptionEncounteredEvent(
                sequenceNumber, index, AIA_TOPIC_DIRECTIVE );
        if( !malformedMessageEvent )
        {
            AiaLogError(
                "generateMalformedMessageExceptionEncounteredEvent failed" );
            return;
        }
        if( !AiaRegulator_Write(
                alertManager->eventRegulator,
                AiaJsonMessage_ToMessage( malformedMessageEvent ) ) )
        {
            AiaLogError( "AiaRegulator_Write failed" );
            AiaJsonMessage_Destroy( malformedMessageEvent );
            return;
        }
        return;
    }

    AiaLogDebug( "Setting offline alert volume to %" PRIu64, alertVolume );
    alertManager->offlineAlertVolume = alertVolume;
    AiaJsonMessage_t* alertVolumeChangedEvent =
        generateAlertVolumeChangedEvent( alertVolume );
    if( !alertVolumeChangedEvent )
    {
        AiaLogError( "generateAlertVolumeChangedEvent failed" );
        return;
    }
    if( !AiaRegulator_Write(
            alertManager->eventRegulator,
            AiaJsonMessage_ToMessage( alertVolumeChangedEvent ) ) )
    {
        AiaLogError( "AiaRegulator_Write failed" );
        AiaJsonMessage_Destroy( alertVolumeChangedEvent );
    }
}

static void AiaAlertManager_OnSetAlertDirectiveReceivedLocked(
    AiaAlertManager_t* alertManager, const void* payload, size_t size,
    AiaSequenceNumber_t sequenceNumber, size_t index )
{
    const char* alertToken = NULL;
    size_t alertTokenLen = 0;

    if( !AiaFindJsonValue( payload, size, AIA_SET_ALERT_TOKEN_KEY,
                           sizeof( AIA_SET_ALERT_TOKEN_KEY ) - 1, &alertToken,
                           &alertTokenLen ) )
    {
        AiaLogError( "No " AIA_SET_ALERT_TOKEN_KEY " found" );
        AiaJsonMessage_t* malformedMessageEvent =
            generateMalformedMessageExceptionEncounteredEvent(
                sequenceNumber, index, AIA_TOPIC_DIRECTIVE );
        if( !malformedMessageEvent )
        {
            AiaLogError(
                "generateMalformedMessageExceptionEncounteredEvent failed" );
            return;
        }
        if( !AiaRegulator_Write(
                alertManager->eventRegulator,
                AiaJsonMessage_ToMessage( malformedMessageEvent ) ) )
        {
            AiaLogError( "AiaRegulator_Write failed" );
            AiaJsonMessage_Destroy( malformedMessageEvent );
            return;
        }
        return;
    }

    if( !AiaJsonUtils_UnquoteString( &alertToken, &alertTokenLen ) )
    {
        AiaLogError( "Malformed JSON" );
        AiaJsonMessage_t* malformedMessageEvent =
            generateMalformedMessageExceptionEncounteredEvent(
                sequenceNumber, index, AIA_TOPIC_DIRECTIVE );
        if( !malformedMessageEvent )
        {
            AiaLogError(
                "generateMalformedMessageExceptionEncounteredEvent failed" );
            return;
        }
        if( !AiaRegulator_Write(
                alertManager->eventRegulator,
                AiaJsonMessage_ToMessage( malformedMessageEvent ) ) )
        {
            AiaLogError( "AiaRegulator_Write failed" );
            AiaJsonMessage_Destroy( malformedMessageEvent );
            return;
        }
        return;
    }

    if( alertTokenLen > AIA_ALERT_TOKEN_CHARS )
    {
        AiaLogError( "Invalid alert token length: %zu", alertTokenLen );
        AiaJsonMessage_t* malformedMessageEvent =
            generateMalformedMessageExceptionEncounteredEvent(
                sequenceNumber, index, AIA_TOPIC_DIRECTIVE );
        if( !malformedMessageEvent )
        {
            AiaLogError(
                "generateMalformedMessageExceptionEncounteredEvent failed" );
            return;
        }
        if( !AiaRegulator_Write(
                alertManager->eventRegulator,
                AiaJsonMessage_ToMessage( malformedMessageEvent ) ) )
        {
            AiaLogError( "AiaRegulator_Write failed" );
            AiaJsonMessage_Destroy( malformedMessageEvent );
            return;
        }
        return;
    }

    char alertTokenStr[ alertTokenLen ];
    strncpy( alertTokenStr, alertToken, alertTokenLen );

    AiaJsonLongType scheduledTime = 0;
    if( !AiaJsonUtils_ExtractLong(
            payload, size, AIA_SET_ALERT_SCHEDULED_TIME_KEY,
            sizeof( AIA_SET_ALERT_SCHEDULED_TIME_KEY ) - 1, &scheduledTime ) )
    {
        AiaLogError( "Failed to get " AIA_SET_ALERT_SCHEDULED_TIME_KEY );
        AiaJsonMessage_t* malformedMessageEvent =
            generateMalformedMessageExceptionEncounteredEvent(
                sequenceNumber, index, AIA_TOPIC_DIRECTIVE );
        if( !malformedMessageEvent )
        {
            AiaLogError(
                "generateMalformedMessageExceptionEncounteredEvent failed" );
            return;
        }
        if( !AiaRegulator_Write(
                alertManager->eventRegulator,
                AiaJsonMessage_ToMessage( malformedMessageEvent ) ) )
        {
            AiaLogError( "AiaRegulator_Write failed" );
            AiaJsonMessage_Destroy( malformedMessageEvent );
            return;
        }
        return;
    }

    AiaJsonLongType duration = 0;
    if( !AiaJsonUtils_ExtractLong(
            payload, size, AIA_SET_ALERT_DURATION_IN_MILLISECONDS_KEY,
            sizeof( AIA_SET_ALERT_DURATION_IN_MILLISECONDS_KEY ) - 1,
            &duration ) )
    {
        AiaLogError(
            "Failed to get " AIA_SET_ALERT_DURATION_IN_MILLISECONDS_KEY );
        AiaJsonMessage_t* malformedMessageEvent =
            generateMalformedMessageExceptionEncounteredEvent(
                sequenceNumber, index, AIA_TOPIC_DIRECTIVE );
        if( !malformedMessageEvent )
        {
            AiaLogError(
                "generateMalformedMessageExceptionEncounteredEvent failed" );
            return;
        }
        if( !AiaRegulator_Write(
                alertManager->eventRegulator,
                AiaJsonMessage_ToMessage( malformedMessageEvent ) ) )
        {
            AiaLogError( "AiaRegulator_Write failed" );
            AiaJsonMessage_Destroy( malformedMessageEvent );
            return;
        }
        return;
    }

    const char* alertType = NULL;
    size_t alertTypeLen = 0;
    if( !AiaFindJsonValue( payload, size, AIA_SET_ALERT_TYPE_KEY,
                           sizeof( AIA_SET_ALERT_TYPE_KEY ) - 1, &alertType,
                           &alertTypeLen ) )
    {
        AiaLogError( "No " AIA_SET_ALERT_TYPE_KEY " found" );
        AiaJsonMessage_t* malformedMessageEvent =
            generateMalformedMessageExceptionEncounteredEvent(
                sequenceNumber, index, AIA_TOPIC_DIRECTIVE );
        if( !malformedMessageEvent )
        {
            AiaLogError(
                "generateMalformedMessageExceptionEncounteredEvent failed" );
            return;
        }
        if( !AiaRegulator_Write(
                alertManager->eventRegulator,
                AiaJsonMessage_ToMessage( malformedMessageEvent ) ) )
        {
            AiaLogError( "AiaRegulator_Write failed" );
            AiaJsonMessage_Destroy( malformedMessageEvent );
            return;
        }
        return;
    }

    if( !AiaJsonUtils_UnquoteString( &alertType, &alertTypeLen ) )
    {
        AiaLogError( "Malformed JSON" );
        AiaJsonMessage_t* malformedMessageEvent =
            generateMalformedMessageExceptionEncounteredEvent(
                sequenceNumber, index, AIA_TOPIC_DIRECTIVE );
        if( !malformedMessageEvent )
        {
            AiaLogError(
                "generateMalformedMessageExceptionEncounteredEvent failed" );
            return;
        }
        if( !AiaRegulator_Write(
                alertManager->eventRegulator,
                AiaJsonMessage_ToMessage( malformedMessageEvent ) ) )
        {
            AiaLogError( "AiaRegulator_Write failed" );
            AiaJsonMessage_Destroy( malformedMessageEvent );
            return;
        }
        return;
    }

    /* Remove all matches of this alert token from the alerts list */
    AiaListDouble( RemoveAllMatches )( &alertManager->allAlerts,
                                       AiaAlertTokenMatcher,
                                       (void*)alertTokenStr, AiaFree, 0 );

    /* Add the given token to the alerts list */
    AiaLogDebug( "Adding the alert token: %.*s, scheduled time: %" PRIu64
                 " duration: %" PRIu32 " alert type: %.*s",
                 alertTokenLen, alertTokenStr, scheduledTime, duration,
                 alertTypeLen, alertType );
    AiaAlertSlot_t* alertSlot = AiaCalloc( 1, sizeof( AiaAlertSlot_t ) );
    if( !alertSlot )
    {
        AiaLogError( "AiaCalloc failed, bytes=%zu", sizeof( AiaAlertSlot_t ) );
        AiaJsonMessage_t* setAlertFailedEvent =
            generateSetAlertFailedEvent( alertTokenStr, alertTokenLen );
        if( !setAlertFailedEvent )
        {
            AiaLogError( "generateSetAlertFailedEvent failed" );
            return;
        }
        if( !AiaRegulator_Write(
                alertManager->eventRegulator,
                AiaJsonMessage_ToMessage( setAlertFailedEvent ) ) )
        {
            AiaLogError( "AiaRegulator_Write failed" );
            AiaJsonMessage_Destroy( setAlertFailedEvent );
        }
        return;
    }
    AiaListDouble( Link_t ) defaultLink = AiaListDouble( LINK_INITIALIZER );
    alertSlot->link = defaultLink;
    if( !AiaAlertType_FromString( alertType, alertTypeLen,
                                  &alertSlot->alertType ) )
    {
        AiaLogError( "Failed to get alert type from %.*s", alertTypeLen,
                     alertType );
        AiaJsonMessage_t* setAlertFailedEvent =
            generateSetAlertFailedEvent( alertTokenStr, alertTokenLen );
        if( !setAlertFailedEvent )
        {
            AiaLogError( "generateSetAlertFailedEvent failed" );
            AiaFree( alertSlot );
            return;
        }
        if( !AiaRegulator_Write(
                alertManager->eventRegulator,
                AiaJsonMessage_ToMessage( setAlertFailedEvent ) ) )
        {
            AiaLogError( "AiaRegulator_Write failed" );
            AiaFree( alertSlot );
            AiaJsonMessage_Destroy( setAlertFailedEvent );
        }
        return;
    }
    strncpy( alertSlot->alertToken, alertTokenStr, alertTokenLen );
    alertSlot->scheduledTime = scheduledTime;
    alertSlot->duration = duration;
    AiaListDouble( InsertSorted )( &alertManager->allAlerts, &alertSlot->link,
                                   AiaAlertScheduledTimeComparator );

    if( !AiaAlertManager_UpdateOfflineAlertTimersLocked(
            alertManager, AiaClock_GetTimeSinceNTPEpoch() ) )
    {
        AiaLogError( "AiaAlertManager_UpdateOfflineAlertTimersLocked failed" );

        /* Remove the in-memory copy of this alert */
        AiaListDouble( RemoveAllMatches )( &alertManager->allAlerts,
                                           AiaAlertTokenMatcher,
                                           (void*)alertTokenStr, AiaFree, 0 );

        AiaJsonMessage_t* setAlertFailedEvent =
            generateSetAlertFailedEvent( alertTokenStr, alertTokenLen );
        if( !setAlertFailedEvent )
        {
            AiaLogError( "generateSetAlertFailedEvent failed" );
            return;
        }
        if( !AiaRegulator_Write(
                alertManager->eventRegulator,
                AiaJsonMessage_ToMessage( setAlertFailedEvent ) ) )
        {
            AiaLogError( "AiaRegulator_Write failed" );
            AiaJsonMessage_Destroy( setAlertFailedEvent );
        }

        return;
    }

    if( !AiaStoreAlert( alertSlot->alertToken, AIA_ALERT_TOKEN_CHARS,
                        scheduledTime, duration, alertSlot->alertType ) )
    {
        AiaLogError( "AiaStoreAlert failed" );
        AiaJsonMessage_t* setAlertFailedEvent =
            generateSetAlertFailedEvent( alertTokenStr, alertTokenLen );
        if( !setAlertFailedEvent )
        {
            AiaLogError( "generateSetAlertFailedEvent failed" );

            /* Remove the in-memory copy of this alert */
            AiaListDouble( RemoveAllMatches )(
                &alertManager->allAlerts, AiaAlertTokenMatcher,
                (void*)alertTokenStr, AiaFree, 0 );

            /* Update the offline alert timer */
            if( !AiaAlertManager_UpdateOfflineAlertTimersLocked(
                    alertManager, AiaClock_GetTimeSinceNTPEpoch() ) )
            {
                AiaLogError(
                    "AiaAlertManager_UpdateOfflineAlertTimersLocked failed" );
            }

            return;
        }
        if( !AiaRegulator_Write(
                alertManager->eventRegulator,
                AiaJsonMessage_ToMessage( setAlertFailedEvent ) ) )
        {
            AiaLogError( "AiaRegulator_Write failed" );
            AiaJsonMessage_Destroy( setAlertFailedEvent );
        }

        /* Remove the in-memory copy of this alert */
        AiaListDouble( RemoveAllMatches )( &alertManager->allAlerts,
                                           AiaAlertTokenMatcher,
                                           (void*)alertTokenStr, AiaFree, 0 );

        /* Update the offline alert timer */
        if( !AiaAlertManager_UpdateOfflineAlertTimersLocked(
                alertManager, AiaClock_GetTimeSinceNTPEpoch() ) )
        {
            AiaLogError(
                "AiaAlertManager_UpdateOfflineAlertTimersLocked failed" );
        }

        return;
    }

    AiaJsonMessage_t* setAlertSucceededEvent =
        generateSetAlertSucceededEvent( alertTokenStr, alertTokenLen );
    if( !setAlertSucceededEvent )
    {
        AiaLogError( "generateSetAlertSucceededEvent failed" );

        /* Remove the in-memory copy of this alert */
        AiaListDouble( RemoveAllMatches )( &alertManager->allAlerts,
                                           AiaAlertTokenMatcher,
                                           (void*)alertTokenStr, AiaFree, 0 );

        /* Update the offline alert timer */
        if( !AiaAlertManager_UpdateOfflineAlertTimersLocked(
                alertManager, AiaClock_GetTimeSinceNTPEpoch() ) )
        {
            AiaLogError(
                "AiaAlertManager_UpdateOfflineAlertTimersLocked failed" );
        }

        /* Remove the alert from persistent storage */
        if( !AiaDeleteAlert( alertTokenStr, AIA_ALERT_TOKEN_CHARS ) )
        {
            AiaLogWarn( "AiaDeleteAlert failed" );
        }

        return;
    }
    if( !AiaRegulator_Write(
            alertManager->eventRegulator,
            AiaJsonMessage_ToMessage( setAlertSucceededEvent ) ) )
    {
        AiaLogError( "AiaRegulator_Write failed" );
        AiaJsonMessage_Destroy( setAlertSucceededEvent );

        /* Remove the in-memory copy of this alert */
        AiaListDouble( RemoveAllMatches )( &alertManager->allAlerts,
                                           AiaAlertTokenMatcher,
                                           (void*)alertTokenStr, AiaFree, 0 );

        /* Update the offline alert timer */
        if( !AiaAlertManager_UpdateOfflineAlertTimersLocked(
                alertManager, AiaClock_GetTimeSinceNTPEpoch() ) )
        {
            AiaLogError(
                "AiaAlertManager_UpdateOfflineAlertTimersLocked failed" );
        }

        /* Remove the alert from persistent storage */
        if( !AiaDeleteAlert( alertTokenStr, AIA_ALERT_TOKEN_CHARS ) )
        {
            AiaLogWarn( "AiaDeleteAlert failed" );
        }
    }
}

static void AiaAlertManager_OnDeleteAlertDirectiveReceivedLocked(
    AiaAlertManager_t* alertManager, const void* payload, size_t size,
    AiaSequenceNumber_t sequenceNumber, size_t index )
{
    const char* alertToken = NULL;
    size_t alertTokenLen = 0;
    if( !AiaFindJsonValue( payload, size, AIA_DELETE_ALERT_TOKEN_KEY,
                           sizeof( AIA_DELETE_ALERT_TOKEN_KEY ) - 1,
                           &alertToken, &alertTokenLen ) )
    {
        AiaLogError( "No " AIA_DELETE_ALERT_TOKEN_KEY " found" );
        AiaJsonMessage_t* malformedMessageEvent =
            generateMalformedMessageExceptionEncounteredEvent(
                sequenceNumber, index, AIA_TOPIC_DIRECTIVE );
        if( !malformedMessageEvent )
        {
            AiaLogError(
                "generateMalformedMessageExceptionEncounteredEvent failed" );
            return;
        }
        if( !AiaRegulator_Write(
                alertManager->eventRegulator,
                AiaJsonMessage_ToMessage( malformedMessageEvent ) ) )
        {
            AiaLogError( "AiaRegulator_Write failed" );
            AiaJsonMessage_Destroy( malformedMessageEvent );
            return;
        }
        return;
    }

    if( !AiaJsonUtils_UnquoteString( &alertToken, &alertTokenLen ) )
    {
        AiaLogError( "Malformed JSON" );
        AiaJsonMessage_t* malformedMessageEvent =
            generateMalformedMessageExceptionEncounteredEvent(
                sequenceNumber, index, AIA_TOPIC_DIRECTIVE );
        if( !malformedMessageEvent )
        {
            AiaLogError(
                "generateMalformedMessageExceptionEncounteredEvent failed" );
            return;
        }
        if( !AiaRegulator_Write(
                alertManager->eventRegulator,
                AiaJsonMessage_ToMessage( malformedMessageEvent ) ) )
        {
            AiaLogError( "AiaRegulator_Write failed" );
            AiaJsonMessage_Destroy( malformedMessageEvent );
            return;
        }
        return;
    }

    if( alertTokenLen > AIA_ALERT_TOKEN_CHARS )
    {
        AiaLogError( "Invalid alert token length: %zu", alertTokenLen );
        AiaJsonMessage_t* malformedMessageEvent =
            generateMalformedMessageExceptionEncounteredEvent(
                sequenceNumber, index, AIA_TOPIC_DIRECTIVE );
        if( !malformedMessageEvent )
        {
            AiaLogError(
                "generateMalformedMessageExceptionEncounteredEvent failed" );
            return;
        }
        if( !AiaRegulator_Write(
                alertManager->eventRegulator,
                AiaJsonMessage_ToMessage( malformedMessageEvent ) ) )
        {
            AiaLogError( "AiaRegulator_Write failed" );
            AiaJsonMessage_Destroy( malformedMessageEvent );
            return;
        }
        return;
    }

    char alertTokenStr[ alertTokenLen ];
    strncpy( alertTokenStr, alertToken, alertTokenLen );

    AiaLogDebug( "Deleting alert token %.*s", alertTokenLen, alertTokenStr );

    /* Remove the alert from persistent storage */
    if( !AiaDeleteAlert( alertTokenStr, AIA_ALERT_TOKEN_CHARS ) )
    {
        AiaLogError( "AiaDeleteAlert failed" );
        AiaJsonMessage_t* deleteAlertFailedEvent =
            generateDeleteAlertFailedEvent( alertTokenStr, alertTokenLen );
        if( !deleteAlertFailedEvent )
        {
            AiaLogError( "generateDeleteAlertFailedEvent failed" );
            return;
        }
        if( !AiaRegulator_Write(
                alertManager->eventRegulator,
                AiaJsonMessage_ToMessage( deleteAlertFailedEvent ) ) )
        {
            AiaLogError( "AiaRegulator_Write failed" );
            AiaJsonMessage_Destroy( deleteAlertFailedEvent );
        }
        return;
    }

    /* Remove all matches of this alert token from the alerts list */
    AiaListDouble( RemoveAllMatches )( &alertManager->allAlerts,
                                       AiaAlertTokenMatcher,
                                       (void*)alertTokenStr, AiaFree, 0 );

    if( !AiaAlertManager_UpdateOfflineAlertTimersLocked(
            alertManager, AiaClock_GetTimeSinceNTPEpoch() ) )
    {
        AiaLogError( "AiaAlertManager_UpdateOfflineAlertTimersLocked failed" );
        return;
    }

    AiaJsonMessage_t* deleteAlertSucceededEvent =
        generateDeleteAlertSucceededEvent( alertTokenStr, alertTokenLen );
    if( !deleteAlertSucceededEvent )
    {
        AiaLogError( "generateDeleteAlertSucceededEvent failed" );
        return;
    }
    if( !AiaRegulator_Write(
            alertManager->eventRegulator,
            AiaJsonMessage_ToMessage( deleteAlertSucceededEvent ) ) )
    {
        AiaLogError( "AiaRegulator_Write failed" );
        AiaJsonMessage_Destroy( deleteAlertSucceededEvent );
    }
}

size_t AiaAlertManager_GetTokens( AiaAlertManager_t* alertManager,
                                  uint8_t** alertTokens )
{
    if( !alertManager )
    {
        AiaLogError( "Null alertManager" );
        return 0;
    }
    if( !alertTokens )
    {
        AiaLogError( "Null alertTokens" );
        return 0;
    }

    AiaMutex( Lock )( &alertManager->mutex );

    size_t numAlertTokens = AiaListDouble( Count )( &alertManager->allAlerts );
    size_t tokenArrayBytes = 0;

    if( numAlertTokens == 0 )
    {
        *alertTokens = NULL;
        AiaLogDebug( "There are no alert tokens" );
        AiaMutex( Unlock )( &alertManager->mutex );
        return 0;
    }

    /* Account for the comma separator between alert tokens and the quotes
     * around token names */
    size_t tokenLengthWithNoSeparator = AIA_ALERT_TOKEN_CHARS + 2;
    size_t tokenLengthWithSeparator = tokenLengthWithNoSeparator + 1;

    /* All except the last one will have a separator afterwards */
    tokenArrayBytes = ( ( numAlertTokens - 1 ) * tokenLengthWithSeparator );
    tokenArrayBytes += tokenLengthWithNoSeparator;

    /* Account for the null terminator added by snprintf at the end */
    *alertTokens = AiaCalloc( sizeof( uint8_t ), tokenArrayBytes + 1 );
    if( !*alertTokens )
    {
        AiaLogError( "AiaCalloc failed, bytes=%zu.", tokenArrayBytes );
        AiaMutex( Unlock )( &alertManager->mutex );
        return 0;
    }

    size_t arrayPosition = 0;
    AiaListDouble( Link_t )* link = NULL;
    AiaListDouble( ForEach )( &alertManager->allAlerts, link )
    {
        AiaAlertSlot_t* slot = (AiaAlertSlot_t*)link;
        /* Skip this alert if it expired for more than the threshold */
        AiaTimepointSeconds_t now = AiaClock_GetTimeSinceNTPEpoch();
        if( now >= slot->scheduledTime )
        {
            AiaDurationSeconds_t timeSinceAlertCreation =
                now - slot->scheduledTime;
            if( timeSinceAlertCreation > AIA_ALERT_EXPIRATION_DURATION )
            {
                AiaLogDebug( "Alert %s expired", slot->alertToken );

                /* Remove the alert from persistent storage */
                if( !AiaDeleteAlert( slot->alertToken, AIA_ALERT_TOKEN_CHARS ) )
                {
                    AiaLogError( "AiaDeleteAlert failed" );
                    AiaJsonMessage_t* deleteAlertFailedEvent =
                        generateDeleteAlertFailedEvent( slot->alertToken,
                                                        AIA_ALERT_TOKEN_CHARS );
                    if( !deleteAlertFailedEvent )
                    {
                        AiaLogError( "generateDeleteAlertFailedEvent failed" );
                        AiaFree( *alertTokens );
                        AiaMutex( Unlock )( &alertManager->mutex );
                        return 0;
                    }
                    if( !AiaRegulator_Write( alertManager->eventRegulator,
                                             AiaJsonMessage_ToMessage(
                                                 deleteAlertFailedEvent ) ) )
                    {
                        AiaLogError( "AiaRegulator_Write failed" );
                        AiaJsonMessage_Destroy( deleteAlertFailedEvent );
                    }

                    AiaFree( *alertTokens );
                    AiaMutex( Unlock )( &alertManager->mutex );
                    return 0;
                }

                continue;
            }
        }
        /* First entry */
        int result = 0;
        if( arrayPosition == 0 )
        {
            result = snprintf( (char*)*alertTokens + arrayPosition,
                               tokenLengthWithNoSeparator + 1, "\"%s\"",
                               slot->alertToken );
        }
        else
        {
            result = snprintf( (char*)*alertTokens + arrayPosition,
                               tokenLengthWithSeparator + 1, ",\"%s\"",
                               slot->alertToken );
        }

        if( result <= 0 )
        {
            AiaLogError( "snprintf failed: %d", result );
            AiaFree( *alertTokens );
            AiaMutex( Unlock )( &alertManager->mutex );
            return 0;
        }
        arrayPosition += result;
    }

    AiaMutex( Unlock )( &alertManager->mutex );

    return tokenArrayBytes;
}

void AiaAlertManager_Destroy( AiaAlertManager_t* alertManager )
{
    if( !alertManager )
    {
        AiaLogDebug( "Null alertManager." );
        return;
    }

    AiaTimer( Destroy )( &alertManager->offlineAlertPlayOrStatusCheckTimer );

    AiaMutex( Lock )( &alertManager->mutex );

    AiaListDouble( RemoveAll )( &alertManager->allAlerts, AiaFree, 0 );

    AiaMutex( Unlock )( &alertManager->mutex );
    AiaMutex( Destroy )( &alertManager->mutex );
    AiaFree( alertManager );
}

static AiaJsonMessage_t* generateSetAlertSucceededEvent( const char* alertToken,
                                                         size_t alertTokenLen )
{
    static const char* AIA_SET_ALERT_SUCCEEDED_FORMAT =
        /* clang-format off */
    "{"
        "\""AIA_SET_ALERT_SUCCEEDED_TOKEN_KEY"\":\"%.*s\""
    "}";
    /* clang-format on */
    int numCharsRequired = snprintf( NULL, 0, AIA_SET_ALERT_SUCCEEDED_FORMAT,
                                     alertTokenLen, alertToken );
    if( numCharsRequired < 0 )
    {
        AiaLogError( "snprintf failed" );
        return NULL;
    }
    char payloadBuffer[ numCharsRequired + 1 ];
    if( snprintf( payloadBuffer, numCharsRequired + 1,
                  AIA_SET_ALERT_SUCCEEDED_FORMAT, alertTokenLen,
                  alertToken ) < 0 )
    {
        AiaLogError( "snprint failed" );
        return NULL;
    }
    AiaJsonMessage_t* jsonMessage = AiaJsonMessage_Create(
        AIA_EVENTS_SET_ALERT_SUCCEEDED, NULL, payloadBuffer );
    return jsonMessage;
}

static AiaJsonMessage_t* generateSetAlertFailedEvent( const char* alertToken,
                                                      size_t alertTokenLen )
{
    static const char* AIA_SET_ALERT_FAILED_FORMAT =
        /* clang-format off */
    "{"
        "\""AIA_SET_ALERT_FAILED_TOKEN_KEY"\":\"%.*s\""
    "}";
    /* clang-format on */
    int numCharsRequired = snprintf( NULL, 0, AIA_SET_ALERT_FAILED_FORMAT,
                                     alertTokenLen, alertToken );
    if( numCharsRequired < 0 )
    {
        AiaLogError( "snprintf failed" );
        return NULL;
    }
    char payloadBuffer[ numCharsRequired + 1 ];
    if( snprintf( payloadBuffer, numCharsRequired + 1,
                  AIA_SET_ALERT_FAILED_FORMAT, alertTokenLen, alertToken ) < 0 )
    {
        AiaLogError( "snprint failed" );
        return NULL;
    }
    AiaJsonMessage_t* jsonMessage = AiaJsonMessage_Create(
        AIA_EVENTS_SET_ALERT_FAILED, NULL, payloadBuffer );
    return jsonMessage;
}

static AiaJsonMessage_t* generateDeleteAlertSucceededEvent(
    const char* alertToken, size_t alertTokenLen )
{
    static const char* AIA_DELETE_ALERT_SUCCEEDED_FORMAT =
        /* clang-format off */
    "{"
        "\""AIA_DELETE_ALERT_SUCCEEDED_TOKEN_KEY"\":\"%.*s\""
    "}";
    /* clang-format on */
    int numCharsRequired = snprintf( NULL, 0, AIA_DELETE_ALERT_SUCCEEDED_FORMAT,
                                     alertTokenLen, alertToken );
    if( numCharsRequired < 0 )
    {
        AiaLogError( "snprintf failed" );
        return NULL;
    }
    char payloadBuffer[ numCharsRequired + 1 ];
    if( snprintf( payloadBuffer, numCharsRequired + 1,
                  AIA_DELETE_ALERT_SUCCEEDED_FORMAT, alertTokenLen,
                  alertToken ) < 0 )
    {
        AiaLogError( "snprint failed" );
        return NULL;
    }
    AiaJsonMessage_t* jsonMessage = AiaJsonMessage_Create(
        AIA_EVENTS_DELETE_ALERT_SUCCEEDED, NULL, payloadBuffer );
    return jsonMessage;
}

static AiaJsonMessage_t* generateDeleteAlertFailedEvent( const char* alertToken,
                                                         size_t alertTokenLen )
{
    static const char* AIA_DELETE_ALERT_FAILED_FORMAT =
        /* clang-format off */
    "{"
        "\""AIA_DELETE_ALERT_FAILED_TOKEN_KEY"\":\"%.*s\""
    "}";
    /* clang-format on */
    int numCharsRequired = snprintf( NULL, 0, AIA_DELETE_ALERT_FAILED_FORMAT,
                                     alertTokenLen, alertToken );
    if( numCharsRequired < 0 )
    {
        AiaLogError( "snprintf failed" );
        return NULL;
    }
    char payloadBuffer[ numCharsRequired + 1 ];
    if( snprintf( payloadBuffer, numCharsRequired + 1,
                  AIA_DELETE_ALERT_FAILED_FORMAT, alertTokenLen,
                  alertToken ) < 0 )
    {
        AiaLogError( "snprint failed" );
        return NULL;
    }
    AiaJsonMessage_t* jsonMessage = AiaJsonMessage_Create(
        AIA_EVENTS_DELETE_ALERT_FAILED, NULL, payloadBuffer );
    return jsonMessage;
}

static AiaJsonMessage_t* generateAlertVolumeChangedEvent(
    const uint8_t alertVolume )
{
    static const char* AIA_ALERT_VOLUME_CHANGED_FORMAT =
        /* clang-format off */
    "{"
        "\""AIA_ALERT_VOLUME_CHANGED_VOLUME_KEY"\":%" PRIu8
    "}";
    /* clang-format on */
    int numCharsRequired =
        snprintf( NULL, 0, AIA_ALERT_VOLUME_CHANGED_FORMAT, alertVolume );
    if( numCharsRequired < 0 )
    {
        AiaLogError( "snprintf failed" );
        return NULL;
    }
    char payloadBuffer[ numCharsRequired + 1 ];
    if( snprintf( payloadBuffer, numCharsRequired + 1,
                  AIA_ALERT_VOLUME_CHANGED_FORMAT, alertVolume ) < 0 )
    {
        AiaLogError( "snprint failed" );
        return NULL;
    }
    AiaJsonMessage_t* jsonMessage = AiaJsonMessage_Create(
        AIA_EVENTS_ALERT_VOLUME_CHANGED, NULL, payloadBuffer );
    return jsonMessage;
}

bool AiaAlertManager_UpdateAlertManagerTime( AiaAlertManager_t* alertManager,
                                             AiaTimepointSeconds_t currentTime )
{
    if( !alertManager )
    {
        AiaLogError( "Null alertManager" );
        return false;
    }

    AiaMutex( Lock )( &alertManager->mutex );
    if( !AiaAlertManager_UpdateOfflineAlertTimersLocked( alertManager,
                                                         currentTime ) )
    {
        AiaLogError( "AiaAlertManager_UpdateOfflineAlertTimersLocked failed" );
        AiaMutex( Unlock )( &alertManager->mutex );
        return false;
    }
    AiaMutex( Unlock )( &alertManager->mutex );

    return true;
}

#ifdef AIA_ENABLE_SPEAKER
void AiaAlertManager_UpdateSpeakerBufferState(
    AiaAlertManager_t* alertManager,
    AiaSpeakerManagerBufferState_t bufferState )
{
    if( !alertManager )
    {
        AiaLogError( "Null alertManager" );
        return;
    }
    AiaMutex( Lock )( &alertManager->mutex );
    alertManager->lastBufferState = alertManager->currentBufferState;
    alertManager->currentBufferState = bufferState;
    if( bufferState == AIA_UNDERRUN_STATE )
    {
        alertManager->numUnderruns++;
    }
    AiaMutex( Unlock )( &alertManager->mutex );
}
#endif

void AiaAlertManager_UpdateUXState( AiaAlertManager_t* alertManager,
                                    AiaUXState_t uxState )
{
    if( !alertManager )
    {
        AiaLogError( "Null alertManager" );
        return;
    }
    AiaMutex( Lock )( &alertManager->mutex );
    alertManager->lastUXState = alertManager->currentUXState;
    alertManager->currentUXState = uxState;
    alertManager->numStateChanges++;
    AiaMutex( Unlock )( &alertManager->mutex );
}

static bool AiaAlertManager_UpdateOfflineAlertTimersLocked(
    AiaAlertManager_t* alertManager, AiaTimepointSeconds_t currentTime )
{
    /* Cancel the existing periodic offline alert related timers */
    AiaTimer( Destroy )( &alertManager->offlineAlertPlayOrStatusCheckTimer );
    if( !AiaTimer( Create )( &alertManager->offlineAlertPlayOrStatusCheckTimer,
                             AiaAlertManager_PlayOfflineAlertOrCheckStatus,
                             alertManager ) )
    {
        AiaLogError( "AiaTimer( Create ) failed" );
        return false;
    }

    /* Set the offline alert related timers if we have any alerts */
    if( AiaListDouble( Count )( &alertManager->allAlerts ) > 0 )
    {
        AiaListDouble( Link_t )* link =
            AiaListDouble( PeekHead )( &alertManager->allAlerts );
        AiaAlertSlot_t* slot = (AiaAlertSlot_t*)link;
        AiaDurationMs_t durationUntilNextOfflineAlert = 0;
        AiaTimepointSeconds_t offlineAlertTime = slot->scheduledTime;

        /* Update duration until next offline alert if necessary */
        if( currentTime < offlineAlertTime )
        {
            durationUntilNextOfflineAlert =
                ( offlineAlertTime - currentTime ) * AIA_MS_PER_SECOND;
        }

        /* Update the @c offlineAlertPlayOrStatusCheckTimer timer */
        AiaLogDebug(
            "Setting the offlineAlertPlayOrStatusCheckTimer for "
            "%" PRIu32 " milliseconds.",
            durationUntilNextOfflineAlert );
        if( !AiaTimer( Arm )( &alertManager->offlineAlertPlayOrStatusCheckTimer,
                              durationUntilNextOfflineAlert,
                              AIA_OFFLINE_ALERT_STATUS_CHECK_CADENCE_MS ) )
        {
            AiaLogError( "AiaTimer( Arm ) failed" );
            AiaTimer( Destroy )(
                &alertManager->offlineAlertPlayOrStatusCheckTimer );
            return false;
        }
#ifdef AIA_ENABLE_SPEAKER
        alertManager->numUnderruns = 0;
        alertManager->lastBufferState = alertManager->currentBufferState;
#endif

        alertManager->numStateChanges = 0;
        alertManager->lastUXState = alertManager->currentUXState;
    }

    return true;
}

static void AiaAlertManager_PlayOfflineAlertOrCheckStatus( void* context )
{
    AiaAlertManager_t* alertManager = (AiaAlertManager_t*)context;
    AiaAssert( alertManager );
    if( !alertManager )
    {
        AiaLogError( "Null alertManager" );
        AiaCriticalFailure();
        return;
    }

    /* Check if the speaker is open and the UX state is speaking,
     * thinking or alerting */
#ifdef AIA_ENABLE_SPEAKER
    AiaMutex( Lock )( &alertManager->mutex );
    AiaUXState_t currentUXState =
        alertManager->uxStateCheckCb( alertManager->uxStateCheckCbUserData );
    if( !alertManager->speakerCheckCb( alertManager->speakerCheckCbUserData ) ||
        ( currentUXState != AIA_UX_SPEAKING &&
          currentUXState != AIA_UX_THINKING &&
          currentUXState != AIA_UX_ALERTING ) )
    {
        /* Get the information about the first available alert if we have any */
        if( !AiaListDouble( Count )( &alertManager->allAlerts ) )
        {
            AiaLogDebug( "There are no alerts" );
            AiaMutex( Unlock )( &alertManager->mutex );
            return;
        }

        AiaListDouble( Link_t )* link =
            AiaListDouble( PeekHead )( &alertManager->allAlerts );
        AiaAlertSlot_t* slot = (AiaAlertSlot_t*)link;

        AiaMutex( Unlock )( &alertManager->mutex );

        /* TODO: ADSER-1963 Create a RequestTimeSynchronization function */
        AiaLogDebug( "Playing the offline alert" );

        if( !alertManager->startOfflineAlertCb(
                slot, alertManager->startOfflineAlertCbUserData,
                alertManager->offlineAlertVolume ) )
        {
            AiaLogDebug( "Failed to play offline alert data" );
            return;
        }
        if( currentUXState != AIA_UX_ALERTING )
        {
            alertManager->uxStateUpdateCb(
                alertManager->uxStateUpdateCbUserData,
                AIA_ATTENTION_STATE_ALERTING );
        }
    }
    else
    {
        AiaLogDebug(
            "Not playing the offline alert, check if we should be "
            "disconnecting from the service" );
        bool shouldDisconnect =
            AiaAlertManager_CheckSpeakerBufferAndUXStateLocked( alertManager );
        AiaMutex( Unlock )( &alertManager->mutex );
        if( shouldDisconnect )
        {
            /* Call the disconnect callback */
            AiaLogDebug(
                "Disconnecting from service due to being in underrun state "
                "longer than the threshold!" );
            if( !alertManager->disconnectCb(
                    alertManager->disconnectCbUserData,
                    AIA_CONNECTION_ON_DISCONNECTED_GOING_OFFLINE, NULL ) )
            {
                AiaLogError( "Failed to disconnect" );
            }
        }
    }
#endif
}

static bool AiaAlertManager_CheckSpeakerBufferAndUXStateLocked(
    AiaAlertManager_t* alertManager )
{
    bool shouldDisconnect = false;
#ifdef AIA_ENABLE_SPEAKER
    if( ( alertManager->numUnderruns > AIA_SPEAKER_STATUS_UNDERRUN_LIMIT ) ||
        ( alertManager->lastBufferState == AIA_UNDERRUN_STATE &&
          alertManager->currentBufferState == AIA_UNDERRUN_STATE &&
          alertManager->numUnderruns == 0 ) )
    {
        shouldDisconnect = true;
    }

    alertManager->numUnderruns = 0;
    alertManager->lastBufferState = alertManager->currentBufferState;
#endif

    if( !shouldDisconnect && alertManager->lastUXState == AIA_UX_ALERTING &&
        alertManager->currentUXState == AIA_UX_ALERTING &&
        alertManager->numStateChanges == 0 )
    {
        shouldDisconnect = true;
    }

    alertManager->numStateChanges = 0;
    alertManager->lastUXState = alertManager->currentUXState;

    return shouldDisconnect;
}

bool AiaAlertManager_DeleteAlert( AiaAlertManager_t* alertManager,
                                  const char* alertToken )
{
    if( !alertManager )
    {
        AiaLogError( "Null alertManager" );
        return false;
    }
    AiaMutex( Lock )( &alertManager->mutex );

    /* Remove the in-memory copy of this alert */
    AiaListDouble( RemoveAllMatches )( &alertManager->allAlerts,
                                       AiaAlertTokenMatcher, (void*)alertToken,
                                       AiaFree, 0 );

    /* Update the offline alert timer */
    if( !AiaAlertManager_UpdateOfflineAlertTimersLocked(
            alertManager, AiaClock_GetTimeSinceNTPEpoch() ) )
    {
        AiaLogError( "AiaAlertManager_UpdateOfflineAlertTimersLocked failed" );
        return false;
    }

    AiaMutex( Unlock )( &alertManager->mutex );
    /* Remove the alert from persistent storage */
    if( !AiaDeleteAlert( alertToken, AIA_ALERT_TOKEN_CHARS ) )
    {
        AiaLogError( "AiaDeleteAlert failed" );
        return false;
    }

    return true;
}
