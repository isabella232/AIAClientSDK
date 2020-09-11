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
 * @file aia_mbedtls_threading.h
 * @brief User-facing functions for the global initialization/cleanup
 * for mbed TLS threading.
 */

#ifndef AIA_MBEDTLS_THREADING_H_
#ifdef __cplusplus
extern "C" {
#endif
#define AIA_MBEDTLS_THREADING_H_

/* The config header is always included first. */
#include <aia_config.h>

/**
 * Initializes mbed TLS threading layer globally.
 */
void AiaMbedtlsThreading_Init();

/**
 * Cleans up mbed TLS threading layer globally.
 */
void AiaMbedtlsThreading_Cleanup();

#ifdef __cplusplus
}
#endif
#endif /* ifndef AIA_MBEDTLS_THREADING_H_ */
