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
 * @file aia_regulator_buffer_tests.c
 * @brief Tests for AiaRegulatorBuffer_t.
 */

/* The config header is always included first. */
#include <aia_config.h>

/* Aia test headers */
#include <aiatestutilities/aia_test_utilities.h>

/* Aia headers */
#include <aiacore/aia_json_message.h>
#include <aiacore/aia_utils.h>
#include <aiaregulator/private/aia_regulator_buffer.h>

/* Test framework includes. */
#include <unity_fixture.h>

/*-----------------------------------------------------------*/

/**
 * Maximum message size to use in tests.  Large enough to
 * hold a few empty @c AiaJsonMessage_t instances.
 */
static const size_t TEST_MAX_MESSAGE_SIZE = 200;

/*-----------------------------------------------------------*/

/**
 * @brief Test group for AiaRegulatorBuffer_t tests.
 */
TEST_GROUP( AiaRegulatorBufferTests );

/*-----------------------------------------------------------*/

/** The @c AiaRegulatorBuffer_t instance to use in tests. */
static AiaRegulatorBuffer_t* testRegulatorBuffer;

/*-----------------------------------------------------------*/

/**
 * @brief Test setup for AiaRegulatorBuffer_t tests.
 */
TEST_SETUP( AiaRegulatorBufferTests )
{
    testRegulatorBuffer = AiaRegulatorBuffer_Create( TEST_MAX_MESSAGE_SIZE );
    TEST_ASSERT_NOT_NULL( testRegulatorBuffer );
}

/*-----------------------------------------------------------*/

/**
 * Data structure used to pass user data from @c RemoveFrontHelper() to @c
 * RemoveFrontHelperCallback().
 */
typedef struct RemoveFrontHelperData
{
    /** Array of chunk sizes we are expecting. */
    const size_t* chunkSizes;

    /** Number of chunks we are expecting. */
    size_t numChunks;

    /** Index of the chunk we are currently expecting. */
    size_t currentChunk;
} RemoveFrontHelperData_t;

/*-----------------------------------------------------------*/

/**
 * Helper function to validate the chunks being emitted from a call to @c
 * AiaRegulatorBuffer_RemoveFront().
 *
 * @param chunkForMessage The chunk currently being emitted.
 * @param remainingBytes Number of bytes remaining to be emitted.
 * @param remainingChunks Number of chunks remaining to be emitted.
 * @param userData Pointer to a @c RemoveFrontHelperData_t instance.
 * @return @c true if successful, else @c false.
 */
static bool RemoveFrontHelperCallback( AiaRegulatorChunk_t* chunkForMessage,
                                       size_t remainingBytes,
                                       size_t remainingChunks, void* userData )
{
    TEST_ASSERT_NOT_NULL( chunkForMessage );
    TEST_ASSERT_NOT_NULL( userData );
    RemoveFrontHelperData_t* helperData = (RemoveFrontHelperData_t*)userData;

    /* Make sure we're not going out of range. */
    TEST_ASSERT_LESS_THAN( helperData->numChunks, helperData->currentChunk );

    /* Tally up expected number of remaining bytes and chunks, including this
     * chunk. */
    size_t i;
    size_t expectedRemainingBytes = 0;
    size_t expectedRemainingChunks = 0;
    for( i = helperData->currentChunk; i < helperData->numChunks; ++i )
    {
        expectedRemainingBytes += helperData->chunkSizes[ i ];
        ++expectedRemainingChunks;
    }

    /* Remove this chunk from the expected amount so we can compare to
     * remainingBytes/remainingChunks. */
    TEST_ASSERT_GREATER_OR_EQUAL(
        helperData->chunkSizes[ helperData->currentChunk ],
        expectedRemainingBytes );
    expectedRemainingBytes -=
        helperData->chunkSizes[ helperData->currentChunk ];
    TEST_ASSERT_EQUAL( expectedRemainingBytes, remainingBytes );
    --expectedRemainingChunks;
    TEST_ASSERT_EQUAL( expectedRemainingChunks, remainingChunks );

    /* Make sure the current chunk's size is what we expected. */
    TEST_ASSERT_EQUAL( helperData->chunkSizes[ helperData->currentChunk ],
                       AiaMessage_GetSize( chunkForMessage ) );

    /* Free the chunk now that we're done with it. */
    AiaJsonMessage_Destroy( AiaJsonMessage_FromMessage( chunkForMessage ) );

    /* Next! */
    ++helperData->currentChunk;
    if( helperData->currentChunk == helperData->numChunks )
    {
        TEST_ASSERT_EQUAL( 0, remainingBytes );
        TEST_ASSERT_EQUAL( 0, remainingChunks );
    }

    return true;
}

/*-----------------------------------------------------------*/

/**
 * Helper function for testing @c AiaRegulatorBuffer_RemoveFront().
 * @param inputSizes Array of input message sizes to test with.
 * @param numInputs Number of entries in @c inputSizes.
 * @param outputChunkSizes Array of arrays of output chunk sizes expected to be
 * emitted.
 * @param outputNumChunks Array of sizes (number of entries) of the inner arrays
 * in @c outputChunkSizes.
 * @param numOutputs Number of entries in @c outputChunkSizes and @c
 * outputNumChunks.
 */
static void RemoveFrontHelper( const size_t* inputSizes, size_t numInputs,
                               const size_t** outputChunkSizes,
                               const size_t* outputNumChunks,
                               size_t numOutputs )
{
    AiaJsonMessage_t* jsonMessage = AiaJsonMessage_Create( "", "", "" );
    size_t minSize =
        AiaMessage_GetSize( AiaJsonMessage_ToConstMessage( jsonMessage ) );
    AiaJsonMessage_Destroy( jsonMessage );

    for( size_t i = 0; i < numInputs; ++i )
    {
        TEST_ASSERT_GREATER_OR_EQUAL( minSize, inputSizes[ i ] );
        char scratch[ 1024 ] = "";
        TEST_ASSERT_LESS_THAN( sizeof( scratch ), inputSizes[ i ] - minSize );
        snprintf( scratch, sizeof( scratch ), "%*s",
                  (int)( inputSizes[ i ] - minSize ), " " );
        scratch[ sizeof( scratch ) - 1 ] = '\0';
        AiaJsonMessage_t* jsonMessage =
            AiaJsonMessage_Create( scratch, "", "" );
        TEST_ASSERT_TRUE( AiaRegulatorBuffer_PushBack(
            testRegulatorBuffer, AiaJsonMessage_ToMessage( jsonMessage ) ) );
    }

    for( size_t i = 0; i < numOutputs; ++i )
    {
        RemoveFrontHelperData_t helperData = { outputChunkSizes[ i ],
                                               outputNumChunks[ i ], 0 };
        TEST_ASSERT_TRUE( AiaRegulatorBuffer_RemoveFront(
            testRegulatorBuffer, RemoveFrontHelperCallback, &helperData ) );
    }
}

/*-----------------------------------------------------------*/

/**
 * @brief Test tear down for AiaRegulatorBuffer_t tests.
 */
TEST_TEAR_DOWN( AiaRegulatorBufferTests )
{
    AiaRegulatorBuffer_Destroy( testRegulatorBuffer,
                                AiaTestUtilities_DestroyJsonChunk, NULL );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test group runner for AiaRegulatorBuffer_t tests.
 */
TEST_GROUP_RUNNER( AiaRegulatorBufferTests )
{
    RUN_TEST_CASE( AiaRegulatorBufferTests, CreateAndDestroyWhenEmpty );
    RUN_TEST_CASE( AiaRegulatorBufferTests, PushBackWithoutChunk );
    RUN_TEST_CASE( AiaRegulatorBufferTests, PushBackWithChunk );
    RUN_TEST_CASE( AiaRegulatorBufferTests, RemoveFrontWithoutCallback );
    RUN_TEST_CASE( AiaRegulatorBufferTests, RemoveFrontWhenEmpty );
    RUN_TEST_CASE( AiaRegulatorBufferTests,
                   RemoveFrontWhenEmptyWithoutUserData );
    RUN_TEST_CASE( AiaRegulatorBufferTests, RemoveFrontWithChunkEmitFailure );
    RUN_TEST_CASE( AiaRegulatorBufferTests, RemoveFrontWithChunkEmitSuccess );
    RUN_TEST_CASE( AiaRegulatorBufferTests, IsEmptyWhenEmpty );
    RUN_TEST_CASE( AiaRegulatorBufferTests, IsEmptyWithChunk );
    RUN_TEST_CASE( AiaRegulatorBufferTests, ClearWhenEmpty );
    RUN_TEST_CASE( AiaRegulatorBufferTests, ClearWithChunkWithoutCallback );
    RUN_TEST_CASE( AiaRegulatorBufferTests, ClearWithChunkWithCallback );
    RUN_TEST_CASE( AiaRegulatorBufferTests,
                   ClearWithChunkWithCallbackAndUserData );
    RUN_TEST_CASE( AiaRegulatorBufferTests, GetMaxMessageSize );
    RUN_TEST_CASE( AiaRegulatorBufferTests, CanFillMessageWhenEmpty );
    RUN_TEST_CASE( AiaRegulatorBufferTests, CanFillMessageWhenInsufficient );
    RUN_TEST_CASE( AiaRegulatorBufferTests, CanFillMessageWhenSufficient );
    RUN_TEST_CASE( AiaRegulatorBufferTests,
                   RemoveFrontSingleMessageSingleChunk );
    RUN_TEST_CASE( AiaRegulatorBufferTests,
                   RemoveFrontSingleMessageMultipleChunks );
    RUN_TEST_CASE( AiaRegulatorBufferTests,
                   RemoveFrontMultipleMessagesMultipleChunksUnaligned );
    RUN_TEST_CASE( AiaRegulatorBufferTests,
                   RemoveFrontMultipleMessagesMultipleChunksAligned );
}

/*-----------------------------------------------------------*/

TEST( AiaRegulatorBufferTests, CreateAndDestroyWhenEmpty )
{
    /* Empty test to verify that setup/teardown pass in isolation. */
}

/*-----------------------------------------------------------*/

TEST( AiaRegulatorBufferTests, PushBackWithoutChunk )
{
    TEST_ASSERT_FALSE(
        AiaRegulatorBuffer_PushBack( testRegulatorBuffer, NULL ) );
    TEST_ASSERT_EQUAL( 0, AiaRegulatorBuffer_GetSize( testRegulatorBuffer ) );
}

/*-----------------------------------------------------------*/

TEST( AiaRegulatorBufferTests, PushBackWithChunk )
{
    AiaJsonMessage_t* jsonMessage = AiaJsonMessage_Create( "", "", "" );
    size_t jsonMessageSize =
        AiaMessage_GetSize( AiaJsonMessage_ToConstMessage( jsonMessage ) );
    TEST_ASSERT_TRUE( AiaRegulatorBuffer_PushBack(
        testRegulatorBuffer, AiaJsonMessage_ToMessage( jsonMessage ) ) );
    TEST_ASSERT_EQUAL( jsonMessageSize,
                       AiaRegulatorBuffer_GetSize( testRegulatorBuffer ) );
}

/*-----------------------------------------------------------*/

TEST( AiaRegulatorBufferTests, RemoveFrontWithoutCallback )
{
    TEST_ASSERT_FALSE(
        AiaRegulatorBuffer_RemoveFront( testRegulatorBuffer, NULL, NULL ) );
}

/*-----------------------------------------------------------*/

bool testEmitMessageChunk( AiaRegulatorChunk_t* chunkForMessage,
                           size_t remainingBytes, size_t remainingChunks,
                           void* userData )
{
    TEST_ASSERT_NOT_NULL( chunkForMessage );
    (void)remainingBytes;
    (void)remainingChunks;
    TEST_ASSERT_NOT_NULL( userData );
    bool* result = (bool*)userData;
    if( *result )
    {
        AiaJsonMessage_Destroy( AiaJsonMessage_FromMessage( chunkForMessage ) );
    }
    return *result;
}

/*-----------------------------------------------------------*/

TEST( AiaRegulatorBufferTests, RemoveFrontWhenEmpty )
{
    bool callbackResult = false;
    TEST_ASSERT_TRUE( AiaRegulatorBuffer_RemoveFront(
        testRegulatorBuffer, testEmitMessageChunk, &callbackResult ) );
}

/*-----------------------------------------------------------*/

TEST( AiaRegulatorBufferTests, RemoveFrontWhenEmptyWithoutUserData )
{
    TEST_ASSERT_TRUE( AiaRegulatorBuffer_RemoveFront(
        testRegulatorBuffer, testEmitMessageChunk, NULL ) );
}

/*-----------------------------------------------------------*/

TEST( AiaRegulatorBufferTests, RemoveFrontWithChunkEmitFailure )
{
    bool callbackResult = false;
    AiaJsonMessage_t* jsonMessage = AiaJsonMessage_Create( "", "", "" );
    size_t jsonMessageSize =
        AiaMessage_GetSize( AiaJsonMessage_ToConstMessage( jsonMessage ) );
    TEST_ASSERT_TRUE( AiaRegulatorBuffer_PushBack(
        testRegulatorBuffer, AiaJsonMessage_ToMessage( jsonMessage ) ) );
    TEST_ASSERT_FALSE( AiaRegulatorBuffer_RemoveFront(
        testRegulatorBuffer, testEmitMessageChunk, &callbackResult ) );
    TEST_ASSERT_EQUAL( jsonMessageSize,
                       AiaRegulatorBuffer_GetSize( testRegulatorBuffer ) );
}

/*-----------------------------------------------------------*/

TEST( AiaRegulatorBufferTests, RemoveFrontWithChunkEmitSuccess )
{
    bool callbackResult = true;
    AiaJsonMessage_t* jsonMessage = AiaJsonMessage_Create( "", "", "" );
    TEST_ASSERT_TRUE( AiaRegulatorBuffer_PushBack(
        testRegulatorBuffer, AiaJsonMessage_ToMessage( jsonMessage ) ) );
    TEST_ASSERT_TRUE( AiaRegulatorBuffer_RemoveFront(
        testRegulatorBuffer, testEmitMessageChunk, &callbackResult ) );
    TEST_ASSERT_EQUAL( 0, AiaRegulatorBuffer_GetSize( testRegulatorBuffer ) );
}

/*-----------------------------------------------------------*/

TEST( AiaRegulatorBufferTests, IsEmptyWhenEmpty )
{
    TEST_ASSERT_TRUE( AiaRegulatorBuffer_IsEmpty( testRegulatorBuffer ) );
}

/*-----------------------------------------------------------*/

TEST( AiaRegulatorBufferTests, IsEmptyWithChunk )
{
    AiaJsonMessage_t* jsonMessage = AiaJsonMessage_Create( "", "", "" );
    TEST_ASSERT_TRUE( AiaRegulatorBuffer_PushBack(
        testRegulatorBuffer, AiaJsonMessage_ToMessage( jsonMessage ) ) );
    TEST_ASSERT_FALSE( AiaRegulatorBuffer_IsEmpty( testRegulatorBuffer ) );
}

/*-----------------------------------------------------------*/

TEST( AiaRegulatorBufferTests, ClearWhenEmpty )
{
    AiaRegulatorBuffer_Clear( testRegulatorBuffer, NULL, NULL );
}

/*-----------------------------------------------------------*/

TEST( AiaRegulatorBufferTests, ClearWithChunkWithoutCallback )
{
    AiaJsonMessage_t* jsonMessage = AiaJsonMessage_Create( "", "", "" );
    TEST_ASSERT_TRUE( AiaRegulatorBuffer_PushBack(
        testRegulatorBuffer, AiaJsonMessage_ToMessage( jsonMessage ) ) );
    AiaRegulatorBuffer_Clear( testRegulatorBuffer, NULL, NULL );
    TEST_ASSERT_TRUE( AiaRegulatorBuffer_IsEmpty( testRegulatorBuffer ) );
    AiaJsonMessage_Destroy( jsonMessage );
}

/*-----------------------------------------------------------*/

TEST( AiaRegulatorBufferTests, ClearWithChunkWithCallback )
{
    AiaJsonMessage_t* jsonMessage = AiaJsonMessage_Create( "", "", "" );
    TEST_ASSERT_TRUE( AiaRegulatorBuffer_PushBack(
        testRegulatorBuffer, AiaJsonMessage_ToMessage( jsonMessage ) ) );
    AiaRegulatorBuffer_Clear( testRegulatorBuffer,
                              AiaTestUtilities_DestroyJsonChunk, NULL );
    TEST_ASSERT_TRUE( AiaRegulatorBuffer_IsEmpty( testRegulatorBuffer ) );
}

/*-----------------------------------------------------------*/

TEST( AiaRegulatorBufferTests, ClearWithChunkWithCallbackAndUserData )
{
    AiaJsonMessage_t* jsonMessage = AiaJsonMessage_Create( "", "", "" );
    TEST_ASSERT_TRUE( AiaRegulatorBuffer_PushBack(
        testRegulatorBuffer, AiaJsonMessage_ToMessage( jsonMessage ) ) );
    AiaTestUtilities_DestroyCall_t call = { AiaTestUtilities_DestroyJsonChunk,
                                            NULL };
    AiaRegulatorBuffer_Clear( testRegulatorBuffer,
                              AiaTestUtilities_DestroyJsonChunkWithUserData,
                              &call );
    TEST_ASSERT_TRUE( AiaRegulatorBuffer_IsEmpty( testRegulatorBuffer ) );
}

/*-----------------------------------------------------------*/

TEST( AiaRegulatorBufferTests, GetMaxMessageSize )
{
    TEST_ASSERT_EQUAL(
        TEST_MAX_MESSAGE_SIZE,
        AiaRegulatorBuffer_GetMaxMessageSize( testRegulatorBuffer ) );
}

/*-----------------------------------------------------------*/

TEST( AiaRegulatorBufferTests, CanFillMessageWhenEmpty )
{
    TEST_ASSERT_FALSE(
        AiaRegulatorBuffer_CanFillMessage( testRegulatorBuffer ) );
}

/*-----------------------------------------------------------*/

TEST( AiaRegulatorBufferTests, CanFillMessageWhenInsufficient )
{
    AiaJsonMessage_t* jsonMessage = AiaJsonMessage_Create( "", "", "" );
    TEST_ASSERT_TRUE( AiaRegulatorBuffer_PushBack(
        testRegulatorBuffer, AiaJsonMessage_ToMessage( jsonMessage ) ) );
    TEST_ASSERT_FALSE(
        AiaRegulatorBuffer_CanFillMessage( testRegulatorBuffer ) );
}

/*-----------------------------------------------------------*/

TEST( AiaRegulatorBufferTests, CanFillMessageWhenSufficient )
{
    while( AiaRegulatorBuffer_GetSize( testRegulatorBuffer ) <
           TEST_MAX_MESSAGE_SIZE )
    {
        TEST_ASSERT_FALSE(
            AiaRegulatorBuffer_CanFillMessage( testRegulatorBuffer ) );
        AiaJsonMessage_t* jsonMessage = AiaJsonMessage_Create( "", "", "" );
        TEST_ASSERT_TRUE( AiaRegulatorBuffer_PushBack(
            testRegulatorBuffer, AiaJsonMessage_ToMessage( jsonMessage ) ) );
    }
    TEST_ASSERT_TRUE(
        AiaRegulatorBuffer_CanFillMessage( testRegulatorBuffer ) );
}

/*-----------------------------------------------------------*/

TEST( AiaRegulatorBufferTests, RemoveFrontSingleMessageSingleChunk )
{
    const size_t inputSizes[] = { 50 };
    const size_t numInputs = AiaArrayLength( inputSizes );
    const size_t outputMessage0ChunkSizes[] = { 50 };
    const size_t* outputChunkSizes[] = { outputMessage0ChunkSizes };
    const size_t outputNumChunks[] = { AiaArrayLength(
        outputMessage0ChunkSizes ) };
    const size_t numOutputs = AiaArrayLength( outputNumChunks );
    RemoveFrontHelper( inputSizes, numInputs, outputChunkSizes, outputNumChunks,
                       numOutputs );
}

/*-----------------------------------------------------------*/

TEST( AiaRegulatorBufferTests, RemoveFrontSingleMessageMultipleChunks )
{
    const size_t inputSizes[] = { 50, 51, 52 };
    const size_t numInputs = AiaArrayLength( inputSizes );
    const size_t outputMessage0ChunkSizes[] = { 50, 51, 52 };
    const size_t* outputChunkSizes[] = { outputMessage0ChunkSizes };
    const size_t outputNumChunks[] = { AiaArrayLength(
        outputMessage0ChunkSizes ) };
    const size_t numOutputs = AiaArrayLength( outputNumChunks );
    RemoveFrontHelper( inputSizes, numInputs, outputChunkSizes, outputNumChunks,
                       numOutputs );
}

/*-----------------------------------------------------------*/

TEST( AiaRegulatorBufferTests,
      RemoveFrontMultipleMessagesMultipleChunksUnaligned )
{
    const size_t inputSizes[] = { 50, 51, TEST_MAX_MESSAGE_SIZE };
    const size_t numInputs = AiaArrayLength( inputSizes );
    const size_t outputMessage0ChunkSizes[] = { 50, 51 };
    const size_t outputMessage1ChunkSizes[] = { TEST_MAX_MESSAGE_SIZE };
    const size_t* outputChunkSizes[] = { outputMessage0ChunkSizes,
                                         outputMessage1ChunkSizes };
    const size_t outputNumChunks[] = {
        AiaArrayLength( outputMessage0ChunkSizes ),
        AiaArrayLength( outputMessage1ChunkSizes )
    };
    const size_t numOutputs = AiaArrayLength( outputNumChunks );
    RemoveFrontHelper( inputSizes, numInputs, outputChunkSizes, outputNumChunks,
                       numOutputs );
}

/*-----------------------------------------------------------*/

TEST( AiaRegulatorBufferTests,
      RemoveFrontMultipleMessagesMultipleChunksAligned )
{
    const size_t inputSizes[] = { TEST_MAX_MESSAGE_SIZE / 2,
                                  TEST_MAX_MESSAGE_SIZE / 2, 50 };
    const size_t numInputs = AiaArrayLength( inputSizes );
    const size_t outputMessage0ChunkSizes[] = { TEST_MAX_MESSAGE_SIZE / 2,
                                                TEST_MAX_MESSAGE_SIZE / 2 };
    const size_t outputMessage1ChunkSizes[] = { 50 };
    const size_t* outputChunkSizes[] = { outputMessage0ChunkSizes,
                                         outputMessage1ChunkSizes };
    const size_t outputNumChunks[] = {
        AiaArrayLength( outputMessage0ChunkSizes ),
        AiaArrayLength( outputMessage1ChunkSizes )
    };
    const size_t numOutputs = AiaArrayLength( outputNumChunks );
    RemoveFrontHelper( inputSizes, numInputs, outputChunkSizes, outputNumChunks,
                       numOutputs );
}
