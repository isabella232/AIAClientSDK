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

/* The config header is always included first. */
#include "aia_config.h"

#include <aiaconnectionmanager/aia_connection_constants.h>
#include <aiaconnectionmanager/aia_connection_manager.h>
#include <aiacore/aia_directive.h>
#include <aiacore/aia_exception_encountered_utils.h>
#include <aiacore/aia_json_constants.h>
#include <aiacore/aia_json_message.h>
#include <aiacore/aia_json_utils.h>
#include <aiacore/aia_topic.h>
#include <aiacore/aia_utils.h>
#include <aiadispatcher/aia_dispatcher.h>

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

/** The maximum amount of time to wait for a sequence number. */
static const AiaDurationMs_t AIA_SEQUENCER_TIMEOUT = 10000;

/**
 * Sequencer number retrieval callback for sequenced messages.
 *
 * @param[out] sequenceNumber The sequence number to return.
 * @param message Input buffer holding common header and encrypted data.
 * @param size Size of @c message.
 * @param userData User data associated with this callback.
 *
 * @return @c true if sequence number is retrieved successfully, else @c false.
 */
static bool getSequenceNumberCallback( AiaSequenceNumber_t* sequenceNumber,
                                       void* message, size_t size,
                                       void* userData )
{
    if( !sequenceNumber )
    {
        AiaLogError( "Null sequenceNumber." );
        return false;
    }
    if( !message )
    {
        AiaLogError( "Null message." );
        return false;
    }
    (void)userData;
    AiaLogDebug( "Get sequence number callback" );

    size_t bytePosition = 0;

    /* Check for valid size */
    if( size < sizeof( *sequenceNumber ) )
    {
        AiaLogError( "Invalid size: %zu", size );
        return false;
    }

    AiaSequenceNumber_t parsedSeqNumber = 0;
    for( bytePosition = 0; bytePosition < sizeof( *sequenceNumber );
         ++bytePosition )
    {
        parsedSeqNumber |= ( (uint8_t*)message )[ bytePosition ]
                           << ( bytePosition * 8 );
    }
    *sequenceNumber = parsedSeqNumber;

    return true;
}

/**
 * Parses the common header and decrypt the data after the header in a given
 * message.
 *
 * @param aiaDispatcher @c AiaDispatcher_t instance to act on.
 * @param message Input buffer holding common header and encrypted data.
 * @param messageSize Size of @c message.i
 * @param topic Topic on which the @c message is received on.
 * @param encryptedData Input buffer holding encrypted data.
 * @param encryptedSize Size of the @c encryptedData buffer.
 * @param[out] decryptedData Buffer to hold decrypted data.
 * @param[out] sequenceNumber Holds the sequence parsed and decrypted from the
 * common header.
 *
 * @note This function assumes that @c messageSize is larger than @c
 * AIA_COMMON_HEADER_ENCRYPTED_SEQUENCE_OFFSET.
 *
 * @return @c true if common header parsing and decryption is successful, else
 * @c false.
 */
static bool parseHeaderAndDecryptMessage(
    AiaDispatcher_t* aiaDispatcher, const uint8_t* message, size_t messageSize,
    AiaTopic_t topic, const uint8_t* encryptedData, size_t encryptedSize,
    uint8_t** decryptedData, AiaSequenceNumber_t* sequenceNumber )
{
    if( !aiaDispatcher )
    {
        AiaLogError( "Null aiaDispatcher" );
        return false;
    }
    if( !message )
    {
        AiaLogError( "Null message" );
        return false;
    }
    if( !messageSize )
    {
        AiaLogError( "Invalid messageSize" );
        return false;
    }
    if( !encryptedData )
    {
        AiaLogError( "Null encryptedData" );
        return false;
    }
    if( !decryptedData )
    {
        AiaLogError( "Null decryptedData" );
        return false;
    }
    if( !sequenceNumber )
    {
        AiaLogError( "Null sequenceNumber" );
        return false;
    }

    size_t bytePosition = 0;
    size_t ivSize = AIA_COMMON_HEADER_IV_SIZE;
    size_t macSize = AIA_COMMON_HEADER_MAC_SIZE;
    uint8_t iv[ ivSize ];
    uint8_t mac[ macSize ];

    /* Parse the common header */
    if( !getSequenceNumberCallback( sequenceNumber, (void*)message, messageSize,
                                    NULL ) )
    {
        AiaLogError( "Failed to get the sequence number." );
        return false;
    }
    bytePosition += sizeof( AiaSequenceNumber_t );

    for( size_t i = 0; i < sizeof( iv ) / sizeof( iv[ 0 ] );
         ++i, ++bytePosition )
    {
        iv[ i ] = ( (uint8_t*)message )[ bytePosition ];
    }

    for( size_t i = 0; i < sizeof( mac ) / sizeof( mac[ 0 ] );
         ++i, ++bytePosition )
    {
        mac[ i ] = ( (uint8_t*)message )[ bytePosition ];
    }

    AiaLogDebug( "Total message size: %zu, encrypted size: %zu", messageSize,
                 encryptedSize );

    /* Decrypt encrypted payload */
    if( !AiaSecretManager_Decrypt( aiaDispatcher->secretManager, topic,
                                   *sequenceNumber, encryptedData,
                                   encryptedSize, *decryptedData, iv,
                                   sizeof( iv ), mac, sizeof( mac ) ) )
    {
        AiaLogError( "Decryption failed." );
        return false;
    }

    return true;
}

/**
 * Calls the appropriate handler for a given directive received on the directive
 * topic.
 *
 * @param aiaDispatcher @c AiaDispatcher_t instance to act on.
 * @param name Name of the directive.
 * @param nameLength Length of @c name not including the null terminator.
 * @param payload Payload for the directive handler.
 * @param payloadLength Lenght of @c payload not including the null terminator.
 * @param sequenceNumber Sequence number of the message to handle.
 * @param index Index of the message in an array of directives.
 */
static void dispatchDirectiveTopicMessage( AiaDispatcher_t* aiaDispatcher,
                                           const char* name, size_t nameLength,
                                           const char* payload,
                                           size_t payloadLength,
                                           AiaSequenceNumber_t sequenceNumber,
                                           size_t index )
{
    /* Call the appropriate handler */
    /* TODO: ADSER-1734 Send ExceptionEncountered event for missing directive
     * handlers */
    AiaDirective_t parsedDirective;
    if( !AiaDirective_FromString( name, nameLength, &parsedDirective ) )
    {
        AiaLogError( "Failed to parse directive from %.*s", nameLength, name );
        return;
    }
    switch( parsedDirective )
    {
        case AIA_DIRECTIVE_ROTATE_SECRET:
            if( !aiaDispatcher->rotateSecretHandler )
            {
                AiaLogError( "Handler for directive %s not set yet",
                             AiaDirective_ToString( parsedDirective ) );
                return;
            }
            aiaDispatcher->rotateSecretHandler( aiaDispatcher->rotateSecretData,
                                                (void*)payload, payloadLength,
                                                sequenceNumber, index );
            return;
#ifdef AIA_ENABLE_CLOCK
        case AIA_DIRECTIVE_SET_CLOCK:
            if( !aiaDispatcher->setClockHandler )
            {
                AiaLogError( "Handler for directive %s not set yet",
                             AiaDirective_ToString( parsedDirective ) );
                return;
            }
            aiaDispatcher->setClockHandler( aiaDispatcher->setClockData,
                                            (void*)payload, payloadLength,
                                            sequenceNumber, index );
            return;
#endif
        case AIA_DIRECTIVE_EXCEPTION:
            if( !aiaDispatcher->exceptionHandler )
            {
                AiaLogError( "Handler for directive %s not set yet",
                             AiaDirective_ToString( parsedDirective ) );
                return;
            }
            aiaDispatcher->exceptionHandler( aiaDispatcher->exceptionData,
                                             (void*)payload, payloadLength,
                                             sequenceNumber, index );
            return;
        case AIA_DIRECTIVE_SET_ATTENTION_STATE:
            if( !aiaDispatcher->setAttentionStateHandler )
            {
                AiaLogError( "Handler for directive %s not set yet",
                             AiaDirective_ToString( parsedDirective ) );
                return;
            }
            aiaDispatcher->setAttentionStateHandler(
                aiaDispatcher->setAttentionStateData, (void*)payload,
                payloadLength, sequenceNumber, index );
            return;
#ifdef AIA_ENABLE_ALERTS
        case AIA_DIRECTIVE_SET_ALERT_VOLUME:
            if( !aiaDispatcher->setAlertVolumeHandler )
            {
                AiaLogError( "Handler for directive %s not set yet",
                             AiaDirective_ToString( parsedDirective ) );
                return;
            }
            aiaDispatcher->setAlertVolumeHandler(
                aiaDispatcher->setAlertVolumeData, (void*)payload,
                payloadLength, sequenceNumber, index );
            return;
        case AIA_DIRECTIVE_SET_ALERT:
            if( !aiaDispatcher->setAlertHandler )
            {
                AiaLogError( "Handler for directive %s not set yet",
                             AiaDirective_ToString( parsedDirective ) );
                return;
            }
            aiaDispatcher->setAlertHandler( aiaDispatcher->setAlertData,
                                            (void*)payload, payloadLength,
                                            sequenceNumber, index );
            return;
        case AIA_DIRECTIVE_DELETE_ALERT:
            if( !aiaDispatcher->deleteAlertHandler )
            {
                AiaLogError( "Handler for directive %s not set yet",
                             AiaDirective_ToString( parsedDirective ) );
                return;
            }
            aiaDispatcher->deleteAlertHandler( aiaDispatcher->deleteAlertData,
                                               (void*)payload, payloadLength,
                                               sequenceNumber, index );
            return;
#endif
#ifdef AIA_ENABLE_SPEAKER
        case AIA_DIRECTIVE_OPEN_SPEAKER:
            if( !aiaDispatcher->openSpeakerHandler )
            {
                AiaLogError( "Handler for directive %s not set yet",
                             AiaDirective_ToString( parsedDirective ) );
                return;
            }
            aiaDispatcher->openSpeakerHandler( aiaDispatcher->openSpeakerData,
                                               (void*)payload, payloadLength,
                                               sequenceNumber, index );
            return;
        case AIA_DIRECTIVE_CLOSE_SPEAKER:
            if( !aiaDispatcher->closeSpeakerHandler )
            {
                AiaLogError( "Handler for directive %s not set yet",
                             AiaDirective_ToString( parsedDirective ) );
                return;
            }
            aiaDispatcher->closeSpeakerHandler( aiaDispatcher->closeSpeakerData,
                                                (void*)payload, payloadLength,
                                                sequenceNumber, index );
            return;
        case AIA_DIRECTIVE_SET_VOLUME:
            if( !aiaDispatcher->setVolumeHandler )
            {
                AiaLogError( "Handler for directive %s not set yet",
                             AiaDirective_ToString( parsedDirective ) );
                return;
            }
            aiaDispatcher->setVolumeHandler( aiaDispatcher->setVolumeData,
                                             (void*)payload, payloadLength,
                                             sequenceNumber, index );
            return;
#endif
#ifdef AIA_ENABLE_MICROPHONE
        case AIA_DIRECTIVE_OPEN_MICROPHONE:
            if( !aiaDispatcher->openMicrophoneHandler )
            {
                AiaLogError( "Handler for directive %s not set yet",
                             AiaDirective_ToString( parsedDirective ) );
                return;
            }
            aiaDispatcher->openMicrophoneHandler(
                aiaDispatcher->openMicrophoneData, (void*)payload,
                payloadLength, sequenceNumber, index );
            return;
        case AIA_DIRECTIVE_CLOSE_MICROPHONE:
            if( !aiaDispatcher->closeMicrophoneHandler )
            {
                AiaLogError( "Handler for directive %s not set yet",
                             AiaDirective_ToString( parsedDirective ) );
                return;
            }
            aiaDispatcher->closeMicrophoneHandler(
                aiaDispatcher->closeMicrophoneData, (void*)payload,
                payloadLength, sequenceNumber, index );
            return;
#endif
    }
    AiaLogError( "Unknown directive name: %d", parsedDirective );
    AiaJsonMessage_t* malformedMessageEvent =
        generateMalformedMessageExceptionEncounteredEvent(
            sequenceNumber, index, AIA_TOPIC_DIRECTIVE );
    if( !AiaRegulator_Write(
            aiaDispatcher->regulator,
            AiaJsonMessage_ToMessage( malformedMessageEvent ) ) )
    {
        AiaLogError( "Failed to write to regulator." );
        AiaJsonMessage_Destroy( malformedMessageEvent );
    }
}

/**
 * Compares the sequence number parsed from the common header against the
 * decrypted one.
 *
 * @param aiaDispatcher @c AiaDispatcher_t instance to act on.
 * @param expectedSequenceNumber Unencrypted sequence number in the common
 * header.
 * @param decryptedSequenceNumber Decrypted sequence number from the common
 * header.
 *
 * @return @c true if sequence numbers match, else @c false.
 */
static bool checkSequenceNumber( AiaDispatcher_t* aiaDispatcher,
                                 AiaSequenceNumber_t expectedSequenceNumber,
                                 AiaSequenceNumber_t decryptedSequenceNumber )
{
    if( expectedSequenceNumber != decryptedSequenceNumber )
    {
        static const char* formatError =
            "Sequence numbers do not match. Expected: %" PRIu32
            ", decrypted: %" PRIu32 ".";
        int numCharsRequired =
            snprintf( NULL, 0, formatError, expectedSequenceNumber,
                      decryptedSequenceNumber );
        if( numCharsRequired < 0 )
        {
            AiaLogError( "snprintf failed, ret=%d", numCharsRequired );
            return false;
        }
        char fullErrorBuffer[ numCharsRequired + 1 ];
        if( snprintf( fullErrorBuffer, numCharsRequired + 1, formatError,
                      expectedSequenceNumber, decryptedSequenceNumber ) < 0 )
        {
            AiaLogError( "snprintf failed" );
            return false;
        }
        AiaLogError( "%s", fullErrorBuffer );
        if( !AiaConnectionManager_Disconnect(
                aiaDispatcher->connectionManager,
                AIA_CONNECTION_DISCONNECT_MESSAGE_TAMPERED, fullErrorBuffer ) )
        {
            AiaLogError( "Failed to disconnect." );
        }
        return false;
    }

    return true;
}

/**
 * Validates that the input message received on the given @c topic is large
 * enough, parses the common header, decrypts the encrypted payload and
 * compares the decrypted sequence number against the unencrypted sequence
 * number.
 *
 * @param aiaDispatcher @c AiaDispatcher_t instance to act on.
 * @param topic Topic on which the @c message is received on.
 * @param message Input buffer holding common header and encrypted data.
 * @param size Size of the @c message buffer.
 * @param[out] decryptedPayload Buffer to hold the decrypted payload.
 * @param[out] encryptedSize Size of the encrypted data in @c message.
 * @param[out] sequenceNumber Pointer to the unencrypted sequence number in @c
 * message.
 * @param[out] decryptedSequenceNumber Pointer to the decrypted sequence number
 * in @c message.
 *
 * @note It is the responsibility of the caller to call @c AiaFree() on @c
 * decryptedPayload if this function returns @c true.
 *
 * @return @c true if @c message is validated successfully, else @c false.
 */
static bool validateAndDecryptMessage(
    AiaDispatcher_t* dispatcher, AiaTopic_t topic, void* message, size_t size,
    uint8_t** decryptedPayload, size_t* encryptedSize,
    AiaSequenceNumber_t* sequenceNumber,
    AiaSequenceNumber_t* decryptedSequenceNumber )
{
    /* Check that we received a message that is large enough */
    if( size < AIA_COMMON_HEADER_ENCRYPTED_SEQUENCE_OFFSET )
    {
        AiaLogError(
            "Received message is smaller than the encrypted sequence offset: "
            "%zu",
            size );
        AiaJsonMessage_t* malformedMessageEvent =
            generateMalformedMessageExceptionEncounteredEvent( 0, 0, topic );
        if( !AiaRegulator_Write(
                dispatcher->regulator,
                AiaJsonMessage_ToMessage( malformedMessageEvent ) ) )
        {
            AiaLogError( "Failed to write to regulator." );
            AiaJsonMessage_Destroy( malformedMessageEvent );
        }
        return false;
    }

    /* Extract the common header and decrypt the encrypted payload */
    uint8_t* encryptedPayload =
        (uint8_t*)message + AIA_COMMON_HEADER_ENCRYPTED_SEQUENCE_OFFSET;
    *encryptedSize = size - AIA_COMMON_HEADER_ENCRYPTED_SEQUENCE_OFFSET;

    *decryptedPayload =
        AiaCalloc( *encryptedSize, sizeof( encryptedPayload[ 0 ] ) );
    if( !*decryptedPayload )
    {
        AiaLogError( "AiaCalloc failed, bytes=%zu", *encryptedSize );
        return false;
    }

    if( !parseHeaderAndDecryptMessage( dispatcher, (uint8_t*)message, size,
                                       topic, encryptedPayload, *encryptedSize,
                                       decryptedPayload, sequenceNumber ) )
    {
        AiaLogError( "Failed to decrypt sequenced data" );
        AiaFree( *decryptedPayload );
        if( !AiaConnectionManager_Disconnect(
                dispatcher->connectionManager,
                AIA_CONNECTION_DISCONNECT_ENCRYPTION_ERROR,
                "Failed to decrypt sequenced data" ) )
        {
            AiaLogError( "Failed to disconnect." );
        }

        return false;
    }

    /* Check that the decrypted sequence number matches with the unencrypted one
     */
    if( !getSequenceNumberCallback( decryptedSequenceNumber,
                                    (void*)*decryptedPayload, *encryptedSize,
                                    NULL ) )
    {
        AiaLogError( "Failed to get the sequence number." );
        AiaFree( *decryptedPayload );
        return false;
    }
    if( !checkSequenceNumber( dispatcher, *sequenceNumber,
                              *decryptedSequenceNumber ) )
    {
        AiaLogError( "Sequence number checking failed." );
        AiaFree( *decryptedPayload );
        return false;
    }

    return true;
}

/**
 * Parses individual fields from a given JSON message.
 *
 * @param messageToParse Message to parse individual fields from.
 * @param[out] name Pointer to the name field in @c messageToParse.
 * @param[out] messageId Pointer to the messageId field in @c messageToParse.
 * @param[out] payload Pointer to the payload field in @c messageToParse.
 * @param[out] nameLength Length of @c name not including the null-terminator.
 * @param[out] messageIdLength Length of @c messageId not including the
 * null-terminator.
 * @param[out] payloadLength Length of @c payload not including the
 * null-terminator.
 *
 * @return @c true if message parsing is successful, else @c false.
 */
static bool parseMessageFields( const char* messageToParse, const char** name,
                                const char** messageId, const char** payload,
                                size_t* nameLength, size_t* messageIdLength,
                                size_t* payloadLength )
{
    const char* parsedField = NULL;

    /* Parse name */
    if( !AiaFindJsonValue( messageToParse, strlen( messageToParse ),
                           AIA_JSON_CONSTANTS_NAME_KEY,
                           sizeof( AIA_JSON_CONSTANTS_NAME_KEY ) - 1,
                           &parsedField, nameLength ) )
    {
        AiaLogError( "Failed to parse the %s key in the payload",
                     AIA_JSON_CONSTANTS_NAME_KEY );
        return false;
    }
    *name = parsedField;

    /* Parse message id */
    if( !AiaFindJsonValue( messageToParse, strlen( messageToParse ),
                           AIA_JSON_CONSTANTS_MESSAGE_ID_KEY,
                           sizeof( AIA_JSON_CONSTANTS_MESSAGE_ID_KEY ) - 1,
                           &parsedField, messageIdLength ) )
    {
        AiaLogError( "Failed to parse the %s key in the payload",
                     AIA_JSON_CONSTANTS_MESSAGE_ID_KEY );
        return false;
    }
    *messageId = parsedField;

    /* Parse payload */
    if( !AiaFindJsonValue( messageToParse, strlen( messageToParse ),
                           AIA_JSON_CONSTANTS_PAYLOAD_KEY,
                           sizeof( AIA_JSON_CONSTANTS_PAYLOAD_KEY ) - 1,
                           &parsedField, payloadLength ) )
    {
        if( messageIdLength )
        {
            AiaLogDebug(
                "Failed to parse the %s key in the payload, messageId: %.*s",
                AIA_JSON_CONSTANTS_PAYLOAD_KEY, *messageIdLength, *messageId );
        }
        else
        {
            AiaLogDebug( "Failed to parse the %s key in the payload",
                         AIA_JSON_CONSTANTS_PAYLOAD_KEY );
        }
        *payload = NULL;
        return true;
    }
    *payload = parsedField;

    return true;
}

/**
 * Sequencer callback for sequenced messages on the directive topic.
 *
 * @param message Input buffer holding common header and encrypted data.
 * @param size Size of the @c message buffer.
 * @param userData User data associated with this callback.
 */
static void directiveMessageSequencedCallback( void* message, size_t size,
                                               void* userData )
{
    if( !message )
    {
        AiaLogError( "Null message." );
        return;
    }
    if( !userData )
    {
        AiaLogError( "Null userData." );
        return;
    }

    AiaDispatcher_t* aiaDispatcher = (AiaDispatcher_t*)userData;

    const char *name = NULL, *messageId = NULL, *payload = NULL,
               *textToParse = NULL;
    size_t nameLength = 0, messageIdLength = 0, payloadLength = 0;

    AiaLogDebug( "Message on directive topic sequenced" );

    /* Validate the payload */
    size_t encryptedSize = 0;
    uint8_t* decryptedPayload = NULL;
    AiaSequenceNumber_t sequenceNumber = 0, decryptedSequenceNumber = 0;

    if( !validateAndDecryptMessage( aiaDispatcher, AIA_TOPIC_DIRECTIVE, message,
                                    size, &decryptedPayload, &encryptedSize,
                                    &sequenceNumber,
                                    &decryptedSequenceNumber ) )
    {
        AiaLogError( "Failed to validate the payload" );
        return;
    }

    /* Assign parsed text to the decrypted-sequenced payload */
    size_t bytePosition = sizeof( AiaSequenceNumber_t );
    textToParse = (char*)decryptedPayload + bytePosition;

    /* TODO: ADSER-1986 Selectively choose which directives are sensitive. */
    AiaLogSensitive( "Parsing %.*s", encryptedSize - bytePosition,
                     textToParse );

    /* Get the topic array name */
    const char* arrayName = AiaTopic_GetJsonArrayName( AIA_TOPIC_DIRECTIVE );
    if( !arrayName )
    {
        AiaLogError( "Failed to get array name for the directive topic" );
        AiaFree( decryptedPayload );
        return;
    }

    size_t arrayNameLength =
        AiaTopic_GetJsonArrayNameLength( AIA_TOPIC_DIRECTIVE );

    /* Extract the array. */
    const char* array;
    size_t arrayLength;
    if( !AiaFindJsonValue( textToParse, encryptedSize - bytePosition, arrayName,
                           arrayNameLength, &array, &arrayLength ) )
    {
        AiaLogError( "Could not find \"%.*s\" array in message.",
                     arrayNameLength, arrayName );
        AiaJsonMessage_t* malformedMessageEvent =
            generateMalformedMessageExceptionEncounteredEvent(
                sequenceNumber, 0, AIA_TOPIC_DIRECTIVE );
        if( !AiaRegulator_Write(
                aiaDispatcher->regulator,
                AiaJsonMessage_ToMessage( malformedMessageEvent ) ) )
        {
            AiaLogError( "Failed to write to regulator." );
            AiaJsonMessage_Destroy( malformedMessageEvent );
        }
        AiaFree( decryptedPayload );
        return;
    }

    const char* arrayElement;
    size_t arrayElementLength;
    size_t index = 0;
    /* TODO: ADSER-1654 Rework AiaJsonUtils_GetArrayElement() so that it can be
     * used iteratively */
    while( AiaJsonUtils_GetArrayElement( array, arrayLength, index,
                                         &arrayElement, &arrayElementLength ) )
    {
        /* Parse individual message fields */
        if( !parseMessageFields( arrayElement, &name, &messageId, &payload,
                                 &nameLength, &messageIdLength,
                                 &payloadLength ) )
        {
            AiaLogError( "Failed to parse message fields." );
            AiaJsonMessage_t* malformedMessageEvent =
                generateMalformedMessageExceptionEncounteredEvent(
                    sequenceNumber, index, AIA_TOPIC_DIRECTIVE );
            if( !AiaRegulator_Write(
                    aiaDispatcher->regulator,
                    AiaJsonMessage_ToMessage( malformedMessageEvent ) ) )
            {
                AiaLogError( "Failed to write to regulator." );
                AiaJsonMessage_Destroy( malformedMessageEvent );
            }
            AiaFree( decryptedPayload );
            return;
        }

        /* Unquote the directive name */
        if( !AiaJsonUtils_UnquoteString( &name, &nameLength ) )
        {
            AiaLogError(
                "Failed to unquote the directive name, messageId: %.*s",
                messageIdLength, messageId );
            AiaJsonMessage_t* malformedMessageEvent =
                generateMalformedMessageExceptionEncounteredEvent(
                    sequenceNumber, index, AIA_TOPIC_DIRECTIVE );
            if( !AiaRegulator_Write(
                    aiaDispatcher->regulator,
                    AiaJsonMessage_ToMessage( malformedMessageEvent ) ) )
            {
                AiaLogError( "Failed to write to regulator." );
                AiaJsonMessage_Destroy( malformedMessageEvent );
            }
            AiaFree( decryptedPayload );
            return;
        }

        /* TODO: ADSER-1986 Selectively choose which directives are sensitive.
         */
        AiaLogSensitive( "%.*s %.*s %.*s", nameLength, name, messageIdLength,
                         messageId, payloadLength, payload );

        dispatchDirectiveTopicMessage( aiaDispatcher, name, nameLength, payload,
                                       payloadLength, decryptedSequenceNumber,
                                       index );

        index++;
    }

    AiaFree( decryptedPayload );
}

/**
 * Sequencer callback for sequenced messages on the capabilities acknowledge
 * topic.
 *
 * @param message Input buffer holding common header and encrypted data.
 * @param size Size of the @c message buffer.
 * @param userData User data associated with this callback.
 */
static void capabilitiesMessageSequencedCallback( void* message, size_t size,
                                                  void* userData )
{
    if( !message )
    {
        AiaLogError( "Null message." );
        return;
    }
    if( !userData )
    {
        AiaLogError( "Null userData." );
        return;
    }

    AiaDispatcher_t* aiaDispatcher = (AiaDispatcher_t*)userData;

    const char *name = NULL, *messageId = NULL, *payload = NULL,
               *textToParse = NULL;
    size_t nameLength = 0, messageIdLength = 0, payloadLength = 0;

    AiaLogDebug( "Message on capabilities acknowledge sequenced" );

    /* Validate the payload */
    size_t encryptedSize = 0;
    uint8_t* decryptedPayload = NULL;
    AiaSequenceNumber_t sequenceNumber = 0, decryptedSequenceNumber = 0;
    if( !validateAndDecryptMessage(
            aiaDispatcher, AIA_TOPIC_CAPABILITIES_ACKNOWLEDGE, message, size,
            &decryptedPayload, &encryptedSize, &sequenceNumber,
            &decryptedSequenceNumber ) )
    {
        AiaLogError( "Failed to validate the payload" );
        return;
    }

    /* Assign parsed text to the decrypted-sequenced payload */
    size_t bytePosition = sizeof( AiaSequenceNumber_t );
    textToParse = (char*)decryptedPayload + bytePosition;

    AiaLogDebug( "Parsing %.*s", encryptedSize - bytePosition, textToParse );

    /* Parse individual message fields */
    if( !parseMessageFields( textToParse, &name, &messageId, &payload,
                             &nameLength, &messageIdLength, &payloadLength ) )
    {
        AiaLogError( "Failed to parse message fields." );
        AiaJsonMessage_t* malformedMessageEvent =
            generateMalformedMessageExceptionEncounteredEvent(
                sequenceNumber, 0, AIA_TOPIC_CAPABILITIES_ACKNOWLEDGE );
        if( !AiaRegulator_Write(
                aiaDispatcher->regulator,
                AiaJsonMessage_ToMessage( malformedMessageEvent ) ) )
        {
            AiaLogError( "Failed to write to regulator." );
            AiaJsonMessage_Destroy( malformedMessageEvent );
        }
        AiaFree( decryptedPayload );
        return;
    }

    /* Pass the payload to the handler */
    AiaLogDebug( "%.*s %.*s %.*s", nameLength, name, messageIdLength, messageId,
                 payloadLength, payload );

    AiaCapabilitiesSender_OnCapabilitiesAcknowledgeMessageReceived(
        aiaDispatcher->capabilitiesSender, payload, payloadLength );

    AiaFree( decryptedPayload );
}

#ifdef AIA_ENABLE_SPEAKER
/**
 * Sequencer callback for sequenced messages on the speaker topic.
 *
 * @param message Input buffer holding common header and encrypted data.
 * @param size Size of the @c message buffer.
 * @param userData User data associated with this callback.
 */
static void speakerMessageSequencedCallback( void* message, size_t size,
                                             void* userData )
{
    if( !message )
    {
        AiaLogError( "Null message." );
        return;
    }
    if( !userData )
    {
        AiaLogError( "Null userData." );
        return;
    }

    AiaDispatcher_t* aiaDispatcher = (AiaDispatcher_t*)userData;

    AiaLogDebug( "Message on speaker topic sequenced" );

    /* Validate the payload */
    size_t encryptedSize = 0;
    uint8_t* decryptedPayload = NULL;
    AiaSequenceNumber_t sequenceNumber = 0, decryptedSequenceNumber = 0;
    if( !validateAndDecryptMessage(
            aiaDispatcher, AIA_TOPIC_SPEAKER, message, size, &decryptedPayload,
            &encryptedSize, &sequenceNumber, &decryptedSequenceNumber ) )
    {
        AiaLogError( "Failed to validate the payload" );
        return;
    }

    /* Advance bytePosition to point at the offset of data in decrypted payload
     */
    size_t bytePosition = sizeof( AiaSequenceNumber_t );

    /* Call the appropriate handler */
    AiaSpeakerManager_OnSpeakerTopicMessageReceived(
        aiaDispatcher->speakerManager, decryptedPayload + bytePosition,
        encryptedSize - bytePosition, decryptedSequenceNumber );

    AiaFree( decryptedPayload );
}
#endif

/**
 * Sequencer timeout callback for messages written to the sequencer.
 *
 * @param userData User data associated with this callback.
 */
static void sequencerTimedoutCallback( void* userData )
{
    if( !userData )
    {
        AiaLogError( "Null userData." );
        return;
    }

    AiaDispatcher_t* aiaDispatcher = (AiaDispatcher_t*)userData;

    AiaLogDebug( "Timed out waiting to sequence message" );

    if( !AiaConnectionManager_Disconnect(
            aiaDispatcher->connectionManager,
            AIA_CONNECTION_DISCONNECT_UNEXPECTED_SEQUENCE_NUMBER,
            "Timed out waiting to sequence message" ) )
    {
        AiaLogError( "Failed to disconnect." );
    }
}

void messageReceivedCallback( void* callbackArg,
                              AiaMqttCallbackParam_t* callbackParam )
{
    if( !callbackArg )
    {
        AiaLogError( "Null callback argument" );
        return;
    }

    AiaDispatcher_t* dispatcher = (AiaDispatcher_t*)callbackArg;

    if( strncmp( callbackParam->u.message.info.pTopicName,
                 dispatcher->deviceTopicRoot,
                 AiaMin( callbackParam->u.message.info.topicNameLength,
                         dispatcher->deviceTopicRootSize ) ) )
    {
        AiaLogError( "Incorrect topic root %.*s",
                     callbackParam->u.message.info.topicNameLength,
                     callbackParam->u.message.info.pTopicName );
        return;
    }

    /* Parse the topic after leaving the topic root out */
    size_t topicStringLength = callbackParam->u.message.info.topicNameLength -
                               dispatcher->deviceTopicRootSize;

    AiaTopic_t parsedTopic;
    if( !AiaTopic_FromString( callbackParam->u.message.info.pTopicName +
                                  dispatcher->deviceTopicRootSize,
                              topicStringLength, &parsedTopic ) )
    {
        AiaLogError( "Failed to parse topic from %.*s", topicStringLength,
                     callbackParam->u.message.info.pTopicName +
                         dispatcher->deviceTopicRootSize );
        return;
    }

    /* Forward the incoming message to the correct handler or sequencer based on
     * the topic */
    switch( parsedTopic )
    {
        case AIA_TOPIC_CONNECTION_FROM_CLIENT:
        case AIA_TOPIC_CAPABILITIES_PUBLISH:
        case AIA_TOPIC_EVENT:
        case AIA_TOPIC_MICROPHONE:
            return;
        case AIA_TOPIC_DIRECTIVE:
            AiaLogDebug( "Calling the directive sequencer" );
            AiaMutex( Lock )( &dispatcher->mutex );
            if( !AiaSequencer_Write(
                    dispatcher->directiveSequencer,
                    (void*)callbackParam->u.message.info.pPayload,
                    callbackParam->u.message.info.payloadLength ) )
            {
                AiaLogError(
                    "Failed to write incoming data to the directive "
                    "sequencer" );
                AiaMutex( Unlock )( &dispatcher->mutex );
                AiaJsonMessage_t* malformedMessageEvent =
                    generateMalformedMessageExceptionEncounteredEvent(
                        0, 0, AIA_TOPIC_DIRECTIVE );
                if( !AiaRegulator_Write(
                        dispatcher->regulator,
                        AiaJsonMessage_ToMessage( malformedMessageEvent ) ) )
                {
                    AiaLogError( "Failed to write to regulator." );
                    AiaJsonMessage_Destroy( malformedMessageEvent );
                }
                return;
            }
            AiaMutex( Unlock )( &dispatcher->mutex );
            return;
        case AIA_TOPIC_SPEAKER:
#ifdef AIA_ENABLE_SPEAKER
            AiaLogDebug( "Calling the speaker sequencer" );
            AiaMutex( Lock )( &dispatcher->mutex );
            if( !AiaSequencer_Write(
                    dispatcher->speakerSequencer,
                    (void*)callbackParam->u.message.info.pPayload,
                    callbackParam->u.message.info.payloadLength ) )
            {
                AiaLogError(
                    "Failed to write incoming data to the speaker sequencer" );
                AiaMutex( Unlock )( &dispatcher->mutex );
                AiaJsonMessage_t* malformedMessageEvent =
                    generateMalformedMessageExceptionEncounteredEvent(
                        0, 0, AIA_TOPIC_SPEAKER );
                if( !AiaRegulator_Write(
                        dispatcher->regulator,
                        AiaJsonMessage_ToMessage( malformedMessageEvent ) ) )
                {
                    AiaLogError( "Failed to write to regulator." );
                    AiaJsonMessage_Destroy( malformedMessageEvent );
                }
                return;
            }
#endif
            AiaMutex( Unlock )( &dispatcher->mutex );
            return;
        case AIA_TOPIC_CAPABILITIES_ACKNOWLEDGE:
            AiaLogDebug( "Calling the capabilities acknowledge sequencer" );
            AiaMutex( Lock )( &dispatcher->mutex );
            if( !AiaSequencer_Write(
                    dispatcher->capabilitiesAcknowledgeSequencer,
                    (void*)callbackParam->u.message.info.pPayload,
                    callbackParam->u.message.info.payloadLength ) )
            {
                /* TODO: ADSER-1687 Return Errors on Connection Messages not
                 * Handled by Aia Instances */
                AiaLogError(
                    "Failed to write incoming data to the capabilities "
                    "acknowledge "
                    "sequencer" );
                AiaMutex( Unlock )( &dispatcher->mutex );
                AiaJsonMessage_t* malformedMessageEvent =
                    generateMalformedMessageExceptionEncounteredEvent(
                        0, 0, AIA_TOPIC_CAPABILITIES_ACKNOWLEDGE );
                if( !AiaRegulator_Write(
                        dispatcher->regulator,
                        AiaJsonMessage_ToMessage( malformedMessageEvent ) ) )
                {
                    AiaLogError( "Failed to write to regulator." );
                    AiaJsonMessage_Destroy( malformedMessageEvent );
                }
                return;
            }
            AiaMutex( Unlock )( &dispatcher->mutex );
            return;
        case AIA_TOPIC_CONNECTION_FROM_SERVICE:
            AiaLogDebug( "Calling the service connection message handler" );

            const char* name;
            size_t nameLength = 0;
            if( !AiaFindJsonValue(
                    (char*)callbackParam->u.message.info.pPayload,
                    callbackParam->u.message.info.payloadLength,
                    AIA_JSON_CONSTANTS_NAME_KEY,
                    sizeof( AIA_JSON_CONSTANTS_NAME_KEY ) - 1, &name,
                    &nameLength ) )
            {
                /* TODO: ADSER-1687 Return Errors on Connection Messages not
                 * Handled by Aia Instances */
                AiaLogError( "Failed to parse the %s key in the header",
                             AIA_JSON_CONSTANTS_NAME_KEY );
                AiaJsonMessage_t* malformedMessageEvent =
                    generateMalformedMessageExceptionEncounteredEvent(
                        0, 0, AIA_TOPIC_CONNECTION_FROM_SERVICE );
                if( !AiaRegulator_Write(
                        dispatcher->regulator,
                        AiaJsonMessage_ToMessage( malformedMessageEvent ) ) )
                {
                    AiaLogError( "Failed to write to regulator." );
                    AiaJsonMessage_Destroy( malformedMessageEvent );
                }
                return;
            }
            else if( !AiaJsonUtils_UnquoteString( &name, &nameLength ) )
            {
                /* TODO: ADSER-1687 Return Errors on Connection Messages not
                 * Handled by Aia Instances */
                AiaLogError( "Malformed JSON" );
                AiaJsonMessage_t* malformedMessageEvent =
                    generateMalformedMessageExceptionEncounteredEvent(
                        0, 0, AIA_TOPIC_CONNECTION_FROM_SERVICE );
                if( !AiaRegulator_Write(
                        dispatcher->regulator,
                        AiaJsonMessage_ToMessage( malformedMessageEvent ) ) )
                {
                    AiaLogError( "Failed to write to regulator." );
                    AiaJsonMessage_Destroy( malformedMessageEvent );
                }
                return;
            }

            if( !strncmp( name, AIA_CONNECTION_ACK_NAME, nameLength ) )
            {
                AiaConnectionManager_OnConnectionAcknowledgementReceived(
                    dispatcher->connectionManager,
                    (char*)callbackParam->u.message.info.pPayload,
                    callbackParam->u.message.info.payloadLength );
            }
            else if( !strncmp( name, AIA_CONNECTION_DISCONNECT_NAME,
                               nameLength ) )
            {
                AiaConnectionManager_OnConnectionDisconnectReceived(
                    dispatcher->connectionManager,
                    (char*)callbackParam->u.message.info.pPayload,
                    callbackParam->u.message.info.payloadLength );
            }
            else
            {
                /* TODO: ADSER-1687 Return Errors on Connection Messages not
                 * Handled by Aia Instances */
                AiaLogError(
                    "No service connection message handler for name: %s",
                    name );
                AiaJsonMessage_t* malformedMessageEvent =
                    generateMalformedMessageExceptionEncounteredEvent(
                        0, 0, AIA_TOPIC_DIRECTIVE );
                if( !AiaRegulator_Write(
                        dispatcher->regulator,
                        AiaJsonMessage_ToMessage( malformedMessageEvent ) ) )
                {
                    AiaLogError( "Failed to write to regulator." );
                    AiaJsonMessage_Destroy( malformedMessageEvent );
                }
            }
            return;
        case AIA_NUM_TOPICS:
            break;
    }
    AiaLogError( "Received message on an unknown topic %d.", parsedTopic );
}

AiaDispatcher_t* AiaDispatcher_Create(
    AiaTaskPool_t aiaTaskPool, AiaCapabilitiesSender_t* capabilitiesSender,
    AiaRegulator_t* regulator, AiaSecretManager_t* secretManager )
{
    if( !aiaTaskPool )
    {
        AiaLogError( "Null aiaTaskPool." );
        return NULL;
    }
    if( !capabilitiesSender )
    {
        AiaLogError( "Null capabilitiesSender." );
        return NULL;
    }
    if( !regulator )
    {
        AiaLogError( "Null regulator." );
        return NULL;
    }
    if( !secretManager )
    {
        AiaLogError( "Null secretManager." );
        return NULL;
    }

    size_t deviceTopicRootSize = AiaGetDeviceTopicRootString( NULL, 0 );
    if( !deviceTopicRootSize )
    {
        AiaLogError( "AiaGetDeviceTopicRootString failed" );
        return NULL;
    }

    size_t dispatcherSize = sizeof( struct AiaDispatcher );
    AiaDispatcher_t* dispatcher =
        (AiaDispatcher_t*)AiaCalloc( 1, dispatcherSize + deviceTopicRootSize );
    if( !dispatcher )
    {
        AiaLogError( "AiaCalloc failed (%zu bytes).", dispatcherSize );
        return NULL;
    }

    dispatcher->deviceTopicRoot = (char*)( dispatcher + 1 );

    if( !AiaMutex( Create )( &dispatcher->mutex, false ) )
    {
        AiaLogError( "AiaMutex( Create ) failed." );
        AiaFree( dispatcher );
        return NULL;
    }

    *(AiaCapabilitiesSender_t**)&dispatcher->capabilitiesSender =
        capabilitiesSender;
    *(AiaRegulator_t**)&dispatcher->regulator = regulator;
    *(AiaSecretManager_t**)&dispatcher->secretManager = secretManager;

    deviceTopicRootSize = AiaGetDeviceTopicRootString(
        dispatcher->deviceTopicRoot, deviceTopicRootSize );
    if( !deviceTopicRootSize )
    {
        AiaLogError( "AiaGetDeviceTopicRootString failed" );
        AiaMutex( Destroy )( &dispatcher->mutex );
        AiaFree( dispatcher );
        return NULL;
    }
    dispatcher->deviceTopicRootSize = deviceTopicRootSize;

    dispatcher->directiveSequencer = AiaSequencer_Create(
        directiveMessageSequencedCallback, dispatcher,
        sequencerTimedoutCallback, dispatcher, getSequenceNumberCallback, NULL,
        AIA_SEQUENCER_SLOTS, 0, AIA_SEQUENCER_TIMEOUT, aiaTaskPool );
    if( !dispatcher->directiveSequencer )
    {
        AiaLogError(
            "AiaSequencer_Create failed to create directiveSequencer" );
        AiaDispatcher_Destroy( dispatcher );
        return NULL;
    }

    dispatcher->capabilitiesAcknowledgeSequencer = AiaSequencer_Create(
        capabilitiesMessageSequencedCallback, dispatcher,
        sequencerTimedoutCallback, dispatcher, getSequenceNumberCallback, NULL,
        AIA_SEQUENCER_SLOTS, 0, AIA_SEQUENCER_TIMEOUT, aiaTaskPool );
    if( !dispatcher->capabilitiesAcknowledgeSequencer )
    {
        AiaLogError(
            "AiaSequencer_Create failed to create "
            "capabilitiesAcknowledgeSequencer" );
        AiaDispatcher_Destroy( dispatcher );
        return NULL;
    }

#ifdef AIA_ENABLE_SPEAKER
    dispatcher->speakerSequencer = AiaSequencer_Create(
        speakerMessageSequencedCallback, dispatcher, sequencerTimedoutCallback,
        dispatcher, getSequenceNumberCallback, NULL, AIA_SEQUENCER_SLOTS, 0,
        AIA_SEQUENCER_TIMEOUT, aiaTaskPool );
    if( !dispatcher->speakerSequencer )
    {
        AiaLogError( "AiaSequencer_Create failed to create speakerSequencer" );
        AiaDispatcher_Destroy( dispatcher );
        return NULL;
    }
#endif

    return dispatcher;
}

bool AiaDispatcher_AddHandler( AiaDispatcher_t* dispatcher,
                               AiaDirectiveHandler_t handler,
                               AiaDirective_t directive, void* userData )
{
    if( !dispatcher )
    {
        AiaLogError( "Null dispatcher." );
        return false;
    }
    if( !handler )
    {
        AiaLogError( "Null handler." );
        return false;
    }
    if( !userData )
    {
        AiaLogError( "Null userData." );
        return false;
    }

    switch( directive )
    {
#ifdef AIA_ENABLE_SPEAKER
        case AIA_DIRECTIVE_OPEN_SPEAKER:
            *(AiaDirectiveHandler_t*)&( dispatcher->openSpeakerHandler ) =
                handler;
            dispatcher->openSpeakerData = (AiaSpeakerManager_t*)userData;
            return true;
        case AIA_DIRECTIVE_CLOSE_SPEAKER:
            *(AiaDirectiveHandler_t*)&( dispatcher->closeSpeakerHandler ) =
                handler;
            dispatcher->closeSpeakerData = (AiaSpeakerManager_t*)userData;
            return true;
        case AIA_DIRECTIVE_SET_VOLUME:
            *(AiaDirectiveHandler_t*)&( dispatcher->setVolumeHandler ) =
                handler;
            dispatcher->setVolumeData = (AiaSpeakerManager_t*)userData;
            return true;
#endif
#ifdef AIA_ENABLE_MICROPHONE
        case AIA_DIRECTIVE_OPEN_MICROPHONE:
            *(AiaDirectiveHandler_t*)&( dispatcher->openMicrophoneHandler ) =
                handler;
            dispatcher->openMicrophoneData = (AiaMicrophoneManager_t*)userData;
            return true;
        case AIA_DIRECTIVE_CLOSE_MICROPHONE:
            *(AiaDirectiveHandler_t*)&( dispatcher->closeMicrophoneHandler ) =
                handler;
            dispatcher->closeMicrophoneData = (AiaMicrophoneManager_t*)userData;
            return true;
#endif
        case AIA_DIRECTIVE_SET_ATTENTION_STATE:
            *(AiaDirectiveHandler_t*)&( dispatcher->setAttentionStateHandler ) =
                handler;
            dispatcher->setAttentionStateData = (AiaUXManager_t*)userData;
            return true;
        case AIA_DIRECTIVE_ROTATE_SECRET:
            *(AiaDirectiveHandler_t*)&( dispatcher->rotateSecretHandler ) =
                handler;
            dispatcher->rotateSecretData = (AiaSecretManager_t*)userData;
            return true;
#ifdef AIA_ENABLE_ALERTS
        case AIA_DIRECTIVE_SET_ALERT_VOLUME:
            *(AiaDirectiveHandler_t*)&( dispatcher->setAlertVolumeHandler ) =
                handler;
            dispatcher->setAlertVolumeData = (AiaAlertManager_t*)userData;
            return true;
        case AIA_DIRECTIVE_SET_ALERT:
            *(AiaDirectiveHandler_t*)&( dispatcher->setAlertHandler ) = handler;
            dispatcher->setAlertData = (AiaAlertManager_t*)userData;
            return true;
        case AIA_DIRECTIVE_DELETE_ALERT:
            *(AiaDirectiveHandler_t*)&( dispatcher->deleteAlertHandler ) =
                handler;
            dispatcher->deleteAlertData = (AiaAlertManager_t*)userData;
            return true;
#endif
#ifdef AIA_ENABLE_CLOCK
        case AIA_DIRECTIVE_SET_CLOCK:
            *(AiaDirectiveHandler_t*)&( dispatcher->setClockHandler ) = handler;
            dispatcher->setClockData = (AiaClockManager_t*)userData;
            return true;
#endif
        case AIA_DIRECTIVE_EXCEPTION:
            *(AiaDirectiveHandler_t*)&( dispatcher->exceptionHandler ) =
                handler;
            dispatcher->exceptionData = (AiaExceptionManager_t*)userData;
            return true;
    }
    AiaLogError( "Unknown directive name: %d", directive );
    return false;
}

void AiaDispatcher_AddConnectionManager(
    AiaDispatcher_t* dispatcher, AiaConnectionManager_t* connectionManager )
{
    if( !dispatcher )
    {
        AiaLogError( "Null dispatcher." );
        return;
    }
    *(AiaConnectionManager_t**)&dispatcher->connectionManager =
        connectionManager;
}

#ifdef AIA_ENABLE_SPEAKER
void AiaDispatcher_AddSpeakerManager( AiaDispatcher_t* dispatcher,
                                      AiaSpeakerManager_t* speakerManager )
{
    if( !dispatcher )
    {
        AiaLogError( "Null dispatcher." );
        return;
    }
    *(AiaSpeakerManager_t**)&dispatcher->speakerManager = speakerManager;
}
#endif

void AiaDispatcher_Destroy( AiaDispatcher_t* dispatcher )
{
    if( !dispatcher )
    {
        AiaLogDebug( "Null dispatcher." );
        return;
    }

    AiaMutex( Lock )( &dispatcher->mutex );
    AiaSequencer_Destroy( dispatcher->capabilitiesAcknowledgeSequencer );
    AiaSequencer_Destroy( dispatcher->directiveSequencer );
#ifdef AIA_ENABLE_SPEAKER
    AiaSequencer_Destroy( dispatcher->speakerSequencer );
#endif
    AiaMutex( Unlock )( &dispatcher->mutex );
    AiaMutex( Destroy )( &dispatcher->mutex );
    AiaFree( dispatcher );
}
