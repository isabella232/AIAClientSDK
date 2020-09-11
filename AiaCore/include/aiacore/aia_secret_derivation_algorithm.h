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
 * @file aia_secret_derivation_algorithm.h
 * @brief User-facing functions and variables for secret derivation.
 */

#ifndef AIA_SECRET_DERIVATION_ALGORITHM_H_
#define AIA_SECRET_DERIVATION_ALGORITHM_H_

/* The config header is always included first. */
#include <aia_config.h>

#include "aia_encryption_algorithm.h"

#include <stddef.h>

/** Secret derivation algorithms supported by Aia */
typedef enum AiaSecretDerivationAlgorithm
{
    /**
     * The 16-byte shared secret will be the output of hashing the 32-byte
     * output of ECDH protocol using Curve25519 by using the Secure Hash
     * Algorithm 256 (SHA-256), employing HKDF with no salt and no info, and
     * then truncating that hashed result to 16 bytes.
     */
    AIA_ECDH_CURVE_25519_16_BYTE_SHA256,
    /** The 32-byte shared secret will be the direct 32-byte output of the ECDH
       protocol using Curve25519. */
    AIA_ECDH_CURVE_25519_32_BYTE
} AiaSecretDerivationAlgorithm_t;

/**
 * @param algorithm The secret derivation algorithm to get the key size of.
 * @return The key size (in bits) of @c algorithm.
 */
size_t AiaSecretDerivationAlgorithm_GetKeySize(
    AiaSecretDerivationAlgorithm_t algorithm );

/**
 * @param algorithm The secret derivation algorithm to get the string
 * representation of.
 * @return The string representation for @c algorithm.
 */
const char* AiaSecretDerivationAlgorithm_ToString(
    AiaSecretDerivationAlgorithm_t algorithm );

/**
 * Converts a secret derivation algorithm to an encryption algorithm.
 *
 * @param secretDerivationAlgorithm The secret derivation algorithm to convert.
 * @return The equivalent encryption algorithm.
 */
AiaEncryptionAlgorithm_t AiaSecretDerivationAlgorithm_ToEncryptionAlgorithm(
    AiaSecretDerivationAlgorithm_t secretDerivationAlgorithm );

#endif /* ifndef AIA_SECRET_DERIVATION_ALGORITHM_H_ */
