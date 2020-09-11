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
 * @file aia_test_utilities.h
 * @brief Shared test utilities for unit tests.
 */

#ifndef AIA_TEST_UTILITIES_H_
#define AIA_TEST_UTILITIES_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <aiamockregulator/aia_mock_regulator.h>

#include <aiacore/aia_json_message.h>

/**
 * Callback used to clean up @c AiaJsonMessage_t instances.
 *
 * @param chunk The chunk to destroy.
 * @param userData Unused/ignored.
 */
void AiaTestUtilities_DestroyJsonChunk( AiaMessage_t* chunk, void* userData );

/** Struct to pass a function pointer and userdata for destroying a chunk. */
typedef struct AiaTestUtilities_DestroyCall
{
    /** The function to call to destroy a chunk. */
    void ( *callback )( AiaMessage_t* chunk, void* userData );

    /** User data to pass to the callback. */
    void* userData;
} AiaTestUtilities_DestroyCall_t;

/**
 * Wrapper for @c DestroyJsonChunk that also tests @c userData.
 *
 * @param chunk The chunk to destroy.
 * @param userData The function to destroy it with.
 */
void AiaTestUtilities_DestroyJsonChunkWithUserData( AiaMessage_t* chunk,
                                                    void* userData );

/**
 * Utility function for creating a JSON message with a specific size.
 * The payload will be a string consisting of spaces to pad out to the requested
 * size.
 *
 * @param totalSize The total size (in bytes) of the desired JSON message.
 * @return The new JSON message, or NULL if there is an error.
 */
AiaJsonMessage_t* AiaTestUtilities_CreateJsonMessage( size_t totalSize );

/**
 * Callback used to clean up @c AiaBinaryMessage_t instances.
 *
 * @param chunk The chunk to destroy.
 * @param userData Unused/ignored.
 */
void AiaTestUtilities_DestroyBinaryChunk( AiaMessage_t* chunk, void* userData );

/**
 * Used to pull a message out of the given @c regulator and assert that
 * it is an @c MALFORMED_MESSAGE @c AIA_EVENTS_EXCEPTION_ENCOUNTERED event
 *
 * @param sequenceNumber The sequence number to verify is embedded in the
 * "message" portion of the event.
 * @param index The index to verify is embedded in the "message" portion of the
 * event.
 */
void AiaTestUtilities_TestMalformedMessageExceptionIsGenerated(
    AiaMockRegulator_t* regulator, AiaSequenceNumber_t sequenceNumber,
    size_t index );

/**
 * Used to pull a message out of the given @c regulator and assert that
 * it is an @c INTERNAL_ERROR @c AIA_EVENTS_EXCEPTION_ENCOUNTERED event
 */
void AiaTestUtilities_TestInternalExceptionExceptionIsGenerated(
    AiaMockRegulator_t* regulator );

#endif /* ifndef AIA_TEST_UTILITIES_H_ */
