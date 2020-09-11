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
 * @file aia_base64_mbedtls.c
 * @brief Implements base64 encoding/decoding utilities functions for AIA.
 */

/* The config header is always included first. */
#include <aia_config.h>

#include <aiacore/mbedtls/aia_base64_mbedtls.h>

/* mbed TLS includes. */
#include <mbedtls/base64.h>

size_t AiaBase64MbedTls_GetEncodeSize( const uint8_t* input, size_t inputLen )
{
    size_t ret = 0;
    /* Deliberately ignoring return value since mbedtls is not clear on return
     * value when only checking for size. */
    mbedtls_base64_encode( NULL, 0, &ret, input, inputLen );
    return ret;
}

size_t AiaBase64MbedTls_GetDecodeSize( const uint8_t* input, size_t inputLen )
{
    size_t ret = 0;
    /* Deliberately ignoring return value since mbedtls is not clear on return
     * value when only checking for size. */
    mbedtls_base64_decode( NULL, 0, &ret, input, inputLen );
    return ret;
}

bool AiaBase64MbedTls_Encode( const uint8_t* input, size_t inputLen,
                              uint8_t* output, size_t outputLen )
{
    size_t numBytesWritten = 0;
    return mbedtls_base64_encode( output, outputLen, &numBytesWritten, input,
                                  inputLen ) == 0;
}

bool AiaBase64MbedTls_Decode( const uint8_t* input, size_t inputLen,
                              uint8_t* output, size_t outputLen )
{
    size_t numBytesWritten = 0;
    return mbedtls_base64_decode( output, outputLen, &numBytesWritten, input,
                                  inputLen ) == 0;
}
