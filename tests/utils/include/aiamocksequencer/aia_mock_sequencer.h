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
 * @file private/aia_mock_sequencer.h
 * @brief User-facing mock functions of the @c AiaSequencer_t type.
 */

#ifndef AIA_MOCK_SEQUENCER_H_
#define AIA_MOCK_SEQUENCER_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <aiacore/aia_message_constants.h>

#include AiaSemaphore( HEADER )

typedef struct AiaMockSequencer
{
    AiaSequenceNumber_t currentSequenceNumber;

    AiaSemaphore_t resetSequenceNumberSemaphore;
} AiaMockSequencer_t;

AiaMockSequencer_t* AiaMockSequencer_Create();

void AiaMockSequencer_Destroy( AiaMockSequencer_t* sequencer );

#endif /* ifndef AIA_MOCK_SEQUENCER_H_ */
