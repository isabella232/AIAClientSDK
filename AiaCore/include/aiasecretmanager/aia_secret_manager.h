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
 * @file aia_secret_manager.h
 * @brief User-facing functions of the @c AiaSecretManager_t type.
 */

#ifndef AIA_SECRET_MANAGER_H_
#define AIA_SECRET_MANAGER_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <aiacore/aia_message_constants.h>
#include <aiacore/aia_topic.h>
#include <aiaregulator/aia_regulator.h>

/**
 * This class manages shared secrets for the SDK and performs
 * encryption/decryption using them.
 *
 * @note Functions in this header which act on an @c AiaSecretManager_t are
 *     thread-safe.
 */
typedef struct AiaSecretManager AiaSecretManager_t;

/**
 * @copydoc AiaEmitter_GetNextSequenceNumber()
 *
 * @param topic The topic to get the next sequence number of.,
 * @param[out] nextSequenceNumber If successful, this will be set to the next
 * sequence number.
 * @param userData Context associated with this callback.
 * @return @c true if the next sequence number is returned successfully or @c
 * false otherwise.
 * @note Implementations are required to be thread-safe.
 */
typedef bool ( *AiaGetNextSequenceNumber_t )(
    AiaTopic_t topic, AiaSequenceNumber_t* nextSequenceNumber, void* userData );

/**
 * @copydoc AiaRegulator_Write()
 *
 * @param chunk Message chunk to be written.
 * @param userData Context associated with this callback.
 * @return @c true if the message chunk was succesfully added, else @c false.
 * @note Implementations are required to be thread-safe.
 */
typedef bool ( *AiaEmitEvent_t )( AiaRegulatorChunk_t* chunk, void* userData );

/**
 * Allocates and initializes a new @c AiaSecretManager_t.  An @c
 * AiaSecretManager_t created by this function should later be released by a
 * call to @c AiaSecretManager_Destroy().
 *
 * @param getNextSequenceNumber Used to query for outbound sequence numbers.
 * @param getNextSequenceNumberUserData User data associated with @c
 * getNextSequenceNumber.
 * @param emitEvent Used to publish events.
 * @param emitEventUserData User data associated with @c emitEvent.
 * @return The newly-constructed @c AiaSecretManager_t when successful, else @c
 *     NULL.
 */
AiaSecretManager_t* AiaSecretManager_Create(
    AiaGetNextSequenceNumber_t getNextSequenceNumber,
    void* getNextSequenceNumberUserData, AiaEmitEvent_t emitEvent,
    void* emitEventUserData );

/**
 * Releases a @c AiaSecretManager_t previously allocated by @c
 * AiaSecretManager_Create().
 *
 * @param secretManager The secret manager instance to act on.
 */
void AiaSecretManager_Destroy( AiaSecretManager_t* secretManager );

/**
 * Encrypts the given input data using the correct shared secret and algorithm
 * for the specified topic and sequence number.
 *
 * @param secretManager The @c SecretManager_t to use for encrypting.
 * @param topic The @c AiaTopic_t that @c outputData will be published on.
 * @param sequenceNumber The @c AiaSequenceNumber_t assigned to @c inputData.
 *
 * For remaining parameters and return value, see AiaCrypto_Encrypt().
 */
bool AiaSecretManager_Encrypt( AiaSecretManager_t* secretManager,
                               AiaTopic_t topic,
                               AiaSequenceNumber_t sequenceNumber,
                               const uint8_t* inputData, const size_t inputLen,
                               uint8_t* outputData, uint8_t* iv, size_t ivLen,
                               uint8_t* tag, const size_t tagLen );

/**
 * Decrypts the given input data using the correct shared secret and algorithm
 * for the specified topic and sequence number.
 *
 * @param secretManager The @c SecretManager_t to use for decrypting.
 * @param topic The @c AiaTopic_t that @c inputData was received from.
 * @param sequenceNumber The @c AiaSequenceNumber_t assigned to @c inputData.
 *
 * For remaining parameters and return value, see AiaCrypto_Decrypt().
 */
bool AiaSecretManager_Decrypt( AiaSecretManager_t* secretManager,
                               AiaTopic_t topic,
                               AiaSequenceNumber_t sequenceNumber,
                               const uint8_t* inputData, const size_t inputLen,
                               uint8_t* outputData, const uint8_t* iv,
                               const size_t ivLen, const uint8_t* tag,
                               const size_t tagLen );

#endif /* ifndef AIA_SECRET_MANAGER_H_ */
