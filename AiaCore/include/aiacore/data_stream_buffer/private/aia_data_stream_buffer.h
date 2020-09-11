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

#ifndef PRIVATE_AIA_DATA_STREAM_BUFFER_H_
#define PRIVATE_AIA_DATA_STREAM_BUFFER_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <aiacore/data_stream_buffer/aia_data_stream_buffer.h>

/**
 * Underying struct that contains all data required to present the @c
 * AiaDataStreamBuffer_t abstraction.
 */
struct AiaDataStreamBuffer
{
    /** The buffer used to store the stream's data. */
    uint8_t* data;

    /** Size in words of the buffer. */
    const size_t dataSize;

    /** Pointer to an array of booleans indicating reader enabled status. */
    AiaAtomicBool_t* readerEnabledArray;

    /** Pointer to an array of reader cursors. */
    AiaDataStreamAtomicIndex_t* readerCursorArray;

    /** Pointer to an array of closing indices for each reader. */
    AiaDataStreamAtomicIndex_t* readerCloseIndexArray;

    /** Word size in bytes to use for all buffer operations. */
    const AiaDataStreamBufferWordSize_t wordSize;

    /** Maximum number of readers to support. */
    const AiaDataStreamBufferReaderId_t maxReaders;

    /**
     * Mutex used to temporarily hold off readers from seeking backwards while
     * @c oldestUnconsumedCursor is being updated. This is necessary to prevent
     * a race condition where a reader seeks at the same time that @c
     * oldestUnconsumedCursor is being updated which could result in reading
     * from an older index than @c oldestUnconsumedCursor.
     */
    AiaMutex_t backwardSeekMutex;

    /** Indicates whether there is an enabled writer. */
    AiaAtomicBool_t isWriterEnabled;

    /**
     * Mutex is used to protect creation of the writer to prevent races between
     * overlapping calls to @c AiaDataStreamBuffer_CreateWriter().
     */
    AiaMutex_t writerEnableMutex;

    /** The next location to write to. */
    AiaDataStreamAtomicIndex_t writeStartCursor;

    /**
     * Contains the end of the region that is currently being written to (==
     * writeStartCursor when no write is in progress).
     */
    AiaDataStreamAtomicIndex_t writeEndCursor;

    /**
     * Cntains the location of oldest word in the buffer which has not been
     * consumed by a read operation. This field is used as a barrier by writers
     * which have a policy not to overwrite readers.
     */
    AiaDataStreamAtomicIndex_t oldestUnconsumedCursor;

    /**
     * Mutex used to protect creation of readers to prevent races between
     * overlapping calls to @c AiaDataStreamBuffer_CreateReader().
     */
    AiaMutex_t readerEnableMutex;
};

/**
 * This function acquires @c m_backwardSeekMutex and calls
 * @_AiaDataStreamBuffer_updateOldestUnconsumedCursorLocked().
 *
 * @param dataStream The @c AiaDataStreamBuffer to act on.
 */
void _AiaDataStreamBuffer_UpdateOldestUnconsumedCursor(
    struct AiaDataStreamBuffer* dataStream );

/**
 * This function scans through @c readerCursorArray to determine the oldest
 * reader and records it as @c oldestUnconsumedCursor. This function should be
 * called whenever a read cursor is moved. This function must be called while @c
 * backwardSeekMutex is held to prevent races between updating the oldest
 * cursor an an active reader seeking backwards.
 *
 * @param dataStream The @c AiaDataStreamBuffer to act on.
 */
void _AiaDataStreamBuffer_UpdateOldestUnconsumedCursorLocked(
    struct AiaDataStreamBuffer* dataStream );

/**
 * This function enables the specified reader with the given @c id. @c
 * readerEnableMutex must be held when calling this function.
 *
 * @param dataStream The @c AiaDataStreamBuffer to act on.
 * @param id The id of the reader to enable.
 */
void _AiaDataStreamBuffer_EnableReaderLocked(
    struct AiaDataStreamBuffer* dataStream, AiaDataStreamBufferReaderId_t id );

/**
 * This function disables the specified reader with the given @c id. @c
 * readerEnableMutex must be held when calling this function.
 *
 * @param dataStream The @c AiaDataStreamBuffer to act on.
 * @param id The id of the reader to disable.
 */
void _AiaDataStreamBuffer_DisableReaderLocked(
    struct AiaDataStreamBuffer* dataStream, AiaDataStreamBufferReaderId_t id );

/**
 * This function returns a count of the number of words after @c after before
 * the circular data will wrap.
 *
 * @param dataStream The @c AiaDataStreamBuffer to act on.
 * @c param after The @c AiaDataStreamIndex_t to count from.
 * @c return The count of words after @c after until the circular data will
 * wrap.
 */
AiaDataStreamIndex_t _AiaDataStreamBuffer_WordsUntilWrap(
    const struct AiaDataStreamBuffer* dataStream, AiaDataStreamIndex_t after );

/**
 * This function provides access to the underlying raw buffer.
 *
 * @param dataStream The @c AiaDataStreamBuffer to act on.
 * @param at An optional word @c AiaDataStreamIndex_t to get a data
 * pointer for.  This function will calculate where @c at would fall in the
 * circular buffer and return a pointer to it, but note that this function does
 * not check whether the specified @c AiaDataStreamIndex_t currently
 * resides in the buffer.
 * @return A pointer to the data which would hold the specified @c Index.
 */
uint8_t* _AiaDataStreamBuffer_GetData( struct AiaDataStreamBuffer* dataStream,
                                       AiaDataStreamIndex_t at );

#endif /* ifndef PRIVATE_AIA_DATA_STREAM_BUFFER_H_ */
