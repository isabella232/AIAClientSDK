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
 * @file aia_connection_manager_tests.c
 * @brief Tests for AiaConnectionManager_t.
 */

/* The config header is always included first. */
#include <aia_config.h>

/* AiaConnectionManager_t headers */
#include <aiaconnectionmanager/aia_connection_constants.h>
#include <aiaconnectionmanager/aia_connection_manager.h>

/* Other AIA headers */
#include <aiacore/aia_mbedtls_threading.h>
#include <aiacore/aia_random_mbedtls.h>

#include AiaTaskPool( HEADER )

/* Test framework includes. */
#include <unity_fixture.h>

#define CONNECTION_MANAGER_TEST_IOT_CLIENT_ID "testIotClientId"
#define CONNECTION_MANAGER_TEST_AWS_ACCOUNT_ID "123456789012"
#define CONNECTION_MANAGER_TEST_PAYLOAD( CODE ) "{\"code\":\"" CODE "\"}"
#define CONNECTION_MANAGER_TEST_DESCRIPTION_PAYLOAD( CODE, DESCRIPTION ) \
    "{\"code\":\"" CODE "\",\"description\":\"" DESCRIPTION "\"}"

#define CONNECTION_MANAGER_TEST_DESCRIPTION "TestDescription"
#define TEST_DEVICE_TOPIC_ROOT "test/topic/root"

/** Sample Seed. */
static const char* TEST_SALT = "TestSalt";
static const size_t TEST_SALT_LENGTH = sizeof( TEST_SALT ) - 1;

/*-----------------------------------------------------------*/

typedef struct AiaTestConnectionManagerObserver
{
    uint32_t onConnectionSuccessCallCount;
    uint32_t onConnectionRejectedCallCount;
    uint32_t onDisconnectedCallCount;
} AiaTestConnectionManagerObserver_t;

static AiaTestConnectionManagerObserver_t*
AiaTestConnectionManagerObserver_Create()
{
    AiaTestConnectionManagerObserver_t* observer =
        (AiaTestConnectionManagerObserver_t*)AiaCalloc(
            1, sizeof( AiaTestConnectionManagerObserver_t ) );
    if( !observer )
    {
        return NULL;
    }
    observer->onConnectionSuccessCallCount = 0;
    observer->onConnectionRejectedCallCount = 0;
    observer->onDisconnectedCallCount = 0;
    return observer;
}

static void AiaTestConnectionManagerObserver_Destroy(
    AiaTestConnectionManagerObserver_t* observer )
{
    AiaFree( observer );
}

static void onConnectionSuccess( void* userData )
{
    TEST_ASSERT_TRUE( userData );
    AiaTestConnectionManagerObserver_t* connectionManagerObserver =
        (AiaTestConnectionManagerObserver_t*)userData;
    connectionManagerObserver->onConnectionSuccessCallCount++;
}

static void onConnectionRejected(
    void* userData, AiaConnectionOnConnectionRejectionCode_t code )
{
    /* Unused parameters; silence the compiler. */
    (void)code;

    TEST_ASSERT_TRUE( userData );
    AiaTestConnectionManagerObserver_t* connectionManagerObserver =
        (AiaTestConnectionManagerObserver_t*)userData;
    connectionManagerObserver->onConnectionRejectedCallCount++;
}

static void onMqttMessageReceived( void* callbackArg,
                                   AiaMqttCallbackParam_t* callbackParam )
{
    (void)callbackArg;
    (void)callbackParam;
    // no-op
    return;
}

static void OnDisconnected( void* userData,
                            AiaConnectionOnDisconnectCode_t code )
{
    /* Unused parameters; silence the compiler. */
    (void)code;

    TEST_ASSERT_TRUE( userData );
    AiaTestConnectionManagerObserver_t* connectionManagerObserver =
        (AiaTestConnectionManagerObserver_t*)userData;
    connectionManagerObserver->onDisconnectedCallCount++;
}

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

bool AiaGetIotClientId( char* iotClientId, size_t* len )
{
    if( !len )
    {
        return false;
    }

    size_t iotClientIdLen = sizeof( CONNECTION_MANAGER_TEST_IOT_CLIENT_ID ) - 1;
    if( iotClientId )
    {
        if( *len < iotClientIdLen )
        {
            return false;
        }
        memcpy( iotClientId, CONNECTION_MANAGER_TEST_IOT_CLIENT_ID,
                iotClientIdLen );
    }
    *len = iotClientIdLen;
    return true;
}

bool AiaGetAwsAccountId( char* awsAccountId, size_t* len )
{
    if( !len )
    {
        return false;
    }

    size_t awsAccountIdLen =
        sizeof( CONNECTION_MANAGER_TEST_AWS_ACCOUNT_ID ) - 1;
    if( awsAccountId )
    {
        if( *len < awsAccountIdLen )
        {
            return false;
        }
        memcpy( awsAccountId, CONNECTION_MANAGER_TEST_AWS_ACCOUNT_ID,
                awsAccountIdLen );
    }
    *len = awsAccountIdLen;
    return true;
}

bool AiaMqttPublish( AiaMqttConnectionPointer_t connection, AiaMqttQos_t qos,
                     const char* topic, size_t topicLength, const void* message,
                     size_t messageLength )
{
    (void)connection;
    (void)qos;
    (void)topic;
    (void)topicLength;
    (void)message;
    (void)messageLength;
    return true;
}

/*-----------------------------------------------------------*/

static AiaMqttConnectionPointer_t mqttConnection =
    IOT_MQTT_CONNECTION_INITIALIZER;

static AiaConnectionManager_t* testConnectionManager;
static AiaTestConnectionManagerObserver_t* testObserver;

/*-----------------------------------------------------------*/

/**
 * @brief Test group for AiaConnectionManager_t tests.
 */
TEST_GROUP( AiaConnectionManagerTests );

/*-----------------------------------------------------------*/

/**
 * @brief Test setup for AiaConnectionManager_t tests.
 */
TEST_SETUP( AiaConnectionManagerTests )
{
    AiaMbedtlsThreading_Init();
    AiaRandomMbedtls_Init();
    TEST_ASSERT_TRUE( AiaRandomMbedtls_Seed( TEST_SALT, TEST_SALT_LENGTH ) );

    AiaTaskPoolInfo_t taskpoolInfo = AiaTaskPool( INFO_INITIALIZER );
    AiaTaskPoolError_t error =
        AiaTaskPool( CreateSystemTaskPool )( &taskpoolInfo );
    TEST_ASSERT_TRUE( AiaTaskPoolSucceeded( error ) );

    mqttConnection = AiaCalloc( 1, sizeof( AiaMqttConnectionPointer_t ) );
    TEST_ASSERT_NOT_NULL( mqttConnection );
    testObserver = AiaTestConnectionManagerObserver_Create();
    TEST_ASSERT_NOT_NULL( testObserver );
    testConnectionManager = AiaConnectionManager_Create(
        onConnectionSuccess, testObserver, onConnectionRejected, testObserver,
        OnDisconnected, testObserver, onMqttMessageReceived, NULL,
        mqttConnection, AiaTaskPool( GetSystemTaskPool )() );
    TEST_ASSERT_NOT_NULL( testConnectionManager );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test tear down for AiaConnectionManager_t tests.
 */
TEST_TEAR_DOWN( AiaConnectionManagerTests )
{
    AiaConnectionManager_Destroy( testConnectionManager );
    AiaTestConnectionManagerObserver_Destroy( testObserver );
    AiaFree( mqttConnection );

    AiaTaskPoolError_t error =
        AiaTaskPool( Destroy )( AiaTaskPool( GetSystemTaskPool )() );
    TEST_ASSERT_TRUE( AiaTaskPoolSucceeded( error ) );

    AiaMbedtlsThreading_Cleanup();
    AiaRandomMbedtls_Cleanup();
}

/*-----------------------------------------------------------*/

/**
 * @brief Test group runner for AiaConnectionManager_t tests.
 */
TEST_GROUP_RUNNER( AiaConnectionManagerTests )
{
    RUN_TEST_CASE( AiaConnectionManagerTests, CreateAndDestroy );
    RUN_TEST_CASE( AiaConnectionManagerTests,
                   CreateWithoutOnConnectionSuccessCallback );
    RUN_TEST_CASE( AiaConnectionManagerTests,
                   CreateWithoutOnConnectionRejectedCallback );
    RUN_TEST_CASE( AiaConnectionManagerTests,
                   CreateWithoutOnDisconnectedCallback );
    RUN_TEST_CASE( AiaConnectionManagerTests,
                   CreateWithoutOnMqttMessageReceivedCallback );
    RUN_TEST_CASE( AiaConnectionManagerTests, CreateWithoutMqttConnection );
    RUN_TEST_CASE( AiaConnectionManagerTests, CreateWithoutTaskPool );
    RUN_TEST_CASE( AiaConnectionManagerTests, DestroyNull );
    RUN_TEST_CASE( AiaConnectionManagerTests,
                   ReceiveAckConnectionNoConnectionManager );
    RUN_TEST_CASE( AiaConnectionManagerTests, ReceiveAckConnectionNoPayload );
    RUN_TEST_CASE( AiaConnectionManagerTests, ReceiveAckConnectionEstablished );
    RUN_TEST_CASE( AiaConnectionManagerTests, ReceiveAckWithDescription );
    RUN_TEST_CASE( AiaConnectionManagerTests, ReceiveAckUnknownFailure );
    RUN_TEST_CASE( AiaConnectionManagerTests, ReceiveAckInvalidAccountId );
    RUN_TEST_CASE( AiaConnectionManagerTests, ReceiveAckInvalidClientId );
    RUN_TEST_CASE( AiaConnectionManagerTests, ReceiveAckApiVersionDeprecated );
    RUN_TEST_CASE( AiaConnectionManagerTests,
                   ReceiveDisconnectNoConnectionManager );
    RUN_TEST_CASE( AiaConnectionManagerTests, ReceiveDisconnectNoPayload );
    RUN_TEST_CASE( AiaConnectionManagerTests,
                   ReceiveDisconnectUnexpectedSequenceNumber );
    RUN_TEST_CASE( AiaConnectionManagerTests,
                   ReceiveDisconnectMessageTampered );
    RUN_TEST_CASE( AiaConnectionManagerTests,
                   ReceiveDisconnectApiVersionDeprecated );
    RUN_TEST_CASE( AiaConnectionManagerTests,
                   ReceiveDisconnectEncryptionError );
    RUN_TEST_CASE( AiaConnectionManagerTests, ReceiveDisconnectGoingOffline );
}

/*-----------------------------------------------------------*/

/**
 * TODO: ADSER-1518 add tests after AiaMqttConnectionPointer_t is mocked for
 * tests
 * ConnectAndAcknowledge
 * ConnectAndBackoff
 * Disconnect
 * ConnectAcknowledgeAndDisconnect
 */

/*-----------------------------------------------------------*/

TEST( AiaConnectionManagerTests, CreateAndDestroy )
{
    /* Empty test to verify that setup/teardown pass in isolation. */
}

/*-----------------------------------------------------------*/

TEST( AiaConnectionManagerTests, CreateWithoutOnConnectionSuccessCallback )
{
    TEST_ASSERT_NULL( AiaConnectionManager_Create(
        NULL, NULL, onConnectionRejected, NULL, OnDisconnected, NULL,
        onMqttMessageReceived, NULL, mqttConnection,
        AiaTaskPool( GetSystemTaskPool )() ) );
}

/*-----------------------------------------------------------*/

TEST( AiaConnectionManagerTests, CreateWithoutOnConnectionRejectedCallback )
{
    TEST_ASSERT_NULL( AiaConnectionManager_Create(
        onConnectionSuccess, NULL, NULL, NULL, OnDisconnected, NULL,
        onMqttMessageReceived, NULL, mqttConnection,
        AiaTaskPool( GetSystemTaskPool )() ) );
}

/*-----------------------------------------------------------*/

TEST( AiaConnectionManagerTests, CreateWithoutOnDisconnectedCallback )
{
    TEST_ASSERT_NULL( AiaConnectionManager_Create(
        onConnectionSuccess, NULL, onConnectionRejected, NULL, NULL, NULL,
        onMqttMessageReceived, NULL, mqttConnection,
        AiaTaskPool( GetSystemTaskPool )() ) );
}

/*-----------------------------------------------------------*/

TEST( AiaConnectionManagerTests, CreateWithoutOnMqttMessageReceivedCallback )
{
    TEST_ASSERT_NULL( AiaConnectionManager_Create(
        onConnectionSuccess, NULL, onConnectionRejected, NULL, OnDisconnected,
        NULL, NULL, NULL, mqttConnection,
        AiaTaskPool( GetSystemTaskPool )() ) );
}

/*-----------------------------------------------------------*/

TEST( AiaConnectionManagerTests, CreateWithoutMqttConnection )
{
    TEST_ASSERT_NULL( AiaConnectionManager_Create(
        onConnectionSuccess, NULL, onConnectionRejected, NULL, OnDisconnected,
        NULL, onMqttMessageReceived, NULL, NULL,
        AiaTaskPool( GetSystemTaskPool )() ) );
}

/*-----------------------------------------------------------*/

TEST( AiaConnectionManagerTests, CreateWithoutTaskPool )
{
    TEST_ASSERT_NULL( AiaConnectionManager_Create(
        onConnectionSuccess, NULL, onConnectionRejected, NULL, OnDisconnected,
        NULL, onMqttMessageReceived, NULL, mqttConnection, NULL ) );
}

/*-----------------------------------------------------------*/

TEST( AiaConnectionManagerTests, DestroyNull )
{
    /* No asserts just to exercise code path. */
    AiaConnectionManager_Destroy( NULL );
}

/*-----------------------------------------------------------*/

TEST( AiaConnectionManagerTests, ReceiveAckConnectionNoConnectionManager )
{
    /* No asserts just to exercise code path. */
    char* payload = CONNECTION_MANAGER_TEST_PAYLOAD(
        AIA_CONNECTION_ACK_CONNECTION_ESTABLISHED );

    AiaConnectionManager_OnConnectionAcknowledgementReceived(
        NULL, payload, strlen( payload ) );
}

/*-----------------------------------------------------------*/

TEST( AiaConnectionManagerTests, ReceiveAckConnectionNoPayload )
{
    AiaConnectionManager_OnConnectionAcknowledgementReceived(
        testConnectionManager, NULL, 0 );

    TEST_ASSERT_EQUAL_INT( 0, testObserver->onConnectionSuccessCallCount );
    TEST_ASSERT_EQUAL_INT( 0, testObserver->onConnectionRejectedCallCount );
}

/*-----------------------------------------------------------*/

TEST( AiaConnectionManagerTests, ReceiveAckConnectionEstablished )
{
    char* payload = CONNECTION_MANAGER_TEST_PAYLOAD(
        AIA_CONNECTION_ACK_CONNECTION_ESTABLISHED );

    AiaConnectionManager_OnConnectionAcknowledgementReceived(
        testConnectionManager, payload, strlen( payload ) );

    TEST_ASSERT_EQUAL_INT( 1, testObserver->onConnectionSuccessCallCount );
    TEST_ASSERT_EQUAL_INT( 0, testObserver->onConnectionRejectedCallCount );
}

/*-----------------------------------------------------------*/

TEST( AiaConnectionManagerTests, ReceiveAckWithDescription )
{
    char* payload = CONNECTION_MANAGER_TEST_DESCRIPTION_PAYLOAD(
        AIA_CONNECTION_ACK_CONNECTION_ESTABLISHED,
        CONNECTION_MANAGER_TEST_DESCRIPTION );

    AiaConnectionManager_OnConnectionAcknowledgementReceived(
        testConnectionManager, payload, strlen( payload ) );

    TEST_ASSERT_EQUAL_INT( 1, testObserver->onConnectionSuccessCallCount );
    TEST_ASSERT_EQUAL_INT( 0, testObserver->onConnectionRejectedCallCount );
}

/*-----------------------------------------------------------*/

TEST( AiaConnectionManagerTests, ReceiveAckUnknownFailure )
{
    char* payload =
        CONNECTION_MANAGER_TEST_PAYLOAD( AIA_CONNECTION_ACK_UNKNOWN_FAILURE );

    AiaConnectionManager_OnConnectionAcknowledgementReceived(
        testConnectionManager, payload, strlen( payload ) );

    TEST_ASSERT_EQUAL_INT( 0, testObserver->onConnectionSuccessCallCount );
    TEST_ASSERT_EQUAL_INT( 1, testObserver->onConnectionRejectedCallCount );
}

/*-----------------------------------------------------------*/

TEST( AiaConnectionManagerTests, ReceiveAckInvalidAccountId )
{
    char* payload = CONNECTION_MANAGER_TEST_PAYLOAD(
        AIA_CONNECTION_ACK_INVALID_ACCOUNT_ID );

    AiaConnectionManager_OnConnectionAcknowledgementReceived(
        testConnectionManager, payload, strlen( payload ) );

    TEST_ASSERT_EQUAL_INT( 0, testObserver->onConnectionSuccessCallCount );
    TEST_ASSERT_EQUAL_INT( 1, testObserver->onConnectionRejectedCallCount );
}

/*-----------------------------------------------------------*/

TEST( AiaConnectionManagerTests, ReceiveAckInvalidClientId )
{
    char* payload =
        CONNECTION_MANAGER_TEST_PAYLOAD( AIA_CONNECTION_ACK_INVALID_CLIENT_ID );

    AiaConnectionManager_OnConnectionAcknowledgementReceived(
        testConnectionManager, payload, strlen( payload ) );

    TEST_ASSERT_EQUAL_INT( 0, testObserver->onConnectionSuccessCallCount );
    TEST_ASSERT_EQUAL_INT( 1, testObserver->onConnectionRejectedCallCount );
}

/*-----------------------------------------------------------*/

TEST( AiaConnectionManagerTests, ReceiveAckApiVersionDeprecated )
{
    char* payload = CONNECTION_MANAGER_TEST_PAYLOAD(
        AIA_CONNECTION_ACK_API_VERSION_DEPRECATED );

    AiaConnectionManager_OnConnectionAcknowledgementReceived(
        testConnectionManager, payload, strlen( payload ) );

    TEST_ASSERT_EQUAL_INT( 0, testObserver->onConnectionSuccessCallCount );
    TEST_ASSERT_EQUAL_INT( 1, testObserver->onConnectionRejectedCallCount );
}

/*-----------------------------------------------------------*/

TEST( AiaConnectionManagerTests, ReceiveDisconnectNoConnectionManager )
{
    /* No asserts just to exercise code path. */
    char* payload = CONNECTION_MANAGER_TEST_PAYLOAD(
        AIA_CONNECTION_DISCONNECT_UNEXPECTED_SEQUENCE_NUMBER );

    AiaConnectionManager_OnConnectionDisconnectReceived( NULL, payload,
                                                         strlen( payload ) );
}

/*-----------------------------------------------------------*/

TEST( AiaConnectionManagerTests, ReceiveDisconnectNoPayload )
{
    AiaConnectionManager_OnConnectionDisconnectReceived( testConnectionManager,
                                                         NULL, 0 );

    TEST_ASSERT_EQUAL_INT( 0, testObserver->onDisconnectedCallCount );
}

/*-----------------------------------------------------------*/

TEST( AiaConnectionManagerTests, ReceiveDisconnectUnexpectedSequenceNumber )
{
    char* payload = CONNECTION_MANAGER_TEST_PAYLOAD(
        AIA_CONNECTION_DISCONNECT_UNEXPECTED_SEQUENCE_NUMBER );

    AiaConnectionManager_OnConnectionDisconnectReceived(
        testConnectionManager, payload, strlen( payload ) );

    TEST_ASSERT_EQUAL_INT( 1, testObserver->onDisconnectedCallCount );
}

/*-----------------------------------------------------------*/

TEST( AiaConnectionManagerTests, ReceiveDisconnectMessageTampered )
{
    char* payload = CONNECTION_MANAGER_TEST_PAYLOAD(
        AIA_CONNECTION_DISCONNECT_MESSAGE_TAMPERED );

    AiaConnectionManager_OnConnectionDisconnectReceived(
        testConnectionManager, payload, strlen( payload ) );

    TEST_ASSERT_EQUAL_INT( 1, testObserver->onDisconnectedCallCount );
}

/*-----------------------------------------------------------*/

TEST( AiaConnectionManagerTests, ReceiveDisconnectApiVersionDeprecated )
{
    char* payload = CONNECTION_MANAGER_TEST_PAYLOAD(
        AIA_CONNECTION_DISCONNECT_API_VERSION_DEPRECATED );

    AiaConnectionManager_OnConnectionDisconnectReceived(
        testConnectionManager, payload, strlen( payload ) );

    TEST_ASSERT_EQUAL_INT( 1, testObserver->onDisconnectedCallCount );
}

/*-----------------------------------------------------------*/

TEST( AiaConnectionManagerTests, ReceiveDisconnectEncryptionError )
{
    char* payload = CONNECTION_MANAGER_TEST_PAYLOAD(
        AIA_CONNECTION_DISCONNECT_ENCRYPTION_ERROR );

    AiaConnectionManager_OnConnectionDisconnectReceived(
        testConnectionManager, payload, strlen( payload ) );

    TEST_ASSERT_EQUAL_INT( 1, testObserver->onDisconnectedCallCount );
}

/*-----------------------------------------------------------*/

TEST( AiaConnectionManagerTests, ReceiveDisconnectGoingOffline )
{
    char* payload = CONNECTION_MANAGER_TEST_PAYLOAD(
        AIA_CONNECTION_DISCONNECT_GOING_OFFLINE );

    AiaConnectionManager_OnConnectionDisconnectReceived(
        testConnectionManager, payload, strlen( payload ) );

    TEST_ASSERT_EQUAL_INT( 1, testObserver->onDisconnectedCallCount );
}

/*-----------------------------------------------------------*/
