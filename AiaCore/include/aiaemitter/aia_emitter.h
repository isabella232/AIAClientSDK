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
 * @file aia_emitter.h
 * @brief User-facing functions of the @c AiaEmitter_t type.
 */

#ifndef AIA_EMITTER_H_
#define AIA_EMITTER_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <aiacore/aia_message.h>
#include <aiacore/aia_message_constants.h>
#include <aiacore/aia_topic.h>
#include <aiaregulator/aia_regulator.h>
#include <aiasecretmanager/aia_secret_manager.h>

/**
 * This class assembles message chunks for @c AiaRegulator into MQTT messages
 * and publishes them.
 *
 * @note Functions in this header which act on an @c AiaEmitter_t are
 *     not thread-safe and must be externally synchronized.
 */
typedef struct AiaEmitter AiaEmitter_t;

/**
 * Constructs a new emitter which publishes to @c topic using @c mqttConnection.
 *
 * @param mqttConnection The MQTT connection to use for publishing.
 * @param secretManager The secret manager to use for encrypting MQTT messages.
 * @param topic The topic to publish to.
 * @return @c true if the emitter was created successfully, else @c false.
 */
AiaEmitter_t* AiaEmitter_Create( AiaMqttConnectionPointer_t mqttConnection,
                                 AiaSecretManager_t* secretManager,
                                 AiaTopic_t topic );

/**
 * Function which accepts message chunks from an @c AiaRegulator.  When all
 * the chunks for a given MQTT message have been received, this function
 * assembles the full message and publishes it.
 *
 * @param emitter The emitter to use for publishing this @c chunkForMessage.
 * @param chunkForMessage The message chunk to publish.
 * @param remainingBytes The number of bytes remaining in the MQTT message after
 *     this chunk.
 * @param remainingChunks The number of chunks remaining in the MQTT message
 *     after this chunk.
 * @return @c true if the chunk was accepted/published, else @c false.
 */
bool AiaEmitter_EmitMessageChunk( AiaEmitter_t* emitter,
                                  AiaRegulatorChunk_t* chunkForMessage,
                                  size_t remainingBytes,
                                  size_t remainingChunks );

/**
 * Cleans up an emitter and releases associated resources.
 *
 * @param emitter The emitter to clean up.
 */
void AiaEmitter_Destroy( AiaEmitter_t* emitter );

/**
 * Returns the sequence number which will be used for the next MQTT message
 * published on this emitter's topic.
 *
 * @param emitter The @c AiaEmitter_t instance to act on.
 * @param[out] nextSequenceNumber If successful, this will be set to the next
 * sequence number.
 * @return @c true if the next sequence number is returned successfully or @c
 * false otherwise.
 * @note This method is safe to call without external synchronization.
 */
bool AiaEmitter_GetNextSequenceNumber(
    AiaEmitter_t* emitter, AiaSequenceNumber_t* nextSequenceNumber );

#endif /* ifndef AIA_EMITTER_H_ */
