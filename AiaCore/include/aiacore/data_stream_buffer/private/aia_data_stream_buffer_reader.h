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

#ifndef PRIVATE_AIA_DATA_STREAM_BUFFER_READER_H_
#define PRIVATE_AIA_DATA_STREAM_BUFFER_READER_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <aiacore/data_stream_buffer/aia_data_stream_buffer.h>

/**
 * Underlying struct that contains all data required to present the @c
 * AiaDataStreamReader_t abstraction.
 */
struct AiaDataStreamReader
{
    /** The @c AiaDataStreamReaderPolicy_t used for for read operations. */
    const AiaDataStreamReaderPolicy_t policy;

    /** The @c AiaDataStreamBuffer_t from which to read data. */
    AiaDataStreamBuffer_t* dataStream;

    /** The id of this reader. */
    const AiaDataStreamBufferReaderId_t id;

    /**
     *  Pointer to the current location of this reader stored within @c
     * dataStream.
     */
    AiaDataStreamAtomicIndex_t* readerCursor;

    /**
     * Pointer to the close index of this reader stored within @c dataStream.
     */
    AiaDataStreamAtomicIndex_t* readerCloseIndex;
};

/**
 * Allocates and initializes a @c AiaDataStreamReader_t object from the heap.
 * The returned pointer should be destroyed using @c
 * AiaDataStreamReader_Destroy().
 *
 * @param policy The policy used to perform read operations for this reader.
 * @param dataStream The @c AiaDataStreamBuffer_t to stream data from.
 * @param id The id to assign this reader.
 *
 * @return The newly created @c AiaDataStreamReader if successful, or NULL
 * otherwise.
 */
struct AiaDataStreamReader* _AiaDataStreamReader_Create(
    AiaDataStreamReaderPolicy_t policy, AiaDataStreamBuffer_t* dataStream,
    uint8_t id );

#endif /* ifndef PRIVATE_AIA_DATA_STREAM_BUFFER_READER_H_ */
