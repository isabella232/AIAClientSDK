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
 * @file aia_microphone_manager.c
 * @brief Implements functions for the AiaMicrophoneManager_t type.
 */

/* The config header is always included first. */
#include <aia_config.h>

#include <aiamicrophonemanager/aia_microphone_manager.h>
#include <aiamicrophonemanager/private/aia_microphone_manager.h>

#include <aiacore/aia_binary_constants.h>
#include <aiacore/aia_binary_message.h>
#include <aiacore/aia_events.h>
#include <aiacore/aia_exception_encountered_utils.h>
#include <aiacore/aia_json_constants.h>
#include <aiacore/aia_json_utils.h>
#include <aiacore/aia_topic.h>

#include AiaClock( HEADER )
#include AiaMutex( HEADER )
#include AiaTimer( HEADER )

#include <inttypes.h>
#include <stdio.h>

/** An internal struct used to hold the current microphone state and pending
 * actions. */
typedef struct AiaCurrentMicrophoneState
{
    /** A flag representing whether the microphone is currently open. */
    bool isMicrophoneOpen;

    /** A flag representing whether a pending OpenMicrophone directive must be
     * handled. */
    bool pendingOpenMicrophone;

    /** The timeout in milliseconds for the last OpenMicrophone directive to
     * handle. */
    AiaTimepointMs_t openMicrophoneExpirationTime;

    /** A token to be echoed back when handling an OpenMicophone directive. This
     * is a null-terminated C string stored in internally managed heap memory.
     */
    char* openMicrophoneInitiator;

    /** Last microphone interaction type. This is used to handle OpenMicrophone
     * directives to know whether to automatically begin streaming microphone
     * data or wait for the user to initiate an interaction again. */
    AiaMicrophoneInitiatorType_t lastMicrophoneInitiatorType;

    /** Last interaction's profile. This is echoed back as needed when handling
     * OpenMicrophone directives. */
    AiaMicrophoneProfile_t lastProfile;

    /** Indicates the last offset sent to the server so that contiguous offsets
     * may be sent on subsequent microphone packets. */
    AiaBinaryAudioStreamOffset_t lastOffsetSent;

} AiaCurrentMicrophoneState_t;

struct AiaMicrophoneManager
{
    /** Mutex used to guard against asynchronous calls in threaded
     * environments. */
    AiaMutex_t mutex;

    /** @name Variables synchronized by mutex. */
    /** @{ */

    /** An object representing the current state of the microphone. */
    AiaCurrentMicrophoneState_t currentMicrophoneState;

    /** Callback to notify of state changes. */
    const AiaMicrophoneStateObserver_t stateObserver;

    /** Context associated with @c stateObserver. */
    void* const stateObserverUserData;

    /** @} */

    /** Used to read microphone data. */
    AiaDataStreamReader_t* const microphoneBufferReader;

    /** Used to publish outbound events. */
    AiaRegulator_t* const eventRegulator;

    /** Used to publish outbound microphone binary messages. */
    AiaRegulator_t* const microphoneRegulator;

    /** Timer which publishes microphone chunks. */
    /* Note: This will run every 50ms and collect @c
     * AIA_MICROPHONE_CHUNK_SIZE_BYTES to publish to the @c regulator. This adds
     * the overhead of a second 50ms timer in addition to the @c regulator's
     * 50ms timer. There is not much benefit to aggregating quicker than this
     * since we'll still be bounded by the 50ms publish rate in the Aia API
     * specification. */
    AiaTimer_t microphonePublishTimer;

    /** Timer to handle @c OpenMicrophone directives. */
    AiaTimer_t openMicrophoneTimer;
};

/**
 * A helper method that builds an @c OpenMicrophone Event, sends it, and kicks
 * off the streaming of microphone data.
 *
 * @param The @c AiaMicrophoneManager_t to act on.
 * @param profile The ASR profile associated with the interaction.
 * @param startSample The sample index at which to begin streaming. For wake
 * word interactions, this must include 500 milliseconds of preroll.
 * @param initiator A C-string containing the initiator object to send in the @c
 * MicrophoneOpened event.
 * @return @c true If the event began successfully or @c false otherwise.
 * @note This method must only be called when @c mutex is locked.
 */
static bool AiaMicrophoneManager_OpenMicrophoneLocked(
    AiaMicrophoneManager_t* microphoneManager, AiaMicrophoneProfile_t profile,
    AiaDataStreamIndex_t startSample, const char* initiator );

/**
 * Called by a @c AiaMicrophoneManager_t's @c openMicrophoneTimer when an @c
 * OpenMicrophone directive is received and the last interaction type that
 * occurred was @c AIA_MICROPHONE_INITIATOR_TYPE_HOLD. This task will execute in
 * the @c OpenMicrophone directive's timeoutInMilliseconds from the time of
 * directive reception.
 *
 * @param userData Pointer to the @c AiaMicrophoneManager_t to act on.
 */
static void AiaMicrophoneManager_OpenMicrophoneTimedOutTask( void* userData );

/**
 * Called by a @c AiaMicrophoneManager_t's @c microphonePublishTimer to stream
 * microphone data to the Aia Service. This task runs at a @c
 * MICROPHONE_PUBLISH_RATE cadence and attempts to stream @c
 * AIA_MICROPHONE_CHUNK_SIZE_SAMPLES samples.
 *
 * @param userData Pointer to the @c AiaMicrophoneManager_t to act on.
 */
static void AiaMicrophoneManager_MicrophoneStreamingTask( void* userData );

/**
 * Streams microphone data to the Aia Service.
 *
 * @param microphoneManager The @c AiaMicrophoneManager_t to act on.
 * @note This method must only be called when @c mutex is locked.
 */
static void AiaMicrophoneManager_MicrophoneStreamingTaskLocked(
    AiaMicrophoneManager_t* microphoneManager );

/**
 * Helper function that generates a @c MicrophoneClosed event.
 *
 * @param offset The byte offset to publish in the event.
 * @return The generated @c AiaJsonMessage_t or @c NULL on failures.
 */
static AiaJsonMessage_t* generateMicrophoneClosedEvent(
    AiaBinaryAudioStreamOffset_t offset );

/**
 * Helper function that handles @c OpenMirophone directives. @c mutex must be
 * locked prior to calling this method.
 *
 * @param microphoneManager The @c AiaMicrophoneManager_t to act on.
 * @param payload Pointer to the unencrypted "payload" body value of the
 * directive. This must remain valid for the duration of this call.
 * @param size The size of the payload.
 * @param sequenceNumber The sequence number of the message.
 * @param index The index of this directive within the Aia message.
 */
static void AiaMicrophoneManager_OnOpenMicrophoneDirectiveReceivedLocked(
    AiaMicrophoneManager_t* microphoneManager, const void* payload, size_t size,
    AiaSequenceNumber_t sequenceNumber, size_t index );

AiaMicrophoneManager_t* AiaMicrophoneManager_Create(
    AiaRegulator_t* eventRegulator, AiaRegulator_t* microphoneRegulator,
    AiaDataStreamReader_t* microphoneBufferReader,
    AiaMicrophoneStateObserver_t stateObserver, void* stateObserverUserData )
{
    if( !eventRegulator )
    {
        AiaLogError( "Null eventRegulator" );
        return NULL;
    }

    if( !microphoneRegulator )
    {
        AiaLogError( "Null microphoneRegulator" );
        return NULL;
    }

    if( !microphoneBufferReader )
    {
        AiaLogError( "Null microphoneBufferReader" );
        return NULL;
    }

    if( AiaDataStreamReader_GetWordSize( microphoneBufferReader ) !=
        AIA_MICROPHONE_BUFFER_WORD_SIZE )
    {
        AiaLogError( "Invalid word size, wordSize=%zu, expected=%zu",
                     AiaDataStreamReader_GetWordSize( microphoneBufferReader ),
                     AIA_MICROPHONE_BUFFER_WORD_SIZE );
        return NULL;
    }

    AiaMicrophoneManager_t* microphoneManager =
        (AiaMicrophoneManager_t*)AiaCalloc( 1,
                                            sizeof( AiaMicrophoneManager_t ) );
    if( !microphoneManager )
    {
        AiaLogError( "AiaCalloc failed, bytes=%zu.",
                     sizeof( AiaMicrophoneManager_t ) );
        return NULL;
    }

    if( !AiaTimer( Create )( &microphoneManager->openMicrophoneTimer,
                             AiaMicrophoneManager_OpenMicrophoneTimedOutTask,
                             microphoneManager ) )
    {
        AiaLogError( "Failed to create OpenMicrophone timer" );
        AiaFree( microphoneManager );
        return NULL;
    }

    *(AiaDataStreamReader_t**)&microphoneManager->microphoneBufferReader =
        microphoneBufferReader;
    *(AiaRegulator_t**)&microphoneManager->eventRegulator = eventRegulator;
    *(AiaRegulator_t**)&microphoneManager->microphoneRegulator =
        microphoneRegulator;
    *(AiaMicrophoneStateObserver_t*)&microphoneManager->stateObserver =
        stateObserver;
    *(void**)&microphoneManager->stateObserverUserData = stateObserverUserData;
    if( microphoneManager->stateObserver )
    {
        microphoneManager->stateObserver(
            AIA_MICROPHONE_STATE_CLOSED,
            microphoneManager->stateObserverUserData );
    }

    if( !AiaMutex( Create )( &microphoneManager->mutex, false ) )
    {
        AiaLogError( "AiaMutex( Create ) failed." );
        AiaTimer( Destroy )( &microphoneManager->openMicrophoneTimer );
        AiaFree( microphoneManager );
        return NULL;
    }

    return microphoneManager;
}

void AiaMicrophonerManager_Destroy( AiaMicrophoneManager_t* microphoneManager )
{
    if( !microphoneManager )
    {
        AiaLogDebug( "Null microphoneManager." );
        return;
    }

    AiaMutex( Lock )( &microphoneManager->mutex );
    if( microphoneManager->currentMicrophoneState.isMicrophoneOpen )
    {
        AiaTimer( Destroy )( &microphoneManager->microphonePublishTimer );
    }

    AiaTimer( Destroy )( &microphoneManager->openMicrophoneTimer );

    if( microphoneManager->currentMicrophoneState.openMicrophoneInitiator )
    {
        AiaFree(
            microphoneManager->currentMicrophoneState.openMicrophoneInitiator );
        microphoneManager->currentMicrophoneState.openMicrophoneInitiator =
            NULL;
    }
    AiaMutex( Unlock )( &microphoneManager->mutex );

    AiaMutex( Destroy )( &microphoneManager->mutex );
    AiaFree( microphoneManager );
}

void AiaMicrophoneManager_OnOpenMicrophoneDirectiveReceived(
    void* manager, const void* payload, size_t size,
    AiaSequenceNumber_t sequenceNumber, size_t index )
{
    AiaMicrophoneManager_t* microphoneManager =
        (AiaMicrophoneManager_t*)manager;
    AiaAssert( microphoneManager );
    if( !microphoneManager )
    {
        AiaLogError( "Null microphoneManager" );
        return;
    }

    if( !payload )
    {
        AiaLogError( "Null payload, sequenceNumber=%" PRIu32 ", index=%zu",
                     sequenceNumber, index );
        return;
    }

    AiaMutex( Lock )( &microphoneManager->mutex );
    AiaMicrophoneManager_OnOpenMicrophoneDirectiveReceivedLocked(
        microphoneManager, payload, size, sequenceNumber, index );
    AiaMutex( Unlock )( &microphoneManager->mutex );
}

static void AiaMicrophoneManager_OnOpenMicrophoneDirectiveReceivedLocked(
    AiaMicrophoneManager_t* microphoneManager, const void* payload, size_t size,
    AiaSequenceNumber_t sequenceNumber, size_t index )
{
    if( microphoneManager->currentMicrophoneState.isMicrophoneOpen )
    {
        AiaLogError( "Microphone already open" );
        return;
    }

    const char* timeoutInMilliseconds;
    size_t timeoutInMillisecondsLen;
    if( !AiaFindJsonValue(
            payload, size, AIA_OPEN_MICROPHONE_TIMEOUT_IN_MILLISECONDS_KEY,
            sizeof( AIA_OPEN_MICROPHONE_TIMEOUT_IN_MILLISECONDS_KEY ) - 1,
            &timeoutInMilliseconds, &timeoutInMillisecondsLen ) )
    {
        AiaLogError( "No timeoutInMilliseconds found" );
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
                microphoneManager->eventRegulator,
                AiaJsonMessage_ToMessage( malformedMessageEvent ) ) )
        {
            AiaLogError( "AiaRegulator_Write failed" );
            AiaJsonMessage_Destroy( malformedMessageEvent );
            return;
        }
        return;
    }

    AiaJsonLongType timeoutInMillisecondsLong = 0;
    if( !AiaExtractLongFromJsonValue( timeoutInMilliseconds,
                                      timeoutInMillisecondsLen,
                                      &timeoutInMillisecondsLong ) )
    {
        AiaLogError(
            "Invalid timeoutInMilliseconds, timeoutInMilliseconds=%.*s",
            timeoutInMillisecondsLen, timeoutInMilliseconds );
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
                microphoneManager->eventRegulator,
                AiaJsonMessage_ToMessage( malformedMessageEvent ) ) )
        {
            AiaLogError( "AiaRegulator_Write failed" );
            AiaJsonMessage_Destroy( malformedMessageEvent );
            return;
        }
        return;
    }

    const char* initiator = NULL;
    size_t initiatorLen = 0;
    if( !AiaFindJsonValue( payload, size, AIA_OPEN_MICROPHONE_INITIATOR_KEY,
                           sizeof( AIA_OPEN_MICROPHONE_INITIATOR_KEY ) - 1,
                           &initiator, &initiatorLen ) )
    {
        initiator = NULL;
        initiatorLen = 0;
    }

    switch(
        microphoneManager->currentMicrophoneState.lastMicrophoneInitiatorType )
    {
        case AIA_MICROPHONE_INITIATOR_TYPE_HOLD:
            if( !AiaTimer( Arm )( &microphoneManager->openMicrophoneTimer,
                                  timeoutInMillisecondsLong, 0 ) )
            {
                AiaLogError( "Failed to arm OpenMicrophone timer" );
                AiaCriticalFailure();
                return;
            }

            microphoneManager->currentMicrophoneState.pendingOpenMicrophone =
                true;
            microphoneManager->currentMicrophoneState
                .openMicrophoneExpirationTime =
                AiaClock( GetTimeMs )() + timeoutInMillisecondsLong;
            if( initiator )
            {
                microphoneManager->currentMicrophoneState
                    .openMicrophoneInitiator = AiaCalloc( 1, initiatorLen + 1 );
                if( !microphoneManager->currentMicrophoneState
                         .openMicrophoneInitiator )
                {
                    AiaLogError( "AiaCalloc failed, bytes=%zu.",
                                 initiatorLen + 1 );
                    AiaCriticalFailure();
                    return;
                }
                memcpy( microphoneManager->currentMicrophoneState
                            .openMicrophoneInitiator,
                        initiator, initiatorLen );
                microphoneManager->currentMicrophoneState
                    .openMicrophoneInitiator[ initiatorLen ] = '\0';
            }

            break;
        case AIA_MICROPHONE_INITIATOR_TYPE_TAP:
        case AIA_MICROPHONE_INITIATOR_TYPE_WAKEWORD:
            /* Start immediately */
            if( !AiaDataStreamReader_Seek(
                    microphoneManager->microphoneBufferReader, 0,
                    AIA_DATA_STREAM_BUFFER_READER_REFERENCE_BEFORE_WRITER ) )
            {
                AiaLogError( "Failed to seek to before writer" );
                AiaCriticalFailure();
                return;
            }
            char initiatorCStr[ initiatorLen + 1 ];
            memcpy( initiatorCStr, initiator, initiatorLen );
            initiatorCStr[ initiatorLen ] = '\0';
            if( !AiaMicrophoneManager_OpenMicrophoneLocked(
                    microphoneManager,
                    microphoneManager->currentMicrophoneState.lastProfile,
                    AiaDataStreamReader_Tell(
                        microphoneManager->microphoneBufferReader,
                        AIA_DATA_STREAM_BUFFER_READER_REFERENCE_ABSOLUTE ),
                    initiator ? initiatorCStr : initiator ) )
            {
                AiaLogError(
                    "AiaMicrophoneManager_OpenMicrophoneLocked failed" );
                return;
            }
            break;
    }
}

void AiaMicrophoneManager_OnCloseMicrophoneDirectiveReceived(
    void* manager, const void* payload, size_t size,
    AiaSequenceNumber_t sequenceNumber, size_t index )
{
    AiaMicrophoneManager_t* microphoneManager =
        (AiaMicrophoneManager_t*)manager;
    AiaAssert( microphoneManager );
    if( !microphoneManager )
    {
        AiaLogError( "Null microphoneManager" );
        return;
    }

    (void)payload;
    (void)size;
    (void)sequenceNumber;
    (void)index;
    AiaMicrophoneManager_CloseMicrophone( microphoneManager );
}

void AiaMicrophoneManager_CloseMicrophone(
    AiaMicrophoneManager_t* microphoneManager )
{
    AiaAssert( microphoneManager );
    if( !microphoneManager )
    {
        AiaLogError( "Null microphoneManager" );
        return;
    }

    AiaMutex( Lock )( &microphoneManager->mutex );
    if( !microphoneManager->currentMicrophoneState.isMicrophoneOpen )
    {
        AiaLogWarn( "Microphone not open" );
    }
    else
    {
        AiaBinaryAudioStreamOffset_t currentOffset =
            microphoneManager->currentMicrophoneState.lastOffsetSent;
        AiaJsonMessage_t* microphoneClosedEvent =
            generateMicrophoneClosedEvent( currentOffset );
        if( !AiaRegulator_Write(
                microphoneManager->eventRegulator,
                AiaJsonMessage_ToMessage( microphoneClosedEvent ) ) )
        {
            AiaLogError( "AiaRegulator_Write failed" );
            AiaJsonMessage_Destroy( microphoneClosedEvent );
        }
        AiaTimer( Destroy )( &microphoneManager->microphonePublishTimer );
        microphoneManager->currentMicrophoneState.isMicrophoneOpen = false;
        if( microphoneManager->stateObserver )
        {
            microphoneManager->stateObserver(
                AIA_MICROPHONE_STATE_CLOSED,
                microphoneManager->stateObserverUserData );
        }
    }

    AiaMutex( Unlock )( &microphoneManager->mutex );
}

bool AiaMicrophoneManager_TapToTalkStart(
    AiaMicrophoneManager_t* microphoneManager, AiaDataStreamIndex_t index,
    AiaMicrophoneProfile_t profile )
{
    AiaAssert( microphoneManager );
    if( !microphoneManager )
    {
        AiaLogError( "Null microphoneManager" );
        return false;
    }

    AiaMutex( Lock )( &microphoneManager->mutex );

    static const char* TAP_TO_TALK_INITATOR_FORMAT =
        /* clang-format off */
    "{"
        "\""AIA_OPEN_MICROPHONE_INITIATOR_TYPE_KEY"\":\"%s\""
    "}";
    /* clang-format on */

    int numCharsRequired = snprintf( NULL, 0, TAP_TO_TALK_INITATOR_FORMAT,
                                     AiaMicrophoneInitiatorType_ToString(
                                         AIA_MICROPHONE_INITIATOR_TYPE_TAP ) );
    if( numCharsRequired < 0 )
    {
        AiaLogError( "snprintf failed" );
        return false;
    }

    char initiatorPayloadBuffer[ numCharsRequired + 1 ];
    if( snprintf( initiatorPayloadBuffer, numCharsRequired + 1,
                  TAP_TO_TALK_INITATOR_FORMAT,
                  AiaMicrophoneInitiatorType_ToString(
                      AIA_MICROPHONE_INITIATOR_TYPE_TAP ) ) < 0 )
    {
        AiaLogError( "snprintf failed" );
        return false;
    }

    if( !AiaMicrophoneManager_OpenMicrophoneLocked(
            microphoneManager, profile, index, initiatorPayloadBuffer ) )
    {
        AiaLogError( "Failed to open microphone" );
        AiaMutex( Unlock )( &microphoneManager->mutex );
        return false;
    }

    microphoneManager->currentMicrophoneState.lastProfile = profile;
    microphoneManager->currentMicrophoneState.lastMicrophoneInitiatorType =
        AIA_MICROPHONE_INITIATOR_TYPE_TAP;
    AiaMutex( Unlock )( &microphoneManager->mutex );
    return true;
}

bool AiaMicrophoneManager_HoldToTalkStart(
    AiaMicrophoneManager_t* microphoneManager, AiaDataStreamIndex_t index )
{
    AiaAssert( microphoneManager );
    if( !microphoneManager )
    {
        AiaLogError( "Null microphoneManager" );
        return false;
    }

    AiaMutex( Lock )( &microphoneManager->mutex );

    if( microphoneManager->currentMicrophoneState.pendingOpenMicrophone &&
        AiaClock( GetTimeMs )() < microphoneManager->currentMicrophoneState
                                      .openMicrophoneExpirationTime )
    {
        if( !AiaMicrophoneManager_OpenMicrophoneLocked(
                microphoneManager, AIA_MICROPHONE_PROFILE_CLOSE_TALK, index,
                microphoneManager->currentMicrophoneState
                    .openMicrophoneInitiator ) )
        {
            AiaLogError( "Failed to open microphone" );
            AiaMutex( Unlock )( &microphoneManager->mutex );
            return false;
        }
        microphoneManager->currentMicrophoneState.pendingOpenMicrophone = false;
        if( microphoneManager->currentMicrophoneState.openMicrophoneInitiator )
        {
            AiaFree( microphoneManager->currentMicrophoneState
                         .openMicrophoneInitiator );
            microphoneManager->currentMicrophoneState.openMicrophoneInitiator =
                NULL;
        }
    }
    else
    {
        static const char* HOLD_TO_TALK_INITATOR_FORMAT =
            /* clang-format off */
        "{"
            "\""AIA_OPEN_MICROPHONE_INITIATOR_TYPE_KEY"\":\"%s\""
        "}";
        /* clang-format on */

        int numCharsRequired =
            snprintf( NULL, 0, HOLD_TO_TALK_INITATOR_FORMAT,
                      AiaMicrophoneInitiatorType_ToString(
                          AIA_MICROPHONE_INITIATOR_TYPE_HOLD ) );
        if( numCharsRequired < 0 )
        {
            AiaLogError( "snprintf failed" );
            return false;
        }

        char initiatorPayloadBuffer[ numCharsRequired + 1 ];
        if( snprintf( initiatorPayloadBuffer, numCharsRequired + 1,
                      HOLD_TO_TALK_INITATOR_FORMAT,
                      AiaMicrophoneInitiatorType_ToString(
                          AIA_MICROPHONE_INITIATOR_TYPE_HOLD ) ) < 0 )
        {
            AiaLogError( "snprintf failed" );
            return false;
        }

        if( !AiaMicrophoneManager_OpenMicrophoneLocked(
                microphoneManager, AIA_MICROPHONE_PROFILE_CLOSE_TALK, index,
                initiatorPayloadBuffer ) )
        {
            AiaLogError( "Failed to open microphone" );
            AiaMutex( Unlock )( &microphoneManager->mutex );
            return false;
        }
    }

    microphoneManager->currentMicrophoneState.lastProfile =
        AIA_MICROPHONE_PROFILE_CLOSE_TALK;
    microphoneManager->currentMicrophoneState.lastMicrophoneInitiatorType =
        AIA_MICROPHONE_INITIATOR_TYPE_HOLD;
    AiaMutex( Unlock )( &microphoneManager->mutex );
    return true;
}

bool AiaMicrophoneManager_WakeWordStart(
    AiaMicrophoneManager_t* microphoneManager, AiaDataStreamIndex_t beginIndex,
    AiaDataStreamIndex_t endIndex, AiaMicrophoneProfile_t profile,
    const char* wakeWord )
{
    AiaAssert( microphoneManager );
    if( !microphoneManager )
    {
        AiaLogError( "Null microphoneManager" );
        return false;
    }

    if( !wakeWord )
    {
        AiaLogError( "Null wakeWord" );
        return false;
    }

    if( strcmp( wakeWord, AIA_ALEXA_WAKE_WORD ) != 0 )
    {
        AiaLogError( "Invalid wakeword, expected=%s, received=%s",
                     AIA_ALEXA_WAKE_WORD, wakeWord );
        return false;
    }

    static const char* WAKE_WORD_INITATOR_FORMAT =
        /* clang-format off */
    "{"
        "\""AIA_OPEN_MICROPHONE_INITIATOR_TYPE_KEY"\":\"%s\","
        "\"" AIA_OPEN_MICROPHONE_INITIATOR_PAYLOAD_KEY "\": {"
            "\"" AIA_MICROPHONE_OPENED_INITIATOR_PAYLOAD_WAKE_WORD_KEY "\": \"%s\","
            "\"" AIA_MICROPHONE_OPENED_INITIATOR_PAYLOAD_WAKE_WORD_INDICES_KEY "\": {" 
                "\"" AIA_MICROPHONE_OPENED_INITIATOR_PAYLOAD_WAKE_WORD_INDICES_BEGIN_OFFSET_KEY "\": %"PRIu64","
                "\"" AIA_MICROPHONE_OPENED_INITIATOR_PAYLOAD_WAKE_WORD_INDICES_END_OFFSET_KEY "\": %"PRIu64""
            "}"
        "}"

    "}";
    /* clang-format on */

    AiaMutex( Lock )( &microphoneManager->mutex );

    AiaBinaryAudioStreamOffset_t wwStreamBeginOffset =
        microphoneManager->currentMicrophoneState.lastOffsetSent +
        ( AIA_MICROPHONE_WAKE_WORD_PREROLL_IN_SAMPLES *
          AIA_MICROPHONE_BUFFER_WORD_SIZE );
    AiaBinaryAudioStreamOffset_t wwStreamEndOffset =
        wwStreamBeginOffset +
        ( ( endIndex - beginIndex ) * AIA_MICROPHONE_BUFFER_WORD_SIZE );
    int numCharsRequired =
        snprintf( NULL, 0, WAKE_WORD_INITATOR_FORMAT,
                  AiaMicrophoneInitiatorType_ToString(
                      AIA_MICROPHONE_INITIATOR_TYPE_WAKEWORD ),
                  wakeWord, wwStreamBeginOffset, wwStreamEndOffset );
    if( numCharsRequired < 0 )
    {
        AiaLogError( "snprintf failed" );
        AiaMutex( Unlock )( &microphoneManager->mutex );
        return false;
    }

    char initiatorPayloadBuffer[ numCharsRequired + 1 ];
    if( snprintf( initiatorPayloadBuffer, numCharsRequired + 1,
                  WAKE_WORD_INITATOR_FORMAT,
                  AiaMicrophoneInitiatorType_ToString(
                      AIA_MICROPHONE_INITIATOR_TYPE_WAKEWORD ),
                  wakeWord, wwStreamBeginOffset, wwStreamEndOffset ) < 0 )
    {
        AiaLogError( "snprintf failed" );
        AiaMutex( Unlock )( &microphoneManager->mutex );
        return false;
    }

    if( microphoneManager->currentMicrophoneState.isMicrophoneOpen )
    {
        AiaLogWarn( "Microphone already open" );
        AiaMutex( Unlock )( &microphoneManager->mutex );
        return false;
    }

    if( beginIndex < AIA_MICROPHONE_WAKE_WORD_PREROLL_IN_SAMPLES )
    {
        AiaLogError( "Not enough samples for preroll" );
        AiaMutex( Unlock )( &microphoneManager->mutex );
        return false;
    }
    AiaDataStreamIndex_t begin =
        beginIndex - AIA_MICROPHONE_WAKE_WORD_PREROLL_IN_SAMPLES;

    if( !AiaMicrophoneManager_OpenMicrophoneLocked(
            microphoneManager, profile, begin, initiatorPayloadBuffer ) )
    {
        AiaLogError( "Failed to open microphone" );
        AiaMutex( Unlock )( &microphoneManager->mutex );
        return false;
    }

    microphoneManager->currentMicrophoneState.lastProfile = profile;
    microphoneManager->currentMicrophoneState.lastMicrophoneInitiatorType =
        AIA_MICROPHONE_INITIATOR_TYPE_WAKEWORD;
    AiaMutex( Unlock )( &microphoneManager->mutex );
    return true;
}

static bool AiaMicrophoneManager_OpenMicrophoneLocked(
    AiaMicrophoneManager_t* microphoneManager, AiaMicrophoneProfile_t profile,
    AiaDataStreamIndex_t startSample, const char* initiator )
{
    if( microphoneManager->currentMicrophoneState.isMicrophoneOpen )
    {
        AiaLogWarn( "Microphone already open" );
        return false;
    }

    if( !AiaDataStreamReader_Seek(
            microphoneManager->microphoneBufferReader, startSample,
            AIA_DATA_STREAM_BUFFER_READER_REFERENCE_ABSOLUTE ) )
    {
        AiaLogError( "Failed to seek to index, index=%" PRIu64, startSample );
        return false;
    }

    if( !AiaTimer( Create )( &microphoneManager->microphonePublishTimer,
                             AiaMicrophoneManager_MicrophoneStreamingTask,
                             microphoneManager ) )
    {
        AiaLogError( "Failed to create microphone timer" );
        AiaCriticalFailure();
        return false;
    }

    if( !AiaTimer( Arm )( &microphoneManager->microphonePublishTimer, 0,
                          MICROPHONE_PUBLISH_RATE ) )
    {
        AiaLogError( "Failed to arm microphone timer" );
        AiaTimer( Destroy )( &microphoneManager->microphonePublishTimer );
        AiaCriticalFailure();
        return false;
    }

    microphoneManager->currentMicrophoneState.isMicrophoneOpen = true;

    if( microphoneManager->stateObserver )
    {
        microphoneManager->stateObserver(
            AIA_MICROPHONE_STATE_OPEN,
            microphoneManager->stateObserverUserData );
    }

    static const char* formatPayloadWithoutInitiator =
        /* clang-format off */
    "{"
        "\""AIA_MICROPHONE_OPENED_PROFILE_KEY"\":\"%s\","
        "\""AIA_MICROPHONE_OPENED_OFFSET_KEY"\":%"PRIu64""  
    "}";
    /* clang-format on */

    static const char* formatPayloadWithInitiator =
        /* clang-format off */
    "{"
        "\""AIA_MICROPHONE_OPENED_PROFILE_KEY"\":\"%s\","
        "\""AIA_MICROPHONE_OPENED_OFFSET_KEY"\":%"PRIu64"," 
        "\""AIA_OPEN_MICROPHONE_INITIATOR_KEY"\": %s"
    "}";
    /* clang-format on */

    const char* formatPayload;
    if( !initiator )
    {
        formatPayload = formatPayloadWithoutInitiator;
    }
    else
    {
        formatPayload = formatPayloadWithInitiator;
    }

    int numCharsRequired = snprintf(
        NULL, 0, formatPayload, AiaMicrophoneProfile_ToString( profile ),
        microphoneManager->currentMicrophoneState.lastOffsetSent, initiator );
    if( numCharsRequired < 0 )
    {
        AiaLogError( "snprintf failed" );
        return false;
    }

    char payloadBuffer[ numCharsRequired + 1 ];
    if( snprintf( payloadBuffer, numCharsRequired + 1, formatPayload,
                  AiaMicrophoneProfile_ToString( profile ),
                  microphoneManager->currentMicrophoneState.lastOffsetSent,
                  initiator ) < 0 )
    {
        AiaLogError( "snprintf failed" );
        return false;
    }

    AiaJsonMessage_t* microphoneOpenedEvent = AiaJsonMessage_Create(
        AIA_EVENTS_MICROPHONE_OPENED, NULL, payloadBuffer );
    if( !microphoneOpenedEvent )
    {
        AiaLogError( "Failed to create microphoneOpenedEvent" );
        return false;
    }

    if( !AiaRegulator_Write(
            microphoneManager->eventRegulator,
            AiaJsonMessage_ToMessage( microphoneOpenedEvent ) ) )
    {
        AiaLogError( "AiaRegulator_Write failed" );
        AiaJsonMessage_Destroy( microphoneOpenedEvent );
        return false;
    }
    return true;
}

static void AiaMicrophoneManager_OpenMicrophoneTimedOutTask( void* userData )
{
    AiaMicrophoneManager_t* microphoneManager =
        (AiaMicrophoneManager_t*)userData;
    AiaAssert( microphoneManager );
    if( !microphoneManager )
    {
        AiaLogError( "Null microphoneManager" );
        return;
    }

    AiaMutex( Lock )( &microphoneManager->mutex );

    if( !microphoneManager->currentMicrophoneState.pendingOpenMicrophone )
    {
        AiaMutex( Unlock )( &microphoneManager->mutex );
        return;
    }

    AiaLogInfo( "OpenMicrophone timed out" );
    AiaJsonMessage_t* openMicrohoneTimedOutMessage = AiaJsonMessage_Create(
        AIA_EVENTS_OPEN_MICROPHONE_TIMED_OUT, NULL, NULL );
    if( !openMicrohoneTimedOutMessage )
    {
        AiaLogError( "Failed to create openMicrohoneTimedOutMessage" );
        AiaMutex( Unlock )( &microphoneManager->mutex );
        return;
    }
    if( !AiaRegulator_Write(
            microphoneManager->eventRegulator,
            AiaJsonMessage_ToMessage( openMicrohoneTimedOutMessage ) ) )
    {
        AiaLogError( "AiaRegulator_Write failed" );
        AiaJsonMessage_Destroy( openMicrohoneTimedOutMessage );
    }

    microphoneManager->currentMicrophoneState.pendingOpenMicrophone = false;
    if( microphoneManager->currentMicrophoneState.openMicrophoneInitiator )
    {
        AiaFree(
            microphoneManager->currentMicrophoneState.openMicrophoneInitiator );
        microphoneManager->currentMicrophoneState.openMicrophoneInitiator =
            NULL;
    }

    AiaMutex( Unlock )( &microphoneManager->mutex );
}

static void AiaMicrophoneManager_MicrophoneStreamingTask( void* userData )
{
    AiaMicrophoneManager_t* microphoneManager =
        (AiaMicrophoneManager_t*)userData;
    AiaAssert( microphoneManager );
    if( !microphoneManager )
    {
        AiaLogError( "Null microphoneManager" );
        return;
    }

    AiaMutex( Lock )( &microphoneManager->mutex );
    AiaMicrophoneManager_MicrophoneStreamingTaskLocked( microphoneManager );
    AiaMutex( Unlock )( &microphoneManager->mutex );
}

static void AiaMicrophoneManager_MicrophoneStreamingTaskLocked(
    AiaMicrophoneManager_t* microphoneManager )
{
    size_t numBytesRequiredForDataAndOffset =
        ( AIA_MICROPHONE_CHUNK_SIZE_SAMPLES *
          AIA_MICROPHONE_BUFFER_WORD_SIZE ) +
        sizeof( AiaBinaryAudioStreamOffset_t );

    uint8_t* buf =
        AiaCalloc( numBytesRequiredForDataAndOffset, sizeof( uint8_t ) );
    if( !buf )
    {
        AiaLogError( "AiaCalloc failed, bytes=%zu.",
                     numBytesRequiredForDataAndOffset );
        return;
    }

    size_t bytePosition = 0;
    for( size_t i = 0;
         i < sizeof( microphoneManager->currentMicrophoneState.lastOffsetSent );
         ++i )
    {
        buf[ bytePosition ] =
            ( microphoneManager->currentMicrophoneState.lastOffsetSent >>
              ( i * 8 ) );
        ++bytePosition;
    }

    ssize_t amountRead = AiaDataStreamReader_Read(
        microphoneManager->microphoneBufferReader, buf + bytePosition,
        AIA_MICROPHONE_CHUNK_SIZE_SAMPLES );
    if( amountRead <= 0 )
    {
        AiaLogDebug( "AiaDataStreamReader_Read failed, status=%s",
                     AiaDataStreamReader_ErrorToString( amountRead ) );
        switch( amountRead )
        {
            case AIA_DATA_STREAM_BUFFER_READER_ERROR_CLOSED:
            case AIA_DATA_STREAM_BUFFER_READER_ERROR_INVALID:
                AiaLogError( "AiaDataStreamReader_Read failed, status=%s",
                             AiaDataStreamReader_ErrorToString( amountRead ) );
                AiaFree( buf );
                AiaCriticalFailure();
                return;
            case AIA_DATA_STREAM_BUFFER_READER_ERROR_OVERRUN:
                AiaFree( buf );
                AiaLogError(
                    "numWordsBehind=%zu",
                    AiaDataStreamReader_Tell(
                        microphoneManager->microphoneBufferReader,
                        AIA_DATA_STREAM_BUFFER_READER_REFERENCE_BEFORE_WRITER ) );
                if( !AiaDataStreamReader_Seek(
                        microphoneManager->microphoneBufferReader, 0,
                        AIA_DATA_STREAM_BUFFER_READER_REFERENCE_BEFORE_WRITER ) )
                {
                    AiaLogError( "Failed to seek to before writer" );
                }
                return;
            case AIA_DATA_STREAM_BUFFER_READER_ERROR_WOULDBLOCK:
                AiaFree( buf );
                return;
        }
    }
    else if( (size_t)amountRead < AIA_MICROPHONE_CHUNK_SIZE_SAMPLES )
    {
        AiaLogDebug(
            "Read less samples than expected, expected=%zu, amountRead=%zu",
            AIA_MICROPHONE_CHUNK_SIZE_SAMPLES, amountRead );
    }

    /* Cleanup of @c buf is left to @c AiaBinaryMessage_Destroy(), which is done
     * downstream of the Regulator . */
    AiaBinaryMessage_t* binaryMessage = AiaBinaryMessage_Create(
        sizeof( AiaBinaryAudioStreamOffset_t ) +
            ( amountRead * AIA_MICROPHONE_BUFFER_WORD_SIZE ),
        AIA_BINARY_STREAM_MICROPHONE_CONTENT_TYPE, 0, buf );
    if( !binaryMessage )
    {
        AiaLogError( "AiaBinaryMessage_Create failed" );
        AiaFree( buf );
        return;
    }
    if( !AiaRegulator_Write( microphoneManager->microphoneRegulator,
                             AiaBinaryMessage_ToMessage( binaryMessage ) ) )
    {
        AiaLogError( "AiaRegulator_Write failed" );
        AiaBinaryMessage_Destroy( binaryMessage );
        return;
    }
    microphoneManager->currentMicrophoneState.lastOffsetSent +=
        ( amountRead * AIA_MICROPHONE_BUFFER_WORD_SIZE );
}

static AiaJsonMessage_t* generateMicrophoneClosedEvent(
    AiaBinaryAudioStreamOffset_t offset )
{
    static const char* AIA_MICROPHONE_CLOSED_FORMAT =
        /* clang-format off */
    "{"
        "\""AIA_MICROPHONE_CLOSED_OFFSET_KEY"\":%"PRIu64
    "}";
    /* clang-format on */
    int numCharsRequired =
        snprintf( NULL, 0, AIA_MICROPHONE_CLOSED_FORMAT, offset );
    if( numCharsRequired < 0 )
    {
        AiaLogError( "snprintf failed" );
        return NULL;
    }
    char payloadBuffer[ numCharsRequired + 1 ];
    if( snprintf( payloadBuffer, numCharsRequired + 1,
                  AIA_MICROPHONE_CLOSED_FORMAT, offset ) < 0 )
    {
        AiaLogError( "snprintf failed" );
        return NULL;
    }
    AiaJsonMessage_t* jsonMessage = AiaJsonMessage_Create(
        AIA_EVENTS_MICROPHONE_CLOSED, NULL, payloadBuffer );
    return jsonMessage;
}
