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
 * @file aia_mock_speaker_manager.h
 * @brief User-facing mock functions of the @c AiaSpeakerManager_t type.
 */

#ifndef AIA_MOCK_SPEAKER_MANAGER_H_
#define AIA_MOCK_SPEAKER_MANAGER_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <aiacore/aia_binary_constants.h>
#include <aiacore/aia_message_constants.h>

typedef struct AiaMockSpeakerManager
{
    const size_t overrunWarningThreshold;

    const size_t underrunWarningThreshold;

    void ( *currentAction )( bool, void* userData );
    AiaBinaryAudioStreamOffset_t currentActionOffset;
    void* currentActionUserData;
} AiaMockSpeakerManager_t;

#endif /* ifndef AIA_MOCK_SPEAKER_MANAGER_H_ */
