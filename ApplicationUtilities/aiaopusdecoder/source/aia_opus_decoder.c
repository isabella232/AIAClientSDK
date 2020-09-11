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
 * @file aia_opus_decoder.c
 * @brief Implements functions for the AiaOpusDecoder_t type.
 */

/* The config header is always included first. */
#include <aia_config.h>

#include <aia_capabilities_config.h>
#include <aiacore/aia_utils.h>
#include <aiaopusdecoder/aia_opus_decoder.h>

#include <opus/opus.h>

/* Default sample rate. */
#define AIA_DECODE_SAMPLE_RATE 48000

/**
 * Underlying struct that contains all the data required to present the @c
 * AiaOpusDecoder_t abstraction.
 */
struct AiaOpusDecoder
{
    /** Underlying libOpus decoder. */
    OpusDecoder* decoder;
};

AiaOpusDecoder_t* AiaOpusDecoder_Create()
{
#ifndef AIA_ENABLE_SPEAKER
#error "Speaker capability must be enabled"
#endif
    AiaOpusDecoder_t* aiaOpusDecoder =
        AiaCalloc( 1, sizeof( AiaOpusDecoder_t ) );
    if( !aiaOpusDecoder )
    {
        AiaLogError( "AiaCalloc failed, bytes=%zu.",
                     sizeof( AiaOpusDecoder_t ) );
        return NULL;
    }

    int size = opus_decoder_get_size( AIA_SPEAKER_AUDIO_DECODER_NUM_CHANNELS );
    aiaOpusDecoder->decoder = AiaCalloc( 1, size );
    if( !aiaOpusDecoder->decoder )
    {
        AiaLogError( "AiaCalloc failed, bytes=%zu.", size );
        AiaFree( aiaOpusDecoder );
        return NULL;
    }

    int error =
        opus_decoder_init( aiaOpusDecoder->decoder, AIA_DECODE_SAMPLE_RATE,
                           AIA_SPEAKER_AUDIO_DECODER_NUM_CHANNELS );
    if( error != OPUS_OK )
    {
        AiaLogError( "opus_decoder_init failed, error=%d", error );
        AiaFree( aiaOpusDecoder->decoder );
        AiaFree( aiaOpusDecoder );
        return NULL;
    }

    return aiaOpusDecoder;
}

void AiaOpusDecoder_Destroy( AiaOpusDecoder_t* aiaOpusDecoder )
{
    AiaAssert( aiaOpusDecoder );
    if( !aiaOpusDecoder )
    {
        AiaLogError( "Null decoder" );
        return;
    }
    AiaFree( aiaOpusDecoder->decoder );
    AiaFree( aiaOpusDecoder );
}

int16_t* AiaOpusDecoder_DecodeFrame( AiaOpusDecoder_t* aiaOpusDecoder,
                                     const void* frame, size_t size,
                                     int* numDecodedSamplesOut )
{
    if( !aiaOpusDecoder )
    {
        AiaLogError( "Null aiaOpusDecoder" );
        return NULL;
    }
    if( !numDecodedSamplesOut )
    {
        AiaLogError( "Null numDecodedSamplesOut" );
        return NULL;
    }
    int totalBits = size * 8;
    AiaDurationMs_t frameDuration =
        totalBits /
        ( AIA_SPEAKER_AUDIO_DECODER_BITS_PER_SECOND / AIA_MS_PER_SECOND );
    int frameSize = AIA_DECODE_SAMPLE_RATE * frameDuration / AIA_MS_PER_SECOND;

    /* TODO: ADSER-1693 This is seriously inefficient, should be a buffer passed
     * by user that can be a static buffer. */
    int16_t* pcmBytes = AiaCalloc(
        frameSize * AIA_SPEAKER_AUDIO_DECODER_NUM_CHANNELS, sizeof( int16_t ) );
    if( !pcmBytes )
    {
        AiaLogError( "AiaCalloc failed, bytes=%zu",
                     frameSize * AIA_SPEAKER_AUDIO_DECODER_NUM_CHANNELS *
                         sizeof( int16_t ) );
        return NULL;
    }
    int numDecodedSamples = opus_decode( aiaOpusDecoder->decoder, frame, size,
                                         pcmBytes, frameSize, 0 );
    if( numDecodedSamples < 0 )
    {
        AiaLogError( "opus_decode failed, error=%s",
                     opus_strerror( numDecodedSamples ) );
        AiaFree( pcmBytes );
        return NULL;
    }

    *numDecodedSamplesOut = numDecodedSamples;
    return pcmBytes;
}
