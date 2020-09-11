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
 * @file aia_init.h
 * @brief User-facing functions for the global initialization/cleanup
 * of AIA.
 */

#ifndef AIA_INIT_H_
#define AIA_INIT_H_

/* The config header is always included first. */
#include <aia_config.h>

/** TODO: ADSER-1700 Implement global AiaInit() and AiaCleanup() */

/**
 * Initializes AIA globally.
 *
 */
void AiaInit();

/**
 * Cleans up AIA globally.
 *
 */
void AiaCleanup();

#endif /* ifndef AIA_INIT_H_ */
