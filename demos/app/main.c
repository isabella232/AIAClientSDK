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
 * @file main.c
 * @brief Main application for AIA Sample Application.
 */

/* The config header is always included first. */
#include <aia_config.h>

#include <aia_sample_app.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <iot_init.h>
/* TODO: Generalize and/or ifdef for easy porting to OpenSSL. */
#include <iot_network_mbedtls.h>

#ifdef __cplusplus
}
#endif

/* Global config */
extern const char* g_aiaIotEndpoint;
extern const char* g_aiaClientId;
extern const char* g_aiaAwsAccountId;
extern const char* g_aiaStorageFolder;
extern char* g_aiaLwaRefreshToken;
extern char* g_aiaLwaClientId;

int main( int argc, const char* argv[] )
{
    /* Validate command-line parameters. */
    /* TODO: Use command-line parameter parsing framework such as getopt(). */
    if( argc != 8 )
    {
        AiaLogError(
            "Usage: %s <IoT Endpoint> <Root CA File> <Client Certificate File> "
            "<Client Certificate Private Key File> <Client Thing Name> <AWS "
            "Account Id> <Persistent Storage Folder>",
            argv[ 0 ] );
        return EXIT_FAILURE;
    }
    const char* endpoint = argv[ 1 ];
    const char* rootCA = argv[ 2 ];
    const char* cert = argv[ 3 ];
    const char* privateKey = argv[ 4 ];
    const char* thingName = argv[ 5 ];
    const char* awsAccountId = argv[ 6 ];
    const char* storageFolder = argv[ 7 ];

    g_aiaIotEndpoint = endpoint;
    g_aiaClientId = thingName;
    g_aiaAwsAccountId = awsAccountId;
    g_aiaStorageFolder = storageFolder;

    /* Basic/Generic IoT SDK initialization. */
    if( !IotSdk_Init() )
    {
        AiaLogError( "IotSdk_Init failed" );
        return EXIT_FAILURE;
    }
    if( IotNetworkMbedtls_Init() != IOT_NETWORK_SUCCESS )
    {
        AiaLogError( "IotNetworkMbedtls_Init failed" );
        return EXIT_FAILURE;
    }
    if( IotMqtt_Init() != IOT_MQTT_SUCCESS )
    {
        AiaLogError( "IotMqtt_Init failed" );
        return EXIT_FAILURE;
    }

    /* Connect to AWS IoT. */
    struct IotNetworkServerInfo serverInfo =
        IOT_NETWORK_SERVER_INFO_MBEDTLS_INITIALIZER;
    serverInfo.pHostName = endpoint;
    serverInfo.port = IOT_DEMO_PORT;

    struct IotNetworkCredentials credentials =
        AWS_IOT_NETWORK_CREDENTIALS_MBEDTLS_INITIALIZER;
    credentials.pClientCert = cert;
    credentials.pPrivateKey = privateKey;
    credentials.pRootCa = rootCA;

    IotMqttNetworkInfo_t networkInfo = IOT_MQTT_NETWORK_INFO_INITIALIZER;
    networkInfo.createNetworkConnection = true;
    networkInfo.u.setup.pNetworkServerInfo = &serverInfo;
    networkInfo.u.setup.pNetworkCredentialInfo = &credentials;
    networkInfo.pNetworkInterface = IOT_NETWORK_INTERFACE_MBEDTLS;

    IotMqttConnectInfo_t connectInfo = IOT_MQTT_CONNECT_INFO_INITIALIZER;
    connectInfo.awsIotMqttMode = true;
    connectInfo.cleanSession = true;
    connectInfo.keepAliveSeconds = 60;

    connectInfo.pClientIdentifier = thingName;
    connectInfo.clientIdentifierLength = (uint16_t)strlen( thingName );

    AiaMqttConnectionPointer_t mqttConnection = IOT_MQTT_CONNECTION_INITIALIZER;
    IotMqttError_t connectStatus = IotMqtt_Connect(
        &networkInfo, &connectInfo, MQTT_TIMEOUT_MS, &mqttConnection );
    if( connectStatus != IOT_MQTT_SUCCESS )
    {
        AiaLogError( "IotMqtt_Connect failed: %s.",
                     IotMqtt_strerror( connectStatus ) );
        return EXIT_FAILURE;
    }

    /* Run the AIA Sample Application. */
    AiaSampleApp_t* sampleApp =
        AiaSampleApp_Create( mqttConnection, thingName );
    if( !sampleApp )
    {
        AiaLogError( "AiaSampleApp_Create failed" );
        return EXIT_FAILURE;
    }
    AiaSampleApp_Run( sampleApp );
    AiaSampleApp_Destroy( sampleApp );

    /* IoT Cleanup. */
    IotMqtt_Disconnect( mqttConnection, 0 );
    IotMqtt_Cleanup();
    IotNetworkMbedtls_Cleanup();
    IotSdk_Cleanup();

    return EXIT_SUCCESS;
}
