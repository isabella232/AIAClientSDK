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
 * @file aia_sequencer_tests.c
 * @brief Tests for AiaSequencer_t.
 */

/* The config header is always included first. */
#include <aia_config.h>

/* Aia headers */
#include <aiasequencer/aia_sequencer.h>

#include AiaTaskPool( HEADER )
#include AiaSemaphore( HEADER )

/* Test framework includes. */
#include <unity_fixture.h>

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

/*-----------------------------------------------------------*/

/**
 * @brief Test group for AiaSequencer_t tests.
 */
TEST_GROUP( AiaSequencerTests );

/*-----------------------------------------------------------*/

/**
 * @brief Test setup for AiaSequencer_t tests.
 */
TEST_SETUP( AiaSequencerTests )
{
    AiaTaskPoolInfo_t taskpoolInfo = AiaTaskPool( INFO_INITIALIZER );
    AiaTaskPoolError_t error =
        AiaTaskPool( CreateSystemTaskPool )( &taskpoolInfo );
    TEST_ASSERT_TRUE( AiaTaskPoolSucceeded( error ) );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test tear down for AiaSequencer_t tests.
 */
TEST_TEAR_DOWN( AiaSequencerTests )
{
    AiaTaskPoolError_t error =
        AiaTaskPool( Destroy )( AiaTaskPool( GetSystemTaskPool )() );
    TEST_ASSERT_TRUE( AiaTaskPoolSucceeded( error ) );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test group runner for AiaSequencer_t tests.
 */
TEST_GROUP_RUNNER( AiaSequencerTests )
{
    RUN_TEST_CASE( AiaSequencerTests, Creation );
    RUN_TEST_CASE( AiaSequencerTests, MessagesInOrder );
    RUN_TEST_CASE( AiaSequencerTests, SingleMessageOutOfOrderInBuffer );
    RUN_TEST_CASE( AiaSequencerTests, MultipleMessageOutOfOrderInBuffer );
    RUN_TEST_CASE( AiaSequencerTests, MultipleMessageOutOfOrderOutOfBuffer );

    RUN_TEST_CASE( AiaSequencerTests, Write );
    RUN_TEST_CASE( AiaSequencerTests, WriteDuplicateBuffer );
    RUN_TEST_CASE( AiaSequencerTests, WriteDropMessage );
    RUN_TEST_CASE( AiaSequencerTests, WriteOverBuffer );
    RUN_TEST_CASE( AiaSequencerTests, WriteManyInOrder );
    RUN_TEST_CASE( AiaSequencerTests, WriteOverUint32Max );
    RUN_TEST_CASE( AiaSequencerTests, Timeout );
    RUN_TEST_CASE( AiaSequencerTests, NoTimeout );
    RUN_TEST_CASE( AiaSequencerTests, ResetNextExpectedSequenceNumberBasic );
    RUN_TEST_CASE( AiaSequencerTests,
                   SequenceNumberResetsCorrectlyWhenCallingDuringEmission );
}

typedef struct AiaTestSequencerObserver
{
    char messagesOutputted[ 10000 ];
    /* TODO: Replace with mechanism that allows for running unit tests in
     * non-threaded environments. */
    AiaSemaphore_t timedOutWaitingSemaphore;

    /* Note: ADSER-1585 Used to invoke @c AiaSequencer_ResetSequenceNumber from
     * within @c messageSequencedCallback. */
    AiaSequencer_t* sequencer;

} AiaTestSequencerObserver_t;

static AiaTestSequencerObserver_t* AiaTestSequencerObserver_Create()
{
    AiaTestSequencerObserver_t* observer =
        AiaCalloc( 1, sizeof( AiaTestSequencerObserver_t ) );
    if( !observer )
    {
        return NULL;
    }
    if( !AiaSemaphore( Create )( &observer->timedOutWaitingSemaphore, 0,
                                 1000 ) )
    {
        AiaFree( observer );
        return NULL;
    }
    return observer;
}

static void AiaTestSequencerObserver_Destroy(
    AiaTestSequencerObserver_t* observer )
{
    AiaSemaphore( Destroy )( &observer->timedOutWaitingSemaphore );
    AiaFree( observer );
}

static void messageSequencedCallback( void* message, size_t size,
                                      void* userData )
{
    TEST_ASSERT_NOT_NULL( userData );
    TEST_ASSERT_NOT_NULL( message );
    TEST_ASSERT_GREATER_THAN( 0, size );
    AiaTestSequencerObserver_t* sequencerObserver =
        (AiaTestSequencerObserver_t*)userData;
    strcat( sequencerObserver->messagesOutputted, message );
}

static void timedOutWaitingCallback( void* userData )
{
    TEST_ASSERT_NOT_NULL( userData );
    AiaTestSequencerObserver_t* sequencerObserver =
        (AiaTestSequencerObserver_t*)userData;
    AiaSemaphore( Post )( &sequencerObserver->timedOutWaitingSemaphore );
}

static bool getSequencerNumberCallback( AiaSequenceNumber_t* sequenceNumber,
                                        void* message, size_t size,
                                        void* userData )
{
    TEST_ASSERT_NOT_NULL( sequenceNumber );
    TEST_ASSERT_NOT_NULL( userData );
    TEST_ASSERT_NOT_NULL( message );
    TEST_ASSERT_GREATER_THAN( 0, size );
    *sequenceNumber = strtoul( (char*)message, NULL, 10 );
    return true;
}

TEST( AiaSequencerTests, Creation )
{
    AiaTestSequencerObserver_t* observer = AiaTestSequencerObserver_Create();
    TEST_ASSERT_NOT_NULL( observer );

    AiaSequencer_t* sequencer =
        AiaSequencer_Create( NULL, observer, timedOutWaitingCallback, observer,
                             getSequencerNumberCallback, observer, 1, 0, 100,
                             AiaTaskPool( GetSystemTaskPool )() );
    TEST_ASSERT_NULL( sequencer );

    sequencer =
        AiaSequencer_Create( messageSequencedCallback, observer, NULL, observer,
                             getSequencerNumberCallback, observer, 1, 0, 100,
                             AiaTaskPool( GetSystemTaskPool )() );
    TEST_ASSERT_NULL( sequencer );

    sequencer = AiaSequencer_Create(
        messageSequencedCallback, observer, timedOutWaitingCallback, observer,
        NULL, observer, 1, 0, 100, AiaTaskPool( GetSystemTaskPool )() );
    TEST_ASSERT_NULL( sequencer );

    sequencer = AiaSequencer_Create( messageSequencedCallback, observer,
                                     timedOutWaitingCallback, observer,
                                     getSequencerNumberCallback, observer, 1, 0,
                                     100, AiaTaskPool( GetSystemTaskPool )() );
    TEST_ASSERT_NOT_NULL( sequencer );

    AiaSequencer_Destroy( sequencer );
    AiaTestSequencerObserver_Destroy( observer );
}

TEST( AiaSequencerTests, MessagesInOrder )
{
    AiaTestSequencerObserver_t* observer = AiaTestSequencerObserver_Create();
    TEST_ASSERT_NOT_NULL( observer );
    AiaSequencer_t* sequencer = AiaSequencer_Create(
        messageSequencedCallback, observer, timedOutWaitingCallback, observer,
        getSequencerNumberCallback, observer, 3, 1, 0,
        AiaTaskPool( GetSystemTaskPool )() );
    TEST_ASSERT_NOT_NULL( sequencer );

    TEST_ASSERT_TRUE( AiaSequencer_Write( sequencer, "1", sizeof( "1" ) ) );
    TEST_ASSERT_TRUE( AiaSequencer_Write( sequencer, "2", sizeof( "2" ) ) );
    TEST_ASSERT_TRUE( AiaSequencer_Write( sequencer, "3", sizeof( "3" ) ) );

    TEST_ASSERT_EQUAL_STRING( "123", observer->messagesOutputted );

    AiaSequencer_Destroy( sequencer );
    AiaTestSequencerObserver_Destroy( observer );
}

TEST( AiaSequencerTests, SingleMessageOutOfOrderInBuffer )
{
    AiaTestSequencerObserver_t* observer = AiaTestSequencerObserver_Create();
    TEST_ASSERT_NOT_NULL( observer );
    AiaSequencer_t* sequencer = AiaSequencer_Create(
        messageSequencedCallback, observer, timedOutWaitingCallback, observer,
        getSequencerNumberCallback, observer, 3, 1, 0,
        AiaTaskPool( GetSystemTaskPool )() );
    TEST_ASSERT_NOT_NULL( sequencer );

    TEST_ASSERT_TRUE( AiaSequencer_Write( sequencer, "2", sizeof( "2" ) ) );
    TEST_ASSERT_TRUE( AiaSequencer_Write( sequencer, "1", sizeof( "1" ) ) );
    TEST_ASSERT_TRUE( AiaSequencer_Write( sequencer, "3", sizeof( "3" ) ) );

    TEST_ASSERT_EQUAL_STRING( "123", observer->messagesOutputted );

    AiaSequencer_Destroy( sequencer );
    AiaTestSequencerObserver_Destroy( observer );
}

TEST( AiaSequencerTests, MultipleMessageOutOfOrderInBuffer )
{
    AiaTestSequencerObserver_t* observer = AiaTestSequencerObserver_Create();
    TEST_ASSERT_NOT_NULL( observer );
    AiaSequencer_t* sequencer = AiaSequencer_Create(
        messageSequencedCallback, observer, timedOutWaitingCallback, observer,
        getSequencerNumberCallback, observer, 3, 1, 0,
        AiaTaskPool( GetSystemTaskPool )() );
    TEST_ASSERT_NOT_NULL( sequencer );

    TEST_ASSERT_TRUE( AiaSequencer_Write( sequencer, "2", sizeof( "2" ) ) );
    TEST_ASSERT_TRUE( AiaSequencer_Write( sequencer, "4", sizeof( "4" ) ) );
    TEST_ASSERT_TRUE( AiaSequencer_Write( sequencer, "1", sizeof( "1" ) ) );
    TEST_ASSERT_TRUE( AiaSequencer_Write( sequencer, "3", sizeof( "3" ) ) );

    TEST_ASSERT_EQUAL_STRING( "1234", observer->messagesOutputted );

    AiaSequencer_Destroy( sequencer );
    AiaTestSequencerObserver_Destroy( observer );
}

TEST( AiaSequencerTests, MultipleMessageOutOfOrderOutOfBuffer )
{
    AiaTestSequencerObserver_t* observer = AiaTestSequencerObserver_Create();
    TEST_ASSERT_NOT_NULL( observer );
    AiaSequencer_t* sequencer = AiaSequencer_Create(
        messageSequencedCallback, observer, timedOutWaitingCallback, observer,
        getSequencerNumberCallback, observer, 3, 1, 0,
        AiaTaskPool( GetSystemTaskPool )() );
    TEST_ASSERT_NOT_NULL( sequencer );

    TEST_ASSERT_TRUE( AiaSequencer_Write( sequencer, "2", sizeof( "2" ) ) );
    TEST_ASSERT_FALSE( AiaSequencer_Write( sequencer, "5", sizeof( "5" ) ) );
    TEST_ASSERT_TRUE( AiaSequencer_Write( sequencer, "4", sizeof( "4" ) ) );
    TEST_ASSERT_TRUE( AiaSequencer_Write( sequencer, "3", sizeof( "3" ) ) );
    TEST_ASSERT_TRUE( AiaSequencer_Write( sequencer, "1", sizeof( "1" ) ) );

    TEST_ASSERT_EQUAL_STRING( "1234", observer->messagesOutputted );

    AiaSequencer_Destroy( sequencer );
    AiaTestSequencerObserver_Destroy( observer );
}

TEST( AiaSequencerTests, Write )
{
    AiaTestSequencerObserver_t* observer = AiaTestSequencerObserver_Create();
    TEST_ASSERT_NOT_NULL( observer );
    AiaSequencer_t* sequencer = AiaSequencer_Create(
        messageSequencedCallback, observer, timedOutWaitingCallback, observer,
        getSequencerNumberCallback, observer, 1, 0, 100,
        AiaTaskPool( GetSystemTaskPool )() );
    TEST_ASSERT_NOT_NULL( sequencer );

    TEST_ASSERT_TRUE( AiaSequencer_Write( sequencer, "0", sizeof( "0" ) ) );

    TEST_ASSERT_EQUAL_STRING( "0", observer->messagesOutputted );

    AiaSequencer_Destroy( sequencer );
    AiaTestSequencerObserver_Destroy( observer );
}

TEST( AiaSequencerTests, WriteDuplicateBuffer )
{
    AiaTestSequencerObserver_t* observer = AiaTestSequencerObserver_Create();
    TEST_ASSERT_NOT_NULL( observer );
    AiaSequencer_t* sequencer = AiaSequencer_Create(
        messageSequencedCallback, observer, timedOutWaitingCallback, observer,
        getSequencerNumberCallback, observer, 1, 0, 100,
        AiaTaskPool( GetSystemTaskPool )() );
    TEST_ASSERT_NOT_NULL( sequencer );

    TEST_ASSERT_TRUE( AiaSequencer_Write( sequencer, "1", sizeof( "1" ) ) );
    TEST_ASSERT_EQUAL_STRING( "", observer->messagesOutputted );
    TEST_ASSERT_TRUE( AiaSequencer_Write( sequencer, "1", sizeof( "1" ) ) );
    TEST_ASSERT_EQUAL_STRING( "", observer->messagesOutputted );

    AiaSequencer_Destroy( sequencer );
    AiaTestSequencerObserver_Destroy( observer );
}

TEST( AiaSequencerTests, WriteDropMessage )
{
    AiaTestSequencerObserver_t* observer = AiaTestSequencerObserver_Create();
    TEST_ASSERT_NOT_NULL( observer );
    AiaSequencer_t* sequencer = AiaSequencer_Create(
        messageSequencedCallback, observer, timedOutWaitingCallback, observer,
        getSequencerNumberCallback, observer, 1, 1, 100,
        AiaTaskPool( GetSystemTaskPool )() );
    TEST_ASSERT_NOT_NULL( sequencer );

    TEST_ASSERT_TRUE( AiaSequencer_Write( sequencer, "0", sizeof( "0" ) ) );
    TEST_ASSERT_EQUAL_STRING( "", observer->messagesOutputted );

    AiaSequencer_Destroy( sequencer );
    AiaTestSequencerObserver_Destroy( observer );
}

TEST( AiaSequencerTests, WriteOverBuffer )
{
    AiaTestSequencerObserver_t* observer = AiaTestSequencerObserver_Create();
    TEST_ASSERT_NOT_NULL( observer );
    AiaSequencer_t* sequencer = AiaSequencer_Create(
        messageSequencedCallback, observer, timedOutWaitingCallback, observer,
        getSequencerNumberCallback, observer, 1, 0, 100,
        AiaTaskPool( GetSystemTaskPool )() );
    TEST_ASSERT_NOT_NULL( sequencer );

    TEST_ASSERT_TRUE( AiaSequencer_Write( sequencer, "0", sizeof( "0" ) ) );
    TEST_ASSERT_EQUAL_STRING( "0", observer->messagesOutputted );

    TEST_ASSERT_FALSE( AiaSequencer_Write( sequencer, "3", sizeof( "3" ) ) );

    AiaSequencer_Destroy( sequencer );
    AiaTestSequencerObserver_Destroy( observer );
}

TEST( AiaSequencerTests, WriteManyInOrder )
{
    AiaTestSequencerObserver_t* observer = AiaTestSequencerObserver_Create();
    TEST_ASSERT_NOT_NULL( observer );
    AiaSequencer_t* sequencer = AiaSequencer_Create(
        messageSequencedCallback, observer, timedOutWaitingCallback, observer,
        getSequencerNumberCallback, observer, 1, 0, 100,
        AiaTaskPool( GetSystemTaskPool )() );
    TEST_ASSERT_NOT_NULL( sequencer );

    char expectedOutput[ 10000 ] = { 0 };

    for( AiaSequenceNumber_t sequenceNumber = 0; sequenceNumber < 1000;
         ++sequenceNumber )
    {
        char message[ 50 ];
        sprintf( message, "%" PRIu32, sequenceNumber );
        TEST_ASSERT_TRUE(
            AiaSequencer_Write( sequencer, message, sizeof( message ) ) );
        strcat( expectedOutput, message );
        TEST_ASSERT_EQUAL_STRING( expectedOutput, observer->messagesOutputted );
    }

    AiaSequencer_Destroy( sequencer );
    AiaTestSequencerObserver_Destroy( observer );
}

TEST( AiaSequencerTests, WriteOverUint32Max )
{
    AiaTestSequencerObserver_t* observer = AiaTestSequencerObserver_Create();
    TEST_ASSERT_NOT_NULL( observer );
    AiaSequencer_t* sequencer = AiaSequencer_Create(
        messageSequencedCallback, observer, timedOutWaitingCallback, observer,
        getSequencerNumberCallback, observer, 6, UINT32_MAX - 5, 0,
        AiaTaskPool( GetSystemTaskPool )() );
    TEST_ASSERT_NOT_NULL( sequencer );

    char expectedOutput[ 100000 ] = { 0 };
    char message[ 100 ];
    AiaSequenceNumber_t sequenceNumber = UINT32_MAX - 5;
    sprintf( message, "%" PRIu32, sequenceNumber );
    TEST_ASSERT_TRUE(
        AiaSequencer_Write( sequencer, message, sizeof( message ) ) );
    strcat( expectedOutput, message );
    TEST_ASSERT_EQUAL_STRING( expectedOutput, observer->messagesOutputted );

    sequenceNumber = UINT32_MAX - 4;
    sprintf( message, "%" PRIu32, sequenceNumber );
    TEST_ASSERT_TRUE(
        AiaSequencer_Write( sequencer, message, sizeof( message ) ) );
    strcat( expectedOutput, message );
    TEST_ASSERT_EQUAL_STRING( expectedOutput, observer->messagesOutputted );

    sequenceNumber = UINT32_MAX - 3;
    sprintf( message, "%" PRIu32, sequenceNumber );
    TEST_ASSERT_TRUE(
        AiaSequencer_Write( sequencer, message, sizeof( message ) ) );
    strcat( expectedOutput, message );
    TEST_ASSERT_EQUAL_STRING( expectedOutput, observer->messagesOutputted );

    sequenceNumber = UINT32_MAX - 2;
    sprintf( message, "%" PRIu32, sequenceNumber );
    TEST_ASSERT_TRUE(
        AiaSequencer_Write( sequencer, message, sizeof( message ) ) );
    strcat( expectedOutput, message );
    TEST_ASSERT_EQUAL_STRING( expectedOutput, observer->messagesOutputted );

    sequenceNumber = UINT32_MAX - 1;
    sprintf( message, "%" PRIu32, sequenceNumber );
    TEST_ASSERT_TRUE(
        AiaSequencer_Write( sequencer, message, sizeof( message ) ) );
    strcat( expectedOutput, message );
    TEST_ASSERT_EQUAL_STRING( expectedOutput, observer->messagesOutputted );

    sequenceNumber = UINT32_MAX;
    sprintf( message, "%" PRIu32, sequenceNumber );
    TEST_ASSERT_TRUE(
        AiaSequencer_Write( sequencer, message, sizeof( message ) ) );
    strcat( expectedOutput, message );
    TEST_ASSERT_EQUAL_STRING( expectedOutput, observer->messagesOutputted );

    sequenceNumber = 0;
    sprintf( message, "%" PRIu32, sequenceNumber );
    TEST_ASSERT_TRUE(
        AiaSequencer_Write( sequencer, message, sizeof( message ) ) );
    strcat( expectedOutput, message );
    TEST_ASSERT_EQUAL_STRING( expectedOutput, observer->messagesOutputted );

    sequenceNumber = 1;
    sprintf( message, "%" PRIu32, sequenceNumber );
    TEST_ASSERT_TRUE(
        AiaSequencer_Write( sequencer, message, sizeof( message ) ) );
    strcat( expectedOutput, message );
    TEST_ASSERT_EQUAL_STRING( expectedOutput, observer->messagesOutputted );

    sequenceNumber = 2;
    sprintf( message, "%" PRIu32, sequenceNumber );
    TEST_ASSERT_TRUE(
        AiaSequencer_Write( sequencer, message, sizeof( message ) ) );
    strcat( expectedOutput, message );
    TEST_ASSERT_EQUAL_STRING( expectedOutput, observer->messagesOutputted );

    sequenceNumber = 3;
    sprintf( message, "%" PRIu32, sequenceNumber );
    TEST_ASSERT_TRUE(
        AiaSequencer_Write( sequencer, message, sizeof( message ) ) );
    strcat( expectedOutput, message );
    TEST_ASSERT_EQUAL_STRING( expectedOutput, observer->messagesOutputted );

    sequenceNumber = 4;
    sprintf( message, "%" PRIu32, sequenceNumber );
    TEST_ASSERT_TRUE(
        AiaSequencer_Write( sequencer, message, sizeof( message ) ) );
    strcat( expectedOutput, message );
    TEST_ASSERT_EQUAL_STRING( expectedOutput, observer->messagesOutputted );

    sequenceNumber = 5;
    sprintf( message, "%" PRIu32, sequenceNumber );
    TEST_ASSERT_TRUE(
        AiaSequencer_Write( sequencer, message, sizeof( message ) ) );
    strcat( expectedOutput, message );
    TEST_ASSERT_EQUAL_STRING( expectedOutput, observer->messagesOutputted );

    AiaSequencer_Destroy( sequencer );
    AiaTestSequencerObserver_Destroy( observer );
}

TEST( AiaSequencerTests, Timeout )
{
    AiaSequenceNumber_t sequenceNumber = 1;
    AiaDurationMs_t timeout = 100;
    TEST_ASSERT_TRUE( sequenceNumber );

    AiaTestSequencerObserver_t* observer = AiaTestSequencerObserver_Create();
    TEST_ASSERT_NOT_NULL( observer );
    AiaSequencer_t* sequencer = AiaSequencer_Create(
        messageSequencedCallback, observer, timedOutWaitingCallback, observer,
        getSequencerNumberCallback, observer, 1, 0, timeout,
        AiaTaskPool( GetSystemTaskPool )() );
    TEST_ASSERT_NOT_NULL( sequencer );

    TEST_ASSERT_TRUE( AiaSequencer_Write( sequencer, "1", sizeof( "1" ) ) );

    TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
        &observer->timedOutWaitingSemaphore, timeout * 1.2 ) );

    TEST_ASSERT_TRUE( AiaSequencer_Write( sequencer, "0", sizeof( "0" ) ) );
    TEST_ASSERT_EQUAL_STRING( "01", observer->messagesOutputted );

    TEST_ASSERT_TRUE( AiaSequencer_Write( sequencer, "2", sizeof( "2" ) ) );
    TEST_ASSERT_EQUAL_STRING( "012", observer->messagesOutputted );

    TEST_ASSERT_TRUE( AiaSequencer_Write( sequencer, "4", sizeof( "4" ) ) );

    TEST_ASSERT_TRUE( AiaSemaphore( TimedWait )(
        &observer->timedOutWaitingSemaphore, timeout * 1.2 ) );

    AiaSequencer_Destroy( sequencer );
    AiaTestSequencerObserver_Destroy( observer );
}

TEST( AiaSequencerTests, NoTimeout )
{
    AiaSequenceNumber_t sequenceNumber = 1;
    AiaDurationMs_t timeout = 0;
    TEST_ASSERT_TRUE( sequenceNumber );

    AiaTestSequencerObserver_t* observer = AiaTestSequencerObserver_Create();
    TEST_ASSERT_NOT_NULL( observer );
    AiaSequencer_t* sequencer = AiaSequencer_Create(
        messageSequencedCallback, observer, timedOutWaitingCallback, observer,
        getSequencerNumberCallback, observer, 1, 0, timeout,
        AiaTaskPool( GetSystemTaskPool )() );
    TEST_ASSERT_NOT_NULL( sequencer );

    TEST_ASSERT_TRUE( AiaSequencer_Write( sequencer, "1", sizeof( "1" ) ) );

    TEST_ASSERT_FALSE(
        AiaSemaphore( TimedWait )( &observer->timedOutWaitingSemaphore, 100 ) );

    AiaSequencer_Destroy( sequencer );
    AiaTestSequencerObserver_Destroy( observer );
}

TEST( AiaSequencerTests, ResetNextExpectedSequenceNumberBasic )
{
    AiaTestSequencerObserver_t* observer = AiaTestSequencerObserver_Create();
    TEST_ASSERT_NOT_NULL( observer );
    AiaSequencer_t* sequencer = AiaSequencer_Create(
        messageSequencedCallback, observer, timedOutWaitingCallback, observer,
        getSequencerNumberCallback, observer, 3, 1, 0,
        AiaTaskPool( GetSystemTaskPool )() );
    TEST_ASSERT_NOT_NULL( sequencer );

    TEST_ASSERT_TRUE( AiaSequencer_Write( sequencer, "1", sizeof( "1" ) ) );
    TEST_ASSERT_TRUE( AiaSequencer_Write( sequencer, "2", sizeof( "2" ) ) );
    TEST_ASSERT_TRUE( AiaSequencer_Write( sequencer, "3", sizeof( "3" ) ) );

    TEST_ASSERT_EQUAL_STRING( "123", observer->messagesOutputted );

    AiaSequencer_ResetSequenceNumber( sequencer, 1 );

    TEST_ASSERT_TRUE( AiaSequencer_Write( sequencer, "2", sizeof( "2" ) ) );
    TEST_ASSERT_TRUE( AiaSequencer_Write( sequencer, "1", sizeof( "1" ) ) );
    TEST_ASSERT_TRUE( AiaSequencer_Write( sequencer, "3", sizeof( "3" ) ) );

    TEST_ASSERT_EQUAL_STRING( "123123", observer->messagesOutputted );

    AiaSequencer_Destroy( sequencer );
    AiaTestSequencerObserver_Destroy( observer );
}

/**
 * Wrapper around messageSequencedCallback that invokes
 * AiaSequencer_ResetSequenceNumber from the @c messageSequencedCb callback.
 */
static void messageSequencedCallbackThatResetsSequenceNumber( void* message,
                                                              size_t size,
                                                              void* userData )
{
    messageSequencedCallback( message, size, userData );
    AiaSequenceNumber_t sequenceNumber;
    TEST_ASSERT_TRUE( getSequencerNumberCallback( &sequenceNumber, message,
                                                  size, userData ) );
    AiaTestSequencerObserver_t* sequencerObserver =
        (AiaTestSequencerObserver_t*)userData;
    AiaSequencer_ResetSequenceNumber( sequencerObserver->sequencer,
                                      sequenceNumber );
}

TEST( AiaSequencerTests,
      SequenceNumberResetsCorrectlyWhenCallingDuringEmission )
{
    AiaDurationMs_t timeout = 100;
    AiaSequenceNumber_t startingSequenceNumber = 1;
    size_t numSequenceSlots = 3;

    AiaTestSequencerObserver_t* observer = AiaTestSequencerObserver_Create();
    TEST_ASSERT_NOT_NULL( observer );
    AiaSequencer_t* sequencer = AiaSequencer_Create(
        messageSequencedCallbackThatResetsSequenceNumber, observer,
        timedOutWaitingCallback, observer, getSequencerNumberCallback, observer,
        numSequenceSlots, startingSequenceNumber, timeout,
        AiaTaskPool( GetSystemTaskPool )() );
    TEST_ASSERT_NOT_NULL( sequencer );
    observer->sequencer = sequencer;

    TEST_ASSERT_TRUE( AiaSequencer_Write( sequencer, "1", sizeof( "1" ) ) );
    TEST_ASSERT_EQUAL_STRING( "1", observer->messagesOutputted );
    /* Next expected sequence number should now be reset to 1. */

    TEST_ASSERT_TRUE( AiaSequencer_Write( sequencer, "1", sizeof( "1" ) ) );
    TEST_ASSERT_FALSE( AiaSemaphore( TimedWait )(
        &observer->timedOutWaitingSemaphore, timeout * 1.2 ) );
    TEST_ASSERT_EQUAL_STRING( "11", observer->messagesOutputted );

    AiaSequencer_Destroy( sequencer );
    AiaTestSequencerObserver_Destroy( observer );
}
