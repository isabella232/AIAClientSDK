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
 * @file aia_capabilities_sender_tests.c
 * @brief Tests for AiaCapabilitiesSender_t.
 */

/* The config header is always included first. */
#include <aia_config.h>

/* Aia headers */
#include <aia_capabilities_config.h>
#include <aiacore/aia_capabilities_constants.h>
#include <aiacore/aia_events.h>
#include <aiacore/aia_json_constants.h>
#include <aiacore/aia_json_message.h>
#include <aiacore/aia_json_utils.h>
#include <aiacore/aia_mbedtls_threading.h>
#include <aiacore/aia_random_mbedtls.h>
#include <aiacore/capabilities_sender/aia_capabilities_sender.h>
#include <aiacore/private/aia_capabilities_sender.h>
#include <aiatestutilities/aia_test_utilities.h>

/* Test framework includes. */
#include <unity_fixture.h>

#include <aiamockregulator/aia_mock_regulator.h>

/*-----------------------------------------------------------*/

/**
 * @brief Test group for AiaCapabilitiesSender_t tests.
 */
TEST_GROUP( AiaCapabilitiesTests );

/*-----------------------------------------------------------*/

/**
 * @brief Test group runner for AiaCapabilitiesSender_t tests.
 */
TEST_GROUP_RUNNER( AiaCapabilitiesTests )
{
    RUN_TEST_CASE( AiaCapabilitiesTests, Creation );
    RUN_TEST_CASE( AiaCapabilitiesTests, PublishCapabilities );
    RUN_TEST_CASE( AiaCapabilitiesTests, CapabilitiesAccepted );
    RUN_TEST_CASE( AiaCapabilitiesTests, CapabilitiesRejectedWithDescription );
    RUN_TEST_CASE( AiaCapabilitiesTests,
                   CapabilitiesRejectedWithoutDescription );
    RUN_TEST_CASE( AiaCapabilitiesTests,
                   DoubleCapabilitiesPublishWithoutAckFails );
    RUN_TEST_CASE( AiaCapabilitiesTests,
                   DoubleCapabilitiesPublishWithAckSucceeds );
}

/*-----------------------------------------------------------*/

/** Sample Seed. */
static const char* TEST_SALT = "TestSalt";
static const size_t TEST_SALT_LENGTH = sizeof( TEST_SALT ) - 1;

typedef struct AiaTestCapabilitiesStateObserver
{
    AiaCapabilitiesSenderState_t currentState;
    const char* lastDescription;
    size_t lastDescriptionLen;
} AiaTestCapabilitiesStateObserver_t;

static void AiaOnCapabilitiesStateChanged( AiaCapabilitiesSenderState_t state,
                                           const char* description,
                                           size_t descriptionLen,
                                           void* userData )
{
    TEST_ASSERT_NOT_NULL( userData );
    AiaTestCapabilitiesStateObserver_t* observer =
        (AiaTestCapabilitiesStateObserver_t*)userData;
    observer->currentState = state;
    observer->lastDescription = description;
    observer->lastDescriptionLen = descriptionLen;
}

/*-----------------------------------------------------------*/

static AiaMockRegulator_t* testCapabilitiesRegulator;

static AiaTestCapabilitiesStateObserver_t* testObserver;

static AiaCapabilitiesSender_t* capabilitiesSender;

#define AIA_CAPABILITIES_TEST_DESCRIPTION "TestDescription"

static const char* TEST_ACCEPTED_PAYLOAD =
    /* clang-format off */
"{"
    "\""AIA_CAPABILITIES_PUBLISH_MESSAGE_ID_KEY"\":\"0\","
    "\""AIA_CAPABILITIES_ACKNOWLEDGE_CODE_KEY"\":\""AIA_CAPABILITIES_ACCEPTED_CODE"\","
    "\""AIA_CAPABILITIES_ACKNOWLEDGE_DESCRIPTION_KEY"\":\""AIA_CAPABILITIES_TEST_DESCRIPTION"\""
"}";
/* clang-format on */

static const char* TEST_REJECTED_PAYLOAD =
    /* clang-format off */
"{"
    "\""AIA_CAPABILITIES_PUBLISH_MESSAGE_ID_KEY"\":\"0\","
    "\""AIA_CAPABILITIES_ACKNOWLEDGE_CODE_KEY"\":\""AIA_CAPABILITIES_REJECTED_CODE"\","
    "\""AIA_CAPABILITIES_ACKNOWLEDGE_DESCRIPTION_KEY"\":\""AIA_CAPABILITIES_TEST_DESCRIPTION"\""
"}";
/* clang-format on */

static const char* TEST_REJECTED_PAYLOAD_WITHOUT_DESCRIPTION =
    /* clang-format off */
"{"
    "\""AIA_CAPABILITIES_PUBLISH_MESSAGE_ID_KEY"\":\"0\","
    "\""AIA_CAPABILITIES_ACKNOWLEDGE_CODE_KEY"\":\""AIA_CAPABILITIES_REJECTED_CODE"\""
"}";
/* clang-format on */

/* TODO: ADSER-1631 Figure out a way to inject different capability config files
 * for test cases. For example, test variations of the config file with
 * different versions of desired capabilities. This was done manually by
 * deleting capabilities from the config, recompiling, and retesting. This won't
 * scale if more capabilities are added.*/
static void validatePublishedCapabilitiesMessage()
{
    TEST_ASSERT_TRUE(
        AiaSemaphore( TryWait )( &testCapabilitiesRegulator->writeSemaphore ) );
    AiaListDouble( Link_t )* link = NULL;
    link = AiaListDouble( PeekHead )(
        &testCapabilitiesRegulator->writtenMessages );
    AiaListDouble( RemoveHead )( &testCapabilitiesRegulator->writtenMessages );
    TEST_ASSERT_NOT_NULL( link );
    AiaJsonMessage_t* capabilitiesPublishMessage = AiaJsonMessage_FromMessage(
        ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
    TEST_ASSERT_EQUAL(
        strcmp( AiaJsonMessage_GetName( capabilitiesPublishMessage ),
                AIA_CAPABILITIES_PUBLISH ),
        0 );
    const char* payload =
        AiaJsonMessage_GetJsonPayload( capabilitiesPublishMessage );
    TEST_ASSERT_NOT_NULL( payload );

    const char* capabilities = NULL;
    size_t capabilitiesLen = 0;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_CAPABILITIES_KEY,
        strlen( AIA_CAPABILITIES_KEY ), &capabilities, &capabilitiesLen ) );
    TEST_ASSERT_NOT_NULL( capabilities );

#ifdef AIA_ENABLE_SPEAKER

    const char* audioBuffer = NULL;
    size_t audioBufferLen = 0;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_CAPABILITIES_SPEAKER_AUDIO_BUFFER,
        strlen( AIA_CAPABILITIES_SPEAKER_AUDIO_BUFFER ), &audioBuffer,
        &audioBufferLen ) );
    TEST_ASSERT_NOT_NULL( audioBuffer );

    const char* audioBufferSize = NULL;
    size_t audioBufferSizeLen = 0;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        audioBuffer, audioBufferLen, AIA_CAPABILITIES_SPEAKER_AUDIO_BUFFER_SIZE,
        strlen( AIA_CAPABILITIES_SPEAKER_AUDIO_BUFFER_SIZE ), &audioBufferSize,
        &audioBufferSizeLen ) );
    TEST_ASSERT_NOT_NULL( audioBufferSize );
    AiaJsonLongType audioBufferSizeLong = 0;
    TEST_ASSERT_TRUE( AiaExtractLongFromJsonValue(
        audioBufferSize, audioBufferSizeLen, &audioBufferSizeLong ) );
    TEST_ASSERT_EQUAL( audioBufferSizeLong, AIA_AUDIO_BUFFER_SIZE );

    const char* audioBufferReporting = NULL;
    size_t audioBufferReportingLen = 0;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        audioBuffer, audioBufferLen, AIA_CAPABILITIES_SPEAKER_AUDIO_REPORTING,
        strlen( AIA_CAPABILITIES_SPEAKER_AUDIO_REPORTING ),
        &audioBufferReporting, &audioBufferReportingLen ) );
    TEST_ASSERT_NOT_NULL( audioBufferReporting );

    const char* overrunThreshold = NULL;
    size_t overrunThresholdLen = 0;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        audioBuffer, audioBufferLen,
        AIA_CAPABILITIES_SPEAKER_AUDIO_OVERRUN_THRESHOLD,
        strlen( AIA_CAPABILITIES_SPEAKER_AUDIO_OVERRUN_THRESHOLD ),
        &overrunThreshold, &overrunThresholdLen ) );
    TEST_ASSERT_NOT_NULL( overrunThreshold );
    AiaJsonLongType overrunThresholdLong = 0;
    TEST_ASSERT_TRUE( AiaExtractLongFromJsonValue(
        overrunThreshold, overrunThresholdLen, &overrunThresholdLong ) );
    TEST_ASSERT_EQUAL( overrunThresholdLong,
                       AIA_AUDIO_BUFFER_OVERRUN_WARN_THRESHOLD );

    const char* underrunThreshold = NULL;
    size_t underrunThresholdLen = 0;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        audioBuffer, audioBufferLen,
        AIA_CAPABILITIES_SPEAKER_AUDIO_UNDERRUN_THRESHOLD,
        strlen( AIA_CAPABILITIES_SPEAKER_AUDIO_UNDERRUN_THRESHOLD ),
        &underrunThreshold, &underrunThresholdLen ) );
    TEST_ASSERT_NOT_NULL( underrunThreshold );
    AiaJsonLongType underrunThresholdLong = 0;
    TEST_ASSERT_TRUE( AiaExtractLongFromJsonValue(
        underrunThreshold, underrunThresholdLen, &underrunThresholdLong ) );
    TEST_ASSERT_EQUAL( underrunThresholdLong,
                       AIA_AUDIO_BUFFER_UNDERRUN_WARN_THRESHOLD );

    const char* audioDecoder = NULL;
    size_t audioDecoderLen = 0;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_CAPABILITIES_SPEAKER_AUDIO_DECODER,
        strlen( AIA_CAPABILITIES_SPEAKER_AUDIO_DECODER ), &audioDecoder,
        &audioDecoderLen ) );
    TEST_ASSERT_NOT_NULL( audioDecoder );

    const char* speakerFormat = NULL;
    size_t speakerFormatLen = 0;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        audioDecoder, audioDecoderLen, AIA_CAPABILITIES_SPEAKER_AUDIO_FORMAT,
        strlen( AIA_CAPABILITIES_SPEAKER_AUDIO_FORMAT ), &speakerFormat,
        &speakerFormatLen ) );
    TEST_ASSERT_NOT_NULL( speakerFormat );
    TEST_ASSERT_TRUE(
        AiaJsonUtils_UnquoteString( &speakerFormat, &speakerFormatLen ) );
    TEST_ASSERT_EQUAL_STRING_LEN( AIA_SPEAKER_AUDIO_DECODER_FORMAT,
                                  speakerFormat, speakerFormatLen );

    const char* bitrate = NULL;
    size_t bitrateLen = 0;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        audioDecoder, audioDecoderLen, AIA_CAPABILITIES_SPEAKER_AUDIO_BITRATE,
        strlen( AIA_CAPABILITIES_SPEAKER_AUDIO_BITRATE ), &bitrate,
        &bitrateLen ) );
    TEST_ASSERT_NOT_NULL( bitrate );

    const char* bitrateType = NULL;
    size_t bitrateTypeLen = 0;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        bitrate, bitrateLen, AIA_CAPABILITIES_SPEAKER_AUDIO_TYPE,
        strlen( AIA_CAPABILITIES_SPEAKER_AUDIO_TYPE ), &bitrateType,
        &bitrateTypeLen ) );
    TEST_ASSERT_NOT_NULL( bitrateType );
    TEST_ASSERT_TRUE(
        AiaJsonUtils_UnquoteString( &bitrateType, &bitrateTypeLen ) );
    TEST_ASSERT_EQUAL_STRING_LEN( AIA_SPEAKER_AUDIO_DECODER_BITRATE_TYPE,
                                  bitrateType, bitrateTypeLen );

    const char* bitsPerSecond = NULL;
    size_t bitsPerSecondLen = 0;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        bitrate, bitrateLen, AIA_CAPABILITIES_SPEAKER_AUDIO_BITS_PER_SECOND,
        strlen( AIA_CAPABILITIES_SPEAKER_AUDIO_BITS_PER_SECOND ),
        &bitsPerSecond, &bitsPerSecondLen ) );
    TEST_ASSERT_NOT_NULL( bitsPerSecond );
    AiaJsonLongType bitsPerSecondLong = 0;
    TEST_ASSERT_TRUE( AiaExtractLongFromJsonValue(
        bitsPerSecond, bitsPerSecondLen, &bitsPerSecondLong ) );
    TEST_ASSERT_EQUAL( bitsPerSecondLong,
                       AIA_SPEAKER_AUDIO_DECODER_BITS_PER_SECOND );

    const char* channels = NULL;
    size_t channelsLen = 0;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        audioDecoder, audioDecoderLen, AIA_CAPABILITIES_SPEAKER_NUM_CHANNELS,
        strlen( AIA_CAPABILITIES_SPEAKER_NUM_CHANNELS ), &channels,
        &channelsLen ) );
    TEST_ASSERT_NOT_NULL( channels );
    AiaJsonLongType channelsLong = 0;
    TEST_ASSERT_TRUE(
        AiaExtractLongFromJsonValue( channels, channelsLen, &channelsLong ) );
    TEST_ASSERT_EQUAL( channelsLong, AIA_SPEAKER_AUDIO_DECODER_NUM_CHANNELS );

#endif

#ifdef AIA_ENABLE_MICROPHONE

    const char* audioEncoder = NULL;
    size_t audioEncoderLen = 0;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_CAPABILITIES_MICROPHONE_AUDIO_ENCODER,
        strlen( AIA_CAPABILITIES_MICROPHONE_AUDIO_ENCODER ), &audioEncoder,
        &audioEncoderLen ) );
    TEST_ASSERT_NOT_NULL( audioEncoder );

    const char* microphoneFormat = NULL;
    size_t microphoneFormatLen = 0;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        audioEncoder, audioEncoderLen, AIA_CAPABILITIES_MICROPHONE_AUDIO_FORMAT,
        strlen( AIA_CAPABILITIES_MICROPHONE_AUDIO_FORMAT ), &microphoneFormat,
        &microphoneFormatLen ) );
    TEST_ASSERT_NOT_NULL( microphoneFormat );
    TEST_ASSERT_TRUE(
        AiaJsonUtils_UnquoteString( &microphoneFormat, &microphoneFormatLen ) );
    TEST_ASSERT_EQUAL_STRING_LEN( AIA_MICROPHONE_AUDIO_ENCODER_FORMAT,
                                  microphoneFormat, microphoneFormatLen );

#endif

#ifdef AIA_ENABLE_ALERTS

    const char* maxAlerts = NULL;
    size_t maxAlertsLen = 0;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_CAPABILITIES_ALERTS_MAX_ALERTS,
        strlen( AIA_CAPABILITIES_ALERTS_MAX_ALERTS ), &maxAlerts,
        &maxAlertsLen ) );
    TEST_ASSERT_NOT_NULL( maxAlerts );
    AiaJsonLongType maxAlertsLong = 0;
    TEST_ASSERT_TRUE( AiaExtractLongFromJsonValue( maxAlerts, maxAlertsLen,
                                                   &maxAlertsLong ) );
    TEST_ASSERT_EQUAL( maxAlertsLong, AIA_ALERTS_MAX_ALERT_COUNT );

#endif

    const char* mqtt = NULL;
    size_t mqttLen = 0;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_CAPABILITIES_SYSTEM_MQTT,
        strlen( AIA_CAPABILITIES_SYSTEM_MQTT ), &mqtt, &mqttLen ) );
    TEST_ASSERT_NOT_NULL( mqtt );

    const char* message = NULL;
    size_t messageLen = 0;
    TEST_ASSERT_TRUE(
        AiaFindJsonValue( mqtt, mqttLen, AIA_CAPABILITIES_SYSTEM_MQTT_MESSAGE,
                          strlen( AIA_CAPABILITIES_SYSTEM_MQTT_MESSAGE ),
                          &message, &messageLen ) );
    TEST_ASSERT_NOT_NULL( message );

    const char* maxMessageSize = NULL;
    size_t maxMessageSizeLen = 0;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        message, messageLen, AIA_CAPABILITIES_SYSTEM_MAX_MESSAGE_SIZE,
        strlen( AIA_CAPABILITIES_SYSTEM_MAX_MESSAGE_SIZE ), &maxMessageSize,
        &maxMessageSizeLen ) );
    TEST_ASSERT_NOT_NULL( maxMessageSize );
    AiaJsonLongType maxMessageSizeLong = 0;
    TEST_ASSERT_TRUE( AiaExtractLongFromJsonValue(
        maxMessageSize, maxMessageSizeLen, &maxMessageSizeLong ) );
    TEST_ASSERT_EQUAL( maxMessageSizeLong, AIA_SYSTEM_MQTT_MESSAGE_MAX_SIZE );

    const char* firmware = NULL;
    size_t firmwareLen = 0;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_CAPABILITIES_SYSTEM_FIRMWARE_VERSION,
        strlen( AIA_CAPABILITIES_SYSTEM_FIRMWARE_VERSION ), &firmware,
        &firmwareLen ) );
    TEST_ASSERT_NOT_NULL( firmware );
    TEST_ASSERT_TRUE( AiaJsonUtils_UnquoteString( &firmware, &firmwareLen ) );
    TEST_ASSERT_EQUAL_STRING_LEN( AIA_SYSTEM_FIRMWARE_VERSION, firmware,
                                  firmwareLen );

    const char* locale = NULL;
    size_t localeLen = 0;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_CAPABILITIES_SYSTEM_LOCALE,
        strlen( AIA_CAPABILITIES_SYSTEM_LOCALE ), &locale, &localeLen ) );
    TEST_ASSERT_NOT_NULL( locale );
    TEST_ASSERT_TRUE( AiaJsonUtils_UnquoteString( &locale, &localeLen ) );
    TEST_ASSERT_EQUAL_STRING_LEN( AIA_SYSTEM_LOCALE, locale, localeLen );
}

/**
 * @brief Test setup for AiaCapabilitiesSender_t tests.
 */
TEST_SETUP( AiaCapabilitiesTests )
{
    AiaMbedtlsThreading_Init();
    AiaRandomMbedtls_Init();
    TEST_ASSERT_TRUE( AiaRandomMbedtls_Seed( TEST_SALT, TEST_SALT_LENGTH ) );

    testObserver = AiaCalloc( 1, sizeof( AiaTestCapabilitiesStateObserver_t ) );
    TEST_ASSERT_NOT_NULL( testObserver );

    testCapabilitiesRegulator = AiaMockRegulator_Create();
    TEST_ASSERT_NOT_NULL( testCapabilitiesRegulator );

    capabilitiesSender = AiaCapabilitiesSender_Create(
        (AiaRegulator_t*)testCapabilitiesRegulator,
        AiaOnCapabilitiesStateChanged, testObserver );
    TEST_ASSERT_NOT_NULL( capabilitiesSender );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test tear down for AiaCapabilitiesSender_t tests.
 */
TEST_TEAR_DOWN( AiaCapabilitiesTests )
{
    AiaCapabilitiesSender_Destroy( capabilitiesSender );
    AiaMockRegulator_Destroy( testCapabilitiesRegulator,
                              AiaTestUtilities_DestroyJsonChunk, NULL );
    AiaFree( testObserver );

    AiaMbedtlsThreading_Cleanup();
    AiaRandomMbedtls_Cleanup();
}

/*-----------------------------------------------------------*/

TEST( AiaCapabilitiesTests, Creation )
{
    AiaCapabilitiesSender_t* invalidCapabilitiesSender = NULL;

    invalidCapabilitiesSender = AiaCapabilitiesSender_Create(
        NULL, AiaOnCapabilitiesStateChanged, testObserver );
    TEST_ASSERT_NULL( invalidCapabilitiesSender );

    invalidCapabilitiesSender = AiaCapabilitiesSender_Create(
        (AiaRegulator_t*)testCapabilitiesRegulator, NULL, testObserver );
    TEST_ASSERT_NULL( invalidCapabilitiesSender );
}

TEST( AiaCapabilitiesTests, PublishCapabilities )
{
    TEST_ASSERT_TRUE(
        AiaCapabilitiesSender_PublishCapabilities( capabilitiesSender ) );
    TEST_ASSERT_EQUAL( testObserver->currentState,
                       AIA_CAPABILITIES_STATE_PUBLISHED );
    validatePublishedCapabilitiesMessage();
}

TEST( AiaCapabilitiesTests, CapabilitiesAccepted )
{
    TEST_ASSERT_TRUE(
        AiaCapabilitiesSender_PublishCapabilities( capabilitiesSender ) );
    TEST_ASSERT_EQUAL( testObserver->currentState,
                       AIA_CAPABILITIES_STATE_PUBLISHED );
    validatePublishedCapabilitiesMessage();
    AiaCapabilitiesSender_OnCapabilitiesAcknowledgeMessageReceived(
        capabilitiesSender, (void*)TEST_ACCEPTED_PAYLOAD,
        strlen( TEST_ACCEPTED_PAYLOAD ) );
    TEST_ASSERT_EQUAL( testObserver->currentState,
                       AIA_CAPABILITIES_STATE_ACCEPTED );
    TEST_ASSERT_NULL( testObserver->lastDescription );
}

TEST( AiaCapabilitiesTests, CapabilitiesRejectedWithDescription )
{
    TEST_ASSERT_TRUE(
        AiaCapabilitiesSender_PublishCapabilities( capabilitiesSender ) );
    TEST_ASSERT_EQUAL( testObserver->currentState,
                       AIA_CAPABILITIES_STATE_PUBLISHED );
    validatePublishedCapabilitiesMessage();
    AiaCapabilitiesSender_OnCapabilitiesAcknowledgeMessageReceived(
        capabilitiesSender, (void*)TEST_REJECTED_PAYLOAD,
        strlen( TEST_REJECTED_PAYLOAD ) );
    TEST_ASSERT_EQUAL( testObserver->currentState,
                       AIA_CAPABILITIES_STATE_REJECTED );
    TEST_ASSERT_EQUAL( testObserver->lastDescriptionLen,
                       strlen( AIA_CAPABILITIES_TEST_DESCRIPTION ) );
    TEST_ASSERT_EQUAL_STRING_LEN( testObserver->lastDescription,
                                  AIA_CAPABILITIES_TEST_DESCRIPTION,
                                  testObserver->lastDescriptionLen );
}

TEST( AiaCapabilitiesTests, CapabilitiesRejectedWithoutDescription )
{
    TEST_ASSERT_TRUE(
        AiaCapabilitiesSender_PublishCapabilities( capabilitiesSender ) );
    TEST_ASSERT_EQUAL( testObserver->currentState,
                       AIA_CAPABILITIES_STATE_PUBLISHED );
    validatePublishedCapabilitiesMessage();
    AiaCapabilitiesSender_OnCapabilitiesAcknowledgeMessageReceived(
        capabilitiesSender, (void*)TEST_REJECTED_PAYLOAD_WITHOUT_DESCRIPTION,
        strlen( TEST_REJECTED_PAYLOAD_WITHOUT_DESCRIPTION ) );
    TEST_ASSERT_EQUAL( testObserver->currentState,
                       AIA_CAPABILITIES_STATE_REJECTED );
    TEST_ASSERT_EQUAL( testObserver->lastDescriptionLen, 0 );
    TEST_ASSERT_NULL( testObserver->lastDescription );
}

TEST( AiaCapabilitiesTests, DoubleCapabilitiesPublishWithoutAckFails )
{
    TEST_ASSERT_TRUE(
        AiaCapabilitiesSender_PublishCapabilities( capabilitiesSender ) );
    TEST_ASSERT_EQUAL( testObserver->currentState,
                       AIA_CAPABILITIES_STATE_PUBLISHED );
    validatePublishedCapabilitiesMessage();
    TEST_ASSERT_FALSE(
        AiaCapabilitiesSender_PublishCapabilities( capabilitiesSender ) );
}

TEST( AiaCapabilitiesTests, DoubleCapabilitiesPublishWithAckSucceeds )
{
    TEST_ASSERT_TRUE(
        AiaCapabilitiesSender_PublishCapabilities( capabilitiesSender ) );
    TEST_ASSERT_EQUAL( testObserver->currentState,
                       AIA_CAPABILITIES_STATE_PUBLISHED );
    validatePublishedCapabilitiesMessage();
    AiaCapabilitiesSender_OnCapabilitiesAcknowledgeMessageReceived(
        capabilitiesSender, (void*)TEST_ACCEPTED_PAYLOAD,
        strlen( TEST_ACCEPTED_PAYLOAD ) );
    TEST_ASSERT_EQUAL( testObserver->currentState,
                       AIA_CAPABILITIES_STATE_ACCEPTED );
    TEST_ASSERT_NULL( testObserver->lastDescription );

    TEST_ASSERT_TRUE(
        AiaCapabilitiesSender_PublishCapabilities( capabilitiesSender ) );
    TEST_ASSERT_EQUAL( testObserver->currentState,
                       AIA_CAPABILITIES_STATE_PUBLISHED );
    validatePublishedCapabilitiesMessage();
}
