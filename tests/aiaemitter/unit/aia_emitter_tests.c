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
 * @file aia_emitter_tests.c
 * @brief Tests for AiaEmitter_t.
 */

/* The config header is always included first. */
#include <aia_config.h>

/* Aia headers */
#include <aiacore/aia_binary_message.h>
#include <aiacore/aia_json_message.h>
#include <aiacore/aia_json_utils.h>
#include <aiacore/aia_message_constants.h>
#include <aiaemitter/aia_emitter.h>

/* Test framework includes. */
#include <unity_fixture.h>

#include <inttypes.h>

/*-----------------------------------------------------------*/

/**
 * @brief Test group for AiaEmitter_t tests.
 */
TEST_GROUP( AiaEmitterTests );

/*-----------------------------------------------------------*/

#define TEST_DEVICE_TOPIC_ROOT "test/topic/root"

/** Test pattern to fill IV with in mock encryption. */
static const int TEST_IV_FILL = 0x11;

/** Test pattern to fill MAC with in mock encryption. */
static const int TEST_MAC_FILL = 0x22;

/** Information about a message that was emitted. */
typedef struct AiaEmitterTestQueuedMessage
{
    /** The actual link in the list. */
    AiaListDouble( Link_t ) link;

    /** The topic on which the message was published. */
    AiaTopic_t topic;

    /** The MQTT array index for this message. */
    size_t index;

    /** The message data. */
    void* message;

    /** The length of @c message. */
    size_t messageLength;
} AiaEmitterTestQueuedMessage_t;

/** Test data for Emitter tests. */
typedef struct AiaEmitterTestData
{
    /** Flag indicating an internal test failure occurred. */
    AiaAtomicBool_t internalTestFailure;

    /** Container for queuing published messages. */
    AiaListDouble_t publishedMessages;

    /** Next sequence number to emit. */
    AiaSequenceNumber_t nextSequenceNumber;

    /** An MQTT connection object to use for publishing. */
    AiaMqttConnectionPointer_t mqttConnection;

    /** A secret manager to use for encrypting. */
    AiaSecretManager_t* secretManager;

    /** An emitter for array JSON messages. */
    AiaEmitter_t* arrayJsonEmitter;

    /** An emitter for non-array JSON messages. */
    AiaEmitter_t* jsonEmitter;

    /** An array for binary messages. */
    AiaEmitter_t* binaryEmitter;
} AiaEmitterTestData_t;

/**
 * Global test data instance.
 * This must be global so that it can be initialized by @c TEST_SETUP(), cleaned
 * up by @c TEST_TEAR_DOWN(), and accessed by individual @c TEST() cases, but
 * probably should not be referenced directly from any other functions.
 */
AiaEmitterTestData_t g_aiaEmitterTestData;

/**
 * Validates correctness of an individual JSON message which was published and
 * queues it for additional validation by individual test cases.
 *
 * @param data The test harness data.
 * @param topic The topic the message was published on.
 * @param payload The payload of the message.
 * @param payloadSize the size of the message.
 * @return @c true if the message was queued successfully, else @c false.
 */
static bool AiaEmitterTest_ValidateIndividualJsonMessage(
    AiaEmitterTestData_t* data, AiaTopic_t topic, const char* payload,
    size_t payloadSize )
{
    /* Queue up the message for validation by the testcase. */
    AiaEmitterTestQueuedMessage_t* node =
        AiaCalloc( 1, sizeof( AiaEmitterTestQueuedMessage_t ) + payloadSize );
    if( !node )
    {
        AiaLogError( "AiaCalloc failed." );
        AiaAtomicBool_Set( &g_aiaEmitterTestData.internalTestFailure );
        return false;
    }
    AiaListDouble( Link_t ) defaultLink = AiaListDouble( LINK_INITIALIZER );
    node->link = defaultLink;
    node->topic = topic;
    node->message = node + 1;
    node->messageLength = payloadSize;
    memcpy( node->message, payload, payloadSize );
    AiaListDouble( InsertTail )( &data->publishedMessages, &node->link );
    return true;
}

/**
 * Validates correctness of an arbitrary JSON message (which may be an array)
 * which was published and queues it for additional validation by individual
 * test cases.
 *
 * @param data The test harness data.
 * @param topic The topic the message was published on.
 * @param payload The payload of the message.
 * @param payloadSize the size of the message.
 * @return @c true if the message was queued successfully, else @c false.
 */
static bool AiaEmitterTest_ValidateJsonMqttMessage( AiaEmitterTestData_t* data,
                                                    AiaTopic_t topic,
                                                    const char* payload,
                                                    size_t payloadSize )
{
    /* Topics without an array name should have a single json message object as
     * the MQTT payload. */
    const char* arrayName = AiaTopic_GetJsonArrayName( topic );
    if( !arrayName )
    {
        return AiaEmitterTest_ValidateIndividualJsonMessage(
            data, topic, payload, payloadSize );
    }

    /* Topics with an array name should have an item with arrayName as the key
     * and an array of json message objects as the value in the MQTT payload. */
    size_t arrayNameLength = AiaTopic_GetJsonArrayNameLength( topic );

    /* Extract the array. */
    const char* array;
    size_t arrayLength;
    if( !AiaFindJsonValue( payload, payloadSize, arrayName, arrayNameLength,
                           &array, &arrayLength ) )
    {
        AiaLogError( "Could not find \"%.*s\" array in message.",
                     arrayNameLength, arrayName );
        AiaAtomicBool_Set( &g_aiaEmitterTestData.internalTestFailure );
        return false;
    }

    /* Extract the individual array elements and validate them. */
    /* TODO: rework this so that we don't have to rescan the whole array
     * repeatedly (ADSER-1654) */
    /* TODO: rework this so that we can distinguish between end-of-array and
     * other errors (ADSER-1654) */
    const char* arrayElement;
    size_t arrayElementLength;
    size_t index = 0;
    while( AiaJsonUtils_GetArrayElement( array, arrayLength, index,
                                         &arrayElement, &arrayElementLength ) )
    {
        if( !AiaEmitterTest_ValidateIndividualJsonMessage(
                data, topic, arrayElement, arrayElementLength ) )
        {
            AiaLogError( "Validation failed for json array element %zu.",
                         index );
            AiaAtomicBool_Set( &g_aiaEmitterTestData.internalTestFailure );
            return false;
        }
        ++index;
    }

    return true;
}

/**
 * Validates correctness of an individual binary message array element
 * queues it for additional validation by individual test cases.
 *
 * @param data The test harness data.
 * @param topic The topic the message was published on.
 * @param payload The payload of the message.
 * @param payloadSize the size of the message.
 * @return @c true if the message was queued successfully, else @c false.
 */
static bool AiaEmitterTest_ValidateIndividualBinaryMessage(
    AiaEmitterTestData_t* data, AiaTopic_t topic, const uint8_t* payload,
    size_t payloadSize )
{
    /* Queue up the message for validation by the testcase. */
    AiaEmitterTestQueuedMessage_t* node =
        AiaCalloc( 1, sizeof( AiaEmitterTestQueuedMessage_t ) + payloadSize );
    if( !node )
    {
        AiaLogError( "AiaCalloc failed." );
        AiaAtomicBool_Set( &g_aiaEmitterTestData.internalTestFailure );
        return false;
    }
    AiaListDouble( Link_t ) defaultLink = AiaListDouble( LINK_INITIALIZER );
    node->link = defaultLink;
    node->topic = topic;
    node->message = node + 1;
    node->messageLength = payloadSize;
    memcpy( node->message, payload, payloadSize );
    AiaListDouble( InsertTail )( &data->publishedMessages, &node->link );
    return true;
}

/**
 * Validates correctness of an binary message which was published and queues it
 * for additional validation by individual test cases.
 *
 * @param data The test harness data.
 * @param topic The topic the message was published on.
 * @param payload The payload of the message.
 * @param payloadSize the size of the message.
 * @return @c true if the message was queued successfully, else @c false.
 */
static bool AiaEmitterTest_ValidateBinaryMqttMessage(
    AiaEmitterTestData_t* data, AiaTopic_t topic, const uint8_t* payload,
    size_t payloadSize )
{
    /* Work through all the messages in the payload. */
    size_t index = 0;
    while( payloadSize )
    {
        if( payloadSize < AIA_SIZE_OF_BINARY_STREAM_HEADER )
        {
            AiaLogError(
                "Payload too small for binary message header at array element "
                "%zu.",
                index );
            AiaAtomicBool_Set( &g_aiaEmitterTestData.internalTestFailure );
            return false;
        }
        AiaBinaryMessageLength_t messageLength = 0;
        for( size_t i = 0; i < sizeof( messageLength ); ++i )
        {
            messageLength |= payload[ i ] << ( i * 8 );
        }
        size_t totalSize = AIA_SIZE_OF_BINARY_STREAM_HEADER + messageLength;
        if( payloadSize < totalSize )
        {
            AiaLogError(
                "Payload too small for binary message body at array element "
                "%zu.",
                index );
            AiaAtomicBool_Set( &g_aiaEmitterTestData.internalTestFailure );
            return false;
        }
        if( !AiaEmitterTest_ValidateIndividualBinaryMessage(
                data, topic, payload, totalSize ) )
        {
            AiaLogError( "Validation failed for binary array element %zu.",
                         index );
            AiaAtomicBool_Set( &g_aiaEmitterTestData.internalTestFailure );
            return false;
        }
        payload += totalSize;
        payloadSize -= totalSize;
        ++index;
    }

    return true;
}

size_t AiaGetDeviceTopicRootString( char* deviceTopicRootBuffer,
                                    size_t deviceTopicRootBufferSize )
{
    (void)deviceTopicRootBufferSize;
    if( !deviceTopicRootBuffer )
    {
        return sizeof( TEST_DEVICE_TOPIC_ROOT ) - 1;
    }
    if( deviceTopicRootBufferSize < sizeof( TEST_DEVICE_TOPIC_ROOT ) - 1 )
    {
        return 0;
    }
    memcpy( deviceTopicRootBuffer, TEST_DEVICE_TOPIC_ROOT,
            sizeof( TEST_DEVICE_TOPIC_ROOT ) - 1 );
    return sizeof( TEST_DEVICE_TOPIC_ROOT ) - 1;
}

/* Mock publisher. */
bool AiaMqttPublish( AiaMqttConnectionPointer_t connection, AiaMqttQos_t qos,
                     const char* topic, size_t topicLength, const void* message,
                     size_t messageLength )
{
    /* Basic parameter validation. */
    AiaEmitterTestData_t* data = (AiaEmitterTestData_t*)connection;
    if( !data )
    {
        AiaLogError( "Null connection." );
        return false;
    }
    if( AIA_MQTT_QOS0 != qos )
    {
        AiaLogError( "qos (%d) is not QOS0.", qos );
        AiaAtomicBool_Set( &data->internalTestFailure );
        return false;
    }
    if( !topicLength )
    {
        AiaLogError( "topicLength is zero." );
        AiaAtomicBool_Set( &data->internalTestFailure );
        return false;
    }
    AiaTopic_t topicValue;
    size_t rootLength = strlen( TEST_DEVICE_TOPIC_ROOT );
    if( topicLength < rootLength )
    {
        AiaLogError( "Topic too short (%zu < %zu).", topicLength, rootLength );
        AiaAtomicBool_Set( &data->internalTestFailure );
        return false;
    }
    if( strncmp( TEST_DEVICE_TOPIC_ROOT, topic, rootLength ) != 0 )
    {
        AiaLogError( "Incorrect topic root (\"%.*s\" != \"%.*s\").", rootLength,
                     TEST_DEVICE_TOPIC_ROOT, rootLength, topic );
        AiaAtomicBool_Set( &data->internalTestFailure );
        return false;
    }
    if( !AiaTopic_FromString( topic + rootLength, topicLength - rootLength,
                              &topicValue ) )
    {
        AiaLogError( "Invalid topic \"%.*s\".", topicLength - rootLength,
                     topic + rootLength );
        AiaAtomicBool_Set( &data->internalTestFailure );
        return false;
    }
    if( !message )
    {
        AiaLogError( "Null message." );
        AiaAtomicBool_Set( &data->internalTestFailure );
        return false;
    }
    if( messageLength < AIA_SIZE_OF_COMMON_HEADER )
    {
        AiaLogError( "messageLength (%zu) is too short ( < %zu ).",
                     messageLength, AIA_SIZE_OF_COMMON_HEADER );
        AiaAtomicBool_Set( &data->internalTestFailure );
        return false;
    }

    /* Copy message so we can "decrypt" in-place below. */
    uint8_t messageCopy[ messageLength ];
    memcpy( messageCopy, message, messageLength );

    /* Validate unencrypted sequence number. */
    AiaSequenceNumber_t sequenceNumber = 0;
    uint8_t* byte = messageCopy;
    for( size_t i = 0; i < sizeof( sequenceNumber ); ++i )
    {
        sequenceNumber |= *byte++ << ( i * 8 );
    }
    if( sequenceNumber != data->nextSequenceNumber )
    {
        AiaLogError( "Unexpected Sequence number (%" PRIu32 " != %" PRIu32 ").",
                     sequenceNumber, data->nextSequenceNumber );
        AiaAtomicBool_Set( &data->internalTestFailure );
        return false;
    }
    ++data->nextSequenceNumber;

    /* Validate IV and MAC. */
    for( size_t i = 0; i < AIA_COMMON_HEADER_IV_SIZE; ++i )
    {
        if( *byte++ != TEST_IV_FILL )
        {
            AiaLogError( "Nonzero IV value at byte %zu", i );
            AiaAtomicBool_Set( &data->internalTestFailure );
            return false;
        }
    }
    for( size_t i = 0; i < AIA_COMMON_HEADER_MAC_SIZE; ++i )
    {
        if( *byte++ != TEST_MAC_FILL )
        {
            AiaLogError( "Nonzero MAC value at byte %zu", i );
            AiaAtomicBool_Set( &data->internalTestFailure );
            return false;
        }
    }

    /* "Decrypt".  In the mock AiaSecretManager_Encrypt() below, encryption
     * consists of a simple inversion of all the bytes. */
    size_t encryptedSize = messageLength - ( byte - messageCopy );
    for( size_t i = 0; i < encryptedSize; ++i )
    {
        byte[ i ] = ~byte[ i ];
    }

    /* Validate encrypted sequence number. */
    AiaSequenceNumber_t encryptedSequenceNumber = 0;
    for( size_t i = 0; i < sizeof( sequenceNumber ); ++i )
    {
        encryptedSequenceNumber |= *byte++ << ( i * 8 );
    }
    if( encryptedSequenceNumber != sequenceNumber )
    {
        AiaLogError( "Sequence numbers do not match (%" PRIu32 " != %" PRIu32
                     ").",
                     encryptedSequenceNumber, sequenceNumber );
        AiaAtomicBool_Set( &data->internalTestFailure );
        return false;
    }

    /* Sanity check message payload size. */
    size_t payloadSize = messageLength - ( byte - messageCopy );
    if( payloadSize != messageLength - AIA_SIZE_OF_COMMON_HEADER )
    {
        AiaLogError( "payloadSize (%zu) incorrect (!= %zu).", payloadSize,
                     messageLength - AIA_SIZE_OF_COMMON_HEADER );
        AiaAtomicBool_Set( &data->internalTestFailure );
        return false;
    }

    AiaTopicType_t topicType = AiaTopic_GetType( topicValue );
    switch( topicType )
    {
        case AIA_TOPIC_TYPE_JSON:
            return AiaEmitterTest_ValidateJsonMqttMessage(
                data, topicValue, (const char*)byte, payloadSize );
        case AIA_TOPIC_TYPE_BINARY:
            return AiaEmitterTest_ValidateBinaryMqttMessage(
                data, topicValue, byte, payloadSize );
    }

    AiaLogError( "Unknown topic type %d", topicType );
    AiaAtomicBool_Set( &data->internalTestFailure );
    return false;
}

/* Mock secret manager. */
bool AiaSecretManager_Encrypt( AiaSecretManager_t* secretManager,
                               AiaTopic_t topic,
                               AiaSequenceNumber_t sequenceNumber,
                               const uint8_t* inputData, const size_t inputLen,
                               uint8_t* outputData, uint8_t* iv, size_t ivLen,
                               uint8_t* tag, const size_t tagLen )
{
    /* Basic parameter validation. */
    AiaEmitterTestData_t* data = (AiaEmitterTestData_t*)secretManager;
    if( !data )
    {
        AiaLogError( "Null connection." );
        return false;
    }
    (void)topic;
    (void)sequenceNumber;
    if( !inputData )
    {
        AiaLogError( "Null inputData." );
        AiaAtomicBool_Set( &data->internalTestFailure );
        return false;
    }
    if( !inputLen )
    {
        AiaLogError( "inputLen is zero." );
        AiaAtomicBool_Set( &data->internalTestFailure );
        return false;
    }
    if( !outputData )
    {
        AiaLogError( "Null outputData." );
        AiaAtomicBool_Set( &data->internalTestFailure );
        return false;
    }
    if( !iv )
    {
        AiaLogError( "Null iv." );
        AiaAtomicBool_Set( &data->internalTestFailure );
        return false;
    }
    if( AIA_COMMON_HEADER_IV_SIZE != ivLen )
    {
        AiaLogError( "Incorrect ivLen %zu (expected %zu).", ivLen,
                     AIA_COMMON_HEADER_IV_SIZE );
        AiaAtomicBool_Set( &data->internalTestFailure );
        return false;
    }
    if( !tag )
    {
        AiaLogError( "Null tag." );
        AiaAtomicBool_Set( &data->internalTestFailure );
        return false;
    }
    if( AIA_COMMON_HEADER_MAC_SIZE != tagLen )
    {
        AiaLogError( "Incorrect tagLen %zu (expected %zu).", tagLen,
                     AIA_COMMON_HEADER_MAC_SIZE );
        AiaAtomicBool_Set( &data->internalTestFailure );
        return false;
    }

    /* Simple inverted copy for now; real encryption is validated in crypto unit
     * tests. */
    memmove( outputData, inputData, inputLen );
    for( size_t i = 0; i < inputLen; ++i )
    {
        outputData[ i ] = ~outputData[ i ];
    }

    /* Set a simple pattern for IV and MAC. */
    memset( iv, TEST_IV_FILL, ivLen );
    memset( tag, TEST_MAC_FILL, tagLen );

    return true;
}

/**
 * Generalized test function used by the test cases below which verifies that
 * messages are emitted as expected.
 *
 * @param data The test harness data.
 * @param numMessages How many messages to test.
 * @param array Whether to emit them on an array topic.
 * @param binary Whether to emit them on a binary topic.
 */
static void AiaEmitterTest_TestEmitMessages( AiaEmitterTestData_t* data,
                                             size_t numMessages, bool array,
                                             bool binary )
{
    /* Build messages to emit. */
    AiaMessage_t* messages[ numMessages ];
    size_t messageSizes[ numMessages ];
    char* messageBytes[ numMessages ];
    size_t cumulativeSize = 0;
    for( size_t i = 0; i < numMessages; ++i )
    {
        AiaBinaryMessage_t* binaryMessage = NULL;
        AiaJsonMessage_t* jsonMessage = NULL;
        if( binary )
        {
            size_t binaryData1Size = 1;
            void* binaryData = AiaCalloc( 1, binaryData1Size );
            if( !binaryData )
            {
                AiaLogError( "AiaCalloc failed." );
                return;
            }
            binaryMessage = AiaBinaryMessage_Create(
                1, AIA_BINARY_STREAM_SPEAKER_CONTENT_TYPE, 0, binaryData );
            messages[ i ] = AiaBinaryMessage_ToMessage( binaryMessage );
        }
        else
        {
            jsonMessage = AiaJsonMessage_Create( "", "", "" );
            messages[ i ] = AiaJsonMessage_ToMessage( jsonMessage );
        }
        messageSizes[ i ] = AiaMessage_GetSize( messages[ i ] );
        messageBytes[ i ] = AiaCalloc( 1, messageSizes[ i ] );
        if( !messageBytes[ i ] )
        {
            AiaLogError( "AiaCalloc failed." );
            return;
        }
        cumulativeSize += messageSizes[ i ];
        if( binary )
        {
            TEST_ASSERT_TRUE( AiaBinaryMessage_BuildMessage(
                binaryMessage, (uint8_t*)messageBytes[ i ],
                messageSizes[ i ] ) );
        }
        else
        {
            TEST_ASSERT_TRUE( AiaJsonMessage_BuildMessage(
                jsonMessage, messageBytes[ i ], messageSizes[ i ] ) );
        }
    }

    /* Pick the requested emitter type. */
    AiaEmitter_t* emitter;
    if( binary )
    {
        emitter = data->binaryEmitter;
    }
    else if( array )
    {
        emitter = data->arrayJsonEmitter;
    }
    else
    {
        emitter = data->jsonEmitter;
    }

    /* Emit the messages. */
    for( size_t i = 0; i < numMessages; ++i )
    {
        cumulativeSize -= messageSizes[ i ];
        bool emitted = AiaEmitter_EmitMessageChunk(
            emitter, messages[ i ], cumulativeSize, numMessages - i - 1 );
        if( binary || array || numMessages == 1 )
        {
            TEST_ASSERT_TRUE( emitted );
        }
        else
        {
            /* First emit should fail if we're trying to write multiple messages
             * to a non-array JSON topic. */
            TEST_ASSERT_FALSE( emitted );
            for( size_t j = 0; j < numMessages; ++j )
            {
                AiaFree( messageBytes[ j ] );
                if( binary )
                {
                    AiaBinaryMessage_Destroy(
                        AiaBinaryMessage_FromMessage( messages[ j ] ) );
                }
                else
                {
                    AiaJsonMessage_Destroy(
                        AiaJsonMessage_FromMessage( messages[ j ] ) );
                }
            }
            return;
        }
    }

    /* Should come out as a single MQTT publish with all the AIS messages. */
    TEST_ASSERT_EQUAL( numMessages,
                       AiaListDouble( Count )( &data->publishedMessages ) );

    AiaSequenceNumber_t nextSequenceNumber = 0;
    TEST_ASSERT_TRUE(
        AiaEmitter_GetNextSequenceNumber( emitter, &nextSequenceNumber ) );
    TEST_ASSERT_EQUAL( data->nextSequenceNumber, nextSequenceNumber );

    /* Values should match the original messages. */
    /* TODO: validate sequence numbers and indices.  (ADSER-1655) */
    for( size_t i = 0; i < numMessages; ++i )
    {
        AiaListDouble( Link_t )* link =
            AiaListDouble( RemoveHead )( &data->publishedMessages );
        AiaEmitterTestQueuedMessage_t* node =
            (AiaEmitterTestQueuedMessage_t*)link;
        TEST_ASSERT_EQUAL( messageSizes[ i ], node->messageLength );
        if( binary )
        {
            TEST_ASSERT_EQUAL_MEMORY_ARRAY( messageBytes[ i ], node->message, 1,
                                            messageSizes[ i ] );
        }
        else
        {
            TEST_ASSERT_EQUAL_STRING_LEN( messageBytes[ i ], node->message,
                                          messageSizes[ i ] );
        }
        AiaFree( link );
        AiaFree( messageBytes[ i ] );
    }
}

/*-----------------------------------------------------------*/

/**
 * @brief Test setup for AiaEmitter_t tests.
 */
TEST_SETUP( AiaEmitterTests )
{
    memset( &g_aiaEmitterTestData, 0, sizeof( g_aiaEmitterTestData ) );
    AiaAtomicBool_Clear( &g_aiaEmitterTestData.internalTestFailure );

    AiaListDouble( Create )( &g_aiaEmitterTestData.publishedMessages );

    /* We're mocking out the connection, so just pass the test data as the
     * 'connection'. */
    g_aiaEmitterTestData.mqttConnection =
        (AiaMqttConnectionPointer_t)&g_aiaEmitterTestData;

    /* We're mocking out the secret manager, so just pass the test data as the
     * 'secretManager'. */
    g_aiaEmitterTestData.secretManager =
        (AiaSecretManager_t*)&g_aiaEmitterTestData;

    g_aiaEmitterTestData.arrayJsonEmitter = AiaEmitter_Create(
        g_aiaEmitterTestData.mqttConnection, g_aiaEmitterTestData.secretManager,
        AIA_TOPIC_EVENT );
    TEST_ASSERT_NOT_NULL( g_aiaEmitterTestData.arrayJsonEmitter );

    g_aiaEmitterTestData.jsonEmitter = AiaEmitter_Create(
        g_aiaEmitterTestData.mqttConnection, g_aiaEmitterTestData.secretManager,
        AIA_TOPIC_CONNECTION_FROM_CLIENT );
    TEST_ASSERT_NOT_NULL( g_aiaEmitterTestData.jsonEmitter );

    g_aiaEmitterTestData.binaryEmitter = AiaEmitter_Create(
        g_aiaEmitterTestData.mqttConnection, g_aiaEmitterTestData.secretManager,
        AIA_TOPIC_MICROPHONE );
    TEST_ASSERT_NOT_NULL( g_aiaEmitterTestData.binaryEmitter );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test tear down for AiaEmitter_t tests.
 */
TEST_TEAR_DOWN( AiaEmitterTests )
{
    AiaEmitter_Destroy( g_aiaEmitterTestData.binaryEmitter );
    AiaEmitter_Destroy( g_aiaEmitterTestData.jsonEmitter );
    AiaEmitter_Destroy( g_aiaEmitterTestData.arrayJsonEmitter );
    AiaListDouble( RemoveAll )( &g_aiaEmitterTestData.publishedMessages,
                                AiaFree, 0 );

    TEST_ASSERT_FALSE(
        AiaAtomicBool_Load( &g_aiaEmitterTestData.internalTestFailure ) );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test group runner for AiaEmitter_t tests.
 */
TEST_GROUP_RUNNER( AiaEmitterTests )
{
    RUN_TEST_CASE( AiaEmitterTests, CreateAndDestroyWhenEmpty );
    RUN_TEST_CASE( AiaEmitterTests, CreateWithoutConnection );
    RUN_TEST_CASE( AiaEmitterTests, CreateWithoutSecretManager );
    RUN_TEST_CASE( AiaEmitterTests, EmitWithoutEmitter );
    RUN_TEST_CASE( AiaEmitterTests, EmitWithoutChunk );
    RUN_TEST_CASE( AiaEmitterTests, EmitSingleChunkArrayJsonMessage );
    RUN_TEST_CASE( AiaEmitterTests, EmitSingleChunkJsonMessage );
    RUN_TEST_CASE( AiaEmitterTests, EmitMultiChunkArrayJsonMessage );
    RUN_TEST_CASE( AiaEmitterTests, EmitMultiChunkJsonMessage );
    RUN_TEST_CASE( AiaEmitterTests, EmitSingleChunkBinaryMessage );
    RUN_TEST_CASE( AiaEmitterTests, EmitMultiChunkBinaryMessage );
    RUN_TEST_CASE( AiaEmitterTests, GetNextSequenceNumberWithNullArgs );
}

/*-----------------------------------------------------------*/

TEST( AiaEmitterTests, CreateAndDestroyWhenEmpty )
{
    /* Empty test to verify that setup/teardown pass in isolation. */
}

/*-----------------------------------------------------------*/

TEST( AiaEmitterTests, CreateWithoutConnection )
{
    TEST_ASSERT_NULL( AiaEmitter_Create(
        NULL, g_aiaEmitterTestData.secretManager, AIA_TOPIC_EVENT ) );
}

/*-----------------------------------------------------------*/

TEST( AiaEmitterTests, CreateWithoutSecretManager )
{
    TEST_ASSERT_NULL( AiaEmitter_Create( g_aiaEmitterTestData.mqttConnection,
                                         NULL, AIA_TOPIC_EVENT ) );
}

/*-----------------------------------------------------------*/

TEST( AiaEmitterTests, EmitWithoutEmitter )
{
    AiaJsonMessage_t* jsonMessage = AiaJsonMessage_Create( "", "", "" );
    TEST_ASSERT_FALSE( AiaEmitter_EmitMessageChunk(
        NULL, AiaJsonMessage_ToMessage( jsonMessage ), 0, 0 ) );
    AiaJsonMessage_Destroy( jsonMessage );
}

/*-----------------------------------------------------------*/

TEST( AiaEmitterTests, EmitWithoutChunk )
{
    TEST_ASSERT_FALSE( AiaEmitter_EmitMessageChunk(
        g_aiaEmitterTestData.arrayJsonEmitter, NULL, 0, 0 ) );
}

/*-----------------------------------------------------------*/

TEST( AiaEmitterTests, EmitSingleChunkArrayJsonMessage )
{
    AiaEmitterTest_TestEmitMessages( &g_aiaEmitterTestData, 1, true, false );
}

/*-----------------------------------------------------------*/

TEST( AiaEmitterTests, EmitSingleChunkJsonMessage )
{
    AiaEmitterTest_TestEmitMessages( &g_aiaEmitterTestData, 1, false, false );
}

/*-----------------------------------------------------------*/

TEST( AiaEmitterTests, EmitMultiChunkArrayJsonMessage )
{
    AiaEmitterTest_TestEmitMessages( &g_aiaEmitterTestData, 2, true, false );
}

/*-----------------------------------------------------------*/

TEST( AiaEmitterTests, EmitMultiChunkJsonMessage )
{
    AiaEmitterTest_TestEmitMessages( &g_aiaEmitterTestData, 2, false, false );
}

/*-----------------------------------------------------------*/

TEST( AiaEmitterTests, EmitSingleChunkBinaryMessage )
{
    AiaEmitterTest_TestEmitMessages( &g_aiaEmitterTestData, 1, true, true );
}

/*-----------------------------------------------------------*/

TEST( AiaEmitterTests, EmitMultiChunkBinaryMessage )
{
    AiaEmitterTest_TestEmitMessages( &g_aiaEmitterTestData, 2, true, true );
}

/*-----------------------------------------------------------*/

TEST( AiaEmitterTests, GetNextSequenceNumberWithNullArgs )
{
    TEST_ASSERT_FALSE( AiaEmitter_GetNextSequenceNumber(
        g_aiaEmitterTestData.arrayJsonEmitter, NULL ) );

    AiaSequenceNumber_t outValue = 0;
    TEST_ASSERT_FALSE( AiaEmitter_GetNextSequenceNumber( NULL, &outValue ) );
}

/*-----------------------------------------------------------*/
