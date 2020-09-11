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
 * @file private/aia_mock_regulator.h
 * @brief User-facing mock functions of the @c AiaRegulator_t type.
 */

#ifndef AIA_MOCK_REGULATOR_H_
#define AIA_MOCK_REGULATOR_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <aiacore/aia_message.h>
#include <aiacore/aia_message_constants.h>
#include <aiaregulator/aia_regulator.h>

#include AiaSemaphore( HEADER )

typedef struct AiaMockRegulator
{
    AiaListDouble_t writtenMessages;

    AiaSemaphore_t writeSemaphore;
} AiaMockRegulator_t;

/** Structure for holding emitted messages and when they were emitted. */
typedef struct AiaMockRegulatorWrittenMessage
{
    /** The node in the list. */
    AiaListDouble( Link_t ) link;

    /** The message that was emitted. */
    AiaRegulatorChunk_t* chunk;
} AiaMockRegulatorWrittenMessage_t;

AiaMockRegulator_t* AiaMockRegulator_Create();

void AiaMockRegulator_Destroy( AiaMockRegulator_t* mockRegulator,
                               AiaRegulatorDestroyChunkCallback_t destroyChunk,
                               void* destroyChunkUserData );

#endif /* ifndef AIA_MOCK_REGULATOR_H_ */
