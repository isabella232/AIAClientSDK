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
 * @file aia_events.h
 * @brief Constants related to Events.
 */

#ifndef AIA_EVENTS_H_
#define AIA_EVENTS_H_

/* The config header is always included first. */
#include <aia_config.h>

/**
 * Used to inform the AIS service that the shared encryption secret has been
 * rotated.
 */
#define AIA_EVENTS_SECRET_ROTATED "SecretRotated"

/**
 * Used to inform the AIS service of any user-initiated actions related to
 * audio playback control.
 */
#define AIA_EVENTS_BUTTON_COMMAND_ISSUED "ButtonCommandIssued"

/**
 * Used to inform the AIS service where the device started streaming speaker
 * data.
 */
#define AIA_EVENTS_SPEAKER_OPENED "SpeakerOpened"

/**
 * Used to inform the AIS service where the device stopped streaming speaker
 * data.
 */
#define AIA_EVENTS_SPEAKER_CLOSED "SpeakerClosed"

/**
 * Used to keep the AIS service apprised of the AIS device's audio playback
 * state.
 */
#define AIA_EVENTS_SPEAKER_MARKER_ENCOUNTERED "SpeakerMarkerEncountered"

/** Used to send user speech to AIS. */
#define AIA_EVENTS_MICROPHONE_OPENED "MicrophoneOpened"

/** Used to inform the AIS service to stop streaming microphone data. */
#define AIA_EVENTS_MICROPHONE_CLOSED "MicrophoneClosed"

/**
 * Used to inform the AIS service that the timeout given in the OpenMicrophone
 * directive has passed but the microphone has not yet been opened.
 */
#define AIA_EVENTS_OPEN_MICROPHONE_TIMED_OUT "OpenMicrophoneTimedOut"

/** Used to inform the AIS service of an on-device buffer's state. */
#define AIA_EVENTS_BUFFER_STATE_CHANGED "BufferStateChanged"

/**
 * Used to inform the AIS service of when the user adjusts the volume on the
 * device via a button press or GUI interaction.
 */
#define AIA_EVENTS_VOLUME_CHANGED "VolumeChanged"

/**
 * Used to request the current time from the service so that the device can
 * update its local clock.
 */
#define AIA_EVENTS_SYNCHRONIZE_CLOCK "SynchronizeClock"

/**
 * Used to inform the AIS service that the alert was successfully saved to
 * local memory.
 */
#define AIA_EVENTS_SET_ALERT_SUCCEEDED "SetAlertSucceeded"

/**
 * Used to inform the AIS service that the alert was not successfully saved to
 * local memory.
 */
#define AIA_EVENTS_SET_ALERT_FAILED "SetAlertFailed"

/**
 * Used to inform the AIS service that the alert was successfully deleted from
 * local memory.
 */
#define AIA_EVENTS_DELETE_ALERT_SUCCEEDED "DeleteAlertSucceeded"

/**
 * Used to inform the AIS service that the alert was not ssuccessfully deleted
 * from local memory.
 */
#define AIA_EVENTS_DELETE_ALERT_FAILED "DeleteAlertFailed"

/**
 * Used to inform the AIS service that the device was able to locally store the
 * volume level to use for any upcoming alerts.
 */
#define AIA_EVENTS_ALERT_VOLUME_CHANGED "AlertVolumeChanged"

/**
 * Used to inform the AIS service of any device-side changes that happened
 * while not connected to AIS.
 */
#define AIA_EVENTS_SYNCHRONIZE_STATE "SynchronizeState"

/**
 * Used to inform the AIS service of any device-detected error, such as a
 * malformed message or a device-side bug.
 */
#define AIA_EVENTS_EXCEPTION_ENCOUNTERED "ExceptionEncountered"

#endif /* ifndef AIA_EVENTS_H_ */
