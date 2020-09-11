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
 * @file aia_exception_encountered_utils.c
 * @brief Implements utility functions in aia_exception_encountered_utils.h.
 */

/* The config header is always included first. */
#include <aia_config.h>

#include <aiacore/aia_exception_encountered_utils.h>

#include <aiacore/aia_events.h>
#include <aiacore/aia_json_constants.h>
#include <aiacore/aia_json_message.h>
#include <aiacore/aia_message_constants.h>

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

AiaJsonMessage_t* generateMalformedMessageExceptionEncounteredEvent(
    AiaSequenceNumber_t sequenceNumber, size_t index, AiaTopic_t topic )
{
    static const char* formatPayload =
        /* clang-format off */
        "{"
            "\"" AIA_EXCEPTION_ENCOUNTERED_ERROR_KEY "\": {"
                "\"" AIA_EXCEPTION_ENCOUNTERED_ERROR_CODE_KEY "\":\"" AIA_EXCEPTION_ENCOUNTERED_MALFORMED_MESSAGE_CODE "\""
            "},"
            "\"" AIA_EXCEPTION_ENCOUNTERED_MESSAGE_KEY "\": {"
                "\"" AIA_EXCEPTION_ENCOUNTERED_MESSAGE_TOPIC_KEY "\":\"" "%s" "\","
                "\"" AIA_EXCEPTION_ENCOUNTERED_MESSAGE_SEQUENCE_NUMBER_KEY "\": %"PRIu32","
                "\"" AIA_EXCEPTION_ENCOUNTERED_MESSAGE_INDEX_KEY "\": %zu"
            "}"
        "}";
    /* clang-format on */
    int numCharsRequired =
        snprintf( NULL, 0, formatPayload, AiaTopic_ToString( topic ),
                  sequenceNumber, index );
    if( numCharsRequired < 0 )
    {
        AiaLogError( "snprintf failed" );
        return NULL;
    }
    char fullPayloadBuffer[ numCharsRequired + 1 ];
    if( snprintf( fullPayloadBuffer, numCharsRequired + 1, formatPayload,
                  AiaTopic_ToString( topic ), sequenceNumber, index ) < 0 )
    {
        AiaLogError( "snprintf failed" );
        return NULL;
    }
    AiaJsonMessage_t* jsonMessage = AiaJsonMessage_Create(
        AIA_EVENTS_EXCEPTION_ENCOUNTERED, NULL, fullPayloadBuffer );
    return jsonMessage;
}

AiaJsonMessage_t* generateInternalErrorExceptionEncounteredEvent()
{
    char* payload =
        /* clang-format off */
        "{"
            "\"" AIA_EXCEPTION_ENCOUNTERED_ERROR_KEY "\": {"
                "\"" AIA_EXCEPTION_ENCOUNTERED_ERROR_CODE_KEY "\":\"" AIA_EXCEPTION_ENCOUNTERED_INTERNAL_ERROR_CODE "\""
            "}"
        "}";
    /* clang-format on */
    AiaJsonMessage_t* jsonMessage = AiaJsonMessage_Create(
        AIA_EVENTS_EXCEPTION_ENCOUNTERED, NULL, payload );
    return jsonMessage;
}
