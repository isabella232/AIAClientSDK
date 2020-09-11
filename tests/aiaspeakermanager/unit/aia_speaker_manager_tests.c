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
 * @file aia_speaker_manager_tests.c
 * @brief Tests for AiaSpeakerManager_t.
 */

/* The config header is always included first. */
#include <aia_config.h>

/* Aia headers */
#include <aiacore/aia_binary_constants.h>
#include <aiacore/aia_events.h>
#include <aiacore/aia_json_constants.h>
#include <aiacore/aia_json_message.h>
#include <aiacore/aia_json_utils.h>
#include <aiacore/aia_mbedtls_threading.h>
#include <aiacore/aia_random_mbedtls.h>
#include <aiacore/aia_topic.h>
#include <aiacore/aia_volume_constants.h>
#include <aiaspeakermanager/aia_speaker_manager.h>
#include <aiaspeakermanager/private/aia_speaker_manager.h>

#include <aiamockregulator/aia_mock_regulator.h>
#include <aiamocksequencer/aia_mock_sequencer.h>
#include <aiatestutilities/aia_test_utilities.h>

#include AiaSemaphore( HEADER )

/* Test framework includes. */
#include <unity_fixture.h>

#include <inttypes.h>
#include <string.h>

/*-----------------------------------------------------------*/

/** Sample Seed. */
static const char* TEST_SALT = "TestSalt";
static const size_t TEST_SALT_LENGTH = sizeof( TEST_SALT ) - 1;

/**
 * @brief Test group for AiaSpeakerManager_t tests.
 */
TEST_GROUP( AiaSpeakerManagerTests );

/*-----------------------------------------------------------*/

typedef struct AiaSpeakerManagerTestObserver AiaSpeakerManagerTestObserver_t;
static AiaSpeakerManagerTestObserver_t* AiaSpeakerManagerTestObserver_Create();
static void AiaSpeakerManagerTestObserver_Destroy(
    AiaSpeakerManagerTestObserver_t* observer );
static bool PlaySpeakerDataCallback( const void* buf, size_t size,
                                     void* userData );
static void SetVolumeCallback( uint8_t volume, void* userData );
static bool PlayOfflineAlertCallback( const AiaAlertSlot_t* offlineAlert,
                                      void* userData );
static bool StopOfflineAlertCallback( void* userData );
static void NotifyObservers(
    void* userData, AiaSpeakerManagerBufferState_t currentBufferState );
static size_t TEST_BUFFER_SIZE = 500;
static size_t TEST_OVERRUN_WARNING_THRESHOLD = 350;
static size_t TEST_UNDERRUN_WARNING_THRESHOLD = 150;

/*-----------------------------------------------------------*/

static AiaSpeakerManagerTestObserver_t* g_observer;
static AiaMockSequencer_t* g_mockSequencer;
static AiaSequencer_t* g_sequencer; /* pre-casted copy of g_mockSequencer */
static AiaMockRegulator_t* g_mockRegulator;
static AiaRegulator_t* g_regulator; /* pre-casted copy of g_mockRegulator */
static AiaSpeakerManager_t* g_speakerManager;

/*-----------------------------------------------------------*/

/**
 * @brief Test setup for AiaSpeakerManager_t tests.
 */
TEST_SETUP( AiaSpeakerManagerTests )
{
    AiaMbedtlsThreading_Init();
    AiaRandomMbedtls_Init();
    TEST_ASSERT_TRUE( AiaRandomMbedtls_Seed( TEST_SALT, TEST_SALT_LENGTH ) );

    g_observer = AiaSpeakerManagerTestObserver_Create();
    TEST_ASSERT_TRUE( g_observer );
    g_mockSequencer = AiaMockSequencer_Create();
    TEST_ASSERT_TRUE( g_mockSequencer );
    g_sequencer = (AiaSequencer_t*)g_mockSequencer;
    g_mockRegulator = AiaMockRegulator_Create();
    TEST_ASSERT_TRUE( g_mockRegulator );
    g_regulator = (AiaRegulator_t*)g_mockRegulator;
    g_speakerManager = AiaSpeakerManager_Create(
        TEST_BUFFER_SIZE, TEST_OVERRUN_WARNING_THRESHOLD,
        TEST_UNDERRUN_WARNING_THRESHOLD, PlaySpeakerDataCallback, g_observer,
        g_sequencer, g_regulator, SetVolumeCallback, g_observer,
        PlayOfflineAlertCallback, g_observer, StopOfflineAlertCallback,
        g_observer, NotifyObservers, g_observer );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test tear down for AiaSpeakerManager_t tests.
 */
TEST_TEAR_DOWN( AiaSpeakerManagerTests )
{
    AiaSpeakerManager_Destroy( g_speakerManager );
    AiaMockRegulator_Destroy( g_mockRegulator,
                              AiaTestUtilities_DestroyJsonChunk, NULL );
    AiaMockSequencer_Destroy( g_mockSequencer );
    AiaSpeakerManagerTestObserver_Destroy( g_observer );

    AiaMbedtlsThreading_Cleanup();
    AiaRandomMbedtls_Cleanup();
}

/*-----------------------------------------------------------*/

/**
 * @brief Test group runner for AiaSpeakerManager_t tests.
 */
TEST_GROUP_RUNNER( AiaSpeakerManagerTests )
{
    RUN_TEST_CASE( AiaSpeakerManagerTests, Create );
    RUN_TEST_CASE( AiaSpeakerManagerTests, PlaybackStoppedWhenSpeakerNotOpen );
    RUN_TEST_CASE( AiaSpeakerManagerTests,
                   SpeakerOpensAtFirstOffsetWhenOffsetReceived );
    RUN_TEST_CASE( AiaSpeakerManagerTests,
                   SpeakerDoesNotOpenAtFutureOffsetIfOffsetNotReceived );
    RUN_TEST_CASE( AiaSpeakerManagerTests,
                   SpeakerDoesOpenAtFutureOffsetWhenOffsetEventuallyReceived );
    RUN_TEST_CASE( AiaSpeakerManagerTests,
                   BargeInWhenSpeakerOpenResultsInSpeakerClosed );
    RUN_TEST_CASE( AiaSpeakerManagerTests,
                   MarkersEchoedWhenOffsetsPlayedReachedBasic );
    RUN_TEST_CASE( AiaSpeakerManagerTests, BufferOverrunSentBasic );
    RUN_TEST_CASE( AiaSpeakerManagerTests, OverrunRepeated );
    RUN_TEST_CASE( AiaSpeakerManagerTests, UnderrunRepeated );
    RUN_TEST_CASE(
        AiaSpeakerManagerTests,
        CloseSpeakerResultsInSpeakerClosedAndNoSubsequentBufferEvents );
    RUN_TEST_CASE( AiaSpeakerManagerTests, NullPayloads );
    RUN_TEST_CASE( AiaSpeakerManagerTests,
                   SpeakerDataStopsPushingWhenRejected );
    RUN_TEST_CASE( AiaSpeakerManagerTests, PersistedVolumeIsInitialVolume );
    RUN_TEST_CASE( AiaSpeakerManagerTests,
                   TestLocalAbsoluteVolumeChangeOutsideRange );
    RUN_TEST_CASE( AiaSpeakerManagerTests,
                   TestLocalAbsoluteVolumeChangeWhenSpeakerNotOpen );
    RUN_TEST_CASE( AiaSpeakerManagerTests,
                   TestLocalAbsoluteVolumeChangeWhenSpeakerOpen );
    RUN_TEST_CASE(
        AiaSpeakerManagerTests,
        TestLocalRelativeVolumeIncrementChangeWhenSpeakerNotOpenWithinBounds );
    RUN_TEST_CASE(
        AiaSpeakerManagerTests,
        TestLocalRelativeVolumeDecrementChangeWhenSpeakerNotOpenWithinBounds );
    RUN_TEST_CASE(
        AiaSpeakerManagerTests,
        TestLocalRelativeVolumeIncrementChangeWhenSpeakerNotOpenOutsideBounds );
    RUN_TEST_CASE(
        AiaSpeakerManagerTests,
        TestLocalRelativeVolumeDecrementChangeWhenSpeakerNotOpenOutsideBounds );
    RUN_TEST_CASE( AiaSpeakerManagerTests, NoVolumeChangeResultsInNoEvent );
    RUN_TEST_CASE( AiaSpeakerManagerTests,
                   SetVolumeWithoutOffsetResultsInImmediateChange );
    RUN_TEST_CASE( AiaSpeakerManagerTests,
                   SetVolumeWithOffsetResultsInEventualChange );
    RUN_TEST_CASE( AiaSpeakerManagerTests,
                   SetMultipleVolumesWithFutureOffsetResultsInEventualChange );
    RUN_TEST_CASE( AiaSpeakerManagerTests, InvokeSingleActionAtCurrentOffset );
    RUN_TEST_CASE( AiaSpeakerManagerTests, InvokeSingleActionAtFutureOffset );
    RUN_TEST_CASE( AiaSpeakerManagerTests,
                   CanceledActionDoesNotResultInCallback );
    RUN_TEST_CASE( AiaSpeakerManagerTests,
                   InvokeMultipleActionsAtFutureOffset );
    RUN_TEST_CASE( AiaSpeakerManagerTests,
                   LocalStoppageResultsInActionInvalidation );
    RUN_TEST_CASE( AiaSpeakerManagerTests, MalformedSpeakerMessage );
    RUN_TEST_CASE( AiaSpeakerManagerTests,
                   BufferStateEventsNotSentWhenSpeakerClosed );
    RUN_TEST_CASE( AiaSpeakerManagerTests,
                   OlderSpeakerDataOverwrittenWhenSpeakerClosed );
    RUN_TEST_CASE( AiaSpeakerManagerTests, GetCurrentOffset );
#ifdef AIA_ENABLE_ALERTS
    RUN_TEST_CASE( AiaSpeakerManagerTests, OfflineAlertPlayback );
#endif
    RUN_TEST_CASE( AiaSpeakerManagerTests, CanSpeakerStream );
}

static const size_t DATA_OVERHEAD_SIZE =
    sizeof( AiaBinaryMessageLength_t ) + sizeof( AiaBinaryMessageType_t ) +
    sizeof( AiaBinaryMessageCount_t ) + AIA_BINARY_MESSAGE_NUM_RESERVED_BYTES;

static const uint8_t TEST_FRAME_1[] = { 0, 0, 0, 0 };

/** Callers must clean up non NULL returned values using AiaFree(). */
const char* generateOpenSpeaker( AiaBinaryAudioStreamOffset_t offset )
{
    char* buf = AiaCalloc( 50, sizeof( char ) );
    if( !buf )
    {
        return NULL;
    }
    sprintf( buf, "{\"" AIA_OPEN_SPEAKER_OFFSET_KEY "\" : %" PRIu64 "}",
             offset );
    return buf;
}

/** Callers must clean up non NULL returned values using AiaFree(). */
const char* generateCloseSpeaker( AiaBinaryAudioStreamOffset_t* offset )
{
    char* buf = AiaCalloc( 50, sizeof( char ) );
    if( !buf )
    {
        return NULL;
    }
    if( offset )
    {
        sprintf( buf, "{\"" AIA_CLOSE_SPEAKER_OFFSET_KEY "\" : %" PRIu64 "}",
                 *offset );
    }
    return buf;
}

/** Callers must clean up non NULL returned values using AiaFree(). */
char* generateSetVolumeWithoutOffset( AiaJsonLongType volume )
{
    static const char* formatPayload =
        /* clang-format off */
        "{"
            "\""AIA_SET_VOLUME_VOLUME_KEY"\":%"PRIu64
        "}";
    /* clang-format on */
    int numCharsRequired = snprintf( NULL, 0, formatPayload, volume );
    if( numCharsRequired < 0 )
    {
        AiaLogError( "snprintf failed, ret=%d", numCharsRequired );
        return NULL;
    }
    char* fullPayloadBuffer = AiaCalloc( 1, numCharsRequired + 1 );
    if( !fullPayloadBuffer )
    {
        AiaLogError( "AiaCalloc failed." );
        return NULL;
    }
    if( snprintf( fullPayloadBuffer, numCharsRequired + 1, formatPayload,
                  volume ) < 0 )
    {
        AiaLogError( "snprintf failed" );
        AiaFree( fullPayloadBuffer );
        return NULL;
    }

    return fullPayloadBuffer;
}

/** Callers must clean up non NULL returned values using AiaFree(). */
char* generateSetVolumeWithOffset( AiaJsonLongType volume,
                                   AiaJsonLongType offset )
{
    static const char* formatPayload =
        /* clang-format off */
        "{"
            "\""AIA_SET_VOLUME_VOLUME_KEY"\":%"PRIu64","
            "\""AIA_SET_VOLUME_OFFSET_KEY"\":%"PRIu64
        "}";
    /* clang-format on */
    int numCharsRequired = snprintf( NULL, 0, formatPayload, volume, offset );
    if( numCharsRequired < 0 )
    {
        AiaLogError( "snprintf failed, ret=%d", numCharsRequired );
        return NULL;
    }
    char* fullPayloadBuffer = AiaCalloc( 1, numCharsRequired + 1 );
    if( !fullPayloadBuffer )
    {
        AiaLogError( "AiaCalloc failed." );
        return NULL;
    }
    if( snprintf( fullPayloadBuffer, numCharsRequired + 1, formatPayload,
                  volume, offset ) < 0 )
    {
        AiaLogError( "snprintf failed" );
        AiaFree( fullPayloadBuffer );
        return NULL;
    }

    return fullPayloadBuffer;
}

/** Callers must clean up non NULL returned values using AiaFree(). */
const uint8_t* generateBinaryAudioMessageEntry(
    const uint8_t* audio, AiaBinaryMessageLength_t audioLength,
    AiaBinaryMessageCount_t count, AiaBinaryAudioStreamOffset_t offset,
    size_t* totalLength )
{
    uint8_t* buf =
        AiaCalloc( 1, DATA_OVERHEAD_SIZE + sizeof( offset ) + audioLength );
    if( !buf )
    {
        return NULL;
    }

    AiaBinaryMessageLength_t length = audioLength + sizeof( offset );

    size_t bytePosition = 0;
    for( size_t i = 0; i < sizeof( length ); ++i )
    {
        buf[ bytePosition ] = ( length >> ( i * 8 ) );
        ++bytePosition;
    }
    for( size_t i = 0; i < sizeof( AiaBinaryMessageType_t ); ++i )
    {
        buf[ bytePosition ] =
            ( AIA_BINARY_STREAM_SPEAKER_CONTENT_TYPE >> ( i * 8 ) );
        ++bytePosition;
    }
    for( size_t i = 0; i < sizeof( count ); ++i )
    {
        buf[ bytePosition ] = ( count >> ( i * 8 ) );
        ++bytePosition;
    }
    for( size_t i = 0; i < AIA_BINARY_MESSAGE_NUM_RESERVED_BYTES; ++i )
    {
        buf[ bytePosition ] = ( '0' );
        ++bytePosition;
    }
    for( size_t i = 0; i < sizeof( offset ); ++i )
    {
        buf[ bytePosition ] = ( offset >> ( i * 8 ) );
        ++bytePosition;
    }
    memcpy( buf + bytePosition, audio, audioLength );
    *totalLength = DATA_OVERHEAD_SIZE + sizeof( offset ) + audioLength;
    return buf;
}

/** Callers must clean up non NULL returned values using AiaFree(). */
const uint8_t* generateBinaryMarkerMessageEntry(
    AiaSpeakerBinaryMarker_t marker, size_t* totalLength )
{
    uint8_t* buf = AiaCalloc( 1, DATA_OVERHEAD_SIZE + sizeof( marker ) );
    if( !buf )
    {
        return NULL;
    }

    AiaBinaryMessageLength_t length = sizeof( marker );

    size_t bytePosition = 0;
    for( size_t i = 0; i < sizeof( length ); ++i )
    {
        buf[ bytePosition ] = ( length >> ( i * 8 ) );
        ++bytePosition;
    }
    for( size_t i = 0; i < sizeof( AiaBinaryMessageType_t ); ++i )
    {
        buf[ bytePosition ] =
            ( AIA_BINARY_STREAM_SPEAKER_MARKER_TYPE >> ( i * 8 ) );
        ++bytePosition;
    }
    for( size_t i = 0; i < sizeof( AiaBinaryMessageCount_t ); ++i )
    {
        buf[ bytePosition ] = ( 0 >> ( i * 8 ) );
        ++bytePosition;
    }
    for( size_t i = 0; i < AIA_BINARY_MESSAGE_NUM_RESERVED_BYTES; ++i )
    {
        buf[ bytePosition ] = ( '0' );
        ++bytePosition;
    }
    memcpy( buf + bytePosition, &marker, sizeof( marker ) );
    *totalLength = DATA_OVERHEAD_SIZE + sizeof( marker );
    return buf;
}

struct AiaSpeakerManagerTestObserver
{
    void* speakerDataReceived[ 1000 ];
    size_t speakerDataReceivedSize;
    /* TODO: Replace with mechanism that allows for running unit tests in
     * non-threaded environments. */
    AiaSemaphore_t numSpeakerFramesPushedSemaphore;

    /** Last volume received in callback. */
    uint8_t volume;
    /* TODO: Replace with mechanism that allows for running unit tests in
     * non-threaded environments. */
    /** Used to wait on asynchronous @c AiaSetVolume_t() notifications. */
    AiaSemaphore_t volumeSemaphore;

    /** Keeps track of the number of observer notifications */
    AiaSemaphore_t numObserversNotifiedSemaphore;
};

static AiaSpeakerManagerTestObserver_t* AiaSpeakerManagerTestObserver_Create()
{
    AiaSpeakerManagerTestObserver_t* observer =
        AiaCalloc( 1, sizeof( AiaSpeakerManagerTestObserver_t ) );
    if( !observer )
    {
        AiaLogError( "AiaCalloc failed." );
        return NULL;
    }
    if( !AiaSemaphore( Create )( &observer->numSpeakerFramesPushedSemaphore, 0,
                                 1000 ) )
    {
        AiaFree( observer );
        return NULL;
    }
    if( !AiaSemaphore( Create )( &observer->volumeSemaphore, 0, 1000 ) )
    {
        AiaSemaphore( Destroy )( &observer->numSpeakerFramesPushedSemaphore );
        AiaFree( observer );
        return NULL;
    }
    if( !AiaSemaphore( Create )( &observer->numObserversNotifiedSemaphore, 0,
                                 1000 ) )
    {
        AiaSemaphore( Destroy )( &observer->volumeSemaphore );
        AiaSemaphore( Destroy )( &observer->numSpeakerFramesPushedSemaphore );
        AiaFree( observer );
        return NULL;
    }
    return observer;
}

static void AiaSpeakerManagerTestObserver_Destroy(
    AiaSpeakerManagerTestObserver_t* observer )
{
    AiaSemaphore( Destroy )( &observer->volumeSemaphore );
    AiaSemaphore( Destroy )( &observer->numSpeakerFramesPushedSemaphore );
    AiaFree( observer );
}

typedef struct AiaTestActionObserver
{
    bool actionInvoked;

    AiaSemaphore_t actionInvokedSemaphore;
} AiaTestActionObserver_t;

static void TestInvokeAction( bool actionValid, void* userData )
{
    TEST_ASSERT_TRUE( userData );
    AiaTestActionObserver_t* observer = (AiaTestActionObserver_t*)userData;
    if( actionValid )
    {
        observer->actionInvoked = true;
        AiaSemaphore( Post )( &observer->actionInvokedSemaphore );
    }
}

static AiaTestActionObserver_t* AiaTestActionObserver_Create()
{
    AiaTestActionObserver_t* observer =
        AiaCalloc( 1, sizeof( AiaTestActionObserver_t ) );
    TEST_ASSERT_NOT_NULL( observer );
    if( !AiaSemaphore( Create )( &observer->actionInvokedSemaphore, 0, 1000 ) )
    {
        AiaFree( observer );
        return NULL;
    }
    return observer;
}

static void AiaTestActionObserver_Destroy( AiaTestActionObserver_t* observer )
{
    AiaSemaphore( Destroy )( &observer->actionInvokedSemaphore );
    AiaFree( observer );
}

static bool PlaySpeakerDataCallback( const void* buf, size_t size,
                                     void* userData )
{
    TEST_ASSERT_TRUE( userData );
    AiaSpeakerManagerTestObserver_t* observer =
        (AiaSpeakerManagerTestObserver_t*)userData;
    memcpy( observer->speakerDataReceived + observer->speakerDataReceivedSize,
            buf, size );
    observer->speakerDataReceivedSize += size;
    AiaSemaphore( Post )( &observer->numSpeakerFramesPushedSemaphore );
    return true;
}

static void SetVolumeCallback( uint8_t volume, void* userData )
{
    TEST_ASSERT_TRUE( userData );
    AiaSpeakerManagerTestObserver_t* observer =
        (AiaSpeakerManagerTestObserver_t*)userData;
    observer->volume = volume;
    AiaSemaphore( Post )( &observer->volumeSemaphore );
}

static bool PlayOfflineAlertCallback( const AiaAlertSlot_t* offlineAlert,
                                      void* userData )
{
    TEST_ASSERT_TRUE( userData );
    (void)offlineAlert;

    return true;
}

static bool StopOfflineAlertCallback( void* userData )
{
    TEST_ASSERT_TRUE( userData );

    return true;
}

static void NotifyObservers( void* userData,
                             AiaSpeakerManagerBufferState_t currentBufferState )
{
    TEST_ASSERT_TRUE( userData );
    (void)currentBufferState;
    AiaSpeakerManagerTestObserver_t* observer =
        (AiaSpeakerManagerTestObserver_t*)userData;
    AiaSemaphore( Post )( &observer->numObserversNotifiedSemaphore );
}

static uint8_t TEST_PERSISTED_VOLUME = AIA_MAX_VOLUME / 2;

uint8_t AiaLoadVolume()
{
    return TEST_PERSISTED_VOLUME;
}

TEST( AiaSpeakerManagerTests, Create )
{
    AiaSpeakerManager_t* speakerManager = AiaSpeakerManager_Create(
        TEST_BUFFER_SIZE, TEST_OVERRUN_WARNING_THRESHOLD,
        TEST_UNDERRUN_WARNING_THRESHOLD, NULL, g_observer, g_sequencer,
        g_regulator, SetVolumeCallback, g_observer, PlayOfflineAlertCallback,
        g_observer, StopOfflineAlertCallback, g_observer, NotifyObservers,
        g_observer );

    TEST_ASSERT_NULL( speakerManager );

    speakerManager = AiaSpeakerManager_Create(
        TEST_BUFFER_SIZE, TEST_OVERRUN_WARNING_THRESHOLD,
        TEST_UNDERRUN_WARNING_THRESHOLD, PlaySpeakerDataCallback, g_observer,
        NULL, g_regulator, SetVolumeCallback, g_observer,
        PlayOfflineAlertCallback, g_observer, StopOfflineAlertCallback,
        g_observer, NotifyObservers, g_observer );

    TEST_ASSERT_NULL( speakerManager );

    speakerManager = AiaSpeakerManager_Create(
        TEST_BUFFER_SIZE, TEST_OVERRUN_WARNING_THRESHOLD,
        TEST_UNDERRUN_WARNING_THRESHOLD, PlaySpeakerDataCallback, g_observer,
        g_sequencer, NULL, SetVolumeCallback, g_observer,
        PlayOfflineAlertCallback, g_observer, StopOfflineAlertCallback,
        g_observer, NotifyObservers, g_observer );

    TEST_ASSERT_NULL( speakerManager );

    speakerManager = AiaSpeakerManager_Create(
        TEST_BUFFER_SIZE, TEST_OVERRUN_WARNING_THRESHOLD,
        TEST_UNDERRUN_WARNING_THRESHOLD, PlaySpeakerDataCallback, g_observer,
        g_sequencer, g_regulator, NULL, g_observer, PlayOfflineAlertCallback,
        g_observer, StopOfflineAlertCallback, g_observer, NotifyObservers,
        g_observer );

    TEST_ASSERT_NULL( speakerManager );

    speakerManager = AiaSpeakerManager_Create(
        TEST_BUFFER_SIZE, TEST_OVERRUN_WARNING_THRESHOLD,
        TEST_UNDERRUN_WARNING_THRESHOLD, PlaySpeakerDataCallback, g_observer,
        g_sequencer, g_regulator, SetVolumeCallback, g_observer, NULL,
        g_observer, StopOfflineAlertCallback, g_observer, NotifyObservers,
        g_observer );

    TEST_ASSERT_NULL( speakerManager );

    speakerManager = AiaSpeakerManager_Create(
        TEST_BUFFER_SIZE, TEST_OVERRUN_WARNING_THRESHOLD,
        TEST_UNDERRUN_WARNING_THRESHOLD, PlaySpeakerDataCallback, g_observer,
        g_sequencer, g_regulator, SetVolumeCallback, g_observer,
        PlayOfflineAlertCallback, g_observer, NULL, g_observer, NotifyObservers,
        g_observer );

    TEST_ASSERT_NULL( speakerManager );

    speakerManager = AiaSpeakerManager_Create(
        TEST_BUFFER_SIZE, TEST_OVERRUN_WARNING_THRESHOLD,
        TEST_UNDERRUN_WARNING_THRESHOLD, PlaySpeakerDataCallback, g_observer,
        g_sequencer, g_regulator, SetVolumeCallback, g_observer,
        PlayOfflineAlertCallback, g_observer, StopOfflineAlertCallback,
        g_observer, NULL, g_observer );

    TEST_ASSERT_NOT_NULL( speakerManager );
}

TEST( AiaSpeakerManagerTests, PlaybackStoppedWhenSpeakerNotOpen )
{
    AiaSpeakerManager_StopPlayback( g_speakerManager );
    TEST_ASSERT_TRUE(
        AiaListDouble( IsEmpty )( &g_mockRegulator->writtenMessages ) );
}

TEST( AiaSpeakerManagerTests, SpeakerOpensAtFirstOffsetWhenOffsetReceived )
{
    static const AiaBinaryAudioStreamOffset_t TEST_OPEN_SPEAKER_OFFSET = 0;
    const char* openSpeakerPayload =
        generateOpenSpeaker( TEST_OPEN_SPEAKER_OFFSET );
    AiaSpeakerManager_OnOpenSpeakerDirectiveReceived(
        g_speakerManager, (void*)openSpeakerPayload,
        strlen( openSpeakerPayload ), 0, 0 );

    size_t binaryMessageLength = 0;
    const uint8_t* binaryMessage = generateBinaryAudioMessageEntry(
        TEST_FRAME_1, sizeof( TEST_FRAME_1 ), 0, TEST_OPEN_SPEAKER_OFFSET,
        &binaryMessageLength );
    AiaSpeakerManager_OnSpeakerTopicMessageReceived(
        g_speakerManager, binaryMessage, binaryMessageLength, 0 );

    size_t expectedFramesPushed = 1;
    while( expectedFramesPushed )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_observer->numSpeakerFramesPushedSemaphore, 100 ) );
        --expectedFramesPushed;
    }

    size_t expectedNumSpeakerOpenedEvents = 1;
    while( expectedNumSpeakerOpenedEvents )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_mockRegulator->writeSemaphore, 100 ) );
        AiaListDouble( Link_t )* link = NULL;
        link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
        AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
        TEST_ASSERT_TRUE( link );
        AiaJsonMessage_t* jsonMessage = AiaJsonMessage_FromMessage(
            ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
        TEST_ASSERT_EQUAL( strcmp( AiaJsonMessage_GetName( jsonMessage ),
                                   AIA_EVENTS_SPEAKER_OPENED ),
                           0 );
        const char* payload = AiaJsonMessage_GetJsonPayload( jsonMessage );
        const char* offset;
        TEST_ASSERT_TRUE( AiaFindJsonValue(
            payload, strlen( payload ), AIA_SPEAKER_OPENED_OFFSET_KEY,
            strlen( AIA_SPEAKER_OPENED_OFFSET_KEY ), &offset, NULL ) );
        TEST_ASSERT_EQUAL( atoi( offset ), TEST_OPEN_SPEAKER_OFFSET );
        --expectedNumSpeakerOpenedEvents;
    }

    TEST_ASSERT_TRUE(
        AiaListDouble( IsEmpty )( &g_mockRegulator->writtenMessages ) );
    TEST_ASSERT_FALSE( AiaSemaphore( TimedWait )(
        &g_mockSequencer->resetSequenceNumberSemaphore, 100 ) );

    AiaFree( (void*)binaryMessage );
    AiaFree( (void*)openSpeakerPayload );
}

TEST( AiaSpeakerManagerTests,
      SpeakerDoesNotOpenAtFutureOffsetIfOffsetNotReceived )
{
    static const AiaBinaryAudioStreamOffset_t TEST_OPEN_SPEAKER_OFFSET = 8;
    const char* openSpeakerPayload =
        generateOpenSpeaker( TEST_OPEN_SPEAKER_OFFSET );
    AiaSpeakerManager_OnOpenSpeakerDirectiveReceived(
        g_speakerManager, (void*)openSpeakerPayload,
        strlen( openSpeakerPayload ), 0, 0 );

    size_t binaryMessageLength = 0;
    static const AiaBinaryAudioStreamOffset_t AUDIO_STREAM_OFFSET_START = 0;
    const uint8_t* binaryMessage = generateBinaryAudioMessageEntry(
        TEST_FRAME_1, sizeof( TEST_FRAME_1 ), 0, AUDIO_STREAM_OFFSET_START,
        &binaryMessageLength );
    AiaSpeakerManager_OnSpeakerTopicMessageReceived(
        g_speakerManager, binaryMessage, binaryMessageLength, 0 );

    size_t expectedNumUnderruns = 1;
    while( expectedNumUnderruns )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_mockRegulator->writeSemaphore, 100 ) );
        AiaListDouble( Link_t )* link = NULL;
        link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
        AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
        TEST_ASSERT_TRUE( link );
        AiaJsonMessage_t* jsonMessage = AiaJsonMessage_FromMessage(
            ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
        TEST_ASSERT_EQUAL( strcmp( AiaJsonMessage_GetName( jsonMessage ),
                                   AIA_EVENTS_BUFFER_STATE_CHANGED ),
                           0 );
        TEST_ASSERT_TRUE( strstr(
            AiaJsonMessage_GetJsonPayload( jsonMessage ),
            AiaSpeakerManagerBufferState_ToString( AIA_UNDERRUN_STATE ) ) );
        --expectedNumUnderruns;
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_observer->numObserversNotifiedSemaphore, 100 ) );
    }

    TEST_ASSERT_FALSE( AiaSemaphore( TimedWait )(
        &g_observer->numSpeakerFramesPushedSemaphore, 100 ) );
    TEST_ASSERT_FALSE( AiaSemaphore( TimedWait )(
        &g_mockSequencer->resetSequenceNumberSemaphore, 100 ) );

    TEST_ASSERT_TRUE(
        AiaListDouble( IsEmpty )( &g_mockRegulator->writtenMessages ) );

    AiaFree( (void*)binaryMessage );
    AiaFree( (void*)openSpeakerPayload );
}

TEST( AiaSpeakerManagerTests,
      SpeakerDoesOpenAtFutureOffsetWhenOffsetEventuallyReceived )
{
    static const AiaBinaryAudioStreamOffset_t TEST_OPEN_SPEAKER_OFFSET = 4;
    const char* openSpeakerPayload =
        generateOpenSpeaker( TEST_OPEN_SPEAKER_OFFSET );
    AiaSpeakerManager_OnOpenSpeakerDirectiveReceived(
        g_speakerManager, (void*)openSpeakerPayload,
        strlen( openSpeakerPayload ), 0, 0 );

    size_t binaryMessageLength = 0;
    static const AiaBinaryAudioStreamOffset_t AUDIO_STREAM_OFFSET_START = 0;
    const uint8_t* binaryMessage = generateBinaryAudioMessageEntry(
        TEST_FRAME_1, sizeof( TEST_FRAME_1 ), 0, AUDIO_STREAM_OFFSET_START,
        &binaryMessageLength );
    AiaSpeakerManager_OnSpeakerTopicMessageReceived(
        g_speakerManager, binaryMessage, binaryMessageLength, 0 );

    size_t expectedNumUnderruns = 1;
    while( expectedNumUnderruns )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_mockRegulator->writeSemaphore, 100 ) );
        AiaListDouble( Link_t )* link = NULL;
        link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
        AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
        TEST_ASSERT_TRUE( link );
        AiaJsonMessage_t* jsonMessage = AiaJsonMessage_FromMessage(
            ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
        TEST_ASSERT_EQUAL( strcmp( AiaJsonMessage_GetName( jsonMessage ),
                                   AIA_EVENTS_BUFFER_STATE_CHANGED ),
                           0 );
        TEST_ASSERT_TRUE( strstr(
            AiaJsonMessage_GetJsonPayload( jsonMessage ),
            AiaSpeakerManagerBufferState_ToString( AIA_UNDERRUN_STATE ) ) );
        --expectedNumUnderruns;
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_observer->numObserversNotifiedSemaphore, 100 ) );
    }

    TEST_ASSERT_FALSE( AiaSemaphore( TimedWait )(
        &g_observer->numSpeakerFramesPushedSemaphore, 100 ) );
    TEST_ASSERT_FALSE( AiaSemaphore( TimedWait )(
        &g_mockSequencer->resetSequenceNumberSemaphore, 100 ) );

    const uint8_t* binaryMessage2 = generateBinaryAudioMessageEntry(
        TEST_FRAME_1, sizeof( TEST_FRAME_1 ), 0, TEST_OPEN_SPEAKER_OFFSET,
        &binaryMessageLength );
    AiaSpeakerManager_OnSpeakerTopicMessageReceived(
        g_speakerManager, binaryMessage2, binaryMessageLength, 0 );

    size_t expectedFramesPushed = 1;
    while( expectedFramesPushed )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_observer->numSpeakerFramesPushedSemaphore, 100 ) );
        --expectedFramesPushed;
    }

    size_t expectedNumSpeakerOpenedEvents = 1;
    while( expectedNumSpeakerOpenedEvents )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_mockRegulator->writeSemaphore, 100 ) );
        AiaListDouble( Link_t )* link = NULL;
        link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
        AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
        TEST_ASSERT_TRUE( link );
        AiaJsonMessage_t* jsonMessage = AiaJsonMessage_FromMessage(
            ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
        TEST_ASSERT_EQUAL( strcmp( AiaJsonMessage_GetName( jsonMessage ),
                                   AIA_EVENTS_SPEAKER_OPENED ),
                           0 );
        const char* payload = AiaJsonMessage_GetJsonPayload( jsonMessage );
        const char* offset;
        TEST_ASSERT_TRUE( AiaFindJsonValue(
            payload, strlen( payload ), AIA_SPEAKER_OPENED_OFFSET_KEY,
            strlen( AIA_SPEAKER_OPENED_OFFSET_KEY ), &offset, NULL ) );
        TEST_ASSERT_EQUAL( atoi( offset ), TEST_OPEN_SPEAKER_OFFSET );
        --expectedNumSpeakerOpenedEvents;
    }

    TEST_ASSERT_FALSE( AiaSemaphore( TimedWait )(
        &g_mockSequencer->resetSequenceNumberSemaphore, 100 ) );

    AiaFree( (void*)binaryMessage2 );
    AiaFree( (void*)binaryMessage );
    AiaFree( (void*)openSpeakerPayload );
}

TEST( AiaSpeakerManagerTests, BargeInWhenSpeakerOpenResultsInSpeakerClosed )
{
    static const AiaBinaryAudioStreamOffset_t TEST_OPEN_SPEAKER_OFFSET = 0;
    const char* openSpeakerPayload =
        generateOpenSpeaker( TEST_OPEN_SPEAKER_OFFSET );
    AiaSpeakerManager_OnOpenSpeakerDirectiveReceived(
        g_speakerManager, (void*)openSpeakerPayload,
        strlen( openSpeakerPayload ), 0, 0 );

    size_t binaryMessageLength = 0;
    const uint8_t* binaryMessage = generateBinaryAudioMessageEntry(
        TEST_FRAME_1, sizeof( TEST_FRAME_1 ), 0, TEST_OPEN_SPEAKER_OFFSET,
        &binaryMessageLength );
    AiaSpeakerManager_OnSpeakerTopicMessageReceived(
        g_speakerManager, binaryMessage, binaryMessageLength, 0 );

    size_t expectedFramesPushed = 1;
    while( expectedFramesPushed )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_observer->numSpeakerFramesPushedSemaphore, 100 ) );
        --expectedFramesPushed;
    }

    size_t expectedNumSpeakerOpenedEvents = 1;
    while( expectedNumSpeakerOpenedEvents )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_mockRegulator->writeSemaphore, 100 ) );
        AiaListDouble( Link_t )* link = NULL;
        link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
        AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
        TEST_ASSERT_TRUE( link );
        AiaJsonMessage_t* jsonMessage = AiaJsonMessage_FromMessage(
            ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
        TEST_ASSERT_EQUAL( strcmp( AiaJsonMessage_GetName( jsonMessage ),
                                   AIA_EVENTS_SPEAKER_OPENED ),
                           0 );
        const char* payload = AiaJsonMessage_GetJsonPayload( jsonMessage );
        const char* offset;
        TEST_ASSERT_TRUE( AiaFindJsonValue(
            payload, strlen( payload ), AIA_SPEAKER_OPENED_OFFSET_KEY,
            strlen( AIA_SPEAKER_OPENED_OFFSET_KEY ), &offset, NULL ) );
        TEST_ASSERT_EQUAL( atoi( offset ), TEST_OPEN_SPEAKER_OFFSET );
        --expectedNumSpeakerOpenedEvents;
    }

    size_t expectedNumUnderruns = 1;
    while( expectedNumUnderruns )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_mockRegulator->writeSemaphore, 100 ) );
        AiaListDouble( Link_t )* link = NULL;
        link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
        AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
        TEST_ASSERT_TRUE( link );
        AiaJsonMessage_t* jsonMessage = AiaJsonMessage_FromMessage(
            ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
        TEST_ASSERT_EQUAL( strcmp( AiaJsonMessage_GetName( jsonMessage ),
                                   AIA_EVENTS_BUFFER_STATE_CHANGED ),
                           0 );
        TEST_ASSERT_TRUE( strstr(
            AiaJsonMessage_GetJsonPayload( jsonMessage ),
            AiaSpeakerManagerBufferState_ToString( AIA_UNDERRUN_STATE ) ) );
        --expectedNumUnderruns;
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_observer->numObserversNotifiedSemaphore, 100 ) );
    }

    TEST_ASSERT_FALSE( AiaSemaphore( TimedWait )(
        &g_mockSequencer->resetSequenceNumberSemaphore, 100 ) );

    AiaSpeakerManager_StopPlayback( g_speakerManager );
    size_t expectedNumSpeakerClosedEvents = 1;
    while( expectedNumSpeakerClosedEvents )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_mockRegulator->writeSemaphore, 100 ) );
        AiaListDouble( Link_t )* link = NULL;
        link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
        AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
        TEST_ASSERT_TRUE( link );
        AiaJsonMessage_t* jsonMessage = AiaJsonMessage_FromMessage(
            ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
        TEST_ASSERT_EQUAL( strcmp( AiaJsonMessage_GetName( jsonMessage ),
                                   AIA_EVENTS_SPEAKER_CLOSED ),
                           0 );
        const char* payload = AiaJsonMessage_GetJsonPayload( jsonMessage );
        const char* offset;
        TEST_ASSERT_TRUE( AiaFindJsonValue(
            payload, strlen( payload ), AIA_SPEAKER_CLOSED_OFFSET_KEY,
            strlen( AIA_SPEAKER_CLOSED_OFFSET_KEY ), &offset, NULL ) );
        TEST_ASSERT_EQUAL( atoi( offset ),
                           TEST_OPEN_SPEAKER_OFFSET + sizeof( TEST_FRAME_1 ) );
        --expectedNumSpeakerClosedEvents;
    }

    TEST_ASSERT_TRUE(
        AiaListDouble( IsEmpty )( &g_mockRegulator->writtenMessages ) );

    TEST_ASSERT_FALSE( AiaSemaphore( TimedWait )(
        &g_mockSequencer->resetSequenceNumberSemaphore, 100 ) );

    AiaFree( (void*)binaryMessage );
    AiaFree( (void*)openSpeakerPayload );
}

TEST( AiaSpeakerManagerTests, MarkersEchoedWhenOffsetsPlayedReachedBasic )
{
    static const AiaBinaryAudioStreamOffset_t TEST_OPEN_SPEAKER_OFFSET = 0;
    const char* openSpeakerPayload =
        generateOpenSpeaker( TEST_OPEN_SPEAKER_OFFSET );
    AiaSpeakerManager_OnOpenSpeakerDirectiveReceived(
        g_speakerManager, (void*)openSpeakerPayload,
        strlen( openSpeakerPayload ), 0, 0 );

    size_t markerMessageLength = 0;
    static const AiaSpeakerBinaryMarker_t TEST_MARKER_TYPE_1 = 2;
    const uint8_t* markerMessage = generateBinaryMarkerMessageEntry(
        TEST_MARKER_TYPE_1, &markerMessageLength );
    AiaSpeakerManager_OnSpeakerTopicMessageReceived(
        g_speakerManager, markerMessage, markerMessageLength, 0 );

    size_t binaryMessageLength = 0;
    const uint8_t* binaryMessage = generateBinaryAudioMessageEntry(
        TEST_FRAME_1, sizeof( TEST_FRAME_1 ), 0, TEST_OPEN_SPEAKER_OFFSET,
        &binaryMessageLength );
    AiaSpeakerManager_OnSpeakerTopicMessageReceived(
        g_speakerManager, binaryMessage, binaryMessageLength, 0 );

    size_t expectedFramesPushed = 1;
    while( expectedFramesPushed )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_observer->numSpeakerFramesPushedSemaphore, 100 ) );
        --expectedFramesPushed;
    }

    size_t expectedNumSpeakerOpenedEvents = 1;
    while( expectedNumSpeakerOpenedEvents )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_mockRegulator->writeSemaphore, 100 ) );
        AiaListDouble( Link_t )* link = NULL;
        link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
        AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
        TEST_ASSERT_TRUE( link );
        AiaJsonMessage_t* jsonMessage = AiaJsonMessage_FromMessage(
            ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
        TEST_ASSERT_EQUAL( strcmp( AiaJsonMessage_GetName( jsonMessage ),
                                   AIA_EVENTS_SPEAKER_OPENED ),
                           0 );
        const char* payload = AiaJsonMessage_GetJsonPayload( jsonMessage );
        const char* offset;
        TEST_ASSERT_TRUE( AiaFindJsonValue(
            payload, strlen( payload ), AIA_SPEAKER_OPENED_OFFSET_KEY,
            strlen( AIA_SPEAKER_OPENED_OFFSET_KEY ), &offset, NULL ) );
        TEST_ASSERT_EQUAL( atoi( offset ), TEST_OPEN_SPEAKER_OFFSET );
        --expectedNumSpeakerOpenedEvents;
    }

    size_t expectedNumMarkersEncountered = 1;
    while( expectedNumMarkersEncountered )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_mockRegulator->writeSemaphore, 100 ) );
        AiaListDouble( Link_t )* link = NULL;
        link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
        AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
        TEST_ASSERT_TRUE( link );
        AiaJsonMessage_t* jsonMessage = AiaJsonMessage_FromMessage(
            ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
        TEST_ASSERT_EQUAL( strcmp( AiaJsonMessage_GetName( jsonMessage ),
                                   AIA_EVENTS_SPEAKER_MARKER_ENCOUNTERED ),
                           0 );
        const char* payload = AiaJsonMessage_GetJsonPayload( jsonMessage );
        const char* marker;
        TEST_ASSERT_TRUE( AiaFindJsonValue(
            payload, strlen( payload ),
            AIA_SPEAKER_MARKER_ENCOUNTERED_MARKER_KEY,
            strlen( AIA_SPEAKER_MARKER_ENCOUNTERED_MARKER_KEY ), &marker,
            NULL ) );
        TEST_ASSERT_EQUAL( atoi( marker ), TEST_MARKER_TYPE_1 );
        --expectedNumMarkersEncountered;
    }

    TEST_ASSERT_FALSE( AiaSemaphore( TimedWait )(
        &g_mockSequencer->resetSequenceNumberSemaphore, 100 ) );

    AiaFree( (void*)markerMessage );
    AiaFree( (void*)binaryMessage );
    AiaFree( (void*)openSpeakerPayload );
}

TEST( AiaSpeakerManagerTests, BufferOverrunSentBasic )
{
    static const AiaBinaryAudioStreamOffset_t TEST_OPEN_SPEAKER_OFFSET = 0;
    const char* openSpeakerPayload =
        generateOpenSpeaker( TEST_OPEN_SPEAKER_OFFSET );
    AiaSpeakerManager_OnOpenSpeakerDirectiveReceived(
        g_speakerManager, (void*)openSpeakerPayload,
        strlen( openSpeakerPayload ), 0, 0 );
    AiaFree( (void*)openSpeakerPayload );

    AiaSequenceNumber_t speakerSequenceNumberToSend = 0;
    const uint8_t LARGE_TEST_FRAME[ TEST_BUFFER_SIZE ];

    AiaBinaryAudioStreamOffset_t offsetToSend = TEST_OPEN_SPEAKER_OFFSET;

    size_t binaryMessageLength = 0;
    const uint8_t* binaryMessage = generateBinaryAudioMessageEntry(
        LARGE_TEST_FRAME, sizeof( LARGE_TEST_FRAME ), 0, offsetToSend,
        &binaryMessageLength );
    AiaSpeakerManager_OnSpeakerTopicMessageReceived(
        g_speakerManager, binaryMessage, binaryMessageLength,
        speakerSequenceNumberToSend++ );
    offsetToSend += sizeof( LARGE_TEST_FRAME );
    AiaFree( (void*)binaryMessage );

    /* First, ensure that the speaker is open so that buffer state changed
       messages are sent. */
    size_t expectedNumSpeakerOpenedEvents = 1;
    while( expectedNumSpeakerOpenedEvents )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_mockRegulator->writeSemaphore, 100 ) );
        AiaListDouble( Link_t )* link = NULL;
        link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
        AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
        TEST_ASSERT_TRUE( link );
        AiaJsonMessage_t* jsonMessage = AiaJsonMessage_FromMessage(
            ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
        TEST_ASSERT_EQUAL( strcmp( AiaJsonMessage_GetName( jsonMessage ),
                                   AIA_EVENTS_SPEAKER_OPENED ),
                           0 );
        const char* payload = AiaJsonMessage_GetJsonPayload( jsonMessage );
        const char* offset;
        TEST_ASSERT_TRUE( AiaFindJsonValue(
            payload, strlen( payload ), AIA_SPEAKER_OPENED_OFFSET_KEY,
            strlen( AIA_SPEAKER_OPENED_OFFSET_KEY ), &offset, NULL ) );
        TEST_ASSERT_EQUAL( atoi( offset ), TEST_OPEN_SPEAKER_OFFSET );
        --expectedNumSpeakerOpenedEvents;
    }

    size_t expectedFramesPushed = TEST_BUFFER_SIZE / sizeof( LARGE_TEST_FRAME );
    while( expectedFramesPushed )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_observer->numSpeakerFramesPushedSemaphore, 100 ) );
        --expectedFramesPushed;
    }

    size_t iterationsToOverflowBuffer =
        ( TEST_BUFFER_SIZE / sizeof( LARGE_TEST_FRAME ) ) + 1;
    while( iterationsToOverflowBuffer )
    {
        size_t binaryMessageLength2 = 0;
        const uint8_t* binaryMessage2 = generateBinaryAudioMessageEntry(
            LARGE_TEST_FRAME, sizeof( LARGE_TEST_FRAME ), 0, offsetToSend,
            &binaryMessageLength2 );
        AiaSpeakerManager_OnSpeakerTopicMessageReceived(
            g_speakerManager, binaryMessage2, binaryMessageLength2,
            speakerSequenceNumberToSend++ );
        offsetToSend += sizeof( LARGE_TEST_FRAME );
        AiaFree( (void*)binaryMessage2 );
        --iterationsToOverflowBuffer;
    }

    size_t numExpectedOverrunWarnings = 1;
    while( numExpectedOverrunWarnings )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_mockRegulator->writeSemaphore, 100 ) );
        AiaListDouble( Link_t )* link = NULL;
        link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
        AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
        TEST_ASSERT_TRUE( link );
        AiaJsonMessage_t* jsonMessage = AiaJsonMessage_FromMessage(
            ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
        TEST_ASSERT_EQUAL( strcmp( AiaJsonMessage_GetName( jsonMessage ),
                                   AIA_EVENTS_BUFFER_STATE_CHANGED ),
                           0 );
        TEST_ASSERT_TRUE( strstr( AiaJsonMessage_GetJsonPayload( jsonMessage ),
                                  AiaSpeakerManagerBufferState_ToString(
                                      AIA_OVERRUN_WARNING_STATE ) ) );
        --numExpectedOverrunWarnings;
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_observer->numObserversNotifiedSemaphore, 100 ) );
    }

    size_t numExpectedOverruns = 1;
    while( numExpectedOverruns )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_mockRegulator->writeSemaphore, 100 ) );
        AiaListDouble( Link_t )* link = NULL;
        link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
        AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
        TEST_ASSERT_TRUE( link );
        AiaJsonMessage_t* jsonMessage = AiaJsonMessage_FromMessage(
            ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
        TEST_ASSERT_EQUAL( strcmp( AiaJsonMessage_GetName( jsonMessage ),
                                   AIA_EVENTS_BUFFER_STATE_CHANGED ),
                           0 );
        TEST_ASSERT_TRUE( strstr(
            AiaJsonMessage_GetJsonPayload( jsonMessage ),
            AiaSpeakerManagerBufferState_ToString( AIA_OVERRUN_STATE ) ) );
        --numExpectedOverruns;
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_observer->numObserversNotifiedSemaphore, 100 ) );
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_mockSequencer->resetSequenceNumberSemaphore, 100 ) );
        TEST_ASSERT_EQUAL( g_mockSequencer->currentSequenceNumber,
                           speakerSequenceNumberToSend - 1 );
    }

    expectedFramesPushed = TEST_BUFFER_SIZE / sizeof( LARGE_TEST_FRAME );
    while( expectedFramesPushed )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_observer->numSpeakerFramesPushedSemaphore, 100 ) );
        --expectedFramesPushed;
    }
}

TEST( AiaSpeakerManagerTests, OverrunRepeated )
{
    static const AiaBinaryAudioStreamOffset_t TEST_OPEN_SPEAKER_OFFSET = 0;
    const char* openSpeakerPayload =
        generateOpenSpeaker( TEST_OPEN_SPEAKER_OFFSET );
    AiaSpeakerManager_OnOpenSpeakerDirectiveReceived(
        g_speakerManager, (void*)openSpeakerPayload,
        strlen( openSpeakerPayload ), 0, 0 );
    AiaFree( (void*)openSpeakerPayload );

    AiaSequenceNumber_t speakerSequenceNumberToSend = 0;
    const uint8_t LARGE_TEST_FRAME[ TEST_BUFFER_SIZE ];

    AiaBinaryAudioStreamOffset_t offsetToSend = TEST_OPEN_SPEAKER_OFFSET;

    size_t binaryMessageLength = 0;
    const uint8_t* binaryMessage = generateBinaryAudioMessageEntry(
        LARGE_TEST_FRAME, sizeof( LARGE_TEST_FRAME ), 0, offsetToSend,
        &binaryMessageLength );
    AiaSpeakerManager_OnSpeakerTopicMessageReceived(
        g_speakerManager, binaryMessage, binaryMessageLength,
        speakerSequenceNumberToSend++ );
    offsetToSend += sizeof( LARGE_TEST_FRAME );
    AiaFree( (void*)binaryMessage );

    /* First, ensure that the speaker is open so that buffer state changed
       messages are sent. */
    size_t expectedNumSpeakerOpenedEvents = 1;
    while( expectedNumSpeakerOpenedEvents )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_mockRegulator->writeSemaphore, 100 ) );
        AiaListDouble( Link_t )* link = NULL;
        link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
        AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
        TEST_ASSERT_TRUE( link );
        AiaJsonMessage_t* jsonMessage = AiaJsonMessage_FromMessage(
            ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
        TEST_ASSERT_EQUAL( strcmp( AiaJsonMessage_GetName( jsonMessage ),
                                   AIA_EVENTS_SPEAKER_OPENED ),
                           0 );
        const char* payload = AiaJsonMessage_GetJsonPayload( jsonMessage );
        const char* offset;
        TEST_ASSERT_TRUE( AiaFindJsonValue(
            payload, strlen( payload ), AIA_SPEAKER_OPENED_OFFSET_KEY,
            strlen( AIA_SPEAKER_OPENED_OFFSET_KEY ), &offset, NULL ) );
        TEST_ASSERT_EQUAL( atoi( offset ), TEST_OPEN_SPEAKER_OFFSET );
        --expectedNumSpeakerOpenedEvents;
    }

    size_t expectedFramesPushed = TEST_BUFFER_SIZE / sizeof( LARGE_TEST_FRAME );
    while( expectedFramesPushed )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_observer->numSpeakerFramesPushedSemaphore, 100 ) );
        --expectedFramesPushed;
    }

    size_t iterationsToOverflowBuffer =
        ( TEST_BUFFER_SIZE / sizeof( LARGE_TEST_FRAME ) ) + 1;
    while( iterationsToOverflowBuffer )
    {
        size_t binaryMessageLength2 = 0;
        const uint8_t* binaryMessage2 = generateBinaryAudioMessageEntry(
            LARGE_TEST_FRAME, sizeof( LARGE_TEST_FRAME ), 0, offsetToSend,
            &binaryMessageLength2 );
        AiaSpeakerManager_OnSpeakerTopicMessageReceived(
            g_speakerManager, binaryMessage2, binaryMessageLength2,
            speakerSequenceNumberToSend++ );
        offsetToSend += sizeof( LARGE_TEST_FRAME );
        AiaFree( (void*)binaryMessage2 );
        --iterationsToOverflowBuffer;
    }

    size_t numExpectedOverrunWarnings = 1;
    while( numExpectedOverrunWarnings )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_mockRegulator->writeSemaphore, 100 ) );
        AiaListDouble( Link_t )* link = NULL;
        link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
        AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
        TEST_ASSERT_TRUE( link );
        AiaJsonMessage_t* jsonMessage = AiaJsonMessage_FromMessage(
            ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
        TEST_ASSERT_EQUAL( strcmp( AiaJsonMessage_GetName( jsonMessage ),
                                   AIA_EVENTS_BUFFER_STATE_CHANGED ),
                           0 );
        TEST_ASSERT_TRUE( strstr( AiaJsonMessage_GetJsonPayload( jsonMessage ),
                                  AiaSpeakerManagerBufferState_ToString(
                                      AIA_OVERRUN_WARNING_STATE ) ) );
        --numExpectedOverrunWarnings;
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_observer->numObserversNotifiedSemaphore, 100 ) );
    }

    size_t numExpectedOverruns = 1;
    while( numExpectedOverruns )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_mockRegulator->writeSemaphore, 100 ) );
        AiaListDouble( Link_t )* link = NULL;
        link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
        AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
        TEST_ASSERT_TRUE( link );
        AiaJsonMessage_t* jsonMessage = AiaJsonMessage_FromMessage(
            ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
        TEST_ASSERT_EQUAL( strcmp( AiaJsonMessage_GetName( jsonMessage ),
                                   AIA_EVENTS_BUFFER_STATE_CHANGED ),
                           0 );
        TEST_ASSERT_TRUE( strstr(
            AiaJsonMessage_GetJsonPayload( jsonMessage ),
            AiaSpeakerManagerBufferState_ToString( AIA_OVERRUN_STATE ) ) );
        --numExpectedOverruns;
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_observer->numObserversNotifiedSemaphore, 100 ) );
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_mockSequencer->resetSequenceNumberSemaphore, 100 ) );
        TEST_ASSERT_EQUAL( g_mockSequencer->currentSequenceNumber,
                           speakerSequenceNumberToSend - 1 );
        speakerSequenceNumberToSend = speakerSequenceNumberToSend - 1;
        offsetToSend -= sizeof( LARGE_TEST_FRAME );
    }

    expectedFramesPushed = TEST_BUFFER_SIZE / sizeof( LARGE_TEST_FRAME );
    while( expectedFramesPushed )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_observer->numSpeakerFramesPushedSemaphore, 100 ) );
        --expectedFramesPushed;
    }

    size_t expectedNumUnderruns = 1;
    while( expectedNumUnderruns )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_mockRegulator->writeSemaphore, 100 ) );
        AiaListDouble( Link_t )* link = NULL;
        link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
        AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
        TEST_ASSERT_TRUE( link );
        AiaJsonMessage_t* jsonMessage = AiaJsonMessage_FromMessage(
            ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
        TEST_ASSERT_EQUAL( strcmp( AiaJsonMessage_GetName( jsonMessage ),
                                   AIA_EVENTS_BUFFER_STATE_CHANGED ),
                           0 );
        TEST_ASSERT_TRUE( strstr(
            AiaJsonMessage_GetJsonPayload( jsonMessage ),
            AiaSpeakerManagerBufferState_ToString( AIA_UNDERRUN_STATE ) ) );
        --expectedNumUnderruns;
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_observer->numObserversNotifiedSemaphore, 100 ) );
    }

    TEST_ASSERT_TRUE(
        AiaListDouble( IsEmpty )( &g_mockRegulator->writtenMessages ) );

    /* Expect that the speaker manager will go back into an overrun state. */
    iterationsToOverflowBuffer =
        ( TEST_BUFFER_SIZE / sizeof( LARGE_TEST_FRAME ) ) + 1;
    while( iterationsToOverflowBuffer )
    {
        size_t binaryMessageLength2 = 0;
        const uint8_t* binaryMessage2 = generateBinaryAudioMessageEntry(
            LARGE_TEST_FRAME, sizeof( LARGE_TEST_FRAME ), 0, offsetToSend,
            &binaryMessageLength2 );
        AiaSpeakerManager_OnSpeakerTopicMessageReceived(
            g_speakerManager, binaryMessage2, binaryMessageLength2,
            speakerSequenceNumberToSend++ );
        offsetToSend += sizeof( LARGE_TEST_FRAME );
        AiaFree( (void*)binaryMessage2 );
        --iterationsToOverflowBuffer;
    }

    numExpectedOverruns = 1;
    while( numExpectedOverruns )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_mockRegulator->writeSemaphore, 100 ) );
        AiaListDouble( Link_t )* link = NULL;
        link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
        AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
        TEST_ASSERT_TRUE( link );
        AiaJsonMessage_t* jsonMessage = AiaJsonMessage_FromMessage(
            ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
        TEST_ASSERT_EQUAL( strcmp( AiaJsonMessage_GetName( jsonMessage ),
                                   AIA_EVENTS_BUFFER_STATE_CHANGED ),
                           0 );
        TEST_ASSERT_TRUE( strstr(
            AiaJsonMessage_GetJsonPayload( jsonMessage ),
            AiaSpeakerManagerBufferState_ToString( AIA_OVERRUN_STATE ) ) );
        --numExpectedOverruns;
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_observer->numObserversNotifiedSemaphore, 100 ) );
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_mockSequencer->resetSequenceNumberSemaphore, 100 ) );
        TEST_ASSERT_EQUAL( g_mockSequencer->currentSequenceNumber,
                           speakerSequenceNumberToSend - 1 );
    }
}

TEST( AiaSpeakerManagerTests, UnderrunRepeated )
{
    static const AiaBinaryAudioStreamOffset_t TEST_OPEN_SPEAKER_OFFSET = 0;
    const char* openSpeakerPayload =
        generateOpenSpeaker( TEST_OPEN_SPEAKER_OFFSET );
    AiaSpeakerManager_OnOpenSpeakerDirectiveReceived(
        g_speakerManager, (void*)openSpeakerPayload,
        strlen( openSpeakerPayload ), 0, 0 );
    AiaFree( (void*)openSpeakerPayload );

    size_t binaryMessageLength = 0;
    static const AiaBinaryAudioStreamOffset_t AUDIO_STREAM_OFFSET_START = 0;
    const uint8_t* binaryMessage = generateBinaryAudioMessageEntry(
        TEST_FRAME_1, sizeof( TEST_FRAME_1 ), 0, AUDIO_STREAM_OFFSET_START,
        &binaryMessageLength );
    AiaSpeakerManager_OnSpeakerTopicMessageReceived(
        g_speakerManager, binaryMessage, binaryMessageLength, 0 );
    AiaFree( (void*)binaryMessage );

    size_t expectedNumSpeakerOpenedEvents = 1;
    while( expectedNumSpeakerOpenedEvents )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_mockRegulator->writeSemaphore, 100 ) );
        AiaListDouble( Link_t )* link = NULL;
        link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
        AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
        TEST_ASSERT_TRUE( link );
        AiaJsonMessage_t* jsonMessage = AiaJsonMessage_FromMessage(
            ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
        TEST_ASSERT_EQUAL( strcmp( AiaJsonMessage_GetName( jsonMessage ),
                                   AIA_EVENTS_SPEAKER_OPENED ),
                           0 );
        const char* payload = AiaJsonMessage_GetJsonPayload( jsonMessage );
        const char* offset;
        TEST_ASSERT_TRUE( AiaFindJsonValue(
            payload, strlen( payload ), AIA_SPEAKER_OPENED_OFFSET_KEY,
            strlen( AIA_SPEAKER_OPENED_OFFSET_KEY ), &offset, NULL ) );
        TEST_ASSERT_EQUAL( atoi( offset ), TEST_OPEN_SPEAKER_OFFSET );
        --expectedNumSpeakerOpenedEvents;
    }

    size_t expectedNumUnderruns = 1;
    while( expectedNumUnderruns )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_mockRegulator->writeSemaphore, 100 ) );
        AiaListDouble( Link_t )* link = NULL;
        link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
        AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
        TEST_ASSERT_TRUE( link );
        AiaJsonMessage_t* jsonMessage = AiaJsonMessage_FromMessage(
            ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
        TEST_ASSERT_EQUAL( strcmp( AiaJsonMessage_GetName( jsonMessage ),
                                   AIA_EVENTS_BUFFER_STATE_CHANGED ),
                           0 );
        TEST_ASSERT_TRUE( strstr(
            AiaJsonMessage_GetJsonPayload( jsonMessage ),
            AiaSpeakerManagerBufferState_ToString( AIA_UNDERRUN_STATE ) ) );
        --expectedNumUnderruns;
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_observer->numObserversNotifiedSemaphore, 100 ) );
    }

    binaryMessage = generateBinaryAudioMessageEntry(
        TEST_FRAME_1, sizeof( TEST_FRAME_1 ), 0,
        AUDIO_STREAM_OFFSET_START + sizeof( TEST_FRAME_1 ),
        &binaryMessageLength );
    AiaSpeakerManager_OnSpeakerTopicMessageReceived(
        g_speakerManager, binaryMessage, binaryMessageLength, 0 );
    AiaFree( (void*)binaryMessage );

    expectedNumUnderruns = 1;
    while( expectedNumUnderruns )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_mockRegulator->writeSemaphore, 100 ) );
        AiaListDouble( Link_t )* link = NULL;
        link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
        AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
        TEST_ASSERT_TRUE( link );
        AiaJsonMessage_t* jsonMessage = AiaJsonMessage_FromMessage(
            ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
        TEST_ASSERT_EQUAL( strcmp( AiaJsonMessage_GetName( jsonMessage ),
                                   AIA_EVENTS_BUFFER_STATE_CHANGED ),
                           0 );
        TEST_ASSERT_TRUE( strstr(
            AiaJsonMessage_GetJsonPayload( jsonMessage ),
            AiaSpeakerManagerBufferState_ToString( AIA_UNDERRUN_STATE ) ) );
        --expectedNumUnderruns;
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_observer->numObserversNotifiedSemaphore, 100 ) );
    }

    TEST_ASSERT_TRUE(
        AiaListDouble( IsEmpty )( &g_mockRegulator->writtenMessages ) );
}

TEST( AiaSpeakerManagerTests,
      CloseSpeakerResultsInSpeakerClosedAndNoSubsequentBufferEvents )
{
    static const AiaBinaryAudioStreamOffset_t TEST_OPEN_SPEAKER_OFFSET = 0;
    const char* openSpeakerPayload =
        generateOpenSpeaker( TEST_OPEN_SPEAKER_OFFSET );
    AiaSpeakerManager_OnOpenSpeakerDirectiveReceived(
        g_speakerManager, (void*)openSpeakerPayload,
        strlen( openSpeakerPayload ), 0, 0 );

    size_t binaryMessageLength = 0;
    const uint8_t* binaryMessage = generateBinaryAudioMessageEntry(
        TEST_FRAME_1, sizeof( TEST_FRAME_1 ), 0, TEST_OPEN_SPEAKER_OFFSET,
        &binaryMessageLength );
    AiaSpeakerManager_OnSpeakerTopicMessageReceived(
        g_speakerManager, binaryMessage, binaryMessageLength, 0 );

    size_t expectedFramesPushed = 1;
    while( expectedFramesPushed )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_observer->numSpeakerFramesPushedSemaphore, 100 ) );
        --expectedFramesPushed;
    }

    size_t expectedNumSpeakerOpenedEvents = 1;
    while( expectedNumSpeakerOpenedEvents )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_mockRegulator->writeSemaphore, 100 ) );
        AiaListDouble( Link_t )* link = NULL;
        link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
        AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
        TEST_ASSERT_TRUE( link );
        AiaJsonMessage_t* jsonMessage = AiaJsonMessage_FromMessage(
            ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
        TEST_ASSERT_EQUAL( strcmp( AiaJsonMessage_GetName( jsonMessage ),
                                   AIA_EVENTS_SPEAKER_OPENED ),
                           0 );
        const char* payload = AiaJsonMessage_GetJsonPayload( jsonMessage );
        const char* offset;
        TEST_ASSERT_TRUE( AiaFindJsonValue(
            payload, strlen( payload ), AIA_SPEAKER_OPENED_OFFSET_KEY,
            strlen( AIA_SPEAKER_OPENED_OFFSET_KEY ), &offset, NULL ) );
        TEST_ASSERT_EQUAL( atoi( offset ), TEST_OPEN_SPEAKER_OFFSET );
        --expectedNumSpeakerOpenedEvents;
    }

    size_t expectedNumUnderruns = 1;
    while( expectedNumUnderruns )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_mockRegulator->writeSemaphore, 100 ) );
        AiaListDouble( Link_t )* link = NULL;
        link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
        AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
        TEST_ASSERT_TRUE( link );
        AiaJsonMessage_t* jsonMessage = AiaJsonMessage_FromMessage(
            ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
        TEST_ASSERT_EQUAL( strcmp( AiaJsonMessage_GetName( jsonMessage ),
                                   AIA_EVENTS_BUFFER_STATE_CHANGED ),
                           0 );
        TEST_ASSERT_TRUE( strstr(
            AiaJsonMessage_GetJsonPayload( jsonMessage ),
            AiaSpeakerManagerBufferState_ToString( AIA_UNDERRUN_STATE ) ) );
        --expectedNumUnderruns;
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_observer->numObserversNotifiedSemaphore, 100 ) );
    }

    TEST_ASSERT_FALSE( AiaSemaphore( TimedWait )(
        &g_mockSequencer->resetSequenceNumberSemaphore, 100 ) );

    static AiaBinaryAudioStreamOffset_t TEST_CLOSE_SPEAKER_OFFSET =
        sizeof( TEST_FRAME_1 );
    const char* closeSpeakerPayload =
        generateCloseSpeaker( &TEST_CLOSE_SPEAKER_OFFSET );
    AiaSpeakerManager_OnCloseSpeakerDirectiveReceived(
        g_speakerManager, (void*)closeSpeakerPayload,
        strlen( closeSpeakerPayload ), 0, 0 );

    size_t expectedNumSpeakerClosedEvents = 1;
    while( expectedNumSpeakerClosedEvents )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_mockRegulator->writeSemaphore, 100 ) );
        AiaListDouble( Link_t )* link = NULL;
        link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
        AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
        TEST_ASSERT_TRUE( link );
        AiaJsonMessage_t* jsonMessage = AiaJsonMessage_FromMessage(
            ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
        TEST_ASSERT_EQUAL( strcmp( AiaJsonMessage_GetName( jsonMessage ),
                                   AIA_EVENTS_SPEAKER_CLOSED ),
                           0 );
        const char* payload = AiaJsonMessage_GetJsonPayload( jsonMessage );
        const char* offset;
        TEST_ASSERT_TRUE( AiaFindJsonValue(
            payload, strlen( payload ), AIA_SPEAKER_CLOSED_OFFSET_KEY,
            strlen( AIA_SPEAKER_CLOSED_OFFSET_KEY ), &offset, NULL ) );
        TEST_ASSERT_EQUAL( atoi( offset ), TEST_CLOSE_SPEAKER_OFFSET );
        --expectedNumSpeakerClosedEvents;
    }

    size_t binaryMessageLength2 = 0;
    const uint8_t* binaryMessage2 = generateBinaryAudioMessageEntry(
        TEST_FRAME_1, sizeof( TEST_FRAME_1 ), 0, sizeof( TEST_FRAME_1 ),
        &binaryMessageLength2 );
    AiaSpeakerManager_OnSpeakerTopicMessageReceived(
        g_speakerManager, binaryMessage2, binaryMessageLength2, 0 );

    TEST_ASSERT_FALSE( AiaSemaphore( TimedWait )(
        &g_observer->numSpeakerFramesPushedSemaphore, 100 ) );
    TEST_ASSERT_FALSE( AiaSemaphore( TimedWait )(
        &g_mockSequencer->resetSequenceNumberSemaphore, 100 ) );

    TEST_ASSERT_TRUE(
        AiaListDouble( IsEmpty )( &g_mockRegulator->writtenMessages ) );

    AiaFree( (void*)binaryMessage );
    AiaFree( (void*)binaryMessage2 );
    AiaFree( (void*)openSpeakerPayload );
    AiaFree( (void*)closeSpeakerPayload );
}

TEST( AiaSpeakerManagerTests, NullPayloads )
{
    AiaSequenceNumber_t TEST_SEQUENCE_NUMBER = 42;
    size_t TEST_INDEX = 44;
    AiaSpeakerManager_OnSpeakerTopicMessageReceived( g_speakerManager, NULL, 0,
                                                     TEST_SEQUENCE_NUMBER );
    TEST_ASSERT_TRUE(
        AiaSemaphore( TryWait )( &g_mockRegulator->writeSemaphore ) );
    AiaListDouble( Link_t )* link = NULL;
    link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
    AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
    TEST_ASSERT_TRUE( link );
    AiaJsonMessage_t* jsonMessage = AiaJsonMessage_FromMessage(
        ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
    TEST_ASSERT_EQUAL( strcmp( AiaJsonMessage_GetName( jsonMessage ),
                               AIA_EVENTS_EXCEPTION_ENCOUNTERED ),
                       0 );
    const char* payload = AiaJsonMessage_GetJsonPayload( jsonMessage );

    const char* message = NULL;
    size_t messageLen = 0;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_EXCEPTION_ENCOUNTERED_MESSAGE_KEY,
        strlen( AIA_EXCEPTION_ENCOUNTERED_MESSAGE_KEY ), &message,
        &messageLen ) );
    TEST_ASSERT_NOT_NULL( message );

    const char* topicString = NULL;
    size_t topicLen = 0;
    AiaTopic_t topic;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        message, messageLen, AIA_EXCEPTION_ENCOUNTERED_MESSAGE_TOPIC_KEY,
        strlen( AIA_EXCEPTION_ENCOUNTERED_MESSAGE_TOPIC_KEY ), &topicString,
        &topicLen ) );
    TEST_ASSERT_NOT_NULL( topicString );
    TEST_ASSERT_TRUE( AiaJsonUtils_UnquoteString( &topicString, &topicLen ) );
    TEST_ASSERT_TRUE( AiaTopic_FromString( topicString, topicLen, &topic ) );
    TEST_ASSERT_EQUAL( AIA_TOPIC_SPEAKER, topic );

    const char* sequenceNumber = NULL;
    size_t sequenceNumberLen = 0;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        message, messageLen,
        AIA_EXCEPTION_ENCOUNTERED_MESSAGE_SEQUENCE_NUMBER_KEY,
        strlen( AIA_EXCEPTION_ENCOUNTERED_MESSAGE_SEQUENCE_NUMBER_KEY ),
        &sequenceNumber, &sequenceNumberLen ) );
    TEST_ASSERT_NOT_NULL( sequenceNumber );
    AiaJsonLongType sequenceNumberLong = 0;
    TEST_ASSERT_TRUE( AiaExtractLongFromJsonValue(
        sequenceNumber, sequenceNumberLen, &sequenceNumberLong ) );
    TEST_ASSERT_EQUAL( sequenceNumberLong, TEST_SEQUENCE_NUMBER );

    const char* index = NULL;
    size_t indexLen = 0;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        message, messageLen, AIA_EXCEPTION_ENCOUNTERED_MESSAGE_INDEX_KEY,
        strlen( AIA_EXCEPTION_ENCOUNTERED_MESSAGE_INDEX_KEY ), &index,
        &indexLen ) );
    TEST_ASSERT_NOT_NULL( index );
    AiaJsonLongType indexLong = 0;
    TEST_ASSERT_TRUE(
        AiaExtractLongFromJsonValue( index, indexLen, &indexLong ) );
    TEST_ASSERT_EQUAL( indexLong, 0 );

    AiaSpeakerManager_OnOpenSpeakerDirectiveReceived(
        g_speakerManager, NULL, 0, TEST_SEQUENCE_NUMBER, TEST_INDEX );
    TEST_ASSERT_TRUE(
        AiaSemaphore( TryWait )( &g_mockRegulator->writeSemaphore ) );
    link = NULL;
    link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
    AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
    TEST_ASSERT_TRUE( link );
    jsonMessage = AiaJsonMessage_FromMessage(
        ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
    TEST_ASSERT_EQUAL( strcmp( AiaJsonMessage_GetName( jsonMessage ),
                               AIA_EVENTS_EXCEPTION_ENCOUNTERED ),
                       0 );
    payload = AiaJsonMessage_GetJsonPayload( jsonMessage );

    message = NULL;
    messageLen = 0;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_EXCEPTION_ENCOUNTERED_MESSAGE_KEY,
        strlen( AIA_EXCEPTION_ENCOUNTERED_MESSAGE_KEY ), &message,
        &messageLen ) );
    TEST_ASSERT_NOT_NULL( message );

    topicString = NULL;
    topicLen = 0;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        message, messageLen, AIA_EXCEPTION_ENCOUNTERED_MESSAGE_TOPIC_KEY,
        strlen( AIA_EXCEPTION_ENCOUNTERED_MESSAGE_TOPIC_KEY ), &topicString,
        &topicLen ) );
    TEST_ASSERT_NOT_NULL( topicString );
    TEST_ASSERT_TRUE( AiaJsonUtils_UnquoteString( &topicString, &topicLen ) );
    TEST_ASSERT_TRUE( AiaTopic_FromString( topicString, topicLen, &topic ) );
    TEST_ASSERT_EQUAL( AIA_TOPIC_DIRECTIVE, topic );

    sequenceNumber = NULL;
    sequenceNumberLen = 0;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        message, messageLen,
        AIA_EXCEPTION_ENCOUNTERED_MESSAGE_SEQUENCE_NUMBER_KEY,
        strlen( AIA_EXCEPTION_ENCOUNTERED_MESSAGE_SEQUENCE_NUMBER_KEY ),
        &sequenceNumber, &sequenceNumberLen ) );
    TEST_ASSERT_NOT_NULL( sequenceNumber );
    sequenceNumberLong = 0;
    TEST_ASSERT_TRUE( AiaExtractLongFromJsonValue(
        sequenceNumber, sequenceNumberLen, &sequenceNumberLong ) );
    TEST_ASSERT_EQUAL( sequenceNumberLong, TEST_SEQUENCE_NUMBER );

    index = NULL;
    indexLen = 0;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        message, messageLen, AIA_EXCEPTION_ENCOUNTERED_MESSAGE_INDEX_KEY,
        strlen( AIA_EXCEPTION_ENCOUNTERED_MESSAGE_INDEX_KEY ), &index,
        &indexLen ) );
    TEST_ASSERT_NOT_NULL( index );
    indexLong = 0;
    TEST_ASSERT_TRUE(
        AiaExtractLongFromJsonValue( index, indexLen, &indexLong ) );
    TEST_ASSERT_EQUAL( indexLong, TEST_INDEX );

    /* Expect this to be treated as an immediate CloseSpeaker directive. */
    AiaSpeakerManager_OnCloseSpeakerDirectiveReceived(
        g_speakerManager, NULL, 0, TEST_SEQUENCE_NUMBER, TEST_INDEX );
    TEST_ASSERT_FALSE(
        AiaSemaphore( TryWait )( &g_mockRegulator->writeSemaphore ) );
}

static bool PlaySpeakerDataCallbackWrapper( const void* buf, size_t size,
                                            void* userData )
{
    PlaySpeakerDataCallback( buf, size, userData );
    return false;
}

TEST( AiaSpeakerManagerTests, SpeakerDataStopsPushingWhenRejected )
{
    AiaSpeakerManager_t* speakerManager = AiaSpeakerManager_Create(
        TEST_BUFFER_SIZE, TEST_OVERRUN_WARNING_THRESHOLD,
        TEST_UNDERRUN_WARNING_THRESHOLD, PlaySpeakerDataCallbackWrapper,
        g_observer, g_sequencer, g_regulator, SetVolumeCallback, g_observer,
        PlayOfflineAlertCallback, g_observer, StopOfflineAlertCallback,
        g_observer, NotifyObservers, g_observer );
    TEST_ASSERT_NOT_NULL( speakerManager );

    static const AiaBinaryAudioStreamOffset_t TEST_OPEN_SPEAKER_OFFSET = 0;
    const char* openSpeakerPayload =
        generateOpenSpeaker( TEST_OPEN_SPEAKER_OFFSET );
    AiaSpeakerManager_OnOpenSpeakerDirectiveReceived(
        speakerManager, (void*)openSpeakerPayload, strlen( openSpeakerPayload ),
        0, 0 );

    size_t binaryMessageLength = 0;
    const uint8_t* binaryMessage = generateBinaryAudioMessageEntry(
        TEST_FRAME_1, sizeof( TEST_FRAME_1 ), 0, TEST_OPEN_SPEAKER_OFFSET,
        &binaryMessageLength );
    AiaSpeakerManager_OnSpeakerTopicMessageReceived(
        speakerManager, binaryMessage, binaryMessageLength, 0 );

    size_t expectedFramesPushed = 1;
    while( expectedFramesPushed )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_observer->numSpeakerFramesPushedSemaphore, 100 ) );
        --expectedFramesPushed;
    }

    binaryMessageLength = 0;
    const uint8_t* binaryMessage2 = generateBinaryAudioMessageEntry(
        TEST_FRAME_1, sizeof( TEST_FRAME_1 ), 0,
        TEST_OPEN_SPEAKER_OFFSET + sizeof( TEST_FRAME_1 ),
        &binaryMessageLength );
    AiaSpeakerManager_OnSpeakerTopicMessageReceived(
        speakerManager, binaryMessage2, binaryMessageLength, 0 );

    TEST_ASSERT_FALSE( AiaSemaphore( TimedWait )(
        &g_observer->numSpeakerFramesPushedSemaphore, 100 ) );

    AiaSpeakerManager_OnSpeakerReady( speakerManager );
    binaryMessageLength = 0;
    const uint8_t* binaryMessage3 = generateBinaryAudioMessageEntry(
        TEST_FRAME_1, sizeof( TEST_FRAME_1 ), 0,
        TEST_OPEN_SPEAKER_OFFSET + sizeof( TEST_FRAME_1 ) +
            sizeof( TEST_FRAME_1 ),
        &binaryMessageLength );
    AiaSpeakerManager_OnSpeakerTopicMessageReceived(
        speakerManager, binaryMessage3, binaryMessageLength, 0 );
    expectedFramesPushed = 1;
    while( expectedFramesPushed )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_observer->numSpeakerFramesPushedSemaphore, 100 ) );
        --expectedFramesPushed;
    }

    AiaFree( (void*)binaryMessage );
    AiaFree( (void*)binaryMessage2 );
    AiaFree( (void*)binaryMessage3 );
    AiaFree( (void*)openSpeakerPayload );
    AiaSpeakerManager_Destroy( speakerManager );
}

TEST( AiaSpeakerManagerTests, PersistedVolumeIsInitialVolume )
{
    TEST_ASSERT_TRUE(
        AiaSemaphore( TimedWait )( &g_observer->volumeSemaphore, 100 ) );
    TEST_ASSERT_EQUAL( g_observer->volume, TEST_PERSISTED_VOLUME );
}

TEST( AiaSpeakerManagerTests, TestLocalAbsoluteVolumeChangeOutsideRange )
{
    TEST_ASSERT_TRUE(
        AiaSemaphore( TimedWait )( &g_observer->volumeSemaphore, 100 ) );
    TEST_ASSERT_EQUAL( g_observer->volume, TEST_PERSISTED_VOLUME );

    TEST_ASSERT_FALSE( AiaSpeakerManager_ChangeVolume( g_speakerManager,
                                                       AIA_MAX_VOLUME + 1 ) );
}

TEST( AiaSpeakerManagerTests, TestLocalAbsoluteVolumeChangeWhenSpeakerNotOpen )
{
    TEST_ASSERT_TRUE(
        AiaSemaphore( TimedWait )( &g_observer->volumeSemaphore, 100 ) );
    TEST_ASSERT_EQUAL( g_observer->volume, TEST_PERSISTED_VOLUME );

    TEST_ASSERT_TRUE(
        AiaSpeakerManager_ChangeVolume( g_speakerManager, AIA_MAX_VOLUME ) );
    TEST_ASSERT_TRUE(
        AiaSemaphore( TimedWait )( &g_observer->volumeSemaphore, 100 ) );
    TEST_ASSERT_EQUAL( g_observer->volume, AIA_MAX_VOLUME );

    TEST_ASSERT_TRUE(
        AiaSemaphore( TimedWait )( &g_mockRegulator->writeSemaphore, 100 ) );
    AiaListDouble( Link_t )* link = NULL;
    link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
    AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
    TEST_ASSERT_TRUE( link );
    AiaJsonMessage_t* jsonMessage = AiaJsonMessage_FromMessage(
        ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
    TEST_ASSERT_EQUAL( strcmp( AiaJsonMessage_GetName( jsonMessage ),
                               AIA_EVENTS_VOLUME_CHANGED ),
                       0 );
    const char* payload = AiaJsonMessage_GetJsonPayload( jsonMessage );

    const char* volume;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_VOLUME_CHANGED_VOLUME_KEY,
        strlen( AIA_VOLUME_CHANGED_VOLUME_KEY ), &volume, NULL ) );
    TEST_ASSERT_EQUAL( atoi( volume ), AIA_MAX_VOLUME );

    const char* offset;
    TEST_ASSERT_FALSE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_VOLUME_CHANGED_OFFSET_KEY,
        strlen( AIA_VOLUME_CHANGED_OFFSET_KEY ), &offset, NULL ) );
}

TEST( AiaSpeakerManagerTests, TestLocalAbsoluteVolumeChangeWhenSpeakerOpen )
{
    TEST_ASSERT_TRUE(
        AiaSemaphore( TimedWait )( &g_observer->volumeSemaphore, 100 ) );
    TEST_ASSERT_EQUAL( g_observer->volume, TEST_PERSISTED_VOLUME );

    static const AiaBinaryAudioStreamOffset_t TEST_OPEN_SPEAKER_OFFSET = 0;
    const char* openSpeakerPayload =
        generateOpenSpeaker( TEST_OPEN_SPEAKER_OFFSET );
    AiaSpeakerManager_OnOpenSpeakerDirectiveReceived(
        g_speakerManager, (void*)openSpeakerPayload,
        strlen( openSpeakerPayload ), 0, 0 );

    size_t binaryMessageLength = 0;
    const uint8_t* binaryMessage = generateBinaryAudioMessageEntry(
        TEST_FRAME_1, sizeof( TEST_FRAME_1 ), 0, TEST_OPEN_SPEAKER_OFFSET,
        &binaryMessageLength );
    AiaSpeakerManager_OnSpeakerTopicMessageReceived(
        g_speakerManager, binaryMessage, binaryMessageLength, 0 );

    size_t expectedFramesPushed = 1;
    while( expectedFramesPushed )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_observer->numSpeakerFramesPushedSemaphore, 100 ) );
        --expectedFramesPushed;
    }

    size_t expectedNumSpeakerOpenedEvents = 1;
    while( expectedNumSpeakerOpenedEvents )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_mockRegulator->writeSemaphore, 100 ) );
        AiaListDouble( Link_t )* link = NULL;
        link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
        AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
        TEST_ASSERT_TRUE( link );
        AiaJsonMessage_t* jsonMessage = AiaJsonMessage_FromMessage(
            ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
        TEST_ASSERT_EQUAL( strcmp( AiaJsonMessage_GetName( jsonMessage ),
                                   AIA_EVENTS_SPEAKER_OPENED ),
                           0 );
        const char* payload = AiaJsonMessage_GetJsonPayload( jsonMessage );
        const char* offset;
        TEST_ASSERT_TRUE( AiaFindJsonValue(
            payload, strlen( payload ), AIA_SPEAKER_OPENED_OFFSET_KEY,
            strlen( AIA_SPEAKER_OPENED_OFFSET_KEY ), &offset, NULL ) );
        TEST_ASSERT_EQUAL( atoi( offset ), TEST_OPEN_SPEAKER_OFFSET );
        --expectedNumSpeakerOpenedEvents;
    }

    TEST_ASSERT_TRUE(
        AiaSpeakerManager_ChangeVolume( g_speakerManager, AIA_MAX_VOLUME ) );
    TEST_ASSERT_TRUE(
        AiaSemaphore( TimedWait )( &g_observer->volumeSemaphore, 100 ) );
    TEST_ASSERT_EQUAL( g_observer->volume, AIA_MAX_VOLUME );

    TEST_ASSERT_TRUE(
        AiaSemaphore( TimedWait )( &g_mockRegulator->writeSemaphore, 100 ) );
    AiaListDouble( Link_t )* link = NULL;
    link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
    AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
    TEST_ASSERT_TRUE( link );
    AiaJsonMessage_t* jsonMessage = AiaJsonMessage_FromMessage(
        ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
    TEST_ASSERT_EQUAL( strcmp( AiaJsonMessage_GetName( jsonMessage ),
                               AIA_EVENTS_VOLUME_CHANGED ),
                       0 );
    const char* payload = AiaJsonMessage_GetJsonPayload( jsonMessage );

    const char* volume;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_VOLUME_CHANGED_VOLUME_KEY,
        strlen( AIA_VOLUME_CHANGED_VOLUME_KEY ), &volume, NULL ) );
    TEST_ASSERT_EQUAL( atoi( volume ), AIA_MAX_VOLUME );

    const char* offset;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_VOLUME_CHANGED_OFFSET_KEY,
        strlen( AIA_VOLUME_CHANGED_OFFSET_KEY ), &offset, NULL ) );
    TEST_ASSERT_EQUAL( atoi( offset ),
                       TEST_OPEN_SPEAKER_OFFSET + sizeof( TEST_FRAME_1 ) );
}

TEST( AiaSpeakerManagerTests,
      TestLocalRelativeVolumeIncrementChangeWhenSpeakerNotOpenWithinBounds )
{
    TEST_ASSERT_TRUE(
        AiaSemaphore( TimedWait )( &g_observer->volumeSemaphore, 100 ) );
    TEST_ASSERT_EQUAL( g_observer->volume, TEST_PERSISTED_VOLUME );

    int8_t DELTA = AIA_MAX_VOLUME - TEST_PERSISTED_VOLUME - 1;

    TEST_ASSERT_TRUE(
        AiaSpeakerManager_AdjustVolume( g_speakerManager, DELTA ) );
    TEST_ASSERT_TRUE(
        AiaSemaphore( TimedWait )( &g_observer->volumeSemaphore, 100 ) );
    TEST_ASSERT_EQUAL( g_observer->volume, TEST_PERSISTED_VOLUME + DELTA );

    TEST_ASSERT_TRUE(
        AiaSemaphore( TimedWait )( &g_mockRegulator->writeSemaphore, 100 ) );
    AiaListDouble( Link_t )* link = NULL;
    link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
    AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
    TEST_ASSERT_TRUE( link );
    AiaJsonMessage_t* jsonMessage = AiaJsonMessage_FromMessage(
        ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
    TEST_ASSERT_EQUAL( strcmp( AiaJsonMessage_GetName( jsonMessage ),
                               AIA_EVENTS_VOLUME_CHANGED ),
                       0 );
    const char* payload = AiaJsonMessage_GetJsonPayload( jsonMessage );

    const char* volume;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_VOLUME_CHANGED_VOLUME_KEY,
        strlen( AIA_VOLUME_CHANGED_VOLUME_KEY ), &volume, NULL ) );
    TEST_ASSERT_EQUAL( atoi( volume ), TEST_PERSISTED_VOLUME + DELTA );

    const char* offset;
    TEST_ASSERT_FALSE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_VOLUME_CHANGED_OFFSET_KEY,
        strlen( AIA_VOLUME_CHANGED_OFFSET_KEY ), &offset, NULL ) );
}

TEST( AiaSpeakerManagerTests,
      TestLocalRelativeVolumeDecrementChangeWhenSpeakerNotOpenWithinBounds )
{
    TEST_ASSERT_TRUE(
        AiaSemaphore( TimedWait )( &g_observer->volumeSemaphore, 100 ) );
    TEST_ASSERT_EQUAL( g_observer->volume, TEST_PERSISTED_VOLUME );

    int8_t DELTA = AIA_MIN_VOLUME - TEST_PERSISTED_VOLUME + 1;

    TEST_ASSERT_TRUE(
        AiaSpeakerManager_AdjustVolume( g_speakerManager, DELTA ) );
    TEST_ASSERT_TRUE(
        AiaSemaphore( TimedWait )( &g_observer->volumeSemaphore, 100 ) );
    TEST_ASSERT_EQUAL( g_observer->volume, TEST_PERSISTED_VOLUME + DELTA );

    TEST_ASSERT_TRUE(
        AiaSemaphore( TimedWait )( &g_mockRegulator->writeSemaphore, 100 ) );
    AiaListDouble( Link_t )* link = NULL;
    link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
    AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
    TEST_ASSERT_TRUE( link );
    AiaJsonMessage_t* jsonMessage = AiaJsonMessage_FromMessage(
        ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
    TEST_ASSERT_EQUAL( strcmp( AiaJsonMessage_GetName( jsonMessage ),
                               AIA_EVENTS_VOLUME_CHANGED ),
                       0 );
    const char* payload = AiaJsonMessage_GetJsonPayload( jsonMessage );

    const char* volume;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_VOLUME_CHANGED_VOLUME_KEY,
        strlen( AIA_VOLUME_CHANGED_VOLUME_KEY ), &volume, NULL ) );
    TEST_ASSERT_EQUAL( atoi( volume ), TEST_PERSISTED_VOLUME + DELTA );

    const char* offset;
    TEST_ASSERT_FALSE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_VOLUME_CHANGED_OFFSET_KEY,
        strlen( AIA_VOLUME_CHANGED_OFFSET_KEY ), &offset, NULL ) );
}

TEST( AiaSpeakerManagerTests,
      TestLocalRelativeVolumeIncrementChangeWhenSpeakerNotOpenOutsideBounds )
{
    TEST_ASSERT_TRUE(
        AiaSemaphore( TimedWait )( &g_observer->volumeSemaphore, 100 ) );
    TEST_ASSERT_EQUAL( g_observer->volume, TEST_PERSISTED_VOLUME );

    int8_t DELTA = AIA_MAX_VOLUME - TEST_PERSISTED_VOLUME + 1;

    TEST_ASSERT_TRUE(
        AiaSpeakerManager_AdjustVolume( g_speakerManager, DELTA ) );
    TEST_ASSERT_TRUE(
        AiaSemaphore( TimedWait )( &g_observer->volumeSemaphore, 100 ) );
    TEST_ASSERT_EQUAL( g_observer->volume, AIA_MAX_VOLUME );

    TEST_ASSERT_TRUE(
        AiaSemaphore( TimedWait )( &g_mockRegulator->writeSemaphore, 100 ) );
    AiaListDouble( Link_t )* link = NULL;
    link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
    AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
    TEST_ASSERT_TRUE( link );
    AiaJsonMessage_t* jsonMessage = AiaJsonMessage_FromMessage(
        ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
    TEST_ASSERT_EQUAL( strcmp( AiaJsonMessage_GetName( jsonMessage ),
                               AIA_EVENTS_VOLUME_CHANGED ),
                       0 );
    const char* payload = AiaJsonMessage_GetJsonPayload( jsonMessage );

    const char* volume;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_VOLUME_CHANGED_VOLUME_KEY,
        strlen( AIA_VOLUME_CHANGED_VOLUME_KEY ), &volume, NULL ) );
    TEST_ASSERT_EQUAL( atoi( volume ), AIA_MAX_VOLUME );

    const char* offset;
    TEST_ASSERT_FALSE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_VOLUME_CHANGED_OFFSET_KEY,
        strlen( AIA_VOLUME_CHANGED_OFFSET_KEY ), &offset, NULL ) );
}

TEST( AiaSpeakerManagerTests,
      TestLocalRelativeVolumeDecrementChangeWhenSpeakerNotOpenOutsideBounds )
{
    TEST_ASSERT_TRUE(
        AiaSemaphore( TimedWait )( &g_observer->volumeSemaphore, 100 ) );
    TEST_ASSERT_EQUAL( g_observer->volume, TEST_PERSISTED_VOLUME );

    int8_t DELTA = AIA_MIN_VOLUME - TEST_PERSISTED_VOLUME - 1;

    TEST_ASSERT_TRUE(
        AiaSpeakerManager_AdjustVolume( g_speakerManager, DELTA ) );
    TEST_ASSERT_TRUE(
        AiaSemaphore( TimedWait )( &g_observer->volumeSemaphore, 100 ) );
    TEST_ASSERT_EQUAL( g_observer->volume, AIA_MIN_VOLUME );

    TEST_ASSERT_TRUE(
        AiaSemaphore( TimedWait )( &g_mockRegulator->writeSemaphore, 100 ) );
    AiaListDouble( Link_t )* link = NULL;
    link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
    AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
    TEST_ASSERT_TRUE( link );
    AiaJsonMessage_t* jsonMessage = AiaJsonMessage_FromMessage(
        ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
    TEST_ASSERT_EQUAL( strcmp( AiaJsonMessage_GetName( jsonMessage ),
                               AIA_EVENTS_VOLUME_CHANGED ),
                       0 );
    const char* payload = AiaJsonMessage_GetJsonPayload( jsonMessage );

    const char* volume;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_VOLUME_CHANGED_VOLUME_KEY,
        strlen( AIA_VOLUME_CHANGED_VOLUME_KEY ), &volume, NULL ) );
    TEST_ASSERT_EQUAL( atoi( volume ), AIA_MIN_VOLUME );

    const char* offset;
    TEST_ASSERT_FALSE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_VOLUME_CHANGED_OFFSET_KEY,
        strlen( AIA_VOLUME_CHANGED_OFFSET_KEY ), &offset, NULL ) );
}

TEST( AiaSpeakerManagerTests, NoVolumeChangeResultsInNoEvent )
{
    TEST_ASSERT_TRUE(
        AiaSemaphore( TimedWait )( &g_observer->volumeSemaphore, 100 ) );
    TEST_ASSERT_EQUAL( g_observer->volume, TEST_PERSISTED_VOLUME );

    TEST_ASSERT_TRUE( AiaSpeakerManager_ChangeVolume( g_speakerManager,
                                                      TEST_PERSISTED_VOLUME ) );
    TEST_ASSERT_TRUE(
        AiaSemaphore( TimedWait )( &g_observer->volumeSemaphore, 100 ) );
    TEST_ASSERT_FALSE(
        AiaSemaphore( TimedWait )( &g_mockRegulator->writeSemaphore, 100 ) );

    TEST_ASSERT_TRUE( AiaSpeakerManager_AdjustVolume( g_speakerManager, 0 ) );
    TEST_ASSERT_TRUE(
        AiaSemaphore( TimedWait )( &g_observer->volumeSemaphore, 100 ) );
    TEST_ASSERT_FALSE(
        AiaSemaphore( TimedWait )( &g_mockRegulator->writeSemaphore, 100 ) );
}

TEST( AiaSpeakerManagerTests, SetVolumeWithoutOffsetResultsInImmediateChange )
{
    TEST_ASSERT_TRUE(
        AiaSemaphore( TimedWait )( &g_observer->volumeSemaphore, 100 ) );
    TEST_ASSERT_EQUAL( g_observer->volume, TEST_PERSISTED_VOLUME );

    uint8_t TEST_VOLUME = 75;

    char* setVolumeDirective = generateSetVolumeWithoutOffset( TEST_VOLUME );
    TEST_ASSERT_NOT_NULL( setVolumeDirective );
    AiaSpeakerManager_OnSetVolumeDirectiveReceived(
        g_speakerManager, (void*)setVolumeDirective,
        strlen( setVolumeDirective ), 0, 0 );
    AiaFree( setVolumeDirective );

    TEST_ASSERT_TRUE(
        AiaSemaphore( TimedWait )( &g_observer->volumeSemaphore, 100 ) );
    TEST_ASSERT_EQUAL( g_observer->volume, TEST_VOLUME );

    TEST_ASSERT_TRUE(
        AiaSemaphore( TimedWait )( &g_mockRegulator->writeSemaphore, 100 ) );
    AiaListDouble( Link_t )* link = NULL;
    link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
    AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
    TEST_ASSERT_TRUE( link );
    AiaJsonMessage_t* jsonMessage = AiaJsonMessage_FromMessage(
        ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
    TEST_ASSERT_EQUAL( strcmp( AiaJsonMessage_GetName( jsonMessage ),
                               AIA_EVENTS_VOLUME_CHANGED ),
                       0 );
    const char* payload = AiaJsonMessage_GetJsonPayload( jsonMessage );

    const char* volume;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_VOLUME_CHANGED_VOLUME_KEY,
        strlen( AIA_VOLUME_CHANGED_VOLUME_KEY ), &volume, NULL ) );
    TEST_ASSERT_EQUAL( atoi( volume ), TEST_VOLUME );

    const char* offset;
    TEST_ASSERT_FALSE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_VOLUME_CHANGED_OFFSET_KEY,
        strlen( AIA_VOLUME_CHANGED_OFFSET_KEY ), &offset, NULL ) );
}

TEST( AiaSpeakerManagerTests, SetVolumeWithOffsetResultsInEventualChange )
{
    TEST_ASSERT_TRUE(
        AiaSemaphore( TimedWait )( &g_observer->volumeSemaphore, 100 ) );
    TEST_ASSERT_EQUAL( g_observer->volume, TEST_PERSISTED_VOLUME );

    static const AiaBinaryAudioStreamOffset_t TEST_OPEN_SPEAKER_OFFSET = 0;
    const char* openSpeakerPayload =
        generateOpenSpeaker( TEST_OPEN_SPEAKER_OFFSET );
    AiaSpeakerManager_OnOpenSpeakerDirectiveReceived(
        g_speakerManager, (void*)openSpeakerPayload,
        strlen( openSpeakerPayload ), 0, 0 );

    uint8_t TEST_VOLUME = 75;
    char* setVolumeDirective =
        generateSetVolumeWithOffset( TEST_VOLUME, sizeof( TEST_FRAME_1 ) );
    TEST_ASSERT_NOT_NULL( setVolumeDirective );
    AiaSpeakerManager_OnSetVolumeDirectiveReceived(
        g_speakerManager, (void*)setVolumeDirective,
        strlen( setVolumeDirective ), 0, 0 );
    AiaFree( setVolumeDirective );

    /* No volume change yet, offset not yet reached. */
    TEST_ASSERT_FALSE(
        AiaSemaphore( TimedWait )( &g_observer->volumeSemaphore, 100 ) );
    TEST_ASSERT_FALSE(
        AiaSemaphore( TimedWait )( &g_mockRegulator->writeSemaphore, 100 ) );

    size_t binaryMessageLength = 0;
    const uint8_t* binaryMessage = generateBinaryAudioMessageEntry(
        TEST_FRAME_1, sizeof( TEST_FRAME_1 ), 0, TEST_OPEN_SPEAKER_OFFSET,
        &binaryMessageLength );
    AiaSpeakerManager_OnSpeakerTopicMessageReceived(
        g_speakerManager, binaryMessage, binaryMessageLength, 0 );

    size_t expectedFramesPushed = 1;
    while( expectedFramesPushed )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_observer->numSpeakerFramesPushedSemaphore, 100 ) );
        --expectedFramesPushed;
    }

    size_t expectedNumSpeakerOpenedEvents = 1;
    while( expectedNumSpeakerOpenedEvents )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_mockRegulator->writeSemaphore, 100 ) );
        AiaListDouble( Link_t )* link = NULL;
        link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
        AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
        TEST_ASSERT_TRUE( link );
        AiaJsonMessage_t* jsonMessage = AiaJsonMessage_FromMessage(
            ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
        TEST_ASSERT_EQUAL( strcmp( AiaJsonMessage_GetName( jsonMessage ),
                                   AIA_EVENTS_SPEAKER_OPENED ),
                           0 );
        const char* payload = AiaJsonMessage_GetJsonPayload( jsonMessage );
        const char* offset;
        TEST_ASSERT_TRUE( AiaFindJsonValue(
            payload, strlen( payload ), AIA_SPEAKER_OPENED_OFFSET_KEY,
            strlen( AIA_SPEAKER_OPENED_OFFSET_KEY ), &offset, NULL ) );
        TEST_ASSERT_EQUAL( atoi( offset ), TEST_OPEN_SPEAKER_OFFSET );
        --expectedNumSpeakerOpenedEvents;
    }

    TEST_ASSERT_TRUE(
        AiaSemaphore( TimedWait )( &g_observer->volumeSemaphore, 100 ) );
    TEST_ASSERT_EQUAL( g_observer->volume, TEST_VOLUME );

    TEST_ASSERT_TRUE(
        AiaSemaphore( TimedWait )( &g_mockRegulator->writeSemaphore, 100 ) );
    AiaListDouble( Link_t )* link = NULL;
    link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
    AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
    TEST_ASSERT_TRUE( link );
    AiaJsonMessage_t* jsonMessage = AiaJsonMessage_FromMessage(
        ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
    TEST_ASSERT_EQUAL( strcmp( AiaJsonMessage_GetName( jsonMessage ),
                               AIA_EVENTS_VOLUME_CHANGED ),
                       0 );
    const char* payload = AiaJsonMessage_GetJsonPayload( jsonMessage );

    const char* volume;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_VOLUME_CHANGED_VOLUME_KEY,
        strlen( AIA_VOLUME_CHANGED_VOLUME_KEY ), &volume, NULL ) );
    TEST_ASSERT_EQUAL( atoi( volume ), TEST_VOLUME );

    const char* offset;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_VOLUME_CHANGED_OFFSET_KEY,
        strlen( AIA_VOLUME_CHANGED_OFFSET_KEY ), &offset, NULL ) );
    TEST_ASSERT_EQUAL( atoi( offset ), sizeof( TEST_FRAME_1 ) );
}

TEST( AiaSpeakerManagerTests,
      SetMultipleVolumesWithFutureOffsetResultsInEventualChange )
{
    TEST_ASSERT_TRUE(
        AiaSemaphore( TimedWait )( &g_observer->volumeSemaphore, 100 ) );
    TEST_ASSERT_EQUAL( g_observer->volume, TEST_PERSISTED_VOLUME );

    static const AiaBinaryAudioStreamOffset_t TEST_OPEN_SPEAKER_OFFSET = 0;
    const char* openSpeakerPayload =
        generateOpenSpeaker( TEST_OPEN_SPEAKER_OFFSET );
    AiaSpeakerManager_OnOpenSpeakerDirectiveReceived(
        g_speakerManager, (void*)openSpeakerPayload,
        strlen( openSpeakerPayload ), 0, 0 );

    uint8_t TEST_VOLUME = 75;
    char* setVolumeDirective =
        generateSetVolumeWithOffset( TEST_VOLUME, sizeof( TEST_FRAME_1 ) );
    TEST_ASSERT_NOT_NULL( setVolumeDirective );
    AiaSpeakerManager_OnSetVolumeDirectiveReceived(
        g_speakerManager, (void*)setVolumeDirective,
        strlen( setVolumeDirective ), 0, 0 );
    AiaFree( setVolumeDirective );

    uint8_t TEST_VOLUME_2 = 50;
    char* setVolumeDirective2 = generateSetVolumeWithOffset(
        TEST_VOLUME_2, sizeof( TEST_FRAME_1 ) + sizeof( TEST_FRAME_1 ) );
    TEST_ASSERT_NOT_NULL( setVolumeDirective2 );
    AiaSpeakerManager_OnSetVolumeDirectiveReceived(
        g_speakerManager, (void*)setVolumeDirective2,
        strlen( setVolumeDirective2 ), 0, 0 );
    AiaFree( setVolumeDirective2 );

    /* No volume change yet, offset not yet reached. */
    TEST_ASSERT_FALSE(
        AiaSemaphore( TimedWait )( &g_observer->volumeSemaphore, 100 ) );
    TEST_ASSERT_FALSE(
        AiaSemaphore( TimedWait )( &g_mockRegulator->writeSemaphore, 100 ) );

    size_t binaryMessageLength = 0;
    const uint8_t* binaryMessage = generateBinaryAudioMessageEntry(
        TEST_FRAME_1, sizeof( TEST_FRAME_1 ), 0, TEST_OPEN_SPEAKER_OFFSET,
        &binaryMessageLength );
    AiaSpeakerManager_OnSpeakerTopicMessageReceived(
        g_speakerManager, binaryMessage, binaryMessageLength, 0 );

    const uint8_t* binaryMessage2 = generateBinaryAudioMessageEntry(
        TEST_FRAME_1, sizeof( TEST_FRAME_1 ), 0,
        TEST_OPEN_SPEAKER_OFFSET + sizeof( TEST_FRAME_1 ),
        &binaryMessageLength );
    AiaSpeakerManager_OnSpeakerTopicMessageReceived(
        g_speakerManager, binaryMessage2, binaryMessageLength, 1 );

    size_t expectedFramesPushed = 2;
    while( expectedFramesPushed )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_observer->numSpeakerFramesPushedSemaphore, 100 ) );
        --expectedFramesPushed;
    }

    size_t expectedNumSpeakerOpenedEvents = 1;
    while( expectedNumSpeakerOpenedEvents )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_mockRegulator->writeSemaphore, 100 ) );
        AiaListDouble( Link_t )* link = NULL;
        link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
        AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
        TEST_ASSERT_TRUE( link );
        AiaJsonMessage_t* jsonMessage = AiaJsonMessage_FromMessage(
            ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
        TEST_ASSERT_EQUAL( strcmp( AiaJsonMessage_GetName( jsonMessage ),
                                   AIA_EVENTS_SPEAKER_OPENED ),
                           0 );
        const char* payload = AiaJsonMessage_GetJsonPayload( jsonMessage );
        const char* offset;
        TEST_ASSERT_TRUE( AiaFindJsonValue(
            payload, strlen( payload ), AIA_SPEAKER_OPENED_OFFSET_KEY,
            strlen( AIA_SPEAKER_OPENED_OFFSET_KEY ), &offset, NULL ) );
        TEST_ASSERT_EQUAL( atoi( offset ), TEST_OPEN_SPEAKER_OFFSET );
        --expectedNumSpeakerOpenedEvents;
    }

    TEST_ASSERT_TRUE(
        AiaSemaphore( TimedWait )( &g_observer->volumeSemaphore, 100 ) );
    TEST_ASSERT_EQUAL( g_observer->volume, TEST_VOLUME );

    TEST_ASSERT_TRUE(
        AiaSemaphore( TimedWait )( &g_mockRegulator->writeSemaphore, 100 ) );
    AiaListDouble( Link_t )* link = NULL;
    link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
    AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
    TEST_ASSERT_TRUE( link );
    AiaJsonMessage_t* jsonMessage = AiaJsonMessage_FromMessage(
        ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
    TEST_ASSERT_EQUAL( strcmp( AiaJsonMessage_GetName( jsonMessage ),
                               AIA_EVENTS_VOLUME_CHANGED ),
                       0 );
    const char* payload = AiaJsonMessage_GetJsonPayload( jsonMessage );

    const char* volume;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_VOLUME_CHANGED_VOLUME_KEY,
        strlen( AIA_VOLUME_CHANGED_VOLUME_KEY ), &volume, NULL ) );
    TEST_ASSERT_EQUAL( atoi( volume ), TEST_VOLUME );

    const char* offset;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_VOLUME_CHANGED_OFFSET_KEY,
        strlen( AIA_VOLUME_CHANGED_OFFSET_KEY ), &offset, NULL ) );
    TEST_ASSERT_EQUAL( atoi( offset ), sizeof( TEST_FRAME_1 ) );

    TEST_ASSERT_TRUE(
        AiaSemaphore( TimedWait )( &g_observer->volumeSemaphore, 100 ) );
    TEST_ASSERT_EQUAL( g_observer->volume, TEST_VOLUME_2 );

    TEST_ASSERT_TRUE(
        AiaSemaphore( TimedWait )( &g_mockRegulator->writeSemaphore, 100 ) );
    link = NULL;
    link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
    AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
    TEST_ASSERT_TRUE( link );
    jsonMessage = AiaJsonMessage_FromMessage(
        ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
    TEST_ASSERT_EQUAL( strcmp( AiaJsonMessage_GetName( jsonMessage ),
                               AIA_EVENTS_VOLUME_CHANGED ),
                       0 );
    payload = AiaJsonMessage_GetJsonPayload( jsonMessage );

    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_VOLUME_CHANGED_VOLUME_KEY,
        strlen( AIA_VOLUME_CHANGED_VOLUME_KEY ), &volume, NULL ) );
    TEST_ASSERT_EQUAL( atoi( volume ), TEST_VOLUME_2 );

    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_VOLUME_CHANGED_OFFSET_KEY,
        strlen( AIA_VOLUME_CHANGED_OFFSET_KEY ), &offset, NULL ) );
    TEST_ASSERT_EQUAL( atoi( offset ),
                       sizeof( TEST_FRAME_1 ) + sizeof( TEST_FRAME_1 ) );
}

TEST( AiaSpeakerManagerTests, InvokeSingleActionAtCurrentOffset )
{
    AiaTestActionObserver_t* actionObserver = AiaTestActionObserver_Create();
    TEST_ASSERT_NOT_NULL( actionObserver );
    TEST_ASSERT_NOT_EQUAL(
        AIA_INVALID_ACTION_ID,
        AiaSpeakerManager_InvokeActionAtOffset(
            g_speakerManager, 0, TestInvokeAction, actionObserver ) );
    TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
        &actionObserver->actionInvokedSemaphore, 100 ) );
}

TEST( AiaSpeakerManagerTests, InvokeSingleActionAtFutureOffset )
{
    AiaTestActionObserver_t* actionObserver = AiaTestActionObserver_Create();
    TEST_ASSERT_NOT_NULL( actionObserver );
    TEST_ASSERT_NOT_EQUAL( AIA_INVALID_ACTION_ID,
                           AiaSpeakerManager_InvokeActionAtOffset(
                               g_speakerManager, sizeof( TEST_FRAME_1 ),
                               TestInvokeAction, actionObserver ) );
    /* Action shouldn't get invoked yet. */
    TEST_ASSERT_FALSE( AiaSemaphore( TimedWait )(
        &actionObserver->actionInvokedSemaphore, 100 ) );

    static const AiaBinaryAudioStreamOffset_t TEST_OPEN_SPEAKER_OFFSET = 0;
    const char* openSpeakerPayload =
        generateOpenSpeaker( TEST_OPEN_SPEAKER_OFFSET );
    AiaSpeakerManager_OnOpenSpeakerDirectiveReceived(
        g_speakerManager, (void*)openSpeakerPayload,
        strlen( openSpeakerPayload ), 0, 0 );

    size_t binaryMessageLength = 0;
    const uint8_t* binaryMessage = generateBinaryAudioMessageEntry(
        TEST_FRAME_1, sizeof( TEST_FRAME_1 ), 0, TEST_OPEN_SPEAKER_OFFSET,
        &binaryMessageLength );
    AiaSpeakerManager_OnSpeakerTopicMessageReceived(
        g_speakerManager, binaryMessage, binaryMessageLength, 0 );

    size_t expectedFramesPushed = 1;
    while( expectedFramesPushed )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_observer->numSpeakerFramesPushedSemaphore, 100 ) );
        --expectedFramesPushed;
    }

    size_t expectedNumSpeakerOpenedEvents = 1;
    while( expectedNumSpeakerOpenedEvents )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_mockRegulator->writeSemaphore, 100 ) );
        AiaListDouble( Link_t )* link = NULL;
        link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
        AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
        TEST_ASSERT_TRUE( link );
        AiaJsonMessage_t* jsonMessage = AiaJsonMessage_FromMessage(
            ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
        TEST_ASSERT_EQUAL( strcmp( AiaJsonMessage_GetName( jsonMessage ),
                                   AIA_EVENTS_SPEAKER_OPENED ),
                           0 );
        const char* payload = AiaJsonMessage_GetJsonPayload( jsonMessage );
        const char* offset;
        TEST_ASSERT_TRUE( AiaFindJsonValue(
            payload, strlen( payload ), AIA_SPEAKER_OPENED_OFFSET_KEY,
            strlen( AIA_SPEAKER_OPENED_OFFSET_KEY ), &offset, NULL ) );
        TEST_ASSERT_EQUAL( atoi( offset ), TEST_OPEN_SPEAKER_OFFSET );
        --expectedNumSpeakerOpenedEvents;
    }

    /* Action now get invoked. */
    TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
        &actionObserver->actionInvokedSemaphore, 100 ) );
}

TEST( AiaSpeakerManagerTests, CanceledActionDoesNotResultInCallback )
{
    AiaTestActionObserver_t* actionObserver = AiaTestActionObserver_Create();
    TEST_ASSERT_NOT_NULL( actionObserver );
    AiaSpeakerActionHandle_t handle = AiaSpeakerManager_InvokeActionAtOffset(
        g_speakerManager, sizeof( TEST_FRAME_1 ), TestInvokeAction,
        actionObserver );
    TEST_ASSERT_NOT_EQUAL( AIA_INVALID_ACTION_ID, handle );
    /* Action shouldn't get invoked yet. */
    TEST_ASSERT_FALSE( AiaSemaphore( TimedWait )(
        &actionObserver->actionInvokedSemaphore, 100 ) );

    AiaSpeakerManager_CancelAction( g_speakerManager, handle );

    static const AiaBinaryAudioStreamOffset_t TEST_OPEN_SPEAKER_OFFSET = 0;
    const char* openSpeakerPayload =
        generateOpenSpeaker( TEST_OPEN_SPEAKER_OFFSET );
    AiaSpeakerManager_OnOpenSpeakerDirectiveReceived(
        g_speakerManager, (void*)openSpeakerPayload,
        strlen( openSpeakerPayload ), 0, 0 );

    size_t binaryMessageLength = 0;
    const uint8_t* binaryMessage = generateBinaryAudioMessageEntry(
        TEST_FRAME_1, sizeof( TEST_FRAME_1 ), 0, TEST_OPEN_SPEAKER_OFFSET,
        &binaryMessageLength );
    AiaSpeakerManager_OnSpeakerTopicMessageReceived(
        g_speakerManager, binaryMessage, binaryMessageLength, 0 );

    size_t expectedFramesPushed = 1;
    while( expectedFramesPushed )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_observer->numSpeakerFramesPushedSemaphore, 100 ) );
        --expectedFramesPushed;
    }

    size_t expectedNumSpeakerOpenedEvents = 1;
    while( expectedNumSpeakerOpenedEvents )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_mockRegulator->writeSemaphore, 100 ) );
        AiaListDouble( Link_t )* link = NULL;
        link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
        AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
        TEST_ASSERT_TRUE( link );
        AiaJsonMessage_t* jsonMessage = AiaJsonMessage_FromMessage(
            ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
        TEST_ASSERT_EQUAL( strcmp( AiaJsonMessage_GetName( jsonMessage ),
                                   AIA_EVENTS_SPEAKER_OPENED ),
                           0 );
        const char* payload = AiaJsonMessage_GetJsonPayload( jsonMessage );
        const char* offset;
        TEST_ASSERT_TRUE( AiaFindJsonValue(
            payload, strlen( payload ), AIA_SPEAKER_OPENED_OFFSET_KEY,
            strlen( AIA_SPEAKER_OPENED_OFFSET_KEY ), &offset, NULL ) );
        TEST_ASSERT_EQUAL( atoi( offset ), TEST_OPEN_SPEAKER_OFFSET );
        --expectedNumSpeakerOpenedEvents;
    }

    /* Action should not get invoked since it was canceled. */
    TEST_ASSERT_FALSE( AiaSemaphore( TimedWait )(
        &actionObserver->actionInvokedSemaphore, 100 ) );
}

TEST( AiaSpeakerManagerTests, InvokeMultipleActionsAtFutureOffset )
{
    AiaTestActionObserver_t* actionObserver1 = AiaTestActionObserver_Create();
    TEST_ASSERT_NOT_NULL( actionObserver1 );
    TEST_ASSERT_NOT_EQUAL( AIA_INVALID_ACTION_ID,
                           AiaSpeakerManager_InvokeActionAtOffset(
                               g_speakerManager, sizeof( TEST_FRAME_1 ),
                               TestInvokeAction, actionObserver1 ) );
    AiaTestActionObserver_t* actionObserver2 = AiaTestActionObserver_Create();
    TEST_ASSERT_NOT_NULL( actionObserver2 );
    TEST_ASSERT_NOT_EQUAL(
        AIA_INVALID_ACTION_ID,
        AiaSpeakerManager_InvokeActionAtOffset(
            g_speakerManager, sizeof( TEST_FRAME_1 ) + sizeof( TEST_FRAME_1 ),
            TestInvokeAction, actionObserver2 ) );

    /* Actions shouldn't get invoked yet. */
    TEST_ASSERT_FALSE( AiaSemaphore( TimedWait )(
        &actionObserver1->actionInvokedSemaphore, 100 ) );
    TEST_ASSERT_FALSE( AiaSemaphore( TimedWait )(
        &actionObserver2->actionInvokedSemaphore, 100 ) );

    static const AiaBinaryAudioStreamOffset_t TEST_OPEN_SPEAKER_OFFSET = 0;
    const char* openSpeakerPayload =
        generateOpenSpeaker( TEST_OPEN_SPEAKER_OFFSET );
    AiaSpeakerManager_OnOpenSpeakerDirectiveReceived(
        g_speakerManager, (void*)openSpeakerPayload,
        strlen( openSpeakerPayload ), 0, 0 );
    AiaFree( (void*)openSpeakerPayload );

    size_t binaryMessageLength = 0;
    const uint8_t* binaryMessage = generateBinaryAudioMessageEntry(
        TEST_FRAME_1, sizeof( TEST_FRAME_1 ), 0, TEST_OPEN_SPEAKER_OFFSET,
        &binaryMessageLength );
    AiaSpeakerManager_OnSpeakerTopicMessageReceived(
        g_speakerManager, binaryMessage, binaryMessageLength, 0 );
    AiaFree( (void*)binaryMessage );
    size_t expectedFramesPushed = 1;
    while( expectedFramesPushed )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_observer->numSpeakerFramesPushedSemaphore, 100 ) );
        --expectedFramesPushed;
    }

    size_t expectedNumSpeakerOpenedEvents = 1;
    while( expectedNumSpeakerOpenedEvents )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_mockRegulator->writeSemaphore, 100 ) );
        AiaListDouble( Link_t )* link = NULL;
        link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
        AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
        TEST_ASSERT_TRUE( link );
        AiaJsonMessage_t* jsonMessage = AiaJsonMessage_FromMessage(
            ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
        TEST_ASSERT_EQUAL( strcmp( AiaJsonMessage_GetName( jsonMessage ),
                                   AIA_EVENTS_SPEAKER_OPENED ),
                           0 );
        const char* payload = AiaJsonMessage_GetJsonPayload( jsonMessage );
        const char* offset;
        TEST_ASSERT_TRUE( AiaFindJsonValue(
            payload, strlen( payload ), AIA_SPEAKER_OPENED_OFFSET_KEY,
            strlen( AIA_SPEAKER_OPENED_OFFSET_KEY ), &offset, NULL ) );
        TEST_ASSERT_EQUAL( atoi( offset ), TEST_OPEN_SPEAKER_OFFSET );
        --expectedNumSpeakerOpenedEvents;
    }

    /* Only a single action should get invoked. */
    TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
        &actionObserver1->actionInvokedSemaphore, 100 ) );
    TEST_ASSERT_FALSE( AiaSemaphore( TimedWait )(
        &actionObserver2->actionInvokedSemaphore, 100 ) );

    size_t binaryMessageLength2 = 0;
    const uint8_t* binaryMessage2 = generateBinaryAudioMessageEntry(
        TEST_FRAME_1, sizeof( TEST_FRAME_1 ), 0, sizeof( TEST_FRAME_1 ),
        &binaryMessageLength2 );
    AiaSpeakerManager_OnSpeakerTopicMessageReceived(
        g_speakerManager, binaryMessage2, binaryMessageLength2, 0 );
    AiaFree( (void*)binaryMessage2 );

    TEST_ASSERT_FALSE( AiaSemaphore( TimedWait )(
        &actionObserver1->actionInvokedSemaphore, 100 ) );
    TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
        &actionObserver2->actionInvokedSemaphore, 100 ) );

    AiaTestActionObserver_Destroy( actionObserver2 );
    AiaTestActionObserver_Destroy( actionObserver1 );
}

TEST( AiaSpeakerManagerTests, LocalStoppageResultsInActionInvalidation )
{
    AiaTestActionObserver_t* actionObserver = AiaTestActionObserver_Create();
    TEST_ASSERT_NOT_NULL( actionObserver );
    TEST_ASSERT_NOT_EQUAL(
        AIA_INVALID_ACTION_ID,
        AiaSpeakerManager_InvokeActionAtOffset(
            g_speakerManager, sizeof( TEST_FRAME_1 ) + sizeof( TEST_FRAME_1 ),
            TestInvokeAction, actionObserver ) );

    /* Action shouldn't get invoked yet. */
    TEST_ASSERT_FALSE( AiaSemaphore( TimedWait )(
        &actionObserver->actionInvokedSemaphore, 100 ) );

    static const AiaBinaryAudioStreamOffset_t TEST_OPEN_SPEAKER_OFFSET = 0;
    const char* openSpeakerPayload =
        generateOpenSpeaker( TEST_OPEN_SPEAKER_OFFSET );
    AiaSpeakerManager_OnOpenSpeakerDirectiveReceived(
        g_speakerManager, (void*)openSpeakerPayload,
        strlen( openSpeakerPayload ), 0, 0 );

    size_t binaryMessageLength = 0;
    const uint8_t* binaryMessage = generateBinaryAudioMessageEntry(
        TEST_FRAME_1, sizeof( TEST_FRAME_1 ), 0, TEST_OPEN_SPEAKER_OFFSET,
        &binaryMessageLength );
    AiaSpeakerManager_OnSpeakerTopicMessageReceived(
        g_speakerManager, binaryMessage, binaryMessageLength, 0 );
    AiaFree( (uint8_t*)binaryMessage );

    /* Mimic a local barge-in while audio is coming in. */
    AiaSpeakerManager_StopPlayback( g_speakerManager );

    binaryMessageLength = 0;
    binaryMessage = generateBinaryAudioMessageEntry(
        TEST_FRAME_1, sizeof( TEST_FRAME_1 ), 0, sizeof( TEST_FRAME_1 ),
        &binaryMessageLength );
    AiaSpeakerManager_OnSpeakerTopicMessageReceived(
        g_speakerManager, binaryMessage, binaryMessageLength, 0 );
    AiaFree( (uint8_t*)binaryMessage );

    size_t expectedFramesPushed = 1;
    while( expectedFramesPushed )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_observer->numSpeakerFramesPushedSemaphore, 100 ) );
        --expectedFramesPushed;
    }

    /* Action shouldn't get invoked. */
    TEST_ASSERT_FALSE( AiaSemaphore( TimedWait )(
        &actionObserver->actionInvokedSemaphore, 100 ) );
}

TEST( AiaSpeakerManagerTests, MalformedSpeakerMessage )
{
    static const AiaBinaryAudioStreamOffset_t TEST_OPEN_SPEAKER_OFFSET = 0;
    size_t binaryMessageLength = 0;
    const uint8_t* binaryMessage = generateBinaryAudioMessageEntry(
        TEST_FRAME_1, sizeof( TEST_FRAME_1 ), 0, TEST_OPEN_SPEAKER_OFFSET,
        &binaryMessageLength );
    uint8_t malformedBinaryMessage[ binaryMessageLength + 1 ];
    memcpy( malformedBinaryMessage, binaryMessage, binaryMessageLength );
    malformedBinaryMessage[ binaryMessageLength ] = 9;
    AiaSpeakerManager_OnSpeakerTopicMessageReceived(
        g_speakerManager, malformedBinaryMessage, binaryMessageLength + 1, 0 );

    AiaTestUtilities_TestMalformedMessageExceptionIsGenerated( g_mockRegulator,
                                                               0, 1 );
    AiaFree( (void*)binaryMessage );
}

TEST( AiaSpeakerManagerTests, BufferStateEventsNotSentWhenSpeakerClosed )
{
    AiaSequenceNumber_t speakerSequenceNumberToSend = 0;
    const uint8_t LARGE_TEST_FRAME[ TEST_BUFFER_SIZE ];

    AiaBinaryAudioStreamOffset_t offsetToSend = 0;

    size_t binaryMessageLength = 0;
    const uint8_t* binaryMessage = generateBinaryAudioMessageEntry(
        LARGE_TEST_FRAME, sizeof( LARGE_TEST_FRAME ), 0, offsetToSend,
        &binaryMessageLength );
    AiaSpeakerManager_OnSpeakerTopicMessageReceived(
        g_speakerManager, binaryMessage, binaryMessageLength,
        speakerSequenceNumberToSend++ );
    offsetToSend += sizeof( LARGE_TEST_FRAME );
    AiaFree( (void*)binaryMessage );

    TEST_ASSERT_FALSE( AiaSemaphore( TimedWait )(
        &g_observer->numSpeakerFramesPushedSemaphore, 100 ) );

    size_t iterationsToOverflowBuffer =
        ( TEST_BUFFER_SIZE / sizeof( LARGE_TEST_FRAME ) ) + 1;
    while( iterationsToOverflowBuffer )
    {
        size_t binaryMessageLength2 = 0;
        const uint8_t* binaryMessage2 = generateBinaryAudioMessageEntry(
            LARGE_TEST_FRAME, sizeof( LARGE_TEST_FRAME ), 0, offsetToSend,
            &binaryMessageLength2 );
        AiaSpeakerManager_OnSpeakerTopicMessageReceived(
            g_speakerManager, binaryMessage2, binaryMessageLength2,
            speakerSequenceNumberToSend++ );
        offsetToSend += sizeof( LARGE_TEST_FRAME );
        AiaFree( (void*)binaryMessage2 );
        --iterationsToOverflowBuffer;
    }

    TEST_ASSERT_FALSE( AiaSemaphore( TimedWait )(
        &g_observer->numSpeakerFramesPushedSemaphore, 100 ) );

    TEST_ASSERT_TRUE(
        AiaListDouble( IsEmpty )( &g_mockRegulator->writtenMessages ) );
}

TEST( AiaSpeakerManagerTests, OlderSpeakerDataOverwrittenWhenSpeakerClosed )
{
    AiaSequenceNumber_t speakerSequenceNumberToSend = 0;
    const uint8_t LARGE_TEST_FRAME[ TEST_BUFFER_SIZE ];

    AiaBinaryAudioStreamOffset_t offsetToSend = 0;

    /* Buffer will be full after this message. */
    size_t binaryMessageLength = 0;
    const uint8_t* binaryMessage = generateBinaryAudioMessageEntry(
        LARGE_TEST_FRAME, sizeof( LARGE_TEST_FRAME ), 0, offsetToSend,
        &binaryMessageLength );
    AiaSpeakerManager_OnSpeakerTopicMessageReceived(
        g_speakerManager, binaryMessage, binaryMessageLength,
        speakerSequenceNumberToSend++ );
    offsetToSend += sizeof( LARGE_TEST_FRAME );
    AiaFree( (void*)binaryMessage );

    /* Overflow the audio buffer. */
    const uint8_t* binaryMessage2 = generateBinaryAudioMessageEntry(
        LARGE_TEST_FRAME, sizeof( LARGE_TEST_FRAME ), 0, offsetToSend,
        &binaryMessageLength );
    AiaSpeakerManager_OnSpeakerTopicMessageReceived(
        g_speakerManager, binaryMessage2, binaryMessageLength,
        speakerSequenceNumberToSend++ );
    offsetToSend += sizeof( LARGE_TEST_FRAME );
    AiaFree( (void*)binaryMessage2 );

    TEST_ASSERT_FALSE( AiaSemaphore( TimedWait )(
        &g_observer->numSpeakerFramesPushedSemaphore, 100 ) );

    /* No overrun should have been sent since speaker is closed. */
    TEST_ASSERT_TRUE(
        AiaListDouble( IsEmpty )( &g_mockRegulator->writtenMessages ) );

    /* Open the speaker. */
    AiaBinaryAudioStreamOffset_t TEST_OPEN_SPEAKER_OFFSET = offsetToSend;
    const char* openSpeakerPayload =
        generateOpenSpeaker( TEST_OPEN_SPEAKER_OFFSET );
    AiaSpeakerManager_OnOpenSpeakerDirectiveReceived(
        g_speakerManager, (void*)openSpeakerPayload,
        strlen( openSpeakerPayload ), 0, 0 );
    AiaFree( (void*)openSpeakerPayload );

    /* Speaker data at next sequence number should be consumed properly. */
    const uint8_t* binaryMessage3 = generateBinaryAudioMessageEntry(
        LARGE_TEST_FRAME, sizeof( LARGE_TEST_FRAME ), 0, offsetToSend,
        &binaryMessageLength );
    AiaSpeakerManager_OnSpeakerTopicMessageReceived(
        g_speakerManager, binaryMessage3, binaryMessageLength,
        speakerSequenceNumberToSend++ );
    offsetToSend += sizeof( LARGE_TEST_FRAME );
    AiaFree( (void*)binaryMessage3 );

    /* Should see the speaker opened. */
    size_t expectedNumSpeakerOpenedEvents = 1;
    while( expectedNumSpeakerOpenedEvents )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_mockRegulator->writeSemaphore, 100 ) );
        AiaListDouble( Link_t )* link = NULL;
        link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
        AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
        TEST_ASSERT_TRUE( link );
        AiaJsonMessage_t* jsonMessage = AiaJsonMessage_FromMessage(
            ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
        TEST_ASSERT_EQUAL( strcmp( AiaJsonMessage_GetName( jsonMessage ),
                                   AIA_EVENTS_SPEAKER_OPENED ),
                           0 );
        const char* payload = AiaJsonMessage_GetJsonPayload( jsonMessage );
        const char* offset;
        TEST_ASSERT_TRUE( AiaFindJsonValue(
            payload, strlen( payload ), AIA_SPEAKER_OPENED_OFFSET_KEY,
            strlen( AIA_SPEAKER_OPENED_OFFSET_KEY ), &offset, NULL ) );
        TEST_ASSERT_EQUAL( atoi( offset ), TEST_OPEN_SPEAKER_OFFSET );
        --expectedNumSpeakerOpenedEvents;
    }

    TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
        &g_observer->numSpeakerFramesPushedSemaphore, 100 ) );

    /* No overrun or exceptions should have been sent. */
    TEST_ASSERT_TRUE(
        AiaListDouble( IsEmpty )( &g_mockRegulator->writtenMessages ) );
}

TEST( AiaSpeakerManagerTests, GetCurrentOffset )
{
    /* Offset should initially be zero. */
    TEST_ASSERT_EQUAL( 0,
                       AiaSpeakerManager_GetCurrentOffset( g_speakerManager ) );

    /* Opening the speaker should not change the offset. */
    static const AiaBinaryAudioStreamOffset_t TEST_OPEN_SPEAKER_OFFSET = 0;
    const char* openSpeakerPayload =
        generateOpenSpeaker( TEST_OPEN_SPEAKER_OFFSET );
    AiaSpeakerManager_OnOpenSpeakerDirectiveReceived(
        g_speakerManager, (void*)openSpeakerPayload,
        strlen( openSpeakerPayload ), 0, 0 );

    TEST_ASSERT_EQUAL( 0,
                       AiaSpeakerManager_GetCurrentOffset( g_speakerManager ) );

    /* A marker also should not change the offset. */
    size_t markerMessageLength = 0;
    static const AiaSpeakerBinaryMarker_t TEST_MARKER_TYPE_1 = 2;
    const uint8_t* markerMessage = generateBinaryMarkerMessageEntry(
        TEST_MARKER_TYPE_1, &markerMessageLength );
    AiaSpeakerManager_OnSpeakerTopicMessageReceived(
        g_speakerManager, markerMessage, markerMessageLength, 0 );
    TEST_ASSERT_EQUAL( 0,
                       AiaSpeakerManager_GetCurrentOffset( g_speakerManager ) );

    /* Arrival of speaker data should not initially change the offset. */
    size_t binaryMessageLength = 0;
    const uint8_t* binaryMessage = generateBinaryAudioMessageEntry(
        TEST_FRAME_1, sizeof( TEST_FRAME_1 ), 0, TEST_OPEN_SPEAKER_OFFSET,
        &binaryMessageLength );
    AiaSpeakerManager_OnSpeakerTopicMessageReceived(
        g_speakerManager, binaryMessage, binaryMessageLength, 0 );
    TEST_ASSERT_EQUAL( 0,
                       AiaSpeakerManager_GetCurrentOffset( g_speakerManager ) );

    /* Once the speaker data is emitted, the offset should change corresponding.
     */
    TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
        &g_observer->numSpeakerFramesPushedSemaphore, 100 ) );
    TEST_ASSERT_EQUAL( g_observer->speakerDataReceivedSize,
                       AiaSpeakerManager_GetCurrentOffset( g_speakerManager ) );

    /* Clean up. */
    AiaFree( (void*)markerMessage );
    AiaFree( (void*)binaryMessage );
    AiaFree( (void*)openSpeakerPayload );
}

#ifdef AIA_ENABLE_ALERTS
TEST( AiaSpeakerManagerTests, OfflineAlertPlayback )
{
    AiaSpeakerManager_PlayOfflineAlert( g_speakerManager, NULL, 100 );
    AiaSpeakerManager_StopOfflineAlert( g_speakerManager );
}
#endif

TEST( AiaSpeakerManagerTests, CanSpeakerStream )
{
    TEST_ASSERT_FALSE( AiaSpeakerManager_CanSpeakerStream( g_speakerManager ) );

    static const AiaBinaryAudioStreamOffset_t TEST_OPEN_SPEAKER_OFFSET = 0;
    const char* openSpeakerPayload =
        generateOpenSpeaker( TEST_OPEN_SPEAKER_OFFSET );
    AiaSpeakerManager_OnOpenSpeakerDirectiveReceived(
        g_speakerManager, (void*)openSpeakerPayload,
        strlen( openSpeakerPayload ), 0, 0 );

    size_t binaryMessageLength = 0;
    const uint8_t* binaryMessage = generateBinaryAudioMessageEntry(
        TEST_FRAME_1, sizeof( TEST_FRAME_1 ), 0, TEST_OPEN_SPEAKER_OFFSET,
        &binaryMessageLength );
    AiaSpeakerManager_OnSpeakerTopicMessageReceived(
        g_speakerManager, binaryMessage, binaryMessageLength, 0 );

    size_t expectedFramesPushed = 1;
    while( expectedFramesPushed )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_observer->numSpeakerFramesPushedSemaphore, 100 ) );
        --expectedFramesPushed;
    }

    size_t expectedNumSpeakerOpenedEvents = 1;
    while( expectedNumSpeakerOpenedEvents )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_mockRegulator->writeSemaphore, 100 ) );
        AiaListDouble( Link_t )* link = NULL;
        link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
        AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
        TEST_ASSERT_TRUE( link );
        AiaJsonMessage_t* jsonMessage = AiaJsonMessage_FromMessage(
            ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
        TEST_ASSERT_EQUAL( strcmp( AiaJsonMessage_GetName( jsonMessage ),
                                   AIA_EVENTS_SPEAKER_OPENED ),
                           0 );
        const char* payload = AiaJsonMessage_GetJsonPayload( jsonMessage );
        const char* offset;
        TEST_ASSERT_TRUE( AiaFindJsonValue(
            payload, strlen( payload ), AIA_SPEAKER_OPENED_OFFSET_KEY,
            strlen( AIA_SPEAKER_OPENED_OFFSET_KEY ), &offset, NULL ) );
        TEST_ASSERT_EQUAL( atoi( offset ), TEST_OPEN_SPEAKER_OFFSET );
        --expectedNumSpeakerOpenedEvents;
    }

    TEST_ASSERT_TRUE( AiaSpeakerManager_CanSpeakerStream( g_speakerManager ) );
    TEST_ASSERT_TRUE(
        AiaListDouble( IsEmpty )( &g_mockRegulator->writtenMessages ) );
    TEST_ASSERT_FALSE( AiaSemaphore( TimedWait )(
        &g_mockSequencer->resetSequenceNumberSemaphore, 100 ) );

    AiaFree( (void*)binaryMessage );
    AiaFree( (void*)openSpeakerPayload );
}
