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
 * @file aia_regulator_buffer.c
 * @brief Implements functions for the AiaRegulatorBuffer_t type.
 */

/* The config header is always included first. */
#include <aia_config.h>

#include <aiacore/aia_utils.h>
#include <aiaregulator/private/aia_regulator_buffer.h>

#define AiaChunks( MEMBER ) AiaListDouble( MEMBER )
#include AiaChunks( HEADER )

/** Dynamic container to hold a series of chunks. */
typedef AiaChunks( t ) AiaChunks_t;

/** Private data for the @c AiaRegulatorBuffer_t type. */
struct AiaRegulatorBuffer
{
    /** Maximum size of message the RegulatorBuffer emits. */
    const size_t maxMessageSize;

    /** Container for buffering the regulator_buffer chunks. */
    AiaChunks_t buffer;

    /**
     * Current aggregate size of the chunks queued.
     *
     * @note This tracks the aggregate data (payload) size of the chunks only,
     * and does not include any overhead from headers.  This variable is used
     * for two purposes:
     *     @li @c canFillMessage() uses it to check whether we have
     * enough data to fill a message.
     *     @li @c getSize() reports it for logging and testing purposes.
     */
    size_t bufferSize;
};

/** Linked list node for managing chunks. */
/* TODO: This could be combined with the AiaMessage type by embedding the
 * AiaChunks(Link_t) in it. */
typedef struct AiaRegulatorBufferNode
{
    /** The actual link in the list. */
    AiaChunks( Link_t ) link;

    /** The @c AiaMessage we're holding in the node. */
    AiaRegulatorChunk_t* chunk;
} AiaRegulatorBufferNode_t;

AiaRegulatorBuffer_t* AiaRegulatorBuffer_Create( const size_t maxMessageSize )
{
    AiaRegulatorBuffer_t* regulatorBuffer =
        (AiaRegulatorBuffer_t*)AiaCalloc( 1, sizeof( AiaRegulatorBuffer_t ) );
    if( !regulatorBuffer )
    {
        AiaLogError( "AiaCalloc failed." );
        return NULL;
    }

    *(size_t*)&regulatorBuffer->maxMessageSize = maxMessageSize;
    AiaChunks( Create )( &regulatorBuffer->buffer );

    return regulatorBuffer;
}

void AiaRegulatorBuffer_Destroy(
    AiaRegulatorBuffer_t* regulatorBuffer,
    AiaRegulatorDestroyChunkCallback_t destroyChunk,
    void* destroyChunkUserData )
{
    if( !regulatorBuffer )
    {
        AiaLogError( "Null regulatorBuffer." );
        return;
    }
    AiaRegulatorBuffer_Clear( regulatorBuffer, destroyChunk,
                              destroyChunkUserData );
    AiaFree( regulatorBuffer );
}

bool AiaRegulatorBuffer_PushBack( AiaRegulatorBuffer_t* regulatorBuffer,
                                  AiaRegulatorChunk_t* chunk )
{
    if( !regulatorBuffer )
    {
        AiaLogError( "Null regulatorBuffer." );
        return false;
    }
    if( !chunk )
    {
        AiaLogError( "Null chunk." );
        return false;
    }
    size_t chunkSize = AiaMessage_GetSize( chunk );
    if( chunkSize > regulatorBuffer->maxMessageSize )
    {
        AiaLogError( "Chunk data size is too big: expected=%zu, actual=%zu.",
                     regulatorBuffer->maxMessageSize, chunkSize );
        return false;
    }
    regulatorBuffer->bufferSize += chunkSize;

    /* Allocate a new list node to hold this chunk. */
    AiaRegulatorBufferNode_t* node =
        AiaCalloc( 1, sizeof( AiaRegulatorBufferNode_t ) );
    if( !node )
    {
        AiaLogError( "AiaCalloc failed." );
        return false;
    }
    AiaChunks( Link_t ) defaultLink = AiaChunks( LINK_INITIALIZER );
    node->link = defaultLink;
    node->chunk = chunk;

    /* Add it to the list. */
    AiaChunks( InsertTail )( &regulatorBuffer->buffer, &node->link );

    return true;
}

bool AiaRegulatorBuffer_RemoveFront(
    AiaRegulatorBuffer_t* regulatorBuffer,
    AiaRegulatorEmitMessageChunkCallback_t emitMessageChunk,
    void* emitMessageChunkUserData )
{
    /* TODO: Add support for automatically aggregating microphone messages so
     * that we don't have to put a header on each microphone chunk (ADSER-1632).
     */
    if( !regulatorBuffer )
    {
        AiaLogError( "Null regulatorBuffer." );
        return false;
    }
    if( !emitMessageChunk )
    {
        AiaLogError( "Null emitMessageChunk." );
        return false;
    }
    if( AiaChunks( IsEmpty )( &regulatorBuffer->buffer ) )
    {
        /* Not considering it an error to attempt to read from an empty buffer;
         * we simply don't emit any chunks. */
        return true;
    }

    /* Measure how many chunks there are and how large the cumulative chunks
     * will be. */
    AiaChunks( Link_t )* link = NULL;
    size_t cumulativeSize = 0;
    size_t maxMessageSize = regulatorBuffer->maxMessageSize;
    size_t numChunks = 0;
    AiaChunks( ForEach )( &regulatorBuffer->buffer, link )
    {
        AiaRegulatorBufferNode_t* node = (AiaRegulatorBufferNode_t*)link;
        AiaRegulatorChunk_t* chunk = node->chunk;
        size_t chunkSize = AiaMessage_GetSize( chunk );
        if( cumulativeSize + chunkSize > maxMessageSize )
        {
            break;
        }
        cumulativeSize += chunkSize;
        ++numChunks;
    }

    /* Emit the chunks */
    while( cumulativeSize > 0 )
    {
        link = AiaChunks( PeekHead )( &regulatorBuffer->buffer );

        /* Sanity check: buffer should be unchanged, so we should have a link.
         */
        AiaAssert( link );
        if( !link )
        {
            AiaLogError( "Buffer inconsistency detected." );
            return false;
        }

        AiaRegulatorBufferNode_t* node = (AiaRegulatorBufferNode_t*)link;
        AiaRegulatorChunk_t* chunk = node->chunk;
        size_t chunkSize = AiaMessage_GetSize( chunk );

        /* Sanity check: buffer should be unchanged, so sizes should add up the
         * same. */
        AiaAssert( cumulativeSize >= chunkSize );
        if( cumulativeSize < chunkSize )
        {
            AiaLogError( "Chunk size inconsistency detected." );
            return false;
        }

        /* Sanity check: should still have chunks remaining. */
        AiaAssert( numChunks > 0 );
        if( numChunks == 0 )
        {
            AiaLogError( "Chunk count inconsistency detected." );
            return false;
        }

        /* Emit the chunk (which transfers ownership if successful). */
        size_t remaining = cumulativeSize - chunkSize;
        if( !emitMessageChunk( chunk, remaining, --numChunks,
                               emitMessageChunkUserData ) )
        {
            AiaLogError(
                "Failed to emit message chunk (size=%zu, remaining=%zu, "
                "numChunks=%zu).",
                chunkSize, remaining, numChunks + 1 );
            return false;
        }

        /* Bookkeeping and cleanup now that we've successfully emitted. */
        cumulativeSize = remaining;
        regulatorBuffer->bufferSize -= chunkSize;
        AiaChunks( Remove )( link );
        AiaFree( node );
    }

    return true;
}

bool AiaRegulatorBuffer_IsEmpty( const AiaRegulatorBuffer_t* regulatorBuffer )
{
    AiaAssert( regulatorBuffer );
    return regulatorBuffer && AiaChunks( IsEmpty )( &regulatorBuffer->buffer );
}

void AiaRegulatorBuffer_Clear( AiaRegulatorBuffer_t* regulatorBuffer,
                               AiaRegulatorDestroyChunkCallback_t destroyChunk,
                               void* destroyChunkUserData )
{
    if( !destroyChunk )
    {
        /* If we're not destroying the chunks, we simply clear the buffer list
         * and trust that the caller has some other way of managing the memory
         * of the nodes. */
        AiaChunks( RemoveAll )( &regulatorBuffer->buffer, AiaFree, 0 );
    }
    else
    {
        /* If we're destroying the removed chunks, do so using the callback. */
        AiaChunks( Link_t )* link = NULL;
        while( ( link = AiaChunks( RemoveHead )( &regulatorBuffer->buffer ) ) !=
               NULL )
        {
            AiaRegulatorBufferNode_t* node = (AiaRegulatorBufferNode_t*)link;
            AiaRegulatorChunk_t* chunk = node->chunk;
            destroyChunk( chunk, destroyChunkUserData );
            AiaFree( node );
        }
    }

    /* Should be empty now. */
    AiaAssert( AiaChunks( IsEmpty )( &regulatorBuffer->buffer ) );
    regulatorBuffer->bufferSize = 0;
}

size_t AiaRegulatorBuffer_GetMaxMessageSize(
    const AiaRegulatorBuffer_t* regulatorBuffer )
{
    AiaAssert( regulatorBuffer );
    return regulatorBuffer ? regulatorBuffer->maxMessageSize : 0;
}

size_t AiaRegulatorBuffer_GetSize( const AiaRegulatorBuffer_t* regulatorBuffer )
{
    AiaAssert( regulatorBuffer );
    return regulatorBuffer ? regulatorBuffer->bufferSize : 0;
}

bool AiaRegulatorBuffer_CanFillMessage(
    const AiaRegulatorBuffer_t* regulatorBuffer )
{
    AiaAssert( regulatorBuffer );
    return regulatorBuffer &&
           regulatorBuffer->bufferSize >= regulatorBuffer->maxMessageSize;
}
