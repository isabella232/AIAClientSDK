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
 * @file aia_binary_message_tests.c
 * @brief Tests for AiaBinaryMessage_t.
 */

/* The config header is always included first. */
#include <aia_config.h>

/* AiaBinaryMessage_t headers */
#include <aiacore/aia_binary_constants.h>
#include <aiacore/aia_binary_message.h>
#include <aiacore/aia_message.h>

/* Test framework includes. */
#include <unity_fixture.h>

/*-----------------------------------------------------------*/

static const AiaBinaryMessageLength_t TEST_LENGTH = 4;

static const AiaBinaryMessageType_t TEST_TYPE = 1;

static const AiaBinaryMessageCount_t TEST_COUNT = 42;

static uint8_t* TEST_DATA;

/*-----------------------------------------------------------*/

/**
 * Utility function which validates that @c messageBuffer contains the expected
 * binary message content.
 *
 * @param messageBuffer A buffer containing the binary blob to validate.
 * @param messageLen Size of @c messageBuffer.
 * @param expectedLength The expected length field.
 * @param expectedType The expected type field.
 * @param expectedCount The expected count field.
 * @param expectedData The expected data in the message.
 */
static void validateBinaryMessage( const uint8_t* messageBuffer,
                                   size_t messageLen,
                                   AiaBinaryMessageLength_t expectedLength,
                                   AiaBinaryMessageType_t expectedType,
                                   AiaBinaryMessageCount_t expectedCount,
                                   const uint8_t* expectedData )
{
    TEST_ASSERT_NOT_NULL( messageBuffer );
    TEST_ASSERT_NOT_EQUAL( messageLen, 0 );
    size_t bytePosition = 0;

    AiaBinaryMessageLength_t parsedLength = 0;
    for( size_t i = 0; i < sizeof( AiaBinaryMessageLength_t );
         ++i, ++bytePosition )
    {
        parsedLength |= messageBuffer[ bytePosition ] << ( i * 8 );
    }
    TEST_ASSERT_EQUAL( parsedLength, expectedLength );

    AiaBinaryMessageType_t parsedType = 0;
    for( size_t i = 0; i < sizeof( AiaBinaryMessageType_t );
         ++i, ++bytePosition )
    {
        parsedType |= messageBuffer[ bytePosition ] << ( i * 8 );
    }
    TEST_ASSERT_EQUAL( parsedType, expectedType );

    AiaBinaryMessageCount_t parsedCount = 0;
    for( size_t i = 0; i < sizeof( AiaBinaryMessageCount_t );
         ++i, ++bytePosition )
    {
        parsedCount |= messageBuffer[ bytePosition ] << ( i * 8 );
    }
    TEST_ASSERT_EQUAL( parsedCount, expectedCount );

    for( size_t i = AIA_BINARY_MESSAGE_NUM_RESERVED_BYTES; i > 0;
         i--, bytePosition++ )
    {
    }

    const uint8_t* data = messageBuffer + bytePosition;

    for( size_t i = 0; i < parsedLength; ++i )
    {
        TEST_ASSERT_EQUAL( expectedData[ i ], data[ i ] );
    }
}

/*-----------------------------------------------------------*/

/**
 * @brief Test group for AiaBinaryMessage_t tests.
 */
TEST_GROUP( AiaBinaryMessageTests );

/*-----------------------------------------------------------*/

/**
 * @brief Test setup for AiaBinaryMessage_t tests.
 */
TEST_SETUP( AiaBinaryMessageTests )
{
    TEST_DATA = AiaCalloc( 1, TEST_LENGTH );
    for( size_t i = 0; i < TEST_LENGTH; ++i )
    {
        TEST_DATA[ i ] = i;
    }
}

/*-----------------------------------------------------------*/

/**
 * @brief Test tear down for AiaBinaryMessage_t tests.
 */
TEST_TEAR_DOWN( AiaBinaryMessageTests )
{
}

/*-----------------------------------------------------------*/

/**
 * @brief Test group runner for AiaBinaryMessage_t tests.
 */
TEST_GROUP_RUNNER( AiaBinaryMessageTests )
{
    RUN_TEST_CASE( AiaBinaryMessageTests, CreateWithInvalidParameters );
    RUN_TEST_CASE( AiaBinaryMessageTests, ValidCreate );
    RUN_TEST_CASE( AiaBinaryMessageTests, GetLength );
    RUN_TEST_CASE( AiaBinaryMessageTests, GetType );
    RUN_TEST_CASE( AiaBinaryMessageTests, GetCount );
    RUN_TEST_CASE( AiaBinaryMessageTests, GetData );
    RUN_TEST_CASE( AiaBinaryMessageTests, BuildMessageNullBuffer );
    RUN_TEST_CASE( AiaBinaryMessageTests, BuildMessageWithInsufficientBuffer );
    RUN_TEST_CASE( AiaBinaryMessageTests, BuildMessage );
}

/*-----------------------------------------------------------*/

TEST( AiaBinaryMessageTests, CreateWithInvalidParameters )
{
    TEST_ASSERT_NULL(
        AiaBinaryMessage_Create( TEST_LENGTH, TEST_TYPE, TEST_COUNT, NULL ) );
    TEST_ASSERT_NULL(
        AiaBinaryMessage_Create( 0, TEST_TYPE, TEST_COUNT, (void*)TEST_DATA ) );
    AiaFree( TEST_DATA );
}

TEST( AiaBinaryMessageTests, ValidCreate )
{
    AiaBinaryMessage_t* binaryMessage = AiaBinaryMessage_Create(
        TEST_LENGTH, TEST_TYPE, TEST_COUNT, (void*)TEST_DATA );
    TEST_ASSERT_NOT_NULL( binaryMessage );
    AiaBinaryMessage_Destroy( binaryMessage );
}

TEST( AiaBinaryMessageTests, GetLength )
{
    AiaBinaryMessage_t* binaryMessage = AiaBinaryMessage_Create(
        TEST_LENGTH, TEST_TYPE, TEST_COUNT, (void*)TEST_DATA );
    TEST_ASSERT_NOT_NULL( binaryMessage );
    TEST_ASSERT_EQUAL( AiaBinaryMessage_GetLength( binaryMessage ),
                       TEST_LENGTH );
    AiaBinaryMessage_Destroy( binaryMessage );
}

TEST( AiaBinaryMessageTests, GetType )
{
    AiaBinaryMessage_t* binaryMessage = AiaBinaryMessage_Create(
        TEST_LENGTH, TEST_TYPE, TEST_COUNT, (void*)TEST_DATA );
    TEST_ASSERT_NOT_NULL( binaryMessage );
    TEST_ASSERT_EQUAL( AiaBinaryMessage_GetType( binaryMessage ), TEST_TYPE );
    AiaBinaryMessage_Destroy( binaryMessage );
}

TEST( AiaBinaryMessageTests, GetCount )
{
    AiaBinaryMessage_t* binaryMessage = AiaBinaryMessage_Create(
        TEST_LENGTH, TEST_TYPE, TEST_COUNT, (void*)TEST_DATA );
    TEST_ASSERT_NOT_NULL( binaryMessage );
    TEST_ASSERT_EQUAL( AiaBinaryMessage_GetCount( binaryMessage ), TEST_COUNT );
    AiaBinaryMessage_Destroy( binaryMessage );
}

TEST( AiaBinaryMessageTests, GetData )
{
    AiaBinaryMessage_t* binaryMessage = AiaBinaryMessage_Create(
        TEST_LENGTH, TEST_TYPE, TEST_COUNT, (void*)TEST_DATA );
    TEST_ASSERT_NOT_NULL( binaryMessage );
    TEST_ASSERT_EQUAL( AiaBinaryMessage_GetData( binaryMessage ), TEST_DATA );
    for( size_t i = 0; i < TEST_LENGTH; ++i )
    {
        TEST_ASSERT_EQUAL(
            TEST_DATA[ i ],
            ( (uint8_t*)AiaBinaryMessage_GetData( binaryMessage ) )[ i ] );
    }
    AiaBinaryMessage_Destroy( binaryMessage );
}

TEST( AiaBinaryMessageTests, BuildMessageNullBuffer )
{
    AiaBinaryMessage_t* binaryMessage = AiaBinaryMessage_Create(
        TEST_LENGTH, TEST_TYPE, TEST_COUNT, (void*)TEST_DATA );
    TEST_ASSERT_NOT_NULL( binaryMessage );

    size_t bufferSize =
        AiaMessage_GetSize( AiaBinaryMessage_ToConstMessage( binaryMessage ) );
    TEST_ASSERT_NOT_EQUAL( bufferSize, 0 );
    TEST_ASSERT_FALSE(
        AiaBinaryMessage_BuildMessage( binaryMessage, NULL, bufferSize ) );

    AiaBinaryMessage_Destroy( binaryMessage );
}

TEST( AiaBinaryMessageTests, BuildMessageWithInsufficientBuffer )
{
    AiaBinaryMessage_t* binaryMessage = AiaBinaryMessage_Create(
        TEST_LENGTH, TEST_TYPE, TEST_COUNT, (void*)TEST_DATA );
    TEST_ASSERT_NOT_NULL( binaryMessage );

    size_t bufferSize =
        AiaMessage_GetSize( AiaBinaryMessage_ToConstMessage( binaryMessage ) );
    TEST_ASSERT_NOT_EQUAL( bufferSize, 0 );
    uint8_t* messageBuffer = AiaCalloc( 1, bufferSize );
    TEST_ASSERT_NOT_NULL( messageBuffer );
    TEST_ASSERT_FALSE( AiaBinaryMessage_BuildMessage(
        binaryMessage, messageBuffer, bufferSize - 1 ) );

    AiaFree( messageBuffer );
    AiaBinaryMessage_Destroy( binaryMessage );
}

TEST( AiaBinaryMessageTests, BuildMessage )
{
    AiaBinaryMessage_t* binaryMessage = AiaBinaryMessage_Create(
        TEST_LENGTH, TEST_TYPE, TEST_COUNT, (void*)TEST_DATA );
    TEST_ASSERT_NOT_NULL( binaryMessage );

    size_t bufferSize =
        AiaMessage_GetSize( AiaBinaryMessage_ToConstMessage( binaryMessage ) );
    TEST_ASSERT_NOT_EQUAL( bufferSize, 0 );
    uint8_t* messageBuffer = AiaCalloc( 1, bufferSize );
    TEST_ASSERT_NOT_NULL( messageBuffer );
    TEST_ASSERT_TRUE( AiaBinaryMessage_BuildMessage(
        binaryMessage, messageBuffer, bufferSize ) );

    validateBinaryMessage( messageBuffer, bufferSize, TEST_LENGTH, TEST_TYPE,
                           TEST_COUNT, TEST_DATA );

    AiaFree( messageBuffer );
    AiaBinaryMessage_Destroy( binaryMessage );
}
