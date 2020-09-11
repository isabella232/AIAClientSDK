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
 * @file aia_exception_manager_tests.c
 * @brief Tests for AiaExceptionManager_t.
 */

/* The config header is always included first. */
#include <aia_config.h>

/* Aia headers */
#include <aiaexceptionmanager/aia_exception_manager.h>

/* Other AIA headers */
#include <aiacore/aia_mbedtls_threading.h>
#include <aiacore/aia_random_mbedtls.h>

/* Test framework includes. */
#include <unity_fixture.h>

#include <aiamockregulator/aia_mock_regulator.h>
#include <aiatestutilities/aia_test_utilities.h>

#define AIA_EXCEPTION_MANAGER_TEST_EXCEPTION_PAYLOAD_FORMAT \
    "{"                                                     \
    "\"code\": \"%s\","                                     \
    "\"description\": \"%s\""                               \
    "}"

static const char* TEST_EXCEPTION_DESCRIPTION = "testDescription";
static const char* TEST_EXCEPTION_INVALID_CODE = "INVALID_CODE";
static const char* TEST_EXCEPTION_NO_CODE = "{\"description\":\"test\"}";
static const char* TEST_EXCEPTION_NO_DESCRIPTION =
    "{\"code\":\"INTERNAL_SERVICE\"}";

/** Sample Seed. */
static const char* TEST_SALT = "TestSalt";
static const size_t TEST_SALT_LENGTH = sizeof( TEST_SALT ) - 1;

typedef struct AiaTestExceptionManagerObserver
{
    bool isCalled;
    AiaExceptionCode_t exceptionCode;
} AiaTestExceptionManagerObserver_t;

static AiaTestExceptionManagerObserver_t*
AiaExceptionManagerTestObserver_Create()
{
    AiaTestExceptionManagerObserver_t* observer =
        AiaCalloc( 1, sizeof( AiaTestExceptionManagerObserver_t ) );
    if( !observer )
    {
        AiaLogError( "AiaCalloc failed." );
        return NULL;
    }

    return observer;
}

static void AiaExceptionManagerTestObserver_Destroy(
    AiaTestExceptionManagerObserver_t* observer )
{
    AiaFree( observer );
}

static void onException( void* userData, AiaExceptionCode_t code )
{
    TEST_ASSERT_NOT_NULL( userData );
    AiaTestExceptionManagerObserver_t* exceptionManagerObserver =
        (AiaTestExceptionManagerObserver_t*)userData;
    exceptionManagerObserver->isCalled = true;
    exceptionManagerObserver->exceptionCode = code;
}

static size_t BuildExceptionPayload( char* payloadBuffer,
                                     size_t payloadBufferSize, const char* code,
                                     const char* description )
{
    int result = snprintf( payloadBuffer, payloadBufferSize,
                           AIA_EXCEPTION_MANAGER_TEST_EXCEPTION_PAYLOAD_FORMAT,
                           code, description );
    if( result <= 0 )
    {
        return 0;
    }
    return (size_t)result + 1;
}

static void TestExceptionPayloadReceived(
    AiaExceptionManager_t* exceptionManager, const char* exceptionCode,
    const char* description )
{
    size_t payloadBufferSize =
        BuildExceptionPayload( NULL, 0, exceptionCode, description );
    TEST_ASSERT_TRUE( payloadBufferSize );
    char payloadBuffer[ payloadBufferSize ];
    TEST_ASSERT_TRUE( BuildExceptionPayload( payloadBuffer, payloadBufferSize,
                                             exceptionCode, description ) );

    AiaExceptionManager_OnExceptionReceived( exceptionManager, payloadBuffer,
                                             payloadBufferSize, 0, 0 );
}

/*-----------------------------------------------------------*/

static AiaExceptionManager_t* g_testExceptionManager;
static AiaTestExceptionManagerObserver_t* g_testObserver;
static AiaRegulator_t* g_mockRegulator;

/*-----------------------------------------------------------*/

/**
 * @brief Test group for AiaExceptionManager_t tests.
 */
TEST_GROUP( AiaExceptionManagerTests );

/*-----------------------------------------------------------*/

/**
 * @brief Test group runner for AiaExceptionManager_t tests.
 */
TEST_GROUP_RUNNER( AiaExceptionManagerTests )
{
    RUN_TEST_CASE( AiaExceptionManagerTests, CreateAndDestroy );
    RUN_TEST_CASE( AiaExceptionManagerTests, CreateNullParams );
    RUN_TEST_CASE( AiaExceptionManagerTests, ReceiveExceptionValidFull );
    RUN_TEST_CASE( AiaExceptionManagerTests,
                   ReceiveExceptionValidNoDescription );
    RUN_TEST_CASE( AiaExceptionManagerTests, ReceiveExceptionInvalidCode );
}

/*-----------------------------------------------------------*/
/**
 * @brief Test setup for AiaExceptionManager_t tests.
 */
TEST_SETUP( AiaExceptionManagerTests )
{
    AiaMbedtlsThreading_Init();
    AiaRandomMbedtls_Init();
    TEST_ASSERT_TRUE( AiaRandomMbedtls_Seed( TEST_SALT, TEST_SALT_LENGTH ) );

    g_mockRegulator = (AiaRegulator_t*)AiaMockRegulator_Create();
    TEST_ASSERT_NOT_NULL( g_mockRegulator );

    g_testObserver = AiaExceptionManagerTestObserver_Create();
    TEST_ASSERT_TRUE( g_testObserver );
    g_testExceptionManager = AiaExceptionManager_Create(
        g_mockRegulator, onException, g_testObserver );
    TEST_ASSERT_NOT_NULL( g_testExceptionManager );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test tear down for AiaExceptionManager_t tests.
 */
TEST_TEAR_DOWN( AiaExceptionManagerTests )
{
    AiaExceptionManager_Destroy( g_testExceptionManager );
    AiaExceptionManagerTestObserver_Destroy( g_testObserver );
    AiaMockRegulator_Destroy( (AiaMockRegulator_t*)g_mockRegulator,
                              AiaTestUtilities_DestroyJsonChunk, NULL );

    AiaMbedtlsThreading_Cleanup();
    AiaRandomMbedtls_Cleanup();
}

/*-----------------------------------------------------------*/

TEST( AiaExceptionManagerTests, CreateAndDestroy )
{
    /* Empty test to verify that setup/teardown pass in isolation. */
}

TEST( AiaExceptionManagerTests, CreateNullParams )
{
    TEST_ASSERT_NULL(
        AiaExceptionManager_Create( NULL, onException, g_testObserver ) );
    TEST_ASSERT_NOT_NULL(
        AiaExceptionManager_Create( g_mockRegulator, NULL, g_testObserver ) );
    TEST_ASSERT_NOT_NULL(
        AiaExceptionManager_Create( g_mockRegulator, onException, NULL ) );
}

TEST( AiaExceptionManagerTests, ReceiveExceptionValidFull )
{
    g_testObserver->isCalled = false;
    TestExceptionPayloadReceived(
        g_testExceptionManager,
        AiaExceptionCode_ToString( AIA_EXCEPTION_AIS_UNAVAILABLE ),
        TEST_EXCEPTION_DESCRIPTION );
    TEST_ASSERT_TRUE( g_testObserver->isCalled );
    TEST_ASSERT_EQUAL( g_testObserver->exceptionCode,
                       AIA_EXCEPTION_AIS_UNAVAILABLE );

    g_testObserver->isCalled = false;
    TestExceptionPayloadReceived(
        g_testExceptionManager,
        AiaExceptionCode_ToString( AIA_EXCEPTION_INTERNAL_SERVICE ),
        TEST_EXCEPTION_DESCRIPTION );
    TEST_ASSERT_TRUE( g_testObserver->isCalled );
    TEST_ASSERT_EQUAL( g_testObserver->exceptionCode,
                       AIA_EXCEPTION_INTERNAL_SERVICE );

    g_testObserver->isCalled = false;
    TestExceptionPayloadReceived(
        g_testExceptionManager,
        AiaExceptionCode_ToString( AIA_EXCEPTION_THROTTLING ),
        TEST_EXCEPTION_DESCRIPTION );
    TEST_ASSERT_TRUE( g_testObserver->isCalled );
    TEST_ASSERT_EQUAL( g_testObserver->exceptionCode,
                       AIA_EXCEPTION_THROTTLING );

    g_testObserver->isCalled = false;
    TestExceptionPayloadReceived(
        g_testExceptionManager,
        AiaExceptionCode_ToString( AIA_EXCEPTION_UNSUPPORTED_API ),
        TEST_EXCEPTION_DESCRIPTION );
    TEST_ASSERT_TRUE( g_testObserver->isCalled );
    TEST_ASSERT_EQUAL( g_testObserver->exceptionCode,
                       AIA_EXCEPTION_UNSUPPORTED_API );

    g_testObserver->isCalled = false;
    TestExceptionPayloadReceived(
        g_testExceptionManager,
        AiaExceptionCode_ToString( AIA_EXCEPTION_INVALID_REQUEST ),
        TEST_EXCEPTION_DESCRIPTION );
    TEST_ASSERT_TRUE( g_testObserver->isCalled );
    TEST_ASSERT_EQUAL( g_testObserver->exceptionCode,
                       AIA_EXCEPTION_INVALID_REQUEST );
}

TEST( AiaExceptionManagerTests, ReceiveExceptionValidNoDescription )
{
    g_testObserver->isCalled = false;
    TestExceptionPayloadReceived(
        g_testExceptionManager,
        AiaExceptionCode_ToString( AIA_EXCEPTION_INTERNAL_SERVICE ), "" );
    TEST_ASSERT_TRUE( g_testObserver->isCalled );
    TEST_ASSERT_EQUAL( g_testObserver->exceptionCode,
                       AIA_EXCEPTION_INTERNAL_SERVICE );

    g_testObserver->isCalled = false;
    AiaExceptionManager_OnExceptionReceived(
        g_testExceptionManager, TEST_EXCEPTION_NO_DESCRIPTION,
        strlen( TEST_EXCEPTION_NO_DESCRIPTION ), 0, 0 );
    TEST_ASSERT_TRUE( g_testObserver->isCalled );
    TEST_ASSERT_EQUAL( g_testObserver->exceptionCode,
                       AIA_EXCEPTION_INTERNAL_SERVICE );
}

TEST( AiaExceptionManagerTests, ReceiveExceptionInvalidCode )
{
    g_testObserver->isCalled = false;
    TestExceptionPayloadReceived( g_testExceptionManager,
                                  TEST_EXCEPTION_INVALID_CODE,
                                  TEST_EXCEPTION_DESCRIPTION );
    TEST_ASSERT_FALSE( g_testObserver->isCalled );

    AiaExceptionManager_OnExceptionReceived(
        g_testExceptionManager, TEST_EXCEPTION_NO_CODE,
        strlen( TEST_EXCEPTION_NO_CODE ), 0, 0 );
    TEST_ASSERT_FALSE( g_testObserver->isCalled );
}
