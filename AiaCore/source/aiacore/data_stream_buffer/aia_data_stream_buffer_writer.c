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
#include <aiacore/data_stream_buffer/aia_data_stream_buffer_writer.h>
#include <aiacore/data_stream_buffer/private/aia_data_stream_buffer.h>
#include <aiacore/data_stream_buffer/private/aia_data_stream_buffer_writer.h>

#include AiaMutex( HEADER )

#include <string.h>

AiaDataStreamWriter_t* _AiaDataStreamWriter_Create(
    AiaDataStreamWriterPolicy_t policy, AiaDataStreamBuffer_t* stream )
{
    if( policy != AIA_DATA_STREAM_BUFFER_WRITER_NONBLOCKABLE &&
        policy != AIA_DATA_STREAM_BUFFER_WRITER_NONBLOCKING &&
        policy != AIA_DATA_STREAM_BUFFER_WRITER_ALL_OR_NOTHING )
    {
        AiaLogError( "Invalid policy, policy=%d.", policy );
        return NULL;
    }
    if( !stream )
    {
        AiaLogError( "Null stream." );
        return NULL;
    }
    AiaDataStreamWriter_t* writer =
        AiaCalloc( 1, sizeof( AiaDataStreamWriter_t ) );
    if( !writer )
    {
        AiaLogError( "AiaCalloc failed, bytes=%zu.",
                     sizeof( AiaDataStreamWriter_t ) );
        return NULL;
    }
    writer->policy = policy;
    writer->stream = stream;
    writer->closed = false;

    AiaAtomicBool_Set( &writer->stream->isWriterEnabled );
    AiaDataStreamAtomicIndex_Store(
        &writer->stream->writeEndCursor,
        AiaDataStreamAtomicIndex_Load( &writer->stream->writeStartCursor ) );
    return writer;
}

void AiaDataStreamWriter_Destroy( AiaDataStreamWriter_t* writer )
{
    AiaAssert( writer );
    if( !writer )
    {
        AiaLogError( "Invalid writer." );
        return;
    }
    AiaDataStreamWriter_Close( writer );
    AiaFree( writer );
}

ssize_t AiaDataStreamWriter_Write( AiaDataStreamWriter_t* writer,
                                   const void* buf, size_t nWords )
{
    AiaAssert( writer );
    if( !writer )
    {
        AiaLogError( "Invalid writer." );
        return 0;
    }
    if( !buf )
    {
        AiaLogError( "Null buf." );
        return AIA_DATA_STREAM_BUFFER_WRITER_ERROR_INVALID;
    }
    if( 0 == nWords )
    {
        AiaLogError( "Invalid nWords: nWords=%zu.", nWords );
        return AIA_DATA_STREAM_BUFFER_WRITER_ERROR_INVALID;
    }

    if( !AiaAtomicBool_Load( &writer->stream->isWriterEnabled ) )
    {
        AiaLogError( "Writer disabled." );
        return AIA_DATA_STREAM_BUFFER_WRITER_ERROR_CLOSED;
    }

    size_t wordsToCopy = nWords;
    const uint8_t* buf8 = (const uint8_t*)buf;
    AiaDataStreamIndex_t writeStart =
        AiaDataStreamAtomicIndex_Load( &writer->stream->writeStartCursor );
    AiaDataStreamIndex_t writeEnd = writeStart + nWords;
    bool backwardSeekMutexAcquired = false;

    switch( writer->policy )
    {
        case AIA_DATA_STREAM_BUFFER_WRITER_NONBLOCKABLE:
            if( nWords > AiaDataStreamBuffer_GetDataSize( writer->stream ) )
            {
                wordsToCopy = nWords =
                    AiaDataStreamBuffer_GetDataSize( writer->stream );
                writeEnd = AiaDataStreamAtomicIndex_Load(
                               &writer->stream->writeStartCursor ) +
                           nWords;
            }
            break;
        case AIA_DATA_STREAM_BUFFER_WRITER_ALL_OR_NOTHING:
            AiaMutex( Lock )( &writer->stream->backwardSeekMutex );
            backwardSeekMutexAcquired = true;

            if( ( writeEnd >= AiaDataStreamAtomicIndex_Load(
                                  &writer->stream->oldestUnconsumedCursor ) ) &&
                ( ( writeEnd - AiaDataStreamAtomicIndex_Load(
                                   &writer->stream->oldestUnconsumedCursor ) ) >
                  AiaDataStreamBuffer_GetDataSize( writer->stream ) ) )
            {
                if( backwardSeekMutexAcquired )
                {
                    AiaMutex( Unlock )( &writer->stream->backwardSeekMutex );
                }
                return AIA_DATA_STREAM_BUFFER_WRITER_ERROR_WOULD_BLOCK;
            }
            break;
        case AIA_DATA_STREAM_BUFFER_WRITER_NONBLOCKING:
            AiaMutex( Lock )( &writer->stream->backwardSeekMutex );
            backwardSeekMutexAcquired = true;
            AiaDataStreamIndex_t spaceAvailable =
                AiaDataStreamBuffer_GetDataSize( writer->stream );
            AiaDataStreamIndex_t oldestUnconsumedCursor =
                AiaDataStreamAtomicIndex_Load(
                    &writer->stream->oldestUnconsumedCursor );
            if( writeStart >= oldestUnconsumedCursor )
            {
                AiaDataStreamIndex_t wordsToOverrun =
                    AiaDataStreamBuffer_GetDataSize( writer->stream ) -
                    ( writeStart - oldestUnconsumedCursor );
                if( writeStart - oldestUnconsumedCursor >
                    AiaDataStreamBuffer_GetDataSize( writer->stream ) )
                {
                    wordsToOverrun = 0;
                }
                if( wordsToOverrun < spaceAvailable )
                {
                    spaceAvailable = wordsToOverrun;
                }
            }

            if( spaceAvailable < nWords )
            {
                wordsToCopy = nWords = spaceAvailable;
                writeEnd = writeStart + nWords;
            }
            break;
    }

    AiaDataStreamAtomicIndex_Store( &writer->stream->writeEndCursor, writeEnd );
    if( backwardSeekMutexAcquired )
    {
        AiaMutex( Unlock )( &writer->stream->backwardSeekMutex );
    }

    if( AIA_DATA_STREAM_BUFFER_WRITER_ALL_OR_NOTHING == writer->policy )
    {
        /* If we have more data than the buffer can hold and we're not going to
         * be overwriting oldestUnconsumedCursor, we can safely discard the
         * initial data and just leave the trailing data in the buffer. */
        if( wordsToCopy > AiaDataStreamBuffer_GetDataSize( writer->stream ) )
        {
            wordsToCopy = AiaDataStreamBuffer_GetDataSize( writer->stream );
            buf8 += ( nWords - wordsToCopy ) *
                    AiaDataStreamWriter_GetWordSize( writer );
        }
    }

    AiaDataStreamIndex_t beforeWrap =
        _AiaDataStreamBuffer_WordsUntilWrap( writer->stream, writeStart );
    if( beforeWrap > wordsToCopy )
    {
        beforeWrap = wordsToCopy;
    }
    size_t afterWrap = wordsToCopy - beforeWrap;

    memcpy( _AiaDataStreamBuffer_GetData( writer->stream, writeStart ), buf8,
            beforeWrap * AiaDataStreamWriter_GetWordSize( writer ) );
    if( afterWrap > 0 )
    {
        memcpy( _AiaDataStreamBuffer_GetData( writer->stream,
                                              writeStart + beforeWrap ),
                buf8 + beforeWrap * AiaDataStreamWriter_GetWordSize( writer ),
                afterWrap * AiaDataStreamWriter_GetWordSize( writer ) );
    }

    AiaDataStreamAtomicIndex_Store( &writer->stream->writeStartCursor,
                                    writeEnd );

    return nWords;
}

AiaDataStreamIndex_t AiaDataStreamWriter_Tell(
    const AiaDataStreamWriter_t* writer )
{
    AiaAssert( writer );
    if( !writer )
    {
        AiaLogError( "Invalid writer." );
        return 0;
    }
    return AiaDataStreamAtomicIndex_Load( &writer->stream->writeStartCursor );
}

void AiaDataStreamWriter_Close( AiaDataStreamWriter_t* writer )
{
    AiaAssert( writer );
    if( !writer )
    {
        AiaLogError( "Invalid writer." );
        return;
    }
    AiaMutex( Lock )( &writer->stream->writerEnableMutex );
    if( writer->closed )
    {
        AiaMutex( Unlock )( &writer->stream->writerEnableMutex );
        return;
    }
    if( AiaAtomicBool_Load( &writer->stream->isWriterEnabled ) )
    {
        AiaAtomicBool_Clear( &writer->stream->isWriterEnabled );
    }
    writer->closed = true;
    AiaMutex( Unlock )( &writer->stream->writerEnableMutex );
}

size_t AiaDataStreamWriter_GetWordSize( const AiaDataStreamWriter_t* writer )
{
    AiaAssert( writer );
    if( !writer )
    {
        AiaLogError( "Invalid writer." );
        return 0;
    }
    return AiaDataStreamBuffer_GetWordSize( writer->stream );
}

void AiaDataStreamWriter_SetPolicy( AiaDataStreamWriter_t* writer,
                                    AiaDataStreamWriterPolicy_t policy )
{
    AiaAssert( writer );
    if( !writer )
    {
        AiaLogError( "Invalid writer." );
        return;
    }
    writer->policy = policy;
}

const char* AiaDataStreamWriter_ErrorToString(
    AiaDataStreamWriterError_t error )
{
    switch( error )
    {
        case AIA_DATA_STREAM_BUFFER_WRITER_ERROR_CLOSED:
            return "WRITER_CLOSED";
        case AIA_DATA_STREAM_BUFFER_WRITER_ERROR_WOULD_BLOCK:
            return "WRITER_WOULD_BLOCK";
        case AIA_DATA_STREAM_BUFFER_WRITER_ERROR_INVALID:
            return "WRITER_INVALID";
    }
    return "WRITER_UNKNOWN_ERROR";
}
