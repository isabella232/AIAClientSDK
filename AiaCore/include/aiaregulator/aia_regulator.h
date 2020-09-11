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
 * @file aia_regulator.h
 * @brief User-facing functions of the @c AiaRegulator_t type.
 */

#ifndef AIA_REGULATOR_H_
#define AIA_REGULATOR_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <aiacore/aia_message.h>

/**
 * This class regulates the size of the message being emitted, as well as
 * allowing delays to be added in between the emits. It buffers the message
 * chunks and keeps track of the emit delay with a timer.
 *
 * @note Functions in this header which act on an @c AiaRegulator_t are
 * thread-safe.
 */
typedef struct AiaRegulator AiaRegulator_t;

/** The data element type being managed and grouped by a regulator. */
typedef AiaMessage_t AiaRegulatorChunk_t;

/**
 * This callback function is used to return a series of chunks when a message is
 * being emitted.  The callback may be called multiple times to emit a series of
 * chunks which can be grouped in a single MQTT message.  The
 * @c remainingBytes parameter indicates the cumulative number of bytes that
 * have not been emitted in the series yet; the final callback in a series will
 * always set @c remainingBytes to zero.
 *
 * @note This callback is expected to return quickly and should not block.
 *
 * @param chunkForMessage The message chunk to be emitted.  If this callback
 * returns true, ownership of @c chunkForMessage is transferred and this
 * callback is responsible for destroying it.
 * @param remainingBytes The cumulative number of bytes that
 * have not been emitted in the series yet; the final callback in a series will
 * always set @c remainingBytes to zero.
 * @param remainingChunks The number of chunks that have not been emitted in the
 * series yet; the final callback in a series will always set @c remainingChunks
 * to zero.
 * @param userData Optional user data pointer which was provided alongside the
 * callback.
 * @return @c true if emit succeeded, else @c false.  As noted above, returning
 * @c true indicates that the callback is taking ownership of @c
 * chunkForMessage, and is responsible for destroying it.
 */
typedef bool ( *AiaRegulatorEmitMessageChunkCallback_t )(
    AiaRegulatorChunk_t* chunkForMessage, size_t remainingBytes,
    size_t remainingChunks, void* userData );

/**
 * This callback function is used to clean up individual chunks when @c
 * AiaRegulator_Destroy() or @c AiaRegulatorBuffer_Clear() are called. The
 * regulator buffer does not have enough context to properly clean up chunks
 * directly, so it depends on this user callback to delegate cleanup.  More
 * specifically, this call transfers ownership of @c chunk to the callback, and
 * the callback is responsible for destroying @c chunk.
 *
 * @note This callback is expected to return quickly and should not block.
 *
 * @param chunk The message chunk to be destroyed.
 * @param userData Optional user data pointer which was provided alongside the
 * callback.
 */
typedef void ( *AiaRegulatorDestroyChunkCallback_t )(
    AiaRegulatorChunk_t* chunk, void* userData );

/** Controls how to balance the tradeoff between latency and message size. */
typedef enum
{
    /** In @c TRICKLE mode, emit any data it has as early as possible. */
    AIA_REGULATOR_TRICKLE,

    /**
     * In @c BURST mode, wait (up to some timeout) to accumulate data before
     * sending.
     */
    AIA_REGULATOR_BURST
} AiaRegulatorEmitMode_t;

/**
 * Allocates and initializes a new @c AiaRegulator_t.  An @c
 * AiaRegulator_t created by this function should later be released by a
 * call to @c AiaRegulator_Destroy().
 *
 * @param maxMessageSize Maximum size of message the regulator can emit.
 * @param emitMessageChunk Function for emitting a chunk of a message from the
 * regulator.  When emitting a single message, this function may be called
 * multiple times to emit all the chunks which have been aggregated.
 * @param emitMessageChunkUserData User data to pass to @c emitMessageChunk.
 * @param minWaitTimeMs Minimum amount of time the regulator will wait before
 *     emitting a message.
 * @return The newly-constructed regulator when successful, else @c NULL.
 *
 * @note: the default @c AiaRegulatorEmitMode is @c TRICKLE.
 */
AiaRegulator_t* AiaRegulator_Create(
    size_t maxMessageSize,
    AiaRegulatorEmitMessageChunkCallback_t emitMessageChunk,
    void* emitMessageChunkUserData, AiaDurationMs_t minWaitTimeMs );

/**
 * Releases a @c AiaRegulator_t previously allocated by @c
 * AiaRegulator_Create().
 *
 * @param regulator The regulator instance to act on.
 * @param destroyChunk A callback which may be used to clean up any chunks
 * remaining in the regulator.  This callback is optional (passing @c NULL is
 * valid) if the caller has another mechanism for managing the lifecycle of the
 * chunks in the .
 * @param destroyChunkUserData An optional user data pointer which will be
 * passed to the @c destroyChunk callback.
 */
void AiaRegulator_Destroy( AiaRegulator_t* regulator,
                           AiaRegulatorDestroyChunkCallback_t destroyChunk,
                           void* destroyChunkUserData );

/**
 * Writes a message chunk to the regulator.  Note that ownership of @c chunk
 * is transferred to @c regulator if this function call succeeds.
 *
 * @param regulator The regulator instance to act on.
 * @param chunk Message chunk to be written.
 * @return @c true if the message chunk was succesfully added, else @c false.
 */
bool AiaRegulator_Write( AiaRegulator_t* regulator,
                         AiaRegulatorChunk_t* chunk );

/**
 * Change the mode to use for emitting data.
 *
 * @param regulator The regulator instance to act on.
 * @param mode The mode to use when emitting data.
 *
 * @note: the default @c AiaRegulatorEmitMode is @c TRICKLE.
 */
void AiaRegulator_SetEmitMode( AiaRegulator_t* regulator,
                               AiaRegulatorEmitMode_t mode );

#endif /* ifndef AIA_REGULATOR_H_ */
