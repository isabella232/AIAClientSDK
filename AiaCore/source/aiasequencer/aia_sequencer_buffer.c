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
 * @file aia_sequencer_buffer.c
 * @brief Implements functions for the AiaSequencerBuffer_t type.
 */

/* The config header is always included first. */
#include <aia_config.h>

#include <aiasequencer/private/aia_sequencer_buffer.h>

AiaSequencerBuffer_t* AiaSequencerBuffer_Create( size_t maxSlots )
{
    AiaSequencerBuffer_t* sequencerBuffer =
        (AiaSequencerBuffer_t*)AiaCalloc( 1, sizeof( AiaSequencerBuffer_t ) );
    if( !sequencerBuffer )
    {
        AiaLogError( "AiaCalloc failed, bytes=%zu.",
                     sizeof( AiaSequencerBuffer_t ) );
        return NULL;
    }

    *(size_t*)&sequencerBuffer->capacity = maxSlots;
    AiaListDouble( Create )( &sequencerBuffer->buffer );

    /* TODO: ADSER-1512 This is inefficient. We are allocating all nodes up
     * front rather than dynamically expanding and contracting our sequencing
     * buffer based on need. It would be more efficient to dynamically allocate
     * sequencer buffers slots on demand.*/
    for( size_t i = 0; i < maxSlots; ++i )
    {
        AiaSequencerSlot_t* slot = AiaCalloc( 1, sizeof( AiaSequencerSlot_t ) );
        if( !slot )
        {
            AiaLogError( "AiaCalloc failed, bytes=%zu.",
                         sizeof( AiaSequencerSlot_t ) );
            AiaListDouble( Link_t )* link = NULL;
            while( ( link = AiaListDouble( RemoveHead )(
                         &sequencerBuffer->buffer ) ) )
            {
                AiaSequencerSlot_t* slot = (AiaSequencerSlot_t*)link;
                AiaFree( slot );
            }
            AiaFree( sequencerBuffer );
            return NULL;
        }
        AiaListDouble( Link_t ) defaultLink = AiaListDouble( LINK_INITIALIZER );
        slot->link = defaultLink;

        /* Add it to the list. */
        AiaListDouble( InsertTail )( &sequencerBuffer->buffer, &slot->link );
    }

    return sequencerBuffer;
}

bool AiaSequencerBuffer_Add( AiaSequencerBuffer_t* sequencerBuffer, void* data,
                             size_t size, size_t index )
{
    AiaAssert( sequencerBuffer );
    if( !sequencerBuffer )
    {
        AiaLogError( "Null sequencerBuffer." );
        return false;
    }

    if( index >= sequencerBuffer->capacity )
    {
        AiaLogError(
            "Provided index greater than capacity, index=%zu, capacity=%zu.",
            index, sequencerBuffer->capacity );
        return false;
    }

    AiaListDouble( Link_t )* link = NULL;
    size_t i = 0;
    AiaListDouble( ForEach )( &sequencerBuffer->buffer, link )
    {
        if( i == index )
        {
            AiaSequencerSlot_t* slot = (AiaSequencerSlot_t*)link;
            bool duplicate = false;
            if( slot->isOccupied )
            {
                AiaLogWarn( "SequencerBuffer slot already occupied, index=%zu",
                            index );
                duplicate = true;
            }
            AiaFree( slot->data );
            slot->data = AiaCalloc( 1, size );
            if( !slot->data )
            {
                AiaLogError( "AiaCalloc failed, bytes=%zu.", size );
                return false;
            }
            memcpy( slot->data, data, size );
            slot->size = size;
            slot->isOccupied = true;
            if( !duplicate )
            {
                ++sequencerBuffer->size;
            }
            return true;
        }
        ++i;
    }

    return false;
}

bool AiaSequencerBuffer_IsOccupied( const AiaSequencerBuffer_t* sequencerBuffer,
                                    size_t index )
{
    AiaAssert( sequencerBuffer );
    if( !sequencerBuffer )
    {
        AiaLogError( "Null sequencerBuffer." );
        return false;
    }

    if( index >= sequencerBuffer->capacity )
    {
        AiaLogError(
            "Provided index greater than capacity, index=%zu, capacity=%zu.",
            index, sequencerBuffer->capacity );
        return false;
    }

    AiaListDouble( Link_t )* link = NULL;
    size_t i = 0;
    AiaListDouble( ForEach )( &sequencerBuffer->buffer, link )
    {
        if( i == index )
        {
            AiaSequencerSlot_t* slot = (AiaSequencerSlot_t*)link;
            return slot->isOccupied;
        }
        ++i;
    }

    return false;
}

void* AiaSequencerBuffer_Front( const AiaSequencerBuffer_t* sequencerBuffer,
                                size_t* size )
{
    AiaAssert( sequencerBuffer );
    if( !sequencerBuffer )
    {
        AiaLogError( "Null sequencerBuffer." );
        return NULL;
    }
    if( !size )
    {
        AiaLogError( "Null size." );
        return NULL;
    }

    AiaListDouble( Link_t )* link =
        AiaListDouble( PeekHead )( &sequencerBuffer->buffer );
    AiaAssert( link );
    if( !link )
    {
        AiaLogError( "Buffer inconsistency detected." );
        return NULL;
    }

    AiaSequencerSlot_t* slot = (AiaSequencerSlot_t*)link;
    *size = slot->size;
    return slot->data;
}

void AiaSequencerBuffer_PopFront( AiaSequencerBuffer_t* sequencerBuffer )
{
    AiaAssert( sequencerBuffer );
    if( !sequencerBuffer )
    {
        AiaLogError( "Null sequencerBuffer." );
        return;
    }

    if( AiaSequencerBuffer_IsOccupied( sequencerBuffer, 0 ) )
    {
        --sequencerBuffer->size;
    }
    AiaListDouble( Link_t )* link =
        AiaListDouble( RemoveHead )( &sequencerBuffer->buffer );
    if( link )
    {
        AiaSequencerSlot_t* removedSlot = (AiaSequencerSlot_t*)link;
        AiaFree( removedSlot->data );
        AiaSequencerSlot_t* slot = AiaCalloc( 1, sizeof( AiaSequencerSlot_t ) );
        if( !slot )
        {
            /* TODO: Send an INTERNAL_ERROR ExceptionEncountered event and tear
             * down Aia connection. */
            AiaLogError( "AiaCalloc failed, bytes=%zu.",
                         sizeof( AiaSequencerSlot_t ) );
            return;
        }
        AiaListDouble( Link_t ) defaultLink = AiaListDouble( LINK_INITIALIZER );
        slot->link = defaultLink;

        /* Add it to the list. */
        AiaListDouble( InsertTail )( &sequencerBuffer->buffer, &slot->link );
    }
}

size_t AiaSequencerBuffer_Size( const AiaSequencerBuffer_t* sequencerBuffer )
{
    AiaAssert( sequencerBuffer );
    if( !sequencerBuffer )
    {
        AiaLogError( "Null sequencerBuffer." );
        return 0;
    }
    return sequencerBuffer->size;
}

size_t AiaSequencerBuffer_Capacity(
    const AiaSequencerBuffer_t* sequencerBuffer )
{
    AiaAssert( sequencerBuffer );
    if( !sequencerBuffer )
    {
        AiaLogError( "Null sequencerBuffer." );
        return 0;
    }
    return sequencerBuffer->capacity;
}

void AiaSequencerBuffer_Destroy( AiaSequencerBuffer_t* sequencerBuffer )
{
    AiaAssert( sequencerBuffer );
    if( !sequencerBuffer )
    {
        AiaLogError( "Null sequencerBuffer." );
        return;
    }

    AiaListDouble( Link_t )* link = NULL;
    while( ( link = AiaListDouble( RemoveHead )( &sequencerBuffer->buffer ) ) )
    {
        AiaSequencerSlot_t* slot = (AiaSequencerSlot_t*)link;
        AiaFree( slot->data );
        AiaFree( slot );
    }

    AiaFree( sequencerBuffer );
}
