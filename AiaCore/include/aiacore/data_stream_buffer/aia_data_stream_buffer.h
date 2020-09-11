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

#ifndef AIA_DATA_STREAM_BUFFER_H_
#ifdef __cplusplus
extern "C" {
#endif
#define AIA_DATA_STREAM_BUFFER_H_

/* The config header is always included first. */
#include <aia_config.h>

#include "aia_data_stream_buffer_reader.h"
#include "aia_data_stream_buffer_writer.h"

#include <stdint.h>

typedef uint16_t AiaDataStreamBufferWordSize_t;
static const AiaDataStreamBufferWordSize_t AiaDataStreamBufferWordSize_MAX =
    UINT16_MAX;

/**
 * This type is used for streaming data from a single source to multiple sinks.
 */
typedef struct AiaDataStreamBuffer AiaDataStreamBuffer_t;

/**
 * Allocates and initializes a @c AiaDataStreamBuffer_t object from the heap.
 * The returned pointer should be destroyed using @c
 * AiaDataStreamBuffer_Destroy().
 *
 * @param buffer The raw buffer which this abstraction will use to stream
 * data into and from. Any existing data in the buffer will be overwritten.
 * Ownership of the buffer is left to the caller but is no longer usable until a
 * call to @c AiaDataStreamBuffer_Destroy().
 * @param bufferSize The size of @c buffer in bytes.
 * @param wordSize The size (in bytes) of words in the stream. All operations
 * that work with data or positions in the stream are quantified in words.
 * @param maxReaders The maximum number readers of readers to allow to consume
 * from the buffer.
 *
 * @return The newly created @c AiaDataStreamBuffer_t if successful, or NULL
 * otherwise.
 */
AiaDataStreamBuffer_t* AiaDataStreamBuffer_Create( void* buffer,
                                                   size_t bufferSize,
                                                   size_t wordSize,
                                                   size_t maxReaders );

/**
 * Uninitializes and deallocates an @c AiaDataStreamBuffer previously created by
 * a call to
 * @c AiaDataStreamBuffer_Create().
 *
 * @param dataStream The @c AiaDataStreamBuffer_t to destroy.
 */
void AiaDataStreamBuffer_Destroy( AiaDataStreamBuffer_t* dataStream );

/**
 * This function reports the maximum number of readers supported by this @c
 * AiaDataStreamBuffer_t. This function is thread-safe.
 *
 * @param dataStream The @c AiaDataStreamBuffer_t to act on.
 * @return The maximum number of readers supported.
 */
AiaDataStreamBufferReaderId_t AiaDataStreamBuffer_GetMaxReaders(
    const AiaDataStreamBuffer_t* dataStream );

/**
 * This function returns the number of words the stream is capable of holding.
 * This function is thread-safe.
 *
 * @param dataStream The @c AiaDataStreamBuffer_t to act on.
 * @return the number of words the stream is able to hold.
 */
size_t AiaDataStreamBuffer_GetDataSize(
    const AiaDataStreamBuffer_t* dataStream );

/**
 * This function returns the word size (in bytes). This function is thread-safe.
 *
 * @param dataStream The @c AiaDataStreamBuffer_t to act on.
 * @return The size (in bytes) of words for this @c AiaDataStreamBuffer_t.
 */
size_t AiaDataStreamBuffer_GetWordSize(
    const AiaDataStreamBuffer_t* dataStream );

/**
 * This function creates an @c AiaDataStreamWriter_t capable of streaming to
 * this buffer. Only one @c AiaDataStreamWriter_t is allowed at a time. This
 * function is thread-safe.
 *
 * @param dataStream The @c AiaDataStreamBuffer_t to act on.
 * @param policy The policy to use for writing to the stream.
 * @param forceReplacement If set to @c true, this parameter forcefully detaches
 * an existing @c AiaDataStreamWriter_t before allocating a new one.
 * @return @c NULL on failure or else the newly created @c
 * AiaDataStreamBuffer_t.
 *
 * @warning Calling this function with `forceReplacement = true` will allow the
 * call to @c createWriter() to succeed, but will not prevent an previously
 * existing @c AiaDataStreamBuffer_t from writing to the stream.  The
 *     `forceReplacement = true` option should only be used when higher level
 * software can guarantee that the previous @c AiaDataStreamBuffer_t will no
 * longer be used to attempt to write to the stream. Note that attempts to
 * destroy the previously created writer may impact the newly created @c
 AiaDataStreamWriter_t.
 *
 * @note Ownership of the writer is transferred to the caller. It is the
 caller's responsibility to deallocate the @c AiaDataStreamWriter_t using @c
 AiaSdsWriter_Destroy().

 */
AiaDataStreamWriter_t* AiaDataStreamBuffer_CreateWriter(
    AiaDataStreamBuffer_t* dataStream, AiaDataStreamWriterPolicy_t policy,
    bool forceReplacement );

/**
 * This function creates an @c AiaDataStreamReader_t capable of reading from the
 * stream. Up to @c AiaDataStreamBuffer_GetMaxReaders() can be added to the
 * stream. This function is thread-safe.
 *
 * @param dataStream The @c AiaDataStreamBuffer_t to act on.
 * @param policy The policy to use for reading from the stream.
 * @param startWithNewData Flag indicating that this @c Reader should start
 * reading data which is written to the buffer after this @c Reader is created.
 * If this parameter is set to false, the @c Reader will start with the oldest
 * valid data in the buffer.
 * @return @c NULL on failure or else the newly created @c
 * AiaDataStreamReader_t.
 *
 * @note Ownership of the reader is transferred to the caller. It is the
 * caller's responsibility to deallocate the @c AiaDataStreamReader_t using @c
 * AiaSdsReader_Destroy().
 */
AiaDataStreamReader_t* AiaDataStreamBuffer_CreateReader(
    AiaDataStreamBuffer_t* dataStream, AiaDataStreamReaderPolicy_t policy,
    bool startWithNewData );

/**
 * This function creates an @c AiaDataStreamReader_t with a specific id. This
 * function is thread-safe.
 *
 * @param dataStream The @c AiaDataStreamBuffer_t to act on.
 * @param id The id to use for this Reader.
 * @param policy The policy to use for reading from the stream.
 * @param startWithNewData Flag indicating that this @c Reader should start
 * reading data which is written to the buffer after this @c Reader is created.
 * If this parameter is set to false, the @c Reader will start with the oldest
 * valid data in the buffer.
 * @param forceReplacement If set to @c true, this parameter forcefully detaches
 * the existing @c AiaDataStreamReader_t with the given id before allocating a
 * new one.
 * @return @c NULL on failure or else the newly created @c
 * AiaDataStreamReader_t.
 *
 * @note Ownership of the reader is transferred to the caller. It is the
 * caller's responsibility to deallocate the @c AiaDataStreamReader_t using @c
 * AiaSdsReader_Destroy().
 */
AiaDataStreamReader_t* AiaDataStreamBuffer_CreateReaderWithId(
    AiaDataStreamBuffer_t* dataStream, AiaDataStreamBufferReaderId_t id,
    AiaDataStreamReaderPolicy_t policy, bool startWithNewData,
    bool forceReplacement );

/**
 * This function checks whether the specified reader is enabled. This function
 * is thread-safe.
 *
 * @param dataStream The @c AiaDataStreamBuffer_t to act on.
 * @param id The id of the reader to check the enabled status of.
 * @return @c true if the specified reader is enabled, else @c false.
 */
bool AiaDataStreamBuffer_IsReaderEnabled(
    const AiaDataStreamBuffer_t* dataStream, AiaDataStreamBufferReaderId_t id );

#ifdef __cplusplus
}
#endif
#endif /* ifndef AIA_DATA_STREAM_BUFFER_H_ */
