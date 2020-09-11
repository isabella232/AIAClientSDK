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
 * @file aia_alert_tone.h
 * @brief Stores the raw PCM bytes for playing an offline alerts.
 */

#ifndef AIA_ALERT_TONE_H_
#ifdef __cplusplus
extern "C" {
#endif
#define AIA_ALERT_TONE_H_

#include <stdint.h>
#include <stdlib.h>

/* TODO: ADSER-2383 Support different tones for each alert type during offline
 * alert playback */
/* TODO: ADSER-2384: Move pre-recorded offline alert tones to a
 * user-configurable header */
/* clang-format off */
/**
 * The pre-recorded alert tone (in raw PCM format) to play during offline alert
 * playback.
 */
static const int16_t AIA_ALERT_TONE[] = {
    INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN,
    INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN,
    INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN,
    INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN,
    INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN,
    INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN,
    INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN,
    INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN, INT16_MAX, INT16_MAX,
    INT16_MAX, INT16_MAX, INT16_MAX, INT16_MAX, INT16_MAX, INT16_MAX, INT16_MAX,
    INT16_MAX, INT16_MAX, INT16_MAX, INT16_MAX, INT16_MAX, INT16_MAX, INT16_MAX,
    INT16_MAX, INT16_MAX, INT16_MAX, INT16_MAX, INT16_MAX, INT16_MAX, INT16_MAX,
    INT16_MAX, INT16_MAX, INT16_MAX, INT16_MAX, INT16_MAX, INT16_MAX, INT16_MAX,
    INT16_MAX, INT16_MAX, INT16_MAX, INT16_MAX, INT16_MAX, INT16_MAX, INT16_MAX,
    INT16_MAX, INT16_MAX, INT16_MAX, INT16_MAX, INT16_MAX, INT16_MAX, INT16_MAX,
    INT16_MAX, INT16_MAX, INT16_MAX, INT16_MAX, INT16_MAX, INT16_MAX, INT16_MAX,
    INT16_MAX, INT16_MAX, INT16_MAX
};
/* clang-format on */

#define AIA_ALERT_TONE_FRAME_LENGTH AiaArrayLength( AIA_ALERT_TONE )

#ifdef __cplusplus
}
#endif
#endif /* ifndef AIA_ALERT_TONE_H_ */
