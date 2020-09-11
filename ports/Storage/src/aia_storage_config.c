/*
 * Copyright 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
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
 * @file aia_storage_config.c
 * @brief Implements platform-specific Storage functions which are not inlined
 * in
 * @c aia_storage_config.h.
 */

#include <storage/aia_storage_config.h>

#include <aia_config.h>

#include <aiaalertmanager/aia_alert_constants.h>
#include <aiacore/aia_volume_constants.h>

#include <errno.h>
#include <stdio.h>

const char* g_aiaClientId;
const char* g_aiaAwsAccountId;
const char* g_aiaStorageFolder;

#ifdef AIA_LOAD_VOLUME

uint8_t AiaLoadVolume()
{
    /* TODO: ADSER-1741 Provide an actual reference implementation of persisting
     * device volume. */
    return AIA_DEFAULT_VOLUME;
}

#endif

#define AIA_SHARED_SECRET_STORAGE_KEY "AiaSharedSecretStorageKey"

bool AiaStoreSecret( const uint8_t* sharedSecret, size_t size )
{
    return AiaStoreBlob( AIA_SHARED_SECRET_STORAGE_KEY, sharedSecret, size );
}

bool AiaLoadSecret( uint8_t* sharedSecret, size_t size )
{
    return AiaLoadBlob( AIA_SHARED_SECRET_STORAGE_KEY, sharedSecret, size );
}

/** If using the provided sample storage implementation, this is the filename
 * format of persistent storage files that the SDK will read/write from/to. */
static const char* PERSISTENT_STORAGE_FILE_PATH_FORMAT = "%s/%s_%s_%s.dat";

bool AiaStoreBlob( const char* key, const uint8_t* blob, size_t size )
{
    int numCharsRequired =
        snprintf( NULL, 0, PERSISTENT_STORAGE_FILE_PATH_FORMAT,
                  g_aiaStorageFolder, g_aiaAwsAccountId, g_aiaClientId, key );
    if( numCharsRequired < 0 )
    {
        AiaLogError( "snprintf failed, ret=%d", numCharsRequired );
        return false;
    }
    char filePath[ numCharsRequired + 1 ];
    if( snprintf( filePath, numCharsRequired + 1,
                  PERSISTENT_STORAGE_FILE_PATH_FORMAT, g_aiaStorageFolder,
                  g_aiaAwsAccountId, g_aiaClientId, key ) < 0 )
    {
        AiaLogError( "snprintf failed" );
        return false;
    }
    FILE* file = fopen( filePath, "wb" );
    if( !file )
    {
        AiaLogError( "fopen failed: %s", strerror( errno ) );
        return false;
    }
    if( fwrite( blob, 1, size, file ) < size )
    {
        AiaLogError( "Failed to store full blob, key=%s", key );
        fclose( file );
        return false;
    }
    if( fclose( file ) != 0 )
    {
        AiaLogWarn( "fclose failed" );
    }
    return true;
}

bool AiaLoadBlob( const char* key, const uint8_t* blob, size_t size )
{
    if( !key )
    {
        AiaLogError( "Null key" );
        return false;
    }
    if( !blob )
    {
        AiaLogError( "Null blob" );
        return false;
    }

    int numCharsRequired =
        snprintf( NULL, 0, PERSISTENT_STORAGE_FILE_PATH_FORMAT,
                  g_aiaStorageFolder, g_aiaAwsAccountId, g_aiaClientId, key );

    if( numCharsRequired < 0 )
    {
        AiaLogError( "snprintf failed, ret=%d", numCharsRequired );
        return false;
    }
    char filePath[ numCharsRequired + 1 ];
    if( snprintf( filePath, numCharsRequired + 1,
                  PERSISTENT_STORAGE_FILE_PATH_FORMAT, g_aiaStorageFolder,
                  g_aiaAwsAccountId, g_aiaClientId, key ) < 0 )
    {
        AiaLogError( "snprintf failed" );
        return false;
    }
    FILE* file = fopen( filePath, "rb" );
    if( !file )
    {
        AiaLogError( "fopen failed: %s", strerror( errno ) );

        return false;
    }
    rewind( file );
    if( fread( (uint8_t*)blob, 1, size, file ) < size )
    {
        AiaLogError( "Failed to read full blob, key=%s, size=%zu", key, size );
        fclose( file );
        return false;
    }

    if( fclose( file ) != 0 )
    {
        AiaLogWarn( "fclose failed" );
    }

    return true;
}

bool AiaBlobExists( const char* key )
{
    if( !key )
    {
        AiaLogError( "Null key" );
        return false;
    }

    int numCharsRequired =
        snprintf( NULL, 0, PERSISTENT_STORAGE_FILE_PATH_FORMAT,
                  g_aiaStorageFolder, g_aiaAwsAccountId, g_aiaClientId, key );
    if( numCharsRequired < 0 )
    {
        AiaLogError( "snprintf failed, ret=%d", numCharsRequired );
        return false;
    }
    char filePath[ numCharsRequired + 1 ];
    if( snprintf( filePath, numCharsRequired + 1,
                  PERSISTENT_STORAGE_FILE_PATH_FORMAT, g_aiaStorageFolder,
                  g_aiaAwsAccountId, g_aiaClientId, key ) < 0 )
    {
        AiaLogError( "snprintf failed" );
        return false;
    }

    FILE* file = fopen( filePath, "r" );
    if( file )
    {
        fclose( file );
        return true;
    }
    else
    {
        return false;
    }
}

size_t AiaGetBlobSize( const char* key )
{
    if( !key )
    {
        AiaLogError( "Null key" );
        return 0;
    }

    int numCharsRequired =
        snprintf( NULL, 0, PERSISTENT_STORAGE_FILE_PATH_FORMAT,
                  g_aiaStorageFolder, g_aiaAwsAccountId, g_aiaClientId, key );
    if( numCharsRequired < 0 )
    {
        AiaLogError( "snprintf failed, ret=%d", numCharsRequired );
        return 0;
    }
    char filePath[ numCharsRequired + 1 ];
    if( snprintf( filePath, numCharsRequired + 1,
                  PERSISTENT_STORAGE_FILE_PATH_FORMAT, g_aiaStorageFolder,
                  g_aiaAwsAccountId, g_aiaClientId, key ) < 0 )
    {
        AiaLogError( "snprintf failed" );
        return 0;
    }
    FILE* file = fopen( filePath, "rb" );
    if( !file )
    {
        AiaLogError( "fopen failed: %s", strerror( errno ) );

        return 0;
    }
    if( fseek( file, 0, SEEK_END ) < 0 )
    {
        AiaLogError( "fseek failed" );
        fclose( file );
        return 0;
    }
    size_t fileSize = (size_t)ftell( file );
    if( fclose( file ) != 0 )
    {
        AiaLogWarn( "fclose failed" );
    }

    return fileSize;
}

#define AIA_ALL_ALERTS_STORAGE_KEY_V0 "AiaAllAlertsStorageKey"

bool AiaStoreAlert( const char* alertToken, size_t alertTokenLen,
                    AiaTimepointSeconds_t scheduledTime,
                    AiaDurationMs_t duration, uint8_t alertType )
{
    if( !alertToken )
    {
        AiaLogError( "Null alertToken" );
        return false;
    }
    if( alertTokenLen != AIA_ALERT_TOKEN_CHARS )
    {
        AiaLogError( "Invalid alert token length" );
        return false;
    }

    size_t allAlertsBytes = AiaGetAlertsSize();
    size_t alertsBytesWithNewAlert =
        allAlertsBytes + AIA_SIZE_OF_ALERT_IN_BYTES;
    size_t startingOffset = allAlertsBytes;
    size_t bytePosition;
    bool updatingExistingAlert = false;

    /**
     * Load all alerts from persistent storage. Allocated additional space
     * for a new alert though in case the token we are trying to insert does
     * not exist in persistent storage yet.
     */
    uint8_t* allAlertsBuffer = AiaCalloc( 1, alertsBytesWithNewAlert );
    if( !allAlertsBuffer )
    {
        AiaLogError( "AiaCalloc failed, bytes=%zu.", alertsBytesWithNewAlert );
        return false;
    }

    if( !AiaLoadAlerts( allAlertsBuffer, allAlertsBytes ) )
    {
        AiaLogError( "AiaLoadBlob failed" );
        AiaFree( allAlertsBuffer );
        return false;
    }

    /** Go through the tokens to find the first empty or matching one */
    for( bytePosition = 0; bytePosition < alertsBytesWithNewAlert;
         bytePosition += AIA_SIZE_OF_ALERT_IN_BYTES )
    {
        startingOffset = bytePosition;
        if( '\0' == allAlertsBuffer[ bytePosition ] )
        {
            /** Found an empty token */
            break;
        }
        else
        {
            /** Check if this token matches with what we are trying to insert */
            if( !strncmp( (const char*)allAlertsBuffer + bytePosition,
                          alertToken, alertTokenLen ) )
            {
                updatingExistingAlert = true;
                break;
            }
        }
    }

    /* Check if we have reached the alerts storage limit */
    if( startingOffset == alertsBytesWithNewAlert )
    {
        AiaLogError(
            "AiaStoreAlert failed: Maximum number of local alerts to store "
            "reached." );
        AiaFree( allAlertsBuffer );
        return false;
    }

    /** Write the new alert token */
    uint8_t* newAlertOffset = allAlertsBuffer + startingOffset;
    memcpy( newAlertOffset, alertToken, alertTokenLen );

    /** Write the other fields: scheduledTime, duration, alertType */
    bytePosition = alertTokenLen;
    for( size_t i = 0; i < sizeof( AiaTimepointSeconds_t );
         ++i, bytePosition++ )
    {
        newAlertOffset[ bytePosition ] = ( scheduledTime >> ( i * 8 ) );
    }
    for( size_t i = 0; i < sizeof( AiaDurationMs_t ); ++i, bytePosition++ )
    {
        newAlertOffset[ bytePosition ] = ( duration >> ( i * 8 ) );
    }
    for( size_t i = 0; i < sizeof( uint8_t ); ++i, bytePosition++ )
    {
        newAlertOffset[ bytePosition ] = ( alertType >> ( i * 8 ) );
    }

    /** Store the new blob in persistent storage */
    size_t storeSize =
        ( updatingExistingAlert ? allAlertsBytes : alertsBytesWithNewAlert );
    /** Store the new blob in persistent storage */
    if( !AiaStoreBlob( AIA_ALL_ALERTS_STORAGE_KEY_V0, allAlertsBuffer,
                       storeSize ) )
    {
        AiaLogError( "AiaStoreBlob failed" );
        AiaFree( allAlertsBuffer );
        return false;
    }

    AiaFree( allAlertsBuffer );
    return true;
}

bool AiaDeleteAlert( const char* alertToken, size_t alertTokenLen )
{
    if( !alertToken )
    {
        AiaLogError( "Null alertToken" );
        return false;
    }
    if( alertTokenLen != AIA_ALERT_TOKEN_CHARS )
    {
        AiaLogError( "Invalid alert token length" );
        return false;
    }

    size_t allAlertsBytes = AiaGetAlertsSize();
    size_t bytePosition;
    bool deletingExistingAlert = false;

    /**
     * Load all alerts from persistent storage.
     */
    uint8_t* allAlertsBuffer = AiaCalloc( 1, allAlertsBytes );
    if( !allAlertsBuffer )
    {
        AiaLogError( "AiaCalloc failed, bytes=%zu.", allAlertsBytes );
        return false;
    }

    if( !AiaLoadAlerts( allAlertsBuffer, allAlertsBytes ) )
    {
        AiaLogError( "AiaLoadBlob failed" );
        AiaFree( allAlertsBuffer );
        return false;
    }

    /** Go through the tokens to find the first empty or matching one */
    for( bytePosition = 0; bytePosition < allAlertsBytes;
         bytePosition += AIA_SIZE_OF_ALERT_IN_BYTES )
    {
        if( '\0' == allAlertsBuffer[ bytePosition ] )
        {
            /** Found an empty token */
            break;
        }
        else
        {
            /** Check if this token matches with what we are trying to delete */
            if( !strncmp( (const char*)allAlertsBuffer + bytePosition,
                          alertToken, alertTokenLen ) )
            {
                uint8_t* moveDst = allAlertsBuffer + bytePosition;
                uint8_t* moveSrc =
                    allAlertsBuffer + bytePosition + AIA_SIZE_OF_ALERT_IN_BYTES;
                size_t moveBytes =
                    allAlertsBytes -
                    ( bytePosition + AIA_SIZE_OF_ALERT_IN_BYTES );
                memmove( moveDst, moveSrc, moveBytes );
                deletingExistingAlert = true;
                break;
            }
        }
    }

    /** Store the new blob in persistent storage */
    size_t storeSize =
        ( deletingExistingAlert ? allAlertsBytes - AIA_SIZE_OF_ALERT_IN_BYTES
                                : allAlertsBytes );
    /** Store the new blob */
    if( !AiaStoreBlob( AIA_ALL_ALERTS_STORAGE_KEY_V0, allAlertsBuffer,
                       storeSize ) )
    {
        AiaLogError( "AiaStoreBlob failed" );
        AiaFree( allAlertsBuffer );
        return false;
    }

    AiaFree( allAlertsBuffer );
    return true;
}

bool AiaLoadAlert( char* alertToken, size_t alertTokenLen,
                   AiaTimepointSeconds_t* scheduledTime,
                   AiaDurationMs_t* duration, uint8_t* alertType,
                   const uint8_t* allAlertsBuffer )
{
    if( !alertToken )
    {
        AiaLogError( "Null alertToken" );
        return false;
    }
    if( !scheduledTime )
    {
        AiaLogError( "Null scheduledTime" );
        return false;
    }
    if( !duration )
    {
        AiaLogError( "Null duration" );
        return false;
    }
    if( !alertType )
    {
        AiaLogError( "Null alertType" );
        return false;
    }
    if( !allAlertsBuffer )
    {
        AiaLogError( "Null allAlertsBuffer" );
        return false;
    }
    if( alertTokenLen != AIA_ALERT_TOKEN_CHARS )
    {
        AiaLogError( "Invalid alert token length" );
        return false;
    }

    size_t bytePosition = 0;
    *scheduledTime = 0;
    *duration = 0;
    *alertType = 0;

    memcpy( alertToken, allAlertsBuffer, alertTokenLen );
    bytePosition += alertTokenLen;

    for( size_t i = 0; i < sizeof( AiaTimepointSeconds_t );
         ++i, ++bytePosition )
    {
        *scheduledTime |= (unsigned)allAlertsBuffer[ bytePosition ]
                          << ( i * 8 );
    }

    for( size_t i = 0; i < sizeof( AiaDurationMs_t ); ++i, ++bytePosition )
    {
        *duration |= (unsigned)allAlertsBuffer[ bytePosition ] << ( i * 8 );
    }

    for( size_t i = 0; i < sizeof( uint8_t ); ++i, ++bytePosition )
    {
        *alertType |= (unsigned)allAlertsBuffer[ bytePosition ] << ( i * 8 );
    }

    return true;
}

bool AiaLoadAlerts( uint8_t* allAlerts, size_t size )
{
    if( !AiaAlertsBlobExists() )
    {
        if( size != 0 )
        {
            AiaLogError( "Alerts blob with size %zu does not exist", size );
            return false;
        }
        else
        {
            return true;
        }
    }
    else
    {
        return AiaLoadBlob( AIA_ALL_ALERTS_STORAGE_KEY_V0, allAlerts, size );
    }
}

size_t AiaGetAlertsSize()
{
    return AiaGetBlobSize( AIA_ALL_ALERTS_STORAGE_KEY_V0 );
}

bool AiaAlertsBlobExists()
{
    return AiaBlobExists( AIA_ALL_ALERTS_STORAGE_KEY_V0 );
}
