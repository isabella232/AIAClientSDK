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
 * @file aia_utils.h
 * @brief Miscellaneous utility functions.
 */

#ifndef AIA_UTILS_H_
#define AIA_UTILS_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <stdbool.h>
#include <stddef.h>

/** Milliseconds in a second. */
#define AIA_MS_PER_SECOND ( (size_t)1000 )

/**
 * Utility macro for calculating the length of an array.
 *
 * @warning This can only be used on bonafide arrays; it will not work on
 * pointers.
 *
 * @param A An array to calculate the length of.
 * @return The number of elements in @c A.
 */
#define AiaArrayLength( A ) ( sizeof( A ) / sizeof( *( A ) ) )

/**
 * Utility macro for picking the lesser of two items.
 *
 * @param A The first item to consider.
 * @param B The second item to consider.
 * @return the lesser of @c A and @c B.
 */
#define AiaMin( A, B ) ( ( A ) < ( B ) ? ( A ) : ( B ) )

/**
 * Utility function that checks if a string ends with the given suffix.
 *
 * @param mainString The main string to check.
 * @param mainStringCmpLength Length (from the beginning) of the part of
 * mainString to use for comparison.
 * @param subString Suffix to check at the end of main string.
 * @return @c true if the mainString ends with the given suffix, else @c false.
 */
inline bool AiaEndsWith( const char* mainString, size_t mainStringCmpLength,
                         const char* subString )
{
    /* Check for null input arguments */
    if( !mainString || !subString )
    {
        return false;
    }

    return ( ( mainStringCmpLength >= strlen( subString ) ) &&
             ( strncmp( mainString + mainStringCmpLength - strlen( subString ),
                        subString, strlen( subString ) ) == 0 ) );
}

/**
 * Utility function that returns the number of bytes needed to hold the
 * specified number of bits.
 *
 * @param bits The number of bits to calculate bytes for.
 * @return The number of bytes needed to hold @c bits.
 */
inline size_t AiaBytesToHoldBits( size_t bits )
{
    return bits ? ( ( bits - 1 ) / 8 + 1 ) : 0;
}

/**
 * Utility function that reverses a byte array.
 *
 * @param byteArray The byte array to reverse.
 * @param byteArrayLen The length of @c byteArray.
 */
inline void AiaReverseByteArray( uint8_t* byteArray, size_t byteArrayLen )
{
    uint8_t temp;

    for( size_t i = 0; i < byteArrayLen / 2; i++ )
    {
        temp = byteArray[ i ];
        byteArray[ i ] = byteArray[ byteArrayLen - 1 - i ];
        byteArray[ byteArrayLen - 1 - i ] = temp;
    }
}

/**
 * Generates a random and unique null-terminated JSON message Id string.
 *
 * @param [out] buffer buffer of length @c bufferLength to write a
 *     null-terminated message ID string into.
 * @param bufferLength The length of @c buffer.
 * @return @c true if the message ID was generated successfully, else @c false.
 */
bool AiaGenerateMessageId( char* buffer, size_t bufferLength );

#endif /* ifndef AIA_UTILS_H_ */
