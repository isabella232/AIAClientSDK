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

/* The config header is always included first. */
#include <aia_config.h>

#include <aiacore/aia_directive.h>
#include <aiacore/aia_topic.h>

#include <aiaalertmanager/aia_alert_manager.h>

/* Anchor the inline from aia_iot_config.h */
extern inline bool AiaFindJsonValue( const char* jsonDocument,
                                     size_t jsonDocumentLength,
                                     const char* jsonKey, size_t jsonKeyLength,
                                     const char** jsonValue,
                                     size_t* jsonValueLength );

/* Anchor the inline functions from aia_alert_constants.h */
extern inline const char* AiaAlertType_ToString( AiaAlertType_t alert );
extern inline size_t AiaAlertType_GetLength( AiaAlertType_t alertType );
extern inline bool AiaAlertType_FromString( const char* alertTypeString,
                                            size_t alertTypeStringLength,
                                            AiaAlertType_t* alertType );

/* Anchor the inline functions from aia_directive.h */
extern inline const char* AiaDirective_ToString( AiaDirective_t directive );
extern inline size_t AiaDirective_GetLength( AiaDirective_t directive );
extern inline bool AiaDirective_FromString( const char* directiveString,
                                            size_t directiveStringLength,
                                            AiaDirective_t* directive );

/* Anchor the inline functions from aia_topic.h */
extern inline AiaTopicType_t AiaTopic_GetType( AiaTopic_t topic );
extern inline bool AiaTopic_IsEncrypted( AiaTopic_t topic );
extern inline bool AiaTopic_IsOutbound( AiaTopic_t topic );
extern inline const char* AiaTopic_ToString( AiaTopic_t topic );
extern inline size_t AiaTopic_GetLength( AiaTopic_t topic );
extern inline bool AiaTopic_FromString( const char* topicString,
                                        size_t topicStringLength,
                                        AiaTopic_t* topic );
extern inline const char* AiaTopic_GetJsonArrayName( AiaTopic_t topic );
extern inline size_t AiaTopic_GetJsonArrayNameLength( AiaTopic_t topic );

/* Anchor the inline functions from aia_utils.h */
extern inline size_t AiaBytesToHoldBits( size_t bits );
extern inline bool AiaEndsWith( const char* mainString,
                                size_t mainStringCmpLength,
                                const char* subString );
extern inline void AiaReverseByteArray( uint8_t* byteArray,
                                        size_t byteArrayLen );
