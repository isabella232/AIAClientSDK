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
 * @file aia_json_utils.c
 * @brief Implements utility functions in aia_json_utils.h.
 */

/* The config header is always included first. */
#include <aia_config.h>

#include <aiacore/aia_json_utils.h>

#include <ctype.h>

bool AiaJsonUtils_UnquoteString( const char** jsonString,
                                 size_t* jsonStringLength )
{
    if( !jsonString || !*jsonString )
    {
        return false;
    }
    if( !jsonStringLength || *jsonStringLength < 2 )
    {
        return false;
    }
    if( ( *jsonString )[ 0 ] != '\"' )
    {
        return false;
    }
    ++*jsonString;
    --*jsonStringLength;
    if( ( *jsonString )[ *jsonStringLength - 1 ] != '\"' )
    {
        return false;
    }
    --*jsonStringLength;
    return true;
}

bool AiaExtractLongFromJsonValue( const char* jsonValue, size_t jsonValueLength,
                                  AiaJsonLongType* longValue )
{
    if( !jsonValue )
    {
        AiaLogError( "null jsonValue" );
        return false;
    }
    if( !longValue )
    {
        AiaLogError( "null longValue" );
        return false;
    }
    char cStrJsonValue[ jsonValueLength + 1 ];
    memcpy( cStrJsonValue, jsonValue, jsonValueLength );
    cStrJsonValue[ jsonValueLength ] = '\0';

    AiaJsonLongType extractedVal = 0;
    char* endPtr;
    extractedVal = (AiaJsonLongType)strtoul( cStrJsonValue, &endPtr, 10 );
    if( endPtr != cStrJsonValue + jsonValueLength )
    {
        AiaLogError( "Invalid jsonValue, jsonValue=%s", cStrJsonValue );
        return false;
    }

    *longValue = extractedVal;
    return true;
}

bool AiaJsonUtils_GetArrayElement( const char* jsonArray,
                                   size_t jsonArrayLength, size_t index,
                                   const char** jsonValue,
                                   size_t* jsonValueLength )
{
    if( !jsonArray )
    {
        AiaLogError( "Null jsonArray." );
        return false;
    }

    if( !jsonArrayLength )
    {
        AiaLogError( "Empty jsonArray." );
        return false;
    }

    if( !jsonValue )
    {
        AiaLogError( "Null jsonValue." );
        return false;
    }

    if( !jsonValueLength )
    {
        AiaLogError( "Null jsonValueLength." );
        return false;
    }

    const char* currentByte = jsonArray;

    /* Make sure it starts with an array bracket. */
    if( *currentByte != '[' )
    {
        return false;
    }

    /* Work through the array one character at a time. */
    size_t depth = 0;
    *jsonValue = NULL;
    while( ( size_t )( ++currentByte - jsonArray ) < jsonArrayLength )
    {
        /* Skip over leading whitespace. */
        if( isspace( (int)( *currentByte ) ) )
        {
            continue;
        }

        /* Note the start of the desired value if we've found it. */
        if( 0 == depth && 0 == index && !*jsonValue )
        {
            *jsonValue = currentByte;
        }

        /* TODO: Add support for treating quoted strings as atomic elements so
         * that we don't interpret commas, braces and brackets inside strings.
         * (ADSER-1651) */

        /* A comma or close-bracket at the top level ends an array entry. */
        if( 0 == depth && ( ',' == *currentByte || ']' == *currentByte ) )
        {
            /* If this was the index we were looking for, we've found the end.
             */
            if( 0 == index )
            {
                /* Sanity check - we should already have noted the start. */
                if( !*jsonValue )
                {
                    AiaLogError(
                        "Value pointer still null when at end of value." );
                    return false;
                }

                /* Strip any trailing whitespace. */
                while( currentByte - *jsonValue > 1 &&
                       isspace( (int)( *( currentByte - 1 ) ) ) )
                {
                    --currentByte;
                }

                *jsonValueLength = currentByte - *jsonValue;
                return true;
            }

            /* Otherwise, this comma moves us to the next array entry. */
            --index;
            continue;
        }

        /* Open-brackets/braces increase our depth. */
        if( '[' == *currentByte || '{' == *currentByte )
        {
            ++depth;
            continue;
        }

        /* Close-brackets/braces decrease our depth. */
        if( ']' == *currentByte || '}' == *currentByte )
        {
            --depth;
            continue;
        }
    }

    AiaLogDebug( "Index not found in array." );
    return false;
}

bool AiaJsonUtils_ExtractLong( const char* jsonDocument,
                               size_t jsonDocumentLength, const char* jsonKey,
                               size_t jsonKeyLength,
                               AiaJsonLongType* longValue )
{
    if( !jsonDocument )
    {
        AiaLogError( "Null jsonDocument" );
        return false;
    }
    if( !jsonKey )
    {
        AiaLogError( "Null jsonKey" );
        return false;
    }
    if( !longValue )
    {
        AiaLogError( "Null longValue" );
        return false;
    }

    const char* valueStr = NULL;
    size_t valueLen = 0;
    if( !AiaFindJsonValue( jsonDocument, jsonDocumentLength, jsonKey,
                           jsonKeyLength, &valueStr, &valueLen ) )
    {
        AiaLogError( "Key not found, key=%.*s", jsonKeyLength, jsonKey );
        return false;
    }

    return AiaExtractLongFromJsonValue( valueStr, valueLen, longValue );
}
