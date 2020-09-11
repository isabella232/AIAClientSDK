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
 * @file aia_message.c
 * @brief Implements functions for the AiaMessage_t type.
 */

#include <aiacore/aia_message.h>
#include <aiacore/private/aia_message.h>

bool _AiaMessage_Initialize( AiaMessage_t* message, size_t size )
{
    if( !message )
    {
        AiaLogError( "Null message." );
        return false;
    }
    message->size = size;
    return true;
}

void _AiaMessage_Uninitialize( AiaMessage_t* message )
{
    /* nothing to do here */
    (void)message;
}

size_t AiaMessage_GetSize( const AiaMessage_t* message )
{
    AiaAssert( message );
    return message ? message->size : 0;
}
