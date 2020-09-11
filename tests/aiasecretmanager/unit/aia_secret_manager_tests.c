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
 * @file aia_secret_manager_tests.c
 * @brief Tests for AiaSecretManager_t.
 */

/* The config header is always included first. */
#include <aia_config.h>

#include <aiamockregulator/aia_mock_regulator.h>
#include <aiatestutilities/aia_test_utilities.h>

#include <aiacore/aia_events.h>
#include <aiacore/aia_json_message.h>
#include <aiacore/aia_json_utils.h>
#include <aiacore/aia_mbedtls_threading.h>
#include <aiacore/aia_random_mbedtls.h>

#include <aiasecretmanager/aia_secret_manager.h>
#include <aiasecretmanager/private/aia_secret_manager.h>

/* Test framework includes. */
#include <unity_fixture.h>

#include <inttypes.h>
#include <string.h>

/** Information about a message that was emitted. */
typedef struct AiaMockNextSequenceNumberGetter
{
    /** The sequence number to set on calls to @c AiaMockGetNextSequenceNumber.
     */
    AiaSequenceNumber_t sequenceNumberToReturn;

    /** The value to return on calls to @c AiaMockGetNextSequenceNumber. */
    bool valueToReturn;
} AiaMockNextSequenceNumberGetter_t;

/**
 * @copydoc AiaEmitter_GetNextSequenceNumber()
 */
static bool AiaMockGetNextSequenceNumber(
    AiaTopic_t topic, AiaSequenceNumber_t* nextSequenceNumber, void* userData )
{
    (void)topic;
    TEST_ASSERT_NOT_NULL( nextSequenceNumber );
    TEST_ASSERT_NOT_NULL( userData );
    AiaMockNextSequenceNumberGetter_t* getter =
        (AiaMockNextSequenceNumberGetter_t*)userData;
    *nextSequenceNumber = getter->sequenceNumberToReturn;
    return getter->valueToReturn;
}

/**
 * @copydoc AiaRegulator_Write()
 */
static bool AiaMockEmitEvent( AiaRegulatorChunk_t* chunk, void* userData )
{
    TEST_ASSERT_NOT_NULL( userData );
    AiaRegulator_t* regulator = (AiaRegulator_t*)userData;
    return AiaRegulator_Write( regulator, chunk );
}

/** Mock type used to persist secrets (in memory). */
typedef struct AiaMockSecretStorer
{
    /** Mock stored secret. */
    uint8_t* storedSecret;

    /** Mock stored secret length. */
    size_t storedSecretLen;

    /** Value to return on calls to @c AiaStoreSecret and @c AiaLoadSecret. */
    bool valueToReturn;
} AiaMockSecretStorer_t;

/** Mock type used to intercept calls to @c AiaCrypto_SetKey. */
typedef struct AiaMockEncryptor
{
    /** Encryption key set by a call to @c AiaCrypto_SetKey. */
    uint8_t* encryptKey;

    /** Size set by a call to @c AiaCrypto_SetKey. */
    size_t encryptKeySize;

    /** Encryption algorithm set by a call to @c AiaCrypto_SetKey. */
    AiaEncryptionAlgorithm_t encryptAlgorithm;

    /** Value to return upon calls to @c AiaCrypto_SetKey. */
    bool valueToReturn;
} AiaMockEncryptor_t;

/** Object used to mock the getting of outbound sequence numbers. */
static AiaMockNextSequenceNumberGetter_t testSequenceNumberGetter;

/** Object used to mock the emission of events. */
static AiaMockRegulator_t* g_mockEventRegulator;

/** Object used to mock the persistent storage of secrets. */
AiaMockSecretStorer_t* testSecretStorer;

/** Object used to mock the setting of encryption keys. */
static AiaMockEncryptor_t* testMockEncryptor;

/** A test initial shared secret stored using @c testSecretStorer. */
static const uint8_t INITIAL_SHARED_SECRET[ 32 ] = { 0x08 };

/** The secret manager to test. */
static AiaSecretManager_t* g_testSecretManager;

bool AiaStoreSecret( const uint8_t* sharedSecret, size_t size )
{
    TEST_ASSERT_NOT_NULL( testSecretStorer );
    testSecretStorer->storedSecret = AiaCalloc( 1, size );
    TEST_ASSERT_NOT_NULL( testSecretStorer->storedSecret );
    memcpy( testSecretStorer->storedSecret, sharedSecret, size );
    testSecretStorer->storedSecretLen = size;
    return testSecretStorer->valueToReturn;
}

bool AiaLoadSecret( uint8_t* sharedSecret, size_t size )
{
    TEST_ASSERT_NOT_NULL( testSecretStorer );
    TEST_ASSERT_NOT_NULL( testSecretStorer->storedSecret );
    TEST_ASSERT_EQUAL( size, testSecretStorer->storedSecretLen );
    memcpy( sharedSecret, testSecretStorer->storedSecret, size );
    return testSecretStorer->valueToReturn;
}

bool AiaCrypto_SetKey( const uint8_t* encryptKey, size_t encryptKeySize,
                       const AiaEncryptionAlgorithm_t encryptAlgorithm )
{
    testMockEncryptor->encryptKey = AiaCalloc( 1, encryptKeySize );
    TEST_ASSERT_NOT_NULL( testMockEncryptor->encryptKey );
    memcpy( testMockEncryptor->encryptKey, encryptKey, encryptKeySize );
    testMockEncryptor->encryptKeySize = encryptKeySize;
    testMockEncryptor->encryptAlgorithm = encryptAlgorithm;
    return testMockEncryptor->valueToReturn;
}

/**
 * Used to pull a message out of the @c g_mockEventRegulator and assert that it
 * is an @c AIA_EVENTS_SECRET_ROTATED event
 *
 * @param[out] eventSequenceNumber This will be set to the @c
 * AIA_ROTATE_SECRET_EVENT_SEQUENCE_NUMBER_KEY found in the payload.
 * @param[out] microphoneSequenceNumber This will be set to the @c
 * AIA_ROTATE_SECRET_MICROPHONE_SEQUENCE_NUMBER_KEY found in the payload.
 */
static void TestSecretRotatedIsGenerated(
    AiaSequenceNumber_t* eventSequenceNumber,
    AiaSequenceNumber_t* microphoneSequenceNumber )
{
    TEST_ASSERT_TRUE(
        AiaSemaphore( TryWait )( &g_mockEventRegulator->writeSemaphore ) );
    AiaListDouble( Link_t )* link = NULL;
    link = AiaListDouble( PeekHead )( &g_mockEventRegulator->writtenMessages );
    AiaListDouble( RemoveHead )( &g_mockEventRegulator->writtenMessages );
    TEST_ASSERT_NOT_NULL( link );
    AiaJsonMessage_t* exceptionEncounteredMessage = AiaJsonMessage_FromMessage(
        ( (AiaMockRegulatorWrittenMessage_t*)link )->chunk );
    TEST_ASSERT_EQUAL(
        strcmp( AiaJsonMessage_GetName( exceptionEncounteredMessage ),
                AIA_EVENTS_SECRET_ROTATED ),
        0 );
    const char* payload =
        AiaJsonMessage_GetJsonPayload( exceptionEncounteredMessage );
    TEST_ASSERT_NOT_NULL( payload );

    const char* eventSequenceNumberStr = NULL;
    size_t eventSequenceNumberStrLen = 0;
    if( AiaFindJsonValue( payload, strlen( payload ),
                          AIA_ROTATE_SECRET_EVENT_SEQUENCE_NUMBER_KEY,
                          strlen( AIA_ROTATE_SECRET_EVENT_SEQUENCE_NUMBER_KEY ),
                          &eventSequenceNumberStr,
                          &eventSequenceNumberStrLen ) )
    {
        AiaJsonLongType extractedEventSequenceNumber = 0;
        AiaExtractLongFromJsonValue( eventSequenceNumberStr,
                                     eventSequenceNumberStrLen,
                                     &extractedEventSequenceNumber );
        *eventSequenceNumber = extractedEventSequenceNumber;
    }

    const char* microphoneSequenceNumberStr = NULL;
    size_t microphoneSequenceNumberStrLen = 0;
    if( AiaFindJsonValue(
            payload, strlen( payload ),
            AIA_ROTATE_SECRET_MICROPHONE_SEQUENCE_NUMBER_KEY,
            strlen( AIA_ROTATE_SECRET_MICROPHONE_SEQUENCE_NUMBER_KEY ),
            &microphoneSequenceNumberStr, &microphoneSequenceNumberStrLen ) )
    {
        AiaJsonLongType extractedMicrophoneSequenceNumber = 0;
        AiaExtractLongFromJsonValue( microphoneSequenceNumberStr,
                                     microphoneSequenceNumberStrLen,
                                     &extractedMicrophoneSequenceNumber );
        *microphoneSequenceNumber = extractedMicrophoneSequenceNumber;
    }
}

/**
 * Generates an @c AIA_DIRECTIVE_ROTATE_SECRET payload.
 *
 * @param newSecret The base64 encoded @c AIA_ROTATE_SECRET_NEW_SECRET_KEY
 * value.
 * @param newSecretLength Length of @c newSecret.
 * @param directiveSequenceNumber The @c
 * AIA_ROTATE_SECRET_DIRECTIVE_SEQUENCE_NUMBER_KEY value.
 * @param speakerSequenceNumber The @c
 * AIA_ROTATE_SECRET_SPEAKER_SEQUENCE_NUMBER_KEY value.
 * @return A null-terminated payload.
 * @note Callers must clean up non-NULL returned values using AiaFree().
 */
static char* generateRotateSecret( uint8_t* newSecret, size_t newSecretLength,
                                   AiaSequenceNumber_t directiveSequenceNumber,
                                   AiaSequenceNumber_t speakerSequenceNumber )
{
    /* clang-format off */
    static const char* formatPayload =
        "{"
            "\""AIA_ROTATE_SECRET_NEW_SECRET_KEY"\":\"%.*s\","
            "\""AIA_ROTATE_SECRET_DIRECTIVE_SEQUENCE_NUMBER_KEY"\":%"PRIu32","
            "\""AIA_ROTATE_SECRET_SPEAKER_SEQUENCE_NUMBER_KEY"\":%"PRIu32
        "}";
    /* clang-format on */

    int numCharsRequired =
        snprintf( NULL, 0, formatPayload, newSecretLength, newSecret,
                  directiveSequenceNumber, speakerSequenceNumber );
    TEST_ASSERT_GREATER_THAN( 0, numCharsRequired );
    char* fullPayloadBuffer = AiaCalloc( 1, numCharsRequired + 1 );
    TEST_ASSERT_NOT_NULL( fullPayloadBuffer );
    TEST_ASSERT_EQUAL(
        numCharsRequired,
        snprintf( fullPayloadBuffer, numCharsRequired + 1, formatPayload,
                  newSecretLength, newSecret, directiveSequenceNumber,
                  speakerSequenceNumber ) );
    return fullPayloadBuffer;
}

/*-----------------------------------------------------------*/

/**
 * @brief Test group for AiaSecretManager_t tests.
 */
TEST_GROUP( AiaSecretManagerTests );

/*-----------------------------------------------------------*/

/**
 * @brief Test group runner for AiaSecretManager_t tests.
 */
TEST_GROUP_RUNNER( AiaSecretManagerTests )
{
    RUN_TEST_CASE( AiaSecretManagerTests, Creation );
    RUN_TEST_CASE( AiaSecretManagerTests, BadRotateSecretDirectiveHandling );
    RUN_TEST_CASE( AiaSecretManagerTests, GetNextSequenceNumberFails );
    RUN_TEST_CASE( AiaSecretManagerTests, SecretStorageFails );
    RUN_TEST_CASE( AiaSecretManagerTests, SecretRotatedIsSent );
    RUN_TEST_CASE( AiaSecretManagerTests, TestAppropriateKeysAreSet );
}

/*-----------------------------------------------------------*/

/*-----------------------------------------------------------*/
/**
 * @brief Test setup for AiaSecretManager_t tests.
 */
TEST_SETUP( AiaSecretManagerTests )
{
    /** Sample Seed. */
    static const char* TEST_SALT = "TestSalt";
    static const size_t TEST_SALT_LENGTH = sizeof( TEST_SALT ) - 1;
    AiaMbedtlsThreading_Init();
    AiaRandomMbedtls_Init();
    TEST_ASSERT_TRUE( AiaRandomMbedtls_Seed( TEST_SALT, TEST_SALT_LENGTH ) );

    g_mockEventRegulator = AiaMockRegulator_Create();
    TEST_ASSERT_NOT_NULL( g_mockEventRegulator );
    testSecretStorer = AiaCalloc( 1, sizeof( AiaMockSecretStorer_t ) );
    TEST_ASSERT_NOT_NULL( testSecretStorer );
    testMockEncryptor = AiaCalloc( 1, sizeof( AiaMockEncryptor_t ) );
    TEST_ASSERT_NOT_NULL( testMockEncryptor );

    testSequenceNumberGetter.sequenceNumberToReturn = 0;
    testSequenceNumberGetter.valueToReturn = true;
    testSecretStorer->valueToReturn = true;
    testMockEncryptor->valueToReturn = true;

    /* Set up initial storage to mimic registration flow. */
    TEST_ASSERT_TRUE(
        AiaStoreSecret( INITIAL_SHARED_SECRET,
                        AiaBytesToHoldBits( AiaEncryptionAlgorithm_GetKeySize(
                            AiaSecretDerivationAlgorithm_ToEncryptionAlgorithm(
                                SECRET_DERIVATION_ALGORITHM ) ) ) ) );

    g_testSecretManager = AiaSecretManager_Create(
        AiaMockGetNextSequenceNumber, &testSequenceNumberGetter,
        AiaMockEmitEvent, g_mockEventRegulator );
    TEST_ASSERT_NOT_NULL( g_testSecretManager );

    /* Key should be set to what was loaded out of storage for secret manager
     * from setup function. */
    TEST_ASSERT_EQUAL_MEMORY( testSecretStorer->storedSecret,
                              testMockEncryptor->encryptKey,
                              testSecretStorer->storedSecretLen );
}

/*-----------------------------------------------------------*/

/**
 * @brief Test tear down for AiaSecretManager_t tests.
 */
TEST_TEAR_DOWN( AiaSecretManagerTests )
{
    AiaSecretManager_Destroy( g_testSecretManager );

    AiaFree( testMockEncryptor );
    AiaFree( testSecretStorer );
    AiaMockRegulator_Destroy( g_mockEventRegulator,
                              AiaTestUtilities_DestroyJsonChunk, NULL );

    AiaMbedtlsThreading_Cleanup();
    AiaRandomMbedtls_Cleanup();
}

/*-----------------------------------------------------------*/

TEST( AiaSecretManagerTests, Creation )
{
    /* Test bad parameters */
    TEST_ASSERT_NULL( AiaSecretManager_Create( NULL, &testSequenceNumberGetter,
                                               AiaMockEmitEvent,
                                               g_mockEventRegulator ) );
    TEST_ASSERT_NULL( AiaSecretManager_Create( AiaMockGetNextSequenceNumber,
                                               &testSequenceNumberGetter, NULL,
                                               g_mockEventRegulator ) );

    /* Creation should fail since secret loading fails. */
    testSecretStorer->valueToReturn = false;
    testMockEncryptor->valueToReturn = true;
    TEST_ASSERT_NULL( AiaSecretManager_Create(
        AiaMockGetNextSequenceNumber, &testSequenceNumberGetter,
        AiaMockEmitEvent, g_mockEventRegulator ) );

    /* Creation should fail initial key setting fails. */
    testSecretStorer->valueToReturn = true;
    testMockEncryptor->valueToReturn = false;
    TEST_ASSERT_NULL( AiaSecretManager_Create(
        AiaMockGetNextSequenceNumber, &testSequenceNumberGetter,
        AiaMockEmitEvent, g_mockEventRegulator ) );

    testSecretStorer->valueToReturn = true;
    testMockEncryptor->valueToReturn = true;
    AiaSecretManager_t* secretManager = AiaSecretManager_Create(
        AiaMockGetNextSequenceNumber, &testSequenceNumberGetter,
        AiaMockEmitEvent, g_mockEventRegulator );
    TEST_ASSERT_NOT_NULL( secretManager );

    AiaSecretManager_Destroy( secretManager );
}

TEST( AiaSecretManagerTests, BadRotateSecretDirectiveHandling )
{
    /* clang-format off */
    static const char* ROTATE_SECRET_WITHOUT_NEW_SECRET = 
    "{"
        "\""AIA_ROTATE_SECRET_DIRECTIVE_SEQUENCE_NUMBER_KEY"\":100,"
        "\""AIA_ROTATE_SECRET_SPEAKER_SEQUENCE_NUMBER_KEY"\":100"
    "}";
    /* clang-format on */
    AiaSequenceNumber_t TEST_SEQUENCE_NUMBER = 4;
    size_t TEST_INDEX = 44;
    AiaSecretManager_OnRotateSecretDirectiveReceived(
        g_testSecretManager, (void*)ROTATE_SECRET_WITHOUT_NEW_SECRET,
        strlen( ROTATE_SECRET_WITHOUT_NEW_SECRET ), TEST_SEQUENCE_NUMBER,
        TEST_INDEX );
    AiaTestUtilities_TestMalformedMessageExceptionIsGenerated(
        g_mockEventRegulator, TEST_SEQUENCE_NUMBER, TEST_INDEX );

    /* New secret is not a properly formatted json string with quotes. */
    /* clang-format off */
    static const char* ROTATE_SECRET_WITH_BAD_NEW_SECRET = 
    "{"
        "\""AIA_ROTATE_SECRET_NEW_SECRET_KEY"\":100,"
        "\""AIA_ROTATE_SECRET_DIRECTIVE_SEQUENCE_NUMBER_KEY"\":100,"
        "\""AIA_ROTATE_SECRET_SPEAKER_SEQUENCE_NUMBER_KEY"\":100"
    "}";
    /* clang-format on */
    AiaSecretManager_OnRotateSecretDirectiveReceived(
        g_testSecretManager, (void*)ROTATE_SECRET_WITH_BAD_NEW_SECRET,
        strlen( ROTATE_SECRET_WITH_BAD_NEW_SECRET ), TEST_SEQUENCE_NUMBER,
        TEST_INDEX );
    AiaTestUtilities_TestMalformedMessageExceptionIsGenerated(
        g_mockEventRegulator, TEST_SEQUENCE_NUMBER, TEST_INDEX );

    /* clang-format off */
    static const char* ROTATE_SECRET_WITHOUT_DIRECTIVE_SEQUENCE_NUMBER = 
    "{"
        "\""AIA_ROTATE_SECRET_NEW_SECRET_KEY"\": \"123\","
        "\""AIA_ROTATE_SECRET_SPEAKER_SEQUENCE_NUMBER_KEY"\":100"
    "}";
    /* clang-format on */
    AiaSecretManager_OnRotateSecretDirectiveReceived(
        g_testSecretManager,
        (void*)ROTATE_SECRET_WITHOUT_DIRECTIVE_SEQUENCE_NUMBER,
        strlen( ROTATE_SECRET_WITHOUT_DIRECTIVE_SEQUENCE_NUMBER ),
        TEST_SEQUENCE_NUMBER, TEST_INDEX );
    AiaTestUtilities_TestMalformedMessageExceptionIsGenerated(
        g_mockEventRegulator, TEST_SEQUENCE_NUMBER, TEST_INDEX );

    /* clang-format off */
    static const char* ROTATE_SECRET_WITHOUT_SPEAKER_SEQUENCE_NUMBER = 
    "{"
        "\""AIA_ROTATE_SECRET_NEW_SECRET_KEY"\": \"123\","
        "\""AIA_ROTATE_SECRET_DIRECTIVE_SEQUENCE_NUMBER_KEY"\":100"
    "}";
    /* clang-format on */
    AiaSecretManager_OnRotateSecretDirectiveReceived(
        g_testSecretManager,
        (void*)ROTATE_SECRET_WITHOUT_SPEAKER_SEQUENCE_NUMBER,
        strlen( ROTATE_SECRET_WITHOUT_SPEAKER_SEQUENCE_NUMBER ),
        TEST_SEQUENCE_NUMBER, TEST_INDEX );
    AiaTestUtilities_TestMalformedMessageExceptionIsGenerated(
        g_mockEventRegulator, TEST_SEQUENCE_NUMBER, TEST_INDEX );

    /* clang-format off */
    static const char* ROTATE_SECRET_WITH_BAD_NEW_DIRECTIVE_SEQUENCE_NUMBER = 
    "{"
        "\""AIA_ROTATE_SECRET_NEW_SECRET_KEY"\": \"123\","
        "\""AIA_ROTATE_SECRET_DIRECTIVE_SEQUENCE_NUMBER_KEY"\":\"abc\","
        "\""AIA_ROTATE_SECRET_SPEAKER_SEQUENCE_NUMBER_KEY"\":100"
    "}";
    /* clang-format on */
    AiaSecretManager_OnRotateSecretDirectiveReceived(
        g_testSecretManager,
        (void*)ROTATE_SECRET_WITH_BAD_NEW_DIRECTIVE_SEQUENCE_NUMBER,
        strlen( ROTATE_SECRET_WITH_BAD_NEW_DIRECTIVE_SEQUENCE_NUMBER ),
        TEST_SEQUENCE_NUMBER, TEST_INDEX );
    AiaTestUtilities_TestMalformedMessageExceptionIsGenerated(
        g_mockEventRegulator, TEST_SEQUENCE_NUMBER, TEST_INDEX );

    /* clang-format off */
    static const char* ROTATE_SECRET_WITH_BAD_SPEAKER_SEQUENCE_NUMBER =
    "{"
        "\""AIA_ROTATE_SECRET_NEW_SECRET_KEY"\":100,"
        "\""AIA_ROTATE_SECRET_DIRECTIVE_SEQUENCE_NUMBER_KEY"\":100,"
        "\""AIA_ROTATE_SECRET_SPEAKER_SEQUENCE_NUMBER_KEY"\": \"abc\""
    "}";
    /* clang-format on */

    AiaSecretManager_OnRotateSecretDirectiveReceived(
        g_testSecretManager,
        (void*)ROTATE_SECRET_WITH_BAD_SPEAKER_SEQUENCE_NUMBER,
        strlen( ROTATE_SECRET_WITH_BAD_SPEAKER_SEQUENCE_NUMBER ),
        TEST_SEQUENCE_NUMBER, TEST_INDEX );
    AiaTestUtilities_TestMalformedMessageExceptionIsGenerated(
        g_mockEventRegulator, TEST_SEQUENCE_NUMBER, TEST_INDEX );

    /* Length of new secret is not @c AiaBytesToHoldBits(
     * AiaEncryptionAlgorithm_GetKeySize(
     * AiaSecretDerivationAlgorithm_ToEncryptionAlgorithm(
     * SECRET_DERIVATION_ALGORITHM ) ) ). */
    /* clang-format off */
    static const char* ROTATE_SECRET_WITH_INVALID_LENGTH_SECRET =
    "{"
        "\""AIA_ROTATE_SECRET_NEW_SECRET_KEY"\": \"123\","
        "\""AIA_ROTATE_SECRET_DIRECTIVE_SEQUENCE_NUMBER_KEY"\":100,"
        "\""AIA_ROTATE_SECRET_SPEAKER_SEQUENCE_NUMBER_KEY"\":100"
    "}";
    /* clang-format on */
    AiaSecretManager_OnRotateSecretDirectiveReceived(
        g_testSecretManager, (void*)ROTATE_SECRET_WITH_INVALID_LENGTH_SECRET,
        strlen( ROTATE_SECRET_WITH_INVALID_LENGTH_SECRET ),
        TEST_SEQUENCE_NUMBER, TEST_INDEX );
    AiaTestUtilities_TestMalformedMessageExceptionIsGenerated(
        g_mockEventRegulator, TEST_SEQUENCE_NUMBER, TEST_INDEX );
}

TEST( AiaSecretManagerTests, GetNextSequenceNumberFails )
{
    size_t newSecretLength =
        AiaBytesToHoldBits( AiaEncryptionAlgorithm_GetKeySize(
            AiaSecretDerivationAlgorithm_ToEncryptionAlgorithm(
                SECRET_DERIVATION_ALGORITHM ) ) );
    uint8_t TEST_NEW_SECRET[ newSecretLength ];
    memset( TEST_NEW_SECRET, 4, newSecretLength );
    size_t newSecretBase64Length =
        Aia_Base64GetEncodeSize( TEST_NEW_SECRET, newSecretLength );
    TEST_ASSERT_NOT_EQUAL( 0, newSecretBase64Length );
    uint8_t BASE64_ENCODED_NEW_SECRET[ newSecretBase64Length ];
    TEST_ASSERT_TRUE( Aia_Base64Encode( TEST_NEW_SECRET, newSecretLength,
                                        BASE64_ENCODED_NEW_SECRET,
                                        newSecretBase64Length ) );
    AiaSequenceNumber_t TEST_DIRECTIVE_SEQUENCE_NUMBER = 44;
    AiaSequenceNumber_t TEST_SPEAKER_SEQUENCE_NUMBER = 88;
    char* rotateSecretEvent = generateRotateSecret(
        BASE64_ENCODED_NEW_SECRET, newSecretBase64Length,
        TEST_DIRECTIVE_SEQUENCE_NUMBER, TEST_SPEAKER_SEQUENCE_NUMBER );
    TEST_ASSERT_NOT_NULL( rotateSecretEvent );
    AiaSequenceNumber_t TEST_SEQUENCE_NUMBER = 4;
    size_t TEST_INDEX = 44;

    /* The @c AiaSecretManager_t's attempts to query for outbound sequence
     * numbers to use for the outbound @c AIA_EVENTS_SECRET_ROTATED event will
     * fail resulting in an exception. */
    testSequenceNumberGetter.valueToReturn = false;
    AiaSecretManager_OnRotateSecretDirectiveReceived(
        g_testSecretManager, (void*)rotateSecretEvent,
        strlen( rotateSecretEvent ), TEST_SEQUENCE_NUMBER, TEST_INDEX );
    AiaFree( rotateSecretEvent );

    AiaTestUtilities_TestInternalExceptionExceptionIsGenerated(
        g_mockEventRegulator );
}

TEST( AiaSecretManagerTests, SecretStorageFails )
{
    size_t newSecretLength =
        AiaBytesToHoldBits( AiaEncryptionAlgorithm_GetKeySize(
            AiaSecretDerivationAlgorithm_ToEncryptionAlgorithm(
                SECRET_DERIVATION_ALGORITHM ) ) );
    uint8_t TEST_NEW_SECRET[ newSecretLength ];
    memset( TEST_NEW_SECRET, 4, newSecretLength );
    size_t newSecretBase64Length =
        Aia_Base64GetEncodeSize( TEST_NEW_SECRET, newSecretLength );
    TEST_ASSERT_NOT_EQUAL( 0, newSecretBase64Length );
    uint8_t BASE64_ENCODED_NEW_SECRET[ newSecretBase64Length ];
    TEST_ASSERT_TRUE( Aia_Base64Encode( TEST_NEW_SECRET, newSecretLength,
                                        BASE64_ENCODED_NEW_SECRET,
                                        newSecretBase64Length ) );
    AiaSequenceNumber_t TEST_DIRECTIVE_SEQUENCE_NUMBER = 44;
    AiaSequenceNumber_t TEST_SPEAKER_SEQUENCE_NUMBER = 88;
    char* rotateSecretEvent = generateRotateSecret(
        BASE64_ENCODED_NEW_SECRET, newSecretBase64Length,
        TEST_DIRECTIVE_SEQUENCE_NUMBER, TEST_SPEAKER_SEQUENCE_NUMBER );
    TEST_ASSERT_NOT_NULL( rotateSecretEvent );
    AiaSequenceNumber_t TEST_SEQUENCE_NUMBER = 4;
    size_t TEST_INDEX = 44;

    /* The @c AiaSecretManager_t's attempts to persist the new secret will fail
     * resulting in an exception. */
    testSecretStorer->valueToReturn = false;
    AiaSecretManager_OnRotateSecretDirectiveReceived(
        g_testSecretManager, (void*)rotateSecretEvent,
        strlen( rotateSecretEvent ), TEST_SEQUENCE_NUMBER, TEST_INDEX );
    AiaFree( rotateSecretEvent );

    AiaTestUtilities_TestInternalExceptionExceptionIsGenerated(
        g_mockEventRegulator );
}

TEST( AiaSecretManagerTests, SecretRotatedIsSent )
{
    size_t newSecretLength =
        AiaBytesToHoldBits( AiaEncryptionAlgorithm_GetKeySize(
            AiaSecretDerivationAlgorithm_ToEncryptionAlgorithm(
                SECRET_DERIVATION_ALGORITHM ) ) );
    uint8_t TEST_NEW_SECRET[ newSecretLength ];
    memset( TEST_NEW_SECRET, 4, newSecretLength );
    size_t newSecretBase64Length =
        Aia_Base64GetEncodeSize( TEST_NEW_SECRET, newSecretLength );
    TEST_ASSERT_NOT_EQUAL( 0, newSecretBase64Length );
    uint8_t BASE64_ENCODED_NEW_SECRET[ newSecretBase64Length ];
    TEST_ASSERT_TRUE( Aia_Base64Encode( TEST_NEW_SECRET, newSecretLength,
                                        BASE64_ENCODED_NEW_SECRET,
                                        newSecretBase64Length ) );

    AiaSequenceNumber_t TEST_DIRECTIVE_SEQUENCE_NUMBER = 44;
    AiaSequenceNumber_t TEST_SPEAKER_SEQUENCE_NUMBER = 88;
    char* rotateSecretEvent = generateRotateSecret(
        BASE64_ENCODED_NEW_SECRET, newSecretBase64Length,
        TEST_DIRECTIVE_SEQUENCE_NUMBER, TEST_SPEAKER_SEQUENCE_NUMBER );
    TEST_ASSERT_NOT_NULL( rotateSecretEvent );
    AiaSequenceNumber_t TEST_SEQUENCE_NUMBER = 4;
    size_t TEST_INDEX = 44;

    AiaSequenceNumber_t TEST_OUTBOUND_SEQUENCE_NUMBER = 50;
    testSequenceNumberGetter.sequenceNumberToReturn =
        TEST_OUTBOUND_SEQUENCE_NUMBER;
    AiaSecretManager_OnRotateSecretDirectiveReceived(
        g_testSecretManager, (void*)rotateSecretEvent,
        strlen( rotateSecretEvent ), TEST_SEQUENCE_NUMBER, TEST_INDEX );
    AiaFree( rotateSecretEvent );
    AiaSequenceNumber_t eventSequenceNumber = 0;
    AiaSequenceNumber_t microphoneSequenceNumber = 0;
    TestSecretRotatedIsGenerated( &eventSequenceNumber,
                                  &microphoneSequenceNumber );

    /* Test that sequence numbers for secret rotation are greater than the
     * mocked current outbound sequence numbers. */
    TEST_ASSERT_GREATER_THAN( TEST_OUTBOUND_SEQUENCE_NUMBER,
                              eventSequenceNumber );

    /* This field should only be included if the Microphone capability is
     * supported. */
#ifdef AIA_ENABLE_MICROPHONE
    TEST_ASSERT_GREATER_THAN( TEST_OUTBOUND_SEQUENCE_NUMBER,
                              microphoneSequenceNumber );
#endif

    /* New secret should be persisted. */
    TEST_ASSERT_EQUAL_MEMORY( testSecretStorer->storedSecret, TEST_NEW_SECRET,
                              newSecretLength );
}

TEST( AiaSecretManagerTests, TestAppropriateKeysAreSet )
{
    size_t newSecretLength =
        AiaBytesToHoldBits( AiaEncryptionAlgorithm_GetKeySize(
            AiaSecretDerivationAlgorithm_ToEncryptionAlgorithm(
                SECRET_DERIVATION_ALGORITHM ) ) );
    uint8_t TEST_NEW_SECRET[ newSecretLength ];
    memset( TEST_NEW_SECRET, 4, newSecretLength );
    size_t newSecretBase64Length =
        Aia_Base64GetEncodeSize( TEST_NEW_SECRET, newSecretLength );
    TEST_ASSERT_NOT_EQUAL( 0, newSecretBase64Length );
    uint8_t BASE64_ENCODED_NEW_SECRET[ newSecretBase64Length ];
    TEST_ASSERT_TRUE( Aia_Base64Encode( TEST_NEW_SECRET, newSecretLength,
                                        BASE64_ENCODED_NEW_SECRET,
                                        newSecretBase64Length ) );

    AiaSequenceNumber_t TEST_DIRECTIVE_SEQUENCE_NUMBER = 44;
    AiaSequenceNumber_t TEST_SPEAKER_SEQUENCE_NUMBER = 88;
    char* rotateSecretEvent = generateRotateSecret(
        BASE64_ENCODED_NEW_SECRET, newSecretBase64Length,
        TEST_DIRECTIVE_SEQUENCE_NUMBER, TEST_SPEAKER_SEQUENCE_NUMBER );
    TEST_ASSERT_NOT_NULL( rotateSecretEvent );
    AiaSequenceNumber_t TEST_SEQUENCE_NUMBER = 4;
    size_t TEST_INDEX = 44;

    AiaSequenceNumber_t TEST_OUTBOUND_SEQUENCE_NUMBER = 50;
    testSequenceNumberGetter.sequenceNumberToReturn =
        TEST_OUTBOUND_SEQUENCE_NUMBER;
    AiaSecretManager_OnRotateSecretDirectiveReceived(
        g_testSecretManager, (void*)rotateSecretEvent,
        strlen( rotateSecretEvent ), TEST_SEQUENCE_NUMBER, TEST_INDEX );
    AiaFree( rotateSecretEvent );
    AiaSequenceNumber_t eventSequenceNumber = 0;
    AiaSequenceNumber_t microphoneSequenceNumber = 0;
    TestSecretRotatedIsGenerated( &eventSequenceNumber,
                                  &microphoneSequenceNumber );

    /* Test that sequence numbers for secret rotation are greater than the
     * mocked current outbound sequence numbers. */
    TEST_ASSERT_GREATER_THAN( TEST_OUTBOUND_SEQUENCE_NUMBER,
                              eventSequenceNumber );

    /* This field should only be included if the Microphone capability is
     * supported. */
#ifdef AIA_ENABLE_MICROPHONE
    TEST_ASSERT_GREATER_THAN( TEST_OUTBOUND_SEQUENCE_NUMBER,
                              microphoneSequenceNumber );
#endif

    /* New secret should be persisted. */
    TEST_ASSERT_EQUAL_MEMORY( TEST_NEW_SECRET, testSecretStorer->storedSecret,
                              newSecretLength );

    /* Secret set for encryption should still be the initial secret before @c
     * eventSequenceNumber. */
    AiaSecretManager_Encrypt( g_testSecretManager, AIA_TOPIC_EVENT,
                              eventSequenceNumber - 1, NULL, 0, NULL, NULL, 0,
                              NULL, 0 );
    TEST_ASSERT_EQUAL_MEMORY( INITIAL_SHARED_SECRET,
                              testMockEncryptor->encryptKey, newSecretLength );

    /* Secret set for encryption should now be swapped to the new secret at @c
     * eventSequenceNumber and above. */
    AiaSecretManager_Encrypt( g_testSecretManager, AIA_TOPIC_EVENT,
                              eventSequenceNumber, NULL, 0, NULL, NULL, 0, NULL,
                              0 );
    TEST_ASSERT_EQUAL_MEMORY( TEST_NEW_SECRET, testMockEncryptor->encryptKey,
                              newSecretLength );
    AiaSecretManager_Encrypt( g_testSecretManager, AIA_TOPIC_EVENT,
                              eventSequenceNumber + 1, NULL, 0, NULL, NULL, 0,
                              NULL, 0 );
    TEST_ASSERT_EQUAL_MEMORY( TEST_NEW_SECRET, testMockEncryptor->encryptKey,
                              newSecretLength );

    /* Secret set for decryption should still be the initial secret before @c
     * TEST_DIRECTIVE_SEQUENCE_NUMBER. */
    AiaSecretManager_Encrypt( g_testSecretManager, AIA_TOPIC_DIRECTIVE,
                              TEST_DIRECTIVE_SEQUENCE_NUMBER - 1, NULL, 0, NULL,
                              NULL, 0, NULL, 0 );
    TEST_ASSERT_EQUAL_MEMORY( INITIAL_SHARED_SECRET,
                              testMockEncryptor->encryptKey, newSecretLength );

    /* Secret set for decryption should now be swapped to the new secret at @c
     * TEST_DIRECTIVE_SEQUENCE_NUMBER and above. */
    AiaSecretManager_Decrypt( g_testSecretManager, AIA_TOPIC_DIRECTIVE,
                              TEST_DIRECTIVE_SEQUENCE_NUMBER, NULL, 0, NULL,
                              NULL, 0, NULL, 0 );
    TEST_ASSERT_EQUAL_MEMORY( TEST_NEW_SECRET, testMockEncryptor->encryptKey,
                              newSecretLength );
    AiaSecretManager_Decrypt( g_testSecretManager, AIA_TOPIC_DIRECTIVE,
                              TEST_DIRECTIVE_SEQUENCE_NUMBER + 1, NULL, 0, NULL,
                              NULL, 0, NULL, 0 );
    TEST_ASSERT_EQUAL_MEMORY( TEST_NEW_SECRET, testMockEncryptor->encryptKey,
                              newSecretLength );

    /* Mimic an overrun that rewinds to a previous sequence number. Secret
     * should be the initial secret. */
    AiaSecretManager_Encrypt( g_testSecretManager, AIA_TOPIC_EVENT,
                              eventSequenceNumber - 1, NULL, 0, NULL, NULL, 0,
                              NULL, 0 );
    TEST_ASSERT_EQUAL_MEMORY( INITIAL_SHARED_SECRET,
                              testMockEncryptor->encryptKey, newSecretLength );
}
