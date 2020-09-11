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
 * @file aia_crypto_mbedtls_tests.c
 * @brief Tests for AiaCryptoMbedtls.
 */

/* The config header is always included first. */
#include <aia_config.h>

#include <aiacore/aia_crypto_mbedtls.h>
#include <aiacore/aia_encryption_algorithm.h>
#include <aiacore/aia_mbedtls_threading.h>
#include <aiacore/aia_message_constants.h>
#include <aiacore/aia_secret_derivation_algorithm.h>

/* Test framework includes. */
#include <unity_fixture.h>

#define TEST_IV_LEN 12
/* Use AES_GCM as the encryption algorithm */
#define TEST_ENCRYPT_ALG AIA_AES_GCM
#define TEST_SECRET_DERIVATION_ALG AIA_ECDH_CURVE_25519_32_BYTE
#define TEST_SECRET_DERIVATION_HASH_ALG AIA_ECDH_CURVE_25519_16_BYTE_SHA256
#define TEST_INVALID_SECRET_DERIVATION_ALG 999
#define TEST_KEY_LEN 32
#define TEST_INPUT_DATA_LEN 64
#define TEST_TAG_LEN 16
#define TEST_GENERATED_KEY_LEN 32
#define TEST_SHARED_SECRET_LEN 32
#define TEST_HASH_SHARED_SECRET_LEN 16

static const unsigned char TEST_ENCRYPT_KEY[ TEST_KEY_LEN ] = {
    0xfe, 0xff, 0xe9, 0x92, 0x86, 0x65, 0x73, 0x1c, 0x6d, 0x6a, 0x8f,
    0x94, 0x67, 0x30, 0x83, 0x08, 0xfe, 0xff, 0xe9, 0x92, 0x86, 0x65,
    0x73, 0x1c, 0x6d, 0x6a, 0x8f, 0x94, 0x67, 0x30, 0x83, 0x08
};

static const unsigned char TEST_INPUT_DATA[ TEST_INPUT_DATA_LEN ] = {
    0xd9, 0x31, 0x32, 0x25, 0xf8, 0x84, 0x06, 0xe5, 0xa5, 0x59, 0x09,
    0xc5, 0xaf, 0xf5, 0x26, 0x9a, 0x86, 0xa7, 0xa9, 0x53, 0x15, 0x34,
    0xf7, 0xda, 0x2e, 0x4c, 0x30, 0x3d, 0x8a, 0x31, 0x8a, 0x72, 0x1c,
    0x3c, 0x0c, 0x95, 0x95, 0x68, 0x09, 0x53, 0x2f, 0xcf, 0x0e, 0x24,
    0x49, 0xa6, 0xb5, 0x25, 0xb1, 0x6a, 0xed, 0xf5, 0xaa, 0x0d, 0xe6,
    0x57, 0xba, 0x63, 0x7b, 0x39, 0x1a, 0xaf, 0xd2, 0x55
};

/*-----------------------------------------------------------*/

/**
 * @brief Test group for AiaCryptoMbedtls_t tests.
 */
TEST_GROUP( AiaCryptoMbedtlsTests );

/*-----------------------------------------------------------*/

/**
 * @brief Test setup for AiaCryptoMbedtls_t tests.
 */
TEST_SETUP( AiaCryptoMbedtlsTests )
{
    AiaMbedtlsThreading_Init();
    TEST_ASSERT_TRUE( AiaCryptoMbedtls_Init() );
    TEST_ASSERT_TRUE( AiaCryptoMbedtls_SetKey( TEST_ENCRYPT_KEY, TEST_KEY_LEN,
                                               TEST_ENCRYPT_ALG ) );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test tear down for AiaCryptoMbedtls_t tests.
 */
TEST_TEAR_DOWN( AiaCryptoMbedtlsTests )
{
    AiaMbedtlsThreading_Cleanup();
    AiaCryptoMbedtls_Cleanup();
}

/*-----------------------------------------------------------*/

/**
 * @brief Test group runner for AiaCryptoMbedtls_t tests.
 */
TEST_GROUP_RUNNER( AiaCryptoMbedtlsTests )
{
    RUN_TEST_CASE( AiaCryptoMbedtlsTests, SetWithoutKey );
    RUN_TEST_CASE( AiaCryptoMbedtlsTests, SetWithIncorrectKeySize );
    RUN_TEST_CASE( AiaCryptoMbedtlsTests, EncryptWithBrokenIV );
    RUN_TEST_CASE( AiaCryptoMbedtlsTests, EncryptWithoutInput );
    RUN_TEST_CASE( AiaCryptoMbedtlsTests, EncryptWithoutOutput );
    RUN_TEST_CASE( AiaCryptoMbedtlsTests, EncryptWithoutTag );
    RUN_TEST_CASE( AiaCryptoMbedtlsTests, EncryptWithInvalidTagLength );
    RUN_TEST_CASE( AiaCryptoMbedtlsTests, EncryptDecryptHappyCase );
    RUN_TEST_CASE( AiaCryptoMbedtlsTests, DecryptWithBrokenIV );
    RUN_TEST_CASE( AiaCryptoMbedtlsTests, DecryptWithInvalidIVLength );
    RUN_TEST_CASE( AiaCryptoMbedtlsTests, DecryptWithoutInput );
    RUN_TEST_CASE( AiaCryptoMbedtlsTests, DecryptWithoutOutput );
    RUN_TEST_CASE( AiaCryptoMbedtlsTests, DecryptWithoutTag );
    RUN_TEST_CASE( AiaCryptoMbedtlsTests, GenerateKeyPairNullKey );
    RUN_TEST_CASE( AiaCryptoMbedtlsTests, GenerateKeyPairInvalidKeyLength );
    RUN_TEST_CASE( AiaCryptoMbedtlsTests,
                   GenerateKeyPairInvalidSecretDerivationAlg );
    RUN_TEST_CASE( AiaCryptoMbedtlsTests, GenerateKeyPair );
    RUN_TEST_CASE( AiaCryptoMbedtlsTests, CalculateSharedSecretNullKey );
    RUN_TEST_CASE( AiaCryptoMbedtlsTests, CalculateSharedSecret );
    RUN_TEST_CASE( AiaCryptoMbedtlsTests, CalculateSharedSecretHash );
}

static bool verifyDecryption( const unsigned char* inputData,
                              unsigned char* decrypted, size_t length )
{
    int diff;
    size_t i;

    for( diff = 0, i = 0; i < length; i++ )
    {
        diff |= inputData[ i ] ^ decrypted[ i ];
    }

    if( !diff )
    {
        return true;
    }
    else
    {
        return false;
    }
}

/*-----------------------------------------------------------*/

TEST( AiaCryptoMbedtlsTests, SetWithoutKey )
{
    TEST_ASSERT_FALSE(
        AiaCryptoMbedtls_SetKey( NULL, TEST_KEY_LEN, TEST_ENCRYPT_ALG ) );
}

TEST( AiaCryptoMbedtlsTests, SetWithIncorrectKeySize )
{
    TEST_ASSERT_FALSE( AiaCryptoMbedtls_SetKey(
        TEST_ENCRYPT_KEY, TEST_KEY_LEN - 1, TEST_ENCRYPT_ALG ) );
    TEST_ASSERT_FALSE(
        AiaCryptoMbedtls_SetKey( TEST_ENCRYPT_KEY, 0, TEST_ENCRYPT_ALG ) );
}

TEST( AiaCryptoMbedtlsTests, EncryptWithBrokenIV )
{
    unsigned char tag[ TEST_TAG_LEN ];
    unsigned char outputBuf[ TEST_INPUT_DATA_LEN ];

    /* Null IV */
    TEST_ASSERT_FALSE( AiaCryptoMbedtls_Encrypt(
        TEST_INPUT_DATA, TEST_INPUT_DATA_LEN, outputBuf, NULL, TEST_IV_LEN, tag,
        TEST_TAG_LEN ) );

    /* Null IV and its length is zero */
    TEST_ASSERT_FALSE( AiaCryptoMbedtls_Encrypt( TEST_INPUT_DATA,
                                                 TEST_INPUT_DATA_LEN, outputBuf,
                                                 NULL, 0, tag, TEST_TAG_LEN ) );

    /* IV is NULL and its length is zero */
    TEST_ASSERT_FALSE( AiaCryptoMbedtls_Encrypt( TEST_INPUT_DATA,
                                                 TEST_INPUT_DATA_LEN, outputBuf,
                                                 NULL, 0, tag, TEST_TAG_LEN ) );
}

TEST( AiaCryptoMbedtlsTests, EncryptWithoutInput )
{
    unsigned char iv[ TEST_IV_LEN ];
    unsigned char tag[ TEST_TAG_LEN ];
    unsigned char outputBuf[ TEST_INPUT_DATA_LEN ];

    /* Null input data and zero input length */
    TEST_ASSERT_TRUE( AiaCryptoMbedtls_Encrypt(
        NULL, 0, outputBuf, iv, TEST_IV_LEN, tag, TEST_TAG_LEN ) );

    /* Null input data and non-zero input length */
    TEST_ASSERT_FALSE( AiaCryptoMbedtls_Encrypt(
        NULL, 1, outputBuf, iv, TEST_IV_LEN, tag, TEST_TAG_LEN ) );

    /* Non-null input data and zero input length */
    TEST_ASSERT_TRUE( AiaCryptoMbedtls_Encrypt(
        TEST_INPUT_DATA, 0, outputBuf, iv, TEST_IV_LEN, tag, TEST_TAG_LEN ) );
}

TEST( AiaCryptoMbedtlsTests, EncryptWithoutOutput )
{
    unsigned char iv[ TEST_IV_LEN ];
    unsigned char tag[ TEST_TAG_LEN ];
    unsigned char outputBuf[ TEST_INPUT_DATA_LEN ];

    /* Null output and zero input length */
    TEST_ASSERT_TRUE( AiaCryptoMbedtls_Encrypt(
        TEST_INPUT_DATA, 0, NULL, iv, TEST_IV_LEN, tag, TEST_TAG_LEN ) );

    /* Null output and non-zero input length */
    TEST_ASSERT_FALSE( AiaCryptoMbedtls_Encrypt(
        TEST_INPUT_DATA, 1, NULL, iv, TEST_IV_LEN, tag, TEST_TAG_LEN ) );

    /* Non-null output and zero input length */
    TEST_ASSERT_TRUE( AiaCryptoMbedtls_Encrypt(
        TEST_INPUT_DATA, 0, outputBuf, iv, TEST_IV_LEN, tag, TEST_TAG_LEN ) );
}

TEST( AiaCryptoMbedtlsTests, EncryptWithoutTag )
{
    unsigned char iv[ TEST_IV_LEN ];
    unsigned char outputBuf[ TEST_INPUT_DATA_LEN ];

    TEST_ASSERT_FALSE( AiaCryptoMbedtls_Encrypt(
        TEST_INPUT_DATA, TEST_INPUT_DATA_LEN, outputBuf, iv, TEST_IV_LEN, NULL,
        TEST_TAG_LEN ) );
}

TEST( AiaCryptoMbedtlsTests, EncryptWithInvalidTagLength )
{
    unsigned char iv[ TEST_IV_LEN ];
    unsigned char tag[ TEST_TAG_LEN ];
    unsigned char outputBuf[ TEST_INPUT_DATA_LEN ];

    TEST_ASSERT_FALSE( AiaCryptoMbedtls_Encrypt( TEST_INPUT_DATA,
                                                 TEST_INPUT_DATA_LEN, outputBuf,
                                                 iv, TEST_IV_LEN, tag, 0 ) );

    TEST_ASSERT_FALSE( AiaCryptoMbedtls_Encrypt( TEST_INPUT_DATA,
                                                 TEST_INPUT_DATA_LEN, outputBuf,
                                                 iv, TEST_IV_LEN, tag, 100 ) );
}

TEST( AiaCryptoMbedtlsTests, EncryptDecryptHappyCase )
{
    unsigned char iv[ TEST_IV_LEN ];
    unsigned char tag[ TEST_TAG_LEN ];
    unsigned char outputBuf[ TEST_INPUT_DATA_LEN ];
    unsigned char decrypted[ TEST_INPUT_DATA_LEN ];

    TEST_ASSERT_TRUE( AiaCryptoMbedtls_Encrypt(
        TEST_INPUT_DATA, TEST_INPUT_DATA_LEN, outputBuf, iv, TEST_IV_LEN, tag,
        TEST_TAG_LEN ) );

    TEST_ASSERT_TRUE( AiaCryptoMbedtls_Decrypt( outputBuf, TEST_INPUT_DATA_LEN,
                                                decrypted, iv, TEST_IV_LEN, tag,
                                                TEST_TAG_LEN ) );

    TEST_ASSERT_TRUE(
        verifyDecryption( TEST_INPUT_DATA, decrypted, TEST_INPUT_DATA_LEN ) );
}

TEST( AiaCryptoMbedtlsTests, DecryptWithBrokenIV )
{
    unsigned char iv[ TEST_IV_LEN ];
    unsigned char tag[ TEST_TAG_LEN ];
    unsigned char outputBuf[ TEST_INPUT_DATA_LEN ];
    unsigned char decrypted[ TEST_INPUT_DATA_LEN ];

    TEST_ASSERT_TRUE( AiaCryptoMbedtls_Encrypt(
        TEST_INPUT_DATA, TEST_INPUT_DATA_LEN, outputBuf, iv, TEST_IV_LEN, tag,
        TEST_TAG_LEN ) );

    /* Null IV */
    TEST_ASSERT_FALSE( AiaCryptoMbedtls_Decrypt( outputBuf, TEST_INPUT_DATA_LEN,
                                                 decrypted, NULL, TEST_IV_LEN,
                                                 tag, TEST_TAG_LEN ) );

    /* IV length is zero */
    TEST_ASSERT_FALSE( AiaCryptoMbedtls_Decrypt(
        outputBuf, TEST_INPUT_DATA_LEN, decrypted, iv, 0, tag, TEST_TAG_LEN ) );

    /* IV is NULL and its length is zero */
    TEST_ASSERT_FALSE( AiaCryptoMbedtls_Decrypt( outputBuf, TEST_INPUT_DATA_LEN,
                                                 decrypted, NULL, 0, tag,
                                                 TEST_TAG_LEN ) );
}

TEST( AiaCryptoMbedtlsTests, DecryptWithInvalidIVLength )
{
    unsigned char iv[ TEST_IV_LEN ];
    unsigned char tag[ TEST_TAG_LEN ];
    unsigned char outputBuf[ TEST_INPUT_DATA_LEN ];
    unsigned char decrypted[ TEST_INPUT_DATA_LEN ];

    TEST_ASSERT_TRUE( AiaCryptoMbedtls_Encrypt(
        TEST_INPUT_DATA, TEST_INPUT_DATA_LEN, outputBuf, iv, TEST_IV_LEN, tag,
        TEST_TAG_LEN ) );

    /* IV length is zero */
    TEST_ASSERT_FALSE( AiaCryptoMbedtls_Decrypt(
        outputBuf, TEST_INPUT_DATA_LEN, decrypted, iv, 0, tag, TEST_TAG_LEN ) );
}

TEST( AiaCryptoMbedtlsTests, DecryptWithoutInput )
{
    unsigned char iv[ TEST_IV_LEN ];
    unsigned char tag[ TEST_TAG_LEN ];
    unsigned char outputBuf[ TEST_INPUT_DATA_LEN ];
    unsigned char decrypted[ TEST_INPUT_DATA_LEN ];

    TEST_ASSERT_TRUE( AiaCryptoMbedtls_Encrypt(
        TEST_INPUT_DATA, TEST_INPUT_DATA_LEN, outputBuf, iv, TEST_IV_LEN, tag,
        TEST_TAG_LEN ) );

    /* Null input data and zero input length */
    TEST_ASSERT_FALSE( AiaCryptoMbedtls_Decrypt(
        NULL, 0, decrypted, iv, TEST_IV_LEN, tag, TEST_TAG_LEN ) );

    /* Null input data and non-zero input length. No issues with the arguments;
     * but, the decryption itself fails. */
    TEST_ASSERT_FALSE( AiaCryptoMbedtls_Decrypt(
        NULL, 1, decrypted, iv, TEST_IV_LEN, tag, TEST_TAG_LEN ) );

    /* Non-null input data and zero input length. No issues with the arguments;
     * but, the decryption itself fails. */
    TEST_ASSERT_FALSE( AiaCryptoMbedtls_Decrypt(
        outputBuf, 0, decrypted, iv, TEST_IV_LEN, tag, TEST_TAG_LEN ) );
}

TEST( AiaCryptoMbedtlsTests, DecryptWithoutOutput )
{
    unsigned char iv[ TEST_IV_LEN ];
    unsigned char tag[ TEST_TAG_LEN ];
    unsigned char outputBuf[ TEST_INPUT_DATA_LEN ];
    unsigned char decrypted[ TEST_INPUT_DATA_LEN ];

    TEST_ASSERT_TRUE( AiaCryptoMbedtls_Encrypt(
        TEST_INPUT_DATA, TEST_INPUT_DATA_LEN, outputBuf, iv, TEST_IV_LEN, tag,
        TEST_TAG_LEN ) );

    /* Null output and zero input length */
    TEST_ASSERT_FALSE( AiaCryptoMbedtls_Decrypt(
        outputBuf, 0, NULL, iv, TEST_IV_LEN, tag, TEST_TAG_LEN ) );

    /* Null output and non-zero input length. No issues with the arguments; but,
     * the decryption itself fails. */
    TEST_ASSERT_FALSE( AiaCryptoMbedtls_Decrypt(
        outputBuf, 1, NULL, iv, TEST_IV_LEN, tag, TEST_TAG_LEN ) );

    /* Non-null output and zero input length. No issues with the arguments; but,
     * the decryption itself fails. */
    TEST_ASSERT_FALSE( AiaCryptoMbedtls_Decrypt(
        outputBuf, 0, decrypted, iv, TEST_IV_LEN, tag, TEST_TAG_LEN ) );
}

TEST( AiaCryptoMbedtlsTests, DecryptWithoutTag )
{
    unsigned char iv[ TEST_IV_LEN ];
    unsigned char tag[ TEST_TAG_LEN ];
    unsigned char outputBuf[ TEST_INPUT_DATA_LEN ];
    unsigned char decrypted[ TEST_INPUT_DATA_LEN ];

    TEST_ASSERT_TRUE( AiaCryptoMbedtls_Encrypt(
        TEST_INPUT_DATA, TEST_INPUT_DATA_LEN, outputBuf, iv, TEST_IV_LEN, tag,
        TEST_TAG_LEN ) );

    TEST_ASSERT_FALSE( AiaCryptoMbedtls_Decrypt( outputBuf, TEST_INPUT_DATA_LEN,
                                                 decrypted, iv, TEST_IV_LEN,
                                                 NULL, TEST_TAG_LEN ) );
}

TEST( AiaCryptoMbedtlsTests, DecryptWithInvalidTagLength )
{
    unsigned char iv[ TEST_IV_LEN ];
    unsigned char tag[ TEST_TAG_LEN ];
    unsigned char outputBuf[ TEST_INPUT_DATA_LEN ];
    unsigned char decrypted[ TEST_INPUT_DATA_LEN ];

    TEST_ASSERT_TRUE( AiaCryptoMbedtls_Encrypt(
        TEST_INPUT_DATA, TEST_INPUT_DATA_LEN, outputBuf, iv, TEST_IV_LEN, tag,
        TEST_TAG_LEN ) );

    TEST_ASSERT_FALSE( AiaCryptoMbedtls_Decrypt(
        outputBuf, TEST_INPUT_DATA_LEN, decrypted, iv, TEST_IV_LEN, tag, 0 ) );

    TEST_ASSERT_FALSE( AiaCryptoMbedtls_Decrypt( outputBuf, TEST_INPUT_DATA_LEN,
                                                 decrypted, iv, TEST_IV_LEN,
                                                 tag, 100 ) );
}

TEST( AiaCryptoMbedtlsTests, GenerateKeyPairNullKey )
{
    uint8_t privateKey[ TEST_GENERATED_KEY_LEN ];
    uint8_t publicKey[ TEST_GENERATED_KEY_LEN ];

    TEST_ASSERT_FALSE( AiaCryptoMbedtls_GenerateKeyPair(
        TEST_SECRET_DERIVATION_ALG, NULL, sizeof( privateKey ), publicKey,
        sizeof( publicKey ) ) );
    TEST_ASSERT_FALSE( AiaCryptoMbedtls_GenerateKeyPair(
        TEST_SECRET_DERIVATION_ALG, privateKey, sizeof( privateKey ), NULL,
        sizeof( publicKey ) ) );
}

TEST( AiaCryptoMbedtlsTests, GenerateKeyPairInvalidKeyLength )
{
    uint8_t privateKey[ TEST_GENERATED_KEY_LEN ];
    uint8_t publicKey[ TEST_GENERATED_KEY_LEN ];

    TEST_ASSERT_FALSE( AiaCryptoMbedtls_GenerateKeyPair(
        TEST_SECRET_DERIVATION_ALG, privateKey, 1, publicKey,
        sizeof( publicKey ) ) );
    TEST_ASSERT_FALSE( AiaCryptoMbedtls_GenerateKeyPair(
        TEST_SECRET_DERIVATION_ALG, privateKey, sizeof( privateKey ), publicKey,
        1 ) );
}

TEST( AiaCryptoMbedtlsTests, GenerateKeyPairInvalidSecretDerivationAlg )
{
    uint8_t privateKey[ TEST_GENERATED_KEY_LEN ];
    uint8_t publicKey[ TEST_GENERATED_KEY_LEN ];

    TEST_ASSERT_FALSE( AiaCryptoMbedtls_GenerateKeyPair(
        TEST_INVALID_SECRET_DERIVATION_ALG, privateKey, sizeof( privateKey ),
        publicKey, sizeof( publicKey ) ) );
}

TEST( AiaCryptoMbedtlsTests, GenerateKeyPair )
{
    uint8_t privateKey[ TEST_GENERATED_KEY_LEN ];
    uint8_t publicKey[ TEST_GENERATED_KEY_LEN ];

    TEST_ASSERT_TRUE( AiaCryptoMbedtls_GenerateKeyPair(
        TEST_SECRET_DERIVATION_ALG, privateKey, sizeof( privateKey ), publicKey,
        sizeof( publicKey ) ) );
}

TEST( AiaCryptoMbedtlsTests, CalculateSharedSecretNullKey )
{
    uint8_t privateKey[ TEST_GENERATED_KEY_LEN ];
    uint8_t publicKey[ TEST_GENERATED_KEY_LEN ];

    TEST_ASSERT_TRUE( AiaCryptoMbedtls_GenerateKeyPair(
        TEST_SECRET_DERIVATION_ALG, privateKey, sizeof( privateKey ), publicKey,
        sizeof( publicKey ) ) );

    uint8_t sharedSecret[ TEST_SHARED_SECRET_LEN ];

    TEST_ASSERT_FALSE( AiaCryptoMbedtls_CalculateSharedSecret(
        privateKey, sizeof( privateKey ), NULL, sizeof( publicKey ),
        TEST_SECRET_DERIVATION_ALG, sharedSecret, sizeof( sharedSecret ) ) );
    TEST_ASSERT_FALSE( AiaCryptoMbedtls_CalculateSharedSecret(
        NULL, sizeof( privateKey ), publicKey, sizeof( publicKey ),
        TEST_SECRET_DERIVATION_ALG, sharedSecret, sizeof( sharedSecret ) ) );
}

TEST( AiaCryptoMbedtlsTests, CalculateSharedSecret )
{
    uint8_t clientPrivateKey[ TEST_GENERATED_KEY_LEN ];
    uint8_t clientPublicKey[ TEST_GENERATED_KEY_LEN ];

    TEST_ASSERT_TRUE( AiaCryptoMbedtls_GenerateKeyPair(
        TEST_SECRET_DERIVATION_ALG, clientPrivateKey,
        sizeof( clientPrivateKey ), clientPublicKey,
        sizeof( clientPublicKey ) ) );

    uint8_t servicePrivateKey[ TEST_GENERATED_KEY_LEN ];
    uint8_t servicePublicKey[ TEST_GENERATED_KEY_LEN ];

    TEST_ASSERT_TRUE( AiaCryptoMbedtls_GenerateKeyPair(
        TEST_SECRET_DERIVATION_ALG, servicePrivateKey,
        sizeof( servicePrivateKey ), servicePublicKey,
        sizeof( servicePublicKey ) ) );

    uint8_t clientSharedSecret[ TEST_SHARED_SECRET_LEN ];
    uint8_t serviceSharedSecret[ TEST_SHARED_SECRET_LEN ];

    TEST_ASSERT_TRUE( AiaCryptoMbedtls_CalculateSharedSecret(
        clientPrivateKey, sizeof( clientPrivateKey ), servicePublicKey,
        sizeof( servicePublicKey ), TEST_SECRET_DERIVATION_ALG,
        clientSharedSecret, sizeof( clientSharedSecret ) ) );
    TEST_ASSERT_TRUE( AiaCryptoMbedtls_CalculateSharedSecret(
        servicePrivateKey, sizeof( servicePrivateKey ), clientPublicKey,
        sizeof( clientPublicKey ), TEST_SECRET_DERIVATION_ALG,
        serviceSharedSecret, sizeof( serviceSharedSecret ) ) );

    TEST_ASSERT_EQUAL( memcmp( clientSharedSecret, serviceSharedSecret,
                               TEST_SHARED_SECRET_LEN ),
                       0 );
}

TEST( AiaCryptoMbedtlsTests, CalculateSharedSecretHash )
{
    uint8_t clientPrivateKey[ TEST_GENERATED_KEY_LEN ];
    uint8_t clientPublicKey[ TEST_GENERATED_KEY_LEN ];

    TEST_ASSERT_TRUE( AiaCryptoMbedtls_GenerateKeyPair(
        TEST_SECRET_DERIVATION_HASH_ALG, clientPrivateKey,
        sizeof( clientPrivateKey ), clientPublicKey,
        sizeof( clientPublicKey ) ) );

    uint8_t servicePrivateKey[ TEST_GENERATED_KEY_LEN ];
    uint8_t servicePublicKey[ TEST_GENERATED_KEY_LEN ];

    TEST_ASSERT_TRUE( AiaCryptoMbedtls_GenerateKeyPair(
        TEST_SECRET_DERIVATION_HASH_ALG, servicePrivateKey,
        sizeof( servicePrivateKey ), servicePublicKey,
        sizeof( servicePublicKey ) ) );

    uint8_t clientSharedSecret[ TEST_HASH_SHARED_SECRET_LEN ];
    uint8_t serviceSharedSecret[ TEST_HASH_SHARED_SECRET_LEN ];

    TEST_ASSERT_TRUE( AiaCryptoMbedtls_CalculateSharedSecret(
        clientPrivateKey, sizeof( clientPrivateKey ), servicePublicKey,
        sizeof( servicePublicKey ), TEST_SECRET_DERIVATION_HASH_ALG,
        clientSharedSecret, sizeof( clientSharedSecret ) ) );
    TEST_ASSERT_TRUE( AiaCryptoMbedtls_CalculateSharedSecret(
        servicePrivateKey, sizeof( servicePrivateKey ), clientPublicKey,
        sizeof( clientPublicKey ), TEST_SECRET_DERIVATION_HASH_ALG,
        serviceSharedSecret, sizeof( serviceSharedSecret ) ) );

    TEST_ASSERT_EQUAL( memcmp( clientSharedSecret, serviceSharedSecret,
                               TEST_HASH_SHARED_SECRET_LEN ),
                       0 );
}

/*-----------------------------------------------------------*/
