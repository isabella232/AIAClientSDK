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
 * @file aia_clock_manager.c
 * @brief Implements functions for the AiaClockManager_t type.
 */

/* The config header is always included first. */
#include <aia_config.h>

#include <aiaclockmanager/aia_clock_manager.h>
#include <aiaclockmanager/private/aia_clock_manager.h>

#include <aiacore/aia_events.h>
#include <aiacore/aia_exception_encountered_utils.h>
#include <aiacore/aia_json_constants.h>
#include <aiacore/aia_json_message.h>
#include <aiacore/aia_json_utils.h>
#include <aiacore/aia_utils.h>

#include <inttypes.h>

/**
 * Underlying struct that contains all the data required to present the @c
 * AiaClockManager_t abstraction.
 */
struct AiaClockManager
{
    /** Used to publish messages on the event topic. */
    AiaRegulator_t* const eventRegulator;

    /** Used to notify observers after the device clock is synchronized. */
    const AiaClockSynchronizedCallback_t notifyObserverCb;

    /** User data associated with @c notifyObserverCb. */
    void* const notifyObserverCbUserData;
};

AiaClockManager_t* AiaClockManager_Create(
    AiaRegulator_t* eventRegulator,
    AiaClockSynchronizedCallback_t notifyObserverCb,
    void* notifyObserverCbUserData )
{
    if( !eventRegulator )
    {
        AiaLogError( "NULL eventRegulator" );
        return NULL;
    }
    if( !notifyObserverCb )
    {
        AiaLogDebug( "NULL notifyObserverCb" );
    }

    AiaClockManager_t* clockManager =
        (AiaClockManager_t*)AiaCalloc( 1, sizeof( AiaClockManager_t ) );
    if( !clockManager )
    {
        AiaLogError( "AiaCalloc failed, bytes=%zu.",
                     sizeof( AiaClockManager_t ) );
        return NULL;
    }

    *(AiaRegulator_t**)&clockManager->eventRegulator = eventRegulator;
    *(AiaClockSynchronizedCallback_t*)&clockManager->notifyObserverCb =
        notifyObserverCb;
    *(void**)&clockManager->notifyObserverCbUserData = notifyObserverCbUserData;

    return clockManager;
}

void AiaClockManager_Destroy( AiaClockManager_t* clockManager )
{
    if( !clockManager )
    {
        AiaLogDebug( "Null clockManager." );
        return;
    }
    AiaFree( clockManager );
}

bool AiaClockManager_SynchronizeClock( AiaClockManager_t* clockManager )
{
    AiaAssert( clockManager );
    if( !clockManager )
    {
        AiaLogError( "Null clockManager." );
        return false;
    }

    AiaJsonMessage_t* synchronizeClockEvent =
        AiaJsonMessage_Create( AIA_EVENTS_SYNCHRONIZE_CLOCK, NULL, NULL );
    if( !synchronizeClockEvent )
    {
        AiaLogError( "AiaJsonMessage_Create failed" );
        return false;
    }
    if( !AiaRegulator_Write(
            clockManager->eventRegulator,
            AiaJsonMessage_ToMessage( synchronizeClockEvent ) ) )
    {
        AiaLogError( "AiaRegulator_Write failed" );
        AiaJsonMessage_Destroy( synchronizeClockEvent );
        return false;
    }
    return true;
}

void AiaClockManager_OnSetClockDirectiveReceived(
    void* manager, const void* payload, size_t size,
    AiaSequenceNumber_t sequenceNumber, size_t index )
{
    AiaClockManager_t* clockManager = (AiaClockManager_t*)manager;
    AiaAssert( clockManager );

    if( !clockManager )
    {
        AiaLogError( "Null clockManager" );
        return;
    }

    AiaAssert( payload );
    if( !payload )
    {
        AiaLogError( "Null payload, sequenceNumber=%" PRIu32 ", index=%zu",
                     sequenceNumber, index );
        return;
    }

    AiaJsonLongType currentTimeInSeconds = 0;
    if( !AiaJsonUtils_ExtractLong( payload, size,
                                   AIA_SET_CLOCK_CURRENT_TIME_KEY,
                                   sizeof( AIA_SET_CLOCK_CURRENT_TIME_KEY ) - 1,
                                   &currentTimeInSeconds ) )
    {
        AiaLogError( "Failed to get " AIA_SET_CLOCK_CURRENT_TIME_KEY );
        AiaJsonMessage_t* malformedMessageEvent =
            generateMalformedMessageExceptionEncounteredEvent(
                sequenceNumber, index, AIA_TOPIC_DIRECTIVE );
        if( !AiaRegulator_Write(
                clockManager->eventRegulator,
                AiaJsonMessage_ToMessage( malformedMessageEvent ) ) )
        {
            AiaLogError( "AiaRegulator_Write failed" );
            AiaJsonMessage_Destroy( malformedMessageEvent );
            return;
        }
        return;
    }

    AiaLogInfo( "SetClock received, seconds since NTP epoch=%" PRIu64,
                currentTimeInSeconds );
    AiaClock_SetTimeSinceNTPEpoch( currentTimeInSeconds );

    /* Notify the observers */
    if( clockManager->notifyObserverCb )
    {
        clockManager->notifyObserverCb( clockManager->notifyObserverCbUserData,
                                        currentTimeInSeconds );
    }
}
