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
 * @file aia_binary_message.c
 * @brief Implements functions for the AiaBinaryMessage_t type.
 */

/* The config header is always included first. */
#include <aia_config.h>

#include <aiacore/aia_binary_message.h>
#include <aiacore/aia_message.h>
#include <aiacore/private/aia_message.h>

#include <inttypes.h>

/* Anchor the inlines from aia_binary_message.h */
extern inline AiaMessage_t* AiaBinaryMessage_ToMessage(
    AiaBinaryMessage_t* binaryMessage );
extern inline const AiaMessage_t* AiaBinaryMessage_ToConstMessage(
    const AiaBinaryMessage_t* binaryMessage );
extern inline AiaBinaryMessage_t* AiaBinaryMessage_FromMessage(
    AiaMessage_t* message );
extern inline const AiaBinaryMessage_t* ConstAiaBinaryMessage_FromMessage(
    const AiaMessage_t* message );

/**
 * Base type for all binary messages.
 *
 * @note Functions in this header may not be thread-safe.  Users are required to
 * provide external synchronization.
 */
struct AiaBinaryMessage
{
    /** The underlying abstract @c AiaMessage type. */
    AiaMessage_t message;

    /** The "length" field of @c data. */
    AiaBinaryMessageLength_t length;

    /** The The "type" field of the binary message. */
    AiaBinaryMessageType_t type;

    /** The "count" field of the binary message. */
    AiaBinaryMessageCount_t count;

    /** The "data" of the binary message. */
    void* data;
};

/**
 * Initializes a @c AiaBinaryMessage.
 *
 * @param binaryMessage Pointer to a caller-owned @c AiaBinaryMessage to
 * initialize.
 * @param length The length of @c data.
 * @param type The "type" of this binary stream message.
 * @param count The number of binary stream data chunks included in this
 * message.
 * @param data Data associated with this binary stream message.
 * @return @c true if @c message was initialized successfully, else @c false.
 */
static bool AiaBinaryMessage_Initialize( struct AiaBinaryMessage* binaryMessage,
                                         AiaBinaryMessageLength_t length,
                                         AiaBinaryMessageType_t type,
                                         AiaBinaryMessageCount_t count,
                                         void* data );

/**
 * Uninitializes a @c AiaBinaryMessage.  This will free up any internally
 * allocated resources so that @c binaryMessage can be safely deallocated and
 * discarded by the caller without leaking memory.
 *
 * @param message Pointer to a caller-owned @c AiaBinaryMessage to uninitialize.
 */
static void AiaBinaryMessage_Uninitialize(
    struct AiaBinaryMessage* binaryMessage );

AiaBinaryMessage_t* AiaBinaryMessage_Create( AiaBinaryMessageLength_t length,
                                             AiaBinaryMessageType_t type,
                                             AiaBinaryMessageCount_t count,
                                             void* data )
{
    AiaBinaryMessage_t* binaryMessage =
        (AiaBinaryMessage_t*)AiaCalloc( 1, sizeof( AiaBinaryMessage_t ) );
    if( !binaryMessage )
    {
        AiaLogError( "AiaCalloc failed, bytes=%zu.",
                     sizeof( AiaBinaryMessage_t ) );
        return NULL;
    }
    if( !AiaBinaryMessage_Initialize( binaryMessage, length, type, count,
                                      data ) )
    {
        AiaLogError( "_AiaBinaryMessage_Initialize failed." );
        AiaFree( binaryMessage );
        return NULL;
    }
    return binaryMessage;
}

bool AiaBinaryMessage_Initialize( AiaBinaryMessage_t* binaryMessage,
                                  AiaBinaryMessageLength_t length,
                                  AiaBinaryMessageType_t type,
                                  AiaBinaryMessageCount_t count, void* data )
{
    AiaAssert( binaryMessage );
    if( !binaryMessage )
    {
        AiaLogError( "Null binaryMessage" );
        return false;
    }
    if( !data )
    {
        AiaLogError( "Null data." );
        return false;
    }
    if( !length )
    {
        AiaLogError( "Invalid length, length=%" PRIu32, length );
        return false;
    }

    size_t messageSize = AIA_SIZE_OF_BINARY_STREAM_HEADER + length;
    if( !_AiaMessage_Initialize( &binaryMessage->message, messageSize ) )
    {
        AiaLogError( "_AiaMessage_Initialize failed." );
        return false;
    }

    binaryMessage->length = length;
    binaryMessage->type = type;
    binaryMessage->count = count;
    binaryMessage->data = data;
    return true;
}

void AiaBinaryMessage_Uninitialize( AiaBinaryMessage_t* binaryMessage )
{
    AiaAssert( binaryMessage );
    if( !binaryMessage )
    {
        AiaLogError( "Null binaryMessage" );
        return;
    }
    _AiaMessage_Uninitialize( &binaryMessage->message );
}

void AiaBinaryMessage_Destroy( AiaBinaryMessage_t* binaryMessage )
{
    AiaAssert( binaryMessage );
    if( !binaryMessage )
    {
        AiaLogError( "Null binaryMessage" );
        return;
    }
    AiaBinaryMessage_Uninitialize( binaryMessage );
    AiaFree( binaryMessage->data );
    AiaFree( binaryMessage );
}

AiaBinaryMessageLength_t AiaBinaryMessage_GetLength(
    const AiaBinaryMessage_t* binaryMessage )
{
    AiaAssert( binaryMessage );
    if( !binaryMessage )
    {
        AiaLogError( "Null binaryMessage" );
        return 0;
    }
    return binaryMessage->length;
}

AiaBinaryMessageType_t AiaBinaryMessage_GetType(
    const AiaBinaryMessage_t* binaryMessage )
{
    AiaAssert( binaryMessage );
    if( !binaryMessage )
    {
        AiaLogError( "Null binaryMessage" );
        return 0;
    }
    return binaryMessage->type;
}

AiaBinaryMessageCount_t AiaBinaryMessage_GetCount(
    const AiaBinaryMessage_t* binaryMessage )
{
    AiaAssert( binaryMessage );
    if( !binaryMessage )
    {
        AiaLogError( "Null binaryMessage" );
        return 0;
    }
    return binaryMessage->count;
}

const void* AiaBinaryMessage_GetData( const AiaBinaryMessage_t* binaryMessage )
{
    AiaAssert( binaryMessage );
    if( !binaryMessage )
    {
        AiaLogError( "Null binaryMessage" );
        return 0;
    }
    return binaryMessage->data;
}

bool AiaBinaryMessage_BuildMessage( const AiaBinaryMessage_t* binaryMessage,
                                    uint8_t* messageBuffer,
                                    size_t messageBufferSize )
{
    AiaAssert( binaryMessage );
    if( !binaryMessage )
    {
        AiaLogError( "Null binaryMessage" );
        return false;
    }
    if( !messageBuffer )
    {
        AiaLogError( "Null messageBuffer." );
        return false;
    }
    if( messageBufferSize < binaryMessage->message.size )
    {
        AiaLogError(
            "messageBufferSize (%zu) is smaller than binaryMessage's size "
            "(%zu).",
            messageBufferSize, binaryMessage->message.size );
        return false;
    }
    size_t bytePosition = 0;
    for( size_t i = 0; i < sizeof( binaryMessage->length ); ++i )
    {
        messageBuffer[ bytePosition ] = ( binaryMessage->length >> ( i * 8 ) );
        ++bytePosition;
    }
    for( size_t i = 0; i < sizeof( binaryMessage->type ); ++i )
    {
        messageBuffer[ bytePosition ] = ( binaryMessage->type >> ( i * 8 ) );
        ++bytePosition;
    }
    for( size_t i = 0; i < sizeof( binaryMessage->count ); ++i )
    {
        messageBuffer[ bytePosition ] = ( binaryMessage->count >> ( i * 8 ) );
        ++bytePosition;
    }

    for( size_t i = 0; i < AIA_BINARY_MESSAGE_NUM_RESERVED_BYTES; ++i )
    {
        messageBuffer[ bytePosition ] = 0;
        ++bytePosition;
    }

    memcpy( messageBuffer + bytePosition, binaryMessage->data,
            binaryMessage->length );
    return true;
}
