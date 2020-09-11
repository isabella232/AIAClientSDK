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
 * @file aia_mock_speaker_manager.c
 * @brief Implements mock functions for the AiaMockSpeakerManager_t type.
 */

/* The config header is always included first. */
#include <aia_config.h>

#include <aiamockspeakermanager/aia_mock_speaker_manager.h>
#include <aiaspeakermanager/aia_speaker_manager.h>
#include <aiaspeakermanager/private/aia_speaker_manager.h>

/** TODO: ADSER-1738 - Investigate mock functions for speaker manager */

AiaSpeakerManager_t* AiaSpeakerManager_Create(
    size_t speakerBufferSize, size_t overrunWarningThreshold,
    size_t underrunWarningThreshold, AiaPlaySpeakerData_t playSpeakerDataCb,
    void* playSpeakerDataCbUserData, AiaSequencer_t* sequencer,
    AiaRegulator_t* regulator, AiaSetVolume_t setVolumeCb,
    void* setVolumeCbUserData, AiaOfflineAlertPlayback_t playOfflineAlertCb,
    void* playOfflineAlertCbUserData, AiaOfflineAlertStop_t stopOfflineAlertCb,
    void* stopOfflineAlertCbUserData,
    AiaSpeakerManagerBufferStateObserver_t notifyObserversCb,
    void* notifyObserversCbUserData )
{
    (void)speakerBufferSize;
    (void)overrunWarningThreshold;
    (void)underrunWarningThreshold;
    (void)playSpeakerDataCb;
    (void)playSpeakerDataCbUserData;
    (void)sequencer;
    (void)regulator;
    (void)setVolumeCb;
    (void)setVolumeCbUserData;
    (void)playOfflineAlertCb;
    (void)playOfflineAlertCbUserData;
    (void)stopOfflineAlertCb;
    (void)stopOfflineAlertCbUserData;
    (void)notifyObserversCb;
    (void)notifyObserversCbUserData;

    AiaMockSpeakerManager_t* mockSpeakerManager =
        AiaCalloc( 1, sizeof( AiaMockSpeakerManager_t ) );
    if( !mockSpeakerManager )
    {
        return NULL;
    }

    return (AiaSpeakerManager_t*)mockSpeakerManager;
}

void AiaSpeakerManager_Destroy( AiaSpeakerManager_t* speakerManager )
{
    AiaFree( speakerManager );
}

void AiaSpeakerManager_OnSpeakerTopicMessageReceived(
    AiaSpeakerManager_t* speakerManager, const uint8_t* message, size_t size,
    AiaSequenceNumber_t sequenceNumber )
{
    (void)speakerManager;
    (void)message;
    (void)size;
    (void)sequenceNumber;
}

void AiaSpeakerManager_OnOpenSpeakerDirectiveReceived(
    void* manager, const void* payload, size_t size,
    AiaSequenceNumber_t sequenceNumber, size_t index )
{
    (void)manager;
    (void)payload;
    (void)size;
    (void)sequenceNumber;
    (void)index;
}

void* AiaSpeakerManager_InvokeActionAtOffset(
    AiaSpeakerManager_t* speakerManager, AiaBinaryAudioStreamOffset_t offset,
    void ( *action )( bool, void* userData ), void* userData )
{
    (void)speakerManager;
    (void)userData;
    ( (AiaMockSpeakerManager_t*)speakerManager )->currentAction = action;
    ( (AiaMockSpeakerManager_t*)speakerManager )->currentActionOffset = offset;
    ( (AiaMockSpeakerManager_t*)speakerManager )->currentActionUserData =
        userData;
    return speakerManager;
}

void AiaSpeakerManager_CancelAction( AiaSpeakerManager_t* speakerManager,
                                     AiaSpeakerActionHandle_t id )
{
    (void)speakerManager;
    (void)id;
}

AiaBinaryAudioStreamOffset_t AiaSpeakerManager_GetCurrentOffset(
    const AiaSpeakerManager_t* speakerManager )
{
    (void)speakerManager;
    return 0;
}
