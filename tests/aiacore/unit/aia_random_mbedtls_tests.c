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
 * @file aia_random_mbedtls_tests.c
 * @brief Tests for AiaRandomMbedtls.
 */

/* The config header is always included first. */
#include <aia_config.h>

#include <aiacore/aia_mbedtls_threading.h>
#include <aiacore/aia_random_mbedtls.h>

/* Test framework includes. */
#include <unity_fixture.h>

/*-----------------------------------------------------------*/

/** Sample Seed. */
static const char* TEST_SALT = "TestSalt";
static const size_t TEST_SALT_LENGTH = 8;

/*-----------------------------------------------------------*/

/**
 * @brief Test group for AiaRandomMbedtls_t tests.
 */
TEST_GROUP( AiaRandomMbedtlsTests );

/*-----------------------------------------------------------*/

/**
 * @brief Test setup for AiaRandomMbedtls_t tests.
 */
TEST_SETUP( AiaRandomMbedtlsTests )
{
    AiaMbedtlsThreading_Init();
    AiaRandomMbedtls_Init();
}

/*-----------------------------------------------------------*/

/**
 * @brief Test tear down for AiaRandomMbedtls_t tests.
 */
TEST_TEAR_DOWN( AiaRandomMbedtlsTests )
{
    AiaMbedtlsThreading_Cleanup();
    AiaRandomMbedtls_Cleanup();
}

/*-----------------------------------------------------------*/

/**
 * @brief Test group runner for AiaRandomMbedtls_t tests.
 */
TEST_GROUP_RUNNER( AiaRandomMbedtlsTests )
{
    RUN_TEST_CASE( AiaRandomMbedtlsTests, SeedWithoutSeed );
    RUN_TEST_CASE( AiaRandomMbedtlsTests, SeedWithAll );
    RUN_TEST_CASE( AiaRandomMbedtlsTests, RandWithoutBuffer );
    RUN_TEST_CASE( AiaRandomMbedtlsTests, RandWithAll );
    RUN_TEST_CASE( AiaRandomMbedtlsTests, RandUnique );
}

/*-----------------------------------------------------------*/

TEST( AiaRandomMbedtlsTests, SeedWithoutSeed )
{
    TEST_ASSERT_TRUE( AiaRandomMbedtls_Seed( NULL, 0 ) );
}

/*-----------------------------------------------------------*/

TEST( AiaRandomMbedtlsTests, SeedWithAll )
{
    TEST_ASSERT_TRUE( AiaRandomMbedtls_Seed( TEST_SALT, TEST_SALT_LENGTH ) );
}

/*-----------------------------------------------------------*/

TEST( AiaRandomMbedtlsTests, RandWithoutBuffer )
{
    AiaRandomMbedtls_Seed( TEST_SALT, TEST_SALT_LENGTH );

    TEST_ASSERT_FALSE( AiaRandomMbedtls_Rand( NULL, 0 ) );
}

/*-----------------------------------------------------------*/

TEST( AiaRandomMbedtlsTests, RandWithAll )
{
    AiaRandomMbedtls_Seed( TEST_SALT, TEST_SALT_LENGTH );

    uint16_t randomNumber;
    TEST_ASSERT_TRUE( AiaRandomMbedtls_Rand( (unsigned char*)&randomNumber,
                                             sizeof( uint16_t ) ) );
}

/*-----------------------------------------------------------*/

TEST( AiaRandomMbedtlsTests, RandUnique )
{
    AiaRandomMbedtls_Seed( TEST_SALT, TEST_SALT_LENGTH );

    size_t genNum = 10;
    uint32_t randomNumbers[ genNum ];

    for( size_t i = 0; i < genNum; i++ )
    {
        uint32_t randomNumber;
        TEST_ASSERT_TRUE( AiaRandomMbedtls_Rand( (unsigned char*)&randomNumber,
                                                 sizeof( uint32_t ) ) );

        for( size_t j = 0; j < i; j++ )
        {
            TEST_ASSERT_TRUE( randomNumber != randomNumbers[ j ] );
        }
        randomNumbers[ i ] = randomNumber;
    }
}

/*-----------------------------------------------------------*/
