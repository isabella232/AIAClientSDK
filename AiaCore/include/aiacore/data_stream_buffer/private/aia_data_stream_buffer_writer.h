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

#ifndef PRIVATE_AIA_DATA_STREAM_BUFFER_WRITER_H_
#define PRIVATE_AIA_DATA_STREAM_BUFFER_WRITER_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <aiacore/data_stream_buffer/aia_data_stream_buffer.h>

/**
 * Underlying struct that contains all data required to present the @c
 * AiaDataStreamWriter_t abstraction.
 */
struct AiaDataStreamWriter
{
    /** The @c AiaDataStreamWriterPolicy_t used for for write operations. */
    AiaDataStreamWriterPolicy_t policy;

    /** The @c AiaDataStreamBuffer_t into which to write data. */
    AiaDataStreamBuffer_t* stream;

    /**
     * A flag indicating whether this writer has closed.  This flag prevents
     * trying to disable the writer during destruction after previously having
     * closed the writer.  Usage of this flag must be locked by
     * @c writerEnableMutex.
     */
    bool closed;
};

/**
 * Allocates and initializes a @c AiaDataStreamWriter_t object from the heap.
 * The returned pointer should be destroyed using @c
 * AiaDataStreamWriter_Destroy().
 *
 * @param policy The policy used to perform write operations for this writer.
 * @param stream The @c AiaDataStreamBuffer_t to stream data to.
 *
 * @return The newly created @c AiaDataStreamWriter_t if successful, or NULL
 * otherwise.
 */
AiaDataStreamWriter_t* _AiaDataStreamWriter_Create(
    AiaDataStreamWriterPolicy_t policy, AiaDataStreamBuffer_t* stream );

#endif /* ifndef PRIVATE_AIA_DATA_STREAM_BUFFER_WRITER_H_ */
