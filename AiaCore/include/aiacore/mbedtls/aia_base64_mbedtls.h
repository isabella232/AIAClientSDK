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
 * @file aia_base64_mbedtls.h
 * @brief base64 encoding/decoding utilities for AIA.
 */

#ifndef AIA_BASE64_MBEDTLS_H_
#define AIA_BASE64_MBEDTLS_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <stdbool.h>
#include <stddef.h>

/**
 * @copyDoc Aia_Base64GetEncodeSize()
 */
size_t AiaBase64MbedTls_GetEncodeSize( const uint8_t* input, size_t inputLen );

/**
 * @copyDoc Aia_Base64GetDecodeSize()
 */
size_t AiaBase64MbedTls_GetDecodeSize( const uint8_t* input, size_t inputLen );

/**
 * @copyDoc Aia_Base64Encode()
 */
bool AiaBase64MbedTls_Encode( const uint8_t* input, size_t inputLen,
                              uint8_t* output, size_t outputLen );

/**
 * @copyDoc Aia_Base64Decode()
 */
bool AiaBase64MbedTls_Decode( const uint8_t* input, size_t inputLen,
                              uint8_t* output, size_t outputLen );

#endif /* ifndef AIA_BASE64_MBEDTLS_H_ */
