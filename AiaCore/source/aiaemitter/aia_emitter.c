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
 * @file aia_emitter.c
 * @brief Implements functions for the AiaEmitter_t type.
 */

/* The config header is always included first. */
#include <aia_config.h>

#include <aiacore/aia_binary_message.h>
#include <aiacore/aia_json_message.h>
#include <aiacore/aia_message_constants.h>
#include <aiacore/aia_topic.h>
#include <aiaemitter/aia_emitter.h>

#define JSON_ARRAY_MESSAGE_PREFIX1 "{\""
#define JSON_ARRAY_MESSAGE_PREFIX2 "\":["
#define JSON_ARRAY_MESSAGE_SEPARATOR ","
#define JSON_ARRAY_MESSAGE_SUFFIX "]}"

/** Private data for the @c AiaEmitter_t type. */
struct AiaEmitter
{
    /** Connection to use for publishing MQTT messages. */
    const AiaMqttConnectionPointer_t mqttConnection;

    /** The secret manager to use for encrypting MQTT messages. */
    AiaSecretManager_t* const secretManager;

    /** Topic to publish MQTT message to. */
    const AiaTopic_t topic;

    /** Device IoT topic root for publishing message to. */
    char* deviceTopicRoot;

    /** The size of @c deviceTopicRoot. */
    size_t deviceTopicRootSize;

    /** Pointer to the start of the buffer holding the current MQTT message
     * being assembled. */
    uint8_t* mqttPayloadStart;

    /** Pointer to a location in the @c mqttPayloadStart buffer to write to
     * next. */
    uint8_t* mqttPayloadEnd;

    /** Total size of the buffer @c mqttPayloadStart points to. */
    size_t mqttPayloadSize;

    /** Sequence number to use for next MQTT message emitted. Note that this
     * should only be updated using atomic operations. */
    AiaSequenceNumber_t nextSequenceNumber;
};

/**
 * Calculates how many more bytes are expected in the current MQTT message.
 *
 * @param emitter The emitter to use.
 * @return the number of bytes remaining to receive in the current MQTT message,
 * or 0 on error.
 */
static size_t AiaEmitter_MqttPayloadSpaceRemaining(
    const AiaEmitter_t* emitter )
{
    AiaAssert( emitter );
    if( !emitter )
    {
        AiaLogError( "Null emitter." );
        return 0;
    }
    AiaAssert( emitter->mqttPayloadStart );
    if( !emitter->mqttPayloadStart )
    {
        AiaLogError( "Null payloadStart." );
        return 0;
    }
    AiaAssert( emitter->mqttPayloadEnd );
    if( !emitter->mqttPayloadEnd )
    {
        AiaLogError( "Null payloadEnd." );
        return 0;
    }
    AiaAssert( emitter->mqttPayloadEnd >= emitter->mqttPayloadStart );
    if( emitter->mqttPayloadEnd < emitter->mqttPayloadStart )
    {
        AiaLogError( "Inconsistent payloadEnd/payloadStart." );
        return 0;
    }
    size_t consumed = emitter->mqttPayloadEnd - emitter->mqttPayloadStart;
    AiaAssert( consumed <= emitter->mqttPayloadSize );
    if( consumed > emitter->mqttPayloadSize )
    {
        AiaLogError( "Buffer overrun detected (consumed=%zu, size=%zu).",
                     consumed, emitter->mqttPayloadSize );
        return 0;
    }
    return emitter->mqttPayloadSize - consumed;
}

/**
 * Appends a sequence of bytes to the current MQTT message being assembled.
 *
 * @param emitter The emitter to use.
 * @param bytes The bytes to append.
 * @param count The number of bytes to append.
 * @return @c true if @c bytes were appended successfully, else @c false.
 */
static bool AiaEmitter_AppendBytesToMqttPayload( AiaEmitter_t* emitter,
                                                 const void* bytes,
                                                 size_t count )
{
    size_t remaining = AiaEmitter_MqttPayloadSpaceRemaining( emitter );
    if( count > remaining )
    {
        AiaLogError( "Count (%zu) is greater than remaining space (%zu).",
                     count, remaining );
        return false;
    }
    memcpy( emitter->mqttPayloadEnd, bytes, count );
    emitter->mqttPayloadEnd += count;
    return true;
}

/**
 * Appends a 32-bit unsigned integer in little-endian byte order to the current
 * MQTT message being assembled.
 *
 * @param emitter The emitter to use.
 * @param value The Value to append.
 * @return @c true if @c value was appended successfully, else @c false.
 */
static bool AiaEmitter_AppendUint32ToMqttPayload( AiaEmitter_t* emitter,
                                                  uint32_t value )
{
    size_t remaining = AiaEmitter_MqttPayloadSpaceRemaining( emitter );
    if( sizeof( value ) > remaining )
    {
        AiaLogError( "Insufficient remaining space (%zu).", remaining );
        return false;
    }
    for( size_t i = 0; i < sizeof( value ); ++i )
    {
        *emitter->mqttPayloadEnd++ = value >> ( i * 8 );
    }
    return true;
}

/**
 * Sets up the emitter to start a new JSON MQTT message.
 *
 * @param emitter The emitter to use.
 * @param chunkForMessage The first chunk that will go in this MQTT message.
 * @param remainingBytes The additional bytes expected after this chunk.
 * @param remainingChunks The additional chunks expected after this chunk.
 * @return @c true if initialization was successful, else @c false.
 */
static bool AiaEmitter_InitializeJsonMqttMessage(
    AiaEmitter_t* emitter, AiaRegulatorChunk_t* chunkForMessage,
    size_t remainingBytes, size_t remainingChunks )
{
    /* Figure out how big the total MQTT payload needs to be. */
    size_t prefixSize = 0;
    size_t suffixSize = 0;
    const char* jsonArrayName = AiaTopic_GetJsonArrayName( emitter->topic );
    size_t jsonArrayNameLength = 0;
    if( jsonArrayName )
    {
        jsonArrayNameLength = AiaTopic_GetJsonArrayNameLength( emitter->topic );
        prefixSize = sizeof( JSON_ARRAY_MESSAGE_PREFIX1 ) - 1 +
                     jsonArrayNameLength +
                     sizeof( JSON_ARRAY_MESSAGE_PREFIX2 ) - 1;
        suffixSize = sizeof( JSON_ARRAY_MESSAGE_SUFFIX ) - 1;
    }
    else if( remainingBytes )
    {
        AiaLogError( "Multi-chunk JSON messages are not permitted on topic %s.",
                     AiaTopic_ToString( emitter->topic ) );
        return false;
    }
    size_t chunkSize = AiaMessage_GetSize( chunkForMessage );
    size_t aiaMessageSize = chunkSize + remainingBytes;
    size_t commaSize = remainingChunks;
    size_t mqttPayloadSize = AIA_SIZE_OF_COMMON_HEADER + prefixSize +
                             aiaMessageSize + commaSize + suffixSize;

    /* Allocate space for the entire MQTT payload up-front and we'll accumulate
     * into it. */
    emitter->mqttPayloadStart = AiaCalloc( mqttPayloadSize, 1 );
    if( !emitter->mqttPayloadStart )
    {
        AiaLogError( "Failed to allocate memory for MQTT message (size=%zu).",
                     mqttPayloadSize );
        return false;
    }

    /* Record new mqttPayload size info. */
    emitter->mqttPayloadSize = mqttPayloadSize;

    /* Move the end cursor to skip over the common header (we'll initialize this
     * after we've collected the entire payload). */
    emitter->mqttPayloadEnd =
        emitter->mqttPayloadStart + AIA_SIZE_OF_COMMON_HEADER;

    /* If we're building a JSON array, add the enclosing object and array
     * name. */
    if( jsonArrayName )
    {
        if( !AiaEmitter_AppendBytesToMqttPayload(
                emitter, JSON_ARRAY_MESSAGE_PREFIX1,
                sizeof( JSON_ARRAY_MESSAGE_PREFIX1 ) - 1 ) )
        {
            AiaLogError( "Failed to append prefix1 to message." );
            AiaFree( emitter->mqttPayloadStart );
            return false;
        }
        if( !AiaEmitter_AppendBytesToMqttPayload( emitter, jsonArrayName,
                                                  jsonArrayNameLength ) )
        {
            AiaLogError( "Failed to append array name to message." );
            AiaFree( emitter->mqttPayloadStart );
            return false;
        }
        if( !AiaEmitter_AppendBytesToMqttPayload(
                emitter, JSON_ARRAY_MESSAGE_PREFIX2,
                sizeof( JSON_ARRAY_MESSAGE_PREFIX2 ) - 1 ) )
        {
            AiaLogError( "Failed to append prefix2 to message." );
            AiaFree( emitter->mqttPayloadStart );
            return false;
        }
    }

    return true;
}

/**
 * Sets up the emitter to start a new binary MQTT message.
 *
 * @param emitter The emitter to use.
 * @param chunkForMessage The first chunk that will go in this MQTT message.
 * @param remainingBytes The additional bytes expected after this chunk.
 * @return @c true if initialization was successful, else @c false.
 */
static bool AiaEmitter_InitializeBinaryMqttMessage(
    AiaEmitter_t* emitter, AiaRegulatorChunk_t* chunkForMessage,
    size_t remainingBytes )
{
    /* Figure out how big the total MQTT payload needs to be. */
    size_t chunkSize = AiaMessage_GetSize( chunkForMessage );
    size_t aiaMessageSize = chunkSize + remainingBytes;
    size_t mqttPayloadSize = AIA_SIZE_OF_COMMON_HEADER + aiaMessageSize;

    /* Allocate space for the entire MQTT payload up-front and we'll accumulate
     * into it. */
    emitter->mqttPayloadStart = AiaCalloc( mqttPayloadSize, 1 );
    if( !emitter->mqttPayloadStart )
    {
        AiaLogError(
            "Failed to allocate memory for MQTT message (mqttPayloadSize=%zu).",
            mqttPayloadSize );
        return false;
    }

    /* Record new mqttPayload size info. */
    emitter->mqttPayloadSize = mqttPayloadSize;

    /* Move the end cursor to skip over the common header (we'll initialize this
     * after we've collected the entire payload). */
    emitter->mqttPayloadEnd =
        emitter->mqttPayloadStart + AIA_SIZE_OF_COMMON_HEADER;

    return true;
}

/**
 * Sets up the emitter to start a new MQTT message.
 *
 * @param emitter The emitter to use.
 * @param chunkForMessage The first chunk that will go in this MQTT message.
 * @param remainingBytes The additional bytes expected after this chunk.
 * @param remainingChunks The additional chunks expected after this chunk.
 * @return @c true if initialization was successful, else @c false.
 */
static bool AiaEmitter_InitializeMqttMessage(
    AiaEmitter_t* emitter, AiaRegulatorChunk_t* chunkForMessage,
    size_t remainingBytes, size_t remainingChunks )
{
    /* Sanity checks. */
    if( emitter->mqttPayloadEnd )
    {
        AiaLogWarn( "Non-null mqttPayloadEnd when starting new message." );
    }
    if( emitter->mqttPayloadSize )
    {
        AiaLogWarn( "Non-zero mqttPayloadSize=%zu when starting new message.",
                    emitter->mqttPayloadSize );
    }

    /* Initialize type-specific header. */
    switch( AiaTopic_GetType( emitter->topic ) )
    {
        case AIA_TOPIC_TYPE_JSON:
            return AiaEmitter_InitializeJsonMqttMessage(
                emitter, chunkForMessage, remainingBytes, remainingChunks );
        case AIA_TOPIC_TYPE_BINARY:
            return AiaEmitter_InitializeBinaryMqttMessage(
                emitter, chunkForMessage, remainingBytes );
    }

    AiaLogError( "Unknown topic %s.", AiaTopic_ToString( emitter->topic ) );
    return false;
}

/**
 * Appends a JSON chunk to an ongoing MQTT message being assembled.
 *
 * @param emitter The emitter to use.
 * @param chunkForMessage The chunk to append.
 * @param remainingBytes The additional bytes expected after this chunk.
 * @return @c true if appending was successful, else @c false.
 */
static bool AiaEmitter_AppendJsonMqttMessageChunk(
    AiaEmitter_t* emitter, AiaRegulatorChunk_t* chunkForMessage,
    size_t remainingBytes )
{
    /* Build the message directly into our MQTT payload array. */
    if( !AiaJsonMessage_BuildMessage(
            AiaJsonMessage_FromMessage( chunkForMessage ),
            (char*)emitter->mqttPayloadEnd,
            AiaEmitter_MqttPayloadSpaceRemaining( emitter ) ) )
    {
        AiaLogError( "AiaJsonMessage_BuildMessage() failed." );
        return false;
    }
    AiaLogDebug( "Emitting JSON message chunk: %.*s",
                 AiaMessage_GetSize( chunkForMessage ),
                 emitter->mqttPayloadEnd );
    emitter->mqttPayloadEnd += AiaMessage_GetSize( chunkForMessage );

    /* If there are more to come, add a comma for the JSON array syntax. */
    if( remainingBytes )
    {
        if( !AiaEmitter_AppendBytesToMqttPayload(
                emitter, JSON_ARRAY_MESSAGE_SEPARATOR,
                sizeof( JSON_ARRAY_MESSAGE_SEPARATOR ) - 1 ) )
        {
            AiaLogError( "Failed to append separator to message." );
            return false;
        }
    }

    return true;
}

/**
 * Appends a binary chunk to an ongoing MQTT message being assembled.
 *
 * @param emitter The emitter to use.
 * @param chunkForMessage The chunk to append.
 * @return @c true if appending was successful, else @c false.
 */
static bool AiaEmitter_AppendBinaryMqttMessageChunk(
    AiaEmitter_t* emitter, AiaRegulatorChunk_t* chunkForMessage )
{
    AiaBinaryMessage_t* binaryStreamMessage =
        AiaBinaryMessage_FromMessage( chunkForMessage );
    AiaBinaryMessageType_t messageType =
        AiaBinaryMessage_GetType( binaryStreamMessage );

    /* TODO: Only supporting content messages for now (ADSER-1628). */
    if( messageType != AIA_BINARY_STREAM_SPEAKER_CONTENT_TYPE )
    {
        AiaLogError( "Unsupported message type %zu.", messageType );
        return false;
    }

    /* Build the message directly into our MQTT payload array. */
    if( !AiaBinaryMessage_BuildMessage(
            binaryStreamMessage, emitter->mqttPayloadEnd,
            AiaEmitter_MqttPayloadSpaceRemaining( emitter ) ) )
    {
        AiaLogError( "AiaBinaryMessage_BuildMessage() failed." );
        return false;
    }
    emitter->mqttPayloadEnd += AiaMessage_GetSize( chunkForMessage );

    return true;
}

/**
 * Appends a chunk to an ongoing MQTT message being assembled.
 *
 * @param emitter The emitter to use.
 * @param chunkForMessage The chunk to append.
 * @param remainingBytes The additional bytes expected after this chunk.
 * @return @c true if appending was successful, else @c false.
 */
static bool AiaEmitter_AppendMqttMessageChunk(
    AiaEmitter_t* emitter, AiaRegulatorChunk_t* chunkForMessage,
    size_t remainingBytes )
{
    switch( AiaTopic_GetType( emitter->topic ) )
    {
        case AIA_TOPIC_TYPE_JSON:
            if( !AiaEmitter_AppendJsonMqttMessageChunk(
                    emitter, chunkForMessage, remainingBytes ) )
            {
                AiaLogError( "Failed to append chunk for topic %s",
                             AiaTopic_ToString( emitter->topic ) );
                return false;
            }
            AiaJsonMessage_Destroy(
                AiaJsonMessage_FromMessage( chunkForMessage ) );
            return true;
        case AIA_TOPIC_TYPE_BINARY:
            if( !AiaEmitter_AppendBinaryMqttMessageChunk( emitter,
                                                          chunkForMessage ) )
            {
                AiaLogError( "Failed to append chunk for topic %s",
                             AiaTopic_ToString( emitter->topic ) );
                return false;
            }
            AiaBinaryMessage_Destroy(
                AiaBinaryMessage_FromMessage( chunkForMessage ) );
            return true;
    }

    AiaLogError( "Unknown topic %s.", AiaTopic_ToString( emitter->topic ) );
    return false;
}

/**
 * Finalizes a JSON MQTT message being assembled by @c emitter.
 *
 * @param emitter The emitter to use.
 * @return @c true if finalization was successful, else @c false.
 */
static bool AiaEmitter_TerminateJsonMqttMessage( AiaEmitter_t* emitter )
{
    /* If this was a json array, terminate the array. */
    if( AiaTopic_GetJsonArrayName( emitter->topic ) )
    {
        if( !AiaEmitter_AppendBytesToMqttPayload(
                emitter, JSON_ARRAY_MESSAGE_SUFFIX,
                sizeof( JSON_ARRAY_MESSAGE_SUFFIX ) - 1 ) )
        {
            AiaLogError( "Failed to append suffix to message." );
            return false;
        }
    }
    return true;
}

/**
 * Finalizes an MQTT message being assembled by @c emitter.
 *
 * @param emitter The emitter to use.
 * @return @c true if finalization was successful, else @c false.
 */
static bool AiaEmitter_TerminateMqttMessage( AiaEmitter_t* emitter )
{
    bool successful = false;

    /* Do any message-specific termination. */
    switch( AiaTopic_GetType( emitter->topic ) )
    {
        case AIA_TOPIC_TYPE_JSON:
            successful = AiaEmitter_TerminateJsonMqttMessage( emitter );
            break;
        case AIA_TOPIC_TYPE_BINARY:
            /* No termination needed for binary messages. */
            successful = true;
            break;
    }
    if( !successful )
    {
        AiaLogError( "Unable to terminate message for topic %s.",
                     AiaTopic_ToString( emitter->topic ) );
        return false;
    }

    /* Sanity check: did we fill up the whole message? */
    size_t remaining = AiaEmitter_MqttPayloadSpaceRemaining( emitter );
    if( remaining )
    {
        AiaLogError( "Did not fill entire message (%zu bytes remaining).",
                     remaining );
        return false;
    }

    AiaSequenceNumber_t nextSequenceNumber =
        AiaAtomic_Load_u32( &emitter->nextSequenceNumber );

    /* Write the encrypted sequence number field. */
    emitter->mqttPayloadEnd =
        emitter->mqttPayloadStart + AIA_COMMON_HEADER_ENCRYPTED_SEQUENCE_OFFSET;
    if( !AiaEmitter_AppendUint32ToMqttPayload( emitter, nextSequenceNumber ) )
    {
        AiaLogError( "Failed to append encrypted sequence number to message." );
        return false;
    }

    /* Encrypt the message. */
    size_t ivSize = AIA_COMMON_HEADER_IV_SIZE;
    size_t macSize = AIA_COMMON_HEADER_MAC_SIZE;
    uint8_t iv[ ivSize ];
    uint8_t mac[ macSize ];
    uint8_t* encryptedPayload =
        emitter->mqttPayloadStart + AIA_COMMON_HEADER_ENCRYPTED_SEQUENCE_OFFSET;
    size_t encryptedSize =
        emitter->mqttPayloadSize - AIA_COMMON_HEADER_ENCRYPTED_SEQUENCE_OFFSET;
    if( !AiaSecretManager_Encrypt( emitter->secretManager, emitter->topic,
                                   nextSequenceNumber, encryptedPayload,
                                   encryptedSize, encryptedPayload, iv,
                                   sizeof( iv ), mac, sizeof( mac ) ) )
    {
        AiaLogError( "Encryption failed." );
        return false;
    }

    /* Fill out the common header. */
    emitter->mqttPayloadEnd = emitter->mqttPayloadStart;
    if( !AiaEmitter_AppendUint32ToMqttPayload( emitter, nextSequenceNumber ) )
    {
        AiaLogError( "Failed to append sequence number to message." );
        return false;
    }
    if( !AiaEmitter_AppendBytesToMqttPayload( emitter, iv,
                                              AiaArrayLength( iv ) ) )
    {
        AiaLogError( "Failed to append IV to message." );
        return false;
    }
    if( !AiaEmitter_AppendBytesToMqttPayload( emitter, mac,
                                              AiaArrayLength( mac ) ) )
    {
        AiaLogError( "Failed to append MAC to message." );
        return false;
    }

    return true;
}

/**
 * Publishes an MQTT message that has been finalized by @c emitter.
 *
 * @param emitter The emitter to use.
 * @return @c true if publishing was successful, else @c false.
 */
static bool AiaEmitter_PublishMqttMessage( AiaEmitter_t* emitter )
{
    size_t topicLength = AiaTopic_GetLength( emitter->topic );
    char topic[ emitter->deviceTopicRootSize + topicLength ];
    strncpy( topic, emitter->deviceTopicRoot, emitter->deviceTopicRootSize );
    strncpy( topic + emitter->deviceTopicRootSize,
             AiaTopic_ToString( emitter->topic ), topicLength );
    if( !AiaMqttPublish( emitter->mqttConnection, AIA_MQTT_QOS0, topic,
                         emitter->deviceTopicRootSize + topicLength,
                         emitter->mqttPayloadStart, emitter->mqttPayloadSize ) )
    {
        AiaLogError( "Failed to publish mqtt message on topic %s.",
                     AiaTopic_ToString( emitter->topic ) );
        return false;
    }
    AiaAtomic_Add_u32( &emitter->nextSequenceNumber, 1 );

    /* If we published successfully, clean up and get ready to start a new
     * message. */
    AiaFree( emitter->mqttPayloadStart );
    emitter->mqttPayloadStart = NULL;
    emitter->mqttPayloadEnd = NULL;
    emitter->mqttPayloadSize = 0;

    return true;
}

bool AiaEmitter_EmitMessageChunk( AiaEmitter_t* emitter,
                                  AiaRegulatorChunk_t* chunkForMessage,
                                  size_t remainingBytes,
                                  size_t remainingChunks )
{
    if( !emitter )
    {
        AiaLogError( "Null emitter." );
        return false;
    }

    if( !chunkForMessage )
    {
        AiaLogError( "Null chunkForMessage." );
        return false;
    }

    /* If we're not already in the middle of a message, start constructing a new
     * one. */
    if( !emitter->mqttPayloadStart )
    {
        if( !AiaEmitter_InitializeMqttMessage(
                emitter, chunkForMessage, remainingBytes, remainingChunks ) )
        {
            AiaLogError( "Failed to initialize MQTT message for topic %s.",
                         AiaTopic_ToString( emitter->topic ) );
            return false;
        }
    }

    /* Append the new chunk to the message. */
    if( !AiaEmitter_AppendMqttMessageChunk( emitter, chunkForMessage,
                                            remainingBytes ) )
    {
        AiaLogError( "Failed to append chunk to MQTT message for topic %s.",
                     AiaTopic_ToString( emitter->topic ) );
        return false;
    }

    /* If there are no chunks remaining, terminate and publish the mesage. */
    if( !remainingBytes )
    {
        if( !AiaEmitter_TerminateMqttMessage( emitter ) )
        {
            AiaLogError( "Failed to terminate MQTT message for topic %s.",
                         AiaTopic_ToString( emitter->topic ) );
            return false;
        }
        if( !AiaEmitter_PublishMqttMessage( emitter ) )
        {
            AiaLogError( "Failed to publish MQTT message for topic %s.",
                         AiaTopic_ToString( emitter->topic ) );
            return false;
        }
    }

    return true;
}

AiaEmitter_t* AiaEmitter_Create( AiaMqttConnectionPointer_t mqttConnection,
                                 AiaSecretManager_t* secretManager,
                                 AiaTopic_t topic )
{
    if( !mqttConnection )
    {
        AiaLogError( "Null mqttConnection." );
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

    AiaEmitter_t* emitter = (AiaEmitter_t*)AiaCalloc(
        1, sizeof( AiaEmitter_t ) + deviceTopicRootSize );
    if( !emitter )
    {
        AiaLogError( "AiaCalloc failed." );
        return NULL;
    }

    emitter->deviceTopicRoot = (char*)( emitter + 1 );

    deviceTopicRootSize = AiaGetDeviceTopicRootString( emitter->deviceTopicRoot,
                                                       deviceTopicRootSize );
    if( !deviceTopicRootSize )
    {
        AiaLogError( "AiaGetDeviceTopicRootString failed" );
        AiaFree( emitter );
        return NULL;
    }
    emitter->deviceTopicRootSize = deviceTopicRootSize;

    *(AiaMqttConnectionPointer_t*)&emitter->mqttConnection = mqttConnection;
    *(AiaSecretManager_t**)&emitter->secretManager = secretManager;
    *(AiaTopic_t*)&emitter->topic = topic;

    return emitter;
}

void AiaEmitter_Destroy( AiaEmitter_t* emitter )
{
    if( !emitter )
    {
        AiaLogDebug( "Null emitter." );
        return;
    }

    if( emitter->mqttPayloadStart )
    {
        AiaFree( emitter->mqttPayloadStart );
    }
    AiaFree( emitter );
}

bool AiaEmitter_GetNextSequenceNumber( AiaEmitter_t* emitter,
                                       AiaSequenceNumber_t* nextSequenceNumber )
{
    if( !emitter )
    {
        AiaLogError( "Null emitter." );
        return false;
    }
    if( !nextSequenceNumber )
    {
        AiaLogError( "Null nextSequenceNumber." );
        return false;
    }
    *nextSequenceNumber = AiaAtomic_Load_u32( &emitter->nextSequenceNumber );
    return true;
}
