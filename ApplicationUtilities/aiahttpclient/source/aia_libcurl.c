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
 * @file aia_libcurl.c
 * @brief Implements functions found in @c aia_libcurl.h
 */

#include <aia_libcurl.h>

#include <curl/curl.h>

/* TODO: This is a ballpark estimate. Calculate this more precisely. */
/** Maximimum length possible in a registration response. */
#define MAX_RESPONSE_BODY_LEN 250

/**
 * Implements the write callback of LibCurl.
 *
 * @param ptr Points to the delivered data.
 * @param size This is always 1.
 * @param nmemb The size of the data.
 * @param userdata Optional user data to be passed back to this callback.
 * @return The number of bytes taken care of.
 * @see https://curl.haxx.se/libcurl/c/CURLOPT_WRITEFUNCTION.html
 */
static size_t AiaLibCurlHttpClient_ResponseBodyCallback( char *ptr, size_t size,
                                                         size_t nmemb,
                                                         void *userdata );

/** Simple struct to keep track of received data. */
struct ResponseBodyMemoryStruct
{
    /** Buffer in which to write received data into. */
    char body[ MAX_RESPONSE_BODY_LEN ];

    /** The amount of bytes received so far via @c
     * AiaLibCurlHttpClient_ResponseBodyCallback. */
    size_t bodyLenConsumedSoFar;
};

bool AiaLibCurlHttpClient_SendHttpsRequest(
    AiaHttpsRequest_t *httpsRequest,
    AiaHttpsConnectionResponseCallback_t responseCallback,
    void *responseCallbackUserData,
    AiaHttpsConnectionFailureCallback_t failureCallback,
    void *failureCallbackUserData )
{
    if( !responseCallback )
    {
        AiaLogError( "Null responseCallback" );
        return false;
    }

    if( !failureCallback )
    {
        AiaLogError( "Null failureCallback" );
        return false;
    }

    CURL *curl = curl_easy_init();
    if( !curl )
    {
        AiaLogError( "curl_easy_init failed" );
        return false;
    }

    /* Setting the url to send the request to. */
    CURLcode res = curl_easy_setopt( curl, CURLOPT_URL, httpsRequest->url );
    if( res != CURLE_OK )
    {
        AiaLogError( "curl_easy_setopt failed, error=%s",
                     curl_easy_strerror( res ) );
        curl_easy_cleanup( curl );
        return false;
    }

    /* Flag to allow redirects. */
    res = curl_easy_setopt( curl, CURLOPT_FOLLOWLOCATION, 1L );
    if( res != CURLE_OK )
    {
        AiaLogError( "curl_easy_setopt failed, error=%s",
                     curl_easy_strerror( res ) );
        curl_easy_cleanup( curl );
        return false;
    }

    /* Adding in headers one by one. */
    struct curl_slist *list = NULL;
    struct curl_slist *temp = NULL;
    for( size_t i = 0; i < httpsRequest->headersLen; ++i )
    {
        /* Note: The string is not deep-copied and must remain valid until this
         * allocation the transfer is complete. */
        temp = curl_slist_append( list, httpsRequest->headers[ i ] );
        if( !temp )
        {
            AiaLogError( "curl_slist_append failed" );
            curl_slist_free_all( list );
            curl_easy_cleanup( curl );
        }
        list = temp;
    }
    res = curl_easy_setopt( curl, CURLOPT_HTTPHEADER, list );
    if( res != CURLE_OK )
    {
        AiaLogError( "curl_easy_setopt failed, error=%s",
                     curl_easy_strerror( res ) );
        curl_slist_free_all( list );
        curl_easy_cleanup( curl );
        return false;
    }

    /* Setting the request body. */
    res = curl_easy_setopt( curl, CURLOPT_POSTFIELDS, httpsRequest->body );
    if( res != CURLE_OK )
    {
        AiaLogError( "curl_easy_setopt failed, error=%s",
                     curl_easy_strerror( res ) );
        curl_slist_free_all( list );
        curl_easy_cleanup( curl );
        return false;
    }
    switch( httpsRequest->method )
    {
        case AIA_HTTPS_METHOD_POST:
            /* Using CURLOPT_POSTFIELDS implies setting CURLOPT_POST to 1. */
            break;
        default:
            /* For other types, something like the following should be done:
             * curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT"); */
            AiaLogError( "Unsupported AiaHttpsMethod_t, method=%d",
                         httpsRequest->method );
            curl_slist_free_all( list );
            curl_easy_cleanup( curl );
            return false;
    }

    /* Setting the callback to receive the response body. */
    res = curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION,
                            AiaLibCurlHttpClient_ResponseBodyCallback );
    if( res != CURLE_OK )
    {
        AiaLogError( "curl_easy_setopt failed, error=%s",
                     curl_easy_strerror( res ) );
        curl_slist_free_all( list );
        curl_easy_cleanup( curl );
        return false;
    }
    struct ResponseBodyMemoryStruct body;
    memset( &body, 0, sizeof( body ) );

    /* Setting user data for @c AiaLibCurlHttpClient_ResponseBodyCallback. */
    res = curl_easy_setopt( curl, CURLOPT_WRITEDATA, &body );
    if( res != CURLE_OK )
    {
        AiaLogError( "curl_easy_setopt failed, error=%s",
                     curl_easy_strerror( res ) );
        curl_slist_free_all( list );
        curl_easy_cleanup( curl );
        return false;
    }

    /* TODO: ADSER-1897 This is blocking. Update to use multi interface for
     * non-blocking functionality. */
    res = curl_easy_perform( curl );
    if( res != CURLE_OK )
    {
        AiaLogError( "curl_easy_perform failed, error=%s",
                     curl_easy_strerror( res ) );
        failureCallback( failureCallbackUserData );
        curl_slist_free_all( list );
        curl_easy_cleanup( curl );
        return true;
    }

    /* All data is now received. */

    /* Parse response code. */
    long response_code;
    res = curl_easy_getinfo( curl, CURLINFO_RESPONSE_CODE, &response_code );
    if( res != CURLE_OK )
    {
        AiaLogError( "curl_easy_perform failed, error=%s",
                     curl_easy_strerror( res ) );
        failureCallback( failureCallbackUserData );
        curl_slist_free_all( list );
        curl_easy_cleanup( curl );
        return true;
    }
    AiaHttpsResponse_t response;
    response.body = body.body;
    response.bodyLen = body.bodyLenConsumedSoFar;
    response.status = response_code;
    responseCallback( &response, responseCallbackUserData );

    curl_slist_free_all( list );
    curl_easy_cleanup( curl );
    return true;
}

static size_t AiaLibCurlHttpClient_ResponseBodyCallback( char *content,
                                                         size_t size,
                                                         size_t nmemb,
                                                         void *userdata )
{
    (void)size;
    (void)nmemb;
    struct ResponseBodyMemoryStruct *body = userdata;
    size_t contentSizeReceived = size * nmemb;
    if( body->bodyLenConsumedSoFar + contentSizeReceived >
        MAX_RESPONSE_BODY_LEN )
    {
        AiaLogError(
            "Not enough space to consume received data, received so far=%zu, "
            "incoming=%zu",
            body->bodyLenConsumedSoFar, contentSizeReceived );
        return 0;
    }
    memcpy( body->body + body->bodyLenConsumedSoFar, content,
            contentSizeReceived );
    body->bodyLenConsumedSoFar += contentSizeReceived;
    return size * nmemb;
}
