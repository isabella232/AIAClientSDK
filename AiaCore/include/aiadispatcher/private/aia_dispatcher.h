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
 * @file aia_private_dispatcher.h
 * @brief User-facing functions for the message parsing and dispatching
 * of AIA.
 */

#ifndef AIA_PRIVATE_DISPATCHER_H_
#define AIA_PRIVATE_DISPATCHER_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <aiaalertmanager/aia_alert_manager.h>
#include <aiaclockmanager/aia_clock_manager.h>
#include <aiaconnectionmanager/aia_connection_manager.h>
#include <aiaexceptionmanager/aia_exception_manager.h>
#include <aiamicrophonemanager/aia_microphone_manager.h>
#include <aiaregulator/aia_regulator.h>
#include <aiasecretmanager/aia_secret_manager.h>
#include <aiasequencer/aia_sequencer.h>
#include <aiaspeakermanager/aia_speaker_manager.h>
#include <aiauxmanager/aia_ux_manager.h>

/** Forward declaration of structs needed in @c AiaDispatcher_t */
/** TODO: ADSER-1757 Investigate moving speakerMessageSequencedCb within
 * speakerManager */
struct AiaConnectionManager;

/**
 * Generic function pointer for directive handlers.
 *
 * @param component The manager instance this function will work on.
 * @param payload Pointer to the unencrypted "payload" body value of the
 * directive. Note that this will be @c NULL if no payload could be parsed.
 * @param size Size of the @c payload.
 * @param sequenceNumber Sequence number of the message.
 * @param index Index of the message within the directive array.
 */
typedef void ( *AiaDirectiveHandler_t )( void* component, const void* payload,
                                         size_t size,
                                         AiaSequenceNumber_t sequenceNumber,
                                         size_t index );

/** Private data for the @c AiaDispatcher_t type. */
struct AiaDispatcher
{
    /** Structs used in topic subscription callbacks */
    /** @{ */

    struct AiaConnectionManager* connectionManager;
    AiaCapabilitiesSender_t* capabilitiesSender;
    AiaSecretManager_t* secretManager;
    char* deviceTopicRoot;
    size_t deviceTopicRootSize;
#ifdef AIA_ENABLE_SPEAKER
    AiaSpeakerManager_t* speakerManager;
#endif

    /** @} */

    /** Used to send ExceptionEncountered events */
    /** @{ */

    AiaRegulator_t* regulator;

    /** @} */

    /** Mutex used to guard against asynchronous calls in threaded
     * environments. */
    AiaMutex_t mutex;

    /** @name Variables synchronized by mutex. */
    /** @{ */

    /** Sequencers used in topic subscription callbacks */
    AiaSequencer_t* capabilitiesAcknowledgeSequencer;
    AiaSequencer_t* directiveSequencer;
#ifdef AIA_ENABLE_SPEAKER
    AiaSequencer_t* speakerSequencer;
#endif

    /** @} */

    /* Pointers to handlers for different directives */
    /** @{ */

#ifdef AIA_ENABLE_SPEAKER
    AiaDirectiveHandler_t openSpeakerHandler;
    AiaDirectiveHandler_t closeSpeakerHandler;
    AiaDirectiveHandler_t setVolumeHandler;
#endif
#ifdef AIA_ENABLE_MICROPHONE
    AiaDirectiveHandler_t openMicrophoneHandler;
    AiaDirectiveHandler_t closeMicrophoneHandler;
#endif
#ifdef AIA_ENABLE_ALERTS
    AiaDirectiveHandler_t setAlertVolumeHandler;
    AiaDirectiveHandler_t setAlertHandler;
    AiaDirectiveHandler_t deleteAlertHandler;
#endif

    AiaDirectiveHandler_t setAttentionStateHandler;
    AiaDirectiveHandler_t rotateSecretHandler;

#ifdef AIA_ENABLE_CLOCK
    AiaDirectiveHandler_t setClockHandler;
#endif

    AiaDirectiveHandler_t exceptionHandler;

    /** @} */

    /* User data for directive handlers */
    /** @{ */

#ifdef AIA_ENABLE_SPEAKER
    AiaSpeakerManager_t* openSpeakerData;
    AiaSpeakerManager_t* closeSpeakerData;
    AiaSpeakerManager_t* setVolumeData;
#endif
#ifdef AIA_ENABLE_MICROPHONE
    AiaMicrophoneManager_t* openMicrophoneData;
    AiaMicrophoneManager_t* closeMicrophoneData;
#endif
#ifdef AIA_ENABLE_ALERTS
    AiaAlertManager_t* setAlertVolumeData;
    AiaAlertManager_t* setAlertData;
    AiaAlertManager_t* deleteAlertData;
#endif

    AiaUXManager_t* setAttentionStateData;
    AiaSecretManager_t* rotateSecretData;

#ifdef AIA_ENABLE_CLOCK
    AiaClockManager_t* setClockData;
#endif

    AiaExceptionManager_t* exceptionData;

    /** @} */
};

#endif /* ifndef AIA_PRIVATE_DISPATCHER_H_ */
