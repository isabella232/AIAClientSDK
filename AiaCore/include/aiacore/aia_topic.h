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
 * @file aia_topic.h
 * @brief Constants related to Topics.
 */

#ifndef AIA_TOPIC_H_
#define AIA_TOPIC_H_

/* The config header is always included first. */
#include <aia_config.h>

#include "aia_utils.h"

#define AIA_API_VERSION "v1"

/**
 * All the topics used in this SDK.
 *
 * @note These are the leaf nodes that sit within a common topic root.
 */
typedef enum AiaTopic
{
    /** For connection-related messages from the client to the service. */
    AIA_TOPIC_CONNECTION_FROM_CLIENT,
    /** For connection-related messages from the service to the client. */
    AIA_TOPIC_CONNECTION_FROM_SERVICE,
    /** For publishing capabilities from the client to the service. */
    AIA_TOPIC_CAPABILITIES_PUBLISH,
    /** For the service to acknowledge receiving capabilities from the client.
     */
    AIA_TOPIC_CAPABILITIES_ACKNOWLEDGE,
    /** For directives sent from the service to the client. */
    AIA_TOPIC_DIRECTIVE,
    /** For events sent from the client to the service. */
    AIA_TOPIC_EVENT,
    /** For streaming microphone data from the client to the service. */
    AIA_TOPIC_MICROPHONE,
    /** For streaming speaker data from the service to the client. */
    AIA_TOPIC_SPEAKER,
    /** Number of unique topic leaf nodes (not a real topic). */
    AIA_NUM_TOPICS
} AiaTopic_t;

/**
 * @name Static strings for each of the @c AiaTopic_t values.
 *
 * These macros provide the raw topic strings, suitable for compile-time
 * concatenation.
 */
/** @{ */

#define AIA_TOPIC_SEPARATOR_STRING "/"
#define AIA_TOPIC_CONNECTION_STRING "connection"
#define AIA_TOPIC_CONNECTION_FROM_CLIENT_STRING \
    AIA_TOPIC_CONNECTION_STRING AIA_TOPIC_SEPARATOR_STRING "fromclient"
#define AIA_TOPIC_CONNECTION_FROM_SERVICE_STRING \
    AIA_TOPIC_CONNECTION_STRING AIA_TOPIC_SEPARATOR_STRING "fromservice"
#define AIA_TOPIC_CAPABILITIES_STRING "capabilities"
#define AIA_TOPIC_CAPABILITIES_PUBLISH_STRING \
    AIA_TOPIC_CAPABILITIES_STRING AIA_TOPIC_SEPARATOR_STRING "publish"
#define AIA_TOPIC_CAPABILITIES_ACKNOWLEDGE_STRING \
    AIA_TOPIC_CAPABILITIES_STRING AIA_TOPIC_SEPARATOR_STRING "acknowledge"
#define AIA_TOPIC_DIRECTIVE_STRING "directive"
#define AIA_TOPIC_EVENT_STRING "event"
#define AIA_TOPIC_MICROPHONE_STRING "microphone"
#define AIA_TOPIC_SPEAKER_STRING "speaker"
#define AIA_TOPIC_DIRECTIVE_ARRAY_NAME_STRING "directives"
#define AIA_TOPIC_EVENT_ARRAY_NAME_STRING "events"

/** @} */

/** The different topic types supported by this SDK. */
typedef enum AiaTopicType
{
    /** Messages are transmited in binary form. */
    AIA_TOPIC_TYPE_BINARY,
    /** Messages are transmitted in JSON form. */
    AIA_TOPIC_TYPE_JSON
} AiaTopicType_t;

/**
 * @param topic A topic to get the @c AiaTopicType_t for.
 * @return The @c AiaTopicType_t of @c topic.
 */
inline AiaTopicType_t AiaTopic_GetType( AiaTopic_t topic )
{
    switch( topic )
    {
        case AIA_TOPIC_CONNECTION_FROM_CLIENT:
        case AIA_TOPIC_CONNECTION_FROM_SERVICE:
        case AIA_TOPIC_CAPABILITIES_PUBLISH:
        case AIA_TOPIC_CAPABILITIES_ACKNOWLEDGE:
        case AIA_TOPIC_DIRECTIVE:
        case AIA_TOPIC_EVENT:
            return AIA_TOPIC_TYPE_JSON;
        case AIA_TOPIC_MICROPHONE:
        case AIA_TOPIC_SPEAKER:
            return AIA_TOPIC_TYPE_BINARY;
        case AIA_NUM_TOPICS:
            break;
    }
    AiaLogError( "Unknown topic %d.", topic );
    AiaAssert( false );
    return AIA_TOPIC_TYPE_BINARY;
}

/**
 * @param topic A topic to get the string representation of.
 * @return The string representation of @c topic.
 */
inline const char* AiaTopic_ToString( AiaTopic_t topic )
{
    switch( topic )
    {
        case AIA_TOPIC_CONNECTION_FROM_CLIENT:
            return AIA_TOPIC_CONNECTION_FROM_CLIENT_STRING;
        case AIA_TOPIC_CONNECTION_FROM_SERVICE:
            return AIA_TOPIC_CONNECTION_FROM_SERVICE_STRING;
        case AIA_TOPIC_CAPABILITIES_PUBLISH:
            return AIA_TOPIC_CAPABILITIES_PUBLISH_STRING;
        case AIA_TOPIC_CAPABILITIES_ACKNOWLEDGE:
            return AIA_TOPIC_CAPABILITIES_ACKNOWLEDGE_STRING;
        case AIA_TOPIC_DIRECTIVE:
            return AIA_TOPIC_DIRECTIVE_STRING;
        case AIA_TOPIC_EVENT:
            return AIA_TOPIC_EVENT_STRING;
        case AIA_TOPIC_MICROPHONE:
            return AIA_TOPIC_MICROPHONE_STRING;
        case AIA_TOPIC_SPEAKER:
            return AIA_TOPIC_SPEAKER_STRING;
        case AIA_NUM_TOPICS:
            break;
    }
    AiaLogError( "Unknown topic %d.", topic );
    AiaAssert( false );
    return "";
}

/**
 * @param topic A topic to get the JSON array name for.
 * @return The JSON array name for @c topic, or @c NULL if topic does not have a
 * JSON array name.
 */
inline const char* AiaTopic_GetJsonArrayName( AiaTopic_t topic )
{
    switch( topic )
    {
        case AIA_TOPIC_CONNECTION_FROM_CLIENT:
        case AIA_TOPIC_CONNECTION_FROM_SERVICE:
        case AIA_TOPIC_CAPABILITIES_PUBLISH:
        case AIA_TOPIC_CAPABILITIES_ACKNOWLEDGE:
            return NULL;
        case AIA_TOPIC_DIRECTIVE:
            return AIA_TOPIC_DIRECTIVE_ARRAY_NAME_STRING;
        case AIA_TOPIC_EVENT:
            return AIA_TOPIC_EVENT_ARRAY_NAME_STRING;
        case AIA_TOPIC_MICROPHONE:
        case AIA_TOPIC_SPEAKER:
            AiaLogError( "%s is a binary topic.", AiaTopic_ToString( topic ) );
            AiaAssert( false );
            return NULL;
        case AIA_NUM_TOPICS:
            break;
    }
    AiaLogError( "Unknown topic %d.", topic );
    AiaAssert( false );
    return NULL;
}

/**
 * @param topic A topic to get the length of the JSON array name for.
 * @return The length of the JSON array name for @c topic, or @c 0 if topic does
 *     not have a JSON array name.
 */
inline size_t AiaTopic_GetJsonArrayNameLength( AiaTopic_t topic )
{
    switch( topic )
    {
        case AIA_TOPIC_CONNECTION_FROM_CLIENT:
        case AIA_TOPIC_CONNECTION_FROM_SERVICE:
        case AIA_TOPIC_CAPABILITIES_PUBLISH:
        case AIA_TOPIC_CAPABILITIES_ACKNOWLEDGE:
            return 0;
        case AIA_TOPIC_DIRECTIVE:
            return sizeof( AIA_TOPIC_DIRECTIVE_ARRAY_NAME_STRING ) - 1;
        case AIA_TOPIC_EVENT:
            return sizeof( AIA_TOPIC_EVENT_ARRAY_NAME_STRING ) - 1;
        case AIA_TOPIC_MICROPHONE:
        case AIA_TOPIC_SPEAKER:
            AiaLogError( "%s is a binary topic.", AiaTopic_ToString( topic ) );
            AiaAssert( false );
            return 0;
        case AIA_NUM_TOPICS:
            break;
    }
    AiaLogError( "Unknown topic %d.", topic );
    AiaAssert( false );
    return 0;
}

/**
 * @param topic A topic to get the encryption requirements of.
 * @return Whether @c topic is encrypted.
 */
inline bool AiaTopic_IsEncrypted( AiaTopic_t topic )
{
    switch( topic )
    {
        case AIA_TOPIC_CONNECTION_FROM_CLIENT:
        case AIA_TOPIC_CONNECTION_FROM_SERVICE:
            return false;
        case AIA_TOPIC_CAPABILITIES_PUBLISH:
        case AIA_TOPIC_CAPABILITIES_ACKNOWLEDGE:
        case AIA_TOPIC_DIRECTIVE:
        case AIA_TOPIC_EVENT:
        case AIA_TOPIC_MICROPHONE:
        case AIA_TOPIC_SPEAKER:
            return true;
        case AIA_NUM_TOPICS:
            break;
    }
    AiaLogError( "Unknown topic %d.", topic );
    AiaAssert( false );
    return true;
}

/**
 * @param topic The topic to query for whether it is outbound or not.
 * @return @c true if the topic is outbound and @c false otherwise.
 */
inline bool AiaTopic_IsOutbound( AiaTopic_t topic )
{
    switch( topic )
    {
        case AIA_TOPIC_CONNECTION_FROM_CLIENT:
        case AIA_TOPIC_CAPABILITIES_PUBLISH:
        case AIA_TOPIC_EVENT:
        case AIA_TOPIC_MICROPHONE:
            return true;
        case AIA_TOPIC_CONNECTION_FROM_SERVICE:
        case AIA_TOPIC_CAPABILITIES_ACKNOWLEDGE:
        case AIA_TOPIC_DIRECTIVE:
        case AIA_TOPIC_SPEAKER:
            return false;
        case AIA_NUM_TOPICS:
            break;
    }
    AiaLogError( "Unknown topic %d.", topic );
    AiaAssert( false );
    return false;
}

/**
 * @param topic A topic to get the length of the string representation of.
 * @return The length of the string representation of @c topic.
 */
inline size_t AiaTopic_GetLength( AiaTopic_t topic )
{
    switch( topic )
    {
        case AIA_TOPIC_CONNECTION_FROM_CLIENT:
            return sizeof( AIA_TOPIC_CONNECTION_FROM_CLIENT_STRING ) - 1;
        case AIA_TOPIC_CONNECTION_FROM_SERVICE:
            return sizeof( AIA_TOPIC_CONNECTION_FROM_SERVICE_STRING ) - 1;
        case AIA_TOPIC_CAPABILITIES_PUBLISH:
            return sizeof( AIA_TOPIC_CAPABILITIES_PUBLISH_STRING ) - 1;
        case AIA_TOPIC_CAPABILITIES_ACKNOWLEDGE:
            return sizeof( AIA_TOPIC_CAPABILITIES_ACKNOWLEDGE_STRING ) - 1;
        case AIA_TOPIC_DIRECTIVE:
            return sizeof( AIA_TOPIC_DIRECTIVE_STRING ) - 1;
        case AIA_TOPIC_EVENT:
            return sizeof( AIA_TOPIC_EVENT_STRING ) - 1;
        case AIA_TOPIC_MICROPHONE:
            return sizeof( AIA_TOPIC_MICROPHONE_STRING ) - 1;
        case AIA_TOPIC_SPEAKER:
            return sizeof( AIA_TOPIC_SPEAKER_STRING ) - 1;
        case AIA_NUM_TOPICS:
            break;
    }
    AiaLogError( "Unknown topic %d.", topic );
    AiaAssert( false );
    return 0;
}

/**
 * @param topicString A string to convert to an @c AiaTopic_t.
 * @param topicStringLength The length of @c topicString, or 0 if @c topicString
 *     is null-terminated.
 * @param[out] topic A topic pointer to return the @c AiaTopic_t value for
 *     @c topicString.
 * @return @c true if topicString was converted successfully, else @c false.
 */
inline bool AiaTopic_FromString( const char* topicString,
                                 size_t topicStringLength, AiaTopic_t* topic )
{
    static const AiaTopic_t topics[ AIA_NUM_TOPICS ] = {
        AIA_TOPIC_CONNECTION_FROM_CLIENT,
        AIA_TOPIC_CONNECTION_FROM_SERVICE,
        AIA_TOPIC_CAPABILITIES_PUBLISH,
        AIA_TOPIC_CAPABILITIES_ACKNOWLEDGE,
        AIA_TOPIC_DIRECTIVE,
        AIA_TOPIC_EVENT,
        AIA_TOPIC_MICROPHONE,
        AIA_TOPIC_SPEAKER
    };
    if( !topicString )
    {
        AiaLogError( "Null topicString." );
        return false;
    }
    if( !topic )
    {
        AiaLogError( "Null topic." );
        return false;
    }
    if( !topicStringLength )
    {
        topicStringLength = strlen( topicString );
    }
    for( size_t i = 0; i < AiaArrayLength( topics ); ++i )
    {
        if( AiaTopic_GetLength( topics[ i ] ) == topicStringLength &&
            strncmp( topicString, AiaTopic_ToString( topics[ i ] ),
                     topicStringLength ) == 0 )
        {
            *topic = topics[ i ];
            return true;
        }
    }
    AiaLogError( "Unknown topicString \"%.*s\".", topicStringLength,
                 topicString );
    return false;
}

/**
 * Persists the topic root.
 *
 * @param topicRoot The buffer to persist.
 * @param size Size of @c topicRoot.
 * @return @c true on success or @c false otherwise.
 */
bool AiaStoreTopicRoot( const uint8_t* topicRoot, size_t size );

/**
 * Loads topic root out of persistent storage.
 *
 * @param[out] topicRoot The buffer to load into.
 * @param size Size of the @c topicRoot buffer.
 * @return @c true on success or @c false othwerwise.
 */
bool AiaLoadTopicRoot( uint8_t* topicRoot, size_t size );

/**
 * Get size of the topic root stored.
 *
 * @return The size of the topic root in bytes. @c 0 will be returned on
 * failures.
 */
size_t AiaGetTopicRootSize();

/**
 * Gets the device topic root. The device topic root is not null-terminated.
 * If @c NULL is passed for @c deviceTopicRootBuffer, this function will
 * calculate the length of the device topic root. If the provided @c
 * deviceTopicRootBuffer is too small to hold the device topic root then @c 0
 * will be returned.
 *
 * @param[out] deviceTopicRootBuffer A user-provided buffer large enough to hold
 * the device topic root.
 * @param deviceTopicRootBufferSize The size (in bytes) of @c
 * deviceTopicRootBuffer.
 * @return The size of the device topic root string on success, @c 0 on failure.
 */
size_t AiaGetDeviceTopicRootString( char* deviceTopicRootBuffer,
                                    size_t deviceTopicRootBufferSize );

#endif /* ifndef AIA_TOPIC_H_ */
