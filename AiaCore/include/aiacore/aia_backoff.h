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
 * @file aia_backoff.h
 * @brief backoff-related functions for AIA.
 */

#ifndef AIA_BACKOFF_H_
#define AIA_BACKOFF_H_

/* The config header is always included first. */
#include <aia_config.h>

/**
 * Get a backoff time before retrying an action using binary exponential backoff
 * with full jitter.
 *
 * @param retryNum The retry attempt number.
 * @param maxBackoff The maximum backoff time able to return.
 *
 * @return The backoff time in milliseconds.
 */
AiaDurationMs_t AiaBackoff_GetBackoffTimeMilliseconds(
    size_t retryNum, AiaDurationMs_t maxBackoff );

#endif /* ifndef AIA_BACKOFF_H_ */
