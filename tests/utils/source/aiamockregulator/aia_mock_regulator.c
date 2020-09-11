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
 * @file aia_mock_regulator.c
 * @brief Implements mock functions for the AiaRegulator_t type.
 */

/* The config header is always included first. */
#include <aia_config.h>

#include <aiamockregulator/aia_mock_regulator.h>

#include <aiacore/aia_json_message.h>

AiaMockRegulator_t* AiaMockRegulator_Create()
{
    AiaMockRegulator_t* mockRegulator =
        AiaCalloc( 1, sizeof( AiaMockRegulator_t ) );
    if( !mockRegulator )
    {
        return NULL;
    }
    if( !AiaSemaphore( Create )( &mockRegulator->writeSemaphore, 0, 1000 ) )
    {
        AiaFree( mockRegulator );
        return NULL;
    }
    AiaListDouble( Create )( &mockRegulator->writtenMessages );
    return mockRegulator;
}

void AiaMockRegulator_Destroy( AiaMockRegulator_t* mockRegulator,
                               AiaRegulatorDestroyChunkCallback_t destroyChunk,
                               void* destroyChunkUserData )
{
    AiaListDouble( Link_t )* link = NULL;
    while( ( link = AiaListDouble( RemoveHead )(
                 &mockRegulator->writtenMessages ) ) )
    {
        AiaMockRegulatorWrittenMessage_t* writtenMessage =
            (AiaMockRegulatorWrittenMessage_t*)link;
        destroyChunk( writtenMessage->chunk, destroyChunkUserData );
        AiaFree( writtenMessage );
    }
    AiaSemaphore( Destroy )( &mockRegulator->writeSemaphore );
    AiaFree( mockRegulator );
}

bool AiaRegulator_Write( AiaRegulator_t* regulator, AiaRegulatorChunk_t* chunk )
{
    AiaMockRegulator_t* mockRegulator = (AiaMockRegulator_t*)regulator;
    AiaMockRegulatorWrittenMessage_t* writtenMessage =
        AiaCalloc( 1, sizeof( AiaMockRegulatorWrittenMessage_t ) );
    if( !writtenMessage )
    {
        AiaLogError( "AiaCalloc() failed." );
        return false;
    }
    AiaListDouble( Link_t ) link = AiaListDouble( LINK_INITIALIZER );
    writtenMessage->link = link;
    writtenMessage->chunk = chunk;
    AiaListDouble( InsertTail )( &mockRegulator->writtenMessages,
                                 &writtenMessage->link );
    AiaSemaphore( Post )( &mockRegulator->writeSemaphore );
    return true;
}
