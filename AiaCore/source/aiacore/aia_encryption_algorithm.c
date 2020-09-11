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
 * @file aia_encryption_algorithm.c
 * @brief Implements functions for the AiaEncryptionAlgorithm_t type.
 */

/* The config header is always included first. */
#include <aia_config.h>
#include <registration/aia_registration_config.h>

#include <aiacore/aia_encryption_algorithm.h>
#include <aiacore/aia_secret_derivation_algorithm.h>

size_t AiaEncryptionAlgorithm_GetKeySize()
{
    return AiaSecretDerivationAlgorithm_GetKeySize(
        SECRET_DERIVATION_ALGORITHM );
}

const char* AiaEncryptionAlgorithm_ToString(
    AiaEncryptionAlgorithm_t algorithm )
{
    switch( algorithm )
    {
        case AIA_AES_GCM:
            return "AIA_AES_GCM";
    }
    AiaLogError( "Unknown encryption algorithm %d.", algorithm );
    AiaAssert( false );
    return "";
}
