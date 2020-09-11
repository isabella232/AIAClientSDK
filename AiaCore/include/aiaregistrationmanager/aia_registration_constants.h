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
 * @file aia_registration_constants.h
 * @brief Constants related to Registration requests.
 */

#ifndef AIA_REGISTRATION_CONSTANTS_H_
#define AIA_REGISTRATION_CONSTANTS_H_

/* The config header is always included first. */
#include <aia_config.h>

#define AIA_REGISTRATION_AUTHENTICATION_KEY "authentication"
#define AIA_REGISTRATION_AUTHENTICATION_TOKEN_KEY "token"
#define AIA_REGISTRATION_AUTHENTICATION_CLIENT_ID_KEY "clientId"

#define AIA_REGISTRATION_ENCRYPTION_KEY "encryption"
#define AIA_REGISTRATION_ENCRYPTION_ALGORITHM_KEY "algorithm"
#define AIA_REGISTRATION_ENCRYPTION_PUBLIC_KEY_KEY "publicKey"

#define AIA_REGISTRATION_CODE_KEY "code"
#define AIA_REGISTRATION_DESCRIPTION_KEY "description"

#define AIA_REGISTRATION_IOT_KEY "iot"
#define AIA_REGISTRATION_IOT_AWS_ACCOUNT_ID_KEY "awsAccountId"
#define AIA_REGISTRATION_IOT_CLIENT_ID_KEY "clientId"
#define AIA_REGISTRATION_IOT_ENDPOINT_KEY "endpoint"
#define AIA_REGISTRATION_IOT_TOPIC_ROOT_KEY "topicRoot"

#endif /* ifndef AIA_REGISTRATION_CONSTANTS_H_ */
