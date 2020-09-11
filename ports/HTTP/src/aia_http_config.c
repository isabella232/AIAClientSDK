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
 * @file aia_http_config.c
 * @brief Implements platform-specific Crypto functions which are not inlined in
 * @c aia_http_config.h.
 */

#include <http/aia_http_config.h>

#include <aia_libcurl.h>

bool AiaSendHttpsRequest( AiaHttpsRequest_t* httpsRequest,
                          AiaHttpsConnectionResponseCallback_t responseCallback,
                          void* responseCallbackUserData,
                          AiaHttpsConnectionFailureCallback_t failureCallback,
                          void* failureCallbackUserData )
{
    return AiaLibCurlHttpClient_SendHttpsRequest(
        httpsRequest, responseCallback, responseCallbackUserData,
        failureCallback, failureCallbackUserData );
}
