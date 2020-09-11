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
 * @file aia_portaudio_speaker.c
 * @brief Reference implementation of a speaker player using PortAudio.
 */

/* The config header is always included first. */
#include <aia_config.h>

#include <aiaportaudiospeaker/aia_portaudio_speaker.h>

#include <aiacore/aia_volume_constants.h>
#include <aiaspeakermanager/aia_speaker_manager.h>

#include AiaMutex( HEADER )
#include AiaTimer( HEADER )

/** Not recording anything in this component, so no input channels. */
static const int NUM_INPUT_CHANNELS = 0;

/** Number of channels in output data. */
static const int NUM_OUTPUT_CHANNELS = AIA_SPEAKER_AUDIO_DECODER_NUM_CHANNELS;

/**
 * Task that polls for space in the buffer. This task runs at
 * @c AIA_SPEAKER_FRAME_PUSH_CADENCE_MS / 4 (arbitrary). It is important that
 * this run as quick as possible to ensure that the SDK is notified as soon as
 * possible when the speaker is ready to accept new data again.
 *
 * @param userData Context associated with this task.
 */
static void AiaPortAudioSpeaker_PollForBufferSpaceTask( void* userData );

/**
 * Underlying struct that contains all the data required to present the @c
 * AiaPortAudioSpeaker_t abstraction.
 */
struct AiaPortAudioSpeaker
{
    /** Mutex used to guard against asynchronous calls in threaded
     * environments. */
    AiaMutex_t mutex;

    /** @name Variables synchronized by mutex. */
    /** @{ */

    /** Callback to indicate when the speaker is ready to receive data again. */
    const AiaOnSpeakerReadyForDataAgainCb speakerReady;

    /** User data associated with @c speakerReady. */
    void* const speakerReadyUserData;

    /** Underlying PortAudio stream. */
    PaStream* paStream;

    /* Boolean indicating whether the speaker buffer is currently overrun. */
    bool speakerOverflowed;

    /** Number of samples to poll for space for when the speaker is overrun. */
    size_t numSamplesOfSpaceToPollFor;

    /** The volume to play at, between @c AIA_MIN_VOLUME and @c AIA_MAX_VOLUME,
     * inclusive.*/
    uint8_t volume;
    /** @} */

    /** Timer which polls for space in the buffer. This is crucial when the
     * buffer overflows and a callback to the SDK is required for data to flow
     * again. */
    AiaTimer_t pollForBufferSpaceTimer;
};

AiaPortAudioSpeaker_t* AiaPortAudioSpeaker_Create(
    AiaOnSpeakerReadyForDataAgainCb speakerReady, void* speakerReadyUserData )
{
    PaError err;
    err = Pa_Initialize();
    if( err != paNoError )
    {
        AiaLogError( "Failed to initialize PortAudio, errorCode=%d", err );
        return NULL;
    }

    AiaPortAudioSpeaker_t* speaker =
        AiaCalloc( 1, sizeof( AiaPortAudioSpeaker_t ) );
    if( !speaker )
    {
        AiaLogError( "AiaCalloc failed, bytes=%zu",
                     sizeof( AiaPortAudioSpeaker_t ) );
        Pa_Terminate();
        return NULL;
    }

    if( !AiaMutex( Create )( &speaker->mutex, false ) )
    {
        AiaLogError( "AiaMutex( Create ) failed." );
        AiaFree( speaker );
        Pa_Terminate();
        return NULL;
    }

    err = Pa_OpenDefaultStream( &speaker->paStream, NUM_INPUT_CHANNELS,
                                NUM_OUTPUT_CHANNELS, paInt16, SAMPLE_RATE,
                                paFramesPerBufferUnspecified, NULL, NULL );
    if( err != paNoError )
    {
        AiaLogError( "Failed to open PortAudio default stream, errorCode=%d",
                     err );
        AiaMutex( Destroy )( &speaker->mutex );
        AiaFree( speaker );
        Pa_Terminate();
        return NULL;
    }

    err = Pa_StartStream( speaker->paStream );
    if( err != paNoError )
    {
        AiaLogError( "Pa_StartStream failed, errorCode=%d", err );
        Pa_CloseStream( speaker->paStream );
        AiaMutex( Destroy )( &speaker->mutex );
        AiaFree( speaker );
        Pa_Terminate();
        return NULL;
    }

    if( !AiaTimer( Create )( &speaker->pollForBufferSpaceTimer,
                             AiaPortAudioSpeaker_PollForBufferSpaceTask,
                             speaker ) )
    {
        AiaLogError( "Failed to create pollForBufferSpaceTimer" );
        Pa_CloseStream( speaker->paStream );
        AiaMutex( Destroy )( &speaker->mutex );
        AiaFree( speaker );
        Pa_Terminate();
        return NULL;
    }

    if( !AiaTimer( Arm )( &speaker->pollForBufferSpaceTimer, 0,
                          AIA_SPEAKER_FRAME_PUSH_CADENCE_MS / 4 ) )
    {
        AiaLogError( "Failed to arm pollForBufferSpaceTimer" );
        AiaTimer( Destroy )( &speaker->pollForBufferSpaceTimer );
        Pa_CloseStream( speaker->paStream );
        AiaMutex( Destroy )( &speaker->mutex );
        AiaFree( speaker );
        Pa_Terminate();
        return NULL;
    }

    *(AiaOnSpeakerReadyForDataAgainCb*)&( speaker->speakerReady ) =
        speakerReady;
    *(void**)&speaker->speakerReadyUserData = speakerReadyUserData;
    speaker->volume = AIA_DEFAULT_VOLUME;

    return speaker;
}

void AiaPortAudioSpeaker_Destroy( AiaPortAudioSpeaker_t* speaker )
{
    AiaAssert( speaker );
    if( !speaker )
    {
        AiaLogError( "Null speaker" );
        return;
    }
    AiaTimer( Destroy )( &speaker->pollForBufferSpaceTimer );

    AiaMutex( Lock )( &speaker->mutex );
    Pa_CloseStream( speaker->paStream );
    AiaMutex( Unlock )( &speaker->mutex );

    AiaMutex( Destroy )( &speaker->mutex );
    AiaFree( speaker );
    Pa_Terminate();
}

bool AiaPortAudioSpeaker_PlaySpeakerData( AiaPortAudioSpeaker_t* speaker,
                                          const int16_t* buf,
                                          size_t numSamples )
{
    AiaAssert( speaker );
    if( !speaker )
    {
        AiaLogError( "Null speaker" );
        return false;
    }

    AiaMutex( Lock )( &speaker->mutex );

    /* Volume adjusted input buffer. */
    int16_t* volumeAdjustedBuffer = AiaCalloc( numSamples, sizeof( int16_t ) );
    if( !volumeAdjustedBuffer )
    {
        AiaLogError( "AiaCalloc failed, bytes=%zu",
                     sizeof( int16_t ) * numSamples );
        return false;
    }

    /* Get number of frames that can be written without blocking. */
    signed long numFramesAvailableToWrite =
        Pa_GetStreamWriteAvailable( speaker->paStream );
    if( numFramesAvailableToWrite < 0 )
    {
        AiaLogError( "Pa_GetStreamWriteAvailable failed, error=%ld",
                     numFramesAvailableToWrite );
        AiaFree( volumeAdjustedBuffer );
        AiaMutex( Unlock )( &speaker->mutex );
        return false;
    }

    if( numFramesAvailableToWrite < (signed long)numSamples )
    {
        AiaLogDebug(
            "Not enough space to consume all frames, available=%ld, "
            "given=%zu",
            numFramesAvailableToWrite, numSamples );
        speaker->speakerOverflowed = true;
        speaker->numSamplesOfSpaceToPollFor = numSamples;
        AiaFree( volumeAdjustedBuffer );
        AiaMutex( Unlock )( &speaker->mutex );
        return false;
    }

    PaError err;
    if( speaker->volume == AIA_MAX_VOLUME )
    {
        err = Pa_WriteStream( speaker->paStream, buf, numSamples );
    }
    else
    {
        /* Note: This reference implementation is scaled to a linear volume
         * curve. In real implementations, the volume curve may need to be
         * adjusted for different platforms to ensure an adequately low volume
         * at the low end and consistent user-perceived volume increments across
         * the range. Ideally, this would be mapped to a system/device volume
         * API rather than modifying
         * in software, but PortAudio lacks an API for this. */
        for( size_t i = 0; i < numSamples; ++i )
        {
            volumeAdjustedBuffer[ i ] =
                buf[ i ] * speaker->volume / AIA_MAX_VOLUME;
        }

        err = Pa_WriteStream( speaker->paStream, volumeAdjustedBuffer,
                              numSamples );
    }
    if( err != paNoError )
    {
        AiaLogDebug( "Pa_WriteStream failed, errorCode=%s",
                     Pa_GetErrorText( err ) );
        if( err != paOutputUnderflowed )
        {
            AiaLogError( "Pa_WriteStream failed, errorCode=%s",
                         Pa_GetErrorText( err ) );
            AiaFree( volumeAdjustedBuffer );
            AiaMutex( Unlock )( &speaker->mutex );
            return false;
        }
    }

    AiaFree( volumeAdjustedBuffer );
    AiaMutex( Unlock )( &speaker->mutex );
    return true;
}

void AiaPortAudioSpeaker_SetNewVolume( AiaPortAudioSpeaker_t* speaker,
                                       uint8_t volume )
{
    if( !speaker )
    {
        AiaLogError( "Null speaker" );
        return;
    }

    AiaMutex( Lock )( &speaker->mutex );
    speaker->volume = volume;
    AiaMutex( Unlock )( &speaker->mutex );
}

void AiaPortAudioSpeaker_PollForBufferSpaceTask( void* userData )
{
    AiaPortAudioSpeaker_t* speaker = (AiaPortAudioSpeaker_t*)userData;
    AiaAssert( speaker );
    if( !speaker )
    {
        AiaLogError( "Null speaker" );
        return;
    }

    AiaMutex( Lock )( &speaker->mutex );
    if( !speaker->speakerOverflowed )
    {
        AiaMutex( Unlock )( &speaker->mutex );
        return;
    }

    signed long numFramesAvailableToWrite =
        Pa_GetStreamWriteAvailable( speaker->paStream );
    if( numFramesAvailableToWrite < (long)speaker->numSamplesOfSpaceToPollFor )
    {
        AiaMutex( Unlock )( &speaker->mutex );
        return;
    }

    speaker->speakerOverflowed = false;
    speaker->numSamplesOfSpaceToPollFor = 0;
    speaker->speakerReady( speaker->speakerReadyUserData );
    AiaMutex( Unlock )( &speaker->mutex );
}
