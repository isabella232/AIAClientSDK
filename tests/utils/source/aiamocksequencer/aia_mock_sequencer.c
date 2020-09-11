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
 * @file aia_mock_sequencer.c
 * @brief Implements mock functions for the AiaSequencer_t type.
 */

/* The config header is always included first. */
#include <aia_config.h>

#include <aiamocksequencer/aia_mock_sequencer.h>
#include <aiasequencer/aia_sequencer.h>

AiaMockSequencer_t* AiaMockSequencer_Create()
{
    AiaMockSequencer_t* mockSequencer =
        AiaCalloc( 1, sizeof( AiaMockSequencer_t ) );
    if( !mockSequencer )
    {
        return NULL;
    }
    if( !AiaSemaphore( Create )( &mockSequencer->resetSequenceNumberSemaphore,
                                 0, 1000 ) )
    {
        AiaFree( mockSequencer );
        return NULL;
    }
    return mockSequencer;
}

void AiaMockSequencer_Destroy( AiaMockSequencer_t* mockSequencer )
{
    AiaSemaphore( Destroy )( &mockSequencer->resetSequenceNumberSemaphore );
    AiaFree( mockSequencer );
}

AiaSequencer_t* AiaSequencer_Create(
    AiaSequencerMessageSequencedCallback_t messageSequencedCb,
    void* messageSequencedUserData,
    AiaSequencerTimeoutExpiredCallback_t timeoutExpiredCb,
    void* timeoutExpiredUserData,
    AiaSequencerGetSequencerNumberCallback_t getSequenceNumberCb,
    void* getSequenceNumberUserData, size_t maxSlots,
    AiaSequenceNumber_t startingSequenceNumber, uint32_t sequenceTimeoutMs,
    AiaTaskPool_t taskPool )
{
    (void)messageSequencedCb;
    (void)messageSequencedUserData;
    (void)timeoutExpiredCb;
    (void)timeoutExpiredUserData;
    (void)getSequenceNumberCb;
    (void)getSequenceNumberUserData;
    (void)maxSlots;
    (void)startingSequenceNumber;
    (void)sequenceTimeoutMs;
    (void)taskPool;

    return (AiaSequencer_t*)AiaMockSequencer_Create();
}

void AiaSequencer_Destroy( AiaSequencer_t* sequencer )
{
    AiaMockSequencer_Destroy( (AiaMockSequencer_t*)sequencer );
}

void AiaSequencer_ResetSequenceNumber(
    AiaSequencer_t* sequencer,
    AiaSequenceNumber_t newNextExpectedSequenceNumber )
{
    AiaMockSequencer_t* mockSequencer = (AiaMockSequencer_t*)sequencer;
    mockSequencer->currentSequenceNumber = newNextExpectedSequenceNumber;
    AiaSemaphore( Post )( &mockSequencer->resetSequenceNumberSemaphore );
}

bool AiaSequencer_Write( AiaSequencer_t* sequencer, void* message, size_t size )
{
    /* TODO: ADSER-1699 Implement AiaSequencer_Write mock function */
    (void)sequencer;
    (void)message;
    (void)size;

    return true;
}
