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
 * @file aia_sample_app.c
 * @brief Aia sample app.
 */

/* The config header is always included first. */
#include <aia_config.h>

#include "aia_sample_app.h"

#include <aia_application_config.h>
#include <aia_capabilities_config.h>
#include <aiaalertmanager/aia_alert_slot.h>
#include <aiaclient/aia_client.h>
#include <aiaconnectionmanager/aia_connection_constants.h>
#include <aiacore/aia_mbedtls_threading.h>
#include <aiacore/aia_random_mbedtls.h>
#include <aiacore/aia_utils.h>
#include <aiacore/capabilities_sender/aia_capabilities_sender.h>
#include <aiacore/data_stream_buffer/aia_data_stream_buffer.h>
#include <aiacore/data_stream_buffer/aia_data_stream_buffer_reader.h>
#include <aiacore/data_stream_buffer/aia_data_stream_buffer_writer.h>
#include <aiaexceptionmanager/aia_exception_code.h>
#include <aiamicrophonemanager/aia_microphone_constants.h>
#include <aiaregistrationmanager/aia_registration_manager.h>
#include <aiauxmanager/aia_ux_state.h>

#include <aiaalerttonesynthesizer/aia_alert_tone.h>

#ifdef AIA_PORTAUDIO_MICROPHONE
#include <aiaportaudiomicrophone/aia_portaudio_microphone.h>
#endif

#ifdef AIA_PORTAUDIO_SPEAKER
#include <aiaportaudiospeaker/aia_portaudio_speaker.h>
#endif

#ifdef AIA_OPUS_DECODER
#include <aiaopusdecoder/aia_opus_decoder.h>
#endif

#ifdef AIA_LIBCURL_HTTP_CLIENT
#include <curl/curl.h>
#endif

#include AiaTaskPool( HEADER )
#include AiaTimer( HEADER )

#include <inttypes.h>
#include <stdio.h>

char* g_aiaLwaRefreshToken;
char* g_aiaLwaClientId;

#define LWA_CLIENT_ID_MAX_SIZE_BYTES 100
#define LWA_REFRESH_TOKEN_MAX_SIZE_BYTES 2048

/** The amount of audio data to keep in the microphone buffer. */
#define AMOUNT_OF_AUDIO_DATA_IN_MIC_BUFFER ( (AiaDurationMs_t)10000 )

/** Buffer size in samples. */
#define MIC_BUFFER_SIZE_IN_SAMPLES                     \
    ( ( size_t )( AMOUNT_OF_AUDIO_DATA_IN_MIC_BUFFER * \
                  AIA_MICROPHONE_SAMPLE_RATE_HZ / AIA_MS_PER_SECOND ) )

#ifdef AIA_PORTAUDIO_SPEAKER
/**
 * Sample app pushes speaker data through PortAudio speaker which publishes
 * @c SAMPLE_RATE samples per second. We calculate here how long it takes in
 * milliseconds to publish all the samples in a given alert tone frame.
 */
#define AIA_ALERT_TONE_FRAME_LENGTH_IN_MS \
    ( ( AiaDurationMs_t )(                \
        ( AIA_ALERT_TONE_FRAME_LENGTH * AIA_MS_PER_SECOND ) / SAMPLE_RATE ) )
#endif

/** Buffer size in bytes; */
static const size_t MIC_BUFFER_SIZE_IN_BYTES =
    MIC_BUFFER_SIZE_IN_SAMPLES * AIA_MICROPHONE_BUFFER_WORD_SIZE;

/** Arbitrary. */
static const size_t MIC_NUM_READERS = 2;

/** @name Methods that simply print states to stdout. */
/** @{ */
static void onAiaConnectionSuccessfulSimpleUI( void* userData );
static void onAiaDisconnectedSimpleUI( void* userData,
                                       AiaConnectionOnDisconnectCode_t code );
static void onAiaConnectionRejectedSimpleUI(
    void* userData, AiaConnectionOnConnectionRejectionCode_t code );
static void onAiaExceptionReceivedSimpleUI( void* userData,
                                            AiaExceptionCode_t code );
static void onAiaCapabilitiesStateChangedSimpleUI(
    AiaCapabilitiesSenderState_t state, const char* description,
    size_t descriptionLen, void* userData );
static void onUXStateChangedSimpleUI( AiaUXState_t state, void* userData );
static void printHelpMessageSimpleUI();
static void printCommandPrompt();
static void onAiaRegistrationSuccess( void* userData );
static void onAiaRegistrationFailed( void* userData,
                                     AiaRegistrationFailureCode_t code );
/** @} */

/**
 * Callback that allows the PortAudio speaker to indicate it is ready for
 * audio data again.
 *
 * @param userData Context to be associated with the callback.
 */
static void onPortAudioSpeakerReadyAgain( void* userData );

#ifdef AIA_ENABLE_SPEAKER
/**
 * Callback from the underlying @c AiaSpeakerManager_t to push frames for
 * playback.
 *
 * @param buf The buffer to play.
 * @param size Size of @c buf
 * @param userData Context for this callback.
 */
static bool onSpeakerFramePushedForPlayback( const void* buf, size_t size,
                                             void* userData );

/**
 * Callback from the underlying @c AiaSpeakerManager_t to change the volume.
 *
 * @param newVolume The new volume, between @c AIA_MIN_VOLUME and @c
 * AIA_MAX_VOLUME, inclusive.
 * @param userData Context for this callback. This should point to an @c
 * AiaPortAudioSpeaker_t.
 */
static void onSpeakerVolumeChanged( uint8_t newVolume, void* userData );

/**
 * Callback from the underlying @c AiaSpeakerManager_t to start offline alert
 * playback.
 *
 * @param offlineAlert The offline alert to play the alert tone for.
 * @param userData Context for this callback.
 * @return @c true If offline alert playback has successfully started or @c
 * false otherwise.
 */
static bool onStartOfflineAlertTone( const AiaAlertSlot_t* offlineAlert,
                                     void* userData );

/**
 * This is a periodic function controlled by the @c offlineAlertPlaybackTimer.
 * It is responsible for pushing a frame to the speaker for the offline alert
 * tone every time it is invoked.
 *
 * @param userData Context for this function.
 */
static void onPlayOfflineAlertTone( void* userData );

/**
 * Callback from the underlying @c AiaSpeakerManager_t to stop offline alert
 * playback.
 *
 * @param userData Context for this callback.
 * @return @c true If offline alert playback has been successfully stopped or @c
 * false otherwise.
 */
static bool onStopOfflineAlertTone( void* userData );
#endif

/* clang-format off */
static const char* HELP_MESSAGE =
"\n"
"+------------------------------------------------------------------------+\n"
"| Quit:                                                                  |\n"
"|   Press 'q' followed by Enter at any time to quit the application.     |\n"
"| Register client with Aia:                                              |\n"
"|   Press 'r' followed by Enter to register client with Aia.             |\n"
"| Initialize client:                                                     |\n"
"|   Press 'e' followed by Enter to initialize client.                    |\n"
"| Connect to Aia:                                                        |\n"
"|   Press 'c' followed by Enter to connect to Aia.                       |\n"
"| Disconnect from Aia:                                                   |\n"
"|   Press 'd' followed by Enter to disconnect from Aia.                  |\n"
"| Mic toggle:                                                            |\n"
"|   Press 'm' followed by Enter to toggle the microphone.                |\n"
"| Tap to talk:                                                           |\n"
"|   Press 't' followed by Enter to start a tap-to-talk interaction.      |\n"
"| Hold to talk:                                                          |\n"
"|   Press 'h' followed by Enter to start/stop a hold-to-talk interaction.|\n"
"| Publish Capabilities:                                                  |\n"
"|   Press 'i' followed by Enter to declare device capabilities.          |\n"
"| Publish Synchronize State:                                             |\n"
"|   Press 's' followed by Enter to synchronize device's state.           |\n"
"| Synchronize NTP clock:                                                 |\n"
"|   Press 'n' followed by Enter to synchronize the device clock with AIA.|\n"
"| Print current device time:                                             |\n"
"|   Press '.' followed by Enter to print the current device time.        |\n"
"| Play:                                                                  |\n"
"|   Press '1' followed by Enter to initiate a play button press.         |\n"
"| Next:                                                                  |\n"
"|   Press '2' followed by Enter to initiate a next button press.         |\n"
"| Previous:                                                              |\n"
"|   Press '3' followed by Enter to initiate a previous button press.     |\n"
"| Stop:                                                                  |\n"
"|   Press '4' followed by Enter to initiate a stop button press.         |\n"
"| Pause:                                                                 |\n"
"|   Press '5' followed by Enter to initiate a play button press.         |\n"
"| Help:                                                                  |\n"
"|   Press '?' followed by Enter to view a help screen.                   |\n"
"+------------------------------------------------------------------------+";
/* clang-format on */

static const char* COMMAND_PROMPT = "[q,r,e,c,d,m,t,h,i,s,n,.,1,2,3,4,5,?] ";

/**
 * Processes inputted command from a user.
 *
 * @param c The command inputted as a character to process.
 * @param[out] exit This is set to @c true if a user wishes to quit the
 * application.
 * @param sampleApp The @c AiaSampleApp_t that will process received commands.
 */
static void processCommand( char c, bool* exit, AiaSampleApp_t* sampleApp );

/**
 * Initialize the AIA Client.
 *
 * @param sampleApp The @c AiaSampleApp_t to initialize the client for.
 */
static bool initAiaClient( AiaSampleApp_t* sampleApp );

/**
 * Register with AIA.
 *
 * @param sampleApp The @c AiaSampleApp_t to initialize the client for.
 */
static bool registerAia( AiaSampleApp_t* sampleApp );

/**
 * Container of all components necessary for the client to run.
 */
struct AiaSampleApp
{
    /** Handle to a connected MQTT connection. */
    AiaMqttConnectionPointer_t mqttConnection;

    /** Raw underlying buffer that will be used to hold microphone data. */
    void* rawMicrophoneBuffer;

    /** This buffer will be used to hold microphone data. */
    AiaMicrophoneBuffer_t* microphoneBuffer;

    /** This will be used by the underlying @c aiaClient to stream microphone
     * data to Aia when needed. */
    AiaDataStreamReader_t* microphoneBufferReader;

    /** Used to capture and write microphone data to the @c microphoneBuffer. */
    AiaMicrophoneBufferWriter_t* microphoneBufferWriter;

    /** The Aia client. */
    AiaClient_t* aiaClient;

    /** The registration manager used to register the Aia client. */
    AiaRegistrationManager_t* registrationManager;

#ifdef AIA_PORTAUDIO_MICROPHONE
    /** Whether microphone is currently being recorded locally. */
    bool isMicrophoneActive;

    /** Whether an interaction (hold-to-talk) is currently occurring. */
    bool isMicrophoneOpen;

    /** The PortAudio microphone wrapper. */
    AiaPortAudioMicrophoneRecorder_t* portAudioMicrophoneRecorder;
#endif

    /** Whether to skip publishing events to Aia service */
    AiaAtomicBool_t shouldPublishEvent;

#ifdef AIA_ENABLE_SPEAKER
    /** Whether speaker is ready to accept new frames */
    AiaAtomicBool_t isSpeakerReady;

    /** Whether the offline alert being currently played should be removed
     * from persistent storage */
    AiaAtomicBool_t shouldDeleteOfflineAlert;

    /** Timer that controls how often a frame should be pushed to the speaker
     * while playing an offline alert. */
    AiaTimer_t offlineAlertPlaybackTimer;

    /** Pointer to the offline alert that is currently being played */
    AiaAlertSlot_t* offlineAlertInProgress;

    /** Keeps track of the time offline alert playback started at */
    AiaTimepointSeconds_t offlineAlertPlaybackStartTime;
#endif

#ifdef AIA_OPUS_DECODER
    /** Opus decoder to decode speaker frames into PCM data. */
    AiaOpusDecoder_t* opusDecoder;
#endif

#ifdef AIA_PORTAUDIO_SPEAKER
    /** PortAudio based speaker to play PCM data. */
    AiaPortAudioSpeaker_t* portAudioSpeaker;
#endif
};

AiaSampleApp_t* AiaSampleApp_Create( AiaMqttConnectionPointer_t mqttConnection,
                                     const char* iotClientId )
{
#ifdef AIA_LIBCURL_HTTP_CLIENT
    curl_global_init( CURL_GLOBAL_ALL );
#endif

    /* Enabled mbed TLS threading layer. */
    AiaMbedtlsThreading_Init();

    /**
     * Initialize mbed TLS contexts needed for random number generation.
     * Required if AiaRandom_Rand() and AiaRandom_Seed() are using the
     * AiaRandomMbedtls implementation.
     */
    AiaRandomMbedtls_Init();

    /**
     * Initialize Aia MbedTLS Crypto usage.
     */
    if( !AiaCryptoMbedtls_Init() )
    {
        AiaLogError( "AiaCryptoMbedtls_Init failed" );
        AiaRandomMbedtls_Cleanup();
        AiaMbedtlsThreading_Cleanup();
        return NULL;
    }

    if( !AiaRandom_Seed( iotClientId, strlen( iotClientId ) ) )
    {
        AiaCryptoMbedtls_Cleanup();
        AiaRandomMbedtls_Cleanup();
        AiaMbedtlsThreading_Cleanup();
        AiaLogError( "AiaRandom_Seed failed" );
        return NULL;
    }

    /* TODO: ADSER-1690 Simplify cleanup. */
    AiaSampleApp_t* sampleApp =
        (AiaSampleApp_t*)AiaCalloc( 1, sizeof( AiaSampleApp_t ) );
    if( !sampleApp )
    {
        AiaLogError( "AiaCalloc failed, bytes=%zu.", sizeof( AiaSampleApp_t ) );
        AiaCryptoMbedtls_Cleanup();
        AiaRandomMbedtls_Cleanup();
        AiaMbedtlsThreading_Cleanup();
        return NULL;
    }

    sampleApp->mqttConnection = mqttConnection;

    sampleApp->rawMicrophoneBuffer = AiaCalloc( MIC_BUFFER_SIZE_IN_BYTES, 1 );
    if( !sampleApp->rawMicrophoneBuffer )
    {
        AiaLogError( "AiaCalloc failed, bytes=%zu", MIC_BUFFER_SIZE_IN_BYTES );
        AiaFree( sampleApp );
        AiaCryptoMbedtls_Cleanup();
        AiaRandomMbedtls_Cleanup();
        AiaMbedtlsThreading_Cleanup();
        return NULL;
    }

    sampleApp->microphoneBuffer = AiaDataStreamBuffer_Create(
        sampleApp->rawMicrophoneBuffer, MIC_BUFFER_SIZE_IN_BYTES,
        AIA_MICROPHONE_BUFFER_WORD_SIZE, MIC_NUM_READERS );
    if( !sampleApp->microphoneBuffer )
    {
        AiaLogError( "AiaDataStreamBuffer_Create failed" );
        AiaFree( sampleApp->rawMicrophoneBuffer );
        AiaFree( sampleApp );
        AiaCryptoMbedtls_Cleanup();
        AiaRandomMbedtls_Cleanup();
        AiaMbedtlsThreading_Cleanup();
        return NULL;
    }

    sampleApp->microphoneBufferReader = AiaDataStreamBuffer_CreateReader(
        sampleApp->microphoneBuffer, AIA_DATA_STREAM_BUFFER_READER_NONBLOCKING,
        true );
    if( !sampleApp->microphoneBufferReader )
    {
        AiaLogError( "AiaDataStreamBuffer_CreateReader failed" );
        AiaDataStreamBuffer_Destroy( sampleApp->microphoneBuffer );
        AiaFree( sampleApp->rawMicrophoneBuffer );
        AiaFree( sampleApp );
        AiaCryptoMbedtls_Cleanup();
        AiaRandomMbedtls_Cleanup();
        AiaMbedtlsThreading_Cleanup();
        return NULL;
    }

    sampleApp->microphoneBufferWriter = AiaDataStreamBuffer_CreateWriter(
        sampleApp->microphoneBuffer, AIA_DATA_STREAM_BUFFER_WRITER_NONBLOCKABLE,
        false );
    if( !sampleApp->microphoneBufferWriter )
    {
        AiaLogError( "AiaDataStreamBuffer_CreateWriter failed" );
        AiaDataStreamReader_Destroy( sampleApp->microphoneBufferReader );
        AiaDataStreamBuffer_Destroy( sampleApp->microphoneBuffer );
        AiaFree( sampleApp->rawMicrophoneBuffer );
        AiaFree( sampleApp );
        AiaCryptoMbedtls_Cleanup();
        AiaRandomMbedtls_Cleanup();
        AiaMbedtlsThreading_Cleanup();
        return NULL;
    }

#ifdef AIA_PORTAUDIO_MICROPHONE
    sampleApp->portAudioMicrophoneRecorder =
        AiaPortAudioMicrophoneRecorder_Create(
            sampleApp->microphoneBufferWriter );
    if( !sampleApp->portAudioMicrophoneRecorder )
    {
        AiaLogError( "AiaPortAudioMicrophoneRecorder_Create failed" );
        AiaDataStreamWriter_Destroy( sampleApp->microphoneBufferWriter );
        AiaDataStreamReader_Destroy( sampleApp->microphoneBufferReader );
        AiaDataStreamBuffer_Destroy( sampleApp->microphoneBuffer );
        AiaFree( sampleApp->rawMicrophoneBuffer );
        AiaFree( sampleApp );
        AiaCryptoMbedtls_Cleanup();
        AiaRandomMbedtls_Cleanup();
        AiaMbedtlsThreading_Cleanup();
        return NULL;
    }

    if( !AiaPortAudioMicrophoneRecorder_StartStreamingMicrophoneData(
            sampleApp->portAudioMicrophoneRecorder ) )
    {
        AiaLogError( "startStreamingMicrophoneData failed" );
        AiaPortAudioMicrophoneRecorder_Destroy(
            sampleApp->portAudioMicrophoneRecorder );
        AiaDataStreamWriter_Destroy( sampleApp->microphoneBufferWriter );
        AiaDataStreamReader_Destroy( sampleApp->microphoneBufferReader );
        AiaDataStreamBuffer_Destroy( sampleApp->microphoneBuffer );
        AiaFree( sampleApp->rawMicrophoneBuffer );
        AiaFree( sampleApp );
        AiaCryptoMbedtls_Cleanup();
        AiaRandomMbedtls_Cleanup();
        AiaMbedtlsThreading_Cleanup();
        return NULL;
    }

    sampleApp->isMicrophoneActive = true;

#endif

#ifdef AIA_OPUS_DECODER
    sampleApp->opusDecoder = AiaOpusDecoder_Create();
    if( !sampleApp->opusDecoder )
    {
        AiaLogError( "AiaOpusDecoder_Create failed" );
#ifdef AIA_PORTAUDIO_MICROPHONE
        AiaPortAudioMicrophoneRecorder_Destroy(
            sampleApp->portAudioMicrophoneRecorder );
#endif
        AiaDataStreamWriter_Destroy( sampleApp->microphoneBufferWriter );
        AiaDataStreamReader_Destroy( sampleApp->microphoneBufferReader );
        AiaDataStreamBuffer_Destroy( sampleApp->microphoneBuffer );
        AiaFree( sampleApp->rawMicrophoneBuffer );
        AiaFree( sampleApp );
        AiaCryptoMbedtls_Cleanup();
        AiaRandomMbedtls_Cleanup();
        AiaMbedtlsThreading_Cleanup();
        return NULL;
    }
#endif

#ifdef AIA_PORTAUDIO_SPEAKER
    sampleApp->portAudioSpeaker =
        AiaPortAudioSpeaker_Create( onPortAudioSpeakerReadyAgain, sampleApp );
    if( !sampleApp->portAudioSpeaker )
    {
        AiaLogError( "AiaPortAudioSpeakerPlayer_Create failed" );
#ifdef AIA_OPUS_DECODER
        AiaOpusDecoder_Destroy( sampleApp->opusDecoder );
#endif
#ifdef AIA_PORTAUDIO_MICROPHONE
        AiaPortAudioMicrophoneRecorder_Destroy(
            sampleApp->portAudioMicrophoneRecorder );
#endif
        AiaDataStreamWriter_Destroy( sampleApp->microphoneBufferWriter );
        AiaDataStreamReader_Destroy( sampleApp->microphoneBufferReader );
        AiaDataStreamBuffer_Destroy( sampleApp->microphoneBuffer );
        AiaFree( sampleApp->rawMicrophoneBuffer );
        AiaFree( sampleApp );
        AiaCryptoMbedtls_Cleanup();
        AiaRandomMbedtls_Cleanup();
        AiaMbedtlsThreading_Cleanup();
        return NULL;
    }
#endif

#ifdef AIA_ENABLE_SPEAKER
    if( !AiaTimer( Create )( &sampleApp->offlineAlertPlaybackTimer,
                             onPlayOfflineAlertTone, sampleApp ) )
    {
        AiaLogError( "AiaTimer( Create ) failed" );
        AiaPortAudioSpeaker_Destroy( sampleApp->portAudioSpeaker );
#ifdef AIA_OPUS_DECODER
        AiaOpusDecoder_Destroy( sampleApp->opusDecoder );
#endif
#ifdef AIA_PORTAUDIO_MICROPHONE
        AiaPortAudioMicrophoneRecorder_Destroy(
            sampleApp->portAudioMicrophoneRecorder );
#endif
        AiaDataStreamWriter_Destroy( sampleApp->microphoneBufferWriter );
        AiaDataStreamReader_Destroy( sampleApp->microphoneBufferReader );
        AiaDataStreamBuffer_Destroy( sampleApp->microphoneBuffer );
        AiaFree( sampleApp->rawMicrophoneBuffer );
        AiaFree( sampleApp );
        AiaCryptoMbedtls_Cleanup();
        AiaRandomMbedtls_Cleanup();
        AiaMbedtlsThreading_Cleanup();
        return NULL;
    }
#endif

    AiaAtomicBool_Clear( &sampleApp->shouldPublishEvent );

#ifdef AIA_ENABLE_SPEAKER
    AiaAtomicBool_Set( &sampleApp->isSpeakerReady );
    AiaAtomicBool_Clear( &sampleApp->shouldDeleteOfflineAlert );
#endif

    return sampleApp;
}

void AiaSampleApp_Destroy( AiaSampleApp_t* sampleApp )
{
    AiaAssert( sampleApp );
    if( !sampleApp )
    {
        AiaLogError( "Null sampleApp" );
        return;
    }

    AiaAtomicBool_Clear( &sampleApp->shouldPublishEvent );

#ifdef AIA_ENABLE_SPEAKER
    AiaAtomicBool_Clear( &sampleApp->isSpeakerReady );
    AiaAtomicBool_Clear( &sampleApp->shouldDeleteOfflineAlert );

    AiaTimer( Destroy )( &sampleApp->offlineAlertPlaybackTimer );
    if( sampleApp->offlineAlertInProgress )
    {
        AiaFree( sampleApp->offlineAlertInProgress );
        sampleApp->offlineAlertInProgress = NULL;
    }
#endif

#ifdef AIA_PORTAUDIO_SPEAKER
    AiaPortAudioSpeaker_Destroy( sampleApp->portAudioSpeaker );
#endif
#ifdef AIA_OPUS_DECODER
    AiaOpusDecoder_Destroy( sampleApp->opusDecoder );
#endif
#ifdef AIA_PORTAUDIO_MICROPHONE
    AiaPortAudioMicrophoneRecorder_Destroy(
        sampleApp->portAudioMicrophoneRecorder );
#endif
    if( sampleApp->aiaClient )
    {
        AiaClient_Destroy( sampleApp->aiaClient );
    }

    AiaDataStreamWriter_Destroy( sampleApp->microphoneBufferWriter );
    AiaDataStreamReader_Destroy( sampleApp->microphoneBufferReader );
    AiaDataStreamBuffer_Destroy( sampleApp->microphoneBuffer );
    AiaFree( sampleApp->rawMicrophoneBuffer );
    AiaFree( sampleApp );

    AiaCryptoMbedtls_Cleanup();
    AiaRandomMbedtls_Cleanup();
    AiaMbedtlsThreading_Cleanup();
#ifdef AIA_LIBCURL_HTTP_CLIENT
    curl_global_cleanup();
#endif
}

void AiaSampleApp_Run( AiaSampleApp_t* sampleApp )
{
    AiaAssert( sampleApp );
    if( !sampleApp )
    {
        AiaLogError( "Null sampleApp" );
        return;
    }
    printHelpMessageSimpleUI();
    printCommandPrompt();
    bool exit = false;
    int lastCommand = '\n';
    while( !exit )
    {
        int command = getchar();
        switch( command )
        {
            case '\n':
                if( lastCommand == '\n' )
                {
                    printCommandPrompt();
                }
                break;
            case EOF:
                break;
            default:
                processCommand( command, &exit, sampleApp );
                break;
        }
        lastCommand = command;
    }
}

static bool initAiaClient( AiaSampleApp_t* sampleApp )
{
    AiaLogInfo( "Initializing client." );
    sampleApp->aiaClient = AiaClient_Create(
        sampleApp->mqttConnection, onAiaConnectionSuccessfulSimpleUI,
        onAiaConnectionRejectedSimpleUI, onAiaDisconnectedSimpleUI, sampleApp,
        AiaTaskPool( GetSystemTaskPool )(), onAiaExceptionReceivedSimpleUI,
        NULL, onAiaCapabilitiesStateChangedSimpleUI, NULL
#ifdef AIA_ENABLE_SPEAKER
        ,
        onSpeakerFramePushedForPlayback, sampleApp, onSpeakerVolumeChanged,
        sampleApp, onStartOfflineAlertTone, sampleApp, onStopOfflineAlertTone,
        sampleApp
#endif
        ,
        onUXStateChangedSimpleUI, NULL
#ifdef AIA_ENABLE_MICROPHONE
        ,
        sampleApp->microphoneBufferReader
#endif
    );

    if( !sampleApp->aiaClient )
    {
        AiaLogError( "AiaClient_Create failed" );
        return false;
    }

    return true;
}

static bool registerAia( AiaSampleApp_t* sampleApp )
{
    int ch;
    /* Clear stdin buffer */
    while( ( ch = getchar() ) != '\n' && ch != EOF )
        ;

    printf( "Enter LWA Client Id: " );
    char aiaLwaClientId[ LWA_CLIENT_ID_MAX_SIZE_BYTES ];
    if( !fgets( aiaLwaClientId, LWA_CLIENT_ID_MAX_SIZE_BYTES, stdin ) )
    {
        AiaLogError( "Failed to get LWA Client Id input." );
        return false;
    }
    /* Replace '\n' from fgets with null-terminating character. */
    aiaLwaClientId[ strlen( aiaLwaClientId ) - 1 ] = '\0';

    g_aiaLwaClientId = aiaLwaClientId;

    printf( "Enter LWA Refresh Token: " );
    g_aiaLwaRefreshToken =
        (char*)AiaCalloc( 1, LWA_REFRESH_TOKEN_MAX_SIZE_BYTES );
    if( !fgets( g_aiaLwaRefreshToken, LWA_REFRESH_TOKEN_MAX_SIZE_BYTES,
                stdin ) )
    {
        AiaLogError( "Failed to get LWA Refresh Token input." );
        AiaFree( g_aiaLwaRefreshToken );
        return false;
    }
    /* Replace '\n' from fgets with null-terminating character. */
    g_aiaLwaRefreshToken[ strlen( g_aiaLwaRefreshToken ) - 1 ] = '\0';

    sampleApp->registrationManager =
        AiaRegistrationManager_Create( onAiaRegistrationSuccess, sampleApp,
                                       onAiaRegistrationFailed, sampleApp );
    if( !sampleApp->registrationManager )
    {
        AiaLogError( "AiaRegistrationManager_Create failed" );
        AiaFree( g_aiaLwaRefreshToken );
        return false;
    }

    if( !AiaRegistrationManager_Register( sampleApp->registrationManager ) )
    {
        AiaLogError( "AiaRegistrationManager_Register Failed" );
        AiaFree( g_aiaLwaRefreshToken );
        AiaRegistrationManager_Destroy( sampleApp->registrationManager );
        sampleApp->registrationManager = NULL;
        return false;
    }
    AiaFree( g_aiaLwaRefreshToken );
    return true;
}

static void onAiaRegistrationSuccess( void* userData )
{
    if( !userData )
    {
        AiaLogError( "Null userData." );
        return;
    }
    AiaSampleApp_t* sampleApp = (AiaSampleApp_t*)userData;
    AiaLogInfo( "Registration Succeeded" );
    AiaRegistrationManager_Destroy( sampleApp->registrationManager );
    sampleApp->registrationManager = NULL;
}

static void onAiaRegistrationFailed( void* userData,
                                     AiaRegistrationFailureCode_t code )
{
    if( !userData )
    {
        AiaLogError( "Null userData." );
        return;
    }
    AiaSampleApp_t* sampleApp = (AiaSampleApp_t*)userData;
    AiaLogInfo( "Registration Failed, code=%d", code );
    AiaRegistrationManager_Destroy( sampleApp->registrationManager );
    sampleApp->registrationManager = NULL;
}

void processCommand( char c, bool* exit, AiaSampleApp_t* sampleApp )
{
    switch( c )
    {
        case '?':
            printHelpMessageSimpleUI();
            printCommandPrompt();
            *exit = false;
            return;
        case 'q':
            AiaLogInfo( "Quitting" );
            *exit = true;
            return;
        case 'e':
            if( sampleApp->aiaClient )
            {
                AiaLogInfo( "Client already initialized." );
                *exit = false;
                return;
            }
            if( !initAiaClient( sampleApp ) )
            {
                AiaLogError( "Client initialization failed" );
                *exit = true;
                return;
            }

            AiaLogInfo( "Client initialized." );
            *exit = false;
            return;
        case 'c':
            if( !sampleApp->aiaClient )
            {
                AiaLogInfo( "Client not yet initialized." );
                *exit = false;
                return;
            }
            AiaLogInfo( "Connecting to Aia" );
            if( !AiaClient_Connect( sampleApp->aiaClient ) )
            {
                AiaLogError( "Failed to connect to Aia" );
                *exit = true;
                return;
            }
            *exit = false;
            return;
        case 'd':
            if( !sampleApp->aiaClient )
            {
                AiaLogInfo( "Client not yet initialized." );
                *exit = false;
                return;
            }
            AiaLogInfo( "Disconnecting from Aia" );
            if( !AiaClient_Disconnect(
                    sampleApp->aiaClient,
                    AIA_CONNECTION_ON_DISCONNECTED_GOING_OFFLINE, NULL ) )
            {
                AiaLogError( "Failed to disconnect from Aia" );
            }
            *exit = false;
            return;
        case 'r':
            if( sampleApp->aiaClient )
            {
                AiaLogInfo(
                    "Client is already initialized. Registration can only be "
                    "done before the client is initialized" );
                *exit = false;
                return;
            }
            AiaLogInfo( "Registering with Aia" );

            if( !registerAia( sampleApp ) )
            {
                AiaLogError( "Registration Failed" );
            }
            *exit = false;
            return;
#ifdef AIA_PORTAUDIO_MICROPHONE
        case 'm':
            AiaLogInfo( "Microphone toggle" );
            if( sampleApp->isMicrophoneActive )
            {
                AiaPortAudioMicrophoneRecorder_StopStreamingMicrophoneData(
                    sampleApp->portAudioMicrophoneRecorder );
            }
            else
            {
                AiaPortAudioMicrophoneRecorder_StartStreamingMicrophoneData(
                    sampleApp->portAudioMicrophoneRecorder );
            }
            sampleApp->isMicrophoneActive = !sampleApp->isMicrophoneActive;
            AiaLogInfo( "New microphone state: %s",
                        sampleApp->isMicrophoneActive ? "on" : "off" );
            *exit = false;
            return;
        case 't':
            if( !sampleApp->aiaClient )
            {
                AiaLogInfo( "Client not yet initialized." );
                *exit = false;
                return;
            }
            AiaLogInfo( "Tap to talk" );
#ifdef AIA_ENABLE_SPEAKER
            AiaClient_StopSpeaker( sampleApp->aiaClient );
            if( !AiaClient_TapToTalkStart(
                    sampleApp->aiaClient,
                    AiaDataStreamWriter_Tell(
                        sampleApp->microphoneBufferWriter ),
                    AIA_MICROPHONE_PROFILE_NEAR_FIELD ) )
            {
                AiaLogError( "Failed to initiate tap-to-talk." );
                *exit = true;
                return;
            }
#endif
            *exit = false;
            return;
        case 'h':
            if( !sampleApp->aiaClient )
            {
                AiaLogInfo( "Client not yet initialized." );
                *exit = false;
                return;
            }
            AiaLogInfo( "Hold to talk" );
            if( !sampleApp->isMicrophoneOpen )
            {
#ifdef AIA_ENABLE_SPEAKER
                AiaClient_StopSpeaker( sampleApp->aiaClient );
                if( !AiaClient_HoldToTalkStart(
                        sampleApp->aiaClient,
                        AiaDataStreamWriter_Tell(
                            sampleApp->microphoneBufferWriter ) ) )
                {
                    AiaLogError( "Failed to initiate hold-to-talk." );
                    *exit = true;
                    return;
                }
#endif
            }
            else
            {
                AiaClient_CloseMicrophone( sampleApp->aiaClient );
            }
            sampleApp->isMicrophoneOpen = !sampleApp->isMicrophoneOpen;
            *exit = false;
            return;
#endif
        case 'i':
            if( !sampleApp->aiaClient )
            {
                AiaLogInfo( "Client not yet initialized." );
                *exit = false;
                return;
            }
            AiaLogInfo( "Publishing capabilities" );
            if( !AiaClient_PublishCapabilities( sampleApp->aiaClient ) )
            {
                AiaLogError( "Failed to publish capabilities." );
                *exit = true;
                return;
            }
            *exit = false;
            return;
        case 's':
            if( !sampleApp->aiaClient )
            {
                AiaLogInfo( "Client not yet initialized." );
                *exit = false;
                return;
            }
            AiaLogInfo( "Publishing SynchronizeState event" );
            if( !AiaClient_SynchronizeState( sampleApp->aiaClient ) )
            {
                AiaLogError( "Failed to synchronize state." );
                *exit = true;
                return;
            }
            *exit = false;
            return;
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        {
            AiaButtonCommand_t indexToCommand[] = {
                AIA_BUTTON_PLAY, AIA_BUTTON_NEXT, AIA_BUTTON_PREVIOUS,
                AIA_BUTTON_STOP, AIA_BUTTON_PAUSE
            };
            size_t index = c - '1';
            if( index >= AiaArrayLength( indexToCommand ) )
            {
                AiaLogError( "Unknown button %c.", c );
                *exit = true;
                return;
            }
            AiaButtonCommand_t command = indexToCommand[ index ];

#ifdef AIA_ENABLE_SPEAKER
            /* Stop offline alert playback only, no need to send the command to
             * the service */
            if( command == AIA_BUTTON_STOP )
            {
                AiaAtomicBool_Set( &sampleApp->shouldDeleteOfflineAlert );

                if( !AiaAtomicBool_Load( &sampleApp->shouldPublishEvent ) )
                {
                    AiaLogDebug( "Stopping offline alert playback" );
                    AiaLogDebug( "Not publishing button command issued event" );
                    *exit = false;
                    return;
                }
            }
#endif

            const char* name = AiaButtonCommand_ToString( command );
            AiaLogInfo( "Initiating %s button press", name );
            if( !AiaClient_OnButtonPressed( sampleApp->aiaClient, command ) )
            {
                AiaLogError( "Failed to press %s button.", name );
                *exit = true;
                return;
            }
            *exit = false;
            return;
        }
#ifdef AIA_ENABLE_CLOCK
        case 'n':
            if( !sampleApp->aiaClient )
            {
                AiaLogInfo( "Client not yet initialized." );
                *exit = false;
                return;
            }
            AiaLogInfo( "Initiating NTP clock synchronization" );
            if( !AiaClient_SynchronizeClock( sampleApp->aiaClient ) )
            {
                AiaLogError( "Failed to synchronize clock." );
                *exit = true;
                return;
            }
            *exit = false;
            return;
#endif
        case '.':
            AiaLogInfo( "Seconds since NTP epoch: %" PRIu64,
                        AiaClock_GetTimeSinceNTPEpoch() );
            *exit = false;
            return;
        default:
            AiaLogInfo( HELP_MESSAGE );
            return;
    }
}

static void onAiaConnectionSuccessfulSimpleUI( void* userData )
{
    AiaSampleApp_t* sampleApp = (AiaSampleApp_t*)userData;
    AiaAssert( sampleApp );
    if( !sampleApp )
    {
        AiaLogError( "Null sampleApp" );
        return;
    }

    AiaAtomicBool_Set( &sampleApp->shouldPublishEvent );
    AiaLogInfo( "Aia connection successful" );
}

static void onAiaDisconnectedSimpleUI( void* userData,
                                       AiaConnectionOnDisconnectCode_t code )
{
    AiaSampleApp_t* sampleApp = (AiaSampleApp_t*)userData;
    AiaAssert( sampleApp );
    if( !sampleApp )
    {
        AiaLogError( "Null sampleApp" );
        return;
    }

    AiaAtomicBool_Clear( &sampleApp->shouldPublishEvent );
    AiaLogInfo( "Disconnected from Aia, code=%d", code );

    /*
     * Clear out the client structure in sampleApp, and re-create it to clean
     * its state.
     */
    AiaClient_Destroy( sampleApp->aiaClient );
    sampleApp->aiaClient = AiaClient_Create(
        sampleApp->mqttConnection, onAiaConnectionSuccessfulSimpleUI,
        onAiaConnectionRejectedSimpleUI, onAiaDisconnectedSimpleUI, sampleApp,
        AiaTaskPool( GetSystemTaskPool )(), onAiaExceptionReceivedSimpleUI,
        NULL, onAiaCapabilitiesStateChangedSimpleUI, NULL
#ifdef AIA_ENABLE_SPEAKER
        ,
        onSpeakerFramePushedForPlayback, sampleApp, onSpeakerVolumeChanged,
        sampleApp, onStartOfflineAlertTone, sampleApp, onStopOfflineAlertTone,
        sampleApp
#endif
        ,
        onUXStateChangedSimpleUI, NULL
#ifdef AIA_ENABLE_MICROPHONE
        ,
        sampleApp->microphoneBufferReader
#endif
    );

    if( !sampleApp->aiaClient )
    {
        AiaLogError( "AiaClient_Create failed" );
        return;
    }
}

static void onAiaConnectionRejectedSimpleUI(
    void* userData, AiaConnectionOnConnectionRejectionCode_t code )
{
    AiaSampleApp_t* sampleApp = (AiaSampleApp_t*)userData;
    AiaAssert( sampleApp );
    if( !sampleApp )
    {
        AiaLogError( "Null sampleApp" );
        return;
    }

    AiaAtomicBool_Clear( &sampleApp->shouldPublishEvent );
    AiaLogInfo( "Aia connection rejected, code=%d", code );
}

static void onAiaExceptionReceivedSimpleUI( void* userData,
                                            AiaExceptionCode_t code )
{
    (void)userData;
    AiaLogInfo( "Aia exception received, code=%d", code );
}

static void onAiaCapabilitiesStateChangedSimpleUI(
    AiaCapabilitiesSenderState_t state, const char* description,
    size_t descriptionLen, void* userData )
{
    (void)userData;
    AiaLogInfo( "Aia capabilities state changed, state=%s, description=%.*s",
                AiaCapabilitiesSenderState_ToString( state ), descriptionLen,
                description );
}

static void onPortAudioSpeakerReadyAgain( void* userData )
{
    AiaSampleApp_t* sampleApp = (AiaSampleApp_t*)userData;
    AiaAssert( sampleApp );
    if( !sampleApp )
    {
        AiaLogError( "Null sampleApp" );
        return;
    }
    AiaAtomicBool_Set( &sampleApp->isSpeakerReady );
#ifdef AIA_ENABLE_SPEAKER
    AiaClient_OnSpeakerReady( sampleApp->aiaClient );
#endif
}

#ifdef AIA_ENABLE_SPEAKER
static bool onSpeakerFramePushedForPlayback( const void* buf, size_t size,
                                             void* userData )
{
    AiaSampleApp_t* sampleApp = (AiaSampleApp_t*)userData;
    AiaAssert( sampleApp );
    if( !sampleApp )
    {
        AiaLogError( "Null sampleApp" );
        return false;
    }
    AiaLogDebug( "Received speaker frame for playback, size=%zu", size );

    int16_t* pcmSamples = NULL;
#ifdef AIA_OPUS_DECODER
    int decodedSamples = 0;
    pcmSamples = AiaOpusDecoder_DecodeFrame( sampleApp->opusDecoder, buf, size,
                                             &decodedSamples );
    if( !pcmSamples )
    {
        AiaLogError( "AiaOpusDecoder_DecodeFrame failed" );
        return false;
    }
#endif

    bool ret = true;
#ifdef AIA_PORTAUDIO_SPEAKER
    ret = AiaPortAudioSpeaker_PlaySpeakerData( sampleApp->portAudioSpeaker,
                                               pcmSamples, decodedSamples );
#endif
    if( pcmSamples )
    {
        AiaFree( pcmSamples );
    }
    return ret;
}

static void onSpeakerVolumeChanged( uint8_t newVolume, void* userData )
{
    AiaSampleApp_t* sampleApp = (AiaSampleApp_t*)userData;
    AiaAssert( sampleApp );
    if( !sampleApp )
    {
        AiaLogError( "Null sampleApp" );
        return;
    }
    AiaLogDebug( "Received volume change, volume=%" PRIu8, newVolume );
#ifdef AIA_PORTAUDIO_SPEAKER
    if( sampleApp->portAudioSpeaker )
    {
        AiaPortAudioSpeaker_SetNewVolume( sampleApp->portAudioSpeaker,
                                          newVolume );
    }
#endif
}

static bool onStopOfflineAlertTone( void* userData )
{
    AiaSampleApp_t* sampleApp = (AiaSampleApp_t*)userData;
    AiaAssert( sampleApp );
    if( !sampleApp )
    {
        AiaLogError( "Null sampleApp" );
        return false;
    }

    /* Destroy offline alert playback timer */
    AiaTimer( Destroy )( &sampleApp->offlineAlertPlaybackTimer );
    if( !AiaTimer( Create )( &sampleApp->offlineAlertPlaybackTimer,
                             onPlayOfflineAlertTone, sampleApp ) )
    {
        AiaLogError( "AiaTimer( Create ) failed" );
        return false;
    }

    /* Clear offline alert in progress, clear conditional flags */
    if( sampleApp->offlineAlertInProgress )
    {
        AiaFree( sampleApp->offlineAlertInProgress );
        sampleApp->offlineAlertInProgress = NULL;
    }
    sampleApp->offlineAlertPlaybackStartTime = 0;
    AiaAtomicBool_Clear( &sampleApp->shouldDeleteOfflineAlert );

    return true;
}

static bool onStartOfflineAlertTone( const AiaAlertSlot_t* offlineAlert,
                                     void* userData )
{
    AiaSampleApp_t* sampleApp = (AiaSampleApp_t*)userData;
    AiaAssert( sampleApp );
    if( !sampleApp )
    {
        AiaLogError( "Null sampleApp" );
        return false;
    }
    if( !offlineAlert )
    {
        AiaLogError( "Null offlineAlert" );
        return false;
    }

    /* Set the offline alert to play, initialize conditional variables */
    sampleApp->offlineAlertInProgress =
        AiaCalloc( 1, sizeof( AiaAlertSlot_t ) );
    if( !sampleApp->offlineAlertInProgress )
    {
        AiaLogError( "AiaCalloc failed, bytes=%zu.", sizeof( AiaAlertSlot_t ) );
        return false;
    }
    *sampleApp->offlineAlertInProgress = *offlineAlert;

    /* Set the time we started offline alert playback */
    sampleApp->offlineAlertPlaybackStartTime = AiaClock_GetTimeSinceNTPEpoch();

    AiaLogDebug( "Playing offline alert tone for %s",
                 AiaAlertType_ToString( offlineAlert->alertType ) );

    /* Arm the offline alert playback timer to go off every @c
     * AIA_SPEAKER_FRAME_PUSH_CADENCE_MS milliseconds */
    if( !AiaTimer( Arm )( &sampleApp->offlineAlertPlaybackTimer, 0,
                          AIA_SPEAKER_FRAME_PUSH_CADENCE_MS ) )
    {
        AiaLogError( "AiaTimer( Arm ) failed" );
        if( sampleApp->offlineAlertInProgress )
        {
            AiaFree( sampleApp->offlineAlertInProgress );
            sampleApp->offlineAlertInProgress = NULL;
        }
        sampleApp->offlineAlertPlaybackStartTime = 0;
        AiaTimer( Destroy )( &sampleApp->offlineAlertPlaybackTimer );
        return false;
    }

    AiaLogInfo( "Started offline alert playback timer" );

    return true;
}

static void onPlayOfflineAlertTone( void* userData )
{
    AiaSampleApp_t* sampleApp = (AiaSampleApp_t*)userData;
    AiaAssert( sampleApp );
    if( !sampleApp )
    {
        AiaLogError( "Null sampleApp" );
        return;
    }

    /* Quit early if speaker is not ready or if offline alert playback is
     * disabled */
    if( !AiaAtomicBool_Load( &sampleApp->isSpeakerReady ) )
    {
        return;
    }

    /* Pointer to the offline alert that should be played */
    AiaAlertSlot_t* offlineAlert = sampleApp->offlineAlertInProgress;

    /* Check if this alert exceeded its duration */
    AiaTimepointSeconds_t now = AiaClock_GetTimeSinceNTPEpoch();
    if( now >= offlineAlert->scheduledTime )
    {
        AiaDurationSeconds_t timeSinceAlertStart =
            now - offlineAlert->scheduledTime;
        if( timeSinceAlertStart * AIA_MS_PER_SECOND > offlineAlert->duration )
        {
            AiaLogDebug( "Offline alert expired, deleting it" );
            AiaAtomicBool_Set( &sampleApp->shouldDeleteOfflineAlert );
        }
    }

    /* Invoke the callback to delete the alert from local storage if it is
     * supposed to be deleted */
    if( AiaAtomicBool_Load( &sampleApp->shouldDeleteOfflineAlert ) )
    {
        if( !AiaClient_DeleteAlert( sampleApp->aiaClient,
                                    offlineAlert->alertToken ) )
        {
            AiaLogWarn( "Failed to delete offline alert" );
        }

        AiaTimer( Destroy )( &sampleApp->offlineAlertPlaybackTimer );
        if( !AiaTimer( Create )( &sampleApp->offlineAlertPlaybackTimer,
                                 onPlayOfflineAlertTone, sampleApp ) )
        {
            AiaLogError( "AiaTimer( Create ) failed" );
        }

        if( sampleApp->offlineAlertInProgress )
        {
            AiaFree( sampleApp->offlineAlertInProgress );
            sampleApp->offlineAlertInProgress = NULL;
        }
        sampleApp->offlineAlertPlaybackStartTime = 0;
        AiaAtomicBool_Clear( &sampleApp->shouldDeleteOfflineAlert );

        return;
    }

    /* Variables needed to play the offline alert tone */
    size_t numFrames = 0;
    size_t frameLength = 0;

    /* Check for correct alert type */
    switch( offlineAlert->alertType )
    {
        case AIA_ALERT_TYPE_ALARM:
        case AIA_ALERT_TYPE_REMINDER:
        case AIA_ALERT_TYPE_TIMER:
#ifdef AIA_PORTAUDIO_SPEAKER
            numFrames = AIA_SPEAKER_FRAME_PUSH_CADENCE_MS /
                        AIA_ALERT_TONE_FRAME_LENGTH_IN_MS;
            /* Make sure we are pushing at least one frame */
            numFrames = numFrames > 0 ? numFrames : 1;
#endif
            frameLength = AIA_ALERT_TONE_FRAME_LENGTH;
            break;
        default:
            AiaLogError( "Unknown alert type:%" PRIu32,
                         offlineAlert->alertType );
            if( sampleApp->offlineAlertInProgress )
            {
                AiaFree( sampleApp->offlineAlertInProgress );
                sampleApp->offlineAlertInProgress = NULL;
            }
            sampleApp->offlineAlertPlaybackStartTime = 0;
            return;
    }

    for( size_t i = 0; i < numFrames; ++i )
    {
#ifdef AIA_PORTAUDIO_SPEAKER
        if( !AiaPortAudioSpeaker_PlaySpeakerData(
                sampleApp->portAudioSpeaker, AIA_ALERT_TONE, frameLength ) )
        {
            AiaLogDebug( "Failed to play offline alert buffer" );
            AiaAtomicBool_Clear( &sampleApp->isSpeakerReady );
            return;
        }
#endif
    }

    return;
}
#endif

static void onUXStateChangedSimpleUI( AiaUXState_t state, void* userData )
{
    (void)userData;
    AiaLogInfo( "**** UX state changed, state=%s ****",
                AiaUXState_ToString( state ) );
}

static void printHelpMessageSimpleUI()
{
    puts( HELP_MESSAGE );
}

static void printCommandPrompt()
{
    fputs( COMMAND_PROMPT, stdout );
    fflush( stdout );
}
