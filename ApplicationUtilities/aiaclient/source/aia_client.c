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
 * @file aia_client.c
 * @brief Implements functions for the AiaClient_t type.
 */

/* The config header is always included first. */
#include <aia_capabilities_config.h>
#include <aia_config.h>

#include <aiaclient/aia_client.h>

#include <aiacore/aia_binary_message.h>
#include <aiacore/aia_button_command_sender.h>
#include <aiacore/aia_directive.h>
#include <aiacore/aia_events.h>
#include <aiacore/aia_json_constants.h>
#include <aiacore/aia_json_message.h>
#include <aiacore/capabilities_sender/aia_capabilities_sender.h>

#include <aiaalertmanager/private/aia_alert_manager.h>
#include <aiaclockmanager/aia_clock_manager.h>
#include <aiaclockmanager/private/aia_clock_manager.h>
#include <aiaconnectionmanager/aia_connection_manager.h>
#include <aiadispatcher/aia_dispatcher.h>
#include <aiaemitter/aia_emitter.h>
#include <aiaexceptionmanager/aia_exception_manager.h>
#include <aiamicrophonemanager/aia_microphone_manager.h>
#include <aiamicrophonemanager/private/aia_microphone_manager.h>
#include <aiaregulator/aia_regulator.h>
#include <aiasecretmanager/aia_secret_manager.h>
#include <aiasecretmanager/private/aia_secret_manager.h>
#include <aiasequencer/aia_sequencer.h>
#include <aiaspeakermanager/aia_speaker_manager.h>
#include <aiauxmanager/aia_ux_manager.h>
#include <aiauxmanager/private/aia_ux_manager.h>

#include <inttypes.h>
#include <stdio.h>

/**
 * Glue function which routes messages coming from an @c AiaRegulator_t to the
 * corresponding @c AiaEmitter_t.
 *
 * @param chunkForMessage The message chunk to be emitted.  If this function
 *     returns @c true, ownership of @c chunkForMessage is transferred to the
 *     emitter, and the emitter is responsible for destroying it.
 * @param remainingBytes The cumulative number of bytes that have not been
 *     emitted in the series yet; the final call in a series must set @c
 *     remainingBytes to zero.
 * @param remainingChunks The number of chunks that have not been emitted in the
 *     series yet; the final callback in a series must set @c remainingChunks to
 *     zero.
 * @param userData A pointer to the emitter to route @c chunkForMessage to.
 * @return @c true if @c chunkForMessage was successfully handed off to the
 *     emitter, else @c false.  As noted above, returning @c true indicates that
 *     the emitter is taking ownership of @c chunkForMessage, and is responsible
 *     for destroying it.
 */
static bool emitMessageChunk( AiaRegulatorChunk_t* chunkForMessage,
                              size_t remainingBytes, size_t remainingChunks,
                              void* userData )
{
    if( !userData )
    {
        AiaLogError( "Null userData." );
        return false;
    }
    AiaEmitter_t* emitter = (AiaEmitter_t*)userData;
    return AiaEmitter_EmitMessageChunk( emitter, chunkForMessage,
                                        remainingBytes, remainingChunks );
}

/**
 * Cleanup function for JSON messages that an @c AiaRegulator_t needs to dispose
 * of.
 *
 * @param chunk The @c AiaJsonMessage_t to be destroyed.
 * @param userData Optional user data pointer which is ignored by this
 *     implementation.
 */
static void destroyJsonChunk( AiaRegulatorChunk_t* chunk, void* userData )
{
    (void)userData;
    if( chunk )
    {
        AiaJsonMessage_Destroy( AiaJsonMessage_FromMessage( chunk ) );
    }
}

/**
 * Cleanup function for binary messages that an @c AiaRegulator_t needs to
 * dispose of.
 *
 * @param chunk The @c AiaBinaryMessage_t to be destroyed.
 * @param userData Optional user data pointer which is ignored by this
 *     implementation.
 */
static void destroyBinaryChunk( AiaRegulatorChunk_t* chunk, void* userData )
{
    (void)userData;
    if( chunk )
    {
        AiaBinaryMessage_Destroy( AiaBinaryMessage_FromMessage( chunk ) );
    }
}

/**
 * @copydoc AiaGetNextSequenceNumber
 */
static bool AiaClient_GetNextSequenceNumber(
    AiaTopic_t topic, AiaSequenceNumber_t* nextSequenceNumber, void* userData );

/**
 * @copydoc AiaEmitEvent
 */
static bool AiaClient_EmitEvent( AiaRegulatorChunk_t* chunk, void* userData );

/**
 * @copydoc AiaStopPlayback_t
 */
static void AiaClient_StopPlayback( void* userData );

/**
 * @copydoc AiaClockManagerNotifyObserver_t
 */
static void AiaClient_SynchronizeTimers( void* userData,
                                         AiaTimepointSeconds_t currentTime );

#ifdef AIA_ENABLE_SPEAKER
/**
 * @copydoc AiaSpeakerManagerBufferStateObserver_t
 */
static void AiaClient_SynchronizeSpeakerBuffer(
    void* userData, AiaSpeakerManagerBufferState_t currentBufferState );

/**
 * @copydoc AiaSpeakerCanStreamCb_t
 */
static bool AiaClient_CanSpeakerStream( void* userData );
#endif

/**
 * @copydoc AiaUXStateObserverCb_t
 */
static void AiaClient_UXStateObserver( AiaUXState_t state, void* userData );

/**
 * @copydoc AiaUXStateObserver_t
 */
static AiaUXState_t AiaClient_CheckUXState( void* userData );

/**
 * @copydoc AiaUXServerAttentionStateUpdateCb_t
 */
static void AiaClient_UpdateServerAttentionState(
    void* userData, AiaServerAttentionState_t newAttentionState );

#ifdef AIA_ENABLE_SPEAKER
/**
 * @copydoc AiaOfflineAlertStart_t
 */
static bool AiaClient_StartOfflineAlertTone( const AiaAlertSlot_t* offlineAlert,
                                             void* userData,
                                             uint8_t offlineAlertVolume );
#endif

/** Holds state information for an @c AiaClient_t instance. */
struct AiaClient
{
    /** Manages shared secrets and rotations for this client. */
    AiaSecretManager_t* const secretManager;

    /** Regulator for @c AIA_TOPIC_CAPABILITIES_PUBLISH messages. */
    AiaRegulator_t* capabiliitiesPublishRegulator;

    /** Regulator for @c AIA_TOPIC_EVENT messages. */
    AiaRegulator_t* eventRegulator;

    /** Emitter for @c AIA_TOPIC_CAPABILITIES_PUBLISH messages. */
    AiaEmitter_t* const capabilitiesPublishEmitter;

    /** Emitter for @c AIA_TOPIC_EVENT messages. */
    AiaEmitter_t* const eventEmitter;

    /** Used for publishing capabilities. */
    AiaCapabilitiesSender_t* capabilitiesSender;

    /** Manages the Aia connection. */
    AiaConnectionManager_t* connectionManager;

    /** Manages exceptions */
    AiaExceptionManager_t* exceptionManager;

    /** Used to parse and distribute messages on subscribed topics. */
    AiaDispatcher_t* dispatcher;

#ifdef AIA_ENABLE_SPEAKER
    /** Manages messages flowing on @c AIA_TOPIC_SPEAKER. */
    AiaSpeakerManager_t* speakerManager;
#endif

    /** Manages and aggregates the end user UX state. */
    AiaUXManager_t* uxManager;

    /** UX state observer callback for the application. */
    AiaUXStateObserverCb_t const uxStateObserverCb;

    /** Context to pass along to @c uxStateObserverCb. */
    void* const uxStateObserverCbUserData;

#ifdef AIA_ENABLE_ALERTS
    /** Manages alert messages flowing on @c AIA_TOPIC_DIRECTIVE. */
    AiaAlertManager_t* alertManager;
#endif

#ifdef AIA_ENABLE_MICROPHONE
    /** Manages messages flowing on @c AIA_TOPIC_MICROPHONE. */
    AiaMicrophoneManager_t* microphoneManager;

    /** Regulator for @c AIA_TOPIC_MICROPHONE messages. */
    AiaRegulator_t* microphoneRegulator;

    /** Emitter for @c AIA_TOPIC_MICROPHONE messages. */
    AiaEmitter_t* const microphoneEmitter;
#endif

    /** Used to act on user-initiated button presses. */
    AiaButtonCommandSender_t* buttonCommandSender;

#ifdef AIA_ENABLE_CLOCK
    /** Used to synchronize the device's clock with the AIA service. */
    AiaClockManager_t* clockManager;
#endif
};

AiaClient_t* AiaClient_Create(
    AiaMqttConnectionPointer_t mqttConnection,
    AiaConnectionManageronConnectionSuccessCallback_t onConnectionSuccess,
    AiaConnectionManagerOnConnectionRejectionCallback_t onConnectionRejected,
    AiaConnectionManagerOnDisconnectedCallback_t onDisconnected,
    void* connectionUserData, AiaTaskPool_t aiaTaskPool,
    AiaExceptionManagerOnExceptionCallback_t onException,
    void* onExceptionUserData,
    AiaCapabilitiesObserver_t capabilitiesStateObserver,
    void* capabilitiesStateObserverUserData
#ifdef AIA_ENABLE_SPEAKER
    ,
    AiaPlaySpeakerData_t receiveSpeakerFramesCb,
    void* receiveSpeakerFramesCbUserData, AiaSetVolume_t setVolumeCb,
    void* setVolumeCbUserData, AiaOfflineAlertPlayback_t playOfflineAlertCb,
    void* playOfflineAlertCbUserData, AiaOfflineAlertStop_t stopOfflineAlertCb,
    void* stopOfflineAlertCbUserData
#endif
    ,
    AiaUXStateObserverCb_t uxObserver, void* uxObserverUserData
#ifdef AIA_ENABLE_MICROPHONE
    ,
    AiaDataStreamReader_t* microphoneBufferReader
#endif
)
{
    if( !mqttConnection )
    {
        AiaLogError( "Null mqttConnection" );
        return NULL;
    }

    AiaClient_t* client = AiaCalloc( 1, sizeof( AiaClient_t ) );
    if( !client )
    {
        AiaLogError( "AiaCalloc failed, bytes=%zu", sizeof( AiaClient_t ) );
        return NULL;
    }

    *(AiaUXStateObserverCb_t*)&client->uxStateObserverCb = uxObserver;
    *(void**)&client->uxStateObserverCbUserData = uxObserverUserData;

    *(AiaSecretManager_t**)&client->secretManager = AiaSecretManager_Create(
        AiaClient_GetNextSequenceNumber, client, AiaClient_EmitEvent, client );
    if( !client->secretManager )
    {
        AiaLogError( "AiaSecretManager_Create failed to create secretManager" );
        AiaClient_Destroy( client );
        return NULL;
    }

    *(AiaEmitter_t**)&client->eventEmitter = AiaEmitter_Create(
        mqttConnection, client->secretManager, AIA_TOPIC_EVENT );
    if( !client->eventEmitter )
    {
        AiaLogError( "AiaEmitter_Create failed to create eventEmitter" );
        AiaClient_Destroy( client );
        return NULL;
    }

    *(AiaEmitter_t**)&client->capabilitiesPublishEmitter = AiaEmitter_Create(
        mqttConnection, client->secretManager, AIA_TOPIC_CAPABILITIES_PUBLISH );
    if( !client->capabilitiesPublishEmitter )
    {
        AiaLogError(
            "AiaEmitter_Create failed to create capabilitiesPublishEmitter" );
        AiaClient_Destroy( client );
        return NULL;
    }

    client->eventRegulator =
        AiaRegulator_Create( AIA_SYSTEM_MQTT_MESSAGE_MAX_SIZE, emitMessageChunk,
                             client->eventEmitter, EVENT_PUBLISH_RATE );
    if( !client->eventRegulator )
    {
        AiaLogError( "AiaRegulator_Create failed to create eventRegulator" );
        AiaClient_Destroy( client );
        return NULL;
    }
    AiaRegulator_SetEmitMode( client->eventRegulator, AIA_REGULATOR_TRICKLE );

    client->capabiliitiesPublishRegulator = AiaRegulator_Create(
        AIA_SYSTEM_MQTT_MESSAGE_MAX_SIZE, emitMessageChunk,
        client->capabilitiesPublishEmitter, EVENT_PUBLISH_RATE );
    if( !client->capabiliitiesPublishRegulator )
    {
        AiaLogError(
            "AiaRegulator_Create failed to create "
            "capabiliitiesPublishRegulator" );
        AiaClient_Destroy( client );
        return NULL;
    }
    AiaRegulator_SetEmitMode( client->capabiliitiesPublishRegulator,
                              AIA_REGULATOR_TRICKLE );

    client->capabilitiesSender = AiaCapabilitiesSender_Create(
        client->capabiliitiesPublishRegulator, capabilitiesStateObserver,
        capabilitiesStateObserverUserData );
    if( !client->capabilitiesSender )
    {
        AiaLogError( "AiaCapabilitiesSender_Create failed" );
        AiaClient_Destroy( client );
        return NULL;
    }

    client->dispatcher =
        AiaDispatcher_Create( aiaTaskPool, client->capabilitiesSender,
                              client->eventRegulator, client->secretManager );
    if( !client->dispatcher )
    {
        AiaLogError( "AiaDispatcher_Create failed" );
        AiaClient_Destroy( client );
        return NULL;
    }

    client->connectionManager = AiaConnectionManager_Create(
        onConnectionSuccess, connectionUserData, onConnectionRejected,
        connectionUserData, onDisconnected, connectionUserData,
        messageReceivedCallback, client->dispatcher, mqttConnection,
        aiaTaskPool );
    if( !client->connectionManager )
    {
        AiaLogError( "AiaConnectionManager_Create failed" );
        AiaClient_Destroy( client );
        return NULL;
    }

    client->exceptionManager = AiaExceptionManager_Create(
        client->eventRegulator, onException, onExceptionUserData );
    if( !client->exceptionManager )
    {
        AiaLogError( "AiaExceptionManager_Create failed" );
        AiaClient_Destroy( client );
        return NULL;
    }

#ifdef AIA_ENABLE_SPEAKER
    /* TODO: ADSER-1757 Investigate moving speakerMessageSequencedCb within
     * speakerManager */
    const AiaDispatcher_t* dispatcher = client->dispatcher;
    client->speakerManager = AiaSpeakerManager_Create(
        AIA_AUDIO_BUFFER_SIZE, AIA_AUDIO_BUFFER_OVERRUN_WARN_THRESHOLD,
        AIA_AUDIO_BUFFER_UNDERRUN_WARN_THRESHOLD, receiveSpeakerFramesCb,
        receiveSpeakerFramesCbUserData, dispatcher->speakerSequencer,
        client->eventRegulator, setVolumeCb, setVolumeCbUserData,
        playOfflineAlertCb, playOfflineAlertCbUserData, stopOfflineAlertCb,
        stopOfflineAlertCbUserData, AiaClient_SynchronizeSpeakerBuffer,
        client );
    if( !client->speakerManager )
    {
        AiaLogError( "AiaSpeakerManager_Create failed" );
        AiaClient_Destroy( client );
        return NULL;
    }

#endif

    client->uxManager = AiaUXManager_Create( client->eventRegulator,
                                             AiaClient_UXStateObserver, client
#ifdef AIA_ENABLE_SPEAKER
                                             ,
                                             client->speakerManager
#endif
    );
    if( !client->uxManager )
    {
        AiaLogError( "AiaUXManager_Create failed" );
        AiaClient_Destroy( client );
        return NULL;
    }

#ifdef AIA_ENABLE_MICROPHONE

    *(AiaEmitter_t**)&client->microphoneEmitter = AiaEmitter_Create(
        mqttConnection, client->secretManager, AIA_TOPIC_MICROPHONE );
    if( !client->microphoneEmitter )
    {
        AiaLogError( "AiaEmitter_Create failed to create microphoneEmitter" );
        AiaClient_Destroy( client );
        return NULL;
    }

    client->microphoneRegulator = AiaRegulator_Create(
        AIA_SYSTEM_MQTT_MESSAGE_MAX_SIZE, emitMessageChunk,
        client->microphoneEmitter, MICROPHONE_PUBLISH_RATE );
    if( !client->microphoneRegulator )
    {
        AiaLogError(
            "AiaRegulator_Create failed to create microphoneRegulator" );
        AiaClient_Destroy( client );
        return NULL;
    }
    AiaRegulator_SetEmitMode( client->microphoneRegulator,
                              AIA_REGULATOR_BURST );

    client->microphoneManager = AiaMicrophoneManager_Create(
        client->eventRegulator, client->microphoneRegulator,
        microphoneBufferReader, AiaUXManager_OnMicrophoneStateChange,
        client->uxManager );
    if( !client->microphoneManager )
    {
        AiaLogError( "AiaMicrophoneManager_Create failed" );
        AiaClient_Destroy( client );
        return NULL;
    }

#endif

    client->buttonCommandSender =
        AiaButtonCommandSender_Create( client->eventRegulator,
#ifdef AIA_BUTTON_LOCAL_STOP
                                       AiaClient_StopPlayback, client
#else
                                       NULL, NULL
#endif
        );
    if( !client->buttonCommandSender )
    {
        AiaLogError( "AiaButtonCommandSender_Create failed" );
        AiaClient_Destroy( client );
        return NULL;
    }

#ifdef AIA_ENABLE_ALERTS
    client->alertManager = AiaAlertManager_Create(
        client->eventRegulator
#ifdef AIA_ENABLE_SPEAKER
        ,
        AiaClient_CanSpeakerStream, client, AiaClient_StartOfflineAlertTone,
        client
#endif
        ,
        AiaClient_UpdateServerAttentionState, client, AiaClient_CheckUXState,
        client, AiaClient_Disconnect, client );
    if( !client->alertManager )
    {
        AiaLogError( "AiaAlertManager_Create failed" );
        AiaClient_Destroy( client );
        return NULL;
    }
#endif

#ifdef AIA_ENABLE_CLOCK
    client->clockManager = AiaClockManager_Create(
        client->eventRegulator, AiaClient_SynchronizeTimers, client );
    if( !client->clockManager )
    {
        AiaLogError( "AiaClockManager_Create failed" );
        AiaClient_Destroy( client );
        return NULL;
    }
#endif

    /* Fill in the fields of dispatcher. */
    AiaDispatcher_AddConnectionManager( client->dispatcher,
                                        client->connectionManager );
#ifdef AIA_ENABLE_SPEAKER
    AiaDispatcher_AddSpeakerManager( client->dispatcher,
                                     client->speakerManager );

    if( !AiaDispatcher_AddHandler(
            client->dispatcher,
            AiaSpeakerManager_OnOpenSpeakerDirectiveReceived,
            AIA_DIRECTIVE_OPEN_SPEAKER, client->speakerManager ) )
    {
        AiaLogError( "Failed to add handler for %s directive",
                     AiaDirective_ToString( AIA_DIRECTIVE_OPEN_SPEAKER ) );
        AiaClient_Destroy( client );
        return NULL;
    }

    if( !AiaDispatcher_AddHandler(
            client->dispatcher,
            AiaSpeakerManager_OnCloseSpeakerDirectiveReceived,
            AIA_DIRECTIVE_CLOSE_SPEAKER, client->speakerManager ) )
    {
        AiaLogError( "Failed to add handler for %s directive",
                     AiaDirective_ToString( AIA_DIRECTIVE_CLOSE_SPEAKER ) );
        AiaClient_Destroy( client );
        return NULL;
    }

    if( !AiaDispatcher_AddHandler(
            client->dispatcher, AiaSpeakerManager_OnSetVolumeDirectiveReceived,
            AIA_DIRECTIVE_SET_VOLUME, client->speakerManager ) )
    {
        AiaLogError( "Failed to add handler for %s directive",
                     AiaDirective_ToString( AIA_DIRECTIVE_SET_VOLUME ) );
        AiaClient_Destroy( client );
        return NULL;
    }
#endif
#ifdef AIA_ENABLE_ALERTS
    if( !AiaDispatcher_AddHandler(
            client->dispatcher,
            AiaAlertManager_OnSetAlertVolumeDirectiveReceived,
            AIA_DIRECTIVE_SET_ALERT_VOLUME, client->alertManager ) )
    {
        AiaLogError( "Failed to add handler for %s directive",
                     AiaDirective_ToString( AIA_DIRECTIVE_SET_ALERT_VOLUME ) );
        AiaClient_Destroy( client );
        return NULL;
    }

    if( !AiaDispatcher_AddHandler(
            client->dispatcher, AiaAlertManager_OnSetAlertDirectiveReceived,
            AIA_DIRECTIVE_SET_ALERT, client->alertManager ) )
    {
        AiaLogError( "Failed to add handler for %s directive",
                     AiaDirective_ToString( AIA_DIRECTIVE_SET_ALERT ) );
        AiaClient_Destroy( client );
        return NULL;
    }

    if( !AiaDispatcher_AddHandler(
            client->dispatcher, AiaAlertManager_OnDeleteAlertDirectiveReceived,
            AIA_DIRECTIVE_DELETE_ALERT, client->alertManager ) )
    {
        AiaLogError( "Failed to add handler for %s directive",
                     AiaDirective_ToString( AIA_DIRECTIVE_DELETE_ALERT ) );
        AiaClient_Destroy( client );
        return NULL;
    }
#endif
#ifdef AIA_ENABLE_MICROPHONE
    if( !AiaDispatcher_AddHandler(
            client->dispatcher,
            AiaMicrophoneManager_OnOpenMicrophoneDirectiveReceived,
            AIA_DIRECTIVE_OPEN_MICROPHONE, client->microphoneManager ) )
    {
        AiaLogError( "Failed to add handler for %s directive",
                     AiaDirective_ToString( AIA_DIRECTIVE_OPEN_MICROPHONE ) );
        AiaClient_Destroy( client );
        return NULL;
    }

    if( !AiaDispatcher_AddHandler(
            client->dispatcher,
            AiaMicrophoneManager_OnCloseMicrophoneDirectiveReceived,
            AIA_DIRECTIVE_CLOSE_MICROPHONE, client->microphoneManager ) )
    {
        AiaLogError( "Failed to add handler for %s directive",
                     AiaDirective_ToString( AIA_DIRECTIVE_CLOSE_MICROPHONE ) );
        AiaClient_Destroy( client );
        return NULL;
    }
#endif
    if( !AiaDispatcher_AddHandler(
            client->dispatcher,
            AiaUXManager_OnSetAttentionStateDirectiveReceived,
            AIA_DIRECTIVE_SET_ATTENTION_STATE, client->uxManager ) )
    {
        AiaLogError(
            "Failed to add handler for %s directive",
            AiaDirective_ToString( AIA_DIRECTIVE_SET_ATTENTION_STATE ) );
        AiaClient_Destroy( client );
        return NULL;
    }

    if( !AiaDispatcher_AddHandler(
            client->dispatcher,
            AiaSecretManager_OnRotateSecretDirectiveReceived,
            AIA_DIRECTIVE_ROTATE_SECRET, client->secretManager ) )
    {
        AiaLogError( "Failed to add handler for %s directive",
                     AiaDirective_ToString( AIA_DIRECTIVE_ROTATE_SECRET ) );
        AiaClient_Destroy( client );
        return NULL;
    }

    if( !AiaDispatcher_AddHandler(
            client->dispatcher, AiaExceptionManager_OnExceptionReceived,
            AIA_DIRECTIVE_EXCEPTION, client->exceptionManager ) )
    {
        AiaLogError( "Failed to add handler for %s directive",
                     AiaDirective_ToString( AIA_DIRECTIVE_EXCEPTION ) );
        AiaClient_Destroy( client );
        return NULL;
    }

#ifdef AIA_ENABLE_CLOCK
    if( !AiaDispatcher_AddHandler(
            client->dispatcher, AiaClockManager_OnSetClockDirectiveReceived,
            AIA_DIRECTIVE_SET_CLOCK, client->clockManager ) )
    {
        AiaLogError( "Failed to add handler for %s directive",
                     AiaDirective_ToString( AIA_DIRECTIVE_SET_CLOCK ) );
        AiaClient_Destroy( client );
        return NULL;
    }
#endif

    return client;
}

void AiaClient_Destroy( AiaClient_t* aiaClient )
{
    if( !aiaClient )
    {
        AiaLogDebug( "Null aiaClient." );
        return;
    }

#ifdef AIA_ENABLE_ALERTS
    AiaAlertManager_Destroy( aiaClient->alertManager );
#endif
#ifdef AIA_ENABLE_CLOCK
    AiaClockManager_Destroy( aiaClient->clockManager );
#endif

    AiaButtonCommandSender_Destroy( aiaClient->buttonCommandSender );
#ifdef AIA_ENABLE_MICROPHONE
    AiaMicrophonerManager_Destroy( aiaClient->microphoneManager );
    AiaRegulator_Destroy( aiaClient->microphoneRegulator, destroyBinaryChunk,
                          NULL );
    AiaEmitter_Destroy( aiaClient->microphoneEmitter );
#endif
    AiaUXManager_Destroy( aiaClient->uxManager );
#ifdef AIA_ENABLE_SPEAKER
    AiaSpeakerManager_Destroy( aiaClient->speakerManager );
#endif
    AiaCapabilitiesSender_Destroy( aiaClient->capabilitiesSender );
    AiaDispatcher_Destroy( aiaClient->dispatcher );
    AiaExceptionManager_Destroy( aiaClient->exceptionManager );
    AiaConnectionManager_Destroy( aiaClient->connectionManager );
    AiaRegulator_Destroy( aiaClient->capabiliitiesPublishRegulator,
                          destroyJsonChunk, NULL );
    AiaRegulator_Destroy( aiaClient->eventRegulator, destroyJsonChunk, NULL );
    AiaEmitter_Destroy( aiaClient->eventEmitter );
    AiaEmitter_Destroy( aiaClient->capabilitiesPublishEmitter );
    AiaSecretManager_Destroy( aiaClient->secretManager );
    AiaFree( aiaClient );
}

bool AiaClient_Connect( AiaClient_t* aiaClient )
{
    AiaAssert( aiaClient );
    if( !aiaClient )
    {
        AiaLogError( "Null aiaClient" );
        return false;
    }

    return AiaConnectionManager_Connect( aiaClient->connectionManager );
}

bool AiaClient_Disconnect( void* userData, int code, const char* description )
{
    AiaClient_t* aiaClient = (AiaClient_t*)userData;
    AiaAssert( aiaClient );
    if( !aiaClient )
    {
        AiaLogWarn( "Null aiaClient" );
        return false;
    }

    /* TODO: ADSER-1685 Use code, refactor connectionManager to take in enums
     * rather than c strings for code. */
    (void)code;
    return AiaConnectionManager_Disconnect(
        aiaClient->connectionManager, AIA_CONNECTION_DISCONNECT_GOING_OFFLINE,
        description );
}

bool AiaClient_PublishCapabilities( AiaClient_t* aiaClient )
{
    AiaAssert( aiaClient );
    if( !aiaClient )
    {
        AiaLogError( "Null aiaClient" );
        return false;
    }

    return AiaCapabilitiesSender_PublishCapabilities(
        aiaClient->capabilitiesSender );
}

#ifdef AIA_ENABLE_SPEAKER

void AiaClient_StopSpeaker( AiaClient_t* aiaClient )
{
    AiaAssert( aiaClient );
    if( !aiaClient )
    {
        AiaLogWarn( "Null aiaClient" );
        return;
    }
    AiaSpeakerManager_StopPlayback( aiaClient->speakerManager );
}

void AiaClient_OnSpeakerReady( AiaClient_t* aiaClient )
{
    AiaAssert( aiaClient );
    if( !aiaClient )
    {
        AiaLogWarn( "Null aiaClient" );
        return;
    }
    AiaSpeakerManager_OnSpeakerReady( aiaClient->speakerManager );
}

bool AiaClient_ChangeVolume( AiaClient_t* aiaClient, uint8_t newVolume )
{
    AiaAssert( aiaClient );
    if( !aiaClient )
    {
        AiaLogWarn( "Null aiaClient" );
        return false;
    }
    return AiaSpeakerManager_ChangeVolume( aiaClient->speakerManager,
                                           newVolume );
}

bool AiaClient_AdjustVolume( AiaClient_t* aiaClient, int8_t delta )
{
    AiaAssert( aiaClient );
    if( !aiaClient )
    {
        AiaLogWarn( "Null aiaClient" );
        return false;
    }
    return AiaSpeakerManager_AdjustVolume( aiaClient->speakerManager, delta );
}

#endif

#ifdef AIA_ENABLE_MICROPHONE

bool AiaClient_HoldToTalkStart( AiaClient_t* aiaClient,
                                AiaBinaryAudioStreamOffset_t index )
{
    AiaAssert( aiaClient );
    if( !aiaClient )
    {
        AiaLogWarn( "Null aiaClient" );
        return false;
    }
    return AiaMicrophoneManager_HoldToTalkStart( aiaClient->microphoneManager,
                                                 index );
}

void AiaClient_CloseMicrophone( AiaClient_t* aiaClient )
{
    AiaAssert( aiaClient );
    if( !aiaClient )
    {
        AiaLogWarn( "Null aiaClient" );
        return;
    }
    AiaMicrophoneManager_CloseMicrophone( aiaClient->microphoneManager );
}

bool AiaClient_TapToTalkStart( AiaClient_t* aiaClient,
                               AiaBinaryAudioStreamOffset_t index,
                               AiaMicrophoneProfile_t profile )
{
    AiaAssert( aiaClient );
    if( !aiaClient )
    {
        AiaLogWarn( "Null aiaClient" );
        return false;
    }
    return AiaMicrophoneManager_TapToTalkStart( aiaClient->microphoneManager,
                                                index, profile );
}

bool AiaClient_WakeWordStart( AiaClient_t* aiaClient,
                              AiaBinaryAudioStreamOffset_t beginIndex,
                              AiaBinaryAudioStreamOffset_t endIndex,
                              AiaMicrophoneProfile_t profile,
                              const char* wakeWord )
{
    AiaAssert( aiaClient );
    if( !aiaClient )
    {
        AiaLogWarn( "Null aiaClient" );
        return false;
    }
    return AiaMicrophoneManager_WakeWordStart(
        aiaClient->microphoneManager, beginIndex, endIndex, profile, wakeWord );
}
#endif

/**
 * Generates a @c SynchronizeState event for publishing to the @c
 * AiaRegulator_t.
 *
 * @param aiaClient The @c AiaClient_t to act on.
 * @return The generated @c AiaJsonMessage_t or @c NULL on failures.
 */
static AiaJsonMessage_t* generateSynchronizeStateEvent( AiaClient_t* aiaClient )
{
#ifndef AIA_ENABLE_ALERTS
    (void)aiaClient;
#endif
    static const char* AIA_SYNCHRONIZE_STATE_EVENT_FORMAT =
        /* clang-format off */
        "{"
#if defined AIA_ENABLE_SPEAKER && defined AIA_LOAD_VOLUME
            "\""AIA_SYNCHRONIZE_STATE_EVENT_SPEAKER_KEY"\":{"
                "\""AIA_SYNCHRONIZE_STATE_EVENT_SPEAKER_VOLUME_KEY"\":%"PRIu64
            "}"
#endif
#ifdef AIA_ENABLE_ALERTS
#if defined AIA_ENABLE_SPEAKER && defined AIA_LOAD_VOLUME
            ","
#endif
            "\""AIA_SYNCHRONIZE_STATE_EVENT_ALERTS_KEY"\":{"
                "\""AIA_SYNCHRONIZE_STATE_EVENT_ALERTS_ALL_ALERTS_KEY"\":[%.*s]"
            "}"
#endif
        "}";
    /* clang-format on */

#if defined AIA_ENABLE_SPEAKER && defined AIA_LOAD_VOLUME
    AiaJsonLongType storedVolume = (AiaJsonLongType)AIA_LOAD_VOLUME();
#endif
#ifdef AIA_ENABLE_ALERTS
    uint8_t* alertsArray;
    size_t alertsArrayLen =
        AiaAlertManager_GetTokens( aiaClient->alertManager, &alertsArray );
#endif

    /* TODO: ADSER-1948 Prepare SynchronizeState event payload with
     * concatenations */
#if defined AIA_ENABLE_SPEAKER && defined AIA_LOAD_VOLUME && \
    !defined AIA_ENABLE_ALERTS
    int numCharsRequired =
        snprintf( NULL, 0, AIA_SYNCHRONIZE_STATE_EVENT_FORMAT, storedVolume );
    if( numCharsRequired < 0 )
    {
        AiaLogError( "snprintf failed" );
        return NULL;
    }
    char payloadBuffer[ numCharsRequired + 1 ];
    if( snprintf( payloadBuffer, numCharsRequired + 1,
                  AIA_SYNCHRONIZE_STATE_EVENT_FORMAT, storedVolume ) < 0 )
    {
        AiaLogError( "snprintf failed" );
        return NULL;
    }
#endif
#if( !defined AIA_ENABLE_SPEAKER || !defined AIA_LOAD_VOLUME ) && \
    defined AIA_ENABLE_ALERTS
    int numCharsRequired =
        snprintf( NULL, 0, AIA_SYNCHRONIZE_STATE_EVENT_FORMAT, alertsArrayLen,
                  alertsArray );
    if( numCharsRequired < 0 )
    {
        AiaFree( alertsArray );
        AiaLogError( "snprintf failed" );
        return NULL;
    }
    char payloadBuffer[ numCharsRequired + 1 ];
    if( snprintf( payloadBuffer, numCharsRequired + 1,
                  AIA_SYNCHRONIZE_STATE_EVENT_FORMAT, alertsArrayLen,
                  alertsArray ) < 0 )
    {
        AiaFree( alertsArray );
        AiaLogError( "snprintf failed" );
        return NULL;
    }
#endif
#if defined AIA_ENABLE_SPEAKER && defined AIA_LOAD_VOLUME && \
    defined AIA_ENABLE_ALERTS
    int numCharsRequired =
        snprintf( NULL, 0, AIA_SYNCHRONIZE_STATE_EVENT_FORMAT, storedVolume,
                  alertsArrayLen, alertsArray );
    if( numCharsRequired < 0 )
    {
        AiaFree( alertsArray );
        AiaLogError( "snprintf failed" );
        return NULL;
    }
    char payloadBuffer[ numCharsRequired + 1 ];
    if( snprintf( payloadBuffer, numCharsRequired + 1,
                  AIA_SYNCHRONIZE_STATE_EVENT_FORMAT, storedVolume,
                  alertsArrayLen, alertsArray ) < 0 )
    {
        AiaFree( alertsArray );
        AiaLogError( "snprintf failed" );
        return NULL;
    }
#endif
#if !defined AIA_ENABLE_SPEAKER && !defined AIA_ENABLE_ALERTS
    char payloadBuffer[ sizeof( AIA_SYNCHRONIZE_STATE_EVENT_FORMAT ) ];
    memcpy( payloadBuffer, AIA_SYNCHRONIZE_STATE_EVENT_FORMAT,
            sizeof( AIA_SYNCHRONIZE_STATE_EVENT_FORMAT ) - 1 );
    payloadBuffer[ sizeof( AIA_SYNCHRONIZE_STATE_EVENT_FORMAT ) - 1 ] = '\0';
#endif

    AiaJsonMessage_t* jsonMessage = AiaJsonMessage_Create(
        AIA_EVENTS_SYNCHRONIZE_STATE, NULL, payloadBuffer );
#ifdef AIA_ENABLE_ALERTS
    AiaFree( alertsArray );
#endif
    return jsonMessage;
}

bool AiaClient_SynchronizeState( AiaClient_t* aiaClient )
{
    AiaAssert( aiaClient );
    if( !aiaClient )
    {
        AiaLogError( "Null aiaClient" );
        return false;
    }
#ifdef AIA_ENABLE_ALERTS
    AiaJsonMessage_t* synchronizeStateEvent =
        generateSynchronizeStateEvent( aiaClient );
#else
    AiaJsonMessage_t* synchronizeStateEvent = generateSynchronizeStateEvent();
#endif
    if( !synchronizeStateEvent )
    {
        AiaLogError( "Failed to create SynchronizeState event" );
        return false;
    }
    if( !AiaRegulator_Write(
            aiaClient->eventRegulator,
            AiaJsonMessage_ToMessage( synchronizeStateEvent ) ) )
    {
        AiaLogError( "Failed to write to regulator." );
        AiaJsonMessage_Destroy( synchronizeStateEvent );
        return false;
    }
    return true;
}

bool AiaClient_OnButtonPressed( AiaClient_t* aiaClient,
                                AiaButtonCommand_t button )
{
    AiaAssert( aiaClient );
    if( !aiaClient )
    {
        AiaLogError( "Null aiaClient" );
        return false;
    }
    return AiaButtonCommandSender_OnButtonPressed(
        aiaClient->buttonCommandSender, button );
}

#ifdef AIA_ENABLE_CLOCK
bool AiaClient_SynchronizeClock( AiaClient_t* aiaClient )
{
    AiaAssert( aiaClient );
    if( !aiaClient )
    {
        AiaLogError( "Null aiaClient" );
        return false;
    }
    return AiaClockManager_SynchronizeClock( aiaClient->clockManager );
}
#endif

#ifdef AIA_ENABLE_ALERTS
bool AiaClient_DeleteAlert( AiaClient_t* aiaClient, const char* alertToken )
{
    AiaAssert( aiaClient );
    if( !aiaClient )
    {
        AiaLogError( "Null aiaClient" );
        return false;
    }

#ifdef AIA_ENABLE_SPEAKER
    AiaSpeakerManager_StopOfflineAlert( aiaClient->speakerManager );
#endif

    AiaClient_UpdateServerAttentionState( aiaClient, AIA_ATTENTION_STATE_IDLE );

    return AiaAlertManager_DeleteAlert( aiaClient->alertManager, alertToken );
}
#endif

bool AiaClient_GetNextSequenceNumber( AiaTopic_t topic,
                                      AiaSequenceNumber_t* nextSequenceNumber,
                                      void* userData )
{
    AiaClient_t* client = (AiaClient_t*)userData;
    if( !client )
    {
        AiaLogError( "Null client" );
        return false;
    }
    switch( topic )
    {
        case AIA_TOPIC_CONNECTION_FROM_CLIENT:
            return false;
        case AIA_TOPIC_CONNECTION_FROM_SERVICE:
            return false;
        case AIA_TOPIC_CAPABILITIES_PUBLISH:
            return AiaEmitter_GetNextSequenceNumber(
                client->capabilitiesPublishEmitter, nextSequenceNumber );
        case AIA_TOPIC_CAPABILITIES_ACKNOWLEDGE:
            return false;
        case AIA_TOPIC_DIRECTIVE:
            return false;
        case AIA_TOPIC_EVENT:
            return AiaEmitter_GetNextSequenceNumber( client->eventEmitter,
                                                     nextSequenceNumber );
        case AIA_TOPIC_MICROPHONE:
            return AiaEmitter_GetNextSequenceNumber( client->microphoneEmitter,
                                                     nextSequenceNumber );
        case AIA_TOPIC_SPEAKER:
            return false;
        case AIA_NUM_TOPICS:
            break;
    }
    AiaLogError( "Unknown topic %d.", topic );
    AiaAssert( false );
    return false;
}

static bool AiaClient_EmitEvent( AiaRegulatorChunk_t* chunk, void* userData )
{
    AiaClient_t* client = (AiaClient_t*)userData;
    if( !client )
    {
        AiaLogError( "Null client" );
        return false;
    }
    return AiaRegulator_Write( client->eventRegulator, chunk );
}

static void AiaClient_StopPlayback( void* userData )
{
    AiaClient_t* client = (AiaClient_t*)userData;
    if( !client )
    {
        AiaLogError( "Null client" );
        return;
    }
#ifdef AIA_ENABLE_SPEAKER
    AiaClient_StopSpeaker( client );
#endif
}

static void AiaClient_SynchronizeTimers( void* userData,
                                         AiaTimepointSeconds_t currentTime )
{
    AiaClient_t* client = (AiaClient_t*)userData;
    if( !client )
    {
        AiaLogError( "Null client" );
        return;
    }
#ifdef AIA_ENABLE_ALERTS
    if( !AiaAlertManager_UpdateAlertManagerTime( client->alertManager,
                                                 currentTime ) )
    {
        AiaLogError( "AiaAlertManager_UpdateAlertManagerTime failed" );
        return;
    }
#endif
}

#ifdef AIA_ENABLE_SPEAKER
static void AiaClient_SynchronizeSpeakerBuffer(
    void* userData, AiaSpeakerManagerBufferState_t currentBufferState )
{
    AiaClient_t* client = (AiaClient_t*)userData;
    if( !client )
    {
        AiaLogError( "Null client" );
        return;
    }
#ifdef AIA_ENABLE_ALERTS
    if( client->alertManager )
    {
        AiaAlertManager_UpdateSpeakerBufferState( client->alertManager,
                                                  currentBufferState );
    }
#endif
}

static bool AiaClient_CanSpeakerStream( void* userData )
{
    AiaClient_t* client = (AiaClient_t*)userData;
    if( !client )
    {
        AiaLogError( "Null client" );
        return false;
    }

    return AiaSpeakerManager_CanSpeakerStream( client->speakerManager );
}
#endif

static void AiaClient_UXStateObserver( AiaUXState_t state, void* userData )
{
    AiaClient_t* client = (AiaClient_t*)userData;
    if( !client )
    {
        AiaLogError( "Null client" );
        return;
    }

    client->uxStateObserverCb( state, client->uxStateObserverCbUserData );

#ifdef AIA_ENABLE_ALERTS
    if( client->alertManager )
    {
        AiaAlertManager_UpdateUXState( client->alertManager, state );
    }
#endif
}

static AiaUXState_t AiaClient_CheckUXState( void* userData )
{
    AiaClient_t* client = (AiaClient_t*)userData;
    if( !client )
    {
        AiaLogError( "Null client" );
        return AIA_UX_IDLE;
    }

    return AiaUXManager_GetUXState( client->uxManager );
}

static void AiaClient_UpdateServerAttentionState(
    void* userData, AiaServerAttentionState_t newAttentionState )
{
    AiaClient_t* client = (AiaClient_t*)userData;
    if( !client )
    {
        AiaLogError( "Null client" );
        return;
    }

    AiaUXManager_UpdateServerAttentionState( client->uxManager,
                                             newAttentionState );
}

#ifdef AIA_ENABLE_SPEAKER
static bool AiaClient_StartOfflineAlertTone( const AiaAlertSlot_t* offlineAlert,
                                             void* userData,
                                             uint8_t offlineAlertVolume )
{
    AiaClient_t* client = (AiaClient_t*)userData;
    if( !client )
    {
        AiaLogError( "Null client" );
        return false;
    }

    AiaSpeakerManager_PlayOfflineAlert( client->speakerManager, offlineAlert,
                                        offlineAlertVolume );
    return true;
}
#endif
