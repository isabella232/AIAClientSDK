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
 * @file aia_crypto_mbedtls.h
 * @brief Implements encryption/decryption functions for AIA using mbedtls.
 */

#ifndef AIA_CRYPTO_MBEDTLS_H_
#define AIA_CRYPTO_MBEDTLS_H_

/* The config header is always included first. */
#include <aia_config.h>

#include "aia_encryption_algorithm.h"
#include "aia_secret_derivation_algorithm.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * One time initialization for encryption/decryption functions.
 *
 * @return @c true if the initialization is successful, else @c false.
 */
bool AiaCryptoMbedtls_Init();

/**
 * @copyDoc AiaCrypt_SetKey()
 */
bool AiaCryptoMbedtls_SetKey( const uint8_t *encryptKey, size_t encryptKeySize,
                              const AiaEncryptionAlgorithm_t encryptAlgorithm );

/**
 * @copyDoc AiaCrypto_Encrypt()
 */
bool AiaCryptoMbedtls_Encrypt( const uint8_t *inputData, const size_t inputLen,
                               uint8_t *outputData, uint8_t *iv, size_t ivLen,
                               uint8_t *tag, const size_t tagLen );

/**
 * @copyDoc AiaCrypto_Decrypt()
 */
bool AiaCryptoMbedtls_Decrypt( const uint8_t *inputData, const size_t inputLen,
                               uint8_t *outputData, const uint8_t *iv,
                               const size_t ivLen, const uint8_t *tag,
                               const size_t tagLen );

/**
 * @copyDoc AiaCrypto_GenerateKeyPair()
 */
bool AiaCryptoMbedtls_GenerateKeyPair(
    AiaSecretDerivationAlgorithm_t secretDerivationAlgorithm,
    uint8_t *privateKey, size_t privateKeyLen, uint8_t *publicKeyconst,
    size_t publicKeyLen );

/**
 * @copyDoc AiaCrypto_CalculateSharedSecret()
 */
bool AiaCryptoMbedtls_CalculateSharedSecret(
    const uint8_t *clientPrivateKey, size_t clientPrivateKeyLen,
    const uint8_t *servicePublicKey, size_t servicePublicKeyLen,
    AiaSecretDerivationAlgorithm_t secretDerivationAlgorithm,
    uint8_t *sharedSecret, size_t sharedSecretLen );

/**
 * One time deinitialization for encryption/decryption functions. Frees
 * resources taken in @c AiaCryptoMbedtls_Init().
 */
void AiaCryptoMbedtls_Cleanup();

#endif /* ifndef AIA_CRYPTO_MBEDTLS_H_ */
