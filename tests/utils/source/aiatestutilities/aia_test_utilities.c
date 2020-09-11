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
 * @file aia_test_utilities.c
 * @brief Shared test utilities for AIA unit tests.
 */

/* The config header is always included first. */
#include <aia_config.h>

/* Aia headers */
#include <aiacore/aia_binary_message.h>
#include <aiacore/aia_events.h>
#include <aiacore/aia_json_message.h>
#include <aiacore/aia_json_utils.h>
#include <aiacore/aia_message.h>
#include <aiatestutilities/aia_test_utilities.h>

/* Test framework includes. */
#include <unity_fixture.h>

void AiaTestUtilities_DestroyJsonChunk( AiaMessage_t* chunk, void* userData )
{
    (void)userData;
    TEST_ASSERT_NOT_NULL( chunk );
    AiaJsonMessage_Destroy( AiaJsonMessage_FromMessage( chunk ) );
}

void AiaTestUtilities_DestroyJsonChunkWithUserData( AiaMessage_t* chunk,
                                                    void* userData )
{
    TEST_ASSERT_NOT_NULL( chunk );
    TEST_ASSERT_NOT_NULL( userData );
    AiaTestUtilities_DestroyCall_t* call =
        (AiaTestUtilities_DestroyCall_t*)userData;
    call->callback( chunk, call->userData );
}

AiaJsonMessage_t* AiaTestUtilities_CreateJsonMessage( size_t totalSize )
{
    /* Measure minimum size of a JSON message. */
    AiaJsonMessage_t* jsonMessage = AiaJsonMessage_Create( "", "", "" );
    size_t minSize =
        AiaMessage_GetSize( AiaJsonMessage_ToConstMessage( jsonMessage ) );
    AiaJsonMessage_Destroy( jsonMessage );

    /* Create a message with the requested total size. */
    if( totalSize < minSize )
    {
        AiaLogError( "Requested size too small (%zu < %zu).", totalSize,
                     minSize );
        return NULL;
    }
    char payload[ totalSize - minSize + 1 ];
    memset( payload, ' ', sizeof( payload ) );
    payload[ sizeof( payload ) - 1 ] = '\0';
    return AiaJsonMessage_Create( "", "", payload );
}

void AiaTestUtilities_DestroyBinaryChunk( AiaMessage_t* chunk, void* userData )
{
    (void)userData;
    TEST_ASSERT_NOT_NULL( chunk );
    AiaBinaryMessage_Destroy( AiaBinaryMessage_FromMessage( chunk ) );
}

void AiaTestUtilities_TestMalformedMessageExceptionIsGenerated(
    AiaMockRegulator_t* regulator, AiaSequenceNumber_t sequenceNumber,
    size_t index )
{
    TEST_ASSERT_TRUE( AiaSemaphore( TryWait )( &regulator->writeSemaphore ) );
    AiaListDouble( Link_t )* link = NULL;
    link = AiaListDouble( PeekHead )( &regulator->writtenMessages );
    AiaListDouble( RemoveHead )( &regulator->writtenMessages );
    TEST_ASSERT_NOT_NULL( link );
    AiaJsonMessage_t* exceptionEncounteredMessage = AiaJsonMessage_FromMessage(
        ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
    TEST_ASSERT_EQUAL(
        strcmp( AiaJsonMessage_GetName( exceptionEncounteredMessage ),
                AIA_EVENTS_EXCEPTION_ENCOUNTERED ),
        0 );
    const char* payload =
        AiaJsonMessage_GetJsonPayload( exceptionEncounteredMessage );
    TEST_ASSERT_NOT_NULL( payload );

    const char* error = NULL;
    size_t errorLen = 0;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_EXCEPTION_ENCOUNTERED_ERROR_KEY,
        strlen( AIA_EXCEPTION_ENCOUNTERED_ERROR_KEY ), &error, &errorLen ) );
    TEST_ASSERT_NOT_NULL( error );

    const char* code = NULL;
    size_t codeLen = 0;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        error, errorLen, AIA_EXCEPTION_ENCOUNTERED_ERROR_CODE_KEY,
        strlen( AIA_EXCEPTION_ENCOUNTERED_ERROR_CODE_KEY ), &code, &codeLen ) );
    TEST_ASSERT_NOT_NULL( code );
    TEST_ASSERT_TRUE( AiaJsonUtils_UnquoteString( &code, &codeLen ) );
    TEST_ASSERT_EQUAL_STRING_LEN(
        AIA_EXCEPTION_ENCOUNTERED_MALFORMED_MESSAGE_CODE, code, codeLen );

    const char* message = NULL;
    size_t messageLen = 0;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_EXCEPTION_ENCOUNTERED_MESSAGE_KEY,
        strlen( AIA_EXCEPTION_ENCOUNTERED_MESSAGE_KEY ), &message,
        &messageLen ) );
    TEST_ASSERT_NOT_NULL( message );

    const char* sequenceNumberStr = NULL;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        message, messageLen,
        AIA_EXCEPTION_ENCOUNTERED_MESSAGE_SEQUENCE_NUMBER_KEY,
        strlen( AIA_EXCEPTION_ENCOUNTERED_MESSAGE_SEQUENCE_NUMBER_KEY ),
        &sequenceNumberStr, NULL ) );
    TEST_ASSERT_NOT_NULL( sequenceNumberStr );
    TEST_ASSERT_EQUAL( atoi( sequenceNumberStr ), sequenceNumber );

    const char* indexStr = NULL;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        message, messageLen, AIA_EXCEPTION_ENCOUNTERED_MESSAGE_INDEX_KEY,
        strlen( AIA_EXCEPTION_ENCOUNTERED_MESSAGE_INDEX_KEY ), &indexStr,
        NULL ) );
    TEST_ASSERT_NOT_NULL( indexStr );
    TEST_ASSERT_EQUAL( atoi( indexStr ), index );
}

void AiaTestUtilities_TestInternalExceptionExceptionIsGenerated(
    AiaMockRegulator_t* regulator )
{
    TEST_ASSERT_TRUE( AiaSemaphore( TryWait )( &regulator->writeSemaphore ) );
    AiaListDouble( Link_t )* link = NULL;
    link = AiaListDouble( PeekHead )( &regulator->writtenMessages );
    AiaListDouble( RemoveHead )( &regulator->writtenMessages );
    TEST_ASSERT_NOT_NULL( link );
    AiaJsonMessage_t* exceptionEncounteredMessage = AiaJsonMessage_FromMessage(
        ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
    TEST_ASSERT_EQUAL(
        strcmp( AiaJsonMessage_GetName( exceptionEncounteredMessage ),
                AIA_EVENTS_EXCEPTION_ENCOUNTERED ),
        0 );
    const char* payload =
        AiaJsonMessage_GetJsonPayload( exceptionEncounteredMessage );
    TEST_ASSERT_NOT_NULL( payload );

    const char* error = NULL;
    size_t errorLen = 0;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        payload, strlen( payload ), AIA_EXCEPTION_ENCOUNTERED_ERROR_KEY,
        strlen( AIA_EXCEPTION_ENCOUNTERED_ERROR_KEY ), &error, &errorLen ) );
    TEST_ASSERT_NOT_NULL( error );

    const char* code = NULL;
    size_t codeLen = 0;
    TEST_ASSERT_TRUE( AiaFindJsonValue(
        error, errorLen, AIA_EXCEPTION_ENCOUNTERED_ERROR_CODE_KEY,
        strlen( AIA_EXCEPTION_ENCOUNTERED_ERROR_CODE_KEY ), &code, &codeLen ) );
    TEST_ASSERT_NOT_NULL( code );
    TEST_ASSERT_TRUE( AiaJsonUtils_UnquoteString( &code, &codeLen ) );
    TEST_ASSERT_EQUAL_STRING_LEN( AIA_EXCEPTION_ENCOUNTERED_INTERNAL_ERROR_CODE,
                                  code, codeLen );
}
