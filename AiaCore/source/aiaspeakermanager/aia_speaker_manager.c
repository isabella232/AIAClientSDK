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
 * @file aia_speaker_manager.c
 * @brief Implements functions for the AiaSpeakerManager_t type.
 */

/* The config header is always included first. */
#include <aia_config.h>

#include <aiaspeakermanager/aia_speaker_manager.h>
#include <aiaspeakermanager/private/aia_speaker_manager.h>

#include <aiacore/aia_binary_constants.h>
#include <aiacore/aia_events.h>
#include <aiacore/aia_exception_encountered_utils.h>
#include <aiacore/aia_json_constants.h>
#include <aiacore/aia_json_message.h>
#include <aiacore/aia_json_utils.h>
#include <aiacore/aia_topic.h>
#include <aiacore/aia_volume_constants.h>
#include <aiacore/data_stream_buffer/aia_data_stream_buffer.h>

#include AiaListDouble( HEADER )
#include AiaMutex( HEADER )
#include AiaTimer( HEADER )

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

/** An internal type used to hold information relevant to a marker. */
typedef struct AiaSpeakerMarkerSlot
{
    /** The actual link in the list. */
    AiaListDouble( Link_t ) link;

    /** Offset associated with the marker. */
    AiaBinaryAudioStreamOffset_t offset;

    /** The actual marker. */
    AiaSpeakerBinaryMarker_t marker;
} AiaSpeakerMarkerSlot_t;

/** Internal type used to hold information about triggers to invoke when an
 * offset is reached. */
typedef struct AiaSpeakerOffsetActionSlot
{
    /** The actual link in the list. */
    AiaListDouble( Link_t ) link;

    /** Offset associated with the action. */
    AiaBinaryAudioStreamOffset_t offset;

    /** The action to invoke. */
    AiaActionAtSpeakerOffset_t action;

    /** User data associated with the action. */
    void* userData;

} AiaSpeakerOffsetActionSlot_t;

/* TODO: ADSER-1925 Make this an extension of @c AiaSpeakerOffsetActionSlot_t
 * rather than maintain separately. */
/** Used to hold information about action callbacks related to a volume change
 * at an offset. */
typedef struct AiaSpeakerManagerVolumeDataForAction
{
    /** The actual link in the list. */
    AiaListDouble( Link_t ) link;

    /** The speaker manager associated with this action. */
    AiaSpeakerManager_t* speakerManager;

    /** The volume associated with this action. */
    uint8_t volume;
} AiaSpeakerManagerVolumeDataForAction_t;

/** An internal struct used to hold the current speaker state and pending
 * actions. */
typedef struct AiaCurrentSpeakerState
{
    /** A flag representing whether the speaker is currently open. */
    bool isSpeakerOpen;

    /* TODO: ADSER-1526 This information should be organized in a list of
     * "actions" ordered by the action's corresponding offset. */
    /** A flag representing whether a pending OpenSpeaker must be handled. */
    bool pendingOpenSpeaker;

    /** The offset associated with the latest OpenSpeaker. */
    AiaBinaryAudioStreamOffset_t speakerOpenOffset;

    /** The current buffer state. */
    AiaSpeakerManagerBufferState_t currentBufferState;

    /** Used to indicate whether the speaker is ready for frames to be pushed -
     * default is @c true and is reset to this value whenever the speaker is
     * closed. This takes on an @c false value when @c AiaPlaySpeakerData_t()
     * fails and goes back to @c true when @c AiaSpeakerManager_OnSpeakerReady()
     * is called. */
    bool isSpeakerReadyForData;

    /** Used to buffer a frame when the speaker fails to accept a frame for
     * playback. */
    uint8_t* bufferedSpeakerFrame;

    /** Used to indicate that @c bufferedSpeakerFrame must be pushed to the
     * speaker before any other frames. */
    bool isBufferedSpeakerFramePending;

    /** The current volume. */
    uint8_t currentVolume;

    /** Flag to not send a VolumeChanged event on the initial volume change. */
    bool initialVolume;

#ifdef AIA_ENABLE_ALERTS
    /** Flag to decide if offline alert playback should be started or not. */
    bool shouldStartOfflineAlertPlayback;

    /** Used to point at the offline alert token for which an offline alert
     * tone will be played if @c shouldStartOfflineAlertPlayback is @c true. */
    AiaAlertSlot_t* alertToPlay;

    /** Used to store the offline alert volume level to use during offline alert
     * playback. */
    uint8_t offlineAlertVolume;
#endif
} AiaCurrentSpeakerState_t;

/**
 * Underlying struct that contains all the data required to present the @c
 * AiaSpeakerManager_t abstraction.
 */
struct AiaSpeakerManager
{
    /** Underlying memory of @c speakerBuffer. */
    void* speakerBufferMemory;

    /** The underlying speaker buffer used to buffer compressed audio data. This
     * is created during initialization and destroyed during cleanup so no
     * thread-safety is needed. Furthermore, all of this object's methods are
     * thread-safe. */
    AiaDataStreamBuffer_t* const speakerBuffer;

    /** A stream writer used to write audio frames to @c speakerBuffer. Methods
     * of this object are thread-safe. */
    AiaDataStreamWriter_t* const speakerBufferWriter;

    /** A stream reader used to pull frames out of the buffer to push to the
     * speaker for playback. Methods of this object are thread-safe. */
    AiaDataStreamReader_t* const speakerBufferReader;

    /** Threshold at which to send an OVERRUN_WARNING. This is only ever written
     * to during initialization and then subsequently read from. Thread-safety
     * is not needed. */
    const size_t overrunWarningThreshold;

    /** Threshold at which to send an UNDERRUN_WARNING. This is only ever
     * written to during initialization and then subsequently read from.
     * Thread-safety is not needed. */
    const size_t underrunWarningThreshold;

    /** Mutex used to guard against asynchronous calls in threaded
     * environments. */
    AiaMutex_t mutex;

    /** @name Variables synchronized by mutex. */
    /** @{ */

    /** The linked list representing accumulated markers. */
    AiaListDouble_t accumulatedMarkers;

    /** An object representing the current state of the speaker. */
    AiaCurrentSpeakerState_t currentSpeakerState;

    /** The last sequence number speaker message successfully processed. */
    AiaSequenceNumber_t lastSpeakerSequenceNumberProcessed;

    /* TODO: ADSER-1529 Implement support for VBR */
    /** The speaker frame size. This is determined using the first speaker topic
     * content message received. Note that this assumes a static constant bit
     * rate. VBR is not currently supported. */
    size_t frameSize;

    /** Callback implemented by the speaker to receive speaker frames for
     * playback. */
    const AiaPlaySpeakerData_t playSpeakerDataCb;

    /** User data to pass to @c playSpeakerDataCb. */
    void* const playSpeakerDataCbUserData;

    /** Sequence number of the last message that caused an overrun. Subsequent
     * messages will not be handled until this sequence number is re-sent. A
     * value of zero indicates that no waiting is required. */
    AiaSequenceNumber_t overrunSpeakerSequenceNumber;

    /* TODO: ADSER-1585 Investigate ways of making this call thread-safe
     * independent of an external component's implementation details. */
    /** Used to reset the next expected sequence number on overruns. Methods of
     * the sequencer are not thread-safe. However, the only call made to the
     * sequencer ( @cAiaSequencer_ResetSequenceNumber() ) is made on the same
     * thread as the thread that messages flow from the @c AiaSequencer to the
     * @c AiaSpeakerManager. */
    AiaSequencer_t* const sequencer;

    /** Callback implemented by the speaker to change volume. */
    const AiaSetVolume_t setVolumeCb;

    /** User data to pass to @c setVolumeCb. */
    void* const setVolumeCbUserData;

    /** Callback to use while synthesizing the offline alert tone. */
    const AiaOfflineAlertPlayback_t playOfflineAlertCb;

    /** User data to pass to @c playOfflineAlertCb. */
    void* const playOfflineAlertCbUserData;

    /** Callback to use while stopping the playback of offline alert tone. */
    const AiaOfflineAlertStop_t stopOfflineAlertCb;

    /** User data to pass to @c stopOfflineAlertCb. */
    void* const stopOfflineAlertCbUserData;

    /** Used to notify observers about speaker buffer state changes */
    const AiaSpeakerManagerBufferStateObserver_t notifyObserversCb;

    /** User data associated with @c notifyObserversCb */
    void* const notifyObserversCbUserData;

    /** Sorted collection of actions to invoke when an offset is reached. */
    AiaListDouble_t offsetActions;

    /** Collection of volume actions. */
    AiaListDouble_t volumeActions;

    /** @} */

    /** Used to schedule jobs for checking the speaker buffer and
     * pushing frames for playback to the speaker as needed. */
    AiaTimer_t speakerWorker;

    /** Used to publish outbound messages. Methods of this object are
     * thread-safe. */
    AiaRegulator_t* const regulator;
};

/** Locked variant of @c AiaSpeakerManager_ChangeVolume. See @c
 * AiaSpeakerManager_ChangeVolume documentation. */
static bool AiaSpeakerManager_ChangeVolumeLocked(
    AiaSpeakerManager_t* speakerManager, uint8_t newVolume );

/**
 * This is a recurring function that occurs at AIA_SPEAKER_FRAME_PUSH_CADENCE_MS
 * that checks the speaker state and pushes frames to the application for
 * playback as needed.
 *
 * @param context User data associated with this routine.
 */
static void AiaSpeakerManager_PlaySpeakerDataRoutine( void* context );

/**
 * An internal helper function used to read and push speaker frames to the
 * speaker.
 *
 * @param speakerManager The @c AiaSpeakerManager_t to act on.
 * @note This method must be called while @c speakerManager->mutex is locked.
 */
static void AiaSpeakerManager_PlaySpeakerDataRoutineLocked(
    AiaSpeakerManager_t* speakerManager );

/**
 * An internal helper function used to check if the speaker is open or
 * streaming.
 *
 * @param speakerManager The @c AiaSpeakerManager_t to act on.
 *
 * @return @c true if the speaker is currently open or streaming, or @c false
 * otherwise.
 */
static bool AiaSpeakerManager_CanSpeakerStreamLocked(
    AiaSpeakerManager_t* speakerManager );

#ifdef AIA_ENABLE_ALERTS
/**
 * An internal helper function used to syntesize and play offline alert tone at
 * the given volume level for an offline alert.
 *
 * @param speakerManager The @c AiaSpeakerManager_t to act on.
 * @param offlineAlert The offline alert to synthesize an alert tone for.
 * @param offlineAlertVolume The volume level at which to play the offline alert
 */
static void AiaSpeakerManager_PlayOfflineAlertLocked(
    AiaSpeakerManager_t* speakerManager, const AiaAlertSlot_t* offlineAlert,
    uint8_t offlineAlertVolume );

/**
 * An internal helper function used to stop the offline alert playback.
 *
 * @param spakermanager The @c AiaSpeakerManager_t to act on.
 */
static void AiaSpeakerManager_StopOfflineAlertLocked(
    AiaSpeakerManager_t* speakerManager );
#endif

/**
 * An internal helper function used to parse content type messages on the
 * speaker topic. This must return whether parsing of the entire binary message
 * should continue or not.
 *
 * @param speakerManager The @c AiaSpeakerManager_t to act on.
 * @param data Pointer to the data section of the Aia Binary Stream.
 * @param length The length of @c data.
 * @param count The number of binary stream data chunks in this message.
 * @param sequenceNumber The sequence number associated with this message.
 * @return @c true if the message was handled or @c false if a failure occurred.
 * An @c MALFORMED_MESSAGE @c ExceptionEncountered event should be sent in this
 * case.
 */
static bool handleSpeakerTopicContentTypeEntryLocked(
    AiaSpeakerManager_t* speakerManager, const uint8_t* data, size_t length,
    AiaBinaryMessageCount_t count, AiaSequenceNumber_t sequenceNumber );

/**
 * An internal helper function used to parse marker type messages on the speaker
 * topic. This must return whether parsing of the entire binary message should
 * continue or not.
 * @param data Pointer to the data section of the Aia Binary Stream.
 * @param length The length of @c data.
 * @param count The number of binary stream data chunks in this message.
 * @return @c true if the message was handled or @c false if a failure occurred.
 * An @c MALFORMED_MESSAGE @c ExceptionEncountered event should be sent in this
 * case.
 */
static bool handleSpeakerTopicMarkerTypeEntryLocked(
    AiaSpeakerManager_t* speakerManager, const uint8_t* data, size_t length,
    AiaBinaryMessageCount_t count );

/**
 * Internal helper method to handle speaker topic messages.
 *
 * @param speakerManager The @c AiaSpeakerManager_t to act on.
 * @param message Pointer to the unencrypted message body (without the common
 * header) i.e. the unencrypted Binary Stream. This must remain valid for the
 * duration of this call.
 * @param size The size of the message.
 * @param sequenceNumber The sequence number of the message.
 * @note This method must be called while @c speakerManager->mutex is locked.
 */
static void AiaSpeakerManager_OnSpeakerTopicMessageReceivedLocked(
    AiaSpeakerManager_t* speakerManager, const uint8_t* message, size_t size,
    AiaSequenceNumber_t sequenceNumber );

/**
 * Internal helper method to invalidate any actions due to local playback
 * stoppage.
 *
 * @param speakerManager The @c AiaSpeakerManager_t to act on.
 */
static void AiaSpeakerManager_InvalidateActionsLocked(
    AiaSpeakerManager_t* speakerManager );

/**
 * Internal helper method to update the speaker buffer state and notify the
 * observers.
 *
 * @param speakerManager The @c AiaSpeakerManager_t to act on.
 * @param newBufferState The new speaker buffer state to update the speaker
 * manager with.
 * @note This method must be called while @c speakerManager->mutex is locked.
 */
static void AiaSpeakerManager_SetBufferStateLocked(
    AiaSpeakerManager_t* speakerManager,
    AiaSpeakerManagerBufferState_t newBufferState );

/**
 * Utility function used to convert a given @AiaSpeakerManagerBufferState_t to a
 * string for logging and message sending purposes.
 *
 * @param error The state to convert.
 * @return The state as a string or @c NULL on failure.
 */
const char* AiaSpeakerManagerBufferState_ToString(
    AiaSpeakerManagerBufferState_t state )
{
    switch( state )
    {
        case AIA_UNDERRUN_STATE:
            return "UNDERRUN";
        case AIA_UNDERRUN_WARNING_STATE:
            return "UNDERRUN_WARNING";
        case AIA_NONE_STATE:
            return "NONE";
        case AIA_OVERRUN_WARNING_STATE:
            return "OVERRUN_WARNING";
        case AIA_OVERRUN_STATE:
            return "OVERRUN";
    }
    AiaLogError( "Unknown speaker manager buffer state %d.", state );
    AiaAssert( false );
    return "";
}

/**
 * Generates a @c BufferStateChanged event for publishing to the @c Regulator.
 *
 * @param sequenceNumber The sequence number of the message.
 * @param state The state of the buffer.
 * @return The generated @c AiaJsonMessage_t or @c NULL on failures.
 */
static AiaJsonMessage_t* generateBufferStateChangedEvent(
    AiaSequenceNumber_t sequenceNumber, AiaSpeakerManagerBufferState_t state )
{
    static const char* formatPayload =
        /* clang-format off */
        "{"
            "\""AIA_BUFFER_STATE_CHANGED_MESSAGE_KEY"\":{"
                "\""AIA_BUFFER_STATE_CHANGED_MESSAGE_TOPIC_KEY"\":\""AIA_TOPIC_SPEAKER_STRING"\","
                "\""AIA_BUFFER_STATE_CHANGED_MESSAGE_SEQUENCE_NUMBER_KEY"\": %"PRIu32
            "},"
            "\""AIA_BUFFER_STATE_CHANGED_STATE_KEY"\":\"%s\""
        "}";
    /* clang-format on */

    const char* bufferState = AiaSpeakerManagerBufferState_ToString( state );
    if( !bufferState )
    {
        AiaLogError( "Invalid bufferState, state=%d", state );
        return NULL;
    }
    int numCharsRequired =
        snprintf( NULL, 0, formatPayload, sequenceNumber, bufferState );
    if( numCharsRequired < 0 )
    {
        AiaLogError( "snprintf failed, ret=%d", numCharsRequired );
        return NULL;
    }
    char fullPayloadBuffer[ numCharsRequired + 1 ];
    if( snprintf( fullPayloadBuffer, numCharsRequired + 1, formatPayload,
                  sequenceNumber, bufferState ) < 0 )
    {
        AiaLogError( "snprintf failed" );
        return NULL;
    }

    AiaJsonMessage_t* jsonMessage = AiaJsonMessage_Create(
        AIA_EVENTS_BUFFER_STATE_CHANGED, NULL, fullPayloadBuffer );
    return jsonMessage;
}

/**
 * Generates a @c SpeakerOpened event for publishing to the @c Regulator.
 *
 * @param offset The offset of the message.
 * @return The generated @c AiaJsonMessage_t or @c NULL on failures.
 */
static AiaJsonMessage_t* generateSpeakerOpenedEvent(
    AiaBinaryAudioStreamOffset_t offset )
{
    static const char* formatPayload =
        /* clang-format off */
        "{"
            "\""AIA_SPEAKER_OPENED_OFFSET_KEY"\":%"PRIu64
        "}";
    /* clang-format on */
    int numCharsRequired = snprintf( NULL, 0, formatPayload, offset );
    if( numCharsRequired < 0 )
    {
        AiaLogError( "snprintf failed, ret=%d", numCharsRequired );
        return NULL;
    }
    char fullPayloadBuffer[ numCharsRequired + 1 ];
    if( snprintf( fullPayloadBuffer, numCharsRequired + 1, formatPayload,
                  offset ) < 0 )
    {
        AiaLogError( "snprintf failed" );
        return NULL;
    }

    AiaJsonMessage_t* jsonMessage = AiaJsonMessage_Create(
        AIA_EVENTS_SPEAKER_OPENED, NULL, fullPayloadBuffer );
    return jsonMessage;
}

/**
 * Generates a @c SpeakerClosed event for publishing to the @c Regulator.
 *
 * @param offset The offset of the message.
 * @return The generated @c AiaJsonMessage_t or @c NULL on failures.
 */
static AiaJsonMessage_t* generateSpeakerClosedEvent(
    AiaBinaryAudioStreamOffset_t offset )
{
    static const char* formatPayload =
        /* clang-format off */
        "{"
            "\""AIA_SPEAKER_CLOSED_OFFSET_KEY"\":%"PRIu64
        "}";
    /* clang-format on */
    int numCharsRequired = snprintf( NULL, 0, formatPayload, offset );
    if( numCharsRequired < 0 )
    {
        AiaLogError( "snprintf failed, ret=%d", numCharsRequired );
        return NULL;
    }
    char fullPayloadBuffer[ numCharsRequired + 1 ];
    if( snprintf( fullPayloadBuffer, numCharsRequired + 1, formatPayload,
                  offset ) < 0 )
    {
        AiaLogError( "snprintf failed" );
        return NULL;
    }

    AiaJsonMessage_t* jsonMessage = AiaJsonMessage_Create(
        AIA_EVENTS_SPEAKER_CLOSED, NULL, fullPayloadBuffer );
    return jsonMessage;
}

/**
 * Generates a @c SpeakerMarkerEncountered event for publishing to the @c
 * Regulator.
 *
 * @param marker The marker of the message.
 * @return The generated @c AiaJsonMessage_t or @c NULL on failures.
 */
static AiaJsonMessage_t* generateSpeakerMarkerEncounteredEvent(
    AiaSpeakerBinaryMarker_t marker )
{
    static const char* formatPayload =
        /* clang-format off */
        "{"
            "\""AIA_SPEAKER_MARKER_ENCOUNTERED_MARKER_KEY"\":%"PRIu32
        "}";
    /* clang-format on */
    int numCharsRequired = snprintf( NULL, 0, formatPayload, marker );
    if( numCharsRequired < 0 )
    {
        AiaLogError( "snprintf failed, ret=%d", numCharsRequired );
        return NULL;
    }
    char fullPayloadBuffer[ numCharsRequired + 1 ];
    if( snprintf( fullPayloadBuffer, numCharsRequired + 1, formatPayload,
                  marker ) < 0 )
    {
        AiaLogError( "snprintf failed" );
        return NULL;
    }

    AiaJsonMessage_t* jsonMessage = AiaJsonMessage_Create(
        AIA_EVENTS_SPEAKER_MARKER_ENCOUNTERED, NULL, fullPayloadBuffer );
    return jsonMessage;
}

/**
 * Generates a @c VolumeChanged event without an offset for publishing to the @c
 * Regulator.
 *
 * @param volume The new volume, between @c AIA_MIN_VOLUME and @c
 * AIA_MAX_VOLUME, inclusive.
 * @return The generated @c AiaJsonMessage_t or @c NULL on failures.
 */
static AiaJsonMessage_t* generateVolumeChangedEventWithoutOffset(
    AiaJsonLongType volume )
{
    static const char* formatPayload =
        /* clang-format off */
        "{"
            "\""AIA_VOLUME_CHANGED_VOLUME_KEY"\":%"PRIu64
        "}";
    /* clang-format on */
    int numCharsRequired = snprintf( NULL, 0, formatPayload, volume );
    if( numCharsRequired < 0 )
    {
        AiaLogError( "snprintf failed, ret=%d", numCharsRequired );
        return NULL;
    }
    char fullPayloadBuffer[ numCharsRequired + 1 ];
    if( snprintf( fullPayloadBuffer, numCharsRequired + 1, formatPayload,
                  volume ) < 0 )
    {
        AiaLogError( "snprintf failed" );
        return NULL;
    }

    AiaJsonMessage_t* jsonMessage = AiaJsonMessage_Create(
        AIA_EVENTS_VOLUME_CHANGED, NULL, fullPayloadBuffer );
    return jsonMessage;
}

/* TODO: ADSER-1742 Consolidate the above and below function using variadic
 * arguments. */

/**
 * Generates a @c VolumeChanged event with an offset for publishing to the @c
 * Regulator.
 *
 * @param volume The new volume, between @c AIA_MIN_VOLUME and @c
 * AIA_MAX_VOLUME, inclusive.
 * @param offset The byte offset in the speaker topic's audio stream at which
 * the volume was changed.
 * @return The generated @c AiaJsonMessage_t or @c NULL on failures.
 */
static AiaJsonMessage_t* generateVolumeChangedEventWithOffset(
    AiaJsonLongType volume, AiaBinaryAudioStreamOffset_t offset )
{
    static const char* formatPayload =
        /* clang-format off */
        "{"
            "\""AIA_VOLUME_CHANGED_VOLUME_KEY"\":%"PRIu64","
            "\""AIA_VOLUME_CHANGED_OFFSET_KEY"\":%"PRIu64
        "}";
    /* clang-format on */
    int numCharsRequired = snprintf( NULL, 0, formatPayload, volume, offset );
    if( numCharsRequired < 0 )
    {
        AiaLogError( "snprintf failed, ret=%d", numCharsRequired );
        return NULL;
    }
    char fullPayloadBuffer[ numCharsRequired + 1 ];
    if( snprintf( fullPayloadBuffer, numCharsRequired + 1, formatPayload,
                  volume, offset ) < 0 )
    {
        AiaLogError( "snprintf failed" );
        return NULL;
    }

    AiaJsonMessage_t* jsonMessage = AiaJsonMessage_Create(
        AIA_EVENTS_VOLUME_CHANGED, NULL, fullPayloadBuffer );
    return jsonMessage;
}

/**
 * Closes the speaker immediately at the current offset.  This may be used by an
 * an action corresponding to a @c CloseSpeaker directive when its offset is
 * reached, or during a barge-in when we need to close immediately.
 *
 * @param manager The @c AiaSpeakerManager_t to act on.
 */
static void AiaSpeakerManager_CloseSpeakerLocked(
    AiaSpeakerManager_t* speakerManager )
{
    if( !speakerManager->currentSpeakerState.isSpeakerOpen )
    {
        AiaLogDebug( "Speaker not open." );
        return;
    }

    speakerManager->currentSpeakerState.isSpeakerOpen = false;
    speakerManager->currentSpeakerState.isSpeakerReadyForData = true;
    speakerManager->currentSpeakerState.isBufferedSpeakerFramePending = false;
    speakerManager->currentSpeakerState.currentBufferState = AIA_NONE_STATE;
    AiaDataStreamWriter_SetPolicy( speakerManager->speakerBufferWriter,
                                   AIA_DATA_STREAM_BUFFER_WRITER_NONBLOCKABLE );
    AiaBinaryAudioStreamOffset_t currentOffset = AiaDataStreamReader_Tell(
        speakerManager->speakerBufferReader,
        AIA_DATA_STREAM_BUFFER_READER_REFERENCE_ABSOLUTE );
    AiaJsonMessage_t* speakerClosedEvent =
        generateSpeakerClosedEvent( currentOffset );
    if( !AiaRegulator_Write( speakerManager->regulator,
                             AiaJsonMessage_ToMessage( speakerClosedEvent ) ) )
    {
        AiaLogError( "AiaRegulator_Write failed" );
        AiaJsonMessage_Destroy( speakerClosedEvent );
    }
    AiaLogDebug( "Playback stopped at offset=%" PRIu64, currentOffset );
}

/**
 * Helper function used as the @c AiaActionAtSpeakerOffset_t callback to pass in
 * when closing the speaker at a future offset.
 *
 * @param actionValid @c true if the speaker was closed or @c false if the
 * future offset was invalidated.
 * @param manager The @c AiaSpeakerManager_t to act on.
 */
static void AiaSpeakerManager_CloseSpeakerActionLocked( bool actionValid,
                                                        void* manager )
{
    if( !actionValid )
    {
        return;
    }
    AiaSpeakerManager_CloseSpeakerLocked( manager );
}

/**
 * Helper function used as the @c AiaActionAtSpeakerOffset_t callback to pass in
 * when changing volume at a future offset.
 *
 * @param actionValid @c true if the volume was changed or @c false if the
 * future offset was invalidated.
 * @param volumeSlot The @c AiaSpeakerManagerVolumeDataForAction_t to act on.
 */
static void AiaSpeakerManager_SetVolumeActionLocked( bool actionValid,
                                                     void* volumeSlot )
{
    AiaSpeakerManagerVolumeDataForAction_t* slot =
        (AiaSpeakerManagerVolumeDataForAction_t*)volumeSlot;
    AiaAssert( slot );
    if( !slot )
    {
        AiaLogError( "Null volumeSlot" );
        return;
    }
    if( actionValid )
    {
        AiaSpeakerManager_ChangeVolumeLocked( slot->speakerManager,
                                              slot->volume );
    }

    AiaListDouble( Remove )( &slot->link );
    AiaFree( slot );
}

/**
 * Helper function used to update internal buffer state based on the amount of
 * space in the audio buffer upon successful reads or writes out of the
 * underlying audio buffer. Note that overruns and underruns should be handled
 * separately as a part of the failure case when successful reads/writes are not
 * able to be performed.
 *
 * @param speakerManager The @c AiaSpeakerManager_t to act on.
 * @note Must be called with @c mutex locked.
 */
static void updateBufferStateLocked( AiaSpeakerManager_t* speakerManager )
{
    size_t amountOfDataInBuffer =
        ( AiaDataStreamWriter_Tell( speakerManager->speakerBufferWriter ) -
          AiaDataStreamReader_Tell(
              speakerManager->speakerBufferReader,
              AIA_DATA_STREAM_BUFFER_READER_REFERENCE_ABSOLUTE ) );
    if( amountOfDataInBuffer < speakerManager->underrunWarningThreshold )
    {
        AiaSpeakerManager_SetBufferStateLocked( speakerManager,
                                                AIA_UNDERRUN_WARNING_STATE );
    }
    else if( amountOfDataInBuffer < speakerManager->overrunWarningThreshold )
    {
        AiaSpeakerManager_SetBufferStateLocked( speakerManager,
                                                AIA_NONE_STATE );
    }
    else if( amountOfDataInBuffer <=
             AiaDataStreamBuffer_GetDataSize( speakerManager->speakerBuffer ) )
    {
        AiaSpeakerManager_SetBufferStateLocked( speakerManager,
                                                AIA_OVERRUN_WARNING_STATE );
    }
}

static void AiaSpeakerManager_PlaySpeakerDataRoutineLocked(
    AiaSpeakerManager_t* speakerManager )
{
    AiaDataStreamIndex_t currentOffset = AiaDataStreamReader_Tell(
        speakerManager->speakerBufferReader,
        AIA_DATA_STREAM_BUFFER_READER_REFERENCE_ABSOLUTE );

    AiaListDouble( Link_t )* actionLink = NULL;
    while(
        ( actionLink =
              AiaListDouble( PeekHead )( &speakerManager->offsetActions ) ) &&
        ( (AiaSpeakerOffsetActionSlot_t*)actionLink )->offset <= currentOffset )
    {
        AiaSpeakerOffsetActionSlot_t* actionInfo =
            ( (AiaSpeakerOffsetActionSlot_t*)actionLink );
        AiaLogInfo( "Action reached, offset=%" PRIu64, actionInfo->offset );
        actionInfo->action( true, actionInfo->userData );
        AiaListDouble( Remove )( actionLink );
        AiaFree( actionInfo );
    }
    actionLink = NULL;

    if( !speakerManager->currentSpeakerState.isSpeakerReadyForData )
    {
        return;
    }
    if( !speakerManager->currentSpeakerState.isSpeakerOpen &&
        !speakerManager->currentSpeakerState.pendingOpenSpeaker )
    {
        /* No-op */
        return;
    }
    else if( !speakerManager->currentSpeakerState.isSpeakerOpen &&
             speakerManager->currentSpeakerState.pendingOpenSpeaker )
    {
        AiaBinaryAudioStreamOffset_t currentWritePosition =
            AiaDataStreamWriter_Tell( speakerManager->speakerBufferWriter );
        if( currentWritePosition >
                speakerManager->currentSpeakerState.speakerOpenOffset &&
            currentWritePosition -
                    speakerManager->currentSpeakerState.speakerOpenOffset >
                AiaDataStreamBuffer_GetDataSize(
                    speakerManager->speakerBuffer ) )
        {
            /* "If the offset given in this directive has already been dropped
             * from the audio buffer, this is considered a fatal case and the
             * device should close the AIS connection." */
            AiaLogError(
                "Seeking to overrun offset, offset=%" PRIu64,
                speakerManager->currentSpeakerState.speakerOpenOffset );
            /* TODO: ADSER-1532 Close the AIS connection */
            return;
        }
        if( !AiaDataStreamReader_Seek(
                speakerManager->speakerBufferReader,
                speakerManager->currentSpeakerState.speakerOpenOffset,
                AIA_DATA_STREAM_BUFFER_READER_REFERENCE_ABSOLUTE ) )
        {
            /* This shouldn't happen. Hopefully this error will recover in the
             * next retry. Else, the service should error out after not
             * receiving the SpeakerOpened acknowledgement. */
            AiaLogError(
                "Failed to seek to offset, offset=%" PRIu64,
                speakerManager->currentSpeakerState.speakerOpenOffset );
            return;
        }

        currentOffset = AiaDataStreamReader_Tell(
            speakerManager->speakerBufferReader,
            AIA_DATA_STREAM_BUFFER_READER_REFERENCE_ABSOLUTE );
        AiaDataStreamWriter_SetPolicy(
            speakerManager->speakerBufferWriter,
            AIA_DATA_STREAM_BUFFER_WRITER_ALL_OR_NOTHING );
    }

    if( !speakerManager->currentSpeakerState.isBufferedSpeakerFramePending )
    {
        /* NOTE: Frame sizes are pretty small (~160 bytes) so there is minimal
         * risk of blowing out the stack here. However, if this grows in the
         * future, we should consider alternative places to store the frame. */
        uint8_t speakerFrame[ speakerManager->frameSize ];
        ssize_t amountRead =
            AiaDataStreamReader_Read( speakerManager->speakerBufferReader,
                                      speakerFrame, speakerManager->frameSize );
        currentOffset = AiaDataStreamReader_Tell(
            speakerManager->speakerBufferReader,
            AIA_DATA_STREAM_BUFFER_READER_REFERENCE_ABSOLUTE );
        if( amountRead <= 0 )
        {
            AiaLogError( "AiaDataStreamReader_Read failed, status=%s",
                         AiaDataStreamReader_ErrorToString( amountRead ) );
        }
        if( amountRead == AIA_DATA_STREAM_BUFFER_READER_ERROR_CLOSED ||
            amountRead == AIA_DATA_STREAM_BUFFER_READER_ERROR_INVALID )
        {
            /* TODO: ADSER-1532 Close connection and tear down once connection
             * component is finished. */
            return;
        }
        else if( amountRead == AIA_DATA_STREAM_BUFFER_READER_ERROR_OVERRUN )
        {
            AiaLogError( "Reader overrun" );
            /* The OVERRUN buffer state changed event is sent on writing inbound
             * messages to the buffer. This should not happen since we set the
             * write policy to ALL_OR_NOTHING above. */
            return;
        }
        else if( amountRead == AIA_DATA_STREAM_BUFFER_READER_ERROR_WOULDBLOCK )
        {
            AiaLogDebug( "No data remaining" );
            if( speakerManager->currentSpeakerState.currentBufferState !=
                AIA_UNDERRUN_STATE )
            {
                /* TODO: ADSER-1586 Sending the event could be extraneous here:
                 * "When the device first opens the speaker its audio buffer may
                 * be empty. The device does not need to send a
                 * BufferStateChanged event indicating UNDERRUN in this case."
                 */
                AiaJsonMessage_t* underrunEvent =
                    generateBufferStateChangedEvent(
                        speakerManager->lastSpeakerSequenceNumberProcessed + 1,
                        AIA_UNDERRUN_STATE );
                if( !AiaRegulator_Write(
                        speakerManager->regulator,
                        AiaJsonMessage_ToMessage( underrunEvent ) ) )
                {
                    AiaLogError( "AiaRegulator_Write failed" );
                    AiaJsonMessage_Destroy( underrunEvent );
                }
                AiaSpeakerManager_SetBufferStateLocked( speakerManager,
                                                        AIA_UNDERRUN_STATE );
            }
            return;
        }
        else
        {
            AiaSpeakerManagerBufferState_t previousBufferState =
                speakerManager->currentSpeakerState.currentBufferState;
            updateBufferStateLocked( speakerManager );
            if( speakerManager->currentSpeakerState.isSpeakerOpen &&
                speakerManager->currentSpeakerState.currentBufferState ==
                    AIA_UNDERRUN_WARNING_STATE &&
                previousBufferState > AIA_UNDERRUN_WARNING_STATE )
            {
                /* TODO: ADSER-1587 We should only send this when draining the
                 * buffer rather than as we're filling it as well. */
                /* TODO: ADSER-1588 Add more fine grained tuning to ensure we
                 * send this when we've definitely dropped below the threshold -
                 * this could result in repeated sending of this event in the
                 * case of extremely small messages continuously straddling the
                 * threshold.
                 */
                AiaJsonMessage_t* underrunWarningEvent =
                    generateBufferStateChangedEvent(
                        speakerManager->lastSpeakerSequenceNumberProcessed,
                        AIA_UNDERRUN_WARNING_STATE );
                if( !AiaRegulator_Write(
                        speakerManager->regulator,
                        AiaJsonMessage_ToMessage( underrunWarningEvent ) ) )
                {
                    AiaLogError( "AiaRegulator_Write failed" );
                    AiaJsonMessage_Destroy( underrunWarningEvent );
                }
            }
        }

        if( !speakerManager->playSpeakerDataCb(
                speakerFrame, amountRead,
                speakerManager->playSpeakerDataCbUserData ) )
        {
            memcpy( speakerManager->currentSpeakerState.bufferedSpeakerFrame,
                    speakerFrame, speakerManager->frameSize );
            speakerManager->currentSpeakerState.isBufferedSpeakerFramePending =
                true;
            speakerManager->currentSpeakerState.isSpeakerReadyForData = false;
        }
    }
    else
    {
        if( !speakerManager->playSpeakerDataCb(
                speakerManager->currentSpeakerState.bufferedSpeakerFrame,
                speakerManager->frameSize,
                speakerManager->playSpeakerDataCbUserData ) )
        {
            speakerManager->currentSpeakerState.isBufferedSpeakerFramePending =
                true;
            speakerManager->currentSpeakerState.isSpeakerReadyForData = false;
        }
        else
        {
            speakerManager->currentSpeakerState.isBufferedSpeakerFramePending =
                false;
        }
    }

    speakerManager->currentSpeakerState.isSpeakerOpen = true;
    if( speakerManager->currentSpeakerState.pendingOpenSpeaker )
    {
        AiaBinaryAudioStreamOffset_t speakerOpenedOffset =
            currentOffset - speakerManager->frameSize;
        AiaLogDebug( "Speaker opened, offset=%" PRIu64, speakerOpenedOffset );
        speakerManager->currentSpeakerState.pendingOpenSpeaker = false;
        AiaJsonMessage_t* speakerOpenedEvent =
            generateSpeakerOpenedEvent( speakerOpenedOffset );
        if( !AiaRegulator_Write(
                speakerManager->regulator,
                AiaJsonMessage_ToMessage( speakerOpenedEvent ) ) )
        {
            AiaLogError( "AiaRegulator_Write failed" );
            AiaJsonMessage_Destroy( speakerOpenedEvent );
        }
    }

    AiaListDouble( Link_t )* link = NULL;

    while( ( link = AiaListDouble( PeekHead )(
                 &speakerManager->accumulatedMarkers ) ) &&
           ( (AiaSpeakerMarkerSlot_t*)link )->offset < currentOffset )
    {
        AiaLogDebug( "Marker reached, marker=%" PRIu32,
                     ( (AiaSpeakerMarkerSlot_t*)link )->marker );
        AiaJsonMessage_t* speakerMarkerEncounteredEvent =
            generateSpeakerMarkerEncounteredEvent(
                ( (AiaSpeakerMarkerSlot_t*)link )->marker );
        if( !AiaRegulator_Write(
                speakerManager->regulator,
                AiaJsonMessage_ToMessage( speakerMarkerEncounteredEvent ) ) )
        {
            AiaLogError( "AiaRegulator_Write failed" );
            AiaJsonMessage_Destroy( speakerMarkerEncounteredEvent );
        }
        AiaListDouble( RemoveHead )( &speakerManager->accumulatedMarkers );
        AiaSpeakerMarkerSlot_t* slot = (AiaSpeakerMarkerSlot_t*)link;
        AiaFree( slot );
    }
}

static void AiaSpeakerManager_PlaySpeakerDataRoutine( void* context )
{
    AiaSpeakerManager_t* speakerManager = (AiaSpeakerManager_t*)context;
    AiaAssert( speakerManager );
    if( !speakerManager )
    {
        AiaLogError( "Null speakerManager" );
        AiaCriticalFailure();
        return;
    }
    AiaMutex( Lock )( &speakerManager->mutex );
#ifdef AIA_ENABLE_ALERTS
    if( speakerManager->currentSpeakerState.shouldStartOfflineAlertPlayback &&
        !AiaSpeakerManager_CanSpeakerStreamLocked( speakerManager ) )
    {
        if( !AiaSpeakerManager_ChangeVolumeLocked(
                speakerManager,
                speakerManager->currentSpeakerState.offlineAlertVolume ) )
        {
            AiaLogWarn( "Failed to set volume for offline alert playback" );
        }
        if( !speakerManager->playOfflineAlertCb(
                speakerManager->currentSpeakerState.alertToPlay,
                speakerManager->playOfflineAlertCbUserData ) )
        {
            AiaLogDebug( "Failed to play offline alert data" );
        }
        else
        {
            AiaLogDebug( "Started offline alert playback successfully" );
            speakerManager->currentSpeakerState
                .shouldStartOfflineAlertPlayback = false;
        }
        AiaMutex( Unlock )( &speakerManager->mutex );
        return;
    }
#endif
#ifdef AIA_ENABLE_ALERTS
    /* Stop offline alert playback if it has already been started */
    if( !speakerManager->currentSpeakerState.shouldStartOfflineAlertPlayback &&
        AiaSpeakerManager_CanSpeakerStreamLocked( speakerManager ) )
    {
        if( !speakerManager->stopOfflineAlertCb(
                speakerManager->stopOfflineAlertCbUserData ) )
        {
            AiaLogDebug( "Failed to stop offline alert" );
            AiaMutex( Unlock )( &speakerManager->mutex );
            return;
        }
        AiaSpeakerManager_StopOfflineAlertLocked( speakerManager );
    }
#endif
    AiaSpeakerManager_PlaySpeakerDataRoutineLocked( speakerManager );
    AiaMutex( Unlock )( &speakerManager->mutex );
}

AiaSpeakerManager_t* AiaSpeakerManager_Create(
    size_t speakerBufferSize, size_t overrunWarningThreshold,
    size_t underrunWarningThreshold, AiaPlaySpeakerData_t playSpeakerDataCb,
    void* playSpeakerDataCbUserData, AiaSequencer_t* sequencer,
    AiaRegulator_t* regulator, AiaSetVolume_t setVolumeCb,
    void* setVolumeCbUserData, AiaOfflineAlertPlayback_t playOfflineAlertCb,
    void* playOfflineAlertCbUserData, AiaOfflineAlertStop_t stopOfflineAlertCb,
    void* stopOfflineAlertCbUserData,
    AiaSpeakerManagerBufferStateObserver_t notifyObserversCb,
    void* notifyObserversCbUserData )
{
    if( !playSpeakerDataCb )
    {
        AiaLogError( "Null playSpeakerDataCb" );
        return NULL;
    }
    if( !sequencer )
    {
        AiaLogError( "Null sequencer" );
        return NULL;
    }
    if( !regulator )
    {
        AiaLogError( "Null regulator" );
        return NULL;
    }
    if( !setVolumeCb )
    {
        AiaLogError( "Null setVolumeCb" );
        return NULL;
    }
    if( !playOfflineAlertCb )
    {
        AiaLogError( "Null playOfflineAlertCb" );
        return NULL;
    }
    if( !stopOfflineAlertCb )
    {
        AiaLogError( "Null stopOfflineAlertCb" );
        return NULL;
    }
    if( !notifyObserversCb )
    {
        AiaLogDebug( "Null notifyObserversCb" );
    }
    AiaSpeakerManager_t* speakerManager =
        (AiaSpeakerManager_t*)AiaCalloc( 1, sizeof( AiaSpeakerManager_t ) );
    if( !speakerManager )
    {
        AiaLogError( "AiaCalloc failed, bytes=%zu.",
                     sizeof( AiaSpeakerManager_t ) );
        return NULL;
    }

    if( !AiaMutex( Create )( &speakerManager->mutex, false ) )
    {
        AiaLogError( "AiaMutex( Create ) failed." );
        AiaFree( speakerManager );
        return NULL;
    }

    speakerManager->speakerBufferMemory = AiaCalloc( 1, speakerBufferSize );
    if( !speakerManager->speakerBufferMemory )
    {
        AiaLogError( "AiaCalloc failed, bytes=%zu.", speakerBufferSize );
        AiaMutex( Destroy )( &speakerManager->mutex );
        AiaFree( speakerManager );
        return NULL;
    }

    *(AiaDataStreamBuffer_t**)&speakerManager->speakerBuffer =
        AiaDataStreamBuffer_Create( speakerManager->speakerBufferMemory,
                                    speakerBufferSize, 1, 1 );
    if( !speakerManager->speakerBuffer )
    {
        AiaLogError( "AiaDataStreamBuffer_Create failed." );
        AiaFree( speakerManager->speakerBufferMemory );
        AiaMutex( Destroy )( &speakerManager->mutex );
        AiaFree( speakerManager );
        return NULL;
    }

    *(AiaDataStreamWriter_t**)&speakerManager->speakerBufferWriter =
        AiaDataStreamBuffer_CreateWriter(
            speakerManager->speakerBuffer,
            AIA_DATA_STREAM_BUFFER_WRITER_NONBLOCKABLE, false );
    if( !speakerManager->speakerBufferWriter )
    {
        AiaLogError( "AiaDataStreamBuffer_CreateWriter failed." );
        AiaDataStreamBuffer_Destroy( speakerManager->speakerBuffer );
        AiaFree( speakerManager->speakerBufferMemory );
        AiaMutex( Destroy )( &speakerManager->mutex );
        AiaFree( speakerManager );
        return NULL;
    }

    *(AiaDataStreamReader_t**)&speakerManager->speakerBufferReader =
        AiaDataStreamBuffer_CreateReader(
            speakerManager->speakerBuffer,
            AIA_DATA_STREAM_BUFFER_READER_NONBLOCKING, false );
    if( !speakerManager->speakerBufferReader )
    {
        AiaLogError( "AiaDataStreamBuffer_CreateReader failed." );
        AiaDataStreamWriter_Destroy( speakerManager->speakerBufferWriter );
        AiaDataStreamBuffer_Destroy( speakerManager->speakerBuffer );
        AiaFree( speakerManager->speakerBufferMemory );
        AiaMutex( Destroy )( &speakerManager->mutex );
        AiaFree( speakerManager );
        return NULL;
    }

    AiaListDouble( Create )( &speakerManager->accumulatedMarkers );
    AiaListDouble( Create )( &speakerManager->offsetActions );
    AiaListDouble( Create )( &speakerManager->volumeActions );

    *(size_t*)&( speakerManager->overrunWarningThreshold ) =
        overrunWarningThreshold;
    *(size_t*)&( speakerManager->underrunWarningThreshold ) =
        underrunWarningThreshold;
    *(AiaPlaySpeakerData_t*)&( speakerManager->playSpeakerDataCb ) =
        playSpeakerDataCb;
    *(void**)&speakerManager->playSpeakerDataCbUserData =
        playSpeakerDataCbUserData;
    *(AiaSequencer_t**)&speakerManager->sequencer = sequencer;
    *(AiaRegulator_t**)&speakerManager->regulator = regulator;
    *(AiaSetVolume_t*)&( speakerManager->setVolumeCb ) = setVolumeCb;
    *(void**)&speakerManager->setVolumeCbUserData = setVolumeCbUserData;
    *(AiaOfflineAlertPlayback_t*)&( speakerManager->playOfflineAlertCb ) =
        playOfflineAlertCb;
    *(void**)&speakerManager->playOfflineAlertCbUserData =
        playOfflineAlertCbUserData;
    *(AiaOfflineAlertStop_t*)&( speakerManager->stopOfflineAlertCb ) =
        stopOfflineAlertCb;
    *(void**)&speakerManager->stopOfflineAlertCbUserData =
        stopOfflineAlertCbUserData;
    *(AiaSpeakerManagerBufferStateObserver_t*)&(
        speakerManager->notifyObserversCb ) = notifyObserversCb;
    *(void**)&speakerManager->notifyObserversCbUserData =
        notifyObserversCbUserData;

    speakerManager->currentSpeakerState.currentBufferState = AIA_NONE_STATE;
    speakerManager->currentSpeakerState.isSpeakerReadyForData = true;

    AiaSpeakerManagerVolumeDataForAction_t* volumeSlot =
        AiaCalloc( 1, sizeof( AiaSpeakerManagerVolumeDataForAction_t ) );
    if( !volumeSlot )
    {
        AiaLogError( "AiaCalloc failed, bytes=%zu",
                     sizeof( AiaSpeakerManagerVolumeDataForAction_t ) );
        AiaDataStreamWriter_Destroy( speakerManager->speakerBufferWriter );
        AiaDataStreamBuffer_Destroy( speakerManager->speakerBuffer );
        AiaListDouble( Link_t )* link = NULL;
        while( ( link = AiaListDouble( RemoveHead )(
                     &speakerManager->accumulatedMarkers ) ) )
        {
            AiaSpeakerMarkerSlot_t* slot = (AiaSpeakerMarkerSlot_t*)link;
            AiaFree( slot );
        }
        AiaFree( speakerManager->speakerBufferMemory );
        AiaMutex( Destroy )( &speakerManager->mutex );
        AiaFree( speakerManager );
        return NULL;
    }
    AiaListDouble( Link_t ) defaultLink = AiaListDouble( LINK_INITIALIZER );
    volumeSlot->link = defaultLink;
#ifdef AIA_LOAD_VOLUME
    volumeSlot->volume = AIA_LOAD_VOLUME();
#else
    volumeSlot->volume = AIA_DEFAULT_VOLUME;
#endif
    volumeSlot->speakerManager = speakerManager;
    speakerManager->currentSpeakerState.initialVolume = true;

    if( AiaSpeakerManager_InvokeActionAtOffset(
            speakerManager, 0, AiaSpeakerManager_SetVolumeActionLocked,
            volumeSlot ) == AIA_INVALID_ACTION_ID )
    {
        AiaLogError( "AiaSpeakerManager_InvokeActionAtOffset failed" );
        AiaFree( volumeSlot );
        AiaListDouble( RemoveAll )( &speakerManager->offsetActions, AiaFree,
                                    0 );
        AiaListDouble( RemoveAll )( &speakerManager->volumeActions, AiaFree,
                                    0 );
        AiaDataStreamWriter_Destroy( speakerManager->speakerBufferWriter );
        AiaDataStreamBuffer_Destroy( speakerManager->speakerBuffer );
        AiaListDouble( Link_t )* link = NULL;
        while( ( link = AiaListDouble( RemoveHead )(
                     &speakerManager->accumulatedMarkers ) ) )
        {
            AiaSpeakerMarkerSlot_t* slot = (AiaSpeakerMarkerSlot_t*)link;
            AiaFree( slot );
        }
        AiaFree( speakerManager->speakerBufferMemory );
        AiaMutex( Destroy )( &speakerManager->mutex );
        AiaFree( speakerManager );
        return NULL;
    }
    AiaListDouble( InsertTail )( &speakerManager->volumeActions,
                                 &volumeSlot->link );

    if( !AiaTimer( Create )( &speakerManager->speakerWorker,
                             AiaSpeakerManager_PlaySpeakerDataRoutine,
                             speakerManager ) )
    {
        AiaLogError( "AiaTimer( Create ) failed" );
        AiaListDouble( RemoveAll )( &speakerManager->offsetActions, AiaFree,
                                    0 );
        AiaListDouble( RemoveAll )( &speakerManager->volumeActions, AiaFree,
                                    0 );
        AiaDataStreamWriter_Destroy( speakerManager->speakerBufferWriter );
        AiaDataStreamBuffer_Destroy( speakerManager->speakerBuffer );
        AiaListDouble( Link_t )* link = NULL;
        while( ( link = AiaListDouble( RemoveHead )(
                     &speakerManager->accumulatedMarkers ) ) )
        {
            AiaSpeakerMarkerSlot_t* slot = (AiaSpeakerMarkerSlot_t*)link;
            AiaFree( slot );
        }
        AiaFree( speakerManager->speakerBufferMemory );
        AiaMutex( Destroy )( &speakerManager->mutex );
        AiaFree( speakerManager );
        return NULL;
    }

    if( !AiaTimer( Arm )( &speakerManager->speakerWorker,
                          AIA_SPEAKER_FRAME_PUSH_CADENCE_MS,
                          AIA_SPEAKER_FRAME_PUSH_CADENCE_MS ) )
    {
        AiaLogError( "AiaTimer( Arm ) failed" );
        AiaTimer( Destroy )( &speakerManager->speakerWorker );
        AiaListDouble( RemoveAll )( &speakerManager->offsetActions, AiaFree,
                                    0 );
        AiaListDouble( RemoveAll )( &speakerManager->volumeActions, AiaFree,
                                    0 );
        AiaDataStreamWriter_Destroy( speakerManager->speakerBufferWriter );
        AiaDataStreamBuffer_Destroy( speakerManager->speakerBuffer );
        AiaListDouble( Link_t )* link = NULL;
        while( ( link = AiaListDouble( RemoveHead )(
                     &speakerManager->accumulatedMarkers ) ) )
        {
            AiaSpeakerMarkerSlot_t* slot = (AiaSpeakerMarkerSlot_t*)link;
            AiaFree( slot );
        }
        AiaFree( speakerManager->speakerBufferMemory );
        AiaMutex( Destroy )( &speakerManager->mutex );
        AiaFree( speakerManager );
        return NULL;
    }

    return speakerManager;
}

void AiaSpeakerManager_Destroy( AiaSpeakerManager_t* speakerManager )
{
    if( !speakerManager )
    {
        AiaLogDebug( "Null speakerManager." );
        return;
    }

    AiaMutex( Lock )( &speakerManager->mutex );

    AiaTimer( Destroy )( &speakerManager->speakerWorker );

#ifdef AIA_ENABLE_ALERTS
    if( speakerManager->currentSpeakerState.alertToPlay )
    {
        AiaFree( speakerManager->currentSpeakerState.alertToPlay );
        speakerManager->currentSpeakerState.alertToPlay = NULL;
    }
#endif

    if( speakerManager->frameSize )
    {
        AiaFree( speakerManager->currentSpeakerState.bufferedSpeakerFrame );
    }
    AiaListDouble( Link_t )* link = NULL;
    while( ( link = AiaListDouble( RemoveHead )(
                 &speakerManager->accumulatedMarkers ) ) )
    {
        AiaSpeakerMarkerSlot_t* slot = (AiaSpeakerMarkerSlot_t*)link;
        AiaFree( slot );
    }
    link = NULL;

    AiaListDouble( RemoveAll )( &speakerManager->offsetActions, AiaFree, 0 );
    AiaListDouble( RemoveAll )( &speakerManager->volumeActions, AiaFree, 0 );

    AiaDataStreamWriter_Destroy( speakerManager->speakerBufferWriter );
    AiaDataStreamReader_Destroy( speakerManager->speakerBufferReader );
    AiaDataStreamBuffer_Destroy( speakerManager->speakerBuffer );
    AiaFree( speakerManager->speakerBufferMemory );
    AiaMutex( Unlock )( &speakerManager->mutex );
    AiaMutex( Destroy )( &speakerManager->mutex );
    AiaFree( speakerManager );
}

/**
 * Helper function to validate a speaker topic message and retrieve its total
 * audio length in bytes.
 *
 * @param speakerManager The @c AiaSpeakerManager_t to act on.
 * @param sequenceNumber The sequence number of the message.
 * @param message Pointer to the unencrypted message body (without the common
 * header) i.e. the unencrypted Binary Stream. This must remain valid for the
 * duration of this call.
 * @param size The size of the message.
 * @param[out] totalAudioLength The total length of the audio bytes found in the
 * message.
 * @return @c true if the message was parsed successfully or @c false otherwise.
 * @note This method must be called while @c speakerManager->mutex is locked.
 */
static bool
AiaSpeakerManager_ValidateSpeakerTopicMessageAndGetTotalAudioLengthLocked(
    AiaSpeakerManager_t* speakerManager, AiaSequenceNumber_t sequenceNumber,
    const uint8_t* message, size_t size, size_t* totalAudioLength )
{
    size_t index = 0;
    size_t totalLength = 0;
    uint32_t bytePosition = 0;
    while( bytePosition < size )
    {
        if( bytePosition + AIA_SIZE_OF_BINARY_STREAM_HEADER > size )
        {
            AiaLogError(
                "Message too small to extract binary stream header, "
                "sequenceNumber=%" PRIu32,
                sequenceNumber );
            AiaJsonMessage_t* malformedMessageEvent =
                generateMalformedMessageExceptionEncounteredEvent(
                    sequenceNumber, index, AIA_TOPIC_SPEAKER );
            if( !AiaRegulator_Write(
                    speakerManager->regulator,
                    AiaJsonMessage_ToMessage( malformedMessageEvent ) ) )
            {
                AiaLogError( "AiaRegulator_Write failed" );
                AiaJsonMessage_Destroy( malformedMessageEvent );
            }
            return false;
        }

        /* TODO: ADSER-1945 Consolidate endian-specific funtionality into a
         * utility function. */
        AiaBinaryMessageLength_t length = 0;
        for( size_t i = 0; i < sizeof( AiaBinaryMessageLength_t );
             ++i, ++bytePosition )
        {
            length |= message[ bytePosition ] << ( i * 8 );
        }

        AiaBinaryMessageType_t type = 0;
        for( size_t i = 0; i < sizeof( AiaBinaryMessageType_t );
             ++i, ++bytePosition )
        {
            type |= message[ bytePosition ] << ( i * 8 );
        }

        AiaBinaryMessageCount_t count = 0;
        for( size_t i = 0; i < sizeof( AiaBinaryMessageCount_t );
             ++i, ++bytePosition )
        {
            count |= message[ bytePosition ] << ( i * 8 );
        }

        AiaLogDebug( "Parsed speaker topic message, length=%" PRIu32
                     ", type=%" PRIu8 ", count=%" PRIu8,
                     length, type, count );

        bytePosition += AIA_BINARY_MESSAGE_NUM_RESERVED_BYTES;

        if( message + bytePosition + length > message + size )
        {
            AiaLogError( "Invalid binary stream length, length=%" PRIu32
                         ", message size=%zu.",
                         length, size );
            AiaJsonMessage_t* malformedMessageEvent =
                generateMalformedMessageExceptionEncounteredEvent(
                    sequenceNumber, index, AIA_TOPIC_SPEAKER );
            if( !AiaRegulator_Write(
                    speakerManager->regulator,
                    AiaJsonMessage_ToMessage( malformedMessageEvent ) ) )
            {
                AiaLogError( "AiaRegulator_Write failed" );
                AiaJsonMessage_Destroy( malformedMessageEvent );
            }
            return false;
        }

        switch( type )
        {
            case AIA_BINARY_STREAM_SPEAKER_CONTENT_TYPE:
                bytePosition += length;
                totalLength +=
                    ( length - sizeof( AiaBinaryAudioStreamOffset_t ) );
                ++index;
                continue;
            case AIA_BINARY_STREAM_SPEAKER_MARKER_TYPE:
                bytePosition += length;
                ++index;
                continue;
        }
        AiaLogError( "Unknown binary stream type, type=%" PRIu8, type );
        return false;
    }
    *totalAudioLength = totalLength;
    return true;
}

static void AiaSpeakerManager_OnSpeakerTopicMessageReceivedLocked(
    AiaSpeakerManager_t* speakerManager, const uint8_t* message, size_t size,
    AiaSequenceNumber_t sequenceNumber )
{
    if( speakerManager->overrunSpeakerSequenceNumber )
    {
        if( sequenceNumber != speakerManager->overrunSpeakerSequenceNumber )
        {
            AiaLogError(
                "Still waiting for message, current sequence number=%" PRIu32
                " expected "
                "sequence number=%" PRIu32,
                sequenceNumber, speakerManager->overrunSpeakerSequenceNumber );
            return;
        }
        else
        {
            AiaLogInfo(
                "Received expected sequence number after overrun, sequence "
                "number=%" PRIu32,
                sequenceNumber );
            speakerManager->overrunSpeakerSequenceNumber = 0;
        }
    }

    size_t totalAudioLength = 0;
    if( !AiaSpeakerManager_ValidateSpeakerTopicMessageAndGetTotalAudioLengthLocked(
            speakerManager, sequenceNumber, message, size, &totalAudioLength ) )
    {
        AiaLogError(
            "AiaSpeakerManager_"
            "ValidateSpeakerTopicMessageAndGetTotalAudioLengthLocked failed" );
        return;
    }
    size_t spaceInBuffer =
        AiaDataStreamBuffer_GetDataSize( speakerManager->speakerBuffer ) -
        ( ( AiaDataStreamWriter_Tell( speakerManager->speakerBufferWriter ) -
            AiaDataStreamReader_Tell(
                speakerManager->speakerBufferReader,
                AIA_DATA_STREAM_BUFFER_READER_REFERENCE_ABSOLUTE ) ) );

    /* Only send an overrun and don't consume this data if the speaker is open.
     */
    if( totalAudioLength > spaceInBuffer &&
        speakerManager->currentSpeakerState.isSpeakerOpen )
    {
        AiaLogInfo(
            "Not enough space in buffer to consume audio, "
            "totalAudioLength=%zu, spaceInBuffer=%zu",
            totalAudioLength, spaceInBuffer );
        if( speakerManager->currentSpeakerState.currentBufferState !=
            AIA_OVERRUN_STATE )
        {
            /* If we're already in an overrun state, no need to send
             * another event Service will redrive this packets. This
             * only needs to be sent once. */
            AiaJsonMessage_t* overrunEvent = generateBufferStateChangedEvent(
                sequenceNumber, AIA_OVERRUN_STATE );
            if( !AiaRegulator_Write(
                    speakerManager->regulator,
                    AiaJsonMessage_ToMessage( overrunEvent ) ) )
            {
                AiaLogError( "AiaRegulator_Write failed" );
                AiaJsonMessage_Destroy( overrunEvent );
            }
            speakerManager->overrunSpeakerSequenceNumber = sequenceNumber;
            AiaSpeakerManager_SetBufferStateLocked( speakerManager,
                                                    AIA_OVERRUN_STATE );
            /* TODO: ADSER-1585 This is thread-safe since emissions out
             * of the sequencer occur on the same thread as writes into
             * it. Therefore calling back into the sequencer from here
             * will occur on the same thread as the inbound message
             * flow. Since the sequencer is not thread-safe, sequencer
             * users are responsible for ensuring no other calls to it
             * are made while a message is being written to it already. */
            AiaSequencer_ResetSequenceNumber( speakerManager->sequencer,
                                              sequenceNumber );
        }
        return;
    }

    /* Else, write message contents into the buffer and allow old buffer
     * contents to be overwritten. */

    size_t index = 0;
    uint32_t bytePosition = 0;
    while( bytePosition < size )
    {
        AiaBinaryMessageLength_t length = 0;
        for( size_t i = 0; i < sizeof( AiaBinaryMessageLength_t );
             ++i, ++bytePosition )
        {
            length |= message[ bytePosition ] << ( i * 8 );
        }

        AiaBinaryMessageType_t type = 0;
        for( size_t i = 0; i < sizeof( AiaBinaryMessageType_t );
             ++i, ++bytePosition )
        {
            type |= message[ bytePosition ] << ( i * 8 );
        }

        AiaBinaryMessageCount_t count = 0;
        for( size_t i = 0; i < sizeof( AiaBinaryMessageCount_t );
             ++i, ++bytePosition )
        {
            count |= message[ bytePosition ] << ( i * 8 );
        }

        bytePosition += AIA_BINARY_MESSAGE_NUM_RESERVED_BYTES;

        switch( type )
        {
            case AIA_BINARY_STREAM_SPEAKER_CONTENT_TYPE:
                if( !handleSpeakerTopicContentTypeEntryLocked(
                        speakerManager, message + bytePosition, length, count,
                        sequenceNumber ) )
                {
                    AiaJsonMessage_t* malformedMessageEvent =
                        generateMalformedMessageExceptionEncounteredEvent(
                            sequenceNumber, index, AIA_TOPIC_SPEAKER );
                    if( !AiaRegulator_Write( speakerManager->regulator,
                                             AiaJsonMessage_ToMessage(
                                                 malformedMessageEvent ) ) )
                    {
                        AiaLogError( "AiaRegulator_Write failed" );
                        AiaJsonMessage_Destroy( malformedMessageEvent );
                    }
                    return;
                }
                bytePosition += length;
                ++index;
                continue;
            case AIA_BINARY_STREAM_SPEAKER_MARKER_TYPE:
                if( !handleSpeakerTopicMarkerTypeEntryLocked(
                        speakerManager, message + bytePosition, length,
                        count ) )
                {
                    AiaJsonMessage_t* malformedMessageEvent =
                        generateMalformedMessageExceptionEncounteredEvent(
                            sequenceNumber, index, AIA_TOPIC_SPEAKER );
                    if( !AiaRegulator_Write( speakerManager->regulator,
                                             AiaJsonMessage_ToMessage(
                                                 malformedMessageEvent ) ) )
                    {
                        AiaLogError( "AiaRegulator_Write failed" );
                        AiaJsonMessage_Destroy( malformedMessageEvent );
                    }
                    return;
                }
                bytePosition += length;
                ++index;
                continue;
        }
        AiaLogError( "Unknown binary stream type, type=%" PRIu8, type );
        return;
    }
}

void AiaSpeakerManager_OnSpeakerTopicMessageReceived(
    AiaSpeakerManager_t* speakerManager, const uint8_t* message, size_t size,
    AiaSequenceNumber_t sequenceNumber )
{
    AiaAssert( speakerManager );
    if( !speakerManager )
    {
        AiaLogError( "Null speakerManager." );
        AiaCriticalFailure();
        return;
    }

    if( !message )
    {
        AiaLogError( "Null message." );
        AiaJsonMessage_t* malformedMessageEvent =
            generateMalformedMessageExceptionEncounteredEvent(
                sequenceNumber, 0, AIA_TOPIC_SPEAKER );
        if( !AiaRegulator_Write(
                speakerManager->regulator,
                AiaJsonMessage_ToMessage( malformedMessageEvent ) ) )
        {
            AiaLogError( "AiaRegulator_Write failed" );
            AiaJsonMessage_Destroy( malformedMessageEvent );
        }
        return;
    }

    AiaMutex( Lock )( &speakerManager->mutex );
    AiaSpeakerManager_OnSpeakerTopicMessageReceivedLocked(
        speakerManager, message, size, sequenceNumber );
    AiaMutex( Unlock )( &speakerManager->mutex );
}

bool handleSpeakerTopicContentTypeEntryLocked(
    AiaSpeakerManager_t* speakerManager, const uint8_t* data, size_t length,
    AiaBinaryMessageCount_t count, AiaSequenceNumber_t sequenceNumber )
{
    if( !speakerManager )
    {
        AiaLogError( "Null speakerManager." );
        AiaCriticalFailure();
        return false;
    }
    if( !data )
    {
        AiaLogError( "Null data." );
        return false;
    }

    uint32_t bytePosition = 0;

    AiaBinaryAudioStreamOffset_t offset = 0;
    for( size_t i = 0; i < sizeof( AiaBinaryAudioStreamOffset_t );
         ++i, ++bytePosition )
    {
        offset |= data[ bytePosition ] << ( i * 8 );
    }
    AiaLogDebug( "Parsed speaker audio content entry offset, offset=%" PRIu64,
                 offset );

    AiaBinaryAudioStreamOffset_t localOffset =
        AiaDataStreamWriter_Tell( speakerManager->speakerBufferWriter );

    if( offset != localOffset )
    {
        AiaLogError( "Received non-contiguous offset, offset received=%" PRIu64
                     ", offset "
                     "expected=%" PRIu64,
                     offset, localOffset );
        return false;
    }

    /* Count is zero-indexed. */
    size_t numFrames = count + 1;
    size_t frameSize = ( length - bytePosition ) / numFrames;

    if( frameSize * numFrames !=
        length - sizeof( AiaBinaryAudioStreamOffset_t ) )
    {
        AiaLogError( "Invalid frame size, frameSize=%zu, numFrames=%zu",
                     frameSize, numFrames );
        return false;
    }

    if( !speakerManager->frameSize )
    {
        AiaLogDebug( "Initial occurrence parsing frame size, frame size=%zu",
                     frameSize );
        speakerManager->frameSize = frameSize;
        speakerManager->currentSpeakerState.bufferedSpeakerFrame =
            AiaCalloc( speakerManager->frameSize, sizeof( uint8_t ) );
        if( !speakerManager->currentSpeakerState.bufferedSpeakerFrame )
        {
            AiaLogError( "AiaCalloc failed, bytes=%zu.",
                         speakerManager->frameSize );
            AiaCriticalFailure();
            return false;
        }
    }
    else
    {
        if( speakerManager->frameSize != frameSize )
        {
            AiaLogError(
                "Different frame size received than previous frame size. VBR "
                "is currently not supported. Frame size=%zu, previous frame "
                "size=%zu",
                frameSize, speakerManager->frameSize );
            return false;
        }
    }

    const uint8_t* audio = data + bytePosition;
    size_t numAudioBytes = length - bytePosition;

    ssize_t amountWritten = AiaDataStreamWriter_Write(
        speakerManager->speakerBufferWriter, audio, numAudioBytes );
    if( amountWritten <= 0 )
    {
        AiaLogError( "AiaDataStreamWriter_Write failed, status=%s",
                     AiaDataStreamWriter_ErrorToString( amountWritten ) );
        switch( amountWritten )
        {
            case AIA_DATA_STREAM_BUFFER_WRITER_ERROR_CLOSED:
            case AIA_DATA_STREAM_BUFFER_WRITER_ERROR_INVALID:
            case AIA_DATA_STREAM_BUFFER_WRITER_ERROR_WOULD_BLOCK:
                return false;
        }
    }
    else if( (size_t)amountWritten < numAudioBytes )
    {
        /* This shouldn't ever happen since the writer is either ALL_OR_NOTHING
         * or NONBLOCKABLE. */
        AiaLogError(
            "Unexpected amount written, amountWritten=%zd, numAudioBytes=%zu",
            amountWritten, numAudioBytes );
        return false;
    }
    else
    {
        /* We were able to write the entire speaker entry to the audio buffer.
         */
        AiaSpeakerManagerBufferState_t previousBufferState =
            speakerManager->currentSpeakerState.currentBufferState;
        updateBufferStateLocked( speakerManager );
        if( speakerManager->currentSpeakerState.isSpeakerOpen &&
            speakerManager->currentSpeakerState.currentBufferState ==
                AIA_OVERRUN_WARNING_STATE &&
            previousBufferState < AIA_OVERRUN_WARNING_STATE )
        {
            AiaJsonMessage_t* overrunWarningEvent =
                generateBufferStateChangedEvent( sequenceNumber,
                                                 AIA_OVERRUN_WARNING_STATE );
            if( !AiaRegulator_Write(
                    speakerManager->regulator,
                    AiaJsonMessage_ToMessage( overrunWarningEvent ) ) )
            {
                AiaLogError( "AiaRegulator_Write failed" );
                AiaJsonMessage_Destroy( overrunWarningEvent );
            }
        }

        speakerManager->lastSpeakerSequenceNumberProcessed = sequenceNumber;
    }

    return true;
}

bool handleSpeakerTopicMarkerTypeEntryLocked(
    AiaSpeakerManager_t* speakerManager, const uint8_t* data, size_t length,
    AiaBinaryMessageCount_t count )
{
    if( !speakerManager )
    {
        AiaLogError( "Null speakerManager." );
        AiaCriticalFailure();
        return false;
    }
    if( !data )
    {
        AiaLogError( "Null data." );
        return false;
    }
    size_t numMarkers = count + 1;
    uint32_t bytePosition = 0;
    if( data + ( numMarkers * sizeof( AiaSpeakerBinaryMarker_t ) ) !=
        data + length )
    {
        AiaLogError(
            "Invalid amount of marker data, data length=%zu, numMarkers=%zu",
            length, numMarkers );
        return false;
    }

    while( numMarkers-- )
    {
        AiaSpeakerBinaryMarker_t marker = 0;
        for( size_t i = 0; i < sizeof( AiaSpeakerBinaryMarker_t );
             ++i, ++bytePosition )
        {
            marker |= data[ bytePosition ] << ( i * 8 );
        }
        AiaSpeakerMarkerSlot_t* markerSlot =
            AiaCalloc( 1, sizeof( AiaSpeakerMarkerSlot_t ) );
        if( !markerSlot )
        {
            AiaLogError( "AiaCalloc failed, bytes=%zu.",
                         sizeof( AiaSpeakerMarkerSlot_t ) );
            AiaJsonMessage_t* internalErrorEvent =
                generateInternalErrorExceptionEncounteredEvent();
            if( !AiaRegulator_Write(
                    speakerManager->regulator,
                    AiaJsonMessage_ToMessage( internalErrorEvent ) ) )
            {
                AiaLogError( "AiaRegulator_Write failed" );
                AiaJsonMessage_Destroy( internalErrorEvent );
            }
            AiaCriticalFailure();
            return false;
        }
        else
        {
            AiaListDouble( Link_t ) defaultLink =
                AiaListDouble( LINK_INITIALIZER );
            markerSlot->link = defaultLink;
            markerSlot->offset =
                AiaDataStreamWriter_Tell( speakerManager->speakerBufferWriter );
            markerSlot->marker = marker;

            /* Add it to the list. */
            AiaListDouble( InsertTail )( &speakerManager->accumulatedMarkers,
                                         &markerSlot->link );
        }
    }

    return true;
}

void AiaSpeakerManager_OnOpenSpeakerDirectiveReceived(
    void* manager, const void* payload, size_t size,
    AiaSequenceNumber_t sequenceNumber, size_t index )
{
    AiaSpeakerManager_t* speakerManager = (AiaSpeakerManager_t*)manager;
    AiaAssert( speakerManager );
    if( !speakerManager )
    {
        AiaLogError( "Null speakerManager." );
        AiaCriticalFailure();
        return;
    }

    if( !payload )
    {
        AiaLogError( "Null payload, sequenceNumber=%" PRIu32 ", index=%zu",
                     sequenceNumber, index );
        AiaJsonMessage_t* malformedMessageEvent =
            generateMalformedMessageExceptionEncounteredEvent(
                sequenceNumber, index, AIA_TOPIC_DIRECTIVE );
        if( !AiaRegulator_Write(
                speakerManager->regulator,
                AiaJsonMessage_ToMessage( malformedMessageEvent ) ) )
        {
            AiaLogError( "AiaRegulator_Write failed" );
            AiaJsonMessage_Destroy( malformedMessageEvent );
        }
        return;
    }
    const char* offset;
    size_t offsetLen;
    if( !AiaFindJsonValue( payload, size, AIA_OPEN_SPEAKER_OFFSET_KEY,
                           sizeof( AIA_OPEN_SPEAKER_OFFSET_KEY ) - 1, &offset,
                           &offsetLen ) )
    {
        AiaLogError( "No offset found" );
        AiaJsonMessage_t* malformedMessageEvent =
            generateMalformedMessageExceptionEncounteredEvent(
                sequenceNumber, index, AIA_TOPIC_DIRECTIVE );
        if( !AiaRegulator_Write(
                speakerManager->regulator,
                AiaJsonMessage_ToMessage( malformedMessageEvent ) ) )
        {
            AiaLogError( "AiaRegulator_Write failed" );
            AiaJsonMessage_Destroy( malformedMessageEvent );
        }
        return;
    }

    AiaBinaryAudioStreamOffset_t openSpeakerOffset = 0;
    if( !AiaExtractLongFromJsonValue( offset, offsetLen, &openSpeakerOffset ) )
    {
        AiaLogError( "Invalid offset, offset=%.*s", offsetLen, offset );
        AiaJsonMessage_t* malformedMessageEvent =
            generateMalformedMessageExceptionEncounteredEvent(
                sequenceNumber, index, AIA_TOPIC_DIRECTIVE );
        if( !AiaRegulator_Write(
                speakerManager->regulator,
                AiaJsonMessage_ToMessage( malformedMessageEvent ) ) )
        {
            AiaLogError( "AiaRegulator_Write failed" );
            AiaJsonMessage_Destroy( malformedMessageEvent );
        }
        return;
    }

    AiaLogDebug( "OpenSpeaker parsed, offset=%" PRIu64, openSpeakerOffset );

    AiaMutex( Lock )( &speakerManager->mutex );
    speakerManager->currentSpeakerState.pendingOpenSpeaker = true;
    speakerManager->currentSpeakerState.speakerOpenOffset = openSpeakerOffset;
    AiaMutex( Unlock )( &speakerManager->mutex );
}

void AiaSpeakerManager_OnCloseSpeakerDirectiveReceived(
    void* manager, const void* payload, size_t size,
    AiaSequenceNumber_t sequenceNumber, size_t index )
{
    AiaSpeakerManager_t* speakerManager = (AiaSpeakerManager_t*)manager;
    AiaAssert( speakerManager );
    if( !speakerManager )
    {
        AiaLogError( "Null speakerManager." );
        AiaCriticalFailure();
        return;
    }

    AiaBinaryAudioStreamOffset_t closeSpeakerOffset = 0;
    const char* offset;
    size_t offsetLen;
    if( !AiaFindJsonValue( payload, size, AIA_CLOSE_SPEAKER_OFFSET_KEY,
                           sizeof( AIA_CLOSE_SPEAKER_OFFSET_KEY ) - 1, &offset,
                           &offsetLen ) )
    {
        AiaLogInfo( "No offset given" );
        closeSpeakerOffset = AiaDataStreamReader_Tell(
            speakerManager->speakerBufferReader,
            AIA_DATA_STREAM_BUFFER_READER_REFERENCE_ABSOLUTE );
    }
    else
    {
        if( !AiaExtractLongFromJsonValue( offset, offsetLen,
                                          &closeSpeakerOffset ) )
        {
            AiaLogError( "Invalid offset, offset=%.*s", offsetLen, offset );
            AiaJsonMessage_t* malformedMessageEvent =
                generateMalformedMessageExceptionEncounteredEvent(
                    sequenceNumber, index, AIA_TOPIC_DIRECTIVE );
            if( !AiaRegulator_Write(
                    speakerManager->regulator,
                    AiaJsonMessage_ToMessage( malformedMessageEvent ) ) )
            {
                AiaLogError( "AiaRegulator_Write failed" );
                AiaJsonMessage_Destroy( malformedMessageEvent );
            }
            return;
        }
    }

    if( AiaSpeakerManager_InvokeActionAtOffset(
            speakerManager, closeSpeakerOffset,
            AiaSpeakerManager_CloseSpeakerActionLocked,
            speakerManager ) == AIA_INVALID_ACTION_ID )
    {
        AiaLogError( "AiaSpeakerManager_InvokeActionAtOffset failed" );
        AiaJsonMessage_t* internalErrorEvent =
            generateInternalErrorExceptionEncounteredEvent();
        if( !AiaRegulator_Write(
                speakerManager->regulator,
                AiaJsonMessage_ToMessage( internalErrorEvent ) ) )
        {
            AiaLogError( "AiaRegulator_Write failed" );
            AiaJsonMessage_Destroy( internalErrorEvent );
        }
    }
}

void AiaSpeakerManager_StopPlayback( AiaSpeakerManager_t* speakerManager )
{
    AiaAssert( speakerManager );
    if( !speakerManager )
    {
        AiaLogError( "Null speakerManager." );
        AiaCriticalFailure();
        return;
    }
    AiaMutex( Lock )( &speakerManager->mutex );

    AiaSpeakerManager_InvalidateActionsLocked( speakerManager );

    AiaSpeakerManager_CloseSpeakerLocked( speakerManager );

    AiaMutex( Unlock )( &speakerManager->mutex );
}

void AiaSpeakerManager_OnSpeakerReady( AiaSpeakerManager_t* speakerManager )
{
    AiaAssert( speakerManager );
    if( !speakerManager )
    {
        AiaLogError( "Null speakerManager." );
        AiaCriticalFailure();
        return;
    }
    AiaMutex( Lock )( &speakerManager->mutex );
    speakerManager->currentSpeakerState.isSpeakerReadyForData = true;
    AiaMutex( Unlock )( &speakerManager->mutex );
}

void AiaSpeakerManager_OnSetVolumeDirectiveReceived(
    void* manager, const void* payload, size_t size,
    AiaSequenceNumber_t sequenceNumber, size_t index )
{
    AiaSpeakerManager_t* speakerManager = (AiaSpeakerManager_t*)manager;
    AiaAssert( speakerManager );
    if( !speakerManager )
    {
        AiaLogError( "Null speakerManager." );
        return;
    }

    if( !payload )
    {
        AiaLogError( "Null payload, sequenceNumber=%" PRIu32 ", index=%zu",
                     sequenceNumber, index );
        AiaJsonMessage_t* malformedMessageEvent =
            generateMalformedMessageExceptionEncounteredEvent(
                sequenceNumber, index, AIA_TOPIC_DIRECTIVE );
        if( !AiaRegulator_Write(
                speakerManager->regulator,
                AiaJsonMessage_ToMessage( malformedMessageEvent ) ) )
        {
            AiaLogError( "AiaRegulator_Write failed" );
            AiaJsonMessage_Destroy( malformedMessageEvent );
        }
        return;
    }

    const char* volumeStr = NULL;
    size_t volumeLen = 0;
    if( !AiaFindJsonValue( payload, size, AIA_SET_VOLUME_VOLUME_KEY,
                           sizeof( AIA_SET_VOLUME_VOLUME_KEY ) - 1, &volumeStr,
                           &volumeLen ) )
    {
        AiaLogError( "No volume found" );
        AiaJsonMessage_t* malformedMessageEvent =
            generateMalformedMessageExceptionEncounteredEvent(
                sequenceNumber, index, AIA_TOPIC_DIRECTIVE );
        if( !AiaRegulator_Write(
                speakerManager->regulator,
                AiaJsonMessage_ToMessage( malformedMessageEvent ) ) )
        {
            AiaLogError( "AiaRegulator_Write failed" );
            AiaJsonMessage_Destroy( malformedMessageEvent );
        }
        return;
    }

    AiaJsonLongType volume;
    if( !AiaExtractLongFromJsonValue( volumeStr, volumeLen, &volume ) )
    {
        AiaLogError( "Invalid volume, volume=%.*s", volumeLen, volumeStr );
        AiaJsonMessage_t* malformedMessageEvent =
            generateMalformedMessageExceptionEncounteredEvent(
                sequenceNumber, index, AIA_TOPIC_DIRECTIVE );
        if( !AiaRegulator_Write(
                speakerManager->regulator,
                AiaJsonMessage_ToMessage( malformedMessageEvent ) ) )
        {
            AiaLogError( "AiaRegulator_Write failed" );
            AiaJsonMessage_Destroy( malformedMessageEvent );
        }
        return;
    }

    AiaBinaryAudioStreamOffset_t volumeOffset = 0;
    const char* offset = NULL;
    size_t offsetLen = 0;
    /* Treat offset as optional. */
    if( !AiaFindJsonValue( payload, size, AIA_SET_VOLUME_OFFSET_KEY,
                           sizeof( AIA_SET_VOLUME_OFFSET_KEY ) - 1, &offset,
                           &offsetLen ) )
    {
        AiaLogDebug( "No offset found" );
        volumeOffset = AiaDataStreamReader_Tell(
            speakerManager->speakerBufferReader,
            AIA_DATA_STREAM_BUFFER_READER_REFERENCE_ABSOLUTE );
    }
    else
    {
        if( !AiaExtractLongFromJsonValue( offset, offsetLen, &volumeOffset ) )
        {
            AiaLogError( "Invalid offset, offset=%.*s", offsetLen, offset );
            AiaJsonMessage_t* malformedMessageEvent =
                generateMalformedMessageExceptionEncounteredEvent(
                    sequenceNumber, index, AIA_TOPIC_DIRECTIVE );
            if( !AiaRegulator_Write(
                    speakerManager->regulator,
                    AiaJsonMessage_ToMessage( malformedMessageEvent ) ) )
            {
                AiaLogError( "AiaRegulator_Write failed" );
                AiaJsonMessage_Destroy( malformedMessageEvent );
            }
            return;
        }
    }

    AiaSpeakerManagerVolumeDataForAction_t* volumeSlot =
        AiaCalloc( 1, sizeof( AiaSpeakerManagerVolumeDataForAction_t ) );
    if( !volumeSlot )
    {
        AiaLogError( "AiaCalloc failed, bytes=%zu",
                     sizeof( AiaSpeakerManagerVolumeDataForAction_t ) );
        AiaJsonMessage_t* internalErrorEvent =
            generateInternalErrorExceptionEncounteredEvent();
        if( !AiaRegulator_Write(
                speakerManager->regulator,
                AiaJsonMessage_ToMessage( internalErrorEvent ) ) )
        {
            AiaLogError( "AiaRegulator_Write failed" );
            AiaJsonMessage_Destroy( internalErrorEvent );
        }
        return;
    }
    AiaListDouble( Link_t ) defaultLink = AiaListDouble( LINK_INITIALIZER );
    volumeSlot->link = defaultLink;
    volumeSlot->volume = volume;
    volumeSlot->speakerManager = speakerManager;

    if( AiaSpeakerManager_InvokeActionAtOffset(
            speakerManager, volumeOffset,
            AiaSpeakerManager_SetVolumeActionLocked,
            volumeSlot ) == AIA_INVALID_ACTION_ID )
    {
        AiaLogError( "AiaSpeakerManager_InvokeActionAtOffset failed" );
        AiaFree( volumeSlot );
        AiaJsonMessage_t* internalErrorEvent =
            generateInternalErrorExceptionEncounteredEvent();
        if( !AiaRegulator_Write(
                speakerManager->regulator,
                AiaJsonMessage_ToMessage( internalErrorEvent ) ) )
        {
            AiaLogError( "AiaRegulator_Write failed" );
            AiaJsonMessage_Destroy( internalErrorEvent );
        }
        return;
    }

    AiaMutex( Lock )( &speakerManager->mutex );
    AiaListDouble( InsertTail )( &speakerManager->volumeActions,
                                 &volumeSlot->link );
    AiaMutex( Unlock )( &speakerManager->mutex );
}

static bool AiaSpeakerManager_ChangeVolumeLocked(
    AiaSpeakerManager_t* speakerManager, uint8_t newVolume )
{
    AiaLogDebug( "Volume change from %" PRIu8 " to %" PRIu8,
                 speakerManager->currentSpeakerState.currentVolume, newVolume );

    speakerManager->setVolumeCb( newVolume,
                                 speakerManager->setVolumeCbUserData );

    if( newVolume == speakerManager->currentSpeakerState.currentVolume )
    {
        AiaLogDebug( "No volume change required, volume=%" PRIu8, newVolume );
        return true;
    }

    speakerManager->currentSpeakerState.currentVolume = newVolume;

    if( speakerManager->currentSpeakerState.initialVolume )
    {
        speakerManager->currentSpeakerState.initialVolume = false;
        return true;
    }

    AiaJsonMessage_t* volumeChangedEvent = NULL;
    if( speakerManager->currentSpeakerState.isSpeakerOpen )
    {
        volumeChangedEvent = generateVolumeChangedEventWithOffset(
            newVolume, AiaDataStreamReader_Tell(
                           speakerManager->speakerBufferReader,
                           AIA_DATA_STREAM_BUFFER_READER_REFERENCE_ABSOLUTE ) );
        if( !volumeChangedEvent )
        {
            AiaLogError( "generateVolumeChangedEventWithOffset failed" );
            return false;
        }
    }
    else
    {
        volumeChangedEvent =
            generateVolumeChangedEventWithoutOffset( newVolume );
        if( !volumeChangedEvent )
        {
            AiaLogError( "generateVolumeChangedEventWithoutOffset failed" );
            return false;
        }
    }

    if( !AiaRegulator_Write( speakerManager->regulator,
                             AiaJsonMessage_ToMessage( volumeChangedEvent ) ) )
    {
        AiaLogError( "AiaRegulator_Write failed" );
        AiaJsonMessage_Destroy( volumeChangedEvent );
        return false;
    }

    return true;
}

bool AiaSpeakerManager_ChangeVolume( AiaSpeakerManager_t* speakerManager,
                                     uint8_t newVolume )
{
    AiaAssert( speakerManager );
    if( !speakerManager )
    {
        AiaLogError( "Null speakerManager." );
        return false;
    }

    if( newVolume < AIA_MIN_VOLUME || newVolume > AIA_MAX_VOLUME )
    {
        AiaLogError( "Volume given out of range, given=%" PRIu8, newVolume );
        return false;
    }

    AiaMutex( Lock )( &speakerManager->mutex );

    if( !AiaSpeakerManager_ChangeVolumeLocked( speakerManager, newVolume ) )
    {
        AiaLogError( "AiaSpeakerManager_ChangeVolumeLocked failed" );
        AiaMutex( Unlock )( &speakerManager->mutex );
        return false;
    }

    AiaMutex( Unlock )( &speakerManager->mutex );
    return true;
}

bool AiaSpeakerManager_AdjustVolume( AiaSpeakerManager_t* speakerManager,
                                     int8_t delta )
{
    AiaAssert( speakerManager );
    if( !speakerManager )
    {
        AiaLogError( "Null speakerManager." );
        return false;
    }
    AiaMutex( Lock )( &speakerManager->mutex );

    int8_t newCurrentVolume = speakerManager->currentSpeakerState.currentVolume;
    if( speakerManager->currentSpeakerState.currentVolume + delta <
        AIA_MIN_VOLUME )
    {
        newCurrentVolume = AIA_MIN_VOLUME;
        delta = 0;
    }
    else if( speakerManager->currentSpeakerState.currentVolume + delta >
             AIA_MAX_VOLUME )
    {
        newCurrentVolume = AIA_MAX_VOLUME;
        delta = 0;
    }
    newCurrentVolume += delta;

    if( !AiaSpeakerManager_ChangeVolumeLocked( speakerManager,
                                               newCurrentVolume ) )
    {
        AiaLogError( "AiaSpeakerManager_ChangeVolumeLocked failed" );
        AiaMutex( Unlock )( &speakerManager->mutex );
        return false;
    }
    AiaMutex( Unlock )( &speakerManager->mutex );
    return true;
}

bool AiaSpeakerManager_CanSpeakerStream( AiaSpeakerManager_t* speakerManager )
{
    AiaAssert( speakerManager );
    if( !speakerManager )
    {
        AiaLogError( "Null speakerManager." );
        return false;
    }
    AiaMutex( Lock )( &speakerManager->mutex );

    /* These are the conditional flags showing if speaker is open and
     * streaming content. If the speaker is open; it is obviously streaming
     * content. Another conditonal variable to watch is @c pendingOpenSpeaker.
     * It indicates there is an OpenSpeaker directive to be processed; so, even
     * though the speaker might not be actively streaming, it will soon start
     * doing so. */
    bool speakerStatus = speakerManager->currentSpeakerState.isSpeakerOpen;
    bool speakerOpenPending =
        speakerManager->currentSpeakerState.pendingOpenSpeaker;

    AiaMutex( Unlock )( &speakerManager->mutex );
    return speakerStatus || speakerOpenPending;
}

static bool AiaSpeakerManager_CanSpeakerStreamLocked(
    AiaSpeakerManager_t* speakerManager )
{
    /* These are the conditional flags showing if speaker is open and
     * streaming content. If the speaker is open; it is obviously streaming
     * content. Another conditonal variable to watch is @c pendingOpenSpeaker.
     * It indicates there is an OpenSpeaker directive to be processed; so, even
     * though the speaker might not be actively streaming, it will soon start
     * doing so. */
    bool speakerStatus = speakerManager->currentSpeakerState.isSpeakerOpen;
    bool speakerOpenPending =
        speakerManager->currentSpeakerState.pendingOpenSpeaker;

    return speakerStatus || speakerOpenPending;
}

#ifdef AIA_ENABLE_ALERTS
void AiaSpeakerManager_PlayOfflineAlert( AiaSpeakerManager_t* speakerManager,
                                         const AiaAlertSlot_t* offlineAlert,
                                         uint8_t offlineAlertVolume )
{
    AiaAssert( speakerManager );
    if( !speakerManager )
    {
        AiaLogError( "Null speakerManager." );
        return;
    }
    if( !offlineAlert )
    {
        AiaLogError( "Null offlineAlert." );
        return;
    }
    AiaMutex( Lock )( &speakerManager->mutex );
    AiaSpeakerManager_PlayOfflineAlertLocked( speakerManager, offlineAlert,
                                              offlineAlertVolume );
    AiaMutex( Unlock )( &speakerManager->mutex );
}

static void AiaSpeakerManager_PlayOfflineAlertLocked(
    AiaSpeakerManager_t* speakerManager, const AiaAlertSlot_t* offlineAlert,
    uint8_t offlineAlertVolume )
{
    speakerManager->currentSpeakerState.alertToPlay =
        AiaCalloc( 1, sizeof( AiaAlertSlot_t ) );
    if( !speakerManager->currentSpeakerState.alertToPlay )
    {
        AiaLogError( "AiaCalloc failed, bytes=%zu.", sizeof( AiaAlertSlot_t ) );
        return;
    }
    memcpy( speakerManager->currentSpeakerState.alertToPlay, offlineAlert,
            sizeof( AiaAlertSlot_t ) );
    speakerManager->currentSpeakerState.shouldStartOfflineAlertPlayback = true;
    speakerManager->currentSpeakerState.offlineAlertVolume = offlineAlertVolume;
}

void AiaSpeakerManager_StopOfflineAlert( AiaSpeakerManager_t* speakerManager )
{
    AiaAssert( speakerManager );
    if( !speakerManager )
    {
        AiaLogError( "Null speakerManager." );
        return;
    }
    AiaMutex( Lock )( &speakerManager->mutex );
    AiaSpeakerManager_StopOfflineAlertLocked( speakerManager );
    AiaMutex( Unlock )( &speakerManager->mutex );
}

static void AiaSpeakerManager_StopOfflineAlertLocked(
    AiaSpeakerManager_t* speakerManager )
{
    speakerManager->currentSpeakerState.shouldStartOfflineAlertPlayback = false;
    if( speakerManager->currentSpeakerState.alertToPlay )
    {
        AiaFree( speakerManager->currentSpeakerState.alertToPlay );
        speakerManager->currentSpeakerState.alertToPlay = NULL;
    }
}
#endif

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
static int32_t AiaActionAtSpeakerOffsetComparator(
    const AiaListDouble( Link_t ) * const link1,
    const AiaListDouble( Link_t ) * const link2 )
{
    return ( (AiaSpeakerOffsetActionSlot_t*)link1 )->offset -
           ( (AiaSpeakerOffsetActionSlot_t*)link2 )->offset;
}

AiaSpeakerActionHandle_t AiaSpeakerManager_InvokeActionAtOffset(
    AiaSpeakerManager_t* speakerManager, AiaBinaryAudioStreamOffset_t offset,
    AiaActionAtSpeakerOffset_t action, void* userData )
{
    AiaAssert( speakerManager );
    if( !speakerManager )
    {
        AiaLogError( "Null speakerManager." );
        return AIA_INVALID_ACTION_ID;
    }
    if( !action )
    {
        AiaLogError( "Null action" );
        return AIA_INVALID_ACTION_ID;
    }

    AiaMutex( Lock )( &speakerManager->mutex );

    AiaSpeakerOffsetActionSlot_t* actionSlot =
        AiaCalloc( 1, sizeof( AiaSpeakerOffsetActionSlot_t ) );
    if( !actionSlot )
    {
        AiaLogError( "AiaCalloc failed, bytes=%zu",
                     sizeof( AiaSpeakerOffsetActionSlot_t ) );
        AiaMutex( Unlock )( &speakerManager->mutex );
        return AIA_INVALID_ACTION_ID;
    }
    AiaListDouble( Link_t ) defaultLink = AiaListDouble( LINK_INITIALIZER );
    actionSlot->link = defaultLink;
    actionSlot->offset = offset;
    actionSlot->action = action;
    actionSlot->userData = userData;
    AiaListDouble( InsertSorted )( &speakerManager->offsetActions,
                                   &actionSlot->link,
                                   AiaActionAtSpeakerOffsetComparator );
    AiaLogInfo( "Action with id=%p scheduled at offset=%" PRIu64, actionSlot,
                actionSlot->offset );
    AiaMutex( Unlock )( &speakerManager->mutex );
    return actionSlot;
}

void AiaSpeakerManager_CancelAction( AiaSpeakerManager_t* speakerManager,
                                     AiaSpeakerActionHandle_t handle )
{
    AiaAssert( speakerManager );
    if( !speakerManager )
    {
        AiaLogError( "Null speakerManager." );
        return;
    }
    if( !handle || handle == AIA_INVALID_ACTION_ID )
    {
        AiaLogError( "Invalid handle" );
        return;
    }

    AiaLogInfo( "AiaSpeakerManager_CancelAction, handle=%p", handle );
    AiaMutex( Lock )( &speakerManager->mutex );

    AiaSpeakerOffsetActionSlot_t* slot = (AiaSpeakerOffsetActionSlot_t*)handle;

    AiaListDouble( Remove )( &slot->link );
    AiaFree( slot );

    AiaMutex( Unlock )( &speakerManager->mutex );
}

AiaBinaryAudioStreamOffset_t AiaSpeakerManager_GetCurrentOffset(
    const AiaSpeakerManager_t* speakerManager )
{
    AiaAssert( speakerManager );
    if( !speakerManager )
    {
        AiaLogError( "Null speakerManager." );
        return 0;
    }
    return AiaDataStreamReader_Tell(
        speakerManager->speakerBufferReader,
        AIA_DATA_STREAM_BUFFER_READER_REFERENCE_ABSOLUTE );
}

static void AiaSpeakerManager_InvalidateActionsLocked(
    AiaSpeakerManager_t* speakerManager )
{
    AiaAssert( speakerManager );
    if( !speakerManager )
    {
        AiaLogDebug( "Null speakerManager." );
        return;
    }

    AiaListDouble( Link_t )* actionLink = NULL;
    while( ( actionLink =
                 AiaListDouble( PeekHead )( &speakerManager->offsetActions ) ) )
    {
        AiaSpeakerOffsetActionSlot_t* actionInfo =
            ( (AiaSpeakerOffsetActionSlot_t*)actionLink );
        AiaLogDebug( "Canceling action, offset=%" PRIu64, actionInfo->offset );
        actionInfo->action( false, actionInfo->userData );
        AiaListDouble( Remove )( actionLink );
        AiaFree( actionInfo );
    }
    actionLink = NULL;
}

static void AiaSpeakerManager_SetBufferStateLocked(
    AiaSpeakerManager_t* speakerManager,
    AiaSpeakerManagerBufferState_t newBufferState )
{
    AiaAssert( speakerManager );
    if( !speakerManager )
    {
        AiaLogDebug( "Null speakerManager." );
        return;
    }

    /* Notify the observers if only the speaker buffer state is being changed */
    if( speakerManager->notifyObserversCb &&
        ( speakerManager->currentSpeakerState.currentBufferState !=
          newBufferState ) )
    {
        speakerManager->notifyObserversCb(
            speakerManager->notifyObserversCbUserData, newBufferState );
    }

    AiaLogDebug( "Changing speaker manager buffer state from %s to %s",
                 AiaSpeakerManagerBufferState_ToString(
                     speakerManager->currentSpeakerState.currentBufferState ),
                 AiaSpeakerManagerBufferState_ToString( newBufferState ) );
    speakerManager->currentSpeakerState.currentBufferState = newBufferState;
}
