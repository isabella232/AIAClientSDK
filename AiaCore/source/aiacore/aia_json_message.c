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
 * @file aia_json_message.c
 * @brief Implements functions for the AiaJsonMessage_t type.
 */

/* The config header is always included first. */
#include <aia_config.h>

#include <aiacore/aia_json_constants.h>
#include <aiacore/aia_json_message.h>
#include <aiacore/aia_utils.h>
#include <aiacore/private/aia_json_message.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Anchor the inline from aia_json_message.h */
extern inline AiaMessage_t *AiaJsonMessage_ToMessage(
    AiaJsonMessage_t *jsonMessage );
extern inline const AiaMessage_t *AiaJsonMessage_ToConstMessage(
    const AiaJsonMessage_t *jsonMessage );
extern inline AiaJsonMessage_t *AiaJsonMessage_FromMessage(
    AiaMessage_t *message );
extern inline const AiaJsonMessage_t *ConstAiaJsonMessage_FromMessage(
    const AiaMessage_t *message );

/* clang-format off */
#define JSON_MESSAGE_FORMAT_HEADER                                \
    "{"                                                           \
        "\"" AIA_JSON_CONSTANTS_HEADER_KEY "\":"                  \
        "{"                                                       \
            "\"" AIA_JSON_CONSTANTS_NAME_KEY "\":\"%s\","         \
            "\"" AIA_JSON_CONSTANTS_MESSAGE_ID_KEY "\":\"%s\""    \
        "}"
#define JSON_MESSAGE_FORMAT_PAYLOAD                               \
        ",\"" AIA_JSON_CONSTANTS_PAYLOAD_KEY "\":%s"
#define JSON_MESSAGE_END                                          \
    "}"
/* clang-format on */

/**
 * Size of message ID to use in JSON messages.  Needs to be large enough to
 * hold the ID and a null-term.
 */
static const size_t AIA_JSON_MESSAGE_ID_SIZE = 9;

/**
 * Generates the JSON text for a message with the specified @c name, @c
 * messageId and @c payload.  If @c NULL is passed for @c messageBuffer, this
 * function will still calculate the length of the generated JSON string
 * (including the trailing @c '\0').
 *
 * @param messageBuffer A user-provided buffer of at least @c
 * AiaMessage_GetSize() bytes to write the message into.
 * @param messageBufferSize The size (in bytes) of @c messageBuffer.
 * @param name The "name" value of the JSON header subsection.
 * @param messageId The "messageId" value of the JSON header subsection.  If @c
 * NULL, a random message ID will be automatically generated.
 * @param payload The value of the "payload" subsection of the JSON message.  In
 * the case that no "payload" is given in the message, a NULL should be passed
 * in.
 * @return The size (in bytes) of the generated JSON text (excluding the
 * trailing @c '\0'), or zero if there was an error.
 */
static size_t _BuildAiaJsonMessage( char *messageBuffer,
                                    size_t messageBufferSize, const char *name,
                                    const char *messageId, const char *payload )
{
    if( !name )
    {
        AiaLogError( "Null name." );
        return 0;
    }
    size_t messageIdBufferSize = AIA_JSON_MESSAGE_ID_SIZE;
    char messageIdBuffer[ messageIdBufferSize ];
    if( !messageId )
    {
        if( !AiaGenerateMessageId( messageIdBuffer, messageIdBufferSize ) )
        {
            AiaLogError( "Failed to generate message ID." );
            return 0;
        }
        messageId = messageIdBuffer;
    }

    /* Build all of the message except for the end.  Note that snprintf will
     * null-terminate the result, which we will then overrite when we add
     * the end string. */
    int result;
    if( payload )
    {
        result =
            snprintf( messageBuffer, messageBufferSize,
                      JSON_MESSAGE_FORMAT_HEADER JSON_MESSAGE_FORMAT_PAYLOAD,
                      name, messageId, payload );
    }
    else
    {
        result = snprintf( messageBuffer, messageBufferSize,
                           JSON_MESSAGE_FORMAT_HEADER, name, messageId );
    }

    if( result <= 0 )
    {
        AiaLogError( "snprintf failed: %d (errno=%d).", result, errno );
        return 0;
    }

    /* If we're just measuring, add the size of the end and return. */
    size_t endSize = sizeof( JSON_MESSAGE_END ) - 1;
    if( 0 == messageBufferSize )
    {
        return result + endSize;
    }

    /* Otherwise, make sure we have enough space to add the ending. */
    size_t remainder = messageBufferSize - result;
    if( remainder < endSize )
    {
        AiaLogError(
            "Insufficient space (%zu bytes) to add ending (%zu bytes).",
            remainder, endSize );
        return 0;
    }

    /* If there is enough space, include a null-term. */
    size_t nullSize = remainder > endSize;

    /* Append the ending string. */
    strncpy( messageBuffer + result, JSON_MESSAGE_END, endSize + nullSize );

    /* Return value should be the size without counting the null-term. */
    return result + endSize;
}

AiaJsonMessage_t *AiaJsonMessage_Create( const char *name,
                                         const char *messageId,
                                         const char *payload )
{
    size_t jsonMessageSize = sizeof( struct AiaJsonMessage );
    AiaJsonMessage_t *jsonMessage =
        (AiaJsonMessage_t *)AiaCalloc( 1, jsonMessageSize );
    if( !jsonMessage )
    {
        AiaLogError( "AiaCalloc failed (%zu bytes).", jsonMessageSize );
        return NULL;
    }
    if( !_AiaJsonMessage_Initialize( jsonMessage, name, messageId, payload ) )
    {
        AiaLogError( "_AiaJsonMessage_Initialize failed." );
        AiaFree( jsonMessage );
        return NULL;
    }
    return jsonMessage;
}

bool _AiaJsonMessage_Initialize( AiaJsonMessage_t *jsonMessage,
                                 const char *name, const char *messageId,
                                 const char *payload )
{
    if( !jsonMessage )
    {
        AiaLogError( "Null jsonMessage." );
        return false;
    }

    if( !name )
    {
        AiaLogError( "Null name." );
        return false;
    }
    size_t messageIdBufferSize = AIA_JSON_MESSAGE_ID_SIZE;
    char messageIdBuffer[ messageIdBufferSize ];
    if( !messageId )
    {
        if( !AiaGenerateMessageId( messageIdBuffer, messageIdBufferSize ) )
        {
            AiaLogError( "Failed to generate message ID." );
            return false;
        }
        messageId = messageIdBuffer;
    }

    size_t messageSize =
        _BuildAiaJsonMessage( NULL, 0, name, messageId, payload );
    if( !messageSize )
    {
        AiaLogError( "_BuildAiaJsonMessage failed." );
        return false;
    }
    if( !_AiaMessage_Initialize( &jsonMessage->message, messageSize ) )
    {
        AiaLogError( "_AiaMessage_Initialize failed." );
        return false;
    }
    size_t nameSize = strlen( name ) + 1;
    size_t messageIdSize = strlen( messageId ) + 1;
    size_t payloadSize = payload ? strlen( payload ) + 1 : 0;

    size_t stringsSize = nameSize + messageIdSize + payloadSize;
    char *strings = (char *)AiaCalloc( 1, stringsSize );
    if( !strings )
    {
        AiaLogError( "AiaCalloc failed (%zu bytes).", stringsSize );
        return false;
    }

    jsonMessage->name = strings;
    jsonMessage->messageId = jsonMessage->name + nameSize;
    if( payload )
    {
        jsonMessage->payload = jsonMessage->messageId + messageIdSize;
    }

    strncpy( (char *)jsonMessage->name, name, nameSize );
    strncpy( (char *)jsonMessage->messageId, messageId, messageIdSize );
    if( payload )
    {
        strncpy( (char *)jsonMessage->payload, payload, payloadSize );
    }

    return true;
}

void _AiaJsonMessage_Uninitialize( AiaJsonMessage_t *jsonMessage )
{
    if( jsonMessage )
    {
        if( jsonMessage->name )
        {
            AiaFree( (void *)jsonMessage->name );
            jsonMessage->name = NULL;
        }
        _AiaMessage_Uninitialize( &jsonMessage->message );
    }
}

void AiaJsonMessage_Destroy( AiaJsonMessage_t *jsonMessage )
{
    if( jsonMessage )
    {
        _AiaJsonMessage_Uninitialize( jsonMessage );
        AiaFree( jsonMessage );
    }
}

const char *AiaJsonMessage_GetName( const AiaJsonMessage_t *jsonMessage )
{
    AiaAssert( jsonMessage );
    AiaAssert( jsonMessage->name );
    return jsonMessage && jsonMessage->name ? jsonMessage->name : "";
}

const char *AiaJsonMessage_GetMessageId( const AiaJsonMessage_t *jsonMessage )
{
    AiaAssert( jsonMessage );
    AiaAssert( jsonMessage->messageId );
    return jsonMessage && jsonMessage->messageId ? jsonMessage->messageId : "";
}

const char *AiaJsonMessage_GetJsonPayload( const AiaJsonMessage_t *jsonMessage )
{
    AiaAssert( jsonMessage );
    return jsonMessage ? jsonMessage->payload : NULL;
}

bool AiaJsonMessage_BuildMessage( const AiaJsonMessage_t *jsonMessage,
                                  char *messageBuffer,
                                  size_t messageBufferSize )
{
    if( !jsonMessage )
    {
        AiaLogError( "Null jsonMessage." );
        return false;
    }
    if( !messageBuffer )
    {
        AiaLogError( "Null messageBuffer." );
        return false;
    }
    if( messageBufferSize < jsonMessage->message.size )
    {
        AiaLogError(
            "messageBufferSize (%zu) is smaller than jsonMessage's size (%zu).",
            messageBufferSize, jsonMessage->message.size );
        return false;
    }
    size_t size = _BuildAiaJsonMessage(
        messageBuffer, messageBufferSize, jsonMessage->name,
        jsonMessage->messageId, jsonMessage->payload );
    if( size != jsonMessage->message.size )
    {
        AiaLogError(
            "_BuildAiaJsonMessage returned incorrect size (expected %zu, "
            "actual "
            "%zu).",
            jsonMessage->message.size, size );
        return false;
    }
    return true;
}
