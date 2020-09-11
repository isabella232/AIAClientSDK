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
 * @file aia_json_utils.h
 * @brief Declares JSON utility functions.
 */

#ifndef AIA_JSON_UTILS_H_
#define AIA_JSON_UTILS_H_

/* The config header is always included first. */
#include <aia_config.h>

#include "aia_json_constants.h"

/* Standard includes. */
#include <stdbool.h>

/**
 * Utility function which verifies that a string is quoted and returns the
 * unquoted string.
 *
 * @param[in,out] jsonString Pointer to the quoted string to examine.  If the
 * string is successfully unquoted, the @c *jsonString will be updated to point
 * to the first unquoted character in the string.
 * @param[in,out] jsonStringLength Pointer to the length of the string.  If the
 * string is successfully unquoted, *jsonStringLength will be updated to
 * indicate the length of the unquoted string.
 * @return @c true if the string was successfully unquoted, else @c false.
 */
bool AiaJsonUtils_UnquoteString( const char** jsonString,
                                 size_t* jsonStringLength );

/**
 * Utility function that extracts Aia long from a jsonValue found using @c
 * AiaFindJsonValue().
 *
 * @param jsonValue A @c const @c char pointer that points to the location the
 * value starts at.
 * @param jsonValueLength The length of @c jsonValue's value (not including a
 * terminating '\0').
 * @param [out] longValue The extracted long if it was able to be found.
 * @return @c true if a long could be successfully parsed, or @c false
 * otherwise.
 */
bool AiaExtractLongFromJsonValue( const char* jsonValue, size_t jsonValueLength,
                                  AiaJsonLongType* longValue );

/**
 * Utility function that extracts an individual element from a JSON array.
 *
 * @param jsonArray A JSON array string (including the '[' and ']').
 * @param jsonArrayLength The length of @c jsonArray (not including the
 *     terminating `\0`).
 * @param index Which (zero-based) element of the array to get.
 * @param [out] jsonValue Pointer to the start of the array element (may not be
 *     null-terminated).
 * @param [out] jsonValueLength Length of the array element string.
 * @return @c true if the array element was returned in @c jsonValue and
 *     @c jsonValueLength, else @c false.
 */
bool AiaJsonUtils_GetArrayElement( const char* jsonArray,
                                   size_t jsonArrayLength, size_t index,
                                   const char** jsonValue,
                                   size_t* jsonValueLength );

/**
 * Utility function to extract an AIA long from a JSON payload.
 *
 * @param jsonDocument A buffer containing the JSON document text to search in
 * (does not need to be '\0' terminated).
 * @param jsonDocumentLength The length of @c jsonDocument (does not need to
 * include a terminating '\0').
 * @param jsonkey A buffer containing the key to search for (does not need to
 * be '\0' terminated).
 * @param jsonkeyLength The length of @c jsonkey (does not need to include a
 * terminating '\0').
 * @param [out] longValue If provided, this will be set to the extracted long if
 * it was able to be found.
 * @return @c true if a long could be successfully parsed, or @c false
 * otherwise.
 */
bool AiaJsonUtils_ExtractLong( const char* jsonDocument,
                               size_t jsonDocumentLength, const char* jsonKey,
                               size_t jsonKeyLength,
                               AiaJsonLongType* longValue );

#endif /* ifndef AIA_JSON_UTILS_H_ */
