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
 * @file aia_regulator_buffer.h
 * @brief User-facing functions of the @c AiaRegulatorBuffer_t type.
 */

#ifndef PRIVATE_AIA_REGULATOR_BUFFER_H_
#define PRIVATE_AIA_REGULATOR_BUFFER_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <aiaregulator/aia_regulator.h>

/**
 * This class buffers provided message chunks into a deque, and aggregates them
 * in the buffer, using the provided @c AiaRegulatorBuffer_PushBack() function,
 * if the combined size is less than the provided maximum message size. When a
 * message is requested from this buffer class, it will immediately remove the
 * chunks in the front of the buffer, regardless of how close they are to the
 * maximum message size. This class is intended to be used as an aggregating
 * buffer to help maximize the size of the message send from the Regulator to
 * the MQTT Publisher. This class is not thread safe.
 *
 */
typedef struct AiaRegulatorBuffer AiaRegulatorBuffer_t;

/**
 * Allocates and initializes a new @c AiaRegulatorBuffer_t.  An @c
 * AiaRegulatorBuffer_t created by this function should later be released by a
 * call to @c AiaRegulatorBuffer_Destroy().
 *
 * @param maxMessageSize Maximum size of message the RegulatorBuffer will emit.
 * @return the new @c AiaRegulatorBuffer_t if successful, else @c NULL.
 */
AiaRegulatorBuffer_t* AiaRegulatorBuffer_Create( const size_t maxMessageSize );

/**
 * Releases a @c AiaRegulatorBuffer_t previously allocated by @c
 * AiaRegulatorBuffer_Create().
 *
 * @param regulatorBuffer The regulator buffer instance to act on.
 * @param destroyChunk A callback which may be used to clean up any chunks
 * remaining in the buffer.  This callback is optional (passing @c NULL is
 * valid) if the caller has another mechanism for managing the lifecycle of the
 * chunks in the buffer.
 * @param destroyChunkUserData An optional user data pointer which will be
 * passed to the @c destroyChunk callback.
 */
void AiaRegulatorBuffer_Destroy(
    AiaRegulatorBuffer_t* regulatorBuffer,
    AiaRegulatorDestroyChunkCallback_t destroyChunk,
    void* destroyChunkUserData );

/**
 * Add message chunk to the end of the buffer.
 *
 * @param regulatorBuffer The regulator buffer instance to act on.
 * @param chunk Message chunk to be written.  When this function returns @c
 * true, ownership of chunk is transferred to the @c AiaRegulatorBuffer_t, and
 * the chunk must not be destroyed until it has been removed by a call to @c
 * AiaRegulatorBuffer_RemoveFront().
 * @return @c true if the chunk was accepted, else @c false.  As noted above,
 * returning @c true indicates that the buffer is taking ownership of @c chunk,
 * and chunk should not be destroyed until it has been removed by a call to @c
 * AiaRegulatorBuffer_RemoveFront().
 */
bool AiaRegulatorBuffer_PushBack( AiaRegulatorBuffer_t* regulatorBuffer,
                                  AiaRegulatorChunk_t* chunk );

/**
 * Remove chunks from the front of the buffer such that the message chunks
 * add up to @c maxMessageSize.
 *
 * @param regulatorBuffer The regulator buffer instance to act on.
 * @param emitMessageChunk A callback to use for emitting the chunks which are
 * being removed.  This callback is required and must not be @c NULL.
 * @param emitMessageChunkUserData Optional user data to pass to @c
 * emitMessageChunk.
 * @return @c true if chunks were removed successfully, else @c false.
 *
 * @note This function can fail partway through emitting a series of chunks
 * (this can occur if @c emitMessageChunk returns @c false).
 * @note This function may return success without making any calls to @c
 * emitMessageChunk if there are no chunks buffered or if the buffered chunks
 * will not fit in @c maxMessageSize.
 */
bool AiaRegulatorBuffer_RemoveFront(
    AiaRegulatorBuffer_t* regulatorBuffer,
    AiaRegulatorEmitMessageChunkCallback_t emitMessageChunk,
    void* emitMessageChunkUserData );

/**
 * @param regulatorBuffer The regulator buffer instance to act on.
 * @return @c true if the buffer is empty, else @c false.
 */
bool AiaRegulatorBuffer_IsEmpty( const AiaRegulatorBuffer_t* regulatorBuffer );

/**
 * Removes and returns any buffered data in the regulator without emitting it.
 *
 * @param regulatorBuffer The regulator buffer instance to act on.
 * @param destroyChunk A callback which may be used to clean up any chunks
 * remaining in the buffer.  This callback is optional (passing @c NULL is
 * valid) if the caller has another mechanism for managing the lifecycle of the
 * chunks in the buffer.
 * @param destroyChunkUserData An optional user data pointer which will be
 * passed to the @c destroyChunk callback.
 */
void AiaRegulatorBuffer_Clear( AiaRegulatorBuffer_t* regulatorBuffer,
                               AiaRegulatorDestroyChunkCallback_t destroyChunk,
                               void* destroyChunkUserData );

/**
 * @param regulatorBuffer The regulator buffer instance to act on.
 * @return The maximum message size emitted by this buffer.
 */
size_t AiaRegulatorBuffer_GetMaxMessageSize(
    const AiaRegulatorBuffer_t* regulatorBuffer );

/**
 * @param regulatorBuffer The regulator buffer instance to act on.
 * @return The aggregate size of the data (payload) in the chunks currently
 * queued.
 */
size_t AiaRegulatorBuffer_GetSize( const AiaRegulatorBuffer_t* reguatorBuffer );

/**
 * Checks whether the buffer contains enough data to completely fill a message
 * to @c _maxMessageSize.  Specifically, it may not be
 * possible to exactly fill the message, but this function will return true if
 * the buffer contains at least enough bytes to fill the message.  The actual
 * size of the message returned by a call to @c AiaRegulatorBuffer_RemoveFront()
 * may be less than full due to the granularity of the chunks, but it will be
 * the largest message possible without exceeding @c maxMessageSize.
 *
 * @note The underlying logic for this function uses the aggregate data
 * (payload) size and does not include overhead from headers.  As such, this
 * function will be slightly pessimistic, and may continue to return @c false
 * when enough chunks exist to fill an MQTT message using both data and
 * overhead, and will only start returning @c true when enough data alone is
 * accumulated to reach the max message size.
 *
 * @param regulatorBuffer The regulator buffer instance to act on.
 * @return @c true if the buffer can fill a message, else @c false.
 */
bool AiaRegulatorBuffer_CanFillMessage(
    const AiaRegulatorBuffer_t* regulatorBuffer );

#endif /* ifndef PRIVATE_AIA_REGULATOR_BUFFER_H_ */
