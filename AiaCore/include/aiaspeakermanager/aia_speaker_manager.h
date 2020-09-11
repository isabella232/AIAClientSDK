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

#ifndef AIA_SPEAKER_MANAGER_H_
#define AIA_SPEAKER_MANAGER_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <aia_application_config.h>
#include <aiacore/aia_binary_constants.h>
#include <aiacore/aia_message_constants.h>

#include <aiaregulator/aia_regulator.h>
#include <aiasequencer/aia_sequencer.h>

/* TODO: ADSER-1867 Calculate this internally at runtime based on Opus frame
 * size and configured capabilities. */
#define AIA_SPEAKER_FRAME_PUSH_CADENCE_MS 20

/**
 * This type is used to manage the speaker data flowing through the system. It
 * provides an interface for applications to interact with to read and play
 * speaker data. It will internally maintain a buffer holding the compressed
 * audio stream speaker topic data in the format that the Aia service vends.
 * The size of the underlying buffer is dictated by the @c speakerBufferSize
 * passed into the @c AiaSpeakerManager_Create() function. Methods of this
 * object are thread-safe.
 */
typedef struct AiaSpeakerManager AiaSpeakerManager_t;

/**
 * Function pointer for speaker-related directive handlers.
 *
 * @param component The @c AiaSpeakerManager_t instance this function will work
 * on.
 * @param payload Pointer to the unencrypted "payload" body value of the
 * directive.
 * @param size Size of the @c payload.
 * @param sequenceNumber Sequence number of the message.
 * @param index Index of the message within the directive array.
 */
typedef void ( *AiaSpeakerManagerDirectiveHandler_t )(
    AiaSpeakerManager_t* component, const void* payload, size_t size,
    AiaSequenceNumber_t sequenceNumber, size_t index );

/** An enum used to represent the various audio buffer states. Note that value
 * based comparisons of these states are used internally. */
typedef enum AiaSpeakerManagerBufferState
{
    /** A BufferStateChanged UNDERRUN state. */
    AIA_UNDERRUN_STATE,

    /** A BufferStateChanged UNDERRUN_WARNING state. */
    AIA_UNDERRUN_WARNING_STATE,

    /** A BufferStateChanged neutral state. */
    AIA_NONE_STATE,

    /** A BufferStateChanged OVERRUN_WARNING state. */
    AIA_OVERRUN_WARNING_STATE,

    /** A BufferStateChanged OVERRUN state. */
    AIA_OVERRUN_STATE,
} AiaSpeakerManagerBufferState_t;

/**
 * Utility function used to convert a given @c AiaSpeakerManagerBufferState_t to
 * a string for logging and message sending purposes.
 *
 * @param state The @c AiaSpeakerManagerBufferState_t to convert.
 * @return The speaker manager buffer state as a string or @c NULL on failure.
 */
const char* AiaSpeakerManagerBufferState_ToString(
    AiaSpeakerManagerBufferState_t state );

/**
 * Function pointer to notify observers of speaker buffer state changes.
 * @note Implementations are not required to be thread-safe. Blocking can result
 * in delays in system processing.
 *
 * @param userData Context to passed with this callback.
 * @param currentBufferState Current speaker buffer state.
 */
typedef void ( *AiaSpeakerManagerBufferStateObserver_t )(
    void* userData, AiaSpeakerManagerBufferState_t currentBufferState );

/**
 * Allocates and initializes a @c AiaSpeakerManager_t object from the heap.
 * The returned pointer should be destroyed using @c
 * AiaSpeakerManager_Destroy().
 *
 * @param speakerBufferSize The size of the underlying buffer to allocate for
 * holding compressed speaker data.
 * @param overrunWarningThreshold The threshold at which to send an
 * OVERRUN_WARNING.
 * @param underrunWarningTreshold The threshold at which to send an
 * UNDERRUN_WARNING.
 * @param playSpeakerDataCb Callback used to push speaker frames for playback.
 * @param playSpeakerDataCbUserData User data to be passed along with @c
 * playSpeakerDataCb.
 * @param sequencer Used to reset the next expected sequence number on overruns.
 * @param regulator Used to publish outbound messages.
 * @param setVolumeCb Callback used to change the speaker volume level.
 * @param setVolumeCbUserData User data to be passed along with @c setVolumeCb.
 * @param playOfflineAlertCb Callback used to play offline alert tone.
 * @param playOfflineAlertCbUserData User data to be passed along with @c
 * playOfflineAlertCb.
 * @param stopOfflineAlertCb Callback used to stop offline alert tone.
 * @param stopOfflineAlertCbUserData User data to be passed along with @c
 * stopOfflineAlertCb.
 * @param notifyObserversCb An optional callback pointer used to notify
 * observers about speaker buffer state changes.
 * @param notifyObserversCbUserData User data associated with @c
 * notifyObserversCb.
 * @return The newly created @c AiaSpeakerManager_t if successful, or NULL
 * otherwise.
 */
AiaSpeakerManager_t* AiaSpeakerManager_Create(
    size_t speakerBufferSize, size_t overrunWarningThreshold,
    size_t underrunWarningTreshold, AiaPlaySpeakerData_t playSpeakerDataCb,
    void* playSpeakerDataCbUserData, AiaSequencer_t* sequencer,
    AiaRegulator_t* regulator, AiaSetVolume_t setVolumeCb,
    void* setVolumeCbUserData, AiaOfflineAlertPlayback_t playOfflineAlertCb,
    void* playOfflineAlertCbUserData, AiaOfflineAlertStop_t stopOfflineAlertCb,
    void* stopOfflineAlertCbUserData,
    AiaSpeakerManagerBufferStateObserver_t notifyObserversCb,
    void* notifyObserversCbUserData );

/**
 * Uninitializes and deallocates an @c AiaSpeakerManager_t previously created by
 * a call to @c AiaSpeakerManager_Create().
 *
 * @param speakerManager The @c AiaSpeakerManager_t to destroy.
 */
void AiaSpeakerManager_Destroy( AiaSpeakerManager_t* speakerManager );

/**
 * Provides applications a way to notify of playback stoppage due to local stops
 * like barge-in.
 *
 * @param speakerManager The @c AiaSpeakerManager_t to act on.
 * @note This function assumes that the last frame pushed via @c
 * playSpeakerDataCb() was when playback was stopped.
 */
void AiaSpeakerManager_StopPlayback( AiaSpeakerManager_t* speakerManager );

/**
 * Provides applications a way to indicate that they are ready to receive
 * speaker frames via @c playSpeakerDataCb() again after a failure to accept a
 * frame of data.
 *
 * @Param speakerManager The @c AiaSpeakerManager_t to act on.
 */
void AiaSpeakerManager_OnSpeakerReady( AiaSpeakerManager_t* speakerManager );

/**
 * Provides applications a way to request a volume change as a result of a local
 * trigger such as a physical button or GUI affordance that changes the volume
 * to an absolute level.
 *
 * @param speakerManager The @c AiaSpeakerManager_t to act on.
 * @param newVolume The new absolute volume. This must be between @c
 * AIA_MIN_VOLUME and @c AIA_MAX_VOLUME, inclusive.
 * @return @c true if everything went as expected or @c false otherwise.
 * @note This will result in a synchronous call to @c setVolumeCb.
 */
bool AiaSpeakerManager_ChangeVolume( AiaSpeakerManager_t* speakerManager,
                                     uint8_t newVolume );

/**
 * Provides applications a way to request a volume change as a result of a local
 * trigger such as a physical button or GUI affordance that adjusts the volume
 * relative to the current level.
 *
 * @param speakerManager The @c AiaSpeakerManager_t to act on.
 * @param delta The absolute delta to modify the volume by. This value will be
 * added to the current volume to get the new resulting absolute volume. @c
 * AIA_MIN_VOLUME and @c AIA_MAX_VOLUME will act as the floor and ceiling,
 * respectively, of the resulting absolute volume.
 * @return @c true if everything went as expected or @c false otherwise.
 * @note This will result in a synchronous call to @c setVolumeCb.
 */
bool AiaSpeakerManager_AdjustVolume( AiaSpeakerManager_t* speakerManager,
                                     int8_t delta );

/**
 * Returns whether the speaker is currently streaming or about the start
 * streaming.
 *
 * @param speakerManager The @c AiaSpeakerManager_t to act on.
 * @return @c true if the speaker is currently open or streaming, or @c false
 * otherwise.
 */
bool AiaSpeakerManager_CanSpeakerStream( AiaSpeakerManager_t* speakerManager );

#ifdef AIA_ENABLE_ALERTS
/**
 * @copydoc AiaOfflineAlertStart_t
 */
void AiaSpeakerManager_PlayOfflineAlert( AiaSpeakerManager_t* speakerManager,
                                         const AiaAlertSlot_t* offlineAlert,
                                         uint8_t offlineAlertVolume );

/**
 * Disables offline alert playback.
 *
 * @param speakerManager The @c AiaSpeakerManager_t to act on.
 */
void AiaSpeakerManager_StopOfflineAlert( AiaSpeakerManager_t* speakerManager );
#endif

#endif /* ifndef AIA_SPEAKER_MANAGER_H_ */
