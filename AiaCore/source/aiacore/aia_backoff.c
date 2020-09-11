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
 * @file aia_backoff.c
 * @brief Implements functions in aia_backoff.h
 */

/* The config header is always included first. */
#include <aia_config.h>

#include <aiacore/aia_backoff.h>

#include <limits.h>

AiaDurationMs_t AiaBackoff_GetBackoffTimeMilliseconds(
    size_t retryNum, AiaDurationMs_t maxBackoff )
{
    if( retryNum == 0 || maxBackoff == 0 )
    {
        return 0;
    }
    AiaDurationMs_t backoff;
    /*
     * Binary exponential backoff is calculated with base * 2 ** retryNum.
     * The base being 1 second which is 1000ms. If 1000 * 2 ** retryNum is
     * greater than the maxBackoff then backoff is equal to maxBackoff else it
     * is equal to 1000 * 2 ** retryNum.
     */
    if( maxBackoff >> retryNum < 1000 )
    {
        backoff = maxBackoff;
    }
    else
    {
        backoff = 1000 << retryNum;
    }
    AiaDurationMs_t jitterBackoff;
    AiaRandom_Rand( (unsigned char *)&jitterBackoff, sizeof( jitterBackoff ) );
    jitterBackoff %= backoff;

    return jitterBackoff;
}
