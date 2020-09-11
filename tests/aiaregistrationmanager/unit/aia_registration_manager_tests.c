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
 * @file aia_registration_manager_tests.c
 * @brief Tests for AiaRegistrationManager.
 */

/* The config header is always included first. */
#include <aia_config.h>

#include <aiacore/aia_mbedtls_threading.h>
#include <aiaregistrationmanager/aia_registration_manager.h>

/* Test framework includes. */
#include <unity_fixture.h>

#define AIA_REGISTRATION_MANAGER_TEST_IOT_ENDPOINT "testIotEndpoint"
#define AIA_REGISTRATION_MANAGER_TEST_IOT_CLIENT_ID "testIotClientId"
#define AIA_REGISTRATION_MANAGER_TEST_AWS_ACCOUNT_ID "123456789012"
#define AIA_REGISTRATION_MANAGER_TEST_LWA_REFRESH_TOKEN "testLwaRefreshToken"
#define AIA_REGISTRATION_MANAGER_TEST_LWA_CLIENT_ID "testLwaClientId"
#define AIA_REGISTRATION_MANAGER_TEST_PUBLIC_KEY \
    "ECwsWzu0i9Uzibkh9kPjHX+UFRQgbJSN42EK7vJ3xmk="
#define AIA_REGISTRATION_MANAGER_TEST_TOPIC_ROOT "test/topic/root"
#define AIA_REGISTRATION_MANAGER_TEST_CODE "TEST_CODE"
#define AIA_REGISTRATION_MANAGER_TEST_DESCRIPTION "testDescription"
#define AIA_REGISTRATION_MANAGER_TEST_INVALID_RESPONSE \
    "{\"invalid\":\"response\"}"

/* clang-format off */
#define AIA_REGISTRATION_MANAGER_TEST_RESPONSE_SUCCESS                          \
    "{"                                                                         \
        "\"encryption\":{"                                                      \
            "\"publicKey\":\"" AIA_REGISTRATION_MANAGER_TEST_PUBLIC_KEY "\""    \
        "}"                                                                     \
        ",\"iot\":{"                                                            \
             "\"topicRoot\":\"" AIA_REGISTRATION_MANAGER_TEST_TOPIC_ROOT "\""   \
        "}"                                                                     \
    "}"
#define AIA_REGISTRATION_MANAGER_TEST_RESPONSE_FAILURE                  \
    "{"                                                                 \
        "\"code\":\"" AIA_REGISTRATION_MANAGER_TEST_CODE                \
        "\","                                                           \
        "\"description\":\"" AIA_REGISTRATION_MANAGER_TEST_DESCRIPTION  \
        "\""                                                            \
    "}"
/* clang-format on */

/* Mock type used to persist secrets (in memory). */
typedef struct AiaMockSecretStorer
{
    /** Mock stored secret. */
    uint8_t* storedSecret;

    /** Mock stored secret length. */
    size_t storedSecretLen;

    /** Value to return on calls to @c AiaStoreSecret and @c AiaLoadSecret. */
    bool valueToReturn;
} AiaMockSecretStorer_t;

/* Mock type used to persist topic root (in memory). */
typedef struct AiaMockTopicRootStorer
{
    /** Mock stored topic root. */
    uint8_t* storedTopicRoot;

    /** Mock stored topic root length. */
    size_t storedTopicRootLen;

    /** Value to return on calls to  @c AiaLoadTopicRoot. */
    bool valueToReturn;
} AiaMockTopicRootStorer_t;

typedef struct AiaRegistrationManagerTestData
{
    bool isSendRequestSuccess;
    bool isRegisterSuccess;

    /** The response status code to receive. */
    size_t responseStatus;

    /** The body of the response to receive. */
    char* responseBody;

    /** Length of @c responseBody. */
    size_t responseBodyLen;

    /** The number of times the failure callback for registration */
    size_t failureCallbackCount;

    /** The number of times the success callback for registration */
    size_t successCallbackCount;
} AiaRegistrationManagerTestData_t;

/** Object used to mock the persistent storage of secrets. */
AiaMockSecretStorer_t* testSecretStorer;

/** Object used to mock the persistent storage of topic root. */
AiaMockTopicRootStorer_t* testTopicRootStorer;

/** Object to hold test data. */
AiaRegistrationManagerTestData_t aiaRegistrationTestData;

/** The secret manager to test. */
static AiaRegistrationManager_t* testRegistrationManager;

bool AiaStoreSecret( const uint8_t* sharedSecret, size_t size )
{
    TEST_ASSERT_NOT_NULL( testSecretStorer );
    if( testSecretStorer->storedSecret )
    {
        AiaFree( testSecretStorer->storedSecret );
    }
    testSecretStorer->storedSecret = AiaCalloc( 1, size );
    TEST_ASSERT_NOT_NULL( testSecretStorer->storedSecret );
    memcpy( testSecretStorer->storedSecret, sharedSecret, size );
    testSecretStorer->storedSecretLen = size;
    return testSecretStorer->valueToReturn;
}

bool AiaLoadSecret( uint8_t* sharedSecret, size_t size )
{
    TEST_ASSERT_NOT_NULL( testSecretStorer );
    TEST_ASSERT_NOT_NULL( testSecretStorer->storedSecret );
    TEST_ASSERT_EQUAL( size, testSecretStorer->storedSecretLen );
    memcpy( sharedSecret, testSecretStorer->storedSecret, size );
    return testSecretStorer->valueToReturn;
}

bool AiaStoreTopicRoot( const uint8_t* topicRoot, size_t size )
{
    TEST_ASSERT_NOT_NULL( testTopicRootStorer );
    if( testTopicRootStorer->storedTopicRoot )
    {
        AiaFree( testTopicRootStorer->storedTopicRoot );
    }
    testTopicRootStorer->storedTopicRoot = AiaCalloc( 1, size );
    TEST_ASSERT_NOT_NULL( testTopicRootStorer->storedTopicRoot );
    memcpy( testTopicRootStorer->storedTopicRoot, topicRoot, size );
    testTopicRootStorer->storedTopicRootLen = size;
    return testTopicRootStorer->valueToReturn;
}

bool AiaLoadTopicRoot( uint8_t* topicRoot, size_t size )
{
    TEST_ASSERT_NOT_NULL( testTopicRootStorer );
    TEST_ASSERT_NOT_NULL( testTopicRootStorer->storedTopicRoot );
    TEST_ASSERT_EQUAL( size, testTopicRootStorer->storedTopicRootLen );
    memcpy( topicRoot, testTopicRootStorer->storedTopicRoot, size );
    return testTopicRootStorer->valueToReturn;
}

bool AiaGetRefreshToken( char* refreshToken, size_t* len )
{
    if( !len )
    {
        return false;
    }

    size_t refreshTokenLen =
        sizeof( AIA_REGISTRATION_MANAGER_TEST_LWA_REFRESH_TOKEN ) - 1;
    if( refreshToken )
    {
        if( *len < refreshTokenLen )
        {
            return false;
        }
        memcpy( refreshToken, AIA_REGISTRATION_MANAGER_TEST_LWA_REFRESH_TOKEN,
                refreshTokenLen );
    }
    *len = refreshTokenLen;
    return true;
}

bool AiaGetLwaClientId( char* lwaClientId, size_t* len )
{
    if( !len )
    {
        return false;
    }

    size_t lwaClientIdLen =
        sizeof( AIA_REGISTRATION_MANAGER_TEST_LWA_CLIENT_ID ) - 1;
    if( lwaClientId )
    {
        if( *len < lwaClientIdLen )
        {
            return false;
        }
        memcpy( lwaClientId, AIA_REGISTRATION_MANAGER_TEST_LWA_CLIENT_ID,
                lwaClientIdLen );
    }
    *len = lwaClientIdLen;
    return true;
}

bool AiaGetIotClientId( char* iotClientId, size_t* len )
{
    if( !len )
    {
        return false;
    }

    size_t iotClientIdLen =
        sizeof( AIA_REGISTRATION_MANAGER_TEST_IOT_CLIENT_ID ) - 1;
    if( iotClientId )
    {
        if( *len < iotClientIdLen )
        {
            return false;
        }
        memcpy( iotClientId, AIA_REGISTRATION_MANAGER_TEST_IOT_CLIENT_ID,
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
        sizeof( AIA_REGISTRATION_MANAGER_TEST_AWS_ACCOUNT_ID ) - 1;
    if( awsAccountId )
    {
        if( *len < awsAccountIdLen )
        {
            return false;
        }
        memcpy( awsAccountId, AIA_REGISTRATION_MANAGER_TEST_AWS_ACCOUNT_ID,
                awsAccountIdLen );
    }
    *len = awsAccountIdLen;
    return true;
}

bool AiaGetIotEndpoint( char* iotEndpoint, size_t* len )
{
    if( !len )
    {
        return false;
    }

    size_t iotEndpointLen =
        sizeof( AIA_REGISTRATION_MANAGER_TEST_IOT_ENDPOINT ) - 1;
    if( iotEndpoint )
    {
        if( *len < iotEndpointLen )
        {
            return false;
        }
        memcpy( iotEndpoint, AIA_REGISTRATION_MANAGER_TEST_IOT_ENDPOINT,
                iotEndpointLen );
    }
    *len = iotEndpointLen;
    return true;
}

bool AiaSendHttpsRequest( AiaHttpsRequest_t* httpsRequest,
                          AiaHttpsConnectionResponseCallback_t responseCallback,
                          void* responseCallbackUserData,
                          AiaHttpsConnectionFailureCallback_t failureCallback,
                          void* failureCallbackUserData )
{
    (void)httpsRequest;
    if( !aiaRegistrationTestData.isSendRequestSuccess )
    {
        return false;
    }

    if( aiaRegistrationTestData.isRegisterSuccess )
    {
        AiaHttpsResponse_t httpsResponse;
        httpsResponse.status = aiaRegistrationTestData.responseStatus;
        httpsResponse.body = aiaRegistrationTestData.responseBody;
        httpsResponse.bodyLen = aiaRegistrationTestData.responseBodyLen;

        responseCallback( &httpsResponse, responseCallbackUserData );
    }
    else
    {
        failureCallback( failureCallbackUserData );
    }
    return true;
}

static void onRegistrationFailed( void* userData,
                                  AiaRegistrationFailureCode_t code )
{
    (void)userData;
    (void)code;
    aiaRegistrationTestData.failureCallbackCount++;
}

static void onRegistrationSuccess( void* userData )
{
    (void)userData;
    aiaRegistrationTestData.successCallbackCount++;
}

/*-----------------------------------------------------------*/

/**
 * @brief Test group for AiaRegistrationManager tests.
 */
TEST_GROUP( AiaRegistrationManagerTests );

/*-----------------------------------------------------------*/

/**
 * @brief Test setup for AiaRegistrationManager tests.
 */
TEST_SETUP( AiaRegistrationManagerTests )
{
    AiaMbedtlsThreading_Init();
    TEST_ASSERT_TRUE( AiaCryptoMbedtls_Init() );

    testSecretStorer = AiaCalloc( 1, sizeof( AiaMockSecretStorer_t ) );
    TEST_ASSERT_NOT_NULL( testSecretStorer );
    testTopicRootStorer = AiaCalloc( 1, sizeof( AiaMockTopicRootStorer_t ) );
    TEST_ASSERT_NOT_NULL( testTopicRootStorer );

    testSecretStorer->valueToReturn = true;
    testTopicRootStorer->valueToReturn = true;
    aiaRegistrationTestData.failureCallbackCount = 0;
    aiaRegistrationTestData.successCallbackCount = 0;

    testRegistrationManager = AiaRegistrationManager_Create(
        onRegistrationSuccess, NULL, onRegistrationFailed, NULL );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test tear down for AiaRegistrationManager tests.
 */
TEST_TEAR_DOWN( AiaRegistrationManagerTests )
{
    AiaRegistrationManager_Destroy( testRegistrationManager );
    if( testSecretStorer->storedSecret )
    {
        AiaFree( testSecretStorer->storedSecret );
    }
    AiaFree( testSecretStorer );
    if( testTopicRootStorer->storedTopicRoot )
    {
        AiaFree( testTopicRootStorer->storedTopicRoot );
    }
    AiaFree( testTopicRootStorer );

    AiaCryptoMbedtls_Cleanup();
    AiaMbedtlsThreading_Cleanup();
}

/*-----------------------------------------------------------*/

/**
 * @brief Test group runner for AiaCryptoMbedtls_t tests.
 */
TEST_GROUP_RUNNER( AiaRegistrationManagerTests )
{
    RUN_TEST_CASE( AiaRegistrationManagerTests, CreateAndDestroy );
    RUN_TEST_CASE( AiaRegistrationManagerTests, CreateNullParams );
    RUN_TEST_CASE( AiaRegistrationManagerTests, RegisterSuccessResponse );
    RUN_TEST_CASE( AiaRegistrationManagerTests,
                   RegisterSuccessResponseInvalidBody );
    RUN_TEST_CASE( AiaRegistrationManagerTests, RegisterFailResponse );
    RUN_TEST_CASE( AiaRegistrationManagerTests,
                   RegisterFailResponseInvalidBody );
    RUN_TEST_CASE( AiaRegistrationManagerTests, RegisterNullParams );
    RUN_TEST_CASE( AiaRegistrationManagerTests, RegisterSendRequestFail );
    RUN_TEST_CASE( AiaRegistrationManagerTests, RegisterSuccessStorageFail );
}

/*-----------------------------------------------------------*/

TEST( AiaRegistrationManagerTests, CreateAndDestroy )
{
    /* Empty test to verify that setup/teardown pass in isolation. */
}

/*-----------------------------------------------------------*/

TEST( AiaRegistrationManagerTests, CreateNullParams )
{
    TEST_ASSERT_NULL( AiaRegistrationManager_Create(
        NULL, NULL, onRegistrationFailed, NULL ) );
    TEST_ASSERT_NULL( AiaRegistrationManager_Create( onRegistrationSuccess,
                                                     NULL, NULL, NULL ) );
}

/*-----------------------------------------------------------*/

TEST( AiaRegistrationManagerTests, RegisterSuccessResponse )
{
    aiaRegistrationTestData.isSendRequestSuccess = true;
    aiaRegistrationTestData.isRegisterSuccess = true;
    aiaRegistrationTestData.responseStatus = 200;
    aiaRegistrationTestData.responseBody =
        AIA_REGISTRATION_MANAGER_TEST_RESPONSE_SUCCESS;
    aiaRegistrationTestData.responseBodyLen =
        sizeof( AIA_REGISTRATION_MANAGER_TEST_RESPONSE_SUCCESS ) - 1;
    TEST_ASSERT_TRUE(
        AiaRegistrationManager_Register( testRegistrationManager ) );

    TEST_ASSERT_EQUAL( 1, aiaRegistrationTestData.successCallbackCount );
}

/*-----------------------------------------------------------*/

TEST( AiaRegistrationManagerTests, RegisterSuccessResponseInvalidBody )
{
    aiaRegistrationTestData.isSendRequestSuccess = true;
    aiaRegistrationTestData.isRegisterSuccess = true;
    aiaRegistrationTestData.responseStatus = 200;
    aiaRegistrationTestData.responseBody =
        AIA_REGISTRATION_MANAGER_TEST_INVALID_RESPONSE;
    aiaRegistrationTestData.responseBodyLen =
        sizeof( AIA_REGISTRATION_MANAGER_TEST_INVALID_RESPONSE ) - 1;
    TEST_ASSERT_TRUE(
        AiaRegistrationManager_Register( testRegistrationManager ) );

    TEST_ASSERT_EQUAL( 1, aiaRegistrationTestData.failureCallbackCount );
}

/*-----------------------------------------------------------*/

TEST( AiaRegistrationManagerTests, RegisterFailResponse )
{
    aiaRegistrationTestData.isSendRequestSuccess = true;
    aiaRegistrationTestData.isRegisterSuccess = false;
    aiaRegistrationTestData.responseStatus = 500;
    aiaRegistrationTestData.responseBody =
        AIA_REGISTRATION_MANAGER_TEST_RESPONSE_FAILURE;
    aiaRegistrationTestData.responseBodyLen =
        sizeof( AIA_REGISTRATION_MANAGER_TEST_RESPONSE_FAILURE ) - 1;
    TEST_ASSERT_TRUE(
        AiaRegistrationManager_Register( testRegistrationManager ) );

    TEST_ASSERT_EQUAL( 1, aiaRegistrationTestData.failureCallbackCount );
}

/*-----------------------------------------------------------*/

TEST( AiaRegistrationManagerTests, RegisterFailResponseInvalidBody )
{
    aiaRegistrationTestData.isSendRequestSuccess = true;
    aiaRegistrationTestData.isRegisterSuccess = false;
    aiaRegistrationTestData.responseStatus = 500;
    aiaRegistrationTestData.responseBody =
        AIA_REGISTRATION_MANAGER_TEST_INVALID_RESPONSE;
    aiaRegistrationTestData.responseBodyLen =
        sizeof( AIA_REGISTRATION_MANAGER_TEST_INVALID_RESPONSE ) - 1;
    TEST_ASSERT_TRUE(
        AiaRegistrationManager_Register( testRegistrationManager ) );

    TEST_ASSERT_EQUAL( 1, aiaRegistrationTestData.failureCallbackCount );
}

/*-----------------------------------------------------------*/

TEST( AiaRegistrationManagerTests, RegisterNullParams )
{
    TEST_ASSERT_FALSE( AiaRegistrationManager_Register( NULL ) );
    TEST_ASSERT_EQUAL( 0, aiaRegistrationTestData.successCallbackCount );
    TEST_ASSERT_EQUAL( 0, aiaRegistrationTestData.failureCallbackCount );
}

/*-----------------------------------------------------------*/

TEST( AiaRegistrationManagerTests, RegisterSendRequestFail )
{
    aiaRegistrationTestData.isSendRequestSuccess = false;
    TEST_ASSERT_FALSE(
        AiaRegistrationManager_Register( testRegistrationManager ) );
}

/*-----------------------------------------------------------*/

TEST( AiaRegistrationManagerTests, RegisterSuccessStorageFail )
{
    aiaRegistrationTestData.isSendRequestSuccess = true;
    aiaRegistrationTestData.isRegisterSuccess = true;
    aiaRegistrationTestData.responseStatus = 200;
    aiaRegistrationTestData.responseBody =
        AIA_REGISTRATION_MANAGER_TEST_RESPONSE_SUCCESS;
    aiaRegistrationTestData.responseBodyLen =
        sizeof( AIA_REGISTRATION_MANAGER_TEST_RESPONSE_SUCCESS ) - 1;

    testSecretStorer->valueToReturn = false;
    testTopicRootStorer->valueToReturn = true;
    TEST_ASSERT_TRUE(
        AiaRegistrationManager_Register( testRegistrationManager ) );

    TEST_ASSERT_EQUAL( 1, aiaRegistrationTestData.failureCallbackCount );

    testSecretStorer->valueToReturn = true;
    testTopicRootStorer->valueToReturn = false;
    TEST_ASSERT_TRUE(
        AiaRegistrationManager_Register( testRegistrationManager ) );

    TEST_ASSERT_EQUAL( 2, aiaRegistrationTestData.failureCallbackCount );
}

/*-----------------------------------------------------------*/
