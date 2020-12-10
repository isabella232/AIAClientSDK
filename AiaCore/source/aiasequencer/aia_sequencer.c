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
 * @file aia_sequencer.c
 * @brief Implements functions for the AiaSequencer_t type.
 */

/* The config header is always included first. */
#include <aia_config.h>

#include <aiasequencer/aia_sequencer.h>
#include <aiasequencer/private/aia_sequencer.h>

#include AiaClock( HEADER )
#include AiaTaskPool( HEADER )

#include <inttypes.h>

static AiaTaskPoolJobStorage_t aiaSequencerMissingSequenceNumberJobStorage =
    AiaTaskPool( JOB_STORAGE_INITIALIZER );
static AiaTaskPoolJob_t aiaSequencerMissingSequenceNumberJob =
    AiaTaskPool( JOB_INITIALIZER );

static void aiaSequencerMissingSequenceNumberTimeoutRoutine(
    AiaTaskPool_t taskPool, AiaTaskPoolJob_t job, void* context )
{
    (void)taskPool;
    (void)job;
    AiaSequencer_t* sequencer = (AiaSequencer_t*)context;
    AiaAssert( sequencer );
    if( !sequencer )
    {
        AiaLogError( "Null sequencer" );
        return;
    }
    if( AiaAtomicBool_Load( &sequencer->waitingForMessage ) )
    {
        sequencer->timeoutExpiredCb( sequencer->timeoutExpiredUserData );
    }
}

/**
 * Emits and removes as many messages as possible from the front of the buffer
 * using the @c messageSequencedCb() function.
 *
 * @return The number of messages emitted from the buffer.
 */
static size_t AiaSequencer_EmitBuffer( AiaSequencer_t* sequencer )
{
    AiaAssert( sequencer );
    if( !sequencer )
    {
        AiaLogError( "Null sequencer" );
        return 0;
    }
    size_t numMessagesEmitted = 0;
    while( AiaSequencerBuffer_Size( sequencer->buffer ) > 0 )
    {
        if( AiaSequencerBuffer_IsOccupied( sequencer->buffer, 0 ) )
        {
            size_t messageSize = 0;
            AiaSequenceNumber_t nextSequenceNumber;
            void* buf_font =
                AiaSequencerBuffer_Front( sequencer->buffer, &messageSize );

            if( !sequencer->getSequenceNumberCb(
                    &nextSequenceNumber, buf_font, messageSize,
                    sequencer->getSequenceNumberUserData ) )
            {
                AiaLogWarn( "Failed to get the sequence number." );
                continue;
            }

            if( nextSequenceNumber == sequencer->nextExpectedSequenceNumber )
            {
                /* Note: ADSER-1585 Calls to @c
                 * AiaSequencer_ResetSequenceNumber() can occur on the same
                 * thread of execution as calls to @c messageSequencedCb().
                 * As such, it makes sense to increment prior to calling
                 * messageSequencedCb() to ensure any attempts to reset
                 * the the next expected sequence number don't get
                 * invalidated by an increment afterwards. */
                ++sequencer->nextExpectedSequenceNumber;
                sequencer->messageSequencedCb(
                    AiaSequencerBuffer_Front( sequencer->buffer, &messageSize ),
                    messageSize, sequencer->messageSequencedUserData );
                AiaSequencerBuffer_PopFront( sequencer->buffer );
                ++numMessagesEmitted;
            }
            else
            {
                /* We reached a nonempty buffer slot that's not the sequence
                number we are looking for next. */
                break;
            }
        }
        else
        {
            /* We reached an empty buffer slot, this is the sequence number we
            want to get next. */
            AiaSequencerBuffer_PopFront( sequencer->buffer );
            break;
        }
    }

    /* If anything was emitted, stop the missing sequence timer. */
    if( numMessagesEmitted )
    {
        AiaAtomicBool_Clear( &sequencer->waitingForMessage );
    }

    /* If anything remains buffered and could not be emitted, start the missing
    sequence timer. */
    if( sequencer->sequenceTimeoutMs &&
        AiaSequencerBuffer_Size( sequencer->buffer ) > 0 )
    {
        AiaAtomicBool_Set( &sequencer->waitingForMessage );
        AiaTaskPoolError_t error = AiaTaskPool( CreateJob )(
            aiaSequencerMissingSequenceNumberTimeoutRoutine, sequencer,
            &aiaSequencerMissingSequenceNumberJobStorage,
            &aiaSequencerMissingSequenceNumberJob );
        if( !AiaTaskPoolSucceeded( error ) )
        {
            AiaLogError( "Failed to start timer, error=%d", error );
            AiaAtomicBool_Clear( &sequencer->waitingForMessage );
            return numMessagesEmitted;
        }

        error = AiaTaskPool( ScheduleDeferred )(
            sequencer->taskPool, aiaSequencerMissingSequenceNumberJob,
            sequencer->sequenceTimeoutMs );
        if( !AiaTaskPoolSucceeded( error ) )
        {
            AiaLogError( "Failed to start timer, error=%d", error );
            AiaAtomicBool_Clear( &sequencer->waitingForMessage );
            return numMessagesEmitted;
        }
    }

    return numMessagesEmitted;
}

AiaSequencer_t* AiaSequencer_Create(
    AiaSequencerMessageSequencedCallback_t messageSequencedCb,
    void* messageSequencedUserData,
    AiaSequencerTimeoutExpiredCallback_t timeoutExpiredCb,
    void* timeoutExpiredUserData,
    AiaSequencerGetSequencerNumberCallback_t getSequenceNumberCb,
    void* getSequenceNumberUserData, size_t maxSlots,
    AiaSequenceNumber_t startingSequenceNumber, uint32_t sequenceTimeoutMs,
    AiaTaskPool_t taskPool )
{
    if( !messageSequencedCb )
    {
        AiaLogError( "Null messageSequencedCb." );
        return NULL;
    }
    if( !timeoutExpiredCb )
    {
        AiaLogError( "Null timeoutExpiredCb." );
        return NULL;
    }
    if( !getSequenceNumberCb )
    {
        AiaLogError( "Null getSequenceNumberCb." );
        return NULL;
    }

    AiaSequencer_t* sequencer =
        (AiaSequencer_t*)AiaCalloc( 1, sizeof( AiaSequencer_t ) );
    if( !sequencer )
    {
        AiaLogError( "AiaCalloc failed, bytes=%zu.", sizeof( AiaSequencer_t ) );
        return NULL;
    }

    sequencer->buffer = AiaSequencerBuffer_Create( maxSlots );
    if( !sequencer->buffer )
    {
        AiaLogError( "Failed to initialize sequencing buffer." );
        AiaFree( sequencer );
        return NULL;
    }

    sequencer->nextExpectedSequenceNumber = startingSequenceNumber;
    sequencer->messageSequencedCb = messageSequencedCb;
    sequencer->messageSequencedUserData = messageSequencedUserData;
    sequencer->timeoutExpiredCb = timeoutExpiredCb;
    sequencer->timeoutExpiredUserData = timeoutExpiredUserData;
    sequencer->getSequenceNumberCb = getSequenceNumberCb;
    sequencer->getSequenceNumberUserData = getSequenceNumberUserData;
    sequencer->sequenceTimeoutMs = sequenceTimeoutMs;
    sequencer->taskPool = taskPool;

    AiaAtomicBool_Clear( &sequencer->waitingForMessage );
    return sequencer;
}

bool AiaSequencer_Write( AiaSequencer_t* sequencer, void* message, size_t size )
{
    AiaAssert( sequencer );
    if( !sequencer )
    {
        AiaLogError( "Null sequencer" );
        return false;
    }
    if( !message )
    {
        AiaLogError( "Null message" );
        return false;
    }

    AiaSequenceNumber_t incomingSequenceNumber;
    if( !sequencer->getSequenceNumberCb(
            &incomingSequenceNumber, message, size,
            sequencer->getSequenceNumberUserData ) )
    {
        AiaLogError( "Failed to get the sequence number." );
        return false;
    }
    AiaLogDebug( "Received sequence number %" PRIu32 ", expected %" PRIu32,
                 incomingSequenceNumber,
                 sequencer->nextExpectedSequenceNumber );

    if( incomingSequenceNumber == sequencer->nextExpectedSequenceNumber )
    {
        /* Note: ADSER-1585 Calls to @c
         * AiaSequencer_ResetSequenceNumber() can occur on the same
         * thread of execution as calls to @c messageSequencedCb().
         * As such, it makes sense to increment prior to calling
         * messageSequencedCb() to ensure any attempts to reset
         * the the next expected sequence number don't get
         * invalidated by an increment afterwards. */
        ++sequencer->nextExpectedSequenceNumber;

        /* Try to emit the message. */
        sequencer->messageSequencedCb( message, size,
                                       sequencer->messageSequencedUserData );
        /* Emit any messages we have leftover in our buffer in case we have the
        next expected sequence numbers. */
        size_t numMessagesEmitted = AiaSequencer_EmitBuffer( sequencer );
        AiaLogDebug( "Emitted %zu additional messages", numMessagesEmitted );
        return true;
    }

    /* Handle sequence numbers which wrap around the maximum unsigned 32-bit
    value. This is done by computing the distance between the
    incomingSequenceNumber and the nextExpectedSequenceNumber and taking
    shorter of the two as the actual distance. If the shorter of the two
    distance make the message and old message, we want to mark it as old
    message to be dropped. Otherwise, this is a future message that needed to
    be buffered. */
    bool isOldMessage = false;
    uint32_t messageDistance;
    if( incomingSequenceNumber < sequencer->nextExpectedSequenceNumber )
    {
        messageDistance =
            sequencer->nextExpectedSequenceNumber - incomingSequenceNumber;
        uint64_t messageDistanceWithWrap =
            (uint64_t)incomingSequenceNumber + UINT32_MAX + 1 -
            sequencer->nextExpectedSequenceNumber;
        if( messageDistance < messageDistanceWithWrap )
        {
            /* This is the normal case where we got a message with a sequence
            number that has already been skipped. We'l just drop it. */
            isOldMessage = true;
        }
        else
        {
            /* This happens when the incoming sequence number wraps around
            and starts at 0 again, while the next expected sequence number
            is still around the maximum.
            Instead of treating this as a old message and dropping it,
            we need to compute its wrapped buffer position and add it
            to the buffer as a future message. */
            messageDistance = messageDistanceWithWrap;
        }
    }
    else
    {
        messageDistance =
            incomingSequenceNumber - sequencer->nextExpectedSequenceNumber;
        uint64_t messageDistanceWithWrap =
            (uint64_t)sequencer->nextExpectedSequenceNumber + UINT32_MAX + 1 -
            incomingSequenceNumber;
        if( messageDistance > messageDistanceWithWrap )
        {
            /* This happen when the next expected sequence number wraps
            around and starts at 0 again, but we got an old message near
            the maximum.
            Instead of pushing the buffer forward to contain this "future"
            message, we want to treat this as an old message and drop it. */
            isOldMessage = true;
        }
        else
        {
            /* Normal case */
        }
    }

    if( isOldMessage )
    {
        AiaLogDebug( "Old message" );
        return true;
    }

    /* If we got here, we're now officially waiting on a missing sequence
    number. */
    if( sequencer->sequenceTimeoutMs &&
        !AiaAtomicBool_Load( &sequencer->waitingForMessage ) )
    {
        AiaAtomicBool_Set( &sequencer->waitingForMessage );

        AiaTaskPoolError_t error = AiaTaskPool( CreateJob )(
            aiaSequencerMissingSequenceNumberTimeoutRoutine, sequencer,
            &aiaSequencerMissingSequenceNumberJobStorage,
            &aiaSequencerMissingSequenceNumberJob );
        if( !AiaTaskPoolSucceeded( error ) )
        {
            AiaLogError( "Failed to start timer, error=%d", error );
            AiaAtomicBool_Clear( &sequencer->waitingForMessage );
            return false;
        }

        error = AiaTaskPool( ScheduleDeferred )(
            sequencer->taskPool, aiaSequencerMissingSequenceNumberJob,
            sequencer->sequenceTimeoutMs );
        if( !AiaTaskPoolSucceeded( error ) )
        {
            AiaLogError( "Failed to start timer, error=%d", error );
            AiaAtomicBool_Clear( &sequencer->waitingForMessage );
            return false;
        }
    }

    AiaLogInfo( "Message sequence number distance from expected=%" PRIu32,
                messageDistance );

    /* Future message, attempt to fit it in the buffer.
    Offset the buffer to start at 0. */
    size_t bufferIndex = messageDistance - 1;

    if( !AiaSequencerBuffer_Add( sequencer->buffer, message, size,
                                 bufferIndex ) )
    {
        /* Quitely drop and hope it comes again. Otherwise, timeout will kick in
         * down the road. */
        /* TODO: ADSER-1843 Add different types of enums to more finely
         * granularly notify the caller of the specific error. */
        AiaLogError( "AiaSequencerBuffer_Add failed" );
        return false;
    }
    return true;
}

void AiaSequencer_ResetSequenceNumber(
    AiaSequencer_t* sequencer,
    AiaSequenceNumber_t newNextExpectedSequenceNumber )
{
    AiaAssert( sequencer );
    if( !sequencer )
    {
        AiaLogError( "Null sequencer" );
        return;
    }
    AiaLogInfo(
        "AiaSequencer_ResetSequenceNumber, "
        "currentNextExpectedSequenceNumber=%" PRIu32
        ", "
        "newNextExpectedSequenceNumber=%" PRIu32,
        sequencer->nextExpectedSequenceNumber, newNextExpectedSequenceNumber );
    sequencer->nextExpectedSequenceNumber = newNextExpectedSequenceNumber;
}

void AiaSequencer_Destroy( AiaSequencer_t* sequencer )
{
    if( !sequencer )
    {
        AiaLogDebug( "Null sequencer." );
        return;
    }

    if( aiaSequencerMissingSequenceNumberJob )
    {
        AiaTaskPoolError_t error = AiaTaskPool( TryCancel )(
            sequencer->taskPool, aiaSequencerMissingSequenceNumberJob, NULL );
        if( !AiaTaskPoolSucceeded( error ) )
        {
            AiaLogWarn( "AiaTaskPool( TryCancel ) failed, error=%s",
                        AiaTaskPool( strerror )( error ) );
        }
    }

    AiaAtomicBool_Clear( &sequencer->waitingForMessage );
    AiaSequencerBuffer_Destroy( sequencer->buffer );
    AiaFree( sequencer );
    sequencer = NULL;
}
