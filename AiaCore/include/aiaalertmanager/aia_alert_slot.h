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
 * @file aia_alert_slot.h
 * @brief Used to store information about an individual alert.
 */

#ifndef AIA_ALERT_SLOT_H_
#define AIA_ALERT_SLOT_H_

/* The config header is always included first. */
#include <aia_config.h>

#include <aiaalertmanager/aia_alert_constants.h>

/** This type is used to hold information relevant to an alert. */
typedef struct AiaAlertSlot
{
    /** The actual link in the list. */
    AiaListDouble( Link_t ) link;

    /** Type of this alert */
    AiaAlertType_t alertType;

    /** Alert token */
    char alertToken[ AIA_ALERT_TOKEN_CHARS ];

    /** Scheduled time for this alert seconds. */
    AiaTimepointSeconds_t scheduledTime;

    /** Duration this alert will be played for if not stopped in milliseconds.
     */
    AiaDurationMs_t duration;
} AiaAlertSlot_t;

#endif /* ifndef AIA_ALERT_SLOT_H_ */
