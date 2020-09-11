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
 * @file aia_secret_manager.c
 * @brief Implements functions for the AiaSecretManager_t type.
 */

/* The config header is always included first. */
#include <aia_config.h>

#include <aiasecretmanager/aia_secret_manager.h>
#include <aiasecretmanager/private/aia_secret_manager.h>

#include <aiacore/aia_encryption_algorithm.h>
#include <aiacore/aia_events.h>
#include <aiacore/aia_exception_encountered_utils.h>
#include <aiacore/aia_json_message.h>
#include <aiacore/aia_json_utils.h>
#include <aiacore/aia_message_constants.h>
#include <aiacore/aia_secret_derivation_algorithm.h>
#include <aiacore/aia_topic.h>

#include <aiaregulator/aia_regulator.h>

#include AiaListDouble( HEADER )
#include AiaMutex( HEADER )

#include <inttypes.h>
#include <stdio.h>

/** Information about a secret. */
typedef struct AiaSecretInfo
{
    /** The actual link in the list. */
    AiaListDouble( Link_t ) link;

    /** The secret. */
    const uint8_t* secret;

    /** The sequence numbers at which @c secret begins to take effect. */
    const AiaSequenceNumber_t startingSequenceNumbers[ AIA_NUM_TOPICS ];
} AiaSecretInfo_t;

/** Private data for the @c AiaSecretManager_t type. */
struct AiaSecretManager
{
    /** Used to query for outbound sequence numbers. */
    const AiaGetNextSequenceNumber_t getNextSequenceNumber;

    /** User data associated with @c getNextSequenceNumber. */
    void* const getNextSequenceNumberUserData;

    /** Used to publish events. */
    const AiaEmitEvent_t emitEvent;

    /** User data associated with @c emitEvent. */
    void* const emitEventUserData;

    /** Mutex used to guard against asynchronous calls in threaded
     * environments. */
    AiaMutex_t mutex;

    /** @name Variables synchronized by mutex. */
    /** @{ */

    /** Collection of all secrets associated with the current session. */
    AiaListDouble_t secrets;

    /** The current secret being used for encryption/decryption. */
    AiaSecretInfo_t* currentSecret;

    /** @} */
};

/** Padding from the current next sequence number at which to apply the next
 * secret. This is chosen arbitrarily. */
static const AiaSequenceNumber_t AIA_SECRET_ROTATION_PADDING = 5;

/**
 * Generates a @c SecretRotated event for publishing to the @c Regulator.
 *
 * @param secretInfo Information about the secret. If this is @c NULL, behavior
 * is undefined.
 * @return The generated @c AiaJsonMessage_t or @c NULL on failures.
 */
static AiaJsonMessage_t* generateSecretRotatedEvent(
    const AiaSecretInfo_t* secretInfo );

/**
 * Sets the key for encryption based on topic and sequence number information.
 *
 * @param secretManager The secret manager instance to act on. If this is @c
 * NULL, behavior is undefined.
 * @param topic The topic to encrypt/decrypt.
 * @param sequenceNumber The sequence number for @c topic to encrypt/decrypt.
 * @return @c true if the key could be set successfully or @c false otherwise.
 */
static bool AiaSecretManager_SetKey( AiaSecretManager_t* secretManager,
                                     AiaTopic_t topic,
                                     AiaSequenceNumber_t sequenceNumber );

/**
 * Generates a block of memory that with sufficient space for both all the
 * members contained in AiaSecretInfo_t as well as @c secretSize.
 *
 * @param secretSize The size of the data that the @c secret member of @c
 * AiaSecretInfo_t will point to.
 * @return The newly allocated @c AiaSecretInfo_t or @c NULL on failures.  Note
 * that the @c secret member of the newly generated @c AiaSecretInfo_t will
 * point to a block of memory of @c secretSize. Note that the @c link member of
 * this will contain a AiaListDouble( LINK_INITIALIZER ). Callers are
 * responsible for freeing this block using @c AiaFree.
 */
static AiaSecretInfo_t* AiaSecretManager_GenerateSecretInfo(
    size_t secretSize );

AiaSecretManager_t* AiaSecretManager_Create(
    AiaGetNextSequenceNumber_t getNextSequenceNumber,
    void* getNextSequenceNumberUserData, AiaEmitEvent_t emitEvent,
    void* emitEventUserData )
{
    if( !getNextSequenceNumber )
    {
        AiaLogError( "Null getNextSequenceNumber" );
        return NULL;
    }
    if( !emitEvent )
    {
        AiaLogError( "Null emitEvent" );
        return NULL;
    }
    size_t encryptionAlgorithmKeySize = AiaEncryptionAlgorithm_GetKeySize(
        AiaSecretDerivationAlgorithm_ToEncryptionAlgorithm(
            SECRET_DERIVATION_ALGORITHM ) );
    size_t encryptionAlgorithmKeyBytes =
        AiaBytesToHoldBits( encryptionAlgorithmKeySize );
    uint8_t initialSharedSecret[ encryptionAlgorithmKeyBytes ];
    if( !AiaLoadSecret( initialSharedSecret, encryptionAlgorithmKeyBytes ) )
    {
        AiaLogError( "AiaLoadSecret failed" );
        return NULL;
    }

    if( !AiaCrypto_SetKey( initialSharedSecret, encryptionAlgorithmKeyBytes,
                           AiaSecretDerivationAlgorithm_ToEncryptionAlgorithm(
                               SECRET_DERIVATION_ALGORITHM ) ) )
    {
        AiaLogError( "Failed to set key." );
        return NULL;
    }

    AiaSecretManager_t* secretManager =
        (AiaSecretManager_t*)AiaCalloc( 1, sizeof( AiaSecretManager_t ) );
    if( !secretManager )
    {
        AiaLogError( "AiaCalloc failed." );
        return NULL;
    }
    *(AiaGetNextSequenceNumber_t*)&secretManager->getNextSequenceNumber =
        getNextSequenceNumber;
    *(void**)&secretManager->getNextSequenceNumberUserData =
        getNextSequenceNumberUserData;
    *(AiaEmitEvent_t*)&secretManager->emitEvent = emitEvent;
    *(void**)&secretManager->emitEventUserData = emitEventUserData;

    if( !AiaMutex( Create )( &secretManager->mutex, false ) )
    {
        AiaLogError( "AiaMutex( Create ) failed." );
        AiaFree( secretManager );
        return NULL;
    }

    AiaListDouble( Create )( &secretManager->secrets );
    AiaSecretInfo_t* secretInfo =
        AiaSecretManager_GenerateSecretInfo( encryptionAlgorithmKeyBytes );
    if( !secretInfo )
    {
        AiaLogError( "AiaSecretManager_GenerateSecretInfo failed" );
        AiaSecretManager_Destroy( secretManager );
        return NULL;
    }
    memcpy( (uint8_t*)secretInfo->secret, initialSharedSecret,
            encryptionAlgorithmKeyBytes );

    /* Assume RotateSecret will come with sequence numbers strictly greater than
     * current sequence numbers. */
    AiaListDouble( InsertTail )( &secretManager->secrets, &secretInfo->link );
    secretManager->currentSecret = secretInfo;

    return secretManager;
}

void AiaSecretManager_Destroy( AiaSecretManager_t* secretManager )
{
    if( !secretManager )
    {
        AiaLogDebug( "Null secretManager." );
        return;
    }

    AiaMutex( Lock )( &secretManager->mutex );

    AiaListDouble( RemoveAll )( &secretManager->secrets, AiaFree, 0 );

    AiaMutex( Unlock )( &secretManager->mutex );

    AiaMutex( Destroy )( &secretManager->mutex );
    AiaFree( secretManager );
}

bool AiaSecretManager_Encrypt( AiaSecretManager_t* secretManager,
                               AiaTopic_t topic,
                               AiaSequenceNumber_t sequenceNumber,
                               const uint8_t* inputData, const size_t inputLen,
                               uint8_t* outputData, uint8_t* iv, size_t ivLen,
                               uint8_t* tag, const size_t tagLen )
{
    AiaAssert( secretManager );

    if( !AiaSecretManager_SetKey( secretManager, topic, sequenceNumber ) )
    {
        AiaLogError( "AiaSecretManager_SetKey failed" );
        return false;
    }

    return AiaCrypto_Encrypt( inputData, inputLen, outputData, iv, ivLen, tag,
                              tagLen );
}

bool AiaSecretManager_Decrypt( AiaSecretManager_t* secretManager,
                               AiaTopic_t topic,
                               AiaSequenceNumber_t sequenceNumber,
                               const uint8_t* inputData, const size_t inputLen,
                               uint8_t* outputData, const uint8_t* iv,
                               const size_t ivLen, const uint8_t* tag,
                               const size_t tagLen )
{
    AiaAssert( secretManager );

    if( !AiaSecretManager_SetKey( secretManager, topic, sequenceNumber ) )
    {
        AiaLogError( "AiaSecretManager_SetKey failed" );
        return false;
    }

    return AiaCrypto_Decrypt( inputData, inputLen, outputData, iv, ivLen, tag,
                              tagLen );
}

void AiaSecretManager_OnRotateSecretDirectiveReceived(
    void* manager, const void* payload, size_t size,
    AiaSequenceNumber_t sequenceNumber, size_t index )
{
    AiaSecretManager_t* secretManager = (AiaSecretManager_t*)manager;
    AiaAssert( secretManager );

    if( !secretManager )
    {
        AiaLogError( "Null secretManager" );
        return;
    }

    AiaAssert( payload );
    if( !payload )
    {
        AiaLogError( "Null payload, sequenceNumber=%" PRIu32 ", index=%zu",
                     sequenceNumber, index );
        return;
    }

    const char* newSecret = NULL;
    size_t newSecretLen = 0;
    if( !AiaFindJsonValue( payload, size, AIA_ROTATE_SECRET_NEW_SECRET_KEY,
                           sizeof( AIA_ROTATE_SECRET_NEW_SECRET_KEY ) - 1,
                           &newSecret, &newSecretLen ) )
    {
        AiaLogError( "No " AIA_ROTATE_SECRET_NEW_SECRET_KEY " found" );
        AiaJsonMessage_t* malformedMessageEvent =
            generateMalformedMessageExceptionEncounteredEvent(
                sequenceNumber, index, AIA_TOPIC_DIRECTIVE );
        if( !secretManager->emitEvent(
                AiaJsonMessage_ToMessage( malformedMessageEvent ),
                secretManager->emitEventUserData ) )
        {
            AiaLogError( "secretManager->emitEvent failed" );
            AiaJsonMessage_Destroy( malformedMessageEvent );
        }
        return;
    }
    if( !AiaJsonUtils_UnquoteString( &newSecret, &newSecretLen ) )
    {
        AiaLogError( "Malformed JSON" );
        AiaJsonMessage_t* malformedMessageEvent =
            generateMalformedMessageExceptionEncounteredEvent(
                sequenceNumber, index, AIA_TOPIC_DIRECTIVE );
        if( !secretManager->emitEvent(
                AiaJsonMessage_ToMessage( malformedMessageEvent ),
                secretManager->emitEventUserData ) )
        {
            AiaLogError( "secretManager->emitEvent failed" );
            AiaJsonMessage_Destroy( malformedMessageEvent );
        }
        return;
    }

    AiaJsonLongType directiveSequenceNumber = 0;
    if( !AiaJsonUtils_ExtractLong(
            payload, size, AIA_ROTATE_SECRET_DIRECTIVE_SEQUENCE_NUMBER_KEY,
            sizeof( AIA_ROTATE_SECRET_DIRECTIVE_SEQUENCE_NUMBER_KEY ) - 1,
            &directiveSequenceNumber ) )
    {
        AiaLogError(
            "Failed to get " AIA_ROTATE_SECRET_DIRECTIVE_SEQUENCE_NUMBER_KEY );
        AiaJsonMessage_t* malformedMessageEvent =
            generateMalformedMessageExceptionEncounteredEvent(
                sequenceNumber, index, AIA_TOPIC_DIRECTIVE );
        if( !secretManager->emitEvent(
                AiaJsonMessage_ToMessage( malformedMessageEvent ),
                secretManager->emitEventUserData ) )
        {
            AiaLogError( "secretManager->emitEvent failed" );
            AiaJsonMessage_Destroy( malformedMessageEvent );
        }
        return;
    }

    AiaJsonLongType speakerSequenceNumber = 0;
    if( !AiaJsonUtils_ExtractLong(
            payload, size, AIA_ROTATE_SECRET_SPEAKER_SEQUENCE_NUMBER_KEY,
            sizeof( AIA_ROTATE_SECRET_SPEAKER_SEQUENCE_NUMBER_KEY ) - 1,
            &speakerSequenceNumber ) )
    {
        AiaLogError(
            "Failed to get " AIA_ROTATE_SECRET_SPEAKER_SEQUENCE_NUMBER_KEY );
        AiaJsonMessage_t* malformedMessageEvent =
            generateMalformedMessageExceptionEncounteredEvent(
                sequenceNumber, index, AIA_TOPIC_DIRECTIVE );
        if( !secretManager->emitEvent(
                AiaJsonMessage_ToMessage( malformedMessageEvent ),
                secretManager->emitEventUserData ) )
        {
            AiaLogError( "secretManager->emitEvent failed" );
            AiaJsonMessage_Destroy( malformedMessageEvent );
        }
        return;
    }

    size_t decodeSize =
        Aia_Base64GetDecodeSize( (const uint8_t*)newSecret, newSecretLen );
    if( decodeSize == 0 )
    {
        AiaLogError( "Aia_Base64GetDecodeSize failed" );
        AiaJsonMessage_t* malformedMessageEvent =
            generateMalformedMessageExceptionEncounteredEvent(
                sequenceNumber, index, AIA_TOPIC_DIRECTIVE );
        if( !secretManager->emitEvent(
                AiaJsonMessage_ToMessage( malformedMessageEvent ),
                secretManager->emitEventUserData ) )
        {
            AiaLogError( "secretManager->emitEvent failed" );
            AiaJsonMessage_Destroy( malformedMessageEvent );
        }
        return;
    }
    if( decodeSize != AiaBytesToHoldBits( AiaEncryptionAlgorithm_GetKeySize(
                          AiaSecretDerivationAlgorithm_ToEncryptionAlgorithm(
                              SECRET_DERIVATION_ALGORITHM ) ) ) )
    {
        AiaLogError( "Incorrect newSecret size (%zu) for algorithm %s (%zu).",
                     decodeSize,
                     AiaEncryptionAlgorithm_ToString(
                         AiaSecretDerivationAlgorithm_ToEncryptionAlgorithm(
                             SECRET_DERIVATION_ALGORITHM ) ),
                     AiaEncryptionAlgorithm_GetKeySize(
                         AiaSecretDerivationAlgorithm_ToEncryptionAlgorithm(
                             SECRET_DERIVATION_ALGORITHM ) ) );
        AiaJsonMessage_t* malformedMessageEvent =
            generateMalformedMessageExceptionEncounteredEvent(
                sequenceNumber, index, AIA_TOPIC_DIRECTIVE );
        if( !secretManager->emitEvent(
                AiaJsonMessage_ToMessage( malformedMessageEvent ),
                secretManager->emitEventUserData ) )
        {
            AiaLogError( "secretManager->emitEvent failed" );
            AiaJsonMessage_Destroy( malformedMessageEvent );
        }
        return;
    }

    AiaSecretInfo_t* secretInfo =
        AiaSecretManager_GenerateSecretInfo( decodeSize );
    if( !secretInfo )
    {
        AiaLogError( "AiaSecretManager_GenerateSecretInfo failed" );
        AiaJsonMessage_t* internalExceptionEvent =
            generateInternalErrorExceptionEncounteredEvent();
        if( !secretManager->emitEvent(
                AiaJsonMessage_ToMessage( internalExceptionEvent ),
                secretManager->emitEventUserData ) )
        {
            AiaLogError( "secretManager->emitEvent failed" );
            AiaJsonMessage_Destroy( internalExceptionEvent );
        }
        return;
    }

    if( !Aia_Base64Decode( (const uint8_t*)newSecret, newSecretLen,
                           (uint8_t*)secretInfo->secret, decodeSize ) )
    {
        AiaLogError( "Aia_Base64Decode failed" );
        AiaFree( secretInfo );
        AiaJsonMessage_t* malformedMessageEvent =
            generateMalformedMessageExceptionEncounteredEvent(
                sequenceNumber, index, AIA_TOPIC_DIRECTIVE );
        if( !secretManager->emitEvent(
                AiaJsonMessage_ToMessage( malformedMessageEvent ),
                secretManager->emitEventUserData ) )
        {
            AiaLogError( "secretManager->emitEvent failed" );
            AiaJsonMessage_Destroy( malformedMessageEvent );
        }
        return;
    }

    *(AiaSequenceNumber_t*)&secretInfo
         ->startingSequenceNumbers[ AIA_TOPIC_SPEAKER ] = speakerSequenceNumber;
    *(AiaSequenceNumber_t*)&secretInfo
         ->startingSequenceNumbers[ AIA_TOPIC_DIRECTIVE ] =
        directiveSequenceNumber;

    for( size_t i = 0; i < AIA_NUM_TOPICS; ++i )
    {
        if( AiaTopic_IsEncrypted( i ) && AiaTopic_IsOutbound( i ) )
        {
            AiaSequenceNumber_t nextSequenceNumber;
            if( !secretManager->getNextSequenceNumber(
                    i, &nextSequenceNumber,
                    secretManager->getNextSequenceNumberUserData ) )
            {
                AiaLogError( "getNextSequenceNumber failed" );
                AiaFree( secretInfo );
                AiaJsonMessage_t* internalExceptionEvent =
                    generateInternalErrorExceptionEncounteredEvent();
                if( !secretManager->emitEvent(
                        AiaJsonMessage_ToMessage( internalExceptionEvent ),
                        secretManager->emitEventUserData ) )
                {
                    AiaLogError( "secretManager->emitEvent failed" );
                    AiaJsonMessage_Destroy( internalExceptionEvent );
                }
                return;
            }
            *(AiaSequenceNumber_t*)&secretInfo->startingSequenceNumbers[ i ] =
                nextSequenceNumber + AIA_SECRET_ROTATION_PADDING;
        }
    }

    AiaMutex( Lock )( &secretManager->mutex );

    if( !AiaStoreSecret( (uint8_t*)secretInfo->secret, decodeSize ) )
    {
        AiaLogError( "AiaStoreSecret failed" );
        AiaFree( secretInfo );
        AiaJsonMessage_t* internalExceptionEvent =
            generateInternalErrorExceptionEncounteredEvent();
        if( !secretManager->emitEvent(
                AiaJsonMessage_ToMessage( internalExceptionEvent ),
                secretManager->emitEventUserData ) )
        {
            AiaLogError( "secretManager->emitEvent failed" );
            AiaJsonMessage_Destroy( internalExceptionEvent );
        }
        AiaMutex( Unlock )( &secretManager->mutex );
        return;
    }

    AiaJsonMessage_t* secretRotatedEvent =
        generateSecretRotatedEvent( secretInfo );
    if( !secretManager->emitEvent(
            AiaJsonMessage_ToMessage( secretRotatedEvent ),
            secretManager->emitEventUserData ) )
    {
        AiaLogError( "secretManager->emitEvent failed" );
        AiaFree( secretInfo );
        AiaJsonMessage_Destroy( secretRotatedEvent );
        AiaJsonMessage_t* internalExceptionEvent =
            generateInternalErrorExceptionEncounteredEvent();
        if( !secretManager->emitEvent(
                AiaJsonMessage_ToMessage( internalExceptionEvent ),
                secretManager->emitEventUserData ) )
        {
            AiaLogError( "secretManager->emitEvent failed" );
            AiaJsonMessage_Destroy( internalExceptionEvent );
        }

        /* TODO: ADSER-1799 There is a theoretical race here if the device
         * crashes or reboots before the next line of code can execute. The
         * "correct" way to solve would be to keep multiple secrets persisted
         * and having some way of validating the correct one upon a restart. */

        if( !AiaStoreSecret( ( secretManager->currentSecret->secret ),
                             decodeSize ) )
        {
            AiaLogError( "Failed to revert secret" );
        }
        AiaMutex( Unlock )( &secretManager->mutex );
        return;
    }

    /* Assume RotateSecret will come with sequence numbers strictly greater than
     * current sequence numbers. */
    AiaListDouble( InsertTail )( &secretManager->secrets, &secretInfo->link );

    AiaMutex( Unlock )( &secretManager->mutex );
}

static AiaJsonMessage_t* generateSecretRotatedEvent(
    const AiaSecretInfo_t* secretInfo )
{
    static const char* formatPayload =
        /* clang-format off */
        "{"
            "\""AIA_ROTATE_SECRET_EVENT_SEQUENCE_NUMBER_KEY"\":%"PRIu32
#ifdef AIA_ENABLE_MICROPHONE
            ","
            "\""AIA_ROTATE_SECRET_MICROPHONE_SEQUENCE_NUMBER_KEY"\":%"PRIu32
#endif
        "}";
    /* clang-format on */
    int numCharsRequired =
        snprintf( NULL, 0, formatPayload,
                  secretInfo->startingSequenceNumbers[ AIA_TOPIC_EVENT ],
                  secretInfo->startingSequenceNumbers[ AIA_TOPIC_MICROPHONE ] );
    if( numCharsRequired < 0 )
    {
        AiaLogError( "snprintf failed, ret=%d", numCharsRequired );
        return NULL;
    }
    char fullPayloadBuffer[ numCharsRequired + 1 ];
    if( snprintf(
            fullPayloadBuffer, numCharsRequired + 1, formatPayload,
            secretInfo->startingSequenceNumbers[ AIA_TOPIC_EVENT ],
            secretInfo->startingSequenceNumbers[ AIA_TOPIC_MICROPHONE ] ) < 0 )
    {
        AiaLogError( "snprintf failed" );
        return NULL;
    }

    AiaJsonMessage_t* jsonMessage = AiaJsonMessage_Create(
        AIA_EVENTS_SECRET_ROTATED, NULL, fullPayloadBuffer );
    return jsonMessage;
}

static bool AiaSecretManager_SetKey( AiaSecretManager_t* secretManager,
                                     AiaTopic_t topic,
                                     AiaSequenceNumber_t sequenceNumber )
{
    if( !AiaTopic_IsEncrypted( topic ) )
    {
        AiaLogError( "Topic not encrypted, topic=%s",
                     AiaTopic_ToString( topic ) );
        return false;
    }

    AiaMutex( Lock )( &secretManager->mutex );

    /* A linear scan of all secrets. This is inefficient and can be improved by
     * going backwards/forwards from the current secret instead. */
    AiaSecretInfo_t* secretToUse =
        (AiaSecretInfo_t*)AiaListDouble( PeekHead )( &secretManager->secrets );
    AiaListDouble( Link_t )* link = NULL;
    AiaListDouble( ForEach )( &secretManager->secrets, link )
    {
        AiaSecretInfo_t* nextSecret = (AiaSecretInfo_t*)link;
        /* Iterate until we find the final secret with a starting sequence
         * number greater than the current sequence number. */
        if( sequenceNumber >= nextSecret->startingSequenceNumbers[ topic ] )
        {
            secretToUse = nextSecret;
        }
        else
        {
            break;
        }
    }

    /* TODO: ADSER-1792 Discard secrets are they become stale or no longer used.
     */
    if( secretToUse != secretManager->currentSecret )
    {
        AiaLogDebug( "Changing secret encryption key" );
        if( !AiaCrypto_SetKey(
                secretToUse->secret,
                AiaBytesToHoldBits( AiaEncryptionAlgorithm_GetKeySize(
                    AiaSecretDerivationAlgorithm_ToEncryptionAlgorithm(
                        SECRET_DERIVATION_ALGORITHM ) ) ),
                AiaSecretDerivationAlgorithm_ToEncryptionAlgorithm(
                    SECRET_DERIVATION_ALGORITHM ) ) )
        {
            AiaLogError( "AiaCrypto_SetKey failed" );
            AiaMutex( Unlock )( &secretManager->mutex );
            return false;
        }
        secretManager->currentSecret = secretToUse;
    }

    AiaMutex( Unlock )( &secretManager->mutex );
    return true;
}

static AiaSecretInfo_t* AiaSecretManager_GenerateSecretInfo( size_t secretSize )
{
    AiaSecretInfo_t* secretInfo =
        AiaCalloc( 1, sizeof( AiaSecretInfo_t ) + secretSize );
    if( !secretInfo )
    {
        AiaLogError( "AiaCalloc failed, bytes=%zu",
                     sizeof( AiaSecretInfo_t ) + secretSize );
        return NULL;
    }

    secretInfo->secret = (uint8_t*)( secretInfo + 1 );
    AiaListDouble( Link_t ) defaultLink = AiaListDouble( LINK_INITIALIZER );
    secretInfo->link = defaultLink;
    return secretInfo;
}
