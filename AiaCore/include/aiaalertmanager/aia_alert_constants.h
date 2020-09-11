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
 * @file aia_alert_constants.h
 * @brief Constants related to Aia alert messages.
 */

#ifndef AIA_ALERT_CONSTANTS_H_
#define AIA_ALERT_CONSTANTS_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <aiacore/aia_utils.h>

/**
 * Static strings for each of the @c AiaAlertType_t values.
 */
/** @{ */

#define AIA_ALERT_TYPE_TIMER_STRING "TIMER"
#define AIA_ALERT_TYPE_ALARM_STRING "ALARM"
#define AIA_ALERT_TYPE_REMINDER_STRING "REMINDER"

/** @} */

/**
 * Supported alert types.
 */
typedef enum AiaAlertType
{
    AIA_ALERT_TYPE_TIMER,
    AIA_ALERT_TYPE_ALARM,
    AIA_ALERT_TYPE_REMINDER
} AiaAlertType_t;

typedef uint8_t AiaAlertStorageType_t;

/**
 * Utility function used to convert a given @c AiaAlertType_t to a
 * string for logging and message sending purposes.
 *
 * @param alert The @c AiaAlertType_t to convert.
 * @return The alert type as a string or @c NULL on failure.
 */
inline const char* AiaAlertType_ToString( AiaAlertType_t alert )
{
    switch( alert )
    {
        case AIA_ALERT_TYPE_TIMER:
            return AIA_ALERT_TYPE_TIMER_STRING;
        case AIA_ALERT_TYPE_ALARM:
            return AIA_ALERT_TYPE_ALARM_STRING;
        case AIA_ALERT_TYPE_REMINDER:
            return AIA_ALERT_TYPE_REMINDER_STRING;
    }
    AiaLogError( "Unknown alert type %d.", alert );
    AiaAssert( false );
    return "";
}

/**
 * @param alertType An alert type to get the length of the string representation
 * of.
 * @return The length of the string representation of @c alertType, without a
 * null-terminating character.
 */
inline size_t AiaAlertType_GetLength( AiaAlertType_t alertType )
{
    switch( alertType )
    {
        case AIA_ALERT_TYPE_TIMER:
            return sizeof( AIA_ALERT_TYPE_TIMER_STRING ) - 1;
        case AIA_ALERT_TYPE_ALARM:
            return sizeof( AIA_ALERT_TYPE_ALARM_STRING ) - 1;
        case AIA_ALERT_TYPE_REMINDER:
            return sizeof( AIA_ALERT_TYPE_REMINDER_STRING ) - 1;
    }
    AiaLogError( "Unknown alertType %d.", alertType );
    AiaAssert( false );
    return 0;
}

/**
 * @param alertTypeString A string to conver to an @c AiaAlertType_t.
 * @param alertTypeStringLength The length of @c alertTypeString, or 0 if @c
 * alertTypeString is null-terminated.
 * @param[out] alertType An alert type pointer to return the @c AiaAlertType_t
 * value for @c alertTypeString.
 * @return @c true if alertTypeString was converted successfully, else @c false.
 */
inline bool AiaAlertType_FromString( const char* alertTypeString,
                                     size_t alertTypeStringLength,
                                     AiaAlertType_t* alertType )
{
    static const AiaAlertType_t alertTypes[] = { AIA_ALERT_TYPE_TIMER,
                                                 AIA_ALERT_TYPE_ALARM,
                                                 AIA_ALERT_TYPE_REMINDER };
    if( !alertTypeString )
    {
        AiaLogError( "Null alertTypeString." );
        return false;
    }
    if( !alertType )
    {
        AiaLogError( "Null alertType." );
        return false;
    }
    if( !alertTypeStringLength )
    {
        alertTypeStringLength = strlen( alertTypeString );
    }
    for( size_t i = 0; i < AiaArrayLength( alertTypes ); ++i )
    {
        if( AiaAlertType_GetLength( alertTypes[ i ] ) ==
                alertTypeStringLength &&
            strncmp( alertTypeString, AiaAlertType_ToString( alertTypes[ i ] ),
                     alertTypeStringLength ) == 0 )
        {
            *alertType = alertTypes[ i ];
            return true;
        }
    }
    AiaLogError( "Unknown alertTypeString \"%.*s\".", alertTypeStringLength,
                 alertTypeString );
    return false;
}

/** 8 characters in alert tokens. */
#define AIA_ALERT_TOKEN_CHARS ( (size_t)8 )

/**
 * Expiration duration of alerts in persistent storage.
 * @see
 * https://developer.amazon.com/en-US/docs/alexa/alexa-voice-service/alerts-overview.html#locally-managing-alerts
 */
#define AIA_ALERT_EXPIRATION_DURATION ( (AiaDurationSeconds_t)1800 )

/**
 * This defines how often to check the speaker buffer status (if @c
 * AIA_ENABLE_SPEAKER is defined) for the underrun
 * status and the UX state for THINKING state to decide if the device
 * should disconnect from the service and start playing offline alerts.
 */
/* TODO: ADSER-1971 Investigate the problem with MacOS periodic timers. This
 * timeout is supposed to be 5 seconds; however, there is a problem with MacOS
 * periodic timers that causes it to go off each second when set to 5 seconds.
 */
#define AIA_OFFLINE_ALERT_STATUS_CHECK_CADENCE_MS ( (AiaDurationMs_t)4000 )

#ifdef AIA_ENABLE_SPEAKER
/**
 * Limit of underruns that can occur within @c
 * AIA_OFFLINE_ALERT_STATUS_CHECK_CADENCE_MS milliseconds before disconnecting
 * from the service and starting to play offline alerts.
 */
#define AIA_SPEAKER_STATUS_UNDERRUN_LIMIT 4
#endif

/** The total size of an alert in persistent storage */
static const size_t AIA_SIZE_OF_ALERT_IN_BYTES =
    sizeof( AiaAlertStorageType_t ) +
    ( ( AIA_ALERT_TOKEN_CHARS ) * ( sizeof( char ) ) ) +
    sizeof( AiaTimepointSeconds_t ) + sizeof( AiaDurationMs_t );

#endif /* ifndef AIA_ALERT_CONSTANTS_H_ */
