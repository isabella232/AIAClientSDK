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
 * @file aia_mock_secret_manager.c
 * @brief Implements mock functions for the AiaMockSecretManager_t type.
 */

/* The config header is always included first. */
#include <aia_config.h>

#include <aiasecretmanager/aia_secret_manager.h>

/** TODO: ADSER-1781 - Investigate mock functions for secret manager */

/** Private data for the @c AiaMockSecretManager_t type. */
typedef struct AiaMockSecretManager
{
    /* TODO: Keep a history of two secrets and a table of per-topic sequence
     * numbers for rotations. (ADSER-1124) */
    int placeholder;
} AiaMockSecretManager_t;

AiaSecretManager_t* AiaSecretManager_Create(
    AiaGetNextSequenceNumber_t getNextSequenceNumber,
    void* getNextSequenceNumberUserData, AiaEmitEvent_t emitEvent,
    void* emitEventUserData )
{
    (void)getNextSequenceNumber;
    (void)getNextSequenceNumberUserData;
    (void)emitEvent;
    (void)emitEventUserData;
    AiaMockSecretManager_t* mockSecretManager =
        AiaCalloc( 1, sizeof( AiaMockSecretManager_t ) );
    if( !mockSecretManager )
    {
        return NULL;
    }

    return (AiaSecretManager_t*)mockSecretManager;
}

void AiaSecretManager_Destroy( AiaSecretManager_t* secretManager )
{
    AiaFree( secretManager );
}

bool AiaSecretManager_Decrypt( AiaSecretManager_t* secretManager,
                               AiaTopic_t topic,
                               AiaSequenceNumber_t sequenceNumber,
                               const uint8_t* inputData, const size_t inputLen,
                               uint8_t* outputData, const uint8_t* iv,
                               const size_t ivLen, const uint8_t* tag,
                               const size_t tagLen )
{
    (void)secretManager;
    (void)topic;
    (void)sequenceNumber;
    (void)inputData;
    (void)inputLen;
    (void)outputData;
    (void)iv;
    (void)ivLen;
    (void)tag;
    (void)tagLen;

    if( !secretManager )
    {
        AiaLogError( "Null secretManager." );
        return false;
    }

    /* Either inputLen should be 0 or input should be non-null */
    if( inputLen != 0 && !inputData )
    {
        AiaLogError( "Invalid input data for encryption." );
        return false;
    }

    /* Either inputLen should be 0 or output should be non-null */
    if( inputLen != 0 && !outputData )
    {
        AiaLogError( "Invalid output data for encryption." );
        return false;
    }

    if( !iv )
    {
        AiaLogError( "Null iv." );
        return false;
    }

    if( !tag )
    {
        AiaLogError( "Null tag." );
        return false;
    }

    return true;
}
