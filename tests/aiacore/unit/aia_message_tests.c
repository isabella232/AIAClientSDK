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
 * @file aia_message_tests.c
 * @brief Tests for AiaMessage_t.
 */

/* The config header is always included first. */
#include <aia_config.h>

/* AiaMessage_t headers */
#include <aiacore/aia_message.h>
#include <aiacore/private/aia_message.h>

/* Test framework includes. */
#include <unity_fixture.h>

/* Standard library includes. */
#include <string.h>

/*-----------------------------------------------------------*/

/**
 * @brief Test group for AiaMessage_t tests.
 */
TEST_GROUP( AiaMessageTests );

/*-----------------------------------------------------------*/

/**
 * @brief Test setup for AiaMessage_t tests.
 */
TEST_SETUP( AiaMessageTests )
{
}

/*-----------------------------------------------------------*/

/**
 * @brief Test tear down for AiaMessage_t tests.
 */
TEST_TEAR_DOWN( AiaMessageTests )
{
}

/*-----------------------------------------------------------*/

/**
 * @brief Test group runner for AiaMessage_t tests.
 */
TEST_GROUP_RUNNER( AiaMessageTests )
{
    RUN_TEST_CASE( AiaMessageTests, InitializeWithoutMessage );
    RUN_TEST_CASE( AiaMessageTests, UninitializeWithoutMessage );
    RUN_TEST_CASE( AiaMessageTests,
                   InitializeGetSizeAndUninitializeWithMessage );
}

/*-----------------------------------------------------------*/

TEST( AiaMessageTests, InitializeWithoutMessage )
{
    TEST_ASSERT_FALSE( _AiaMessage_Initialize( NULL, 0 ) );
}

/*-----------------------------------------------------------*/

TEST( AiaMessageTests, UninitializeWithoutMessage )
{
    _AiaMessage_Uninitialize( NULL );
}

/*-----------------------------------------------------------*/

TEST( AiaMessageTests, InitializeGetSizeAndUninitializeWithMessage )
{
    struct AiaMessage message;
    memset( &message, 0, sizeof( message ) );
    static const size_t TEST_SIZE = 1234;
    TEST_ASSERT_TRUE( _AiaMessage_Initialize( &message, TEST_SIZE ) );
    TEST_ASSERT_EQUAL( AiaMessage_GetSize( &message ), TEST_SIZE );
    _AiaMessage_Uninitialize( &message );
}

/*-----------------------------------------------------------*/
