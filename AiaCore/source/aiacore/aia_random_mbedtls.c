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
 * @file aia_random_mbedtls.c
 * @brief Implements functions for the AiaRandomMbedtls random number generator.
 */

/* The config header is always included first. */
#include <aia_config.h>

#include <aiacore/aia_random_mbedtls.h>

/* mbed TLS includes. */
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>

#include <string.h>

/* The longest error message has 134 characters (including \0) */
#define AIA_RANDOM_MBEDTLS_ERROR_BUFFER_LENGTH 134

/**
 * mbed TLS entropy context for generation of random numbers.
 */
static mbedtls_entropy_context g_entropyContext;

/**
 * mbed TLS CTR DRBG context for generation of random numbers.
 */
static mbedtls_ctr_drbg_context g_ctrDrbgContext;

void AiaRandomMbedtls_Init()
{
    /* Initialize contexts for random number generation. */
    mbedtls_entropy_init( &( g_entropyContext ) );
    mbedtls_ctr_drbg_init( &( g_ctrDrbgContext ) );
}

bool AiaRandomMbedtls_Seed( const char *salt, size_t saltLength )
{
    int mbedtlsError = 0;

    mbedtlsError = mbedtls_ctr_drbg_seed(
        &( g_ctrDrbgContext ), mbedtls_entropy_func, &( g_entropyContext ),
        (const unsigned char *)salt, saltLength );

    if( mbedtlsError != 0 )
    {
        char errorBuffer[ AIA_RANDOM_MBEDTLS_ERROR_BUFFER_LENGTH ];
        mbedtls_strerror( mbedtlsError, errorBuffer,
                          AIA_RANDOM_MBEDTLS_ERROR_BUFFER_LENGTH );
        AiaLogError( "Failed to seed RNG. Error: %s", errorBuffer );
        return false;
    }

    return true;
}

bool AiaRandomMbedtls_Rand( unsigned char *buffer, size_t bufferLength )
{
    if( !buffer )
    {
        AiaLogError( "Null buffer." );
        return false;
    }

    int mbedtlsError = 0;

    mbedtlsError =
        mbedtls_ctr_drbg_random( &( g_ctrDrbgContext ), buffer, bufferLength );

    if( mbedtlsError != 0 )
    {
        char errorBuffer[ AIA_RANDOM_MBEDTLS_ERROR_BUFFER_LENGTH ];
        mbedtls_strerror( mbedtlsError, errorBuffer,
                          AIA_RANDOM_MBEDTLS_ERROR_BUFFER_LENGTH );
        AiaLogError( "Failed to generate random number. Error: %s",
                     errorBuffer );
        return false;
    }

    return true;
}

void AiaRandomMbedtls_Cleanup()
{
    /* Free the contexts for random number generation. */
    mbedtls_entropy_free( &( g_entropyContext ) );
    mbedtls_ctr_drbg_free( &( g_ctrDrbgContext ) );
}
