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
 * @file aia_capabilities_constants.h
 * @brief Constants related to Capabilities.
 */

#ifndef AIA_CAPABILITIES_H_
#define AIA_CAPABILITIES_H_

/* The config header is always included first. */
#include "aia_json_constants.h"
#include "aia_topic.h"

#define AIA_CAPABILITIES_PUBLISH "Publish"

#define AIA_CAPABILITIES_KEY AIA_TOPIC_CAPABILITIES_STRING

#define AIA_CAPABILITIES_TYPE_KEY "type"
#define AIA_CAPABILITIES_AIS_INTERFACE "AisInterface"
#define AIA_CAPABILITIES_AIS_INTERFACE_KEY "interface"
#define AIA_CAPABILITIES_VERSION_KEY "version"
#define AIA_CAPABILITIES_CONFIGURATIONS_KEY "configurations"

#define AIA_SPEAKER_VERSION "1.0"
#define AIA_CAPABILITIES_SPEAKER "Speaker"
#define AIA_CAPABILITIES_SPEAKER_AUDIO_BUFFER "audioBuffer"
#define AIA_CAPABILITIES_SPEAKER_AUDIO_BUFFER_SIZE "sizeInBytes"
#define AIA_CAPABILITIES_SPEAKER_AUDIO_REPORTING "reporting"
#define AIA_CAPABILITIES_SPEAKER_AUDIO_OVERRUN_THRESHOLD \
    "overrunWarningThreshold"
#define AIA_CAPABILITIES_SPEAKER_AUDIO_UNDERRUN_THRESHOLD \
    "underrunWarningThreshold"
#define AIA_CAPABILITIES_SPEAKER_AUDIO_DECODER "audioDecoder"
#define AIA_CAPABILITIES_SPEAKER_AUDIO_FORMAT "format"
#define AIA_CAPABILITIES_SPEAKER_AUDIO_BITRATE "bitrate"
#define AIA_CAPABILITIES_SPEAKER_AUDIO_TYPE "type"
#define AIA_CAPABILITIES_SPEAKER_AUDIO_BITS_PER_SECOND "bitsPerSecond"
#define AIA_CAPABILITIES_SPEAKER_NUM_CHANNELS "numberOfChannels"

#define AIA_MICROPHONE_VERSION "1.0"
#define AIA_CAPABILITIES_MICROPHONE "Microphone"
#define AIA_CAPABILITIES_MICROPHONE_AUDIO_ENCODER "audioEncoder"
#define AIA_CAPABILITIES_MICROPHONE_AUDIO_FORMAT \
    AIA_CAPABILITIES_SPEAKER_AUDIO_FORMAT

#define AIA_ALERTS_VERSION "1.0"
#define AIA_CAPABILITIES_ALERTS "Alerts"
#define AIA_CAPABILITIES_ALERTS_MAX_ALERTS "maxAlertCount"

#define AIA_CLOCK_VERSION "1.0"
#define AIA_CAPABILITIES_CLOCK "Clock"

#define AIA_SYSTEM_VERSION "1.0"
#define AIA_CAPABILITIES_SYSTEM "System"
#define AIA_CAPABILITIES_SYSTEM_MQTT "mqtt"
#define AIA_CAPABILITIES_SYSTEM_MQTT_MESSAGE \
    AIA_EXCEPTION_ENCOUNTERED_MESSAGE_KEY
#define AIA_CAPABILITIES_SYSTEM_MAX_MESSAGE_SIZE "maxSizeInBytes"
#define AIA_CAPABILITIES_SYSTEM_FIRMWARE_VERSION "firmwareVersion"
#define AIA_CAPABILITIES_SYSTEM_LOCALE "locale"

#endif /* ifndef AIA_CAPABILITIES_H_ */
