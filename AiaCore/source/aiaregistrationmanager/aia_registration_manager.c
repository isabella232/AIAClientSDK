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
 * @file aia_registration_manager.c
 * @brief Implements registration functions for AIA.
 */

/* The config header is always included first. */
#include <aia_config.h>

#include <aiacore/aia_crypto_mbedtls.h>
#include <aiacore/aia_encryption_algorithm.h>
#include <aiacore/aia_json_utils.h>
#include <aiacore/aia_topic.h>
#include <aiacore/aia_utils.h>
#include <aiaregistrationmanager/aia_registration_constants.h>
#include <aiaregistrationmanager/aia_registration_manager.h>

#include <stdio.h>

/**
 * The generated key lengths are always 32.
 * @see
 * https://developer.amazon.com/en-US/docs/alexa/alexa-voice-service/avs-for-aws-iot-registration.html#request
 */
#define AIA_REGISTRATION_MANAGER_GENERATED_KEY_LENGTH 32

#define REGISTRATION_REQUEST_CONTENT "Content-Type: application/json"

/* clang-format off */
#define REGISTRATION_REQUEST_FORMAT                                           \
    "{"                                                                       \
        "\"" AIA_REGISTRATION_AUTHENTICATION_KEY "\": {"                      \
            "\"" AIA_REGISTRATION_AUTHENTICATION_TOKEN_KEY "\": \"%s\","      \
            "\"" AIA_REGISTRATION_AUTHENTICATION_CLIENT_ID_KEY "\": \"%s\""   \
        "}"                                                                   \
        ",\"" AIA_REGISTRATION_ENCRYPTION_KEY "\": {"                         \
            "\"" AIA_REGISTRATION_ENCRYPTION_ALGORITHM_KEY "\": \"%s\","      \
            "\"" AIA_REGISTRATION_ENCRYPTION_PUBLIC_KEY_KEY "\": \"%s\""      \
        "}"                                                                   \
        ",\"" AIA_REGISTRATION_IOT_KEY "\": {"                                \
            "\"" AIA_REGISTRATION_IOT_AWS_ACCOUNT_ID_KEY "\": \"%s\","        \
            "\"" AIA_REGISTRATION_IOT_CLIENT_ID_KEY "\": \"%s\","             \
            "\"" AIA_REGISTRATION_IOT_ENDPOINT_KEY "\": \"%s\""               \
        "}"                                                                   \
    "}"
/* clang-format on */

/** Private data for the @c AiaRegistrationManager_t */
struct AiaRegistrationManager
{
    /** Callback function to run when registration is successful. */
    const AiaRegistrationManagerOnRegisterSuccessCallback_t onRegisterSuccess;

    /** User data to pass to @c onRegisterSuccess. */
    void* const onRegisterSuccessUserData;

    /** Callback function to run when registration fails. */
    const AiaRegistrationManagerOnRegisterFailureCallback_t onRegisterFailure;

    /** User data to pass to @c onRegisterFailure. */
    void* const onRegisterFailureUserData;

    /** The client generated private key used for shared secret calculation */
    uint8_t privateKey[ AIA_REGISTRATION_MANAGER_GENERATED_KEY_LENGTH ];

    /** The client generated public key used for shared secret calculation */
    uint8_t publicKey[ AIA_REGISTRATION_MANAGER_GENERATED_KEY_LENGTH ];

    /** Indicates a registration is in progress. */
    bool isRegistrationInProgress;
};

/**
 * Builds the Registration request body.
 * If @c NULL is passed for @c payloadBuffer, this function will calculate
 * the length of the generated JSON payload (including the trailing @c
 * '\0').
 *
 * @param[out] requestBodyBuffer A user-provided buffer large enough to hold the
 * payload text.
 * @param requestBodyBufferSize The size (in bytes) of @c payloadBuffer.
 * @param lwaRefreshToken The LWA refresh token.
 * @param lwaClientId The LWA client ID.
 * @param algorithm The algorithm used to compute the shared secret.
 * @param publicKey The client generated public key.
 * @param awsAccountId The AWS account associated with the device.
 * @param clientId The client ID of the device.
 * @param endpoint The AWS IoT endpoint the device connects to.
 *
 * @return The size (in bytes) of the generated text (excluding the trailing @c
 * '\0'), or zero if there was an error.
 */
static size_t BuildRegistrationRequestBody(
    char* requestBodyBuffer, size_t requestBodyBufferSize,
    const char* lwaRefreshToken, const char* lwaClientId,
    AiaSecretDerivationAlgorithm_t algorithm, const char* publicKey,
    const char* awsAccountId, const char* clientId, const char* endpoint )
{
    int result =
        snprintf( requestBodyBuffer, requestBodyBufferSize,
                  REGISTRATION_REQUEST_FORMAT, lwaRefreshToken, lwaClientId,
                  AiaSecretDerivationAlgorithm_ToString( algorithm ), publicKey,
                  awsAccountId, clientId, endpoint );

    if( result <= 0 )
    {
        AiaLogError( "snprintf failed: %d", result );
        return 0;
    }
    if( requestBodyBuffer )
    {
        AiaLogInfo( "Registration request body built: %s", requestBodyBuffer );
    }
    return (size_t)result + 1;
}

/**
 * Parses a success response body, calculates shared secret, and stores topic
 * root and shared secret
 * @note When storing topic root and shared secret, if one is stored
 * successfully and the other fails then there will be an inconsistent set of
 * registration values.
 *
 * @param clientPrivateKey The client private key paired with the client public
 * key sent in the registration request.
 * @param clientPrivateKeyLen The length of @c clientPrivateKey.
 * @param body The response body.
 * @param bodyLen The length of @c body.
 *
 * @return @c true if shared secret and topic root are stored in persistent
 * storage, @c false otherwise.
 */
static bool HandleRegistrationSuccessResponseBody(
    const uint8_t* clientPrivateKey, const size_t clientPrivateKeyLen,
    const char* body, const size_t bodyLen )
{
    const char* serviceBase64PublicKey;
    size_t serviceBase64PublicKeyLength;
    if( !AiaFindJsonValue(
            body, bodyLen, AIA_REGISTRATION_ENCRYPTION_PUBLIC_KEY_KEY,
            sizeof( AIA_REGISTRATION_ENCRYPTION_PUBLIC_KEY_KEY ) - 1,
            &serviceBase64PublicKey, &serviceBase64PublicKeyLength ) )
    {
        AiaLogError( "Failed to parse the %s key in the reponse body",
                     AIA_REGISTRATION_ENCRYPTION_PUBLIC_KEY_KEY );
        return false;
    }
    if( !AiaJsonUtils_UnquoteString( &serviceBase64PublicKey,
                                     &serviceBase64PublicKeyLength ) )
    {
        AiaLogError( "Malformed JSON" );
        return false;
    }

    const char* topicRoot;
    size_t topicRootLength;
    if( !AiaFindJsonValue( body, bodyLen, AIA_REGISTRATION_IOT_TOPIC_ROOT_KEY,
                           sizeof( AIA_REGISTRATION_IOT_TOPIC_ROOT_KEY ) - 1,
                           &topicRoot, &topicRootLength ) )
    {
        AiaLogError( "Failed to parse the %s key in the response body",
                     AIA_REGISTRATION_IOT_TOPIC_ROOT_KEY );
        return false;
    }
    if( !AiaJsonUtils_UnquoteString( &topicRoot, &topicRootLength ) )
    {
        AiaLogError( "Malformed JSON" );
        return false;
    }

    size_t servicePublicKeyLen = Aia_Base64GetDecodeSize(
        (uint8_t*)serviceBase64PublicKey, serviceBase64PublicKeyLength );
    if( !servicePublicKeyLen )
    {
        AiaLogError( "Aia_Base64GetDecodeSize failed." );
        return false;
    }

    uint8_t servicePublicKey[ servicePublicKeyLen ];
    if( !Aia_Base64Decode( (uint8_t*)serviceBase64PublicKey,
                           serviceBase64PublicKeyLength, servicePublicKey,
                           servicePublicKeyLen ) )
    {
        AiaLogError( "Aia_Base64Decode failed." );
        return false;
    }

    size_t sharedSecretSizeInBits =
        AiaSecretDerivationAlgorithm_GetKeySize( SECRET_DERIVATION_ALGORITHM );
    size_t sharedSecretSizeInBytes =
        AiaBytesToHoldBits( sharedSecretSizeInBits );
    uint8_t sharedSecret[ sharedSecretSizeInBytes ];

    if( !AiaCrypto_CalculateSharedSecret(
            clientPrivateKey, clientPrivateKeyLen, servicePublicKey,
            servicePublicKeyLen, SECRET_DERIVATION_ALGORITHM, sharedSecret,
            sharedSecretSizeInBytes ) )
    {
        AiaLogError( "AiaCryptoMbedtls_CalculateSharedSecret failed" );
        return false;
    }

    if( !AiaStoreTopicRoot( (const uint8_t*)topicRoot, topicRootLength ) )
    {
        AiaLogError( "Failed to store topic root" );
        return false;
    }
    if( !AiaStoreSecret( sharedSecret, sharedSecretSizeInBytes ) )
    {
        AiaLogError( "Failed to store shared secret" );
        return false;
    }

    return true;
}

/**
 * Parses a failure response body
 *
 * @param body The response body.
 * @param bodyLen The length of @c body.
 *
 * @return @c true if parsing is successful, @c false otherwise.
 */
static bool HandleRegistrationFailedResponseBody(
    const char* body, const size_t bodyLen,
    AiaRegistrationFailureCode_t* failureCode )
{
    const char* code;
    size_t codeLen;
    if( !AiaFindJsonValue( body, bodyLen, AIA_REGISTRATION_CODE_KEY,
                           sizeof( AIA_REGISTRATION_CODE_KEY ) - 1, &code,
                           &codeLen ) )
    {
        AiaLogError( "Failed to parse the %s key in the reponse body",
                     AIA_REGISTRATION_CODE_KEY );
        *failureCode = AIA_REGISTRATION_FAILURE_RESPONSE_ERROR;
        return false;
    }
    if( !AiaJsonUtils_UnquoteString( &code, &codeLen ) )
    {
        AiaLogError( "Malformed JSON" );
        *failureCode = AIA_REGISTRATION_FAILURE_RESPONSE_ERROR;
        return false;
    }

    const char* description;
    size_t descriptionLen;
    if( !AiaFindJsonValue( body, bodyLen, AIA_REGISTRATION_DESCRIPTION_KEY,
                           sizeof( AIA_REGISTRATION_DESCRIPTION_KEY ) - 1,
                           &description, &descriptionLen ) )
    {
        AiaLogError( "Failed to parse the %s key in the response body",
                     AIA_REGISTRATION_DESCRIPTION_KEY );
        *failureCode = AIA_REGISTRATION_FAILURE_RESPONSE_ERROR;
        return false;
    }
    if( !AiaJsonUtils_UnquoteString( &description, &descriptionLen ) )
    {
        AiaLogError( "Malformed JSON" );
        *failureCode = AIA_REGISTRATION_FAILURE_RESPONSE_ERROR;
        return false;
    }

    AiaLogInfo(
        "Registration Failure Response received. code=%.*s, description=%.*s",
        codeLen, code, descriptionLen, description );

    if( !AiaRegistrationFailureCode_FromString( code, codeLen, failureCode ) )
    {
        *failureCode = AIA_REGISTRATION_FAILURE_RESPONSE_ERROR;
    }

    return true;
}

/**
 * Callback called when a response is received for the registration request.
 *
 * @param httpsResponse The data for the response received from the server.
 * @param userData The user data for the callback.
 */
static void OnRegistrationResponseReceived( AiaHttpsResponse_t* httpsResponse,
                                            void* userData )
{
    AiaAssert( httpsResponse );
    if( !httpsResponse )
    {
        AiaLogError( "Null httpsResponse." );
        return;
    }
    AiaAssert( userData );
    if( !userData )
    {
        AiaLogError( "Null userData." );
        return;
    }

    AiaRegistrationManager_t* registrationManager =
        (AiaRegistrationManager_t*)userData;

    if( httpsResponse->status == 200 )
    {
        if( !HandleRegistrationSuccessResponseBody(
                registrationManager->privateKey,
                sizeof( registrationManager->privateKey ), httpsResponse->body,
                httpsResponse->bodyLen ) )
        {
            AiaLogError( "HandleRegistrationSuccessResponseBody failed." );
            registrationManager->isRegistrationInProgress = false;
            registrationManager->onRegisterFailure(
                registrationManager->onRegisterFailureUserData,
                AIA_REGISTRATION_FAILURE_RESPONSE_ERROR );
            return;
        }

        registrationManager->isRegistrationInProgress = false;
        registrationManager->onRegisterSuccess(
            registrationManager->onRegisterSuccessUserData );
    }
    else
    {
        AiaRegistrationFailureCode_t failureCode;
        if( !HandleRegistrationFailedResponseBody(
                httpsResponse->body, httpsResponse->bodyLen, &failureCode ) )
        {
            AiaLogError( "HandleRegistrationFailedResponseBody failed." );
        }

        registrationManager->isRegistrationInProgress = false;
        registrationManager->onRegisterFailure(
            registrationManager->onRegisterFailureUserData, failureCode );
    }
    return;
}

/**
 * Callback called when the registration request fails to send.
 *
 * @param userData The user data for the callback.
 */
static void OnRegistrationRequestFailure( void* userData )
{
    AiaAssert( userData );
    if( !userData )
    {
        AiaLogError( "Null userData." );
        return;
    }
    AiaRegistrationManager_t* registrationManager =
        (AiaRegistrationManager_t*)userData;
    registrationManager->isRegistrationInProgress = false;
    registrationManager->onRegisterFailure(
        registrationManager->onRegisterFailureUserData,
        AIA_REGISTRATION_FAILURE_SEND_FAILED );
}

AiaRegistrationManager_t* AiaRegistrationManager_Create(
    AiaRegistrationManagerOnRegisterSuccessCallback_t onRegisterSuccess,
    void* onRegisterSuccessUserData,
    AiaRegistrationManagerOnRegisterFailureCallback_t onRegisterFailure,
    void* onRegisterFailureUserData )
{
    if( !onRegisterSuccess )
    {
        AiaLogError( "Null onRegisterSuccess callback." );
        return NULL;
    }
    if( !onRegisterFailure )
    {
        AiaLogError( "Null onRegisterFailure callback." );
        return NULL;
    }

    size_t registrationManagerSize = sizeof( struct AiaRegistrationManager );
    AiaRegistrationManager_t* registrationManager =
        (AiaRegistrationManager_t*)AiaCalloc( 1, registrationManagerSize );
    if( !registrationManager )
    {
        AiaLogError( "AiaCalloc failed (%ze bytes).", registrationManagerSize );
        return NULL;
    }

    *(AiaRegistrationManagerOnRegisterSuccessCallback_t*)&registrationManager
         ->onRegisterSuccess = onRegisterSuccess;
    *(void**)&registrationManager->onRegisterSuccessUserData =
        onRegisterSuccessUserData;
    *(AiaRegistrationManagerOnRegisterFailureCallback_t*)&registrationManager
         ->onRegisterFailure = onRegisterFailure;
    *(void**)&registrationManager->onRegisterFailureUserData =
        onRegisterFailureUserData;

    if( !AiaCrypto_GenerateKeyPair( SECRET_DERIVATION_ALGORITHM,
                                    registrationManager->privateKey,
                                    sizeof( registrationManager->privateKey ),
                                    registrationManager->publicKey,
                                    sizeof( registrationManager->publicKey ) ) )
    {
        AiaLogError( "Failed to generate key pair." );
        AiaRegistrationManager_Destroy( registrationManager );
        return NULL;
    }

    return registrationManager;
}

bool AiaRegistrationManager_Register(
    AiaRegistrationManager_t* registrationManager )
{
    if( !registrationManager )
    {
        AiaLogError( "Null connectionManager." );
        return false;
    }

    if( registrationManager->isRegistrationInProgress )
    {
        AiaLogError( "Registration already in progress" );
        return false;
    }

    registrationManager->isRegistrationInProgress = true;

    size_t base64PublicKeyLen =
        Aia_Base64GetEncodeSize( registrationManager->publicKey,
                                 sizeof( registrationManager->publicKey ) );
    if( !base64PublicKeyLen )
    {
        registrationManager->isRegistrationInProgress = false;
        AiaLogError( "Aia_Base64GetEncodeSize failed." );
        return false;
    }

    uint8_t base64PublicKey[ base64PublicKeyLen ];
    if( !Aia_Base64Encode( registrationManager->publicKey,
                           sizeof( registrationManager->publicKey ),
                           base64PublicKey, base64PublicKeyLen ) )
    {
        registrationManager->isRegistrationInProgress = false;
        AiaLogError( "Aia_Base64Encode failed." );
        return false;
    }

    size_t refreshTokenLen;
    if( !AiaGetRefreshToken( NULL, &refreshTokenLen ) )
    {
        registrationManager->isRegistrationInProgress = false;
        AiaLogError(
            "AiaGetRefreshToken Failed. Failed to get LWA refresh token "
            "length." );
        return false;
    }
    char* refreshToken = (char*)AiaCalloc( 1, refreshTokenLen );
    if( !AiaGetRefreshToken( refreshToken, &refreshTokenLen ) )
    {
        registrationManager->isRegistrationInProgress = false;
        AiaFree( refreshToken );
        AiaLogError(
            "AiaGetRefreshToken Failed. Failed to retrieve LWA refresh "
            "token." );
        return false;
    }

    size_t lwaClientIdLen;
    if( !AiaGetLwaClientId( NULL, &lwaClientIdLen ) )
    {
        registrationManager->isRegistrationInProgress = false;
        AiaFree( refreshToken );
        AiaLogError(
            "AiaGetLwaClientId Failed. Failed to get LWA Client Id length." );
        return false;
    }
    char lwaClientId[ lwaClientIdLen ];
    if( !AiaGetLwaClientId( lwaClientId, &lwaClientIdLen ) )
    {
        registrationManager->isRegistrationInProgress = false;
        AiaFree( refreshToken );
        AiaLogError(
            "AiaGetLwaClientId Failed. Failed to retrieve LWA Client Id." );
        return false;
    }

    size_t iotClientIdLen;
    if( !AiaGetIotClientId( NULL, &iotClientIdLen ) )
    {
        registrationManager->isRegistrationInProgress = false;
        AiaFree( refreshToken );
        AiaLogError(
            "AiaGetIotClientId Failed. Failed to get IoT Client Id length." );
        return false;
    }
    char iotClientId[ iotClientIdLen ];
    if( !AiaGetIotClientId( iotClientId, &iotClientIdLen ) )
    {
        registrationManager->isRegistrationInProgress = false;
        AiaFree( refreshToken );
        AiaLogError(
            "AiaGetIotClientId Failed. Failed to retrieve IoT Client Id." );
        return false;
    }

    size_t awsAccountIdLen;
    if( !AiaGetAwsAccountId( NULL, &awsAccountIdLen ) )
    {
        registrationManager->isRegistrationInProgress = false;
        AiaFree( refreshToken );
        AiaLogError(
            "AiaGetAwsAccountId Failed. Failed to get AWS Account Id length." );
        return false;
    }
    char awsAccountId[ awsAccountIdLen ];
    if( !AiaGetAwsAccountId( awsAccountId, &awsAccountIdLen ) )
    {
        registrationManager->isRegistrationInProgress = false;
        AiaFree( refreshToken );
        AiaLogError(
            "AiaGetAwsAccountId Failed. Failed to retrieve AWS Account Id." );
        return false;
    }

    size_t iotEndpointLen;
    if( !AiaGetIotEndpoint( NULL, &iotEndpointLen ) )
    {
        registrationManager->isRegistrationInProgress = false;
        AiaFree( refreshToken );
        AiaLogError(
            "AiaGetIotEndpoint Failed. Failed to get IoT endpoint length." );
        return false;
    }
    char iotEndpoint[ iotEndpointLen ];
    if( !AiaGetIotEndpoint( iotEndpoint, &iotEndpointLen ) )
    {
        registrationManager->isRegistrationInProgress = false;
        AiaFree( refreshToken );
        AiaLogError(
            "AiaGetIotEndpoint Failed. Failed to retrieve IoT endpoint." );
        return false;
    }

    size_t requestBodyBufferSize = BuildRegistrationRequestBody(
        NULL, 0, refreshToken, lwaClientId, SECRET_DERIVATION_ALGORITHM,
        (char*)base64PublicKey, awsAccountId, iotClientId, iotEndpoint );
    if( !requestBodyBufferSize )
    {
        registrationManager->isRegistrationInProgress = false;
        AiaFree( refreshToken );
        AiaLogError( "BuildRegistrationRequestBody failed." );
        return false;
    }

    char requestBodyBuffer[ requestBodyBufferSize ];
    BuildRegistrationRequestBody(
        requestBodyBuffer, requestBodyBufferSize, refreshToken, lwaClientId,
        SECRET_DERIVATION_ALGORITHM, (char*)base64PublicKey, awsAccountId,
        iotClientId, iotEndpoint );

    const char* headers[] = { REGISTRATION_REQUEST_CONTENT };

    AiaHttpsRequest_t httpsRequest;
    httpsRequest.method = AIA_HTTPS_METHOD_POST;
    httpsRequest.headers = headers;
    httpsRequest.headersLen = 1;
    httpsRequest.url = AIA_REGISTRATION_ENDPOINT;
    httpsRequest.body = requestBodyBuffer;

    if( !AiaSendHttpsRequest( &httpsRequest, OnRegistrationResponseReceived,
                              registrationManager, OnRegistrationRequestFailure,
                              registrationManager ) )
    {
        registrationManager->isRegistrationInProgress = false;
        AiaFree( refreshToken );
        AiaLogError( "AiaSendHttpsRequest failed." );
        return false;
    }

    AiaFree( refreshToken );
    return true;
}

void AiaRegistrationManager_Destroy(
    AiaRegistrationManager_t* registrationManager )
{
    if( !registrationManager )
    {
        AiaLogDebug( "Null registrationManager" );
        return;
    }

    AiaFree( registrationManager );
}
