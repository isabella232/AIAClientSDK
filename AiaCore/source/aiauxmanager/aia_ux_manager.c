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
 * @file aia_ux_manager.c
 * @brief Implements functions for the AiaUXManager_t type.
 */

#include <aiauxmanager/aia_ux_manager.h>
#include <aiauxmanager/private/aia_ux_manager.h>

#include <aiacore/aia_exception_encountered_utils.h>
#include <aiacore/aia_json_constants.h>
#include <aiacore/aia_json_message.h>
#include <aiacore/aia_json_utils.h>
#include <aiacore/aia_topic.h>

#ifdef AIA_ENABLE_SPEAKER
#include <aiaspeakermanager/aia_speaker_manager.h>
#include <aiaspeakermanager/private/aia_speaker_manager.h>
#endif

#include <inttypes.h>

#include AiaListDouble( HEADER )
#include AiaMutex( HEADER )

#ifdef AIA_ENABLE_SPEAKER
/** Used to hold information about action callbacks related to an offset. */
typedef struct AiaUXManagerOffsetActionSlot
{
    /** The actual link in the list. */
    AiaListDouble( Link_t ) link;

    /** The UX manager associated with this action. */
    AiaUXManager_t* uxManager;

    /** Opaque handle for an action. */
    AiaSpeakerActionHandle_t id;

    /** The attention state associated with this action. */
    AiaServerAttentionState_t attentionState;
} AiaUXManagerOffsetActionSlot_t;
#endif

/**
 * Underlying struct that contains all the data required to present the @c
 * AiaUXManager_t abstraction.
 */
struct AiaUXManager
{
    /** Mutex used to guard against asynchronous calls in threaded
     * environments. */
    AiaMutex_t mutex;

    /** @name Variables synchronized by mutex. */
    /** @{ */

#ifdef AIA_ENABLE_MICROPHONE
    /** The current state of the microphone. */
    AiaMicrophoneState_t currentMicrophoneState;
#endif

    /** The current attention state sent by AIA. */
    AiaServerAttentionState_t currentAttentionState;

    /** The current UX state of the @c AiaUXManager_t. */
    AiaUXState_t currentUXState;

    /** Used to notify the client of the UX state and attention state to
     * manifest. */
    const AiaUXStateObserverCb_t observer;

    /** Context to pass along to @c observer. */
    void* const observerUserData;

#ifdef AIA_ENABLE_SPEAKER
    /** Collection of actions. */
    AiaListDouble_t offsetActions;
#endif

    /** @} */

    /** Used to publish outbound events. */
    AiaRegulator_t* const eventRegulator;

#ifdef AIA_ENABLE_SPEAKER
    /** Used to change state at specific offsets. */
    AiaSpeakerManager_t* const speakerManager;
#endif
};

/**
 * Aggregates @c currentMicrophoneState and @c currentAttentionState into the
 * appropriate @c AiaUXState_t and notifies the @c observer.
 *
 * @param uxManager The @c AiaUXManager_t to act on.
 * @note @c mutex must be locked before invoking this method.
 */
static void AiaUXManager_AggregateStatesLocked( AiaUXManager_t* uxManager );

/**
 * Converts a char array representing an Attention State string received in a
 * json directive to its corresponding enum value.
 *
 * @param state The attention state in the payload.
 * @param stateLen The length of @c state, without a null-terminating character.
 * @param[out] attentionState The converted enum. Behavior is unspecified if
 * this is NULL.
 * @return @c true if a conversion could be performed or @c false otherwise.
 */
static bool AttentionStateJsonToEnum(
    const char* state, size_t stateLen,
    AiaServerAttentionState_t* attentionState );

#ifdef AIA_ENABLE_SPEAKER

/**
 * Helper function that handles @c SetAttentionState directives that come
 * associated with an offset.
 *
 * @param uxManager The @c AiaUXManager_t to act on.
 * @param attentionState The @c AiaServerAttentionState_t in the directive.
 * @param offset The offset in the directive.
 */
static void AiaUXManager_HandleSetAttentionStateAtOffset(
    AiaUXManager_t* uxManager, AiaServerAttentionState_t attentionState,
    AiaBinaryAudioStreamOffset_t offset );

/**
 * Callback from a @c AiaSpeakerManager_t to notify that an offset has been
 * reached.
 *
 * @param actionValid @c true if the offset was reached or @c false if the
 * offset was invalidated.
 * @param userData This should point to the @c AiaUXManager_t that invoked the
 * action.
 */
static void AiaUXManager_OnActionInvokedAtOffset( bool actionValid,
                                                  void* userData );

#endif

AiaUXManager_t* AiaUXManager_Create( AiaRegulator_t* eventRegulator,
                                     AiaUXStateObserverCb_t stateObserver,
                                     void* stateObserverUserData
#ifdef AIA_ENABLE_SPEAKER
                                     ,
                                     AiaSpeakerManager_t* speakerManager
#endif
)
{
    if( !eventRegulator )
    {
        AiaLogError( "Null eventRegulator" );
        return NULL;
    }
    if( !stateObserver )
    {
        AiaLogError( "Null stateObserver" );
        return NULL;
    }

    AiaUXManager_t* uxManager =
        (AiaUXManager_t*)AiaCalloc( 1, sizeof( AiaUXManager_t ) );
    if( !uxManager )
    {
        AiaLogError( "AiaCalloc failed, bytes=%zu.", sizeof( AiaUXManager_t ) );
        return NULL;
    }

    if( !AiaMutex( Create )( &uxManager->mutex, false ) )
    {
        AiaLogError( "AiaMutex( Create ) failed." );
        AiaFree( uxManager );
        return NULL;
    }

    *(AiaRegulator_t**)&uxManager->eventRegulator = eventRegulator;
    *(AiaUXStateObserverCb_t*)&uxManager->observer = stateObserver;
    *(void**)&uxManager->observerUserData = stateObserverUserData;

#ifdef AIA_ENABLE_SPEAKER
    AiaListDouble( Create )( &uxManager->offsetActions );
    if( !speakerManager )
    {
        AiaLogError( "Null speakerManager" );
        AiaUXManager_Destroy( uxManager );
        return NULL;
    }
    *(AiaSpeakerManager_t**)&uxManager->speakerManager = speakerManager;
#endif

    return uxManager;
}

AiaUXState_t AiaUXManager_GetUXState( AiaUXManager_t* uxManager )
{
    AiaAssert( uxManager );
    if( !uxManager )
    {
        AiaLogError( "Null uxManager" );
        return AIA_UX_IDLE;
    }
    AiaMutex( Lock )( &uxManager->mutex );

    AiaUXState_t currentUXState = uxManager->currentUXState;

    AiaMutex( Unlock )( &uxManager->mutex );
    return currentUXState;
}

void AiaUXManager_Destroy( AiaUXManager_t* uxManager )
{
    if( !uxManager )
    {
        AiaLogDebug( "Null uxManager" );
        return;
    }

#ifdef AIA_ENABLE_SPEAKER
    AiaMutex( Lock )( &uxManager->mutex );
    AiaListDouble( Link_t )* link = NULL;
    AiaListDouble( ForEach )( &uxManager->offsetActions, link )
    {
        AiaUXManagerOffsetActionSlot_t* slot =
            (AiaUXManagerOffsetActionSlot_t*)link;
        AiaSpeakerManager_CancelAction( uxManager->speakerManager, slot->id );
    }
    AiaMutex( Unlock )( &uxManager->mutex );

    AiaMutex( Lock )( &uxManager->mutex );
    AiaListDouble( RemoveAll )( &uxManager->offsetActions, AiaFree, 0 );
    AiaMutex( Unlock )( &uxManager->mutex );
#endif

    AiaMutex( Destroy )( &uxManager->mutex );
    AiaFree( uxManager );
}

#ifdef AIA_ENABLE_MICROPHONE
void AiaUXManager_OnMicrophoneStateChange( AiaMicrophoneState_t state,
                                           void* userData )
{
    AiaUXManager_t* uxManager = (AiaUXManager_t*)userData;
    AiaAssert( uxManager );
    if( !uxManager )
    {
        AiaLogError( "Null uxManager" );
        return;
    }

    AiaLogDebug( "Microphone state changed, state=%s",
                 AiaMicrophoneState_ToString( state ) );

    AiaMutex( Lock )( &uxManager->mutex );

    uxManager->currentMicrophoneState = state;
    AiaUXManager_AggregateStatesLocked( uxManager );

    AiaMutex( Unlock )( &uxManager->mutex );
}
#endif

void AiaUXManager_OnSetAttentionStateDirectiveReceived(
    void* manager, const void* payload, size_t size,
    AiaSequenceNumber_t sequenceNumber, size_t index )
{
    AiaUXManager_t* uxManager = (AiaUXManager_t*)manager;
    AiaAssert( uxManager );
    if( !uxManager )
    {
        AiaLogError( "Null uxManager" );
        return;
    }
    AiaAssert( payload );
    if( !payload )
    {
        AiaLogError( "Null payload, sequenceNumber=%" PRIu32 ", index=%zu",
                     sequenceNumber, index );
        return;
    }

    const char* state = NULL;
    size_t stateLen = 0;
    if( !AiaFindJsonValue( payload, size, AIA_SET_ATTENTION_STATE_STATE_KEY,
                           sizeof( AIA_SET_ATTENTION_STATE_STATE_KEY ) - 1,
                           &state, &stateLen ) )
    {
        AiaLogError( "No state found" );
        AiaJsonMessage_t* malformedMessageEvent =
            generateMalformedMessageExceptionEncounteredEvent(
                sequenceNumber, index, AIA_TOPIC_DIRECTIVE );
        if( !AiaRegulator_Write(
                uxManager->eventRegulator,
                AiaJsonMessage_ToMessage( malformedMessageEvent ) ) )
        {
            AiaLogError( "AiaRegulator_Write failed" );
            AiaJsonMessage_Destroy( malformedMessageEvent );
            return;
        }
        return;
    }

    AiaLogDebug( "AttentionState received, state= %.*s", stateLen, state );

    if( !AiaJsonUtils_UnquoteString( &state, &stateLen ) )
    {
        AiaLogError( "Malformed JSON" );
        AiaJsonMessage_t* malformedMessageEvent =
            generateMalformedMessageExceptionEncounteredEvent(
                sequenceNumber, index, AIA_TOPIC_DIRECTIVE );
        if( !AiaRegulator_Write(
                uxManager->eventRegulator,
                AiaJsonMessage_ToMessage( malformedMessageEvent ) ) )
        {
            AiaLogError( "AiaRegulator_Write failed" );
            AiaJsonMessage_Destroy( malformedMessageEvent );
            return;
        }
        return;
    }

    AiaServerAttentionState_t attentionState;
    if( !AttentionStateJsonToEnum( state, stateLen, &attentionState ) )
    {
        AiaLogError( "Unknown attentionState" );
        AiaJsonMessage_t* malformedMessageEvent =
            generateMalformedMessageExceptionEncounteredEvent(
                sequenceNumber, index, AIA_TOPIC_DIRECTIVE );
        if( !AiaRegulator_Write(
                uxManager->eventRegulator,
                AiaJsonMessage_ToMessage( malformedMessageEvent ) ) )
        {
            AiaLogError( "AiaRegulator_Write failed" );
            AiaJsonMessage_Destroy( malformedMessageEvent );
            return;
        }
        return;
    }

#ifdef AIA_ENABLE_SPEAKER
    const char* offsetStr = NULL;
    size_t offsetLen = 0;
    if( AiaFindJsonValue( payload, size, AIA_SET_ATTENTION_STATE_OFFSET_KEY,
                          sizeof( AIA_SET_ATTENTION_STATE_OFFSET_KEY ) - 1,
                          &offsetStr, &offsetLen ) )
    {
        AiaBinaryAudioStreamOffset_t offset = 0;
        if( !AiaExtractLongFromJsonValue( offsetStr, offsetLen, &offset ) )
        {
            AiaLogError( "Invalid offset, offset=%.*s", offsetLen, offsetStr );
            AiaJsonMessage_t* malformedMessageEvent =
                generateMalformedMessageExceptionEncounteredEvent(
                    sequenceNumber, index, AIA_TOPIC_DIRECTIVE );
            if( !AiaRegulator_Write(
                    uxManager->eventRegulator,
                    AiaJsonMessage_ToMessage( malformedMessageEvent ) ) )
            {
                AiaLogError( "AiaRegulator_Write failed" );
                AiaJsonMessage_Destroy( malformedMessageEvent );
                return;
            }
            return;
        }

        AiaLogDebug( "SetAttentionState offset=%" PRIu64, offset );

        AiaUXManager_HandleSetAttentionStateAtOffset( uxManager, attentionState,
                                                      offset );
        return;
    }
    else
    {
        AiaLogDebug( "No offset found in SetAttentionState" );
    }

#endif

    AiaMutex( Lock )( &uxManager->mutex );

    uxManager->currentAttentionState = attentionState;
    AiaUXManager_AggregateStatesLocked( uxManager );

    AiaMutex( Unlock )( &uxManager->mutex );
}

bool AttentionStateJsonToEnum( const char* state, size_t stateLen,
                               AiaServerAttentionState_t* attentionState )
{
    if( !strncmp( state, "IDLE", stateLen ) )
    {
        *attentionState = AIA_ATTENTION_STATE_IDLE;
        return true;
    }
    if( !strncmp( state, "THINKING", stateLen ) )
    {
        *attentionState = AIA_ATTENTION_STATE_THINKING;
        return true;
    }
#ifdef AIA_ENABLE_SPEAKER
    if( !strncmp( state, "SPEAKING", stateLen ) )
    {
        *attentionState = AIA_ATTENTION_STATE_SPEAKING;
        return true;
    }
#endif
#ifdef AIA_ENABLE_ALERTS
    if( !strncmp( state, "ALERTING", stateLen ) )
    {
        *attentionState = AIA_ATTENTION_STATE_ALERTING;
        return true;
    }
#endif
    if( !strncmp( state, "NOTIFICATION_AVAILABLE", stateLen ) )
    {
        *attentionState = AIA_ATTENTION_STATE_NOTIFICATION_AVAILABLE;
        return true;
    }
    if( !strncmp( state, "DO_NOT_DISTURB", stateLen ) )
    {
        *attentionState = AIA_ATTENTION_STATE_DO_NOT_DISTURB;
        return true;
    }
    AiaLogError( "Unknown state, state=%.*s", stateLen, state );
    return false;
}

void AiaUXManager_AggregateStatesLocked( AiaUXManager_t* uxManager )
{
    /* Listening takes highest priority. */
#ifdef AIA_ENABLE_MICROPHONE
    if( uxManager->currentMicrophoneState == AIA_MICROPHONE_STATE_OPEN )
    {
        uxManager->currentUXState = AIA_UX_LISTENING;
        uxManager->observer( uxManager->currentUXState,
                             uxManager->observerUserData );
        return;
    }
#endif

    switch( uxManager->currentAttentionState )
    {
        case AIA_ATTENTION_STATE_IDLE:
            uxManager->currentUXState = AIA_UX_IDLE;
            break;
        case AIA_ATTENTION_STATE_THINKING:
            uxManager->currentUXState = AIA_UX_THINKING;
            break;
#ifdef AIA_ENABLE_SPEAKER
        case AIA_ATTENTION_STATE_SPEAKING:
            uxManager->currentUXState = AIA_UX_SPEAKING;
            break;
#endif
#ifdef AIA_ENABLE_ALERTS
        case AIA_ATTENTION_STATE_ALERTING:
            uxManager->currentUXState = AIA_UX_ALERTING;
            break;
#endif
        case AIA_ATTENTION_STATE_NOTIFICATION_AVAILABLE:
            uxManager->currentUXState = AIA_UX_NOTIFICATION_AVAILABLE;
            break;
        case AIA_ATTENTION_STATE_DO_NOT_DISTURB:
            uxManager->currentUXState = AIA_UX_DO_NOT_DISTURB;
            break;
    }

    uxManager->observer( uxManager->currentUXState,
                         uxManager->observerUserData );
}

#ifdef AIA_ENABLE_SPEAKER

void AiaUXManager_HandleSetAttentionStateAtOffset(
    AiaUXManager_t* uxManager, AiaServerAttentionState_t attentionState,
    AiaBinaryAudioStreamOffset_t offset )
{
    AiaMutex( Lock )( &uxManager->mutex );

    AiaUXManagerOffsetActionSlot_t* actionSlot =
        AiaCalloc( 1, sizeof( AiaUXManagerOffsetActionSlot_t ) );
    if( !actionSlot )
    {
        AiaLogError( "AiaCalloc failed, bytes=%zu",
                     sizeof( AiaUXManagerOffsetActionSlot_t ) );
        AiaMutex( Unlock )( &uxManager->mutex );
        return;
    }
    AiaListDouble( Link_t ) defaultLink = AiaListDouble( LINK_INITIALIZER );
    actionSlot->link = defaultLink;
    actionSlot->uxManager = uxManager;
    actionSlot->attentionState = attentionState;
    actionSlot->id = AiaSpeakerManager_InvokeActionAtOffset(
        uxManager->speakerManager, offset, AiaUXManager_OnActionInvokedAtOffset,
        actionSlot );
    if( actionSlot->id == AIA_INVALID_ACTION_ID )
    {
        AiaJsonMessage_t* internalExceptionEvent =
            generateInternalErrorExceptionEncounteredEvent();
        if( !AiaRegulator_Write(
                uxManager->eventRegulator,
                AiaJsonMessage_ToMessage( internalExceptionEvent ) ) )
        {
            AiaLogError( "AiaRegulator_Write failed" );
            AiaJsonMessage_Destroy( internalExceptionEvent );
        }
        AiaFree( actionSlot );
        AiaMutex( Unlock )( &uxManager->mutex );
        return;
    }

    AiaListDouble( InsertTail )( &uxManager->offsetActions, &actionSlot->link );

    AiaMutex( Unlock )( &uxManager->mutex );
}

void AiaUXManager_OnActionInvokedAtOffset( bool actionValid, void* userData )
{
    AiaUXManagerOffsetActionSlot_t* actionSlot =
        (AiaUXManagerOffsetActionSlot_t*)userData;
    AiaAssert( actionSlot );
    if( !actionSlot )
    {
        AiaLogError( "Null actionSlot" );
        return;
    }

    AiaMutex( Lock )( &actionSlot->uxManager->mutex );

    if( actionValid )
    {
        actionSlot->uxManager->currentAttentionState =
            actionSlot->attentionState;
        AiaUXManager_AggregateStatesLocked( actionSlot->uxManager );
    }
    AiaListDouble( Remove )( &actionSlot->link );

    AiaMutex( Unlock )( &actionSlot->uxManager->mutex );

    AiaFree( actionSlot );
}
#endif

void AiaUXManager_UpdateServerAttentionState(
    AiaUXManager_t* uxManager, AiaServerAttentionState_t newAttentionState )
{
    AiaMutex( Lock )( &uxManager->mutex );

    uxManager->currentAttentionState = newAttentionState;
    AiaUXManager_AggregateStatesLocked( uxManager );

    AiaMutex( Unlock )( &uxManager->mutex );
}
