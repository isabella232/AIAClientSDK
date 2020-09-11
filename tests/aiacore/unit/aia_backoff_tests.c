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
 * @file aia_backoff_tests.c
 * @brief Tests for functions for AiaBackoff.
 */

/* The config header is always included first. */
#include <aia_config.h>

#include <aiacore/aia_backoff.h>
#include <aiacore/aia_random_mbedtls.h>

/* Test framework includes. */
#include <unity_fixture.h>

/*-----------------------------------------------------------*/

/**
 * @brief Test group for AiaBackoff tests.
 */
TEST_GROUP( AiaBackoffTests );

/*-----------------------------------------------------------*/

/**
 * @brief Test setup for AiaBackoff tests.
 */
TEST_SETUP( AiaBackoffTests )
{
    AiaRandomMbedtls_Init();
    AiaRandomMbedtls_Seed( NULL, 0 );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test tear down for AiaBackoff tests.
 */
TEST_TEAR_DOWN( AiaBackoffTests )
{
    AiaRandomMbedtls_Cleanup();
}

/*-----------------------------------------------------------*/

/**
 * @brief Test group runner for AiaBackoff tests.
 */
TEST_GROUP_RUNNER( AiaBackoffTests )
{
    RUN_TEST_CASE( AiaBackoffTests, ParamsZero );
    RUN_TEST_CASE( AiaBackoffTests, RetryNumZero );
    RUN_TEST_CASE( AiaBackoffTests, MaxBackoffZero );
    RUN_TEST_CASE( AiaBackoffTests, AllParams );
}

/*-----------------------------------------------------------*/

TEST( AiaBackoffTests, ParamsZero )
{
    TEST_ASSERT_EQUAL( 0, AiaBackoff_GetBackoffTimeMilliseconds( 0, 0 ) );
}

/*-----------------------------------------------------------*/

TEST( AiaBackoffTests, RetryNumZero )
{
    size_t retryNum = 1;
    TEST_ASSERT_EQUAL( 0,
                       AiaBackoff_GetBackoffTimeMilliseconds( retryNum, 0 ) );
}

/*-----------------------------------------------------------*/

TEST( AiaBackoffTests, MaxBackoffZero )
{
    AiaDurationMs_t maxBackoff = 1000;
    TEST_ASSERT_EQUAL( 0,
                       AiaBackoff_GetBackoffTimeMilliseconds( 0, maxBackoff ) );
}

/*-----------------------------------------------------------*/

TEST( AiaBackoffTests, AllParams )
{
    size_t retryNum = 5;
    AiaDurationMs_t maxBackoff = 100000;
    bool isNotZero = false;

    for( size_t i = 0; i < 10; i++ )
    {
        if( AiaBackoff_GetBackoffTimeMilliseconds( retryNum, maxBackoff ) != 0 )
        {
            isNotZero = true;
        }
    }
    TEST_ASSERT_TRUE( isNotZero );
}

/*-----------------------------------------------------------*/
