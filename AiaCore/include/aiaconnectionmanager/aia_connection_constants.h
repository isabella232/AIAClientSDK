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
 * @file aia_connection_constants.h
 * @brief Constants related to Connection messages.
 */

#ifndef AIA_CONNECTION_CONSTANTS_H_
#define AIA_CONNECTION_CONSTANTS_H_

/* The config header is always included first. */
#include <aia_config.h>

/* Connect message constants */

/** Value for the name field in a Aia JSON Connection Connect message
 * header. */
#define AIA_CONNECTION_CONNECT_NAME "Connect"

/** Key for the awsAccountId field in a Aia JSON Connection Connect message
 * payload. */
#define AIA_CONNECTION_CONNECT_AWS_ACCOUNT_ID_KEY "awsAccountId"

/** Key for the clientId field in a Aia JSON Connection Connect message
 * payload. */
#define AIA_CONNECTION_CONNECT_CLIENT_ID_KEY "clientId"

/* Acknowledge message constants */

/** Value for the name field in a Aia JSON Connection Acknowledge message
 * header. */
#define AIA_CONNECTION_ACK_NAME "Acknowledge"

/** Key for the connect message ID in a Aia JSON Connection Acknowledge
 * message payload. */
#define AIA_CONNECTION_ACK_CONNECT_MESSAGE_ID_KEY "connectMessageId"

/** Key for the code field in a Aia JSON Connection Acknowledge message
 * payload. */
#define AIA_CONNECTION_ACK_CODE_KEY "code"

/** Key for the description in a Aia JSON Connection Acknowledge message
 * payload. */
#define AIA_CONNECTION_ACK_DESCRIPTION_KEY "description"

/** Values for the code field in a Aia JSON Connection Acknowledge message
 * payload. */
#define AIA_CONNECTION_ACK_CONNECTION_ESTABLISHED "CONNECTION_ESTABLISHED"
#define AIA_CONNECTION_ACK_INVALID_ACCOUNT_ID "INVALID_ACCOUNT_ID"
#define AIA_CONNECTION_ACK_INVALID_CLIENT_ID "INVALID_CLIENT_ID"
#define AIA_CONNECTION_ACK_API_VERSION_DEPRECATED "API_VERSION_DEPRECATED"
#define AIA_CONNECTION_ACK_UNKNOWN_FAILURE "UNKNOWN_FAILURE"

/**
 * Codes that are that can be passed to the
 * @c AiaConnectionManagerOnConnectionRejectionCallback_t
 */
typedef enum AiaConnectionOnConnectionRejectionCode
{
    AIA_CONNECTION_ON_CONNECTION_REJECTION_INVALID_ACCOUNT_ID,
    AIA_CONNECTION_ON_CONNECTION_REJECTION_INVALID_CLIENT_ID,
    AIA_CONNECTION_ON_CONNECTION_REJECTION_API_VERSION_DEPRECATED,
    AIA_CONNECTION_ON_CONNECTION_REJECTION_UNKNOWN_FAILURE,
    AIA_CONNECTION_ON_CONNECTION_REJECTION_INVALID_CODE
} AiaConnectionOnConnectionRejectionCode_t;

/* Disconnect message constants */

/** Value for the name field in a Aia JSON Connection Disconnect message
 * header. */
#define AIA_CONNECTION_DISCONNECT_NAME "Disconnect"

/** Key for the code field in a Aia JSON Connection Disconnect message
 * payload. */
#define AIA_CONNECTION_DISCONNECT_CODE_KEY "code"

/** Key for the description in a Aia JSON Connection Disconnect message
 * payload. */
#define AIA_CONNECTION_DISCONNECT_DESCRIPTION_KEY "description"

/** Values for the code field in a Aia JSON Connection Disconnect message
 * payload. */
#define AIA_CONNECTION_DISCONNECT_UNEXPECTED_SEQUENCE_NUMBER \
    "UNEXPECTED_SEQUENCE_NUMBER"
#define AIA_CONNECTION_DISCONNECT_MESSAGE_TAMPERED "MESSAGE_TAMPERED"
#define AIA_CONNECTION_DISCONNECT_API_VERSION_DEPRECATED \
    "API_VERSION_DEPRECATED"
#define AIA_CONNECTION_DISCONNECT_ENCRYPTION_ERROR "ENCRYPTION_ERROR"
#define AIA_CONNECTION_DISCONNECT_GOING_OFFLINE "GOING_OFFLINE"

/**
 * Codes that are that can be passed to the
 * @c AiaConnectionManagerOnDisconnectedCallback_t
 */
typedef enum AiaConnectionOnDisconnectCode
{
    AIA_CONNECTION_ON_DISCONNECTED_UNEXPECTED_SEQUENCE_NUMBER,
    AIA_CONNECTION_ON_DISCONNECTED_MESSAGE_TAMPERED,
    AIA_CONNECTION_ON_DISCONNECTED_API_VERSION_DEPRECATED,
    AIA_CONNECTION_ON_DISCONNECTED_ENCRYPTION_ERROR,
    AIA_CONNECTION_ON_DISCONNECTED_GOING_OFFLINE,
    AIA_CONNECTION_ON_DISCONNECTED_INVALID_CODE
} AiaConnectionOnDisconnectCode_t;

#endif /* ifndef AIA_CONNECTION_CONSTANTS_H_ */
