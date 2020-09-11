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
 * @file aia_json_utils_tests.c
 * @brief Tests for AiaJsonUtils.
 */

/* The config header is always included first. */
#include <aia_config.h>

#include <aiacore/aia_json_utils.h>

/* Test framework includes. */
#include <unity_fixture.h>

/* Standard library includes. */
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

/*-----------------------------------------------------------*/

static AiaJsonLongType TEST_LONG = 44;
static char TEST_STRING[ 100 ];

/*-----------------------------------------------------------*/

/**
 * @brief Test group for AiaJsonUtilsTests tests.
 */
TEST_GROUP( AiaJsonUtilsTests );

/*-----------------------------------------------------------*/

/**
 * @brief Test setup for AiaJsonUtilsTests tests.
 */
TEST_SETUP( AiaJsonUtilsTests )
{
    int numCharsForCharBuffer = snprintf( NULL, 0, "%" PRIu64, TEST_LONG );
    TEST_ASSERT_GREATER_THAN( 0, numCharsForCharBuffer );
    TEST_ASSERT_GREATER_THAN(
        0, snprintf( TEST_STRING, numCharsForCharBuffer + 1, "%" PRIu64,
                     TEST_LONG ) );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test tear down for AiaJsonUtilsTests tests.
 */
TEST_TEAR_DOWN( AiaJsonUtilsTests )
{
}

/*-----------------------------------------------------------*/

/**
 * @brief Test group runner for AiaJsonMessage_t tests.
 */
TEST_GROUP_RUNNER( AiaJsonUtilsTests )
{
    RUN_TEST_CASE( AiaJsonUtilsTests, LongExtractionNullInputs );
    RUN_TEST_CASE( AiaJsonUtilsTests, HappyCase );
    RUN_TEST_CASE( AiaJsonUtilsTests, InvalidString );
    RUN_TEST_CASE( AiaJsonUtilsTests, GetArrayElementWithNullJsonArray );
    RUN_TEST_CASE( AiaJsonUtilsTests, GetArrayElementWithZeroJsonArrayLength );
    RUN_TEST_CASE( AiaJsonUtilsTests, GetArrayElementWithNullJsonValue );
    RUN_TEST_CASE( AiaJsonUtilsTests, GetArrayElementWithNullJsonValueLength );
    RUN_TEST_CASE( AiaJsonUtilsTests, GetArrayElementAllElementsInSimpleArray );
    RUN_TEST_CASE( AiaJsonUtilsTests, GetArrayElementIndexTooLarge );
    RUN_TEST_CASE( AiaJsonUtilsTests, GetArrayElementWhitespaceBefore );
    RUN_TEST_CASE( AiaJsonUtilsTests, GetArrayElementWhitespaceAfter );
    RUN_TEST_CASE( AiaJsonUtilsTests, GetArrayElementAfterNestedArray );
    RUN_TEST_CASE( AiaJsonUtilsTests, GetArrayElementAfterNestedObject );
    RUN_TEST_CASE( AiaJsonUtilsTests, GetArrayElementObject );
    RUN_TEST_CASE( AiaJsonUtilsTests, GetArrayElementInvalidArray );
    RUN_TEST_CASE( AiaJsonUtilsTests, ExtractLong );
    RUN_TEST_CASE( AiaJsonUtilsTests, ExtractLongWithInvalidLong );
    RUN_TEST_CASE( AiaJsonUtilsTests, ExtractLongWithNullArgs );
}

/*-----------------------------------------------------------*/

TEST( AiaJsonUtilsTests, LongExtractionNullInputs )
{
    AiaJsonLongType out;
    TEST_ASSERT_FALSE(
        AiaExtractLongFromJsonValue( NULL, strlen( TEST_STRING ), &out ) );
    TEST_ASSERT_FALSE( AiaExtractLongFromJsonValue(
        TEST_STRING, strlen( TEST_STRING ), NULL ) );
}

/*-----------------------------------------------------------*/

TEST( AiaJsonUtilsTests, HappyCase )
{
    AiaJsonLongType out;
    TEST_ASSERT_TRUE( AiaExtractLongFromJsonValue(
        TEST_STRING, strlen( TEST_STRING ), &out ) );
    TEST_ASSERT_EQUAL( TEST_LONG, out );
}

/*-----------------------------------------------------------*/

TEST( AiaJsonUtilsTests, InvalidString )
{
    AiaJsonLongType out;
    char INVALID_STRING[ 100 ] = "hello_world";
    TEST_ASSERT_FALSE( AiaExtractLongFromJsonValue(
        INVALID_STRING, strlen( INVALID_STRING ), &out ) );
    TEST_ASSERT_NOT_EQUAL( TEST_LONG, out );
}

/*-----------------------------------------------------------*/

TEST( AiaJsonUtilsTests, GetArrayElementWithNullJsonArray )
{
    const char* jsonValue;
    size_t jsonValueLength;
    TEST_ASSERT_FALSE( AiaJsonUtils_GetArrayElement( NULL, 1, 0, &jsonValue,
                                                     &jsonValueLength ) );
}

/*-----------------------------------------------------------*/

TEST( AiaJsonUtilsTests, GetArrayElementWithZeroJsonArrayLength )
{
    const char jsonArray[] = "";
    const char* jsonValue;
    size_t jsonValueLength;
    TEST_ASSERT_FALSE( AiaJsonUtils_GetArrayElement(
        jsonArray, 0, 0, &jsonValue, &jsonValueLength ) );
}

/*-----------------------------------------------------------*/

TEST( AiaJsonUtilsTests, GetArrayElementWithNullJsonValue )
{
    const char jsonArray[] = "[]";
    size_t jsonValueLength;
    TEST_ASSERT_FALSE( AiaJsonUtils_GetArrayElement(
        jsonArray, sizeof( jsonArray ) - 1, 0, NULL, &jsonValueLength ) );
}

/*-----------------------------------------------------------*/

TEST( AiaJsonUtilsTests, GetArrayElementWithNullJsonValueLength )
{
    const char jsonArray[] = "[]";
    const char* jsonValue;
    TEST_ASSERT_FALSE( AiaJsonUtils_GetArrayElement(
        jsonArray, sizeof( jsonArray ) - 1, 0, &jsonValue, NULL ) );
}

/*-----------------------------------------------------------*/

TEST( AiaJsonUtilsTests, GetArrayElementAllElementsInSimpleArray )
{
    const char jsonArray[] = "[a,b,c,d]";
    const char* jsonValue;
    size_t jsonValueLength;
    for( size_t index = 0; index < 4; ++index )
    {
        TEST_ASSERT_TRUE( AiaJsonUtils_GetArrayElement(
            jsonArray, sizeof( jsonArray ) - 1, index, &jsonValue,
            &jsonValueLength ) );
        TEST_ASSERT_EQUAL( 1, jsonValueLength );
        TEST_ASSERT_EQUAL( jsonArray[ index * 2 + 1 ], *jsonValue );
    }
}

/*-----------------------------------------------------------*/

TEST( AiaJsonUtilsTests, GetArrayElementIndexTooLarge )
{
    const char jsonArray[] = "[a,b,c,d]";
    const char* jsonValue;
    size_t jsonValueLength;
    size_t index = 5;
    TEST_ASSERT_FALSE(
        AiaJsonUtils_GetArrayElement( jsonArray, sizeof( jsonArray ) - 1, index,
                                      &jsonValue, &jsonValueLength ) );
}

/*-----------------------------------------------------------*/

TEST( AiaJsonUtilsTests, GetArrayElementWhitespaceBefore )
{
    const char jsonArray[] = "[a,b, c,d]";
    const char* jsonValue;
    size_t jsonValueLength;
    size_t index = 2;
    TEST_ASSERT_TRUE(
        AiaJsonUtils_GetArrayElement( jsonArray, sizeof( jsonArray ) - 1, index,
                                      &jsonValue, &jsonValueLength ) );
    TEST_ASSERT_EQUAL( 1, jsonValueLength );
    TEST_ASSERT_EQUAL( 'c', *jsonValue );
}

/*-----------------------------------------------------------*/

TEST( AiaJsonUtilsTests, GetArrayElementWhitespaceAfter )
{
    const char jsonArray[] = "[a,b,c ,d]";
    const char* jsonValue;
    size_t jsonValueLength;
    size_t index = 2;
    TEST_ASSERT_TRUE(
        AiaJsonUtils_GetArrayElement( jsonArray, sizeof( jsonArray ) - 1, index,
                                      &jsonValue, &jsonValueLength ) );
    TEST_ASSERT_EQUAL( 1, jsonValueLength );
    TEST_ASSERT_EQUAL( 'c', *jsonValue );
}

/*-----------------------------------------------------------*/

TEST( AiaJsonUtilsTests, GetArrayElementAfterNestedArray )
{
    const char jsonArray[] = "[a,[b,b],c,d]";
    const char* jsonValue;
    size_t jsonValueLength;
    size_t index = 2;
    TEST_ASSERT_TRUE(
        AiaJsonUtils_GetArrayElement( jsonArray, sizeof( jsonArray ) - 1, index,
                                      &jsonValue, &jsonValueLength ) );
    TEST_ASSERT_EQUAL( 1, jsonValueLength );
    TEST_ASSERT_EQUAL( 'c', *jsonValue );
}

/*-----------------------------------------------------------*/

TEST( AiaJsonUtilsTests, GetArrayElementAfterNestedObject )
{
    const char jsonArray[] = "[a,{b,b},c,d]";
    const char* jsonValue;
    size_t jsonValueLength;
    size_t index = 2;
    TEST_ASSERT_TRUE(
        AiaJsonUtils_GetArrayElement( jsonArray, sizeof( jsonArray ) - 1, index,
                                      &jsonValue, &jsonValueLength ) );
    TEST_ASSERT_EQUAL( 1, jsonValueLength );
    TEST_ASSERT_EQUAL( 'c', *jsonValue );
}

/*-----------------------------------------------------------*/

TEST( AiaJsonUtilsTests, GetArrayElementObject )
{
    const char jsonArray[] = "[a,{b,b},c,d]";
    const char* jsonValue;
    size_t jsonValueLength;
    size_t index = 1;
    TEST_ASSERT_TRUE(
        AiaJsonUtils_GetArrayElement( jsonArray, sizeof( jsonArray ) - 1, index,
                                      &jsonValue, &jsonValueLength ) );
    TEST_ASSERT_EQUAL( 5, jsonValueLength );
    TEST_ASSERT_EQUAL_STRING_LEN( "{b,b}", jsonValue, jsonValueLength );
}

/*-----------------------------------------------------------*/

TEST( AiaJsonUtilsTests, GetArrayElementInvalidArray )
{
    const char jsonArray[] = "a,b,c,d]";
    const char* jsonValue;
    size_t jsonValueLength;
    size_t index = 1;
    TEST_ASSERT_FALSE(
        AiaJsonUtils_GetArrayElement( jsonArray, sizeof( jsonArray ) - 1, index,
                                      &jsonValue, &jsonValueLength ) );
}

/*-----------------------------------------------------------*/

TEST( AiaJsonUtilsTests, ExtractLong )
{
    static const char* payload = "{\"testKey\": 100}";
    AiaJsonLongType out;
    TEST_ASSERT_TRUE(
        AiaJsonUtils_ExtractLong( payload, strlen( payload ), "testKey",
                                  sizeof( "testKey" ) - 1, &out ) );
    TEST_ASSERT_EQUAL( out, 100 );
}

/*-----------------------------------------------------------*/

TEST( AiaJsonUtilsTests, ExtractLongWithInvalidLong )
{
    static const char* payload = "{\"testKey\": \"abc\"}";
    AiaJsonLongType out;
    TEST_ASSERT_FALSE(
        AiaJsonUtils_ExtractLong( payload, strlen( payload ), "testKey",
                                  sizeof( "testKey" ) - 1, &out ) );
}

/*-----------------------------------------------------------*/

TEST( AiaJsonUtilsTests, ExtractLongWithNullArgs )
{
    static const char* payload = "{\"testKey\": \"abc\"}";
    AiaJsonLongType out;
    TEST_ASSERT_FALSE( AiaJsonUtils_ExtractLong(
        NULL, strlen( payload ), "testKey", sizeof( "testKey" ) - 1, &out ) );

    TEST_ASSERT_FALSE(
        AiaJsonUtils_ExtractLong( payload, strlen( payload ), "testKey",
                                  sizeof( "testKey" ) - 1, NULL ) );
}

/*-----------------------------------------------------------*/
