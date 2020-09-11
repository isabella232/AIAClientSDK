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
 * @file aia_connection_manager.h
 * @brief Implements functions for the AiaConnectionManager_t type.
 */

#ifndef AIA_CONNECTION_MANAGER_H_
#define AIA_CONNECTION_MANAGER_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <aia_application_config.h>
#include <aiaconnectionmanager/aia_connection_constants.h>

#include AiaTaskPool( HEADER )

typedef struct AiaConnectionManager AiaConnectionManager_t;

/**
 * Attempts to close a connection with the Aia service.
 *
 * @param userData Context associated with this disconnect.
 * @param code The disconnect code to send.
 * @param description Optional additional information the client may provide.
 * @return @c true if disconnect message is sent and disconnect callback will be
 * called, @c false otherwise (including if already disconnected).
 */
typedef bool ( *AiaDisconnectHandler_t )( void* userData, int code,
                                          const char* description );

/**
 * Allocates and initializes a new @c AiaConnectionManager_t.  An @c
 * AiaConnectionManager_t created by this function should later be released by a
 * call to @c AiaConnectionManager_Destroy().
 * @note Connect is enabled on creation.
 *
 * @param onConnectionSuccess A callback that is ran after the client is
 * connected to the service.
 * @param onConnectionSuccessUserData User data to pass to @c
 * onConnectionSuccess.
 * @param onConnectionRejected A callback that is ran when a unsuccessful
 * Connection Acknowledgement message is received.
 * @param onConnectionRejectedUserData User data to pass to @c
 * onConnectionRejected.
 * @param onDisconnected A callback that is ran after the client is disconnected
 * from the service.
 * @param onDisconnectedUserData User data to pass to @c onDisconnected.
 * @param onMqttMessageReceived A callback that is ran when a mqtt message is
 * received on a subscribed IoT topic.
 * @param onMqttMessageReceivedUserData User data to pass to @c
 * onMqttMessageReceived.
 * @param mqttConnection Pointer to the active MQTT connection.
 * @param taskPool Taskpool used to schedule jobs.
 * @return the new @c AiaConnectionManager_t if successful, else @c NULL.
 */
AiaConnectionManager_t* AiaConnectionManager_Create(
    AiaConnectionManageronConnectionSuccessCallback_t onConnectionSuccess,
    void* onConnectionSuccessUserData,
    AiaConnectionManagerOnConnectionRejectionCallback_t onConnectionRejected,
    void* onConnectionRejectedUserData,
    AiaConnectionManagerOnDisconnectedCallback_t onDisconnected,
    void* onDisconnectedUserData, AiaMqttTopicHandler_t onMqttMessageReceived,
    void* onMqttMessageReceivedUserData,
    AiaMqttConnectionPointer_t mqttConnection, AiaTaskPool_t taskPool );

/**
 * Send a Connect message to the Service.
 *
 * @param connectionManager The connection manager instance to act on.
 * @return @c true if the connection request is sent and connection callbacks
 * will be called, @c false otherwise (including if already connected).
 */
bool AiaConnectionManager_Connect( AiaConnectionManager_t* connectionManager );

/**
 * Disconnects from the Service.
 *
 * @param connectionManager The connection manager instance to act on.
 * @param code The disconnect code to send to the service.
 * @param description The disconnect description to send to the service.
 * @return @c true if disconnect message is sent and disconnect callback will be
 * called, @c false otherwise (including if already disconnected).
 */
bool AiaConnectionManager_Disconnect( AiaConnectionManager_t* connectionManager,
                                      const char* code,
                                      const char* description );

/**
 * This function may be used to notify the @c connectionManager of sequenced
 * Connection Acknowledgement messages.
 *
 * @param connectionManager The connection manager instance to act on.
 * @param payload The unencrypted payload of the Connection Acknowledgement.
 * message. This must be valid for the lifecycle of this call.
 * @param size The size of the payload.
 *
 * @note The @c payload is expected to be in this format "{"connectMessageId":
 * X, "code": Y, "description": Z}". The description field is optional.
 */
void AiaConnectionManager_OnConnectionAcknowledgementReceived(
    AiaConnectionManager_t* connectionManager, const char* payload,
    size_t size );

/**
 * This function may be used to notify the @c connectionManager of sequenced
 * Connection Disconnect messages.
 *
 * @param connectionManager The connection manager instance to act on.
 * @param payload The unencrypted payload of the Connection Disconnect.
 * message. This must be valid for the lifecycle of this call.
 * @param size The size of the payload.
 *
 * @note The @c payload is expected to be in this format "{ "code": X,
 * "description": Y }". The description field is optional.
 */
void AiaConnectionManager_OnConnectionDisconnectReceived(
    AiaConnectionManager_t* connectionManager, const char* payload,
    size_t size );

/**
 * Releases a @c AiaConnectionManager_t previously allocated by @c
 * AiaConnectionManager_Create().
 *
 * @param connectionManager The connection manager instance to act on.
 */
void AiaConnectionManager_Destroy( AiaConnectionManager_t* connectionManager );

#endif /* ifndef AIA_CONNECTION_MANAGER_H_ */
