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
#include <aiacore/data_stream_buffer/private/aia_data_stream_buffer.h>
#include <aiacore/data_stream_buffer/private/aia_data_stream_buffer_reader.h>

#include AiaMutex( HEADER )

#include <inttypes.h>
#include <string.h>

AiaDataStreamReader_t* _AiaDataStreamReader_Create(
    AiaDataStreamReaderPolicy_t policy, AiaDataStreamBuffer_t* stream,
    AiaDataStreamBufferReaderId_t id )
{
    if( policy != AIA_DATA_STREAM_BUFFER_READER_NONBLOCKING )
    {
        AiaLogError( "Invalid policy, policy=%d.", policy );
        return NULL;
    }
    if( !stream )
    {
        AiaLogError( "Null stream." );
        return NULL;
    }
    AiaDataStreamReader_t* reader =
        AiaCalloc( 1, sizeof( AiaDataStreamReader_t ) );
    if( !reader )
    {
        AiaLogError( "AiaCalloc failed, bytes=%zu.",
                     sizeof( AiaDataStreamReader_t ) );
        return NULL;
    }
    *(AiaDataStreamReaderPolicy_t*)&reader->policy = policy;
    reader->dataStream = stream;
    *(AiaDataStreamBufferReaderId_t*)&reader->id = id;
    reader->readerCursor = &stream->readerCursorArray[ reader->id ];
    reader->readerCloseIndex = &stream->readerCloseIndexArray[ reader->id ];

    /*
     * Note - _AiaDataStreamBuffer_CreateReaderLocked() holds readerEnableMutex
     * while calling this function.
     *
     * Read new data only.
     * Note: It is important that new readers start with their cursor at the
     * writer. This allows _AiaDataStreamBuffer_UpdateOldestUnconsumedCursor()
     * to be thread-safe without holding readerEnableMutex. See
     * _AiaDataStreamBuffer_UpdateOldestUnconsumedCursor() comments for further
     * explanation.
     */
    AiaDataStreamAtomicIndex_Store(
        reader->readerCursor,
        AiaDataStreamAtomicIndex_Load( &stream->writeStartCursor ) );

    /* Read indefinitely. */
    AiaDataStreamAtomicIndex_Store( reader->readerCloseIndex,
                                    AIA_DATA_STREAM_INDEX_MAX );

    _AiaDataStreamBuffer_EnableReaderLocked( reader->dataStream, reader->id );
    return reader;
}

void AiaDataStreamReader_Destroy( AiaDataStreamReader_t* reader )
{
    AiaAssert( reader );
    if( !reader )
    {
        AiaLogError( "Invalid reader." );
        return;
    }
    /*
     * Note: We can't leave a reader with its cursor in the future; doing so can
     * introduce a race condition in
     * _AiaDataStreamBuffer_UpdateOldestUnconsumedCursor().  See
     * _AiaDataStreamBuffer_UpdateOldestUnconsumedCursor() comments for further
     * explanation.
     */
    AiaDataStreamReader_Seek(
        reader, 0, AIA_DATA_STREAM_BUFFER_READER_REFERENCE_BEFORE_WRITER );
    AiaMutex( Lock )( &reader->dataStream->readerEnableMutex );
    _AiaDataStreamBuffer_DisableReaderLocked( reader->dataStream, reader->id );
    _AiaDataStreamBuffer_UpdateOldestUnconsumedCursor( reader->dataStream );
    AiaMutex( Unlock )( &reader->dataStream->readerEnableMutex );
    AiaFree( reader );
}

ssize_t AiaDataStreamReader_Read( AiaDataStreamReader_t* reader, void* buf,
                                  size_t nWords )
{
    AiaAssert( reader );
    if( !reader )
    {
        AiaLogError( "Invalid reader." );
        return AIA_DATA_STREAM_BUFFER_READER_ERROR_INVALID;
    }
    if( !buf )
    {
        AiaLogError( "Null buf." );
        return AIA_DATA_STREAM_BUFFER_READER_ERROR_INVALID;
    }

    if( 0 == nWords )
    {
        AiaLogError( "Invalid nWords: nWords=%zu.", nWords );
        return AIA_DATA_STREAM_BUFFER_READER_ERROR_INVALID;
    }

    AiaDataStreamAtomicIndex_t readerCloseIndex =
        AiaDataStreamAtomicIndex_Load( reader->readerCloseIndex );
    AiaDataStreamAtomicIndex_t readerCursor =
        AiaDataStreamAtomicIndex_Load( reader->readerCursor );
    if( readerCursor >= readerCloseIndex )
    {
        return AIA_DATA_STREAM_BUFFER_READER_ERROR_CLOSED;
    }

    if( ( AiaDataStreamAtomicIndex_Load(
              &reader->dataStream->writeEndCursor ) >= readerCursor ) &&
        AiaDataStreamAtomicIndex_Load( &reader->dataStream->writeEndCursor ) -
                readerCursor >
            AiaDataStreamBuffer_GetDataSize( reader->dataStream ) )
    {
        return AIA_DATA_STREAM_BUFFER_READER_ERROR_OVERRUN;
    }

    size_t wordsAvailable = AiaDataStreamReader_Tell(
        reader, AIA_DATA_STREAM_BUFFER_READER_REFERENCE_BEFORE_WRITER );
    if( 0 == wordsAvailable )
    {
        if( AiaDataStreamAtomicIndex_Load(
                &reader->dataStream->writeEndCursor ) > 0 &&
            !AiaAtomicBool_Load( &reader->dataStream->isWriterEnabled ) )
        {
            return AIA_DATA_STREAM_BUFFER_READER_ERROR_CLOSED;
        }
        else
        {
            return AIA_DATA_STREAM_BUFFER_READER_ERROR_WOULDBLOCK;
        }
    }

    if( nWords > wordsAvailable )
    {
        nWords = wordsAvailable;
    }

    if( readerCursor + nWords > readerCloseIndex )
    {
        nWords = readerCloseIndex - readerCursor;
    }

    size_t beforeWrap =
        _AiaDataStreamBuffer_WordsUntilWrap( reader->dataStream, readerCursor );
    if( beforeWrap > nWords )
    {
        beforeWrap = nWords;
    }
    size_t afterWrap = nWords - beforeWrap;
    size_t wordSize = AiaDataStreamReader_GetWordSize( reader );

    uint8_t* buf8 = (uint8_t*)buf;
    memcpy( buf8,
            _AiaDataStreamBuffer_GetData(
                reader->dataStream,
                AiaDataStreamAtomicIndex_Load( reader->readerCursor ) ),
            beforeWrap * wordSize );
    if( afterWrap > 0 )
    {
        memcpy( buf8 + ( beforeWrap * wordSize ),
                _AiaDataStreamBuffer_GetData( reader->dataStream,
                                              readerCursor + beforeWrap ),
                afterWrap * wordSize );
    }

    AiaDataStreamAtomicIndex_Add( reader->readerCursor, nWords );

    bool overrun = ( ( AiaDataStreamAtomicIndex_Load(
                           &reader->dataStream->writeEndCursor ) -
                       AiaDataStreamAtomicIndex_Load( reader->readerCursor ) ) >
                     AiaDataStreamBuffer_GetDataSize( reader->dataStream ) );

    /* Move the unconsumed cursor before returning. */
    _AiaDataStreamBuffer_UpdateOldestUnconsumedCursor( reader->dataStream );
    if( overrun )
    {
        return AIA_DATA_STREAM_BUFFER_READER_ERROR_OVERRUN;
    }

    return nWords;
}

bool AiaDataStreamReader_Seek( AiaDataStreamReader_t* reader,
                               AiaDataStreamIndex_t offset,
                               AiaDataStreamReaderReference_t reference )
{
    AiaAssert( reader );
    if( !reader )
    {
        AiaLogError( "Invalid reader." );
        return false;
    }
    AiaDataStreamIndex_t absolute = AIA_DATA_STREAM_INDEX_MAX;
    AiaDataStreamAtomicIndex_t readerIndex =
        AiaDataStreamAtomicIndex_Load( reader->readerCursor );

    switch( reference )
    {
        case AIA_DATA_STREAM_BUFFER_READER_REFERENCE_AFTER_READER:
            absolute = readerIndex + offset;
            break;
        case AIA_DATA_STREAM_BUFFER_READER_REFERENCE_BEFORE_READER:
            if( offset > readerIndex )
            {
                AiaLogError( "Seek before stream start index, offset=%" PRIu32
                             ", reader=%" PRIu32 ".",
                             offset, readerIndex );
                return false;
            }
            absolute = readerIndex - offset;
            break;
        case AIA_DATA_STREAM_BUFFER_READER_REFERENCE_BEFORE_WRITER:
            if( offset > AiaDataStreamAtomicIndex_Load(
                             &reader->dataStream->writeStartCursor ) )
            {
                AiaLogError( "Seek before stream start index, offset=%" PRIu32
                             ", reader=%" PRIu32 ".",
                             offset, readerIndex );
                return false;
            }
            absolute = AiaDataStreamAtomicIndex_Load(
                           &reader->dataStream->writeStartCursor ) -
                       offset;
            break;
        case AIA_DATA_STREAM_BUFFER_READER_REFERENCE_ABSOLUTE:
            absolute = offset;
            break;
    }

    if( absolute > AiaDataStreamAtomicIndex_Load( reader->readerCloseIndex ) )
    {
        AiaLogError( "Seek beyond close index." );
        return false;
    }

    /*
     * Per documentation of _AiaDataStreamBuffer_UpdateOldestUnconsumedCursor(),
     * don't try to seek backwards while oldestConsumedCursor is being updated.
     */
    bool backward = absolute < readerIndex;
    if( backward )
    {
        AiaMutex( Lock )( &reader->dataStream->backwardSeekMutex );
    }

    /*
     * Don't seek to past data which has been (or soon will be) overwritten.
     * Note: If this is a backward seek, it is important that this check is
     * performed while holding the backwardSeekMutex to prevent a writer from
     * starting to overwrite us between here and the readerCursor update below.
     * If this is not a backward seek, then the mutex is not held.
     */
    if( AiaDataStreamAtomicIndex_Load( &reader->dataStream->writeEndCursor ) >=
            absolute &&
        AiaDataStreamAtomicIndex_Load( &reader->dataStream->writeEndCursor ) -
                absolute >
            AiaDataStreamBuffer_GetDataSize( reader->dataStream ) )
    {
        AiaLogError( "Seek overwritten data." );
        if( backward )
        {
            AiaMutex( Unlock )( &reader->dataStream->backwardSeekMutex );
        }
        return false;
    }

    AiaDataStreamAtomicIndex_Store( reader->readerCursor, absolute );

    if( backward )
    {
        _AiaDataStreamBuffer_UpdateOldestUnconsumedCursorLocked(
            reader->dataStream );
        AiaMutex( Unlock )( &reader->dataStream->backwardSeekMutex );
    }
    else
    {
        _AiaDataStreamBuffer_UpdateOldestUnconsumedCursor( reader->dataStream );
    }

    return true;
}

AiaDataStreamIndex_t AiaDataStreamReader_Tell(
    const AiaDataStreamReader_t* reader,
    AiaDataStreamReaderReference_t reference )
{
    AiaAssert( reader );
    if( !reader )
    {
        AiaLogError( "Invalid reader." );
        return 0;
    }
    AiaDataStreamIndex_t readerCursor =
        AiaDataStreamAtomicIndex_Load( reader->readerCursor );
    AiaDataStreamIndex_t writerStartCursor =
        AiaDataStreamAtomicIndex_Load( &reader->dataStream->writeStartCursor );
    switch( reference )
    {
        case AIA_DATA_STREAM_BUFFER_READER_REFERENCE_AFTER_READER:
            return 0;
        case AIA_DATA_STREAM_BUFFER_READER_REFERENCE_BEFORE_READER:
            return 0;
        case AIA_DATA_STREAM_BUFFER_READER_REFERENCE_BEFORE_WRITER:
            return ( writerStartCursor >= readerCursor )
                       ? writerStartCursor - readerCursor
                       : 0;
        case AIA_DATA_STREAM_BUFFER_READER_REFERENCE_ABSOLUTE:
            return readerCursor;
    }

    AiaLogError( "Invalid reference." );
    return AIA_DATA_STREAM_INDEX_MAX;
}

void AiaDataStreamReader_Close( AiaDataStreamReader_t* reader,
                                AiaDataStreamIndex_t offset,
                                AiaDataStreamReaderReference_t reference )
{
    AiaAssert( reader );
    if( !reader )
    {
        AiaLogError( "Invalid reader." );
        return;
    }
    AiaDataStreamIndex_t absolute = 0;
    bool validReference = false;
    switch( reference )
    {
        case AIA_DATA_STREAM_BUFFER_READER_REFERENCE_AFTER_READER:
            absolute =
                AiaDataStreamAtomicIndex_Load( reader->readerCursor ) + offset;
            validReference = true;
            break;
        case AIA_DATA_STREAM_BUFFER_READER_REFERENCE_BEFORE_READER:
            absolute = AiaDataStreamAtomicIndex_Load( reader->readerCursor );
            validReference = true;
            break;
        case AIA_DATA_STREAM_BUFFER_READER_REFERENCE_BEFORE_WRITER:
            if( AiaDataStreamAtomicIndex_Load(
                    &reader->dataStream->writeStartCursor ) < offset )
            {
                AiaLogError( "Invalid index." );
            }
            else
            {
                absolute = AiaDataStreamAtomicIndex_Load(
                               &reader->dataStream->writeStartCursor ) -
                           offset;
            }
            validReference = true;
            break;
        case AIA_DATA_STREAM_BUFFER_READER_REFERENCE_ABSOLUTE:
            absolute = offset;
            validReference = true;
            break;
    }

    if( !validReference )
    {
        AiaLogError( "Invalid reference." );
    }

    AiaDataStreamAtomicIndex_Store( reader->readerCloseIndex, absolute );
}

AiaDataStreamBufferReaderId_t AiaDataStreamReader_GetId(
    const AiaDataStreamReader_t* reader )
{
    AiaAssert( reader );
    if( !reader )
    {
        AiaLogError( "Invalid reader." );
        return 0;
    }
    return reader->id;
}

size_t AiaDataStreamReader_GetWordSize( const AiaDataStreamReader_t* reader )
{
    AiaAssert( reader );
    if( !reader )
    {
        AiaLogError( "Invalid reader." );
        return 0;
    }
    return AiaDataStreamBuffer_GetWordSize( reader->dataStream );
}

const char* AiaDataStreamReader_ErrorToString(
    AiaDataStreamReaderError_t error )
{
    switch( error )
    {
        case AIA_DATA_STREAM_BUFFER_READER_ERROR_CLOSED:
            return "READER_CLOSED";
        case AIA_DATA_STREAM_BUFFER_READER_ERROR_OVERRUN:
            return "READER_OVERRUN";
        case AIA_DATA_STREAM_BUFFER_READER_ERROR_WOULDBLOCK:
            return "READER_WOULDBLOCK";
        case AIA_DATA_STREAM_BUFFER_READER_ERROR_INVALID:
            return "READER_INVALID";
    }
    return "READER_UNKNOWN_ERROR";
}
