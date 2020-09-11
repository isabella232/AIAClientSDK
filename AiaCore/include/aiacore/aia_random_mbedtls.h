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
 * @file aia_random_mbedtls.h
 * @brief random number generator for AIA.
 */

#ifndef AIA_RANDOM_MBEDTLS_H_
#define AIA_RANDOM_MBEDTLS_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <stdbool.h>

/**
 * One time initialization for random number generation. Calling more than once
 * without calling @c AiaRandomMbedtls_Cleanup() first may result in a crash.
 */
void AiaRandomMbedtls_Init();

/**
 * Seeds the random number generator.
 * @note Thread safe if mbed TLS threading layer is enabled by calling
 * mbedtls_threading_set_alt().
 *
 * @param salt Changes the starting point for the random generator (may
 * be NULL and does not need to be '\0' terminated).
 * @param saltLength The length of @c salt.
 * @return @c true if random number generator is successfully seeded, else @c
 * false.
 */
bool AiaRandomMbedtls_Seed( const char* salt, size_t saltLength );

/**
 * Generates a cryptographically secure random number.
 * @note Thread safe if mbed TLS threading layer is enabled by calling
 * mbedtls_threading_set_alt().
 *
 * @param [out] buffer The buffer to fill with random data
 * @param bufferLength The length of @c buffer.
 * @return @c true if random number is generated successfully, else
 * @c false.
 */
bool AiaRandomMbedtls_Rand( unsigned char* buffer, size_t bufferLength );

/**
 * One time deinitialization for random number generation. Frees resources taken
 * in @c AiaRandomMbedtls_Init().
 */
void AiaRandomMbedtls_Cleanup();

#endif /* ifndef AIA_RANDOM_MBEDTLS_H_ */
