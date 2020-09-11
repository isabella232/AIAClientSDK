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
 * @file aia_portaudio_speaker.h
 * @brief Reference implementation of a speaker player using PortAudio
 */

#ifndef AIA_PORTAUDIO_SPEAKER_H_
#ifdef __cplusplus
extern "C" {
#endif
#define AIA_PORTAUDIO_SPEAKER_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <portaudio.h>

/** Sample rate of output data. */
static const double SAMPLE_RATE = 48000;

/**
 * Provides the @c AiaPortAudioSpeaker_t a way to indicate that they are ready
 * to receive speaker frames via @c playSpeakerDataCb() again after a failure to
 * accept a frame of data.
 *
 * @param userData Context to pass to this callback.
 */
typedef void ( *AiaOnSpeakerReadyForDataAgainCb )( void* userData );

/**
 * A thin wrapper around PortAudio used for playing PCM speaker data. Methods of
 * this component are thread-safe.
 */
typedef struct AiaPortAudioSpeaker AiaPortAudioSpeaker_t;

/**
 * Creates an @c AiaPortAudioSpeaker_t. The returned pointer should
 * be destroyed using @c AiaPortAudioSpeaker_Destroy().
 *
 * @param speakerReady Callback that will be invoked when the @c
 * AiaPortAudioSpeaker_t is ready for new frames to be fed again.
 * @param speakerReadyUserData User data associated with @c speakerReady.
 * @return A newly created @c AiaPortAudioSpeaker_t if successful or
 * @c NULL otherwise.
 */
AiaPortAudioSpeaker_t* AiaPortAudioSpeaker_Create(
    AiaOnSpeakerReadyForDataAgainCb speakerReady, void* speakerReadyUserData );

/**
 * Uninitializes and deallocates an @c AiaPortAudioSpeaker_t
 * previously created by a call to
 * @c AiaPortAudioSpeaker_Create().
 *
 * @param speaker The @c AiaPortAudioSpeaker_t to destroy.
 */
void AiaPortAudioSpeaker_Destroy( AiaPortAudioSpeaker_t* speaker );

/**
 * Pushes given PCM bytes for playback.
 *
 * @param speaker The @c AiaPortAudioSpeaker_t to act on.
 * @param buf Buffer contaning samples to play.
 * @param size Number of samples in @c buf.
 * @return @c true if data was written successfully or @c false otherwise.
 */
bool AiaPortAudioSpeaker_PlaySpeakerData( AiaPortAudioSpeaker_t* speaker,
                                          const int16_t* buf, size_t size );

/**
 * Changes the volume.
 *
 * @param speaker The @c AiaPortAudioSpeaker_t to act on.
 * @param volume The new volume, between @c AIA_MIN_VOLUME and @c
 * AIA_MAX_VOLUME, inclusive.
 */
void AiaPortAudioSpeaker_SetNewVolume( AiaPortAudioSpeaker_t* speaker,
                                       uint8_t volume );

#ifdef __cplusplus
}
#endif
#endif /* ifndef AIA_PORTAUDIO_SPEAKER_H_ */
