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
 * @file aia_client.h
 * @brief User-facing functions of the @c AiaClient_t type.
 */

#ifndef AIA_CLIENT_H_
#ifdef __cplusplus
extern "C" {
#endif
#define AIA_CLIENT_H_

/* The config header is always included first. */
#include <aia_capabilities_config.h>
#include <aia_config.h>

#include <aiaconnectionmanager/aia_connection_manager.h>
#include <aiacore/aia_button_command.h>
#include <aiacore/capabilities_sender/aia_capabilities_sender.h>
#include <aiacore/data_stream_buffer/aia_data_stream_buffer_reader.h>
#include <aiamicrophonemanager/aia_microphone_manager.h>
#include <aiaspeakermanager/aia_speaker_manager.h>
#include <aiauxmanager/aia_ux_manager.h>

/**
 * This type serves as the high level component that applications are expected
 * to interact with for Aia interactions. Methods of this object are
 * thread-safe.
 */
typedef struct AiaClient AiaClient_t;

/**
 * Allocates and initializes a @c AiaClient_t object from the heap.
 * The returned pointer should be destroyed using @c
 * AiaClient_Destroy().
 *
 * @param mqttConnection An MQTT connection handle that is connected to AWS IoT.
 * @param onConnectionSuccess Callback to notify of Aia connection success.
 *     Implementations are expected to be non-blocking.
 * @param onConnectionRejected Callback to notify of Aia connection
 *     rejections. Implementations are expected to be non-blocking.
 * @param onDisconnected Callback to notify of Aia disconnections.
 *     Implementations are expected to be non-blocking.
 * @param connectionUserData User data to be associated with the above three
 *     callbacks.
 * @param aiaTaskPool Task pool instance to be used for certain asychronous
 *     actions.
 * @param onException An optional callback when an exception directive is
 * received from the service.
 * @param onExceptionUserData User data associated with the above
 *     callback.
 * @param capabilitiesStateObserver Callback used to observe the state of
 *     capabilities declaration.
 * @param capabilitiesStateObserverUserData User data associated with the above
 *     callback.
 * @param receiveSpeakerFramesCb Callback to receive speaker from an internal
 *     buffer to the platform for playback every @c
 *     AIA_SPEAKER_FRAME_PUSH_CADENCE_MS. Implementations are expected to be
 *     non-blocking.
 * @param receiveSpeakerFramesCbUserData User data to be associated with the
 *     above callback.
 * @param setVolumeCb Callback used to change the client's volume.
 * @param setVolumeCbUserData User data to be associated with the above
 * callback.
 * @param playOfflineAlertCb Callback used to play an offline alert tone.
 * @param playOfflineAlertCbUserData User data to be associated with the above
 * callback.
 * @param stopOfflineAlertCb Callback used to stop an offline alert tone.
 * @param stopOfflineAlertCbUserData User data to be associated with the above
 * callback.
 * @param uxObserver Callback to observe the current UX state to display to the
 * end user.
 * @param uxObserverUserData User data to be associated with the above
 * @param microphoneBufferReader Reader used to stream microphone data to the
 *     Aia cloud on user interactions. Data contained in the underlying
 *     buffer must be in 16-bit linear PCM, 16-kHz sample rate, single channel,
 *     little-endian byte order format.
 * @param enableLocalStopOnButtonPresses Flag used to enable local stops as an
 * optimization for button presses that stop or pause playback. @see
 * https://developer.amazon.com/en-US/docs/alexa/alexa-voice-service/avs-for-aws-iot-speaker.html#buttoncommandissued.
 * @return The newly created @c AiaClient_t if successful, or NULL
 *     otherwise.
 */
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
);

/**
 * Uninitializes and deallocates an @c AiaClient_t previously created by
 * a call to @c AiaClient_Create().
 *
 * @param aiaClient The @c AiaClient_t to destroy.
 */
void AiaClient_Destroy( AiaClient_t* aiaClient );

/**
 * Attempts to initiate a connection with the Aia service. Successes and
 * failures are communicated asynchronously via @c onConnectionSuccess and @c
 * onConnectionRejected.
 *
 * @param aiaClient The @c AiaClient_t to act on.
 * @return @c true if the connection is successful, @c false otherwise
 * (including if already connected).
 */
bool AiaClient_Connect( AiaClient_t* aiaClient );

/**
 * @copydoc AiaDisconnectHandler_t
 */
bool AiaClient_Disconnect( void* userData, int code, const char* description );

/**
 * Publishes capabilities from @c aia_capabilities_config.h on behalf of the
 * client.
 *
 * @param aiaClient The @c AiaClient_t to act on.
 * @return @c true if the capabilities were published or @c false if not.
 * @note Capabilities will be automatically rejected if the @c
 * capabilitiesStateObserver's state is @c AIA_CAPABILITIES_STATE_PUBLISHED.
 */
bool AiaClient_PublishCapabilities( AiaClient_t* aiaClient );

#ifdef AIA_ENABLE_SPEAKER
/**
 * Provides applications a way to notify of the Aia service of playback
 * stoppage due to local stops like barge-in. This method must be called when
 * speaker playback is locally stopped.
 *
 * @param aiaClient The @c AiaClient_t to act on.
 * @note This function assumes that the last frame pushed via @c
 * receiveSpeakerFramesCb was when playback was stopped.
 */
void AiaClient_StopSpeaker( AiaClient_t* aiaClient );

/**
 * Provides applications a way to indicate that they are ready to receive
 * speaker frames via @c receiveSpeakerFramesCb again after a failure to accept
 * a frame of data.
 *
 * @param aiaClient The @c AiaClient_t to act on.
 * @note Speaker frames will stop being pushed to the application when @c
 * receiveSpeakerFramesCb returns @c false and will resume being pushed to the
 * application when this method is called.
 * @note The default state is "ready". A call to this method is needed when @c
 * receiveSpeakerFramesCb returns @c false to indicate that the speaker is ready
 * to accept frames again.
 */
void AiaClient_OnSpeakerReady( AiaClient_t* aiaClient );

/**
 * Provides applications a way to request a volume change as a result of a local
 * trigger such as a physical button or GUI affordance that changes the volume
 * to an absolute level.
 *
 * @param aiaClient The @c AiaClient_t to act on.
 * @param newVolume The new absolute volume. This must be between @c
 * AIA_MIN_VOLUME and @c AIA_MAX_VOLUME, inclusive.
 * @return @c true if everything went as expected or @c false otherwise.
 * @note This will result in a synchronous call to @c setVolumeCb.
 */
bool AiaClient_ChangeVolume( AiaClient_t* aiaClient, uint8_t newVolume );

/**
 * Provides applications a way to request a volume change as a result of a local
 * trigger such as a physical button or GUI affordance that adjusts the volume
 * relative to the current level.
 *
 * @param aiaClient The @c AiaClient_t to act on.
 * @param delta The absolute delta to modify the volume by. This value will be
 * added to the current volume to get the new resulting absolute volume. @c
 * AIA_MIN_VOLUME and @c AIA_MAX_VOLUME will act as the floor and ceiling,
 * respectively, of the resulting absolute volume.
 * @return @c true if everything went as expected or @c false otherwise.
 * @note This will result in a synchronous call to @c setVolumeCb.
 */
bool AiaClient_AdjustVolume( AiaClient_t* aiaClient, int8_t delta );
#endif

#ifdef AIA_ENABLE_MICROPHONE
/**
 * Begins a hold to talk initiated Alexa interaction.
 *
 * @param aiaClient The @c AiaClient_t to act on.
 * @param index The sample index that @c microphoneBufferReader should
 * begin streaming from.
 * @return @c true if the interaction began successfully or @c false otherwise.
 */
bool AiaClient_HoldToTalkStart( AiaClient_t* aiaClient,
                                AiaBinaryAudioStreamOffset_t index );

/**
 * Ends a hold to talk interaction by stopping the streaming of audio data sent
 * to the Aia cloud.
 *
 * @param aiaClient The @c AiaClient_t to act on.
 */
void AiaClient_CloseMicrophone( AiaClient_t* aiaClient );

/**
 * Begins a tap to talk initiated Alexa interaction.
 *
 * @param aiaClient The @c AiaClient_t to act on.
 * @param index The sample index that @c microphoneBufferReader should
 * begin streaming from.
 * @param profile The ASR profile associated with this interaction. Only
 * @c AIA_MICROPHONE_PROFILE_NEAR_FIELD and @c AIA_MICROPHONE_PROFILE_FAR_FIELD
 * are supported for this type of interaction.
 * @return @c true if the interaction began successfully or @c false otherwise.
 *
 * @see
 * https://developer.amazon.com/docs/alexa-voice-service/speechrecognizer.html
 * @see
 * https://developer.amazon.com/docs/alexa-voice-service/audio-hardware-configurations.html#asr
 */
bool AiaClient_TapToTalkStart( AiaClient_t* aiaClient,
                               AiaBinaryAudioStreamOffset_t index,
                               AiaMicrophoneProfile_t profile );

/* TODO: ADSER-1628 Add Metadata support */
/**
 * Begins a wake word initiated Alexa interaction.
 *
 * @param aiaClient The @c AiaClient_t to act on.
 * @param beginIndex The sample index corresponding with the start of the
 * detected wake word.
 * @param endIndex The sample index corresponding with the end of the detected
 * wake word.
 * @param profile The ASR profile associated with this interaction. Only
 * @c AIA_MICROPHONE_PROFILE_NEAR_FIELD and @c AIA_MICROPHONE_PROFILE_FAR_FIELD
 * are supported for this type of interaction.
 * @param wakeWord The wake word that was detected - note that only "ALEXA" is
 * currently supported.
 * @return @c true if the interaction began successfully or @c false otherwise.
 *
 * @see
 * https://developer.amazon.com/docs/alexa-voice-service/speechrecognizer.html
 * @see
 * https://developer.amazon.com/docs/alexa-voice-service/audio-hardware-configurations.html#asr
 */
bool AiaClient_WakeWordStart( AiaClient_t* aiaClient,
                              AiaBinaryAudioStreamOffset_t beginIndex,
                              AiaBinaryAudioStreamOffset_t endIndex,
                              AiaMicrophoneProfile_t profile,
                              const char* wakeWord );
#endif

/* TODO: ADSER-1691 Handle SynchronizeState publishing internally */
/**
 * Synchronizes the client state with the service. This should be called after
 * a successful connection. Note that capabilities should be published first
 * prior to sending this event if new capabilities are being declared.
 *
 * @return @c true if the event was successfully queued for publishing or @c
 * false otherwise.
 * @note This currently sends a dummy payload.
 */
bool AiaClient_SynchronizeState( AiaClient_t* aiaClient );

/**
 * Provides applications a way to inform AIA of a user-initiated button press.
 *
 * @param aiaClient The @c AiaClient_t to act on.
 * @param button The button that was pressed.
 * @return @c true if an event was successfully initiated or @c false otherwise.
 */
bool AiaClient_OnButtonPressed( AiaClient_t* aiaClient,
                                AiaButtonCommand_t button );

#ifdef AIA_ENABLE_CLOCK
/**
 * Attempts to synchronize the device clock with the AIA server. Responses from
 * the service will be sent to @c AiaClock_SetTimeMsSinceNTPEpoch.
 *
 * @param aiaClient The @c AiaClient_t to act on.
 * @return @c true if an attempt was successfully made or @c false otherwise.
 */
bool AiaClient_SynchronizeClock( AiaClient_t* aiaClient );
#endif

#ifdef AIA_ENABLE_ALERTS
/**
 * Provides applications a way to delete an alert from memory and local storage.
 * This function should be called when a local stop is detected while playing an
 * offline alert or when the duration of the alert has finished.
 *
 * @param aiaClient The @c AiaClient_t to act on.
 * @param alertToken The token of the alert to delete.
 * @return @c true if alert has been deleted successfully or @c false otherwise.
 */
bool AiaClient_DeleteAlert( AiaClient_t* aiaClient, const char* alertToken );
#endif

#ifdef __cplusplus
}
#endif
#endif /* ifndef AIA_CLIENT_H_ */
