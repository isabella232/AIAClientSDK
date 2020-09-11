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
 * @brief Internal header for the AiaSequencer_t type. This header should not
 * be directly included in typical application code.
 */

#ifndef PRIVATE_AIA_SEQUENCER_H_
#define PRIVATE_AIA_SEQUENCER_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <aiasequencer/aia_sequencer.h>
#include <aiasequencer/private/aia_sequencer_buffer.h>

#include AiaTaskPool( HEADER )

/**
 * Underlying struct that contains all the data required to present the @c
 * AiaSequencer_t abstraction.
 */
struct AiaSequencer
{
    /** Callback to notify users of sequenced messages. */
    AiaSequencerMessageSequencedCallback_t messageSequencedCb;

    /** User data to pass to @c messageSequencedCb. */
    void* messageSequencedUserData;

    /** Callback to notify users that a message did not arrive within @c
     * sequenceTimeoutMs. */
    AiaSequencerTimeoutExpiredCallback_t timeoutExpiredCb;

    /** User data to pass to @c timeoutExpiredCb. */
    void* timeoutExpiredUserData;

    /** Callback to parse a sequence number from a message. */
    AiaSequencerGetSequencerNumberCallback_t getSequenceNumberCb;

    /** User data to pass to @c getSequenceNumberCb. */
    void* getSequenceNumberUserData;

    /** The next expected sequence number. */
    uint32_t nextExpectedSequenceNumber;

    /** The underlying buffer used to re-order out of sequence messages. Note
     * that this buffer does not dynamically expand and contract based on
     * sequencing needs but is fixed to to @c maxSlots from start. */
    AiaSequencerBuffer_t* buffer;

    /** Amount of time to wait for a missing message before making a call to @c
     * timeoutExpiredCb. */
    AiaDurationMs_t sequenceTimeoutMs;

    /** Taskpool used to schedule jobs for waiting on a missing sequence number.
     */
    AiaTaskPool_t taskPool;

    /** Boolean which is used to track the state of @c
     * missingSequenceNumberTimer. */
    AiaAtomicBool_t waitingForMessage;
};

#endif /* ifndef PRIVATE_AIA_SEQUENCER_H_ */
