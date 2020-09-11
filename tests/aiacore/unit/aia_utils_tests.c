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
 * @file aia_utils_tests.c
 * @brief Tests for AiaUtils.
 */

/* The config header is always included first. */
#include <aia_config.h>

#include <aiacore/aia_mbedtls_threading.h>
#include <aiacore/aia_random_mbedtls.h>
#include <aiacore/aia_utils.h>

/* Test framework includes. */
#include <unity_fixture.h>

/* Standard library includes. */
#include <ctype.h>
#include <inttypes.h>
#include <string.h>

/** Sample Seed. */
static const char* TEST_SALT = "TestSalt";
static const size_t TEST_SALT_LENGTH = sizeof( TEST_SALT ) - 1;

/*-----------------------------------------------------------*/

/**
 * @brief Test group for AiaUtilsTests tests.
 */
TEST_GROUP( AiaUtilsTests );

/*-----------------------------------------------------------*/

/**
 * @brief Test setup for AiaUtilsTests tests.
 */
TEST_SETUP( AiaUtilsTests )
{
    AiaMbedtlsThreading_Init();
    AiaRandomMbedtls_Init();
    TEST_ASSERT_TRUE( AiaRandomMbedtls_Seed( TEST_SALT, TEST_SALT_LENGTH ) );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test tear down for AiaUtilsTests tests.
 */
TEST_TEAR_DOWN( AiaUtilsTests )
{
    AiaMbedtlsThreading_Cleanup();
    AiaRandomMbedtls_Cleanup();
}

/*-----------------------------------------------------------*/

/**
 * @brief Test group runner for AiaJsonMessage_t tests.
 */
TEST_GROUP_RUNNER( AiaUtilsTests )
{
    RUN_TEST_CASE( AiaUtilsTests, AiaGenerateMessageIdWithoutBuffer );
    RUN_TEST_CASE( AiaUtilsTests, AiaGenerateMessageIdWithoutBufferLength );
    RUN_TEST_CASE( AiaUtilsTests, AiaGenerateMessageId );
}

/*-----------------------------------------------------------*/

TEST( AiaUtilsTests, AiaGenerateMessageIdWithoutBuffer )
{
    TEST_ASSERT_FALSE( AiaGenerateMessageId( NULL, 42 ) );
}

/*-----------------------------------------------------------*/

TEST( AiaUtilsTests, AiaGenerateMessageIdWithoutBufferLength )
{
    char buffer[ 42 ];
    TEST_ASSERT_FALSE( AiaGenerateMessageId( buffer, 0 ) );
}

/*-----------------------------------------------------------*/

TEST( AiaUtilsTests, AiaGenerateMessageId )
{
    char buffer[ 42 ];
    TEST_ASSERT_TRUE( AiaGenerateMessageId( buffer, sizeof( buffer ) ) );
    for( size_t i = 0; i < sizeof( buffer ) - 1; ++i )
    {
        TEST_ASSERT_TRUE( isprint( buffer[ i ] ) );
        TEST_ASSERT_NOT_EQUAL( '\\', buffer[ i ] );
        TEST_ASSERT_NOT_EQUAL( '"', buffer[ i ] );
        TEST_ASSERT_NOT_EQUAL( ' ', buffer[ i ] );
    }
    TEST_ASSERT_EQUAL( sizeof( buffer ) - 1, strlen( buffer ) );
}

/*-----------------------------------------------------------*/
