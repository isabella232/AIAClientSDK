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
 * @file aia_ux_manager_tests.c
 * @brief Tests for AiaUXManager_t.
 */

/* The config header is always included first. */
#include <aia_config.h>

/* Aia headers */
#include <aiauxmanager/aia_ux_manager.h>
#include <aiauxmanager/private/aia_ux_manager.h>

#include <aiacore/aia_json_constants.h>
#include <aiacore/aia_mbedtls_threading.h>
#include <aiacore/aia_message_constants.h>
#include <aiacore/aia_random_mbedtls.h>
#include <aiamicrophonemanager/aia_microphone_state.h>

#include <aiamockregulator/aia_mock_regulator.h>
#include <aiamockspeakermanager/aia_mock_speaker_manager.h>
#include <aiatestutilities/aia_test_utilities.h>

/* Test framework includes. */
#include <unity_fixture.h>

#include <inttypes.h>
#include <string.h>

typedef struct AiaTestUXStateObserver
{
    AiaUXState_t currentState;

    AiaSemaphore_t numObserversNotifiedSemaphore;
} AiaTestUXStateObserver_t;

static void AiaOnUXStateChanged( AiaUXState_t state, void* userData )
{
    TEST_ASSERT_NOT_NULL( userData );
    AiaTestUXStateObserver_t* observer = (AiaTestUXStateObserver_t*)userData;
    observer->currentState = state;
}

static AiaTestUXStateObserver_t* AiaTestUXStateObserver_Create()
{
    AiaTestUXStateObserver_t* observer =
        AiaCalloc( 1, sizeof( AiaTestUXStateObserver_t ) );
    if( !observer )
    {
        AiaLogError( "AiaCalloc failed." );
        return NULL;
    }
    if( !AiaSemaphore( Create )( &observer->numObserversNotifiedSemaphore, 0,
                                 1000 ) )
    {
        AiaFree( observer );
        return NULL;
    }
    return observer;
}

static void AiaTestUXStateObserver_Destroy( AiaTestUXStateObserver_t* observer )
{
    AiaSemaphore( Destroy )( &observer->numObserversNotifiedSemaphore );
    AiaFree( observer );
}

/** Callers must clean up non NULL returned values using AiaFree(). Using @c
 * AIA_UX_LISTENING for @c state will result in a failure. */
char* generateSetAttentionStateWithoutOffset( AiaUXState_t state )
{
#ifdef AIA_ENABLE_MICROPHONE
    if( state == AIA_UX_LISTENING )
    {
        return NULL;
    }
#endif
    static const char* formatPayload =
        /* clang-format off */
        "{"
            "\""AIA_SET_ATTENTION_STATE_STATE_KEY"\":\"%s\""
        "}";
    /* clang-format on */
    int numCharsRequired =
        snprintf( NULL, 0, formatPayload, AiaUXState_ToString( state ) );
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
                  AiaUXState_ToString( state ) ) < 0 )
    {
        AiaLogError( "snprintf failed" );
        AiaFree( fullPayloadBuffer );
        return NULL;
    }

    return fullPayloadBuffer;
}

/* TODO: ADSER-1742 Consolidate the above and below function using variadic
 * arguments. */

/** Callers must clean up non NULL returned values using AiaFree(). Using @c
 * AIA_UX_LISTENING for @c state will result in a failure. */
char* generateSetAttentionStateWithOffset( AiaUXState_t state,
                                           AiaBinaryAudioStreamOffset_t offset )
{
#ifdef AIA_ENABLE_MICROPHONE
    if( state == AIA_UX_LISTENING )
    {
        return NULL;
    }
#endif
    static const char* formatPayload =
        /* clang-format off */
        "{"
            "\""AIA_SET_ATTENTION_STATE_STATE_KEY"\":\"%s\","
            "\""AIA_SET_ATTENTION_STATE_OFFSET_KEY"\":%"PRIu64
        "}";
    /* clang-format on */
    int numCharsRequired = snprintf( NULL, 0, formatPayload,
                                     AiaUXState_ToString( state ), offset );
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
                  AiaUXState_ToString( state ), offset ) < 0 )
    {
        AiaLogError( "snprintf failed" );
        AiaFree( fullPayloadBuffer );
        return NULL;
    }

    return fullPayloadBuffer;
}

static AiaMockRegulator_t* g_mockEventRegulator;

static AiaRegulator_t* g_eventRegulator;

static AiaTestUXStateObserver_t* g_testObserver;

#ifdef AIA_ENABLE_SPEAKER
static AiaSpeakerManager_t* g_testSpeakerManager;
#endif

static AiaUXManager_t* g_testUxManager;

/*-----------------------------------------------------------*/

/**
 * @brief Test group for AiaUXManager_t tests.
 */
TEST_GROUP( AiaUXManagerTests );

/*-----------------------------------------------------------*/

/**
 * @brief Test group runner for AiaUXManager_t tests.
 */
TEST_GROUP_RUNNER( AiaUXManagerTests )
{
    RUN_TEST_CASE( AiaUXManagerTests, Creation );
    RUN_TEST_CASE( AiaUXManagerTests,
                   TestStateChangesWhenMicrophoneClosedWithoutOffset );
    RUN_TEST_CASE( AiaUXManagerTests,
                   TestStateChangesWhenMicrophoneOpenWithoutOffset );
#ifdef AIA_ENABLE_SPEAKER
    RUN_TEST_CASE( AiaUXManagerTests, TestStateChangesWithOffset );
    RUN_TEST_CASE( AiaUXManagerTests, TestStateChangesWithOffsetInvalidated );
#endif
    RUN_TEST_CASE( AiaUXManagerTests,
                   TestGetUXStateWhenMicrophoneClosedWithoutOffset );
}

/*-----------------------------------------------------------*/

/*-----------------------------------------------------------*/
/**
 * @brief Test setup for AiaUXManager_t tests.
 */
TEST_SETUP( AiaUXManagerTests )
{
    /** Sample Seed. */
    static const char* TEST_SALT = "TestSalt";
    static const size_t TEST_SALT_LENGTH = sizeof( TEST_SALT ) - 1;
    AiaMbedtlsThreading_Init();
    AiaRandomMbedtls_Init();
    TEST_ASSERT_TRUE( AiaRandomMbedtls_Seed( TEST_SALT, TEST_SALT_LENGTH ) );

    g_mockEventRegulator = AiaMockRegulator_Create();
    TEST_ASSERT_NOT_NULL( g_mockEventRegulator );
    g_eventRegulator = (AiaRegulator_t*)g_mockEventRegulator;

    g_testObserver = AiaTestUXStateObserver_Create();
    TEST_ASSERT_NOT_NULL( g_testObserver );

#ifdef AIA_ENABLE_SPEAKER
    g_testSpeakerManager =
        AiaSpeakerManager_Create( 0, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL,
                                  NULL, NULL, NULL, NULL, NULL, NULL );
    TEST_ASSERT_NOT_NULL( g_testSpeakerManager );
    g_testUxManager =
        AiaUXManager_Create( g_eventRegulator, AiaOnUXStateChanged,
                             g_testObserver, g_testSpeakerManager );
#else
    g_testUxManager = AiaUXManager_Create(
        g_eventRegulator, AiaOnUXStateChanged, g_testObserver );
#endif
    TEST_ASSERT_NOT_NULL( g_testUxManager );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test tear down for AiaUXManager_t tests.
 */
TEST_TEAR_DOWN( AiaUXManagerTests )
{
    AiaUXManager_Destroy( g_testUxManager );

#ifdef AIA_ENABLE_SPEAKER
    AiaSpeakerManager_Destroy( g_testSpeakerManager );
#endif

    AiaTestUXStateObserver_Destroy( g_testObserver );
    AiaMockRegulator_Destroy( g_mockEventRegulator,
                              AiaTestUtilities_DestroyJsonChunk, NULL );

    AiaMbedtlsThreading_Cleanup();
    AiaRandomMbedtls_Cleanup();
}

/*-----------------------------------------------------------*/

TEST( AiaUXManagerTests, Creation )
{
#ifndef AIA_ENABLE_SPEAKER
    TEST_ASSERT_NULL(
        AiaUXManager_Create( NULL, AiaOnUXStateChanged, g_testObserver ) );

    TEST_ASSERT_NULL(
        AiaUXManager_Create( g_eventRegulator, NULL, g_testObserver ) );

    AiaUXManager_t* uxManager =
        AiaUXManager_Create( g_eventRegulator, AiaOnUXStateChanged, NULL );
    TEST_ASSERT_NOT_NULL( uxManager );

    uxManager = AiaUXManager_Create( g_eventRegulator, AiaOnUXStateChanged,
                                     g_testObserver );
    TEST_ASSERT_NOT_NULL( uxManager );
#else
    TEST_ASSERT_NULL( AiaUXManager_Create(
        NULL, AiaOnUXStateChanged, g_testObserver, g_testSpeakerManager ) );

    TEST_ASSERT_NULL( AiaUXManager_Create(
        g_eventRegulator, NULL, g_testObserver, g_testSpeakerManager ) );

    AiaUXManager_t* uxManager = AiaUXManager_Create(
        g_eventRegulator, AiaOnUXStateChanged, NULL, g_testSpeakerManager );
    TEST_ASSERT_NOT_NULL( uxManager );

    TEST_ASSERT_NULL( AiaUXManager_Create(
        g_eventRegulator, AiaOnUXStateChanged, g_testObserver, NULL ) );

    uxManager = AiaUXManager_Create( g_eventRegulator, AiaOnUXStateChanged,
                                     g_testObserver, g_testSpeakerManager );
    TEST_ASSERT_NOT_NULL( uxManager );
#endif
    AiaUXManager_Destroy( uxManager );
}

TEST( AiaUXManagerTests, TestStateChangesWhenMicrophoneClosedWithoutOffset )
{
#ifdef AIA_ENABLE_MICROPHONE
    AiaUXManager_OnMicrophoneStateChange( AIA_MICROPHONE_STATE_CLOSED,
                                          g_testUxManager );
#endif

    TEST_ASSERT_EQUAL( AIA_UX_IDLE, g_testObserver->currentState );

    char* setAttentionState =
        generateSetAttentionStateWithoutOffset( AIA_UX_THINKING );
    TEST_ASSERT_NOT_NULL( setAttentionState );
    AiaSequenceNumber_t TEST_SEQUENCE_NUMBER = 44;
    size_t TEST_INDEX = 44;
    AiaUXManager_OnSetAttentionStateDirectiveReceived(
        g_testUxManager, setAttentionState, strlen( setAttentionState ),
        TEST_SEQUENCE_NUMBER, TEST_INDEX );
    AiaFree( setAttentionState );
    TEST_ASSERT_EQUAL( AIA_UX_THINKING, g_testObserver->currentState );

#ifdef AIA_ENABLE_SPEAKER
    setAttentionState =
        generateSetAttentionStateWithoutOffset( AIA_UX_SPEAKING );
    TEST_ASSERT_NOT_NULL( setAttentionState );
    AiaUXManager_OnSetAttentionStateDirectiveReceived(
        g_testUxManager, setAttentionState, strlen( setAttentionState ),
        TEST_SEQUENCE_NUMBER, TEST_INDEX );
    AiaFree( setAttentionState );
    TEST_ASSERT_EQUAL( AIA_UX_SPEAKING, g_testObserver->currentState );
#endif

#ifdef AIA_ENABLE_ALERTS
    setAttentionState =
        generateSetAttentionStateWithoutOffset( AIA_UX_ALERTING );
    TEST_ASSERT_NOT_NULL( setAttentionState );
    AiaUXManager_OnSetAttentionStateDirectiveReceived(
        g_testUxManager, setAttentionState, strlen( setAttentionState ),
        TEST_SEQUENCE_NUMBER, TEST_INDEX );
    AiaFree( setAttentionState );
    TEST_ASSERT_EQUAL( AIA_UX_ALERTING, g_testObserver->currentState );
#endif

    setAttentionState =
        generateSetAttentionStateWithoutOffset( AIA_UX_NOTIFICATION_AVAILABLE );
    TEST_ASSERT_NOT_NULL( setAttentionState );
    AiaUXManager_OnSetAttentionStateDirectiveReceived(
        g_testUxManager, setAttentionState, strlen( setAttentionState ),
        TEST_SEQUENCE_NUMBER, TEST_INDEX );
    AiaFree( setAttentionState );
    TEST_ASSERT_EQUAL( AIA_UX_NOTIFICATION_AVAILABLE,
                       g_testObserver->currentState );

    setAttentionState =
        generateSetAttentionStateWithoutOffset( AIA_UX_DO_NOT_DISTURB );
    TEST_ASSERT_NOT_NULL( setAttentionState );
    AiaUXManager_OnSetAttentionStateDirectiveReceived(
        g_testUxManager, setAttentionState, strlen( setAttentionState ),
        TEST_SEQUENCE_NUMBER, TEST_INDEX );
    AiaFree( setAttentionState );
    TEST_ASSERT_EQUAL( AIA_UX_DO_NOT_DISTURB, g_testObserver->currentState );

    setAttentionState = generateSetAttentionStateWithoutOffset( AIA_UX_IDLE );
    TEST_ASSERT_NOT_NULL( setAttentionState );
    AiaUXManager_OnSetAttentionStateDirectiveReceived(
        g_testUxManager, setAttentionState, strlen( setAttentionState ),
        TEST_SEQUENCE_NUMBER, TEST_INDEX );
    AiaFree( setAttentionState );
    TEST_ASSERT_EQUAL( AIA_UX_IDLE, g_testObserver->currentState );
}

TEST( AiaUXManagerTests, TestStateChangesWhenMicrophoneOpenWithoutOffset )
{
#ifdef AIA_ENABLE_MICROPHONE
    AiaUXManager_OnMicrophoneStateChange( AIA_MICROPHONE_STATE_OPEN,
                                          g_testUxManager );
    TEST_ASSERT_EQUAL( AIA_UX_LISTENING, g_testObserver->currentState );
#endif

    char* setAttentionState =
        generateSetAttentionStateWithoutOffset( AIA_UX_THINKING );
    TEST_ASSERT_NOT_NULL( setAttentionState );
    AiaSequenceNumber_t TEST_SEQUENCE_NUMBER = 44;
    size_t TEST_INDEX = 44;
    AiaUXManager_OnSetAttentionStateDirectiveReceived(
        g_testUxManager, setAttentionState, strlen( setAttentionState ),
        TEST_SEQUENCE_NUMBER, TEST_INDEX );
    AiaFree( setAttentionState );
#ifdef AIA_ENABLE_MICROPHONE
    TEST_ASSERT_EQUAL( AIA_UX_LISTENING, g_testObserver->currentState );
    AiaUXManager_OnMicrophoneStateChange( AIA_MICROPHONE_STATE_CLOSED,
                                          g_testUxManager );
#endif
    TEST_ASSERT_EQUAL( AIA_UX_THINKING, g_testObserver->currentState );

#ifdef AIA_ENABLE_MICROPHONE
    AiaUXManager_OnMicrophoneStateChange( AIA_MICROPHONE_STATE_OPEN,
                                          g_testUxManager );
    TEST_ASSERT_EQUAL( AIA_UX_LISTENING, g_testObserver->currentState );
#endif
#ifdef AIA_ENABLE_SPEAKER
    setAttentionState =
        generateSetAttentionStateWithoutOffset( AIA_UX_SPEAKING );
    TEST_ASSERT_NOT_NULL( setAttentionState );
    AiaUXManager_OnSetAttentionStateDirectiveReceived(
        g_testUxManager, setAttentionState, strlen( setAttentionState ),
        TEST_SEQUENCE_NUMBER, TEST_INDEX );
    AiaFree( setAttentionState );
#ifdef AIA_ENABLE_MICROPHONE
    TEST_ASSERT_EQUAL( AIA_UX_LISTENING, g_testObserver->currentState );
    AiaUXManager_OnMicrophoneStateChange( AIA_MICROPHONE_STATE_CLOSED,
                                          g_testUxManager );
#endif
    TEST_ASSERT_EQUAL( AIA_UX_SPEAKING, g_testObserver->currentState );
#endif

#ifdef AIA_ENABLE_MICROPHONE
    AiaUXManager_OnMicrophoneStateChange( AIA_MICROPHONE_STATE_OPEN,
                                          g_testUxManager );
    TEST_ASSERT_EQUAL( AIA_UX_LISTENING, g_testObserver->currentState );
#endif
#ifdef AIA_ENABLE_ALERTS
    setAttentionState =
        generateSetAttentionStateWithoutOffset( AIA_UX_ALERTING );
    TEST_ASSERT_NOT_NULL( setAttentionState );
    AiaUXManager_OnSetAttentionStateDirectiveReceived(
        g_testUxManager, setAttentionState, strlen( setAttentionState ),
        TEST_SEQUENCE_NUMBER, TEST_INDEX );
    AiaFree( setAttentionState );
#ifdef AIA_ENABLE_MICROPHONE
    TEST_ASSERT_EQUAL( AIA_UX_LISTENING, g_testObserver->currentState );
    AiaUXManager_OnMicrophoneStateChange( AIA_MICROPHONE_STATE_CLOSED,
                                          g_testUxManager );
#endif
    TEST_ASSERT_EQUAL( AIA_UX_ALERTING, g_testObserver->currentState );
#endif

#ifdef AIA_ENABLE_MICROPHONE
    AiaUXManager_OnMicrophoneStateChange( AIA_MICROPHONE_STATE_OPEN,
                                          g_testUxManager );
    TEST_ASSERT_EQUAL( AIA_UX_LISTENING, g_testObserver->currentState );
#endif
    setAttentionState =
        generateSetAttentionStateWithoutOffset( AIA_UX_NOTIFICATION_AVAILABLE );
    TEST_ASSERT_NOT_NULL( setAttentionState );
    AiaUXManager_OnSetAttentionStateDirectiveReceived(
        g_testUxManager, setAttentionState, strlen( setAttentionState ),
        TEST_SEQUENCE_NUMBER, TEST_INDEX );
    AiaFree( setAttentionState );
#ifdef AIA_ENABLE_MICROPHONE
    TEST_ASSERT_EQUAL( AIA_UX_LISTENING, g_testObserver->currentState );
    AiaUXManager_OnMicrophoneStateChange( AIA_MICROPHONE_STATE_CLOSED,
                                          g_testUxManager );
#endif
    TEST_ASSERT_EQUAL( AIA_UX_NOTIFICATION_AVAILABLE,
                       g_testObserver->currentState );

#ifdef AIA_ENABLE_MICROPHONE
    AiaUXManager_OnMicrophoneStateChange( AIA_MICROPHONE_STATE_OPEN,
                                          g_testUxManager );
    TEST_ASSERT_EQUAL( AIA_UX_LISTENING, g_testObserver->currentState );
#endif
    setAttentionState =
        generateSetAttentionStateWithoutOffset( AIA_UX_DO_NOT_DISTURB );
    TEST_ASSERT_NOT_NULL( setAttentionState );
    AiaUXManager_OnSetAttentionStateDirectiveReceived(
        g_testUxManager, setAttentionState, strlen( setAttentionState ),
        TEST_SEQUENCE_NUMBER, TEST_INDEX );
    AiaFree( setAttentionState );
#ifdef AIA_ENABLE_MICROPHONE
    TEST_ASSERT_EQUAL( AIA_UX_LISTENING, g_testObserver->currentState );
    AiaUXManager_OnMicrophoneStateChange( AIA_MICROPHONE_STATE_CLOSED,
                                          g_testUxManager );
#endif
    TEST_ASSERT_EQUAL( AIA_UX_DO_NOT_DISTURB, g_testObserver->currentState );

#ifdef AIA_ENABLE_MICROPHONE
    AiaUXManager_OnMicrophoneStateChange( AIA_MICROPHONE_STATE_OPEN,
                                          g_testUxManager );
    TEST_ASSERT_EQUAL( AIA_UX_LISTENING, g_testObserver->currentState );
#endif
    setAttentionState = generateSetAttentionStateWithoutOffset( AIA_UX_IDLE );
    TEST_ASSERT_NOT_NULL( setAttentionState );
    AiaUXManager_OnSetAttentionStateDirectiveReceived(
        g_testUxManager, setAttentionState, strlen( setAttentionState ),
        TEST_SEQUENCE_NUMBER, TEST_INDEX );
    AiaFree( setAttentionState );
#ifdef AIA_ENABLE_MICROPHONE
    TEST_ASSERT_EQUAL( AIA_UX_LISTENING, g_testObserver->currentState );
    AiaUXManager_OnMicrophoneStateChange( AIA_MICROPHONE_STATE_CLOSED,
                                          g_testUxManager );
#endif
    TEST_ASSERT_EQUAL( AIA_UX_IDLE, g_testObserver->currentState );
}

TEST( AiaUXManagerTests, TestStateChangesWithOffset )
{
#ifndef AIA_ENABLE_SPEAKER
    AiaUXManager_t* uxManager = AiaUXManager_Create(
        g_eventRegulator, AiaOnUXStateChanged, g_testObserver );
#else
    AiaUXManager_t* uxManager =
        AiaUXManager_Create( g_eventRegulator, AiaOnUXStateChanged,
                             g_testObserver, g_testSpeakerManager );
#endif
    TEST_ASSERT_NOT_NULL( uxManager );

#ifdef AIA_ENABLE_MICROPHONE
    AiaUXManager_OnMicrophoneStateChange( AIA_MICROPHONE_STATE_CLOSED,
                                          g_testUxManager );
#endif

    TEST_ASSERT_EQUAL( AIA_UX_IDLE, g_testObserver->currentState );

    AiaBinaryAudioStreamOffset_t TEST_OFFSET = 44;

    char* setAttentionState =
        generateSetAttentionStateWithOffset( AIA_UX_THINKING, TEST_OFFSET );
    TEST_ASSERT_NOT_NULL( setAttentionState );
    AiaSequenceNumber_t TEST_SEQUENCE_NUMBER = 44;
    size_t TEST_INDEX = 44;
    AiaUXManager_OnSetAttentionStateDirectiveReceived(
        g_testUxManager, setAttentionState, strlen( setAttentionState ),
        TEST_SEQUENCE_NUMBER, TEST_INDEX );
    AiaFree( setAttentionState );
    TEST_ASSERT_NOT_EQUAL( AIA_UX_THINKING, g_testObserver->currentState );
#ifdef AIA_ENABLE_SPEAKER
    TEST_ASSERT_EQUAL( TEST_OFFSET,
                       ( (AiaMockSpeakerManager_t*)g_testSpeakerManager )
                           ->currentActionOffset );

    ( (AiaMockSpeakerManager_t*)g_testSpeakerManager )
        ->currentAction( true,
                         ( (AiaMockSpeakerManager_t*)g_testSpeakerManager )
                             ->currentActionUserData );
#endif
    TEST_ASSERT_EQUAL( AIA_UX_THINKING, g_testObserver->currentState );
}

TEST( AiaUXManagerTests, TestStateChangesWithOffsetInvalidated )
{
#ifdef AIA_ENABLE_MICROPHONE
    AiaUXManager_OnMicrophoneStateChange( AIA_MICROPHONE_STATE_CLOSED,
                                          g_testUxManager );
#endif

    TEST_ASSERT_EQUAL( AIA_UX_IDLE, g_testObserver->currentState );

    AiaBinaryAudioStreamOffset_t TEST_OFFSET = 44;

    char* setAttentionState =
        generateSetAttentionStateWithOffset( AIA_UX_THINKING, TEST_OFFSET );
    TEST_ASSERT_NOT_NULL( setAttentionState );
    AiaSequenceNumber_t TEST_SEQUENCE_NUMBER = 44;
    size_t TEST_INDEX = 44;
    AiaUXManager_OnSetAttentionStateDirectiveReceived(
        g_testUxManager, setAttentionState, strlen( setAttentionState ),
        TEST_SEQUENCE_NUMBER, TEST_INDEX );
    AiaFree( setAttentionState );
    TEST_ASSERT_NOT_EQUAL( AIA_UX_THINKING, g_testObserver->currentState );
    TEST_ASSERT_EQUAL( TEST_OFFSET,
                       ( (AiaMockSpeakerManager_t*)g_testSpeakerManager )
                           ->currentActionOffset );

    ( (AiaMockSpeakerManager_t*)g_testSpeakerManager )
        ->currentAction( false,
                         ( (AiaMockSpeakerManager_t*)g_testSpeakerManager )
                             ->currentActionUserData );
    TEST_ASSERT_NOT_EQUAL( AIA_UX_THINKING, g_testObserver->currentState );
}

TEST( AiaUXManagerTests, TestGetUXStateWhenMicrophoneClosedWithoutOffset )
{
#ifdef AIA_MICROPHONE_VERSION
    AiaUXManager_OnMicrophoneStateChange( AIA_MICROPHONE_STATE_CLOSED,
                                          g_testUxManager );
#endif

    TEST_ASSERT_EQUAL( AIA_UX_IDLE, g_testObserver->currentState );

    char* setAttentionState =
        generateSetAttentionStateWithoutOffset( AIA_UX_THINKING );
    TEST_ASSERT_NOT_NULL( setAttentionState );
    AiaSequenceNumber_t TEST_SEQUENCE_NUMBER = 44;
    size_t TEST_INDEX = 44;
    AiaUXManager_OnSetAttentionStateDirectiveReceived(
        g_testUxManager, setAttentionState, strlen( setAttentionState ),
        TEST_SEQUENCE_NUMBER, TEST_INDEX );
    AiaFree( setAttentionState );
    TEST_ASSERT_EQUAL( AIA_UX_THINKING, g_testObserver->currentState );

    TEST_ASSERT_EQUAL( AIA_UX_THINKING,
                       AiaUXManager_GetUXState( g_testUxManager ) );
}
