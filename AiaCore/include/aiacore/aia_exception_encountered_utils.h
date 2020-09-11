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
 * @file aia_exception_encountered_utils.h
 * @brief Declares ExceptionEncountered event generation utility functions.
 */

#ifndef AIA_EXCEPTION_ENCOUNTERED_UTILS_H_
#define AIA_EXCEPTION_ENCOUNTERED_UTILS_H_

/* The config header is always included first. */
#include <aia_config.h>

#include "aia_json_message.h"
#include "aia_message_constants.h"
#include "aia_topic.h"

/* TODO: ADSER-1578 Add support for adding the optional "description" field. */
/**
 * Generates a @c MALFORMED_MESSAGE @c ExceptionEncountered event which may be
used for publishing.
 *
 * @param sequenceNumber The sequence number of the message.
 * @param index The index of the message.
 * @param topic The topic that the exception occurred on.
 * @return The generated @c AiaJsonMessage_t or @c NULL on failures. Callers are
required to free the returned message using @c AiaJsonMessage_Destroy().
 */
AiaJsonMessage_t* generateMalformedMessageExceptionEncounteredEvent(
    AiaSequenceNumber_t sequenceNumber, size_t index, AiaTopic_t topic );

/* TODO: ADSER-1578 Add support for adding the optional "description" field. */
/**
 * Generates an @c INTERNAL_ERROR @c ExceptionEncountered event which may be
used for publishing.
 *
 * @return The generated @c AiaJsonMessage_t or @c NULL on failures. Callers are
required to free the returned message using @c AiaJsonMessage_Destroy().
 */
AiaJsonMessage_t* generateInternalErrorExceptionEncounteredEvent();

#endif /* ifndef AIA_EXCEPTION_ENCOUNTERED_UTILS_H_ */
