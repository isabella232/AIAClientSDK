# Porting the AIA Client SDK

## Overview
The SDK has been written to be platform-agnostic and abstracts away functionality that we believe might be platform-specific or desirable to point to custom implementations.

### Capabilities
AIA clients are expected to assert a set of capabilities. Users of the SDK can define their client capabilities by modifying `aia_capabilities_config.h`. See https://developer.amazon.com/en-US/docs/alexa/alexa-voice-service/avs-for-aws-iot-capability-assertion.html for more information.

### Library Compilation Portability and Configuration Layer
For basic SDK library compilation, we offer a set of compile-time abstractions that users can configure and implement to for their platform and needs. The SDK invokes these APIs.

Roughly grouped by functionality, they are:

#### Button
- The SDK offers the users the ability to enable local stoppage of audio for faster processing on "pause" and "stop" button presses. See `ports/Button`.

#### Clock
- For devices which implement the `Clock` capability, getting and setting of the time since the NTP epoch is required. See `ports/Clock`.
- By default, these point to standard C `time()`.

#### Cryptography
- See `Ports/Crypto`

##### Encryption/Decryption
- The SDK requires AES-GCM encryption/decryption and curve25519 secret derivation APIs.
- By default these point to MbedTLS APIs but can point to custom hardware accelerated APIs or other libraries if desired.

##### Encoding/Decoding
- The SDK requires base64 encoding and decoding when serializing binary data to and from strings.
- By default these point to MbedTLS APIs but can point to custom hardware accelerated APIs or other libraries if desired.

##### Entropy Generation
- The SDK abstracts APIs to seed entropy and to generate random numbers.
- By default these point to MbedTLS APIs but can point to custom hardware accelerated APIs or other libraries if desired.

#### HTTPS Client
- The SDK makes use of an HTTPS client to register with AIA. See `ports/HTTP`.
- By default, this is implemented using Libcurl.

#### AWS IoT Device SDK
- The AWS IoT Device SDK offers the following configurable functionality. Users wishing to configure this should do so via the AWS IoT Device SDK. However, we do present the option to configure these specific to this SDK as well, although this is not recommended. See `ports/IoT`.

##### Logging
- Applications can configure these macros to point to their existing logging infrastructure if desired.
- Applications can set their desired logging level by defining `IOT_LOG_LEVEL_AIA`.
- By default, these point to the AWS IoT Device SDK's logging infrastructure.

##### Clock
- The SDK makes use of monotonic clock operations for relative time calculations.
- By default, these point to the AWS IoT Device SDK's clock APIs.

##### TaskPool
- The SDK uses a taskpool to accomplish certain asynchronous behaviors.
- By default, these point to the AWS IoT Device SDK's TaskPool.

##### JSON Parsing
- The SDK offers an abstraction around a simple JSON parsing API that applications may point to their existing JSON parsing infrastructure if desired.
- By default, this points to the AWS IoT Device SDK's JSON parser.

##### Atomic Primitive Type Operations
- To ensure thread safe behavior on multi-threaded applications, the SDK makes use of atomic types that applications can point to custom hardware instructions.
- Single threaded applications may point these to simple primitive type operations.
- By default, these point to the AWS IoT Device SDK's atomic abstractions.

##### Mutex
- To ensure thread-safe behavior on multi-threaded applications, the SDK makes use of locking mechanisms.
- Single threaded applications can implement these operations as no-ops.
- By default, these point to the AWS IoT Device SDK's threading abstractions.

##### Semaphore
- Implemeted the same way as mutexes.

##### Linked Lists
- The SDK makes use of dynamically expandable doubly linked lists that applications may point to existing platform data structures if desired.
- By default, these point to the AWS IoT Device SDK's linear containers.

##### Timers
- The SDK also makes use of Timer APIs for periodic tasks, which by default point to the AWS IoT Device SDK's timer APIs.

##### MQTT Operations
- The SDK needs to connect, publish, and subscribe to AWS IoT to perform AIA related functionality. These APIs point to the AWS IoT Device SDK's APIs by default.

#### LWA
- Any application making use of the SDK) must provide an implementation of LWA functionality. On a real device, this function would map to the platform's LWA configuration (such as [CBL](https://developer.amazon.com/en-US/docs/alexa/alexa-voice-service/code-based-linking-other-platforms.html) or [companion app](https://developer.amazon.com/en-US/docs/alexa/alexa-voice-service/authorize-companion-app.html) based authentication).

#### Memory Management
- The SDK offers abstractions around `calloc` and `free` that applications may implement to point to custom memory blocks if desired.
- By default, these point to standard C APIs.

#### Microphone
- The SDK offers users the ability to configure the microphone publishing sizes and rates to best fit their platform. See `ports/Microphone`.

#### Persistent Storage
- The SDK persists and loads opaque blobs of data; such as offline alerts, to ensure state is kept in case of reboots or network connectivity loss.
- By default, this is implemented using files; but, it can also point to other implementations; such as flash storage, if file systems are not supported on the device.
- The SDK also loads and persists the encryption key used for end-to-end encryption. By default, these abstractions point to the opaque storage abstractions listed above; but, can also point to the more secure storage if desired by the device. See `ports/Storage`.

#### Registration
- Information required for AIA registration may be provided by implementing the functions within `aia_registration_config.h`.
- The provided demo takes these in from command line at the application startup. A real device will likely override this behavior with something different.

### Application Compilation Portability Layer
A complete application that builds around the SDK will also be required to implement the following APIs, mostly related to audio I/O. These APIs are enumerated in `aia_application_config.h` and are all required parameters for creation of the `AiaClient_t` object.

## How do I provide an alternative implementation?
You have two options to modify the porting layer of the SDK.

1. You can directly modify the source code of this project to provide your intended functionality. This can be helpful for rapid iterative development.

2. You can provide your own "ports" folder. To do this, you would specify the following option in your CMake command:
```
-DALT_PORTS_FOLDER=<path-to-alternative-ports-folder>
```
This folder would in turn contain the selective ports you wish to modify. For example, if you wanted to modify the provided storage implementation of the SDK, your `ALT_PORTS_FOLDER` should contain a `Storage` folder which AIA will allow to take precedence over the default provided `Storage` port. You are required to write CMake code such that it will output the appropriate library and include files that are expected by the SDK.
