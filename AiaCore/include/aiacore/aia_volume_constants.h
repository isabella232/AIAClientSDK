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
 * @file aia_volume_constants.h
 * @brief Constants related to volume.
 */

#ifndef AIA_VOLUME_CONSTANTS_H_
#define AIA_VOLUME_CONSTANTS_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <stdint.h>

/** Max volume. */
#define AIA_MAX_VOLUME UINT8_C( 100 )

/** Min volume. */
static const uint8_t AIA_MIN_VOLUME = 0;

/** The default volume. */
static const uint8_t AIA_DEFAULT_VOLUME = AIA_MAX_VOLUME;

/** The default offline alert volume */
static const uint8_t AIA_DEFAULT_OFFLINE_ALERT_VOLUME = AIA_MAX_VOLUME;

#endif /* ifndef AIA_VOLUME_CONSTANTS_H_ */
