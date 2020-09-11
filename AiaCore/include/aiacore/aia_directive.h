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
 * @file aia_directive.h
 * @brief Constants related to Directives.
 */

#ifndef AIA_DIRECTIVE_H_
#define AIA_DIRECTIVE_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <aiacore/aia_utils.h>

/**
 * Directives used in this SDK.
 */
typedef enum AiaDirective
{
    /** Tells device to rotate the shared secret used to encrypt AIS messages.
     */
    AIA_DIRECTIVE_ROTATE_SECRET,
    /** Tells the device about changes in Alexa's attention state. */
    AIA_DIRECTIVE_SET_ATTENTION_STATE,
    /** Tells the device about a server-detected error occurence. */
    AIA_DIRECTIVE_EXCEPTION,
#ifdef AIA_ENABLE_SPEAKER
    /** Tells the device to open the speaker and play an audio stream. */
    AIA_DIRECTIVE_OPEN_SPEAKER,
    /** Tells the device to close the speaker and stop playing an audio stream.
     */
    AIA_DIRECTIVE_CLOSE_SPEAKER,
    /** Tells the device to adjust the speaker volume. */
    AIA_DIRECTIVE_SET_VOLUME,
#endif
#ifdef AIA_ENABLE_MICROPHONE
    /** Tells the device to open the microphone. */
    AIA_DIRECTIVE_OPEN_MICROPHONE,
    /** Tells the device to close the microphone immediately. */
    AIA_DIRECTIVE_CLOSE_MICROPHONE,
#endif
#ifdef AIA_ENABLE_ALERTS
    /**
     * Tells device to adjust the volume of alerts playback received via
     * SetAlert directive.
     */
    AIA_DIRECTIVE_SET_ALERT_VOLUME,
    /** Tells the device to save an alert to persistent storage. */
    AIA_DIRECTIVE_SET_ALERT,
    /** Tells the device to delete an alert from local memory. */
    AIA_DIRECTIVE_DELETE_ALERT,
#endif
#ifdef AIA_ENABLE_CLOCK
    /** Tells the device to set its clock to the provided time. */
    AIA_DIRECTIVE_SET_CLOCK
#endif
} AiaDirective_t;

/**
 * Static strings for each of the @c AiaDirective_t values.
 */
/** @{ */

#define AIA_DIRECTIVE_ROTATE_SECRET_STRING "RotateSecret"
#define AIA_DIRECTIVE_SET_ATTENTION_STATE_STRING "SetAttentionState"
#define AIA_DIRECTIVE_OPEN_SPEAKER_STRING "OpenSpeaker"
#define AIA_DIRECTIVE_CLOSE_SPEAKER_STRING "CloseSpeaker"
#define AIA_DIRECTIVE_OPEN_MICROPHONE_STRING "OpenMicrophone"
#define AIA_DIRECTIVE_CLOSE_MICROPHONE_STRING "CloseMicrophone"
#define AIA_DIRECTIVE_SET_VOLUME_STRING "SetVolume"
#define AIA_DIRECTIVE_SET_ALERT_VOLUME_STRING "SetAlertVolume"
#define AIA_DIRECTIVE_SET_CLOCK_STRING "SetClock"
#define AIA_DIRECTIVE_SET_ALERT_STRING "SetAlert"
#define AIA_DIRECTIVE_DELETE_ALERT_STRING "DeleteAlert"
#define AIA_DIRECTIVE_EXCEPTION_STRING "Exception"

/** @} */

/**
 * @param directive A directive to get the string representation of.
 * @return The string representation of @c directive.
 */
inline const char* AiaDirective_ToString( AiaDirective_t directive )
{
    switch( directive )
    {
#ifdef AIA_ENABLE_SPEAKER
        case AIA_DIRECTIVE_OPEN_SPEAKER:
            return AIA_DIRECTIVE_OPEN_SPEAKER_STRING;
        case AIA_DIRECTIVE_CLOSE_SPEAKER:
            return AIA_DIRECTIVE_CLOSE_SPEAKER_STRING;
        case AIA_DIRECTIVE_SET_VOLUME:
            return AIA_DIRECTIVE_SET_VOLUME_STRING;
#endif
#ifdef AIA_ENABLE_MICROPHONE
        case AIA_DIRECTIVE_OPEN_MICROPHONE:
            return AIA_DIRECTIVE_OPEN_MICROPHONE_STRING;
        case AIA_DIRECTIVE_CLOSE_MICROPHONE:
            return AIA_DIRECTIVE_CLOSE_MICROPHONE_STRING;
#endif
#ifdef AIA_ENABLE_ALERTS
        case AIA_DIRECTIVE_SET_ALERT_VOLUME:
            return AIA_DIRECTIVE_SET_ALERT_VOLUME_STRING;
        case AIA_DIRECTIVE_SET_ALERT:
            return AIA_DIRECTIVE_SET_ALERT_STRING;
        case AIA_DIRECTIVE_DELETE_ALERT:
            return AIA_DIRECTIVE_DELETE_ALERT_STRING;
#endif
#ifdef AIA_ENABLE_CLOCK
        case AIA_DIRECTIVE_SET_CLOCK:
            return AIA_DIRECTIVE_SET_CLOCK_STRING;
#endif
        case AIA_DIRECTIVE_ROTATE_SECRET:
            return AIA_DIRECTIVE_ROTATE_SECRET_STRING;
        case AIA_DIRECTIVE_SET_ATTENTION_STATE:
            return AIA_DIRECTIVE_SET_ATTENTION_STATE_STRING;
        case AIA_DIRECTIVE_EXCEPTION:
            return AIA_DIRECTIVE_EXCEPTION_STRING;
    }
    AiaLogError( "Unknown directive %d.", directive );
    AiaAssert( false );
    return "";
}

/**
 * @param directive A directive to get the length of the string representation
 * of.
 * @return The length of the string representation of @c directive.
 */
inline size_t AiaDirective_GetLength( AiaDirective_t directive )
{
    switch( directive )
    {
#ifdef AIA_ENABLE_SPEAKER
        case AIA_DIRECTIVE_OPEN_SPEAKER:
            return sizeof( AIA_DIRECTIVE_OPEN_SPEAKER_STRING ) - 1;
        case AIA_DIRECTIVE_CLOSE_SPEAKER:
            return sizeof( AIA_DIRECTIVE_CLOSE_SPEAKER_STRING ) - 1;
        case AIA_DIRECTIVE_SET_VOLUME:
            return sizeof( AIA_DIRECTIVE_SET_VOLUME_STRING ) - 1;
#endif
#ifdef AIA_ENABLE_MICROPHONE
        case AIA_DIRECTIVE_OPEN_MICROPHONE:
            return sizeof( AIA_DIRECTIVE_OPEN_MICROPHONE_STRING ) - 1;
        case AIA_DIRECTIVE_CLOSE_MICROPHONE:
            return sizeof( AIA_DIRECTIVE_CLOSE_MICROPHONE_STRING ) - 1;
#endif
#ifdef AIA_ENABLE_ALERTS
        case AIA_DIRECTIVE_SET_ALERT_VOLUME:
            return sizeof( AIA_DIRECTIVE_SET_ALERT_VOLUME_STRING ) - 1;
        case AIA_DIRECTIVE_SET_ALERT:
            return sizeof( AIA_DIRECTIVE_SET_ALERT_STRING ) - 1;
        case AIA_DIRECTIVE_DELETE_ALERT:
            return sizeof( AIA_DIRECTIVE_DELETE_ALERT_STRING ) - 1;
#endif
#ifdef AIA_ENABLE_CLOCK
        case AIA_DIRECTIVE_SET_CLOCK:
            return sizeof( AIA_DIRECTIVE_SET_CLOCK_STRING ) - 1;
#endif
        case AIA_DIRECTIVE_ROTATE_SECRET:
            return sizeof( AIA_DIRECTIVE_ROTATE_SECRET_STRING ) - 1;
        case AIA_DIRECTIVE_SET_ATTENTION_STATE:
            return sizeof( AIA_DIRECTIVE_SET_ATTENTION_STATE_STRING ) - 1;
        case AIA_DIRECTIVE_EXCEPTION:
            return sizeof( AIA_DIRECTIVE_EXCEPTION_STRING ) - 1;
    }
    AiaLogError( "Unknown directive %d.", directive );
    AiaAssert( false );
    return 0;
}

/**
 * @param directiveString A string to convert to an @c AiaDirective_t.
 * @param directiveStringLength The length of @c directiveString, or 0 if @c
 * directiveString is null-terminated.
 * @param[out] directive A directive pointer to return the @c AiaDirective_t
 * value for @c directiveString.
 * @return @c true if directiveString was converted successfully, else @c false.
 */
inline bool AiaDirective_FromString( const char* directiveString,
                                     size_t directiveStringLength,
                                     AiaDirective_t* directive )
{
    static const AiaDirective_t directives[] = {
#ifdef AIA_ENABLE_SPEAKER
        AIA_DIRECTIVE_OPEN_SPEAKER,     AIA_DIRECTIVE_CLOSE_SPEAKER,
        AIA_DIRECTIVE_SET_VOLUME,
#endif
#ifdef AIA_ENABLE_MICROPHONE
        AIA_DIRECTIVE_OPEN_MICROPHONE,  AIA_DIRECTIVE_CLOSE_MICROPHONE,
#endif
#ifdef AIA_ENABLE_ALERTS
        AIA_DIRECTIVE_SET_ALERT_VOLUME, AIA_DIRECTIVE_SET_ALERT,
        AIA_DIRECTIVE_DELETE_ALERT,
#endif
#ifdef AIA_ENABLE_CLOCK
        AIA_DIRECTIVE_SET_CLOCK,
#endif
        AIA_DIRECTIVE_ROTATE_SECRET,    AIA_DIRECTIVE_SET_ATTENTION_STATE,
        AIA_DIRECTIVE_EXCEPTION
    };

    if( !directiveString )
    {
        AiaLogError( "Null directiveString." );
        return false;
    }
    if( !directive )
    {
        AiaLogError( "Null directive." );
        return false;
    }
    if( !directiveStringLength )
    {
        directiveStringLength = strlen( directiveString );
    }
    for( size_t i = 0; i < AiaArrayLength( directives ); ++i )
    {
        if( AiaDirective_GetLength( directives[ i ] ) ==
                directiveStringLength &&
            strncmp( directiveString, AiaDirective_ToString( directives[ i ] ),
                     directiveStringLength ) == 0 )
        {
            *directive = directives[ i ];
            return true;
        }
    }
    AiaLogError( "Unknown directiveString \"%.*s\".", directiveStringLength,
                 directiveString );
    return false;
}

#endif /* ifndef AIA_DIRECTIVE_H_ */
