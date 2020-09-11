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
 * @file aia_crypto_mbedtls.c
 * @brief Implements encryption/decryption functions for AIA.
 */

/* The config header is always included first. */
#include <aia_config.h>

#include <aiacore/aia_crypto_mbedtls.h>
#include <aiacore/aia_encryption_algorithm.h>
#include <aiacore/aia_message_constants.h>
#include <aiacore/aia_utils.h>

/* mbed TLS includes. */
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/ecdh.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/gcm.h>
#include <mbedtls/hkdf.h>

/* Length of the shared secret after shared secret computation */
#define AIA_CRYPTO_MBEDTLS_SHARED_SECRET_BUFFER_LENGTH 32
/* The longest error message has 40 characters (including \0) */
#define AIA_CRYPTO_MBEDTLS_ERROR_BUFFER_LENGTH 40

/* Personalization data for seeding the RNG */
static const char *AIA_CRYPTO_PERS_DATA = "AIA_CRYPTO_PERS_DATA";

/* Used to provide thread-safety for GCM context */
static AiaMutex_t gcmMutex;

/**
 * mbed TLS entropy and ctr_drbg contexts need for RNG.
 */
/** @{ */
static mbedtls_entropy_context g_entropyContext;
static mbedtls_ctr_drbg_context g_ctrdrbgContext;
/** @} */

/**
 * mbed TLS gcm context for implementation of encryption/decryption functions.
 */
static mbedtls_gcm_context g_gcmContext;

/**
 * Logs a mbedTLS error
 *
 * @param errorMessage The error message to log along with the mbedTLS error.
 * @param errorCode The error code returned from the mbedTLS function.
 */
static void AiaCryptoMbedtls_LogMbedtlsError( const char *errorMessage,
                                              int errorCode )
{
    char errorBuffer[ AIA_CRYPTO_MBEDTLS_ERROR_BUFFER_LENGTH ];
    mbedtls_strerror( errorCode, errorBuffer,
                      AIA_CRYPTO_MBEDTLS_ERROR_BUFFER_LENGTH );
    AiaLogError( "%s. Error: %s", errorMessage, errorBuffer );
}

static const char *AiaCryptoMbedtls_GenerateKeyPairInternal(
    AiaSecretDerivationAlgorithm_t secretDerivationAlgorithm,
    uint8_t *privateKey, size_t privateKeyLen, uint8_t *publicKey,
    size_t publicKeyLen, mbedtls_ecdh_context *ecdhContext, int *errorCode )
{
    if( secretDerivationAlgorithm != AIA_ECDH_CURVE_25519_16_BYTE_SHA256 &&
        secretDerivationAlgorithm != AIA_ECDH_CURVE_25519_32_BYTE )
    {
        *errorCode = 1;
        return "unsupported secretDerivationAlgorithm";
    }

    *errorCode =
        mbedtls_ecp_group_load( &ecdhContext->grp, MBEDTLS_ECP_DP_CURVE25519 );
    if( *errorCode )
    {
        return "mbedtls_ecp_group_load failed";
    }

    *errorCode = mbedtls_ecdh_gen_public(
        &ecdhContext->grp, &ecdhContext->d, &ecdhContext->Q,
        mbedtls_ctr_drbg_random, &g_ctrdrbgContext );
    if( *errorCode )
    {
        return "Key pair generation failed";
    }

    /* mbedtls_mpi_write_binary() writes to buffer in big endian. */
    *errorCode =
        mbedtls_mpi_write_binary( &ecdhContext->d, privateKey, privateKeyLen );
    if( *errorCode )
    {
        return "mbedtls_mpi_write_binary for private key failed";
    }

    /* mbedtls_mpi_write_binary() writes to buffer in big endian. */
    *errorCode =
        mbedtls_mpi_write_binary( &ecdhContext->Q.X, publicKey, publicKeyLen );
    if( *errorCode )
    {
        return "mbedtls_mpi_write_binary for public key failed";
    }

    /* Reverse privateKey to convert from big endian to little endian. */
    AiaReverseByteArray( privateKey, privateKeyLen );
    /* Reverse publicKey to convert from big endian to little endian. */
    AiaReverseByteArray( publicKey, publicKeyLen );

    return NULL;
}

static const char *AiaCryptoMbedtls_CalculateSharedSecretInternal(
    const uint8_t *clientPrivateKey, size_t clientPrivateKeyLen,
    const uint8_t *servicePublicKey, size_t servicePublicKeyLen,
    AiaSecretDerivationAlgorithm_t secretDerivationAlgorithm,
    uint8_t *sharedSecret, size_t sharedSecretLen,
    mbedtls_ecdh_context *ecdhContext, int *errorCode )
{
    *errorCode =
        mbedtls_ecp_group_load( &ecdhContext->grp, MBEDTLS_ECP_DP_CURVE25519 );
    if( *errorCode )
    {
        return "mbedtls_ecp_group_load failed";
    }

    /* Reverse clientPrivateKey to convert from little endian to big endian. */
    uint8_t bigEndianClientPrivateKey[ clientPrivateKeyLen ];
    for( size_t i = 0; i < clientPrivateKeyLen; i++ )
    {
        bigEndianClientPrivateKey[ i ] =
            clientPrivateKey[ clientPrivateKeyLen - 1 - i ];
    }

    /* Read private key into ecdhContext. mbedtls_mpi_read_binary() reads big
     * endian data. */
    *errorCode = mbedtls_mpi_read_binary(
        &ecdhContext->d, bigEndianClientPrivateKey, clientPrivateKeyLen );
    if( *errorCode )
    {
        return "mbedtls_mpi_read_binary for client private key failed";
    }

    /* Reverse servicePublicKey to convert from little endian to big endian. */
    uint8_t bigEndianServicePublicKey[ servicePublicKeyLen ];
    for( size_t i = 0; i < servicePublicKeyLen; i++ )
    {
        bigEndianServicePublicKey[ i ] =
            servicePublicKey[ servicePublicKeyLen - 1 - i ];
    }

    /* Read public key into ecdhContext. mbedtls_mpi_read_binary() reads big
     * endian data. */
    *errorCode = mbedtls_mpi_read_binary(
        &ecdhContext->Qp.X, bigEndianServicePublicKey, servicePublicKeyLen );
    if( *errorCode )
    {
        return "mbedtls_mpi_read_binary for service public key failed";
    }

    *errorCode = mbedtls_mpi_lset( &ecdhContext->Qp.Z, 1 );
    if( *errorCode )
    {
        return "mbedtls_mpi_lset failed";
    }

    *errorCode = mbedtls_ecdh_compute_shared(
        &ecdhContext->grp, &ecdhContext->z, &ecdhContext->Qp, &ecdhContext->d,
        mbedtls_ctr_drbg_random, &g_ctrdrbgContext );

    if( *errorCode )
    {
        return "Shared secret calculation failed";
    }

    uint8_t
        sharedSecretBuffer[ AIA_CRYPTO_MBEDTLS_SHARED_SECRET_BUFFER_LENGTH ];

    /* mbedtls_mpi_write_binary() writes to buffer in big endian. */
    *errorCode = mbedtls_mpi_write_binary( &ecdhContext->z, sharedSecretBuffer,
                                           sizeof( sharedSecretBuffer ) );
    if( *errorCode )
    {
        return "mbedtls_mpi_write_binary for shared secret failed";
    }

    /* Reverse sharedSecretBuffer to convert from big endian to little endian.
     */
    AiaReverseByteArray( sharedSecretBuffer, sizeof( sharedSecretBuffer ) );

    size_t outputSharedSecretLen = AiaBytesToHoldBits(
        AiaSecretDerivationAlgorithm_GetKeySize( secretDerivationAlgorithm ) );
    if( outputSharedSecretLen > sizeof( sharedSecretBuffer ) )
    {
        return "Desired shared secret output larger than calculated shared "
               "secret";
    }
    if( sharedSecretLen < outputSharedSecretLen )
    {
        return "Provided sharedSecret buffer not large enough to hold "
               "calculated shared secret.";
    }

    if( secretDerivationAlgorithm == AIA_ECDH_CURVE_25519_32_BYTE )
    {
        memcpy( sharedSecret, sharedSecretBuffer, outputSharedSecretLen );
    }
    else if( secretDerivationAlgorithm == AIA_ECDH_CURVE_25519_16_BYTE_SHA256 )
    {
        const mbedtls_md_info_t *md_info =
            mbedtls_md_info_from_type( MBEDTLS_MD_SHA256 );
        size_t hash_len = mbedtls_md_get_size( md_info );
        uint8_t hashBuffer[ hash_len ];

        *errorCode = mbedtls_hkdf( md_info, NULL, 0, sharedSecretBuffer,
                                   sizeof( sharedSecretBuffer ), NULL, 0,
                                   hashBuffer, hash_len );
        if( *errorCode )
        {
            return "mbedtls_hkdf failed";
        }
        if( hash_len < outputSharedSecretLen )
        {
            return "Desired shared secret output larger than hashed calculated "
                   "shared secret";
        }

        memcpy( sharedSecret, hashBuffer, outputSharedSecretLen );
    }
    else
    {
        *errorCode = 1;
        return "Unsupported secretDerivationAlgorithm.";
    }

    return NULL;
}

bool AiaCryptoMbedtls_Init()
{
    int mbedgcmError = 0;

    if( !AiaMutex( Create )( &gcmMutex, false ) )
    {
        AiaLogError( "AiaMutex( Create ) failed." );
        return NULL;
    }

    /* Initialize context for encryption/decryption functions. */
    mbedtls_gcm_init( &( g_gcmContext ) );

    /* Create entropy and ctr_drbg contexts */
    mbedtls_entropy_init( &( g_entropyContext ) );
    mbedtls_ctr_drbg_init( &( g_ctrdrbgContext ) );

    mbedgcmError = mbedtls_ctr_drbg_seed(
        &( g_ctrdrbgContext ), mbedtls_entropy_func, &( g_entropyContext ),
        (const unsigned char *)AIA_CRYPTO_PERS_DATA,
        sizeof( AIA_CRYPTO_PERS_DATA ) - 1 );
    if( mbedgcmError != 0 )
    {
        AiaCryptoMbedtls_LogMbedtlsError( "Failed to seed RNG", mbedgcmError );
        AiaMutex( Destroy )( &gcmMutex );
        return false;
    }

    return true;
}

bool AiaCryptoMbedtls_SetKey( const uint8_t *encryptKey, size_t encryptKeySize,
                              const AiaEncryptionAlgorithm_t encryptAlgorithm )
{
    int mbedgcmError = 0;

    if( !encryptKey )
    {
        AiaLogError( "Null encryptKey." );
        return false;
    }

    if( !encryptKeySize )
    {
        AiaLogError( "Empty encryptKey." );
        return false;
    }

    size_t sizeInBits = AiaEncryptionAlgorithm_GetKeySize( encryptAlgorithm );
    size_t expectedBytes = AiaBytesToHoldBits( sizeInBits );
    if( encryptKeySize != expectedBytes )
    {
        AiaLogError( "Wrong encryptKeySize (%zu, expected %zu).",
                     encryptKeySize, expectedBytes );
        return false;
    }

    /* Check that this is a supported algorithm. We support AES_GCM_128 and
     * AES_GCM_256. */
    mbedtls_cipher_id_t cipher = MBEDTLS_CIPHER_ID_NONE;
    switch( encryptAlgorithm )
    {
        case AIA_AES_GCM:
            cipher = MBEDTLS_CIPHER_ID_AES;
            break;
    }
    if( MBEDTLS_CIPHER_ID_NONE == cipher )
    {
        AiaLogError( "Unsupported encryptionAlgorithm %s",
                     AiaEncryptionAlgorithm_ToString( encryptAlgorithm ) );
        return false;
    }

    /* Set the key */
    AiaMutex( Lock )( &gcmMutex );

    mbedgcmError =
        mbedtls_gcm_setkey( &( g_gcmContext ), cipher, encryptKey, sizeInBits );

    AiaMutex( Unlock )( &gcmMutex );

    if( mbedgcmError != 0 )
    {
        AiaCryptoMbedtls_LogMbedtlsError( "Failed to set gcm key",
                                          mbedgcmError );
        return false;
    }

    return true;
}

bool AiaCryptoMbedtls_Encrypt( const uint8_t *inputData, const size_t inputLen,
                               uint8_t *outputData, uint8_t *iv, size_t ivLen,
                               uint8_t *tag, const size_t tagLen )
{
    int mbedgcmError = 0;

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

    /* Check that IV is not null */
    if( !iv )
    {
        AiaLogError( "Null IV." );
        return false;
    }

    /* Check the tag is not null */
    if( !tag )
    {
        AiaLogError( "Null tag." );
        return false;
    }

    /* Create the IV */
    mbedgcmError = mbedtls_ctr_drbg_random( &( g_ctrdrbgContext ), iv,
                                            ivLen * sizeof( iv[ 0 ] ) );
    if( mbedgcmError != 0 )
    {
        AiaCryptoMbedtls_LogMbedtlsError( "Failed to create IV", mbedgcmError );
        return false;
    }

    /* Perform the encryption */
    AiaMutex( Lock )( &gcmMutex );

    mbedgcmError = mbedtls_gcm_crypt_and_tag(
        &( g_gcmContext ), MBEDTLS_GCM_ENCRYPT, inputLen, iv, ivLen, NULL, 0,
        inputData, outputData, tagLen, tag );

    AiaMutex( Unlock )( &gcmMutex );

    if( mbedgcmError != 0 )
    {
        AiaCryptoMbedtls_LogMbedtlsError( "Failed to encrypt data",
                                          mbedgcmError );
        return false;
    }

    return true;
}

bool AiaCryptoMbedtls_Decrypt( const uint8_t *inputData, const size_t inputLen,
                               uint8_t *outputData, const uint8_t *iv,
                               const size_t ivLen, const uint8_t *tag,
                               const size_t tagLen )
{
    int mbedgcmError = 0;

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

    /* Check that IV is not null */
    if( !iv )
    {
        AiaLogError( "Null IV." );
        return false;
    }

    /* Check the tag is not null */
    if( !tag )
    {
        AiaLogError( "Null tag." );
        return false;
    }

    /* Perform the decryption */
    AiaMutex( Lock )( &gcmMutex );

    mbedgcmError =
        mbedtls_gcm_auth_decrypt( &( g_gcmContext ), inputLen, iv, ivLen, NULL,
                                  0, tag, tagLen, inputData, outputData );

    AiaMutex( Unlock )( &gcmMutex );

    if( mbedgcmError != 0 )
    {
        AiaCryptoMbedtls_LogMbedtlsError( "Failed to decrypt data",
                                          mbedgcmError );
        return false;
    }

    return true;
}

/**
 * Key pair generation referencing example provided by mbedTLS
 * @see
 * https://github.com/ARMmbed/mbedtls/blob/development/programs/pkey/ecdh_curve25519.c
 */
bool AiaCryptoMbedtls_GenerateKeyPair(
    AiaSecretDerivationAlgorithm_t secretDerivationAlgorithm,
    uint8_t *privateKey, size_t privateKeyLen, uint8_t *publicKey,
    size_t publicKeyLen )
{
    if( !privateKey )
    {
        AiaLogError( "Null privateKey." );
        return false;
    }
    if( !publicKey )
    {
        AiaLogError( "Null publicKey." );
        return false;
    }

    mbedtls_ecdh_context ecdhContext;
    mbedtls_ecdh_init( &( ecdhContext ) );

    int errorCode = 0;
    const char *errorMessage = AiaCryptoMbedtls_GenerateKeyPairInternal(
        secretDerivationAlgorithm, privateKey, privateKeyLen, publicKey,
        publicKeyLen, &ecdhContext, &errorCode );
    if( errorCode )
    {
        AiaCryptoMbedtls_LogMbedtlsError( errorMessage, errorCode );
    }

    mbedtls_ecdh_free( &ecdhContext );
    return !errorCode;
}

/**
 * Shared secret calculation referencing example provided by mbedTLS
 * @see
 * https://github.com/ARMmbed/mbedtls/blob/development/programs/pkey/ecdh_curve25519.c
 */
bool AiaCryptoMbedtls_CalculateSharedSecret(
    const uint8_t *clientPrivateKey, size_t clientPrivateKeyLen,
    const uint8_t *servicePublicKey, size_t servicePublicKeyLen,
    AiaSecretDerivationAlgorithm_t secretDerivationAlgorithm,
    uint8_t *sharedSecret, size_t sharedSecretLen )
{
    if( !clientPrivateKey )
    {
        AiaLogError( "Null clientPrivateKey." );
        return false;
    }
    if( !servicePublicKey )
    {
        AiaLogError( "Null servicePublicKey." );
        return false;
    }

    mbedtls_ecdh_context ecdhContext;
    mbedtls_ecdh_init( &( ecdhContext ) );

    int errorCode = 0;
    const char *errorMessage = AiaCryptoMbedtls_CalculateSharedSecretInternal(
        clientPrivateKey, clientPrivateKeyLen, servicePublicKey,
        servicePublicKeyLen, secretDerivationAlgorithm, sharedSecret,
        sharedSecretLen, &ecdhContext, &errorCode );
    if( errorCode )
    {
        AiaCryptoMbedtls_LogMbedtlsError( errorMessage, errorCode );
    }

    mbedtls_ecdh_free( &ecdhContext );
    return !errorCode;
}

void AiaCryptoMbedtls_Cleanup()
{
    /* Free the contexts for gcm. */
    AiaMutex( Lock )( &gcmMutex );
    mbedtls_gcm_free( &( g_gcmContext ) );
    AiaMutex( Unlock )( &gcmMutex );

    /* Free the RNG contexts */
    mbedtls_entropy_free( &( g_entropyContext ) );
    mbedtls_ctr_drbg_free( &( g_ctrdrbgContext ) );

    AiaMutex( Destroy )( &gcmMutex );
}
