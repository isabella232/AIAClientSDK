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
 * @file aia_mbedtls_threading.c
 * @brief Implements functions for the global initialization/cleanup for mbed
 * TLS threading.
 */

/* The config header is always included first. */
#include <aia_config.h>

#include <aiacore/aia_mbedtls_threading.h>

#include AiaMutex( HEADER )

#include <mbedtls/threading.h>

/**
 * Initializes a new mutex. Used by mbed TLS to provide thread-safety.
 *
 * Sets the valid member of `mbedtls_threading_mutex_t`.
 *
 * @param[in] mbedtlsMutex The mutex to initialize.
 */
static void _AiaMbedtlsThreading_MutexInit(
    mbedtls_threading_mutex_t* mbedtlsMutex )
{
    mbedtlsMutex->mutex = AiaCalloc( sizeof( AiaMutex_t ), 1 );
    if( !mbedtlsMutex->mutex )
    {
        AiaLogError( "Failed to allocate %zu bytes.", sizeof( AiaMutex_t ) );
        mbedtlsMutex->valid = false;
    }
    mbedtlsMutex->valid = AiaMutex( Create )( mbedtlsMutex->mutex, false );
}

/**
 * Frees a mutex. Used by mbed TLS to provide thread-safety.
 *
 * @param[in] mbedtlsMutex The mutex to destroy.
 */
static void _AiaMbedtlsThreading_MutexFree(
    mbedtls_threading_mutex_t* mbedtlsMutex )
{
    if( mbedtlsMutex->valid == true )
    {
        AiaMutex( Destroy )( mbedtlsMutex->mutex );
        AiaFree( mbedtlsMutex->mutex );
        mbedtlsMutex->valid = false;
    }
}

/**
 * Locks a mutex. Used by mbed TLS to provide thread-safety.
 *
 * @param[in] mbedtlsMutex The mutex to lock.
 *
 * @return `0` on success; one of `MBEDTLS_ERR_THREADING_BAD_INPUT_DATA`
 * or `MBEDTLS_ERR_THREADING_MUTEX_ERROR` on error.
 */
static int _AiaMbedtlsThreading_MutexLock(
    mbedtls_threading_mutex_t* mbedtlsMutex )
{
    int status = 0;

    if( mbedtlsMutex->valid == false )
    {
        status = MBEDTLS_ERR_THREADING_BAD_INPUT_DATA;
    }
    else
    {
        AiaMutex( Lock )( mbedtlsMutex->mutex );
    }

    return status;
}

/**
 * Unlocks a mutex. Used by mbed TLS to provide thread-safety.
 *
 * @param[in] mbedtlsMutex The mutex to unlock.
 *
 * @return `0` on success; one of `MBEDTLS_ERR_THREADING_BAD_INPUT_DATA`
 * or `MBEDTLS_ERR_THREADING_MUTEX_ERROR` on error.
 */
static int _AiaMbedtlsThreading_MutexUnlock(
    mbedtls_threading_mutex_t* mbedtlsMutex )
{
    int status = 0;

    if( mbedtlsMutex->valid == false )
    {
        status = MBEDTLS_ERR_THREADING_BAD_INPUT_DATA;
    }
    else
    {
        AiaMutex( Unlock )( mbedtlsMutex->mutex );
    }

    return status;
}

void AiaMbedtlsThreading_Init()
{
    /* Set the mutex functions for mbed TLS thread safety. */
    mbedtls_threading_set_alt(
        _AiaMbedtlsThreading_MutexInit, _AiaMbedtlsThreading_MutexFree,
        _AiaMbedtlsThreading_MutexLock, _AiaMbedtlsThreading_MutexUnlock );
}

void AiaMbedtlsThreading_Cleanup()
{
    /* Clear the mutex functions for mbed TLS thread safety. */
    mbedtls_threading_free_alt();
}
