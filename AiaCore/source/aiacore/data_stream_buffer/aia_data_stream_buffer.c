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

/* The config header is always included first. */
#include <aia_config.h>

#include <aiacore/data_stream_buffer/aia_data_stream_buffer.h>
#include <aiacore/data_stream_buffer/aia_data_stream_buffer_reader.h>
#include <aiacore/data_stream_buffer/aia_data_stream_buffer_writer.h>
#include <aiacore/data_stream_buffer/private/aia_data_stream_buffer.h>
#include <aiacore/data_stream_buffer/private/aia_data_stream_buffer_reader.h>
#include <aiacore/data_stream_buffer/private/aia_data_stream_buffer_writer.h>

#include AiaMutex( HEADER )

#include <inttypes.h>

/**
 * This function rounds @c size up to a multiple of @c align.
 *
 * @param size The size to round up.
 * @param align The alignment to round to.
 * @return @c size rounded up to a multiple of @c align.
 */
static size_t _AiaDataStreamBuffer_AlignSizeTo( size_t size, size_t align )
{
    if( size )
    {
        return ( ( ( size - 1 ) / align ) + 1 ) * align;
    }
    else
    {
        return 0;
    }
}

/**
 * Allocates and initializes a @c AiaDataStreamReader_t object from the heap.
 * The returned pointer should be destroyed using @c
 * AiaDataStreamReader_Destroy().
 *
 * @param dataStream The @c AiaDataStreamBuffer_t to stream data from.
 * @param id The id to assign this reader.
 * @param policy The policy used to perform read operations for this reader.
 * @param startWithNewData Flag indicating that this @c AiaDataStreamReader_t
 * should start reading data which is written to the buffer after this @c
 * AiaDataStreamReader_t is created. If this parameter is set to false, the @c
 * AiaDataStreamReader_t will start with the oldest valid data in the buffer.
 * @param forceReplacement If set to @c true, this parameter forcefully detaches
 * the existing @c AiaDataStreamReader_t with the given id before allocating a
 * new one.
 * @param lock A locked reference to @c readerEnableMutex. This will be
 * unlocked upon return.
 *
 * @return The newly created @c AiaDataStreamReader if successful, or NULL
 * otherwise.
 */
static AiaDataStreamReader_t* _AiaDataStreamBuffer_CreateReaderLocked(
    struct AiaDataStreamBuffer* dataStream, AiaDataStreamBufferReaderId_t id,
    AiaDataStreamReaderPolicy_t policy, bool startWithNewData,
    bool forceReplacement, AiaMutex_t* lock )
{
    AiaAssert( dataStream );
    if( !dataStream )
    {
        AiaLogError( "Invalid dataStream." );
        return NULL;
    }
    if( AiaDataStreamBuffer_IsReaderEnabled( dataStream, id ) &&
        !forceReplacement )
    {
        AiaLogError( "reader already attached, id=%" PRIu8, id );
        AiaMutex( Unlock )( lock );
        return NULL;
    }

    /*
    Note: Reader constructor does not call
    _AiaDataStreamBuffer_UpdateOldestUnconsumedCursor() automatically,
    because we may be seeking to a blocked writer's cursor below (if
    !startWithNewData), and we don't want the writer to start moving
    before we seek.
    */
    AiaDataStreamReader_t* reader =
        _AiaDataStreamReader_Create( policy, dataStream, id );
    AiaMutex( Unlock )( lock );

    if( startWithNewData )
    {
        /*
        We're not moving the cursor again, so call
        _AiaDataStreamBuffer_UpdateOldestUnconsumedCursor() now.
        */
        _AiaDataStreamBuffer_UpdateOldestUnconsumedCursor( dataStream );
    }
    else
    {
        AiaDataStreamIndex_t offset = dataStream->dataSize;
        if( AiaDataStreamAtomicIndex_Load( &dataStream->writeStartCursor ) <
            offset )
        {
            offset =
                AiaDataStreamAtomicIndex_Load( &dataStream->writeStartCursor );
        }
        /* Note: AiaDataStreamReader_Seek() will call
         * _AiaDataStreamBuffer_UpdateOldestUnconsumedCursor(). */
        if( !AiaDataStreamReader_Seek(
                reader, offset,
                AIA_DATA_STREAM_BUFFER_READER_REFERENCE_BEFORE_WRITER ) )
        {
            AiaDataStreamReader_Destroy( reader );
            return NULL;
        }
    }
    return reader;
}

AiaDataStreamBuffer_t* AiaDataStreamBuffer_Create( void* buffer,
                                                   size_t bufferSize,
                                                   size_t wordSize,
                                                   size_t maxReaders )
{
    if( !buffer || bufferSize < wordSize )
    {
        AiaLogError( "Null or invalid buffer." );
        return NULL;
    }
    if( wordSize > AiaDataStreamBufferWordSize_MAX )
    {
        AiaLogError( "word size too large, wordSize=%zu.", wordSize );
        return NULL;
    }
    if( maxReaders > AiaDataStreamBufferReader_MAX )
    {
        AiaLogError( "max readers too large, maxReaders=%zu.", maxReaders );
        return NULL;
    }
    if( wordSize == 0 )
    {
        AiaLogError( "invalid word size, wordSize=%zu", wordSize );
        return NULL;
    }

    size_t dataStreamSize = sizeof( AiaDataStreamBuffer_t );
    size_t readerEnabledArraySize = sizeof( AiaAtomicBool_t ) * maxReaders;
    size_t readerCursorArraySize =
        sizeof( AiaDataStreamAtomicIndex_t ) * maxReaders;
    size_t readerCloseArraySize =
        sizeof( AiaDataStreamAtomicIndex_t ) * maxReaders;

    size_t totalSize = dataStreamSize + readerEnabledArraySize +
                       readerCursorArraySize + readerCloseArraySize;
    uint8_t* memory = AiaCalloc( 1, totalSize );
    if( !memory )
    {
        AiaLogError( "AiaCalloc failed, bytes=%zu.", totalSize );
        return NULL;
    }

    AiaDataStreamBuffer_t* dataStream = (AiaDataStreamBuffer_t*)( memory );

    dataStream->readerEnabledArray =
        (AiaAtomicBool_t*)( memory + dataStreamSize );
    dataStream->readerCursorArray =
        (AiaDataStreamAtomicIndex_t*)( memory + dataStreamSize +
                                       readerEnabledArraySize );
    dataStream->readerCloseIndexArray =
        (AiaDataStreamAtomicIndex_t*)( memory + dataStreamSize +
                                       readerEnabledArraySize +
                                       readerCursorArraySize );
    dataStream->data = buffer;
    *(size_t*)&dataStream->dataSize = bufferSize / wordSize;

    *(AiaDataStreamBufferWordSize_t*)&dataStream->wordSize = wordSize;
    *(AiaDataStreamBufferReaderId_t*)&dataStream->maxReaders = maxReaders;
    if( !AiaMutex( Create )( &dataStream->backwardSeekMutex, false ) )
    {
        AiaLogError( "AiaMutex(Create) failed." );
        AiaFree( memory );
        return NULL;
    }

    AiaAtomicBool_Clear( &dataStream->isWriterEnabled );

    if( !AiaMutex( Create )( &dataStream->writerEnableMutex, false ) )
    {
        AiaLogError( "AiaMutex(Create) failed." );
        AiaMutex( Destroy )( &dataStream->backwardSeekMutex );
        AiaFree( memory );
        return NULL;
    }

    AiaDataStreamAtomicIndex_Store( &dataStream->writeStartCursor, 0 );
    AiaDataStreamAtomicIndex_Store( &dataStream->writeEndCursor, 0 );
    AiaDataStreamAtomicIndex_Store( &dataStream->oldestUnconsumedCursor, 0 );

    if( !AiaMutex( Create )( &dataStream->readerEnableMutex, false ) )
    {
        AiaLogError( "AiaMutex(Create) failed." );
        AiaMutex( Destroy )( &dataStream->writerEnableMutex );
        AiaMutex( Destroy )( &dataStream->backwardSeekMutex );
        AiaFree( memory );
        return NULL;
    }

    /* Reader arrays initialization. */
    AiaDataStreamBufferReaderId_t id;
    for( id = 0; id < dataStream->maxReaders; ++id )
    {
        AiaAtomicBool_Clear( &dataStream->readerEnabledArray[ id ] );
        AiaDataStreamAtomicIndex_Store( &dataStream->readerCursorArray[ id ],
                                        0 );
        AiaDataStreamAtomicIndex_Store(
            &dataStream->readerCloseIndexArray[ id ], 0 );
    }
    return dataStream;
}

void AiaDataStreamBuffer_Destroy( AiaDataStreamBuffer_t* dataStream )
{
    AiaAssert( dataStream );
    if( !dataStream )
    {
        AiaLogError( "Invalid dataStream." );
        return;
    }
    AiaMutex( Destroy )( &dataStream->readerEnableMutex );
    AiaMutex( Destroy )( &dataStream->writerEnableMutex );
    AiaMutex( Destroy )( &dataStream->backwardSeekMutex );
    AiaFree( dataStream );
}

AiaDataStreamBufferReaderId_t AiaDataStreamBuffer_GetMaxReaders(
    const AiaDataStreamBuffer_t* dataStream )
{
    AiaAssert( dataStream );
    if( !dataStream )
    {
        AiaLogError( "Invalid dataStream." );
        return 0;
    }
    return dataStream->maxReaders;
}

size_t AiaDataStreamBuffer_GetDataSize(
    const AiaDataStreamBuffer_t* dataStream )
{
    AiaAssert( dataStream );
    if( !dataStream )
    {
        AiaLogError( "Invalid dataStream." );
        return 0;
    }
    return dataStream->dataSize;
}

size_t AiaDataStreamBuffer_GetWordSize(
    const AiaDataStreamBuffer_t* dataStream )
{
    AiaAssert( dataStream );
    if( !dataStream )
    {
        AiaLogError( "Invalid dataStream." );
        return 0;
    }
    return dataStream->wordSize;
}

AiaDataStreamWriter_t* AiaDataStreamBuffer_CreateWriter(
    AiaDataStreamBuffer_t* dataStream, AiaDataStreamWriterPolicy_t policy,
    bool forceReplacement )
{
    AiaAssert( dataStream );
    if( !dataStream )
    {
        AiaLogError( "Invalid dataStream." );
        return NULL;
    }
    AiaMutex( Lock )( &dataStream->writerEnableMutex );
    bool isWriterEnabled = AiaAtomicBool_Load( &dataStream->isWriterEnabled );
    if( isWriterEnabled && !forceReplacement )
    {
        AiaLogError( "existing writer attached" );
        AiaMutex( Unlock )( &dataStream->writerEnableMutex );
        return NULL;
    }
    else
    {
        AiaDataStreamWriter_t* writer =
            _AiaDataStreamWriter_Create( policy, dataStream );
        AiaMutex( Unlock )( &dataStream->writerEnableMutex );
        return writer;
    }
}

AiaDataStreamReader_t* AiaDataStreamBuffer_CreateReader(
    AiaDataStreamBuffer_t* dataStream, AiaDataStreamReaderPolicy_t policy,
    bool startWithNewData )
{
    AiaAssert( dataStream );
    if( !dataStream )
    {
        AiaLogError( "Invalid dataStream." );
        return NULL;
    }
    AiaMutex( Lock )( &dataStream->readerEnableMutex );
    AiaDataStreamBufferReaderId_t id;
    for( id = 0; id < dataStream->maxReaders; ++id )
    {
        if( !AiaDataStreamBuffer_IsReaderEnabled( dataStream, id ) )
        {
            return _AiaDataStreamBuffer_CreateReaderLocked(
                dataStream, id, policy, startWithNewData, false,
                &dataStream->readerEnableMutex );
        }
    }
    AiaMutex( Unlock )( &dataStream->readerEnableMutex );
    AiaLogError( "no available readers" );
    return NULL;
}

AiaDataStreamReader_t* AiaDataStreamBuffer_CreateReaderWithId(
    AiaDataStreamBuffer_t* dataStream, AiaDataStreamBufferReaderId_t id,
    AiaDataStreamReaderPolicy_t policy, bool startWithNewData,
    bool forceReplacement )
{
    AiaAssert( dataStream );
    if( !dataStream )
    {
        AiaLogError( "Invalid dataStream." );
        return NULL;
    }
    AiaMutex( Lock )( &dataStream->readerEnableMutex );
    return _AiaDataStreamBuffer_CreateReaderLocked(
        dataStream, id, policy, startWithNewData, forceReplacement,
        &dataStream->readerEnableMutex );
}

bool AiaDataStreamBuffer_IsReaderEnabled(
    const AiaDataStreamBuffer_t* dataStream, AiaDataStreamBufferReaderId_t id )
{
    AiaAssert( dataStream );
    if( !dataStream )
    {
        AiaLogError( "Invalid dataStream." );
        return false;
    }
    return AiaAtomicBool_Load( &dataStream->readerEnabledArray[ id ] );
}

void _AiaDataStreamBuffer_UpdateOldestUnconsumedCursor(
    AiaDataStreamBuffer_t* dataStream )
{
    AiaAssert( dataStream );
    if( !dataStream )
    {
        AiaLogError( "Invalid dataStream." );
        return;
    }
    AiaMutex( Lock )( &dataStream->backwardSeekMutex );
    _AiaDataStreamBuffer_UpdateOldestUnconsumedCursorLocked( dataStream );
    AiaMutex( Unlock )( &dataStream->backwardSeekMutex );
}

void _AiaDataStreamBuffer_UpdateOldestUnconsumedCursorLocked(
    AiaDataStreamBuffer_t* dataStream )
{
    AiaAssert( dataStream );
    if( !dataStream )
    {
        AiaLogError( "Invalid dataStream." );
        return;
    }

    /*
    The only barrier to a blocking writer overrunning a reader is
    oldestUnconsumedCursor, so we have to be careful not to ever move it ahead
    of any readers.  The loop below searches through the readers to find the
    oldest point, without moving oldestUnconsumedCursor.  Note that readers can
    continue to read while we are looping; it means oldest may not be completely
    accurate, but it will always be older than the readers because they are
    reading away from it.  Also note that backwards seeks (which would break the
    invariant) are prevented with a mutex which is held while this function is
    called.  Also note that all read cursors may be in the future, so we start
    with an unlimited barrier and work back from there.
    */
    AiaDataStreamIndex_t oldest = AIA_DATA_STREAM_INDEX_MAX;
    AiaDataStreamBufferReaderId_t maxReaders = dataStream->maxReaders;
    for( AiaDataStreamBufferReaderId_t id = 0; id < maxReaders; ++id )
    {
        /*
        Note that this code is calling AiaDataStreamBuffer_IsReaderEnabled()
        without holding readerEnableMutex.  On the surface, this appears to be a
        race condition because a reader may be disabled and/or re-enabled before
        the subsequent code reads the cursor, but it turns out to be safe
        because:
        - if a reader is enabled, its cursor is valid
        - if a reader becomes disabled, its cursor moves to writeCursor (which
        will never be the oldest)
        - if a reader becomes re-enabled, its cursor defaults to writeCursor
        (which will never be the oldest)
        - if a reader is created that wants to be at an older index, it gets
        there by doing a backward seek (which is locked when this function is
        called)
        */
        if( AiaDataStreamBuffer_IsReaderEnabled( dataStream, id ) &&
            AiaDataStreamAtomicIndex_Load(
                &dataStream->readerCursorArray[ id ] ) < oldest )
        {
            oldest = dataStream->readerCursorArray[ id ];
        }
    }

    /*
    If no barrier was found, stop overwriting at the write cursor so that we
    retain data until a reader comes along to read it.
    */
    if( AIA_DATA_STREAM_INDEX_MAX == oldest )
    {
        oldest = AiaDataStreamAtomicIndex_Load( &dataStream->writeStartCursor );
    }

    /*
    Now that we've measured the oldest cursor, we can safely update
    oldestUnconsumedCursor with no risk of an overrun of any readers.

    To clarify the logic here, the code above reviewed all of the enabled
    readers to see where the oldest cursor is at.  This value is captured in the
    'oldest' variable.  Now we want to move up our writer barrier
    ('oldestUnconsumedCursor') if it is older than it needs to be.
    */
    if( oldest >
        AiaDataStreamAtomicIndex_Load( &dataStream->oldestUnconsumedCursor ) )
    {
        AiaDataStreamAtomicIndex_Store( &dataStream->oldestUnconsumedCursor,
                                        oldest );
    }
}

void _AiaDataStreamBuffer_EnableReaderLocked( AiaDataStreamBuffer_t* dataStream,
                                              AiaDataStreamBufferReaderId_t id )
{
    AiaAssert( dataStream );
    if( !dataStream )
    {
        AiaLogError( "Invalid dataStream." );
        return;
    }
    AiaAtomicBool_Set( &dataStream->readerEnabledArray[ id ] );
}

void _AiaDataStreamBuffer_DisableReaderLocked(
    AiaDataStreamBuffer_t* dataStream, AiaDataStreamBufferReaderId_t id )
{
    AiaAssert( dataStream );
    if( !dataStream )
    {
        AiaLogError( "Invalid dataStream." );
        return;
    }
    AiaAtomicBool_Clear( &dataStream->readerEnabledArray[ id ] );
}

AiaDataStreamIndex_t _AiaDataStreamBuffer_WordsUntilWrap(
    const AiaDataStreamBuffer_t* dataStream, AiaDataStreamIndex_t after )
{
    AiaAssert( dataStream );
    if( !dataStream )
    {
        AiaLogError( "Invalid dataStream." );
        return 0;
    }
    return _AiaDataStreamBuffer_AlignSizeTo(
               after, AiaDataStreamBuffer_GetDataSize( dataStream ) ) -
           after;
}

uint8_t* _AiaDataStreamBuffer_GetData( AiaDataStreamBuffer_t* dataStream,
                                       AiaDataStreamIndex_t at )
{
    AiaAssert( dataStream );
    if( !dataStream )
    {
        AiaLogError( "Invalid dataStream." );
        return NULL;
    }
    return dataStream->data +
           ( at % AiaDataStreamBuffer_GetDataSize( dataStream ) ) *
               AiaDataStreamBuffer_GetWordSize( dataStream );
}
