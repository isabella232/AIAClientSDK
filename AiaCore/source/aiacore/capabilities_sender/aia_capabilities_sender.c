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
 * @file aia_capabilities_sender.c
 * @brief Implements functions for the AiaCapabilitiesSender_t type.
 */

/* The config header is always included first. */
#include <aia_config.h>

/* Aia headers */
#include <aia_capabilities_config.h>
#include <aiacore/aia_capabilities_constants.h>
#include <aiacore/aia_events.h>
#include <aiacore/aia_json_constants.h>
#include <aiacore/aia_json_message.h>
#include <aiacore/aia_json_utils.h>
#include <aiacore/capabilities_sender/aia_capabilities_sender.h>
#include <aiacore/capabilities_sender/aia_capabilities_sender_state.h>
#include <aiacore/private/aia_capabilities_sender.h>

/* Standard library includes */
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

/**
 * Underlying struct that contains all the data required to present the @c
 * AiaCapabilitiesSender_t abstraction.
 */
struct AiaCapabilitiesSender
{
    /** Mutex used to guard against asynchronous calls in threaded
     * environments. */
    AiaMutex_t mutex;

    /** @name Variables synchronized by mutex. */
    /** @{ */

    /** The current state. */
    AiaCapabilitiesSenderState_t state;

    /** The observer to notify of state changes. */
    const AiaCapabilitiesObserver_t stateObserver;

    /** Context associated with @c stateObserver. */
    void* const stateObserverUserData;

    /** @} */

    /** Used to publish messages on the capabilities topic. */
    AiaRegulator_t* const capabilitiesRegulator;
};

static const char* AIA_CAPABILITIES_PAYLOAD_FORMAT =
    /* clang-format off */
"{"
    "\""AIA_CAPABILITIES_KEY"\":["
    	#ifdef AIA_ENABLE_SPEAKER
    	"{"
    		"\""AIA_CAPABILITIES_TYPE_KEY"\":\""AIA_CAPABILITIES_AIS_INTERFACE"\","
    		"\""AIA_CAPABILITIES_AIS_INTERFACE_KEY"\":\""AIA_CAPABILITIES_SPEAKER"\","
    		"\""AIA_CAPABILITIES_VERSION_KEY"\":\""AIA_SPEAKER_VERSION"\","
    		"\""AIA_CAPABILITIES_CONFIGURATIONS_KEY"\":{" 
    			"\""AIA_CAPABILITIES_SPEAKER_AUDIO_BUFFER"\":{"
    				"\""AIA_CAPABILITIES_SPEAKER_AUDIO_BUFFER_SIZE"\":%"PRIu64"," 
    				"\""AIA_CAPABILITIES_SPEAKER_AUDIO_REPORTING"\":{"
    					"\""AIA_CAPABILITIES_SPEAKER_AUDIO_OVERRUN_THRESHOLD"\":%"PRIu64"," 
    					"\""AIA_CAPABILITIES_SPEAKER_AUDIO_UNDERRUN_THRESHOLD"\":%"PRIu64"" 
    				"}"
    			"},"
    			"\""AIA_CAPABILITIES_SPEAKER_AUDIO_DECODER"\":{"
    				"\""AIA_CAPABILITIES_SPEAKER_AUDIO_FORMAT"\": \""AIA_SPEAKER_AUDIO_DECODER_FORMAT"\","
    				"\""AIA_CAPABILITIES_SPEAKER_AUDIO_BITRATE"\": {"
    					"\""AIA_CAPABILITIES_SPEAKER_AUDIO_TYPE"\": \""AIA_SPEAKER_AUDIO_DECODER_BITRATE_TYPE"\","
    					"\""AIA_CAPABILITIES_SPEAKER_AUDIO_BITS_PER_SECOND"\":%"PRIu64""
    				"},"
    				"\""AIA_CAPABILITIES_SPEAKER_NUM_CHANNELS"\":%"PRIu64"" 
    			"}"
    		"}"
    	"},"
    	#endif
    	#ifdef AIA_ENABLE_MICROPHONE
    	"{"
    		"\""AIA_CAPABILITIES_TYPE_KEY"\":\""AIA_CAPABILITIES_AIS_INTERFACE"\","
    		"\""AIA_CAPABILITIES_AIS_INTERFACE_KEY"\":\""AIA_CAPABILITIES_MICROPHONE"\","
    		"\""AIA_CAPABILITIES_VERSION_KEY"\":\""AIA_MICROPHONE_VERSION"\","
    		"\""AIA_CAPABILITIES_CONFIGURATIONS_KEY"\":{" 
    			"\""AIA_CAPABILITIES_MICROPHONE_AUDIO_ENCODER"\":{"
    				"\""AIA_CAPABILITIES_MICROPHONE_AUDIO_FORMAT"\": \""AIA_MICROPHONE_AUDIO_ENCODER_FORMAT"\""
    			"}"
    		"}"
    	"},"
    	#endif
    	#ifdef AIA_ENABLE_ALERTS
    	"{"
    		"\""AIA_CAPABILITIES_TYPE_KEY"\":\""AIA_CAPABILITIES_AIS_INTERFACE"\","
    		"\""AIA_CAPABILITIES_AIS_INTERFACE_KEY"\":\""AIA_CAPABILITIES_ALERTS"\","
    		"\""AIA_CAPABILITIES_VERSION_KEY"\":\""AIA_ALERTS_VERSION"\","
    		"\""AIA_CAPABILITIES_CONFIGURATIONS_KEY"\":{" 
				"\""AIA_CAPABILITIES_ALERTS_MAX_ALERTS"\":%"PRIu64""
    		"}"
    	"},"
    	#endif
    	#ifdef AIA_ENABLE_CLOCK
    	"{"
    		"\""AIA_CAPABILITIES_TYPE_KEY"\":\""AIA_CAPABILITIES_AIS_INTERFACE"\","
    		"\""AIA_CAPABILITIES_AIS_INTERFACE_KEY"\":\""AIA_CAPABILITIES_CLOCK"\","
    		"\""AIA_CAPABILITIES_VERSION_KEY"\":\""AIA_CLOCK_VERSION"\""
    	"},"
    	#endif
    	"{"
    		"\""AIA_CAPABILITIES_TYPE_KEY"\":\""AIA_CAPABILITIES_AIS_INTERFACE"\","
    		"\""AIA_CAPABILITIES_AIS_INTERFACE_KEY"\":\""AIA_CAPABILITIES_SYSTEM"\","
    		"\""AIA_CAPABILITIES_VERSION_KEY"\":\""AIA_SYSTEM_VERSION"\","
    		"\""AIA_CAPABILITIES_CONFIGURATIONS_KEY"\":{" 
    			"\""AIA_CAPABILITIES_SYSTEM_MQTT"\":{"
    				"\""AIA_CAPABILITIES_SYSTEM_MQTT_MESSAGE"\":{"
    					"\""AIA_CAPABILITIES_SYSTEM_MAX_MESSAGE_SIZE"\":%"PRIu64""
    				"}"
    			"},"
				"\""AIA_CAPABILITIES_SYSTEM_FIRMWARE_VERSION"\": \""AIA_SYSTEM_FIRMWARE_VERSION"\","
				"\""AIA_CAPABILITIES_SYSTEM_LOCALE"\": \""AIA_SYSTEM_LOCALE"\""
    		"}"
    	"}"
    "]"
"}";
/* clang-format on */

#ifdef AIA_ENABLE_SPEAKER
#define AIA_CAPABILITIES_SPEAKER_ARGS                               \
    AIA_AUDIO_BUFFER_SIZE, AIA_AUDIO_BUFFER_OVERRUN_WARN_THRESHOLD, \
        AIA_AUDIO_BUFFER_UNDERRUN_WARN_THRESHOLD,                   \
        AIA_SPEAKER_AUDIO_DECODER_BITS_PER_SECOND,                  \
        AIA_SPEAKER_AUDIO_DECODER_NUM_CHANNELS,
#else
#define AIA_CAPABILITIES_SPEAKER_ARGS
#endif

#ifdef AIA_ENABLE_ALERTS
#define AIA_CAPABILITIES_ALERTS_ARGS AIA_ALERTS_MAX_ALERT_COUNT,
#else
#define AIA_CAPABILITIES_ALERTS_ARGS
#endif

#define AIA_CAPABILITIES_SYSTEM_ARGS AIA_SYSTEM_MQTT_MESSAGE_MAX_SIZE

#define AIA_CAPABILITIES_ARGS                                  \
    AIA_CAPABILITIES_SPEAKER_ARGS AIA_CAPABILITIES_ALERTS_ARGS \
        AIA_CAPABILITIES_SYSTEM_ARGS

/**
 * Helper function used to generate an @c AiaJsonMessage_t that contains the @c
 * Capabilities Publish message using macros defined in @c
 * aia_capabilities_config.h
 *
 * @return The generated message or @c NULL on failure.
 */
static AiaJsonMessage_t* generateCapabilitiesMessage();

AiaCapabilitiesSender_t* AiaCapabilitiesSender_Create(
    AiaRegulator_t* capabilitiesRegulator,
    AiaCapabilitiesObserver_t stateObserver, void* stateObserverUserData )
{
    if( !capabilitiesRegulator )
    {
        AiaLogError( "NULL capabilitiesRegulator" );
        return NULL;
    }

    if( !stateObserver )
    {
        AiaLogError( "NULL stateObserver" );
        return NULL;
    }

    AiaCapabilitiesSender_t* capabilitiesSender =
        (AiaCapabilitiesSender_t*)AiaCalloc(
            1, sizeof( AiaCapabilitiesSender_t ) );
    if( !capabilitiesSender )
    {
        AiaLogError( "AiaCalloc failed, bytes=%zu.",
                     sizeof( AiaCapabilitiesSender_t ) );
        return NULL;
    }

    if( !AiaMutex( Create )( &capabilitiesSender->mutex, false ) )
    {
        AiaLogError( "AiaMutex( Create ) failed." );
        AiaFree( capabilitiesSender );
        return NULL;
    }

    *(AiaRegulator_t**)&capabilitiesSender->capabilitiesRegulator =
        capabilitiesRegulator;
    *(AiaCapabilitiesObserver_t*)&capabilitiesSender->stateObserver =
        stateObserver;
    *(void**)&capabilitiesSender->stateObserverUserData = stateObserverUserData;

    capabilitiesSender->stateObserver(
        capabilitiesSender->state, NULL, 0,
        capabilitiesSender->stateObserverUserData );
    return capabilitiesSender;
}

void AiaCapabilitiesSender_Destroy(
    AiaCapabilitiesSender_t* capabilitiesSender )
{
    if( !capabilitiesSender )
    {
        AiaLogDebug( "Null capabilitiesSender." );
        return;
    }
    AiaMutex( Destroy )( &capabilitiesSender->mutex );
    AiaFree( capabilitiesSender );
}

bool AiaCapabilitiesSender_PublishCapabilities(
    AiaCapabilitiesSender_t* capabilitiesSender )
{
    AiaAssert( capabilitiesSender );
    if( !capabilitiesSender )
    {
        AiaLogError( "Null capabilitiesSender." );
        AiaCriticalFailure();
        return false;
    }

#ifndef AIA_ENABLE_SYSTEM
#error "System Capabilities are required"
#endif

    AiaMutex( Lock )( &capabilitiesSender->mutex );

    if( capabilitiesSender->state == AIA_CAPABILITIES_STATE_PUBLISHED )
    {
        AiaLogError(
            "Capabilities have already been sent, waiting for "
            "acknowledgement" );
        AiaMutex( Unlock )( &capabilitiesSender->mutex );
        return false;
    }
    AiaJsonMessage_t* capabilitiesPublishMessage =
        generateCapabilitiesMessage();
    if( !capabilitiesPublishMessage )
    {
        AiaLogError( "generateCapabilitiesMessage failed" );
        AiaMutex( Unlock )( &capabilitiesSender->mutex );
        return false;
    }
    if( !AiaRegulator_Write(
            capabilitiesSender->capabilitiesRegulator,
            AiaJsonMessage_ToMessage( capabilitiesPublishMessage ) ) )
    {
        AiaLogError( "Capabilities publish failed" );
        AiaMutex( Unlock )( &capabilitiesSender->mutex );
        AiaJsonMessage_Destroy( capabilitiesPublishMessage );
        return false;
    }
    capabilitiesSender->state = AIA_CAPABILITIES_STATE_PUBLISHED;
    capabilitiesSender->stateObserver(
        capabilitiesSender->state, NULL, 0,
        capabilitiesSender->stateObserverUserData );

    AiaMutex( Unlock )( &capabilitiesSender->mutex );
    return true;
}

void AiaCapabilitiesSender_OnCapabilitiesAcknowledgeMessageReceived(
    AiaCapabilitiesSender_t* capabilitiesSender, const char* payload,
    size_t size )
{
    AiaAssert( capabilitiesSender );
    if( !capabilitiesSender )
    {
        AiaLogError( "Null capabilitiesSender" );
        return;
    }

    AiaMutex( Lock )( &capabilitiesSender->mutex );

    if( !payload )
    {
        AiaLogError( "Null payload" );
        /* TODO: ADSER-1532 Close the AIS connection */
        /* Can't send an ExceptionEncountered event since "capabilities" is not
         * an accepted topic */
        capabilitiesSender->state = AIA_CAPABILITIES_STATE_NONE;
        capabilitiesSender->stateObserver(
            capabilitiesSender->state, NULL, 0,
            capabilitiesSender->stateObserverUserData );
        AiaMutex( Unlock )( &capabilitiesSender->mutex );
        return;
    }

    const char* code = NULL;
    size_t codeLen = 0;
    if( !AiaFindJsonValue( payload, size, AIA_CAPABILITIES_ACKNOWLEDGE_CODE_KEY,
                           sizeof( AIA_CAPABILITIES_ACKNOWLEDGE_CODE_KEY ) - 1,
                           &code, &codeLen ) )
    {
        AiaLogError( "No code found" );
        /* TODO: ADSER-1532 Close the AIS connection */
        /* Can't send an ExceptionEncountered event since "capabilities" is not
         * an accepted topic */
        capabilitiesSender->state = AIA_CAPABILITIES_STATE_NONE;
        capabilitiesSender->stateObserver(
            capabilitiesSender->state, NULL, 0,
            capabilitiesSender->stateObserverUserData );
        AiaMutex( Unlock )( &capabilitiesSender->mutex );
        return;
    }
    if( !AiaJsonUtils_UnquoteString( &code, &codeLen ) )
    {
        AiaLogError( "Malformed Json" );
        capabilitiesSender->state = AIA_CAPABILITIES_STATE_NONE;
        capabilitiesSender->stateObserver(
            capabilitiesSender->state, NULL, 0,
            capabilitiesSender->stateObserverUserData );
        AiaMutex( Unlock )( &capabilitiesSender->mutex );
        return;
    }

    if( strncmp( code, AIA_CAPABILITIES_ACCEPTED_CODE, codeLen ) == 0 )
    {
        AiaLogDebug( "Capabilities accepted" );
        capabilitiesSender->state = AIA_CAPABILITIES_STATE_ACCEPTED;
        capabilitiesSender->stateObserver(
            capabilitiesSender->state, NULL, 0,
            capabilitiesSender->stateObserverUserData );
        AiaMutex( Unlock )( &capabilitiesSender->mutex );
        return;
    }
    else if( strncmp( code, AIA_CAPABILITIES_REJECTED_CODE, codeLen ) == 0 )
    {
        AiaLogDebug( "Capabilities rejected" );
        capabilitiesSender->state = AIA_CAPABILITIES_STATE_REJECTED;

        const char* description = NULL;
        size_t descriptionLen = 0;
        if( AiaFindJsonValue(
                payload, size, AIA_CAPABILITIES_ACKNOWLEDGE_DESCRIPTION_KEY,
                sizeof( AIA_CAPABILITIES_ACKNOWLEDGE_DESCRIPTION_KEY ) - 1,
                &description, &descriptionLen ) )
        {
            if( !AiaJsonUtils_UnquoteString( &description, &descriptionLen ) )
            {
                AiaLogError( "Malformed Json" );
            }
        }

        capabilitiesSender->stateObserver(
            capabilitiesSender->state, description, descriptionLen,
            capabilitiesSender->stateObserverUserData );
        AiaMutex( Unlock )( &capabilitiesSender->mutex );
        return;
    }
    else
    {
        AiaLogInfo( "Unknown capabilities code, %.*s", codeLen, code );
    }

    capabilitiesSender->state = AIA_CAPABILITIES_STATE_NONE;
    capabilitiesSender->stateObserver(
        capabilitiesSender->state, NULL, 0,
        capabilitiesSender->stateObserverUserData );
    AiaMutex( Unlock )( &capabilitiesSender->mutex );
}

static AiaJsonMessage_t* generateCapabilitiesMessage()
{
    int numCharsRequired = snprintf( NULL, 0, AIA_CAPABILITIES_PAYLOAD_FORMAT,
                                     AIA_CAPABILITIES_ARGS );
    if( numCharsRequired < 0 )
    {
        AiaLogError( "snprintf failed, ret=%d", numCharsRequired );
        return NULL;
    }
    char* fullPayloadBuffer = AiaCalloc( 1, numCharsRequired + 1 );
    if( !fullPayloadBuffer )
    {
        AiaLogError( "AiaCalloc failed, bytes=%zu.", numCharsRequired + 1 );
        return NULL;
    }
    if( snprintf( fullPayloadBuffer, numCharsRequired + 1,
                  AIA_CAPABILITIES_PAYLOAD_FORMAT, AIA_CAPABILITIES_ARGS ) < 0 )
    {
        AiaLogError( "snprintf failed" );
        AiaFree( fullPayloadBuffer );
        return NULL;
    }
    AiaJsonMessage_t* jsonMessage = AiaJsonMessage_Create(
        AIA_CAPABILITIES_PUBLISH, NULL, fullPayloadBuffer );
    AiaFree( fullPayloadBuffer );
    return jsonMessage;
}
