# AIA SDK Quick Start Guide
**Note:**: This quick start guide has only been tested on a Raspberry Pi.

## 1. Create a workspace and set up prerequisites
First, choose a folder to serve as your workspace. This will be called `<workspace>` in the following instructions and should be replaced wherever referenced.

Then, install the following required packages.
```
sudo apt-get install git
sudo apt-get install cmake
sudo apt-get install pkg-config
```

## 2. Clone/Obtain the AIA SDK
This QuickStart Guide assumes that the SDK will be located in `<workspace>`.

## 3. Build Dependencies
### 3.1 MbedTLS
If you are using MbedTLS, you will need to build it using the config header supplied with the SDK. This can be done as follows:
```
cd <workspace>
mkdir mbedtlsBuild
mkdir mbedtlsInstall
git clone --recursive https://github.com/ARMmbed/mbedtls.git
cd mbedtls
git checkout mbedtls-2.16.5 // Note that this is the version most recently tested against the SDK.
cd ../mbedtlsBuild
CFLAGS="-I<workspace>/AIAClientSDK/ports/include -DMBEDTLS_CONFIG_FILE='<aia_config_mbedtls.h>' -fPIC -no-pie -fvisibility=hidden" cmake -DCMAKE_INSTALL_PREFIX=../mbedtlsInstall ../mbedtls
make
make install
```

### 3.2 Unity
The Unity test framework could be built and installed as follows:
```
cd <workspace>
mkdir unityBuild
mkdir unityInstall
git clone https://github.com/ThrowTheSwitch/Unity.git
cd Unity
git checkout 5e9acef74faffba86375258d822f49d0b1173b5e // Note that this is the version most recently tested against the SDK.
cd ../unityBuild
cmake -DCMAKE_INSTALL_PREFIX=../unityInstall ../Unity
make
make install
```

### 3.3 Libcurl
LibCurl could be built using the following:
```
sudo apt-get install libcurl4-openssl-dev
```

### 3.4 AWS IoT Embedded C SDK
AWS IoT Device SDK could be built following the instructions below:
```
cd <workspace>
mkdir iotBuild
git clone https://github.com/aws/aws-iot-device-sdk-embedded-C.git
cd aws-iot-device-sdk-embedded-C
git checkout v4_beta
cd ../iotBuild
cmake ../aws-iot-device-sdk-embedded-C
make
```

## 4 Build and Install Sample App Dependencies
The SDK's sample app makes full use of Audio I/O off the device platform. It requires [PortAudio - an Open-Source Cross-Platform Audio API](http://www.portaudio.com/) and [libopus 1.3 - Opus Codec](http://opus-codec.org/release/stable/2018/10/18/libopus-1_3.html).  These are enabled by default and will be autodetected if they are installed in a system path or a path you have specified in your CMAKE_PREFIX_PATH.

**Note:**: The sample app included with the SDK will only be compiled if all of the above dependencies are available. They could be installed using the following instructions:
```
cd <workspace>
mkdir opusInstall
sudo apt-get install autoconf automake libtool gcc make
sudo apt-get install libasound2-dev
git clone https://github.com/xiph/opus.git
cd opus
./autogen.sh
./configure --prefix=<workspace>/opusInstall
make
make install

cd <workspace>
mkdir portaudioInstall
wget -c http://www.portaudio.com/archives/pa_stable_v190600_20161030.tgz
tar zxf pa_stable_v190600_20161030.tgz
cd portaudio
./configure --without-jack --prefix=<workspace>/portaudioInstall
make
make install
```

You also need to set up your microphone following the instructions [here](https://developer.amazon.com/en-US/docs/alexa/avs-device-sdk/raspberry-pi.html#set-up-the-microphone) for the sample app to work.

## 5. Authorize and Register your Client
### 5.1 Register your AVS device with Amazon
Before you can authorize a user of your device, you must first register an AVS product and create a security profile.  There are detailed instructions [here](https://developer.amazon.com/en-US/docs/alexa/alexa-voice-service/register-a-product.html), but note the following two deviations from the instructions for filling in product information:
- For **Product category**, choose "Smart Home" instead of "Other".
- For **AWS IoT Core Accounts associated with the device**, Select "Yes" and enter your AWS Account ID(s).

After registering your device, you download a config.json file. This file contains your `clientID` and `productID`. The `clientID` and `refresh_token` generated in the next section authorize your device, so you can retrieve access tokens from AVS. Your config.json file facilitates the authorization calls between your device and AVS.

> **Important**: Save the config.json file somewhere accessible. You use its contents later in the tutorial to build the SDK.

### 5.2 Authorization
All clients of the AIA service must complete a [standard AVS LWA Authentication flow](https://developer.amazon.com/en-US/docs/alexa/alexa-voice-service/api-overview.html#authorization).  Note that AIA only supports [Companion App Authorization](https://developer.amazon.com/en-US/docs/alexa/alexa-voice-service/authorize-companion-app.html) and [Code-Based Linking Authorization](https://developer.amazon.com/en-US/docs/alexa/alexa-voice-service/code-based-linking-other-platforms.html).

The following are some sample commands to use CBL authorization:

- Make sure to replace `YOUR_LWA_CLIENT_ID` with the `clientID` in config.json file you downloaded earlier and `YOUR_PRODUCT_ID` with the `productID` from the same file. `YOUR_DSN` can be any unique device serial number you specify.

```
curl -k -d'response_type=device_code&client_id=YOUR_LWA_CLIENT_ID&scope=alexa%3Aall&scope_data=%7B%22alexa%3Aall%22%3A%7B%22productID%22%3A%22YOUR_PRODUCT_ID%22,%22productInstanceAttributes%22%3A%7B%22deviceSerialNumber%22%3A%22YOUR_DSN%22%7D%7D%7D'-H"Content-Type: application/x-www-form-urlencoded"-X POST https://api.amazon.com/auth/O2/create/codepair
```

- Next, sign into here using the `user_code` returned from the previous curl command: [https://amazon.com/us/code](https://amazon.com/us/code)
- Finally, use the `device_code` and `user_code` returned from the first command in the following one in a polling fashion.

```
curl 'https://api.amazon.com/auth/O2/token' -H "Content-Type: application/x-www-form-urlencoded" -X POST -d 'grant_type=device_code&device_code=YOUR_DEVICE_CODE&user_code=YOUR_USER_CODE'
```

**Note:**: It may take the user some time to complete registration (or registration may be abandoned). During polling the following responses may be delivered: `authorization_pending`, `slow_down`, `invalid_code_pair`, `invalid_client` or `unauthorized_client`. On `invalid_code_pair`, restart the registration flow.

4. Be sure to note the `refresh_token` you get back somewhere. This and the `clientID` from the config.json file will be used in the subsequent registration step.

### 5.3 Registration
As per the [Specification](https://developer.amazon.com/en-US/docs/alexa/alexa-voice-service/avs-for-aws-iot-registration.html), to enable your client with AIA, you must perform a registration request.

Registration can be done by running the sample app and using the registration command `r` (See Section 8), where you will prompoted to provide the `LWA client id` (`clientID` from config.json file) and `refresh_token` (generated in the previous section).

## 6. Building and Installing the SDK
Before building the SDK and running the sample app; first create the `databases` and `certs` folders. They are going to be used by the sample app.
```
cd <workspace>
mkdir databases
mkdir certs
```

### 6.1 Building the SDK
We are now ready to build the SDK. The following is a sample `CMake` command for building the SDK and its sample app from an out of source build directory:
```
cd <workspace>
mkdir sdkBuild
cd sdkBuild
cmake ../AIAClientSDK -DCMAKE_PREFIX_PATH="<workspace>/mbedtlsInstall;<workspace>/unityInstall;<workspace>/Unity;<workspace>/aws-iot-device-sdk-embedded-C;<workspace>/iotBuild/output" -DPORTAUDIO_INCLUDE_DIR="<workspace>/portaudioInstall/include" -DPORTAUDIO_LIB_PATH="<workspace>/portaudioInstall/lib/libportaudio.so" -DLIBOPUS_INCLUDE_DIR="<workspace>/opusInstall/include" -DLIBOPUS_LIB_PATH="<workspace>/opusInstall/lib/libopus.so"
make
```

**Notes:**
- You can turn off dependencies (if desired) during the CMake process with the following CMake flags:
```
-DUSE_MBEDTLS=OFF
-DAIA_LIBCURL_HTTP_CLIENT=OFF
-DAIA_PORTAUDIO_MICROPHONE=OFF
-DAIA_PORTAUDIO_SPEAKER=OFF
-DAIA_OPUS_DECODER=OFF
```

- If your platform can not be automatically detected by the build, you can manually specify it with an additional cmake parameter:
```
cmake -DIOTSDK_PLATFORM_NAME=<platform-name> /path/to/AiaSDK
```

  Platform may be "posix" or [whichever custom port you have implemented when building the AWS IoT Device SDK](https://docs.aws.amazon.com/freertos/latest/lib-ref/c-sdk/main/guide_developer_porting.html).

- **If you run into issues** with the failures to find dependencies, you can manually pass in dependency paths, with the following flags:
```
-DLIBCURL_INCLUDE_DIR=/path/to/curlInstall/include
-DLIBCURL_LIB_PATH=/path/to/curlInstall/lib/libcurl.so
```

  Alternatively, if you do not prefer to use LibCurl (with `-DAIA_LIBCURL_HTTP_CLIENT=OFF`), you can point the LibCurl functionality to your platform's specific HTTP(s) client APIs by replacing the implementations of functions in `aia_http_config.h`.

  ```
  -DMBEDTLS_INCLUDE_DIR=/path/to/mbedtlsInstall/include
  -DMBEDCRYPTO_LIB_PATH=/path/to/mbedtlsInstall/lib/libmbedtls.a
  ```

  Similar to LibCurl, if you do not prefer to use MbedTLS (with `-DUSE_MBEDTLS=OFF`), you can point its functionality to your platform's specific cryptographic and random number generation APIs by replacing the implementations of functions in `aia_crypto_config.h`.

### 6.2 Installing the SDK
**Note:** This section is not required for running the sample app.

The AIA SDK includes a `make install` target which can be used to stage the headers and libraries for building applications outside of the AIA tree.  To specify an install directory, use the cmake command-line flag `-DCMAKE_INSTALL_PREFIX=/path/to/aia-sdk-install`.

An application can be built using this installation as follows:
```
export PKG_CONFIG_PATH=/path/to/aia-sdk-install/lib/pkgconfig
eval gcc aia_sample_app.c main.c -I. $(pkg-config AiaClientSDK --cflags --libs)
```

There are a few known issues with the install target to keep in mind when building external applications:
- The leading `eval` is required to work around a problem with how characters are escaped in the `--cflags` output.
- `AiaClientSDK.pc` includes absolute paths to the AWS IoT Device SDK and MbedTLS; if you will be relocating these, you may need to hand-edit `AiaClientSDK.pc`.

## 7. Running the Sample App
The sample app needs a few certificates to connect to AWS IoT before running it.

### 7.1 Create an AWS IoT Thing and Certificates
Please refer to the following [page](https://docs.aws.amazon.com/iot/latest/developerguide/sdk-tutorials.html#iot-sdk-create-thing) to create an AWS IoT Thing and its certificates. Download the certificates and place them in the `<workspace>/certs` folder.

### 7.2 Run the Sample App
You are now ready to run the sample app with the following command:
```
cd <workspace>/sdkBuild/demos/app
PA_ALSA_PLUGHW=1 ./aia_demo <iot_endpoint> <workspace>/certs/AmazonRootCA1.pem <workspace>/certs/certificate.pem.crt <workspace>/certs/private.pem.key <YOUR_AIA_CLIENT_ID> <YOUR_AWS_ACCOUNT_ID> <workspace>/databases
```
- `iot_endpoint` could be retrieved from the Setting tab of AWS IoT Core console.
- `YOUR_AIA_CLIENT_ID` should be the name you have given to the AWS IoT Thing created in Section 7.1.
- `YOUR_AWS_ACCOUNT_ID` is the `account ID` you have used in Section 5.1

**Note:** Arguments are borrowed from the AWS IoT Device SDK's demo. See [their documentation for a better explanation of required demo arguments](https://github.com/aws/aws-iot-device-sdk-embedded-C/tree/v4_beta). It may be worth it to run the AWS IoT demo first after building it as a dependency of this SDK to verify that you have correctly configured it.

**Note:** The sample app is a reference application written around the main component that applications are expected to use for AIA communication: `AiaClient_t`.

Example command:
```
PA_ALSA_PLUGHW=1 ./aia_demo a2g0alos9ozw7t-ats.iot.us-west-2.amazonaws.com <workspace>/certs/AmazonRootCA1.pem <workspace>/certs/certificate.pem.crt <workspace>/certs/private.pem.key TestThing 12345678 <workspace>/databases
```

## 8. Interacting with the Service
Before any sort of interaction can occur, a few initial steps need to occur first. These are manual for now in the sample app via keyboard inputs but can and should be automated in any production device. Note that a help screen of all commands can be shown at any time by pressing the `h` button. A real application will automate this via programatic calls to AiaClient. The sample app is only a reference implementation illustrating usage of the SDK.

- [Register with the service using the `r` button if not already registered.](https://developer.amazon.com/en-US/docs/alexa/alexa-voice-service/avs-for-aws-iot-registration.html)
  - Provide LWA Client Id. This is the `client ID` from config.json file.
  - Provide LWA Refresh Token. This is the `refresh_token` from section 5.2.
- Initialize the client using the `e` button.
- [Connect to service using the `c` button.](https://developer.amazon.com/en-US/docs/alexa/alexa-voice-service/avs-for-aws-iot-connection.html)
- [If this is your first connection, declare your device capabilities using the `i` button. Note that these are found in aia_capabilities_config.h and should be defined based on device capabilities. See API documentation for a more thorough explanation.](https://developer.amazon.com/en-US/docs/alexa/alexa-voice-service/avs-for-aws-iot-capability-assertion.html)
- [Then, synchronize the device's offline state with the service using the `s` button.](https://developer.amazon.com/en-US/docs/alexa/alexa-voice-service/avs-for-aws-iot-system.html#synchronizestate)
- [Now, an interaction can occur (`t` to simulate a tap to talk interaction, or `h` twice to simulate a hold to talk interaction).](https://developer.amazon.com/en-US/docs/alexa/alexa-voice-service/avs-for-aws-iot-microphone.html#microphoneopened)


## 9. Enable further logging
You can enable further logging in the SDK by configuring `aia_config.h` and changing `#define IOT_LOG_LEVEL_AIA` from `IOT_LOG_INFO` to `IOT_LOG_DEBUG`.
