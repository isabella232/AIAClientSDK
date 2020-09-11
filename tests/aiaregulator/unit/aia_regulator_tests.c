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
 * @file aia_regulator_tests.c
 * @brief Tests for AiaRegulator_t.
 */

/* The config header is always included first. */
#include <aia_config.h>

/* Aia headers */
#include <aiacore/aia_json_message.h>
#include <aiacore/aia_utils.h>
#include <aiaregulator/aia_regulator.h>
#include <aiatestutilities/aia_test_utilities.h>
#include AiaClock( HEADER )

/* Test framework includes. */
#include <unity_fixture.h>

/*-----------------------------------------------------------*/

/** A small message size, several of which can aggregate in a
 * TEST_MAX_MESSAGE_SIZE message. */
static const size_t TEST_RUNT_MESSAGE_SIZE = 50;

/**
 * Maximum message size to use in tests.  Large enough to
 * hold a few empty @c AiaJsonMessage_t instances.
 */
#define TEST_MAX_MESSAGE_SIZE ( (size_t)200 )

/**
 * An oversize message that can not be queued.
 */
static const size_t TEST_OVERSIZE_MESSAGE_SIZE = TEST_MAX_MESSAGE_SIZE + 1;

/** Emit delay for the Regulator.*/
#define TEST_EMIT_DELAY_TIME_MS ( (AiaDurationMs_t)150 )

/** Used for cases where the message should have been emitted immediately
 * without delay. */
static const AiaDurationMs_t TEST_EMIT_NO_DELAY_TIME_MS =
    TEST_EMIT_DELAY_TIME_MS / 2;

/** Used for cases where the message took too long to emit. */
static const AiaDurationMs_t TEST_EMIT_DELAY_TIMEOUT_MS =
    TEST_EMIT_DELAY_TIME_MS * 2;

/*-----------------------------------------------------------*/

/**
 * @brief Test group for AiaRegulator_t tests.
 */
TEST_GROUP( AiaRegulatorTests );

/*-----------------------------------------------------------*/

/** Structure for holding emitted messages and when they were emitted. */
typedef struct EmittedMessage
{
    /** The node in the list. */
    AiaListDouble( Link_t ) link;

    /** The message that was emitted. */
    AiaRegulatorChunk_t* chunk;

    /** The time the message was emitted at. */
    AiaTimepointMs_t timepointMs;
} EmittedMessage_t;

/*-----------------------------------------------------------*/

/** Test data for Regulator tests. */
typedef struct AiaRegulatorTestData
{
    /** Flag indicating an internal test failure occurred. */
    AiaAtomicBool_t internalTestFailure;

    /** Queue for storing emitted messages and when they were emitted. */
    AiaListDouble_t emitOutput;

    /** Semaphore used to wait for @c EmitCallback. */
    AiaSemaphore_t emitSemaphore;

    /** Regulator for this test. */
    AiaRegulator_t* testRegulator;

    /** Flag used force all @c EmitCallback calls to fail. */
    bool failAllEmits;
} AiaRegulatorTestData_t;

/**
 * Global test data instance.
 * This must be global so that it can be initialized by @c TEST_SETUP(), cleaned
 * up by @c TEST_TEAR_DOWN(), and accessed by individual @c TEST() cases, but
 * probably should not be referenced directly from any other functions.
 */
AiaRegulatorTestData_t g_aiaRegulatorTestData;

/*-----------------------------------------------------------*/

/**
 * Helper function to validate the chunks being emitted from a call to @c
 * AiaRegulatorBuffer_RemoveFront().
 *
 * @param chunkForMessage The chunk currently being emitted.
 * @param remainingBytes Number of bytes remaining to be emitted.
 * @param remainingChunks Number of chunks remaining to be emitted.
 * @param userData The @c AiaRegulatorTestData_t for this test.
 * @return @c true if successful, else @c false.
 */
static bool EmitCallback( AiaRegulatorChunk_t* chunkForMessage,
                          size_t remainingBytes, size_t remainingChunks,
                          void* userData )
{
    (void)remainingBytes;
    (void)remainingChunks;
    if( !userData )
    {
        AiaLogError( "Null userData." );
        return false;
    }
    AiaRegulatorTestData_t* testData = (AiaRegulatorTestData_t*)userData;
    if( !chunkForMessage )
    {
        AiaLogError( "Null chunkForMessage." );
        AiaAtomicBool_Set( &testData->internalTestFailure );
        return false;
    }

    if( testData->failAllEmits )
    {
        return false;
    }

    EmittedMessage_t* emittedMessage =
        AiaCalloc( 1, sizeof( EmittedMessage_t ) );
    if( !emittedMessage )
    {
        AiaLogError( "AiaCalloc() failed." );
        AiaAtomicBool_Set( &testData->internalTestFailure );
        return false;
    }
    AiaListDouble( Link_t ) link = AiaListDouble( LINK_INITIALIZER );
    emittedMessage->link = link;
    emittedMessage->chunk = chunkForMessage;
    emittedMessage->timepointMs = AiaClock( GetTimeMs )();
    AiaListDouble( InsertTail )( &testData->emitOutput, &emittedMessage->link );

    AiaSemaphore( Post )( &testData->emitSemaphore );
    return true;
}

/*-----------------------------------------------------------*/

/**
 * Helper function for blocking and waiting for emit message call to happen.
 *
 * @param testData Pointer to the state information for this test suite.
 * @param expectedNumberOfEmits Number of emit we are waiting for
 * @return true, if we reached the given number of messages in emitOutput,
 *         false, if wait timed out.
 */
bool WaitForEmit( AiaRegulatorTestData_t* testData,
                  size_t expectedNumberOfEmits )
{
    if( !testData )
    {
        AiaLogError( "Null testData." );
        return false;
    }
    AiaTimepointMs_t t0 = AiaClock( GetTimeMs )();
    while( expectedNumberOfEmits-- )
    {
        AiaTimepointMs_t t1 = AiaClock( GetTimeMs )();
        AiaDurationMs_t elapsedMs = t1 - t0;
        if( elapsedMs > TEST_EMIT_DELAY_TIMEOUT_MS ||
            !AiaSemaphore( TimedWait )(
                &testData->emitSemaphore,
                TEST_EMIT_DELAY_TIMEOUT_MS - elapsedMs ) )
        {
            AiaLogError( "Timed out waiting for %zu emits.",
                         expectedNumberOfEmits + 1 );
            return false;
        }
    }
    return true;
}

/*-----------------------------------------------------------*/

/**
 * Helper function for verifying if the message gets emitted before
 * TEST_EMIT_DELAY_TIMEOUT_MS and after TEST_EMIT_DELAY_TIME_MS.
 *
 * @param startTimeMs When we started waiting for the message to be emitted.
 * @param emitTimeMs When the message was actually emitted.
 * @param messageName Identifier for the message being emitted.
 * @return @c true if the message was emitted at the expected time, else @c
 * false.
 */
bool CheckDelayedEmitTimestamp( AiaTimepointMs_t startTimeMs,
                                AiaTimepointMs_t emitTimeMs,
                                const char* messageName )
{
    AiaDurationMs_t deltaMs = emitTimeMs - startTimeMs;
    if( deltaMs < TEST_EMIT_DELAY_TIME_MS )
    {
        AiaLogError( "%s emitted too early.", messageName );
        return false;
    }
    if( deltaMs >= TEST_EMIT_DELAY_TIMEOUT_MS )
    {
        AiaLogError( "%s should not take that long to emit.", messageName );
        return false;
    }
    return true;
}

/*-----------------------------------------------------------*/

/**
 * Helper function for verifying if the message gets emitted before
 * TEST_EMIT_NO_DELAY_TIME_MS.
 *
 * @param startTimeMs When we started waiting for the message to be emitted.
 * @param emitTimeMs When the message was actually emitted.
 * @param messageName Identifier for the message being emitted.
 * @return @c true if the message was emitted at the expected time, else @c
 * false.
 */
bool CheckImmediateEmitTimestamp( AiaTimepointMs_t startTimeMs,
                                  AiaTimepointMs_t emitTimeMs,
                                  const char* messageName )
{
    AiaDurationMs_t deltaMs = emitTimeMs - startTimeMs;
    if( deltaMs >= TEST_EMIT_NO_DELAY_TIME_MS )
    {
        AiaLogError( "%s should have been emitted immediately.", messageName );
        return false;
    }
    return true;
}

/*-----------------------------------------------------------*/

/**
 * @brief Test setup for AiaRegulator_t tests.
 */
TEST_SETUP( AiaRegulatorTests )
{
    memset( &g_aiaRegulatorTestData, 0, sizeof( g_aiaRegulatorTestData ) );
    AiaAtomicBool_Clear( &g_aiaRegulatorTestData.internalTestFailure );
    AiaListDouble( Create )( &g_aiaRegulatorTestData.emitOutput );
    TEST_ASSERT_TRUE(
        AiaSemaphore( Create )( &g_aiaRegulatorTestData.emitSemaphore, 0, 1 ) );
    g_aiaRegulatorTestData.testRegulator =
        AiaRegulator_Create( TEST_MAX_MESSAGE_SIZE, EmitCallback,
                             &g_aiaRegulatorTestData, TEST_EMIT_DELAY_TIME_MS );
    TEST_ASSERT_NOT_NULL( g_aiaRegulatorTestData.testRegulator );
}

/*-----------------------------------------------------------*/

/**
 * Callback used to clean up @c EmittedMessage instances.
 *
 * @param userData The chunk to destroy.
 */
static void DestroyEmittedMessage( void* userData )
{
    TEST_ASSERT_NOT_NULL( userData );
    EmittedMessage_t* emittedMessage = (EmittedMessage_t*)userData;
    AiaJsonMessage_Destroy(
        AiaJsonMessage_FromMessage( emittedMessage->chunk ) );
    AiaFree( emittedMessage );
}

/*-----------------------------------------------------------*/

/**
 * Callback used to clean up @c AiaJsonMessage_t instances.
 *
 * @param chunk The chunk to destroy.
 * @param userData The @c AiaRegulatorTestData_t for this test.
 */
static void DestroyJsonChunk( AiaRegulatorChunk_t* chunk, void* userData )
{
    if( !userData )
    {
        AiaLogError( "Null userData." );
        return;
    }
    AiaRegulatorTestData_t* testData = (AiaRegulatorTestData_t*)userData;
    if( !chunk )
    {
        AiaLogError( "Null chunk." );
        AiaAtomicBool_Set( &testData->internalTestFailure );
        return;
    }
    AiaJsonMessage_Destroy( AiaJsonMessage_FromMessage( chunk ) );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test tear down for AiaRegulator_t tests.
 */
TEST_TEAR_DOWN( AiaRegulatorTests )
{
    /* TODO: Getting intermittent crashes without a small delay here.
     * (ADSER-1501) */
    AiaClock( SleepMs( 10 ) );

    AiaRegulator_Destroy( g_aiaRegulatorTestData.testRegulator,
                          DestroyJsonChunk, &g_aiaRegulatorTestData );
    AiaSemaphore( Destroy )( &g_aiaRegulatorTestData.emitSemaphore );
    AiaListDouble( RemoveAll )( &g_aiaRegulatorTestData.emitOutput,
                                DestroyEmittedMessage,
                                offsetof( EmittedMessage_t, link ) );
    TEST_ASSERT_FALSE(
        AiaAtomicBool_Load( &g_aiaRegulatorTestData.internalTestFailure ) );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test group runner for AiaRegulator_t tests.
 */
TEST_GROUP_RUNNER( AiaRegulatorTests )
{
    RUN_TEST_CASE( AiaRegulatorTests, CreateAndDestroyWhenEmpty );
    RUN_TEST_CASE( AiaRegulatorTests, CreateWithZeroMaxMessageSize );
    RUN_TEST_CASE( AiaRegulatorTests, CreateWithNullEmitMessageChunk );
    RUN_TEST_CASE( AiaRegulatorTests, WriteWithNullRegulator );
    RUN_TEST_CASE( AiaRegulatorTests, WriteWithNullChunk );
    RUN_TEST_CASE( AiaRegulatorTests, WriteTooBig );
    RUN_TEST_CASE( AiaRegulatorTests, CleanDestruction );
    RUN_TEST_CASE( AiaRegulatorTests, BurstModeOneRunt );
    RUN_TEST_CASE( AiaRegulatorTests, BurstModeOneMax );
    RUN_TEST_CASE( AiaRegulatorTests, BurstModeTwoRunts );
    RUN_TEST_CASE( AiaRegulatorTests, BurstModeOneRuntOneMax );
    RUN_TEST_CASE( AiaRegulatorTests, BurstModeTwoMax );
    RUN_TEST_CASE( AiaRegulatorTests, BadEmitAllMessage );
}

/*-----------------------------------------------------------*/

TEST( AiaRegulatorTests, CreateAndDestroyWhenEmpty )
{
    /* Empty test to verify that setup/teardown pass in isolation. */
}

/*-----------------------------------------------------------*/

TEST( AiaRegulatorTests, CreateWithZeroMaxMessageSize )
{
    TEST_ASSERT_NULL(
        AiaRegulator_Create( 0, EmitCallback, NULL, TEST_EMIT_DELAY_TIME_MS ) );
}

/*-----------------------------------------------------------*/

TEST( AiaRegulatorTests, CreateWithNullEmitMessageChunk )
{
    TEST_ASSERT_NULL( AiaRegulator_Create( TEST_MAX_MESSAGE_SIZE, NULL, NULL,
                                           TEST_EMIT_DELAY_TIME_MS ) );
}

/*-----------------------------------------------------------*/

TEST( AiaRegulatorTests, WriteWithNullRegulator )
{
    AiaJsonMessage_t* jsonMessage = AiaJsonMessage_Create( "", "", "" );
    TEST_ASSERT_FALSE(
        AiaRegulator_Write( NULL, AiaJsonMessage_ToMessage( jsonMessage ) ) );
}

/*-----------------------------------------------------------*/

TEST( AiaRegulatorTests, WriteWithNullChunk )
{
    TEST_ASSERT_FALSE(
        AiaRegulator_Write( g_aiaRegulatorTestData.testRegulator, NULL ) );
}

/*-----------------------------------------------------------*/

/**
 * Test writing message that's too big, should get rejected right away.
 */
TEST( AiaRegulatorTests, WriteTooBig )
{
    AiaJsonMessage_t* jsonMessage =
        AiaTestUtilities_CreateJsonMessage( TEST_OVERSIZE_MESSAGE_SIZE );
    TEST_ASSERT_NOT_NULL( jsonMessage );
    TEST_ASSERT_FALSE(
        AiaRegulator_Write( g_aiaRegulatorTestData.testRegulator,
                            AiaJsonMessage_ToMessage( jsonMessage ) ) );
}

/*-----------------------------------------------------------*/

/**
 * Test writing multiple messages and immediately end the test to check if the
 * Regulator get destructed gracefully. We should only see the first message
 * being emitted immediately. The test should have terminated before anything
 * else get emitted.
 */
TEST( AiaRegulatorTests, CleanDestruction )
{
    AiaJsonMessage_t* c1 =
        AiaTestUtilities_CreateJsonMessage( TEST_MAX_MESSAGE_SIZE );
    TEST_ASSERT_NOT_NULL( c1 );
    TEST_ASSERT_TRUE( AiaRegulator_Write( g_aiaRegulatorTestData.testRegulator,
                                          AiaJsonMessage_ToMessage( c1 ) ) );

    AiaJsonMessage_t* c2 =
        AiaTestUtilities_CreateJsonMessage( TEST_RUNT_MESSAGE_SIZE + 5 );
    TEST_ASSERT_NOT_NULL( c2 );
    TEST_ASSERT_TRUE( AiaRegulator_Write( g_aiaRegulatorTestData.testRegulator,
                                          AiaJsonMessage_ToMessage( c2 ) ) );

    AiaJsonMessage_t* c3 =
        AiaTestUtilities_CreateJsonMessage( TEST_RUNT_MESSAGE_SIZE + 4 );
    TEST_ASSERT_NOT_NULL( c2 );
    TEST_ASSERT_TRUE( AiaRegulator_Write( g_aiaRegulatorTestData.testRegulator,
                                          AiaJsonMessage_ToMessage( c3 ) ) );

    AiaJsonMessage_t* c4 =
        AiaTestUtilities_CreateJsonMessage( TEST_RUNT_MESSAGE_SIZE + 2 );
    TEST_ASSERT_NOT_NULL( c2 );
    TEST_ASSERT_TRUE( AiaRegulator_Write( g_aiaRegulatorTestData.testRegulator,
                                          AiaJsonMessage_ToMessage( c4 ) ) );

    TEST_ASSERT_LESS_THAN(
        2, AiaListDouble( Count )( &g_aiaRegulatorTestData.emitOutput ) );
}

/*-----------------------------------------------------------*/

/**
 * Test writing one runt message while in burst mode.  Should see a delayed
 * emit.
 */
TEST( AiaRegulatorTests, BurstModeOneRunt )
{
    AiaRegulator_SetEmitMode( g_aiaRegulatorTestData.testRegulator,
                              AIA_REGULATOR_BURST );

    AiaTimepointMs_t t0 = AiaClock( GetTimeMs )();
    AiaJsonMessage_t* c1 =
        AiaTestUtilities_CreateJsonMessage( TEST_RUNT_MESSAGE_SIZE );
    TEST_ASSERT_TRUE( AiaRegulator_Write( g_aiaRegulatorTestData.testRegulator,
                                          AiaJsonMessage_ToMessage( c1 ) ) );
    TEST_ASSERT_TRUE( WaitForEmit( &g_aiaRegulatorTestData, 1 ) );
    EmittedMessage_t* front = (EmittedMessage_t*)AiaListDouble( RemoveHead )(
        &g_aiaRegulatorTestData.emitOutput );
    TEST_ASSERT_TRUE(
        CheckDelayedEmitTimestamp( t0, front->timepointMs, "First message" ) );
    DestroyEmittedMessage( front );
}

/*-----------------------------------------------------------*/

/**
 * Test writing one max message while in burst mode.  Should see an immediate
 * emit.
 */
TEST( AiaRegulatorTests, BurstModeOneMax )
{
    AiaRegulator_SetEmitMode( g_aiaRegulatorTestData.testRegulator,
                              AIA_REGULATOR_BURST );

    AiaTimepointMs_t t0 = AiaClock( GetTimeMs )();
    AiaJsonMessage_t* c1 =
        AiaTestUtilities_CreateJsonMessage( TEST_MAX_MESSAGE_SIZE );
    TEST_ASSERT_TRUE( AiaRegulator_Write( g_aiaRegulatorTestData.testRegulator,
                                          AiaJsonMessage_ToMessage( c1 ) ) );
    TEST_ASSERT_TRUE( WaitForEmit( &g_aiaRegulatorTestData, 1 ) );
    EmittedMessage_t* front = (EmittedMessage_t*)AiaListDouble( RemoveHead )(
        &g_aiaRegulatorTestData.emitOutput );
    TEST_ASSERT_TRUE( CheckImmediateEmitTimestamp( t0, front->timepointMs,
                                                   "First message" ) );
    DestroyEmittedMessage( front );
}

/*-----------------------------------------------------------*/

/**
 * Test writing two runt messages while in burst mode.  Should see a delayed
 * emit.
 */
TEST( AiaRegulatorTests, BurstModeTwoRunts )
{
    AiaRegulator_SetEmitMode( g_aiaRegulatorTestData.testRegulator,
                              AIA_REGULATOR_BURST );

    AiaTimepointMs_t t0 = AiaClock( GetTimeMs )();
    AiaJsonMessage_t* c1 =
        AiaTestUtilities_CreateJsonMessage( TEST_RUNT_MESSAGE_SIZE );
    TEST_ASSERT_TRUE( AiaRegulator_Write( g_aiaRegulatorTestData.testRegulator,
                                          AiaJsonMessage_ToMessage( c1 ) ) );
    c1 = AiaTestUtilities_CreateJsonMessage( TEST_RUNT_MESSAGE_SIZE );
    TEST_ASSERT_TRUE( AiaRegulator_Write( g_aiaRegulatorTestData.testRegulator,
                                          AiaJsonMessage_ToMessage( c1 ) ) );
    TEST_ASSERT_TRUE( WaitForEmit( &g_aiaRegulatorTestData, 1 ) );
    EmittedMessage_t* front = (EmittedMessage_t*)AiaListDouble( RemoveHead )(
        &g_aiaRegulatorTestData.emitOutput );
    TEST_ASSERT_TRUE(
        CheckDelayedEmitTimestamp( t0, front->timepointMs, "First message" ) );
    DestroyEmittedMessage( front );
}

/*-----------------------------------------------------------*/

/**
 * Test writing one runt and one max message while in burst mode.  Should see an
 * immediate emit of the first, but the second should be delayed.
 */
TEST( AiaRegulatorTests, BurstModeOneRuntOneMax )
{
    AiaRegulator_SetEmitMode( g_aiaRegulatorTestData.testRegulator,
                              AIA_REGULATOR_BURST );

    AiaTimepointMs_t t0 = AiaClock( GetTimeMs )();
    AiaJsonMessage_t* c1 =
        AiaTestUtilities_CreateJsonMessage( TEST_RUNT_MESSAGE_SIZE );
    TEST_ASSERT_TRUE( AiaRegulator_Write( g_aiaRegulatorTestData.testRegulator,
                                          AiaJsonMessage_ToMessage( c1 ) ) );
    AiaJsonMessage_t* c2 =
        AiaTestUtilities_CreateJsonMessage( TEST_MAX_MESSAGE_SIZE );
    TEST_ASSERT_TRUE( AiaRegulator_Write( g_aiaRegulatorTestData.testRegulator,
                                          AiaJsonMessage_ToMessage( c2 ) ) );
    TEST_ASSERT_TRUE( WaitForEmit( &g_aiaRegulatorTestData, 1 ) );
    EmittedMessage_t* front = (EmittedMessage_t*)AiaListDouble( RemoveHead )(
        &g_aiaRegulatorTestData.emitOutput );
    AiaTimepointMs_t t1 = front->timepointMs;
    DestroyEmittedMessage( front );
    TEST_ASSERT_TRUE( CheckImmediateEmitTimestamp( t0, t1, "First message" ) );
    TEST_ASSERT_TRUE( WaitForEmit( &g_aiaRegulatorTestData, 1 ) );
    front = (EmittedMessage_t*)AiaListDouble( RemoveHead )(
        &g_aiaRegulatorTestData.emitOutput );
    TEST_ASSERT_TRUE(
        CheckDelayedEmitTimestamp( t1, front->timepointMs, "Second message" ) );
    DestroyEmittedMessage( front );
}

/*-----------------------------------------------------------*/

/**
 * Test writing two max messages while in burst mode.  Should see an immediate
 * emit of the first, but the second should still be delayed.
 */
TEST( AiaRegulatorTests, BurstModeTwoMax )
{
    AiaRegulator_SetEmitMode( g_aiaRegulatorTestData.testRegulator,
                              AIA_REGULATOR_BURST );

    AiaTimepointMs_t t0 = AiaClock( GetTimeMs )();
    AiaJsonMessage_t* c1 =
        AiaTestUtilities_CreateJsonMessage( TEST_MAX_MESSAGE_SIZE );
    TEST_ASSERT_TRUE( AiaRegulator_Write( g_aiaRegulatorTestData.testRegulator,
                                          AiaJsonMessage_ToMessage( c1 ) ) );
    c1 = AiaTestUtilities_CreateJsonMessage( TEST_MAX_MESSAGE_SIZE );
    TEST_ASSERT_TRUE( AiaRegulator_Write( g_aiaRegulatorTestData.testRegulator,
                                          AiaJsonMessage_ToMessage( c1 ) ) );
    TEST_ASSERT_TRUE( WaitForEmit( &g_aiaRegulatorTestData, 1 ) );
    EmittedMessage_t* front = (EmittedMessage_t*)AiaListDouble( RemoveHead )(
        &g_aiaRegulatorTestData.emitOutput );
    AiaTimepointMs_t t1 = front->timepointMs;
    DestroyEmittedMessage( front );
    TEST_ASSERT_TRUE( CheckImmediateEmitTimestamp( t0, t1, "First message" ) );
    TEST_ASSERT_TRUE( WaitForEmit( &g_aiaRegulatorTestData, 1 ) );
    front = (EmittedMessage_t*)AiaListDouble( RemoveHead )(
        &g_aiaRegulatorTestData.emitOutput );
    TEST_ASSERT_TRUE(
        CheckDelayedEmitTimestamp( t1, front->timepointMs, "Second message" ) );
    DestroyEmittedMessage( front );
}

/*-----------------------------------------------------------*/

/**
 * Test Regulator with bad emitMessage function that fail on every call.
 * Nothing should have been written to the buffer.
 */
TEST( AiaRegulatorTests, BadEmitAllMessage )
{
    g_aiaRegulatorTestData.failAllEmits = true;

    AiaJsonMessage_t* c1 =
        AiaTestUtilities_CreateJsonMessage( TEST_MAX_MESSAGE_SIZE );
    TEST_ASSERT_TRUE( AiaRegulator_Write( g_aiaRegulatorTestData.testRegulator,
                                          AiaJsonMessage_ToMessage( c1 ) ) );

    AiaJsonMessage_t* c2 =
        AiaTestUtilities_CreateJsonMessage( TEST_MAX_MESSAGE_SIZE );
    TEST_ASSERT_TRUE( AiaRegulator_Write( g_aiaRegulatorTestData.testRegulator,
                                          AiaJsonMessage_ToMessage( c2 ) ) );

    AiaJsonMessage_t* c3 =
        AiaTestUtilities_CreateJsonMessage( TEST_MAX_MESSAGE_SIZE );
    TEST_ASSERT_TRUE( AiaRegulator_Write( g_aiaRegulatorTestData.testRegulator,
                                          AiaJsonMessage_ToMessage( c3 ) ) );

    AiaClock( SleepMs( TEST_EMIT_NO_DELAY_TIME_MS ) );

    TEST_ASSERT_TRUE(
        AiaListDouble( IsEmpty )( &g_aiaRegulatorTestData.emitOutput ) );
}
