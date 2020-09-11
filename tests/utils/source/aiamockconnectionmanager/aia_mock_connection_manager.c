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
 * @file aia_mock_connection_manager.c
 * @brief Implements mock functions for the AiaConnectionManager_t type.
 */

/* The config header is always included first. */
#include <aia_config.h>

#include <aiaconnectionmanager/aia_connection_manager.h>

/** Private data for the @c AiaMockConnectionManager_t type. */
typedef struct AiaMockConnectionManager
{
    int placeholder;
} AiaMockConnectionManager_t;

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
    (void)onConnectionSuccess;
    (void)onConnectionSuccessUserData;
    (void)onConnectionRejected;
    (void)onConnectionRejectedUserData;
    (void)onDisconnected;
    (void)onDisconnectedUserData;
    (void)onMqttMessageReceived;
    (void)onMqttMessageReceivedUserData;
    (void)mqttConnection;
    (void)taskPool;

    AiaMockConnectionManager_t* mockConnectionManager =
        AiaCalloc( 1, sizeof( AiaMockConnectionManager_t ) );
    if( !mockConnectionManager )
    {
        return NULL;
    }

    return (AiaConnectionManager_t*)mockConnectionManager;
}

bool AiaConnectionManager_Connect( AiaConnectionManager_t* connectionManager )
{
    if( !connectionManager )
    {
        AiaLogError( "Null connectionManager." );
        return false;
    }
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
    if( !code )
    {
        AiaLogError( "Null code." );
        return false;
    }
    if( !description )
    {
        AiaLogError( "Null description." );
        return false;
    }
    return true;
}

void AiaConnectionManager_OnConnectionAcknowledgementReceived(
    AiaConnectionManager_t* connectionManager, const char* payload,
    size_t size )
{
    (void)connectionManager;
    (void)payload;
    (void)size;
}

void AiaConnectionManager_OnConnectionDisconnectReceived(
    AiaConnectionManager_t* connectionManager, const char* payload,
    size_t size )
{
    (void)connectionManager;
    (void)payload;
    (void)size;
}

void AiaConnectionManager_Destroy( AiaConnectionManager_t* connectionManager )
{
    AiaFree( connectionManager );
}
