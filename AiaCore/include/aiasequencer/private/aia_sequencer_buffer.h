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
 * @file private/aia_sequencer_buffer.h
 * @brief Internal header for the AiaSequencerBuffer_t type. This header should
 * not be directly included in typical application code.
 */

#ifndef PRIVATE_AIA_SEQUENCER_BUFFER_H_
#define PRIVATE_AIA_SEQUENCER_BUFFER_H_

/* The config header is always included first. */
#include <aia_config.h>

#include AiaListDouble( HEADER )

#include <stdbool.h>

/** Linked list node for managing chunks. */
typedef struct AiaSequencerSlot
{
    /** The actual link in the list. */
    AiaListDouble( Link_t ) link;

    /** The data held in this slot. */
    void* data;

    /** The size of the data. */
    size_t size;

    /** Flag to indicate whether a slot is occupied with data. */
    bool isOccupied;
} AiaSequencerSlot_t;

/**
 * Underlying struct that contains all the data required to present the @c
 * AiaSequencerBuffer_t abstraction.
 *
 * @note Functions in this header which act on an @c AiaSequencerBuffer_t are
 * not thread-safe.
 */
typedef struct AiaSequencerBuffer
{
    /** The linked list representing the sequencing buffer. */
    AiaListDouble_t buffer;

    /** The maximum amount of slots in the buffer. */
    const size_t capacity;

    /** The current number of slots used for buffering. */
    size_t size;
} AiaSequencerBuffer_t;

/**
 * Allocates and initializes a @c AiaSequencerBuffer_t object from the heap.
 * The returned pointer should be destroyed using @c
 * AiaSequencerBuffer_Destroy().
 *
 * @param maxSlots The maximum amount of slots to use for buffering when
 * sequencing messages. Note that each slot will be composed of a pointer to a
 * message, a boolean, and a @c AiaListDoubleLink_t.
 */
AiaSequencerBuffer_t* AiaSequencerBuffer_Create( size_t maxSlots );

/**
 * Add the element to the provided index in the buffer.
 *
 * @param sequencerBuffer The @c AiaSequencerBuffer_t to act on.
 * @param data Data to add. Note that this data is copied into the buffer.
 * @param size Size of data.
 * @param index The slot into which to store the data.
 * @return @c true if the data was stored successfully or @c false if the index
 * was located outside the buffer start and capacity.
 * @note If the given slot index was already occupied, this call will replace
 * it.
 *
 * @note Ownership of the memory pointed to by data is still held by the caller.
 */
bool AiaSequencerBuffer_Add( AiaSequencerBuffer_t* sequencerBuffer, void* data,
                             size_t size, size_t index );

/**
 * Checks if the given index is occupied.
 *
 * @param sequencerBuffer The @c AiaSequencerBuffer_t to act on.
 * @param index The slot into which to store the data.
 * @return @c true if the slot was occupied or @c false if not.
 */
bool AiaSequencerBuffer_IsOccupied( const AiaSequencerBuffer_t* sequencerBuffer,
                                    size_t index );

/**
 * Returns the data at the front of the buffer.
 *
 * @param sequencerBuffer The @c AiaSequencerBuffer_t to act on.
 * @param [out] size The size of the data returned.
 * @return The data living at the front of the buffer.
 */
void* AiaSequencerBuffer_Front( const AiaSequencerBuffer_t* sequencerBuffer,
                                size_t* size );

/**
 * Removes the element at the front of the buffer.
 *
 * @param sequencerBuffer The @c AiaSequencerBuffer_t to act on.
 */
void AiaSequencerBuffer_PopFront( AiaSequencerBuffer_t* sequencerBuffer );

/**
 * Returns the number of elements currently buffered.
 *
 * @param sequencerBuffer The @c AiaSequencerBuffer_t to act on.
 * @return the number of currently buffered elements
 */
size_t AiaSequencerBuffer_Size( const AiaSequencerBuffer_t* sequencerBuffer );

/**
 * Returns the total capacity of the buffer.
 *
 * @param sequencerBuffer The @c AiaSequencerBuffer_t to act on.
 * @return the total number of slots in the buffer.
 */
size_t AiaSequencerBuffer_Capacity(
    const AiaSequencerBuffer_t* sequencerBuffer );

/**
 * Uninitializes and deallocates an @c AiaSequencerBuffer_t previously created
 * by a call to
 * @c _AiaSequencerBuffer_Create().
 *
 * @param sequencerBuffer The @c AiaSequencerBuffer_t to destroy.
 */
void AiaSequencerBuffer_Destroy( AiaSequencerBuffer_t* sequencerBuffer );

#endif /* ifndef PRIVATE_AIA_SEQUENCER_BUFFER_H_ */
