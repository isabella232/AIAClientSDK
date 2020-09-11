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
 * @file aia_dispatcher_tests.c
 * @brief Tests for AiaDispatcher_t.
 */

/* The config header is always included first. */
#include <aia_capabilities_config.h>
#include <aia_config.h>

/* Aia headers */
#include <aiacore/aia_directive.h>
#include <aiacore/aia_mbedtls_threading.h>
#include <aiacore/aia_random_mbedtls.h>
#include <aiacore/aia_topic.h>
#include <aiadispatcher/aia_dispatcher.h>
#include <aiaspeakermanager/aia_speaker_manager.h>

/* Test framework includes. */
#include <unity_fixture.h>

#include <aiamockregulator/aia_mock_regulator.h>
#include <aiatestutilities/aia_test_utilities.h>

/** Sample Seed. */
static const char* TEST_SALT = "TestSalt";
static const size_t TEST_SALT_LENGTH = sizeof( TEST_SALT ) - 1;

/*-----------------------------------------------------------*/

static AiaDispatcher_t* testDispatcher;
static AiaSpeakerManager_t* testSpeakerManager;
static AiaRegulator_t* testRegulator;
static AiaRegulator_t* testCapabilitiesRegulator;
static AiaCapabilitiesSender_t* testCapabilitiesSender;
static AiaSecretManager_t* testSecretManager;

static void AiaOnCapabilitiesStateChanged( AiaCapabilitiesSenderState_t state,
                                           const char* description,
                                           size_t descriptionLen,
                                           void* userData )
{
    (void)state;
    (void)description;
    (void)descriptionLen;
    (void)userData;
}

#define TEST_DEVICE_TOPIC_ROOT "test/topic/root/"
#define TEST_TOPIC_STRING( TOPIC ) \
    TEST_DEVICE_TOPIC_ROOT AIA_TOPIC_##TOPIC##_STRING

/* clang-format off */
static const char* TEST_PAYLOAD_SINGLE =
    "{\"header\": {\"name\": \"OpenSpeaker\", \"messageId\": "
    "\"testMessageId\"}, \"payload\": {\"offset\": 100}}";

static const char* TEST_PAYLOAD_NO_NAME =
    "{\"header\": {\"messageId\": "
    "\"testMessageId\"}, \"payload\": {\"offset\": 100}}";

static const char* TEST_PAYLOAD_INVALID_NAME =
    "{\"header\": {\"name\":, \"messageId\": "
    "\"testMessageId\"}, \"payload\": {\"offset\": 100}}";

static const char* TEST_PAYLOAD_CONNECTION_VALID_ACK =
    "{\"header\": {\"name\": \"Acknowledge\", {\"messageId\": "
    "\"testMessageId\"}, \"payload\": {\"code\": "AIA_CONNECTION_ACK_CONNECTION_ESTABLISHED"}}";

static const char* TEST_PAYLOAD_CONNECTION_VALID_DISCONNECT =
    "{\"header\": {\"name\": \"Disconnect\", {\"messageId\": "
    "\"testMessageId\"}, \"payload\": {\"code\": "AIA_CONNECTION_DISCONNECT_MESSAGE_TAMPERED"}}";
/* clang-format on */

size_t AiaGetDeviceTopicRootString( char* deviceTopicRootBuffer,
                                    size_t deviceTopicRootBufferSize )
{
    (void)deviceTopicRootBufferSize;
    if( !deviceTopicRootBuffer )
    {
        return sizeof( TEST_DEVICE_TOPIC_ROOT ) - 1;
    }
    if( deviceTopicRootBufferSize < sizeof( TEST_DEVICE_TOPIC_ROOT ) - 1 )
    {
        return 0;
    }
    memcpy( deviceTopicRootBuffer, TEST_DEVICE_TOPIC_ROOT,
            sizeof( TEST_DEVICE_TOPIC_ROOT ) - 1 );
    return sizeof( TEST_DEVICE_TOPIC_ROOT ) - 1;
}

/**
 * Generates a callback parameter for @c messageReceivedCallback function with
 * the given params.
 *
 * @param topic Topic on which to parse the message.
 * @param payload Payload received on @c topic.
 *
 * @return The generated @c AiaMqttCallbackParam_t or @c NULL on failures.
 */
AiaMqttCallbackParam_t* generateCallbackParam( const char* topic,
                                               const char* payload )
{
    AiaMqttCallbackParam_t* callbackParam =
        AiaCalloc( 1, sizeof( AiaMqttCallbackParam_t ) );
    if( !callbackParam )
    {
        return NULL;
    }

    callbackParam->u.message.info.topicNameLength = strlen( topic );
    callbackParam->u.message.info.pTopicName = topic;
    callbackParam->u.message.topicFilterLength =
        callbackParam->u.message.info.topicNameLength;
    callbackParam->u.message.pTopicFilter =
        callbackParam->u.message.info.pTopicName;
    callbackParam->u.message.info.retain = false;
    callbackParam->u.message.info.qos = AIA_MQTT_QOS0;
    callbackParam->u.message.info.payloadLength =
        ( ( uint16_t )( strlen( payload ) ) );
    callbackParam->u.message.info.pPayload = payload;

    return callbackParam;
}

/*-----------------------------------------------------------*/

/**
 * @brief Test group for AiaDispatcher_t tests.
 */
TEST_GROUP( AiaDispatcherTests );

/*-----------------------------------------------------------*/

/**
 * @brief Test group runner for AiaDispatcher_t tests.
 */
TEST_GROUP_RUNNER( AiaDispatcherTests )
{
    RUN_TEST_CASE( AiaDispatcherTests, Creation );
#ifdef AIA_ENABLE_SPEAKER
    RUN_TEST_CASE( AiaDispatcherTests, AddHandlerNullDispatcher );
    RUN_TEST_CASE( AiaDispatcherTests, AddHandlerNullHandler );
    RUN_TEST_CASE( AiaDispatcherTests, AddHandlerNullUserData );
#endif
    RUN_TEST_CASE( AiaDispatcherTests, AddHandlerInvalidDirective );
#ifdef AIA_ENABLE_SPEAKER
    RUN_TEST_CASE( AiaDispatcherTests, AddHandlerHappyCase );
#endif
    RUN_TEST_CASE( AiaDispatcherTests, CallbackNullDispatcher );
    RUN_TEST_CASE( AiaDispatcherTests, CallbackOnInvalidTopic );
    RUN_TEST_CASE( AiaDispatcherTests, CallbackWithEmptyPayload );
    RUN_TEST_CASE( AiaDispatcherTests, CallbackOnDirectiveTopic );
#ifdef AIA_ENABLE_SPEAKER
    RUN_TEST_CASE( AiaDispatcherTests, CallbackOnSpeakerTopic );
#endif
    RUN_TEST_CASE( AiaDispatcherTests, CallbackOnCapabilitiesAcknowledgeTopic );
    RUN_TEST_CASE( AiaDispatcherTests,
                   CallbackOnConnectionFromServiceTopicInvalidPayload );
    RUN_TEST_CASE( AiaDispatcherTests,
                   CallbackOnConnectionFromServiceTopicValidPayload );
    RUN_TEST_CASE( AiaDispatcherTests, AddConnectionManagerDispatcherNull );
    RUN_TEST_CASE( AiaDispatcherTests, AddConnectionManagerNull );
#ifdef AIA_ENABLE_SPEAKER
    RUN_TEST_CASE( AiaDispatcherTests, AddSpeakerManagerDispatcherNull );
    RUN_TEST_CASE( AiaDispatcherTests, AddSpeakerManagerNull );
#endif
    RUN_TEST_CASE( AiaDispatcherTests, DestroyNull );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test setup for AiaDispatcher_t tests.
 */
TEST_SETUP( AiaDispatcherTests )
{
    AiaMbedtlsThreading_Init();
    AiaRandomMbedtls_Init();
    TEST_ASSERT_TRUE( AiaRandomMbedtls_Seed( TEST_SALT, TEST_SALT_LENGTH ) );

    /** Create testCapabilitiesSender */
    testCapabilitiesRegulator = (AiaRegulator_t*)AiaMockRegulator_Create();
    TEST_ASSERT_NOT_NULL( testCapabilitiesRegulator );

    testCapabilitiesSender = AiaCapabilitiesSender_Create(
        testCapabilitiesRegulator, AiaOnCapabilitiesStateChanged, NULL );
    TEST_ASSERT_NOT_NULL( testCapabilitiesSender );

    /** Create testRegulator */
    testRegulator = (AiaRegulator_t*)AiaMockRegulator_Create();
    TEST_ASSERT_NOT_NULL( testRegulator );

    /** Create testSecretManager */
    testSecretManager = AiaSecretManager_Create( NULL, NULL, NULL, NULL );
    TEST_ASSERT_NOT_NULL( testSecretManager );

    /** Create the testDispatcher */
    testDispatcher = AiaDispatcher_Create( AiaTaskPool( GetSystemTaskPool )(),
                                           testCapabilitiesSender,
                                           testRegulator, testSecretManager );
    TEST_ASSERT_NOT_NULL( testDispatcher );
    TEST_ASSERT_NOT_NULL( testDispatcher->directiveSequencer );
    TEST_ASSERT_NOT_NULL( testDispatcher->capabilitiesAcknowledgeSequencer );
#ifdef AIA_ENABLE_SPEAKER
    TEST_ASSERT_NOT_NULL( testDispatcher->speakerSequencer );
#endif

    testSpeakerManager =
        AiaSpeakerManager_Create( 0, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL,
                                  NULL, NULL, NULL, NULL, NULL, NULL );
    TEST_ASSERT_NOT_NULL( testSpeakerManager );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test tear down for AiaDispatcher_t tests.
 */
TEST_TEAR_DOWN( AiaDispatcherTests )
{
    AiaSpeakerManager_Destroy( testSpeakerManager );
    AiaDispatcher_Destroy( testDispatcher );
    AiaCapabilitiesSender_Destroy( testCapabilitiesSender );
    AiaMockRegulator_Destroy( (AiaMockRegulator_t*)testCapabilitiesRegulator,
                              AiaTestUtilities_DestroyJsonChunk, NULL );
    AiaMockRegulator_Destroy( (AiaMockRegulator_t*)testRegulator,
                              AiaTestUtilities_DestroyJsonChunk, NULL );
    AiaSecretManager_Destroy( testSecretManager );
    AiaMbedtlsThreading_Cleanup();
    AiaRandomMbedtls_Cleanup();
}

/*-----------------------------------------------------------*/

TEST( AiaDispatcherTests, Creation )
{
    AiaDispatcher_t* invalidDispatcher = NULL;

    invalidDispatcher = AiaDispatcher_Create(
        NULL, testCapabilitiesSender, testRegulator, testSecretManager );
    TEST_ASSERT_NULL( invalidDispatcher );

    invalidDispatcher =
        AiaDispatcher_Create( AiaTaskPool( GetSystemTaskPool )(), NULL,
                              testRegulator, testSecretManager );
    TEST_ASSERT_NULL( invalidDispatcher );

    invalidDispatcher =
        AiaDispatcher_Create( AiaTaskPool( GetSystemTaskPool )(),
                              testCapabilitiesSender, NULL, testSecretManager );
    TEST_ASSERT_NULL( invalidDispatcher );

    invalidDispatcher =
        AiaDispatcher_Create( AiaTaskPool( GetSystemTaskPool )(),
                              testCapabilitiesSender, testRegulator, NULL );
    TEST_ASSERT_NULL( invalidDispatcher );
}

#ifdef AIA_ENABLE_SPEAKER
TEST( AiaDispatcherTests, AddHandlerNullDispatcher )
{
    TEST_ASSERT_FALSE( AiaDispatcher_AddHandler(
        NULL, AiaSpeakerManager_OnOpenSpeakerDirectiveReceived,
        AIA_DIRECTIVE_OPEN_SPEAKER, testSpeakerManager ) );
}

TEST( AiaDispatcherTests, AddHandlerNullHandler )
{
    TEST_ASSERT_FALSE( AiaDispatcher_AddHandler( testDispatcher, NULL,
                                                 AIA_DIRECTIVE_OPEN_SPEAKER,
                                                 testSpeakerManager ) );
}

TEST( AiaDispatcherTests, AddHandlerNullUserData )
{
    TEST_ASSERT_FALSE( AiaDispatcher_AddHandler(
        testDispatcher, AiaSpeakerManager_OnOpenSpeakerDirectiveReceived,
        AIA_DIRECTIVE_OPEN_SPEAKER, NULL ) );
}
#endif

TEST( AiaDispatcherTests, AddHandlerInvalidDirective )
{
    TEST_ASSERT_FALSE( AiaDispatcher_AddHandler(
        testDispatcher, AiaSpeakerManager_OnOpenSpeakerDirectiveReceived, -1,
        testSpeakerManager ) );
}

#ifdef AIA_ENABLE_SPEAKER
TEST( AiaDispatcherTests, AddHandlerHappyCase )
{
    TEST_ASSERT_TRUE( AiaDispatcher_AddHandler(
        testDispatcher, AiaSpeakerManager_OnOpenSpeakerDirectiveReceived,
        AIA_DIRECTIVE_OPEN_SPEAKER, testSpeakerManager ) );
}
#endif

TEST( AiaDispatcherTests, CallbackNullDispatcher )
{
    AiaMqttCallbackParam_t* callbackParam = generateCallbackParam(
        TEST_TOPIC_STRING( DIRECTIVE ), TEST_PAYLOAD_SINGLE );
    messageReceivedCallback( NULL, callbackParam );
    AiaFree( (void*)callbackParam );
}

TEST( AiaDispatcherTests, CallbackOnInvalidTopic )
{
    AiaMqttCallbackParam_t* callbackParam = generateCallbackParam(
        TEST_TOPIC_STRING( CONNECTION_FROM_CLIENT ), TEST_PAYLOAD_SINGLE );
    messageReceivedCallback( testDispatcher, callbackParam );
    AiaFree( (void*)callbackParam );
}

TEST( AiaDispatcherTests, CallbackWithEmptyPayload )
{
    AiaMqttCallbackParam_t* callbackParam = generateCallbackParam(
        TEST_TOPIC_STRING( CONNECTION_FROM_CLIENT ), "" );
    messageReceivedCallback( testDispatcher, callbackParam );
    AiaFree( (void*)callbackParam );
}

TEST( AiaDispatcherTests, CallbackOnDirectiveTopic )
{
    AiaMqttCallbackParam_t* callbackParam = generateCallbackParam(
        TEST_TOPIC_STRING( DIRECTIVE ), TEST_PAYLOAD_SINGLE );
    messageReceivedCallback( testDispatcher, callbackParam );
    AiaFree( (void*)callbackParam );
}

#ifdef AIA_ENABLE_SPEAKER
TEST( AiaDispatcherTests, CallbackOnSpeakerTopic )
{
    AiaMqttCallbackParam_t* callbackParam = generateCallbackParam(
        TEST_TOPIC_STRING( SPEAKER ), TEST_PAYLOAD_SINGLE );
    messageReceivedCallback( testDispatcher, callbackParam );
    AiaFree( (void*)callbackParam );
}
#endif

TEST( AiaDispatcherTests, CallbackOnCapabilitiesAcknowledgeTopic )
{
    AiaMqttCallbackParam_t* callbackParam = generateCallbackParam(
        TEST_TOPIC_STRING( CAPABILITIES_ACKNOWLEDGE ), TEST_PAYLOAD_SINGLE );
    messageReceivedCallback( testDispatcher, callbackParam );
    AiaFree( (void*)callbackParam );
}

TEST( AiaDispatcherTests, CallbackOnConnectionFromServiceTopicInvalidPayload )
{
    /** Payload does not have the name field */
    AiaMqttCallbackParam_t* callbackParam = generateCallbackParam(
        TEST_TOPIC_STRING( CONNECTION_FROM_SERVICE ), TEST_PAYLOAD_NO_NAME );
    messageReceivedCallback( testDispatcher, callbackParam );
    AiaFree( (void*)callbackParam );

    /** Name field containts invalid data */
    callbackParam =
        generateCallbackParam( TEST_TOPIC_STRING( CONNECTION_FROM_SERVICE ),
                               TEST_PAYLOAD_INVALID_NAME );
    messageReceivedCallback( testDispatcher, callbackParam );
    AiaFree( (void*)callbackParam );

    /** Payload does not belong to connection/fromservice topic */
    callbackParam = generateCallbackParam(
        TEST_TOPIC_STRING( CONNECTION_FROM_SERVICE ), TEST_PAYLOAD_SINGLE );
    messageReceivedCallback( testDispatcher, callbackParam );
    AiaFree( (void*)callbackParam );
}

TEST( AiaDispatcherTests, CallbackOnConnectionFromServiceTopicValidPayload )
{
    /** Connection successfully established */
    AiaMqttCallbackParam_t* callbackParam =
        generateCallbackParam( TEST_TOPIC_STRING( CONNECTION_FROM_SERVICE ),
                               TEST_PAYLOAD_CONNECTION_VALID_ACK );
    messageReceivedCallback( testDispatcher, callbackParam );
    AiaFree( (void*)callbackParam );

    /** Disconnect with message tampered */
    callbackParam =
        generateCallbackParam( TEST_TOPIC_STRING( CONNECTION_FROM_SERVICE ),
                               TEST_PAYLOAD_CONNECTION_VALID_DISCONNECT );
    messageReceivedCallback( testDispatcher, callbackParam );
    AiaFree( (void*)callbackParam );
}

TEST( AiaDispatcherTests, AddConnectionManagerDispatcherNull )
{
    /* No asserts just to exercise code path. */
    AiaDispatcher_AddConnectionManager( NULL, NULL );
}

TEST( AiaDispatcherTests, AddConnectionManagerNull )
{
    /* No asserts just to exercise code path. */
    AiaDispatcher_AddConnectionManager( testDispatcher, NULL );
}

#ifdef AIA_ENABLE_SPEAKER
TEST( AiaDispatcherTests, AddSpeakerManagerDispatcherNull )
{
    /* No asserts just to exercise code path. */
    AiaDispatcher_AddSpeakerManager( NULL, NULL );
}

TEST( AiaDispatcherTests, AddSpeakerManagerNull )
{
    /* No asserts just to exercise code path. */
    AiaDispatcher_AddSpeakerManager( testDispatcher, NULL );
}
#endif

TEST( AiaDispatcherTests, DestroyNull )
{
    /* No asserts just to exercise code path. */
    AiaDispatcher_Destroy( NULL );
}
