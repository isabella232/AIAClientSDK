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

#ifndef AIA_DATA_STREAM_BUFFER_READER_H_
#ifdef __cplusplus
extern "C" {
#endif
#define AIA_DATA_STREAM_BUFFER_READER_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <stdint.h>

#include <stdbool.h>

typedef uint8_t AiaDataStreamBufferReaderId_t;
static const AiaDataStreamBufferReaderId_t AiaDataStreamBufferReader_MAX =
    UINT8_MAX;

/**
 * This is a sub-type of the @c AiaDataStreamBuffer_t type that provides an
 * interface for consuming data from a stream.
 *
 * @note This interface is primarily intended to be used from a single thread.
 * The @c AiaDataStreamReader_t as a whole is thread-safe in the sense that the
 * @c AiaDataStreamWriter_t and @c AiaDataStreamReader_ts can all live in
 * different threads, but individual member functions of a
 * @c AiaDataStreamReader_t instance should not be called from multiple threads
 * except where specifically noted in function documentation below.
 */
typedef struct AiaDataStreamReader AiaDataStreamReader_t;

/** The policies used when reading from the @c AiaDataStreamBuffer_t. */
typedef enum AiaDataStreamReaderPolicy
{
    /**
     * A nonblocking reader will return any available words (up to the requested
     * amount) immediately. If no words are available, @c
     * AIA_DATA_STREAM_BUFFER_READER_ERROR_WOULDBLOCK is returned.
     */
    AIA_DATA_STREAM_BUFFER_READER_NONBLOCKING
} AiaDataStreamReaderPolicy_t;

/**
 * Specifies reference points used to measure seek/tell/close function call
 * offsets against.
 */
typedef enum AiaDataStreamReaderReference
{
    /**
     * The offset is from this @c Reader's current position: `(index = reader +
     * offset)`.
     */
    AIA_DATA_STREAM_BUFFER_READER_REFERENCE_AFTER_READER,
    /**
     * The offset is to this @c Reader's current position: `(index = reader -
     * offset)`.
     */
    AIA_DATA_STREAM_BUFFER_READER_REFERENCE_BEFORE_READER,
    /**
     * The offset is to the @c Writer's current position: `(index = writer -
     * offset)`.
     */
    AIA_DATA_STREAM_BUFFER_READER_REFERENCE_BEFORE_WRITER,
    /**
     * The offset is absolute: `(index = 0 + offset)`.
     */
    AIA_DATA_STREAM_BUFFER_READER_REFERENCE_ABSOLUTE
} AiaDataStreamReaderReference_t;

/**
 * Error codes which may be returned by the @c AiaDataStreamReader_Read() call.
 */
typedef enum AiaDataStreamReaderError
{
    /**
     * Returned when the stream is closed either due to an @c
     * AiaDataStreamWriterClose() call and no remaining buffered data, or due to
     * a @c AiaDataStreamReader_Close() call which has reached its close index.
     */
    AIA_DATA_STREAM_BUFFER_READER_ERROR_CLOSED = 0,

    /**
     * Returned when the data requested has been overwritten and is invalid.
     */
    AIA_DATA_STREAM_BUFFER_READER_ERROR_OVERRUN = -1,

    /**
     * Returned when no data is available.
     */
    AIA_DATA_STREAM_BUFFER_READER_ERROR_WOULDBLOCK = -2,

    /**
     * Returned when a @c AiaDataStreamReader_Read() parameter is invalid.
     */
    AIA_DATA_STREAM_BUFFER_READER_ERROR_INVALID = -3
} AiaDataStreamReaderError_t;

/**
 * Uninitializes and deallocates an @c AiaDataStreamReader_t previously created
 * by a call to
 * @c AiaDataStreamBuffer_CreateReader().
 *
 * @param reader The @c reader to destroy.
 */
void AiaDataStreamReader_Destroy( AiaDataStreamReader_t* reader );

/**
 * This function consumes data from the stream and copies it to the provided
 * buffer. This function is thread-safe.
 *
 * @param reader The @c AiaDataStreamReader_t to act on.
 * @param buf A buffer to copy the consumed data to.  This buffer must be large
 * enough to hold @c nWords
 *     (`nWords * wordSize` bytes).
 * @param nWords The maximum number of @c wordSize words to copy.
 * @return The number of @c wordSize words copied if data was consumed, or zero
 * if the stream has closed, or a negative @c Error code if the stream is still
 * open, but no data could be consumed.
 *
 * @note A stream is closed for the @c AiaDataStreamReader_t if @c
 * AiaDataStreamReader_Close() has been called on it, or if @c
 * AiaDataStreamWriter_Close() has been called and the
 * @c AiaDataStreamReader_t has consumed all remaining data left in the stream
 * when the
 * @c AiaDataStreamWriter_t closed.  In the special case of a new stream, where
 * no @c AiaDataStreamWriter_t has been created, the stream is not considered to
 * be closed for the @c AiaDataStreamReader_t; attempts to @c read() will return
 * @c AIA_DATA_STREAM_BUFFER_READER_ERROR_WOULDBLOCK.
 */
ssize_t AiaDataStreamReader_Read( AiaDataStreamReader_t* reader, void* buf,
                                  size_t nWords );

/**
 * This function moves the @c AiaDataStreamReader_t to the specified location in
 * the stream.  If successful, subsequent calls to
 * @c AiaDataStreamReader_Read() will start from the new location.  For this
 * function to succeed, the specified location *must* point at data which has
 * not been pushed out of the buffer; if the specified location points at old
 * data which has already been overwritten, the call will fail.  If the
 * specified location points at future data which does not yet exist in the
 * buffer, the call will succeed.  If the call fails, the @c reader position
 * will remain unchanged. This function is thread-safe.
 *
 * @param reader The @c AiaDataStreamReader_t to act on.
 * @param offset The position (in @c wordSize words) in the stream, relative to
 * @c reference, to move the @c reader to.
 * @param reference The position in the stream @c offset is applied to.
 * @return @c true if the specified position points at unconsumed data, else @c
 * false.
 */
bool AiaDataStreamReader_Seek( AiaDataStreamReader_t* reader,
                               AiaDataStreamIndex_t offset,
                               AiaDataStreamReaderReference_t reference );

/**
 * This function reports the current position of the @c AiaDataStreamReader_t in
 * the stream. This function is thread-safe.
 *
 * @param reader The @c AiaDataStreamReader_t to act on.
 * @param reference The position in the stream the return value is measured
 * against.
 * @return The @c AiaDataStreamReader_t's position (in @c wordSize words) in the
 * stream relative to @c reference.
 *
 * @note For @c AIA_DATA_STREAM_BUFFER_READER_REFERENCE_BEFORE_WRITER, if the
 * read cursor points at a location in the future (after the writer), then the
 * reader is not before the writer, so this function will return 0.
 */
AiaDataStreamIndex_t AiaDataStreamReader_Tell(
    const AiaDataStreamReader_t* reader,
    AiaDataStreamReaderReference_t reference );

/**
 * This function sets the point at which the @c AiaDataStreamReader_t's stream
 * will close. To schedule the stream to close once all the data which is
 * currently in the buffer has been read, call
 * `AiaDataStreamReader_Close(reader, 0,
 * AIA_DATA_STREAM_BUFFER_READER_REFERENCE_BEFORE_WRITER)`. If another close
 * point is desired, it can be specified using a different
 * @c offset and/or @c reference. This function is thread-safe.
 *
 * @param reader The @c AiaDataStreamReader_t to act on.
 * @param offset The position (in @c wordSize words) in the stream, relative to
 * @c reference, to close at.
 * @param reference The position in the stream the close point is measured
 * against.
 *
 */
void AiaDataStreamReader_Close( AiaDataStreamReader_t* reader,
                                AiaDataStreamIndex_t offset,
                                AiaDataStreamReaderReference_t reference );

/**
 * This function returns the id assigned to this @c AiaDataStreamReader_t.
 *
 * @param reader The @c AiaDataStreamReader_t to act on.
 * @return The id assigned to this @c AiaDataStreamReader_t.
 */
AiaDataStreamBufferReaderId_t AiaDataStreamReader_GetId(
    const AiaDataStreamReader_t* reader );

/**
 * This function returns the word size (in bytes).
 *
 * @param reader The @c AiaDataStreamReader_t to act on.
 * @return The size (in bytes) of words for this @c AiaDataStreamReader_t's @c
 * AiaDataStreamBuffer_t.
 */
size_t AiaDataStreamReader_GetWordSize( const AiaDataStreamReader_t* reader );

/**
 * Utility function used to convert a given @AiaDataStreamReaderError_t to a
 * string for logging purposes.
 *
 * @param error The error to convert.
 * @return The error as a string.
 */
const char* AiaDataStreamReader_ErrorToString(
    AiaDataStreamReaderError_t error );

#ifdef __cplusplus
}
#endif
#endif /* ifndef AIA_DATA_STREAM_BUFFER_READER_H_ */
