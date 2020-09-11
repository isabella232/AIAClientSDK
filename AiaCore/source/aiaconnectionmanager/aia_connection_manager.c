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
 * @file aia_connection_manager.c
 * @brief Implements functions for the AiaConnectionManager_t type.
 */

/* The config header is always included first. */
#include <aia_config.h>

#include <aiaconnectionmanager/aia_connection_manager.h>
#include <aiacore/aia_backoff.h>
#include <aiacore/aia_json_message.h>
#include <aiacore/aia_json_utils.h>
#include <aiacore/aia_topic.h>

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>

/** Private data for the @c AiaConnectionManager_t type. */
struct AiaConnectionManager
{
    /** Callback function to run when client is connected */
    AiaConnectionManageronConnectionSuccessCallback_t onConnectionSuccess;

    /** User data to pass to @c onConnectionSuccess. */
    void* onConnectionSuccessUserData;

    /** Callback function to run when unsuccessful Connection Acknowledgement is
     * received */
    AiaConnectionManagerOnConnectionRejectionCallback_t onConnectionRejected;

    /** User data to pass to @c onConnectionRejected. */
    void* onConnectionRejectedUserData;

    /** Callback function to run when client is disconnected */
    AiaConnectionManagerOnDisconnectedCallback_t onDisconnected;

    /** User data to pass to @c onDisconnected. */
    void* onDisconnectedUserData;

    /** Pointer to the MQTT connection */
    AiaMqttConnectionPointer_t mqttConnection;

    /** Callback function when a mqtt message is received on a IoT Topic */
    AiaMqttTopicHandler_t onMqttMessageReceived;

    /** User data to pass to @c onMqttMessageReceived */
    void* onMqttMessageReceivedUserData;

    /** Taskpool used to schedule jobs for waiting for acknowledgement and
     * backoffs */
    AiaTaskPool_t taskPool;

    /** Indicates whether the client is connected. */
    AiaAtomicBool_t isConnected;

    /** An atomic number representing the number of times client has attempted
     * to connect without getting an acknowledgement. */
    uint32_t retryNum;

    /** The full topic paths to subscribe to. */
    char** topicsToSubscribe;

    /** The full topic path to send connection messages to. */
    char* connectionTopic;
};

/* Anchor the inline functions from aia_iot_config.h */
extern inline bool AiaMqttSubscribe( AiaMqttConnectionPointer_t connection,
                                     AiaMqttQos_t qos, const char* topic,
                                     AiaMqttTopicHandler_t handler,
                                     void* userData );
extern inline bool AiaMqttUnsubscribe( AiaMqttConnectionPointer_t connection,
                                       AiaMqttQos_t qos, const char* topic,
                                       AiaMqttTopicHandler_t handler,
                                       void* userData );

/** Pre-defined IoT topics */
#define AIA_TOPIC_STRING( TOPIC ) AIA_TOPIC_##TOPIC##_STRING

static const char* g_topicsToSubscribe[] = {
    AIA_TOPIC_STRING( DIRECTIVE ), AIA_TOPIC_STRING( SPEAKER ),
    AIA_TOPIC_STRING( CAPABILITIES_ACKNOWLEDGE ),
    AIA_TOPIC_STRING( CONNECTION_FROM_SERVICE )
};

/* clang-format off */
#define CONNECTION_MANAGER_CONNECT_PAYLOAD_FORMAT                                \
    "{"                                                                          \
        "\"" AIA_CONNECTION_CONNECT_AWS_ACCOUNT_ID_KEY "\":\"%s\""               \
        ",\"" AIA_CONNECTION_CONNECT_CLIENT_ID_KEY "\":\"%s\""                   \
    "}"

#define CONNECTION_MANAGER_DISCONNECT_PAYLOAD_FORMAT_START                       \
    "{"                                                                          \
        "\"" AIA_CONNECTION_DISCONNECT_CODE_KEY "\":\"%s\""
#define CONNECTION_MANAGER_DISCONNECT_PAYLOAD_FORMAT_DESCRIPTION                 \
        ",\"" AIA_CONNECTION_DISCONNECT_DESCRIPTION_KEY "\":\"%s\""
#define CONNECTION_MANAGER_DISCONNECT_PAYLOAD_FORMAT_END                         \
    "}"
/* clang-format on */

static AiaTaskPoolJobStorage_t aiaConnectionManagerAcknowledgeJobStorage =
    AiaTaskPool( JOB_STORAGE_INITIALIZER );
static AiaTaskPoolJob_t aiaConnectionManagerAcknowledgeJob =
    AiaTaskPool( JOB_INITIALIZER );

static AiaTaskPoolJobStorage_t aiaConnectionManagerBackoffJobStorage =
    AiaTaskPool( JOB_STORAGE_INITIALIZER );
static AiaTaskPoolJob_t aiaConnectionManagerBackoffJob =
    AiaTaskPool( JOB_INITIALIZER );

#define CONNECTION_ACKNOWLEDGE_WAIT_MILLISECONDS 10000
#define CONNECTION_MAX_BACKOFF_MILLISECONDS 3600000

/**
 * Converts a char array representing a Disconnect message code to its
 * corresponding enum value.
 *
 * @param code The Disconnect code for the payload.
 * @param codeLen The length of @c code.
 * @return The enum value for the Disconnect message code.
 */
static AiaConnectionOnDisconnectCode_t CharArrayToOnDisconnectedCode(
    const char* code, size_t codeLen )
{
    if( !strncmp( code, AIA_CONNECTION_DISCONNECT_GOING_OFFLINE, codeLen ) )
    {
        return AIA_CONNECTION_ON_DISCONNECTED_GOING_OFFLINE;
    }
    if( !strncmp( code, AIA_CONNECTION_DISCONNECT_UNEXPECTED_SEQUENCE_NUMBER,
                  codeLen ) )
    {
        return AIA_CONNECTION_ON_DISCONNECTED_UNEXPECTED_SEQUENCE_NUMBER;
    }
    if( !strncmp( code, AIA_CONNECTION_DISCONNECT_ENCRYPTION_ERROR, codeLen ) )
    {
        return AIA_CONNECTION_ON_DISCONNECTED_ENCRYPTION_ERROR;
    }
    if( !strncmp( code, AIA_CONNECTION_DISCONNECT_API_VERSION_DEPRECATED,
                  codeLen ) )
    {
        return AIA_CONNECTION_ON_DISCONNECTED_API_VERSION_DEPRECATED;
    }
    if( !strncmp( code, AIA_CONNECTION_DISCONNECT_MESSAGE_TAMPERED, codeLen ) )
    {
        return AIA_CONNECTION_ON_DISCONNECTED_MESSAGE_TAMPERED;
    }
    return AIA_CONNECTION_ON_DISCONNECTED_INVALID_CODE;
}

/**
 * Builds the JSON text payload for a Connect message.
 * If @c NULL is passed for @c payloadBuffer, this function will calculate
 * the length of the generated JSON payload (including the trailing @c
 * '\0').
 *
 * @param[out] payloadBuffer A user-provided buffer large enough to hold the
 * payload text.
 * @param payloadBufferSize The size (in bytes) of @c payloadBuffer.
 * @param awsAccountId The AWS Account Id for the payload.
 * @param iotClientId The IoT Client Id for the payload.
 * @return The size (in bytes) of the generated text or zero if there was an
 * error.
 */
static size_t BuildAiaConnectMessagePayload( char* payloadBuffer,
                                             size_t payloadBufferSize,
                                             const char* awsAccountId,
                                             const char* iotClientId )
{
    if( !awsAccountId )
    {
        AiaLogError( "Null awsAccountId." );
        return 0;
    }
    if( !iotClientId )
    {
        AiaLogError( "Null iotClientId." );
        return 0;
    }

    int result = 0;
    result = snprintf( payloadBuffer, payloadBufferSize,
                       CONNECTION_MANAGER_CONNECT_PAYLOAD_FORMAT, awsAccountId,
                       iotClientId );

    if( result <= 0 )
    {
        AiaLogError( "snprintf failed: %d", result );
        return 0;
    }
    if( payloadBuffer )
    {
        AiaLogInfo( "Connect payload built: %s", payloadBuffer );
    }
    return (size_t)result + 1;
}

/**
 * Builds the JSON text payload for a Disconnect message.
 * If @c NULL is passed for @c payloadBuffer, this function will calculate
 * the length of the generated JSON payload (including the trailing @c
 * '\0').
 *
 * @param[out] payloadBuffer A user-provided buffer large enough to hold the
 * payload text.
 * @param payloadBufferSize The size (in bytes) of @c payloadBuffer.
 * @param code The Disconnect code for the payload.
 * @param description The optional description in the payload. If no
 * description pass NULL instead.
 */
static size_t BuildAiaDisconnectMessagePayload( char* payloadBuffer,
                                                size_t payloadBufferSize,
                                                const char* code,
                                                const char* description )
{
    if( !code )
    {
        AiaLogError( "Null code." );
        return 0;
    }

    int result = 0;
    if( description )
    {
        result = snprintf(
            payloadBuffer, payloadBufferSize,
            CONNECTION_MANAGER_DISCONNECT_PAYLOAD_FORMAT_START
                CONNECTION_MANAGER_DISCONNECT_PAYLOAD_FORMAT_DESCRIPTION
                    CONNECTION_MANAGER_DISCONNECT_PAYLOAD_FORMAT_END,
            code, description );
    }
    else
    {
        result = snprintf( payloadBuffer, payloadBufferSize,
                           CONNECTION_MANAGER_DISCONNECT_PAYLOAD_FORMAT_START
                               CONNECTION_MANAGER_DISCONNECT_PAYLOAD_FORMAT_END,
                           code );
    }

    if( result <= 0 )
    {
        AiaLogError( "snprintf failed: %d", result );
        return 0;
    }
    if( payloadBuffer )
    {
        AiaLogInfo( "Disconnect payload built: %s", payloadBuffer );
    }
    return (size_t)result + 1;
}

/**
 * Sends a Connection message to the connection/fromclient topic
 *
 * @param jsonMessage The message to send.
 * @param mqttConnection Pointer to the active MQTT connection.
 * @param connectionTopic The connection topic to send the message to.
 * @return @c true if connection message is successfully published, @c false
 * otherwise.
 */
static bool SendConnectionMessage( AiaJsonMessage_t* jsonMessage,
                                   AiaMqttConnectionPointer_t mqttConnection,
                                   const char* connectionTopic )
{
    size_t messageBufferSize =
        AiaMessage_GetSize( AiaJsonMessage_ToConstMessage( jsonMessage ) );
    char messageBuffer[ messageBufferSize ];

    if( AiaJsonMessage_BuildMessage( jsonMessage, messageBuffer,
                                     messageBufferSize ) )
    {
        if( !AiaMqttPublish( mqttConnection, IOT_MQTT_QOS_0, connectionTopic, 0,
                             messageBuffer, sizeof( messageBuffer ) ) )
        {
            AiaLogError( "Failed to publish message. Message: %s",
                         messageBuffer );
            return false;
        }

        AiaLogDebug( "Message sent. Message: %s", messageBuffer );
        return true;
    }
    else
    {
        AiaLogError( "AiaJsonMessage_BuildMessage failed." );
        return false;
    }
}

/**
 * Callback routine of aiaConnectionManagerBackoffJob for the global @c
 * AiaTaskPool_t.
 */
static void ConnectionBackoffTimeoutRoutine( AiaTaskPool_t taskPool,
                                             AiaTaskPoolJob_t job,
                                             void* context )
{
    /* Unused parameters; silence the compiler. */
    (void)taskPool;
    (void)job;

    AiaConnectionManager_t* connectionManager =
        (AiaConnectionManager_t*)context;
    AiaAssert( connectionManager );

    AiaConnectionManager_Connect( connectionManager );
}

/**
 * Callback routine of aiaConnectionManagerAcknowledgeJob for the global @c
 * AiaTaskPool_t.
 */
static void ConnectionAckTimeoutRoutine( AiaTaskPool_t taskPool,
                                         AiaTaskPoolJob_t job, void* context )
{
    /* Unused parameters; silence the compiler. */
    (void)taskPool;
    (void)job;

    AiaConnectionManager_t* connectionManager =
        (AiaConnectionManager_t*)context;
    AiaAssert( connectionManager );
    if( !connectionManager )
    {
        AiaLogError( "Null connectionManager." );
        return;
    }

    if( AiaAtomicBool_Load( &connectionManager->isConnected ) )
    {
        AiaLogDebug( "Already connected" );
        return;
    }

    /* Calculate backoff time before sending another Connect message */
    uint32_t backoff = AiaBackoff_GetBackoffTimeMilliseconds(
        AiaAtomic_Load_u32( &connectionManager->retryNum ),
        CONNECTION_MAX_BACKOFF_MILLISECONDS );

    /* Create and schedule job to send a new Connect message after the calculate
     * backoff. */
    AiaTaskPoolError_t taskPoolError = AiaTaskPool( CreateJob )(
        ConnectionBackoffTimeoutRoutine, connectionManager,
        &aiaConnectionManagerBackoffJobStorage,
        &aiaConnectionManagerBackoffJob );
    AiaAssert( AiaTaskPoolSucceeded( taskPoolError ) );

    taskPoolError = AiaTaskPool( ScheduleDeferred )(
        connectionManager->taskPool, aiaConnectionManagerBackoffJob, backoff );

    if( AiaTaskPoolSucceeded( taskPoolError ) )
    {
        AiaLogInfo( "Connect backoff job scheduled in %" PRIu32 " ms.",
                    backoff );
    }
    else
    {
        AiaLogError( "Failed to schedule connect backoff job, error %s.",
                     AiaTaskPool( strerror )( taskPoolError ) );
    }
}

AiaConnectionManager_t* AiaConnectionManager_Create(
    AiaConnectionManageronConnectionSuccessCallback_t onConnectionSuccess,
    void* onConnectionSuccessUserData,
    AiaConnectionManagerOnConnectionRejectionCallback_t onConnectionRejected,
    void* onConnectionRejectedUserData,
    AiaConnectionManagerOnDisconnectedCallback_t onDisconnected,
    void* onDisconnectedUserData, AiaMqttTopicHandler_t onMqttMessageReceived,
    void* onMqttMessageReceivedUserData,
    AiaMqttConnectionPointer_t mqttConnection, AiaTaskPool_t taskPool )
{
    if( !onConnectionSuccess )
    {
        AiaLogError( "Null onConnectionSuccess callback." );
        return NULL;
    }
    if( !onConnectionRejected )
    {
        AiaLogError( "Null onConnectionRejected callback." );
        return NULL;
    }
    if( !onDisconnected )
    {
        AiaLogError( "Null onDisconnected callback." );
        return NULL;
    }
    if( !onMqttMessageReceived )
    {
        AiaLogError( "Null onMqttMessageReceived callback." );
        return NULL;
    }
    if( !mqttConnection )
    {
        AiaLogError( "Null mqttConnection." );
        return NULL;
    }
    if( !taskPool )
    {
        AiaLogError( "Null taskPool." );
        return NULL;
    }

    size_t connectionManagerSize = sizeof( struct AiaConnectionManager );
    AiaConnectionManager_t* connectionManager =
        (AiaConnectionManager_t*)AiaCalloc( 1, connectionManagerSize );
    if( !connectionManager )
    {
        AiaLogError( "AiaCalloc failed (%zu bytes).", connectionManagerSize );
        return NULL;
    }
    size_t numTopicsToSubscribe = AiaArrayLength( g_topicsToSubscribe );
    size_t topicsToSubscribeSize = numTopicsToSubscribe * sizeof( char* );
    connectionManager->topicsToSubscribe =
        AiaCalloc( 1, topicsToSubscribeSize );
    if( !connectionManager->topicsToSubscribe )
    {
        AiaLogError( "AiaCalloc failed (%zu bytes).", topicsToSubscribeSize );
        AiaFree( connectionManager );
        return NULL;
    }

    size_t deviceTopicRootSize = AiaGetDeviceTopicRootString( NULL, 0 );
    if( !deviceTopicRootSize )
    {
        AiaLogError( "AiaGetDeviceTopicRootString failed" );
        AiaFree( connectionManager->topicsToSubscribe );
        AiaFree( connectionManager );
        return NULL;
    }

    char deviceTopicRoot[ deviceTopicRootSize ];
    deviceTopicRootSize =
        AiaGetDeviceTopicRootString( deviceTopicRoot, deviceTopicRootSize );
    if( !deviceTopicRootSize )
    {
        AiaLogError( "AiaGetDeviceTopicRootString failed" );
        AiaFree( connectionManager->topicsToSubscribe );
        AiaFree( connectionManager );
        return NULL;
    }

    size_t topicLength = sizeof( AIA_TOPIC_CONNECTION_FROM_CLIENT_STRING ) - 1;
    size_t fullTopicPathSize =
        deviceTopicRootSize + sizeof( AIA_TOPIC_CONNECTION_FROM_CLIENT_STRING );
    connectionManager->connectionTopic = AiaCalloc( 1, fullTopicPathSize );
    if( !connectionManager->connectionTopic )
    {
        AiaLogError( "AiaCalloc failed (%zu bytes).", fullTopicPathSize );
        AiaFree( connectionManager->topicsToSubscribe );
        AiaFree( connectionManager );
        return NULL;
    }
    strncpy( connectionManager->connectionTopic, deviceTopicRoot,
             deviceTopicRootSize );
    memcpy( connectionManager->connectionTopic + deviceTopicRootSize,
            AIA_TOPIC_CONNECTION_FROM_CLIENT_STRING, topicLength );
    memcpy(
        connectionManager->connectionTopic + deviceTopicRootSize + topicLength,
        "\0", 1 );

    for( size_t i = 0; i < numTopicsToSubscribe; ++i )
    {
        size_t topicLength = strlen( g_topicsToSubscribe[ i ] );
        size_t fullTopicPathSize = deviceTopicRootSize + topicLength + 1;
        connectionManager->topicsToSubscribe[ i ] =
            AiaCalloc( 1, fullTopicPathSize );
        if( !connectionManager->topicsToSubscribe[ i ] )
        {
            for( size_t i = 0; i < numTopicsToSubscribe; ++i )
            {
                AiaFree( connectionManager->topicsToSubscribe[ i ] );
            }
            AiaFree( connectionManager->connectionTopic );
            AiaFree( connectionManager->topicsToSubscribe );
            AiaFree( connectionManager );
            AiaLogError( "AiaCalloc failed (%zu bytes).", fullTopicPathSize );
            return NULL;
        }
        strncpy( connectionManager->topicsToSubscribe[ i ], deviceTopicRoot,
                 deviceTopicRootSize );
        strncpy(
            connectionManager->topicsToSubscribe[ i ] + deviceTopicRootSize,
            g_topicsToSubscribe[ i ], topicLength );
        memcpy( connectionManager->topicsToSubscribe[ i ] +
                    deviceTopicRootSize + topicLength,
                "\0", 1 );
    }

    AiaAtomicBool_Clear( &connectionManager->isConnected );
    connectionManager->onConnectionSuccess = onConnectionSuccess;
    connectionManager->onConnectionSuccessUserData =
        onConnectionSuccessUserData;
    connectionManager->onConnectionRejected = onConnectionRejected,
    connectionManager->onConnectionRejectedUserData =
        onConnectionRejectedUserData,
    connectionManager->onDisconnected = onDisconnected;
    connectionManager->onDisconnectedUserData = onDisconnectedUserData;
    connectionManager->mqttConnection = mqttConnection;
    connectionManager->onMqttMessageReceived = onMqttMessageReceived;
    connectionManager->onMqttMessageReceivedUserData =
        onMqttMessageReceivedUserData;
    connectionManager->taskPool = taskPool;
    AiaAtomic_Store_u32( &connectionManager->retryNum, 0 );

    return connectionManager;
}

bool AiaConnectionManager_Connect( AiaConnectionManager_t* connectionManager )
{
    if( !connectionManager )
    {
        AiaLogError( "Null connectionManager." );
        return false;
    }
    if( AiaAtomicBool_Load( &connectionManager->isConnected ) )
    {
        AiaLogInfo( "Already connected" );
        return false;
    }

    /* Subscribe to the pre-defined topics before trying to connect */
    for( size_t i = 0; i < AiaArrayLength( g_topicsToSubscribe ); ++i )
    {
        if( !AiaMqttSubscribe(
                connectionManager->mqttConnection, IOT_MQTT_QOS_0,
                connectionManager->topicsToSubscribe[ i ],
                connectionManager->onMqttMessageReceived,
                connectionManager->onMqttMessageReceivedUserData ) )
        {
            AiaLogError( "Subscription request to the \"%s\" topic failed.",
                         connectionManager->topicsToSubscribe[ i ] );
            return false;
        }
        else
        {
            AiaLogDebug( "Successfully subscribed to the \"%s\" topic.",
                         connectionManager->topicsToSubscribe[ i ] );
        }
    }

    AiaAtomic_Add_u32( &connectionManager->retryNum, 1 );

    size_t iotClientIdLen;
    if( !AiaGetIotClientId( NULL, &iotClientIdLen ) )
    {
        AiaLogError(
            "AiaGetIotClientId Failed. Failed to get IoT Client Id length." );
        return false;
    }
    char iotClientId[ iotClientIdLen ];
    if( !AiaGetIotClientId( iotClientId, &iotClientIdLen ) )
    {
        AiaLogError(
            "AiaGetIotClientId Failed. Failed to retrieve IoT Client Id." );
        return false;
    }

    size_t awsAccountIdLen;
    if( !AiaGetAwsAccountId( NULL, &awsAccountIdLen ) )
    {
        AiaLogError(
            "AiaGetAwsAccountId Failed. Failed to get AWS Account Id length." );
        return false;
    }
    char awsAccountId[ awsAccountIdLen ];
    if( !AiaGetAwsAccountId( awsAccountId, &awsAccountIdLen ) )
    {
        AiaLogError(
            "AiaGetAwsAccountId Failed. Failed to retrieve AWS Account Id." );
        return false;
    }

    size_t payloadBufferSize =
        BuildAiaConnectMessagePayload( NULL, 0, awsAccountId, iotClientId );
    if( !payloadBufferSize )
    {
        AiaLogError( "BuildAiaConnectMessagePayload get buffer size failed." );
        return false;
    }

    char payloadBuffer[ payloadBufferSize ];
    if( !BuildAiaConnectMessagePayload( payloadBuffer, payloadBufferSize,
                                        awsAccountId, iotClientId ) )
    {
        AiaLogError( "BuildAiaConnectMessagePayload failed." );
        return false;
    }

    AiaJsonMessage_t* jsonMessage = AiaJsonMessage_Create(
        AIA_CONNECTION_CONNECT_NAME, NULL, payloadBuffer );

    if( !SendConnectionMessage( jsonMessage, connectionManager->mqttConnection,
                                connectionManager->connectionTopic ) )
    {
        AiaLogError( "SendConnectionMessage failed" );
        AiaJsonMessage_Destroy( jsonMessage );
        return false;
    }

    AiaJsonMessage_Destroy( jsonMessage );

    /* Create and schedule job to check if Connect request has been acknowledged
     * within CONNECTION_ACKNOWLEDGE_WAIT_MILLISECONDS. */
    AiaTaskPoolError_t taskPoolError = AiaTaskPool( CreateJob )(
        ConnectionAckTimeoutRoutine, connectionManager,
        &aiaConnectionManagerAcknowledgeJobStorage,
        &aiaConnectionManagerAcknowledgeJob );
    AiaAssert( AiaTaskPoolSucceeded( taskPoolError ) );

    taskPoolError = AiaTaskPool( ScheduleDeferred )(
        connectionManager->taskPool, aiaConnectionManagerAcknowledgeJob,
        CONNECTION_ACKNOWLEDGE_WAIT_MILLISECONDS );

    if( !AiaTaskPoolSucceeded( taskPoolError ) )
    {
        AiaLogError(
            "Failed to schedule connection acknowledgement timeout job, error "
            "%s.",
            AiaTaskPool( strerror )( taskPoolError ) );
        return false;
    }

    AiaLogDebug( "Connection acknowledgement timeout job scheduled" );
    return true;
}

bool AiaConnectionManager_Disconnect( AiaConnectionManager_t* connectionManager,
                                      const char* code,
                                      const char* description )
{
    if( !connectionManager )
    {
        AiaLogError( "Null connectionManager." );
        return false;
    }
    if( !AiaAtomicBool_Load( &connectionManager->isConnected ) )
    {
        AiaLogInfo( "Already disconnected" );
        return false;
    }

    /* Unsubscribe from the pre-defined IoT topics before invoking the callback
     */
    for( size_t i = 0; i < AiaArrayLength( g_topicsToSubscribe ); ++i )
    {
        if( !AiaMqttUnsubscribe(
                connectionManager->mqttConnection, IOT_MQTT_QOS_0,
                connectionManager->topicsToSubscribe[ i ], NULL, NULL ) )
        {
            AiaLogError( "Unsubscription request from the \"%s\" topic failed.",
                         connectionManager->topicsToSubscribe[ i ] );
            return false;
        }
        else
        {
            AiaLogInfo( "Successfully unsubscribed from the \"%s\" topic.",
                        connectionManager->topicsToSubscribe[ i ] );
        }
    }

    size_t payloadBufferSize =
        BuildAiaDisconnectMessagePayload( NULL, 0, code, description );
    if( !payloadBufferSize )
    {
        AiaLogError( "BuildAiaDisconnectMessagePayload failed." );
        return false;
    }

    char payloadBuffer[ payloadBufferSize ];
    BuildAiaDisconnectMessagePayload( payloadBuffer, payloadBufferSize, code,
                                      description );

    AiaJsonMessage_t* jsonMessage = AiaJsonMessage_Create(
        AIA_CONNECTION_DISCONNECT_NAME, NULL, payloadBuffer );

    if( !SendConnectionMessage( jsonMessage, connectionManager->mqttConnection,
                                connectionManager->connectionTopic ) )
    {
        AiaLogError( "SendConnectionMessage failed." );
        AiaJsonMessage_Destroy( jsonMessage );
        return false;
    }

    AiaJsonMessage_Destroy( jsonMessage );

    AiaConnectionOnDisconnectCode_t onDisconnectCode =
        CharArrayToOnDisconnectedCode( code, strlen( code ) );
    AiaAtomicBool_Clear( &connectionManager->isConnected );

    connectionManager->onDisconnected(
        connectionManager->onDisconnectedUserData, onDisconnectCode );
    return true;
}

void AiaConnectionManager_OnConnectionAcknowledgementReceived(
    AiaConnectionManager_t* connectionManager, const char* payload,
    size_t size )
{
    if( !connectionManager )
    {
        AiaLogError( "Null connectionManager." );
        return;
    }
    if( !payload )
    {
        AiaLogError( "Null payload." );
        return;
    }

    const char* code;
    size_t codeLen;
    if( !AiaFindJsonValue( payload, size, AIA_CONNECTION_ACK_CODE_KEY,
                           strlen( AIA_CONNECTION_ACK_CODE_KEY ), &code,
                           &codeLen ) )
    {
        AiaLogError( "No code json key found" );
        return;
    }
    else if( !AiaJsonUtils_UnquoteString( &code, &codeLen ) )
    {
        AiaLogError( "Malformed JSON" );
        return;
    }

    const char* description;
    size_t descriptionLen = 0;
    if( !AiaFindJsonValue( payload, size, AIA_CONNECTION_ACK_DESCRIPTION_KEY,
                           strlen( AIA_CONNECTION_ACK_DESCRIPTION_KEY ),
                           &description, &descriptionLen ) )
    {
        AiaLogDebug( "No optional description key found" );
    }
    else if( !AiaJsonUtils_UnquoteString( &description, &descriptionLen ) )
    {
        AiaLogError( "Malformed JSON" );
        return;
    }

    if( !strncmp( code, AIA_CONNECTION_ACK_CONNECTION_ESTABLISHED, codeLen ) )
    {
        AiaLogDebug( "Connected to Service. code: %.*s, description: %.*s",
                     codeLen, code, descriptionLen, description );
        AiaAtomicBool_Set( &connectionManager->isConnected );
        AiaAtomic_Store_u32( &connectionManager->retryNum, 0 );

        AiaTaskPoolError_t error = AiaTaskPool( TryCancel )(
            connectionManager->taskPool, aiaConnectionManagerAcknowledgeJob,
            NULL );
        if( !AiaTaskPoolSucceeded( error ) )
        {
            AiaLogWarn(
                "AiaTaskPool( TryCancel ) failed for "
                "aiaConnectionManagerAcknowledgeJob, error=%s",
                AiaTaskPool( strerror )( error ) );
        }
        if( aiaConnectionManagerBackoffJob )
        {
            error = AiaTaskPool( TryCancel )( connectionManager->taskPool,
                                              aiaConnectionManagerBackoffJob,
                                              NULL );
            if( !AiaTaskPoolSucceeded( error ) )
            {
                AiaLogWarn(
                    "AiaTaskPool( TryCancel ) failed for "
                    "aiaConnectionManagerBackoffJob, error=%s",
                    AiaTaskPool( strerror )( error ) );
            }
        }

        connectionManager->onConnectionSuccess(
            connectionManager->onConnectionSuccessUserData );
    }
    else
    {
        AiaLogError( "Connection failed. code: %.*s, description: %.*s",
                     codeLen, code, descriptionLen, description );

        AiaConnectionOnConnectionRejectionCode_t onConnectionRejectionCode;
        if( !strncmp( code, AIA_CONNECTION_ACK_UNKNOWN_FAILURE, codeLen ) )
        {
            onConnectionRejectionCode =
                AIA_CONNECTION_ON_CONNECTION_REJECTION_UNKNOWN_FAILURE;
        }
        else if( !strncmp( code, AIA_CONNECTION_ACK_API_VERSION_DEPRECATED,
                           codeLen ) )
        {
            onConnectionRejectionCode =
                AIA_CONNECTION_ON_CONNECTION_REJECTION_API_VERSION_DEPRECATED;
        }
        else if( !strncmp( code, AIA_CONNECTION_ACK_INVALID_CLIENT_ID,
                           codeLen ) )
        {
            onConnectionRejectionCode =
                AIA_CONNECTION_ON_CONNECTION_REJECTION_INVALID_CLIENT_ID;
        }
        else if( !strncmp( code, AIA_CONNECTION_ACK_INVALID_ACCOUNT_ID,
                           codeLen ) )
        {
            onConnectionRejectionCode =
                AIA_CONNECTION_ON_CONNECTION_REJECTION_INVALID_ACCOUNT_ID;
        }
        else
        {
            onConnectionRejectionCode =
                AIA_CONNECTION_ON_CONNECTION_REJECTION_INVALID_CODE;
        }
        connectionManager->onConnectionRejected(
            connectionManager->onConnectionRejectedUserData,
            onConnectionRejectionCode );
    }
}

void AiaConnectionManager_OnConnectionDisconnectReceived(
    AiaConnectionManager_t* connectionManager, const char* payload,
    size_t size )
{
    if( !connectionManager )
    {
        AiaLogError( "Null connectionManager." );
        return;
    }
    if( !payload )
    {
        AiaLogError( "Null payload." );
        return;
    }

    const char* code;
    size_t codeLen;
    if( !AiaFindJsonValue( payload, size, AIA_CONNECTION_DISCONNECT_CODE_KEY,
                           strlen( AIA_CONNECTION_DISCONNECT_CODE_KEY ), &code,
                           &codeLen ) )
    {
        AiaLogError( "No code json key found" );
        return;
    }
    else if( !AiaJsonUtils_UnquoteString( &code, &codeLen ) )
    {
        AiaLogError( "Malformed JSON" );
        return;
    }

    const char* description;
    size_t descriptionLen = 0;
    if( !AiaFindJsonValue( payload, size,
                           AIA_CONNECTION_DISCONNECT_DESCRIPTION_KEY,
                           strlen( AIA_CONNECTION_DISCONNECT_DESCRIPTION_KEY ),
                           &description, &descriptionLen ) )
    {
        AiaLogInfo( "No optional description key found" );
    }
    else if( !AiaJsonUtils_UnquoteString( &description, &descriptionLen ) )
    {
        AiaLogError( "Malformed JSON" );
        return;
    }

    AiaLogInfo( "Disconnect message received. code: %.*s, description: %.*s",
                codeLen, code, descriptionLen, description );

    AiaConnectionOnDisconnectCode_t onDisconnectCode =
        CharArrayToOnDisconnectedCode( code, codeLen );
    AiaAtomicBool_Clear( &connectionManager->isConnected );
    connectionManager->onDisconnected(
        connectionManager->onDisconnectedUserData, onDisconnectCode );
}

void AiaConnectionManager_Destroy( AiaConnectionManager_t* connectionManager )
{
    if( !connectionManager )
    {
        AiaLogDebug( "Null connectionManager." );
        return;
    }

    AiaTaskPoolError_t error = AiaTaskPool( TryCancel )(
        connectionManager->taskPool, aiaConnectionManagerAcknowledgeJob, NULL );
    if( !AiaTaskPoolSucceeded( error ) )
    {
        AiaLogWarn(
            "AiaTaskPool( TryCancel ) failed for "
            "aiaConnectionManagerAcknowledgeJob, error=%d",
            error );
    }

    if( aiaConnectionManagerBackoffJob )
    {
        error = AiaTaskPool( TryCancel )(
            connectionManager->taskPool, aiaConnectionManagerBackoffJob, NULL );
        if( !AiaTaskPoolSucceeded( error ) )
        {
            AiaLogWarn(
                "AiaTaskPool( TryCancel ) failed for "
                "aiaConnectionManagerBackoffJob, error=%s",
                AiaTaskPool( strerror )( error ) );
        }
    }

    for( size_t i = 0; i < AiaArrayLength( g_topicsToSubscribe ); ++i )
    {
        AiaFree( connectionManager->topicsToSubscribe[ i ] );
    }
    AiaFree( connectionManager->topicsToSubscribe );
    AiaFree( connectionManager->connectionTopic );
    AiaFree( connectionManager );
}
