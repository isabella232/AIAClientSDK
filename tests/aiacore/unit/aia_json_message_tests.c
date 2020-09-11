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
 * @file aia_json_message_tests.c
 * @brief Tests for AiaJsonMessage_t.
 */

/* The config header is always included first. */
#include <aia_config.h>

/* AiaJsonMessage_t headers */
#include <aiacore/aia_json_constants.h>
#include <aiacore/aia_json_message.h>
#include <aiacore/aia_mbedtls_threading.h>
#include <aiacore/aia_message.h>
#include <aiacore/aia_random_mbedtls.h>

#include <aiacore/aia_json_utils.h>

/* Test framework includes. */
#include <unity_fixture.h>

/* Standard library includes. */
#include <string.h>

/*-----------------------------------------------------------*/

/** Sample JSON message name. */
static const char* TEST_NAME = "TestMessageName";

/** Sample JSON message ID. */
static const char* TEST_MESSAGE_ID = "TestMessageId";

/** Sample JSON message payload key. */
#define TEST_PAYLOAD_KEY "TestMessagePayloadKey"

/** Sample JSON message payload value. */
#define TEST_PAYLOAD_VALUE "TestMessagePayloadValue"

/** Sample JSON message payload object. */
static const char* TEST_PAYLOAD =
    "{\"" TEST_PAYLOAD_KEY "\":\"" TEST_PAYLOAD_VALUE "\"}";

/** Sample Seed. */
static const char* TEST_SALT = "TestSalt";
static const size_t TEST_SALT_LENGTH = sizeof( TEST_SALT ) - 1;

/*-----------------------------------------------------------*/

/**
 * Utility function which validates that @c messageBuffer contains the expected
 * JSON content.
 *
 * @param messageBuffer A string containing the JSON text to validate.
 * @param withPayload A flag indicating whether @c messageBuffer is expected to
 * contain a payload section.
 */
static void validateJsonMessage( const char* messageBuffer, bool withPayload )
{
    const char* header;
    size_t headerLength;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        messageBuffer, strlen( messageBuffer ), AIA_JSON_CONSTANTS_HEADER_KEY,
        strlen( AIA_JSON_CONSTANTS_HEADER_KEY ), &header, &headerLength ) );

    const char* name;
    size_t nameLength;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        header, headerLength, AIA_JSON_CONSTANTS_NAME_KEY,
        strlen( AIA_JSON_CONSTANTS_NAME_KEY ), &name, &nameLength ) );
    TEST_ASSERT_TRUE( AiaJsonUtils_UnquoteString( &name, &nameLength ) );
    TEST_ASSERT_EQUAL_STRING_LEN( TEST_NAME, name, nameLength );

    const char* messageId;
    size_t messageIdLength;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        header, headerLength, AIA_JSON_CONSTANTS_MESSAGE_ID_KEY,
        strlen( AIA_JSON_CONSTANTS_MESSAGE_ID_KEY ), &messageId,
        &messageIdLength ) );
    TEST_ASSERT_TRUE(
        AiaJsonUtils_UnquoteString( &messageId, &messageIdLength ) );
    TEST_ASSERT_EQUAL_STRING_LEN( TEST_MESSAGE_ID, messageId, messageIdLength );

    const char* payload;
    size_t payloadLength;
    TEST_ASSERT_EQUAL(
        AiaFindJsonValue( messageBuffer, strlen( messageBuffer ),
                          AIA_JSON_CONSTANTS_PAYLOAD_KEY,
                          strlen( AIA_JSON_CONSTANTS_PAYLOAD_KEY ), &payload,
                          &payloadLength ),
        withPayload );

    if( !withPayload )
    {
        return;
    }

    const char* testPayloadValue;
    size_t testPayloadValueLength;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, payloadLength, TEST_PAYLOAD_KEY, strlen( TEST_PAYLOAD_KEY ),
        &testPayloadValue, &testPayloadValueLength ) );
    TEST_ASSERT_TRUE( AiaJsonUtils_UnquoteString( &testPayloadValue,
                                                  &testPayloadValueLength ) );
    TEST_ASSERT_EQUAL_STRING_LEN( TEST_PAYLOAD_VALUE, testPayloadValue,
                                  testPayloadValueLength );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test group for AiaJsonMessage_t tests.
 */
TEST_GROUP( AiaJsonMessageTests );

/*-----------------------------------------------------------*/

/**
 * @brief Test setup for AiaJsonMessage_t tests.
 */
TEST_SETUP( AiaJsonMessageTests )
{
    AiaMbedtlsThreading_Init();
    AiaRandomMbedtls_Init();
    TEST_ASSERT_TRUE( AiaRandomMbedtls_Seed( TEST_SALT, TEST_SALT_LENGTH ) );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test tear down for AiaJsonMessage_t tests.
 */
TEST_TEAR_DOWN( AiaJsonMessageTests )
{
    AiaMbedtlsThreading_Cleanup();
    AiaRandomMbedtls_Cleanup();
}

/*-----------------------------------------------------------*/

/**
 * @brief Test group runner for AiaJsonMessage_t tests.
 */
TEST_GROUP_RUNNER( AiaJsonMessageTests )
{
    RUN_TEST_CASE( AiaJsonMessageTests, CreateWithoutName );
    RUN_TEST_CASE( AiaJsonMessageTests, CreateWithoutMessageId );
    RUN_TEST_CASE( AiaJsonMessageTests, CreateWithoutPayload );
    RUN_TEST_CASE( AiaJsonMessageTests, CreateWithAllAndDestroy );
    RUN_TEST_CASE( AiaJsonMessageTests, DestroyWithoutMessage );
    RUN_TEST_CASE( AiaJsonMessageTests, GetName );
    RUN_TEST_CASE( AiaJsonMessageTests, GetMessageId );
    RUN_TEST_CASE( AiaJsonMessageTests, GetJsonPayloadWithPayload );
    RUN_TEST_CASE( AiaJsonMessageTests, GetJsonPayloadWithoutPayload );
    RUN_TEST_CASE( AiaJsonMessageTests, BuildMessageWithPayload );
    RUN_TEST_CASE( AiaJsonMessageTests, BuildMessageWithoutPayload );
    RUN_TEST_CASE( AiaJsonMessageTests, BuildMessageWithExactBufferSize );
    RUN_TEST_CASE( AiaJsonMessageTests, BuildMessageWithInsufficientBuffer );
    RUN_TEST_CASE( AiaJsonMessageTests, BuildMessageWithoutBuffer );
}

/*-----------------------------------------------------------*/

TEST( AiaJsonMessageTests, CreateWithoutName )
{
    TEST_ASSERT_EQUAL(
        AiaJsonMessage_Create( NULL, TEST_MESSAGE_ID, TEST_PAYLOAD ), NULL );
}

/*-----------------------------------------------------------*/

TEST( AiaJsonMessageTests, CreateWithoutMessageId )
{
    TEST_ASSERT_NOT_NULL(
        AiaJsonMessage_Create( TEST_NAME, NULL, TEST_PAYLOAD ) );
}

/*-----------------------------------------------------------*/

TEST( AiaJsonMessageTests, CreateWithoutPayload )
{
    AiaJsonMessage_t* jsonMessage =
        AiaJsonMessage_Create( TEST_NAME, TEST_MESSAGE_ID, NULL );
    TEST_ASSERT_NOT_EQUAL( jsonMessage, NULL );
    AiaJsonMessage_Destroy( jsonMessage );
}

/*-----------------------------------------------------------*/

TEST( AiaJsonMessageTests, CreateWithAllAndDestroy )
{
    AiaJsonMessage_t* jsonMessage =
        AiaJsonMessage_Create( TEST_NAME, TEST_MESSAGE_ID, TEST_PAYLOAD );
    TEST_ASSERT_NOT_EQUAL( jsonMessage, NULL );
    AiaJsonMessage_Destroy( jsonMessage );
}

/*-----------------------------------------------------------*/

TEST( AiaJsonMessageTests, DestroyWithoutMessage )
{
    AiaJsonMessage_Destroy( NULL );
}

/*-----------------------------------------------------------*/

TEST( AiaJsonMessageTests, GetName )
{
    AiaJsonMessage_t* jsonMessage =
        AiaJsonMessage_Create( TEST_NAME, TEST_MESSAGE_ID, TEST_PAYLOAD );
    TEST_ASSERT_EQUAL_STRING( AiaJsonMessage_GetName( jsonMessage ),
                              TEST_NAME );
    AiaJsonMessage_Destroy( jsonMessage );
}

/*-----------------------------------------------------------*/

TEST( AiaJsonMessageTests, GetMessageId )
{
    AiaJsonMessage_t* jsonMessage =
        AiaJsonMessage_Create( TEST_NAME, TEST_MESSAGE_ID, TEST_PAYLOAD );
    TEST_ASSERT_EQUAL_STRING( AiaJsonMessage_GetMessageId( jsonMessage ),
                              TEST_MESSAGE_ID );
    AiaJsonMessage_Destroy( jsonMessage );
}

/*-----------------------------------------------------------*/

TEST( AiaJsonMessageTests, GetJsonPayloadWithPayload )
{
    AiaJsonMessage_t* jsonMessage =
        AiaJsonMessage_Create( TEST_NAME, TEST_MESSAGE_ID, TEST_PAYLOAD );
    TEST_ASSERT_EQUAL_STRING( AiaJsonMessage_GetJsonPayload( jsonMessage ),
                              TEST_PAYLOAD );
    AiaJsonMessage_Destroy( jsonMessage );
}

/*-----------------------------------------------------------*/

TEST( AiaJsonMessageTests, GetJsonPayloadWithoutPayload )
{
    AiaJsonMessage_t* jsonMessage =
        AiaJsonMessage_Create( TEST_NAME, TEST_MESSAGE_ID, NULL );
    TEST_ASSERT_EQUAL( AiaJsonMessage_GetJsonPayload( jsonMessage ), NULL );
    AiaJsonMessage_Destroy( jsonMessage );
}

/*-----------------------------------------------------------*/

TEST( AiaJsonMessageTests, BuildMessageWithPayload )
{
    AiaJsonMessage_t* jsonMessage =
        AiaJsonMessage_Create( TEST_NAME, TEST_MESSAGE_ID, TEST_PAYLOAD );
    char messageBuffer[ 1024 ];
    TEST_ASSERT_TRUE( AiaJsonMessage_BuildMessage( jsonMessage, messageBuffer,
                                                   sizeof( messageBuffer ) ) );

    validateJsonMessage( messageBuffer, true );

    AiaJsonMessage_Destroy( jsonMessage );
}

/*-----------------------------------------------------------*/

TEST( AiaJsonMessageTests, BuildMessageWithoutPayload )
{
    AiaJsonMessage_t* jsonMessage =
        AiaJsonMessage_Create( TEST_NAME, TEST_MESSAGE_ID, NULL );
    char messageBuffer[ 1024 ];
    TEST_ASSERT_TRUE( AiaJsonMessage_BuildMessage( jsonMessage, messageBuffer,
                                                   sizeof( messageBuffer ) ) );

    validateJsonMessage( messageBuffer, false );

    AiaJsonMessage_Destroy( jsonMessage );
}

/*-----------------------------------------------------------*/

TEST( AiaJsonMessageTests, BuildMessageWithExactBufferSize )
{
    AiaJsonMessage_t* jsonMessage =
        AiaJsonMessage_Create( TEST_NAME, TEST_MESSAGE_ID, TEST_PAYLOAD );
    size_t bufferSize =
        AiaMessage_GetSize( AiaJsonMessage_ToConstMessage( jsonMessage ) );
    TEST_ASSERT_NOT_EQUAL( bufferSize, 0 );
    char* messageBuffer = (char*)AiaCalloc( 1, bufferSize );
    TEST_ASSERT_TRUE(
        AiaJsonMessage_BuildMessage( jsonMessage, messageBuffer, bufferSize ) );

    validateJsonMessage( messageBuffer, true );

    AiaFree( messageBuffer );
    AiaJsonMessage_Destroy( jsonMessage );
}

/*-----------------------------------------------------------*/

TEST( AiaJsonMessageTests, BuildMessageWithInsufficientBuffer )
{
    AiaJsonMessage_t* jsonMessage =
        AiaJsonMessage_Create( TEST_NAME, TEST_MESSAGE_ID, TEST_PAYLOAD );
    size_t bufferSize =
        AiaMessage_GetSize( AiaJsonMessage_ToConstMessage( jsonMessage ) );
    TEST_ASSERT_NOT_EQUAL( bufferSize, 0 );
    char* messageBuffer = (char*)AiaCalloc( 1, bufferSize );
    TEST_ASSERT_FALSE( AiaJsonMessage_BuildMessage( jsonMessage, messageBuffer,
                                                    bufferSize - 1 ) );

    AiaFree( messageBuffer );
    AiaJsonMessage_Destroy( jsonMessage );
}

/*-----------------------------------------------------------*/

TEST( AiaJsonMessageTests, BuildMessageWithoutBuffer )
{
    AiaJsonMessage_t* jsonMessage =
        AiaJsonMessage_Create( TEST_NAME, TEST_MESSAGE_ID, TEST_PAYLOAD );
    TEST_ASSERT_FALSE( AiaJsonMessage_BuildMessage( jsonMessage, NULL, 0 ) );

    AiaJsonMessage_Destroy( jsonMessage );
}

/*-----------------------------------------------------------*/
