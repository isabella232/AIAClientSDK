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
 * @file aia_button_command_sender.c
 * @brief Implements functions for the AiaButtonCommandSender_t type.
 */

/* The config header is always included first. */
#include <aia_config.h>

#include <aiacore/aia_button_command_sender.h>

#include <aiacore/aia_events.h>
#include <aiacore/aia_json_message.h>

#include <stdio.h>

/**
 * Underlying struct that contains all the data required to present the @c
 * AiaButtonCommandSender_t abstraction.
 */
struct AiaButtonCommandSender
{
    /** Used to publish messages on the event topic. */
    AiaRegulator_t* const eventRegulator;

    /** Optionally used to stop local playback. @c NULL if not enabled. */
    const AiaStopPlayback_t stopPlayback;

    /** User data associated with @c stopPlayback. */
    void* const stopPlaybackUserData;
};

/**
 * Helper function used to generate an @c AiaJsonMessage_t that contains the @c
 * ButtonCommandIussued event.
 *
 * @param button The button pressed
 * @return The generated message or @c NULL on failure.
 */
static AiaJsonMessage_t* generateButtonCommandIssuedEvent(
    AiaButtonCommand_t button );

AiaButtonCommandSender_t* AiaButtonCommandSender_Create(
    AiaRegulator_t* eventRegulator, AiaStopPlayback_t stopPlayback,
    void* stopPlaybackUserData )
{
    if( !eventRegulator )
    {
        AiaLogError( "NULL eventRegulator" );
        return NULL;
    }

    AiaButtonCommandSender_t* buttonCommandSender =
        (AiaButtonCommandSender_t*)AiaCalloc(
            1, sizeof( AiaButtonCommandSender_t ) );
    if( !buttonCommandSender )
    {
        AiaLogError( "AiaCalloc failed, bytes=%zu.",
                     sizeof( AiaButtonCommandSender_t ) );
        return NULL;
    }

    *(AiaRegulator_t**)&buttonCommandSender->eventRegulator = eventRegulator;
    *(AiaStopPlayback_t*)&buttonCommandSender->stopPlayback = stopPlayback;
    *(void**)&buttonCommandSender->stopPlaybackUserData = stopPlaybackUserData;

    return buttonCommandSender;
}

void AiaButtonCommandSender_Destroy(
    AiaButtonCommandSender_t* buttonCommandSender )
{
    if( !buttonCommandSender )
    {
        AiaLogDebug( "Null buttonCommandSender." );
        return;
    }
    AiaFree( buttonCommandSender );
}

bool AiaButtonCommandSender_OnButtonPressed(
    AiaButtonCommandSender_t* buttonCommandSender, AiaButtonCommand_t button )
{
    AiaAssert( buttonCommandSender );
    if( !buttonCommandSender )
    {
        AiaLogError( "Null buttonCommandSender." );
        return false;
    }

    AiaJsonMessage_t* buttonCommandIssuedEvent = NULL;
    switch( button )
    {
        case AIA_BUTTON_STOP:
            /* Fall-through */
        case AIA_BUTTON_PAUSE:
            if( buttonCommandSender->stopPlayback )
            {
                buttonCommandSender->stopPlayback(
                    buttonCommandSender->stopPlaybackUserData );
            }
            /* Fall-through */
        case AIA_BUTTON_PLAY:
            /* Fall-through */
        case AIA_BUTTON_NEXT:
            /* Fall-through */
        case AIA_BUTTON_PREVIOUS:
            buttonCommandIssuedEvent =
                generateButtonCommandIssuedEvent( button );
            if( !buttonCommandIssuedEvent )
            {
                AiaLogError( "generateButtonCommandIssuedEvent failed" );
                return false;
            }
            if( !AiaRegulator_Write(
                    buttonCommandSender->eventRegulator,
                    AiaJsonMessage_ToMessage( buttonCommandIssuedEvent ) ) )
            {
                AiaLogError( "AiaRegulator_Write failed" );
                AiaJsonMessage_Destroy( buttonCommandIssuedEvent );
                return false;
            }
    }
    return true;
}

static AiaJsonMessage_t* generateButtonCommandIssuedEvent(
    AiaButtonCommand_t button )
{
    /* clang-format off */
    static const char* formatPayload =
        "{"
            "\""AIA_BUTTON_COMMAND_ISSUED_COMMAND_KEY"\":\"%s\""
        "}";
    /* clang-format on */
    int numCharsRequired =
        snprintf( NULL, 0, formatPayload, AiaButtonCommand_ToString( button ) );
    if( numCharsRequired < 0 )
    {
        AiaLogError( "snprintf failed, ret=%d", numCharsRequired );
        return NULL;
    }
    char fullPayloadBuffer[ numCharsRequired + 1 ];
    if( snprintf( fullPayloadBuffer, numCharsRequired + 1, formatPayload,
                  AiaButtonCommand_ToString( button ) ) < 0 )
    {
        AiaLogError( "snprintf failed" );
        return NULL;
    }

    AiaJsonMessage_t* jsonMessage = AiaJsonMessage_Create(
        AIA_EVENTS_BUTTON_COMMAND_ISSUED, NULL, fullPayloadBuffer );
    return jsonMessage;
}
