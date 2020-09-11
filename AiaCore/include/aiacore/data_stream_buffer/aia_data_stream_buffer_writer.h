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

#ifndef AIA_DATA_STREAM_BUFFER_WRITER_H_
#ifdef __cplusplus
extern "C" {
#endif
#define AIA_DATA_STREAM_BUFFER_WRITER_H_

/* The config header is always included first. */
#include <aia_config.h>

/**
 * This is a sub-type of the @c AiaDataStreamBuffer_t type which provides an
 * interface for producing data to the stream.
 *
 * @note This class is primarily intended to be used from a single thread.  The
 * @c AiaDataStreamWriter_t as a whole is thread-safe in the sense that the @c
 * AiaDataStreamWriter_t and @c AiaDataStreamReader_ts can all live in different
 * threads, but individual member functions of a
 * @c AiaDataStreamWriter_t instance should not be called from multiple threads
 * except where specifically noted in function documentation below.
 */
typedef struct AiaDataStreamWriter AiaDataStreamWriter_t;

/** The policies used when writing to an @c AiaDataStreamBuffer_t. */
typedef enum AiaDataStreamWriterPolicy
{
    /**
     * A nonblockable writer will always write all words provided to the buffer
     * irrespective of overwriting readers or unconsumed data.
     */
    AIA_DATA_STREAM_BUFFER_WRITER_NONBLOCKABLE,

    /**
     * A nonblocking writer will write as many words as it can without
     * overwriting unconsumed data.
     */
    AIA_DATA_STREAM_BUFFER_WRITER_NONBLOCKING,

    /**
     * An all or nothing writer will either write all the data provided if it
     * can do so without overwriting unconsumed data, or it will return
     * AIA_DATA_STREAM_BUFFER_WRITER_ERROR_WOULD_BLOCK without writing any data
     * at all.
     */
    AIA_DATA_STREAM_BUFFER_WRITER_ALL_OR_NOTHING,
} AiaDataStreamWriterPolicy_t;

/**
 * Error codes which may be returned by the @c AiaDataStreamWriter_Write()
 * call.
 */
typedef enum AiaDataStreamWriterError
{
    /**
     * Returned when @c AiaDataStreamWriter_Close() has been previously called
     * on the writer.
     */
    AIA_DATA_STREAM_BUFFER_WRITER_ERROR_CLOSED = 0,

    /** Returned when policy is @c AIA_DATA_STREAM_BUFFER_WRITER_ALL_OR_NOTHING
       and the write call would overwrrite unconsumed data. */
    AIA_DATA_STREAM_BUFFER_WRITER_ERROR_WOULD_BLOCK = -1,

    /** Returned when a @c AiaDataStreamWriter_Write() parameter is invalid. */
    AIA_DATA_STREAM_BUFFER_WRITER_ERROR_INVALID = -2,
} AiaDataStreamWriterError_t;

/**
 * Uninitializes and deallocates an @c AiaDataStreamWriter_t previously created
 * by a call to
 * @c AiaDataStreamWriter_Create().
 *
 * @param writer The @c writer to destroy.
 */
void AiaDataStreamWriter_Destroy( AiaDataStreamWriter_t* writer );

/**
 * This function adds new data to the stream by copying it from the provided
 * buffer. This function is thread-safe.
 *
 * @param writer The @c AiaDataStreamWriter_t to act on.
 * @param buf A buffer to copy the data from.
 * @param nWords The maximum number of @c wordSize words to copy.
 * @return The number of @c wordSize words copied, or @c
 * AIA_DATA_STREAM_BUFFER_WRITER_ERROR_CLOSED if the stream has closed, or a
 * negative @c Error code if the stream is still open, but no data could be
 * written.
 *
 */
ssize_t AiaDataStreamWriter_Write( AiaDataStreamWriter_t* writer,
                                   const void* buf, size_t nWords );

/**
 * This function reports the current position of the @c AiaDataStreamWriter_t in
 * the stream. This function is thread-safe.
 *
 * @param writer The @c AiaDataStreamWriter_t to act on.
 * @return The @c Writer's position (in @c wordSize words) in the stream.
 */
AiaDataStreamIndex_t AiaDataStreamWriter_Tell(
    const AiaDataStreamWriter_t* writer );

/**
 * This function closes the @c AiaDataStreamWriter_t, such that @c Readers will
 * return @c AIA_DATA_STREAM_BUFFER_READER_ERROR_CLOSED when they catch up with
 * the @c Writer, and subsequent calls to @c AiaDataStreamWriter_Write() will
 * return @c AIA_DATA_STREAM_BUFFER_WRITER_ERROR_CLOSED. This function is
 * thread-safe.
 *
 * @param writer The @c AiaDataStreamWriter_t to act on.
 */
void AiaDataStreamWriter_Close( AiaDataStreamWriter_t* writer );

/**
 * This function returns the word size (in bytes).
 *
 * @param writer The @c AiaDataStreamWriter_t to act on.
 * @return The size (in bytes) of words for this @c AiaDataStreamWriter_t's @c
 * AiaDataStreamBuffer_t.
 */
size_t AiaDataStreamWriter_GetWordSize( const AiaDataStreamWriter_t* writer );

/**
 * This function changes the writer's policy. It is not thread-safe. Higher
 * level application code must verify that no parallel writes are occurring when
 * making this call.
 *
 * @param writer The @c AiaDataStreamWriter_t to act on.
 * @param policy The policy used to perform write operations for this writer.
 */
void AiaDataStreamWriter_SetPolicy( AiaDataStreamWriter_t* writer,
                                    AiaDataStreamWriterPolicy_t policy );

/**
 * Utility function used to convert a given @AiaDataStreamWriterError_t to a
 * string for logging purposes.
 *
 * @param error The error to convert.
 * @return The error as a string.
 */
const char* AiaDataStreamWriter_ErrorToString(
    AiaDataStreamWriterError_t error );

#ifdef __cplusplus
}
#endif
#endif /* ifndef AIA_DATA_STREAM_BUFFER_WRITER_H_ */
