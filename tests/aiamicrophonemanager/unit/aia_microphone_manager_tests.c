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
 * @file aia_microphone_manager_tests.c
 * @brief Tests for AiaMicrophoneManager_t.
 */

/* The config header is always included first. */
#include <aia_config.h>

/* Aia headers */
#include <aiacore/aia_binary_constants.h>
#include <aiacore/aia_binary_message.h>
#include <aiacore/aia_events.h>
#include <aiacore/aia_json_constants.h>
#include <aiacore/aia_json_message.h>
#include <aiacore/aia_json_utils.h>
#include <aiacore/aia_mbedtls_threading.h>
#include <aiacore/aia_random_mbedtls.h>
#include <aiacore/data_stream_buffer/aia_data_stream_buffer.h>
#include <aiacore/data_stream_buffer/aia_data_stream_buffer_reader.h>
#include <aiamicrophonemanager/aia_microphone_constants.h>
#include <aiamicrophonemanager/aia_microphone_manager.h>
#include <aiamicrophonemanager/private/aia_microphone_manager.h>

#include <aiamockregulator/aia_mock_regulator.h>
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

static AiaBinaryAudioStreamOffset_t getStreamOffsetFromData(
    const uint8_t* data )
{
    AiaBinaryAudioStreamOffset_t offset = 0;
    for( size_t i = 0; i < sizeof( AiaBinaryAudioStreamOffset_t ); ++i )
    {
        offset |= data[ i ] << ( i * 8 );
    }
    return offset;
}

#define BUFFER_SAMPLES_CAPACITY 32000

static const AiaDurationMs_t LEEWAY = 10;

typedef struct AiaTestMicrophoneStateObserver
{
    /* TODO: ADSER-1595 Replace with mechanism that allows for running unit
     * tests in non-threaded environments. */
    AiaSemaphore_t currentStateChanged;
    AiaMicrophoneState_t currentState;
} AiaTestMicrophoneStateObserver_t;

static void AiaOnMicrophoneStateChanged( AiaMicrophoneState_t state,
                                         void* userData )
{
    TEST_ASSERT_NOT_NULL( userData );
    AiaTestMicrophoneStateObserver_t* observer =
        (AiaTestMicrophoneStateObserver_t*)userData;
    observer->currentState = state;
    AiaSemaphore( Post )( &observer->currentStateChanged );
}

static AiaTestMicrophoneStateObserver_t* AiaTestMicrophoneStateObserver_Create()
{
    AiaTestMicrophoneStateObserver_t* observer =
        AiaCalloc( 1, sizeof( AiaTestMicrophoneStateObserver_t ) );
    TEST_ASSERT_NOT_NULL( observer );
    TEST_ASSERT_TRUE(
        AiaSemaphore( Create )( &observer->currentStateChanged, 0, 1000 ) );
    return observer;
}

static void AiaTestMicrophoneStateObserver_Destroy(
    AiaTestMicrophoneStateObserver_t* observer )
{
    AiaSemaphore( Destroy )( &observer->currentStateChanged );
    AiaFree( observer );
}

static const char* OPEN_MICROPHONE_PAYLOAD_WITHOUT_INITIATOR_FORMAT =
    /* clang-format off */
"{"
    "\""AIA_OPEN_MICROPHONE_TIMEOUT_IN_MILLISECONDS_KEY"\":%"PRIu32
"}";
/* clang-format on */

static const char* OPEN_MICROPHONE_PAYLOAD_WITH_INITIATOR_FORMAT =
    /* clang-format off */
"{"
    "\""AIA_OPEN_MICROPHONE_TIMEOUT_IN_MILLISECONDS_KEY"\":%"PRIu32","
    "\""AIA_OPEN_MICROPHONE_INITIATOR_KEY"\":%s"
"}";
/* clang-format on */

/** Callers must clean up non NULL returned values using AiaFree(). Initiator is
 * optional. */
const char* generateOpenMicrophone( AiaDurationMs_t timeoutInMilliseconds,
                                    const char* initiator )
{
    char* buf = AiaCalloc( 50, sizeof( char ) );
    TEST_ASSERT_NOT_NULL( buf );
    if( !initiator )
    {
        sprintf( buf, OPEN_MICROPHONE_PAYLOAD_WITHOUT_INITIATOR_FORMAT,
                 timeoutInMilliseconds );
    }
    else
    {
        sprintf( buf, OPEN_MICROPHONE_PAYLOAD_WITH_INITIATOR_FORMAT,
                 timeoutInMilliseconds, initiator );
    }
    return buf;
}

static AiaMockRegulator_t* g_mockEventRegulator;

static AiaRegulator_t* g_eventRegulator;

static AiaMockRegulator_t* g_mockMicrophoneRegulator;

static AiaRegulator_t* g_microphoneRegulator;

static void* buffer;

static AiaDataStreamBuffer_t* sds;

static AiaDataStreamReader_t* reader;

static AiaDataStreamWriter_t* writer;

static AiaMicrophoneManager_t* microphoneManager;

static AiaTestMicrophoneStateObserver_t* testObserver;

/*-----------------------------------------------------------*/

/**
 * @brief Test group for AiaMicrophoneManager_t tests.
 */
TEST_GROUP( AiaMicrophoneManagerTests );

/*-----------------------------------------------------------*/

/**
 * @brief Test group runner for AiaMicrophoneManager_t tests.
 */
TEST_GROUP_RUNNER( AiaMicrophoneManagerTests )
{
    RUN_TEST_CASE( AiaMicrophoneManagerTests, Creation );
    RUN_TEST_CASE( AiaMicrophoneManagerTests, HoldToTalkFollowedByLocalEnd );
    RUN_TEST_CASE( AiaMicrophoneManagerTests,
                   TapToTalkFollowedByCloseMicrophone );
    RUN_TEST_CASE( AiaMicrophoneManagerTests,
                   WakeWordFollowedByCloseMicrophone );
    RUN_TEST_CASE( AiaMicrophoneManagerTests, WakeWordNonAlexaFails );
    RUN_TEST_CASE( AiaMicrophoneManagerTests,
                   WakeWordWithoutAmpleSamplesFails );
    RUN_TEST_CASE( AiaMicrophoneManagerTests, HoldToTalkOpenMicrophoneTimeout );
    RUN_TEST_CASE( AiaMicrophoneManagerTests,
                   HoldToTalkOpenMicrophoneWithinTimeout );
    RUN_TEST_CASE( AiaMicrophoneManagerTests,
                   OpenMicrophoneWithInitiatorEchoedBack )
}

/*-----------------------------------------------------------*/
/**
 * @brief Test setup for AiaMicrophoneManager_t tests.
 */
TEST_SETUP( AiaMicrophoneManagerTests )
{
    AiaMbedtlsThreading_Init();
    AiaRandomMbedtls_Init();
    TEST_ASSERT_TRUE( AiaRandomMbedtls_Seed( TEST_SALT, TEST_SALT_LENGTH ) );

    g_mockEventRegulator = AiaMockRegulator_Create();
    TEST_ASSERT_NOT_NULL( g_mockEventRegulator );
    g_eventRegulator = (AiaRegulator_t*)g_mockEventRegulator;

    g_mockMicrophoneRegulator = AiaMockRegulator_Create();
    TEST_ASSERT_NOT_NULL( g_mockMicrophoneRegulator );
    g_microphoneRegulator = (AiaRegulator_t*)g_mockMicrophoneRegulator;

    static const size_t MAXREADERS = 1;

    /* Initialize the buffer. */
    size_t bufferSize =
        AIA_MICROPHONE_BUFFER_WORD_SIZE * BUFFER_SAMPLES_CAPACITY;
    buffer = AiaCalloc( 1, bufferSize );
    TEST_ASSERT_NOT_NULL( buffer );
    sds = AiaDataStreamBuffer_Create(
        buffer, bufferSize, AIA_MICROPHONE_BUFFER_WORD_SIZE, MAXREADERS );
    TEST_ASSERT_NOT_NULL( sds );

    reader = AiaDataStreamBuffer_CreateReader(
        sds, AIA_DATA_STREAM_BUFFER_READER_NONBLOCKING, false );
    TEST_ASSERT_NOT_NULL( reader );

    writer = AiaDataStreamBuffer_CreateWriter(
        sds, AIA_DATA_STREAM_BUFFER_WRITER_NONBLOCKABLE, false );
    TEST_ASSERT_NOT_NULL( writer );

    uint16_t TEST_SAMPLES[ BUFFER_SAMPLES_CAPACITY ];
    for( size_t i = 0; i < BUFFER_SAMPLES_CAPACITY; ++i )
    {
        TEST_SAMPLES[ i ] = i;
    }
    ssize_t samplesWritten = AiaDataStreamWriter_Write(
        writer, TEST_SAMPLES, BUFFER_SAMPLES_CAPACITY );
    TEST_ASSERT_EQUAL( samplesWritten, BUFFER_SAMPLES_CAPACITY );

    testObserver = AiaTestMicrophoneStateObserver_Create();

    microphoneManager = AiaMicrophoneManager_Create(
        g_eventRegulator, g_microphoneRegulator, reader,
        AiaOnMicrophoneStateChanged, testObserver );
    TEST_ASSERT_NOT_NULL( microphoneManager );
    TEST_ASSERT_EQUAL( testObserver->currentState,
                       AIA_MICROPHONE_STATE_CLOSED );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test tear down for AiaMicrophoneManager_t tests.
 */
TEST_TEAR_DOWN( AiaMicrophoneManagerTests )
{
    AiaMicrophonerManager_Destroy( microphoneManager );
    AiaTestMicrophoneStateObserver_Destroy( testObserver );
    AiaDataStreamReader_Destroy( reader );
    AiaDataStreamBuffer_Destroy( sds );
    AiaFree( buffer );
    AiaMockRegulator_Destroy( g_mockMicrophoneRegulator,
                              AiaTestUtilities_DestroyBinaryChunk, NULL );
    AiaMockRegulator_Destroy( g_mockEventRegulator,
                              AiaTestUtilities_DestroyJsonChunk, NULL );

    AiaMbedtlsThreading_Cleanup();
    AiaRandomMbedtls_Cleanup();
}

/*-----------------------------------------------------------*/

TEST( AiaMicrophoneManagerTests, Creation )
{
    AiaMicrophoneManager_t* invalidMicrophoneManager = NULL;

    invalidMicrophoneManager = AiaMicrophoneManager_Create(
        NULL, NULL, NULL, AiaOnMicrophoneStateChanged, testObserver );
    TEST_ASSERT_NULL( invalidMicrophoneManager );
    invalidMicrophoneManager = AiaMicrophoneManager_Create(
        g_eventRegulator, NULL, NULL, AiaOnMicrophoneStateChanged,
        testObserver );
    TEST_ASSERT_NULL( invalidMicrophoneManager );
    invalidMicrophoneManager = AiaMicrophoneManager_Create(
        NULL, g_microphoneRegulator, NULL, AiaOnMicrophoneStateChanged,
        testObserver );
    TEST_ASSERT_NULL( invalidMicrophoneManager );
    invalidMicrophoneManager = AiaMicrophoneManager_Create(
        NULL, NULL, reader, AiaOnMicrophoneStateChanged, testObserver );
    TEST_ASSERT_NULL( invalidMicrophoneManager );
    invalidMicrophoneManager = AiaMicrophoneManager_Create(
        g_eventRegulator, g_microphoneRegulator, NULL,
        AiaOnMicrophoneStateChanged, testObserver );
    TEST_ASSERT_NULL( invalidMicrophoneManager );
    invalidMicrophoneManager = AiaMicrophoneManager_Create(
        g_eventRegulator, NULL, reader, AiaOnMicrophoneStateChanged,
        testObserver );
    TEST_ASSERT_NULL( invalidMicrophoneManager );
    invalidMicrophoneManager = AiaMicrophoneManager_Create(
        NULL, g_microphoneRegulator, reader, AiaOnMicrophoneStateChanged,
        testObserver );
    TEST_ASSERT_NULL( invalidMicrophoneManager );
}

TEST( AiaMicrophoneManagerTests, HoldToTalkFollowedByLocalEnd )
{
    const AiaDataStreamIndex_t BUFFER_START_INDEX = 500;
    AiaBinaryAudioStreamOffset_t streamOffset = 0;

    TEST_ASSERT_TRUE( AiaMicrophoneManager_HoldToTalkStart(
        microphoneManager, BUFFER_START_INDEX ) );
    TEST_ASSERT_TRUE(
        AiaSemaphore( TryWait )( &testObserver->currentStateChanged ) );
    TEST_ASSERT_EQUAL( testObserver->currentState, AIA_MICROPHONE_STATE_OPEN );

    TEST_ASSERT_TRUE(
        AiaSemaphore( TryWait )( &g_mockEventRegulator->writeSemaphore ) );
    AiaListDouble( Link_t )* link = NULL;
    link = AiaListDouble( PeekHead )( &g_mockEventRegulator->writtenMessages );
    AiaListDouble( RemoveHead )( &g_mockEventRegulator->writtenMessages );
    TEST_ASSERT_NOT_NULL( link );
    AiaJsonMessage_t* microphoneOpenedMessage = AiaJsonMessage_FromMessage(
        ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
    TEST_ASSERT_EQUAL(
        strcmp( AiaJsonMessage_GetName( microphoneOpenedMessage ),
                AIA_EVENTS_MICROPHONE_OPENED ),
        0 );
    const char* payload =
        AiaJsonMessage_GetJsonPayload( microphoneOpenedMessage );
    TEST_ASSERT_NOT_NULL( payload );

    const char* profile = NULL;
    size_t profileLen;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_MICROPHONE_OPENED_PROFILE_KEY,
        strlen( AIA_MICROPHONE_OPENED_PROFILE_KEY ), &profile, &profileLen ) );
    TEST_ASSERT_NOT_NULL( profile );
    TEST_ASSERT_TRUE( AiaJsonUtils_UnquoteString( &profile, &profileLen ) );
    TEST_ASSERT_EQUAL_STRING_LEN(
        AiaMicrophoneProfile_ToString( AIA_MICROPHONE_PROFILE_CLOSE_TALK ),
        profile, profileLen );

    const char* microphoneOpenedOffset = NULL;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_MICROPHONE_OPENED_OFFSET_KEY,
        strlen( AIA_MICROPHONE_OPENED_OFFSET_KEY ), &microphoneOpenedOffset,
        NULL ) );
    TEST_ASSERT_NOT_NULL( microphoneOpenedOffset );
    TEST_ASSERT_EQUAL( atoi( microphoneOpenedOffset ), streamOffset );

    const char* initiator = NULL;
    size_t initiatorLen;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_MICROPHONE_OPENED_INITIATOR_KEY,
        strlen( AIA_MICROPHONE_OPENED_INITIATOR_KEY ), &initiator,
        &initiatorLen ) );
    TEST_ASSERT_NOT_NULL( initiator );

    const char* type = NULL;
    size_t typeLen;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        initiator, initiatorLen, AIA_MICROPHONE_OPENED_INITIATOR_TYPE_KEY,
        strlen( AIA_MICROPHONE_OPENED_INITIATOR_TYPE_KEY ), &type, &typeLen ) );
    TEST_ASSERT_NOT_NULL( type );
    TEST_ASSERT_TRUE( AiaJsonUtils_UnquoteString( &type, &typeLen ) );
    TEST_ASSERT_EQUAL_STRING_LEN( AiaMicrophoneInitiatorType_ToString(
                                      AIA_MICROPHONE_INITIATOR_TYPE_HOLD ),
                                  type, typeLen );

    size_t numIterationsUntilStop = 4;
    const AiaDataStreamIndex_t BUFFER_STOP_INDEX =
        BUFFER_START_INDEX +
        ( numIterationsUntilStop * AIA_MICROPHONE_CHUNK_SIZE_SAMPLES );
    AiaDataStreamIndex_t bufferCurrentIndex = BUFFER_START_INDEX;
    while( bufferCurrentIndex < BUFFER_STOP_INDEX )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_mockMicrophoneRegulator->writeSemaphore,
            MICROPHONE_PUBLISH_RATE + LEEWAY ) );
        link = NULL;
        link = AiaListDouble( PeekHead )(
            &g_mockMicrophoneRegulator->writtenMessages );
        AiaListDouble( RemoveHead )(
            &g_mockMicrophoneRegulator->writtenMessages );
        TEST_ASSERT_NOT_NULL( link );
        AiaBinaryMessage_t* binaryMessage = AiaBinaryMessage_FromMessage(
            ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
        TEST_ASSERT_EQUAL( AiaBinaryMessage_GetType( binaryMessage ),
                           AIA_BINARY_STREAM_MICROPHONE_CONTENT_TYPE );
        TEST_ASSERT_EQUAL( AiaBinaryMessage_GetCount( binaryMessage ), 0 );
        TEST_ASSERT_EQUAL( AiaBinaryMessage_GetLength( binaryMessage ),
                           sizeof( AiaBinaryAudioStreamOffset_t ) +
                               ( AIA_MICROPHONE_CHUNK_SIZE_SAMPLES *
                                 AIA_MICROPHONE_BUFFER_WORD_SIZE ) );
        const uint8_t* data = AiaBinaryMessage_GetData( binaryMessage );
        TEST_ASSERT_NOT_NULL( data );
        TEST_ASSERT_EQUAL( getStreamOffsetFromData( data ), streamOffset );
        size_t dataLenInBytes = AiaBinaryMessage_GetLength( binaryMessage ) -
                                sizeof( AiaBinaryAudioStreamOffset_t );

        const uint16_t* dataInSamples =
            (const uint16_t*)( data + sizeof( AiaBinaryAudioStreamOffset_t ) );
        for( size_t i = 0; i < AIA_MICROPHONE_CHUNK_SIZE_SAMPLES; ++i )
        {
            TEST_ASSERT_EQUAL( dataInSamples[ i ], bufferCurrentIndex + i );
        }

        streamOffset += dataLenInBytes;
        bufferCurrentIndex +=
            ( dataLenInBytes / AIA_MICROPHONE_BUFFER_WORD_SIZE );
    }

    AiaMicrophoneManager_CloseMicrophone( microphoneManager );
    TEST_ASSERT_FALSE( AiaSemaphore( TimedWait )(
        &g_mockMicrophoneRegulator->writeSemaphore, MICROPHONE_PUBLISH_RATE ) );

    TEST_ASSERT_TRUE(
        AiaSemaphore( TryWait )( &g_mockEventRegulator->writeSemaphore ) );
    link = NULL;
    link = AiaListDouble( PeekHead )( &g_mockEventRegulator->writtenMessages );
    AiaListDouble( RemoveHead )( &g_mockEventRegulator->writtenMessages );
    TEST_ASSERT_NOT_NULL( link );
    AiaJsonMessage_t* microphoneClosedMessage = AiaJsonMessage_FromMessage(
        ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
    TEST_ASSERT_EQUAL(
        strcmp( AiaJsonMessage_GetName( microphoneClosedMessage ),
                AIA_EVENTS_MICROPHONE_CLOSED ),
        0 );
    payload = AiaJsonMessage_GetJsonPayload( microphoneClosedMessage );
    TEST_ASSERT_NOT_NULL( payload );

    const char* microphoneClosedOffset = NULL;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_MICROPHONE_CLOSED_OFFSET_KEY,
        strlen( AIA_MICROPHONE_CLOSED_OFFSET_KEY ), &microphoneClosedOffset,
        NULL ) );
    TEST_ASSERT_NOT_NULL( microphoneClosedOffset );
    TEST_ASSERT_EQUAL( atoi( microphoneClosedOffset ), streamOffset );
}

TEST( AiaMicrophoneManagerTests, TapToTalkFollowedByCloseMicrophone )
{
    AiaMicrophoneProfile_t TEST_PROFILE = AIA_MICROPHONE_PROFILE_NEAR_FIELD;
    const AiaDataStreamIndex_t BUFFER_START_INDEX = 500;
    AiaBinaryAudioStreamOffset_t streamOffset = 0;

    TEST_ASSERT_TRUE( AiaMicrophoneManager_TapToTalkStart(
        microphoneManager, BUFFER_START_INDEX, TEST_PROFILE ) );
    TEST_ASSERT_TRUE(
        AiaSemaphore( TryWait )( &testObserver->currentStateChanged ) );
    TEST_ASSERT_EQUAL( testObserver->currentState, AIA_MICROPHONE_STATE_OPEN );

    TEST_ASSERT_TRUE(
        AiaSemaphore( TryWait )( &g_mockEventRegulator->writeSemaphore ) );
    AiaListDouble( Link_t )* link = NULL;
    link = AiaListDouble( PeekHead )( &g_mockEventRegulator->writtenMessages );
    AiaListDouble( RemoveHead )( &g_mockEventRegulator->writtenMessages );
    TEST_ASSERT_NOT_NULL( link );
    AiaJsonMessage_t* microphoneOpenedMessage = AiaJsonMessage_FromMessage(
        ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
    TEST_ASSERT_EQUAL(
        strcmp( AiaJsonMessage_GetName( microphoneOpenedMessage ),
                AIA_EVENTS_MICROPHONE_OPENED ),
        0 );
    const char* payload =
        AiaJsonMessage_GetJsonPayload( microphoneOpenedMessage );
    TEST_ASSERT_NOT_NULL( payload );

    const char* profile = NULL;
    size_t profileLen;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_MICROPHONE_OPENED_PROFILE_KEY,
        strlen( AIA_MICROPHONE_OPENED_PROFILE_KEY ), &profile, &profileLen ) );
    TEST_ASSERT_NOT_NULL( profile );
    TEST_ASSERT_TRUE( AiaJsonUtils_UnquoteString( &profile, &profileLen ) );
    TEST_ASSERT_EQUAL_STRING_LEN( AiaMicrophoneProfile_ToString( TEST_PROFILE ),
                                  profile, profileLen );

    const char* microphoneOpenedOffset = NULL;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_MICROPHONE_OPENED_OFFSET_KEY,
        strlen( AIA_MICROPHONE_OPENED_OFFSET_KEY ), &microphoneOpenedOffset,
        NULL ) );
    TEST_ASSERT_NOT_NULL( microphoneOpenedOffset );
    TEST_ASSERT_EQUAL( atoi( microphoneOpenedOffset ), streamOffset );

    const char* initiator = NULL;
    size_t initiatorLen;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_MICROPHONE_OPENED_INITIATOR_KEY,
        strlen( AIA_MICROPHONE_OPENED_INITIATOR_KEY ), &initiator,
        &initiatorLen ) );
    TEST_ASSERT_NOT_NULL( initiator );

    const char* type = NULL;
    size_t typeLen;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        initiator, initiatorLen, AIA_MICROPHONE_OPENED_INITIATOR_TYPE_KEY,
        strlen( AIA_MICROPHONE_OPENED_INITIATOR_TYPE_KEY ), &type, &typeLen ) );
    TEST_ASSERT_NOT_NULL( type );
    TEST_ASSERT_TRUE( AiaJsonUtils_UnquoteString( &type, &typeLen ) );
    TEST_ASSERT_EQUAL_STRING_LEN( AiaMicrophoneInitiatorType_ToString(
                                      AIA_MICROPHONE_INITIATOR_TYPE_TAP ),
                                  type, typeLen );

    const AiaDataStreamIndex_t BUFFER_STOP_INDEX =
        BUFFER_START_INDEX + ( 4 * AIA_MICROPHONE_CHUNK_SIZE_SAMPLES );
    AiaDataStreamIndex_t bufferCurrentIndex = BUFFER_START_INDEX;
    while( bufferCurrentIndex < BUFFER_STOP_INDEX )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_mockMicrophoneRegulator->writeSemaphore,
            MICROPHONE_PUBLISH_RATE + LEEWAY ) );
        link = NULL;
        link = AiaListDouble( PeekHead )(
            &g_mockMicrophoneRegulator->writtenMessages );
        AiaListDouble( RemoveHead )(
            &g_mockMicrophoneRegulator->writtenMessages );
        TEST_ASSERT_NOT_NULL( link );
        AiaBinaryMessage_t* binaryMessage = AiaBinaryMessage_FromMessage(
            ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
        TEST_ASSERT_EQUAL( AiaBinaryMessage_GetType( binaryMessage ),
                           AIA_BINARY_STREAM_MICROPHONE_CONTENT_TYPE );
        TEST_ASSERT_EQUAL( AiaBinaryMessage_GetCount( binaryMessage ), 0 );
        TEST_ASSERT_EQUAL( AiaBinaryMessage_GetLength( binaryMessage ),
                           sizeof( AiaBinaryAudioStreamOffset_t ) +
                               ( AIA_MICROPHONE_CHUNK_SIZE_SAMPLES *
                                 AIA_MICROPHONE_BUFFER_WORD_SIZE ) );
        const uint8_t* data = AiaBinaryMessage_GetData( binaryMessage );
        TEST_ASSERT_NOT_NULL( data );
        TEST_ASSERT_EQUAL( getStreamOffsetFromData( data ), streamOffset );
        size_t dataLenInBytes = AiaBinaryMessage_GetLength( binaryMessage ) -
                                sizeof( AiaBinaryAudioStreamOffset_t );

        const uint16_t* dataInSamples =
            (const uint16_t*)( data + sizeof( AiaBinaryAudioStreamOffset_t ) );
        for( size_t i = 0; i < AIA_MICROPHONE_CHUNK_SIZE_SAMPLES; ++i )
        {
            TEST_ASSERT_EQUAL( dataInSamples[ i ], bufferCurrentIndex + i );
        }

        streamOffset += dataLenInBytes;
        bufferCurrentIndex +=
            ( dataLenInBytes / AIA_MICROPHONE_BUFFER_WORD_SIZE );
    }

    AiaMicrophoneManager_OnCloseMicrophoneDirectiveReceived( microphoneManager,
                                                             NULL, 0, 0, 0 );
    TEST_ASSERT_FALSE( AiaSemaphore( TimedWait )(
        &g_mockMicrophoneRegulator->writeSemaphore, MICROPHONE_PUBLISH_RATE ) );

    TEST_ASSERT_TRUE(
        AiaSemaphore( TryWait )( &g_mockEventRegulator->writeSemaphore ) );
    link = NULL;
    link = AiaListDouble( PeekHead )( &g_mockEventRegulator->writtenMessages );
    AiaListDouble( RemoveHead )( &g_mockEventRegulator->writtenMessages );
    TEST_ASSERT_NOT_NULL( link );
    AiaJsonMessage_t* microphoneClosedMessage = AiaJsonMessage_FromMessage(
        ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
    TEST_ASSERT_EQUAL(
        strcmp( AiaJsonMessage_GetName( microphoneClosedMessage ),
                AIA_EVENTS_MICROPHONE_CLOSED ),
        0 );
    payload = AiaJsonMessage_GetJsonPayload( microphoneClosedMessage );
    TEST_ASSERT_NOT_NULL( payload );

    const char* microphoneClosedOffset = NULL;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_MICROPHONE_CLOSED_OFFSET_KEY,
        strlen( AIA_MICROPHONE_CLOSED_OFFSET_KEY ), &microphoneClosedOffset,
        NULL ) );
    TEST_ASSERT_NOT_NULL( microphoneClosedOffset );
    TEST_ASSERT_EQUAL( atoi( microphoneClosedOffset ), streamOffset );
}

TEST( AiaMicrophoneManagerTests, WakeWordFollowedByCloseMicrophone )
{
    AiaMicrophoneProfile_t TEST_PROFILE = AIA_MICROPHONE_PROFILE_FAR_FIELD;
    const AiaDataStreamIndex_t BUFFER_START_INDEX = 500;
    const AiaDataStreamIndex_t WW_BUFFER_START_INDEX =
        BUFFER_START_INDEX + AIA_MICROPHONE_WAKE_WORD_PREROLL_IN_SAMPLES;
    const AiaBinaryAudioStreamOffset_t WW_BUFFER_END_INDEX =
        WW_BUFFER_START_INDEX + 8000;
    AiaBinaryAudioStreamOffset_t streamOffset = 0;

    TEST_ASSERT_TRUE( AiaMicrophoneManager_WakeWordStart(
        microphoneManager, WW_BUFFER_START_INDEX, WW_BUFFER_END_INDEX,
        TEST_PROFILE, AIA_ALEXA_WAKE_WORD ) );
    TEST_ASSERT_TRUE(
        AiaSemaphore( TryWait )( &testObserver->currentStateChanged ) );
    TEST_ASSERT_EQUAL( testObserver->currentState, AIA_MICROPHONE_STATE_OPEN );

    TEST_ASSERT_TRUE(
        AiaSemaphore( TryWait )( &g_mockEventRegulator->writeSemaphore ) );
    AiaListDouble( Link_t )* link = NULL;
    link = AiaListDouble( PeekHead )( &g_mockEventRegulator->writtenMessages );
    AiaListDouble( RemoveHead )( &g_mockEventRegulator->writtenMessages );
    TEST_ASSERT_NOT_NULL( link );
    AiaJsonMessage_t* microphoneOpenedMessage = AiaJsonMessage_FromMessage(
        ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
    TEST_ASSERT_EQUAL(
        strcmp( AiaJsonMessage_GetName( microphoneOpenedMessage ),
                AIA_EVENTS_MICROPHONE_OPENED ),
        0 );
    const char* payload =
        AiaJsonMessage_GetJsonPayload( microphoneOpenedMessage );
    TEST_ASSERT_NOT_NULL( payload );

    const char* profile = NULL;
    size_t profileLen = 0;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_MICROPHONE_OPENED_PROFILE_KEY,
        strlen( AIA_MICROPHONE_OPENED_PROFILE_KEY ), &profile, &profileLen ) );
    TEST_ASSERT_NOT_NULL( profile );
    TEST_ASSERT_TRUE( AiaJsonUtils_UnquoteString( &profile, &profileLen ) );
    TEST_ASSERT_EQUAL_STRING_LEN( AiaMicrophoneProfile_ToString( TEST_PROFILE ),
                                  profile, profileLen );

    const char* microphoneOpenedOffset = NULL;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_MICROPHONE_OPENED_OFFSET_KEY,
        strlen( AIA_MICROPHONE_OPENED_OFFSET_KEY ), &microphoneOpenedOffset,
        NULL ) );
    TEST_ASSERT_NOT_NULL( microphoneOpenedOffset );
    TEST_ASSERT_EQUAL( atoi( microphoneOpenedOffset ), streamOffset );

    const char* initiator = NULL;
    size_t initiatorLen = 0;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_MICROPHONE_OPENED_INITIATOR_KEY,
        strlen( AIA_MICROPHONE_OPENED_INITIATOR_KEY ), &initiator,
        &initiatorLen ) );
    TEST_ASSERT_NOT_NULL( initiator );

    const char* type = NULL;
    size_t typeLen = 0;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        initiator, initiatorLen, AIA_MICROPHONE_OPENED_INITIATOR_TYPE_KEY,
        strlen( AIA_MICROPHONE_OPENED_INITIATOR_TYPE_KEY ), &type, &typeLen ) );
    TEST_ASSERT_NOT_NULL( type );
    TEST_ASSERT_TRUE( AiaJsonUtils_UnquoteString( &type, &typeLen ) );
    TEST_ASSERT_EQUAL_STRING_LEN( AiaMicrophoneInitiatorType_ToString(
                                      AIA_MICROPHONE_INITIATOR_TYPE_WAKEWORD ),
                                  type, typeLen );

    const char* initiatorPayload = NULL;
    size_t initiatorPayloadLen = 0;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        initiator, initiatorLen, AIA_OPEN_MICROPHONE_INITIATOR_PAYLOAD_KEY,
        strlen( AIA_OPEN_MICROPHONE_INITIATOR_PAYLOAD_KEY ), &initiatorPayload,
        &initiatorPayloadLen ) );
    TEST_ASSERT_NOT_NULL( initiatorPayload );

    const char* initiatorPayloadWakeword = NULL;
    size_t initiatorPayloadWakewordLen = 0;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        initiatorPayload, initiatorPayloadLen,
        AIA_MICROPHONE_OPENED_INITIATOR_PAYLOAD_WAKE_WORD_KEY,
        strlen( AIA_MICROPHONE_OPENED_INITIATOR_PAYLOAD_WAKE_WORD_KEY ),
        &initiatorPayloadWakeword, &initiatorPayloadWakewordLen ) );
    TEST_ASSERT_NOT_NULL( initiatorPayloadWakeword );
    TEST_ASSERT_TRUE( AiaJsonUtils_UnquoteString(
        &initiatorPayloadWakeword, &initiatorPayloadWakewordLen ) );
    TEST_ASSERT_EQUAL_STRING_LEN( AIA_ALEXA_WAKE_WORD, initiatorPayloadWakeword,
                                  initiatorPayloadWakewordLen );

    const char* initiatorPayloadWakewordIndices = NULL;
    size_t initiatorPayloadWakewordIndicesLen = 0;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        initiatorPayload, initiatorPayloadLen,
        AIA_MICROPHONE_OPENED_INITIATOR_PAYLOAD_WAKE_WORD_INDICES_KEY,
        strlen( AIA_MICROPHONE_OPENED_INITIATOR_PAYLOAD_WAKE_WORD_INDICES_KEY ),
        &initiatorPayloadWakewordIndices,
        &initiatorPayloadWakewordIndicesLen ) );
    TEST_ASSERT_NOT_NULL( initiatorPayload );

    const char* wakeWordBeginOffset = NULL;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        initiatorPayloadWakewordIndices, initiatorPayloadWakewordIndicesLen,
        AIA_MICROPHONE_OPENED_INITIATOR_PAYLOAD_WAKE_WORD_INDICES_BEGIN_OFFSET_KEY,
        strlen(
            AIA_MICROPHONE_OPENED_INITIATOR_PAYLOAD_WAKE_WORD_INDICES_BEGIN_OFFSET_KEY ),
        &wakeWordBeginOffset, NULL ) );
    TEST_ASSERT_NOT_NULL( wakeWordBeginOffset );
    TEST_ASSERT_EQUAL(
        ( streamOffset + ( ( WW_BUFFER_START_INDEX - BUFFER_START_INDEX ) *
                           AIA_MICROPHONE_BUFFER_WORD_SIZE ) ),
        atoi( wakeWordBeginOffset ) );

    const char* wakeWordEndOffset = NULL;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        initiatorPayloadWakewordIndices, initiatorPayloadWakewordIndicesLen,
        AIA_MICROPHONE_OPENED_INITIATOR_PAYLOAD_WAKE_WORD_INDICES_END_OFFSET_KEY,
        strlen(
            AIA_MICROPHONE_OPENED_INITIATOR_PAYLOAD_WAKE_WORD_INDICES_END_OFFSET_KEY ),
        &wakeWordEndOffset, NULL ) );
    TEST_ASSERT_NOT_NULL( wakeWordEndOffset );
    TEST_ASSERT_EQUAL(
        ( streamOffset + WW_BUFFER_END_INDEX - BUFFER_START_INDEX ) *
            AIA_MICROPHONE_BUFFER_WORD_SIZE,
        atoi( wakeWordEndOffset ) );

    const AiaBinaryAudioStreamOffset_t BUFFER_STOP_INDEX =
        BUFFER_START_INDEX + ( 4 * AIA_MICROPHONE_CHUNK_SIZE_SAMPLES );
    AiaBinaryAudioStreamOffset_t bufferCurrentIndex = BUFFER_START_INDEX;
    while( bufferCurrentIndex < BUFFER_STOP_INDEX )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_mockMicrophoneRegulator->writeSemaphore,
            MICROPHONE_PUBLISH_RATE + LEEWAY ) );
        link = NULL;
        link = AiaListDouble( PeekHead )(
            &g_mockMicrophoneRegulator->writtenMessages );
        AiaListDouble( RemoveHead )(
            &g_mockMicrophoneRegulator->writtenMessages );
        TEST_ASSERT_NOT_NULL( link );
        AiaBinaryMessage_t* binaryMessage = AiaBinaryMessage_FromMessage(
            ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
        TEST_ASSERT_EQUAL( AiaBinaryMessage_GetType( binaryMessage ),
                           AIA_BINARY_STREAM_MICROPHONE_CONTENT_TYPE );
        TEST_ASSERT_EQUAL( AiaBinaryMessage_GetCount( binaryMessage ), 0 );
        TEST_ASSERT_EQUAL( AiaBinaryMessage_GetLength( binaryMessage ),
                           sizeof( AiaBinaryAudioStreamOffset_t ) +
                               ( AIA_MICROPHONE_CHUNK_SIZE_SAMPLES *
                                 AIA_MICROPHONE_BUFFER_WORD_SIZE ) );
        const uint8_t* data = AiaBinaryMessage_GetData( binaryMessage );
        TEST_ASSERT_NOT_NULL( data );
        TEST_ASSERT_EQUAL( getStreamOffsetFromData( data ), streamOffset );
        size_t dataLenInBytes = AiaBinaryMessage_GetLength( binaryMessage ) -
                                sizeof( AiaBinaryAudioStreamOffset_t );

        const uint16_t* dataInSamples =
            (const uint16_t*)( data + sizeof( AiaBinaryAudioStreamOffset_t ) );
        for( size_t i = 0; i < AIA_MICROPHONE_CHUNK_SIZE_SAMPLES; ++i )
        {
            TEST_ASSERT_EQUAL( dataInSamples[ i ], bufferCurrentIndex + i );
        }

        streamOffset += dataLenInBytes;
        bufferCurrentIndex +=
            ( dataLenInBytes / AIA_MICROPHONE_BUFFER_WORD_SIZE );
    }

    AiaMicrophoneManager_OnCloseMicrophoneDirectiveReceived( microphoneManager,
                                                             NULL, 0, 0, 0 );
    TEST_ASSERT_FALSE( AiaSemaphore( TimedWait )(
        &g_mockMicrophoneRegulator->writeSemaphore, MICROPHONE_PUBLISH_RATE ) );

    TEST_ASSERT_TRUE(
        AiaSemaphore( TryWait )( &g_mockEventRegulator->writeSemaphore ) );
    link = NULL;
    link = AiaListDouble( PeekHead )( &g_mockEventRegulator->writtenMessages );
    AiaListDouble( RemoveHead )( &g_mockEventRegulator->writtenMessages );
    TEST_ASSERT_NOT_NULL( link );
    AiaJsonMessage_t* microphoneClosedMessage = AiaJsonMessage_FromMessage(
        ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
    TEST_ASSERT_EQUAL(
        strcmp( AiaJsonMessage_GetName( microphoneClosedMessage ),
                AIA_EVENTS_MICROPHONE_CLOSED ),
        0 );
    payload = AiaJsonMessage_GetJsonPayload( microphoneClosedMessage );
    TEST_ASSERT_NOT_NULL( payload );

    const char* microphoneClosedOffset = NULL;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_MICROPHONE_CLOSED_OFFSET_KEY,
        strlen( AIA_MICROPHONE_CLOSED_OFFSET_KEY ), &microphoneClosedOffset,
        NULL ) );
    TEST_ASSERT_NOT_NULL( microphoneClosedOffset );
    TEST_ASSERT_EQUAL( atoi( microphoneClosedOffset ), streamOffset );
}

TEST( AiaMicrophoneManagerTests, WakeWordNonAlexaFails )
{
    AiaMicrophoneProfile_t TEST_PROFILE = AIA_MICROPHONE_PROFILE_FAR_FIELD;
    AiaBinaryAudioStreamOffset_t WW_STREAM_OFFSET_START = 8000;
    AiaBinaryAudioStreamOffset_t WW_STREAM_OFFSET_END = 16000;

    TEST_ASSERT_FALSE( AiaMicrophoneManager_WakeWordStart(
        microphoneManager, WW_STREAM_OFFSET_START, WW_STREAM_OFFSET_END,
        TEST_PROFILE, "SANJAY" ) );
}

TEST( AiaMicrophoneManagerTests, WakeWordWithoutAmpleSamplesFails )
{
    AiaMicrophoneProfile_t TEST_PROFILE = AIA_MICROPHONE_PROFILE_FAR_FIELD;
    AiaBinaryAudioStreamOffset_t WW_STREAM_OFFSET_START = 8000 - 1;
    AiaBinaryAudioStreamOffset_t WW_STREAM_OFFSET_END = 16000;

    TEST_ASSERT_FALSE( AiaMicrophoneManager_WakeWordStart(
        microphoneManager, WW_STREAM_OFFSET_START, WW_STREAM_OFFSET_END,
        TEST_PROFILE, AIA_ALEXA_WAKE_WORD ) );
}

TEST( AiaMicrophoneManagerTests, HoldToTalkOpenMicrophoneTimeout )
{
    const AiaBinaryAudioStreamOffset_t BUFFER_START_INDEX = 500;
    AiaBinaryAudioStreamOffset_t streamOffset = 0;

    TEST_ASSERT_TRUE( AiaMicrophoneManager_HoldToTalkStart(
        microphoneManager, BUFFER_START_INDEX ) );
    TEST_ASSERT_TRUE(
        AiaSemaphore( TryWait )( &testObserver->currentStateChanged ) );
    TEST_ASSERT_EQUAL( testObserver->currentState, AIA_MICROPHONE_STATE_OPEN );

    TEST_ASSERT_TRUE(
        AiaSemaphore( TryWait )( &g_mockEventRegulator->writeSemaphore ) );
    AiaListDouble( Link_t )* link = NULL;
    link = AiaListDouble( PeekHead )( &g_mockEventRegulator->writtenMessages );
    AiaListDouble( RemoveHead )( &g_mockEventRegulator->writtenMessages );
    TEST_ASSERT_NOT_NULL( link );
    AiaJsonMessage_t* microphoneOpenedMessage = AiaJsonMessage_FromMessage(
        ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
    TEST_ASSERT_EQUAL(
        strcmp( AiaJsonMessage_GetName( microphoneOpenedMessage ),
                AIA_EVENTS_MICROPHONE_OPENED ),
        0 );
    const char* payload =
        AiaJsonMessage_GetJsonPayload( microphoneOpenedMessage );
    TEST_ASSERT_NOT_NULL( payload );

    const char* profile = NULL;
    size_t profileLen;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_MICROPHONE_OPENED_PROFILE_KEY,
        strlen( AIA_MICROPHONE_OPENED_PROFILE_KEY ), &profile, &profileLen ) );
    TEST_ASSERT_NOT_NULL( profile );
    TEST_ASSERT_TRUE( AiaJsonUtils_UnquoteString( &profile, &profileLen ) );
    TEST_ASSERT_EQUAL_STRING_LEN(
        AiaMicrophoneProfile_ToString( AIA_MICROPHONE_PROFILE_CLOSE_TALK ),
        profile, profileLen );

    const char* microphoneOpenedOffset = NULL;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_MICROPHONE_OPENED_OFFSET_KEY,
        strlen( AIA_MICROPHONE_OPENED_OFFSET_KEY ), &microphoneOpenedOffset,
        NULL ) );
    TEST_ASSERT_NOT_NULL( microphoneOpenedOffset );
    TEST_ASSERT_EQUAL( atoi( microphoneOpenedOffset ), streamOffset );

    const char* initiator = NULL;
    size_t initiatorLen;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_MICROPHONE_OPENED_INITIATOR_KEY,
        strlen( AIA_MICROPHONE_OPENED_INITIATOR_KEY ), &initiator,
        &initiatorLen ) );
    TEST_ASSERT_NOT_NULL( initiator );

    const char* type = NULL;
    size_t typeLen;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        initiator, initiatorLen, AIA_MICROPHONE_OPENED_INITIATOR_TYPE_KEY,
        strlen( AIA_MICROPHONE_OPENED_INITIATOR_TYPE_KEY ), &type, &typeLen ) );
    TEST_ASSERT_NOT_NULL( type );
    TEST_ASSERT_TRUE( AiaJsonUtils_UnquoteString( &type, &typeLen ) );
    TEST_ASSERT_EQUAL_STRING_LEN( AiaMicrophoneInitiatorType_ToString(
                                      AIA_MICROPHONE_INITIATOR_TYPE_HOLD ),
                                  type, typeLen );

    const AiaBinaryAudioStreamOffset_t BUFFER_STOP_INDEX =
        BUFFER_START_INDEX + ( 4 * AIA_MICROPHONE_CHUNK_SIZE_SAMPLES );
    AiaBinaryAudioStreamOffset_t bufferCurrentIndex = BUFFER_START_INDEX;
    while( bufferCurrentIndex < BUFFER_STOP_INDEX )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_mockMicrophoneRegulator->writeSemaphore,
            MICROPHONE_PUBLISH_RATE + LEEWAY ) );
        link = NULL;
        link = AiaListDouble( PeekHead )(
            &g_mockMicrophoneRegulator->writtenMessages );
        AiaListDouble( RemoveHead )(
            &g_mockMicrophoneRegulator->writtenMessages );
        TEST_ASSERT_NOT_NULL( link );
        AiaBinaryMessage_t* binaryMessage = AiaBinaryMessage_FromMessage(
            ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
        TEST_ASSERT_EQUAL( AiaBinaryMessage_GetType( binaryMessage ),
                           AIA_BINARY_STREAM_MICROPHONE_CONTENT_TYPE );
        TEST_ASSERT_EQUAL( AiaBinaryMessage_GetCount( binaryMessage ), 0 );
        TEST_ASSERT_EQUAL( AiaBinaryMessage_GetLength( binaryMessage ),
                           sizeof( AiaBinaryAudioStreamOffset_t ) +
                               ( AIA_MICROPHONE_CHUNK_SIZE_SAMPLES *
                                 AIA_MICROPHONE_BUFFER_WORD_SIZE ) );
        const uint8_t* data = AiaBinaryMessage_GetData( binaryMessage );
        TEST_ASSERT_NOT_NULL( data );
        TEST_ASSERT_EQUAL( getStreamOffsetFromData( data ), streamOffset );
        size_t dataLenInBytes = AiaBinaryMessage_GetLength( binaryMessage ) -
                                sizeof( AiaBinaryAudioStreamOffset_t );

        const uint16_t* dataInSamples =
            (const uint16_t*)( data + sizeof( AiaBinaryAudioStreamOffset_t ) );
        for( size_t i = 0; i < AIA_MICROPHONE_CHUNK_SIZE_SAMPLES; ++i )
        {
            TEST_ASSERT_EQUAL( dataInSamples[ i ], bufferCurrentIndex + i );
        }

        streamOffset += dataLenInBytes;
        bufferCurrentIndex +=
            ( dataLenInBytes / AIA_MICROPHONE_BUFFER_WORD_SIZE );
    }

    AiaMicrophoneManager_CloseMicrophone( microphoneManager );
    TEST_ASSERT_FALSE( AiaSemaphore( TimedWait )(
        &g_mockMicrophoneRegulator->writeSemaphore, MICROPHONE_PUBLISH_RATE ) );

    TEST_ASSERT_TRUE(
        AiaSemaphore( TryWait )( &g_mockEventRegulator->writeSemaphore ) );
    link = NULL;
    link = AiaListDouble( PeekHead )( &g_mockEventRegulator->writtenMessages );
    AiaListDouble( RemoveHead )( &g_mockEventRegulator->writtenMessages );
    TEST_ASSERT_NOT_NULL( link );
    AiaJsonMessage_t* microphoneClosedMessage = AiaJsonMessage_FromMessage(
        ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
    TEST_ASSERT_EQUAL(
        strcmp( AiaJsonMessage_GetName( microphoneClosedMessage ),
                AIA_EVENTS_MICROPHONE_CLOSED ),
        0 );
    payload = AiaJsonMessage_GetJsonPayload( microphoneClosedMessage );
    TEST_ASSERT_NOT_NULL( payload );

    const char* microphoneClosedOffset = NULL;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_MICROPHONE_CLOSED_OFFSET_KEY,
        strlen( AIA_MICROPHONE_CLOSED_OFFSET_KEY ), &microphoneClosedOffset,
        NULL ) );
    TEST_ASSERT_NOT_NULL( microphoneClosedOffset );
    TEST_ASSERT_EQUAL( atoi( microphoneClosedOffset ), streamOffset );

    AiaDurationMs_t TIMEOUT = 100;
    const char* openMicrophonePayload = generateOpenMicrophone( TIMEOUT, NULL );
    TEST_ASSERT_NOT_NULL( openMicrophonePayload );
    AiaMicrophoneManager_OnOpenMicrophoneDirectiveReceived(
        microphoneManager, (void*)openMicrophonePayload,
        strlen( openMicrophonePayload ), 0, 0 );

    TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
        &g_mockEventRegulator->writeSemaphore, TIMEOUT + LEEWAY ) );
    link = NULL;
    link = AiaListDouble( PeekHead )( &g_mockEventRegulator->writtenMessages );
    AiaListDouble( RemoveHead )( &g_mockEventRegulator->writtenMessages );
    TEST_ASSERT_NOT_NULL( link );
    AiaJsonMessage_t* openMicrophoneTimedOutMessage =
        AiaJsonMessage_FromMessage(
            ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
    TEST_ASSERT_EQUAL(
        strcmp( AiaJsonMessage_GetName( openMicrophoneTimedOutMessage ),
                AIA_EVENTS_OPEN_MICROPHONE_TIMED_OUT ),
        0 );

    AiaFree( (void*)openMicrophonePayload );
}

TEST( AiaMicrophoneManagerTests, HoldToTalkOpenMicrophoneWithinTimeout )
{
    const AiaBinaryAudioStreamOffset_t BUFFER_START_INDEX = 500;
    AiaBinaryAudioStreamOffset_t streamOffset = 0;

    TEST_ASSERT_TRUE( AiaMicrophoneManager_HoldToTalkStart(
        microphoneManager, BUFFER_START_INDEX ) );
    TEST_ASSERT_TRUE(
        AiaSemaphore( TryWait )( &testObserver->currentStateChanged ) );
    TEST_ASSERT_EQUAL( testObserver->currentState, AIA_MICROPHONE_STATE_OPEN );

    TEST_ASSERT_TRUE(
        AiaSemaphore( TryWait )( &g_mockEventRegulator->writeSemaphore ) );
    AiaListDouble( Link_t )* link = NULL;
    link = AiaListDouble( PeekHead )( &g_mockEventRegulator->writtenMessages );
    AiaListDouble( RemoveHead )( &g_mockEventRegulator->writtenMessages );
    TEST_ASSERT_NOT_NULL( link );
    AiaJsonMessage_t* microphoneOpenedMessage = AiaJsonMessage_FromMessage(
        ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
    TEST_ASSERT_EQUAL(
        strcmp( AiaJsonMessage_GetName( microphoneOpenedMessage ),
                AIA_EVENTS_MICROPHONE_OPENED ),
        0 );
    const char* payload =
        AiaJsonMessage_GetJsonPayload( microphoneOpenedMessage );
    TEST_ASSERT_NOT_NULL( payload );

    const char* profile = NULL;
    size_t profileLen;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_MICROPHONE_OPENED_PROFILE_KEY,
        strlen( AIA_MICROPHONE_OPENED_PROFILE_KEY ), &profile, &profileLen ) );
    TEST_ASSERT_NOT_NULL( profile );
    TEST_ASSERT_TRUE( AiaJsonUtils_UnquoteString( &profile, &profileLen ) );
    TEST_ASSERT_EQUAL_STRING_LEN(
        AiaMicrophoneProfile_ToString( AIA_MICROPHONE_PROFILE_CLOSE_TALK ),
        profile, profileLen );

    const char* microphoneOpenedOffset = NULL;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_MICROPHONE_OPENED_OFFSET_KEY,
        strlen( AIA_MICROPHONE_OPENED_OFFSET_KEY ), &microphoneOpenedOffset,
        NULL ) );
    TEST_ASSERT_NOT_NULL( microphoneOpenedOffset );
    TEST_ASSERT_EQUAL( atoi( microphoneOpenedOffset ), streamOffset );

    const char* initiator = NULL;
    size_t initiatorLen;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_MICROPHONE_OPENED_INITIATOR_KEY,
        strlen( AIA_MICROPHONE_OPENED_INITIATOR_KEY ), &initiator,
        &initiatorLen ) );
    TEST_ASSERT_NOT_NULL( initiator );

    const char* type = NULL;
    size_t typeLen;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        initiator, initiatorLen, AIA_MICROPHONE_OPENED_INITIATOR_TYPE_KEY,
        strlen( AIA_MICROPHONE_OPENED_INITIATOR_TYPE_KEY ), &type, &typeLen ) );
    TEST_ASSERT_NOT_NULL( type );
    TEST_ASSERT_TRUE( AiaJsonUtils_UnquoteString( &type, &typeLen ) );
    TEST_ASSERT_EQUAL_STRING_LEN( AiaMicrophoneInitiatorType_ToString(
                                      AIA_MICROPHONE_INITIATOR_TYPE_HOLD ),
                                  type, typeLen );

    const AiaBinaryAudioStreamOffset_t BUFFER_STOP_INDEX =
        BUFFER_START_INDEX + ( 4 * AIA_MICROPHONE_CHUNK_SIZE_SAMPLES );
    AiaBinaryAudioStreamOffset_t bufferCurrentIndex = BUFFER_START_INDEX;
    while( bufferCurrentIndex < BUFFER_STOP_INDEX )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_mockMicrophoneRegulator->writeSemaphore,
            MICROPHONE_PUBLISH_RATE + LEEWAY ) );
        link = NULL;
        link = AiaListDouble( PeekHead )(
            &g_mockMicrophoneRegulator->writtenMessages );
        AiaListDouble( RemoveHead )(
            &g_mockMicrophoneRegulator->writtenMessages );
        TEST_ASSERT_NOT_NULL( link );
        AiaBinaryMessage_t* binaryMessage = AiaBinaryMessage_FromMessage(
            ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
        TEST_ASSERT_EQUAL( AiaBinaryMessage_GetType( binaryMessage ),
                           AIA_BINARY_STREAM_MICROPHONE_CONTENT_TYPE );
        TEST_ASSERT_EQUAL( AiaBinaryMessage_GetCount( binaryMessage ), 0 );
        TEST_ASSERT_EQUAL( AiaBinaryMessage_GetLength( binaryMessage ),
                           sizeof( AiaBinaryAudioStreamOffset_t ) +
                               ( AIA_MICROPHONE_CHUNK_SIZE_SAMPLES *
                                 AIA_MICROPHONE_BUFFER_WORD_SIZE ) );
        const uint8_t* data = AiaBinaryMessage_GetData( binaryMessage );
        TEST_ASSERT_NOT_NULL( data );
        TEST_ASSERT_EQUAL( getStreamOffsetFromData( data ), streamOffset );
        size_t dataLenInBytes = AiaBinaryMessage_GetLength( binaryMessage ) -
                                sizeof( AiaBinaryAudioStreamOffset_t );

        const uint16_t* dataInSamples =
            (const uint16_t*)( data + sizeof( AiaBinaryAudioStreamOffset_t ) );
        for( size_t i = 0; i < AIA_MICROPHONE_CHUNK_SIZE_SAMPLES; ++i )
        {
            TEST_ASSERT_EQUAL( dataInSamples[ i ], bufferCurrentIndex + i );
        }

        streamOffset += dataLenInBytes;
        bufferCurrentIndex +=
            ( dataLenInBytes / AIA_MICROPHONE_BUFFER_WORD_SIZE );
    }

    AiaMicrophoneManager_CloseMicrophone( microphoneManager );
    TEST_ASSERT_FALSE( AiaSemaphore( TimedWait )(
        &g_mockMicrophoneRegulator->writeSemaphore, MICROPHONE_PUBLISH_RATE ) );

    TEST_ASSERT_TRUE(
        AiaSemaphore( TryWait )( &g_mockEventRegulator->writeSemaphore ) );
    link = NULL;
    link = AiaListDouble( PeekHead )( &g_mockEventRegulator->writtenMessages );
    AiaListDouble( RemoveHead )( &g_mockEventRegulator->writtenMessages );
    TEST_ASSERT_NOT_NULL( link );
    AiaJsonMessage_t* microphoneClosedMessage = AiaJsonMessage_FromMessage(
        ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
    TEST_ASSERT_EQUAL(
        strcmp( AiaJsonMessage_GetName( microphoneClosedMessage ),
                AIA_EVENTS_MICROPHONE_CLOSED ),
        0 );
    payload = AiaJsonMessage_GetJsonPayload( microphoneClosedMessage );
    TEST_ASSERT_NOT_NULL( payload );

    const char* microphoneClosedOffset = NULL;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_MICROPHONE_CLOSED_OFFSET_KEY,
        strlen( AIA_MICROPHONE_CLOSED_OFFSET_KEY ), &microphoneClosedOffset,
        NULL ) );
    TEST_ASSERT_NOT_NULL( microphoneClosedOffset );
    TEST_ASSERT_EQUAL( atoi( microphoneClosedOffset ), streamOffset );

    AiaDurationMs_t TIMEOUT = 100;
    const char* openMicrophonePayload = generateOpenMicrophone( TIMEOUT, NULL );
    TEST_ASSERT_NOT_NULL( openMicrophonePayload );
    AiaMicrophoneManager_OnOpenMicrophoneDirectiveReceived(
        microphoneManager, (void*)openMicrophonePayload,
        strlen( openMicrophonePayload ), 0, 0 );

    TEST_ASSERT_TRUE( AiaMicrophoneManager_HoldToTalkStart(
        microphoneManager, BUFFER_START_INDEX ) );
    TEST_ASSERT_TRUE(
        AiaSemaphore( TryWait )( &testObserver->currentStateChanged ) );
    TEST_ASSERT_EQUAL( testObserver->currentState, AIA_MICROPHONE_STATE_OPEN );

    TEST_ASSERT_TRUE(
        AiaSemaphore( TryWait )( &g_mockEventRegulator->writeSemaphore ) );
    link = NULL;
    link = AiaListDouble( PeekHead )( &g_mockEventRegulator->writtenMessages );
    AiaListDouble( RemoveHead )( &g_mockEventRegulator->writtenMessages );
    TEST_ASSERT_NOT_NULL( link );
    microphoneOpenedMessage = AiaJsonMessage_FromMessage(
        ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
    TEST_ASSERT_EQUAL(
        strcmp( AiaJsonMessage_GetName( microphoneOpenedMessage ),
                AIA_EVENTS_MICROPHONE_OPENED ),
        0 );

    /* No OpenMicrophoneTimedOut events expected. */
    TEST_ASSERT_FALSE( AiaSemaphore( TimedWait )(
        &g_mockEventRegulator->writeSemaphore, TIMEOUT + LEEWAY ) );

    AiaFree( (void*)openMicrophonePayload );
}

TEST( AiaMicrophoneManagerTests, OpenMicrophoneWithInitiatorEchoedBack )
{
    const AiaBinaryAudioStreamOffset_t BUFFER_START_INDEX = 500;
    AiaBinaryAudioStreamOffset_t streamOffset = 0;

    TEST_ASSERT_TRUE( AiaMicrophoneManager_HoldToTalkStart(
        microphoneManager, BUFFER_START_INDEX ) );
    TEST_ASSERT_TRUE(
        AiaSemaphore( TryWait )( &testObserver->currentStateChanged ) );
    TEST_ASSERT_EQUAL( testObserver->currentState, AIA_MICROPHONE_STATE_OPEN );

    TEST_ASSERT_TRUE(
        AiaSemaphore( TryWait )( &g_mockEventRegulator->writeSemaphore ) );
    AiaListDouble( Link_t )* link = NULL;
    link = AiaListDouble( PeekHead )( &g_mockEventRegulator->writtenMessages );
    AiaListDouble( RemoveHead )( &g_mockEventRegulator->writtenMessages );
    TEST_ASSERT_NOT_NULL( link );
    AiaJsonMessage_t* microphoneOpenedMessage = AiaJsonMessage_FromMessage(
        ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
    TEST_ASSERT_EQUAL(
        strcmp( AiaJsonMessage_GetName( microphoneOpenedMessage ),
                AIA_EVENTS_MICROPHONE_OPENED ),
        0 );
    const char* payload =
        AiaJsonMessage_GetJsonPayload( microphoneOpenedMessage );
    TEST_ASSERT_NOT_NULL( payload );

    const char* profile = NULL;
    size_t profileLen;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_MICROPHONE_OPENED_PROFILE_KEY,
        strlen( AIA_MICROPHONE_OPENED_PROFILE_KEY ), &profile, &profileLen ) );
    TEST_ASSERT_NOT_NULL( profile );
    TEST_ASSERT_TRUE( AiaJsonUtils_UnquoteString( &profile, &profileLen ) );
    TEST_ASSERT_EQUAL_STRING_LEN(
        AiaMicrophoneProfile_ToString( AIA_MICROPHONE_PROFILE_CLOSE_TALK ),
        profile, profileLen );

    const char* microphoneOpenedOffset = NULL;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_MICROPHONE_OPENED_OFFSET_KEY,
        strlen( AIA_MICROPHONE_OPENED_OFFSET_KEY ), &microphoneOpenedOffset,
        NULL ) );
    TEST_ASSERT_NOT_NULL( microphoneOpenedOffset );
    TEST_ASSERT_EQUAL( atoi( microphoneOpenedOffset ), streamOffset );

    const char* initiator = NULL;
    size_t initiatorLen;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_MICROPHONE_OPENED_INITIATOR_KEY,
        strlen( AIA_MICROPHONE_OPENED_INITIATOR_KEY ), &initiator,
        &initiatorLen ) );
    TEST_ASSERT_NOT_NULL( initiator );

    const char* type = NULL;
    size_t typeLen;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        initiator, initiatorLen, AIA_MICROPHONE_OPENED_INITIATOR_TYPE_KEY,
        strlen( AIA_MICROPHONE_OPENED_INITIATOR_TYPE_KEY ), &type, &typeLen ) );
    TEST_ASSERT_NOT_NULL( type );
    TEST_ASSERT_TRUE( AiaJsonUtils_UnquoteString( &type, &typeLen ) );
    TEST_ASSERT_EQUAL_STRING_LEN( AiaMicrophoneInitiatorType_ToString(
                                      AIA_MICROPHONE_INITIATOR_TYPE_HOLD ),
                                  type, typeLen );

    const AiaBinaryAudioStreamOffset_t BUFFER_STOP_INDEX =
        BUFFER_START_INDEX + ( 4 * AIA_MICROPHONE_CHUNK_SIZE_SAMPLES );
    AiaBinaryAudioStreamOffset_t bufferCurrentIndex = BUFFER_START_INDEX;
    while( bufferCurrentIndex < BUFFER_STOP_INDEX )
    {
        TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
            &g_mockMicrophoneRegulator->writeSemaphore,
            MICROPHONE_PUBLISH_RATE + LEEWAY ) );
        link = NULL;
        link = AiaListDouble( PeekHead )(
            &g_mockMicrophoneRegulator->writtenMessages );
        AiaListDouble( RemoveHead )(
            &g_mockMicrophoneRegulator->writtenMessages );
        TEST_ASSERT_NOT_NULL( link );
        AiaBinaryMessage_t* binaryMessage = AiaBinaryMessage_FromMessage(
            ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
        TEST_ASSERT_EQUAL( AiaBinaryMessage_GetType( binaryMessage ),
                           AIA_BINARY_STREAM_MICROPHONE_CONTENT_TYPE );
        TEST_ASSERT_EQUAL( AiaBinaryMessage_GetCount( binaryMessage ), 0 );
        TEST_ASSERT_EQUAL( AiaBinaryMessage_GetLength( binaryMessage ),
                           sizeof( AiaBinaryAudioStreamOffset_t ) +
                               ( AIA_MICROPHONE_CHUNK_SIZE_SAMPLES *
                                 AIA_MICROPHONE_BUFFER_WORD_SIZE ) );
        const uint8_t* data = AiaBinaryMessage_GetData( binaryMessage );
        TEST_ASSERT_NOT_NULL( data );
        TEST_ASSERT_EQUAL( getStreamOffsetFromData( data ), streamOffset );
        size_t dataLenInBytes = AiaBinaryMessage_GetLength( binaryMessage ) -
                                sizeof( AiaBinaryAudioStreamOffset_t );

        const uint16_t* dataInSamples =
            (const uint16_t*)( data + sizeof( AiaBinaryAudioStreamOffset_t ) );
        for( size_t i = 0; i < AIA_MICROPHONE_CHUNK_SIZE_SAMPLES; ++i )
        {
            TEST_ASSERT_EQUAL( dataInSamples[ i ], bufferCurrentIndex + i );
        }

        streamOffset += dataLenInBytes;
        bufferCurrentIndex +=
            ( dataLenInBytes / AIA_MICROPHONE_BUFFER_WORD_SIZE );
    }

    AiaMicrophoneManager_CloseMicrophone( microphoneManager );
    TEST_ASSERT_FALSE( AiaSemaphore( TimedWait )(
        &g_mockMicrophoneRegulator->writeSemaphore, MICROPHONE_PUBLISH_RATE ) );

    TEST_ASSERT_TRUE(
        AiaSemaphore( TryWait )( &g_mockEventRegulator->writeSemaphore ) );
    link = NULL;
    link = AiaListDouble( PeekHead )( &g_mockEventRegulator->writtenMessages );
    AiaListDouble( RemoveHead )( &g_mockEventRegulator->writtenMessages );
    TEST_ASSERT_NOT_NULL( link );
    AiaJsonMessage_t* microphoneClosedMessage = AiaJsonMessage_FromMessage(
        ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
    TEST_ASSERT_EQUAL(
        strcmp( AiaJsonMessage_GetName( microphoneClosedMessage ),
                AIA_EVENTS_MICROPHONE_CLOSED ),
        0 );
    payload = AiaJsonMessage_GetJsonPayload( microphoneClosedMessage );
    TEST_ASSERT_NOT_NULL( payload );

    const char* microphoneClosedOffset = NULL;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_MICROPHONE_CLOSED_OFFSET_KEY,
        strlen( AIA_MICROPHONE_CLOSED_OFFSET_KEY ), &microphoneClosedOffset,
        NULL ) );
    TEST_ASSERT_NOT_NULL( microphoneClosedOffset );
    TEST_ASSERT_EQUAL( atoi( microphoneClosedOffset ), streamOffset );

    AiaDurationMs_t TIMEOUT = 100;
    const char* TEST_INITIATOR = "{ SANJAY }";
    const char* openMicrophonePayload =
        generateOpenMicrophone( TIMEOUT, TEST_INITIATOR );
    TEST_ASSERT_NOT_NULL( openMicrophonePayload );
    AiaMicrophoneManager_OnOpenMicrophoneDirectiveReceived(
        microphoneManager, (void*)openMicrophonePayload,
        strlen( openMicrophonePayload ), 0, 0 );

    TEST_ASSERT_TRUE( AiaMicrophoneManager_HoldToTalkStart(
        microphoneManager, BUFFER_START_INDEX ) );
    TEST_ASSERT_TRUE(
        AiaSemaphore( TryWait )( &testObserver->currentStateChanged ) );
    TEST_ASSERT_EQUAL( testObserver->currentState, AIA_MICROPHONE_STATE_OPEN );

    TEST_ASSERT_TRUE(
        AiaSemaphore( TryWait )( &g_mockEventRegulator->writeSemaphore ) );
    link = NULL;
    link = AiaListDouble( PeekHead )( &g_mockEventRegulator->writtenMessages );
    AiaListDouble( RemoveHead )( &g_mockEventRegulator->writtenMessages );
    TEST_ASSERT_NOT_NULL( link );
    microphoneOpenedMessage = AiaJsonMessage_FromMessage(
        ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
    TEST_ASSERT_EQUAL(
        strcmp( AiaJsonMessage_GetName( microphoneOpenedMessage ),
                AIA_EVENTS_MICROPHONE_OPENED ),
        0 );
    payload = AiaJsonMessage_GetJsonPayload( microphoneOpenedMessage );
    TEST_ASSERT_NOT_NULL( payload );

    initiator = NULL;
    initiatorLen = 0;
    ;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_OPEN_MICROPHONE_INITIATOR_KEY,
        strlen( AIA_OPEN_MICROPHONE_INITIATOR_KEY ), &initiator,
        &initiatorLen ) );
    TEST_ASSERT_NOT_NULL( initiator );
    TEST_ASSERT_EQUAL_STRING_LEN( TEST_INITIATOR, initiator, initiatorLen );

    /* No OpenMicrophoneTimedOut events expected */
    TEST_ASSERT_FALSE( AiaSemaphore( TimedWait )(
        &g_mockEventRegulator->writeSemaphore, TIMEOUT + LEEWAY ) );

    AiaFree( (void*)openMicrophonePayload );
}
