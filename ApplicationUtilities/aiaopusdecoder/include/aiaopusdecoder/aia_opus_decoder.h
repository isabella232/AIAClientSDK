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
 * @file aia_opus_decoder.h
 * @brief User-facing functions of the @c AiaOpusDecoder_t type
 */

#ifndef AIA_OPUS_DECODER_H_
#ifdef __cplusplus
extern "C" {
#endif
#define AIA_OPUS_DECODER_H_

/* The config header is always included first. */
#include <aia_config.h>

/**
 * Thin wrapper around libOpus for Opus frame decoding.
 */
typedef struct AiaOpusDecoder AiaOpusDecoder_t;

/**
 * Allocates and initializes a @c AiaOpusDecoder_t object from the heap.
 * The returned pointer should be destroyed using @c
 * AiaOpusDecoder_Destroy().
 *
 * @return The newly created @c AiaOpusDecoder_t if successful, or NULL
 * otherwise.
 */
AiaOpusDecoder_t* AiaOpusDecoder_Create();

/**
 * Uninitializes and deallocates an @c AiaOpusDecoder_t previously created by
 * a call to @c AiaOpusDecoder_Create().
 *
 * @param decoder The @c AiaOpusDecoder_t to destroy.
 */
void AiaOpusDecoder_Destroy( AiaOpusDecoder_t* decoder );

/**
 * Decodes Opus encoded data provided and returns pcm bytes. This mirrors @c
 * AiaSpeakerManager_t's @c AiaPlaySpeakerData_t callback.
 *
 * @param decoder The AiaOpusDecoder_t to act on.
 * @param frame The frame to decode.
 * @param size The size in bytes of the frame.
 * @param[out] numDecodedSamplesOut The number of decoded samples contained in
 * the returned buffer.
 * @return Buffer containing decoded pcm bytes or @c NULL on failure. This must
 * be cleaned up via @c AiaFree.
 */
int16_t* AiaOpusDecoder_DecodeFrame( AiaOpusDecoder_t* decoder,
                                     const void* frame, size_t size,
                                     int* numDecodedSamplesOut );

#ifdef __cplusplus
}
#endif
#endif /* ifndef AIA_OPUS_DECODER_H_ */
