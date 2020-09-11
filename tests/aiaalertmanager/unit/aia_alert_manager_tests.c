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
 * @file aia_alert_manager_tests.c
 * @brief Tests for AiaAlertManager_t.
 */

/* The config header is always included first. */
#include <aia_config.h>

/* Aia headers */
#include <aiaalertmanager/aia_alert_manager.h>
#include <aiaalertmanager/private/aia_alert_manager.h>
#include <aiacore/aia_events.h>
#include <aiacore/aia_json_message.h>
#include <aiacore/aia_json_utils.h>
#include <aiacore/aia_mbedtls_threading.h>
#include <aiacore/aia_random_mbedtls.h>
#include <aiacore/aia_volume_constants.h>

#include <aiamockregulator/aia_mock_regulator.h>
#include <aiatestutilities/aia_test_utilities.h>

/* Test framework includes. */
#include <unity_fixture.h>

#include <inttypes.h>
#include <string.h>

/*-----------------------------------------------------------*/

/** Sample Seed. */
static const char* TEST_SALT = "TestSalt";
static const size_t TEST_SALT_LENGTH = sizeof( TEST_SALT ) - 1;

static AiaAlertManager_t* g_testAlertManager;
static AiaMockRegulator_t* g_mockRegulator;
static AiaRegulator_t* g_regulator; /* pre-casted copy of g_mockRegulator */

static void TestSetAlertVolumeIsGenerated( const uint8_t alertVolume )
{
    TEST_ASSERT_TRUE(
        AiaSemaphore( TryWait )( &g_mockRegulator->writeSemaphore ) );
    AiaListDouble( Link_t )* link = NULL;
    link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
    AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
    TEST_ASSERT_NOT_NULL( link );
    AiaJsonMessage_t* alertVolumeChangedMessage = AiaJsonMessage_FromMessage(
        ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
    TEST_ASSERT_EQUAL(
        strcmp( AiaJsonMessage_GetName( alertVolumeChangedMessage ),
                AIA_EVENTS_ALERT_VOLUME_CHANGED ),
        0 );
    const char* payload =
        AiaJsonMessage_GetJsonPayload( alertVolumeChangedMessage );
    TEST_ASSERT_NOT_NULL( payload );

    const char* alertVolumeStr = NULL;
    size_t alertVolumeLen = 0;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_ALERT_VOLUME_CHANGED_VOLUME_KEY,
        strlen( AIA_ALERT_VOLUME_CHANGED_VOLUME_KEY ), &alertVolumeStr,
        &alertVolumeLen ) );

    AiaJsonLongType parsedAlertVolume;
    TEST_ASSERT_TRUE( AiaExtractLongFromJsonValue(
        alertVolumeStr, alertVolumeLen, &parsedAlertVolume ) );
    TEST_ASSERT_EQUAL( alertVolume, parsedAlertVolume );
}

static void TestSetAlertSucceededIsGenerated( const char* token )
{
    TEST_ASSERT_TRUE(
        AiaSemaphore( TryWait )( &g_mockRegulator->writeSemaphore ) );
    AiaListDouble( Link_t )* link = NULL;
    link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
    AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
    TEST_ASSERT_NOT_NULL( link );
    AiaJsonMessage_t* setAlertSucceededMessage = AiaJsonMessage_FromMessage(
        ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
    TEST_ASSERT_EQUAL(
        strcmp( AiaJsonMessage_GetName( setAlertSucceededMessage ),
                AIA_EVENTS_SET_ALERT_SUCCEEDED ),
        0 );
    const char* payload =
        AiaJsonMessage_GetJsonPayload( setAlertSucceededMessage );
    TEST_ASSERT_NOT_NULL( payload );

    const char* alertTokenStr = NULL;
    size_t alertTokenStrLen = 0;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_SET_ALERT_SUCCEEDED_TOKEN_KEY,
        strlen( AIA_SET_ALERT_SUCCEEDED_TOKEN_KEY ), &alertTokenStr,
        &alertTokenStrLen ) );
    TEST_ASSERT_EQUAL_STRING_LEN( token, alertTokenStr, alertTokenStrLen );
}

static void TestDeleteAlertSucceededIsGenerated( const char* token )
{
    TEST_ASSERT_TRUE(
        AiaSemaphore( TryWait )( &g_mockRegulator->writeSemaphore ) );
    AiaListDouble( Link_t )* link = NULL;
    link = AiaListDouble( PeekHead )( &g_mockRegulator->writtenMessages );
    AiaListDouble( RemoveHead )( &g_mockRegulator->writtenMessages );
    TEST_ASSERT_NOT_NULL( link );
    AiaJsonMessage_t* deleteAlertSucceededMessage = AiaJsonMessage_FromMessage(
        ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
    TEST_ASSERT_EQUAL(
        strcmp( AiaJsonMessage_GetName( deleteAlertSucceededMessage ),
                AIA_EVENTS_DELETE_ALERT_SUCCEEDED ),
        0 );
    const char* payload =
        AiaJsonMessage_GetJsonPayload( deleteAlertSucceededMessage );
    TEST_ASSERT_NOT_NULL( payload );

    const char* alertTokenStr = NULL;
    size_t alertTokenStrLen = 0;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_DELETE_ALERT_SUCCEEDED_TOKEN_KEY,
        strlen( AIA_DELETE_ALERT_SUCCEEDED_TOKEN_KEY ), &alertTokenStr,
        &alertTokenStrLen ) );
    TEST_ASSERT_EQUAL_STRING_LEN( token, alertTokenStr, alertTokenStrLen );
}

static AiaTimepointSeconds_t currentTime = 0;

#ifdef AIA_ENABLE_SPEAKER
static bool SpeakerCheckCallback( void* userData )
{
    (void)userData;

    return true;
}

static bool StartOfflineAlertToneCallback( const AiaAlertSlot_t* offlineAlert,
                                           void* userData,
                                           uint8_t offlineAlertVolume )
{
    (void)offlineAlert;
    (void)userData;
    (void)offlineAlertVolume;

    return true;
}
#endif

static void UXUpdateCallback( void* userData,
                              AiaServerAttentionState_t newAttentionState )
{
    (void)userData;
    (void)newAttentionState;
}

static AiaUXState_t UXCheckCallback( void* userData )
{
    (void)userData;

    return true;
}

static bool DisconnectCallback( void* userData, int code,
                                const char* description )
{
    (void)userData;
    (void)code;
    (void)description;

    return true;
}

/* TODO: ADSER-1866 - Implement mocked alert storage functions */
bool AiaStoreAlert( const char* alertToken, size_t alertTokenLen,
                    AiaTimepointSeconds_t scheduledTime,
                    AiaDurationMs_t duration, uint8_t alertType )
{
    (void)alertToken;
    (void)alertTokenLen;
    (void)scheduledTime;
    (void)duration;
    (void)alertType;

    return true;
}

bool AiaLoadAlerts( uint8_t* allAlerts, size_t size )
{
    (void)allAlerts;
    (void)size;

    return true;
}

bool AiaDeleteAlert( const char* alertToken, size_t alertTokenLen )
{
    (void)alertToken;
    (void)alertTokenLen;

    return true;
}

bool AiaLoadAlert( char* alertToken, size_t alertTokenLen,
                   AiaTimepointSeconds_t* scheduledTime,
                   AiaDurationMs_t* duration, uint8_t* alertType,
                   const uint8_t* allAlertsBuffer )
{
    (void)alertToken;
    (void)alertTokenLen;
    (void)scheduledTime;
    (void)duration;
    (void)alertType;
    (void)allAlertsBuffer;

    return true;
}

size_t AiaGetAlertsSize()
{
    return 0;
}

bool AiaAlertsBlobExists()
{
    return true;
}

AiaTimepointSeconds_t AiaClock_GetTimeSinceNTPEpoch()
{
    return 0;
}

/*-----------------------------------------------------------*/

/**
 * @brief Test group for AiaAlertManager_t tests.
 */
TEST_GROUP( AiaAlertManagerTests );

/*-----------------------------------------------------------*/

/**
 * @brief Test setup for AiaAlertManager_t tests.
 */
TEST_SETUP( AiaAlertManagerTests )
{
    AiaMbedtlsThreading_Init();
    AiaRandomMbedtls_Init();
    TEST_ASSERT_TRUE( AiaRandomMbedtls_Seed( TEST_SALT, TEST_SALT_LENGTH ) );

    /** Create g_mockRegulator */
    g_mockRegulator = AiaMockRegulator_Create();
    TEST_ASSERT_TRUE( g_mockRegulator );
    g_regulator = (AiaRegulator_t*)g_mockRegulator;

    /** Create the g_testAlertManager */
    g_testAlertManager = AiaAlertManager_Create(
        g_regulator
#ifdef AIA_ENABLE_SPEAKER
        ,
        SpeakerCheckCallback, NULL, StartOfflineAlertToneCallback, NULL
#endif
        ,
        UXUpdateCallback, NULL, UXCheckCallback, NULL, DisconnectCallback,
        NULL );
    TEST_ASSERT_NOT_NULL( g_testAlertManager );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test tear down for AiaAlertManager_t tests.
 */
TEST_TEAR_DOWN( AiaAlertManagerTests )
{
    AiaMbedtlsThreading_Cleanup();
    AiaRandomMbedtls_Cleanup();
    AiaAlertManager_Destroy( g_testAlertManager );
    AiaMockRegulator_Destroy( g_mockRegulator,
                              AiaTestUtilities_DestroyJsonChunk, NULL );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test group runner for AiaAlertManager_t tests.
 */
TEST_GROUP_RUNNER( AiaAlertManagerTests )
{
    RUN_TEST_CASE( AiaAlertManagerTests, Create );
    RUN_TEST_CASE( AiaAlertManagerTests, DestroyNull );
    RUN_TEST_CASE( AiaAlertManagerTests, BadAlertDirectiveHandling );
    RUN_TEST_CASE( AiaAlertManagerTests, ValidAlertDirectiveHandling );
    RUN_TEST_CASE( AiaAlertManagerTests, UpdateAlertManagerTime );
#ifdef AIA_ENABLE_SPEAKER
    RUN_TEST_CASE( AiaAlertManagerTests, UpdateSpeakerBufferState );
#endif
    RUN_TEST_CASE( AiaAlertManagerTests, DeleteAlert );
    RUN_TEST_CASE( AiaAlertManagerTests, UpdateUXState );
}

TEST( AiaAlertManagerTests, Create )
{
    AiaAlertManager_t* invalidAlertManager = NULL;

    invalidAlertManager = AiaAlertManager_Create(
        NULL
#ifdef AIA_ENABLE_SPEAKER
        ,
        SpeakerCheckCallback, NULL, StartOfflineAlertToneCallback, NULL
#endif
        ,
        UXUpdateCallback, NULL, UXCheckCallback, NULL, DisconnectCallback,
        NULL );
    TEST_ASSERT_NULL( invalidAlertManager );

    invalidAlertManager =
        AiaAlertManager_Create( g_regulator
#ifdef AIA_ENABLE_SPEAKER
                                ,
                                NULL, NULL, StartOfflineAlertToneCallback, NULL
#endif
                                ,
                                UXUpdateCallback, NULL, UXCheckCallback, NULL,
                                DisconnectCallback, NULL );
    TEST_ASSERT_NULL( invalidAlertManager );

    invalidAlertManager =
        AiaAlertManager_Create( g_regulator
#ifdef AIA_ENABLE_SPEAKER
                                ,
                                SpeakerCheckCallback, NULL, NULL, NULL
#endif
                                ,
                                UXUpdateCallback, NULL, UXCheckCallback, NULL,
                                DisconnectCallback, NULL );
    TEST_ASSERT_NULL( invalidAlertManager );

    invalidAlertManager = AiaAlertManager_Create(
        g_regulator
#ifdef AIA_ENABLE_SPEAKER
        ,
        SpeakerCheckCallback, NULL, StartOfflineAlertToneCallback, NULL
#endif
        ,
        NULL, NULL, UXCheckCallback, NULL, DisconnectCallback, NULL );
    TEST_ASSERT_NULL( invalidAlertManager );

    invalidAlertManager = AiaAlertManager_Create(
        g_regulator
#ifdef AIA_ENABLE_SPEAKER
        ,
        SpeakerCheckCallback, NULL, StartOfflineAlertToneCallback, NULL
#endif
        ,
        UXUpdateCallback, NULL, NULL, NULL, DisconnectCallback, NULL );
    TEST_ASSERT_NULL( invalidAlertManager );

    invalidAlertManager = AiaAlertManager_Create(
        g_regulator
#ifdef AIA_ENABLE_SPEAKER
        ,
        SpeakerCheckCallback, NULL, StartOfflineAlertToneCallback, NULL
#endif
        ,
        UXUpdateCallback, NULL, UXCheckCallback, NULL, NULL, NULL );
    TEST_ASSERT_NULL( invalidAlertManager );
}

TEST( AiaAlertManagerTests, DestroyNull )
{
    /* No asserts just to exercise code path. */
    AiaAlertManager_Destroy( NULL );
}

TEST( AiaAlertManagerTests, BadAlertDirectiveHandling )
{
    /* clang-format off */
    static const char* SET_ALERT_WITHOUT_TOKEN =
    "{"
        "\""AIA_SET_ALERT_SCHEDULED_TIME_KEY"\":100,"
        "\""AIA_SET_ALERT_DURATION_IN_MILLISECONDS_KEY"\":100,"
        "\""AIA_SET_ALERT_TYPE_KEY"\":\"TIMER\""
    "}";

    /* clang-format on */
    AiaSequenceNumber_t TEST_SEQUENCE_NUMBER = 4;
    size_t TEST_INDEX = 44;
    AiaAlertManager_OnSetAlertDirectiveReceived(
        g_testAlertManager, (void*)SET_ALERT_WITHOUT_TOKEN,
        strlen( SET_ALERT_WITHOUT_TOKEN ), TEST_SEQUENCE_NUMBER, TEST_INDEX );
    AiaTestUtilities_TestMalformedMessageExceptionIsGenerated(
        g_mockRegulator, TEST_SEQUENCE_NUMBER, TEST_INDEX );

    /* clang-format off */
    static const char* SET_ALERT_WITHOUT_SCHEDULED_TIME =
    "{"
        "\""AIA_SET_ALERT_TOKEN_KEY"\":\"abcdefgh\","
        "\""AIA_SET_ALERT_DURATION_IN_MILLISECONDS_KEY"\":100,"
        "\""AIA_SET_ALERT_TYPE_KEY"\":\"TIMER\""
    "}";

    /* clang-format on */
    AiaAlertManager_OnSetAlertDirectiveReceived(
        g_testAlertManager, (void*)SET_ALERT_WITHOUT_SCHEDULED_TIME,
        strlen( SET_ALERT_WITHOUT_SCHEDULED_TIME ), TEST_SEQUENCE_NUMBER,
        TEST_INDEX );
    AiaTestUtilities_TestMalformedMessageExceptionIsGenerated(
        g_mockRegulator, TEST_SEQUENCE_NUMBER, TEST_INDEX );

    /* clang-format off */
    static const char* SET_ALERT_WITHOUT_DURATION =
    "{"
        "\""AIA_SET_ALERT_TOKEN_KEY"\":\"abcdefgh\","
        "\""AIA_SET_ALERT_SCHEDULED_TIME_KEY"\":100,"
        "\""AIA_SET_ALERT_TYPE_KEY"\":\"TIMER\""
    "}";

    /* clang-format on */
    AiaAlertManager_OnSetAlertDirectiveReceived(
        g_testAlertManager, (void*)SET_ALERT_WITHOUT_DURATION,
        strlen( SET_ALERT_WITHOUT_DURATION ), TEST_SEQUENCE_NUMBER,
        TEST_INDEX );
    AiaTestUtilities_TestMalformedMessageExceptionIsGenerated(
        g_mockRegulator, TEST_SEQUENCE_NUMBER, TEST_INDEX );

    /* clang-format off */
    static const char* SET_ALERT_WITHOUT_TYPE =
    "{"
        "\""AIA_SET_ALERT_TOKEN_KEY"\":\"abcdefgh\","
        "\""AIA_SET_ALERT_SCHEDULED_TIME_KEY"\":100,"
        "\""AIA_SET_ALERT_DURATION_IN_MILLISECONDS_KEY"\":100"
    "}";

    /* clang-format on */
    AiaAlertManager_OnSetAlertDirectiveReceived(
        g_testAlertManager, (void*)SET_ALERT_WITHOUT_TYPE,
        strlen( SET_ALERT_WITHOUT_TYPE ), TEST_SEQUENCE_NUMBER, TEST_INDEX );
    AiaTestUtilities_TestMalformedMessageExceptionIsGenerated(
        g_mockRegulator, TEST_SEQUENCE_NUMBER, TEST_INDEX );

    /* clang-format off */
    static const char* SET_ALERT_WITH_INVALID_TOKEN =
    "{"
        "\""AIA_SET_ALERT_TOKEN_KEY"\":100,"
        "\""AIA_SET_ALERT_SCHEDULED_TIME_KEY"\":\"abc\","
        "\""AIA_SET_ALERT_DURATION_IN_MILLISECONDS_KEY"\":100,"
        "\""AIA_SET_ALERT_TYPE_KEY"\":\"TIMER\""
    "}";

    /* clang-format on */
    AiaAlertManager_OnSetAlertDirectiveReceived(
        g_testAlertManager, (void*)SET_ALERT_WITH_INVALID_TOKEN,
        strlen( SET_ALERT_WITH_INVALID_TOKEN ), TEST_SEQUENCE_NUMBER,
        TEST_INDEX );
    AiaTestUtilities_TestMalformedMessageExceptionIsGenerated(
        g_mockRegulator, TEST_SEQUENCE_NUMBER, TEST_INDEX );

    /* clang-format off */
    static const char* SET_ALERT_WITH_INVALID_TIME =
    "{"
        "\""AIA_SET_ALERT_TOKEN_KEY"\":\"abcdefgh\""
        "\""AIA_SET_ALERT_SCHEDULED_TIME_KEY"\":\"abc\","
        "\""AIA_SET_ALERT_DURATION_IN_MILLISECONDS_KEY"\":100,"
        "\""AIA_SET_ALERT_TYPE_KEY"\":\"TIMER\""
    "}";

    /* clang-format on */
    AiaAlertManager_OnSetAlertDirectiveReceived(
        g_testAlertManager, (void*)SET_ALERT_WITH_INVALID_TIME,
        strlen( SET_ALERT_WITH_INVALID_TIME ), TEST_SEQUENCE_NUMBER,
        TEST_INDEX );
    AiaTestUtilities_TestMalformedMessageExceptionIsGenerated(
        g_mockRegulator, TEST_SEQUENCE_NUMBER, TEST_INDEX );

    /* clang-format off */
    static const char* SET_ALERT_WITH_INVALID_DURATION =
    "{"
        "\""AIA_SET_ALERT_TOKEN_KEY"\":\"abcdefgh\""
        "\""AIA_SET_ALERT_SCHEDULED_TIME_KEY"\":100,"
        "\""AIA_SET_ALERT_DURATION_IN_MILLISECONDS_KEY"\":\"abc\","
        "\""AIA_SET_ALERT_TYPE_KEY"\":\"TIMER\""
    "}";

    /* clang-format on */
    AiaAlertManager_OnSetAlertDirectiveReceived(
        g_testAlertManager, (void*)SET_ALERT_WITH_INVALID_DURATION,
        strlen( SET_ALERT_WITH_INVALID_DURATION ), TEST_SEQUENCE_NUMBER,
        TEST_INDEX );
    AiaTestUtilities_TestMalformedMessageExceptionIsGenerated(
        g_mockRegulator, TEST_SEQUENCE_NUMBER, TEST_INDEX );

    /* clang-format off */
    static const char* SET_ALERT_WITH_INVALID_TYPE =
    "{"
        "\""AIA_SET_ALERT_TOKEN_KEY"\":\"abcdefgh\""
        "\""AIA_SET_ALERT_SCHEDULED_TIME_KEY"\":100,"
        "\""AIA_SET_ALERT_DURATION_IN_MILLISECONDS_KEY"\":100,"
        "\""AIA_SET_ALERT_TYPE_KEY"\":123"
    "}";

    /* clang-format on */
    AiaAlertManager_OnSetAlertDirectiveReceived(
        g_testAlertManager, (void*)SET_ALERT_WITH_INVALID_TYPE,
        strlen( SET_ALERT_WITH_INVALID_TYPE ), TEST_SEQUENCE_NUMBER,
        TEST_INDEX );
    AiaTestUtilities_TestMalformedMessageExceptionIsGenerated(
        g_mockRegulator, TEST_SEQUENCE_NUMBER, TEST_INDEX );

    /* clang-format off */
    static const char* DELETE_ALERT_WITHOUT_TOKEN = "{}";

    /* clang-format on */
    AiaAlertManager_OnDeleteAlertDirectiveReceived(
        g_testAlertManager, (void*)DELETE_ALERT_WITHOUT_TOKEN,
        strlen( DELETE_ALERT_WITHOUT_TOKEN ), TEST_SEQUENCE_NUMBER,
        TEST_INDEX );
    AiaTestUtilities_TestMalformedMessageExceptionIsGenerated(
        g_mockRegulator, TEST_SEQUENCE_NUMBER, TEST_INDEX );

    /* clang-format off */
    static const char* SET_ALERT_VOLUME_WITHOUT_VOLUME = "{}";

    /* clang-format on */
    AiaAlertManager_OnSetAlertVolumeDirectiveReceived(
        g_testAlertManager, (void*)SET_ALERT_VOLUME_WITHOUT_VOLUME,
        strlen( SET_ALERT_VOLUME_WITHOUT_VOLUME ), TEST_SEQUENCE_NUMBER,
        TEST_INDEX );
    AiaTestUtilities_TestMalformedMessageExceptionIsGenerated(
        g_mockRegulator, TEST_SEQUENCE_NUMBER, TEST_INDEX );
}

TEST( AiaAlertManagerTests, ValidAlertDirectiveHandling )
{
    /* clang-format off */
    static const char* SET_ALERT_VALID_PAYLOAD =
    "{"
        "\""AIA_SET_ALERT_TOKEN_KEY"\":\"abcdefgh\""
        "\""AIA_SET_ALERT_SCHEDULED_TIME_KEY"\":100,"
        "\""AIA_SET_ALERT_DURATION_IN_MILLISECONDS_KEY"\":100,"
        "\""AIA_SET_ALERT_TYPE_KEY"\":\"TIMER\""
    "}";

    /* clang-format on */
    AiaSequenceNumber_t TEST_SEQUENCE_NUMBER = 4;
    size_t TEST_INDEX = 44;
    AiaAlertManager_OnSetAlertDirectiveReceived(
        g_testAlertManager, (void*)SET_ALERT_VALID_PAYLOAD,
        strlen( SET_ALERT_VALID_PAYLOAD ), TEST_SEQUENCE_NUMBER, TEST_INDEX );
    TestSetAlertSucceededIsGenerated( "\"abcdefgh\"" );

    /* clang-format off */
    static const char* DELETE_ALERT_VALID_PAYLOAD =
    "{"
        "\""AIA_DELETE_ALERT_TOKEN_KEY"\":\"abcdefgh\""
    "}";

    /* clang-format on */
    AiaAlertManager_OnDeleteAlertDirectiveReceived(
        g_testAlertManager, (void*)DELETE_ALERT_VALID_PAYLOAD,
        strlen( DELETE_ALERT_VALID_PAYLOAD ), TEST_SEQUENCE_NUMBER,
        TEST_INDEX );
    TestDeleteAlertSucceededIsGenerated( "\"abcdefgh\"" );

    /* Try deleting an alert that has not been created yet */
    /* clang-format off */
    static const char* DELETE_ALERT_NON_EXISTANT_PAYLOAD =
    "{"
        "\""AIA_DELETE_ALERT_TOKEN_KEY"\":\"ijklmnop\""
    "}";

    /* clang-format on */
    AiaAlertManager_OnDeleteAlertDirectiveReceived(
        g_testAlertManager, (void*)DELETE_ALERT_NON_EXISTANT_PAYLOAD,
        strlen( DELETE_ALERT_NON_EXISTANT_PAYLOAD ), TEST_SEQUENCE_NUMBER,
        TEST_INDEX );
    TestDeleteAlertSucceededIsGenerated( "\"ijklmnop\"" );

    /* clang-format off */
    static const char* SET_ALERT_VOLUME_VALID_PAYLOAD =
    "{"
        "\""AIA_SET_ALERT_VOLUME_VOLUME_KEY"\":10"
    "}";

    /* clang-format on */
    AiaAlertManager_OnSetAlertVolumeDirectiveReceived(
        g_testAlertManager, (void*)SET_ALERT_VOLUME_VALID_PAYLOAD,
        strlen( SET_ALERT_VOLUME_VALID_PAYLOAD ), TEST_SEQUENCE_NUMBER,
        TEST_INDEX );
    TestSetAlertVolumeIsGenerated( 10 );
}

TEST( AiaAlertManagerTests, UpdateAlertManagerTime )
{
    TEST_ASSERT_FALSE(
        AiaAlertManager_UpdateAlertManagerTime( NULL, currentTime ) );
    TEST_ASSERT_TRUE( AiaAlertManager_UpdateAlertManagerTime(
        g_testAlertManager, currentTime ) );
}

#ifdef AIA_ENABLE_SPEAKER
TEST( AiaAlertManagerTests, UpdateSpeakerBufferState )
{
    /* No asserts just to exercise code path. */
    /* TODO: ADSER-1866 - Implement mocked alert storage functions */
    AiaAlertManager_UpdateSpeakerBufferState( NULL, AIA_UNDERRUN_STATE );
    AiaAlertManager_UpdateSpeakerBufferState( g_testAlertManager,
                                              AIA_UNDERRUN_STATE );
}
#endif

TEST( AiaAlertManagerTests, DeleteAlert )
{
    TEST_ASSERT_FALSE( AiaAlertManager_DeleteAlert( NULL, "testAlertToken" ) );
    TEST_ASSERT_TRUE(
        AiaAlertManager_DeleteAlert( g_testAlertManager, "testAlertToken" ) );
}

TEST( AiaAlertManagerTests, UpdateUXState )
{
    /* No asserts just to exercise code path. */
    /* TODO: ADSER-1866 - Implement mocked alert storage functions */
    AiaAlertManager_UpdateUXState( NULL, AIA_UX_IDLE );
    AiaAlertManager_UpdateUXState( g_testAlertManager, AIA_UX_IDLE );
}
