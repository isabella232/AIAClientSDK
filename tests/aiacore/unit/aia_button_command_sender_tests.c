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
 * @file aia_button_command_sender_tests.c
 * @brief Tests for AiaButtonCommandSender_t.
 */

/* The config header is always included first. */
#include <aia_config.h>

/* Aia headers */
#include <aiacore/aia_button_command.h>
#include <aiacore/aia_button_command_sender.h>
#include <aiacore/aia_events.h>
#include <aiacore/aia_json_message.h>
#include <aiacore/aia_json_utils.h>
#include <aiacore/aia_mbedtls_threading.h>
#include <aiacore/aia_random_mbedtls.h>

#include <aiamockregulator/aia_mock_regulator.h>
#include <aiatestutilities/aia_test_utilities.h>

/* Test framework includes. */
#include <unity_fixture.h>

/*-----------------------------------------------------------*/

/** Sample Seed. */
static const char* TEST_SALT = "TestSalt";
static const size_t TEST_SALT_LENGTH = sizeof( TEST_SALT ) - 1;

/** Object used to mock the emission of events. */
static AiaMockRegulator_t* g_mockEventRegulator;
static AiaRegulator_t* g_regulator; /* pre-casted copy of g_mockRegulator */

/** Count used to track calls to @c AiaMockStopPlayback() */
static size_t localStopsCounter;

/** Passed to the @c AiaButtonCommandSender_t as a way to stop local playback.
 */
static void AiaMockStopPlayback( void* userData )
{
    (void)userData;
    ++localStopsCounter;
}

/**
 * Used to pull a message out of the @c testEventRegulator and assert that it is
 * an @c AIA_EVENTS_BUTTON_COMMAND_ISSUED event
 *
 * @param expectedButton The button to verify is embedded in the event.
 */
static void TestButtonCommandIssuedIsGenerated(
    AiaButtonCommand_t expectedButton )
{
    TEST_ASSERT_TRUE(
        AiaSemaphore( TryWait )( &g_mockEventRegulator->writeSemaphore ) );
    AiaListDouble( Link_t )* link = NULL;
    link = AiaListDouble( PeekHead )( &g_mockEventRegulator->writtenMessages );
    AiaListDouble( RemoveHead )( &g_mockEventRegulator->writtenMessages );
    TEST_ASSERT_NOT_NULL( link );
    AiaJsonMessage_t* buttonCommandIssuedMessage = AiaJsonMessage_FromMessage(
        ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
    TEST_ASSERT_EQUAL(
        strcmp( AiaJsonMessage_GetName( buttonCommandIssuedMessage ),
                AIA_EVENTS_BUTTON_COMMAND_ISSUED ),
        0 );
    const char* payload =
        AiaJsonMessage_GetJsonPayload( buttonCommandIssuedMessage );
    TEST_ASSERT_NOT_NULL( payload );

    const char* button = NULL;
    size_t buttonLen = 0;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_BUTTON_COMMAND_ISSUED_COMMAND_KEY,
        strlen( AIA_BUTTON_COMMAND_ISSUED_COMMAND_KEY ), &button,
        &buttonLen ) );
    TEST_ASSERT_NOT_NULL( button );
    TEST_ASSERT_TRUE( AiaJsonUtils_UnquoteString( &button, &buttonLen ) );
    TEST_ASSERT_EQUAL_STRING_LEN( AiaButtonCommand_ToString( expectedButton ),
                                  button, buttonLen );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test group for AiaButtonCommandSender_t tests.
 */
TEST_GROUP( AiaButtonCommandTests );

/*-----------------------------------------------------------*/

/**
 * @brief Test group runner for AiaButtonCommandSender_t tests.
 */
TEST_GROUP_RUNNER( AiaButtonCommandTests )
{
    RUN_TEST_CASE( AiaButtonCommandTests, Creation );
    RUN_TEST_CASE( AiaButtonCommandTests,
                   ButtonCommandPressesWithoutLocalStops );
    RUN_TEST_CASE( AiaButtonCommandTests, ButtonCommandPressesWithLocalStops );
}

/*-----------------------------------------------------------*/
/**
 * @brief Test setup for AiaButtonCommandSender_t tests.
 */
TEST_SETUP( AiaButtonCommandTests )
{
    AiaRandomMbedtls_Init();
    TEST_ASSERT_TRUE( AiaRandomMbedtls_Seed( TEST_SALT, TEST_SALT_LENGTH ) );

    g_mockEventRegulator = AiaMockRegulator_Create();
    TEST_ASSERT_NOT_NULL( g_mockEventRegulator );
    g_regulator = (AiaRegulator_t*)g_mockEventRegulator;
    localStopsCounter = 0;
}

/*-----------------------------------------------------------*/

/**
 * @brief Test tear down for AiaButtonCommandSender_t tests.
 */
TEST_TEAR_DOWN( AiaButtonCommandTests )
{
    AiaMockRegulator_Destroy( g_mockEventRegulator,
                              AiaTestUtilities_DestroyJsonChunk, NULL );
    AiaRandomMbedtls_Cleanup();
}

/*-----------------------------------------------------------*/

TEST( AiaButtonCommandTests, Creation )
{
    AiaButtonCommandSender_t* buttonCommandSender = NULL;

    /* Null regulator. */
    buttonCommandSender =
        AiaButtonCommandSender_Create( NULL, AiaMockStopPlayback, NULL );
    TEST_ASSERT_NULL( buttonCommandSender );

    /* Creation should work with all parameters passed in and local stops
     * enabled. */
    buttonCommandSender =
        AiaButtonCommandSender_Create( g_regulator, AiaMockStopPlayback, NULL );
    TEST_ASSERT_NOT_NULL( buttonCommandSender );
    AiaButtonCommandSender_Destroy( buttonCommandSender );
}

TEST( AiaButtonCommandTests, ButtonCommandPressesWithoutLocalStops )
{
    AiaButtonCommandSender_t* buttonCommandSender =
        AiaButtonCommandSender_Create( g_regulator, NULL, NULL );
    TEST_ASSERT_NOT_NULL( buttonCommandSender );

    TEST_ASSERT_TRUE( AiaButtonCommandSender_OnButtonPressed(
        buttonCommandSender, AIA_BUTTON_PLAY ) );
    TestButtonCommandIssuedIsGenerated( AIA_BUTTON_PLAY );

    TEST_ASSERT_TRUE( AiaButtonCommandSender_OnButtonPressed(
        buttonCommandSender, AIA_BUTTON_NEXT ) );
    TestButtonCommandIssuedIsGenerated( AIA_BUTTON_NEXT );

    TEST_ASSERT_TRUE( AiaButtonCommandSender_OnButtonPressed(
        buttonCommandSender, AIA_BUTTON_PREVIOUS ) );
    TestButtonCommandIssuedIsGenerated( AIA_BUTTON_PREVIOUS );

    TEST_ASSERT_TRUE( AiaButtonCommandSender_OnButtonPressed(
        buttonCommandSender, AIA_BUTTON_STOP ) );
    TestButtonCommandIssuedIsGenerated( AIA_BUTTON_STOP );

    TEST_ASSERT_TRUE( AiaButtonCommandSender_OnButtonPressed(
        buttonCommandSender, AIA_BUTTON_PAUSE ) );
    TestButtonCommandIssuedIsGenerated( AIA_BUTTON_PAUSE );

    TEST_ASSERT_EQUAL( 0, localStopsCounter );
    AiaButtonCommandSender_Destroy( buttonCommandSender );
}

TEST( AiaButtonCommandTests, ButtonCommandPressesWithLocalStops )
{
    AiaButtonCommandSender_t* buttonCommandSender =
        AiaButtonCommandSender_Create( g_regulator, AiaMockStopPlayback, NULL );
    TEST_ASSERT_NOT_NULL( buttonCommandSender );

    TEST_ASSERT_TRUE( AiaButtonCommandSender_OnButtonPressed(
        buttonCommandSender, AIA_BUTTON_PLAY ) );
    TestButtonCommandIssuedIsGenerated( AIA_BUTTON_PLAY );

    TEST_ASSERT_TRUE( AiaButtonCommandSender_OnButtonPressed(
        buttonCommandSender, AIA_BUTTON_NEXT ) );
    TestButtonCommandIssuedIsGenerated( AIA_BUTTON_NEXT );

    TEST_ASSERT_TRUE( AiaButtonCommandSender_OnButtonPressed(
        buttonCommandSender, AIA_BUTTON_PREVIOUS ) );
    TestButtonCommandIssuedIsGenerated( AIA_BUTTON_PREVIOUS );

    TEST_ASSERT_TRUE( AiaButtonCommandSender_OnButtonPressed(
        buttonCommandSender, AIA_BUTTON_STOP ) );
    TEST_ASSERT_EQUAL( 1, localStopsCounter );
    TestButtonCommandIssuedIsGenerated( AIA_BUTTON_STOP );

    TEST_ASSERT_TRUE( AiaButtonCommandSender_OnButtonPressed(
        buttonCommandSender, AIA_BUTTON_PAUSE ) );
    TEST_ASSERT_EQUAL( 2, localStopsCounter );
    TestButtonCommandIssuedIsGenerated( AIA_BUTTON_PAUSE );

    AiaButtonCommandSender_Destroy( buttonCommandSender );
}
