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
 * @file aia_exception_encountered_utils_tests.c
 * @brief Tests for aia_exception_encountered_utils.
 */

/* The config header is always included first. */
#include <aia_config.h>

#include <aiacore/aia_events.h>
#include <aiacore/aia_exception_encountered_utils.h>
#include <aiacore/aia_json_constants.h>
#include <aiacore/aia_json_message.h>
#include <aiacore/aia_json_utils.h>
#include <aiacore/aia_mbedtls_threading.h>
#include <aiacore/aia_message_constants.h>
#include <aiacore/aia_random_mbedtls.h>
#include <aiacore/aia_topic.h>

/* Test framework includes. */
#include <unity_fixture.h>

/*-----------------------------------------------------------*/

static const AiaSequenceNumber_t TEST_SEQUENCE_NUMBER = 42;

static const size_t TEST_INDEX = 4;

/** Sample Seed. */
static const char* TEST_SALT = "TestSalt";
static const size_t TEST_SALT_LENGTH = sizeof( TEST_SALT ) - 1;

/*-----------------------------------------------------------*/

/**
 * @brief Test group for aia_exception_encountered_utils tests.
 */
TEST_GROUP( AiaExceptionEncounteredUtilsTests );

/*-----------------------------------------------------------*/

/**
 * @brief Test setup for aia_exception_encountered_utils tests.
 */
TEST_SETUP( AiaExceptionEncounteredUtilsTests )
{
    AiaMbedtlsThreading_Init();
    AiaRandomMbedtls_Init();
    TEST_ASSERT_TRUE( AiaRandomMbedtls_Seed( TEST_SALT, TEST_SALT_LENGTH ) );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test tear down for aia_exception_encountered_utils tests.
 */
TEST_TEAR_DOWN( AiaExceptionEncounteredUtilsTests )
{
    AiaMbedtlsThreading_Cleanup();
    AiaRandomMbedtls_Cleanup();
}

/*-----------------------------------------------------------*/

/**
 * @brief Test group runner for aia_exception_encountered_utils tests.
 */
TEST_GROUP_RUNNER( AiaExceptionEncounteredUtilsTests )
{
    RUN_TEST_CASE( AiaExceptionEncounteredUtilsTests,
                   GenerateMalformedMessageExceptionEncounteredMessage );
    RUN_TEST_CASE( AiaExceptionEncounteredUtilsTests,
                   GenerateInternalErrorExceptionEncounteredMessage );
}

/*-----------------------------------------------------------*/

TEST( AiaExceptionEncounteredUtilsTests,
      GenerateMalformedMessageExceptionEncounteredMessage )
{
    AiaJsonMessage_t* jsonMessage =
        generateMalformedMessageExceptionEncounteredEvent(
            TEST_SEQUENCE_NUMBER, TEST_INDEX, AIA_TOPIC_SPEAKER );
    TEST_ASSERT_NOT_NULL( jsonMessage );

    const char* name = AiaJsonMessage_GetName( jsonMessage );
    TEST_ASSERT_NOT_NULL( name );
    TEST_ASSERT_EQUAL_STRING( name, AIA_EVENTS_EXCEPTION_ENCOUNTERED );

    const char* payload = AiaJsonMessage_GetJsonPayload( jsonMessage );
    TEST_ASSERT_NOT_NULL( payload );

    const char* error;
    size_t errorLength;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_EXCEPTION_ENCOUNTERED_ERROR_KEY,
        strlen( AIA_EXCEPTION_ENCOUNTERED_ERROR_KEY ), &error, &errorLength ) );
    TEST_ASSERT_NOT_NULL( error );
    TEST_ASSERT_GREATER_THAN( 0, errorLength );

    const char* code;
    size_t codeLength;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        error, strlen( error ), AIA_EXCEPTION_ENCOUNTERED_ERROR_CODE_KEY,
        strlen( AIA_EXCEPTION_ENCOUNTERED_ERROR_CODE_KEY ), &code,
        &codeLength ) );
    TEST_ASSERT_NOT_NULL( code );
    TEST_ASSERT_GREATER_THAN( 0, codeLength );
    TEST_ASSERT_TRUE( AiaJsonUtils_UnquoteString( &code, &codeLength ) );
    TEST_ASSERT_EQUAL_STRING_LEN(
        AIA_EXCEPTION_ENCOUNTERED_MALFORMED_MESSAGE_CODE, code, codeLength );

    const char* message;
    size_t messageLength;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_EXCEPTION_ENCOUNTERED_MESSAGE_KEY,
        strlen( AIA_EXCEPTION_ENCOUNTERED_MESSAGE_KEY ), &message,
        &messageLength ) );
    TEST_ASSERT_NOT_NULL( message );
    TEST_ASSERT_GREATER_THAN( 0, messageLength );

    const char* topicString;
    size_t topicStringLength;
    AiaTopic_t topic;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        message, strlen( message ), AIA_EXCEPTION_ENCOUNTERED_MESSAGE_TOPIC_KEY,
        strlen( AIA_EXCEPTION_ENCOUNTERED_MESSAGE_TOPIC_KEY ), &topicString,
        &topicStringLength ) );
    TEST_ASSERT_NOT_NULL( topicString );
    TEST_ASSERT_GREATER_THAN( 0, topicStringLength );
    TEST_ASSERT_TRUE(
        AiaJsonUtils_UnquoteString( &topicString, &topicStringLength ) );
    TEST_ASSERT_TRUE(
        AiaTopic_FromString( topicString, topicStringLength, &topic ) );
    TEST_ASSERT_EQUAL( AIA_TOPIC_SPEAKER, topic );

    const char* sequenceNumber;
    size_t sequenceNumberLength;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        message, strlen( message ),
        AIA_EXCEPTION_ENCOUNTERED_MESSAGE_SEQUENCE_NUMBER_KEY,
        strlen( AIA_EXCEPTION_ENCOUNTERED_MESSAGE_SEQUENCE_NUMBER_KEY ),
        &sequenceNumber, &sequenceNumberLength ) );
    TEST_ASSERT_NOT_NULL( sequenceNumber );
    TEST_ASSERT_GREATER_THAN( 0, sequenceNumberLength );
    AiaJsonLongType sequenceNumberLong;
    TEST_ASSERT_TRUE( AiaExtractLongFromJsonValue(
        sequenceNumber, sequenceNumberLength, &sequenceNumberLong ) );
    TEST_ASSERT_EQUAL( sequenceNumberLong, TEST_SEQUENCE_NUMBER );

    const char* index;
    size_t indexLength;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        message, strlen( message ), AIA_EXCEPTION_ENCOUNTERED_MESSAGE_INDEX_KEY,
        strlen( AIA_EXCEPTION_ENCOUNTERED_MESSAGE_INDEX_KEY ), &index,
        &indexLength ) );
    TEST_ASSERT_NOT_NULL( index );
    TEST_ASSERT_GREATER_THAN( 0, indexLength );
    AiaJsonLongType indexLong;
    TEST_ASSERT_TRUE(
        AiaExtractLongFromJsonValue( index, indexLength, &indexLong ) );
    TEST_ASSERT_EQUAL( indexLong, TEST_INDEX );
}

TEST( AiaExceptionEncounteredUtilsTests,
      GenerateInternalErrorExceptionEncounteredMessage )
{
    AiaJsonMessage_t* jsonMessage =
        generateInternalErrorExceptionEncounteredEvent();
    TEST_ASSERT_NOT_NULL( jsonMessage );

    const char* name = AiaJsonMessage_GetName( jsonMessage );
    TEST_ASSERT_NOT_NULL( name );
    TEST_ASSERT_EQUAL_STRING( name, AIA_EVENTS_EXCEPTION_ENCOUNTERED );

    const char* payload = AiaJsonMessage_GetJsonPayload( jsonMessage );
    TEST_ASSERT_NOT_NULL( payload );

    const char* error;
    size_t errorLength;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_EXCEPTION_ENCOUNTERED_ERROR_KEY,
        strlen( AIA_EXCEPTION_ENCOUNTERED_ERROR_KEY ), &error, &errorLength ) );
    TEST_ASSERT_NOT_NULL( error );
    TEST_ASSERT_GREATER_THAN( 0, errorLength );

    const char* code;
    size_t codeLength;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        error, strlen( error ), AIA_EXCEPTION_ENCOUNTERED_ERROR_CODE_KEY,
        strlen( AIA_EXCEPTION_ENCOUNTERED_ERROR_CODE_KEY ), &code,
        &codeLength ) );
    TEST_ASSERT_NOT_NULL( code );
    TEST_ASSERT_GREATER_THAN( 0, codeLength );
    TEST_ASSERT_TRUE( AiaJsonUtils_UnquoteString( &code, &codeLength ) );
    TEST_ASSERT_EQUAL_STRING_LEN( AIA_EXCEPTION_ENCOUNTERED_INTERNAL_ERROR_CODE,
                                  code, codeLength );

    const char* message;
    size_t messageLength;
    TEST_ASSERT_FALSE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_EXCEPTION_ENCOUNTERED_MESSAGE_KEY,
        strlen( AIA_EXCEPTION_ENCOUNTERED_MESSAGE_KEY ), &message,
        &messageLength ) );
}
