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
 * @file aia_secret_derivation_algorithm.c
 * @brief Implements functions for secret derivation.
 */

/* The config header is always included first. */
#include <aia_config.h>

#include <aiacore/aia_secret_derivation_algorithm.h>

size_t AiaSecretDerivationAlgorithm_GetKeySize(
    AiaSecretDerivationAlgorithm_t algorithm )
{
    switch( algorithm )
    {
        case AIA_ECDH_CURVE_25519_16_BYTE_SHA256:
            return 128;
        case AIA_ECDH_CURVE_25519_32_BYTE:
            return 256;
    }
    AiaLogError( "Unknown encryption algorithm %d.", algorithm );
    AiaAssert( false );
    return 0;
}

const char* AiaSecretDerivationAlgorithm_ToString(
    AiaSecretDerivationAlgorithm_t algorithm )
{
    switch( algorithm )
    {
        case AIA_ECDH_CURVE_25519_16_BYTE_SHA256:
            return "ECDH_CURVE_25519_16_BYTE_SHA256";
        case AIA_ECDH_CURVE_25519_32_BYTE:
            return "ECDH_CURVE_25519_32_BYTE";
    }
    AiaLogError( "Unknown encryption algorithm %d.", algorithm );
    AiaAssert( false );
    return "";
}

AiaEncryptionAlgorithm_t AiaSecretDerivationAlgorithm_ToEncryptionAlgorithm(
    AiaSecretDerivationAlgorithm_t secretDerivationAlgorithm )
{
    switch( secretDerivationAlgorithm )
    {
        case AIA_ECDH_CURVE_25519_16_BYTE_SHA256:
            /* Fall-through */
        case AIA_ECDH_CURVE_25519_32_BYTE:
            return AIA_AES_GCM;
    }
    AiaLogError( "Unknown secret derivation algorithm %d.",
                 secretDerivationAlgorithm );
    AiaAssert( false );
    return 0;
}
