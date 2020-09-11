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
 * @file aia_regulator.c
 * @brief Implements functions for the AiaRegulator_t type.
 */

/* The config header is always included first. */
#include <aia_config.h>

#include AiaClock( HEADER )
#include AiaMutex( HEADER )
#include AiaTimer( HEADER )

#include <aiaregulator/aia_regulator.h>
#include <aiaregulator/private/aia_regulator_buffer.h>

/** Private data for the @c AiaRegulator_t type. */
struct AiaRegulator
{
    /**
     * Minimum amount of time the regulator will wait between emitted messages.
     */
    const AiaDurationMs_t minWaitTimeMs;

    /** Function for emitting the message chunks from the Regulator. */
    const AiaRegulatorEmitMessageChunkCallback_t emitMessageChunk;

    /** User data for @c emitMessageChunk callback. */
    void* const emitMessageChunkUserData;

    /** Mutex for synchronizing access to variables in the group below. */
    AiaMutex_t mutex;

    /** @name Variables synchronized by mutex. */
    /** @{ */

    /** Mode to use for buffering/emitting messages. */
    AiaRegulatorEmitMode_t emitMode;

    /** Data structure for buffering the message chunks. */
    AiaRegulatorBuffer_t* buffer;

    /** Timestamp tracking when the last message was emitted. */
    AiaTimepointMs_t lastEmitTimestampMs;

    /** Timestamp tracking when the oldest buffered data was written. */
    AiaTimepointMs_t firstWriteTimestampMs;

    /** @} */

    /** Timer which emits the buffer. */
    AiaTimer_t timer;
};

/**
 * Called by @c regulator->timer to emit messages.
 *
 * @param regulator Pointer to the regulator instance to act on.
 *
 * @note Caller must be holding @c regulator->mutex while calling this function.
 */
static void AiaRegulator_EmitMessageLocked( AiaRegulator_t* regulator )
{
    if( AiaClock( GetTimeMs )() - regulator->lastEmitTimestampMs <
        regulator->minWaitTimeMs )
    {
        /* Check to ensure we wait the minimum amount of time between emissions.
         * This acts as a guard to protect from underlying timer bugs. */
        return;
    }
    if( AiaRegulatorBuffer_IsEmpty( regulator->buffer ) )
    {
        return;
    }

    if( !AiaRegulatorBuffer_RemoveFront( regulator->buffer,
                                         regulator->emitMessageChunk,
                                         regulator->emitMessageChunkUserData ) )
    {
        AiaLogError( "Failed to remove a message from the buffer." );
        return;
    }

    regulator->lastEmitTimestampMs = AiaClock( GetTimeMs )();
}

/**
 * Called by @c regulator->timer to emit messages.
 *
 * @param userData Pointer to the regulator instance to act on.
 */
static void AiaRegulator_EmitMessage( void* userData )
{
    AiaRegulator_t* regulator = (AiaRegulator_t*)userData;
    AiaMutex( Lock )( &regulator->mutex );
    AiaRegulator_EmitMessageLocked( regulator );
    AiaMutex( Unlock )( &regulator->mutex );
}

/**
 * Causes the regulator to (re)start emitting messages, and continue until the
 * buffer empties.
 *
 * @param regulator The regulator instance to act on.
 * @return @c true if emitting (re)started successfully, else @c false.
 *
 * @note Caller must be holding @c regulator->mutex while calling this function.
 */
static bool AiaRegulator_StartEmittingLocked( AiaRegulator_t* regulator )
{
    /* Gather some timing data. */
    AiaTimepointMs_t now = AiaClock( GetTimeMs )();
    AiaDurationMs_t timeSinceEmitMs = now - regulator->lastEmitTimestampMs;
    AiaDurationMs_t timeSinceWriteMs = now - regulator->firstWriteTimestampMs;

    /* Assume we don't need to wait to emit, and then refine below. */
    AiaDurationMs_t delay = 0;

    /* If it hasn't been long enough since the last emit, we need to wait at
     * *least* that long. */
    if( timeSinceEmitMs < regulator->minWaitTimeMs )
    {
        delay = regulator->minWaitTimeMs - timeSinceEmitMs;
    }

    /* If we're in burst mode and we can't fill a message yet and it hasn't been
     * long enough since the first write, we can extend the delay. */
    bool extendDelay =
        AIA_REGULATOR_BURST == regulator->emitMode &&
        !AiaRegulatorBuffer_CanFillMessage( regulator->buffer ) &&
        timeSinceWriteMs < regulator->minWaitTimeMs &&
        timeSinceWriteMs < timeSinceEmitMs;
    if( extendDelay )
    {
        delay = regulator->minWaitTimeMs - timeSinceWriteMs;
    }

    if( !AiaTimer( Arm )( &regulator->timer, delay, regulator->minWaitTimeMs ) )
    {
        AiaLogError( "Failed to start timer." );
        return false;
    }

    return true;
}

/**
 * Causes the regulator to (re)start emitting messages, and continue until the
 * buffer empties.
 *
 * @param regulator The regulator instance to act on.
 * @return @c true if emitting started successfully, else @c false.
 */
static bool AiaRegulator_StartEmitting( AiaRegulator_t* regulator )
{
    AiaMutex( Lock )( &regulator->mutex );
    bool result = AiaRegulator_StartEmittingLocked( regulator );
    AiaMutex( Unlock )( &regulator->mutex );
    return result;
}

/**
 * Writes a message chunk to the regulator.
 *
 * @param regulator The regulator instance to act on.
 * @param chunk Message chunk to be written.
 * @return @c true if the message chunk was succesfully added, else @c false.
 *
 * @note Caller must be holding @c regulator->mutex while calling this function.
 */
static bool AiaRegulator_WriteLocked( AiaRegulator_t* regulator,
                                      AiaRegulatorChunk_t* chunk )
{
    /* Update the write timestamp if this is the first write to an empty buffer.
     */
    if( AiaRegulatorBuffer_IsEmpty( regulator->buffer ) )
    {
        regulator->firstWriteTimestampMs = AiaClock( GetTimeMs )();
    }

    /* Queue the chunks. */
    /* TODO: failure may mean we're full and need to retry (ADSER-1496). */
    if( !AiaRegulatorBuffer_PushBack( regulator->buffer, chunk ) )
    {
        AiaLogError( "Failed to push chunk onto queue." );
        return false;
    }

    return true;
}

AiaRegulator_t* AiaRegulator_Create(
    size_t maxMessageSize,
    const AiaRegulatorEmitMessageChunkCallback_t emitMessageChunk,
    void* emitMessageChunkUserData, AiaDurationMs_t minWaitTimeMs )
{
    if( maxMessageSize == 0 )
    {
        AiaLogError( "Zero maxMessageSize." );
        return NULL;
    }

    if( !emitMessageChunk )
    {
        AiaLogError( "Null emitMessageChunk." );
        return NULL;
    }

    AiaRegulator_t* regulator =
        (AiaRegulator_t*)AiaCalloc( 1, sizeof( AiaRegulator_t ) );
    if( !regulator )
    {
        AiaLogError( "AiaCalloc failed." );
        return NULL;
    }

    *(AiaDurationMs_t*)&regulator->minWaitTimeMs = minWaitTimeMs;
    *(AiaRegulatorEmitMessageChunkCallback_t*)&regulator->emitMessageChunk =
        emitMessageChunk;
    *(void**)&regulator->emitMessageChunkUserData = emitMessageChunkUserData;
    if( !AiaMutex( Create )( &regulator->mutex, false ) )
    {
        AiaLogError( "AiaMutex(Create) failed." );
        AiaFree( regulator );
        return NULL;
    }
    regulator->emitMode = AIA_REGULATOR_TRICKLE;
    if( !AiaTimer( Create )( &regulator->timer, AiaRegulator_EmitMessage,
                             regulator ) )
    {
        AiaLogError( "AiaRegulatorBuffer_Create failed (maxMessageSize=%zu).",
                     maxMessageSize );
        AiaMutex( Destroy )( &regulator->mutex );
        AiaFree( regulator );
        return NULL;
    }

    /* Initialize the buffer last so that we don't have any need to try to clean
     * it up here.  Calling AiaRegulatorBuffer_Destroy is problematic because it
     * requires the caller to dispose of the removed chunks, and we're not
     * equipped to do that here. */
    regulator->buffer = AiaRegulatorBuffer_Create( maxMessageSize );
    if( !regulator->buffer )
    {
        AiaLogError( "AiaRegulatorBuffer_Create failed (maxMessageSize=%zu).",
                     maxMessageSize );
        AiaTimer( Destroy )( &regulator->timer );
        AiaMutex( Destroy )( &regulator->mutex );
        AiaFree( regulator );
        return NULL;
    }

    return regulator;
}

void AiaRegulator_Destroy( AiaRegulator_t* regulator,
                           AiaRegulatorDestroyChunkCallback_t destroyChunk,
                           void* destroyChunkUserData )
{
    if( !regulator )
    {
        AiaLogDebug( "Null regulator." );
        return;
    }

    AiaTimer( Destroy )( &regulator->timer );
    AiaRegulatorBuffer_Destroy( regulator->buffer, destroyChunk,
                                destroyChunkUserData );
    AiaMutex( Destroy )( &regulator->mutex );
    AiaFree( regulator );
}

bool AiaRegulator_Write( AiaRegulator_t* regulator, AiaRegulatorChunk_t* chunk )
{
    if( !regulator )
    {
        AiaLogError( "Null regulator." );
        return false;
    }
    if( !chunk )
    {
        AiaLogError( "Null chunk." );
        return false;
    }

    /* Write the chunks to m_buffer. */
    AiaMutex( Lock )( &regulator->mutex );
    bool result = AiaRegulator_WriteLocked( regulator, chunk );
    AiaMutex( Unlock )( &regulator->mutex );
    if( !result )
    {
        return false;
    }

    /* Restart the timer, which will schedule the next emit appropriately. */
    /* TODO: this is horribly inefficient; rework (ADSER-1497) */
    return AiaRegulator_StartEmitting( regulator );
}

void AiaRegulator_SetEmitMode( AiaRegulator_t* regulator,
                               AiaRegulatorEmitMode_t mode )
{
    AiaAssert( regulator );
    if( !regulator )
    {
        AiaLogError( "Null regulator." );
        return;
    }
    AiaMutex( Lock )( &regulator->mutex );
    if( regulator->emitMode == mode )
    {
        AiaLogDebug( "Emit mode already set (mode=%d).", mode );
    }
    regulator->emitMode = mode;
    AiaMutex( Unlock )( &regulator->mutex );
}
