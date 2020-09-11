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
 * @file aia_encryption_algorithm.h
 * @brief User-facing functions and variables for encryption/decryption.
 */

#ifndef AIA_ENCRYPTION_ALGORITHM_H_
#define AIA_ENCRYPTION_ALGORITHM_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <stddef.h>

/** Encryption algorithms supported by Aia */
typedef enum AiaEncryptionAlgorithm
{
    /* AES-GCM encryption. */
    AIA_AES_GCM
} AiaEncryptionAlgorithm_t;

/**
 * @param algorithm The encryption algorithm to get the key size of.
 * @return The key size (in bits) of @c algorithm.
 */
size_t AiaEncryptionAlgorithm_GetKeySize();

/**
 * @param algorithm The encryption algorithm to get the string representation
 *     of.
 * @return The string representation for @c algorithm.
 */
const char* AiaEncryptionAlgorithm_ToString(
    AiaEncryptionAlgorithm_t algorithm );

#endif /* ifndef AIA_ENCRYPTION_ALGORITHM_H_ */
