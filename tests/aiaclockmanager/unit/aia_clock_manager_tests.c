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
 * @file aia_clock_manager_tests.c
 * @brief Tests for AiaClockManager_t.
 */

/* The config header is always included first. */
#include <aia_config.h>

#include <aiamockregulator/aia_mock_regulator.h>
#include <aiatestutilities/aia_test_utilities.h>

#include <aiacore/aia_events.h>
#include <aiacore/aia_json_message.h>
#include <aiacore/aia_json_utils.h>
#include <aiacore/aia_mbedtls_threading.h>
#include <aiacore/aia_random_mbedtls.h>

#include <aiaclockmanager/aia_clock_manager.h>
#include <aiaclockmanager/private/aia_clock_manager.h>

/* Test framework includes. */
#include <unity_fixture.h>

#include <inttypes.h>
#include <string.h>

/** Observer needed to check certain functions are being called */
typedef struct AiaClockManagerTestObserver AiaClockManagerTestObserver_t;
static AiaClockManagerTestObserver_t* AiaClockManagerTestObserver_Create();
static void AiaClockManagerTestObserver_Destroy(
    AiaClockManagerTestObserver_t* observer );

/** Object used to mock the emission of events. */
static AiaMockRegulator_t* g_mockEventRegulator;
static AiaRegulator_t* g_regulator; /* pre-casted copy of g_mockRegulator */
static AiaClockManager_t* g_clockManager;
static AiaClockManagerTestObserver_t* g_observer;

/**
 * Used to pull a message out of the @c g_mockEventRegulator and assert that it
 * is an @c AIA_EVENTS_SYNCHRONIZE_CLOCK event
 *
 */
static void TestSynchronizeClockIsGenerated()
{
    TEST_ASSERT_TRUE(
        AiaSemaphore( TryWait )( &g_mockEventRegulator->writeSemaphore ) );
    AiaListDouble( Link_t )* link = NULL;
    link = AiaListDouble( PeekHead )( &g_mockEventRegulator->writtenMessages );
    AiaListDouble( RemoveHead )( &g_mockEventRegulator->writtenMessages );
    TEST_ASSERT_NOT_NULL( link );
    AiaJsonMessage_t* synchronizeClockMessage = AiaJsonMessage_FromMessage(
        ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
    TEST_ASSERT_EQUAL(
        strcmp( AiaJsonMessage_GetName( synchronizeClockMessage ),
                AIA_EVENTS_SYNCHRONIZE_CLOCK ),
        0 );
    const char* payload =
        AiaJsonMessage_GetJsonPayload( synchronizeClockMessage );
    TEST_ASSERT_NULL( payload );
}

/**
 * Generates an @c AIA_DIRECTIVE_SET_CLOCK payload.
 *
 * @param currentTime The time encoded @c AIA_SET_CLOCK_CURRENT_TIME_KEY
 * value.
 * @return A null-terminated payload.
 * @note Callers must clean up non-NULL returned values using AiaFree().
 */
static char* generateSetClock( AiaJsonLongType currentTime )
{
    /* clang-format off */
    static const char* formatPayload =
    "{"
        "\""AIA_SET_CLOCK_CURRENT_TIME_KEY"\":%"PRIu64
    "}";
    /* clang-format on */

    int numCharsRequired = snprintf( NULL, 0, formatPayload, currentTime );
    TEST_ASSERT_GREATER_THAN( 0, numCharsRequired );
    char* fullPayloadBuffer = AiaCalloc( 1, numCharsRequired + 1 );
    TEST_ASSERT_NOT_NULL( fullPayloadBuffer );
    TEST_ASSERT_EQUAL( numCharsRequired,
                       snprintf( fullPayloadBuffer, numCharsRequired + 1,
                                 formatPayload, currentTime ) );
    return fullPayloadBuffer;
}

struct AiaClockManagerTestObserver
{
    /** Keeps track of the number of observer notifications */
    AiaSemaphore_t numObserversNotifiedSemaphore;
};

static AiaClockManagerTestObserver_t* AiaClockManagerTestObserver_Create()
{
    AiaClockManagerTestObserver_t* observer =
        AiaCalloc( 1, sizeof( AiaClockManagerTestObserver_t ) );
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

static void AiaClockManagerTestObserver_Destroy(
    AiaClockManagerTestObserver_t* observer )
{
    AiaSemaphore( Destroy )( &observer->numObserversNotifiedSemaphore );
    AiaFree( observer );
}

static AiaTimepointSeconds_t currentTime = 0;

/* Override of config for testing purposes. */
void AiaClock_SetTimeSinceNTPEpoch( AiaTimepointSeconds_t secondsSinceEpoch )
{
    currentTime = secondsSinceEpoch;
}

void AiaClock_NotifyObservers( void* userData,
                               AiaTimepointSeconds_t currentTime )
{
    (void)currentTime;
    TEST_ASSERT_TRUE( userData );
    AiaClockManagerTestObserver_t* observer =
        (AiaClockManagerTestObserver_t*)userData;
    AiaSemaphore( Post )( &observer->numObserversNotifiedSemaphore );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test group for AiaClockManager_t tests.
 */
TEST_GROUP( AiaClockManagerTests );

/*-----------------------------------------------------------*/

/**
 * @brief Test group runner for AiaClockManager_t tests.
 */
TEST_GROUP_RUNNER( AiaClockManagerTests )
{
    RUN_TEST_CASE( AiaClockManagerTests, Creation );
    RUN_TEST_CASE( AiaClockManagerTests, SynchronizeClockIsSent );
    RUN_TEST_CASE( AiaClockManagerTests, BadSetClockResultsInException );
    RUN_TEST_CASE( AiaClockManagerTests, GoodSetClockIsHandled );
}

/*-----------------------------------------------------------*/

/*-----------------------------------------------------------*/
/**
 * @brief Test setup for AiaClockManager_t tests.
 */
TEST_SETUP( AiaClockManagerTests )
{
    /** Sample Seed. */
    static const char* TEST_SALT = "TestSalt";
    static const size_t TEST_SALT_LENGTH = sizeof( TEST_SALT ) - 1;
    AiaMbedtlsThreading_Init();
    AiaRandomMbedtls_Init();
    TEST_ASSERT_TRUE( AiaRandomMbedtls_Seed( TEST_SALT, TEST_SALT_LENGTH ) );

    g_observer = AiaClockManagerTestObserver_Create();
    TEST_ASSERT_TRUE( g_observer );
    g_mockEventRegulator = AiaMockRegulator_Create();
    TEST_ASSERT_TRUE( g_mockEventRegulator );
    g_regulator = (AiaRegulator_t*)g_mockEventRegulator;

    g_clockManager = AiaClockManager_Create(
        g_regulator, AiaClock_NotifyObservers, g_observer );
    TEST_ASSERT_NOT_NULL( g_clockManager );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test tear down for AiaClockManager_t tests.
 */
TEST_TEAR_DOWN( AiaClockManagerTests )
{
    AiaClockManager_Destroy( g_clockManager );
    AiaMockRegulator_Destroy( g_mockEventRegulator,
                              AiaTestUtilities_DestroyJsonChunk, NULL );
    AiaClockManagerTestObserver_Destroy( g_observer );
    AiaMbedtlsThreading_Cleanup();
    AiaRandomMbedtls_Cleanup();
}

/*-----------------------------------------------------------*/

TEST( AiaClockManagerTests, Creation )
{
    AiaClockManager_t* clockManager =
        AiaClockManager_Create( NULL, AiaClock_NotifyObservers, g_observer );
    TEST_ASSERT_NULL( clockManager );

    clockManager = AiaClockManager_Create( g_regulator, NULL, g_observer );
    TEST_ASSERT_NOT_NULL( clockManager );

    clockManager =
        AiaClockManager_Create( g_regulator, AiaClock_NotifyObservers, NULL );
    TEST_ASSERT_NOT_NULL( clockManager );
}

TEST( AiaClockManagerTests, SynchronizeClockIsSent )
{
    TEST_ASSERT_TRUE( AiaClockManager_SynchronizeClock( g_clockManager ) );
    TestSynchronizeClockIsGenerated();
}

TEST( AiaClockManagerTests, BadSetClockResultsInException )
{
    AiaSequenceNumber_t TEST_SEQUENCE_NUMBER = 4;
    size_t TEST_INDEX = 1;
    AiaClockManager_OnSetClockDirectiveReceived(
        g_clockManager, "{}", strlen( "{}" ), TEST_SEQUENCE_NUMBER,
        TEST_INDEX );
    AiaTestUtilities_TestMalformedMessageExceptionIsGenerated(
        g_mockEventRegulator, TEST_SEQUENCE_NUMBER, TEST_INDEX );
    TEST_ASSERT_EQUAL( 0, currentTime );
    TEST_ASSERT_FALSE( AiaSemaphore( TimedWait )(
        &g_observer->numObserversNotifiedSemaphore, 100 ) );
}

TEST( AiaClockManagerTests, GoodSetClockIsHandled )
{
    AiaSequenceNumber_t TEST_SEQUENCE_NUMBER = 4;
    size_t TEST_INDEX = 1;
    AiaJsonLongType TEST_TIME = 44;
    char* setClockDirective = generateSetClock( TEST_TIME );
    TEST_ASSERT_NOT_NULL( setClockDirective );
    AiaClockManager_OnSetClockDirectiveReceived(
        g_clockManager, (void*)setClockDirective, strlen( setClockDirective ),
        TEST_SEQUENCE_NUMBER, TEST_INDEX );
    AiaFree( setClockDirective );
    TEST_ASSERT_EQUAL( TEST_TIME, currentTime );
    TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
        &g_observer->numObserversNotifiedSemaphore, 100 ) );
}
