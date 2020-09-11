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
 * @file aia_exception_manager.c
 * @brief Implements functions for the AiaExceptionManager_t type.
 */

/* The config header is always included first. */
#include <aia_config.h>

#include <aiaexceptionmanager/aia_exception_constants.h>
#include <aiaexceptionmanager/aia_exception_manager.h>

#include <aiacore/aia_exception_encountered_utils.h>
#include <aiacore/aia_json_utils.h>
#include <aiacore/aia_topic.h>

#include <inttypes.h>

/** Private data for the @c AiaExceptionManager_t type. */
struct AiaExceptionManager
{
    /** Used to publish messages on the event topic. */
    AiaRegulator_t* const eventRegulator;

    /** Optional callback function to run when Exception directive is received
     */
    const AiaExceptionManagerOnExceptionCallback_t onException;

    /** User data to pass to @c onException. */
    void* const onExceptionUserData;
};

/**
 * An internal function to send a @c MalformedMessage event to the service.
 *
 * @param eventRegulator The eventRegulator used to send outbound messages.
 * @param sequenceNumber The sequence number of the malformed message.
 * @param index The index of the message that was malformed.
 * @param topic The topic that the malformed message was received on.
 */
static void sendMalformedMessageEvent( AiaRegulator_t* eventRegulator,
                                       AiaSequenceNumber_t sequenceNumber,
                                       size_t index, AiaTopic_t topic )
{
    AiaJsonMessage_t* malformedMessageEvent =
        generateMalformedMessageExceptionEncounteredEvent( sequenceNumber,
                                                           index, topic );
    if( !malformedMessageEvent )
    {
        AiaLogError(
            "generateMalformedMessageExceptionEncounteredEvent failed" );
    }
    else if( !AiaRegulator_Write(
                 eventRegulator,
                 AiaJsonMessage_ToMessage( malformedMessageEvent ) ) )
    {
        AiaLogError( "AiaRegulator_Write failed" );
        AiaJsonMessage_Destroy( malformedMessageEvent );
    }
}

AiaExceptionManager_t* AiaExceptionManager_Create(
    AiaRegulator_t* eventRegulator,
    AiaExceptionManagerOnExceptionCallback_t onException,
    void* onExceptionUserData )
{
    if( !eventRegulator )
    {
        AiaLogError( "NULL eventRegulator" );
        return NULL;
    }

    AiaExceptionManager_t* exceptionManager =
        (AiaExceptionManager_t*)AiaCalloc( 1, sizeof( AiaExceptionManager_t ) );
    if( !exceptionManager )
    {
        AiaLogError( "AiaCalloc failed, bytes=%zu.",
                     sizeof( AiaExceptionManager_t ) );
        return NULL;
    }

    *(AiaRegulator_t**)&exceptionManager->eventRegulator = eventRegulator;
    *(AiaExceptionManagerOnExceptionCallback_t*)&exceptionManager->onException =
        onException;
    *(void**)&exceptionManager->onExceptionUserData = onExceptionUserData;

    return exceptionManager;
}

void AiaExceptionManager_OnExceptionReceived(
    void* manager, const void* payload, size_t size,
    AiaSequenceNumber_t sequenceNumber, size_t index )
{
    AiaExceptionManager_t* exceptionManager = (AiaExceptionManager_t*)manager;
    AiaAssert( exceptionManager );

    if( !exceptionManager )
    {
        AiaLogError( "Null exceptionManager" );
        return;
    }

    AiaAssert( payload );
    if( !payload )
    {
        AiaLogError( "Null payload, sequenceNumber=%" PRIu32 ", index=%zu",
                     sequenceNumber, index );
        return;
    }

    const char* codeStr;
    size_t codeLen;
    if( !AiaFindJsonValue( payload, size, AIA_EXCEPTION_CODE_KEY,
                           sizeof( AIA_EXCEPTION_CODE_KEY ) - 1, &codeStr,
                           &codeLen ) )
    {
        AiaLogError( "Failed to parse the %s key in the payload",
                     AIA_EXCEPTION_CODE_KEY );
        sendMalformedMessageEvent( exceptionManager->eventRegulator,
                                   sequenceNumber, index, AIA_TOPIC_DIRECTIVE );
        return;
    }
    if( !AiaJsonUtils_UnquoteString( &codeStr, &codeLen ) )
    {
        AiaLogError( "Malformed JSON" );
        sendMalformedMessageEvent( exceptionManager->eventRegulator,
                                   sequenceNumber, index, AIA_TOPIC_DIRECTIVE );
        return;
    }

    AiaExceptionCode_t code;
    if( !AiaExceptionCode_FromString( codeStr, codeLen, &code ) )
    {
        AiaLogError( "Invalid code, code=%.*s", codeLen, codeStr );
        sendMalformedMessageEvent( exceptionManager->eventRegulator,
                                   sequenceNumber, index, AIA_TOPIC_DIRECTIVE );
        return;
    }

    const char* description;
    size_t descriptionLen;
    if( !AiaFindJsonValue( payload, size, AIA_EXCEPTION_DESCRIPTION_KEY,
                           sizeof( AIA_EXCEPTION_DESCRIPTION_KEY ) - 1,
                           &description, &descriptionLen ) )
    {
        AiaLogDebug( "Optional %s key in the response body missing",
                     AIA_EXCEPTION_DESCRIPTION_KEY );
        AiaLogInfo( "Exception directive received. code: %.*s", codeLen,
                    codeStr );
    }
    else if( !AiaJsonUtils_UnquoteString( &description, &descriptionLen ) )
    {
        AiaLogError( "Malformed JSON" );
        sendMalformedMessageEvent( exceptionManager->eventRegulator,
                                   sequenceNumber, index, AIA_TOPIC_DIRECTIVE );
    }
    else
    {
        AiaLogInfo(
            "Exception directive received. code: %.*s, description: %.*s",
            codeLen, codeStr, descriptionLen, description );
    }

    if( exceptionManager->onException )
    {
        exceptionManager->onException( exceptionManager->onExceptionUserData,
                                       code );
    }
}

void AiaExceptionManager_Destroy( AiaExceptionManager_t* exceptionManager )
{
    if( !exceptionManager )
    {
        AiaLogDebug( "Null exceptionManager." );
        return;
    }
    AiaFree( exceptionManager );
}
