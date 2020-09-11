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
 * @file aia_exception_code.h
 * @brief Exception codes.
 */

#ifndef AIA_EXCEPTION_CODE_H_
#define AIA_EXCEPTION_CODE_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <aiacore/aia_utils.h>

/**
 * Codes that can be passed to the
 * @c AiaExceptionManagerOnExceptionCallback_t
 * @see
 * https://developer.amazon.com/en-US/docs/alexa/alexa-voice-service/avs-for-aws-iot-system.html#exception
 */
typedef enum AiaExceptionCode
{
    AIA_EXCEPTION_INVALID_REQUEST,
    AIA_EXCEPTION_UNSUPPORTED_API,
    AIA_EXCEPTION_THROTTLING,
    AIA_EXCEPTION_INTERNAL_SERVICE,
    AIA_EXCEPTION_AIS_UNAVAILABLE
} AiaExceptionCode_t;

/**
 * Static strings for the @c AiaExceptionCode_t values.
 */
/** @{ */

#define AIA_EXCEPTION_INVALID_REQUEST_STRING "INVALID_REQUEST"
#define AIA_EXCEPTION_UNSUPPORTED_API_STRING "UNSUPPORTED_API"
#define AIA_EXCEPTION_THROTTLING_STRING "THROTTLING"
#define AIA_EXCEPTION_INTERNAL_SERVICE_STRING "INTERNAL_SERVICE"
#define AIA_EXCEPTION_AIS_UNAVAILABLE_STRING "AIS_UNAVAILABLE"

/** @} */

/**
 * @param exceptionCode An exception code to get the string representation of.
 * @return The string representation of @c exceptionCode.
 */
static inline const char* AiaExceptionCode_ToString(
    AiaExceptionCode_t exceptionCode )
{
    switch( exceptionCode )
    {
        case AIA_EXCEPTION_INVALID_REQUEST:
            return AIA_EXCEPTION_INVALID_REQUEST_STRING;
        case AIA_EXCEPTION_UNSUPPORTED_API:
            return AIA_EXCEPTION_UNSUPPORTED_API_STRING;
        case AIA_EXCEPTION_THROTTLING:
            return AIA_EXCEPTION_THROTTLING_STRING;
        case AIA_EXCEPTION_INTERNAL_SERVICE:
            return AIA_EXCEPTION_INTERNAL_SERVICE_STRING;
        case AIA_EXCEPTION_AIS_UNAVAILABLE:
            return AIA_EXCEPTION_AIS_UNAVAILABLE_STRING;
        default:
            AiaLogError( "Unknown exception code %d.", exceptionCode );
            AiaAssert( false );
            return "";
    }
}

/**
 * @param exceptionCode An exception code to get the length of the string
 * representation of.
 * @return The length of the string representation of @c exceptionCode.
 */
static inline size_t AiaExceptionCode_GetLength(
    AiaExceptionCode_t exceptionCode )
{
    switch( exceptionCode )
    {
        case AIA_EXCEPTION_INVALID_REQUEST:
            return sizeof( AIA_EXCEPTION_INVALID_REQUEST_STRING ) - 1;
        case AIA_EXCEPTION_UNSUPPORTED_API:
            return sizeof( AIA_EXCEPTION_UNSUPPORTED_API_STRING ) - 1;
        case AIA_EXCEPTION_THROTTLING:
            return sizeof( AIA_EXCEPTION_THROTTLING_STRING ) - 1;
        case AIA_EXCEPTION_INTERNAL_SERVICE:
            return sizeof( AIA_EXCEPTION_INTERNAL_SERVICE_STRING ) - 1;
        case AIA_EXCEPTION_AIS_UNAVAILABLE:
            return sizeof( AIA_EXCEPTION_AIS_UNAVAILABLE_STRING ) - 1;
        default:
            AiaLogError( "Unknown exception code %d.", exceptionCode );
            AiaAssert( false );
            return 0;
    }
}

/**
 * @param exceptionCodeString A string to convert to an @c
 * AiaExceptionCode_t.
 * @param exceptionCodeStringLength The length of @c exceptionCodeString, or 0
 * if @c exceptionCodeString is null-terminated.
 * @param[out] exceptionCode A exception code pointer to return the @c
 * AiaExceptionCode_t value for @c exceptionCodeString.
 * @return @c true if exceptionCodeString was converted successfully, else @c
 * false.
 */
static inline bool AiaExceptionCode_FromString(
    const char* exceptionCodeString, size_t exceptionCodeStringLength,
    AiaExceptionCode_t* exceptionCode )
{
    static const AiaExceptionCode_t exceptionCodes[] = {
        AIA_EXCEPTION_INVALID_REQUEST, AIA_EXCEPTION_UNSUPPORTED_API,
        AIA_EXCEPTION_THROTTLING, AIA_EXCEPTION_INTERNAL_SERVICE,
        AIA_EXCEPTION_AIS_UNAVAILABLE
    };

    if( !exceptionCodeString )
    {
        AiaLogError( "Null exceptionCodeString." );
        return false;
    }
    if( !exceptionCode )
    {
        AiaLogError( "Null exceptionCode." );
        return false;
    }
    if( !exceptionCodeStringLength )
    {
        exceptionCodeStringLength = strlen( exceptionCodeString );
    }
    for( size_t i = 0; i < AiaArrayLength( exceptionCodes ); ++i )
    {
        if( AiaExceptionCode_GetLength( exceptionCodes[ i ] ) ==
                exceptionCodeStringLength &&
            strncmp( exceptionCodeString,
                     AiaExceptionCode_ToString( exceptionCodes[ i ] ),
                     exceptionCodeStringLength ) == 0 )
        {
            *exceptionCode = exceptionCodes[ i ];
            return true;
        }
    }
    AiaLogError( "Unknown exceptionCodeString \"%.*s\".",
                 exceptionCodeStringLength, exceptionCodeString );
    return false;
}

#endif /* ifndef AIA_EXCEPTION_CODE_H_ */
