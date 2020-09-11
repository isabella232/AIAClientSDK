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
 * @file aia_registration_failure_code.h
 * @brief Registration failure codes.
 */

#ifndef AIA_REGISTRATION_FAILURE_CODE_H_
#define AIA_REGISTRATION_FAILURE_CODE_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <aiacore/aia_utils.h>

/**
 * Codes that are that can be passed to the
 * @c AiaConnectionManagerOnConnectionRejectionCallback_t
 * @see
 * https://developer.amazon.com/en-US/docs/alexa/alexa-voice-service/avs-for-aws-iot-registration.html#failure-response-http-4xx-and-5xx
 */
typedef enum AiaRegistrationFailureCode
{
    AIA_REGISTRATION_FAILURE_INVALID_REQUEST,
    AIA_REGISTRATION_FAILURE_MISSING_PARAM,
    AIA_REGISTRATION_FAILURE_INVALID_ENCRYPTION_ALGORITHM,
    AIA_REGISTRATION_FAILURE_INVALID_ENCRYPTION_DATA,
    AIA_REGISTRATION_FAILURE_INVALID_AUTHENTICATION_CREDENTIALS,
    AIA_REGISTRATION_FAILURE_INVALID_AWS_ACCOUNT,
    AIA_REGISTRATION_FAILURE_INVALID_IOT_ENDPOINT,
    AIA_REGISTRATION_FAILURE_INTERNAL_SERVER_ERROR,
    AIA_REGISTRATION_FAILURE_RESPONSE_ERROR,
    AIA_REGISTRATION_FAILURE_SEND_FAILED
} AiaRegistrationFailureCode_t;

/**
 * Static strings for the @c AiaRegistrationFailureCode_t values.
 */
/** @{ */

#define AIA_REGISTRATION_FAILURE_INVALID_REQUEST_STRING "INVALID_REQUEST"
#define AIA_REGISTRATION_FAILURE_MISSING_PARAM_STRING "MISSING_PARAM"
#define AIA_REGISTRATION_FAILURE_INVALID_ENCRYPTION_ALGORITHM_STRING \
    "INVALID_ENCRYPTION_ALGORITHM"
#define AIA_REGISTRATION_FAILURE_INVALID_ENCRYPTION_DATA_STRING \
    "INVALID_ENCRYPTION_DATA"
#define AIA_REGISTRATION_FAILURE_INVALID_AUTHENTICATION_CREDENTIALS_STRING \
    "INVALID_AUTHENTICATION_CREDENTIALS"
#define AIA_REGISTRATION_FAILURE_INVALID_AWS_ACCOUNT_STRING \
    "INVALID_AWS_ACCOUNT"
#define AIA_REGISTRATION_FAILURE_INVALID_IOT_ENDPOINT_STRING \
    "INVALID_IOT_ENDPOINT"
#define AIA_REGISTRATION_FAILURE_INTERNAL_SERVER_ERROR_STRING \
    "INTERNAL_SERVER_ERROR"

/** @} */

/**
 * @param failureCode A failure code to get the string representation of.
 * @return The string representation of @c failureCode.
 */
static inline const char* AiaRegistrationFailureCode_ToString(
    AiaRegistrationFailureCode_t failureCode )
{
    switch( failureCode )
    {
        case AIA_REGISTRATION_FAILURE_INVALID_REQUEST:
            return AIA_REGISTRATION_FAILURE_INVALID_REQUEST_STRING;
        case AIA_REGISTRATION_FAILURE_MISSING_PARAM:
            return AIA_REGISTRATION_FAILURE_MISSING_PARAM_STRING;
        case AIA_REGISTRATION_FAILURE_INVALID_ENCRYPTION_ALGORITHM:
            return AIA_REGISTRATION_FAILURE_INVALID_ENCRYPTION_ALGORITHM_STRING;
        case AIA_REGISTRATION_FAILURE_INVALID_ENCRYPTION_DATA:
            return AIA_REGISTRATION_FAILURE_INVALID_ENCRYPTION_DATA_STRING;
        case AIA_REGISTRATION_FAILURE_INVALID_AUTHENTICATION_CREDENTIALS:
            return AIA_REGISTRATION_FAILURE_INVALID_AUTHENTICATION_CREDENTIALS_STRING;
        case AIA_REGISTRATION_FAILURE_INVALID_AWS_ACCOUNT:
            return AIA_REGISTRATION_FAILURE_INVALID_AWS_ACCOUNT_STRING;
        case AIA_REGISTRATION_FAILURE_INVALID_IOT_ENDPOINT:
            return AIA_REGISTRATION_FAILURE_INVALID_IOT_ENDPOINT_STRING;
        case AIA_REGISTRATION_FAILURE_INTERNAL_SERVER_ERROR:
            return AIA_REGISTRATION_FAILURE_INTERNAL_SERVER_ERROR_STRING;
        default:
            AiaLogError( "Unknown registration failure code %d.", failureCode );
            AiaAssert( false );
            return "";
    }
}

/**
 * @param failureCode A failure code to get the length of the string
 * representation of.
 * @return The length of the string representation of @c failureCode.
 */
static inline size_t AiaRegistrationFailureCode_GetLength(
    AiaRegistrationFailureCode_t failureCode )
{
    switch( failureCode )
    {
        case AIA_REGISTRATION_FAILURE_INVALID_REQUEST:
            return sizeof( AIA_REGISTRATION_FAILURE_INVALID_REQUEST_STRING ) -
                   1;
        case AIA_REGISTRATION_FAILURE_MISSING_PARAM:
            return sizeof( AIA_REGISTRATION_FAILURE_MISSING_PARAM_STRING ) - 1;
        case AIA_REGISTRATION_FAILURE_INVALID_ENCRYPTION_ALGORITHM:
            return sizeof(
                       AIA_REGISTRATION_FAILURE_INVALID_ENCRYPTION_ALGORITHM_STRING ) -
                   1;
        case AIA_REGISTRATION_FAILURE_INVALID_ENCRYPTION_DATA:
            return sizeof(
                       AIA_REGISTRATION_FAILURE_INVALID_ENCRYPTION_DATA_STRING ) -
                   1;
        case AIA_REGISTRATION_FAILURE_INVALID_AUTHENTICATION_CREDENTIALS:
            return sizeof(
                       AIA_REGISTRATION_FAILURE_INVALID_AUTHENTICATION_CREDENTIALS_STRING ) -
                   1;
        case AIA_REGISTRATION_FAILURE_INVALID_AWS_ACCOUNT:
            return sizeof(
                       AIA_REGISTRATION_FAILURE_INVALID_AWS_ACCOUNT_STRING ) -
                   1;
        case AIA_REGISTRATION_FAILURE_INVALID_IOT_ENDPOINT:
            return sizeof(
                       AIA_REGISTRATION_FAILURE_INVALID_IOT_ENDPOINT_STRING ) -
                   1;
        case AIA_REGISTRATION_FAILURE_INTERNAL_SERVER_ERROR:
            return sizeof(
                       AIA_REGISTRATION_FAILURE_INTERNAL_SERVER_ERROR_STRING ) -
                   1;
        default:
            AiaLogError( "Unknown registration failure code %d.", failureCode );
            AiaAssert( false );
            return 0;
    }
}

/**
 * @param failureCodeString A string to convert to an @c
 * AiaRegistrationFailureCode_t.
 * @param failureCodeStringLength The length of @c failureCodeString, or 0
 * if @c failureCodeString is null-terminated.
 * @param[out] failureCode A failure code pointer to return the @c
 * AiaRegistrationFailureCode_t value for @c failureCodeString.
 * @return @c true if failureCodeString was converted successfully, else @c
 * false.
 */
static inline bool AiaRegistrationFailureCode_FromString(
    const char* failureCodeString, size_t failureCodeStringLength,
    AiaRegistrationFailureCode_t* failureCode )
{
    static const AiaRegistrationFailureCode_t failureCodes[] = {
        AIA_REGISTRATION_FAILURE_INVALID_REQUEST,
        AIA_REGISTRATION_FAILURE_MISSING_PARAM,
        AIA_REGISTRATION_FAILURE_INVALID_ENCRYPTION_ALGORITHM,
        AIA_REGISTRATION_FAILURE_INVALID_ENCRYPTION_DATA,
        AIA_REGISTRATION_FAILURE_INVALID_AUTHENTICATION_CREDENTIALS,
        AIA_REGISTRATION_FAILURE_INVALID_AWS_ACCOUNT,
        AIA_REGISTRATION_FAILURE_INVALID_IOT_ENDPOINT,
        AIA_REGISTRATION_FAILURE_INTERNAL_SERVER_ERROR,
        AIA_REGISTRATION_FAILURE_RESPONSE_ERROR,
        AIA_REGISTRATION_FAILURE_SEND_FAILED
    };

    if( !failureCodeString )
    {
        AiaLogError( "Null failureCodeString." );
        return false;
    }
    if( !failureCode )
    {
        AiaLogError( "Null failureCode." );
        return false;
    }
    if( !failureCodeStringLength )
    {
        failureCodeStringLength = strlen( failureCodeString );
    }
    for( size_t i = 0; i < AiaArrayLength( failureCodes ); ++i )
    {
        if( AiaRegistrationFailureCode_GetLength( failureCodes[ i ] ) ==
                failureCodeStringLength &&
            strncmp( failureCodeString,
                     AiaRegistrationFailureCode_ToString( failureCodes[ i ] ),
                     failureCodeStringLength ) == 0 )
        {
            *failureCode = failureCodes[ i ];
            return true;
        }
    }
    AiaLogError( "Unknown failureCodeString \"%.*s\".", failureCodeStringLength,
                 failureCodeString );
    return false;
}

#endif /* ifndef AIA_REGISTRATION_FAILURE_CODE_H_ */
