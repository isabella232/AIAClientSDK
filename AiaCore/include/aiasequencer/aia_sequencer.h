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
 * @file private/aia_sequencer.h
 * @brief User-facing functions of the @c AiaSequencer_t type.
 */

#ifndef AIA_SEQUENCER_H_
#define AIA_SEQUENCER_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <aiacore/aia_message_constants.h>

#include AiaTaskPool( HEADER )

/**
 * This class reorders incoming sequence messages in a push driven data flow.
 * The sequencer will only write/emit in response to the write, and all
 * callbacks occur on the caller's thread. Per the Aia specification, this
 * class represents sequence numbers using unsigned 32-bit integers and wraps
 * around back to 0 once the maximum 32-bit width sequence number is reached.
 * the sequencer will wrap around and continue to handle message accordingly.
 *
 * @note Functions in this header which act on an @c AiaSequencer_t are not
 * thread-safe.
 */
typedef struct AiaSequencer AiaSequencer_t;

/**
 * This callback is used to return a sequenced message to the user.
 *
 * @param message The message that was sequenced.
 * @param size The size of @c message.
 * @param userData Optional user data pointer which was provided alongside the
 * callback.
 *
 * @note This callback will only be made synchronously in response to a call to
 * @c AiaSequencer_Write() before returning.
 * @note Implementations are not expected to block. Doing so could block
 * processing additional processing by the @c AiaSequencer_t and the entire
 * system in a non-threaded environment.
 */
typedef void ( *AiaSequencerMessageSequencedCallback_t )( void* message,
                                                          size_t size,
                                                          void* userData );

/**
 * This callback is used to notify a user that the @c AiaSequencer_t has given
 * up waiting for a message to arrive after the @c sequenceTimeoutMs has
 * expired.
 *
 * @param userData Optional user data pointer which was provided alongside the
 * callback.
 *
 * @note This callback will be made on the thread servicing the implementation
 * of @c AiaClock.
 * @note Implementations are not expected to block. Doing so could block the the
 * @ AiaClock and the entire system in a non-threaded environment.
 */
typedef void ( *AiaSequencerTimeoutExpiredCallback_t )( void* userData );

/**
 * This callback is used to parse the sequence number associated with an
 * incoming message.
 *
 * @param[out] sequenceNumber Sequence number to parse and return.
 * @param message The message to be processed.
 * @param size The size of the message to be processed.
 * @param userData Optional user data pointer which was provided alongside the
 * callback.
 *
 * @return @c true if sequence number is retrieved successfully, else @c false.
 *
 * @note This callback will only be made synchronously in response to a call to
 * @c AiaSequencer_Write() before returning.
 * @note Implementations are not expected to block. Doing so could block
 * processing additional processing by the @c AiaSequencer_t and the entire
 * system in a non-threaded environment.
 */
typedef bool ( *AiaSequencerGetSequencerNumberCallback_t )(
    AiaSequenceNumber_t* sequenceNumber, void* message, size_t size,
    void* userData );

/**
 * Allocates and initializes a @c AiaSequencer_t object from the heap.
 * The returned pointer should be destroyed using @c
 * AiaSequencer_Destroy().
 *
 * @param messageSequencedCb Callback to notify users of sequenced messages.
 * @param messageSequencedUserData User data to pass to @c messageSequencedCb.
 * @param timeoutExpiredCb Callback to notify users that a message did not
 * arrive within @c sequenceTimeoutMs.
 * @param timeoutExpiredUserData User data to pass to @c timeoutExpiredCb.
 * @param getSequenceNumberCb Callback to parse a sequence number from a
 * message.
 * @param getSequenceNumberUserData User data to pass to @c getSequenceNumberCb.
 * @param maxSlots The maximum amount of slots to use for buffering when
 * sequencing messages. Note that each slot will be composed of a pointer to a
 * message, a boolean, and a @c AiaListDoubleLink_t.
 * @param startingSequenceNumber The first sequence number message to expect.
 * @param sequenceTimeoutMs The maximum amount of time to wait for a sequence
 * number before making a call to @c timeoutExpiredCb. Setting this to zero will
 * disable the timeout.
 * @param taskPool A taskpool used to schedule future jobs.
 * @return The newly created @c AiaSequencer_t if successful, or NULL
 * otherwise.
 */
AiaSequencer_t* AiaSequencer_Create(
    AiaSequencerMessageSequencedCallback_t messageSequencedCb,
    void* messageSequencedUserData,
    AiaSequencerTimeoutExpiredCallback_t timeoutExpiredCb,
    void* timeoutExpiredUserData,
    AiaSequencerGetSequencerNumberCallback_t getSequenceNumberCb,
    void* getSequenceNumberUserData, size_t maxSlots,
    AiaSequenceNumber_t startingSequenceNumber, uint32_t sequenceTimeoutMs,
    AiaTaskPool_t taskPool );

/* TODO: ADSER-1843 Provide callers more visibility into failures. */
/**
 * Adds a message to the sequencer to be sequenced for in-order emission. If the
 * message matches the expected message, a call to @c messageSequencedCb with @c
 * message will be made prior to returning. Otherwise, it will be buffered
 * internally and emitted later when the next expected message is written to.
 * @param sequencer The @c AiaSequencer_t to act on.
 * @param message The message to sequence. Note that the caller retains
 * ownership of @c message.
 * @param size The size of @c message.
 * @return @c false on any messages that failed to be buffered or processed
 * properly and @c true otherwise. Note that the failure case includes messages
 * with sequence numbers that fit beyond the maximum amount of slots in the
 * buffer and does not include the handling of duplicate messages.
 */
bool AiaSequencer_Write( AiaSequencer_t* sequencer, void* message,
                         size_t size );

/**
 * Resets the next expected sequence number to wait on.
 *
 * @param sequencer The @c AiaSequencer_t to act on.
 * @param newNextExpectedSequenceNumber The new next expected sequence number to
 * wait on.
 */
void AiaSequencer_ResetSequenceNumber(
    AiaSequencer_t* sequencer,
    AiaSequenceNumber_t newNextExpectedSequenceNumber );

/**
 * Uninitializes and deallocates an @c AiaSequencer_t previously created by
 * a call to
 * @c AiaSequencer_Create().
 *
 * @param sequencer The @c AiaSequencer_t to destroy.
 */
void AiaSequencer_Destroy( AiaSequencer_t* sequencer );

#endif /* ifndef AIA_SEQUENCER_H_ */
