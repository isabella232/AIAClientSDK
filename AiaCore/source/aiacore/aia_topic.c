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
 * @file aia_topic.c
 * @brief Implements topic related functions in aia_topic.h.
 */

/* The config header is always included first. */
#include <aia_config.h>

#include <aiacore/aia_topic.h>

#define AIA_TOPIC_ROOT_KEY "AiaTopicRootKey"
#define AIA_DEVICE_TOPIC_ROOT_SUFFIX "/ais/" AIA_API_VERSION "/"

bool AiaStoreTopicRoot( const uint8_t* topicRoot, size_t size )
{
    return AiaStoreBlob( AIA_TOPIC_ROOT_KEY, topicRoot, size );
}

bool AiaLoadTopicRoot( uint8_t* topicRoot, size_t size )
{
    return AiaLoadBlob( AIA_TOPIC_ROOT_KEY, topicRoot, size );
}

size_t AiaGetTopicRootSize()
{
    return AiaGetBlobSize( AIA_TOPIC_ROOT_KEY );
}

size_t AiaGetDeviceTopicRootString( char* deviceTopicRootBuffer,
                                    size_t deviceTopicRootBufferSize )
{
    size_t iotClientIdLen;
    if( !AiaGetIotClientId( NULL, &iotClientIdLen ) )
    {
        AiaLogError(
            "AiaGetIotClientId Failed. Failed to get IoT Client Id length." );
        return 0;
    }
    size_t topicRootSize = AiaGetTopicRootSize();
    if( !topicRootSize )
    {
        AiaLogError( "AiaGetTopicRootSize failed" );
        return 0;
    }
    /* The '\0' counted in iotClientIdLen accounts for the trailing '/' added to
     * the device topic root string returned. */
    size_t deviceTopicRootSize = topicRootSize +
                                 sizeof( AIA_DEVICE_TOPIC_ROOT_SUFFIX ) - 1 +
                                 iotClientIdLen;

    if( !deviceTopicRootBuffer )
    {
        return deviceTopicRootSize;
    }
    if( deviceTopicRootBufferSize < deviceTopicRootSize )
    {
        AiaLogError(
            "deviceTopicRootBuffer too small to hold device topic root." );
        return 0;
    }

    if( !AiaLoadTopicRoot( (uint8_t*)deviceTopicRootBuffer, topicRootSize ) )
    {
        AiaLogError( "AiaLoadTopicRoot failed" );
        return 0;
    }
    memcpy( deviceTopicRootBuffer + topicRootSize, AIA_DEVICE_TOPIC_ROOT_SUFFIX,
            sizeof( AIA_DEVICE_TOPIC_ROOT_SUFFIX ) - 1 );
    if( !AiaGetIotClientId( deviceTopicRootBuffer + topicRootSize +
                                sizeof( AIA_DEVICE_TOPIC_ROOT_SUFFIX ) - 1,
                            &iotClientIdLen ) )
    {
        AiaLogError( "AiaLoadTopicRoot failed" );
        return 0;
    }

    memcpy( deviceTopicRootBuffer + deviceTopicRootSize - 1, "/", 1 );

    return deviceTopicRootSize;
}
